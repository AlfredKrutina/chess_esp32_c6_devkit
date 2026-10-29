/**
 * @file web_server_task.c
 * @brief HTTP server (port 80), Wi‑Fi STA/AP according to NVS, REST API and WebSocket for mobile application
 *
 * @details
 * Browser UI not on root path — GET `/` returns indicative JSON; full service is in the app.
 * Hotspot of the board (AP) is disabled by default; enable via BLE (`wifi_ap_set`) or NVS `ap_user_en`.
 *
 * =============================================================================
 * WHAT DID THIS FILE DO?
 * =============================================================================
 *
 * 1. Wi‑Fi — STA according to NVS; AP only when the user turns it on (BLE / NVS).
 * 2. HTTP server (port 80) — REST `/api/...`, root without HTML interface.
 * 3. WebSocket — live snapshot for clients.
 * 4. The `game_command_queue` queue — commands from HTTP (move, reset, ...).
 *
 * =============================================================================
 * REST (orientation statement)
 * =============================================================================
 *
 * GET /api/status, /api/board, POST /api/move, POST /api/reset, GET /api/timer,
 * POST /api/demo/config and more — see implemented handlers in this file.
 *
 * =============================================================================
 * COMMUNICATION (QUEUES)
 * =============================================================================
 *
 * QUEUES - We send orders:
 * - game_command_queue -> Commands from the web (move, reset)
 *
 * API FUNCTION CALLS:
 * - game_get_status_json() -> Get JSON status
 * - game_get_board_json() -> Get board JSON
 * - game_get_history_json() -> Get JSON history
 *
 * =============================================================================
 * REST API ENDPOINTS
 * =============================================================================
 *
 * GET /api/status:
 * - JSON: {game_state, current_player, move_count, in_check, game_end,
 * error_state}
 *
 * GET /api/board:
 * - JSON: {board: [[...], ...]} // 8x8 array of pieces
 *
 * POST /api/move:
 * - Points: {from: "e2", to: "e4"}
 * - Response: {success: true/false, message: "..."}
 *
 * GET /api/timer:
 * - JSON: {white_time, black_time, running, paused}
 *
 * POST /api/demo/config:
 * - Points: {enabled: true, speed_ms: 2000}
 *
 * =============================================================================
 * CRITIC OF RULES
 * =============================================================================
 *
 * @warning WHAT NOT TO DO:
 *
 * 1. NEVER block in the HTTP handler!
 * ❌ vTaskDelay(1000);  // Blocks the HTTP server
 * ✅ Execute quickly, return an answer immediately
 *
 * 2. NEVER forget to send an HTTP response!
 * ❌ return ESP_OK;  // Without httpd_resp_send()
 * ✅ httpd_resp_send(req, json, strlen(json)); return ESP_OK;
 *
 * 3. ALWAYS check JSON buffer overflow!
 * Buffer is 8KB - if JSON is bigger, stack overflow!
 *
 * 4. ALWAYS use a mutex to access the game state!
 * API functions like game_get_status_json() already do that
 *
 * =============================================================================
 * TABLE OF CONTENTS
 * =============================================================================
 *
 * Section 1: WiFi Setup .......................... line 100
 * Section 2: HTTP Handlers ........................ line 300
 * Section 3: REST API Functions ................... line 800
 * Section 4: HTTP handlers and REST .................. (see file structure)
 * Section 5: Main Web Server Task .................. (see file structure)
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-23
 *
 * @note Task priority 2; stack due to large JSON buffers; AP IP when the hotspot is on, typically 192.168.4.1.
 *
 * @see game_task.c — Game JSON API
 */

#include "web_server_task.h"
#include "cmd_wire.h"
#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "web_routes.h"
#include "esp_http_server.h"
#include "mdns.h"
#endif
#include "transport_policy.h"
#include "web_server_internal.h"
#include "../game_hooks/include/game_state_notify.h"
#include "../game_task/include/game_task.h"
#include "../game_task/include/game_board_core.h"
#include "../squares_task/include/squares_task.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
// #include "../timer_system/include/timer_system.h" // UNUSED
#include "esp_event.h"
#include "board_api_auth.h"
#include "ota_update.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos_chess.h"
#include "../ble_task/include/ble_task.h"
#include "../config_manager/include/config_manager.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "shared_buffer_pool.h"
#include "cJSON.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Externi deklarace fronty
extern QueueHandle_t game_command_queue;
/** From main.c — comparison with xTaskGetCurrentTaskHandle() for TWDT reset only in this task. */
extern TaskHandle_t web_server_task_handle;

// ============================================================================
// LOKALNI PROMENNE A KONSTANTY
// ============================================================================

static const char *TAG = "WEB_SERVER_TASK";


// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Reset TWDT only if current task is `web_server_task`.
 *
 * `build_snapshot_json` and mutex waits are also called from `httpd` workers (GET
 * snapshot) – these are not in TWDT; `esp_task_wdt_reset()` then returns ESP_ERR_NOT_FOUND
 * and the task_wdt component spams the serial number ERROR lines.
 *
 * Returns ESP_OK even when skipped (foreign task or NULL handle).
 */
esp_err_t web_server_task_wdt_reset_safe(void) {
  if (web_server_task_handle == NULL) {
    return ESP_OK;
  }
  if (xTaskGetCurrentTaskHandle() != web_server_task_handle) {
    return ESP_OK;
  }

  esp_err_t ret = esp_task_wdt_reset();
  if (ret == ESP_ERR_NOT_FOUND) {
    ESP_LOGW(TAG,
             "WDT reset: web_server_task not subscribed to TWDT yet (startup)");
    return ESP_OK;
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }
  return ESP_OK;
}

// WiFi configuration — AP SSID: first neighborhood scan; first plate = base only, next = base_1 … base_N
#define WIFI_AP_SSID_BASE "ESP32-CzechMate"
/** Max. suffix index _N (0 = no suffix = first free "slot"). */
#define WIFI_AP_SSID_MAX_SLOTS 16
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS                                                \
  10 // Support for 6+ clients (ESP32-C6 can handle up to ~10-16)
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_GATEWAY "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"

// NVS konfigurace pro WiFi STA
#define WIFI_NVS_NAMESPACE "wifi_config"
#define WIFI_NVS_KEY_SSID "sta_ssid"
#define WIFI_NVS_KEY_PASSWORD "sta_password"
/** 0/1 — user enabled board hotspot (AP); default NVS missing → AP disabled. */
#define WIFI_NVS_KEY_AP_USER "ap_user_en"
/** CSV of blocked 3rd IPv4 octets for STA (eg `88` → reject x.x.88.x); BLE `wifi_sta_ip_block` or default from FW. */
#define WIFI_NVS_KEY_STA_BLK_OCT "sta_blk_oct"
/** After flash / key error in NVS — until the user saves otherwise via the application. */
#define WIFI_STA_BLK_OCT_DEFAULT_CSV "88"

// NVS konfigurace pro Web Lock
#define WEB_NVS_NAMESPACE "web_config"
#define WEB_NVS_KEY_LOCKED "locked"

// Konfigurace HTTP serveru
#define HTTP_SERVER_PORT 80
#define HTTP_SERVER_MAX_URI_LEN 512
#define HTTP_SERVER_MAX_HEADERS 8
#define HTTP_SERVER_MAX_CLIENTS 4

/** GET /api/timer — timer_get_json (name 32 + description 64 + number); there must not be 8 KiB on the stack. */

// Web server status monitoring
static bool task_running = false;
static bool web_server_active = false;
static bool wifi_ap_active = false;
static uint32_t web_server_start_time = 0;
static esp_err_t last_http_start_error = ESP_OK;
uint32_t client_count = 0; // External for UART commands

// Handle HTTP serveru
#if CONFIG_CHESS_ENABLE_WEB_SERVER
static httpd_handle_t httpd_handle = NULL;
#endif


// Handle netif
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

// STA status promenne
static bool sta_connected = false;
static bool sta_connecting =
    false; // Flag for tracking connection status (prevents race condition)
/** True if the user has explicitly disconnected the STA - disables automatic connection. */
static bool sta_manual_disconnect = false;
/** Prodleva pred dalsim pokusem o STA reconnect (backoff). Reset na 3 s pri GOT_IP. */
static uint32_t sta_reconnect_delay_ms = 3000;
#define STA_RECONNECT_DELAY_MIN_MS  3000
#define STA_RECONNECT_DELAY_MAX_MS  30000
static esp_timer_handle_t sta_reconnect_timer = NULL;
static bool web_locked = false; // Flag to lock the web interface
char sta_ip[16] = {0};          // External for UART commands
static char sta_ssid[33] = {0};
/** Skutecny AP SSID po startu (po skenu okolnich CzechMate AP). */
static char wifi_ap_ssid_effective[33] = {0};
static int last_disconnect_reason =
    0; // Posledni disconnection reason pro error handling

#define STA_BLK_OCT_MAX 16
static uint8_t s_sta_blk_oct[STA_BLK_OCT_MAX];
static size_t s_sta_blk_oct_n;
/** How many times in a row the DHCP address has been rejected (the router can always offer the same one). */
static unsigned s_sta_blk_reject_streak;

/* Brightness cache for GET /api/status - prevents NVS + CONFIG_MANAGER on each
 * request */
uint8_t cached_brightness = 100;
bool cached_brightness_valid = false;

// Externi promenne
QueueHandle_t web_server_status_queue = NULL;
QueueHandle_t web_server_command_queue = NULL;

// Extern funcions from main.c for Demo Mode
extern void toggle_demo_mode(bool enabled);
extern void set_demo_speed_ms(uint32_t speed_ms);
extern bool is_demo_mode_enabled(void);

/** Ping queue — build snapshot only in web_server_task (not in esp_timer: small stack). */
QueueHandle_t snapshot_notify_queue;

// ============================================================================
// PREDBEZNE DEKLARACE
// ============================================================================

static void wifi_select_ap_ssid_by_scan(void);
static esp_err_t wifi_init_apsta(void);
static void wifi_ap_user_apply_task(void *arg);
#if CONFIG_CHESS_ENABLE_WEB_SERVER
static void czechmate_mdns_refresh_sta_txt(void);
#endif
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void wifi_sta_parse_blocked_csv(const char *csv);
static void wifi_sta_refresh_blocked_octets_from_nvs(void);
static bool wifi_sta_third_octet_is_blocked(uint8_t oct);
static void wifi_sta_disconnect_if_current_ip_blocked(void);
static esp_err_t wifi_sta_save_blocked_octets_csv(const char *csv_in);
#if CONFIG_CHESS_ENABLE_WEB_SERVER
static esp_err_t start_http_server(void);
static void stop_http_server(void);
#endif

// static esp_err_t web_post_move_handler(httpd_req_t *req);  // VYPNUTO - web
// je 100% READ-ONLY

// Handler for Moves

// WiFi API handlers

// Handlers for the Demo API

// Handler for Virtual Actions

// Handlers for the MQTT API

// WiFi NVS function
esp_err_t wifi_load_config_from_nvs(char *ssid, size_t ssid_len, char *password,
                                    size_t password_len) {
  if (ssid == NULL || password == NULL || ssid_len == 0 || password_len == 0) {
    ESP_LOGE(TAG, "Invalid parameters for wifi_load_config_from_nvs");
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGD(TAG, "WiFi NVS namespace missing (no STA credentials saved yet)");
    } else {
      ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    }
    return ret;
  }

  // Nazi SSID
  size_t required_size = ssid_len;
  ret = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid, &required_size);
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGD(TAG, "WiFi SSID not in NVS");
    } else {
      ESP_LOGE(TAG, "Failed to get SSID from NVS: %s", esp_err_to_name(ret));
    }
    nvs_close(nvs_handle);
    return ret;
  }

  // Nazi password
  required_size = password_len;
  ret =
      nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password, &required_size);
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGD(TAG, "WiFi password not in NVS");
    } else {
      ESP_LOGE(TAG, "Failed to get password from NVS: %s", esp_err_to_name(ret));
    }
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "WiFi config loaded from NVS: SSID=%s", ssid);

  return ESP_OK;
}

// WiFi STA function (static for internal use)

// Gettery pro externi pouziti
esp_err_t wifi_get_sta_ip(char *buffer, size_t max_len) {
  if (buffer == NULL || max_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  strncpy(buffer, sta_ip, max_len - 1);
  buffer[max_len - 1] = '\0';
  return ESP_OK;
}

esp_err_t wifi_get_sta_ssid(char *buffer, size_t max_len) {
  if (buffer == NULL || max_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  strncpy(buffer, sta_ssid, max_len - 1);
  buffer[max_len - 1] = '\0';
  return ESP_OK;
}

bool wifi_ap_is_broadcasting(void) { return wifi_ap_active; }

const char *wifi_ap_effective_ssid(void) {
  if (wifi_ap_ssid_effective[0] == '\0') {
    return WIFI_AP_SSID_BASE;
  }
  return wifi_ap_ssid_effective;
}

// External WiFi function (for UART commands)
esp_err_t wifi_save_config_to_nvs(const char *ssid, const char *password) {
  if (ssid == NULL || password == NULL) {
    ESP_LOGE(TAG, "Invalid parameters: ssid or password is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  size_t ssid_len = strlen(ssid);
  size_t password_len = strlen(password);

  if (ssid_len == 0 || ssid_len > 32) {
    ESP_LOGE(TAG, "Invalid SSID length: %zu (must be 1-32)", ssid_len);
    return ESP_ERR_INVALID_ARG;
  }

  if (password_len > 64) {
    ESP_LOGE(TAG, "Invalid password length: %zu (must be 0-64)", password_len);
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = nvs_set_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set SSID in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set password in NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "WiFi config saved to NVS: SSID=%s", ssid);

  // If it is connected and the SSID has changed, disconnect
  if (sta_connected && strcmp(sta_ssid, ssid) != 0) {
    ESP_LOGI(TAG, "SSID changed from '%s' to '%s', disconnecting...", sta_ssid,
             ssid);
    wifi_disconnect_sta();
  }

  return ESP_OK;
}

esp_err_t wifi_connect_sta(void) {
  /* V2 lazy WiFi: the radio may have been off at boot. Start it now before
   * attempting a connection. */
  transport_policy_ensure_wifi_started();

  // Check: if already connected, return success
  if (sta_connected) {
    ESP_LOGI(TAG, "Already connected to WiFi: %s (IP: %s)", sta_ssid, sta_ip);
    return ESP_OK;
  }

  // Check: if it is actually connecting, return an error (race condition
  // protection)
  if (sta_connecting) {
    ESP_LOGW(TAG, "WiFi connection already in progress");
    return ESP_ERR_INVALID_STATE;
  }

  char ssid[33] = {0};
  char password[65] = {0};

  // Clear configuration from NVS
  esp_err_t ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));
  if (ret != ESP_OK) {
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
      ESP_LOGI(TAG, "WiFi STA: no credentials in NVS (use AP or BLE to configure)");
    } else {
      ESP_LOGE(TAG, "Failed to load WiFi config from NVS: %s",
               esp_err_to_name(ret));
    }
    return ret;
  }

  ESP_LOGI(TAG, "Connecting to WiFi: SSID=%s", ssid);
  sta_manual_disconnect = false; // Povolit automaticke prepojeni pri ztrate
  sta_connecting = true;         // Nastavit flag pripojovani

  // Nastavit WiFi STA konfiguraci
  wifi_config_t wifi_config = {0};
  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, password,
          sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;

  ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi STA config: %s", esp_err_to_name(ret));
    sta_connecting = false;
    return ret;
  }

  // Start the connection
  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
    sta_connecting = false;
    return ret;
  }

  // Wait for connection (max 30 seconds)
  int retry_count = 0;
  const int max_retries = 30; // 30 sekund (1 sekunda per retry)

  while (retry_count < max_retries) {
    // Reset WDT to prevent timeout (the function can block up to 30
    // sekund)
    web_server_task_wdt_reset_safe();

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (sta_connected) {
      ESP_LOGI(TAG, "WiFi connected successfully! IP: %s", sta_ip);
      sta_connecting = false; // Resetovat flag
      return ESP_OK;
    }

    retry_count++;
    if (retry_count % 5 == 0) {
      ESP_LOGI(TAG, "Waiting for WiFi connection... (%d/%d)", retry_count,
               max_retries);
    }
  }

  ESP_LOGE(TAG, "WiFi connection timeout after %d seconds", max_retries);
  /* STA_DISCONNECTED arms sta_reconnect_timer while this flag is false and
   * that keeps the driver's scan list. Hold reconnect off until the next
   * explicit connect (this function, or the AP toggle path). */
  sta_manual_disconnect = true;
  sta_reconnect_delay_ms = STA_RECONNECT_DELAY_MIN_MS;
  if (sta_reconnect_timer != NULL) {
    (void)esp_timer_stop(sta_reconnect_timer);
  }
  (void)esp_wifi_disconnect();
  (void)esp_wifi_clear_ap_list();
  sta_connecting = false;

  // Vratit specificky error code podle disconnection reason
  // WIFI_REASON_NO_AP_FOUND = 201
  // WIFI_REASON_AUTH_FAIL = 202
  // WIFI_REASON_ASSOC_FAIL = 203
  // WIFI_REASON_HANDSHAKE_TIMEOUT = 204
  if (last_disconnect_reason == 201) {
    return ESP_ERR_NOT_FOUND; // Network not found
  } else if (last_disconnect_reason == 202 || last_disconnect_reason == 203 ||
             last_disconnect_reason == 204) {
    return ESP_ERR_INVALID_RESPONSE; // Authentication/association failed (wrong
                                     // password)
  }

  return ESP_ERR_TIMEOUT; // General timeout
}

esp_err_t wifi_disconnect_sta(void) {
  // If not connected and not connecting, return success
  if (!sta_connected && !sta_connecting) {
    ESP_LOGI(TAG, "WiFi already disconnected");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Disconnecting from WiFi...");

  sta_manual_disconnect = true;
  if (sta_reconnect_timer != NULL) {
    esp_timer_stop(sta_reconnect_timer); // Zrusit naplanovany reconnect
  }
  if (sta_connecting) {
    ESP_LOGW(TAG, "Cancelling WiFi connection in progress");
    sta_connecting = false;
  }

  esp_err_t ret = esp_wifi_disconnect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to disconnect WiFi: %s", esp_err_to_name(ret));
    return ret;
  }

  // Cekat na odpojeni (max 5 sekund)
  int retry_count = 0;
  const int max_retries = 5;

  while (retry_count < max_retries && sta_connected) {
    // Resetovat WDT aby se zabranilo timeoutu
    web_server_task_wdt_reset_safe();
    vTaskDelay(pdMS_TO_TICKS(1000));
    retry_count++;
  }

  if (!sta_connected) {
    ESP_LOGI(TAG, "WiFi disconnected successfully");
  } else {
    ESP_LOGW(TAG, "WiFi disconnect timeout, but continuing");
  }

  return ESP_OK;
}

esp_err_t wifi_clear_config_from_nvs(void) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = nvs_erase_key(nvs_handle, WIFI_NVS_KEY_SSID);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Failed to erase SSID: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_erase_key(nvs_handle, WIFI_NVS_KEY_PASSWORD);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "Failed to erase password: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "WiFi config cleared from NVS");

  return ESP_OK;
}

static bool wifi_ap_user_desired_from_nvs(void) {
  nvs_handle_t h;
  uint8_t v = 0;
  esp_err_t r = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &h);
  if (r != ESP_OK) {
    return false;
  }
  r = nvs_get_u8(h, WIFI_NVS_KEY_AP_USER, &v);
  nvs_close(h);
  if (r != ESP_OK) {
    return false;
  }
  return v != 0;
}

static esp_err_t wifi_ap_user_persist_desired(bool en) {
  nvs_handle_t h;
  esp_err_t r = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &h);
  if (r != ESP_OK) {
    return r;
  }
  r = nvs_set_u8(h, WIFI_NVS_KEY_AP_USER, en ? 1U : 0U);
  if (r == ESP_OK) {
    r = nvs_commit(h);
  }
  nvs_close(h);
  return r;
}

typedef struct {
  bool enable;
} wifi_ap_toggle_msg_t;

static void wifi_restore_sta_connect_async(void) {
  char ssid[33] = {0};
  char password[65] = {0};
  if (wifi_load_config_from_nvs(ssid, sizeof(ssid), password,
                                sizeof(password)) != ESP_OK ||
      ssid[0] == '\0') {
    return;
  }
  sta_manual_disconnect = false;
  sta_connecting = true;
  wifi_config_t wcfg = {0};
  strncpy((char *)wcfg.sta.ssid, ssid, sizeof(wcfg.sta.ssid) - 1);
  strncpy((char *)wcfg.sta.password, password, sizeof(wcfg.sta.password) - 1);
  wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wcfg.sta.pmf_cfg.capable = true;
  wcfg.sta.pmf_cfg.required = false;
  if (esp_wifi_set_config(WIFI_IF_STA, &wcfg) != ESP_OK) {
    sta_connecting = false;
    return;
  }
  if (esp_wifi_connect() != ESP_OK) {
    sta_connecting = false;
  }
}

static void wifi_ap_user_apply_task(void *arg) {
  wifi_ap_toggle_msg_t *m = (wifi_ap_toggle_msg_t *)arg;
  if (m == NULL) {
    vTaskDelete(NULL);
    return;
  }
  const bool en = m->enable;
  free(m);

  (void)wifi_ap_user_persist_desired(en);

  esp_err_t er = esp_wifi_stop();
  if (er != ESP_OK && er != ESP_ERR_WIFI_NOT_INIT) {
    ESP_LOGW(TAG, "wifi_ap_set: esp_wifi_stop → %s", esp_err_to_name(er));
  }
  vTaskDelay(pdMS_TO_TICKS(100));

  er = esp_wifi_set_mode(en ? WIFI_MODE_APSTA : WIFI_MODE_STA);
  if (er != ESP_OK) {
    ESP_LOGE(TAG, "wifi_ap_set: esp_wifi_set_mode → %s", esp_err_to_name(er));
    ble_task_push_network_info();
    vTaskDelete(NULL);
    return;
  }

  er = esp_wifi_start();
  if (er != ESP_OK) {
    ESP_LOGE(TAG, "wifi_ap_set: esp_wifi_start → %s", esp_err_to_name(er));
    ble_task_push_network_info();
    vTaskDelete(NULL);
    return;
  }

  wifi_ap_active = en;
  ESP_LOGI(TAG, "wifi_ap_set: AP broadcasting=%s", en ? "true" : "false");
  czechmate_mdns_refresh_sta_txt();
  ble_task_push_network_info();
  wifi_restore_sta_connect_async();

  vTaskDelete(NULL);
}

/** Message for asynchronous STA provisioning from BLE (wifi_connect_sta can take up to ~30s). */
typedef struct {
  char ssid[33];
  char password[65];
} wifi_ble_prov_msg_t;

static void wifi_ble_prov_task(void *arg) {
  wifi_ble_prov_msg_t *m = (wifi_ble_prov_msg_t *)arg;
  const char ack_cmd[] = "{\"cmd\":\"wifi_sta_config\"}";

  if (m == NULL) {
    vTaskDelete(NULL);
    return;
  }

  ESP_LOGI(TAG, "BLE wifi_sta_config: task start SSID=%s", m->ssid);
  esp_err_t err = wifi_save_config_to_nvs(m->ssid, m->password);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "BLE wifi_sta_config: NVS save failed %s", esp_err_to_name(err));
    ble_task_notify_command_result(err, ack_cmd);
    free(m);
    vTaskDelete(NULL);
    return;
  }

  err = wifi_connect_sta();
  ble_task_push_network_info();
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "BLE wifi_sta_config: STA connected");
  } else {
    ESP_LOGW(TAG, "BLE wifi_sta_config: connect failed %s", esp_err_to_name(err));
  }
  ble_task_notify_command_result(err, ack_cmd);
  free(m);
  vTaskDelete(NULL);
}

typedef struct {
  uint32_t id;
  bool has_id;
} survey_ack_meta_t;

/** BLE wifi_survey: active scan of the surroundings, result via cmd_ack JSON (encrypted link). */
static void wifi_ble_survey_task(void *arg) {
  survey_ack_meta_t meta = {0};
  if (arg != NULL) {
    meta = *(survey_ack_meta_t *)arg;
    free(arg);
  }

  typedef struct {
    char ssid[33];
    int8_t rssi;
  } survey_best_t;

  enum {
    kSurveyRecordCap = 64,
    kSurveyOutCap = 24,
  };

  const size_t rec_bytes =
      (size_t)kSurveyRecordCap * sizeof(wifi_ap_record_t);
  wifi_ap_record_t *records =
      (wifi_ap_record_t *)get_shared_buffer(rec_bytes);
  if (records == NULL) {
    cmd_wire_notify_with_id(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"pool_busy\","
        "\"cmd\":\"wifi_survey\",\"message\":\"buffer_pool\"}",
        meta.id, meta.has_id);
    vTaskDelete(NULL);
    return;
  }
  uint16_t number = kSurveyRecordCap;
  memset(records, 0, rec_bytes);

  wifi_scan_config_t scan_cfg = {0};
  scan_cfg.show_hidden = true;

  transport_policy_ensure_wifi_started();

  esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
  if (err != ESP_OK) {
    char buf[288];
    snprintf(buf, sizeof(buf),
             "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"scan_failed\","
             "\"cmd\":\"wifi_survey\",\"message\":\"scan_start\",\"esp\":%d}",
             (int)err);
    cmd_wire_notify_with_id(buf, meta.id, meta.has_id);
    (void)release_shared_buffer((char *)records);
    vTaskDelete(NULL);
    return;
  }

  err = esp_wifi_scan_get_ap_records(&number, records);
  if (err != ESP_OK) {
    char buf[288];
    snprintf(buf, sizeof(buf),
             "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"scan_failed\","
             "\"cmd\":\"wifi_survey\",\"message\":\"get_ap_records\",\"esp\":%d}",
             (int)err);
    cmd_wire_notify_with_id(buf, meta.id, meta.has_id);
    (void)release_shared_buffer((char *)records);
    vTaskDelete(NULL);
    return;
  }

  survey_best_t uniq[kSurveyRecordCap];
  int nu = 0;

  for (uint16_t i = 0; i < number; i++) {
    const uint8_t *raw = records[i].ssid;
    size_t slen = strnlen((const char *)raw, sizeof(records[i].ssid));
    if (slen == 0) {
      continue;
    }
    char key[33];
    memcpy(key, raw, slen);
    key[slen] = '\0';

    int idx = -1;
    for (int j = 0; j < nu; j++) {
      if (strcmp(uniq[j].ssid, key) == 0) {
        idx = j;
        break;
      }
    }
    if (idx >= 0) {
      if (records[i].rssi > uniq[idx].rssi) {
        uniq[idx].rssi = records[i].rssi;
      }
    } else if (nu < (int)kSurveyRecordCap) {
      strncpy(uniq[nu].ssid, key, sizeof(uniq[nu].ssid) - 1);
      uniq[nu].ssid[sizeof(uniq[nu].ssid) - 1] = '\0';
      uniq[nu].rssi = records[i].rssi;
      nu++;
    }
  }

  for (int a = 0; a < nu - 1; a++) {
    for (int b = a + 1; b < nu; b++) {
      if (uniq[b].rssi > uniq[a].rssi) {
        survey_best_t tmp = uniq[a];
        uniq[a] = uniq[b];
        uniq[b] = tmp;
      }
    }
  }

  const int out_n = nu < (int)kSurveyOutCap ? nu : (int)kSurveyOutCap;

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    cmd_wire_notify_with_id(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"root\",\"esp\":-1}",
        meta.id, meta.has_id);
    (void)release_shared_buffer((char *)records);
    vTaskDelete(NULL);
    return;
  }
  cJSON_AddStringToObject(root, "channel", "cmd_ack");
  cJSON_AddBoolToObject(root, "ok", true);
  cJSON_AddStringToObject(root, "code", "ok");
  cJSON_AddStringToObject(root, "cmd", "wifi_survey");
  cJSON *arr = cJSON_CreateArray();
  if (arr == NULL) {
    cJSON_Delete(root);
    cmd_wire_notify_with_id(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"array\",\"esp\":-1}",
        meta.id, meta.has_id);
    (void)release_shared_buffer((char *)records);
    vTaskDelete(NULL);
    return;
  }
  for (int i = 0; i < out_n; i++) {
    cJSON *o = cJSON_CreateObject();
    if (o == NULL) {
      continue;
    }
    cJSON_AddStringToObject(o, "ssid", uniq[i].ssid);
    cJSON_AddNumberToObject(o, "rssi", uniq[i].rssi);
    cJSON_AddItemToArray(arr, o);
  }
  cJSON_AddItemToObject(root, "networks", arr);

  char *printed = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (printed == NULL) {
    cmd_wire_notify_with_id(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"print\",\"esp\":-1}",
        meta.id, meta.has_id);
    (void)release_shared_buffer((char *)records);
    vTaskDelete(NULL);
    return;
  }
  cmd_wire_notify_with_id(printed, meta.id, meta.has_id);
  cJSON_free(printed);
  (void)release_shared_buffer((char *)records);
  vTaskDelete(NULL);
}

bool wifi_is_sta_connected(void) { return sta_connected; }

// ============================================================================
// WEB LOCK NVS FUNCTION
// ============================================================================

/**
 * @brief Save lock state to NVS
 *
 * @param locked True for lock, false for unlock
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t web_lock_save_to_nvs(bool locked) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WEB_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  uint8_t locked_value = locked ? 1 : 0;
  ret = nvs_set_blob(nvs_handle, WEB_NVS_KEY_LOCKED, &locked_value,
                     sizeof(locked_value));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set web lock: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "Web lock saved to NVS: %s", locked ? "locked" : "unlocked");

  return ESP_OK;
}

/**
 * @brief Nacte lock status from NVS
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_lock_load_from_nvs(void) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WEB_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    // If NVS namespace does not exist, use default (unlocked)
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
      web_locked = false;
      ESP_LOGI(TAG,
               "Web lock NVS namespace not found, using default: unlocked");
      return ESP_OK;
    }
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  uint8_t locked_value = 0;
  size_t required_size = sizeof(locked_value);
  ret = nvs_get_blob(nvs_handle, WEB_NVS_KEY_LOCKED, &locked_value,
                     &required_size);
  if (ret == ESP_ERR_NVS_NOT_FOUND) {
    // Key does not exist, use default (unlocked)
    web_locked = false;
    ESP_LOGI(TAG, "Web lock key not found, using default: unlocked");
    nvs_close(nvs_handle);
    return ESP_OK;
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get web lock: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  web_locked = (locked_value != 0);
  nvs_close(nvs_handle);
  ESP_LOGI(TAG, "Web lock loaded from NVS: %s",
           web_locked ? "locked" : "unlocked");

  return ESP_OK;
}

/**
 * @brief Determine if the web interface is locked
 *
 * @return true if locked, false if unlocked
 */
bool web_is_locked(void) { return web_locked; }

/**
 * @brief Set lock state and save to NVS
 *
 * @param locked True for lock, false for unlock
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_lock_set(bool locked) {
  web_locked = locked;
  esp_err_t ret = web_lock_save_to_nvs(locked);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Web interface %s", locked ? "locked" : "unlocked");
  }
  return ret;
}

// ============================================================================
// WIFI APSTA SETUP
// ============================================================================

/** Forward declaration for STA reconnect timer callback (defined below). */
static void sta_reconnect_timer_cb(void *arg);

#define WIFI_AP_SCAN_MAX_APS 64

/**
 * Return true if the SSID matches our AP convention (base | base_N) and set
 * out_slot: 0 = base only, 1..N = base_N.
 */
static bool wifi_ap_ssid_matches_czechmate(const uint8_t *ssid_raw,
                                           size_t ssid_len, int *out_slot) {
  const char *base = WIFI_AP_SSID_BASE;
  const size_t bl = strlen(base);
  *out_slot = -1;
  if (ssid_len < bl) {
    return false;
  }
  if (memcmp(ssid_raw, base, bl) != 0) {
    return false;
  }
  if (ssid_len == bl) {
    *out_slot = 0;
    return true;
  }
  if (ssid_raw[bl] != (uint8_t)'_') {
    return false;
  }
  unsigned n = 0;
  for (size_t i = bl + 1; i < ssid_len; i++) {
    uint8_t c = ssid_raw[i];
    if (c < (uint8_t)'0' || c > (uint8_t)'9') {
      return false;
    }
    n = n * 10u + (unsigned)(c - (uint8_t)'0');
  }
  if (n < 1u || n > (unsigned)WIFI_AP_SSID_MAX_SLOTS) {
    return false;
  }
  *out_slot = (int)n;
  return true;
}

/**
 * @brief One-time STA scan: selects the lowest free SSID (first board without
 * suffixes, another _1 ... _N) according to what has already been broadcast in the area.
 *
 * Upon return, WiFi is stopped; caller set APSTA and restart.
 */
static void wifi_select_ap_ssid_by_scan(void) {
  const char *base = WIFI_AP_SSID_BASE;
  strncpy(wifi_ap_ssid_effective, base, sizeof(wifi_ap_ssid_effective) - 1);
  wifi_ap_ssid_effective[sizeof(wifi_ap_ssid_effective) - 1] = '\0';

  esp_err_t ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "AP SSID scan: set STA mode failed: %s — using %s",
             esp_err_to_name(ret), base);
    return;
  }

  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "AP SSID scan: wifi_start failed: %s — using %s",
             esp_err_to_name(ret), base);
    return;
  }

  /* Rozptyl soubezneho startu vice desek (sniz sanci stejneho volneho slotu). */
  vTaskDelay(pdMS_TO_TICKS(50 + (esp_random() % 450)));
  (void)web_server_task_wdt_reset_safe();

  wifi_scan_config_t scan_cfg = {0};
  scan_cfg.show_hidden = true;

  ret = esp_wifi_scan_start(&scan_cfg, true);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "AP SSID scan: scan_start failed: %s — using %s",
             esp_err_to_name(ret), base);
    esp_wifi_stop();
    return;
  }

  const size_t ap_rec_bytes =
      (size_t)WIFI_AP_SCAN_MAX_APS * sizeof(wifi_ap_record_t);
  wifi_ap_record_t *ap_records =
      (wifi_ap_record_t *)get_shared_buffer(ap_rec_bytes);
  if (ap_records == NULL) {
    ESP_LOGW(TAG, "AP SSID scan: buffer pool busy — using %s", base);
    esp_wifi_stop();
    return;
  }
  memset(ap_records, 0, ap_rec_bytes);
  uint16_t number = WIFI_AP_SCAN_MAX_APS;
  ret = esp_wifi_scan_get_ap_records(&number, ap_records);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "AP SSID scan: get_ap_records failed: %s — using %s",
             esp_err_to_name(ret), base);
    (void)release_shared_buffer((char *)ap_records);
    esp_wifi_stop();
    return;
  }

  bool taken[WIFI_AP_SSID_MAX_SLOTS + 1];
  memset(taken, 0, sizeof(taken));

  for (unsigned i = 0; i < number; i++) {
    const uint8_t *s = ap_records[i].ssid;
    size_t slen = strnlen((const char *)s, sizeof(ap_records[i].ssid));
    int slot = -1;
    if (wifi_ap_ssid_matches_czechmate(s, slen, &slot) && slot >= 0 &&
        slot <= WIFI_AP_SSID_MAX_SLOTS) {
      taken[slot] = true;
    }
  }

  int chosen = -1;
  for (int k = 0; k <= WIFI_AP_SSID_MAX_SLOTS; k++) {
    if (!taken[k]) {
      chosen = k;
      break;
    }
  }
  if (chosen < 0) {
    ESP_LOGW(TAG, "AP SSID: all slots 0..%d occupied — use %s",
             WIFI_AP_SSID_MAX_SLOTS, base);
    chosen = 0;
  }

  if (chosen == 0) {
    strncpy(wifi_ap_ssid_effective, base, sizeof(wifi_ap_ssid_effective) - 1);
  } else {
    int n = snprintf(wifi_ap_ssid_effective, sizeof(wifi_ap_ssid_effective),
                     "%s_%d", base, chosen);
    if (n < 0 || n >= (int)sizeof(wifi_ap_ssid_effective)) {
      strncpy(wifi_ap_ssid_effective, base, sizeof(wifi_ap_ssid_effective) - 1);
      ESP_LOGW(TAG, "AP SSID: snprintf overflow — fallback %s", base);
    }
  }
  wifi_ap_ssid_effective[sizeof(wifi_ap_ssid_effective) - 1] = '\0';

  ESP_LOGI(TAG, "AP SSID po skenu okoli: %s (zaklad=%s)", wifi_ap_ssid_effective,
           base);

  (void)release_shared_buffer((char *)ap_records);
  esp_wifi_stop();
}

/**
 * @brief Initializes WiFi Access Point and Station (APSTA)
 *
 * This function initializes WiFi in APSTA mode - at the same time as an Access Point
 * for a web server and as a Station for connecting to the Internet.
 *
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * The function initializes netif, event loop, WiFi and sets AP and STA configuration.
 * Registers an event handler for WiFi events and starts the Access Point.
 * The station interface is ready for connection to a WiFi network.
 */
static esp_err_t wifi_init_apsta(void) {
  ESP_LOGI(TAG, "Initializing WiFi APSTA...");

  // Important: Initialize netif BEFORE creating default netif
  ESP_LOGI(TAG, "Initializing netif...");
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
    return ret;
  }
  if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Netif already initialized");
  }

  // Important: Create a default event loop BEFORE WiFi initialization
  ESP_LOGI(TAG, "Creating default event loop...");
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to create default event loop: %s",
             esp_err_to_name(ret));
    return ret;
  }
  if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Event loop already created");
  } else {
    ESP_LOGI(TAG, "Event loop ready");
  }

  /* On the ESP32-C6 (and other RISC-V), the internal driver order may depend on
   * which default netif will be created first. Let's try STA before AP (reduce Load access
   * fault in ieee80211_hostap_attach at esp_wifi_start()). */
  ESP_LOGI(TAG, "Creating default WiFi STA netif...");
  sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create STA netif");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Creating default WiFi AP netif...");
  ap_netif = esp_netif_create_default_wifi_ap();
  if (ap_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create AP netif");
    return ESP_FAIL;
  }

  // Initialize WiFi with default configuration
  ESP_LOGI(TAG, "Initializing WiFi...");
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Registrovat WiFi event handler
  ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            &wifi_event_handler, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi event handler: %s",
             esp_err_to_name(ret));
    return ret;
  }

  // Register IP event handler for STA
  ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &wifi_event_handler, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IP event handler: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_LOST_IP,
                                            &wifi_event_handler, NULL, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IP lost event handler: %s",
             esp_err_to_name(ret));
    return ret;
  }

  const esp_timer_create_args_t reconnect_timer_args = {
      .callback = &sta_reconnect_timer_cb,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "sta_reconnect",
      .skip_unhandled_events = true,
  };
  ret = esp_timer_create(&reconnect_timer_args, &sta_reconnect_timer);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "STA reconnect timer create failed: %s (auto-reconnect disabled)", esp_err_to_name(ret));
    sta_reconnect_timer = NULL;
  }

  wifi_select_ap_ssid_by_scan();

  wifi_config_t wifi_config = {0};
  strncpy((char *)wifi_config.ap.ssid, wifi_ap_ssid_effective,
          sizeof(wifi_config.ap.ssid) - 1);
  wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';
  wifi_config.ap.ssid_len =
      (uint8_t)strlen((const char *)wifi_config.ap.ssid);
  strncpy((char *)wifi_config.ap.password, WIFI_AP_PASSWORD,
          sizeof(wifi_config.ap.password) - 1);
  wifi_config.ap.channel = WIFI_AP_CHANNEL;
  wifi_config.ap.max_connection = WIFI_AP_MAX_CONNECTIONS;
  wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

  const bool ap_on_boot = wifi_ap_user_desired_from_nvs();
  const bool wifi_on_boot = transport_policy_should_enable_wifi_on_boot();
  ret = esp_wifi_set_mode(ap_on_boot ? WIFI_MODE_APSTA : WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
    return ret;
  }

  /* STA-only: setting AP configuration would return ESP_ERR_WIFI_MODE — AP interface does not exist. */
  if (ap_on_boot) {
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set WiFi AP config: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  /* V2 policy: keep WiFi radio off at boot if there are no credentials and
   * no user-enabled hotspot. BLE advertises immediately; WiFi starts lazily
   * when the app provisions credentials over BLE. */
  if (wifi_on_boot) {
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
      return ret;
    }
    wifi_ap_active = ap_on_boot;
  } else {
    wifi_ap_active = false;
    ESP_LOGI(TAG, "WiFi radio held off at boot (no STA credentials, no AP request); "
                  "will start on BLE provisioning");
  }

  ESP_LOGI(TAG, "WiFi initialized (%s)", ap_on_boot ? "AP+STA" : "STA only");
  ESP_LOGI(TAG, "AP user hotspot: %s", ap_on_boot ? "ON" : "OFF (default)");
  ESP_LOGI(TAG, "AP SSID (when enabled): %s", wifi_ap_ssid_effective);
  ESP_LOGI(TAG, "AP Password: %s", WIFI_AP_PASSWORD);
  ESP_LOGI(TAG, "AP IP (when enabled): %s", WIFI_AP_IP);
  ESP_LOGI(TAG, "STA interface ready for connection");

  wifi_sta_refresh_blocked_octets_from_nvs();

  return ESP_OK;
}

/**
 * @brief Callback of one-time timer for automatic STA connection.
 * Called after WIFI_EVENT_STA_DISCONNECTED with backoff; just run esp_wifi_connect().
 */
static void sta_reconnect_timer_cb(void *arg) {
  (void)arg;
  if (sta_manual_disconnect) {
    return;
  }
  char ssid[33] = {0};
  char password[65] = {0};
  if (wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password)) != ESP_OK || ssid[0] == '\0') {
    return;
  }
  wifi_config_t wifi_config = {0};
  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.pmf_cfg.capable = true;
  wifi_config.sta.pmf_cfg.required = false;
  if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) == ESP_OK && esp_wifi_connect() == ESP_OK) {
    sta_connecting = true;
    ESP_LOGI(TAG, "STA reconnect scheduled (backoff %lu ms)", (unsigned long)sta_reconnect_delay_ms);
  }
}

static void wifi_sta_parse_blocked_csv(const char *csv) {
  s_sta_blk_oct_n = 0;
  if (csv == NULL) {
    return;
  }
  while (*csv != '\0' && s_sta_blk_oct_n < STA_BLK_OCT_MAX) {
    while (*csv == ' ' || *csv == '\t' || *csv == ',') {
      csv++;
    }
    if (*csv == '\0') {
      break;
    }
    int val = 0;
    int digits = 0;
    while (*csv >= '0' && *csv <= '9' && digits < 3) {
      val = val * 10 + (*csv - '0');
      csv++;
      digits++;
    }
    if (digits > 0 && val >= 0 && val <= 255) {
      s_sta_blk_oct[s_sta_blk_oct_n++] = (uint8_t)val;
    }
    while (*csv != '\0' && *csv != ',') {
      csv++;
    }
  }
}

static void wifi_sta_refresh_blocked_octets_from_nvs(void) {
  s_sta_blk_oct_n = 0;
  nvs_handle_t nh;
  esp_err_t r = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nh);
  if (r != ESP_OK) {
    wifi_sta_parse_blocked_csv(WIFI_STA_BLK_OCT_DEFAULT_CSV);
    ESP_LOGI(TAG, "STA DHCP blocklist: default %s (no NVS ns)",
             WIFI_STA_BLK_OCT_DEFAULT_CSV);
    return;
  }
  char buf[96];
  size_t sz = sizeof(buf);
  r = nvs_get_str(nh, WIFI_NVS_KEY_STA_BLK_OCT, buf, &sz);
  nvs_close(nh);
  if (r == ESP_ERR_NVS_NOT_FOUND) {
    wifi_sta_parse_blocked_csv(WIFI_STA_BLK_OCT_DEFAULT_CSV);
    ESP_LOGI(TAG, "STA DHCP blocklist: default %s (NVS unset)",
             WIFI_STA_BLK_OCT_DEFAULT_CSV);
    return;
  }
  if (r != ESP_OK) {
    wifi_sta_parse_blocked_csv(WIFI_STA_BLK_OCT_DEFAULT_CSV);
    ESP_LOGW(TAG, "STA DHCP blocklist: NVS read %s — default %s",
             esp_err_to_name(r), WIFI_STA_BLK_OCT_DEFAULT_CSV);
    return;
  }
  wifi_sta_parse_blocked_csv(buf);
  ESP_LOGI(TAG, "STA DHCP blocklist: %u third-octet value(s)",
           (unsigned)s_sta_blk_oct_n);
}

static bool wifi_sta_third_octet_is_blocked(uint8_t oct) {
  for (size_t i = 0; i < s_sta_blk_oct_n; i++) {
    if (s_sta_blk_oct[i] == oct) {
      return true;
    }
  }
  return false;
}

static void wifi_sta_disconnect_if_current_ip_blocked(void) {
  if (!sta_connected || sta_ip[0] == '\0' || s_sta_blk_oct_n == 0) {
    return;
  }
  unsigned a = 0, b = 0, c = 0, d = 0;
  if (sscanf(sta_ip, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
    return;
  }
  if (c > 255U) {
    return;
  }
  if (wifi_sta_third_octet_is_blocked((uint8_t)c)) {
    ESP_LOGW(TAG,
             "STA: IP %s matches DHCP blocklist — disconnect for new lease",
             sta_ip);
    sta_connected = false;
    sta_connecting = false;
    sta_ip[0] = '\0';
    esp_wifi_disconnect();
  }
}

static esp_err_t wifi_sta_save_blocked_octets_csv(const char *csv_in) {
  const char *csv = (csv_in != NULL) ? csv_in : "";
  while (*csv == ' ' || *csv == '\t') {
    csv++;
  }
  nvs_handle_t h;
  esp_err_t r = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &h);
  if (r != ESP_OK) {
    return r;
  }
  /* Empty string = disable filtering (key must exist; NOT_FOUND = default FW). */
  if (*csv == '\0') {
    r = nvs_set_str(h, WIFI_NVS_KEY_STA_BLK_OCT, "");
    if (r != ESP_OK) {
      nvs_close(h);
      return r;
    }
    r = nvs_commit(h);
    nvs_close(h);
    return (r == ESP_OK) ? ESP_OK : r;
  }
  size_t len = strlen(csv);
  if (len > 80U) {
    nvs_close(h);
    return ESP_ERR_INVALID_ARG;
  }
  for (size_t i = 0; i < len; i++) {
    unsigned char ch = (unsigned char)csv[i];
    if (!(isdigit((int)ch) || ch == ',' || ch == ' ')) {
      nvs_close(h);
      return ESP_ERR_INVALID_ARG;
    }
  }
  r = nvs_set_str(h, WIFI_NVS_KEY_STA_BLK_OCT, csv);
  if (r != ESP_OK) {
    nvs_close(h);
    return r;
  }
  r = nvs_commit(h);
  nvs_close(h);
  return r;
}

/**
 * @brief Handles WiFi events
 *
 * This function handles WiFi events for both Access Point and Station.
 * Monitors the connection and disconnection of the client to the AP and the status of the STA connection.
 *
 * @param arg Argument (not used)
 * @param event_base Event type (WIFI_EVENT or IP_EVENT)
 * @param event_id The ID of the event
 * @param event_data Event data
 *
 * @details
 * The function tracks:
 * - Connecting and disconnecting the client to the WiFi hotspot (AP)
 * - Connecting and disconnecting the STA to the WiFi network
 * - Get IP address for STA
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  // AP events - connection of the client to the hotspot
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
    wifi_event_ap_staconnected_t *event =
        (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "AP: Station connected, AID=%d", event->aid);
    client_count++;
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_AP_STADISCONNECTED) {
    wifi_event_ap_stadisconnected_t *event =
        (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "AP: Station disconnected, AID=%d", event->aid);
    if (client_count > 0) {
      client_count--;
    }
    web_game_on_client_drop();
  }
  // STA events - connected to a WiFi network
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "STA: Started");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    wifi_event_sta_connected_t *event =
        (wifi_event_sta_connected_t *)event_data;
    ESP_LOGI(TAG, "STA: Connected to SSID: %s", event->ssid);
    strncpy(sta_ssid, (char *)event->ssid, sizeof(sta_ssid) - 1);
    sta_ssid[sizeof(sta_ssid) - 1] = '\0';
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *event =
        (wifi_event_sta_disconnected_t *)event_data;
    last_disconnect_reason = event->reason;
    ESP_LOGI(TAG, "STA: Disconnected, reason: %d", event->reason);
    sta_connected = false;
    sta_connecting = false;
    sta_ip[0] = '\0';
    czechmate_mdns_refresh_sta_txt();
    ble_task_push_network_info(); // Notify BLE clients about disconnect
    if (!sta_manual_disconnect && sta_reconnect_timer != NULL) {
      esp_timer_start_once(sta_reconnect_timer,
                           (uint64_t)sta_reconnect_delay_ms * 1000);
      if (sta_reconnect_delay_ms < STA_RECONNECT_DELAY_MAX_MS) {
        sta_reconnect_delay_ms *= 2;
        if (sta_reconnect_delay_ms > STA_RECONNECT_DELAY_MAX_MS) {
          sta_reconnect_delay_ms = STA_RECONNECT_DELAY_MAX_MS;
        }
      }
    }
  }
  // IP eventy - ziskani IP adresy
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "STA: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    uint8_t third = esp_ip4_addr3(&event->ip_info.ip);
    if (s_sta_blk_oct_n > 0 && wifi_sta_third_octet_is_blocked(third)) {
      if (s_sta_blk_reject_streak < 15U) {
        s_sta_blk_reject_streak++;
        ESP_LOGW(TAG,
                 "STA: rejecting DHCP IP (blocked third octet %u), attempt %u",
                 (unsigned)third, s_sta_blk_reject_streak);
        sta_connected = false;
        sta_connecting = false;
        sta_ip[0] = '\0';
        esp_wifi_disconnect();
        return;
      }
      ESP_LOGW(TAG,
               "STA: blocklist third octet %u — max DHCP rejects, keeping IP",
               (unsigned)third);
      s_sta_blk_reject_streak = 0;
    } else {
      s_sta_blk_reject_streak = 0;
    }
    snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
    sta_connected = true;
    sta_connecting = false;
    sta_reconnect_delay_ms = STA_RECONNECT_DELAY_MIN_MS; // Reset backoff
    /* Modem sleep drops the MQTT keepalive and the broker aborts the socket. */
    esp_err_t ps = esp_wifi_set_ps(WIFI_PS_NONE);
    if (ps != ESP_OK) {
      ESP_LOGW(TAG, "STA: wifi power save off failed: %s", esp_err_to_name(ps));
    }
    czechmate_mdns_refresh_sta_txt();
    ble_task_push_network_info(); // Notify BLE clients about new IP
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGI(TAG, "STA: Lost IP");
    sta_connected = false;
    sta_connecting = false; // Resetovat flag pripojovani
    sta_ip[0] = '\0';
    czechmate_mdns_refresh_sta_txt();
    ble_task_push_network_info(); // Notify BLE clients about lost IP
  }
}

// ============================================================================
// WIFI NVS FUNCTION
// ============================================================================

// Duplicate implementation removed - external functions defined are used
// vyse

/**
 * @brief Create JSON with WiFi status (AP and STA)
 *
 * This function will create a JSON string with the current WiFi status (AP and STA).
 *
 * @param buffer Buffer for JSON string
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * Function to create JSON with AP information (SSID, IP, clients)
 * and STA (SSID, IP, connected status).
 */
esp_err_t wifi_get_sta_status_json(char *buffer, size_t buffer_size) {
  if (buffer == NULL || buffer_size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Ziskat AP IP adresu
  esp_netif_ip_info_t ip_info;
  const char *ap_ip_str = WIFI_AP_IP;
  if (ap_netif != NULL) {
    esp_err_t ret = esp_netif_get_ip_info(ap_netif, &ip_info);
    if (ret == ESP_OK) {
      char ap_ip[16];
      snprintf(ap_ip, sizeof(ap_ip), IPSTR, IP2STR(&ip_info.ip));
      ap_ip_str = ap_ip;
    }
  }

  // Get STA SSID from NVS (if any)
  char saved_ssid[33] = {0};
  char saved_password[65] = {0};
  esp_err_t nvs_ret = wifi_load_config_from_nvs(
      saved_ssid, sizeof(saved_ssid), saved_password, sizeof(saved_password));
  const char *sta_ssid_display =
      (nvs_ret == ESP_OK) ? saved_ssid : "Not configured";

  // Zjistit online status: STA connected a ma IP adresu
  bool online = sta_connected && sta_ip[0] != '\0';
  const bool ap_on = wifi_ap_active;
  const unsigned long ap_clients_out =
      ap_on ? (unsigned long)client_count : 0UL;
  const char *ap_ip_out = ap_on ? ap_ip_str : "";

  char blk_csv[96] = {0};
  char mdns_host[24] = {0};
  czechmate_mdns_copy_hostname(mdns_host, sizeof(mdns_host));
  nvs_handle_t nh;
  if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nh) == ESP_OK) {
    size_t bsz = sizeof(blk_csv);
    esp_err_t gr = nvs_get_str(nh, WIFI_NVS_KEY_STA_BLK_OCT, blk_csv, &bsz);
    nvs_close(nh);
    if (gr == ESP_ERR_NVS_NOT_FOUND) {
      strncpy(blk_csv, WIFI_STA_BLK_OCT_DEFAULT_CSV, sizeof(blk_csv) - 1);
      blk_csv[sizeof(blk_csv) - 1] = '\0';
    } else if (gr != ESP_OK) {
      strncpy(blk_csv, WIFI_STA_BLK_OCT_DEFAULT_CSV, sizeof(blk_csv) - 1);
      blk_csv[sizeof(blk_csv) - 1] = '\0';
    }
  } else {
    strncpy(blk_csv, WIFI_STA_BLK_OCT_DEFAULT_CSV, sizeof(blk_csv) - 1);
    blk_csv[sizeof(blk_csv) - 1] = '\0';
  }

  int len = snprintf(
      buffer, buffer_size,
      "{"
      "\"ap_ssid\":\"%s\","
      "\"ap_ip\":\"%s\","
      "\"ap_clients\":%lu,"
      "\"ap_active\":%s,"
      "\"sta_ssid\":\"%s\","
      "\"sta_ip\":\"%s\","
      "\"sta_connected\":%s,"
      "\"online\":%s,"
      "\"sta_blk_oct\":\"%s\","
      "\"mdns_host\":\"%s\","
      "\"locked\":%s"
      "}",
      wifi_ap_ssid_effective, ap_ip_out, ap_clients_out,
      ap_on ? "true" : "false",
      sta_ssid_display,
      (sta_connected && sta_ip[0] != '\0') ? sta_ip : "Not connected",
      sta_connected ? "true" : "false", online ? "true" : "false",
      blk_csv,
      mdns_host,
      web_locked ? "true" : "false");

  if (len < 0 || len >= (int)buffer_size) {
    ESP_LOGE(TAG, "Failed to create WiFi status JSON (buffer too small)");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}


// ============================================================================
// HTTP SERVER SETUP
// ============================================================================

/**
 * @brief Start the HTTP server
 *
 * This function will start an HTTP server with REST API endpoints for the sach system.
 * Registers all necessary handlers for the web interface.
 *
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * The function creates an HTTP server with configuration and registers handlers for:
 * - Main page (/)
 * - State of the board (/board)
 * - Game status (/status)
 * - Drag history (/history)
 * - Captured figures (/captured)
 */
#if CONFIG_CHESS_ENABLE_WEB_SERVER
/* newlib: ECONNABORTED is 113. lwIP's private table is not used
 * (port includes <errno.h>, LWIP_PROVIDE_ERRNO is off). */
static vprintf_like_t s_prev_log_vprintf;
static TickType_t s_accept_abort_log_tick;

static int web_accept_log_vprintf(const char *fmt, va_list ap) {
  bool abort_line = fmt != NULL && strstr(fmt, "error in accept") != NULL;
  bool follow_line =
      fmt != NULL && strstr(fmt, "error accepting new connection") != NULL;
  if (abort_line) {
    char line[160];
    char needle[8];
    va_list cp;
    va_copy(cp, ap);
    vsnprintf(line, sizeof(line), fmt, cp);
    va_end(cp);
    snprintf(needle, sizeof(needle), "(%d)", ECONNABORTED);
    if (strstr(line, needle) != NULL) {
      TickType_t now = xTaskGetTickCount();
      if (s_accept_abort_log_tick != 0 &&
          (now - s_accept_abort_log_tick) < pdMS_TO_TICKS(3000)) {
        return 0;
      }
      s_accept_abort_log_tick = now;
    }
  } else if (follow_line && s_accept_abort_log_tick != 0 &&
             (xTaskGetTickCount() - s_accept_abort_log_tick) <
                 pdMS_TO_TICKS(3000)) {
    return 0;
  }
  if (s_prev_log_vprintf != NULL) {
    return s_prev_log_vprintf(fmt, ap);
  }
  return vprintf(fmt, ap);
}

static void web_accept_log_limit_install(void) {
  static bool installed;
  if (installed) {
    return;
  }
  s_prev_log_vprintf = esp_log_set_vprintf(web_accept_log_vprintf);
  installed = true;
}

static esp_err_t start_http_server(void) {
  if (httpd_handle != NULL) {
    ESP_LOGW(TAG, "HTTP server already running");
    return ESP_OK;
  }

  web_accept_log_limit_install();

  ESP_LOGI(TAG, "Starting HTTP server...");

  // Konfigurovat HTTP server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_SERVER_PORT;
  /* Number of httpd_register_uri_handler in start_http_server: 54+ (2026-04, incl. WS + PNG figures).
   * Increase stock when adding endpoint (HTTPD doesn't register "quietly" extra). */
  config.max_uri_handlers = 64;
  /* httpd keeps 3 sockets internally. One active TCP PCB stays for MQTT. */
#if (CONFIG_LWIP_MAX_ACTIVE_TCP > 1) && \
    ((CONFIG_LWIP_MAX_ACTIVE_TCP - 1) < (CONFIG_LWIP_MAX_SOCKETS - 3))
  config.max_open_sockets = CONFIG_LWIP_MAX_ACTIVE_TCP - 1;
#elif CONFIG_LWIP_MAX_SOCKETS > 4
  config.max_open_sockets = CONFIG_LWIP_MAX_SOCKETS - 3;
#else
  config.max_open_sockets = 1;
#endif
  config.lru_purge_enable = false; // CRITICAL: Disabled to prevent socket
                                   // closure during chunked transfer
  config.recv_wait_timeout = 20;    // 20 s – stable connection with a slower network
  config.send_wait_timeout =
      5; // seconds – spolehlivy chunked transfer pri vykyvech
  config.max_resp_headers = 8;
  config.backlog_conn = 6;         // Vetsi fronta pri napadu klientu
  config.stack_size = 8192;        // Zvysena velikost stacku pro HTTP server task

  // Start the HTTP server
  esp_err_t ret = httpd_start(&httpd_handle, &config);
  if (ret != ESP_OK) {
    last_http_start_error = ret;
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return ret;
  }
  last_http_start_error = ESP_OK;

  ret = web_routes_register(httpd_handle);
  if (ret != ESP_OK) {
    last_http_start_error = ret;
    httpd_stop(httpd_handle);
    httpd_handle = NULL;
    return ret;
  }

  return ESP_OK;
}

/**
 * @brief Stop the HTTP server
 *
 * This function stops the HTTP server and frees the resources.
 *
 * @details
 * The function stops the HTTP server and sets the handle to NULL.
 * Used when shutting down the web server.
 */
static void stop_http_server(void) {
  web_ws_shutdown();
  if (httpd_handle != NULL) {
    httpd_stop(httpd_handle);
    httpd_handle = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
  }
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */


// ============================================================================
// REST API HANDLERS
// ============================================================================


bool web_json_body_complete(const char *body, int received, int content_len) {
  const char *p;
  int depth = 0;
  bool in_str = false;
  bool esc = false;
  bool seen = false;
  if (body == NULL || received <= 0 || body[0] == '\0') {
    return false;
  }
  if (content_len > received) {
    return false;
  }
  for (p = body; *p != '\0'; p++) {
    unsigned char c = (unsigned char)*p;
    if (in_str) {
      if (esc) {
        esc = false;
        continue;
      }
      if (c == '\\') {
        esc = true;
        continue;
      }
      if (c == '"') {
        in_str = false;
      }
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      continue;
    }
    if (c == '"') {
      in_str = true;
      continue;
    }
    if (c == '{') {
      depth++;
      seen = true;
      continue;
    }
    if (c == '}') {
      if (depth <= 0) {
        return false;
      }
      depth--;
      if (depth == 0) {
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
          p++;
        }
        return *p == '\0';
      }
      continue;
    }
    if (!seen) {
      return false;
    }
  }
  return false;
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
bool web_recv_json_body(httpd_req_t *req, char *buf, size_t cap) {
  int total = 0;
  int timeouts = 0;
  int want;
  if (req == NULL || buf == NULL || cap < 2) {
    return false;
  }
  if (req->content_len > 0 && req->content_len >= cap) {
    return false;
  }
  want = (int)cap - 1;
  while (total < want) {
    int r = httpd_req_recv(req, buf + total, (size_t)(want - total));
    if (r == HTTPD_SOCK_ERR_TIMEOUT) {
      if (++timeouts > 3) {
        return false;
      }
      continue;
    }
    if (r <= 0) {
      break;
    }
    timeouts = 0;
    total += r;
    if (req->content_len > 0 && (size_t)total >= req->content_len) {
      break;
    }
    if (req->content_len == 0) {
      break;
    }
  }
  if (total <= 0) {
    return false;
  }
  buf[total] = '\0';
  return web_json_body_complete(buf, total,
                                req->content_len > 0x7fffffffU
                                    ? 0x7fffffff
                                    : (int)req->content_len);
}
#endif

static bool json_extract_cmd_string(const char *buf, char *cmd_out, size_t cmd_sz) {
  const char *p = strstr(buf, "\"cmd\"");
  if (!p) {
    return false;
  }
  p = strchr(p, ':');
  if (!p) {
    return false;
  }
  p++;
  while (*p && isspace((unsigned char)*p))
    p++;
  if (*p != '"') {
    return false;
  }
  p++;
  size_t i = 0;
  while (*p && *p != '"' && i + 1 < cmd_sz) {
    cmd_out[i++] = *p++;
  }
  cmd_out[i] = '\0';
  return i > 0;
}

bool web_server_ble_extract_cmd_for_ack(const char *json, char *cmd_out,
                                        size_t cmd_out_sz) {
  if (json == NULL || cmd_out == NULL || cmd_out_sz == 0) {
    return false;
  }
  return json_extract_cmd_string(json, cmd_out, cmd_out_sz);
}


static bool s_ble_dispatch_custom_ack_sent;

bool web_server_ble_dispatch_custom_ack_was_sent(void) {
  return s_ble_dispatch_custom_ack_sent;
}

/** Clear feedback for the client — differentiates from web lock (same ESP_ERR_INVALID_STATE). */
static void ble_dispatch_ack_needs_encryption(const char *cmd_literal) {
  char out[288];
  int n =
      snprintf(out, sizeof(out),
               "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"needs_encryption\","
               "\"cmd\":\"%s\","
               "\"message\":\"Encrypted BLE connection required (complete pairing).\","
               "\"esp\":%d}",
               cmd_literal, (int)ESP_ERR_INVALID_STATE);
  if (n < 0 || (size_t)n >= sizeof(out)) {
    cmd_wire_notify_with_current(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"needs_encryption\","
        "\"cmd\":\"unknown\",\"message\":\"Encrypted BLE connection required.\","
        "\"esp\":259}");
  } else {
    cmd_wire_notify_with_current(out);
  }
  s_ble_dispatch_custom_ack_sent = true;
}

esp_err_t web_server_ble_command_dispatch(const char *json, size_t json_len) {
  s_ble_dispatch_custom_ack_sent = false;
  if (json == NULL || json_len == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  char buf[768];
  size_t n = json_len < sizeof(buf) - 1 ? json_len : sizeof(buf) - 1;
  memcpy(buf, json, n);
  buf[n] = '\0';
  if (!web_json_body_complete(buf, (int)n,
                              json_len > 0x7fffffffU ? 0x7fffffff
                                                     : (int)json_len)) {
    ESP_LOGW(TAG, "BLE JSON: incomplete, dropped");
    return ESP_ERR_INVALID_ARG;
  }

  char cmd[40];
  esp_err_t gate_err = ESP_OK;
  if (!json_extract_cmd_string(buf, cmd, sizeof(cmd))) {
    ESP_LOGW(TAG, "BLE JSON: missing cmd");
    return ESP_ERR_NOT_FOUND;
  }
  if (cmd_wire_gate(cmd, buf, &gate_err) != CMD_WIRE_PROCEED) {
    return gate_err;
  }

  if (strcmp(cmd, "ping") == 0) {
    ESP_LOGI(TAG, "BLE cmd: ping");
    return ESP_OK;
  }
  if (strcmp(cmd, "ota_start") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE ota_start: encrypted link required");
      ble_dispatch_ack_needs_encryption("ota_start");
      return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ota_err = ota_update_ble_try_dispatch(buf);
    if (ota_err == ESP_OK) {
      ESP_LOGI(TAG, "BLE ota_start accepted");
      return ESP_OK;
    }
    if (ota_err == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "BLE ota_start: busy (OTA already running)");
      return ota_err;
    }
    if (ota_err == ESP_ERR_NOT_ALLOWED) {
      ESP_LOGW(TAG,
               "BLE ota_start: HTTPS URL needs Wi‑Fi STA (connect board to router)");
      return ota_err;
    }
    if (ota_err == ESP_ERR_INVALID_ARG) {
      ESP_LOGW(TAG, "BLE ota_start: bad url (need http:// or https://)");
      return ota_err;
    }
    ESP_LOGE(TAG, "BLE ota_start failed: %s", esp_err_to_name(ota_err));
    return ota_err;
  }
  if (strcmp(cmd, "ota_ble_begin") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE ota_ble_begin: encrypted link required");
      ble_dispatch_ack_needs_encryption("ota_ble_begin");
      return ESP_ERR_INVALID_STATE;
    }
    esp_err_t e = ota_update_ble_begin_from_json(buf);
    if (e == ESP_OK) {
      ESP_LOGI(TAG, "BLE ota_ble_begin accepted");
      return ESP_OK;
    }
    if (e == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "BLE ota_ble_begin: busy or semaphore timeout");
      return e;
    }
    if (e == ESP_ERR_INVALID_ARG || e == ESP_ERR_INVALID_SIZE) {
      ESP_LOGW(TAG, "BLE ota_ble_begin: invalid size or JSON");
      return e;
    }
    if (e == ESP_ERR_NOT_SUPPORTED) {
      ESP_LOGD(TAG,
               "BLE ota_ble_begin: no OTA slots (factory-only partition table)");
      return e;
    }
    if (e == ESP_ERR_NOT_FOUND) {
      return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGE(TAG, "BLE ota_ble_begin failed: %s", esp_err_to_name(e));
    return e;
  }
  if (strcmp(cmd, "ota_ble_abort") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE ota_ble_abort: encrypted link required");
      ble_dispatch_ack_needs_encryption("ota_ble_abort");
      return ESP_ERR_INVALID_STATE;
    }
    esp_err_t e = ota_update_ble_abort();
    if (e == ESP_OK) {
      return ESP_OK;
    }
    ESP_LOGW(TAG, "BLE ota_ble_abort: %s", esp_err_to_name(e));
    return e;
  }
  if (strcmp(cmd, "ota_ble_status") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE ota_ble_status: encrypted link required");
      ble_dispatch_ack_needs_encryption("ota_ble_status");
      return ESP_ERR_INVALID_STATE;
    }
    char status_ack[768];
    esp_err_t er =
        ota_update_ble_build_status_ack_json(status_ack, sizeof(status_ack));
    if (er != ESP_OK) {
      return er;
    }
    cmd_wire_notify_with_current(status_ack);
    s_ble_dispatch_custom_ack_sent = true;
    return ESP_OK;
  }
  if (strcmp(cmd, "factory_reset") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE factory_reset: encrypted link required");
      ble_dispatch_ack_needs_encryption("factory_reset");
      return ESP_ERR_INVALID_STATE;
    }
    if (!json_body_has_factory_confirm(buf)) {
      ESP_LOGW(TAG, "BLE factory_reset: missing/invalid confirm");
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGW(TAG, "[STAGING] BLE factory_reset: scheduling NVS erase + restart");
    return factory_reset_schedule();
  }
  if (strcmp(cmd, "wifi_survey") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE wifi_survey: encrypted link required");
      ble_dispatch_ack_needs_encryption("wifi_survey");
      return ESP_ERR_INVALID_STATE;
    }
    survey_ack_meta_t *meta = malloc(sizeof(*meta));
    if (meta == NULL) {
      return ESP_ERR_NO_MEM;
    }
    cmd_wire_copy_request_id(&meta->id, &meta->has_id);
    BaseType_t ok =
        xTaskCreate(wifi_ble_survey_task, "wifi_survey", 8192, meta, 5, NULL);
    if (ok != pdPASS) {
      free(meta);
      ESP_LOGE(TAG, "BLE wifi_survey: xTaskCreate failed");
      return ESP_FAIL;
    }
    s_ble_dispatch_custom_ack_sent = true;
    ESP_LOGI(TAG, "BLE wifi_survey: scan task queued");
    return ESP_OK;
  }
  if (strcmp(cmd, "wifi_sta_config") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE wifi_sta_config: encrypted link required");
      ble_dispatch_ack_needs_encryption("wifi_sta_config");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "BLE wifi_sta_config: invalid JSON");
      return ESP_FAIL;
    }
    cJSON *sj = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    if (!cJSON_IsString(sj) || sj->valuestring == NULL || sj->valuestring[0] == '\0') {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    const char *pwd = "";
    cJSON *pj = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(pj) && pj->valuestring != NULL) {
      pwd = pj->valuestring;
    }
    size_t slen = strlen(sj->valuestring);
    size_t plen = strlen(pwd);
    if (slen > 32U) {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    if (plen > 64U) {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    wifi_ble_prov_msg_t *msg = (wifi_ble_prov_msg_t *)calloc(1, sizeof(wifi_ble_prov_msg_t));
    if (msg == NULL) {
      cJSON_Delete(root);
      return ESP_FAIL;
    }
    strncpy(msg->ssid, sj->valuestring, sizeof(msg->ssid) - 1);
    strncpy(msg->password, pwd, sizeof(msg->password) - 1);
    cJSON_Delete(root);

    BaseType_t ok =
        xTaskCreate(wifi_ble_prov_task, "wifi_ble_prov", 8192, msg, 5, NULL);
    if (ok != pdPASS) {
      ESP_LOGE(TAG, "BLE wifi_sta_config: xTaskCreate failed");
      free(msg);
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BLE wifi_sta_config: provisioning task queued");
    return ESP_OK;
  }
  if (strcmp(cmd, "wifi_sta_ip_block") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE wifi_sta_ip_block: encrypted link required");
      ble_dispatch_ack_needs_encryption("wifi_sta_ip_block");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "BLE wifi_sta_ip_block: invalid JSON");
      return ESP_FAIL;
    }
    const char *csv = "";
    cJSON *tj = cJSON_GetObjectItemCaseSensitive(root, "third_octets");
    if (cJSON_IsString(tj) && tj->valuestring != NULL) {
      csv = tj->valuestring;
    }
    esp_err_t se = wifi_sta_save_blocked_octets_csv(csv);
    cJSON_Delete(root);
    if (se != ESP_OK) {
      ESP_LOGW(TAG, "BLE wifi_sta_ip_block: save failed: %s",
               esp_err_to_name(se));
      return se;
    }
    wifi_sta_refresh_blocked_octets_from_nvs();
    wifi_sta_disconnect_if_current_ip_blocked();
    ESP_LOGI(TAG, "BLE wifi_sta_ip_block: NVS updated");
    return ESP_OK;
  }
  if (strcmp(cmd, "wifi_ap_set") == 0) {
    if (!ble_task_conn_is_encrypted()) {
      ESP_LOGW(TAG, "BLE wifi_ap_set: encrypted link required");
      ble_dispatch_ack_needs_encryption("wifi_ap_set");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "BLE wifi_ap_set: invalid JSON");
      return ESP_FAIL;
    }
    const cJSON *jen =
        cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (!cJSON_IsBool(jen)) {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    const bool want = cJSON_IsTrue(jen);
    cJSON_Delete(root);

    wifi_ap_toggle_msg_t *msg =
        (wifi_ap_toggle_msg_t *)calloc(1, sizeof(wifi_ap_toggle_msg_t));
    if (msg == NULL) {
      return ESP_ERR_NO_MEM;
    }
    msg->enable = want;
    BaseType_t ok =
        xTaskCreate(wifi_ap_user_apply_task, "wifi_ap_set", 8192, msg, 5, NULL);
    if (ok != pdPASS) {
      ESP_LOGE(TAG, "BLE wifi_ap_set: xTaskCreate failed");
      free(msg);
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "BLE wifi_ap_set: task queued (enabled=%s)",
             want ? "true" : "false");
    return ESP_OK;
  }
  if (strcmp(cmd, "hint_highlight") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "BLE hint_highlight blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    return web_server_apply_hint_highlight_json_body(buf);
  }
  if (strcmp(cmd, "hint_clear") == 0) {
    if (!web_game_submit_hint_clear()) {
      return ESP_FAIL;
    }
    ESP_LOGD(TAG, "BLE cmd: hint_clear");
    return ESP_OK;
  }
  if (strcmp(cmd, "guard_clear") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "BLE guard_clear blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    if (game_is_squares_guard_active() || squares_is_guard_mode_active()) {
      game_force_clear_squares_guard();
    }
    ESP_LOGD(TAG, "BLE cmd: guard_clear");
    return ESP_OK;
  }
  if (strcmp(cmd, "hall_rest") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "BLE hall_rest blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    hall_i2c_squares_request_rest_measure();
    ESP_LOGI(TAG, "BLE hall_rest");
    return ESP_OK;
  }
  if (strcmp(cmd, "brightness") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "BLE brightness blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    int brightness_val = -1;
    const char *pp = strstr(buf, "\"percent\"");
    if (pp && sscanf(pp, "\"percent\":%d", &brightness_val) != 1) {
      brightness_val = -1;
    }
    if (brightness_val < 0) {
      pp = strstr(buf, "\"brightness\"");
      if (pp) {
        (void)sscanf(pp, "\"brightness\":%d", &brightness_val);
      }
    }
    if (brightness_val < 0 || brightness_val > 100) {
      return ESP_ERR_INVALID_ARG;
    }
    led_set_brightness_global((uint8_t)brightness_val);
    system_config_t config;
    config.brightness_level = 100;
    if (config_load_from_nvs(&config) == ESP_OK) {
      config.brightness_level = (uint8_t)brightness_val;
      config_save_to_nvs(&config);
    } else {
      config.brightness_level = (uint8_t)brightness_val;
      config_save_to_nvs(&config);
    }
    cached_brightness = (uint8_t)brightness_val;
    cached_brightness_valid = true;
    ESP_LOGI(TAG, "BLE brightness %d%%", brightness_val);
    return ESP_OK;
  }

  if (strcmp(cmd, "light_command") == 0) {
    bool state = true;
    int r = 255, g = 255, b = 255;
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "BLE light_command: invalid JSON");
      return ESP_FAIL;
    }
    cJSON *stj = cJSON_GetObjectItemCaseSensitive(root, "state");
    if (cJSON_IsBool(stj) && cJSON_IsFalse(stj)) {
      state = false;
    }
    cJSON *jr = cJSON_GetObjectItemCaseSensitive(root, "r");
    cJSON *jg = cJSON_GetObjectItemCaseSensitive(root, "g");
    cJSON *jb = cJSON_GetObjectItemCaseSensitive(root, "b");
    if (cJSON_IsNumber(jr)) {
      r = (int)cJSON_GetNumberValue(jr);
    }
    if (cJSON_IsNumber(jg)) {
      g = (int)cJSON_GetNumberValue(jg);
    }
    if (cJSON_IsNumber(jb)) {
      b = (int)cJSON_GetNumberValue(jb);
    }
    cJSON_Delete(root);
    if (r < 0) {
      r = 0;
    }
    if (r > 255) {
      r = 255;
    }
    if (g < 0) {
      g = 0;
    }
    if (g > 255) {
      g = 255;
    }
    if (b < 0) {
      b = 0;
    }
    if (b > 255) {
      b = 255;
    }
    if (!ha_light_request_web_lamp(state, (uint8_t)r, (uint8_t)g, (uint8_t)b)) {
      ESP_LOGW(TAG, "BLE light_command: ha_light_request_web_lamp failed");
      return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "BLE light_command state=%d rgb=%d,%d,%d", (int)state, r, g, b);
    return ESP_OK;
  }
  if (strcmp(cmd, "light_game_mode") == 0) {
    ha_light_report_activity("web_game_mode");
    ESP_LOGI(TAG, "BLE light_game_mode");
    return ESP_OK;
  }

  esp_err_t game_ble = web_server_ble_game_command_dispatch(cmd, buf);
  if (game_ble != ESP_ERR_NOT_FOUND) {
    return game_ble;
  }

  esp_err_t sys_ble = web_server_ble_system_command_dispatch(cmd, buf);
  if (sys_ble != ESP_ERR_NOT_FOUND) {
    return sys_ble;
  }

  ESP_LOGW(TAG, "BLE JSON: unknown cmd \"%s\"", cmd);
  return ESP_ERR_NOT_SUPPORTED;
}

static void czechmate_push_ble_ready(size_t len) {
  (void)web_server_task_wdt_reset_safe();
  if (len == 0 || !ble_task_should_push_snapshot()) {
    return;
  }
  ble_task_push_snapshot_json((const uint8_t *)snapshot_buffer, len);
}

static void czechmate_push_ble_snapshot(void) {
  size_t len = 0;
  if (build_snapshot_json(&len) != ESP_OK) {
    return;
  }
  czechmate_push_ble_ready(len);
  web_snapshot_buffer_release();
}

void czechmate_mdns_copy_hostname(char *out, size_t cap) {
  if (out == NULL || cap == 0) {
    return;
  }
  out[0] = '\0';
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK &&
      esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY) != ESP_OK) {
    return;
  }
  snprintf(out, cap, "czechmate-%02x%02x%02x", mac[3], mac[4], mac[5]);
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
static bool czechmate_mdns_started = false;

/**
 * Bonjour TXT "sta_ip" + "ap_ip" — iOS/watch often does not resolve hostname .local
 * on time; direct IPv4 in TXT will enable HTTP/WS without dependency on mDNS A record.
 *
 * ap_ip fix (AP hotspot): ESP only sends mDNS packets via the STA interface to
 * home networks. A phone connected to an AP hotspot will never receive these packets,
 * because it belongs to a different L2 segment. Fixed:
 * 1) TXT ap_ip = IP address of the AP interface (typically 192.168.4.1)
 * 2) mdns_netif_action on ap_netif → ESP sends mDNS to the AP subnet as well
 */
static void czechmate_mdns_refresh_sta_txt(void) {
  if (!czechmate_mdns_started) {
    return;
  }

  /* ── sta_ip: direct STA IPv4 address (home network) ── */
  if (sta_ip[0] != '\0') {
    esp_err_t e =
        mdns_service_txt_item_set("_http", "_tcp", "sta_ip", sta_ip);
    if (e != ESP_OK) {
      ESP_LOGW(TAG, "mDNS TXT sta_ip: %s", esp_err_to_name(e));
    } else {
      ESP_LOGI(TAG, "mDNS TXT sta_ip=%s (Bonjour + prime HTTP)", sta_ip);
    }
    if (sta_netif != NULL) {
      (void)mdns_netif_action(
          sta_netif, MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ENABLE_IP4);
    }
  } else {
    esp_err_t r = mdns_service_txt_item_remove("_http", "_tcp", "sta_ip");
    if (r != ESP_OK && r != ESP_ERR_NOT_FOUND) {
      ESP_LOGD(TAG, "mDNS TXT sta_ip remove: %s", esp_err_to_name(r));
    }
  }

  /* ── ap_ip: IP adresa AP hotspotu ESP (typicky 192.168.4.1) ──
   * iOS reads ap_ip from the TXT record and connects directly without .local resolve.
   * Aktivace mdns_netif_action na ap_netif zajisti, ze ESP vysila mDNS
   * multicasty take do AP subnetu — tak je iPhone na hotspotu najde. */
  if (ap_netif != NULL) {
    char ap_ip_str[16];
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
      snprintf(ap_ip_str, sizeof(ap_ip_str), IPSTR, IP2STR(&ip_info.ip));
    } else {
      snprintf(ap_ip_str, sizeof(ap_ip_str), "%s", WIFI_AP_IP);
    }
    esp_err_t e2 =
        mdns_service_txt_item_set("_http", "_tcp", "ap_ip", ap_ip_str);
    if (e2 != ESP_OK) {
      ESP_LOGW(TAG, "mDNS TXT ap_ip: %s", esp_err_to_name(e2));
    } else {
      ESP_LOGI(TAG, "mDNS TXT ap_ip=%s (AP hotspot discovery)", ap_ip_str);
    }
    /* Enable mDNS broadcast via AP interface */
    (void)mdns_netif_action(
        ap_netif, MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4);
  }
}

static void czechmate_mdns_ensure_started(void) {
  if (czechmate_mdns_started) {
    return;
  }
  /* Stack comes from internal RAM. A failed create returns ESP_FAIL and
   * leaves no task; do not call mdns_init when the block cannot fit. */
  const size_t mdns_need = (size_t)CONFIG_MDNS_TASK_STACK_SIZE + 512;
  size_t largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (largest < mdns_need) {
    static bool warned;
    if (!warned) {
      warned = true;
      ESP_LOGW(TAG, "mDNS skipped, largest free block %u B < %u B",
               (unsigned)largest, (unsigned)mdns_need);
    }
    return;
  }
  esp_err_t e = mdns_init();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "mdns_init: %s", esp_err_to_name(e));
    return;
  }
  char host[24];
  czechmate_mdns_copy_hostname(host, sizeof(host));
  if (host[0] == '\0') {
    ESP_LOGW(TAG, "mDNS hostname unavailable (MAC read failed)");
    return;
  }
  mdns_hostname_set(host);
  mdns_instance_name_set("CZECHMATE");
  mdns_txt_item_t txt[] = {
      {"board", "czechmate"},
  };
  e = mdns_service_add("CZECHMATE", "_http", "_tcp", HTTP_SERVER_PORT, txt, 1);
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "mdns_service_add: %s", esp_err_to_name(e));
  } else {
    ESP_LOGI(TAG, "[STAGING] mDNS %s.local · _http._tcp port %d", host,
             HTTP_SERVER_PORT);
  }
  czechmate_mdns_started = true;
  /* refresh_sta_txt activates mDNS on both STA and AP interfaces + fill TXT records */
  czechmate_mdns_refresh_sta_txt();
}
#else
static void czechmate_mdns_refresh_sta_txt(void) {}
static void czechmate_mdns_ensure_started(void) {}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */


/** Steady free after HTTP+mDNS is ~19 KiB. Do not reuse the 20 KiB WiFi/MQTT floor. */
#define WEB_SNAPSHOT_SKIP_FREE_BYTES (12 * 1024)
/** Accept keeps running above these. A real pause fills the listen backlog. */
#define WEB_HTTP_BACKOFF_FREE_BYTES (8 * 1024)
#define WEB_HTTP_BACKOFF_LARGEST_BYTES (2 * 1024)

/** Ping queue from game_task — WS + BLE snapshot in web_server_task (does not block
 * game_task at httpd_ws_send_data / malloc). Must be outside #if WS — BLE always. */

void czechmate_ensure_snapshot_notify_queue(void) {
  if (snapshot_notify_queue != NULL) {
    return;
  }
  snapshot_notify_queue = xQueueCreate(2, sizeof(uint8_t));
  if (snapshot_notify_queue == NULL) {
    ESP_LOGE(TAG, "snapshot_notify_queue create failed (fallback sync)");
  }
}

void web_server_process_snapshot_notify_queue(void) {
  if (snapshot_notify_queue == NULL) {
    return;
  }
  static TickType_t s_snap_last_send;
  static TickType_t s_snap_skip_log;
  static bool s_snap_pending;
  uint8_t ping;
  while (xQueueReceive(snapshot_notify_queue, &ping, 0) == pdTRUE) {
    s_snap_pending = true;
  }
  if (!s_snap_pending) {
    return;
  }
  TickType_t now = xTaskGetTickCount();
  if (s_snap_last_send != 0 && (now - s_snap_last_send) < pdMS_TO_TICKS(250)) {
    return;
  }
  /* Snapshot + WS sending can block for a long time (httpd send timeout) — no
   * TWDT reset reports web_server_task timeout. */
  (void)web_server_task_wdt_reset_safe();
  size_t fh = esp_get_free_heap_size();
  /* Slabs are static (20 KiB + 8 KiB). Publish when both are free.
   * 12 KiB is the highest skip gate; ~19 KiB steady must still send. */
  bool slabs = shared_buffer_pair_free(SNAPSHOT_BUFFER_SIZE, JSON_BUFFER_SIZE);
  if (!slabs || fh < (size_t)WEB_SNAPSHOT_SKIP_FREE_BYTES) {
    if ((now - s_snap_skip_log) >= pdMS_TO_TICKS(5000)) {
      s_snap_skip_log = now;
      ESP_LOGW(TAG, "snapshot skipped, free heap %zu B, slabs %s", fh,
               slabs ? "free" : "busy");
    }
    s_snap_last_send = now;
    return;
  }
  size_t snap_len = 0;
  if (build_snapshot_json(&snap_len) != ESP_OK) {
    s_snap_pending = false;
    s_snap_last_send = now;
    return;
  }
#if CONFIG_CHESS_ENABLE_WEB_SERVER && CONFIG_HTTPD_WS_SUPPORT
  ws_broadcast_prepared(snap_len);
#endif
  (void)web_server_task_wdt_reset_safe();
  czechmate_push_ble_ready(snap_len);
  (void)web_server_task_wdt_reset_safe();
  web_snapshot_buffer_release();
  s_snap_pending = false;
  s_snap_last_send = now;
}

/** Strong implementation — overrides weak from game_hooks (WS + BLE, independent of
 * CONFIG_HTTPD_WS_SUPPORT for BLE). */
void czechmate_on_game_state_changed(void) {
  czechmate_ensure_snapshot_notify_queue();
  if (snapshot_notify_queue == NULL) {
#if CONFIG_CHESS_ENABLE_WEB_SERVER && CONFIG_HTTPD_WS_SUPPORT
    ESP_LOGD(TAG,
             "[STAGING] czechmate_on_game_state_changed sync → WS broadcast");
    ws_broadcast_snapshot();
#endif
    czechmate_push_ble_snapshot();
    return;
  }
  uint8_t ping = 1;
  if (xQueueSend(snapshot_notify_queue, &ping, 0) != pdTRUE) {
    ESP_LOGD(TAG,
             "[STAGING] snapshot notify coalesced (queue full, already pending)");
  } else {
    ESP_LOGD(TAG, "[STAGING] czechmate_on_game_state_changed → notify queued");
  }
}


// ============================================================================
// TIMER API HANDLERS
// ============================================================================

esp_err_t web_get_timer_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/timer");

  // Lokalni buffer jen pro timer JSON (~1 KiB); JSON_BUFFER_SIZE na stacku = overflow httpd (8192 B).
  char local_json[TIMER_HTTP_JSON_MAX];
  esp_err_t ret = game_get_timer_json(local_json, sizeof(local_json));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get timer state", -1);
    return ESP_FAIL;
  }

  // Prevent caching of the response timer in the browser
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, local_json, strlen(local_json));

  return ESP_OK;
}

esp_err_t web_get_chess_js_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "404 Not Found");
  httpd_resp_set_type(req, "application/json; charset=utf-8");
  return httpd_resp_send(req, "{\"error\":\"browser_ui_removed\"}",
                         HTTPD_RESP_USE_STRLEN);
}

/** GET /favicon.ico — no body; silent 404 in browser log on random request. */
esp_err_t web_get_favicon_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, "", 0);
}

static const char k_http_root_json[] =
    "{\"service\":\"czechmate\",\"client\":\"mobile_app\","
    "\"api\":\"/api/\",\"websocket\":\"/ws\"}";

esp_err_t web_get_root_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET / (JSON — CzechMate app-only HTTP surface)");
  httpd_resp_set_type(req, "application/json; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, k_http_root_json, HTTPD_RESP_USE_STRLEN);
}

// WRAP FUNCTIONS FOR ESP_DIAGNOSTICS
// ============================================================================
// EMPTY implementation to prevent stack overflow
// esp_diagnostics will not work, but web server will function

void __wrap_esp_log_writev(esp_log_level_t level, const char *tag,
                           const char *format, va_list args) {
  // EMPTY - do nothing to prevent stack overflow
  (void)level;
  (void)tag;
  (void)format;
  (void)args;
}

void __wrap_esp_log_write(esp_log_level_t level, const char *tag,
                          const char *format, ...) {
  // EMPTY - do nothing to prevent stack overflow
  (void)level;
  (void)tag;
  (void)format;
}

static bool web_http_heap_critical(void) {
  size_t free_b = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return free_b < (size_t)WEB_HTTP_BACKOFF_FREE_BYTES ||
         largest < (size_t)WEB_HTTP_BACKOFF_LARGEST_BYTES;
}

static void web_http_heap_backoff(void) {
  if (!web_server_active || !web_http_heap_critical()) {
    return;
  }
  /* Accept stays in the httpd task. Do not mute or stop it: a paused
   * listen socket fills the backlog and aborted clients come back as 113. */
  static TickType_t s_log;
  TickType_t now = xTaskGetTickCount();
  if (s_log != 0 && (now - s_log) < pdMS_TO_TICKS(3000)) {
    return;
  }
  s_log = now;
  size_t free_b = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t largest =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  ESP_LOGW(TAG,
           "HTTP heap critical, accept continues: free %u B largest %u B "
           "(gate free %d / largest %d)",
           (unsigned)free_b, (unsigned)largest, WEB_HTTP_BACKOFF_FREE_BYTES,
           WEB_HTTP_BACKOFF_LARGEST_BYTES);
}

// ============================================================================
// WEB SERVER TASK IMPLEMENTATION
// ============================================================================

void web_server_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "Web server task starting...");

  // CRITICAL: Register with TWDT
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGE(TAG, "Failed to register web server task with TWDT: %s",
             esp_err_to_name(wdt_ret));
  } else {
    ESP_LOGI(TAG, "✅ Web server task registered with TWDT");
  }

  // NVS is already initialized in main.c - skip it here
  ESP_LOGI(TAG, "NVS already initialized, skipping...");

  // Load web lock status from NVS
  esp_err_t lock_ret = web_lock_load_from_nvs();
  if (lock_ret == ESP_OK) {
    ESP_LOGI(TAG, "Web interface lock status: %s",
             web_locked ? "locked" : "unlocked");
  } else {
    ESP_LOGW(TAG, "Failed to load web lock status, using default: unlocked");
  }

#if CONFIG_CHESS_ENABLE_WEB_SERVER
  // Initialize WiFi APSTA
  esp_err_t ret = wifi_init_apsta();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG, "❌ Web server task exiting");

    esp_task_wdt_delete(NULL); // Unregister from WDT before deleting
    vTaskDelete(NULL);
    return;
  }
  ESP_LOGI(TAG, "WiFi initialized (wifi_ap_active=%s)",
           wifi_ap_active ? "true" : "false");

  // Wait for WiFi to be ready
  vTaskDelay(pdMS_TO_TICKS(2000));

  // Automatically connected STA if the configuration is in NVS
  char ssid[33] = {0};
  char password[65] = {0};
  esp_err_t nvs_ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));
  if (nvs_ret == ESP_OK) {
    ESP_LOGI(TAG, "WiFi config found in NVS, attempting auto-connect...");
    esp_err_t connect_ret = wifi_connect_sta();
    if (connect_ret == ESP_OK) {
      ESP_LOGI(TAG, "✅ WiFi STA auto-connected successfully");
    } else {
      ESP_LOGW(TAG,
               "⚠️ WiFi STA auto-connect failed: %s (%s)",
               esp_err_to_name(connect_ret),
               wifi_ap_active ? "board hotspot ON — AP+STA"
                              : "board hotspot OFF — STA-only until credentials work or hotspot enabled");
    }
  } else {
    ESP_LOGI(TAG, "No WiFi config in NVS, STA will remain disconnected");
  }

  /* BLE notifies snapshots from this task and does not use httpd.
   * With STA down and the hotspot off, nobody can open a TCP socket.
   * httpd (8 KiB stack) plus mDNS is what collapsed the heap to ~772 B. */
  if (sta_connected || wifi_ap_active) {
    ret = start_http_server();
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "❌ Failed to start HTTP server: %s", esp_err_to_name(ret));
      ESP_LOGE(TAG,
               "❌ Web server task will continue but HTTP will not be available");

      task_running = true;
      uint32_t retry_counter = 0;
      while (task_running) {
        web_server_task_wdt_reset_safe();
        if ((retry_counter++ % 5) == 0) {
          ESP_LOGW(TAG, "Retrying HTTP server start...");
          esp_err_t retry_ret = start_http_server();
          if (retry_ret == ESP_OK) {
            web_server_active = true;
            web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            ESP_LOGI(TAG, "HTTP server recovered and started");
            break;
          }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
      }

      if (web_server_active) {
        ESP_LOGI(TAG, "Switching to normal web server loop after recovery");
      } else {
        esp_task_wdt_delete(NULL);
        vTaskDelete(NULL);
        return;
      }
    }
    web_server_active = true;
    web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "HTTP server started");
    czechmate_mdns_ensure_started();
    {
      size_t free_b = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      ESP_LOGW(TAG, "SRAM after HTTP+mDNS: free=%u B, largest_block=%u B",
               (unsigned)free_b, (unsigned)largest);
    }
  } else {
    ESP_LOGW(TAG,
             "HTTP+mDNS not started: no STA and board hotspot OFF "
             "(BLE and the game stay up)");
    {
      size_t free_b = heap_caps_get_free_size(MALLOC_CAP_8BIT);
      size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
      ESP_LOGW(TAG,
               "SRAM with HTTP+mDNS held: free=%u B, largest_block=%u B",
               (unsigned)free_b, (unsigned)largest);
    }
  }

  /* Do not restore a stored dim. Status stays at 100% until the user changes it. */
  cached_brightness = 100;
  cached_brightness_valid = true;

  task_running = true;
  ESP_LOGI(TAG, "Web server task started successfully");
  if (wifi_ap_active) {
    ESP_LOGI(TAG, "Board hotspot SSID: %s", wifi_ap_ssid_effective);
    ESP_LOGI(TAG, "Board hotspot password: %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "Board HTTP (AP): http://%s", WIFI_AP_IP);
  } else {
    ESP_LOGI(TAG, "Board hotspot: OFF (enable from CzechMate app over BLE)");
  }

#else /* !CONFIG_CHESS_ENABLE_WEB_SERVER */
  (void)board_api_auth_init();
  task_running = true;
  ESP_LOGI(TAG,
           "HTTP disabled (CONFIG_CHESS_ENABLE_WEB_SERVER=n) — BLE JSON bridge only");
#endif

  // Main task loop
  uint32_t loop_count = 0;

  while (task_running) {
    // Reset task watchdog timer
    esp_err_t wdt_ret = web_server_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet
    }

#if CONFIG_CHESS_ENABLE_WEB_SERVER
    /* BLE provision or wifi_ap_set can make STA/AP reachable after boot. */
    if ((loop_count % 50) == 0 && (sta_connected || wifi_ap_active)) {
      if (!web_server_active) {
        esp_err_t http_ret = start_http_server();
        if (http_ret == ESP_OK) {
          web_server_active = true;
          web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
          ESP_LOGI(TAG, "HTTP server started (WiFi reachable)");
        }
      }
      if (web_server_active) {
        czechmate_mdns_ensure_started();
      }
    }
#endif

    // Process web server commands from queue
    web_server_process_commands();

    /* Snapshot WS/BLE mimo game_task (fronta z czechmate_on_game_state_changed). */
    web_server_process_snapshot_notify_queue();
    web_http_heap_backoff();

    // Update web server state
    web_server_update_state();

    // Periodic status logging
    if (loop_count % 1000 == 0) {
      ESP_LOGI(TAG, "Web Server Status: Active=%s, Clients=%lu, Uptime=%lu ms",
               web_server_active ? "Yes" : "No", client_count,
               web_server_active ? (xTaskGetTickCount() * portTICK_PERIOD_MS -
                                    web_server_start_time)
                                 : 0);
    }

    loop_count++;

    // Wait for next update cycle (100ms)
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // Cleanup
#if CONFIG_CHESS_ENABLE_WEB_SERVER
  stop_http_server();
  esp_wifi_stop();
  esp_wifi_deinit();
#endif

  ESP_LOGI(TAG, "Web server task stopped");

  // Task function should not return
  vTaskDelete(NULL);
}

// ============================================================================
// WEB SERVER COMMAND PROCESSING
// ============================================================================

void web_server_process_commands(void) {
  uint8_t command;

  // Check for new web server commands from queue
  if (web_server_command_queue != NULL &&
      xQueueReceive(web_server_command_queue, &command, 0) == pdTRUE) {
    web_server_execute_command(command);
  }
}

esp_err_t web_server_enqueue_command(web_command_type_t cmd) {
  if (web_server_command_queue == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  uint8_t c = (uint8_t)cmd;
  if (xQueueSend(web_server_command_queue, &c, pdMS_TO_TICKS(2000)) !=
      pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

void web_server_execute_command(uint8_t command) {
  switch (command) {
  case WEB_CMD_START_SERVER:
    web_server_start();
    break;

  case WEB_CMD_STOP_SERVER:
    web_server_stop();
    break;

  case WEB_CMD_GET_STATUS:
    web_server_get_status();
    break;

  case WEB_CMD_SET_CONFIG:
    web_server_set_config();
    break;

  default:
    ESP_LOGW(TAG, "Unknown web server command: %d", command);
    break;
  }
}

// ============================================================================
// WEB SERVER CONTROL FUNCTIONS
// ============================================================================

void web_server_start(void) {
#if !CONFIG_CHESS_ENABLE_WEB_SERVER
  ESP_LOGW(TAG, "HTTP web server disabled at build time");
  return;
#else
  if (web_server_active) {
    ESP_LOGW(TAG, "Web server already active");
    return;
  }

  ESP_LOGI(TAG, "Starting web server...");

  esp_err_t ret = start_http_server();
  if (ret == ESP_OK) {
    web_server_active = true;
    web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Web server started successfully");
  } else {
    ESP_LOGE(TAG, "Failed to start web server");
  }

  // Send status to status queue
  if (web_server_status_queue != NULL) {
    uint8_t status = web_server_active ? 1 : 0;
    xQueueSend(web_server_status_queue, &status, 0);
  }
#endif
}

void web_server_stop(void) {
#if !CONFIG_CHESS_ENABLE_WEB_SERVER
  ESP_LOGW(TAG, "HTTP web server disabled at build time");
  return;
#else
  if (!web_server_active) {
    ESP_LOGW(TAG, "Web server not active - cannot stop");
    return;
  }

  ESP_LOGI(TAG, "Stopping web server...");

  stop_http_server();
  web_server_active = false;
  web_server_start_time = 0;

  ESP_LOGI(TAG, "Web server stopped successfully");

  // Send status to status queue
  if (web_server_status_queue != NULL) {
    uint8_t status = 0;
    xQueueSend(web_server_status_queue, &status, 0);
  }
#endif
}

void web_server_get_status(void) {
  ESP_LOGI(TAG, "Web Server Status - Active: %s, Clients: %lu, Uptime: %lu ms",
           web_server_active ? "Yes" : "No", client_count,
           web_server_active ? (xTaskGetTickCount() * portTICK_PERIOD_MS -
                                web_server_start_time)
                             : 0);

  // Send status to status queue
  if (web_server_status_queue != NULL) {
    uint8_t status = web_server_active ? 1 : 0;
    xQueueSend(web_server_status_queue, &status, 0);
  }
}

void web_server_set_config(void) {
  ESP_LOGI(TAG, "Web server configuration update requested");
  ESP_LOGI(TAG, "Web server configuration updated");
}

// ============================================================================
// WEB SERVER STATE UPDATE
// ============================================================================

void web_server_update_state(void) {
  if (!web_server_active) {
    return;
  }

  // Web server is running, no additional updates needed
  // State is updated via HTTP handlers
}

// ============================================================================
// HTTP HANDLERS (Legacy placeholder functions)
// ============================================================================

void web_server_handle_root(void) {
  ESP_LOGI(TAG, "Handling root HTTP request");
  ESP_LOGD(TAG, "Root page served successfully");
}

void web_server_handle_api_status(void) {
  ESP_LOGI(TAG, "Handling API status request");
  ESP_LOGD(TAG, "API status served successfully");
}

void web_server_handle_api_board(void) {
  ESP_LOGI(TAG, "Handling API board request");
  ESP_LOGD(TAG, "API board data served successfully");
}

void web_server_handle_api_move(void) {
  ESP_LOGI(TAG, "Handling API move request");
  ESP_LOGD(TAG, "API move request processed successfully");
}


// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool web_server_is_active(void) { return web_server_active; }

#if CONFIG_CHESS_ENABLE_WEB_SERVER
httpd_handle_t web_server_get_httpd_handle(void) { return httpd_handle; }
#else
httpd_handle_t web_server_get_httpd_handle(void) { return NULL; }
#endif

const char *web_server_get_ap_ssid(void) {
#if !CONFIG_CHESS_ENABLE_WEB_SERVER
  return "(HTTP disabled)";
#else
  if (wifi_ap_ssid_effective[0] == '\0') {
    return WIFI_AP_SSID_BASE;
  }
  return wifi_ap_ssid_effective;
#endif
}

uint32_t web_server_get_client_count(void) { return client_count; }

esp_err_t web_server_get_last_http_error(void) { return last_http_start_error; }

uint32_t web_server_get_uptime(void) {
  if (!web_server_active) {
    return 0;
  }

  uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  return current_time - web_server_start_time;
}

void web_server_log_request(const char *method, const char *path) {
  if (method == NULL || path == NULL) {
    return;
  }

  ESP_LOGI(TAG, "HTTP Request: %s %s", method, path);
}

void web_server_log_error(const char *error_message) {
  if (error_message == NULL) {
    return;
  }

  ESP_LOGE(TAG, "Web Server Error: %s", error_message);
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

void web_server_set_port(uint16_t port) {
  ESP_LOGI(TAG, "Setting web server port to %d", port);
  ESP_LOGI(TAG, "Web server port updated to %d", port);
}

void web_server_set_max_clients(uint32_t max_clients) {
  ESP_LOGI(TAG, "Setting web server max clients to %lu", max_clients);
  ESP_LOGI(TAG, "Web server max clients updated to %lu", max_clients);
}

void web_server_enable_ssl(bool enable) {
  ESP_LOGI(TAG, "Setting web server SSL to %s",
           enable ? "enabled" : "disabled");
  ESP_LOGI(TAG, "Web server SSL %s", enable ? "enabled" : "disabled");
}

// ============================================================================
// STATUS AND CONTROL FUNCTIONS
// ============================================================================

bool web_server_is_task_running(void) { return task_running; }

void web_server_stop_task(void) {
  task_running = false;
  ESP_LOGI(TAG, "Web server task stop requested");
}

void web_server_reset(void) {
  ESP_LOGI(TAG, "Resetting web server...");

  web_server_active = false;
  web_server_start_time = 0;
  client_count = 0;

  ESP_LOGI(TAG, "Web server reset completed");
}

/* Included last so lwIP socket macros do not rewrite the rest of this file. */
#include "lwip/sockets.h"

extern int __real_lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen);

int __wrap_lwip_accept(int s, struct sockaddr *addr, socklen_t *addrlen) {
  int fd = __real_lwip_accept(s, addr, addrlen);
  /* ~19 KiB steady is healthy. Nap only when lwIP is actually out of RAM,
   * and only once per 3 s so aborted clients still get drained. */
  if (fd >= 0 || !web_http_heap_critical()) {
    return fd;
  }
  static TickType_t s_nap;
  TickType_t now = xTaskGetTickCount();
  if (s_nap != 0 && (now - s_nap) < pdMS_TO_TICKS(3000)) {
    return fd;
  }
  s_nap = now;
  vTaskDelay(pdMS_TO_TICKS(200));
  return fd;
}
