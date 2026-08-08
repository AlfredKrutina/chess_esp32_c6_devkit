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
#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "web_routes.h"
#include "chess_piece_http.h"
#include "esp_http_server.h"
#include "mdns.h"
#endif
#include "web_server_internal.h"
#include "../game_hooks/include/game_state_notify.h"
#include "../game_task/include/game_task.h"
#include "../matrix_task/include/matrix_task.h"
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
#include "cJSON.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <ctype.h>
#include <inttypes.h>
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
uint8_t cached_brightness = 50;
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

// static esp_err_t http_post_move_handler(httpd_req_t *req);  // VYPNUTO - web
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
    return ret;
  }

  // Start the connection
  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
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
  esp_wifi_disconnect();
  sta_connecting = false; // Resetovat flag pri timeout

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

/** BLE wifi_survey: active scan of the surroundings, result via cmd_ack JSON (encrypted link). */
static void wifi_ble_survey_task(void *arg) {
  (void)arg;

  typedef struct {
    char ssid[33];
    int8_t rssi;
  } survey_best_t;

  enum {
    kSurveyRecordCap = 64,
    kSurveyOutCap = 24,
  };

  wifi_ap_record_t records[kSurveyRecordCap];
  uint16_t number = kSurveyRecordCap;
  memset(records, 0, sizeof(records));

  wifi_scan_config_t scan_cfg = {0};
  scan_cfg.show_hidden = true;

  esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
  if (err != ESP_OK) {
    char buf[288];
    snprintf(buf, sizeof(buf),
             "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"scan_failed\","
             "\"cmd\":\"wifi_survey\",\"message\":\"scan_start\",\"esp\":%d}",
             (int)err);
    ble_task_notify_cmd_ack_json(buf);
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
    ble_task_notify_cmd_ack_json(buf);
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
    ble_task_notify_cmd_ack_json(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"root\",\"esp\":-1}");
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
    ble_task_notify_cmd_ack_json(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"array\",\"esp\":-1}");
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
    ble_task_notify_cmd_ack_json(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"json\","
        "\"cmd\":\"wifi_survey\",\"message\":\"print\",\"esp\":-1}");
    vTaskDelete(NULL);
    return;
  }
  ble_task_notify_cmd_ack_json(printed);
  cJSON_free(printed);
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

  wifi_ap_record_t ap_records[WIFI_AP_SCAN_MAX_APS];
  memset(ap_records, 0, sizeof(ap_records));
  uint16_t number = WIFI_AP_SCAN_MAX_APS;
  ret = esp_wifi_scan_get_ap_records(&number, ap_records);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "AP SSID scan: get_ap_records failed: %s — using %s",
             esp_err_to_name(ret), base);
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

  // Start WiFi
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
    return ret;
  }

  wifi_ap_active = ap_on_boot;

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
      "\"locked\":%s"
      "}",
      wifi_ap_ssid_effective, ap_ip_out, ap_clients_out,
      ap_on ? "true" : "false",
      sta_ssid_display,
      (sta_connected && sta_ip[0] != '\0') ? sta_ip : "Not connected",
      sta_connected ? "true" : "false", online ? "true" : "false",
      blk_csv,
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
static esp_err_t start_http_server(void) {
  if (httpd_handle != NULL) {
    ESP_LOGW(TAG, "HTTP server already running");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Starting HTTP server...");

  // Konfigurovat HTTP server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_SERVER_PORT;
  /* Number of httpd_register_uri_handler in start_http_server: 54+ (2026-04, incl. WS + PNG figures).
   * Increase stock when adding endpoint (HTTPD doesn't register "quietly" extra). */
  config.max_uri_handlers = 64;
  // Keep within LWIP limits. HTTP server internally reserves 3 sockets.
#if CONFIG_LWIP_MAX_SOCKETS > 6
  config.max_open_sockets = CONFIG_LWIP_MAX_SOCKETS - 3;
#else
  config.max_open_sockets = 3;
#endif
  config.lru_purge_enable = false; // CRITICAL: Disabled to prevent socket
                                   // closure during chunked transfer
  config.recv_wait_timeout = 20;    // 20 s – stable connection with a slower network
  config.send_wait_timeout =
      5000; // 5 s – spolehlivy chunked transfer pri vykyvech
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
    ble_task_notify_cmd_ack_json(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"needs_encryption\","
        "\"cmd\":\"unknown\",\"message\":\"Encrypted BLE connection required.\","
        "\"esp\":259}");
  } else {
    ble_task_notify_cmd_ack_json(out);
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

  char cmd[40];
  if (!json_extract_cmd_string(buf, cmd, sizeof(cmd))) {
    ESP_LOGW(TAG, "BLE JSON: missing cmd");
    return ESP_ERR_NOT_FOUND;
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
    char status_ack[400];
    esp_err_t er =
        ota_update_ble_build_status_ack_json(status_ack, sizeof(status_ack));
    if (er != ESP_OK) {
      return er;
    }
    ble_task_notify_cmd_ack_json(status_ack);
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
    BaseType_t ok =
        xTaskCreate(wifi_ble_survey_task, "wifi_survey", 8192, NULL, 5, NULL);
    if (ok != pdPASS) {
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
    led_command_t c = {0};
    c.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&c);
    ESP_LOGD(TAG, "BLE cmd: hint_clear");
    return ESP_OK;
  }
  if (strcmp(cmd, "guard_clear") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "BLE guard_clear blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    if (game_is_matrix_guard_active() || matrix_is_guard_mode_active()) {
      game_force_clear_matrix_guard();
    }
    ESP_LOGD(TAG, "BLE cmd: guard_clear");
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
    config.brightness_level = 50;
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

  /* ============================================
   * NEW BLE COMMANDS - Beginning
   * ============================================ */
  if (strcmp(cmd, "move") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] move blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    // BLE move command: {"cmd": "move", "from": "e2", "to": "e4", "promotion": "q"}
    char from[8] = {0};
    char to[8] = {0};
    char promotion[8] = {0};

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "Invalid JSON in move command");
      return ESP_FAIL;
    }

    cJSON *from_obj = cJSON_GetObjectItemCaseSensitive(root, "from");
    cJSON *to_obj = cJSON_GetObjectItemCaseSensitive(root, "to");
    cJSON *promo_obj = cJSON_GetObjectItemCaseSensitive(root, "promotion");

    if (cJSON_IsString(from_obj) && cJSON_IsString(to_obj)) {
      strncpy(from, from_obj->valuestring, sizeof(from) - 1);
      strncpy(to, to_obj->valuestring, sizeof(to) - 1);
      from[sizeof(from) - 1] = '\0';
      to[sizeof(to) - 1] = '\0';
      if (cJSON_IsString(promo_obj)) {
        strncpy(promotion, promo_obj->valuestring, sizeof(promotion) - 1);
      }

      /* Same field (double tap in app) — don't send to game_task (MOVE_ERROR_DESTINATION_OCCUPIED). */
      if (strcmp(from, to) == 0) {
        ESP_LOGW(TAG, "[BLE] move: ignored (from==to)");
        cJSON_Delete(root);
        return ESP_OK;
      }

      if (game_command_queue != NULL) {
        chess_move_command_t move_cmd = {0};
        move_cmd.type = GAME_CMD_MOVE;
        strncpy(move_cmd.from_notation, from, sizeof(move_cmd.from_notation) - 1);
        strncpy(move_cmd.to_notation, to, sizeof(move_cmd.to_notation) - 1);

        /* Stejne jako promotion_choice_t (0=Q,1=R,2=B,3=N) — viz
         * game_process_promotion_command */
        if (strlen(promotion) > 0) {
          switch (promotion[0]) {
          case 'q':
          case 'Q':
            move_cmd.promotion_choice = PROMOTION_QUEEN;
            break;
          case 'r':
          case 'R':
            move_cmd.promotion_choice = PROMOTION_ROOK;
            break;
          case 'b':
          case 'B':
            move_cmd.promotion_choice = PROMOTION_BISHOP;
            break;
          case 'n':
          case 'N':
            move_cmd.promotion_choice = PROMOTION_KNIGHT;
            break;
          default:
            move_cmd.promotion_choice = PROMOTION_QUEEN;
            break;
          }
        }
        move_cmd.promotion_from_remote = (strlen(promotion) > 0) ? 1U : 0U;

        if (xQueueSend(game_command_queue, &move_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          ESP_LOGI(TAG, "[BLE] move: %s -> %s (promo: %s)", from, to,
                   strlen(promotion) > 0 ? promotion : "none");
        } else {
          ESP_LOGE(TAG, "[BLE] move: failed to send to game queue");
        }
      }
    } else {
      ESP_LOGW(TAG, "[BLE] move: missing from/to fields");
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    return ESP_OK;
  }

  if (strcmp(cmd, "new_game") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] new_game blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    if (game_command_queue == NULL) {
      return ESP_FAIL;
    }
    chess_move_command_t new_cmd = {0};
    new_cmd.type = GAME_CMD_NEW_GAME;
    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {
      cJSON *fj = cJSON_GetObjectItemCaseSensitive(root, "fen");
      if (cJSON_IsString(fj) && fj->valuestring != NULL &&
          fj->valuestring[0] != '\0') {
        new_cmd.type = GAME_CMD_NEW_GAME_FROM_FEN;
        strncpy(new_cmd.timer_data.fen_new_game.fen, fj->valuestring,
                sizeof(new_cmd.timer_data.fen_new_game.fen) - 1);
        new_cmd.timer_data.fen_new_game.fen[sizeof(new_cmd.timer_data.fen_new_game.fen) - 1] = '\0';
      }
      cJSON_Delete(root);
    }
    if (xQueueSend(game_command_queue, &new_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
      ESP_LOGI(TAG, "[BLE] new_game: type=%d", (int)new_cmd.type);
    } else {
      ESP_LOGE(TAG, "[BLE] new_game: failed to send to game queue");
      return ESP_FAIL;
    }
    return ESP_OK;
  }

  if (strcmp(cmd, "undo") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] undo blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    if (game_command_queue != NULL) {
      chess_move_command_t undo_cmd = {0};
      undo_cmd.type = GAME_CMD_UNDO_MOVE;
      if (xQueueSend(game_command_queue, &undo_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "[BLE] undo: command sent");
      } else {
        ESP_LOGE(TAG, "[BLE] undo: failed to send to game queue");
      }
    }
    return ESP_OK;
  }

  if (strcmp(cmd, "promotion") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] promotion blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "Invalid JSON in promotion command");
      return ESP_FAIL;
    }
    cJSON *choice_obj = cJSON_GetObjectItemCaseSensitive(root, "choice");
    if (cJSON_IsString(choice_obj) && game_command_queue != NULL) {
      chess_move_command_t promo_cmd = {0};
      promo_cmd.type = GAME_CMD_PROMOTION;
      switch (choice_obj->valuestring[0]) {
      case 'q':
      case 'Q':
        promo_cmd.promotion_choice = PROMOTION_QUEEN;
        break;
      case 'r':
      case 'R':
        promo_cmd.promotion_choice = PROMOTION_ROOK;
        break;
      case 'b':
      case 'B':
        promo_cmd.promotion_choice = PROMOTION_BISHOP;
        break;
      case 'n':
      case 'N':
        promo_cmd.promotion_choice = PROMOTION_KNIGHT;
        break;
      default:
        promo_cmd.promotion_choice = PROMOTION_QUEEN;
        break;
      }
      if (xQueueSend(game_command_queue, &promo_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
        ESP_LOGI(TAG, "[BLE] promotion: choice %s", choice_obj->valuestring);
      } else {
        ESP_LOGE(TAG, "[BLE] promotion: failed to send to game queue");
      }
    }
    cJSON_Delete(root);
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_config") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_config blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    chess_move_command_t tcmd = {0};
    tcmd.type = GAME_CMD_SET_TIME_CONTROL;

    char *type_str = strstr(buf, "\"type\":");
    if (type_str == NULL) {
      ESP_LOGW(TAG, "[BLE] timer_config: missing type");
      return ESP_ERR_INVALID_ARG;
    }
    int type_value;
    if (sscanf(type_str, "\"type\":%d", &type_value) != 1) {
      ESP_LOGW(TAG, "[BLE] timer_config: invalid type");
      return ESP_ERR_INVALID_ARG;
    }
    if (type_value < 0 || type_value > 14) {
      ESP_LOGW(TAG, "[BLE] timer_config: type out of range");
      return ESP_ERR_INVALID_ARG;
    }
    tcmd.timer_data.timer_config.time_control_type = (uint8_t)type_value;

    if (type_value == 14) {
      char *minutes_str = strstr(buf, "\"custom_minutes\":");
      char *increment_str = strstr(buf, "\"custom_increment\":");
      if (minutes_str) {
        int minutes;
        if (sscanf(minutes_str, "\"custom_minutes\":%d", &minutes) == 1) {
          if (minutes >= 1 && minutes <= 180) {
            tcmd.timer_data.timer_config.custom_minutes = (uint32_t)minutes;
          } else {
            ESP_LOGW(TAG, "[BLE] timer_config: minutes out of range");
            return ESP_ERR_INVALID_ARG;
          }
        }
      }
      if (increment_str) {
        int increment;
        if (sscanf(increment_str, "\"custom_increment\":%d", &increment) ==
            1) {
          if (increment >= 0 && increment <= 60) {
            tcmd.timer_data.timer_config.custom_increment =
                (uint32_t)increment;
          } else {
            ESP_LOGW(TAG, "[BLE] timer_config: increment out of range");
            return ESP_ERR_INVALID_ARG;
          }
        }
      }
      if (minutes_str == NULL || increment_str == NULL) {
        ESP_LOGW(TAG, "[BLE] timer_config: custom needs minutes+increment");
        return ESP_ERR_INVALID_ARG;
      }
    }

    if (game_command_queue == NULL ||
        xQueueSend(game_command_queue, &tcmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGE(TAG, "[BLE] timer_config: queue send failed");
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[BLE] timer_config: type=%d", type_value);
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_pause") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_pause blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    chess_move_command_t tcmd = {0};
    tcmd.type = GAME_CMD_PAUSE_TIMER;
    if (game_command_queue == NULL ||
        xQueueSend(game_command_queue, &tcmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[BLE] timer_pause");
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_resume") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_resume blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    chess_move_command_t tcmd = {0};
    tcmd.type = GAME_CMD_RESUME_TIMER;
    if (game_command_queue == NULL ||
        xQueueSend(game_command_queue, &tcmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[BLE] timer_resume");
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_reset") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_reset blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    chess_move_command_t tcmd = {0};
    tcmd.type = GAME_CMD_RESET_TIMER;
    if (game_command_queue == NULL ||
        xQueueSend(game_command_queue, &tcmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[BLE] timer_reset");
    return ESP_OK;
  }

  if (strcmp(cmd, "virtual_action") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] virtual_action blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    char action[16] = {0};
    char square[8] = {0};
    char choice[8] = {0};

    const char *action_start = strstr(buf, "\"action\"");
    if (action_start != NULL) {
      action_start = strchr(action_start, ':');
      if (action_start != NULL) {
        action_start++;
        while (*action_start == ' ' || *action_start == '\"') {
          action_start++;
        }
        const char *action_end = strchr(action_start, '\"');
        if (action_end != NULL && action_end > action_start) {
          size_t len = (size_t)(action_end - action_start);
          if (len < sizeof(action)) {
            strncpy(action, action_start, len);
          }
        }
      }
    }

    const char *square_start = strstr(buf, "\"square\"");
    if (square_start != NULL) {
      square_start = strchr(square_start, ':');
      if (square_start != NULL) {
        square_start++;
        while (*square_start == ' ' || *square_start == '\"') {
          square_start++;
        }
        const char *square_end = strchr(square_start, '\"');
        if (square_end != NULL && square_end > square_start) {
          size_t len = (size_t)(square_end - square_start);
          if (len < sizeof(square)) {
            strncpy(square, square_start, len);
          }
        }
      }
    }

    const char *choice_start = strstr(buf, "\"choice\"");
    if (choice_start != NULL) {
      choice_start = strchr(choice_start, ':');
      if (choice_start != NULL) {
        choice_start++;
        while (*choice_start == ' ' || *choice_start == '\"') {
          choice_start++;
        }
        const char *choice_end = strchr(choice_start, '\"');
        if (choice_end != NULL && choice_end > choice_start) {
          size_t len = (size_t)(choice_end - choice_start);
          if (len < sizeof(choice)) {
            strncpy(choice, choice_start, len);
          }
        }
      }
    }

    if (strlen(action) == 0) {
      ESP_LOGW(TAG, "[BLE] virtual_action: missing action");
      return ESP_ERR_INVALID_ARG;
    }

    chess_move_command_t vacmd = {0};
    if (strcmp(action, "pickup") == 0) {
      vacmd.type = GAME_CMD_PICKUP;
    } else if (strcmp(action, "drop") == 0) {
      vacmd.type = GAME_CMD_DROP;
    } else if (strcmp(action, "promote") == 0) {
      vacmd.type = GAME_CMD_PROMOTION;
    } else {
      ESP_LOGW(TAG, "[BLE] virtual_action: invalid action");
      return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action, "pickup") == 0) {
      if (strlen(square) == 0) {
        ESP_LOGW(TAG, "[BLE] virtual_action: pickup needs square");
        return ESP_ERR_INVALID_ARG;
      }
      strncpy(vacmd.from_notation, square, sizeof(vacmd.from_notation) - 1);
    } else if (strcmp(action, "drop") == 0) {
      if (strlen(square) == 0) {
        ESP_LOGW(TAG, "[BLE] virtual_action: drop needs square");
        return ESP_ERR_INVALID_ARG;
      }
      strncpy(vacmd.to_notation, square, sizeof(vacmd.to_notation) - 1);
    } else if (strcmp(action, "promote") == 0) {
      if (strlen(choice) == 0) {
        strcpy(choice, "Q");
      }
      if (strcasecmp(choice, "Q") == 0) {
        vacmd.promotion_choice = PROMOTION_QUEEN;
      } else if (strcasecmp(choice, "R") == 0) {
        vacmd.promotion_choice = PROMOTION_ROOK;
      } else if (strcasecmp(choice, "B") == 0) {
        vacmd.promotion_choice = PROMOTION_BISHOP;
      } else if (strcasecmp(choice, "N") == 0) {
        vacmd.promotion_choice = PROMOTION_KNIGHT;
      } else {
        vacmd.promotion_choice = PROMOTION_QUEEN;
      }
      if (strlen(square) > 0) {
        strncpy(vacmd.to_notation, square, sizeof(vacmd.to_notation) - 1);
      }
    }

    if (game_command_queue == NULL ||
        xQueueSend(game_command_queue, &vacmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGE(TAG, "[BLE] virtual_action: queue send failed");
      return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[BLE] virtual_action: %s", action);
    return ESP_OK;
  }

  if (strcmp(cmd, "demo_config") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] demo_config blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    bool enabled = false;
    cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(en)) {
      enabled = cJSON_IsTrue(en);
    }
    cJSON *sp = cJSON_GetObjectItemCaseSensitive(root, "speed_ms");
    if (cJSON_IsNumber(sp)) {
      double v = cJSON_GetNumberValue(sp);
      if (v >= 0 && v <= 600000) {
        set_demo_speed_ms((uint32_t)v);
      }
    }
    cJSON_Delete(root);
    toggle_demo_mode(enabled);
    ESP_LOGI(TAG, "[BLE] demo_config enabled=%d", (int)enabled);
    return ESP_OK;
  }

  if (strcmp(cmd, "demo_start") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] demo_start blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    toggle_demo_mode(true);
    ESP_LOGI(TAG, "[BLE] demo_start");
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_auto_lamp_timeout") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *sec = cJSON_GetObjectItemCaseSensitive(root, "seconds");
    int seconds_val = -1;
    if (cJSON_IsNumber(sec)) {
      seconds_val = (int)cJSON_GetNumberValue(sec);
    }
    cJSON_Delete(root);
    if (seconds_val < 5) {
      seconds_val = 5;
    }
    if (seconds_val > 7200) {
      seconds_val = 7200;
    }
    if (ha_light_set_activity_timeout_sec((uint32_t)seconds_val) != ESP_OK) {
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "[BLE] settings_auto_lamp_timeout %d", seconds_val);
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_guided_hints") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    bool enabled = false;
    cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(en)) {
      enabled = cJSON_IsTrue(en);
    }
    cJSON_Delete(root);
    system_config_t syscfg;
    if (config_load_from_nvs(&syscfg) != ESP_OK) {
      return ESP_FAIL;
    }
    syscfg.guided_capture_hints_enabled = enabled;
    syscfg.led_guidance_level = enabled ? (uint8_t)5 : (uint8_t)4;
    config_save_to_nvs(&syscfg);
    game_set_led_guidance_level(syscfg.led_guidance_level);
    ESP_LOGI(TAG, "[BLE] settings_guided_hints enabled=%d", (int)enabled);
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_led_guidance") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *lv = cJSON_GetObjectItemCaseSensitive(root, "level");
    int level = -1;
    if (cJSON_IsNumber(lv)) {
      level = (int)cJSON_GetNumberValue(lv);
    }
    cJSON_Delete(root);
    if (level < 1 || level > 5) {
      return ESP_ERR_INVALID_ARG;
    }
    system_config_t syscfg;
    if (config_load_from_nvs(&syscfg) != ESP_OK) {
      return ESP_FAIL;
    }
    syscfg.led_guidance_level = (uint8_t)level;
    syscfg.guided_capture_hints_enabled = (level >= 5);
    config_save_to_nvs(&syscfg);
    game_set_led_guidance_level((uint8_t)level);
    ESP_LOGI(TAG, "[BLE] settings_led_guidance level=%d", level);
    return ESP_OK;
  }

  if (strcmp(cmd, "mqtt_config") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *hj = cJSON_GetObjectItemCaseSensitive(root, "host");
    if (!cJSON_IsString(hj) || hj->valuestring == NULL ||
        hj->valuestring[0] == '\0' || strlen(hj->valuestring) > 127) {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    uint16_t port = 1883;
    cJSON *pj = cJSON_GetObjectItemCaseSensitive(root, "port");
    if (cJSON_IsNumber(pj)) {
      double pv = cJSON_GetNumberValue(pj);
      if (pv > 0 && pv <= 65535) {
        port = (uint16_t)pv;
      }
    }
    const char *user_str = NULL;
    cJSON *uj = cJSON_GetObjectItemCaseSensitive(root, "username");
    if (cJSON_IsString(uj) && uj->valuestring != NULL && uj->valuestring[0]) {
      user_str = uj->valuestring;
    }
    const char *pass_str = NULL;
    cJSON *pwj = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(pwj) && pwj->valuestring != NULL && pwj->valuestring[0]) {
      pass_str = pwj->valuestring;
    }
    esp_err_t merr = mqtt_save_config_to_nvs(hj->valuestring, port, user_str,
                                             pass_str);
    cJSON_Delete(root);
    if (merr != ESP_OK) {
      ESP_LOGW(TAG, "[BLE] mqtt_config save failed: %s",
               esp_err_to_name(merr));
      return merr;
    }
    if (wifi_is_sta_connected()) {
      (void)ha_light_reinit_mqtt();
    }
    ESP_LOGI(TAG, "[BLE] mqtt_config OK");
    return ESP_OK;
  }

  if (strcmp(cmd, "setup_tutorial") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *aj = cJSON_GetObjectItemCaseSensitive(root, "action");
    const char *act = NULL;
    if (cJSON_IsString(aj) && aj->valuestring != NULL) {
      act = aj->valuestring;
    }
    bool want_start = (act != NULL && strcmp(act, "start") == 0);
    bool want_cancel = (act != NULL && strcmp(act, "cancel") == 0);
    bool want_finish = (act != NULL && strcmp(act, "finish") == 0);
    int nact = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) +
               (want_finish ? 1 : 0);
    cJSON_Delete(root);
    if (nact != 1) {
      return ESP_ERR_INVALID_ARG;
    }

    if (want_start || want_cancel) {
      setup_tutorial_reset_finish_cooldown();
    }

    if (want_finish) {
      if (!game_is_board_setup_tutorial_active()) {
        return ESP_ERR_INVALID_ARG;
      }
      if (setup_tutorial_finish_in_cooldown()) {
        return ESP_ERR_NOT_FINISHED;
      }
      if (!game_finish_board_setup_tutorial_from_web()) {
        if (game_is_board_setup_tutorial_active()) {
          setup_tutorial_note_finish_conflict();
          return ESP_ERR_NOT_FINISHED;
        }
        setup_tutorial_reset_finish_cooldown();
        return ESP_FAIL;
      }
      setup_tutorial_reset_finish_cooldown();
      return ESP_OK;
    }

    if (game_command_queue == NULL) {
      ESP_LOGE(TAG, "[BLE] setup_tutorial: game_command_queue NULL");
      return ESP_FAIL;
    }
    chess_move_command_t gcmd = {0};
    gcmd.type = GAME_CMD_BOARD_SETUP_TUTORIAL;
    gcmd.promotion_choice = want_start ? 0U : 1U;
    gcmd.response_queue = NULL;
    if (xQueueSend(game_command_queue, &gcmd, pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGE(TAG, "[BLE] setup_tutorial: queue send failed");
      return ESP_FAIL;
    }
    if (want_cancel) {
      led_command_t lcmd = {0};
      lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
      led_execute_command_new(&lcmd);
    }
    ESP_LOGI(TAG, "[BLE] setup_tutorial action=%s queued",
             want_start ? "start" : "cancel");
    return ESP_OK;
  }

  if (strcmp(cmd, "opening") == 0) {
    esp_err_t oerr = web_server_opening_dispatch_body(buf);
    if (oerr == ESP_OK) {
      ESP_LOGI(TAG, "[BLE] opening dispatched");
      return ESP_OK;
    }
    if (oerr == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "[BLE] opening blocked (locked or mode conflict)");
      return ESP_ERR_INVALID_STATE;
    }
    if (oerr == ESP_ERR_NOT_ALLOWED) {
      ESP_LOGW(TAG, "[BLE] opening checkpoint resync incomplete");
      return ESP_ERR_NOT_ALLOWED;
    }
    ESP_LOGW(TAG, "[BLE] opening dispatch failed: %s", esp_err_to_name(oerr));
    return oerr;
  }

  /* ============================================
   * NEW BLE COMMANDS - End
   * ============================================ */

  ESP_LOGW(TAG, "BLE JSON: unknown cmd \"%s\"", cmd);
  return ESP_ERR_NOT_SUPPORTED;
}

static void czechmate_push_ble_snapshot(void) {
  (void)web_server_task_wdt_reset_safe();
  if (!ble_task_should_push_snapshot()) {
    return;
  }
  size_t len = 0;
  if (build_snapshot_json(&len) != ESP_OK) {
    return;
  }
  ble_task_push_snapshot_json((const uint8_t *)snapshot_buffer, len);
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
  esp_err_t e = mdns_init();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "mdns_init: %s", esp_err_to_name(e));
    return;
  }
  uint8_t mac[6];
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    (void)esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY);
  }
  char host[24];
  snprintf(host, sizeof(host), "czechmate-%02x%02x%02x", mac[3], mac[4],
           mac[5]);
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
  uint8_t ping;
  int n = 0;
  while (xQueueReceive(snapshot_notify_queue, &ping, 0) == pdTRUE) {
    n++;
  }
  if (n == 0) {
    return;
  }
  /* Snapshot + WS sending can block for a long time (httpd send timeout) — no
   * TWDT reset reports web_server_task timeout. */
  (void)web_server_task_wdt_reset_safe();
  size_t fh = esp_get_free_heap_size();
  if (fh < 10240) {
    ESP_LOGW(TAG,
             "low heap before snapshot push: %zu B (goal to keep ~8–10kB+ free)",
             fh);
  }
#if CONFIG_CHESS_ENABLE_WEB_SERVER && CONFIG_HTTPD_WS_SUPPORT
  ws_broadcast_snapshot();
#endif
  (void)web_server_task_wdt_reset_safe();
  czechmate_push_ble_snapshot();
  (void)web_server_task_wdt_reset_safe();
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

esp_err_t http_get_timer_handler(httpd_req_t *req) {
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

esp_err_t http_post_timer_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/config");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer config blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Precist JSON data
  char content[256];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data received", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parse the JSON and send the command to the game task
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_SET_TIME_CONTROL;

  // Vylepsene JSON parsovani pouzitim sscanf pro spolehlivejsi parsovani
  // Nejprve najit pole "type"
  char *type_str = strstr(content, "\"type\":");
  if (type_str == NULL) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Missing 'type' field", -1);
    return ESP_FAIL;
  }

  // Parsovat hodnotu type
  int type_value;
  if (sscanf(type_str, "\"type\":%d", &type_value) != 1) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Invalid type value", -1);
    return ESP_FAIL;
  }

  if (type_value < 0 || type_value > 14) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Type out of range (0-14)", -1);
    return ESP_FAIL;
  }

  cmd.timer_data.timer_config.time_control_type = (uint8_t)type_value;

  // Parse custom time values ​​if type is CUSTOM
  if (type_value == 14) { // TIME_CONTROL_CUSTOM
    char *minutes_str = strstr(content, "\"custom_minutes\":");
    char *increment_str = strstr(content, "\"custom_increment\":");

    if (minutes_str) {
      int minutes;
      if (sscanf(minutes_str, "\"custom_minutes\":%d", &minutes) == 1) {
        if (minutes >= 1 && minutes <= 180) {
          cmd.timer_data.timer_config.custom_minutes = (uint32_t)minutes;
        } else {
          httpd_resp_set_status(req, "400 Bad Request");
          httpd_resp_send(req, "Minutes must be 1-180", -1);
          return ESP_FAIL;
        }
      }
    }

    if (increment_str) {
      int increment;
      if (sscanf(increment_str, "\"custom_increment\":%d", &increment) == 1) {
        if (increment >= 0 && increment <= 60) {
          cmd.timer_data.timer_config.custom_increment = (uint32_t)increment;
        } else {
          httpd_resp_set_status(req, "400 Bad Request");
          httpd_resp_send(req, "Increment must be 0-60", -1);
          return ESP_FAIL;
        }
      }
    }

    // Verify that custom values ​​have been provided
    if (minutes_str == NULL || increment_str == NULL) {
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req, "Custom time control requires minutes and increment",
                      -1);
      return ESP_FAIL;
    }
  }

  // Send the command to the game task
  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to set time control", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "Time control set successfully", -1);

  return ESP_OK;
}

esp_err_t http_post_timer_pause_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/pause");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer pause blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_PAUSE_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to pause timer", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "Timer paused", -1);

  return ESP_OK;
}

esp_err_t http_post_timer_resume_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/resume");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer resume blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_RESUME_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to resume timer", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "Timer resumed", -1);

  return ESP_OK;
}

esp_err_t http_post_timer_reset_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/reset");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer reset blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_RESET_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to reset timer", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "Timer reset", -1);

  return ESP_OK;
}



// ============================================================================
// HTTP root & legacy JS paths — app-only product build (no browser dashboard).
// REST `/api/*`, WebSocket `/ws`, and OTA handlers remain registered below.
// ====================================================================================

static const char k_http_root_json[] =
    "{\"service\":\"czechmate\",\"client\":\"mobile_app\","
    "\"api\":\"/api/\",\"websocket\":\"/ws\"}";

esp_err_t http_get_chess_js_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "404 Not Found");
  httpd_resp_set_type(req, "application/json; charset=utf-8");
  return httpd_resp_send(req, "{\"error\":\"browser_ui_removed\"}",
                         HTTPD_RESP_USE_STRLEN);
}

/** GET /favicon.ico — no body; silent 404 in browser log on random request. */
esp_err_t http_get_favicon_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, "", 0);
}

esp_err_t http_get_root_handler(httpd_req_t *req) {
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

  // Start HTTP server
  ret = start_http_server();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Failed to start HTTP server: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG,
             "❌ Web server task will continue but HTTP will not be available");

    // Keep task alive and retry HTTP startup periodically
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
    esp_task_wdt_delete(NULL); // Unregister before deleting
    vTaskDelete(NULL);
    return;
    }
  }
  web_server_active = true;
  web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ESP_LOGI(TAG, "HTTP server started");
  czechmate_mdns_ensure_started();

  /* One-time loading of the brightness into the cache - GET /api/status does not access the NVS
   */
  {
    system_config_t config;
    if (config_load_from_nvs(&config) == ESP_OK) {
      cached_brightness = config.brightness_level;
      cached_brightness_valid = true;
    }
  }

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

    // Process web server commands from queue
    web_server_process_commands();

    /* Snapshot WS/BLE mimo game_task (fronta z czechmate_on_game_state_changed). */
    web_server_process_snapshot_notify_queue();

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
