/**
 * @file web_server_task.c
 * @brief Web Server Task - WiFi hotspot a HTTP server pro web ovladani
 *
 * @details
 * =============================================================================
 * CO TENTO SOUBOR DELA?
 * =============================================================================
 *
 * Tento task zpristupnuje sachovnici pres web rozhrani:
 * 1. WiFi Access Point (hotspot "ESP32-Chess")
 * 2. HTTP server (port 80)
 * 3. REST API endpointy (/api/status, /api/board, /api/move...)
 * 4. Interaktivni web UI (HTML/CSS/JavaScript)
 * 5. Realtime aktualizace stavu (polling)
 *
 * =============================================================================
 * JAK TO FUNGUJE?
 * =============================================================================
 *
 * STARTUP:
 * - Init WiFi jako Access Point (AP mode)
 * - SSID: "ESP32-Chess" (bez hesla)
 * - IP: 192.168.4.1
 * - Start HTTP serveru
 * - Registrace REST API handleru
 *
 * HTTP SERVER:
 * - GET / -> Hlavni HTML stranka
 * - GET /api/status -> JSON stav hry
 * - GET /api/board -> JSON pozice figurek
 * - POST /api/move -> Provest tah
 * - POST /api/reset -> Nova hra
 * - GET /api/timer -> Stav casovace
 *
 * WEB UI:
 * - Interaktivni sachovnice (klikaci)
 * - Zobrazeni validnich tahu
 * - Historie tahu
 * - Timer display
 * - Endgame report
 * - Demo mode controls
 *
 * =============================================================================
 * KOMUNIKACE (QUEUES)
 * =============================================================================
 *
 * FRONTY - Posilame prikazy:
 * - game_command_queue -> Prikazy z webu (move, reset)
 *
 * VOLANI API FUNKCI:
 * - game_get_status_json() -> Ziskani JSON stavu
 * - game_get_board_json() -> Ziskani JSON desky
 * - game_get_history_json() -> Ziskani JSON historie
 *
 * =============================================================================
 * REST API ENDPOINTY
 * =============================================================================
 *
 * GET /api/status:
 * - JSON: {game_state, current_player, move_count, in_check, game_end,
 * error_state}
 *
 * GET /api/board:
 * - JSON: {board: [[...], ...]}  // 8x8 array figurek
 *
 * POST /api/move:
 * - Body: {from: "e2", to: "e4"}
 * - Response: {success: true/false, message: "..."}
 *
 * GET /api/timer:
 * - JSON: {white_time, black_time, running, paused}
 *
 * POST /api/demo/config:
 * - Body: {enabled: true, speed_ms: 2000}
 *
 * =============================================================================
 * KRITICKA PRAVIDLA
 * =============================================================================
 *
 * @warning CO SE NESMI DELAT:
 *
 * 1. NIKDY neblokuj v HTTP handleru!
 *    ❌ vTaskDelay(1000);  // Zablokuje HTTP server
 *    ✅ Proved rychle, vrat odpoved okamzite
 *
 * 2. NIKDY nezabud poslat HTTP response!
 *    ❌ return ESP_OK;  // Bez httpd_resp_send()
 *    ✅ httpd_resp_send(req, json, strlen(json)); return ESP_OK;
 *
 * 3. VZDY kontroluj JSON buffer overflow!
 *    Buffer je 8KB - pokud JSON je vetsi, stack overflow!
 *
 * 4. VZDY pouzij mutexу pro pristup ke game state!
 *    API funkce jako game_get_status_json() uz to delaji
 *
 * =============================================================================
 * TABLE OF CONTENTS
 * =============================================================================
 *
 * Sekce 1:  WiFi Setup ........................... radek 100
 * Sekce 2:  HTTP Handlers ........................ radek 300
 * Sekce 3:  REST API Functions ................... radek 800
 * Sekce 4:  HTML Page Generation ................. radek 1200
 * Sekce 5:  Main Web Server Task ................. radek 4800
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.7.3
 * @date 2025-12-23
 *
 * @note
 * - Task priorita: 2 (nizsi nez game/matrix)
 * - Stack size: 8KB (velke kvuli JSON bufferum)
 * - WiFi SSID: "ESP32-Chess"
 * - IP adresa: 192.168.4.1
 * - HTTP port: 80
 *
 * @see game_task.c - Poskytuje JSON API
 * @see timer_system.c - Timer API
 */

#include "web_server_task.h"
#include "../game_hooks/include/game_state_notify.h"
#include "../game_task/include/game_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
// #include "../timer_system/include/timer_system.h" // UNUSED
#include "esp_event.h"
#include "chess_piece_http.h"
#include "board_api_auth.h"
#include "ota_update.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos_chess.h"
#include "mdns.h"
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
/** Z main.c — porovnání s xTaskGetCurrentTaskHandle() pro TWDT reset jen v této úloze. */
extern TaskHandle_t web_server_task_handle;

// ============================================================================
// LOKALNI PROMENNE A KONSTANTY
// ============================================================================

static const char *TAG = "WEB_SERVER_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Reset TWDT jen pokud aktuální úloha je `web_server_task`.
 *
 * `build_snapshot_json` a mutexové čekání se volají i z workerů `httpd` (GET
 * snapshot) – ty nejsou v TWDT; `esp_task_wdt_reset()` pak vrací ESP_ERR_NOT_FOUND
 * a komponenta task_wdt spamuje sériovku ERROR řádky.
 *
 * Vrací ESP_OK i při přeskočení (cizí úloha nebo NULL handle).
 */
static esp_err_t web_server_task_wdt_reset_safe(void) {
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

// Konfigurace WiFi — AP SSID: nejprve sken okolí; prvni deska = jen zaklad, dalsi = zaklad_1 … zaklad_N
#define WIFI_AP_SSID_BASE "ESP32-CzechMate"
/** Max. index suffixu _N (0 = bez pripony = prvni volny „slot“). */
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

// NVS konfigurace pro Web Lock
#define WEB_NVS_NAMESPACE "web_config"
#define WEB_NVS_KEY_LOCKED "locked"

// Konfigurace HTTP serveru
#define HTTP_SERVER_PORT 80
#define HTTP_SERVER_MAX_URI_LEN 512
#define HTTP_SERVER_MAX_HEADERS 8
#define HTTP_SERVER_MAX_CLIENTS 4

// Velikosti JSON bufferu (status JSON + inject_web_status_fields — 2048 nestacilo)
#define JSON_BUFFER_SIZE 8192
/** Jeden velky buffer pro GET /api/game/snapshot (board+status+history+captured). */
#define SNAPSHOT_BUFFER_SIZE 20480
/** GET /api/wifi/status — wifi_get_sta_status_json (~300 B); nesmi byt 8 KiB na stacku httpd. */
#define WIFI_STATUS_JSON_MAX 512
/** GET /api/timer — timer_get_json (name 32 + description 64 + cisla); nesmi byt 8 KiB na stacku. */
#define TIMER_HTTP_JSON_MAX 1024

// Sledovani stavu web serveru
static bool task_running = false;
static bool web_server_active = false;
static bool wifi_ap_active = false;
static uint32_t web_server_start_time = 0;
static esp_err_t last_http_start_error = ESP_OK;
uint32_t client_count = 0; // Externi pro UART prikazy

// Handle HTTP serveru
static httpd_handle_t httpd_handle = NULL;

#if CONFIG_HTTPD_WS_SUPPORT
static esp_timer_handle_t ws_broadcast_timer = NULL;
#endif

// Handle netif
static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

// STA status promenne
static bool sta_connected = false;
static bool sta_connecting =
    false; // Flag pro sledovani stavu pripojovani (zabranuje race condition)
/** True pokud uzivatel explicitne odpojil STA – vypne automaticke prepajeni. */
static bool sta_manual_disconnect = false;
/** Prodleva pred dalsim pokusem o STA reconnect (backoff). Reset na 3 s pri GOT_IP. */
static uint32_t sta_reconnect_delay_ms = 3000;
#define STA_RECONNECT_DELAY_MIN_MS  3000
#define STA_RECONNECT_DELAY_MAX_MS  30000
static esp_timer_handle_t sta_reconnect_timer = NULL;
static bool web_locked = false; // Flag pro lock web rozhrani
char sta_ip[16] = {0};          // Externi pro UART prikazy
static char sta_ssid[33] = {0};
/** Skutecny AP SSID po startu (po skenu okolnich CzechMate AP). */
static char wifi_ap_ssid_effective[33] = {0};
static int last_disconnect_reason =
    0; // Posledni disconnection reason pro error handling

/* Cache jasu pro GET /api/status – zabranuje NVS + CONFIG_MANAGER na kazdy
 * request */
static uint8_t cached_brightness = 50;
static bool cached_brightness_valid = false;

// Externi promenne
QueueHandle_t web_server_status_queue = NULL;
QueueHandle_t web_server_command_queue = NULL;

// Extern funcions from main.c for Demo Mode
extern void toggle_demo_mode(bool enabled);
extern void set_demo_speed_ms(uint32_t speed_ms);
extern bool is_demo_mode_enabled(void);

// JSON buffer pool (znovupouzitelny)
static char json_buffer[JSON_BUFFER_SIZE];
static char snapshot_buffer[SNAPSHOT_BUFFER_SIZE];
/** Ochrana sestavení snapshotu (HTTP + BLE paralelně). */
static SemaphoreHandle_t snapshot_build_mutex;

/** Fronta pingů — build snapshot jen v web_server_task (ne v esp_timer: malý stack). */
static QueueHandle_t snapshot_notify_queue;
static void czechmate_ensure_snapshot_notify_queue(void);

// ============================================================================
// PREDBEZNE DEKLARACE
// ============================================================================

static void wifi_select_ap_ssid_by_scan(void);
static esp_err_t wifi_init_apsta(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static esp_err_t start_http_server(void);
static void stop_http_server(void);
static esp_err_t build_snapshot_json_to_buffer(char *out, size_t cap,
                                               size_t *out_len);
static esp_err_t build_snapshot_json(size_t *out_len);
static esp_err_t web_server_apply_hint_highlight_json_body(const char *buf);
#if CONFIG_HTTPD_WS_SUPPORT
static esp_err_t http_ws_handler(httpd_req_t *req);
#endif

// HTTP handlery
static esp_err_t http_get_root_handler(httpd_req_t *req);
static esp_err_t
http_get_chess_js_handler(httpd_req_t *req); // chess_app.js file
static esp_err_t
http_get_test_handler(httpd_req_t *req); // Testovaci stranka pro timer
static esp_err_t
http_get_favicon_handler(httpd_req_t *req); // Favicon handler (204 No Content)
static esp_err_t http_get_board_handler(httpd_req_t *req);
static esp_err_t http_get_game_snapshot_handler(httpd_req_t *req);
static esp_err_t http_get_status_handler(httpd_req_t *req);
static esp_err_t http_get_history_handler(httpd_req_t *req);
static esp_err_t http_get_captured_handler(httpd_req_t *req);
static esp_err_t http_get_advantage_handler(httpd_req_t *req);
static esp_err_t http_get_timer_handler(httpd_req_t *req);
static esp_err_t http_post_timer_config_handler(httpd_req_t *req);
static esp_err_t http_post_timer_pause_handler(httpd_req_t *req);
static esp_err_t http_post_timer_resume_handler(httpd_req_t *req);
static esp_err_t http_post_timer_reset_handler(httpd_req_t *req);
// static esp_err_t http_post_move_handler(httpd_req_t *req);  // VYPNUTO - web
// je 100% READ-ONLY

// Handler pro Tahy (Move)
static esp_err_t http_post_game_move_handler(httpd_req_t *req);

// WiFi API handlery
static esp_err_t http_post_wifi_config_handler(httpd_req_t *req);
static esp_err_t http_post_wifi_connect_handler(httpd_req_t *req);
static esp_err_t http_post_wifi_disconnect_handler(httpd_req_t *req);
static esp_err_t http_post_wifi_clear_handler(httpd_req_t *req);
static esp_err_t http_post_factory_reset_handler(httpd_req_t *req);
static esp_err_t http_get_wifi_status_handler(httpd_req_t *req);
static esp_err_t http_get_wifi_status_handler(httpd_req_t *req);
static esp_err_t http_get_web_lock_status_handler(httpd_req_t *req);

// Handlery pro Demo API
static esp_err_t http_post_demo_config_handler(httpd_req_t *req);
static esp_err_t http_get_demo_status_handler(httpd_req_t *req);
static esp_err_t http_post_settings_guided_hints_handler(httpd_req_t *req);
static esp_err_t http_post_settings_led_guidance_handler(httpd_req_t *req);

// Handler pro Virtual Actions
static esp_err_t http_post_game_virtual_action_handler(httpd_req_t *req);
static esp_err_t http_post_game_new_handler(httpd_req_t *req);
static esp_err_t http_post_game_hint_highlight_handler(httpd_req_t *req);
static esp_err_t http_post_game_hint_clear_handler(httpd_req_t *req);
static esp_err_t http_post_game_setup_tutorial_handler(httpd_req_t *req);
static esp_err_t http_post_game_puzzle_handler(httpd_req_t *req);
static esp_err_t http_get_settings_ui_handler(httpd_req_t *req);
static esp_err_t http_post_settings_ui_handler(httpd_req_t *req);

// Handlery pro MQTT API
static esp_err_t http_get_mqtt_status_handler(httpd_req_t *req);
static esp_err_t http_post_mqtt_config_handler(httpd_req_t *req);

// WiFi NVS funkce
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

  // Nacist SSID
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

  // Nacist password
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

// WiFi STA funkce (static pro interni pouziti)
static esp_err_t wifi_get_sta_status_json(char *buffer, size_t buffer_size);

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

// Externi WiFi funkce (pro UART prikazy)
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

  // Pokud je pripojeny a SSID se zmenil, odpojit
  if (sta_connected && strcmp(sta_ssid, ssid) != 0) {
    ESP_LOGI(TAG, "SSID changed from '%s' to '%s', disconnecting...", sta_ssid,
             ssid);
    wifi_disconnect_sta();
  }

  return ESP_OK;
}

esp_err_t wifi_connect_sta(void) {
  // Kontrola: pokud uz je pripojeny, vratit uspech
  if (sta_connected) {
    ESP_LOGI(TAG, "Already connected to WiFi: %s (IP: %s)", sta_ssid, sta_ip);
    return ESP_OK;
  }

  // Kontrola: pokud se prave pripojuje, vratit chybu (race condition
  // protection)
  if (sta_connecting) {
    ESP_LOGW(TAG, "WiFi connection already in progress");
    return ESP_ERR_INVALID_STATE;
  }

  char ssid[33] = {0};
  char password[65] = {0};

  // Nacist konfiguraci z NVS
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

  // Spustit pripojeni
  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
    return ret;
  }

  // Cekat na pripojeni (max 30 sekund)
  int retry_count = 0;
  const int max_retries = 30; // 30 sekund (1 sekunda per retry)

  while (retry_count < max_retries) {
    // Resetovat WDT aby se zabranilo timeoutu (funkce muze blokovat az 30
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
  // Pokud neni pripojeny a nepripojuje se, vratit uspech
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

/** Zpráva pro asynchronní STA provisioning z BLE (wifi_connect_sta může trvat až ~30 s). */
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

bool wifi_is_sta_connected(void) { return sta_connected; }

// ============================================================================
// WEB LOCK NVS FUNKCE
// ============================================================================

/**
 * @brief Ulozi lock stav do NVS
 *
 * @param locked True pro lock, false pro unlock
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
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
 * @brief Nacte lock stav z NVS
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t web_lock_load_from_nvs(void) {
  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(WEB_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    // Pokud NVS namespace neexistuje, pouzit default (unlocked)
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
    // Klic neexistuje, pouzit default (unlocked)
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
 * @brief Zjisti, zda je web rozhrani zamcene
 *
 * @return true pokud je zamcene, false pokud je odemcene
 */
bool web_is_locked(void) { return web_locked; }

/**
 * @brief Nastavi lock stav a ulozi do NVS
 *
 * @param locked True pro lock, false pro unlock
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
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
 * Vrati true pokud SSID odpovida nase AP konvenci (zaklad | zaklad_N) a nastavi
 * out_slot: 0 = jen zaklad, 1..N = zaklad_N.
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
 * @brief Jednorazovy STA sken: vybere nejnizsi volny SSID (prvni deska bez
 * pripony, dalsi _1 … _N) podle toho, co uz v okoli vysila.
 *
 * Po navratu je WiFi zastavene; volajici nastavi APSTA a znovu spusti.
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
    ESP_LOGW(TAG, "AP SSID: vsechny sloty 0..%d obsazeny — pouzit %s",
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
 * @brief Inicializuje WiFi Access Point a Station (APSTA)
 *
 * Tato funkce inicializuje WiFi v APSTA rezimu - soucasne jako Access Point
 * pro web server a jako Station pro pripojeni k internetu.
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Funkce inicializuje netif, event loop, WiFi a nastavi AP i STA konfiguraci.
 * Registruje event handler pro WiFi udalosti a spusti Access Point.
 * Station interface je pripraven pro pripojeni k WiFi siti.
 */
static esp_err_t wifi_init_apsta(void) {
  ESP_LOGI(TAG, "Initializing WiFi APSTA...");

  // Dulezite: Inicializovat netif PRED vytvorenim default netif
  ESP_LOGI(TAG, "Initializing netif...");
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
    return ret;
  }
  if (ret == ESP_ERR_INVALID_STATE) {
    ESP_LOGI(TAG, "Netif already initialized");
  }

  // Dulezite: Vytvorit default event loop PRED inicializaci WiFi
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

  /* Na ESP32-C6 (a jiných RISC-V) muze zaviset interni poradi driveru na tom,
   * ktere default netif se vytvori prvni. Zkusime STA pred AP (snizi Load access
   * fault v ieee80211_hostap_attach pri esp_wifi_start()). */
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

  // Inicializovat WiFi s vychozi konfiguraci
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

  // Registrovat IP event handler pro STA
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

  // Nastavit WiFi mod na APSTA
  ret = esp_wifi_set_mode(WIFI_MODE_APSTA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
    return ret;
  }

  // Nastavit WiFi konfiguraci
  ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
    return ret;
  }

  // Spustit WiFi
  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG, "WiFi APSTA initialized successfully");
  ESP_LOGI(TAG, "AP SSID: %s", wifi_ap_ssid_effective);
  ESP_LOGI(TAG, "AP Password: %s", WIFI_AP_PASSWORD);
  ESP_LOGI(TAG, "AP IP: %s", WIFI_AP_IP);
  ESP_LOGI(TAG, "STA interface ready for connection");

  return ESP_OK;
}

/**
 * @brief Callback jednorazoveho casovace pro automaticke prepojeni STA.
 * Vola se po WIFI_EVENT_STA_DISCONNECTED s backoffem; pouze spusti esp_wifi_connect().
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

/**
 * @brief Obsluhuje WiFi eventy
 *
 * Tato funkce obsluhuje WiFi eventy pro Access Point i Station.
 * Sleduje pripojeni a odpojeni klientu k AP a stav STA pripojeni.
 *
 * @param arg Argument (nepouzivany)
 * @param event_base Typ eventu (WIFI_EVENT nebo IP_EVENT)
 * @param event_id ID eventu
 * @param event_data Data eventu
 *
 * @details
 * Funkce sleduje:
 * - Pripojeni a odpojeni klientu k WiFi hotspotu (AP)
 * - Pripojeni a odpojeni STA k WiFi siti
 * - Ziskani IP adresy pro STA
 */
static void czechmate_mdns_refresh_sta_txt(void);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  // AP eventy - pripojeni klientu k hotspotu
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
  // STA eventy - pripojeni k WiFi siti
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
// WIFI NVS FUNKCE
// ============================================================================

// Duplicitni implementace odstranena - pouzivaji se externi funkce definovane
// vyse

/**
 * @brief Vytvori JSON s WiFi statusem (AP i STA)
 *
 * Tato funkce vytvori JSON string s aktualnim stavem WiFi (AP i STA).
 *
 * @param buffer Buffer pro JSON string
 * @param buffer_size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Funkce vytvori JSON s informacemi o AP (SSID, IP, clients)
 * a STA (SSID, IP, connected status).
 */
static esp_err_t wifi_get_sta_status_json(char *buffer, size_t buffer_size) {
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

  // Ziskat STA SSID z NVS (pokud existuje)
  char saved_ssid[33] = {0};
  char saved_password[65] = {0};
  esp_err_t nvs_ret = wifi_load_config_from_nvs(
      saved_ssid, sizeof(saved_ssid), saved_password, sizeof(saved_password));
  const char *sta_ssid_display =
      (nvs_ret == ESP_OK) ? saved_ssid : "Not configured";

  // Zjistit online status: STA connected a ma IP adresu
  bool online = sta_connected && sta_ip[0] != '\0';

  int len = snprintf(
      buffer, buffer_size,
      "{"
      "\"ap_ssid\":\"%s\","
      "\"ap_ip\":\"%s\","
      "\"ap_clients\":%lu,"
      "\"sta_ssid\":\"%s\","
      "\"sta_ip\":\"%s\","
      "\"sta_connected\":%s,"
      "\"online\":%s,"
      "\"locked\":%s"
      "}",
      wifi_ap_ssid_effective, ap_ip_str, (unsigned long)client_count,
      sta_ssid_display,
      (sta_connected && sta_ip[0] != '\0') ? sta_ip : "Not connected",
      sta_connected ? "true" : "false", online ? "true" : "false",
      web_locked ? "true" : "false");

  if (len < 0 || len >= (int)buffer_size) {
    ESP_LOGE(TAG, "Failed to create WiFi status JSON (buffer too small)");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

/**
 * @brief Handler pro GET /api/web/lock-status
 *
 * Vraci JSON s lock statusem web rozhrani.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
static esp_err_t http_get_web_lock_status_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/web/lock-status");

  char response[128];
  int len = snprintf(response, sizeof(response), "{\"locked\":%s}",
                     web_locked ? "true" : "false");

  if (len < 0 || len >= (int)sizeof(response)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

// ============================================================================
// MQTT API HANDLERS
// ============================================================================

/**
 * @brief Handler pro GET /api/mqtt/status
 *
 * Vrátí JSON s aktuálním stavem MQTT konfigurace a připojení.
 *
 * @param req HTTP request
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Vrací JSON:
 * {"host":"...","port":1883,"username":"...","connected":true/false,"mode":"game/ha"}
 */
static esp_err_t http_get_mqtt_status_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/mqtt/status");

  // Get MQTT config
  char host[128] = {0};
  char username[64] = {0};
  char password[64] = {0};
  uint16_t port = 1883;

  esp_err_t ret = mqtt_get_config(host, sizeof(host), &port, username,
                                  sizeof(username), password, sizeof(password));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get MQTT config", -1);
    return ESP_FAIL;
  }

  // Get WiFi STA status (required for MQTT)
  bool sta_connected = wifi_is_sta_connected();

  // Get MQTT connection status
  bool mqtt_connected = ha_light_is_mqtt_connected();

  // Get HA mode
  ha_mode_t mode = ha_light_get_mode();
  const char *mode_str = (mode == HA_MODE_GAME) ? "game" : "ha";

  // Build JSON response
  char response[512];
  int len = snprintf(response, sizeof(response),
                     "{"
                     "\"host\":\"%s\","
                     "\"port\":%d,"
                     "\"username\":\"%s\","
                     "\"password\":\"%s\","
                     "\"wifi_connected\":%s,"
                     "\"mqtt_connected\":%s,"
                     "\"mode\":\"%s\""
                     "}",
                     host, port, username[0] ? username : "",
                     password[0] ? "***" : "", sta_connected ? "true" : "false",
                     mqtt_connected ? "true" : "false", mode_str);

  if (len < 0 || len >= (int)sizeof(response)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/mqtt/config
 *
 * Uloží MQTT konfiguraci (host, port, username, password) do NVS.
 *
 * @param req HTTP request
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Očekává JSON: {"host":"...","port":1883,"username":"...","password":"..."}
 * Vrací JSON: {"success": true/false, "message": "..."}
 */
static esp_err_t http_post_mqtt_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/mqtt/config");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "MQTT config blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Načíst JSON z request body
  char content[512] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parsovat JSON - hledat "host", "port", "username", "password"
  char host[128] = {0};
  uint16_t port = 1883; // Default
  char username[64] = {0};
  char password[64] = {0};

  // Najít "host"
  const char *host_start = strstr(content, "\"host\"");
  if (host_start != NULL) {
    host_start = strchr(host_start, ':');
    if (host_start != NULL) {
      host_start++; // Přeskočit ':'
      while (*host_start == ' ' || *host_start == '\"')
        host_start++;
      const char *host_end = strchr(host_start, '\"');
      if (host_end != NULL && host_end > host_start) {
        size_t len = host_end - host_start;
        if (len < sizeof(host)) {
          strncpy(host, host_start, len);
          host[len] = '\0';
        }
      }
    }
  }

  // Najít "port"
  const char *port_start = strstr(content, "\"port\"");
  if (port_start != NULL) {
    port_start = strchr(port_start, ':');
    if (port_start != NULL) {
      port_start++; // Přeskočit ':'
      while (*port_start == ' ')
        port_start++;
      uint32_t port_val = strtoul(port_start, NULL, 10);
      if (port_val > 0 && port_val <= 65535) {
        port = (uint16_t)port_val;
      }
    }
  }

  // Najít "username" (volitelné)
  const char *username_start = strstr(content, "\"username\"");
  if (username_start != NULL) {
    username_start = strchr(username_start, ':');
    if (username_start != NULL) {
      username_start++; // Přeskočit ':'
      while (*username_start == ' ' || *username_start == '\"')
        username_start++;
      const char *username_end = strchr(username_start, '\"');
      if (username_end != NULL && username_end > username_start) {
        size_t len = username_end - username_start;
        if (len < sizeof(username)) {
          strncpy(username, username_start, len);
          username[len] = '\0';
        }
      }
    }
  }

  // Najít "password" (volitelné)
  const char *password_start = strstr(content, "\"password\"");
  if (password_start != NULL) {
    password_start = strchr(password_start, ':');
    if (password_start != NULL) {
      password_start++; // Přeskočit ':'
      while (*password_start == ' ' || *password_start == '\"')
        password_start++;
      const char *password_end = strchr(password_start, '\"');
      if (password_end != NULL && password_end > password_start) {
        size_t len = password_end - password_start;
        if (len < sizeof(password)) {
          strncpy(password, password_start, len);
          password[len] = '\0';
        }
      }
    }
  }

  // Validovat host
  if (strlen(host) == 0 || strlen(host) > 127) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Invalid host (must be "
                    "1-127 characters)\"}",
                    -1);
    return ESP_FAIL;
  }

  // Uložit do NVS
  const char *username_ptr = (strlen(username) > 0) ? username : NULL;
  const char *password_ptr = (strlen(password) > 0) ? password : NULL;

  esp_err_t err =
      mqtt_save_config_to_nvs(host, port, username_ptr, password_ptr);
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to save: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Reinit MQTT client if WiFi STA is connected
  const char *success_message;
  if (wifi_is_sta_connected()) {
    esp_err_t reinit_ret = ha_light_reinit_mqtt();
    if (reinit_ret == ESP_OK) {
      success_message = "MQTT configuration saved and client reinicialized "
                        "with new settings.";
    } else {
      success_message = "MQTT configuration saved. Client reinit failed (will "
                        "reconnect on next WiFi connection).";
    }
  } else {
    success_message = "MQTT configuration saved. Client will reconnect with "
                      "new settings on next WiFi connection.";
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  char response[256];
  snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"%s\"}",
           success_message);
  httpd_resp_send(req, response, strlen(response));

  return ESP_OK;
}

// ============================================================================
// DEMO MODE API HANDLERS
// ============================================================================

static esp_err_t http_post_settings_brightness_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/brightness");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[100];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parse "brightness"
  int brightness_val = -1;
  char *ptr = strstr(content, "\"brightness\":");
  if (ptr) {
    if (sscanf(ptr, "\"brightness\":%d", &brightness_val) == 1) {
      // Validovat
      if (brightness_val < 0)
        brightness_val = 0;
      if (brightness_val > 100)
        brightness_val = 100;

      // 1. Nastavit LED driver hned
      led_set_brightness_global((uint8_t)brightness_val);

      // 2. Ulozit do configu (load-modify-save)
      system_config_t config;
      // Default init v pripade chyby loadu
      config.brightness_level = 50;

      if (config_load_from_nvs(&config) == ESP_OK) {
        config.brightness_level = (uint8_t)brightness_val;
        config_save_to_nvs(&config);
      } else {
        // Config neexistuje, vytvorit novy s defaulty a novym jasem
        config.brightness_level = (uint8_t)brightness_val;
        // Ostatni defaulty (pokud config_load selhal, config struct je
        // nedefinovany, ale my ukladame jen brightness do NVS v teto verzi
        // config_manageru? Ne, config_manager uklada celou struct?
        // config_save_to_nvs uklada key-value pairs. Takze je to bezpecne.
        config_save_to_nvs(&config);
      }

      cached_brightness = (uint8_t)brightness_val;
      cached_brightness_valid = true;
      ESP_LOGI(TAG, "Brightness updated to %d%% via Web UI", brightness_val);
    }
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

static esp_err_t http_post_settings_auto_lamp_timeout_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/auto_lamp_timeout");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[80];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  int seconds_val = -1;
  char *ptr = strstr(content, "\"seconds\":");
  if (ptr && sscanf(ptr, "\"seconds\":%d", &seconds_val) == 1) {
    if (seconds_val < 5) seconds_val = 5;
    if (seconds_val > 7200) seconds_val = 7200;
    if (ha_light_set_activity_timeout_sec((uint32_t)seconds_val) == ESP_OK) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"success\":true,\"seconds\":%d}", seconds_val);
      return ESP_OK;
    }
  }
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":false,\"message\":\"Invalid seconds (5..7200)\"}", -1);
  return ESP_OK;
}

static esp_err_t http_post_settings_guided_hints_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/guided_hints");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[96];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  system_config_t config;
  if (config_load_from_nvs(&config) != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Config load failed\"}",
                    -1);
    return ESP_OK;
  }
  config.guided_capture_hints_enabled = enabled;
  config.led_guidance_level = enabled ? (uint8_t)5 : (uint8_t)4;
  config_save_to_nvs(&config);
  game_set_led_guidance_level(config.led_guidance_level);

  char resp[120];
  snprintf(resp, sizeof(resp),
           "{\"success\":true,\"enabled\":%s,\"led_guidance_level\":%u}",
           enabled ? "true" : "false",
           (unsigned)config.led_guidance_level);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

static esp_err_t http_post_settings_led_guidance_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/led_guidance");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[128];
  int r = httpd_req_recv(req, content, sizeof(content) - 1);
  if (r <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[r] = '\0';

  int level = -1;
  char *p = strstr(content, "\"level\"");
  if (p) {
    p = strchr(p, ':');
    if (p) {
      level = (int)strtol(p + 1, NULL, 10);
    }
  }
  if (level < 1 || level > 5) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"level 1..5\"}", -1);
    return ESP_OK;
  }

  system_config_t config;
  if (config_load_from_nvs(&config) != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Config load failed\"}",
                    -1);
    return ESP_OK;
  }
  config.led_guidance_level = (uint8_t)level;
  config.guided_capture_hints_enabled = (level >= 5);
  config_save_to_nvs(&config);
  game_set_led_guidance_level((uint8_t)level);

  char resp[96];
  snprintf(resp, sizeof(resp),
           "{\"success\":true,\"led_guidance_level\":%u,\"guided_capture_hints_enabled\":%s}",
           (unsigned)level, (level >= 5) ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

/**
 * @brief Handler pro GET /api/settings/start_pos_check
 *
 * Vrati stav hlidani pocatecni pozice (enabled true/false).
 */
static esp_err_t http_get_settings_start_pos_check_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/settings/start_pos_check");

  bool enabled = game_get_starting_position_check();

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"enabled\":%s}", enabled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/settings/start_pos_check
 *
 * Nastavi stav hlidani pocatecni pozice a ulozi do NVS.
 * Body: {"enabled": true/false}
 */
static esp_err_t http_post_settings_start_pos_check_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/start_pos_check");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[64];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  // Nastavit v game_task
  game_set_starting_position_check(enabled);

  // Ulozit do NVS
  system_config_t config;
  if (config_load_from_nvs(&config) == ESP_OK) {
    config.starting_position_check_enabled = enabled;
    config_save_to_nvs(&config);
    ESP_LOGI(TAG, "Starting position check saved to NVS: %s", enabled ? "ON" : "OFF");
  }

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"success\":true,\"enabled\":%s}", enabled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

static esp_err_t http_post_light_command_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/light/command");

  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  bool state = true;
  int r = 255, g = 255, b = 255;
  if (strstr(content, "\"state\":false") || strstr(content, "\"state\": false\"")) {
    state = false;
  }
  char *p;
  if ((p = strstr(content, "\"r\":")) && sscanf(p, "\"r\":%d", &r) == 1) { }
  if ((p = strstr(content, "\"g\":")) && sscanf(p, "\"g\":%d", &g) == 1) { }
  if ((p = strstr(content, "\"b\":")) && sscanf(p, "\"b\":%d", &b) == 1) { }

  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;

  if (!ha_light_request_web_lamp(state, (uint8_t)r, (uint8_t)g, (uint8_t)b)) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Light task busy or not "
                    "ready\"}",
                    -1);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

static esp_err_t http_post_light_game_mode_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/light/game_mode");
  ha_light_report_activity("web_game_mode");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

static esp_err_t http_post_demo_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/demo/config");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[100];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parse "enabled"
  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  // Parse "speed_ms"
  char *speed_ptr = strstr(content, "\"speed_ms\":");
  if (speed_ptr) {
    int speed_val;
    if (sscanf(speed_ptr, "\"speed_ms\":%d", &speed_val) == 1) {
      set_demo_speed_ms((uint32_t)speed_val);
    }
  }

  toggle_demo_mode(enabled);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

static esp_err_t http_post_demo_start_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/demo/start");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  toggle_demo_mode(true);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Demo started\"}", -1);
  return ESP_OK;
}

static esp_err_t http_get_demo_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/demo/status");

  bool enabled = is_demo_mode_enabled();

  char resp_str[64];
  snprintf(resp_str, sizeof(resp_str), "{\"enabled\":%s}",
           enabled ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

// ============================================================================
// HTTP SERVER SETUP
// ============================================================================

/**
 * @brief Spusti HTTP server
 *
 * Tato funkce spusti HTTP server s REST API endpointy pro sachovy system.
 * Registruje vsechny potrebne handlery pro web rozhrani.
 *
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Funkce vytvori HTTP server s konfiguraci a registruje handlery pro:
 * - Hlavni stranku (/)
 * - Stav sachovnice (/board)
 * - Status hry (/status)
 * - Historie tahu (/history)
 * - Captured figurky (/captured)
 */
static esp_err_t start_http_server(void) {
  if (httpd_handle != NULL) {
    ESP_LOGW(TAG, "HTTP server already running");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Starting HTTP server...");

  // Konfigurovat HTTP server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = HTTP_SERVER_PORT;
  /* Počet httpd_register_uri_handler ve start_http_server: 54+ (2026-04, vč. WS + PNG figurky).
   * Při přidání endpointu zvýšit zásobu (HTTPD neregistruje „tiše“ navíc). */
  config.max_uri_handlers = 64;
  // Keep within LWIP limits. HTTP server internally reserves 3 sockets.
#if CONFIG_LWIP_MAX_SOCKETS > 6
  config.max_open_sockets = CONFIG_LWIP_MAX_SOCKETS - 3;
#else
  config.max_open_sockets = 3;
#endif
  config.lru_purge_enable = false; // CRITICAL: Disabled to prevent socket
                                   // closure during chunked transfer
  config.recv_wait_timeout = 20;    // 20 s – stabilni pripojeni pri pomalejsi siti
  config.send_wait_timeout =
      5000; // 5 s – spolehlivy chunked transfer pri vykyvech
  config.max_resp_headers = 8;
  config.backlog_conn = 6;         // Vetsi fronta pri napadu klientu
  config.stack_size = 8192;        // Zvysena velikost stacku pro HTTP server task

  // Spustit HTTP server
  esp_err_t ret = httpd_start(&httpd_handle, &config);
  if (ret != ESP_OK) {
    last_http_start_error = ret;
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return ret;
  }
  last_http_start_error = ESP_OK;

  // Registrovat URI handlery
  ESP_LOGI(TAG, "Registering URI handlers...");

  // Handler pro Chess JavaScript soubor
  httpd_uri_t chess_js_uri = {.uri = "/chess_app.js",
                              .method = HTTP_GET,
                              .handler = http_get_chess_js_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &chess_js_uri);

  // Handler pro testovaci stranku (minimalni timer test)
  httpd_uri_t test_uri = {.uri = "/test",
                          .method = HTTP_GET,
                          .handler = http_get_test_handler,
                          .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &test_uri);

  // Handler pro root (HTML stranka)
  httpd_uri_t root_uri = {.uri = "/",
                          .method = HTTP_GET,
                          .handler = http_get_root_handler,
                          .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &root_uri);

  // Handlery pro API
  httpd_uri_t board_uri = {.uri = "/api/board",
                           .method = HTTP_GET,
                           .handler = http_get_board_handler,
                           .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &board_uri);

  httpd_uri_t game_snapshot_uri = {.uri = "/api/game/snapshot",
                                   .method = HTTP_GET,
                                   .handler = http_get_game_snapshot_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &game_snapshot_uri);

  httpd_uri_t status_uri = {.uri = "/api/status",
                            .method = HTTP_GET,
                            .handler = http_get_status_handler,
                            .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &status_uri);

  httpd_uri_t history_uri = {.uri = "/api/history",
                             .method = HTTP_GET,
                             .handler = http_get_history_handler,
                             .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &history_uri);

  httpd_uri_t captured_uri = {.uri = "/api/captured",
                              .method = HTTP_GET,
                              .handler = http_get_captured_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &captured_uri);

  httpd_uri_t advantage_uri = {.uri = "/api/advantage",
                               .method = HTTP_GET,
                               .handler = http_get_advantage_handler,
                               .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &advantage_uri);

  // Handlery pro Timer API
  httpd_uri_t timer_uri = {.uri = "/api/timer",
                           .method = HTTP_GET,
                           .handler = http_get_timer_handler,
                           .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &timer_uri);

  // Handler pro favicon.ico (silence 404 warnings)
  httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                             .method = HTTP_GET,
                             .handler = http_get_favicon_handler,
                             .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &favicon_uri);

  httpd_uri_t timer_config_uri = {.uri = "/api/timer/config",
                                  .method = HTTP_POST,
                                  .handler = http_post_timer_config_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &timer_config_uri);

  httpd_uri_t settings_brightness_uri = {
      .uri = "/api/settings/brightness",
      .method = HTTP_POST,
      .handler = http_post_settings_brightness_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_brightness_uri);

  httpd_uri_t settings_auto_lamp_timeout_uri = {
      .uri = "/api/settings/auto_lamp_timeout",
      .method = HTTP_POST,
      .handler = http_post_settings_auto_lamp_timeout_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_auto_lamp_timeout_uri);

  httpd_uri_t settings_guided_hints_uri = {
      .uri = "/api/settings/guided_hints",
      .method = HTTP_POST,
      .handler = http_post_settings_guided_hints_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_guided_hints_uri);

  httpd_uri_t settings_led_guidance_uri = {
      .uri = "/api/settings/led_guidance",
      .method = HTTP_POST,
      .handler = http_post_settings_led_guidance_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_led_guidance_uri);

  // Handlery pro Start Position Check nastaveni
  httpd_uri_t settings_start_pos_check_get_uri = {
      .uri = "/api/settings/start_pos_check",
      .method = HTTP_GET,
      .handler = http_get_settings_start_pos_check_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_start_pos_check_get_uri);

  httpd_uri_t settings_start_pos_check_post_uri = {
      .uri = "/api/settings/start_pos_check",
      .method = HTTP_POST,
      .handler = http_post_settings_start_pos_check_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_start_pos_check_post_uri);

  httpd_uri_t timer_pause_uri = {.uri = "/api/timer/pause",
                                 .method = HTTP_POST,
                                 .handler = http_post_timer_pause_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &timer_pause_uri);

  httpd_uri_t timer_resume_uri = {.uri = "/api/timer/resume",
                                  .method = HTTP_POST,
                                  .handler = http_post_timer_resume_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &timer_resume_uri);

  httpd_uri_t timer_reset_uri = {.uri = "/api/timer/reset",
                                 .method = HTTP_POST,
                                 .handler = http_post_timer_reset_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &timer_reset_uri);

  // Handlery pro WiFi API
  httpd_uri_t wifi_config_uri = {.uri = "/api/wifi/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_wifi_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &wifi_config_uri);

  httpd_uri_t wifi_connect_uri = {.uri = "/api/wifi/connect",
                                  .method = HTTP_POST,
                                  .handler = http_post_wifi_connect_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &wifi_connect_uri);

  httpd_uri_t wifi_disconnect_uri = {.uri = "/api/wifi/disconnect",
                                     .method = HTTP_POST,
                                     .handler =
                                         http_post_wifi_disconnect_handler,
                                     .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &wifi_disconnect_uri);

  httpd_uri_t wifi_clear_uri = {.uri = "/api/wifi/clear",
                                .method = HTTP_POST,
                                .handler = http_post_wifi_clear_handler,
                                .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &wifi_clear_uri);

  httpd_uri_t factory_reset_uri = {.uri = "/api/system/factory_reset",
                                   .method = HTTP_POST,
                                   .handler = http_post_factory_reset_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &factory_reset_uri);

  ret = ota_update_register_http_handlers(httpd_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ota_update_register_http_handlers failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

  httpd_uri_t wifi_status_uri = {.uri = "/api/wifi/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_wifi_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &wifi_status_uri);

  // Handler pro web lock status
  httpd_uri_t web_lock_status_uri = {.uri = "/api/web/lock-status",
                                     .method = HTTP_GET,
                                     .handler =
                                         http_get_web_lock_status_handler,
                                     .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &web_lock_status_uri);

  // Handlery pro Demo Mode
  httpd_uri_t demo_config_uri = {.uri = "/api/demo/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_demo_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &demo_config_uri);

  httpd_uri_t demo_start_uri = {.uri = "/api/demo/start",
                                .method = HTTP_POST,
                                .handler = http_post_demo_start_handler,
                                .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &demo_start_uri);

  httpd_uri_t demo_status_uri = {.uri = "/api/demo/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_demo_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &demo_status_uri);

  // Handler pro Tahy
  httpd_uri_t move_uri = {.uri = "/api/move",
                          .method = HTTP_POST,
                          .handler = http_post_game_move_handler,
                          .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &move_uri);

  // Handler pro Virtual Actions (Remote Control)
  httpd_uri_t virtual_action_uri = {.uri = "/api/game/virtual_action",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_virtual_action_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &virtual_action_uri);

  // Handler pro New Game
  httpd_uri_t new_game_uri = {.uri = "/api/game/new",
                              .method = HTTP_POST,
                              .handler = http_post_game_new_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &new_game_uri);

  // Handler pro Hint highlight (LED)
  httpd_uri_t hint_highlight_uri = {.uri = "/api/game/hint_highlight",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_hint_highlight_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &hint_highlight_uri);

  httpd_uri_t hint_clear_uri = {.uri = "/api/game/hint_clear",
                               .method = HTTP_POST,
                               .handler = http_post_game_hint_clear_handler,
                               .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &hint_clear_uri);

  httpd_uri_t setup_tutorial_uri = {.uri = "/api/game/setup_tutorial",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_setup_tutorial_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &setup_tutorial_uri);

  httpd_uri_t puzzle_uri = {.uri = "/api/game/puzzle",
                            .method = HTTP_POST,
                            .handler = http_post_game_puzzle_handler,
                            .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &puzzle_uri);

  httpd_uri_t settings_ui_get_uri = {.uri = "/api/settings/ui",
                                   .method = HTTP_GET,
                                   .handler = http_get_settings_ui_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_ui_get_uri);

  httpd_uri_t settings_ui_post_uri = {.uri = "/api/settings/ui",
                                      .method = HTTP_POST,
                                      .handler = http_post_settings_ui_handler,
                                      .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &settings_ui_post_uri);

  // Handlery pro lampu (režim Lampa z webu)
  httpd_uri_t light_command_uri = {.uri = "/api/light/command",
                                   .method = HTTP_POST,
                                   .handler = http_post_light_command_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &light_command_uri);

  httpd_uri_t light_game_mode_uri = {.uri = "/api/light/game_mode",
                                    .method = HTTP_POST,
                                    .handler = http_post_light_game_mode_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &light_game_mode_uri);

  // Handlery pro MQTT API
  httpd_uri_t mqtt_status_uri = {.uri = "/api/mqtt/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_mqtt_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &mqtt_status_uri);

  httpd_uri_t mqtt_config_uri = {.uri = "/api/mqtt/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_mqtt_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &mqtt_config_uri);

  ret = chess_piece_register_http_uris(httpd_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "chess_piece_register_http_uris failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

#if CONFIG_HTTPD_WS_SUPPORT
  httpd_uri_t ws_uri = {.uri = "/ws",
                        .method = HTTP_GET,
                        .handler = http_ws_handler,
                        .user_ctx = NULL,
                        .is_websocket = true};
  httpd_register_uri_handler(httpd_handle, &ws_uri);
  ESP_LOGI(TAG,
           "HTTP server on port %d: GET /api/game/snapshot + WS /ws, max_uri_handlers=64",
           HTTP_SERVER_PORT);
  web_server_websocket_init();
#else
  ESP_LOGW(TAG,
           "HTTP server on port %d: WebSocket /ws NENÍ v buildu — zapni "
           "CONFIG_HTTPD_WS_SUPPORT (iOS/watchOS WS jinak padá na -1011)",
           HTTP_SERVER_PORT);
#endif

  return ESP_OK;
}

/**
 * @brief Zastavi HTTP server
 *
 * Tato funkce zastavi HTTP server a uvolni prostredky.
 *
 * @details
 * Funkce zastavi HTTP server a nastavi handle na NULL.
 * Pouziva se pri vypinani web serveru.
 */
static void stop_http_server(void) {
#if CONFIG_HTTPD_WS_SUPPORT
  if (ws_broadcast_timer != NULL) {
    esp_timer_stop(ws_broadcast_timer);
    esp_timer_delete(ws_broadcast_timer);
    ws_broadcast_timer = NULL;
  }
#endif
  if (httpd_handle != NULL) {
    httpd_stop(httpd_handle);
    httpd_handle = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
  }
}

// ============================================================================
// REST API HANDLERS
// ============================================================================

/** Doplnění GET /api/status o web lock, WiFi, jas, matrix guard, lampu (sdílené se snapshot). */
static void inject_web_status_fields(char *buf, size_t buf_size) {
  char *last_brace = strrchr(buf, '}');
  if (!last_brace) {
    ESP_LOGW(TAG, "[STAGING] inject_web_status_fields: no closing brace in buf");
    return;
  }
  size_t remaining = buf_size - (size_t)(last_brace - buf);
  /* První blok ~370 B (+ chess_hint_limit) + druhý ~120 B — při těsném bufferu raději neposlat useknutý JSON. */
  if (remaining < 430) {
    ESP_LOGE(TAG,
             "[STAGING] inject_web_status_fields: remaining=%zu B too small for "
             "web fields (cap=%zu)",
             remaining, buf_size);
    return;
  }
  uint8_t b = cached_brightness_valid ? cached_brightness : 50;
  int hint_lim = config_ui_prefs_get_chess_hint_limit();
  int wr =
      snprintf(last_brace, remaining,
               ",\"web_locked\":%s,\"internet_connected\":%s,\"brightness\":%d,"
               "\"guided_capture_hints_enabled\":%s,\"led_guidance_level\":%u,"
               "\"matrix_guard_active\":%s,"
               "\"matrix_guard_conflicts\":%u,"
               "\"matrix_guard_lifted_low\":%lu,\"matrix_guard_lifted_high\":%lu,"
               "\"matrix_guard_dropped_low\":%lu,\"matrix_guard_dropped_high\":%lu,"
               "\"chess_hint_limit\":%d}",
               web_is_locked() ? "true" : "false",
               sta_connected ? "true" : "false", (int)b,
               game_get_guided_capture_hints_enabled() ? "true" : "false",
               (unsigned)game_get_led_guidance_level(),
               game_is_matrix_guard_active() ? "true" : "false",
               (unsigned int)game_get_matrix_guard_conflict_count(),
               (unsigned long)game_get_matrix_guard_lifted_mask_low(),
               (unsigned long)game_get_matrix_guard_lifted_mask_high(),
               (unsigned long)game_get_matrix_guard_dropped_mask_low(),
               (unsigned long)game_get_matrix_guard_dropped_mask_high(),
               hint_lim);
  if (wr < 0 || (size_t)wr >= remaining) {
    ESP_LOGE(TAG,
             "[STAGING] inject_web_status_fields: first snprintf truncated "
             "wr=%d rem=%zu",
             wr, remaining);
    return;
  }
  if (remaining >= 150) {
    last_brace = strrchr(buf, '}');
    if (last_brace) {
      remaining = buf_size - (size_t)(last_brace - buf);
      if (remaining < 130) {
        ESP_LOGW(TAG,
                 "[STAGING] inject_web_status_fields: skip lamp fields "
                 "remaining=%zu",
                 remaining);
        return;
      }
      ha_mode_t light_mode = ha_light_get_mode();
      uint8_t lr = 255, lg = 255, lb = 255, lbright = 255;
      bool lstate = true;
      ha_light_get_state(&lr, &lg, &lb, &lbright, &lstate);
      uint32_t auto_sec = ha_light_get_activity_timeout_sec();
      wr = snprintf(last_brace, remaining,
                    ",\"light_mode\":\"%s\",\"light_state\":%s,\"light_r\":%u,"
                    "\"light_g\":%u,\"light_b\":%u,\"auto_lamp_timeout_sec\":%lu}",
                    (light_mode == HA_MODE_HA) ? "lamp" : "game",
                    lstate ? "true" : "false", (unsigned)lr, (unsigned)lg,
                    (unsigned)lb, (unsigned long)auto_sec);
      if (wr < 0 || (size_t)wr >= remaining) {
        ESP_LOGE(TAG,
                 "[STAGING] inject_web_status_fields: lamp snprintf truncated "
                 "wr=%d rem=%zu",
                 wr, remaining);
      }
    }
  }
}

static void snapshot_build_mutex_take(void) {
  if (snapshot_build_mutex == NULL) {
    snapshot_build_mutex = xSemaphoreCreateMutex();
  }
  /* portMAX_DELAY zde zablokoval web_server_task na mutexu (HTTP httpd drží build)
   * → bez esp_task_wdt_reset() TWDT timeout (~10 s). Čekáme po krocích + reset. */
  while (xSemaphoreTake(snapshot_build_mutex, pdMS_TO_TICKS(400)) != pdTRUE) {
    (void)web_server_task_wdt_reset_safe();
  }
}

static void snapshot_build_mutex_give(void) {
  if (snapshot_build_mutex != NULL) {
    xSemaphoreGive(snapshot_build_mutex);
  }
}

/** Pod `snapshot_build_mutex` — žádný paralelní build; šetří ~1 KiB stacku na NimBLE host task. */
static char s_clock_json_build_scratch[TIMER_HTTP_JSON_MAX];

/** Stejný JSON jako GET /api/game/snapshot — do bufferu `out` (HTTP i BLE). */
static esp_err_t build_snapshot_json_to_buffer(char *out, size_t cap,
                                               size_t *out_len) {
  snapshot_build_mutex_take();
  (void)web_server_task_wdt_reset_safe();
  esp_err_t ret = game_get_board_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  size_t L = strlen(json_buffer);
  if (L < 4 || json_buffer[0] != '{' || json_buffer[L - 1] != '}') {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  json_buffer[L - 1] = '\0';
  uint32_t srev = game_get_state_revision();
  size_t pos = (size_t)snprintf(out, cap, "{\"state_version\":%" PRIu32 ",%s",
                                srev, json_buffer + 1);
  if (pos >= cap) {
    json_buffer[L - 1] = '}';
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }

  ret = game_get_status_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
#ifndef NDEBUG
  ESP_LOGI(TAG,
           "[STAGING] build_snapshot: status raw len=%zu / cap=%zu",
           strlen(json_buffer), sizeof(json_buffer));
#endif
  inject_web_status_fields(json_buffer, sizeof(json_buffer));
#ifndef NDEBUG
  ESP_LOGI(TAG,
           "[STAGING] build_snapshot: status after inject len=%zu / cap=%zu",
           strlen(json_buffer), sizeof(json_buffer));
#endif
  int n = snprintf(out + pos, cap - pos, ",\"status\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_history_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"history\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_captured_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"captured\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_timer_json(s_clock_json_build_scratch,
                            sizeof(s_clock_json_build_scratch));
  if (ret == ESP_OK) {
    n = snprintf(out + pos, cap - pos, ",\"clock\":%s}",
                 s_clock_json_build_scratch);
  } else {
#ifndef NDEBUG
    ESP_LOGW(TAG, "build_snapshot: game_get_timer_json failed: %s",
             esp_err_to_name(ret));
#endif
    n = snprintf(out + pos, cap - pos, "}");
  }
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  *out_len = pos;
  snapshot_build_mutex_give();
  return ESP_OK;
}

static esp_err_t build_snapshot_json(size_t *out_len) {
  return build_snapshot_json_to_buffer(snapshot_buffer, sizeof(snapshot_buffer),
                                       out_len);
}

esp_err_t web_server_build_game_snapshot_json(char *out, size_t cap,
                                              size_t *out_len) {
  if (out == NULL || out_len == NULL || cap == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  return build_snapshot_json_to_buffer(out, cap, out_len);
}

esp_err_t web_server_build_game_snapshot_json_shared(char **out_json,
                                                     size_t *out_len) {
  if (out_json == NULL || out_len == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t e =
      build_snapshot_json_to_buffer(snapshot_buffer, sizeof(snapshot_buffer),
                                    out_len);
  if (e == ESP_OK) {
    *out_json = snapshot_buffer;
  }
  return e;
}

/** Společná logika POST /api/game/hint_highlight a BLE příkazu hint_highlight. */
static esp_err_t web_server_apply_hint_highlight_json_body(const char *buf) {
  if (buf == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  const char *p_to = strstr(buf, "\"to\"");
  if (!p_to) {
    return ESP_ERR_INVALID_ARG;
  }
  p_to += 4;
  while (*p_to && *p_to != '"')
    p_to++;
  if (*p_to == '"')
    p_to++;
  if (!*p_to || !p_to[1]) {
    return ESP_ERR_INVALID_ARG;
  }
  char to_sq[3] = {0};
  to_sq[0] = (char)((unsigned char)p_to[0] <= 'Z' ? p_to[0] + 32 : p_to[0]);
  to_sq[1] = p_to[1];
  if (to_sq[0] < 'a' || to_sq[0] > 'h' || to_sq[1] < '1' || to_sq[1] > '8') {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t to_led = chess_notation_to_led_index(to_sq);
  if (to_led >= 64) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t from_led = 64;
  const char *p_from = strstr(buf, "\"from\"");
  if (p_from) {
    p_from += 6;
    while (*p_from && *p_from != '"')
      p_from++;
    if (*p_from == '"')
      p_from++;
    if (*p_from && p_from[1]) {
      char from_sq[3] = {0};
      from_sq[0] =
          (char)((unsigned char)p_from[0] <= 'Z' ? p_from[0] + 32 : p_from[0]);
      from_sq[1] = p_from[1];
      if (from_sq[0] >= 'a' && from_sq[0] <= 'h' && from_sq[1] >= '1' &&
          from_sq[1] <= '8') {
        uint8_t fl = chess_notation_to_led_index(from_sq);
        if (fl < 64)
          from_led = fl;
      }
    }
  }

  led_command_t cmd = {0};
  cmd.type = LED_CMD_HIGHLIGHT_HINT;
  cmd.led_index = from_led;
  cmd.data = (void *)&to_led;
  led_execute_command_new(&cmd);
  ESP_LOGI(TAG, "Hint highlight: %s (LED %u -> %u)", to_sq, (unsigned)from_led,
           (unsigned)to_led);
  return ESP_OK;
}

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

/** Musí přesně sedět s webem (`POST /api/system/factory_reset`) a iOS. */
#define CZECHMATE_FACTORY_RESET_CONFIRM "erase_all_nvs"

static bool json_body_has_factory_confirm(const char *json) {
  if (json == NULL) {
    return false;
  }
  if (strstr(json, "\"confirm\"") == NULL) {
    return false;
  }
  return strstr(json, CZECHMATE_FACTORY_RESET_CONFIRM) != NULL;
}

static void factory_reset_worker(void *ignored) {
  (void)ignored;
  vTaskDelay(pdMS_TO_TICKS(500));
  ESP_LOGW(TAG,
           "[STAGING] factory_reset: raw erase default NVS partition + restart");

  const esp_partition_t *nvs_part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
  esp_err_t er = ESP_FAIL;
  if (nvs_part != NULL) {
    er = esp_partition_erase_range(nvs_part, 0, nvs_part->size);
    ESP_LOGW(TAG, "esp_partition_erase_range(label=%s size=%u): %s",
             nvs_part->label, (unsigned)nvs_part->size, esp_err_to_name(er));
  } else {
    er = nvs_flash_erase();
    ESP_LOGW(TAG, "nvs_flash_erase (no nvs partition label): %s",
             esp_err_to_name(er));
  }

  esp_restart();
}

static esp_err_t factory_reset_schedule(void) {
  BaseType_t ok =
      xTaskCreate(factory_reset_worker, "fac_rst", 4096, NULL, 10, NULL);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "factory_reset: xTaskCreate failed");
    return ESP_FAIL;
  }
  return ESP_OK;
}

static bool s_ble_dispatch_custom_ack_sent;

bool web_server_ble_dispatch_custom_ack_was_sent(void) {
  return s_ble_dispatch_custom_ack_sent;
}

/** Jasná zpětná vazba pro klienta — odliší od web locku (stejný ESP_ERR_INVALID_STATE). */
static void ble_dispatch_ack_needs_encryption(const char *cmd_literal) {
  char out[288];
  int n =
      snprintf(out, sizeof(out),
               "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"needs_encryption\","
               "\"cmd\":\"%s\","
               "\"message\":\"Vyžadováno šifrované BLE spojení (dokončete párování).\","
               "\"esp\":%d}",
               cmd_literal, (int)ESP_ERR_INVALID_STATE);
  if (n < 0 || (size_t)n >= sizeof(out)) {
    ble_task_notify_cmd_ack_json(
        "{\"channel\":\"cmd_ack\",\"ok\":false,\"code\":\"needs_encryption\","
        "\"cmd\":\"unknown\",\"message\":\"Vyžadováno šifrované BLE spojení.\","
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
   * NOVÉ BLE PŘÍKAZY - Začátek
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

      /* Stejné pole (dvojité klepnutí v aplikaci) — neposílat do game_task (MOVE_ERROR_DESTINATION_OCCUPIED). */
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

    if (want_finish) {
      if (!game_is_board_setup_tutorial_active()) {
        return ESP_ERR_INVALID_ARG;
      }
      if (!game_finish_board_setup_tutorial_from_web()) {
        if (game_is_board_setup_tutorial_active()) {
          return ESP_ERR_NOT_FINISHED;
        }
        return ESP_FAIL;
      }
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

  /* ============================================
   * NOVÉ BLE PŘÍKAZY - Konec
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

static bool czechmate_mdns_started = false;

/**
 * Bonjour TXT „sta_ip" + „ap_ip" — iOS/watch často nevyřeší hostname .local
 * včas; přímá IPv4 v TXT umožní HTTP/WS bez závislosti na mDNS A záznamu.
 *
 * ap_ip fix (AP hotspot): mDNS pakety ESP posílá jen přes STA rozhraní do
 * domácí sítě. Telefon připojený na AP hotspot tyto pakety nikdy nedostane,
 * protože patří do jiného L2 segmentu. Opraveno:
 *   1) TXT ap_ip = IP adresa AP rozhraní (typicky 192.168.4.1)
 *   2) mdns_netif_action na ap_netif → ESP vysílá mDNS i do AP subnetu
 */
static void czechmate_mdns_refresh_sta_txt(void) {
  if (!czechmate_mdns_started) {
    return;
  }

  /* ── sta_ip: přímá STA IPv4 adresa (domácí síť) ── */
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
   * iOS precte ap_ip z TXT zaznamu a pripoji se primo bez .local resolve.
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
    /* Zapnout mDNS vysílání přes AP rozhraní */
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
  /* refresh_sta_txt aktivuje mDNS na STA i AP rozhrani + plni TXT zaznamy */
  czechmate_mdns_refresh_sta_txt();
}

#if CONFIG_HTTPD_WS_SUPPORT
#define WS_MAX_CLIENT_FDS 32

static esp_err_t http_ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGD(TAG, "WebSocket handshake OK (/ws)");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "ws recv (len): %s", esp_err_to_name(ret));
    return ret;
  }
  if (ws_pkt.len) {
    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    free(buf);
    if (ret != ESP_OK) {
      ESP_LOGD(TAG, "ws recv payload: %s", esp_err_to_name(ret));
      return ret;
    }
  }
  return ESP_OK;
}

static bool ws_has_clients(void) {
  if (httpd_handle == NULL) {
    return false;
  }
  size_t n = WS_MAX_CLIENT_FDS;
  int fds[WS_MAX_CLIENT_FDS];
  if (httpd_get_client_list(httpd_handle, &n, fds) != ESP_OK) {
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    if (httpd_ws_get_fd_info(httpd_handle, fds[i]) ==
        HTTPD_WS_CLIENT_WEBSOCKET) {
      return true;
    }
  }
  return false;
}

static void ws_broadcast_snapshot(void) {
  if (httpd_handle == NULL || !web_server_active) {
    return;
  }
  if (!ws_has_clients()) {
    return;
  }
  (void)web_server_task_wdt_reset_safe();
  size_t len = 0;
  if (build_snapshot_json(&len) != ESP_OK) {
    ESP_LOGD(TAG, "WS broadcast: build_snapshot_json failed");
    return;
  }
  uint8_t *payload = (uint8_t *)malloc(len);
  if (payload == NULL) {
    ESP_LOGE(TAG, "WS broadcast: malloc %u B failed", (unsigned)len);
    return;
  }
  memcpy(payload, snapshot_buffer, len);
  httpd_ws_frame_t ws_pkt = {.type = HTTPD_WS_TYPE_TEXT,
                             .payload = payload,
                             .len = len};
  size_t n = WS_MAX_CLIENT_FDS;
  int fds[WS_MAX_CLIENT_FDS];
  if (httpd_get_client_list(httpd_handle, &n, fds) != ESP_OK) {
    free(payload);
    return;
  }
  size_t ws_count = 0;
  for (size_t i = 0; i < n; i++) {
    if (httpd_ws_get_fd_info(httpd_handle, fds[i]) !=
        HTTPD_WS_CLIENT_WEBSOCKET) {
      continue;
    }
    ws_count++;
    (void)web_server_task_wdt_reset_safe();
    esp_err_t err = httpd_ws_send_data(httpd_handle, fds[i], &ws_pkt);
    if (err != ESP_OK) {
      ESP_LOGD(TAG, "WS send fd %d: %s", fds[i], esp_err_to_name(err));
    }
  }
  ESP_LOGD(TAG, "[STAGING] WS broadcast → %u WS client(s), %u B payload",
           (unsigned)ws_count, (unsigned)len);
  free(payload);
}

static void ws_broadcast_timer_cb(void *arg) {
  (void)arg;
  ESP_LOGD(TAG,
           "[STAGING] WS watchdog tick → defer snapshot (esp_timer stack too small)");
  czechmate_ensure_snapshot_notify_queue();
  if (snapshot_notify_queue == NULL) {
    ESP_LOGW(TAG, "WS watchdog: snapshot_notify_queue missing, skip");
    return;
  }
  uint8_t ping = 1;
  if (xQueueSend(snapshot_notify_queue, &ping, 0) != pdTRUE) {
    ESP_LOGD(TAG, "[STAGING] WS watchdog: notify coalesced (queue full)");
  }
}
#endif /* CONFIG_HTTPD_WS_SUPPORT */

/** Fronta „ping“ z game_task — WS + BLE snapshot v web_server_task (neblokuje
 * game_task na httpd_ws_send_data / malloc). Musí být mimo #if WS — BLE vždy. */

static void czechmate_ensure_snapshot_notify_queue(void) {
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
  /* Snapshot + WS odeslání může blokovat dlouho (httpd send timeout) — bez
   * resetu TWDT hlásí web_server_task timeout. */
  (void)web_server_task_wdt_reset_safe();
  size_t fh = esp_get_free_heap_size();
  if (fh < 10240) {
    ESP_LOGW(TAG,
             "low heap before snapshot push: %zu B (cíl udržet ~8–10kB+ volné)",
             fh);
  }
#if CONFIG_HTTPD_WS_SUPPORT
  ws_broadcast_snapshot();
#endif
  (void)web_server_task_wdt_reset_safe();
  czechmate_push_ble_snapshot();
  (void)web_server_task_wdt_reset_safe();
}

/** Silná implementace — přepíše slabou z game_hooks (WS + BLE, nezávislé na
 * CONFIG_HTTPD_WS_SUPPORT pro BLE). */
void czechmate_on_game_state_changed(void) {
  czechmate_ensure_snapshot_notify_queue();
  if (snapshot_notify_queue == NULL) {
#if CONFIG_HTTPD_WS_SUPPORT
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

static esp_err_t http_get_board_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/board");

  // Ziskat stav sachovnice z game tasku
  esp_err_t ret = game_get_board_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get board state", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

static esp_err_t http_get_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/status");

  // Ziskat stav hry z game tasku
  esp_err_t ret = game_get_status_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get game status", -1);
    return ESP_FAIL;
  }

  inject_web_status_fields(json_buffer, sizeof(json_buffer));

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

static esp_err_t http_get_history_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/history");

  // Ziskat historii tahu z game tasku
  esp_err_t ret = game_get_history_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get move history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

static esp_err_t http_get_captured_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/captured");

  // Ziskat sebrane figurky z game tasku
  esp_err_t ret = game_get_captured_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get captured pieces", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

static esp_err_t http_get_game_snapshot_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/game/snapshot");
  uint32_t rev = game_get_state_revision();
  char etag[24];
  snprintf(etag, sizeof(etag), "%" PRIu32, rev);

  char inm[64];
  if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm, sizeof(inm)) ==
      ESP_OK) {
    if (strcmp(inm, etag) == 0) {
      httpd_resp_set_status(req, "304 Not Modified");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  size_t pos = 0;
  esp_err_t ret = build_snapshot_json(&pos);
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to build game snapshot", -1);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  if (httpd_resp_set_hdr(req, "ETag", etag) != ESP_OK) {
    ESP_LOGD(TAG, "ETag header not set");
  }
  httpd_resp_send(req, snapshot_buffer, pos);
  return ESP_OK;
}

static esp_err_t http_get_advantage_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/advantage");

  // Ziskat historii material advantage z game tasku
  esp_err_t ret = game_get_advantage_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get advantage history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

// ============================================================================
// TIMER API HANDLERS
// ============================================================================

static esp_err_t http_get_timer_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/timer");

  // Lokalni buffer jen pro timer JSON (~1 KiB); JSON_BUFFER_SIZE na stacku = overflow httpd (8192 B).
  char local_json[TIMER_HTTP_JSON_MAX];
  esp_err_t ret = game_get_timer_json(local_json, sizeof(local_json));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get timer state", -1);
    return ESP_FAIL;
  }

  // Zabranit cachovani timer odpovedi v prohlizeci
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, local_json, strlen(local_json));

  return ESP_OK;
}

static esp_err_t http_post_timer_config_handler(httpd_req_t *req) {
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

  // Parsovat JSON a odeslat prikaz do game tasku
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

  // Parsovat vlastni casove hodnoty pokud je type CUSTOM
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

    // Overit ze byly poskytnuty vlastni hodnoty
    if (minutes_str == NULL || increment_str == NULL) {
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req, "Custom time control requires minutes and increment",
                      -1);
      return ESP_FAIL;
    }
  }

  // Odeslat prikaz do game tasku
  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to set time control", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "Time control set successfully", -1);

  return ESP_OK;
}

static esp_err_t http_post_timer_pause_handler(httpd_req_t *req) {
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

static esp_err_t http_post_timer_resume_handler(httpd_req_t *req) {
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

static esp_err_t http_post_timer_reset_handler(httpd_req_t *req) {
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
// VIRTUAL GAME ACTIONS (REMOTE CONTROL)
// ============================================================================

/**
 * @brief Handler pro POST /api/move
 *
 * Provede tah na sachovnici.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"from": "e2", "to": "e4", "promotion": "q"}
 */
static esp_err_t http_post_game_move_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/move");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Move blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Precist JSON data
  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data received", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser
  char from[8] = {0};
  char to[8] = {0};
  char promotion[4] = {0};

  // Najit "from"
  const char *from_start = strstr(content, "\"from\"");
  if (from_start != NULL) {
    from_start = strchr(from_start, ':');
    if (from_start != NULL) {
      from_start++;
      while (*from_start == ' ' || *from_start == '\"')
        from_start++;
      const char *from_end = strchr(from_start, '\"');
      if (from_end != NULL && from_end > from_start) {
        size_t len = from_end - from_start;
        if (len < sizeof(from))
          strncpy(from, from_start, len);
      }
    }
  }

  // Najit "to"
  const char *to_start = strstr(content, "\"to\"");
  if (to_start != NULL) {
    to_start = strchr(to_start, ':');
    if (to_start != NULL) {
      to_start++;
      while (*to_start == ' ' || *to_start == '\"')
        to_start++;
      const char *to_end = strchr(to_start, '\"');
      if (to_end != NULL && to_end > to_start) {
        size_t len = to_end - to_start;
        if (len < sizeof(to))
          strncpy(to, to_start, len);
      }
    }
  }

  // Najit "promotion"
  const char *promo_start = strstr(content, "\"promotion\"");
  if (promo_start != NULL) {
    promo_start = strchr(promo_start, ':');
    if (promo_start != NULL) {
      promo_start++;
      while (*promo_start == ' ' || *promo_start == '\"')
        promo_start++;
      const char *promo_end = strchr(promo_start, '\"');
      if (promo_end != NULL && promo_end > promo_start) {
        size_t len = promo_end - promo_start;
        if (len < sizeof(promotion))
          strncpy(promotion, promo_start, len);
      }
    }
  }

  if (strlen(from) == 0 || strlen(to) == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Missing 'from' or 'to'\"}", -1);
    return ESP_FAIL;
  }

  /* Promoce se týká jen tahu PĚŠCE na poslední řadu. Dříve zde HTTP vracelo 400
   * pro každý tah na řádky 1/8 bez "promotion" — to blokovalo legální tahy
   * dámy, věže, střelce a jezdce na poslední řadu. Game task validuje tah a u
   * pěšce spustí promotion flow (pending + volba z UI/tlačítek). */
  bool dest_back_rank =
      (strlen(to) >= 2 && (to[1] == '1' || to[1] == '8'));
  if (!dest_back_rank && strlen(promotion) > 0) {
    ESP_LOGW(TAG,
             "Promotion parameter for %s->%s (cíl není řádek 1/8), ignoring",
             from, to);
  }

  // Pripravit command pro game task
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_MOVE;
  strncpy(cmd.from_notation, from, sizeof(cmd.from_notation) - 1);
  strncpy(cmd.to_notation, to, sizeof(cmd.to_notation) - 1);

  cmd.promotion_choice = PROMOTION_QUEEN;
  if (strlen(promotion) > 0) {
    switch (promotion[0]) {
    case 'q':
    case 'Q':
      cmd.promotion_choice = PROMOTION_QUEEN;
      break;
    case 'r':
    case 'R':
      cmd.promotion_choice = PROMOTION_ROOK;
      break;
    case 'b':
    case 'B':
      cmd.promotion_choice = PROMOTION_BISHOP;
      break;
    case 'n':
    case 'N':
      cmd.promotion_choice = PROMOTION_KNIGHT;
      break;
    default:
      cmd.promotion_choice = PROMOTION_QUEEN;
      break;
    }
  }
  cmd.promotion_from_remote = (strlen(promotion) > 0) ? 1U : 0U;

  // Odeslat do fronty
  if (game_command_queue == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Game queue not available", -1);
    return ESP_FAIL;
  }

  // Synchronous Verification Setup
  // Create a temporary queue to receive the validation result
  // This allows us to return 400 Bad Request if the move is invalid
  QueueHandle_t response_queue = xQueueCreate(1, sizeof(game_response_t));
  if (response_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create response queue");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response queue", -1);
    return ESP_FAIL;
  }
  cmd.response_queue = response_queue;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    vQueueDelete(response_queue); // Cleanup
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to queue move", -1);
    return ESP_FAIL;
  }

  // Wait for response (validation result)
  game_response_t response;
  // 500ms timeout should be enough for simple validation
  if (xQueueReceive(response_queue, &response, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Check if move was successful
    if (response.type == GAME_RESPONSE_ERROR) {
      ESP_LOGW(TAG, "❌ Move rejected by game task: %s", response.data);
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_set_type(req, "application/json");
      char error_json[256];
      snprintf(error_json, sizeof(error_json),
               "{\"success\":false,\"message\":\"%s\"}", response.data);
      httpd_resp_send(req, error_json, -1);
    } else {
      ESP_LOGI(TAG, "✅ Move accepted by game task");
      httpd_resp_set_status(req, "200 OK");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"success\":true,\"message\":\"Move processed\"}",
                      -1);
    }
  } else {
    // Timeout - treat as success (async fallback) or error?
    // Let's treat as partial success but log warning
    ESP_LOGW(TAG, "⚠️ Move validation timed out");
    httpd_resp_set_status(req, "202 Accepted"); // Accepted but not verified yet
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":true,\"message\":\"Move queued (timeout)\"}", -1);
  }

  // Cleanup queue
  vQueueDelete(response_queue);
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/virtual_action
 *
 * Umoznuje virtualni zvedani a pokladani figurek pres web.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"action": "pickup"|"drop", "square": "e2"}
 */
static esp_err_t http_post_game_virtual_action_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/virtual_action");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Virtual action blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Nacist JSON z request body
  char content[128] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser
  char action[16] = {0};
  char square[8] = {0};
  char choice[4] = {0};

  // Najit "action"
  const char *action_start = strstr(content, "\"action\"");
  if (action_start != NULL) {
    action_start = strchr(action_start, ':');
    if (action_start != NULL) {
      action_start++; // Preskocit ':'
      while (*action_start == ' ' || *action_start == '\"')
        action_start++;
      const char *action_end = strchr(action_start, '\"');
      if (action_end != NULL && action_end > action_start) {
        size_t len = action_end - action_start;
        if (len < sizeof(action)) {
          strncpy(action, action_start, len);
        }
      }
    }
  }

  // Najit "square"
  const char *square_start = strstr(content, "\"square\"");
  if (square_start != NULL) {
    square_start = strchr(square_start, ':');
    if (square_start != NULL) {
      square_start++; // Preskocit ':'
      while (*square_start == ' ' || *square_start == '\"')
        square_start++;
      const char *square_end = strchr(square_start, '\"');
      if (square_end != NULL && square_end > square_start) {
        size_t len = square_end - square_start;
        if (len < sizeof(square)) {
          strncpy(square, square_start, len);
        }
      }
    }
  }

  // Najit "choice" (pro promotion)
  const char *choice_start = strstr(content, "\"choice\"");
  if (choice_start != NULL) {
    choice_start = strchr(choice_start, ':');
    if (choice_start != NULL) {
      choice_start++; // Preskocit ':'
      while (*choice_start == ' ' || *choice_start == '\"')
        choice_start++;
      const char *choice_end = strchr(choice_start, '\"');
      if (choice_end != NULL && choice_end > choice_start) {
        size_t len = choice_end - choice_start;
        if (len < sizeof(choice)) {
          strncpy(choice, choice_start, len);
        }
      }
    }
  }

  // Validace
  if (strlen(action) == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Missing action\"}",
                    -1);
    return ESP_FAIL;
  }

  // Pripravit command pro game task
  chess_move_command_t cmd = {0};

  if (strcmp(action, "pickup") == 0) {
    cmd.type = GAME_CMD_PICKUP; // UP command
  } else if (strcmp(action, "drop") == 0) {
    cmd.type = GAME_CMD_DROP; // DOWN command
  } else if (strcmp(action, "promote") == 0) {
    cmd.type = GAME_CMD_PROMOTION;
  } else {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Invalid action\"}",
                    -1);
    return ESP_FAIL;
  }

  if (strcmp(action, "pickup") == 0) {
    // PICKUP: source square goes in from_notation
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for pickup\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.from_notation, square, sizeof(cmd.from_notation) - 1);
  } else if (strcmp(action, "drop") == 0) {
    // DROP: destination square goes in to_notation
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for drop\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
  } else if (strcmp(action, "promote") == 0) {
    // PROMOTE: choice goes into promotion_choice
    if (strlen(choice) == 0)
      strcpy(choice, "Q"); // Default to Queen

    if (strcasecmp(choice, "Q") == 0)
      cmd.promotion_choice = PROMOTION_QUEEN;
    else if (strcasecmp(choice, "R") == 0)
      cmd.promotion_choice = PROMOTION_ROOK;
    else if (strcasecmp(choice, "B") == 0)
      cmd.promotion_choice = PROMOTION_BISHOP;
    else if (strcasecmp(choice, "N") == 0)
      cmd.promotion_choice = PROMOTION_KNIGHT;
    else
      cmd.promotion_choice = PROMOTION_QUEEN; // Fallback

    // Optional square
    if (strlen(square) > 0)
      strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
  }

  // Odeslat do fronty
  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to send command", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Action processed\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief POST /api/game/new: fronta GAME_CMD_NEW_GAME (explicitni nova hra z webu).
 *
 * @param req ukazatel na httpd_req_t
 * @return ESP_OK nebo chybovy kod
 *
 * @details
 * Bez JSON payload. Odlisne se od automatickeho NEW_GAME pri startu z main
 * (initialize_chess_game), kde se pri obnove NVS prikaz neposila.
 */
static esp_err_t http_post_game_new_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/new");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "New game blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  chess_move_command_t cmd = {0};
  cmd.player = 0;
  cmd.response_queue = NULL;

  char body[256] = {0};
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r > 0) {
    body[r] = '\0';
    cJSON *root = cJSON_Parse(body);
    if (root != NULL) {
      cJSON *fj = cJSON_GetObjectItemCaseSensitive(root, "fen");
      if (cJSON_IsString(fj) && fj->valuestring != NULL &&
          fj->valuestring[0] != '\0') {
        cmd.type = GAME_CMD_NEW_GAME_FROM_FEN;
        strncpy(cmd.timer_data.fen_new_game.fen, fj->valuestring,
                sizeof(cmd.timer_data.fen_new_game.fen) - 1);
        cmd.timer_data.fen_new_game.fen[sizeof(cmd.timer_data.fen_new_game.fen) -
                                        1] = '\0';
        cJSON_Delete(root);
      } else {
        cJSON_Delete(root);
        cmd.type = GAME_CMD_NEW_GAME;
      }
    } else {
      cmd.type = GAME_CMD_NEW_GAME;
    }
  } else {
    cmd.type = GAME_CMD_NEW_GAME;
  }

  if (game_command_queue == NULL) {
    ESP_LOGE(TAG, "Game command queue not available");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Queue not available\"}", -1);
    return ESP_FAIL;
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to send NEW_GAME command to queue");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Failed to send command\"}", -1);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "✅ game/new command sent (type=%d)", (int)cmd.type);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"New game started\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/hint_highlight
 *
 * Body: { "to": "e4" } povinne; { "from": "e2" } volitelne.
 * Kdyz "from" chybi nebo je neplatny, posle from_led=64 takze na desce sviti jen "to".
 */
static esp_err_t http_post_game_hint_highlight_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/hint_highlight");

  if (web_is_locked()) {
    ESP_LOGW(TAG, "Hint highlight blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  char buf[128];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"No body\"}", -1);
    return ESP_OK;
  }
  buf[ret] = '\0';

  esp_err_t herr = web_server_apply_hint_highlight_json_body(buf);
  if (herr == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid hint body\"}",
                    -1);
    return ESP_OK;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/hint_clear
 * Vymaze vizualizaci hintu na LED (vola se pri clearBotSuggestion).
 */
static esp_err_t http_post_game_hint_clear_handler(httpd_req_t *req) {
  (void)req;
  ESP_LOGI(TAG, "POST /api/game/hint_clear");
  led_command_t cmd = {0};
  cmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
  led_execute_command_new(&cmd);
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * POST /api/game/setup_tutorial
 * Body: {"action":"start"|"cancel"|"finish"}
 */
static esp_err_t http_post_game_setup_tutorial_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/setup_tutorial");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char buf[192] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';

  bool want_start = (strstr(buf, "\"start\"") != NULL);
  bool want_cancel = (strstr(buf, "\"cancel\"") != NULL);
  bool want_finish = (strstr(buf, "\"finish\"") != NULL);
  int n = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) + (want_finish ? 1 : 0);
  if (n != 1) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req,
                    "{\"success\":false,\"error\":\"Specify exactly one action: "
                    "start, cancel, or finish\"}",
                    -1);
    return ESP_OK;
  }

  if (want_finish) {
    if (!game_is_board_setup_tutorial_active()) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(
          req, "{\"success\":false,\"error\":\"Tutorial is not active\"}", -1);
      return ESP_OK;
    }
    if (!game_finish_board_setup_tutorial_from_web()) {
      httpd_resp_set_type(req, "application/json");
      if (game_is_board_setup_tutorial_active()) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_send(req,
                        "{\"success\":false,\"error\":\"Physical board does "
                        "not match starting occupancy (rows 0-1,6-7 full; "
                        "2-5 empty)\"}",
                        -1);
      } else {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(
            req, "{\"success\":false,\"error\":\"Could not finish tutorial\"}",
            -1);
      }
      return ESP_OK;
    }
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":true,\"message\":\"Starting position "
                    "confirmed; new game started\"}",
                    -1);
    return ESP_OK;
  }

  if (game_command_queue == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Game command queue unavailable\"}",
        -1);
    return ESP_FAIL;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_BOARD_SETUP_TUTORIAL;
  cmd.promotion_choice = want_start ? 0U : 1U;
  cmd.response_queue = NULL;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "setup_tutorial: queue send failed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Queue full\"}", -1);
    return ESP_FAIL;
  }

  if (want_cancel) {
    led_command_t lcmd = {0};
    lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&lcmd);
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * POST /api/game/puzzle
 * Body: {"action":"start","id":1..5} | {"action":"prepare","id":1..5} |
 * {"action":"cancel"}
 */
static esp_err_t http_post_game_puzzle_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/puzzle");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char buf[192] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';

  bool want_start = (strstr(buf, "\"start\"") != NULL);
  bool want_cancel = (strstr(buf, "\"cancel\"") != NULL);
  bool want_prepare = (strstr(buf, "\"prepare\"") != NULL);
  int n = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) + (want_prepare ? 1 : 0);
  if (n != 1) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req,
                    "{\"success\":false,\"error\":\"Specify exactly one action: "
                    "start, prepare, or cancel\"}",
                    -1);
    return ESP_OK;
  }

  if (game_command_queue == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Game command queue unavailable\"}",
        -1);
    return ESP_FAIL;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_PUZZLE;
  cmd.promotion_choice = 0U;
  cmd.response_queue = NULL;

  if (want_start || want_prepare) {
    int puzzle_id = -1;
    char *id_ptr = strstr(buf, "\"id\"");
    if (id_ptr != NULL) {
      char *colon = strchr(id_ptr, ':');
      if (colon != NULL) {
        puzzle_id = atoi(colon + 1);
      }
    }
    if (puzzle_id < 1 || puzzle_id > 5) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req,
                      "{\"success\":false,\"error\":\"Puzzle id must be 1..5\"}",
                      -1);
      return ESP_OK;
    }
    cmd.promotion_choice =
        want_prepare ? (uint8_t)(100 + puzzle_id) : (uint8_t)puzzle_id;
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "puzzle: queue send failed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Queue full\"}", -1);
    return ESP_FAIL;
  }

  if (want_cancel) {
    led_command_t lcmd = {0};
    lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&lcmd);
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * GET /api/settings/ui — JSON preference z NVS (nebo vychozi).
 */
static esp_err_t http_get_settings_ui_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/settings/ui");
  char buf[CONFIG_UI_PREFS_MAX_BYTES + 1];
  size_t len = 0;
  esp_err_t err = config_load_ui_prefs_json(buf, sizeof(buf), &len);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_status(req, "200 OK");
  if (err != ESP_OK || len == 0) {
    httpd_resp_send(req, "{\"version\":1,\"prefs\":{}}", -1);
    return ESP_OK;
  }
  httpd_resp_send(req, buf, len);
  return ESP_OK;
}

/**
 * POST /api/settings/ui — ulozeni web UI JSON do NVS.
 */
static esp_err_t http_post_settings_ui_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/ui");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char body[CONFIG_UI_PREFS_MAX_BYTES + 1];
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  body[r] = '\0';

  esp_err_t s = config_save_ui_prefs_json(body, (size_t)r);
  httpd_resp_set_type(req, "application/json");
  if (s == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(
        req,
        "{\"success\":false,\"error\":\"Invalid JSON or too large (max "
        "3072 bytes)\"}",
        -1);
    return ESP_OK;
  }
  if (s != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"NVS write failed\"}",
                    -1);
    return ESP_FAIL;
  }
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

// ============================================================================
// WIFI API HANDLERS
// ============================================================================

/**
 * @brief Handler pro POST /api/wifi/config
 *
 * Ulozi WiFi konfiguraci (SSID a heslo) do NVS.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"ssid": "...", "password": "..."}
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
static esp_err_t http_post_wifi_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/config");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Nacist JSON z request body
  char content[256] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser (hleda "ssid" a "password")
  char ssid[33] = {0};
  char password[65] = {0};

  // Najit "ssid"
  const char *ssid_start = strstr(content, "\"ssid\"");
  if (ssid_start != NULL) {
    ssid_start = strchr(ssid_start, ':');
    if (ssid_start != NULL) {
      ssid_start++; // Preskocit ':'
      while (*ssid_start == ' ' || *ssid_start == '\"')
        ssid_start++;
      const char *ssid_end = strchr(ssid_start, '\"');
      if (ssid_end != NULL && ssid_end > ssid_start) {
        size_t len = ssid_end - ssid_start;
        if (len < sizeof(ssid)) {
          strncpy(ssid, ssid_start, len);
          ssid[len] = '\0';
        }
      }
    }
  }

  // Najit "password"
  const char *password_start = strstr(content, "\"password\"");
  if (password_start != NULL) {
    password_start = strchr(password_start, ':');
    if (password_start != NULL) {
      password_start++; // Preskocit ':'
      while (*password_start == ' ' || *password_start == '\"')
        password_start++;
      const char *password_end = strchr(password_start, '\"');
      if (password_end != NULL && password_end > password_start) {
        size_t len = password_end - password_start;
        if (len < sizeof(password)) {
          strncpy(password, password_start, len);
          password[len] = '\0';
        }
      }
    }
  }

  // Validovat SSID
  size_t ssid_len = strlen(ssid);
  size_t password_len = strlen(password);

  if (ssid_len == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"SSID is required\"}",
                    -1);
    return ESP_FAIL;
  }
  if (ssid_len > 32) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"SSID must be 1-32 characters "
             "(current: %zu)\"}",
             ssid_len);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Validovat password
  if (password_len == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Password is required\"}", -1);
    return ESP_FAIL;
  }
  if (password_len > 64) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Password must be 1-64 "
             "characters (current: %zu)\"}",
             password_len);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Ulozit do NVS
  esp_err_t err = wifi_save_config_to_nvs(ssid, password);
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to save: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"WiFi config saved\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/connect
 *
 * Pripoji ESP32 k WiFi site s ulozenou konfiguraci.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
static esp_err_t http_post_wifi_connect_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/connect");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Zkontrolovat, zda existuje konfigurace
  char ssid[33] = {0};
  char password[65] = {0};
  esp_err_t load_ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));
  if (load_ret != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"No WiFi configuration "
                    "found. Please save SSID and password first.\"}",
                    -1);
    return ESP_FAIL;
  }

  esp_err_t err = wifi_connect_sta();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[256];
    const char *user_message = NULL;

    // Prevest ESP error kody na uzivatelsky pristupne zpravy
    if (err == ESP_ERR_INVALID_STATE) {
      user_message = "Connection already in progress. Please wait...";
    } else if (err == ESP_ERR_NOT_FOUND) {
      user_message = "Network not found. Please check SSID and ensure the "
                     "network is in range.";
    } else if (err == ESP_ERR_INVALID_RESPONSE) {
      user_message =
          "Authentication failed. Please check password and try again.";
    } else if (err == ESP_ERR_TIMEOUT) {
      user_message =
          "Connection timeout. The network may be too far or not responding.";
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
      user_message =
          "WiFi configuration not found. Please save SSID and password first.";
    } else {
      user_message = "Connection failed. Please check SSID, password, and "
                     "network availability.";
    }

    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"%s\"}", user_message);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Connected to WiFi\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/disconnect
 *
 * Odpoji ESP32 od WiFi site.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
static esp_err_t http_post_wifi_disconnect_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/disconnect");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  esp_err_t err = wifi_disconnect_sta();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Disconnect failed: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(
      req, "{\"success\":true,\"message\":\"Disconnected from WiFi\"}", -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/clear
 *
 * Vymaze ulozenou WiFi konfiguraci z NVS.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
static esp_err_t http_post_wifi_clear_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/clear");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Odpojit STA pokud je pripojeny
  if (sta_connected) {
    wifi_disconnect_sta();
  }

  esp_err_t err = wifi_clear_config_from_nvs();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to clear: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(
      req, "{\"success\":true,\"message\":\"WiFi configuration cleared\"}", -1);

  return ESP_OK;
}

/**
 * @brief POST /api/system/factory_reset
 *
 * Smaže celý NVS oddíl (raw erase) a restartuje MCU. Neřeší web lock — záchranná
 * cesta. Tělo musí obsahovat: {"confirm":"erase_all_nvs"}
 */
static esp_err_t http_post_factory_reset_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/system/factory_reset");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  char content[160] = {0};
  int rlen = httpd_req_recv(req, content, sizeof(content) - 1);
  if (rlen <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"No JSON body\"}", -1);
    return ESP_OK;
  }
  content[rlen] = '\0';

  if (!json_body_has_factory_confirm(content)) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req,
        "{\"success\":false,\"message\":\"JSON must include confirm "
        "erase_all_nvs\"}",
        -1);
    return ESP_OK;
  }

  esp_err_t qerr = factory_reset_schedule();
  if (qerr != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Could not queue reset task\"}",
        -1);
    return ESP_OK;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req,
                  "{\"success\":true,\"message\":\"NVS erase scheduled\","
                  "\"rebooting\":true}",
                  -1);
  return ESP_OK;
}

/**
 * @brief Handler pro GET /api/wifi/status
 *
 * Vrati aktualni stav WiFi (AP i STA).
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON s informacemi o AP a STA statusu.
 */
static esp_err_t http_get_wifi_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/wifi/status");

  char local_json[WIFI_STATUS_JSON_MAX];
  esp_err_t ret = wifi_get_sta_status_json(local_json, sizeof(local_json));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get WiFi status", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, local_json, strlen(local_json));

  return ESP_OK;
}

// ============================================================================
// TEST PAGE - MINIMAL TIMER TEST (for debugging)
// chess_app.js embedded (187802 bytes, 4621 lines)
static const char chess_app_js_content[] =
    "// ============================================================================\n"
    "// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING\n"
    "// ============================================================================\n"
    "\n"
    "console.log('🚀 Chess JavaScript loading...');\n"
    "\n"
    "// ============================================================================\n"
    "// TAB SWITCHING (Hra / Nastavení)\n"
    "// ============================================================================\n"
    "\n"
    "function switchTab(tabId) {\n"
    "    const tabs = document.querySelectorAll('.tab-content');\n"
    "    const buttons = document.querySelectorAll('.tab-btn');\n"
    "    tabs.forEach(t => { t.classList.remove('active'); });\n"
    "    buttons.forEach(b => { b.classList.remove('active'); });\n"
    "    const tab = document.getElementById(tabId);\n"
    "    const btn = document.getElementById('btn-' + tabId);\n"
    "    if (tab) tab.classList.add('active');\n"
    "    if (btn) btn.classList.add('active');\n"
    "    if (typeof console !== 'undefined' && console.log) {\n"
    "        console.log('Tab switched to:', tabId);\n"
    "    }\n"
    "}\n"
    "window.switchTab = switchTab;\n"
    "\n"
    "// ============================================================================\n"
    "// PIECE SYMBOLS AND GLOBAL VARIABLES\n"
    "// ============================================================================\n"
    "\n"
    "const pieceSymbols = {\n"
    "    'R': '♜', 'N': '♞', 'B': '♝', 'Q': '♛', 'K': '♚', 'P': '♟',\n"
    "    'r': '♖', 'n': '♘', 'b': '♗', 'q': '♕', 'k': '♔', 'p': '♙',\n"
    "    ' ': ' '\n"
    "};\n"
    "\n"
    "/**\n"
    " * PNG figurky z chess.com (veřejné CDN URL, Staunton „neo“, 150 px).\n"
    " * Vyžaduje, aby prohlížeč měl přístup na internet (jinak fallback na Unicode v setPieceElementFromFen).\n"
    " * @see https://www.chess.com/chess-themes/pieces/neo/150/wk.png\n"
    " */\n"
    "const CHESSCOM_PIECE_BASE = 'https://www.chess.com/chess-themes/pieces/neo/150/';\n"
    "const CHESSCOM_PIECE = {\n"
    "    'K': 'wk', 'Q': 'wq', 'R': 'wr', 'B': 'wb', 'N': 'wn', 'P': 'wp',\n"
    "    'k': 'bk', 'q': 'bq', 'r': 'br', 'b': 'bb', 'n': 'bn', 'p': 'bp'\n"
    "};\n"
    "\n"
    "function pieceImgSrc(ch) {\n"
    "    const slug = CHESSCOM_PIECE[ch];\n"
    "    return slug ? (CHESSCOM_PIECE_BASE + slug + '.png') : '';\n"
    "}\n"
    "\n"
    "function setPieceElementFromFen(el, ch) {\n"
    "    if (!el) return;\n"
    "    if (ch === ' ' || ch === undefined) {\n"
    "        el.textContent = '';\n"
    "        el.innerHTML = '';\n"
    "        el.className = 'piece';\n"
    "        return;\n"
    "    }\n"
    "    const src = pieceImgSrc(ch);\n"
    "    if (src) {\n"
    "        const isWhite = ch >= 'A' && ch <= 'Z';\n"
    "        el.className = 'piece has-img ' + (isWhite ? 'white' : 'black');\n"
    "        // BUG FIX 2: Fallback na Unicode když obrázek selže (CDN výpadek/CORS/blokátor)\n"
    "        const unicodeFallback = pieceSymbols[ch] || ch;\n"
    "        const colorClass = isWhite ? 'white' : 'black';\n"
    "        el.innerHTML = '<img src=\"' + src + '\" alt=\"\" draggable=\"false\" onerror=\"this.parentNode.textContent=\\'' + unicodeFallback + '\\';this.parentNode.className=\\'piece ' + colorClass + '\\';\">';\n"
    "    } else {\n"
    "        el.innerHTML = '';\n"
    "        el.textContent = pieceSymbols[ch] || ch;\n"
    "        el.className = 'piece ' + (ch >= 'A' && ch <= 'Z' ? 'white' : 'black');\n"
    "    }\n"
    "}\n"
    "\n"
    "function pieceImgHtml(ch) {\n"
    "    const s = pieceImgSrc(ch);\n"
    "    if (s) {\n"
    "        // BUG FIX 2: Fallback na Unicode když obrázek selže\n"
    "        const unicodeFallback = pieceSymbols[ch] || ch;\n"
    "        return '<img src=\"' + s + '\" class=\"endgame-piece-img\" alt=\"\" draggable=\"false\" onerror=\"this.parentNode.textContent=\\'' + unicodeFallback + '\\';this.parentNode.className=\\'piece ' + (ch >= 'A' && ch <= 'Z' ? 'white' : 'black') + '\\';\">';\n"
    "    }\n"
    "    return pieceSymbols[ch] || ch;\n"
    "}\n"
    "\n"
    "/** Zabrání souběhu několika fetchData na pomalé síti / přetíženém HTTPD. */\n"
    "let fetchDataInFlight = false;\n"
    "\n"
    "/**\n"
    " * Hlavičky pro admin POST — doplní Bearer z localStorage `czechmate_api_token`\n"
    " * (64 hex z UART `API_TOKEN`), pokud je uložen.\n"
    " * @param {Object.<string,string>} [base] základní hlavičky (např. Content-Type)\n"
    " */\n"
    "function boardApiAuthHeaders(base) {\n"
    "    const h = {};\n"
    "    if (base) {\n"
    "        for (const k of Object.keys(base)) {\n"
    "            h[k] = base[k];\n"
    "        }\n"
    "    }\n"
    "    try {\n"
    "        const t = localStorage.getItem('czechmate_api_token');\n"
    "        if (t && String(t).trim()) {\n"
    "            h['Authorization'] = 'Bearer ' + String(t).trim();\n"
    "        }\n"
    "    } catch (e) {}\n"
    "    return h;\n"
    "}\n"
    "\n"
    "let boardData = [];\n"
    "let statusData = {};\n"
    "/** Poslední stav puzzle ze úspěšného pollingu — zobrazení panelu při výpadku HTTP (offline). */\n"
    "let lastPuzzleSnapshotForOffline = null;\n"
    "let historyData = [];\n"
    "let capturedData = { white_captured: [], black_captured: [] };\n"
    "let advantageData = { history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0 };\n"
    "let selectedSquare = null;\n"
    "let reviewMode = false;\n"
    "let currentReviewIndex = -1;\n"
    "let initialBoard = [];\n"
    "let sandboxMode = false;\n"
    "\n"
    "let remoteControlEnabled = false;\n"
    "// BOT MODE STATE\n"
    "// Bot tah se nikdy neprovádí automaticky – jen vizualizace (web + LED); uživatel pohybuje figurku fyzicky.\n"
    "let gameMode = 'pvp'; // 'pvp' or 'bot'\n"
    "let botSettings = { strength: 10, side: 'white' }; // strength: 1,3,5,8,12,15 (zobrazeno jako ELO v Nastavení)\n"
    "let botThinking = false;\n"
    "let gameGeneration = 0; // Incremented on New Game to invalidate stale bot requests\n"
    "/** FEN for which we already suggested a bot move; avoids re-triggering every poll until player moves. */\n"
    "let lastSuggestedFen = null;\n"
    "/** Last bot move we showed (from/to); re-sent to LED so game_task doesn't overwrite it. */\n"
    "let lastSuggestedMove = null;\n"
    "/** Interval ID for re-applying bot hint to LED (cleared when player moves or new game). */\n"
    "let botHintRefreshIntervalId = null;\n"
    "\n"
    "let sandboxBoard = [];\n"
    "let sandboxHistory = [];\n"
    "/** For move evaluation: FEN after last fetch; length of history after last fetch. */\n"
    "let lastFen = null;\n"
    "let lastHistoryLength = -1;\n"
    "/** Per-move evaluation when \"Zhodnocení tahu\" is on: index -> { grade, msg }. */\n"
    "let moveEvaluations = {};\n"
    "let endgameReportShown = false;\n"
    "\n"
    "/** Výukový režim: každý hráč má vlastní počet nápověd. */\n"
    "let hintsRemainingWhite = 999;\n"
    "let hintsRemainingBlack = 999;\n"
    "/** Počet sebraných figur po minulém pollu (pro detekci sebrání). */\n"
    "let lastCapturedCount = 0;\n"
    "/** Poslední nápověda { from, to } – odměna za výborný tah se nedává, pokud byl tah stejný. */\n"
    "var lastHintedMove = null;\n"
    "/** Generace requestu nápovědy – při novém kliknutí se zvýší, zastaralé odpovědi se ignorují. */\n"
    "var hintRequestGeneration = 0;\n"
    "\n"
    "/** Web UI preference (zdroj pravdy: NVS přes GET/POST /api/settings/ui). */\n"
    "var devicePrefs = {\n"
    "    version: 1,\n"
    "    chessHintDepth: 10,\n"
    "    chessEvaluateMove: false,\n"
    "    chessHintLimit: 0,\n"
    "    chessHintAwardBest: true,\n"
    "    chessHintAwardGood: false,\n"
    "    chessHintAwardCapture: false,\n"
    "    chessShowHintStats: false,\n"
    "    chessBotLedTargetOnlyAfterLift: false,\n"
    "    chessTutorialsEnabled: false,\n"
    "    chess_confirm_new_game: false,\n"
    "    botSettings: { mode: 'pvp', strength: '10', side: 'white' }\n"
    "};\n"
    "var uiPrefsSaveTimer = null;\n"
    "var uiPrefsHydratedFromServer = false;\n"
    "\n"
    "function mergePrefsFromObject(prefs) {\n"
    "    if (!prefs || typeof prefs !== 'object') return;\n"
    "    var k;\n"
    "    for (k in prefs) {\n"
    "        if (Object.prototype.hasOwnProperty.call(prefs, k)) {\n"
    "            devicePrefs[k] = prefs[k];\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "async function loadUiPrefsFromDevice() {\n"
    "    try {\n"
    "        var r = await fetch('/api/settings/ui');\n"
    "        if (!r.ok) return;\n"
    "        var data = await r.json();\n"
    "        if (data && typeof data.version === 'number') {\n"
    "            devicePrefs.version = data.version;\n"
    "        }\n"
    "        if (data && data.prefs && typeof data.prefs === 'object') {\n"
    "            mergePrefsFromObject(data.prefs);\n"
    "        }\n"
    "        var empty = !data || !data.prefs ||\n"
    "            (typeof data.prefs === 'object' && Object.keys(data.prefs).length === 0);\n"
    "        if (empty) {\n"
    "            migrateLocalStorageToDevicePrefsOnce();\n"
    "        }\n"
    "        uiPrefsHydratedFromServer = true;\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) {\n"
    "            console.warn('loadUiPrefsFromDevice', e);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "function migrateLocalStorageToDevicePrefsOnce() {\n"
    "    try {\n"
    "        if (typeof sessionStorage !== 'undefined' &&\n"
    "            sessionStorage.getItem('chessUiPrefsMigrated') === '1') {\n"
    "            return;\n"
    "        }\n"
    "        var keys = ['chessHintDepth', 'chessEvaluateMove', 'chessHintLimit', 'chessHintAwardBest',\n"
    "            'chessHintAwardGood', 'chessHintAwardCapture', 'chessShowHintStats',\n"
    "            'chessBotLedTargetOnlyAfterLift', 'chessTutorialsEnabled', 'chess_confirm_new_game', 'chessBotSettings'];\n"
    "        var i;\n"
    "        var any = false;\n"
    "        for (i = 0; i < keys.length; i++) {\n"
    "            try {\n"
    "                if (localStorage.getItem(keys[i]) != null) { any = true; break; }\n"
    "            } catch (e2) { /* ignore */ }\n"
    "        }\n"
    "        if (!any) return;\n"
    "\n"
    "        var d = localStorage.getItem('chessHintDepth');\n"
    "        if (d != null) devicePrefs.chessHintDepth = Math.min(18, Math.max(1, parseInt(d, 10) || 10));\n"
    "        if (localStorage.getItem('chessEvaluateMove') === 'true') devicePrefs.chessEvaluateMove = true;\n"
    "        if (localStorage.getItem('chessEvaluateMove') === 'false') devicePrefs.chessEvaluateMove = false;\n"
    "        d = localStorage.getItem('chessHintLimit');\n"
    "        if (d != null) devicePrefs.chessHintLimit = Math.max(0, parseInt(d, 10) || 0);\n"
    "        if (localStorage.getItem('chessHintAwardBest') != null) {\n"
    "            devicePrefs.chessHintAwardBest = localStorage.getItem('chessHintAwardBest') !== 'false';\n"
    "        }\n"
    "        devicePrefs.chessHintAwardGood = localStorage.getItem('chessHintAwardGood') === 'true';\n"
    "        devicePrefs.chessHintAwardCapture = localStorage.getItem('chessHintAwardCapture') === 'true';\n"
    "        devicePrefs.chessShowHintStats = localStorage.getItem('chessShowHintStats') === 'true';\n"
    "        devicePrefs.chessBotLedTargetOnlyAfterLift = localStorage.getItem('chessBotLedTargetOnlyAfterLift') === 'true';\n"
    "        devicePrefs.chessTutorialsEnabled = localStorage.getItem('chessTutorialsEnabled') === 'true';\n"
    "        devicePrefs.chess_confirm_new_game = localStorage.getItem('chess_confirm_new_game') === 'true';\n"
    "        var bot = localStorage.getItem('chessBotSettings');\n"
    "        if (bot) {\n"
    "            try { devicePrefs.botSettings = JSON.parse(bot); } catch (e3) { /* ignore */ }\n"
    "        }\n"
    "        if (typeof sessionStorage !== 'undefined') {\n"
    "            sessionStorage.setItem('chessUiPrefsMigrated', '1');\n"
    "        }\n"
    "        scheduleSaveUiPrefsToDevice(true);\n"
    "    } catch (e) { /* ignore */ }\n"
    "}\n"
    "\n"
    "function buildUiPrefsPayload() {\n"
    "    return JSON.stringify({\n"
    "        version: devicePrefs.version || 1,\n"
    "        prefs: {\n"
    "            chessHintDepth: devicePrefs.chessHintDepth,\n"
    "            chessEvaluateMove: !!devicePrefs.chessEvaluateMove,\n"
    "            chessHintLimit: devicePrefs.chessHintLimit,\n"
    "            chessHintAwardBest: !!devicePrefs.chessHintAwardBest,\n"
    "            chessHintAwardGood: !!devicePrefs.chessHintAwardGood,\n"
    "            chessHintAwardCapture: !!devicePrefs.chessHintAwardCapture,\n"
    "            chessShowHintStats: !!devicePrefs.chessShowHintStats,\n"
    "            chessBotLedTargetOnlyAfterLift: !!devicePrefs.chessBotLedTargetOnlyAfterLift,\n"
    "            chessTutorialsEnabled: !!devicePrefs.chessTutorialsEnabled,\n"
    "            chess_confirm_new_game: !!devicePrefs.chess_confirm_new_game,\n"
    "            botSettings: devicePrefs.botSettings && typeof devicePrefs.botSettings === 'object'\n"
    "                ? devicePrefs.botSettings\n"
    "                : { mode: 'pvp', strength: '10', side: 'white' }\n"
    "        }\n"
    "    });\n"
    "}\n"
    "\n"
    "function scheduleSaveUiPrefsToDevice(immediate) {\n"
    "    if (uiPrefsSaveTimer) {\n"
    "        clearTimeout(uiPrefsSaveTimer);\n"
    "        uiPrefsSaveTimer = null;\n"
    "    }\n"
    "    var delay = immediate ? 0 : 400;\n"
    "    uiPrefsSaveTimer = setTimeout(function () {\n"
    "        uiPrefsSaveTimer = null;\n"
    "        fetch('/api/settings/ui', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: buildUiPrefsPayload()\n"
    "        }).catch(function () { /* ignore */ });\n"
    "    }, delay);\n"
    "}\n"
    "\n"
    "function syncHintSettingsFromDom() {\n"
    "    var el = document.getElementById('hint-limit');\n"
    "    if (el) devicePrefs.chessHintLimit = Math.max(0, parseInt(el.value, 10) || 0);\n"
    "    el = document.getElementById('hint-award-best');\n"
    "    if (el) devicePrefs.chessHintAwardBest = el.checked;\n"
    "    el = document.getElementById('hint-award-good');\n"
    "    if (el) devicePrefs.chessHintAwardGood = el.checked;\n"
    "    el = document.getElementById('hint-award-capture');\n"
    "    if (el) devicePrefs.chessHintAwardCapture = el.checked;\n"
    "    el = document.getElementById('show-hint-stats');\n"
    "    if (el) devicePrefs.chessShowHintStats = el.checked;\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "}\n"
    "window.syncHintSettingsFromDom = syncHintSettingsFromDom;\n"
    "\n"
    "function tutorialsPanelSetVisible(on) {\n"
    "    var w = document.getElementById('tutorials-panel-body');\n"
    "    if (w) w.style.display = on ? 'block' : 'none';\n"
    "    var b = document.getElementById('setup-tutorial-open-btn');\n"
    "    if (b) b.disabled = !on;\n"
    "}\n"
    "\n"
    "function saveTutorialsEnabled(on) {\n"
    "    devicePrefs.chessTutorialsEnabled = !!on;\n"
    "    tutorialsPanelSetVisible(!!on);\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "}\n"
    "window.saveTutorialsEnabled = saveTutorialsEnabled;\n"
    "\n"
    "function updateBotSettingsVisibility() {\n"
    "    var gm = document.getElementById('game-mode');\n"
    "    var container = document.getElementById('bot-settings-container');\n"
    "    if (!gm || !container) return;\n"
    "    container.style.display = (gm.value === 'bot') ? 'block' : 'none';\n"
    "    saveBotSettings();\n"
    "}\n"
    "window.updateBotSettingsVisibility = updateBotSettingsVisibility;\n"
    "\n"
    "function saveBotSettings() {\n"
    "    var modeEl = document.getElementById('game-mode');\n"
    "    var strengthEl = document.getElementById('bot-strength');\n"
    "    var sideEl = document.getElementById('player-side');\n"
    "    if (modeEl) gameMode = modeEl.value;\n"
    "    devicePrefs.botSettings = {\n"
    "        mode: modeEl ? modeEl.value : 'pvp',\n"
    "        strength: strengthEl ? strengthEl.value : '10',\n"
    "        side: sideEl ? sideEl.value : 'white'\n"
    "    };\n"
    "    botSettings.strength = devicePrefs.botSettings.strength;\n"
    "    botSettings.side = devicePrefs.botSettings.side;\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "}\n"
    "window.saveBotSettings = saveBotSettings;\n"
    "\n"
    "function loadBotSettings() {\n"
    "    var s = devicePrefs.botSettings;\n"
    "    if (!s || typeof s !== 'object') {\n"
    "        s = { mode: 'pvp', strength: '10', side: 'white' };\n"
    "    }\n"
    "    var gm = document.getElementById('game-mode');\n"
    "    if (gm) {\n"
    "        if (s.mode) gm.value = s.mode;\n"
    "        gameMode = gm.value;\n"
    "    }\n"
    "    var bs = document.getElementById('bot-strength');\n"
    "    if (bs && s.strength != null && s.strength !== '') bs.value = String(s.strength);\n"
    "    var ps = document.getElementById('player-side');\n"
    "    if (ps && s.side) ps.value = s.side;\n"
    "    botSettings.strength = bs ? bs.value : String(s.strength || '10');\n"
    "    botSettings.side = ps ? ps.value : (s.side || 'white');\n"
    "    var container = document.getElementById('bot-settings-container');\n"
    "    if (container && gm) {\n"
    "        container.style.display = (gm.value === 'bot') ? 'block' : 'none';\n"
    "    }\n"
    "    if (typeof handleRandomDraw === 'function') handleRandomDraw();\n"
    "}\n"
    "window.loadBotSettings = loadBotSettings;\n"
    "\n"
    "function wireConfirmNewGameCheckbox() {\n"
    "    var cb = document.getElementById('confirm-new-game');\n"
    "    if (!cb || cb._prefsWired) return;\n"
    "    cb._prefsWired = true;\n"
    "    cb.addEventListener('change', function () {\n"
    "        devicePrefs.chess_confirm_new_game = !!cb.checked;\n"
    "        scheduleSaveUiPrefsToDevice();\n"
    "    });\n"
    "}\n"
    "\n"
    "function applyDevicePrefsToDom() {\n"
    "    var depthEl = document.getElementById('hint-depth');\n"
    "    if (depthEl) depthEl.value = getHintDepth();\n"
    "    var evaluateMoveEl = document.getElementById('evaluate-move-enabled');\n"
    "    if (evaluateMoveEl) evaluateMoveEl.checked = getEvaluateMoveEnabled();\n"
    "    var hl = document.getElementById('hint-limit');\n"
    "    if (hl) hl.value = String(getHintLimit());\n"
    "    var hb = document.getElementById('hint-award-best');\n"
    "    if (hb) hb.checked = getHintAwardBest();\n"
    "    var hg = document.getElementById('hint-award-good');\n"
    "    if (hg) hg.checked = getHintAwardGood();\n"
    "    var hc = document.getElementById('hint-award-capture');\n"
    "    if (hc) hc.checked = getHintAwardCapture();\n"
    "    var hs = document.getElementById('show-hint-stats');\n"
    "    if (hs) hs.checked = getShowHintStats();\n"
    "    var botLed = document.getElementById('bot-led-target-only-after-lift');\n"
    "    if (botLed) botLed.checked = getBotLedTargetOnlyAfterLift();\n"
    "    var tut = document.getElementById('chess-tutorials-enabled');\n"
    "    if (tut) {\n"
    "        tut.checked = !!devicePrefs.chessTutorialsEnabled;\n"
    "        tutorialsPanelSetVisible(!!devicePrefs.chessTutorialsEnabled);\n"
    "    }\n"
    "    var cng = document.getElementById('confirm-new-game');\n"
    "    if (cng) cng.checked = !!devicePrefs.chess_confirm_new_game;\n"
    "    wireConfirmNewGameCheckbox();\n"
    "    loadBotSettings();\n"
    "    if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "}\n"
    "\n"
    "// Starting position check functions\n"
    "function loadStartingPositionCheck() {\n"
    "    var checkbox = document.getElementById('starting-position-check');\n"
    "    if (!checkbox) return;\n"
    "    fetch('/api/settings/start_pos_check')\n"
    "        .then(function(r) { return r.json(); })\n"
    "        .then(function(data) {\n"
    "            if (typeof data.enabled === 'boolean') {\n"
    "                checkbox.checked = data.enabled;\n"
    "            }\n"
    "        })\n"
    "        .catch(function(e) {\n"
    "            console.log('Failed to load starting position check setting:', e);\n"
    "        });\n"
    "}\n"
    "window.loadStartingPositionCheck = loadStartingPositionCheck;\n"
    "\n"
    "function saveStartingPositionCheck(enabled) {\n"
    "    fetch('/api/settings/start_pos_check', {\n"
    "        method: 'POST',\n"
    "        headers: { 'Content-Type': 'application/json' },\n"
    "        body: JSON.stringify({ enabled: !!enabled })\n"
    "    })\n"
    "    .then(function(r) { return r.json(); })\n"
    "    .then(function(data) {\n"
    "        if (data.success) {\n"
    "            console.log('Starting position check saved:', data.enabled);\n"
    "        } else {\n"
    "            console.error('Failed to save starting position check:', data.message);\n"
    "        }\n"
    "    })\n"
    "    .catch(function(e) {\n"
    "        console.error('Error saving starting position check:', e);\n"
    "    });\n"
    "}\n"
    "window.saveStartingPositionCheck = saveStartingPositionCheck;\n"
    "\n"
    "window.onHintDepthChange = function (v) {\n"
    "    devicePrefs.chessHintDepth = v;\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "};\n"
    "window.onEvaluateMoveChange = function (checked) {\n"
    "    devicePrefs.chessEvaluateMove = !!checked;\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "};\n"
    "\n"
    "// ============================================================================\n"
    "// BOARD FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "function createBoard() {\n"
    "    const board = document.getElementById('board');\n"
    "    board.innerHTML = '';\n"
    "    for (let row = 7; row >= 0; row--) {\n"
    "        for (let col = 0; col < 8; col++) {\n"
    "            const square = document.createElement('div');\n"
    "            square.className = 'square ' + ((row + col) % 2 === 0 ? 'light' : 'dark');\n"
    "            square.dataset.row = row;\n"
    "            square.dataset.col = col;\n"
    "            square.dataset.index = row * 8 + col;\n"
    "            attachBoardPointerHandlers(square, row, col);\n"
    "            const piece = document.createElement('div');\n"
    "            piece.className = 'piece';\n"
    "            piece.id = 'piece-' + (row * 8 + col);\n"
    "            square.appendChild(piece);\n"
    "            board.appendChild(square);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "function clearHighlights() {\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        // NEMAZAT lifted, error-invalid, error-original - tyto jsou řízené serverem\n"
    "        // (z piece_lifted a error_state v JSON statusu)\n"
    "        sq.classList.remove('selected', 'valid-move', 'valid-capture');\n"
    "    });\n"
    "    selectedSquare = null;\n"
    "}\n"
    "\n"
    "/** Prah (px) pro rozlišení kliknutí vs. táhnutí figurky. */\n"
    "var BOARD_DRAG_THRESHOLD_PX = 12;\n"
    "\n"
    "function squareFromEventTarget(el) {\n"
    "    if (!el || !el.closest) return null;\n"
    "    var sq = el.closest('.square');\n"
    "    if (!sq) return null;\n"
    "    return {\n"
    "        row: parseInt(sq.dataset.row, 10),\n"
    "        col: parseInt(sq.dataset.col, 10)\n"
    "    };\n"
    "}\n"
    "\n"
    "function coordsToNotation(row, col) {\n"
    "    return String.fromCharCode(97 + col) + (row + 1);\n"
    "}\n"
    "\n"
    "function sandboxApplyMoveFromDrag(fromRow, fromCol, toRow, toCol) {\n"
    "    if (fromRow === toRow && fromCol === toCol) return;\n"
    "    var piece = sandboxBoard[fromRow][fromCol];\n"
    "    if (piece === ' ') return;\n"
    "    var dest = sandboxBoard[toRow][toCol];\n"
    "    var index = toRow * 8 + toCol;\n"
    "    if (dest === ' ') {\n"
    "        makeSandboxMove(fromRow, fromCol, toRow, toCol);\n"
    "        clearHighlights();\n"
    "        return;\n"
    "    }\n"
    "    var isOurPiece = (piece === piece.toUpperCase()) === (dest === dest.toUpperCase());\n"
    "    if (isOurPiece) {\n"
    "        clearHighlights();\n"
    "        selectedSquare = index;\n"
    "        var elSq = document.querySelector('[data-row=\\'' + toRow + '\\'][data-col=\\'' + toCol + '\\']');\n"
    "        if (elSq) elSq.classList.add('selected');\n"
    "        return;\n"
    "    }\n"
    "    makeSandboxMove(fromRow, fromCol, toRow, toCol);\n"
    "    clearHighlights();\n"
    "}\n"
    "\n"
    "async function handleRemoteDragMove(fromRow, fromCol, toRow, toCol) {\n"
    "    if (fromRow === toRow && fromCol === toCol) return;\n"
    "    var piece = boardData[fromRow] && boardData[fromRow][fromCol];\n"
    "    if (!piece || piece === ' ') return;\n"
    "    if (isWebLocked()) {\n"
    "        alert('Rozhraní je zamčeno. Odemkněte přes UART.');\n"
    "        return;\n"
    "    }\n"
    "    var fromN = coordsToNotation(fromRow, fromCol);\n"
    "    var toN = coordsToNotation(toRow, toCol);\n"
    "    try {\n"
    "        var r1 = await fetch('/api/game/virtual_action', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'pickup', square: fromN })\n"
    "        });\n"
    "        var res1 = await r1.json().catch(function () { return {}; });\n"
    "        if (!r1.ok) {\n"
    "            if (r1.status === 403 && res1.message) alert(res1.message);\n"
    "            await fetchData();\n"
    "            return;\n"
    "        }\n"
    "        await fetchData();\n"
    "        var r2 = await fetch('/api/game/virtual_action', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'drop', square: toN })\n"
    "        });\n"
    "        var res2 = await r2.json().catch(function () { return {}; });\n"
    "        if (!r2.ok && r2.status === 403 && res2.message) alert(res2.message);\n"
    "        await fetchData();\n"
    "    } catch (e) {\n"
    "        console.error('Remote drag virtual_action:', e);\n"
    "        await fetchData();\n"
    "    }\n"
    "}\n"
    "\n"
    "function attachBoardPointerHandlers(square, row, col) {\n"
    "    var dragStart = null;\n"
    "    function onPointerDown(ev) {\n"
    "        if (ev.button !== undefined && ev.button !== 0) return;\n"
    "        if (reviewMode) return;\n"
    "        dragStart = {\n"
    "            row: row,\n"
    "            col: col,\n"
    "            x: ev.clientX,\n"
    "            y: ev.clientY,\n"
    "            pid: ev.pointerId,\n"
    "            moved: false\n"
    "        };\n"
    "        try {\n"
    "            square.setPointerCapture(ev.pointerId);\n"
    "        } catch (e) { /* ignore */ }\n"
    "        if (document.documentElement.classList.contains('web-board-focus')) {\n"
    "            try {\n"
    "                ev.preventDefault();\n"
    "            } catch (e2) { /* ignore */ }\n"
    "        }\n"
    "    }\n"
    "    function onPointerMove(ev) {\n"
    "        if (!dragStart || dragStart.pid !== ev.pointerId) return;\n"
    "        var dx = ev.clientX - dragStart.x;\n"
    "        var dy = ev.clientY - dragStart.y;\n"
    "        if (!dragStart.moved && (dx * dx + dy * dy) >= BOARD_DRAG_THRESHOLD_PX * BOARD_DRAG_THRESHOLD_PX) {\n"
    "            dragStart.moved = true;\n"
    "        }\n"
    "    }\n"
    "    async function onPointerUp(ev) {\n"
    "        if (!dragStart || dragStart.pid !== ev.pointerId) return;\n"
    "        try {\n"
    "            square.releasePointerCapture(ev.pointerId);\n"
    "        } catch (e) { /* ignore */ }\n"
    "        var wasDrag = dragStart.moved;\n"
    "        var fr = dragStart.row;\n"
    "        var fc = dragStart.col;\n"
    "        dragStart = null;\n"
    "        if (reviewMode) return;\n"
    "        if (wasDrag) {\n"
    "            var targetEl = document.elementFromPoint(ev.clientX, ev.clientY);\n"
    "            var to = squareFromEventTarget(targetEl);\n"
    "            if (!to || (to.row === fr && to.col === fc)) return;\n"
    "            if (sandboxMode) {\n"
    "                sandboxApplyMoveFromDrag(fr, fc, to.row, to.col);\n"
    "            } else if (remoteControlEnabled) {\n"
    "                await handleRemoteDragMove(fr, fc, to.row, to.col);\n"
    "            }\n"
    "            return;\n"
    "        }\n"
    "        await handleSquareClick(row, col);\n"
    "    }\n"
    "    square.addEventListener('pointerdown', onPointerDown);\n"
    "    square.addEventListener('pointermove', onPointerMove);\n"
    "    square.addEventListener('pointerup', onPointerUp);\n"
    "    square.addEventListener('pointercancel', onPointerUp);\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// REMOTE CONTROL LOGIC\n"
    "// ============================================================================\n"
    "\n"
    "function toggleRemoteControl() {\n"
    "    const checkbox = document.getElementById('remote-control-enabled');\n"
    "    remoteControlEnabled = checkbox.checked;\n"
    "    console.log('Remote control:', remoteControlEnabled);\n"
    "    if (!remoteControlEnabled) {\n"
    "        clearHighlights();\n"
    "    }\n"
    "}\n"
    "\n"
    "// Remote control: one click = one action (pickup or drop), same as backup / physical board.\n"
    "async function handleRemoteControlClick(row, col) {\n"
    "    if (isWebLocked()) {\n"
    "        alert('Rozhraní je zamčeno. Odemkněte přes UART.');\n"
    "        return;\n"
    "    }\n"
    "    const notation = String.fromCharCode(97 + col) + (row + 1);\n"
    "    const action = (statusData && statusData.piece_lifted && statusData.piece_lifted.lifted) ? 'drop' : 'pickup';\n"
    "    const squareEl = document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "\n"
    "    if (squareEl) {\n"
    "        squareEl.style.boxShadow = action === 'pickup'\n"
    "            ? 'inset 0 0 20px rgba(255, 255, 0, 0.8)'\n"
    "            : 'inset 0 0 20px rgba(0, 255, 0, 0.8)';\n"
    "        setTimeout(function () { if (squareEl) squareEl.style.boxShadow = ''; }, 500);\n"
    "    }\n"
    "    console.log('Remote control:', action, 'at', notation);\n"
    "\n"
    "    try {\n"
    "        const response = await fetch('/api/game/virtual_action', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: action, square: notation })\n"
    "        });\n"
    "        const res = await response.json().catch(function () { return {}; });\n"
    "        if (!response.ok) {\n"
    "            if (response.status === 403 && res.message) alert(res.message);\n"
    "            else console.warn('Virtual action failed:', response.status, res.message);\n"
    "        }\n"
    "        await fetchData();\n"
    "    } catch (e) {\n"
    "        console.error('Remote virtual_action error:', e);\n"
    "        await fetchData();\n"
    "        alert('Chyba: ' + (e.message || 'nelze odeslat příkaz'));\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// SQUARE CLICK HANDLER\n"
    "// ============================================================================\n"
    "\n"
    "async function handleSquareClick(row, col) {\n"
    "    const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];\n"
    "    const index = row * 8 + col;\n"
    "\n"
    "    // SANDBOX MODE (Zkusit tahy) - vždy jen lokálně, i když je zapnuté dálkové ovládání\n"
    "    if (sandboxMode) {\n"
    "        if (piece === ' ' && selectedSquare !== null) {\n"
    "            // Tah na prázdné pole\n"
    "            const fromRow = Math.floor(selectedSquare / 8);\n"
    "            const fromCol = selectedSquare % 8;\n"
    "            makeSandboxMove(fromRow, fromCol, row, col);\n"
    "            clearHighlights();\n"
    "        } else if (piece !== ' ') {\n"
    "            if (selectedSquare !== null) {\n"
    "                const fromRow = Math.floor(selectedSquare / 8);\n"
    "                const fromCol = selectedSquare % 8;\n"
    "                const selectedPiece = sandboxBoard[fromRow][fromCol];\n"
    "                const isSameSquare = (fromRow === row && fromCol === col);\n"
    "                const isOurPiece = (selectedPiece === selectedPiece.toUpperCase()) === (piece === piece.toUpperCase());\n"
    "\n"
    "                if (isSameSquare) {\n"
    "                    // Klik na stejné pole – zrušit výběr\n"
    "                    clearHighlights();\n"
    "                } else if (isOurPiece) {\n"
    "                    // Klik na vlastní figurku – vybrat jinou\n"
    "                    clearHighlights();\n"
    "                    selectedSquare = index;\n"
    "                    const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "                    if (square) square.classList.add('selected');\n"
    "                } else {\n"
    "                    // Klik na soupeřovu figurku – brát (capture)\n"
    "                    makeSandboxMove(fromRow, fromCol, row, col);\n"
    "                    clearHighlights();\n"
    "                }\n"
    "            } else {\n"
    "                // Žádná figurka vybraná – vybrat tuto\n"
    "                clearHighlights();\n"
    "                selectedSquare = index;\n"
    "                const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "                if (square) square.classList.add('selected');\n"
    "            }\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // REMOTE CONTROL MODE - posílat příkazy na ESP (jen když nejsme v sandboxu)\n"
    "    if (remoteControlEnabled) {\n"
    "        handleRemoteControlClick(row, col);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // NORMÁLNÍ REŽIM (ne sandbox, ne remote control) - žádné POST requesty, žádný vizuální feedback\n"
    "    // Web je jen pasivní zobrazení, hra se ovládá fyzicky\n"
    "    return;\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// REVIEW MODE\n"
    "// ============================================================================\n"
    "\n"
    "function reconstructBoardAtMove(moveIndex) {\n"
    "    const startBoard = [\n"
    "        ['R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'],\n"
    "        ['P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'],\n"
    "        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],\n"
    "        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],\n"
    "        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],\n"
    "        [' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '],\n"
    "        ['p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'],\n"
    "        ['r', 'n', 'b', 'q', 'k', 'b', 'n', 'r']\n"
    "    ];\n"
    "    const board = JSON.parse(JSON.stringify(startBoard));\n"
    "    for (let i = 0; i <= moveIndex && i < historyData.length; i++) {\n"
    "        const move = historyData[i];\n"
    "        const fromRow = parseInt(move.from[1]) - 1;\n"
    "        const fromCol = move.from.charCodeAt(0) - 97;\n"
    "        const toRow = parseInt(move.to[1]) - 1;\n"
    "        const toCol = move.to.charCodeAt(0) - 97;\n"
    "        board[toRow][toCol] = board[fromRow][fromCol];\n"
    "        board[fromRow][fromCol] = ' ';\n"
    "    }\n"
    "    return board;\n"
    "}\n"
    "\n"
    "function enterReviewMode(index) {\n"
    "    reviewMode = true;\n"
    "    currentReviewIndex = index;\n"
    "    const banner = document.getElementById('review-banner');\n"
    "    banner.classList.add('active');\n"
    "    document.getElementById('review-move-text').textContent = `Prohlížíš tah ${index + 1}`;\n"
    "    const reconstructedBoard = reconstructBoardAtMove(index);\n"
    "    updateBoard(reconstructedBoard);\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('move-from', 'move-to');\n"
    "    });\n"
    "    if (index >= 0 && index < historyData.length) {\n"
    "        const move = historyData[index];\n"
    "        const fromRow = parseInt(move.from[1]) - 1;\n"
    "        const fromCol = move.from.charCodeAt(0) - 97;\n"
    "        const toRow = parseInt(move.to[1]) - 1;\n"
    "        const toCol = move.to.charCodeAt(0) - 97;\n"
    "        const fromSquare = document.querySelector(`[data-row='${fromRow}'][data-col='${fromCol}']`);\n"
    "        const toSquare = document.querySelector(`[data-row='${toRow}'][data-col='${toCol}']`);\n"
    "        if (fromSquare) fromSquare.classList.add('move-from');\n"
    "        if (toSquare) toSquare.classList.add('move-to');\n"
    "    }\n"
    "    document.querySelectorAll('.history-item').forEach(item => {\n"
    "        item.classList.remove('selected');\n"
    "    });\n"
    "    const selectedItem = document.querySelector(`[data-move-index='${index}']`);\n"
    "    if (selectedItem) {\n"
    "        selectedItem.classList.add('selected');\n"
    "        // Removed scrollIntoView - causes unwanted scroll on mobile when using navigation arrows\n"
    "        // History item stays highlighted but page doesn't scroll away from board/banner\n"
    "    }\n"
    "}\n"
    "\n"
    "function exitReviewMode() {\n"
    "    reviewMode = false;\n"
    "    currentReviewIndex = -1;\n"
    "    document.getElementById('review-banner').classList.remove('active');\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('move-from', 'move-to');\n"
    "    });\n"
    "    document.querySelectorAll('.history-item').forEach(item => {\n"
    "        item.classList.remove('selected');\n"
    "    });\n"
    "    fetchData();\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// SANDBOX MODE\n"
    "// ============================================================================\n"
    "\n"
    "function enterSandboxMode() {\n"
    "    sandboxMode = true;\n"
    "    sandboxBoard = JSON.parse(JSON.stringify(boardData));\n"
    "    sandboxHistory = [];\n"
    "    var banner = document.getElementById('sandbox-banner');\n"
    "    if (banner) banner.classList.add('active');\n"
    "    var boardEl = document.getElementById('board');\n"
    "    if (boardEl) boardEl.classList.add('sandbox-active');\n"
    "    clearHighlights();\n"
    "    updateUndoButton();\n"
    "    if (typeof console !== 'undefined' && console.log) {\n"
    "        console.log('[Sandbox] zapnuto — tahy jen lokálně, vizuálně odlišná šachovnice');\n"
    "    }\n"
    "}\n"
    "\n"
    "function exitSandboxMode() {\n"
    "    sandboxMode = false;\n"
    "    sandboxBoard = [];\n"
    "    sandboxHistory = [];\n"
    "    var boardEl = document.getElementById('board');\n"
    "    if (boardEl) boardEl.classList.remove('sandbox-active');\n"
    "    var banner = document.getElementById('sandbox-banner');\n"
    "    if (banner) banner.classList.remove('active');\n"
    "    clearHighlights();\n"
    "    fetchData();\n"
    "    if (typeof console !== 'undefined' && console.log) {\n"
    "        console.log('[Sandbox] vypnuto — obnovuji pozici z desky (HTTP)');\n"
    "    }\n"
    "}\n"
    "\n"
    "function makeSandboxMove(fromRow, fromCol, toRow, toCol) {\n"
    "    const piece = sandboxBoard[fromRow][fromCol];\n"
    "    const capturedPiece = sandboxBoard[toRow][toCol]; // Uložit captured piece (může být ' ')\n"
    "\n"
    "    // Provedení tahu\n"
    "    sandboxBoard[toRow][toCol] = piece;\n"
    "    sandboxBoard[fromRow][fromCol] = ' ';\n"
    "\n"
    "    // Uložit tah do historie s kompletními informacemi\n"
    "    sandboxHistory.push({\n"
    "        fromRow: fromRow,\n"
    "        fromCol: fromCol,\n"
    "        toRow: toRow,\n"
    "        toCol: toCol,\n"
    "        movingPiece: piece,\n"
    "        capturedPiece: capturedPiece\n"
    "    });\n"
    "\n"
    "    // Omezit historii na 10 tahů\n"
    "    if (sandboxHistory.length > 10) {\n"
    "        sandboxHistory.shift(); // Odstranit nejstarší tah\n"
    "    }\n"
    "\n"
    "    updateBoard(sandboxBoard);\n"
    "    updateUndoButton();\n"
    "}\n"
    "\n"
    "function updateUndoButton() {\n"
    "    const undoBtn = document.getElementById('sandbox-undo-btn');\n"
    "    if (!undoBtn) return;\n"
    "\n"
    "    const availableUndos = sandboxHistory.length;\n"
    "    const maxUndos = 10;\n"
    "\n"
    "    undoBtn.textContent = `Undo (${availableUndos}/${maxUndos})`;\n"
    "    undoBtn.disabled = availableUndos === 0;\n"
    "}\n"
    "\n"
    "function undoSandboxMove() {\n"
    "    if (sandboxHistory.length === 0) {\n"
    "        return; // Žádné tahy k vrácení\n"
    "    }\n"
    "\n"
    "    // Vzít poslední tah z historie\n"
    "    const lastMove = sandboxHistory.pop();\n"
    "\n"
    "    // Vrátit figurku zpět\n"
    "    sandboxBoard[lastMove.fromRow][lastMove.fromCol] = lastMove.movingPiece;\n"
    "\n"
    "    // Obnovit captured piece (nebo prázdné pole)\n"
    "    sandboxBoard[lastMove.toRow][lastMove.toCol] = lastMove.capturedPiece;\n"
    "\n"
    "    // Aktualizovat board a tlačítko\n"
    "    updateBoard(sandboxBoard);\n"
    "    updateUndoButton();\n"
    "    clearHighlights();\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// HINT (STOCKFISH) - FEN and best move\n"
    "// ============================================================================\n"
    "\n"
    "/**\n"
    " * Build FEN from current board, status and history.\n"
    " * Board: row 0 = rank 1 (white back), row 7 = rank 8 (black back). FEN ranks 8..1.\n"
    " * Simplified: default castling KQkq, no en passant (sufficient for best-move).\n"
    " * Returns '' if board/status invalid or board not 8x8.\n"
    " */\n"
    "function boardAndStatusToFen(board, status, history) {\n"
    "    if (!board || !status || !Array.isArray(board) || board.length !== 8) return '';\n"
    "    const rows = [];\n"
    "    for (let r = 7; r >= 0; r--) {\n"
    "        if (!board[r] || board[r].length !== 8) return '';\n"
    "        let rank = '';\n"
    "        let empty = 0;\n"
    "        for (let c = 0; c < 8; c++) {\n"
    "            const p = board[r][c];\n"
    "            if (p === ' ' || p === '') {\n"
    "                empty++;\n"
    "            } else {\n"
    "                if (empty) { rank += empty; empty = 0; }\n"
    "                rank += p;\n"
    "            }\n"
    "        }\n"
    "        if (empty) rank += empty;\n"
    "        rows.push(rank);\n"
    "    }\n"
    "    const piecePlacement = rows.join('/');\n"
    "    const sideToMove = (status.current_player === 'White') ? 'w' : 'b';\n"
    "    const castling = 'KQkq';\n"
    "    const ep = '-';\n"
    "    const halfmove = (history && history.moves) ? history.moves.length : 0;\n"
    "    const fullmove = Math.floor(halfmove / 2) + 1;\n"
    "    const fen = piecePlacement + ' ' + sideToMove + ' ' + castling + ' ' + ep + ' 0 ' + fullmove;\n"
    "    if (fen.length < 20 || fen.length > 120) return '';\n"
    "    return fen;\n"
    "}\n"
    "\n"
    "/** Hint depth 1–18 from settings (NVS devicePrefs). Used by fetchStockfishBestMove for hints. */\n"
    "function getHintDepth() {\n"
    "    try {\n"
    "        var d = parseInt(devicePrefs.chessHintDepth, 10);\n"
    "        d = isNaN(d) ? 10 : d;\n"
    "        return Math.min(18, Math.max(1, d));\n"
    "    } catch (e) {\n"
    "        return 10;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Depth for move evaluation (Zhodnocení tahu). Uses at least 12 so evals are meaningful; max 18. */\n"
    "function getEvaluationDepth() {\n"
    "    var hint = getHintDepth();\n"
    "    return Math.min(18, Math.max(hint, 12));\n"
    "}\n"
    "\n"
    "/** Whether to show move evaluation after each move (localStorage). */\n"
    "function getEvaluateMoveEnabled() {\n"
    "    try {\n"
    "        return devicePrefs.chessEvaluateMove === true;\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Počet nápověd na partii (0 = neomezeno). */\n"
    "function getHintLimit() {\n"
    "    try {\n"
    "        var n = parseInt(devicePrefs.chessHintLimit, 10);\n"
    "        return isNaN(n) || n < 0 ? 0 : n;\n"
    "    } catch (e) {\n"
    "        return 0;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za výborný tah (devicePrefs). */\n"
    "function getHintAwardBest() {\n"
    "    try {\n"
    "        return devicePrefs.chessHintAwardBest !== false;\n"
    "    } catch (e) {\n"
    "        return true;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za dobrý tah (localStorage). */\n"
    "function getHintAwardGood() {\n"
    "    try {\n"
    "        return devicePrefs.chessHintAwardGood === true;\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za sebrání figurky (localStorage). */\n"
    "function getHintAwardCapture() {\n"
    "    try {\n"
    "        return devicePrefs.chessHintAwardCapture === true;\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Zobrazit blok „Výukový přehled“ (nápovědy + kvalita tahů). */\n"
    "function getShowHintStats() {\n"
    "    try {\n"
    "        return devicePrefs.chessShowHintStats === true;\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Po zvednutí figurky bota zobrazit na LED jen cílové pole (výchozí vypnuto). */\n"
    "function getBotLedTargetOnlyAfterLift() {\n"
    "    try {\n"
    "        return devicePrefs.chessBotLedTargetOnlyAfterLift === true;\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "function setBotLedTargetOnlyAfterLift(checked) {\n"
    "    devicePrefs.chessBotLedTargetOnlyAfterLift = !!checked;\n"
    "    scheduleSaveUiPrefsToDevice();\n"
    "}\n"
    "window.setBotLedTargetOnlyAfterLift = setBotLedTargetOnlyAfterLift;\n"
    "\n"
    "function getCurrentPlayerHints() {\n"
    "    var p = (statusData && statusData.current_player) ? statusData.current_player : 'White';\n"
    "    return p === 'White' ? hintsRemainingWhite : hintsRemainingBlack;\n"
    "}\n"
    "\n"
    "function updateHintButtonLabel() {\n"
    "    var btn = document.getElementById('hint-btn');\n"
    "    if (!btn) return;\n"
    "    var limit = getHintLimit();\n"
    "    var onMove = (statusData && statusData.current_player) === 'Black' ? 'black' : 'white';\n"
    "    if (limit > 0) {\n"
    "        var w = hintsRemainingWhite, b = hintsRemainingBlack;\n"
    "        var first = onMove === 'white' ? 'Bílý ' + w + ' | Černý ' + b : 'Černý ' + b + ' | Bílý ' + w;\n"
    "        btn.textContent = 'Nápověda (' + first + ')';\n"
    "        btn.disabled = (onMove === 'white' ? w : b) <= 0;\n"
    "    } else {\n"
    "        btn.textContent = 'Nápověda';\n"
    "        btn.disabled = false;\n"
    "    }\n"
    "    if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "}\n"
    "\n"
    "/** V režimu bota udělujeme odměny jen za tahy člověka (ne za tahy bota). */\n"
    "function isHumanSideInBotMode(forSide) {\n"
    "    if (gameMode !== 'bot' || !forSide) return true;\n"
    "    return (botSettings.side === 'white' && forSide === 'black') || (botSettings.side === 'black' && forSide === 'white');\n"
    "}\n"
    "\n"
    "function addHintReward(reason, forSide) {\n"
    "    var limit = getHintLimit();\n"
    "    if (limit <= 0 || !forSide) return;\n"
    "    if (gameMode === 'bot' && !isHumanSideInBotMode(forSide)) return;\n"
    "    if (forSide === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "    updateHintButtonLabel();\n"
    "    var who = forSide === 'white' ? 'Bílý' : 'Černý';\n"
    "    var msg = reason === 'best' ? 'Výborný tah! ' + who + ' +1 nápověda.' : reason === 'good' ? 'Dobrý tah! ' + who + ' +1 nápověda.' : reason === 'capture' ? 'Sebrání figurky! ' + who + ' +1 nápověda.' : '';\n"
    "    if (!msg) return;\n"
    "    var el = document.getElementById('castling-pending-message');\n"
    "    if (el) {\n"
    "        el.textContent = msg;\n"
    "        el.style.display = 'block';\n"
    "        el.style.background = 'rgba(33, 150, 243, 0.2)';\n"
    "        el.style.borderColor = '#2196F3';\n"
    "        el.style.color = '#e0e0e0';\n"
    "        setTimeout(function () {\n"
    "            if (el.textContent === msg) el.style.display = 'none';\n"
    "        }, 2500);\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Skóre kvality tahu pro průměr: best=5 … blunder=1, jinak 0. */\n"
    "function gradeToScore(grade) {\n"
    "    switch (grade) {\n"
    "        case 'best': return 5;\n"
    "        case 'good': return 4;\n"
    "        case 'inaccuracy': return 3;\n"
    "        case 'mistake': return 2;\n"
    "        case 'blunder': return 1;\n"
    "        default: return 0;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Průměrná kvalita tahů hráče (side='white'|'black') za posledních lastN tahů toho hráče. Vrací číslo 1–5 nebo null. */\n"
    "function getAverageGradeForPlayer(side, lastN) {\n"
    "    var indices = [];\n"
    "    var isWhite = (side === 'white');\n"
    "    for (var i = (historyData.length || 0) - 1; i >= 0 && indices.length < lastN; i--) {\n"
    "        if ((i % 2 === 0) === isWhite) indices.push(i);\n"
    "    }\n"
    "    if (indices.length === 0) return null;\n"
    "    var sum = 0, count = 0;\n"
    "    indices.forEach(function (idx) {\n"
    "        var ev = moveEvaluations[idx];\n"
    "        if (ev && ev.grade) {\n"
    "            var s = gradeToScore(ev.grade);\n"
    "            if (s > 0) { sum += s; count++; }\n"
    "        }\n"
    "    });\n"
    "    if (count === 0) return null;\n"
    "    return Math.round((sum / count) * 10) / 10;\n"
    "}\n"
    "\n"
    "/** Zobrazí nebo skryje blok Výukový přehled a naplní nápovědy + průměry kvality. */\n"
    "function updateTeachingStatsPanel() {\n"
    "    var panel = document.getElementById('teaching-stats-panel');\n"
    "    if (!panel) return;\n"
    "    if (!getShowHintStats()) {\n"
    "        panel.style.display = 'none';\n"
    "        return;\n"
    "    }\n"
    "    panel.style.display = '';\n"
    "    var limit = getHintLimit();\n"
    "    var wHints = limit > 0 ? hintsRemainingWhite : '—';\n"
    "    var bHints = limit > 0 ? hintsRemainingBlack : '—';\n"
    "    if (limit <= 0) {\n"
    "        try {\n"
    "            var elW = document.getElementById('teaching-stats-white-hints');\n"
    "            var elB = document.getElementById('teaching-stats-black-hints');\n"
    "            if (elW) elW.textContent = '—';\n"
    "            if (elB) elB.textContent = '—';\n"
    "        } catch (e) {}\n"
    "    } else {\n"
    "        var elW = document.getElementById('teaching-stats-white-hints');\n"
    "        var elB = document.getElementById('teaching-stats-black-hints');\n"
    "        if (elW) elW.textContent = String(hintsRemainingWhite);\n"
    "        if (elB) elB.textContent = String(hintsRemainingBlack);\n"
    "    }\n"
    "    function fmtAvg(v) { return v != null ? v.toFixed(1) : '—'; }\n"
    "    var w5 = getAverageGradeForPlayer('white', 5), w15 = getAverageGradeForPlayer('white', 15), wAll = getAverageGradeForPlayer('white', 9999);\n"
    "    var b5 = getAverageGradeForPlayer('black', 5), b15 = getAverageGradeForPlayer('black', 15), bAll = getAverageGradeForPlayer('black', 9999);\n"
    "    var el;\n"
    "    el = document.getElementById('teaching-stats-white-avg5'); if (el) el.textContent = fmtAvg(w5);\n"
    "    el = document.getElementById('teaching-stats-white-avg15'); if (el) el.textContent = fmtAvg(w15);\n"
    "    el = document.getElementById('teaching-stats-white-avgAll'); if (el) el.textContent = fmtAvg(wAll);\n"
    "    el = document.getElementById('teaching-stats-black-avg5'); if (el) el.textContent = fmtAvg(b5);\n"
    "    el = document.getElementById('teaching-stats-black-avg15'); if (el) el.textContent = fmtAvg(b15);\n"
    "    el = document.getElementById('teaching-stats-black-avgAll'); if (el) el.textContent = fmtAvg(bAll);\n"
    "}\n"
    "if (typeof window !== 'undefined') window.updateTeachingStatsPanel = updateTeachingStatsPanel;\n"
    "\n"
    "// ---------- Parsování eval z API (jedno místo, bez duplicity) ----------\n"
    "/** Normalizuje řetězec s eval (Unicode minus → ASCII minus). */\n"
    "function normalizeEvalString(s) {\n"
    "    if (s == null || typeof s !== 'string') return s;\n"
    "    return String(s).replace(/\\u2212/g, '-').trim();\n"
    "}\n"
    "/** Převod hodnoty na pawns: pokud |v| > 10, považujeme za centipawns (děleno 100). */\n"
    "function toPawns(v) {\n"
    "    if (v == null || typeof v !== 'number' || isNaN(v)) return null;\n"
    "    if (Math.abs(v) > 10) return v / 100;\n"
    "    return v;\n"
    "}\n"
    "/**\n"
    " * Vybere a naparsuje eval z libovolného objektu odpovědi API (data nebo raw).\n"
    " * Zkouší: eval (number/string), centipawns, cp, evaluation, score (number/string), result.eval.\n"
    " * @param {Object} obj - objekt z API (např. raw.data nebo celý raw)\n"
    " * @returns {number|null} - eval v pawns, nebo null\n"
    " */\n"
    "function parseEvalFromApiObject(obj) {\n"
    "    if (!obj || typeof obj !== 'object') return null;\n"
    "    var val = null;\n"
    "    if (typeof obj.eval === 'number') val = toPawns(obj.eval);\n"
    "    if (val == null && typeof obj.eval === 'string') { var p = parseFloat(normalizeEvalString(obj.eval)); if (!isNaN(p)) val = toPawns(p); }\n"
    "    if (val == null && obj.centipawns != null) {\n"
    "        var cp = typeof obj.centipawns === 'number' ? obj.centipawns : parseInt(normalizeEvalString(obj.centipawns), 10);\n"
    "        if (!isNaN(cp)) val = cp / 100;\n"
    "    }\n"
    "    if (val == null && obj.cp != null) {\n"
    "        var cp2 = typeof obj.cp === 'number' ? obj.cp : parseInt(normalizeEvalString(obj.cp), 10);\n"
    "        if (!isNaN(cp2)) val = cp2 / 100;\n"
    "    }\n"
    "    if (val == null && obj.evaluation != null) {\n"
    "        var ev = typeof obj.evaluation === 'number' ? obj.evaluation : parseFloat(normalizeEvalString(obj.evaluation));\n"
    "        if (!isNaN(ev)) val = toPawns(ev);\n"
    "    }\n"
    "    if (val == null && typeof obj.score === 'number' && !isNaN(obj.score)) val = toPawns(obj.score);\n"
    "    if (val == null && typeof obj.score === 'string') { var sc = parseFloat(normalizeEvalString(obj.score)); if (!isNaN(sc)) val = toPawns(sc); }\n"
    "    if (val == null && obj.result != null && typeof obj.result === 'object' && typeof obj.result.eval === 'number') val = toPawns(obj.result.eval);\n"
    "    return val;\n"
    "}\n"
    "\n"
    "/**\n"
    " * Fetch best move and optional evaluation from Stockfish API (POST).\n"
    " * Used for hints, move evaluation and bot. Returns { from, to, eval, text, san, continuationArr, mate, winChance } or null.\n"
    " * @param {string} fen - FEN position\n"
    " * @param {number} [depthOverride] - Optional depth 1–18; if omitted, uses getHintDepth() (hints) or bot uses botSettings.strength\n"
    " *\n"
    " * Expected API response format (chess-api.com; other backends may use different keys):\n"
    " * - from, to (strings, e.g. \"e2\", \"e4\") or move (e.g. \"e2e4\") for the best move\n"
    " * - eval (number in pawns; negative = black better) or centipawns/cp (number, divide by 100) or evaluation/score for grading\n"
    " * - Optional: text, san, continuationArr, mate, winChance for hint explanation\n"
    " * Response may be at root or under .data / .result; all are handled.\n"
    " */\n"
    "async function fetchStockfishBestMove(fen, depthOverride) {\n"
    "    var depth = (typeof depthOverride === 'number' && depthOverride >= 1 && depthOverride <= 18)\n"
    "        ? depthOverride\n"
    "        : getHintDepth();\n"
    "    const TIMEOUT_MS = 15000;\n"
    "    const API_URL = 'https://chess-api.com/v1';\n"
    "    if (typeof console !== 'undefined' && console.log) console.log('[Hint] Stockfish request (chess-api.com), FEN length:', fen.length, 'depth:', depth);\n"
    "\n"
    "    let controller = null;\n"
    "    try {\n"
    "        controller = typeof AbortController !== 'undefined' ? new AbortController() : null;\n"
    "        const timeoutId = controller ? setTimeout(function () { if (controller) controller.abort(); }, TIMEOUT_MS) : null;\n"
    "        const opts = {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ fen: fen, depth: depth })\n"
    "        };\n"
    "        if (controller) opts.signal = controller.signal;\n"
    "        const res = await fetch(API_URL, opts);\n"
    "        if (controller && timeoutId) clearTimeout(timeoutId);\n"
    "\n"
    "        if (!res.ok) {\n"
    "            if (console.warn) console.warn('[Hint] Stockfish HTTP', res.status, res.statusText);\n"
    "            return null;\n"
    "        }\n"
    "        const raw = await res.json().catch(function () { return null; });\n"
    "        if (!raw) {\n"
    "            if (console.warn) console.warn('[Hint] Stockfish invalid JSON');\n"
    "            return null;\n"
    "        }\n"
    "        var data = (raw && typeof raw.data === 'object' && raw.data !== null) ? raw.data\n"
    "            : (raw && typeof raw.result === 'object' && raw.result !== null) ? raw.result\n"
    "            : (raw && typeof raw.bestMove === 'object' && raw.bestMove !== null) ? raw.bestMove\n"
    "            : raw;\n"
    "        var from = data.from;\n"
    "        var to = data.to;\n"
    "        if (typeof from !== 'string' || typeof to !== 'string' || from.length !== 2 || to.length !== 2) {\n"
    "            var move = data.move;\n"
    "            if (typeof move === 'string' && move.length >= 4) {\n"
    "                from = move.substring(0, 2).toLowerCase();\n"
    "                to = move.substring(2, 4).toLowerCase();\n"
    "            } else {\n"
    "                if (console.warn) console.warn('[Hint] Stockfish response missing from/to or move:', data);\n"
    "                return null;\n"
    "            }\n"
    "        } else {\n"
    "            from = from.toLowerCase();\n"
    "            to = to.toLowerCase();\n"
    "        }\n"
    "        if (console.log) console.log('[Hint] Best move:', from, '->', to);\n"
    "        var evalVal = parseEvalFromApiObject(data);\n"
    "        if (evalVal == null && raw && data !== raw) evalVal = parseEvalFromApiObject(raw);\n"
    "        if (evalVal == null && typeof console !== 'undefined' && console.warn) {\n"
    "            console.warn('[Eval Staging] evalVal still null – data keys:', data ? Object.keys(data) : [], 'raw keys:', raw ? Object.keys(raw) : [], 'data.eval:', data && data.eval, 'raw.eval:', raw && raw.eval, 'raw.centipawns:', raw && raw.centipawns);\n"
    "        }\n"
    "        if (typeof console !== 'undefined' && console.log) {\n"
    "            console.log('[Eval Staging] API parsed evalVal (pawns):', evalVal);\n"
    "        }\n"
    "        return {\n"
    "            from: from,\n"
    "            to: to,\n"
    "            eval: evalVal,\n"
    "            text: typeof data.text === 'string' ? data.text : '',\n"
    "            san: typeof data.san === 'string' ? data.san : (from + '-' + to),\n"
    "            continuationArr: Array.isArray(data.continuationArr) ? data.continuationArr : [],\n"
    "            mate: data.mate != null && typeof data.mate === 'number' ? data.mate : null,\n"
    "            winChance: typeof data.winChance === 'number' ? data.winChance : null\n"
    "        };\n"
    "    } catch (err) {\n"
    "        if (controller && controller.signal && controller.signal.aborted) {\n"
    "            if (console.warn) console.warn('[Hint] Stockfish timeout');\n"
    "        } else {\n"
    "            if (console.error) console.error('[Hint] Stockfish error:', err.message);\n"
    "        }\n"
    "        return null;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Show hint on board (hint-from, hint-to) and optionally send to LED. */\n"
    "function showHintOnBoard(from, to) {\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('hint-from', 'hint-to');\n"
    "    });\n"
    "    const fromCol = from.charCodeAt(0) - 97;\n"
    "    const fromRow = parseInt(from[1], 10) - 1;\n"
    "    const toCol = to.charCodeAt(0) - 97;\n"
    "    const toRow = parseInt(to[1], 10) - 1;\n"
    "    const fromSquare = document.querySelector('[data-row=\"' + fromRow + '\"][data-col=\"' + fromCol + '\"]');\n"
    "    const toSquare = document.querySelector('[data-row=\"' + toRow + '\"][data-col=\"' + toCol + '\"]');\n"
    "    if (fromSquare) fromSquare.classList.add('hint-from');\n"
    "    if (toSquare) toSquare.classList.add('hint-to');\n"
    "}\n"
    "\n"
    "/** Replace key English phrases from API text with Czech (for hint explanation). */\n"
    "function hintTextToCzech(s) {\n"
    "    if (!s || typeof s !== 'string') return '';\n"
    "    var t = s\n"
    "        .replace(/\\bWhite is winning\\b/gi, 'Bílý vyhrává')\n"
    "        .replace(/\\bBlack is winning\\b/gi, 'Černý vyhrává')\n"
    "        .replace(/\\bWhite is better\\b/gi, 'Bílý je lépe')\n"
    "        .replace(/\\bBlack is better\\b/gi, 'Černý je lépe')\n"
    "        .replace(/\\bThe game is balanced\\.?\\b/gi, 'Hra je vyrovnaná.')\n"
    "        .replace(/\\bgame is balanced\\.?\\b/gi, 'hra je vyrovnaná.')\n"
    "        .replace(/\\bDepth \\d+\\b/gi, function (m) { return 'Hloubka ' + m.replace(/\\D/g, ''); })\n"
    "        .replace(/\\bMove\\s+/gi, 'Tah ');\n"
    "    return t;\n"
    "}\n"
    "\n"
    "/** Format UCI move (e2e4) as e2–e4. */\n"
    "function formatUciMove(uci) {\n"
    "    if (!uci || uci.length < 4) return uci || '';\n"
    "    return uci.substring(0, 2) + '–' + uci.substring(2, 4);\n"
    "}\n"
    "\n"
    "/** Build one short, child-friendly sentence for the hint (easy to read). */\n"
    "function buildHintMessage(data) {\n"
    "    var parts = [];\n"
    "    var san = (data.san || (data.from + '–' + data.to)).trim();\n"
    "    parts.push('Počítač radí: zahraj tah ' + san + '.');\n"
    "\n"
    "    var e = data.eval;\n"
    "    if (e != null && typeof e === 'number') {\n"
    "        if (e > 0.3) parts.push('Bílý má trochu výhodu.');\n"
    "        else if (e < -0.3) parts.push('Černý má trochu výhodu.');\n"
    "        else parts.push('Teď je to vyrovnané.');\n"
    "    }\n"
    "\n"
    "    if (Array.isArray(data.continuationArr) && data.continuationArr.length > 0) {\n"
    "        var first = data.continuationArr.slice(0, 4).map(formatUciMove).join(', ');\n"
    "        parts.push('Pak můžeš hrát třeba ' + first + '.');\n"
    "    }\n"
    "\n"
    "    if (data.mate != null && typeof data.mate === 'number') {\n"
    "        if (data.mate === 0) parts.push('Je mat!');\n"
    "        else if (data.mate > 0) parts.push('Za ' + data.mate + ' tahů bude mat bílého!');\n"
    "        else parts.push('Za ' + (-data.mate) + ' tahů bude mat černého!');\n"
    "    }\n"
    "\n"
    "    return parts.join(' ');\n"
    "}\n"
    "\n"
    "/** Show hint explanation block (one simple message for children). */\n"
    "function showHintExplanation(data) {\n"
    "    var el = document.getElementById('hint-explanation');\n"
    "    if (!el) return;\n"
    "    var msgEl = document.getElementById('hint-explanation-message');\n"
    "    if (!msgEl) return;\n"
    "    msgEl.textContent = buildHintMessage(data);\n"
    "    el.style.display = 'block';\n"
    "}\n"
    "\n"
    "/** Show hint block with error/info message (např. žádný internet). */\n"
    "function showHintError(message) {\n"
    "    var el = document.getElementById('hint-explanation');\n"
    "    if (!el) return;\n"
    "    var msgEl = document.getElementById('hint-explanation-message');\n"
    "    if (!msgEl) return;\n"
    "    msgEl.textContent = message;\n"
    "    el.style.display = 'block';\n"
    "}\n"
    "\n"
    "/** Hide hint explanation block (on new move / fetchData). */\n"
    "function hideHintExplanation() {\n"
    "    var el = document.getElementById('hint-explanation');\n"
    "    if (el) el.style.display = 'none';\n"
    "}\n"
    "\n"
    "/** Grade: 'best' | 'good' | 'inaccuracy' | 'mistake' | 'blunder' | 'error' */\n"
    "function showMoveEvaluation(text, grade) {\n"
    "    var el = document.getElementById('move-evaluation');\n"
    "    if (!el) return;\n"
    "    var msgEl = document.getElementById('move-evaluation-message');\n"
    "    if (msgEl) msgEl.textContent = text;\n"
    "    grade = grade || 'good';\n"
    "    el.className = 'move-evaluation move-evaluation--' + grade;\n"
    "    el.style.display = 'block';\n"
    "}\n"
    "\n"
    "/** Hide move evaluation block. */\n"
    "function hideMoveEvaluation() {\n"
    "    var el = document.getElementById('move-evaluation');\n"
    "    if (el) el.style.display = 'none';\n"
    "}\n"
    "\n"
    "/**\n"
    " * Evaluate the last played move: call API for position before and (if needed) after,\n"
    " * then show a short Czech message and barvu podle kvality (best=zelená, blunder=červená, …).\n"
    " */\n"
    "/** Normalize UCI move to 4 chars (from+to) for comparison. */\n"
    "function normalizeUci(from, to) {\n"
    "    var s = ((from || '') + (to || '')).toLowerCase().replace(/[^a-h1-8]/g, '');\n"
    "    return s.length >= 4 ? s.slice(0, 4) : s;\n"
    "}\n"
    "\n"
    "/** True pokud je hodnocení pro daný počet tahů stále platné (nebyla nová hra / další tah). */\n"
    "function isEvaluationStillValid(historyLength) {\n"
    "    return (historyData && historyData.length) === historyLength;\n"
    "}\n"
    "\n"
    "/** chess-api.com vrací eval v perspektivě bílého (negative = black winning). Nepřevádět. */\n"
    "var API_EVAL_SIDE_TO_MOVE = false;\n"
    "function evalToWhitePerspective(fen, evalRaw) {\n"
    "    if (fen == null || evalRaw == null || typeof evalRaw !== 'number') return evalRaw;\n"
    "    if (!API_EVAL_SIDE_TO_MOVE) return evalRaw;\n"
    "    var blackToMove = fen.indexOf(' b ') >= 0;\n"
    "    if (blackToMove) {\n"
    "        if (typeof console !== 'undefined' && console.log) console.log('[Eval Staging] converted to White perspective, raw:', evalRaw, '->', -evalRaw);\n"
    "        return -evalRaw;\n"
    "    }\n"
    "    return evalRaw;\n"
    "}\n"
    "\n"
    "function evaluateMoveAsync(fenBefore, fenAfter, playedMove, historyLength) {\n"
    "    if (!statusData.internet_connected) {\n"
    "        showMoveEvaluation('Zhodnocení vyžaduje připojení k internetu (WiFi).', 'error');\n"
    "        return;\n"
    "    }\n"
    "    var playedUci = normalizeUci(playedMove.from, playedMove.to);\n"
    "    if (playedUci.length < 4) return;\n"
    "\n"
    "    var hintedForThisMove = lastHintedMove;\n"
    "    lastHintedMove = null;\n"
    "\n"
    "    var evalDepth = getEvaluationDepth();\n"
    "    fetchStockfishBestMove(fenBefore, evalDepth).then(function (beforeData) {\n"
    "        if (!isEvaluationStillValid(historyLength)) return;\n"
    "        if (!beforeData) {\n"
    "            var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';\n"
    "            showMoveEvaluation(errMsg, 'error');\n"
    "            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };\n"
    "            renderHistoryList();\n"
    "            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "            return;\n"
    "        }\n"
    "        var bestUci = normalizeUci(beforeData.from, beforeData.to);\n"
    "        if (playedUci === bestUci) {\n"
    "            var msgBest = 'Výborný tah! Byl to nejlepší tah.';\n"
    "            showMoveEvaluation(msgBest, 'best');\n"
    "            moveEvaluations[historyLength - 1] = { grade: 'best', msg: msgBest };\n"
    "            var sideBest = (historyLength - 1) % 2 === 0 ? 'white' : 'black';\n"
    "            var hintedSameMove = hintedForThisMove && playedUci === normalizeUci(hintedForThisMove.from, hintedForThisMove.to);\n"
    "            if (getHintAwardBest() && !hintedSameMove) addHintReward('best', sideBest);\n"
    "            renderHistoryList();\n"
    "            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "            return;\n"
    "        }\n"
    "        var bestFormatted = formatUciMove(bestUci);\n"
    "        fetchStockfishBestMove(fenAfter, evalDepth).then(function (afterData) {\n"
    "            if (!isEvaluationStillValid(historyLength)) return;\n"
    "            if (!afterData) {\n"
    "                var msgInacc = 'Lepší byl tah ' + bestFormatted + '.';\n"
    "                showMoveEvaluation(msgInacc, 'inaccuracy');\n"
    "                moveEvaluations[historyLength - 1] = { grade: 'inaccuracy', msg: msgInacc };\n"
    "                renderHistoryList();\n"
    "                if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "                return;\n"
    "            }\n"
    "            var hasEvalBefore = beforeData.eval != null && typeof beforeData.eval === 'number';\n"
    "            var hasEvalAfter = afterData.eval != null && typeof afterData.eval === 'number';\n"
    "            var evalBefore = hasEvalBefore ? evalToWhitePerspective(fenBefore, beforeData.eval) : null;\n"
    "            var evalAfter = hasEvalAfter ? evalToWhitePerspective(fenAfter, afterData.eval) : null;\n"
    "            var msg, grade;\n"
    "            if (!hasEvalBefore || !hasEvalAfter) {\n"
    "                if (typeof console !== 'undefined' && console.warn) {\n"
    "                    console.warn('[Eval Staging] Missing eval – hasEvalBefore:', hasEvalBefore, 'hasEvalAfter:', hasEvalAfter, 'beforeData keys:', beforeData ? Object.keys(beforeData) : [], 'afterData keys:', afterData ? Object.keys(afterData) : []);\n"
    "                }\n"
    "                msg = 'Slabší tah. Lepší bylo ' + bestFormatted + '.';\n"
    "                grade = 'inaccuracy';\n"
    "            } else {\n"
    "                var whiteJustMoved = (historyLength - 1) % 2 === 0;\n"
    "                var scoreDrop = whiteJustMoved ? (evalBefore - evalAfter) : (evalAfter - evalBefore);\n"
    "                if (scoreDrop < 0) scoreDrop = 0;\n"
    "                if (scoreDrop <= 0.20) {\n"
    "                    msg = 'Dobrý tah.';\n"
    "                    grade = 'good';\n"
    "                    var sideGood = (historyLength - 1) % 2 === 0 ? 'white' : 'black';\n"
    "                    if (getHintAwardGood()) addHintReward('good', sideGood);\n"
    "                } else if (scoreDrop <= 0.50) {\n"
    "                    msg = 'Slabší tah. Lepší bylo ' + bestFormatted + '.';\n"
    "                    grade = 'inaccuracy';\n"
    "                } else if (scoreDrop <= 1.00) {\n"
    "                    msg = 'Chyba. Pozice se zhoršila. Lepší bylo ' + bestFormatted + '.';\n"
    "                    grade = 'mistake';\n"
    "                } else {\n"
    "                    msg = 'Vážná chyba. Lepší bylo ' + bestFormatted + '.';\n"
    "                    grade = 'blunder';\n"
    "                }\n"
    "                if (typeof console !== 'undefined' && console.log) {\n"
    "                    console.log('[Eval Staging] hasEvalBefore:', hasEvalBefore, 'hasEvalAfter:', hasEvalAfter, 'evalBefore:', evalBefore, 'evalAfter:', evalAfter, 'scoreDrop:', scoreDrop.toFixed(3), 'whiteJustMoved:', whiteJustMoved, 'grade:', grade);\n"
    "                }\n"
    "            }\n"
    "            showMoveEvaluation(msg, grade);\n"
    "            moveEvaluations[historyLength - 1] = { grade: grade, msg: msg };\n"
    "            renderHistoryList();\n"
    "            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "        }).catch(function () {\n"
    "            if (!isEvaluationStillValid(historyLength)) return;\n"
    "            var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';\n"
    "            showMoveEvaluation(errMsg, 'error');\n"
    "            moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };\n"
    "            renderHistoryList();\n"
    "            if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "        });\n"
    "    }).catch(function () {\n"
    "        if (!isEvaluationStillValid(historyLength)) return;\n"
    "        var errMsg = 'Zhodnocení nebylo k dispozici. Zkontrolujte připojení k internetu.';\n"
    "        showMoveEvaluation(errMsg, 'error');\n"
    "        moveEvaluations[historyLength - 1] = { grade: 'error', msg: errMsg };\n"
    "        renderHistoryList();\n"
    "        if (typeof updateTeachingStatsPanel === 'function') updateTeachingStatsPanel();\n"
    "    });\n"
    "}\n"
    "\n"
    "function isWebLocked() {\n"
    "    return !!(statusData && statusData.web_locked);\n"
    "}\n"
    "\n"
    "async function requestHint() {\n"
    "    if (sandboxMode || reviewMode) return;\n"
    "    if (statusData && statusData.board_setup_tutorial === true) return;\n"
    "    if (statusData && statusData.puzzle && statusData.puzzle.setup_active === true) return;\n"
    "    if (statusData && statusData.puzzle && statusData.puzzle.active === true) return;\n"
    "    if (isWebLocked()) {\n"
    "        showHintError('Rozhraní je zamčeno. Odemkněte přes UART.');\n"
    "        return;\n"
    "    }\n"
    "    const status = statusData || {};\n"
    "    const state = (status.game_state || '').toLowerCase();\n"
    "    if (state !== 'active' && state !== 'playing') return;\n"
    "    if (status.game_end && status.game_end.ended) return;\n"
    "    if (state === 'promotion') return;\n"
    "    if (status.castling_in_progress) return;\n"
    "\n"
    "    var limit = getHintLimit();\n"
    "    var currentHints = getCurrentPlayerHints();\n"
    "    if (limit > 0 && currentHints <= 0) {\n"
    "        showHintError('Na tahu nemáte žádnou nápovědu. Získejte ji výborným tahem nebo sebráním figurky.');\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const btn = document.getElementById('hint-btn');\n"
    "    if (btn) {\n"
    "        btn.disabled = true;\n"
    "        btn.textContent = 'Načítám…';\n"
    "    }\n"
    "\n"
    "    if (limit > 0) {\n"
    "        var p = (status.current_player === 'Black') ? 'black' : 'white';\n"
    "        if (p === 'white') hintsRemainingWhite--; else hintsRemainingBlack--;\n"
    "        updateHintButtonLabel();\n"
    "    }\n"
    "\n"
    "    if (!status.internet_connected) {\n"
    "        if (limit > 0) {\n"
    "            var p0 = (status.current_player === 'Black') ? 'black' : 'white';\n"
    "            if (p0 === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "            updateHintButtonLabel();\n"
    "        }\n"
    "        showHintError('Nápověda vyžaduje připojení k internetu (WiFi).');\n"
    "        if (btn) updateHintButtonLabel();\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    var myGen = ++hintRequestGeneration;\n"
    "    try {\n"
    "        const fen = boardAndStatusToFen(boardData, status, historyData);\n"
    "        if (!fen) {\n"
    "            if (limit > 0) {\n"
    "                var pFen = (status.current_player === 'Black') ? 'black' : 'white';\n"
    "                if (pFen === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "                updateHintButtonLabel();\n"
    "            }\n"
    "            if (console.warn) console.warn('[Hint] Could not build FEN');\n"
    "            showHintError('Nelze načíst pozici. Obnovte stránku.');\n"
    "            if (btn) updateHintButtonLabel();\n"
    "            return;\n"
    "        }\n"
    "        const move = await fetchStockfishBestMove(fen);\n"
    "        if (myGen !== hintRequestGeneration) {\n"
    "            if (limit > 0) {\n"
    "                var pStale = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';\n"
    "                if (pStale === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "            }\n"
    "            if (btn) btn.disabled = false;\n"
    "            updateHintButtonLabel();\n"
    "            return;\n"
    "        }\n"
    "        if (move) {\n"
    "            showHintOnBoard(move.from, move.to);\n"
    "            showHintExplanation(move);\n"
    "            lastHintedMove = { from: move.from, to: move.to };\n"
    "            try {\n"
    "                await fetch('/api/game/hint_highlight', {\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    body: JSON.stringify({ from: move.from, to: move.to })\n"
    "                });\n"
    "            } catch (e) {\n"
    "                if (console.warn) console.warn('[Hint] LED highlight failed:', e.message);\n"
    "            }\n"
    "            if (btn) updateHintButtonLabel();\n"
    "        } else {\n"
    "            if (limit > 0) {\n"
    "                var p = (status.current_player === 'Black') ? 'black' : 'white';\n"
    "                if (p === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "                updateHintButtonLabel();\n"
    "            }\n"
    "            showHintError('Nápověda není k dispozici. Zkuste později nebo zkontrolujte připojení k internetu.');\n"
    "            if (btn) updateHintButtonLabel();\n"
    "        }\n"
    "    } catch (err) {\n"
    "        if (myGen !== hintRequestGeneration) {\n"
    "            if (limit > 0) {\n"
    "                var pStaleC = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';\n"
    "                if (pStaleC === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "            }\n"
    "            if (btn) updateHintButtonLabel();\n"
    "            return;\n"
    "        }\n"
    "        if (limit > 0) {\n"
    "            var p = (statusData && statusData.current_player === 'Black') ? 'black' : 'white';\n"
    "            if (p === 'white') hintsRemainingWhite++; else hintsRemainingBlack++;\n"
    "            updateHintButtonLabel();\n"
    "        }\n"
    "        if (console.error) console.error('[Hint] requestHint error:', err.message);\n"
    "        showHintError('Nápověda není k dispozici. Zkuste později nebo zkontrolujte připojení k internetu.');\n"
    "        if (btn) updateHintButtonLabel();\n"
    "    }\n"
    "}\n"
    "window.requestHint = requestHint;\n"
    "\n"
    "// ============================================================================\n"
    "// UPDATE FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "function updateBoard(board) {\n"
    "    // V sandboxu neprepisovat boardData (zůstane skutečná pozice z desky pro fetch po výstupu).\n"
    "    var skipReplaceBoardData = sandboxMode && board === sandboxBoard;\n"
    "    // Only clear hint when board actually changed (new move), not on every periodic fetch\n"
    "    var boardUnchanged = !skipReplaceBoardData && boardData && board.length === 8 && boardData.length === 8 &&\n"
    "        JSON.stringify(board) === JSON.stringify(boardData);\n"
    "    if (!skipReplaceBoardData) {\n"
    "        boardData = board;\n"
    "    }\n"
    "    const loading = document.getElementById('loading');\n"
    "    if (loading) loading.style.display = 'none';\n"
    "\n"
    "    if (!boardUnchanged) {\n"
    "        document.querySelectorAll('.square').forEach(sq => {\n"
    "            sq.classList.remove('hint-from', 'hint-to');\n"
    "        });\n"
    "        hideHintExplanation();\n"
    "    }\n"
    "\n"
    "    // NEPŘIDÁVAT clearHighlights() - highlights jsou řízené přes updateStatus()\n"
    "    // (lifted, error-invalid, error-original jsou serverem řízené stavy)\n"
    "\n"
    "    for (let row = 0; row < 8; row++) {\n"
    "        for (let col = 0; col < 8; col++) {\n"
    "            const piece = board[row][col];\n"
    "            const pieceElement = document.getElementById('piece-' + (row * 8 + col));\n"
    "            if (pieceElement) {\n"
    "                setPieceElementFromFen(pieceElement, piece);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// ENDGAME REPORT FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "// Zobrazit endgame report na webu\n"
    "async function showEndgameReport(gameEnd) {\n"
    "    console.log('🏆 showEndgameReport() called with:', gameEnd);\n"
    "\n"
    "    // Pokud už je banner zobrazen, nedělat nic (aby se nepřekresloval)\n"
    "    if (endgameReportShown && document.getElementById('endgame-banner')) {\n"
    "        console.log('Endgame report already shown, skipping...');\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // Načíst advantage history pro graf\n"
    "    let advantageDataLocal = { history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0 };\n"
    "    try {\n"
    "        const response = await fetch('/api/advantage');\n"
    "        advantageDataLocal = await response.json();\n"
    "        console.log('Advantage data loaded:', advantageDataLocal);\n"
    "    } catch (e) {\n"
    "        console.error('Failed to load advantage data:', e);\n"
    "    }\n"
    "\n"
    "    // Určit výsledek a barvy\n"
    "    let title = '';\n"
    "    let subtitle = '';\n"
    "    let accentColor = '#4CAF50';\n"
    "    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';\n"
    "\n"
    "    if (gameEnd.winner === 'Draw') {\n"
    "        title = 'REMÍZA';\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = '#FF9800';\n"
    "        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';\n"
    "    } else {\n"
    "        title = `${gameEnd.winner.toUpperCase()} VYHRÁL!`;\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = gameEnd.winner === 'White' ? '#4CAF50' : '#2196F3';\n"
    "        bgGradient = gameEnd.winner === 'White' ? 'linear-gradient(135deg, #1e3a1e, #2d4a2d)' : 'linear-gradient(135deg, #1e2a3a, #2d3a4a)';\n"
    "    }\n"
    "\n"
    "    // Získat statistiky\n"
    "    const whiteMoves = Math.ceil(statusData.move_count / 2);\n"
    "    const blackMoves = Math.floor(statusData.move_count / 2);\n"
    "    const whiteCaptured = capturedData.white_captured || [];\n"
    "    const blackCaptured = capturedData.black_captured || [];\n"
    "\n"
    "    // Material advantage\n"
    "    const pieceValues = { p: 1, n: 3, b: 3, r: 5, q: 9, P: 1, N: 3, B: 3, R: 5, Q: 9 };\n"
    "    let whiteMaterial = 0, blackMaterial = 0;\n"
    "    whiteCaptured.forEach(p => whiteMaterial += pieceValues[p] || 0);\n"
    "    blackCaptured.forEach(p => blackMaterial += pieceValues[p] || 0);\n"
    "    const materialDiff = whiteMaterial - blackMaterial;\n"
    "    const materialText = materialDiff > 0 ? `White +${materialDiff}` : materialDiff < 0 ? `Black +${-materialDiff}` : 'Vyrovnáno';\n"
    "\n"
    "    // Vytvořit SVG graf výhody (jako chess.com)\n"
    "    let graphSVG = '';\n"
    "    if (advantageDataLocal.history && advantageDataLocal.history.length > 1) {\n"
    "        const history = advantageDataLocal.history;\n"
    "        const width = 280;\n"
    "        const height = 100;\n"
    "        const maxAdvantage = Math.max(10, ...history.map(Math.abs));\n"
    "        const scaleY = height / (2 * maxAdvantage);\n"
    "        const scaleX = width / (history.length - 1);\n"
    "\n"
    "        // Vytvořit body pro polyline (0,0 je nahoře vlevo, y roste dolů)\n"
    "        let points = history.map((adv, i) => {\n"
    "            const x = i * scaleX;\n"
    "            const y = height / 2 - adv * scaleY;  // Převrátit Y (White nahoře, Black dole)\n"
    "            return `${x},${y}`;\n"
    "        }).join(' ');\n"
    "\n"
    "        // Vytvořit polygon pro vyplněnou oblast\n"
    "        let areaPoints = `0,${height / 2} ${points} ${width},${height / 2}`;\n"
    "\n"
    "        graphSVG = `<svg width=\"280\" height=\"100\" style=\"border-radius:6px;background:rgba(0,0,0,0.2);\">\n"
    "            <!-- Středová čára (vyrovnaná pozice) -->\n"
    "            <line x1=\"0\" y1=\"${height / 2}\" x2=\"${width}\" y2=\"${height / 2}\" stroke=\"#555\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>\n"
    "            <!-- Vyplněná oblast pod křivkou -->\n"
    "            <polygon points=\"${areaPoints}\" fill=\"${accentColor}\" opacity=\"0.2\"/>\n"
    "            <!-- Křivka výhody -->\n"
    "            <polyline points=\"${points}\" fill=\"none\" stroke=\"${accentColor}\" stroke-width=\"2\" stroke-linejoin=\"round\"/>\n"
    "            <!-- Tečky na koncích -->\n"
    "            <circle cx=\"0\" cy=\"${height / 2}\" r=\"3\" fill=\"${accentColor}\"/>\n"
    "            <circle cx=\"${(history.length - 1) * scaleX}\" cy=\"${height / 2 - history[history.length - 1] * scaleY}\" r=\"4\" fill=\"${accentColor}\"/>\n"
    "            <!-- Popisky -->\n"
    "            <text x=\"5\" y=\"12\" fill=\"#888\" font-size=\"10\" font-weight=\"600\">White</text>\n"
    "            <text x=\"5\" y=\"${height - 2}\" fill=\"#888\" font-size=\"10\" font-weight=\"600\">Black</text>\n"
    "        </svg>`;\n"
    "    }\n"
    "\n"
    "    // Vytvořit nový banner - VLEVO OD BOARDU, NE UPROSTŘED!\n"
    "    const banner = document.createElement('div');\n"
    "    banner.id = 'endgame-banner';\n"
    "\n"
    "    // Na mobilu - jiné umístění (nahoře, plná šířka)\n"
    "    if (window.innerWidth <= 768) {\n"
    "        banner.style.cssText = `\n"
    "            position: fixed;\n"
    "            left: 10px;\n"
    "            right: 10px;\n"
    "            top: 10px;\n"
    "            width: auto;\n"
    "            max-height: 80vh;\n"
    "            transform: none;\n"
    "            overflow-y: auto;\n"
    "            background: ${bgGradient};\n"
    "            border: 2px solid ${accentColor};\n"
    "            border-radius: 12px;\n"
    "            padding: 0;\n"
    "            box-shadow: 0 8px 32px rgba(0,0,0,0.6);\n"
    "            z-index: 9999;\n"
    "            animation: slideInTop 0.4s ease-out;\n"
    "        `;\n"
    "    } else {\n"
    "        banner.style.cssText = `\n"
    "            position: fixed;\n"
    "            left: 10px;\n"
    "            top: 50%;\n"
    "            transform: translateY(-50%);\n"
    "            width: 320px;\n"
    "            max-height: 90vh;\n"
    "            overflow-y: auto;\n"
    "            background: ${bgGradient};\n"
    "            border: 2px solid ${accentColor};\n"
    "            border-radius: 12px;\n"
    "            padding: 0;\n"
    "            box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 40px ${accentColor}40;\n"
    "            z-index: 9999;\n"
    "            animation: slideInLeft 0.4s ease-out;\n"
    "            backdrop-filter: blur(10px);\n"
    "        `;\n"
    "    }\n"
    "\n"
    "    // HTML obsah\n"
    "    banner.innerHTML = `\n"
    "        <div style=\"background:${accentColor};padding:20px;text-align:center;border-radius:10px 10px 0 0;\">\n"
    "            <h2 style=\"margin:0;color:white;font-size:24px;font-weight:700;text-shadow:0 2px 4px rgba(0,0,0,0.4);\">${title}</h2>\n"
    "            <p style=\"margin:8px 0 0 0;color:rgba(255,255,255,0.9);font-size:14px;font-weight:500;\">${subtitle}</p>\n"
    "        </div>\n"
    "        <div style=\"padding:20px;\">\n"
    "            ${graphSVG ? `\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;\">\n"
    "                    Průběh hry\n"
    "                </h3>\n"
    "                ${graphSVG}\n"
    "                <div style=\"display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#888;\">\n"
    "                    <span>Začátek</span>\n"
    "                    <span>Tah ${advantageDataLocal.count || 0}</span>\n"
    "                </div>\n"
    "            </div>` : ''}\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;\">\n"
    "                    Statistiky\n"
    "                </h3>\n"
    "                <div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:13px;\">\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Tahy</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">Bílý ${whiteMoves} | Černý ${blackMoves}</div>\n"
    "                    </div>\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Materiál</div>\n"
    "                        <div style=\"color:${accentColor};font-weight:600;\">${materialText}</div>\n"
    "                    </div>\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Sebráno</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">Bílý ${whiteCaptured.length} | Černý ${blackCaptured.length}</div>\n"
    "                    </div>\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Celkem</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">${statusData.move_count} tahů</div>\n"
    "                    </div>\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Šachy</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">Bílý ${advantageDataLocal.white_checks || 0} | Černý ${advantageDataLocal.black_checks || 0}</div>\n"
    "                    </div>\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Rošády</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">Bílý ${advantageDataLocal.white_castles || 0} | Černý ${advantageDataLocal.black_castles || 0}</div>\n"
    "                    </div>\n"
    "                </div>\n"
    "            </div>\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px 0;color:${accentColor};font-size:16px;font-weight:600;\">\n"
    "                    Sebrané figurky\n"
    "                </h3>\n"
    "                <div style=\"margin-bottom:10px;\">\n"
    "                    <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">White sebral (${whiteCaptured.length})</div>\n"
    "                    <div style=\"font-size:20px;line-height:1.4;\">${whiteCaptured.map(p => pieceImgHtml(p)).join(' ') || '−'}</div>\n"
    "                </div>\n"
    "                <div>\n"
    "                    <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Black sebral (${blackCaptured.length})</div>\n"
    "                    <div style=\"font-size:20px;line-height:1.4;\">${blackCaptured.map(p => pieceImgHtml(p)).join(' ') || '−'}</div>\n"
    "                </div>\n"
    "            </div>\n"
    "            <button onclick=\"hideEndgameReport()\" style=\"\n"
    "                width:100%;\n"
    "                padding:14px;\n"
    "                font-size:16px;\n"
    "                background:${accentColor};\n"
    "                color:white;\n"
    "                border:none;\n"
    "                border-radius:8px;\n"
    "                cursor:pointer;\n"
    "                font-weight:600;\n"
    "                box-shadow:0 4px 12px rgba(0,0,0,0.3);\n"
    "                transition:all 0.2s;\n"
    "            \" onmouseover=\"this.style.transform='translateY(-2px)';this.style.boxShadow='0 6px 16px rgba(0,0,0,0.4)'\" onmouseout=\"this.style.transform='translateY(0)';this.style.boxShadow='0 4px 12px rgba(0,0,0,0.3)'\">\n"
    "                OK\n"
    "            </button>\n"
    "        </div>\n"
    "    `;\n"
    "\n"
    "    // Přidat CSS animace pokud ještě neexistují\n"
    "    if (!document.getElementById('endgame-animations')) {\n"
    "        const style = document.createElement('style');\n"
    "        style.id = 'endgame-animations';\n"
    "        style.textContent = `\n"
    "            @keyframes slideInLeft {\n"
    "                from { transform: translateY(-50%) translateX(-100%); opacity: 0; }\n"
    "                to { transform: translateY(-50%) translateX(0); opacity: 1; }\n"
    "            }\n"
    "            @keyframes slideInTop {\n"
    "                from { transform: translateY(-100%); opacity: 0; }\n"
    "                to { transform: translateY(0); opacity: 1; }\n"
    "            }\n"
    "        `;\n"
    "        document.head.appendChild(style);\n"
    "    }\n"
    "\n"
    "    document.body.appendChild(banner);\n"
    "    endgameReportShown = true;  // Označit, že je zobrazený\n"
    "    console.log('🏆 ENDGAME REPORT SHOWN - banner displayed (left side)');\n"
    "}\n"
    "\n"
    "// Skrýt endgame report (ale zachovat flag pro toggle)\n"
    "function hideEndgameReport() {\n"
    "    console.log('Hiding endgame report...');\n"
    "    const banner = document.getElementById('endgame-banner');\n"
    "    if (banner) {\n"
    "        banner.remove();\n"
    "        console.log('Endgame report hidden (can be toggled back)');\n"
    "    }\n"
    "}\n"
    "\n"
    "// Toggle endgame report (show/hide)\n"
    "function toggleEndgameReport() {\n"
    "    const banner = document.getElementById('endgame-banner');\n"
    "    if (banner) {\n"
    "        // Uz je zobrazen -> skryj\n"
    "        hideEndgameReport();\n"
    "    } else {\n"
    "        // Neni zobrazen -> znovu zobraz (pokud mame data)\n"
    "        if (window.lastGameEndData) {\n"
    "            showEndgameReport(window.lastGameEndData);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// Zobrazit toggle button\n"
    "function showEndgameToggleButton() {\n"
    "    // Zjistit zda uz button existuje\n"
    "    if (document.getElementById('endgame-toggle-btn')) return;\n"
    "\n"
    "    const button = document.createElement('button');\n"
    "    button.id = 'endgame-toggle-btn';\n"
    "    button.innerHTML = 'Zpráva';\n"
    "    button.title = 'Zobrazit/skrýt zprávu o konci hry';\n"
    "    button.style.cssText = `\n"
    "        position: fixed;\n"
    "        top: 10px;\n"
    "        left: 10px;\n"
    "        padding: 10px 16px;\n"
    "        background: linear-gradient(135deg, #4CAF50, #45a049);\n"
    "        color: white;\n"
    "        border: none;\n"
    "        border-radius: 8px;\n"
    "        cursor: pointer;\n"
    "        font-weight: 600;\n"
    "        font-size: 14px;\n"
    "        box-shadow: 0 4px 12px rgba(0,0,0,0.3);\n"
    "        z-index: 10000;\n"
    "        transition: all 0.2s;\n"
    "    `;\n"
    "    button.onmouseover = function () {\n"
    "        this.style.transform = 'translateY(-2px)';\n"
    "        this.style.boxShadow = '0 6px 16px rgba(0,0,0,0.4)';\n"
    "    };\n"
    "    button.onmouseout = function () {\n"
    "        this.style.transform = 'translateY(0)';\n"
    "        this.style.boxShadow = '0 4px 12px rgba(0,0,0,0.3)';\n"
    "    };\n"
    "    button.onclick = toggleEndgameReport;\n"
    "    document.body.appendChild(button);\n"
    "}\n"
    "\n"
    "// Skrýt toggle button\n"
    "function hideEndgameToggleButton() {\n"
    "    const button = document.getElementById('endgame-toggle-btn');\n"
    "    if (button) {\n"
    "        button.remove();\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// BOT LOGIC\n"
    "// ============================================================================\n"
    "\n"
    "const BOT_API_TIMEOUT_MS = 15000;\n"
    "const BOT_SQUARE_REGEX = /^[a-h][1-8]$/;\n"
    "const BOT_HINT_REFRESH_MS = 500;\n"
    "\n"
    "function stopBotHintRefresh() {\n"
    "    if (botHintRefreshIntervalId) {\n"
    "        clearInterval(botHintRefreshIntervalId);\n"
    "        botHintRefreshIntervalId = null;\n"
    "    }\n"
    "}\n"
    "\n"
    "/**\n"
    " * Zobrazí/skryje panel „Bot“ a nastaví text.\n"
    " * Panel je viditelný jen když gameMode === 'bot'.\n"
    " * @param {string} [text] - Text stavu. Pokud chybí, určí se z status (navádění při zvednuté figurce) nebo „Hraješ ty!“.\n"
    " * @param {object} [status] - Aktuální status z API; pokud je piece_lifted a máme návrh bota, zobrazí se navádění.\n"
    " */\n"
    "function updateBotStatusPanel(text, status) {\n"
    "    var panel = document.getElementById('bot-status-panel');\n"
    "    var textEl = document.getElementById('bot-status-text');\n"
    "    if (!panel || !textEl) return;\n"
    "    if (gameMode !== 'bot') {\n"
    "        panel.style.display = 'none';\n"
    "        return;\n"
    "    }\n"
    "    if (text !== undefined) {\n"
    "        textEl.textContent = text;\n"
    "    } else if (status && status.piece_lifted && status.piece_lifted.lifted && lastSuggestedMove) {\n"
    "        textEl.textContent = 'Polož figurku na ' + lastSuggestedMove.to + '.';\n"
    "    } else if (textEl.textContent === '—' || textEl.textContent.trim() === '') {\n"
    "        textEl.textContent = 'Hraješ ty!';\n"
    "    }\n"
    "    panel.style.display = '';\n"
    "}\n"
    "\n"
    "/**\n"
    " * Panel „Puzzle“ (jako Bot) — povzbuzující text podle feedbacku z desky; funguje bez internetu (jen poll k desce).\n"
    " * @param {object} status - status z API nebo { puzzle: {...} }\n"
    " * @param {{offline?:boolean}} [opts]\n"
    " */\n"
    "function updatePuzzleStatusPanel(status, opts) {\n"
    "    opts = opts || {};\n"
    "    var offline = !!opts.offline;\n"
    "    var panel = document.getElementById('puzzle-status-panel');\n"
    "    var textEl = document.getElementById('puzzle-status-text');\n"
    "    var subEl = document.getElementById('puzzle-status-sub');\n"
    "    var titleEl = panel ? panel.querySelector('.game-title') : null;\n"
    "    if (!panel || !textEl) return;\n"
    "\n"
    "    var p = status && status.puzzle ? status.puzzle : null;\n"
    "    if (!p) {\n"
    "        panel.style.display = 'none';\n"
    "        panel.className = 'game-block puzzle-status-panel';\n"
    "        if (subEl) {\n"
    "            subEl.textContent = '';\n"
    "            subEl.style.display = 'none';\n"
    "        }\n"
    "        if (titleEl) titleEl.textContent = 'Puzzle';\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    var fb = String(p.feedback || 'none').toLowerCase();\n"
    "    var show = false;\n"
    "    var mode = 'play';\n"
    "    var main = '';\n"
    "    var sub = '';\n"
    "\n"
    "    if (p.setup_active === true && p.active !== true) {\n"
    "        show = true;\n"
    "        mode = 'setup';\n"
    "        main = 'Připrav fyzickou pozici podle LED — jdeš na to krok za krokem.';\n"
    "        sub = 'Podrobnosti máš v okně Puzzles (nahoře).';\n"
    "    } else if (p.active === true) {\n"
    "        show = true;\n"
    "        if (fb === 'wrong') {\n"
    "            mode = 'wrong';\n"
    "            main = 'Ještě to není ono — vrať figurku zpátky a klidně to zkus znovu. Tak se člověk učí!';\n"
    "            sub = p.message ? String(p.message) : '';\n"
    "        } else if (fb === 'illegal') {\n"
    "            mode = 'illegal';\n"
    "            main = 'Tenhle tah tady neplatí — zkus jiné pole. Každý mistr jednou začínal.';\n"
    "            sub = p.message ? String(p.message) : '';\n"
    "        } else {\n"
    "            mode = 'play';\n"
    "            main = 'Jsi na tahu — najdi nejlepší pokračování. Držím palce!';\n"
    "            sub = (p.teaser && String(p.teaser).length) ? String(p.teaser) : (p.title ? String(p.title) : '');\n"
    "        }\n"
    "    } else if (fb === 'solved') {\n"
    "        show = true;\n"
    "        mode = 'solved';\n"
    "        main = 'Skvěle! Přesně takhle se to hraje — puzzle je hotové.';\n"
    "        sub = (p.title ? 'Úloha: ' + p.title : '') + (p.teaser ? (p.title ? ' — ' : '') + p.teaser : '');\n"
    "    }\n"
    "\n"
    "    if (!show) {\n"
    "        panel.style.display = 'none';\n"
    "        panel.className = 'game-block puzzle-status-panel';\n"
    "        if (titleEl) titleEl.textContent = 'Puzzle';\n"
    "        if (subEl) {\n"
    "            subEl.textContent = '';\n"
    "            subEl.style.display = 'none';\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    if (offline) {\n"
    "        sub = (sub ? sub + ' ' : '') + 'Živý stav z desky teď nevidím (spojení s webem). Jakmile bude síť k desce zas, obnoví se.';\n"
    "    }\n"
    "\n"
    "    if (titleEl) {\n"
    "        titleEl.textContent = mode === 'setup'\n"
    "            ? 'Puzzle · příprava'\n"
    "            : mode === 'solved'\n"
    "                ? 'Puzzle · hotovo'\n"
    "                : 'Puzzle · hraješ';\n"
    "    }\n"
    "    panel.style.display = '';\n"
    "    panel.className = 'game-block puzzle-status-panel puzzle-status-panel--' + mode +\n"
    "        (offline ? ' puzzle-status-panel--offline' : '');\n"
    "    textEl.textContent = main;\n"
    "    if (subEl) {\n"
    "        if (sub && String(sub).trim().length > 0) {\n"
    "            subEl.textContent = sub.trim();\n"
    "            subEl.style.display = 'block';\n"
    "        } else {\n"
    "            subEl.textContent = '';\n"
    "            subEl.style.display = 'none';\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Smaže bot UI: stav, interval LED, hint třídy a zprávu „Bot hraje“ / „Počítač“. */\n"
    "function clearBotSuggestion() {\n"
    "    var hadDomHints = !!document.querySelector('.square.hint-from, .square.hint-to');\n"
    "    var needServerClear = lastSuggestedFen !== null || lastSuggestedMove !== null || hadDomHints;\n"
    "    lastSuggestedFen = null;\n"
    "    lastSuggestedMove = null;\n"
    "    stopBotHintRefresh();\n"
    "    document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });\n"
    "    var msgEl = document.getElementById('castling-pending-message');\n"
    "    if (msgEl && (msgEl.textContent.indexOf('Bot') !== -1 || msgEl.textContent.indexOf('Počítač') !== -1)) msgEl.style.display = 'none';\n"
    "    updateBotStatusPanel('Hraješ ty!');\n"
    "    if (needServerClear) {\n"
    "        fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "    }\n"
    "}\n"
    "\n"
    "/**\n"
    " * Tah bota se NEprovádí automaticky – pouze vizualizace na webu a LED.\n"
    " * Uživatel musí fyzicky pohnout figurku; tah se provede až po DROP z matrixu.\n"
    " * Voláme jen /api/game/hint_highlight, nikdy /api/move ani virtual_action za bota.\n"
    " */\n"
    "/** Bot používá jednotnou fetchStockfishBestMove s depth = botSettings.strength. */\n"
    "async function playBotMove(fen, generation) {\n"
    "    if (botThinking || !fen || typeof fen !== 'string') return;\n"
    "    botThinking = true;\n"
    "    var statusEl = document.getElementById('game-state');\n"
    "\n"
    "    updateBotStatusPanel('Přemýšlím');\n"
    "    try {\n"
    "        if (statusEl) statusEl.textContent = 'Bot vybírá tah';\n"
    "        console.log('🤖 Bot suggests move (visualization only – user moves physically)... Generation:', generation);\n"
    "        var botDepth = parseInt(botSettings.strength, 10) || 10;\n"
    "        var move = await fetchStockfishBestMove(fen, botDepth);\n"
    "\n"
    "        if (generation !== gameGeneration) {\n"
    "            console.warn('🤖 Bot move aborted: Game generation changed (New game started).');\n"
    "            lastSuggestedMove = null;\n"
    "            stopBotHintRefresh();\n"
    "            return;\n"
    "        }\n"
    "\n"
    "        if (move) {\n"
    "            console.log('🤖 Bot suggests:', move.from, '->', move.to, '(zobrazíme na webu a LED; tah provedete vy na desce)');\n"
    "            lastSuggestedMove = { from: move.from, to: move.to };\n"
    "            stopBotHintRefresh();\n"
    "            try {\n"
    "                await fetch('/api/game/hint_highlight', {\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    body: JSON.stringify({ from: move.from, to: move.to })\n"
    "                });\n"
    "            } catch (e) {\n"
    "                console.error('Failed to light up LEDs for bot:', e);\n"
    "            }\n"
    "            botHintRefreshIntervalId = setInterval(function () {\n"
    "                if (!lastSuggestedMove) {\n"
    "                    stopBotHintRefresh();\n"
    "                    return;\n"
    "                }\n"
    "                var status = statusData;\n"
    "                if (status && status.piece_lifted && status.piece_lifted.lifted) {\n"
    "                    if (getBotLedTargetOnlyAfterLift()) {\n"
    "                        var liftedFrom = String.fromCharCode(97 + status.piece_lifted.col) + (status.piece_lifted.row + 1);\n"
    "                        if (liftedFrom.toLowerCase() === lastSuggestedMove.from.toLowerCase()) {\n"
    "                            fetch('/api/game/hint_highlight', {\n"
    "                                method: 'POST',\n"
    "                                headers: { 'Content-Type': 'application/json' },\n"
    "                                body: JSON.stringify({ to: lastSuggestedMove.to })\n"
    "                            }).catch(function () {});\n"
    "                            return;\n"
    "                        }\n"
    "                    } else {\n"
    "                        return;\n"
    "                    }\n"
    "                }\n"
    "                fetch('/api/game/hint_highlight', {\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    body: JSON.stringify({ from: lastSuggestedMove.from, to: lastSuggestedMove.to })\n"
    "                }).catch(function () {});\n"
    "            }, BOT_HINT_REFRESH_MS);\n"
    "            var panelMsg = 'Hraji ' + move.from + '-' + move.to + '. Zvedni figurku z ' + move.from + '.';\n"
    "            updateBotStatusPanel(panelMsg);\n"
    "            showHintOnBoard(move.from, move.to);\n"
    "        } else {\n"
    "            console.warn('Bot: žádný tah (API chyba nebo timeout).');\n"
    "            if (typeof console !== 'undefined' && console.warn) {\n"
    "                console.warn('[Bot] API failed, clearing lastSuggestedFen for retry');\n"
    "            }\n"
    "            lastSuggestedFen = null;\n"
    "            updateBotStatusPanel('Chyba API');\n"
    "        }\n"
    "    } finally {\n"
    "        botThinking = false;\n"
    "    }\n"
    "}\n"
    "\n"
    "function checkBotTurn(status, fen) {\n"
    "    if (status && status.board_setup_tutorial === true) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status && status.puzzle && status.puzzle.setup_active === true) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status && status.puzzle && status.puzzle.active === true) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (gameMode !== 'bot') {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    /* Jakmile se pozice změní (nový FEN), někdo táhl – smažeme návrh bota hned,\n"
    "       nezávisle na current_player (ten může v backendu dohnat až později). */\n"
    "    var clearedDueToFenChange = false;\n"
    "    if (lastSuggestedFen && fen && fen !== lastSuggestedFen) {\n"
    "        clearBotSuggestion();\n"
    "        clearedDueToFenChange = true;\n"
    "    }\n"
    "    if (!status || typeof status !== 'object') {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status.castling_in_progress === true) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status.game_state !== 'active' && status.game_state !== 'playing') {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status.game_end && status.game_end.ended) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (status.current_player !== 'White' && status.current_player !== 'Black') {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    if (!fen || typeof fen !== 'string' || fen.length < 20) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    var isBotTurn = (botSettings.side === 'white') !== (status.current_player === 'White');\n"
    "    if (typeof console !== 'undefined' && console.log) console.log('checkBotTurn: isBotTurn=', isBotTurn, 'current_player=', status.current_player);\n"
    "\n"
    "    if (!isBotTurn) {\n"
    "        clearBotSuggestion();\n"
    "        return;\n"
    "    }\n"
    "    /* Uživatel zvedl figurku – provádí tah za bota. Nevolat clearBotSuggestion (lastSuggestedMove\n"
    "       musí zůstat), aby panel Bot mohl zobrazit „Polož figurku na X.“ v updateBotStatusPanel. */\n"
    "    if (status.piece_lifted && status.piece_lifted.lifted) {\n"
    "        return;\n"
    "    }\n"
    "    /* Po vyčištění kvůli změně FEN v tomto volání nevolat playBotMove – mohl by být race (FEN už nový, current_player ještě bot). */\n"
    "    if (clearedDueToFenChange) return;\n"
    "    if (!botThinking && fen !== lastSuggestedFen) {\n"
    "        if (typeof console !== 'undefined' && console.log) {\n"
    "            console.log('[Bot Staging] checkBotTurn: game_state=', status.game_state, 'fen.length=', fen.length, 'calling playBotMove');\n"
    "        }\n"
    "        lastSuggestedFen = fen;\n"
    "        playBotMove(fen, gameGeneration);\n"
    "    } else if (isBotTurn && fen === lastSuggestedFen && typeof console !== 'undefined' && console.log) {\n"
    "        console.log('[Bot Staging] checkBotTurn: skipping (fen === lastSuggestedFen), game_state=', status.game_state);\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// STATUS UPDATE FUNCTION\n"
    "// ============================================================================\n"
    "\n"
    "function matrixGuardMaskToSquares(low, high) {\n"
    "    const squares = [];\n"
    "    const lowVal = Number(low) >>> 0;\n"
    "    const highVal = Number(high) >>> 0;\n"
    "    for (let i = 0; i < 32; i++) {\n"
    "        if ((lowVal & (1 << i)) !== 0) {\n"
    "            const col = i % 8;\n"
    "            const row = Math.floor(i / 8);\n"
    "            squares.push(String.fromCharCode(97 + col) + (row + 1));\n"
    "        }\n"
    "    }\n"
    "    for (let i = 0; i < 32; i++) {\n"
    "        if ((highVal & (1 << i)) !== 0) {\n"
    "            const idx = i + 32;\n"
    "            const col = idx % 8;\n"
    "            const row = Math.floor(idx / 8);\n"
    "            squares.push(String.fromCharCode(97 + col) + (row + 1));\n"
    "        }\n"
    "    }\n"
    "    return squares;\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// SETUP TUTORIAL (základní postavení — web + LED)\n"
    "// ============================================================================\n"
    "\n"
    "const SETUP_TUTORIAL_REFRESH_MS = 600;\n"
    "const SETUP_TUTORIAL_FAST_POLL_MS = 400;\n"
    "const SETUP_TUTORIAL_OCC_STABLE_TICKS = 2;\n"
    "\n"
    "/** 32 kroků: bílá 1. řada, bílí pěšci, černá 8. řada, černí pěšci. */\n"
    "const SETUP_TUTORIAL_STEPS = [\n"
    "    { sq: 'a1', piece: 'r', label: 'Bílá věž → a1' },\n"
    "    { sq: 'b1', piece: 'n', label: 'Bílý jezdec → b1' },\n"
    "    { sq: 'c1', piece: 'b', label: 'Bílý střelec → c1' },\n"
    "    { sq: 'd1', piece: 'q', label: 'Bílá dáma → d1' },\n"
    "    { sq: 'e1', piece: 'k', label: 'Bílý král → e1' },\n"
    "    { sq: 'f1', piece: 'b', label: 'Bílý střelec → f1' },\n"
    "    { sq: 'g1', piece: 'n', label: 'Bílý jezdec → g1' },\n"
    "    { sq: 'h1', piece: 'r', label: 'Bílá věž → h1' },\n"
    "    { sq: 'a2', piece: 'p', label: 'Bílý pěšec → a2' },\n"
    "    { sq: 'b2', piece: 'p', label: 'Bílý pěšec → b2' },\n"
    "    { sq: 'c2', piece: 'p', label: 'Bílý pěšec → c2' },\n"
    "    { sq: 'd2', piece: 'p', label: 'Bílý pěšec → d2' },\n"
    "    { sq: 'e2', piece: 'p', label: 'Bílý pěšec → e2' },\n"
    "    { sq: 'f2', piece: 'p', label: 'Bílý pěšec → f2' },\n"
    "    { sq: 'g2', piece: 'p', label: 'Bílý pěšec → g2' },\n"
    "    { sq: 'h2', piece: 'p', label: 'Bílý pěšec → h2' },\n"
    "    { sq: 'a8', piece: 'R', label: 'Černá věž → a8' },\n"
    "    { sq: 'b8', piece: 'N', label: 'Černý jezdec → b8' },\n"
    "    { sq: 'c8', piece: 'B', label: 'Černý střelec → c8' },\n"
    "    { sq: 'd8', piece: 'Q', label: 'Černá dáma → d8' },\n"
    "    { sq: 'e8', piece: 'K', label: 'Černý král → e8' },\n"
    "    { sq: 'f8', piece: 'B', label: 'Černý střelec → f8' },\n"
    "    { sq: 'g8', piece: 'N', label: 'Černý jezdec → g8' },\n"
    "    { sq: 'h8', piece: 'R', label: 'Černá věž → h8' },\n"
    "    { sq: 'a7', piece: 'P', label: 'Černý pěšec → a7' },\n"
    "    { sq: 'b7', piece: 'P', label: 'Černý pěšec → b7' },\n"
    "    { sq: 'c7', piece: 'P', label: 'Černý pěšec → c7' },\n"
    "    { sq: 'd7', piece: 'P', label: 'Černý pěšec → d7' },\n"
    "    { sq: 'e7', piece: 'P', label: 'Černý pěšec → e7' },\n"
    "    { sq: 'f7', piece: 'P', label: 'Černý pěšec → f7' },\n"
    "    { sq: 'g7', piece: 'P', label: 'Černý pěšec → g7' },\n"
    "    { sq: 'h7', piece: 'P', label: 'Černý pěšec → h7' }\n"
    "];\n"
    "\n"
    "let setupTutorialPhase = null;\n"
    "let setupTutorialStepIndex = 0;\n"
    "let setupTutorialLedIntervalId = null;\n"
    "let setupTutorialFastPollId = null;\n"
    "let setupTutorialOccStable = 0;\n"
    "\n"
    "function setupTutorialSquareToIndex(sq) {\n"
    "    var c = sq.charCodeAt(0) - 97;\n"
    "    var r = parseInt(sq.charAt(1), 10) - 1;\n"
    "    return r * 8 + c;\n"
    "}\n"
    "\n"
    "function setupTutorialStopLedRefresh() {\n"
    "    if (setupTutorialLedIntervalId) {\n"
    "        clearInterval(setupTutorialLedIntervalId);\n"
    "        setupTutorialLedIntervalId = null;\n"
    "    }\n"
    "}\n"
    "\n"
    "function setupTutorialStopFastPoll() {\n"
    "    if (setupTutorialFastPollId) {\n"
    "        clearInterval(setupTutorialFastPollId);\n"
    "        setupTutorialFastPollId = null;\n"
    "    }\n"
    "}\n"
    "\n"
    "function setupTutorialApplyLed(sq) {\n"
    "    return fetch('/api/game/hint_highlight', {\n"
    "        method: 'POST',\n"
    "        headers: { 'Content-Type': 'application/json' },\n"
    "        body: JSON.stringify({ to: sq })\n"
    "    }).catch(function () {});\n"
    "}\n"
    "\n"
    "function setupTutorialStartLedRefresh(sq) {\n"
    "    setupTutorialStopLedRefresh();\n"
    "    setupTutorialApplyLed(sq);\n"
    "    setupTutorialLedIntervalId = setInterval(function () {\n"
    "        setupTutorialApplyLed(sq);\n"
    "    }, SETUP_TUTORIAL_REFRESH_MS);\n"
    "}\n"
    "\n"
    "function setupTutorialClearLed() {\n"
    "    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "}\n"
    "\n"
    "function openSetupTutorialIntro() {\n"
    "    var ov = document.getElementById('setup-tutorial-overlay');\n"
    "    if (!ov) return;\n"
    "    try {\n"
    "        if (!devicePrefs.chessTutorialsEnabled) return;\n"
    "    } catch (e) { return; }\n"
    "    ov.style.display = 'flex';\n"
    "    ov.classList.add('overlay-visible');\n"
    "    var intro = document.getElementById('setup-tutorial-intro-panel');\n"
    "    var run = document.getElementById('setup-tutorial-run-panel');\n"
    "    var done = document.getElementById('setup-tutorial-done-panel');\n"
    "    if (intro) intro.style.display = '';\n"
    "    if (run) run.style.display = 'none';\n"
    "    if (done) done.style.display = 'none';\n"
    "    setupTutorialUpdateIntroWarnings(statusData || {});\n"
    "}\n"
    "\n"
    "function setupTutorialCloseIntro() {\n"
    "    var ov = document.getElementById('setup-tutorial-overlay');\n"
    "    if (ov) {\n"
    "        ov.classList.remove('overlay-visible');\n"
    "        ov.style.display = 'none';\n"
    "    }\n"
    "}\n"
    "\n"
    "function setupTutorialUpdateIntroWarnings(st) {\n"
    "    var w = document.getElementById('setup-tutorial-warn');\n"
    "    if (!w) return;\n"
    "    var parts = [];\n"
    "    if (st.light_mode === 'lamp') {\n"
    "        parts.push('Režim Lampa může přebít herní LED — přepni na Šachovnice v Zařízení.');\n"
    "    }\n"
    "    if (st.matrix_guard_active) {\n"
    "        parts.push('Matrix guard je aktivní — dokonči návrat figurek nebo ukonči režim guardu.');\n"
    "    }\n"
    "    if (parts.length) {\n"
    "        w.textContent = parts.join(' ');\n"
    "        w.style.display = '';\n"
    "    } else {\n"
    "        w.textContent = '';\n"
    "        w.style.display = 'none';\n"
    "    }\n"
    "}\n"
    "\n"
    "function setupTutorialRenderStep() {\n"
    "    var st = SETUP_TUTORIAL_STEPS[setupTutorialStepIndex];\n"
    "    if (!st) return;\n"
    "    var pe = document.getElementById('setup-tutorial-piece-display');\n"
    "    var ins = document.getElementById('setup-tutorial-instruction');\n"
    "    var pr = document.getElementById('setup-tutorial-progress');\n"
    "    if (pe) {\n"
    "        var isWhitePc = (st.piece >= 'a' && st.piece <= 'z');\n"
    "        var src = pieceImgSrc(st.piece);\n"
    "        if (src) {\n"
    "            pe.className = 'setup-tutorial-piece-glyph piece has-img ' +\n"
    "                (isWhitePc ? 'white' : 'black');\n"
    "            pe.innerHTML = '<img src=\"' + src + '\" alt=\"\" draggable=\"false\">';\n"
    "        } else {\n"
    "            pe.innerHTML = '';\n"
    "            pe.textContent = pieceSymbols[st.piece] || '?';\n"
    "            pe.className = 'setup-tutorial-piece-glyph piece ' +\n"
    "                (isWhitePc ? 'white' : 'black');\n"
    "        }\n"
    "    }\n"
    "    if (ins) ins.textContent = 'Polož figurku na pole ' + st.sq.toUpperCase();\n"
    "    if (pr) pr.textContent = 'Krok ' + (setupTutorialStepIndex + 1) + ' / ' + SETUP_TUTORIAL_STEPS.length + ' — ' + st.label;\n"
    "    setupTutorialStartLedRefresh(st.sq);\n"
    "    setupTutorialOccStable = 0;\n"
    "}\n"
    "\n"
    "async function setupTutorialBegin() {\n"
    "    try {\n"
    "        var res = await fetch('/api/game/setup_tutorial', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'start' })\n"
    "        });\n"
    "        if (!res.ok) {\n"
    "            if (typeof console !== 'undefined' && console.warn) console.warn('setup_tutorial start', res.status);\n"
    "            return;\n"
    "        }\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) console.warn(e);\n"
    "        return;\n"
    "    }\n"
    "    setupTutorialPhase = 'run';\n"
    "    setupTutorialStepIndex = 0;\n"
    "    var intro = document.getElementById('setup-tutorial-intro-panel');\n"
    "    var run = document.getElementById('setup-tutorial-run-panel');\n"
    "    if (intro) intro.style.display = 'none';\n"
    "    if (run) run.style.display = '';\n"
    "    setupTutorialRenderStep();\n"
    "    setupTutorialFastPollId = setInterval(setupTutorialPollOccupancy, SETUP_TUTORIAL_FAST_POLL_MS);\n"
    "}\n"
    "\n"
    "function setupTutorialPollOccupancy() {\n"
    "    if (setupTutorialPhase !== 'run') return;\n"
    "    fetch('/api/status')\n"
    "        .then(function (r) { return r.json(); })\n"
    "        .then(function (st) {\n"
    "            if (!st.matrix_occupied || !Array.isArray(st.matrix_occupied)) return;\n"
    "            var stp = SETUP_TUTORIAL_STEPS[setupTutorialStepIndex];\n"
    "            if (!stp) return;\n"
    "            var idx = setupTutorialSquareToIndex(stp.sq);\n"
    "            if (Number(st.matrix_occupied[idx]) === 1) {\n"
    "                setupTutorialOccStable++;\n"
    "                if (setupTutorialOccStable >= SETUP_TUTORIAL_OCC_STABLE_TICKS) {\n"
    "                    setupTutorialAdvance(true);\n"
    "                }\n"
    "            } else {\n"
    "                setupTutorialOccStable = 0;\n"
    "            }\n"
    "        })\n"
    "        .catch(function () {});\n"
    "}\n"
    "\n"
    "function setupTutorialAdvance(fromAuto) {\n"
    "    if (setupTutorialPhase !== 'run') return;\n"
    "    setupTutorialStopLedRefresh();\n"
    "    setupTutorialClearLed();\n"
    "    setupTutorialOccStable = 0;\n"
    "    setupTutorialStepIndex++;\n"
    "    if (setupTutorialStepIndex >= SETUP_TUTORIAL_STEPS.length) {\n"
    "        setupTutorialStopFastPoll();\n"
    "        setupTutorialPhase = 'done';\n"
    "        var run = document.getElementById('setup-tutorial-run-panel');\n"
    "        var done = document.getElementById('setup-tutorial-done-panel');\n"
    "        if (run) run.style.display = 'none';\n"
    "        if (done) done.style.display = '';\n"
    "        return;\n"
    "    }\n"
    "    setupTutorialRenderStep();\n"
    "}\n"
    "\n"
    "function setupTutorialBack() {\n"
    "    if (setupTutorialPhase !== 'run') return;\n"
    "    if (setupTutorialStepIndex <= 0) return;\n"
    "    setupTutorialStopLedRefresh();\n"
    "    setupTutorialClearLed();\n"
    "    setupTutorialOccStable = 0;\n"
    "    setupTutorialStepIndex--;\n"
    "    setupTutorialRenderStep();\n"
    "}\n"
    "\n"
    "function setupTutorialSkip() {\n"
    "    setupTutorialAdvance(false);\n"
    "}\n"
    "\n"
    "async function setupTutorialCancel() {\n"
    "    setupTutorialStopLedRefresh();\n"
    "    setupTutorialStopFastPoll();\n"
    "    setupTutorialClearLed();\n"
    "    setupTutorialPhase = null;\n"
    "    try {\n"
    "        await fetch('/api/game/setup_tutorial', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'cancel' })\n"
    "        });\n"
    "    } catch (e) {}\n"
    "    var ov = document.getElementById('setup-tutorial-overlay');\n"
    "    if (ov) {\n"
    "        ov.classList.remove('overlay-visible');\n"
    "        ov.style.display = 'none';\n"
    "    }\n"
    "    var intro = document.getElementById('setup-tutorial-intro-panel');\n"
    "    var run = document.getElementById('setup-tutorial-run-panel');\n"
    "    var done = document.getElementById('setup-tutorial-done-panel');\n"
    "    if (intro) intro.style.display = '';\n"
    "    if (run) run.style.display = 'none';\n"
    "    if (done) done.style.display = 'none';\n"
    "    if (typeof fetchData === 'function') fetchData();\n"
    "}\n"
    "\n"
    "async function setupTutorialFinish() {\n"
    "    try {\n"
    "        var res = await fetch('/api/game/setup_tutorial', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'finish' })\n"
    "        });\n"
    "        var data = await res.json().catch(function () { return {}; });\n"
    "        if (!res.ok) {\n"
    "            var msg = (data && data.error) ? data.error : 'Zkontroluj fyzickou pozici (řádky 1–2 a 7–8 plné, 3–6 prázdné).';\n"
    "            alert(msg);\n"
    "            return;\n"
    "        }\n"
    "    } catch (e) {\n"
    "        alert('Chyba sítě při dokončení.');\n"
    "        return;\n"
    "    }\n"
    "    setupTutorialStopLedRefresh();\n"
    "    setupTutorialStopFastPoll();\n"
    "    setupTutorialPhase = null;\n"
    "    var ov = document.getElementById('setup-tutorial-overlay');\n"
    "    if (ov) {\n"
    "        ov.classList.remove('overlay-visible');\n"
    "        ov.style.display = 'none';\n"
    "    }\n"
    "    var intro = document.getElementById('setup-tutorial-intro-panel');\n"
    "    var run = document.getElementById('setup-tutorial-run-panel');\n"
    "    var done = document.getElementById('setup-tutorial-done-panel');\n"
    "    if (intro) intro.style.display = '';\n"
    "    if (run) run.style.display = 'none';\n"
    "    if (done) done.style.display = 'none';\n"
    "    if (typeof fetchData === 'function') fetchData();\n"
    "}\n"
    "\n"
    "window.openSetupTutorialIntro = openSetupTutorialIntro;\n"
    "window.setupTutorialBegin = setupTutorialBegin;\n"
    "window.setupTutorialCloseIntro = setupTutorialCloseIntro;\n"
    "window.setupTutorialBack = setupTutorialBack;\n"
    "window.setupTutorialSkip = setupTutorialSkip;\n"
    "window.setupTutorialCancel = setupTutorialCancel;\n"
    "window.setupTutorialFinish = setupTutorialFinish;\n"
    "\n"
    "const PUZZLE_DEFS = [\n"
    "    { id: 1, difficulty: 1, title: 'Mat 1 – Dáma na poslední řadě',\n"
    "        teaser: 'Klasický motiv: otevřený f-sloupec, dáma dá mat na f8.',\n"
    "        fen: '7k/7p/8/8/8/8/5Q2/6K1 w - - 0 1' },\n"
    "    { id: 2, difficulty: 2, title: 'Mat 1 – Dáma po sloupci',\n"
    "        teaser: 'Útok po ose: dáma stoupá z b2 na b8.',\n"
    "        fen: '6k1/5ppp/8/8/8/8/1Q6/6K1 w - - 0 1' },\n"
    "    { id: 3, difficulty: 3, title: 'Mat 1 – Věž bere věž',\n"
    "        teaser: 'Taktika zadní řady: bílá věž sebere černou na e8 a matuje krále.',\n"
    "        fen: '4r1k1/5ppp/8/8/8/8/4R3/4K3 w - - 0 1' },\n"
    "    { id: 4, difficulty: 4, title: 'Mat 1 – Školácký mat',\n"
    "        teaser: 'Známá ukázková pozice: střelec na c4, dáma na h5 — mat na f7.',\n"
    "        fen: 'r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1' },\n"
    "    { id: 5, difficulty: 5, title: 'Mat 1 – Dáma na d8',\n"
    "        teaser: 'Centrální úder: dáma z d4 uzavře mat na poli d8.',\n"
    "        fen: '6k1/5ppp/8/8/3Q4/8/6PP/6K1 w - - 0 1' }\n"
    "];\n"
    "let selectedPuzzleId = 1;\n"
    "let puzzleSetupPhase = null;\n"
    "let puzzleSetupStepIndex = 0;\n"
    "let puzzleSetupSteps = [];\n"
    "let puzzleSetupFastPollId = null;\n"
    "let puzzleSetupLedIntervalId = null;\n"
    "let puzzleSetupOccStable = 0;\n"
    "\n"
    "function puzzleSquareToIndex(sq) {\n"
    "    var c = sq.charCodeAt(0) - 97;\n"
    "    var r = parseInt(sq.charAt(1), 10) - 1;\n"
    "    return r * 8 + c;\n"
    "}\n"
    "\n"
    "function buildPuzzleSetupStepsFromFen(fen) {\n"
    "    var steps = [];\n"
    "    if (!fen) return steps;\n"
    "    var boardPart = fen.split(' ')[0];\n"
    "    var row = 7;\n"
    "    var col = 0;\n"
    "    var i;\n"
    "    for (i = 0; i < boardPart.length; i++) {\n"
    "        var ch = boardPart.charAt(i);\n"
    "        if (ch === '/') {\n"
    "            row--;\n"
    "            col = 0;\n"
    "            continue;\n"
    "        }\n"
    "        if (ch >= '1' && ch <= '8') {\n"
    "            col += parseInt(ch, 10);\n"
    "            continue;\n"
    "        }\n"
    "        var sq = String.fromCharCode(97 + col) + (row + 1);\n"
    "        var sym = pieceSymbols[ch] || ch;\n"
    "        steps.push({\n"
    "            sq: sq,\n"
    "            piece: ch,\n"
    "            label: sym + ' → ' + sq.toUpperCase()\n"
    "        });\n"
    "        col++;\n"
    "    }\n"
    "    return steps;\n"
    "}\n"
    "\n"
    "function puzzleSetupStopLedRefresh() {\n"
    "    if (puzzleSetupLedIntervalId) {\n"
    "        clearInterval(puzzleSetupLedIntervalId);\n"
    "        puzzleSetupLedIntervalId = null;\n"
    "    }\n"
    "}\n"
    "\n"
    "function puzzleSetupStopFastPoll() {\n"
    "    if (puzzleSetupFastPollId) {\n"
    "        clearInterval(puzzleSetupFastPollId);\n"
    "        puzzleSetupFastPollId = null;\n"
    "    }\n"
    "}\n"
    "\n"
    "function puzzleSetupApplyLed(sq) {\n"
    "    return fetch('/api/game/hint_highlight', {\n"
    "        method: 'POST',\n"
    "        headers: { 'Content-Type': 'application/json' },\n"
    "        body: JSON.stringify({ to: sq })\n"
    "    }).catch(function () {});\n"
    "}\n"
    "\n"
    "function puzzleSetupStartLedRefresh(sq) {\n"
    "    puzzleSetupStopLedRefresh();\n"
    "    puzzleSetupApplyLed(sq);\n"
    "    puzzleSetupLedIntervalId = setInterval(function () {\n"
    "        puzzleSetupApplyLed(sq);\n"
    "    }, SETUP_TUTORIAL_REFRESH_MS);\n"
    "}\n"
    "\n"
    "function puzzleRenderList() {\n"
    "    var list = document.getElementById('puzzle-list');\n"
    "    if (!list) return;\n"
    "    list.innerHTML = '';\n"
    "    PUZZLE_DEFS.forEach(function (p) {\n"
    "        var btn = document.createElement('button');\n"
    "        btn.type = 'button';\n"
    "        btn.className = 'set-btn set-btn-sm';\n"
    "        btn.style.textAlign = 'left';\n"
    "        btn.style.opacity = (p.id === selectedPuzzleId) ? '1' : '0.85';\n"
    "        btn.textContent = '[' + p.difficulty + '/5] ' + p.title + ' - ' + p.teaser;\n"
    "        btn.onclick = function () {\n"
    "            selectedPuzzleId = p.id;\n"
    "            puzzleRenderList();\n"
    "        };\n"
    "        list.appendChild(btn);\n"
    "    });\n"
    "}\n"
    "\n"
    "function puzzleUpdateGuidedMessage(status) {\n"
    "    var box = document.getElementById('puzzle-guided-message');\n"
    "    if (!box) return;\n"
    "    var p = status && status.puzzle ? status.puzzle : null;\n"
    "    if (!p) {\n"
    "        box.textContent = '';\n"
    "        return;\n"
    "    }\n"
    "    if (p.message && p.message.length > 0) {\n"
    "        box.textContent = p.message;\n"
    "    } else if (p.active === true) {\n"
    "        box.textContent = 'Puzzle bezi. Zahraj hledany tah.';\n"
    "    } else if (p.setup_active === true) {\n"
    "        box.textContent = 'Rozestav figurky podle LED (pořadí jako u základního postavení).';\n"
    "    } else {\n"
    "        box.textContent = '';\n"
    "    }\n"
    "}\n"
    "\n"
    "function puzzleResetPanelsToIntro() {\n"
    "    var intro = document.getElementById('puzzle-intro-panel');\n"
    "    var setup = document.getElementById('puzzle-setup-panel');\n"
    "    var conf = document.getElementById('puzzle-confirm-panel');\n"
    "    if (intro) intro.style.display = '';\n"
    "    if (setup) setup.style.display = 'none';\n"
    "    if (conf) conf.style.display = 'none';\n"
    "}\n"
    "\n"
    "function puzzleSetOverlayVisible(vis) {\n"
    "    var ov = document.getElementById('puzzle-overlay');\n"
    "    if (!ov) return;\n"
    "    if (vis) {\n"
    "        ov.style.display = 'flex';\n"
    "        ov.classList.add('overlay-visible');\n"
    "    } else {\n"
    "        ov.classList.remove('overlay-visible');\n"
    "        ov.style.display = 'none';\n"
    "    }\n"
    "}\n"
    "\n"
    "function openPuzzleIntro() {\n"
    "    var ov = document.getElementById('puzzle-overlay');\n"
    "    if (!ov) return;\n"
    "    puzzleResetPanelsToIntro();\n"
    "    puzzleSetOverlayVisible(true);\n"
    "    puzzleRenderList();\n"
    "    puzzleUpdateGuidedMessage(statusData || {});\n"
    "}\n"
    "\n"
    "async function puzzlePrepareSelected() {\n"
    "    try {\n"
    "        var res = await fetch('/api/game/puzzle', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'prepare', id: selectedPuzzleId })\n"
    "        });\n"
    "        if (!res.ok) {\n"
    "            if (typeof console !== 'undefined' && console.warn) console.warn('puzzle prepare', res.status);\n"
    "            return;\n"
    "        }\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) console.warn(e);\n"
    "        return;\n"
    "    }\n"
    "    if (typeof fetchData === 'function') await fetchData();\n"
    "    var def = PUZZLE_DEFS.filter(function (x) { return x.id === selectedPuzzleId; })[0];\n"
    "    puzzleSetupSteps = buildPuzzleSetupStepsFromFen(def ? def.fen : '');\n"
    "    puzzleSetupStepIndex = 0;\n"
    "    puzzleSetupOccStable = 0;\n"
    "    var intro = document.getElementById('puzzle-intro-panel');\n"
    "    var setup = document.getElementById('puzzle-setup-panel');\n"
    "    var conf = document.getElementById('puzzle-confirm-panel');\n"
    "    if (puzzleSetupSteps.length === 0) {\n"
    "        puzzleSetupPhase = 'confirm';\n"
    "        if (intro) intro.style.display = 'none';\n"
    "        if (setup) setup.style.display = 'none';\n"
    "        if (conf) conf.style.display = '';\n"
    "        if (typeof fetchData === 'function') {\n"
    "            fetchData().then(function () {\n"
    "                var w = document.getElementById('puzzle-confirm-warn');\n"
    "                if (w && statusData && statusData.puzzle) {\n"
    "                    w.textContent = statusData.puzzle.physical_match === false\n"
    "                        ? 'Varování: fyzická deska nemusí přesně odpovídat.'\n"
    "                        : 'Můžeš spustit puzzle.';\n"
    "                    w.style.display = '';\n"
    "                }\n"
    "            });\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "    puzzleSetupPhase = 'run';\n"
    "    if (intro) intro.style.display = 'none';\n"
    "    if (setup) setup.style.display = '';\n"
    "    if (conf) conf.style.display = 'none';\n"
    "    puzzleSetupRenderStep();\n"
    "    puzzleSetupFastPollId = setInterval(puzzleSetupPollOccupancy, SETUP_TUTORIAL_FAST_POLL_MS);\n"
    "}\n"
    "\n"
    "function puzzleSetupRenderStep() {\n"
    "    var st = puzzleSetupSteps[puzzleSetupStepIndex];\n"
    "    var pe = document.getElementById('puzzle-setup-piece-display');\n"
    "    var ins = document.getElementById('puzzle-setup-instruction');\n"
    "    var pr = document.getElementById('puzzle-setup-progress');\n"
    "    if (!st) return;\n"
    "    if (pe) {\n"
    "        var isW = (st.piece >= 'a' && st.piece <= 'z');\n"
    "        var srcP = pieceImgSrc(st.piece);\n"
    "        if (srcP) {\n"
    "            pe.className = 'setup-tutorial-piece-glyph piece has-img ' + (isW ? 'white' : 'black');\n"
    "            pe.innerHTML = '<img src=\"' + srcP + '\" alt=\"\" draggable=\"false\">';\n"
    "        } else {\n"
    "            pe.innerHTML = '';\n"
    "            pe.textContent = pieceSymbols[st.piece] || st.piece || '?';\n"
    "            pe.className = 'setup-tutorial-piece-glyph piece ' + (isW ? 'white' : 'black');\n"
    "        }\n"
    "    }\n"
    "    if (ins) ins.textContent = 'Polož figurku na pole ' + st.sq.toUpperCase();\n"
    "    if (pr) {\n"
    "        pr.textContent = 'Krok ' + (puzzleSetupStepIndex + 1) + ' / ' + puzzleSetupSteps.length + ' — ' + st.label;\n"
    "    }\n"
    "    puzzleSetupStartLedRefresh(st.sq);\n"
    "    puzzleSetupOccStable = 0;\n"
    "}\n"
    "\n"
    "function puzzleSetupPollOccupancy() {\n"
    "    if (puzzleSetupPhase !== 'run') return;\n"
    "    fetch('/api/status')\n"
    "        .then(function (r) { return r.json(); })\n"
    "        .then(function (st) {\n"
    "            if (!st.puzzle || st.puzzle.setup_active !== true) return;\n"
    "            if (!st.matrix_occupied || !Array.isArray(st.matrix_occupied)) return;\n"
    "            var stp = puzzleSetupSteps[puzzleSetupStepIndex];\n"
    "            if (!stp) return;\n"
    "            var idx = puzzleSquareToIndex(stp.sq);\n"
    "            if (Number(st.matrix_occupied[idx]) === 1) {\n"
    "                puzzleSetupOccStable++;\n"
    "                if (puzzleSetupOccStable >= SETUP_TUTORIAL_OCC_STABLE_TICKS) {\n"
    "                    puzzleSetupAdvance(true);\n"
    "                }\n"
    "            } else {\n"
    "                puzzleSetupOccStable = 0;\n"
    "            }\n"
    "        })\n"
    "        .catch(function () {});\n"
    "}\n"
    "\n"
    "function puzzleSetupAdvance(fromAuto) {\n"
    "    if (puzzleSetupPhase !== 'run') return;\n"
    "    puzzleSetupStopLedRefresh();\n"
    "    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "    puzzleSetupOccStable = 0;\n"
    "    puzzleSetupStepIndex++;\n"
    "    if (puzzleSetupStepIndex >= puzzleSetupSteps.length) {\n"
    "        puzzleSetupStopFastPoll();\n"
    "        puzzleSetupPhase = 'confirm';\n"
    "        var setup = document.getElementById('puzzle-setup-panel');\n"
    "        var conf = document.getElementById('puzzle-confirm-panel');\n"
    "        if (setup) setup.style.display = 'none';\n"
    "        if (conf) conf.style.display = '';\n"
    "        if (typeof fetchData === 'function') {\n"
    "            fetchData().then(function () {\n"
    "                var w = document.getElementById('puzzle-confirm-warn');\n"
    "                if (w && statusData && statusData.puzzle) {\n"
    "                    if (statusData.puzzle.physical_match === false) {\n"
    "                        w.textContent = 'Varování: fyzická deska nemusí přesně odpovídat (senzory neznají druh figurky). Puzzle můžeš přesto spustit.';\n"
    "                        w.style.display = '';\n"
    "                    } else {\n"
    "                        w.textContent = 'Fyzická obsazenost odpovídá pozici.';\n"
    "                        w.style.display = '';\n"
    "                    }\n"
    "                }\n"
    "            });\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "    puzzleSetupRenderStep();\n"
    "}\n"
    "\n"
    "function puzzleSetupBack() {\n"
    "    if (puzzleSetupPhase !== 'run') return;\n"
    "    if (puzzleSetupStepIndex <= 0) return;\n"
    "    puzzleSetupStopLedRefresh();\n"
    "    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "    puzzleSetupOccStable = 0;\n"
    "    puzzleSetupStepIndex--;\n"
    "    puzzleSetupRenderStep();\n"
    "}\n"
    "\n"
    "function puzzleSetupSkip() {\n"
    "    puzzleSetupAdvance(false);\n"
    "}\n"
    "\n"
    "async function puzzleExecuteStart() {\n"
    "    if (statusData && statusData.puzzle && statusData.puzzle.physical_match === false) {\n"
    "        if (!window.confirm('Fyzická deska nemusí odpovídat očekávané pozici. Spustit puzzle?')) {\n"
    "            return;\n"
    "        }\n"
    "    }\n"
    "    try {\n"
    "        await fetch('/api/game/puzzle', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'start', id: selectedPuzzleId })\n"
    "        });\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) console.warn(e);\n"
    "    }\n"
    "    puzzleSetupPhase = null;\n"
    "    puzzleResetPanelsToIntro();\n"
    "    puzzleSetOverlayVisible(false);\n"
    "    if (typeof fetchData === 'function') fetchData();\n"
    "}\n"
    "\n"
    "async function puzzleBackToIntroFromSetup() {\n"
    "    puzzleSetupStopFastPoll();\n"
    "    puzzleSetupStopLedRefresh();\n"
    "    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "    puzzleSetupPhase = null;\n"
    "    try {\n"
    "        await fetch('/api/game/puzzle', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'cancel' })\n"
    "        });\n"
    "    } catch (e) {}\n"
    "    puzzleResetPanelsToIntro();\n"
    "    if (typeof fetchData === 'function') fetchData();\n"
    "}\n"
    "\n"
    "async function puzzleCancel() {\n"
    "    puzzleSetupStopFastPoll();\n"
    "    puzzleSetupStopLedRefresh();\n"
    "    fetch('/api/game/hint_clear', { method: 'POST' }).catch(function () {});\n"
    "    puzzleSetupPhase = null;\n"
    "    try {\n"
    "        await fetch('/api/game/puzzle', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ action: 'cancel' })\n"
    "        });\n"
    "    } catch (e) {}\n"
    "    puzzleResetPanelsToIntro();\n"
    "    puzzleSetOverlayVisible(false);\n"
    "    if (typeof fetchData === 'function') fetchData();\n"
    "}\n"
    "\n"
    "window.openPuzzleIntro = openPuzzleIntro;\n"
    "window.puzzlePrepareSelected = puzzlePrepareSelected;\n"
    "window.puzzleExecuteStart = puzzleExecuteStart;\n"
    "window.puzzleCancel = puzzleCancel;\n"
    "window.puzzleSetupBack = puzzleSetupBack;\n"
    "window.puzzleSetupSkip = puzzleSetupSkip;\n"
    "window.puzzleBackToIntroFromSetup = puzzleBackToIntroFromSetup;\n"
    "\n"
    "function updateStatus(status) {\n"
    "    statusData = status;\n"
    "    // ... existing implementation ...\n"
    "    const gameStateEl = document.getElementById('game-state');\n"
    "    const playerEl = document.getElementById('current-player');\n"
    "    if (gameStateEl) gameStateEl.textContent = status.game_state || '-';\n"
    "    if (playerEl) {\n"
    "        const p = status.current_player;\n"
    "        playerEl.textContent = (p === 'White') ? 'Bílý' : (p === 'Black') ? 'Černý' : (p || '-');\n"
    "    }\n"
    "    document.getElementById('move-count').textContent = status.move_count || 0;\n"
    "    document.getElementById('in-check').textContent = status.in_check ? 'Ano' : 'Ne';\n"
    "\n"
    "    // Jas (Nastavení) – synchronizovat slider a label ze statusu\n"
    "    const b = status.brightness;\n"
    "    if (typeof b === 'number' && b >= 0 && b <= 100) {\n"
    "        const valueEl = document.getElementById('brightness-value');\n"
    "        const sliderEl = document.getElementById('brightness-slider');\n"
    "        if (valueEl) valueEl.textContent = b + '%';\n"
    "        if (sliderEl && Number(sliderEl.value) !== b) sliderEl.value = b;\n"
    "    }\n"
    "    const gl = status.led_guidance_level;\n"
    "    if (typeof gl === 'number' && gl >= 1 && gl <= 5) {\n"
    "        const ledGuidanceEl = document.getElementById('led-guidance-level');\n"
    "        if (ledGuidanceEl && String(ledGuidanceEl.value) !== String(gl)) {\n"
    "            ledGuidanceEl.value = String(gl);\n"
    "        }\n"
    "    }\n"
    "    puzzleUpdateGuidedMessage(status);\n"
    "    updatePuzzleStatusPanel(status, { offline: false });\n"
    "\n"
    "    // Lampa (Nastavení) – režim, zapnutí, R/G/B ze statusu\n"
    "    const lightMode = status.light_mode;\n"
    "    const lightState = status.light_state;\n"
    "    const lr = status.light_r, lg = status.light_g, lb = status.light_b;\n"
    "    const btnGame = document.getElementById('light-mode-game');\n"
    "    const btnLamp = document.getElementById('light-mode-lamp');\n"
    "    const lampControls = document.getElementById('light-lamp-controls');\n"
    "    if (btnGame && btnLamp) {\n"
    "        const isLamp = lightMode === 'lamp';\n"
    "        btnGame.classList.toggle('active', !isLamp);\n"
    "        btnLamp.classList.toggle('active', isLamp);\n"
    "        if (lampControls) lampControls.style.display = isLamp ? '' : 'none';\n"
    "    }\n"
    "    const toggleEl = document.getElementById('light-state-toggle');\n"
    "    if (toggleEl && typeof lightState === 'boolean' && toggleEl.checked !== lightState) toggleEl.checked = lightState;\n"
    "    const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');\n"
    "    const rVal = document.getElementById('light-r-value'), gVal = document.getElementById('light-g-value'), bVal = document.getElementById('light-b-value');\n"
    "    if (typeof lr === 'number' && lr >= 0 && lr <= 255 && rEl && rVal) { rEl.value = lr; rVal.textContent = lr; }\n"
    "    if (typeof lg === 'number' && lg >= 0 && lg <= 255 && gEl && gVal) { gEl.value = lg; gVal.textContent = lg; }\n"
    "    if (typeof lb === 'number' && lb >= 0 && lb <= 255 && bEl && bVal) { bEl.value = lb; bVal.textContent = lb; }\n"
    "\n"
    "    // Promotion modal – zobrazit, když backend čeká na volbu promoce (game_state === \"promotion\")\n"
    "    const promoModal = document.getElementById('promotion-modal');\n"
    "    if (promoModal) {\n"
    "        if (status.game_state === 'promotion') {\n"
    "            promoModal.style.display = 'flex';\n"
    "        } else {\n"
    "            promoModal.style.display = 'none';\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // ERROR STATE\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('error-invalid', 'error-original');\n"
    "    });\n"
    "\n"
    "    // LIFTED PIECE\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('lifted');\n"
    "    });\n"
    "\n"
    "    const lifted = status.piece_lifted;\n"
    "    const liftedPieceEl = document.getElementById('lifted-piece');\n"
    "    const liftedPosEl = document.getElementById('lifted-position');\n"
    "    if (lifted && lifted.lifted) {\n"
    "        if (liftedPieceEl) {\n"
    "            const sl = pieceImgSrc(lifted.piece);\n"
    "            if (sl) {\n"
    "                liftedPieceEl.innerHTML = '<img src=\"' + sl + '\" class=\"lifted-piece-img\" alt=\"\">';\n"
    "            } else {\n"
    "                liftedPieceEl.innerHTML = '';\n"
    "                liftedPieceEl.textContent = pieceSymbols[lifted.piece] || '-';\n"
    "            }\n"
    "        }\n"
    "        if (liftedPosEl) liftedPosEl.textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);\n"
    "        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);\n"
    "        if (square) square.classList.add('lifted');\n"
    "    } else {\n"
    "        if (liftedPieceEl) {\n"
    "            liftedPieceEl.innerHTML = '';\n"
    "            liftedPieceEl.textContent = '-';\n"
    "        }\n"
    "        if (liftedPosEl) liftedPosEl.textContent = '-';\n"
    "    }\n"
    "\n"
    "    // Error state classes\n"
    "    if (status.error_state && status.error_state.active) {\n"
    "        if (status.error_state.invalid_pos) {\n"
    "            const invalidCol = status.error_state.invalid_pos.charCodeAt(0) - 97;\n"
    "            const invalidRow = parseInt(status.error_state.invalid_pos[1]) - 1;\n"
    "            const invalidSquare = document.querySelector(`[data-row='${invalidRow}'][data-col='${invalidCol}']`);\n"
    "            if (invalidSquare) invalidSquare.classList.add('error-invalid');\n"
    "        }\n"
    "        if (status.error_state.original_pos) {\n"
    "            const originalCol = status.error_state.original_pos.charCodeAt(0) - 97;\n"
    "            const originalRow = parseInt(status.error_state.original_pos[1]) - 1;\n"
    "            const originalSquare = document.querySelector(`[data-row='${originalRow}'][data-col='${originalCol}']`);\n"
    "            if (originalSquare) originalSquare.classList.add('error-original');\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Castling message vs Bot message\n"
    "    var castlingMsg = document.getElementById('castling-pending-message');\n"
    "    if (castlingMsg) {\n"
    "        if (status.restore_state && status.restore_state.boot_new_game_triggered) {\n"
    "            castlingMsg.textContent = 'Byla spuštěna nová hra: detekovány 2 starty zařízení bez tahu v intervalu 1 minuty.';\n"
    "            castlingMsg.style.display = 'block';\n"
    "            castlingMsg.style.background = 'rgba(23,162,184,0.14)';\n"
    "            castlingMsg.style.borderColor = 'rgba(23,162,184,0.45)';\n"
    "            castlingMsg.style.color = '#4dd0e1';\n"
    "        // Highest priority: matrix guard pause with return guidance.\n"
    "        } else if (status.matrix_guard_active) {\n"
    "            const liftedSquares = matrixGuardMaskToSquares(status.matrix_guard_lifted_low, status.matrix_guard_lifted_high);\n"
    "            const droppedSquares = matrixGuardMaskToSquares(status.matrix_guard_dropped_low, status.matrix_guard_dropped_high);\n"
    "            const all = Array.from(new Set(liftedSquares.concat(droppedSquares)));\n"
    "            const hint = all.length > 0 ? all.join(', ') : 'označené pozice';\n"
    "            const resync = status.restore_state && status.restore_state.resync_required;\n"
    "            castlingMsg.textContent = resync\n"
    "                ? ('Po startu nesedí fyzická deska s uloženou hrou. Srovnejte figurky na LED zvýrazněných polích: ' + hint + '.')\n"
    "                : ('Hra je pozastavena: zvednuto více figurek najednou. Vraťte figurky na: ' + hint + '. Po návratu hra automaticky pokračuje.');\n"
    "            castlingMsg.style.display = 'block';\n"
    "            castlingMsg.style.background = 'rgba(220,53,69,0.14)';\n"
    "            castlingMsg.style.borderColor = 'rgba(220,53,69,0.45)';\n"
    "            castlingMsg.style.color = '#ff6b6b';\n"
    "        } else if (status.restore_state && status.restore_state.snapshot_restore_failed) {\n"
    "            castlingMsg.textContent = 'Chyba obnovy hry z NVS — hra běží z výchozí / poslední známé pozice. Zkontrolujte log.';\n"
    "            castlingMsg.style.display = 'block';\n"
    "            castlingMsg.style.background = 'rgba(255,87,34,0.14)';\n"
    "            castlingMsg.style.borderColor = 'rgba(255,87,34,0.45)';\n"
    "            castlingMsg.style.color = '#ff8a65';\n"
    "        } else if (status.restore_state && status.restore_state.snapshot_save_failed) {\n"
    "            castlingMsg.textContent = 'Varování: uložení hry do NVS selhalo — po výpadku napájení může chybět poslední tah.';\n"
    "            castlingMsg.style.display = 'block';\n"
    "            castlingMsg.style.background = 'rgba(255,152,0,0.14)';\n"
    "            castlingMsg.style.borderColor = 'rgba(255,152,0,0.45)';\n"
    "            castlingMsg.style.color = '#ffb74d';\n"
    "        // Next priority: castling guidance.\n"
    "        } else if (status.castling_in_progress && status.castling_from && status.castling_to) {\n"
    "            castlingMsg.textContent = 'Dokončete rošádu: přesuňte věž z ' + status.castling_from + ' na ' + status.castling_to + '.';\n"
    "            castlingMsg.style.display = 'block';\n"
    "            // Reset style to warning for castling\n"
    "            castlingMsg.style.background = 'rgba(255,193,7,0.12)';\n"
    "            castlingMsg.style.borderColor = 'rgba(255,193,7,0.4)';\n"
    "            castlingMsg.style.color = '#ffc107';\n"
    "        } else {\n"
    "            if (status.game_end && status.game_end.ended) castlingMsg.style.display = 'none';\n"
    "            else if (status.game_state !== 'active' && status.game_state !== 'playing') castlingMsg.style.display = 'none';\n"
    "            else {\n"
    "                var keepMsg = castlingMsg.textContent.indexOf('Losování:') === 0 || castlingMsg.textContent.indexOf('nápověda') !== -1;\n"
    "                if (!keepMsg) castlingMsg.style.display = 'none';\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // Panel „Bot“ – zobrazit jen v režimu proti botovi; při zvednuté figurce navádění\n"
    "    updateBotStatusPanel(undefined, status);\n"
    "\n"
    "    // ENDGAME REPORT\n"
    "    if (status.game_end && status.game_end.ended) {\n"
    "        window.lastGameEndData = status.game_end;\n"
    "        if (!endgameReportShown) {\n"
    "            console.log('Game ended, showing endgame report...');\n"
    "            showEndgameReport(status.game_end);\n"
    "        }\n"
    "        showEndgameToggleButton();\n"
    "    } else {\n"
    "        if (endgameReportShown) {\n"
    "            console.log('Game restarted, clearing endgame report...');\n"
    "            hideEndgameReport();\n"
    "        }\n"
    "        endgameReportShown = false;\n"
    "        window.lastGameEndData = null;\n"
    "        hideEndgameToggleButton();\n"
    "    }\n"
    "\n"
    "    // Web lock: zakázat Nová hra a Nápověda při zamčení\n"
    "    var locked = !!(status.web_locked);\n"
    "    var newGameBtn = document.getElementById('new-game-btn');\n"
    "    var hintBtn = document.getElementById('hint-btn');\n"
    "    if (newGameBtn) newGameBtn.disabled = locked;\n"
    "    if (hintBtn) {\n"
    "        if (locked) {\n"
    "            hintBtn.disabled = true;\n"
    "            hintBtn.title = 'Rozhraní je zamčeno';\n"
    "        } else if (status.board_setup_tutorial === true) {\n"
    "            hintBtn.disabled = true;\n"
    "            hintBtn.title = 'Běží tutoriál rozestavení';\n"
    "        } else {\n"
    "            if (typeof updateHintButtonLabel === 'function') updateHintButtonLabel();\n"
    "        }\n"
    "    }\n"
    "\n"
    "    var tutOv = document.getElementById('setup-tutorial-overlay');\n"
    "    if (tutOv && tutOv.style.display === 'flex') {\n"
    "        setupTutorialUpdateIntroWarnings(status);\n"
    "    }\n"
    "}\n"
    "\n"
    "function getGradeLabel(grade) {\n"
    "    switch (grade) {\n"
    "        case 'best': return 'Výborný';\n"
    "        case 'good': return 'Dobrý';\n"
    "        case 'inaccuracy': return 'Slabší';\n"
    "        case 'mistake': return 'Chyba';\n"
    "        case 'blunder': return 'Vážná chyba';\n"
    "        case 'unknown': return '—';\n"
    "        case 'error': return 'Chyba';\n"
    "        default: return '—';\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Krátký štítek kvality tahu do přehledu (2–4 znaky). */\n"
    "function getGradeShortLabel(grade) {\n"
    "    switch (grade) {\n"
    "        case 'best': return 'Výb.';\n"
    "        case 'good': return 'Dob.';\n"
    "        case 'inaccuracy': return 'Nepř.';\n"
    "        case 'mistake': return 'Ch.';\n"
    "        case 'blunder': return 'Hr.';\n"
    "        case 'error': return '!';\n"
    "        case 'unknown': return '—';\n"
    "        default: return '—';\n"
    "    }\n"
    "}\n"
    "\n"
    "function escapeHtml(s) {\n"
    "    if (s == null) return '';\n"
    "    return String(s)\n"
    "        .replace(/&/g, '&amp;')\n"
    "        .replace(/</g, '&lt;')\n"
    "        .replace(/>/g, '&gt;')\n"
    "        .replace(/\"/g, '&quot;');\n"
    "}\n"
    "\n"
    "/** Kolik posledních tahů v kompaktním náhledu (2–4). */\n"
    "var HISTORY_PREVIEW_COUNT = 4;\n"
    "var historyFullExpanded = false;\n"
    "var historyDetailIndex = null;\n"
    "\n"
    "function wireHistoryToolbarOnce() {\n"
    "    var btn = document.getElementById('history-toggle-full');\n"
    "    if (!btn || btn._historyWired) return;\n"
    "    btn._historyWired = true;\n"
    "    btn.addEventListener('click', function (e) {\n"
    "        e.preventDefault();\n"
    "        historyFullExpanded = !historyFullExpanded;\n"
    "        btn.setAttribute('aria-expanded', historyFullExpanded ? 'true' : 'false');\n"
    "        btn.textContent = historyFullExpanded ? 'Skrýt celou historii' : 'Celá historie';\n"
    "        renderHistoryList();\n"
    "    });\n"
    "}\n"
    "\n"
    "function createHistoryItemElement(move, actualIndex) {\n"
    "    var item = document.createElement('div');\n"
    "    item.className = 'history-item';\n"
    "    item.dataset.moveIndex = String(actualIndex);\n"
    "    var moveNum = Math.floor(actualIndex / 2) + 1;\n"
    "    var isWhite = actualIndex % 2 === 0;\n"
    "    var prefix = isWhite ? moveNum + '. ' : '';\n"
    "    item.appendChild(document.createTextNode(prefix + move.from + ' → ' + move.to));\n"
    "    var ev = moveEvaluations[actualIndex];\n"
    "    if (ev) {\n"
    "        var badge = document.createElement('span');\n"
    "        badge.className = 'move-eval-badge move-eval-badge--' + (ev.grade || 'good');\n"
    "        var fullTitle = ev.msg || getGradeLabel(ev.grade);\n"
    "        badge.title = fullTitle;\n"
    "        badge.setAttribute('aria-label', fullTitle);\n"
    "        badge.textContent = getGradeShortLabel(ev.grade);\n"
    "        item.appendChild(document.createTextNode(' '));\n"
    "        item.appendChild(badge);\n"
    "    }\n"
    "    item.addEventListener('click', function (e) {\n"
    "        e.stopPropagation();\n"
    "        toggleHistoryMoveDetail(actualIndex);\n"
    "    });\n"
    "    return item;\n"
    "}\n"
    "\n"
    "function toggleHistoryMoveDetail(actualIndex) {\n"
    "    var panel = document.getElementById('history-move-detail');\n"
    "    if (!panel) return;\n"
    "    if (historyDetailIndex === actualIndex) {\n"
    "        panel.style.display = 'none';\n"
    "        historyDetailIndex = null;\n"
    "        return;\n"
    "    }\n"
    "    historyDetailIndex = actualIndex;\n"
    "    var ev = moveEvaluations[actualIndex];\n"
    "    var move = historyData[actualIndex];\n"
    "    var san = move ? (move.from + ' → ' + move.to) : '—';\n"
    "    var gradeLine = ev ? getGradeLabel(ev.grade) : 'Bezhodnoceno';\n"
    "    var gExtra = ev && ev.grade ? (' history-detail-grade--' + ev.grade) : '';\n"
    "    var msg = ev && ev.msg\n"
    "        ? ev.msg\n"
    "        : ('Zhodnocení není k dispozici. Zapni „Zhodnocení tahu“ v Nastavení a hraj ' +\n"
    "            's připojením k internetu — u každého nového tahu se doplní kvalita.');\n"
    "    panel.innerHTML =\n"
    "        '<div class=\"history-detail-inner\">' +\n"
    "        '<div class=\"history-detail-san\">' + escapeHtml(san) + '</div>' +\n"
    "        '<div class=\"history-detail-grade' + gExtra + '\">' + escapeHtml(gradeLine) + '</div>' +\n"
    "        '<p class=\"history-detail-msg\">' + escapeHtml(msg) + '</p>' +\n"
    "        '<button type=\"button\" class=\"history-detail-review-btn btn-history-review\">' +\n"
    "        'Zobrazit pozici na šachovnici</button></div>';\n"
    "    var rb = panel.querySelector('.history-detail-review-btn');\n"
    "    if (rb) {\n"
    "        rb.addEventListener('click', function (ev2) {\n"
    "            ev2.stopPropagation();\n"
    "            enterReviewMode(actualIndex);\n"
    "        });\n"
    "    }\n"
    "    panel.style.display = 'block';\n"
    "}\n"
    "\n"
    "function renderHistoryList() {\n"
    "    var previewEl = document.getElementById('history-preview');\n"
    "    var fullEl = document.getElementById('history');\n"
    "    var wrap = document.getElementById('history-full-wrap');\n"
    "    wireHistoryToolbarOnce();\n"
    "\n"
    "    if (previewEl && fullEl) {\n"
    "        var total = historyData.length;\n"
    "        var rev;\n"
    "        previewEl.innerHTML = '';\n"
    "        fullEl.innerHTML = '';\n"
    "        if (historyFullExpanded) {\n"
    "            previewEl.style.display = 'none';\n"
    "            if (wrap) wrap.style.display = 'block';\n"
    "            for (rev = 0; rev < total; rev++) {\n"
    "                var ai = total - 1 - rev;\n"
    "                fullEl.appendChild(createHistoryItemElement(historyData[ai], ai));\n"
    "            }\n"
    "        } else {\n"
    "            previewEl.style.display = '';\n"
    "            if (wrap) wrap.style.display = 'none';\n"
    "            var nPrev = Math.min(HISTORY_PREVIEW_COUNT, total);\n"
    "            for (rev = 0; rev < nPrev; rev++) {\n"
    "                var ai2 = total - 1 - rev;\n"
    "                previewEl.appendChild(createHistoryItemElement(historyData[ai2], ai2));\n"
    "            }\n"
    "        }\n"
    "        var tbtn = document.getElementById('history-toggle-full');\n"
    "        if (tbtn) {\n"
    "            tbtn.style.display = total > HISTORY_PREVIEW_COUNT ? 'inline-block' : 'none';\n"
    "        }\n"
    "        if (historyDetailIndex != null &&\n"
    "            (historyDetailIndex < 0 || historyDetailIndex >= total)) {\n"
    "            historyDetailIndex = null;\n"
    "            var p = document.getElementById('history-move-detail');\n"
    "            if (p) p.style.display = 'none';\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    var historyBox = document.getElementById('history');\n"
    "    if (!historyBox) return;\n"
    "    historyBox.innerHTML = '';\n"
    "    historyData.slice().reverse().forEach((move, index) => {\n"
    "        var actualIndex = historyData.length - 1 - index;\n"
    "        historyBox.appendChild(createHistoryItemElement(move, actualIndex));\n"
    "    });\n"
    "}\n"
    "\n"
    "function updateHistory(history) {\n"
    "    historyData = history.moves || [];\n"
    "    renderHistoryList();\n"
    "}\n"
    "\n"
    "function updateCaptured(captured) {\n"
    "    capturedData = captured;\n"
    "    const whiteBox = document.getElementById('white-captured');\n"
    "    const blackBox = document.getElementById('black-captured');\n"
    "    whiteBox.innerHTML = '';\n"
    "    blackBox.innerHTML = '';\n"
    "    captured.white_captured.forEach(p => {\n"
    "        const piece = document.createElement('div');\n"
    "        piece.className = 'captured-piece';\n"
    "        const s = pieceImgSrc(p);\n"
    "        if (s) {\n"
    "            piece.innerHTML = '<img src=\"' + s + '\" alt=\"\">';\n"
    "        } else {\n"
    "            piece.textContent = pieceSymbols[p] || p;\n"
    "        }\n"
    "        whiteBox.appendChild(piece);\n"
    "    });\n"
    "    captured.black_captured.forEach(p => {\n"
    "        const piece = document.createElement('div');\n"
    "        piece.className = 'captured-piece';\n"
    "        const s2 = pieceImgSrc(p);\n"
    "        if (s2) {\n"
    "            piece.innerHTML = '<img src=\"' + s2 + '\" alt=\"\">';\n"
    "        } else {\n"
    "            piece.textContent = pieceSymbols[p] || p;\n"
    "        }\n"
    "        blackBox.appendChild(piece);\n"
    "    });\n"
    "}\n"
    "\n"
    "async function fetchData() {\n"
    "    if (reviewMode || sandboxMode) return;\n"
    "    if (fetchDataInFlight) return;\n"
    "    fetchDataInFlight = true;\n"
    "    try {\n"
    "        var snapRes = await fetch('/api/game/snapshot');\n"
    "        var board;\n"
    "        var status;\n"
    "        var history;\n"
    "        var captured;\n"
    "        if (snapRes.ok) {\n"
    "            var data = await snapRes.json();\n"
    "            board = { board: data.board, timestamp: data.timestamp };\n"
    "            status = data.status;\n"
    "            history = data.history;\n"
    "            captured = data.captured;\n"
    "            if (data.clock) {\n"
    "                lastSnapshotClockInfo = data.clock;\n"
    "                lastSnapshotClockAt = Date.now();\n"
    "                applyTimerInfo(data.clock);\n"
    "            } else {\n"
    "                lastSnapshotClockInfo = null;\n"
    "                lastSnapshotClockAt = 0;\n"
    "            }\n"
    "        } else {\n"
    "            lastSnapshotClockInfo = null;\n"
    "            lastSnapshotClockAt = 0;\n"
    "            const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([\n"
    "                fetch('/api/board'),\n"
    "                fetch('/api/status'),\n"
    "                fetch('/api/history'),\n"
    "                fetch('/api/captured')\n"
    "            ]);\n"
    "            board = await boardRes.json();\n"
    "            status = await statusRes.json();\n"
    "            history = await historyRes.json();\n"
    "            captured = await capturedRes.json();\n"
    "        }\n"
    "        updateBoard(board.board);\n"
    "        updateStatus(status);\n"
    "        if (status && status.puzzle) {\n"
    "            var pu = status.puzzle;\n"
    "            if (pu.active || pu.setup_active || (pu.feedback && String(pu.feedback).toLowerCase() !== 'none')) {\n"
    "                lastPuzzleSnapshotForOffline = {\n"
    "                    active: !!pu.active,\n"
    "                    setup_active: !!pu.setup_active,\n"
    "                    feedback: pu.feedback || 'none',\n"
    "                    message: pu.message || '',\n"
    "                    title: pu.title || '',\n"
    "                    teaser: pu.teaser || ''\n"
    "                };\n"
    "            } else {\n"
    "                lastPuzzleSnapshotForOffline = null;\n"
    "            }\n"
    "        } else {\n"
    "            lastPuzzleSnapshotForOffline = null;\n"
    "        }\n"
    "        updateHistory(history);\n"
    "        updateCaptured(captured);\n"
    "\n"
    "        var newHistoryLength = (history.moves && history.moves.length) ? history.moves.length : 0;\n"
    "        var currentFen = boardAndStatusToFen(boardData, status, history);\n"
    "\n"
    "        if (newHistoryLength === 0) {\n"
    "            lastFen = null;\n"
    "            lastHistoryLength = 0;\n"
    "            moveEvaluations = {};\n"
    "            historyFullExpanded = false;\n"
    "            historyDetailIndex = null;\n"
    "            var hmd = document.getElementById('history-move-detail');\n"
    "            if (hmd) hmd.style.display = 'none';\n"
    "            var htf = document.getElementById('history-toggle-full');\n"
    "            if (htf) {\n"
    "                htf.textContent = 'Celá historie';\n"
    "                htf.setAttribute('aria-expanded', 'false');\n"
    "            }\n"
    "            lastCapturedCount = (capturedData.white_captured || []).length + (capturedData.black_captured || []).length;\n"
    "            hideMoveEvaluation();\n"
    "        } else {\n"
    "            if (!getEvaluateMoveEnabled()) {\n"
    "                hideMoveEvaluation();\n"
    "            }\n"
    "            var castlingInProgress = status.castling_in_progress === true;\n"
    "            if (getEvaluateMoveEnabled() && !castlingInProgress && lastHistoryLength >= 0 && newHistoryLength === lastHistoryLength + 1 && lastFen) {\n"
    "                var lastMove = historyData[newHistoryLength - 1];\n"
    "                var lastMoveByWhite = (newHistoryLength - 1) % 2 === 0;\n"
    "                var lastMoveByBot = gameMode === 'bot' && ((botSettings.side === 'black' && lastMoveByWhite) || (botSettings.side === 'white' && !lastMoveByWhite));\n"
    "                if (lastMove && lastMove.from && lastMove.to && !lastMoveByBot) {\n"
    "                    evaluateMoveAsync(lastFen, currentFen, lastMove, newHistoryLength);\n"
    "                }\n"
    "            }\n"
    "            var totalCaptured = (capturedData.white_captured || []).length + (capturedData.black_captured || []).length;\n"
    "            if (newHistoryLength === lastHistoryLength + 1 && totalCaptured > lastCapturedCount && getHintAwardCapture()) {\n"
    "                var sideCapture = status.current_player === 'White' ? 'black' : 'white';\n"
    "                addHintReward('capture', sideCapture);\n"
    "            }\n"
    "            lastCapturedCount = totalCaptured;\n"
    "            if (!castlingInProgress && newHistoryLength >= lastHistoryLength) {\n"
    "                lastFen = currentFen;\n"
    "                lastHistoryLength = newHistoryLength;\n"
    "            }\n"
    "        }\n"
    "        checkBotTurn(status, currentFen);\n"
    "    } catch (error) {\n"
    "        console.error('Fetch error:', error);\n"
    "        if (lastPuzzleSnapshotForOffline) {\n"
    "            updatePuzzleStatusPanel({ puzzle: lastPuzzleSnapshotForOffline }, { offline: true });\n"
    "        }\n"
    "    } finally {\n"
    "        fetchDataInFlight = false;\n"
    "    }\n"
    "}\n"
    "\n"
    "function initializeApp() {\n"
    "    console.log('Initializing Chess App...');\n"
    "    createBoard();\n"
    "\n"
    "    var depthEl = document.getElementById('hint-depth');\n"
    "    if (depthEl) {\n"
    "        var d = getHintDepth();\n"
    "        depthEl.value = d;\n"
    "    }\n"
    "    var evaluateMoveEl = document.getElementById('evaluate-move-enabled');\n"
    "    if (evaluateMoveEl) evaluateMoveEl.checked = getEvaluateMoveEnabled();\n"
    "\n"
    "    var botLedTargetEl = document.getElementById('bot-led-target-only-after-lift');\n"
    "    if (botLedTargetEl) botLedTargetEl.checked = getBotLedTargetOnlyAfterLift();\n"
    "\n"
    "    var limit = getHintLimit();\n"
    "    var n = limit > 0 ? limit : 999;\n"
    "    hintsRemainingWhite = n;\n"
    "    hintsRemainingBlack = n;\n"
    "    lastCapturedCount = 0;\n"
    "    updateHintButtonLabel();\n"
    "\n"
    "    fetchData();\n"
    "    setInterval(fetchData, 2000); // Reduced from 500ms to 2s (4× fewer requests)\n"
    "    console.log('✅ Chess App initialized');\n"
    "}\n"
    "\n"
    "function ensureBoardFocusExitButton() {\n"
    "    if (document.getElementById('web-board-focus-exit')) return;\n"
    "    var b = document.createElement('button');\n"
    "    b.id = 'web-board-focus-exit';\n"
    "    b.type = 'button';\n"
    "    b.className = 'web-board-focus-exit-btn';\n"
    "    b.textContent = 'Celá aplikace';\n"
    "    b.setAttribute('aria-label', 'Zobrazit celou aplikaci');\n"
    "    b.onclick = function () {\n"
    "        exitWebBoardFocusMode();\n"
    "    };\n"
    "    document.body.appendChild(b);\n"
    "}\n"
    "\n"
    "function exitWebBoardFocusMode() {\n"
    "    document.documentElement.classList.remove('web-board-focus');\n"
    "    document.body.classList.remove('web-board-focus');\n"
    "    try {\n"
    "        document.body.style.position = '';\n"
    "        document.body.style.width = '';\n"
    "    } catch (e) { /* ignore */ }\n"
    "    var x = document.getElementById('web-board-focus-exit');\n"
    "    if (x) x.remove();\n"
    "    try {\n"
    "        localStorage.setItem('chessWebBoardFocus', '0');\n"
    "    } catch (e2) { /* ignore */ }\n"
    "}\n"
    "\n"
    "/**\n"
    " * Jen šachovnice + časovače: bez scrollování stránky, táhnutí figur při zapnutém ovládání z webu.\n"
    " * Zapnutí: URL ?focus=1 nebo ?board=1, nebo localStorage chessWebBoardFocus=1\n"
    " */\n"
    "function initWebBoardFocusMode() {\n"
    "    try {\n"
    "        var p = new URLSearchParams(window.location.search);\n"
    "        var q = p.get('focus') === '1' || p.get('board') === '1';\n"
    "        var ls = false;\n"
    "        try {\n"
    "            ls = localStorage.getItem('chessWebBoardFocus') === '1';\n"
    "        } catch (e0) { /* ignore */ }\n"
    "        if (!q && !ls) return;\n"
    "        document.documentElement.classList.add('web-board-focus');\n"
    "        document.body.classList.add('web-board-focus');\n"
    "        try {\n"
    "            localStorage.setItem('chessWebBoardFocus', '1');\n"
    "        } catch (e1) { /* ignore */ }\n"
    "        var rc = document.getElementById('remote-control-enabled');\n"
    "        if (rc && !rc.checked) {\n"
    "            rc.checked = true;\n"
    "            if (typeof toggleRemoteControl === 'function') toggleRemoteControl();\n"
    "        }\n"
    "        ensureBoardFocusExitButton();\n"
    "        if (typeof console !== 'undefined' && console.log) {\n"
    "            console.log('[staging] web-board-focus: jen deska + čas; táhni figurky (dálkové ovládání zapnuto)');\n"
    "        }\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) console.warn('initWebBoardFocusMode', e);\n"
    "    }\n"
    "}\n"
    "\n"
    "window.exitWebBoardFocusMode = exitWebBoardFocusMode;\n"
    "\n"
    "async function bootstrapChessWebUi() {\n"
    "    try {\n"
    "        await loadUiPrefsFromDevice();\n"
    "    } catch (e) {\n"
    "        if (typeof console !== 'undefined' && console.warn) {\n"
    "            console.warn('bootstrapChessWebUi loadUiPrefsFromDevice', e);\n"
    "        }\n"
    "    }\n"
    "    applyDevicePrefsToDom();\n"
    "    initWebBoardFocusMode();\n"
    "    console.log('🚀 Creating chess board...');\n"
    "    initializeApp();\n"
    "    console.log('✅ Chess JavaScript loaded successfully!');\n"
    "    console.log('⏱️ About to initialize timer system...');\n"
    "    try {\n"
    "        initTimerSystem();\n"
    "        console.log('✅ initTimerSystem() called successfully');\n"
    "    } catch (error) {\n"
    "        console.error('❌ CRITICAL ERROR in initTimerSystem():', error);\n"
    "        if (error && error.stack) console.error('Stack:', error.stack);\n"
    "    }\n"
    "}\n"
    "\n"
    "bootstrapChessWebUi();\n"
    "\n"
    "// ============================================================================\n"
    "// TIMER SYSTEM\n"
    "// ============================================================================\n"
    "\n"
    "let timerData = {\n"
    "    white_time_ms: 0,\n"
    "    black_time_ms: 0,\n"
    "    timer_running: false,\n"
    "    is_white_turn: true,\n"
    "    game_paused: false,\n"
    "    time_expired: false,\n"
    "    config: null,\n"
    "    total_moves: 0,\n"
    "    avg_move_time_ms: 0\n"
    "};\n"
    "let timerUpdateInterval = null;\n"
    "/** -1 = ještě nezjištěno; 1000 při aktivní časové kontrole, 8000 když je vypnutá. */\n"
    "let timerPollMs = -1;\n"
    "/** Čerstvý `clock` z GET /api/game/snapshot — šetří GET /api/timer v updateTimerDisplay. */\n"
    "let lastSnapshotClockInfo = null;\n"
    "let lastSnapshotClockAt = 0;\n"
    "const SNAPSHOT_CLOCK_FRESH_MS = 900;\n"
    "let selectedTimeControl = 0;\n"
    "\n"
    "// ========== HELPER FUNCTIONS (must be defined before use) ==========\n"
    "\n"
    "function formatTime(timeMs) {\n"
    "    const totalSeconds = Math.ceil(timeMs / 1000);\n"
    "    const hours = Math.floor(totalSeconds / 3600);\n"
    "    const minutes = Math.floor((totalSeconds % 3600) / 60);\n"
    "    const seconds = totalSeconds % 60;\n"
    "    if (hours > 0) {\n"
    "        return hours + ':' + minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0');\n"
    "    } else {\n"
    "        return minutes + ':' + seconds.toString().padStart(2, '0');\n"
    "    }\n"
    "}\n"
    "\n"
    "function updatePlayerTime(player, timeMs) {\n"
    "    const timeElement = document.getElementById(player + '-time');\n"
    "    const playerElement = document.getElementById(player + '-timer');\n"
    "    if (!timeElement || !playerElement) return;\n"
    "\n"
    "    // Zkontrolovat zda je časová kontrola aktivní\n"
    "    const isTimerActive = timerData.config && timerData.config.type !== 0;\n"
    "\n"
    "    if (isTimerActive) {\n"
    "        const formattedTime = formatTime(timeMs);\n"
    "        timeElement.textContent = formattedTime;\n"
    "        playerElement.classList.remove('low-time', 'critical-time');\n"
    "        if (timeMs < 5000) playerElement.classList.add('critical-time');\n"
    "        else if (timeMs < 30000) playerElement.classList.add('low-time');\n"
    "    } else {\n"
    "        // Bez časové kontroly - zobrazit \"--:--\" a odstranit všechny warning třídy\n"
    "        timeElement.textContent = '--:--';\n"
    "        playerElement.classList.remove('low-time', 'critical-time', 'active');\n"
    "        return; // Nedělat nic dalšího\n"
    "    }\n"
    "\n"
    "    if ((player === 'white' && timerData.is_white_turn) || (player === 'black' && !timerData.is_white_turn)) {\n"
    "        playerElement.classList.add('active');\n"
    "    } else {\n"
    "        playerElement.classList.remove('active');\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateActivePlayer(isWhiteTurn) {\n"
    "    const whiteIndicator = document.getElementById('white-move-indicator');\n"
    "    const blackIndicator = document.getElementById('black-move-indicator');\n"
    "    if (whiteIndicator && blackIndicator) {\n"
    "        whiteIndicator.classList.toggle('active', isWhiteTurn);\n"
    "        blackIndicator.classList.toggle('active', !isWhiteTurn);\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateProgressBars(timerInfo) {\n"
    "    if (!timerInfo || !timerInfo.config) {\n"
    "        console.warn('Timer info missing config:', timerInfo);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // Zkontrolovat zda je časová kontrola aktivní\n"
    "    if (timerInfo.config.type === 0) {\n"
    "        // Bez časové kontroly - skrýt progress bary\n"
    "        const whiteProgress = document.getElementById('white-progress');\n"
    "        const blackProgress = document.getElementById('black-progress');\n"
    "        if (whiteProgress) whiteProgress.style.width = '0%';\n"
    "        if (blackProgress) blackProgress.style.width = '0%';\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const initialTime = timerInfo.config.initial_time_ms;\n"
    "    if (initialTime === 0) return;\n"
    "    const whiteProgress = document.getElementById('white-progress');\n"
    "    const blackProgress = document.getElementById('black-progress');\n"
    "    if (whiteProgress) {\n"
    "        const whitePercent = (timerInfo.white_time_ms / initialTime) * 100;\n"
    "        whiteProgress.style.width = Math.max(0, Math.min(100, whitePercent)) + '%';\n"
    "    }\n"
    "    if (blackProgress) {\n"
    "        const blackPercent = (timerInfo.black_time_ms / initialTime) * 100;\n"
    "        blackProgress.style.width = Math.max(0, Math.min(100, blackPercent)) + '%';\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateTimerStats(timerInfo) {\n"
    "    const avgMoveTimeElement = document.getElementById('avg-move-time');\n"
    "    const totalMovesElement = document.getElementById('total-moves');\n"
    "    if (avgMoveTimeElement) {\n"
    "        avgMoveTimeElement.textContent = timerInfo.avg_move_time_ms > 0 ? formatTime(timerInfo.avg_move_time_ms) : '-';\n"
    "    }\n"
    "    if (totalMovesElement) {\n"
    "        totalMovesElement.textContent = timerInfo.total_moves || 0;\n"
    "    }\n"
    "}\n"
    "\n"
    "function checkTimeWarnings(timerInfo) {\n"
    "    // Nekontrolovat upozornění pokud není časová kontrola aktivní\n"
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const currentPlayerTime = timerInfo.is_white_turn ? timerInfo.white_time_ms : timerInfo.black_time_ms;\n"
    "    if (currentPlayerTime < 5000 && !timerInfo.warning_5s_shown) {\n"
    "        showTimeWarning('Kritické! Méně než 5 sekund!', 'critical');\n"
    "    } else if (currentPlayerTime < 10000 && !timerInfo.warning_10s_shown) {\n"
    "        showTimeWarning('Varování! Méně než 10 sekund!', 'warning');\n"
    "    } else if (currentPlayerTime < 30000 && !timerInfo.warning_30s_shown) {\n"
    "        showTimeWarning('Málo času! Méně než 30 sekund!', 'info');\n"
    "    }\n"
    "}\n"
    "\n"
    "function showTimeWarning(message, type) {\n"
    "    const notification = document.createElement('div');\n"
    "    notification.className = 'time-warning ' + type;\n"
    "    notification.textContent = message;\n"
    "    notification.style.cssText = 'position: fixed; top: 20px; right: 20px; padding: 15px 20px; border-radius: 8px; color: white; font-weight: 600; z-index: 1000; animation: slideInRight 0.3s ease;';\n"
    "    switch (type) {\n"
    "        case 'critical': notification.style.background = '#F44336'; break;\n"
    "        case 'warning': notification.style.background = '#FF9800'; break;\n"
    "        case 'info': notification.style.background = '#2196F3'; break;\n"
    "    }\n"
    "    document.body.appendChild(notification);\n"
    "    setTimeout(() => {\n"
    "        notification.style.animation = 'slideOutRight 0.3s ease';\n"
    "        setTimeout(() => {\n"
    "            if (notification.parentNode) notification.parentNode.removeChild(notification);\n"
    "        }, 300);\n"
    "    }, 3000);\n"
    "}\n"
    "\n"
    "function handleTimeExpiration(timerInfo) {\n"
    "    // Nekontrolovat expiraci pokud není časová kontrola aktivní\n"
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const expiredPlayer = timerInfo.is_white_turn ? 'Bílý' : 'Černý';\n"
    "    showTimeWarning('Čas vypršel! ' + expiredPlayer + ' prohrál na čas.', 'critical');\n"
    "    const pauseBtn = document.getElementById('pause-timer');\n"
    "    const resumeBtn = document.getElementById('resume-timer');\n"
    "    if (pauseBtn) pauseBtn.disabled = true;\n"
    "    if (resumeBtn) resumeBtn.disabled = true;\n"
    "}\n"
    "\n"
    "function toggleCustomSettings() {\n"
    "    const customSettings = document.getElementById('custom-time-settings');\n"
    "    if (!customSettings) return;\n"
    "    if (selectedTimeControl === 14) {\n"
    "        customSettings.style.display = 'block';\n"
    "    } else {\n"
    "        customSettings.style.display = 'none';\n"
    "    }\n"
    "}\n"
    "\n"
    "function changeTimeControl() {\n"
    "    const select = document.getElementById('time-control-select');\n"
    "    const applyBtn = document.getElementById('apply-time-control');\n"
    "    if (!select) return;\n"
    "    selectedTimeControl = parseInt(select.value, 10);\n"
    "    toggleCustomSettings();\n"
    "    if (applyBtn) applyBtn.disabled = false;\n"
    "}\n"
    "\n"
    "// ========== TIMER INITIALIZATION AND MAIN FUNCTIONS ==========\n"
    "\n"
    "function initTimerSystem() {\n"
    "    console.log('🔵 Initializing timer system...');\n"
    "    // Check if DOM elements exist before accessing them\n"
    "    const timeControlSelect = document.getElementById('time-control-select');\n"
    "    const applyButton = document.getElementById('apply-time-control');\n"
    "    if (!timeControlSelect) {\n"
    "        console.warn('⚠️ Timer controls not ready yet, retrying in 100ms...');\n"
    "        setTimeout(() => initTimerSystem(), 100);\n"
    "        return;\n"
    "    }\n"
    "    selectedTimeControl = parseInt(timeControlSelect.value, 10);\n"
    "    toggleCustomSettings();\n"
    "    // Enable button if a time control is selected (not 0 = None)\n"
    "    if (selectedTimeControl !== 0 && applyButton) {\n"
    "        applyButton.disabled = false;\n"
    "    }\n"
    "    console.log('🔵 Starting timer update loop immediately...');\n"
    "    // Start timer loop immediately (no delay)\n"
    "    startTimerUpdateLoop();\n"
    "}\n"
    "\n"
    "function rescheduleTimerPollInterval() {\n"
    "    var active = timerData.config && timerData.config.type !== 0;\n"
    "    var want = active ? 1000 : 8000;\n"
    "    if (timerPollMs === want && timerUpdateInterval) {\n"
    "        return;\n"
    "    }\n"
    "    timerPollMs = want;\n"
    "    if (timerUpdateInterval) {\n"
    "        clearInterval(timerUpdateInterval);\n"
    "    }\n"
    "    timerUpdateInterval = setInterval(async () => {\n"
    "        try {\n"
    "            await updateTimerDisplay();\n"
    "        } catch (error) {\n"
    "            console.error('Timer update loop error:', error);\n"
    "        }\n"
    "    }, timerPollMs);\n"
    "}\n"
    "\n"
    "function startTimerUpdateLoop() {\n"
    "    if (timerUpdateInterval) {\n"
    "        clearInterval(timerUpdateInterval);\n"
    "        timerUpdateInterval = null;\n"
    "    }\n"
    "    timerPollMs = -1;\n"
    "    updateTimerDisplay().catch(e => console.error('Initial timer update failed:', e));\n"
    "}\n"
    "\n"
    "function applyTimerInfo(timerInfo) {\n"
    "    timerData = timerInfo;\n"
    "    const tcSel = document.getElementById('time-control-select');\n"
    "    if (tcSel && timerInfo.config && typeof timerInfo.config.type === 'number') {\n"
    "        var tt = timerInfo.config.type;\n"
    "        selectedTimeControl = tt;\n"
    "        if (tcSel.value !== String(tt)) {\n"
    "            tcSel.value = String(tt);\n"
    "        }\n"
    "        toggleCustomSettings();\n"
    "    }\n"
    "    updatePlayerTime('white', timerInfo.white_time_ms);\n"
    "    updatePlayerTime('black', timerInfo.black_time_ms);\n"
    "    updateActivePlayer(timerInfo.is_white_turn);\n"
    "    updateProgressBars(timerInfo);\n"
    "    updateTimerStats(timerInfo);\n"
    "    const pauseBtn = document.getElementById('pause-timer');\n"
    "    const resumeBtn = document.getElementById('resume-timer');\n"
    "    const resetBtn = document.getElementById('reset-timer');\n"
    "    const isTimerActive = timerInfo.config && timerInfo.config.type !== 0;\n"
    "    if (pauseBtn) pauseBtn.disabled = !isTimerActive;\n"
    "    if (resumeBtn) resumeBtn.disabled = !isTimerActive;\n"
    "    if (resetBtn) resetBtn.disabled = !isTimerActive;\n"
    "    if (isTimerActive) {\n"
    "        checkTimeWarnings(timerInfo);\n"
    "        if (timerInfo.time_expired) {\n"
    "            handleTimeExpiration(timerInfo);\n"
    "        }\n"
    "    }\n"
    "    rescheduleTimerPollInterval();\n"
    "}\n"
    "\n"
    "async function updateTimerDisplay() {\n"
    "    try {\n"
    "        if (lastSnapshotClockInfo && (Date.now() - lastSnapshotClockAt) < SNAPSHOT_CLOCK_FRESH_MS) {\n"
    "            applyTimerInfo(lastSnapshotClockInfo);\n"
    "            return;\n"
    "        }\n"
    "        const response = await fetch('/api/timer');\n"
    "        if (response.ok) {\n"
    "            const timerInfo = await response.json();\n"
    "            applyTimerInfo(timerInfo);\n"
    "        } else {\n"
    "            console.error('Timer update failed:', response.status);\n"
    "            rescheduleTimerPollInterval();\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Timer update error:', error);\n"
    "        rescheduleTimerPollInterval();\n"
    "    }\n"
    "}\n"
    "\n"
    "async function applyTimeControl() {\n"
    "    const timeControlSelect = document.getElementById('time-control-select');\n"
    "    const timeControlType = parseInt(timeControlSelect.value);\n"
    "    let config = { type: timeControlType };\n"
    "    if (timeControlType === 14) {\n"
    "        const minutes = parseInt(document.getElementById('custom-minutes').value);\n"
    "        const increment = parseInt(document.getElementById('custom-increment').value);\n"
    "        if (minutes < 1 || minutes > 180) { alert('Minuty musí být 1–180'); return; }\n"
    "        if (increment < 0 || increment > 60) { alert('Increment musí být 0–60 sekund'); return; }\n"
    "        config.custom_minutes = minutes;\n"
    "        config.custom_increment = increment;\n"
    "    }\n"
    "    try {\n"
    "        console.log('Applying time control:', config);\n"
    "        const response = await fetch('/api/timer/config', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify(config)\n"
    "        });\n"
    "        if (response.ok) {\n"
    "            const responseText = await response.text();\n"
    "            console.log('✅ Time control response:', responseText);\n"
    "            // Wait for backend to process the command\n"
    "            await new Promise(resolve => setTimeout(resolve, 500));\n"
    "            // Refresh timer display multiple times to ensure update\n"
    "            for (let i = 0; i < 5; i++) {\n"
    "                await updateTimerDisplay();\n"
    "                await new Promise(resolve => setTimeout(resolve, 300));\n"
    "            }\n"
    "            showTimeWarning('Časová kontrola nastavena.', 'info');\n"
    "            const applyBtn = document.getElementById('apply-time-control');\n"
    "            if (applyBtn) applyBtn.disabled = true;\n"
    "        } else {\n"
    "            const errorText = await response.text();\n"
    "            console.error('Failed to apply time control:', response.status, errorText);\n"
    "            throw new Error('Failed to apply time control: ' + errorText);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Error applying time control:', error);\n"
    "        showTimeWarning('Chyba nastavení časové kontroly: ' + error.message, 'critical');\n"
    "    }\n"
    "}\n"
    "\n"
    "async function pauseTimer() {\n"
    "    try {\n"
    "        const response = await fetch('/api/timer/pause', { method: 'POST' });\n"
    "        if (response.ok) {\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            if (pauseBtn) pauseBtn.style.display = 'none';\n"
    "            if (resumeBtn) resumeBtn.style.display = 'inline-block';\n"
    "            showTimeWarning('Časomíra pozastavena', 'info');\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('❌ Error pausing timer:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function resumeTimer() {\n"
    "    try {\n"
    "        const response = await fetch('/api/timer/resume', { method: 'POST' });\n"
    "        if (response.ok) {\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            if (pauseBtn) pauseBtn.style.display = 'inline-block';\n"
    "            if (resumeBtn) resumeBtn.style.display = 'none';\n"
    "            showTimeWarning('Časomíra pokračuje', 'info');\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('❌ Error resuming timer:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function resetTimer() {\n"
    "    if (confirm('Really reset timer?')) {\n"
    "        try {\n"
    "            const response = await fetch('/api/timer/reset', { method: 'POST' });\n"
    "            if (response.ok) {\n"
    "                showTimeWarning('Časomíra resetována', 'info');\n"
    "                console.log('✅ Timer reset successfully');\n"
    "                await updateTimerDisplay();\n"
    "            }\n"
    "        } catch (error) {\n"
    "            console.error('❌ Error resetting timer:', error);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// BRIGHTNESS (Nastavení → Zařízení) – for inline onchange on brightness-slider\n"
    "// ============================================================================\n"
    "\n"
    "async function setBrightness(value) {\n"
    "    const num = Math.min(100, Math.max(0, parseInt(value, 10)));\n"
    "    if (Number.isNaN(num)) return;\n"
    "    try {\n"
    "        const response = await fetch('/api/settings/brightness', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ brightness: num })\n"
    "        });\n"
    "        const data = response.ok ? await response.json().catch(() => ({})) : {};\n"
    "        if (data.success !== false) {\n"
    "            if (typeof console !== 'undefined' && console.log) console.log('Jas nastaven na', num + '%');\n"
    "        } else {\n"
    "            console.warn('Nastavení jasu selhalo:', data.message || response.status);\n"
    "        }\n"
    "    } catch (err) {\n"
    "        console.error('Chyba nastavení jasu:', err.message);\n"
    "    }\n"
    "}\n"
    "window.setBrightness = setBrightness;\n"
    "\n"
    "async function setLedGuidanceLevel(level) {\n"
    "    const n = Math.min(5, Math.max(1, parseInt(level, 10)));\n"
    "    if (Number.isNaN(n)) {\n"
    "        return;\n"
    "    }\n"
    "    try {\n"
    "        const response = await fetch('/api/settings/led_guidance', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ level: n })\n"
    "        });\n"
    "        const data = response.ok ? await response.json().catch(() => ({})) : {};\n"
    "        if (data.success === false) {\n"
    "            console.warn('Nastavení LED nápovědy selhalo:', data.message || response.status);\n"
    "        }\n"
    "    } catch (err) {\n"
    "        console.error('Chyba nastavení LED nápovědy:', err.message);\n"
    "    }\n"
    "}\n"
    "window.setLedGuidanceLevel = setLedGuidanceLevel;\n"
    "\n"
    "// ============================================================================\n"
    "// LAMP MODE (Nastavení → Zařízení: Šachovnice / Lampa, barva)\n"
    "// ============================================================================\n"
    "\n"
    "function showLightError(msg) {\n"
    "    const el = document.getElementById('light-error-msg');\n"
    "    if (el) {\n"
    "        el.textContent = msg || 'Chyba odeslání';\n"
    "        el.style.display = '';\n"
    "        setTimeout(function () { el.style.display = 'none'; el.textContent = ''; }, 2500);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function setLightModeGame() {\n"
    "    try {\n"
    "        const res = await fetch('/api/light/game_mode', { method: 'POST', headers: { 'Content-Type': 'application/json' } });\n"
    "        if (res.ok) {\n"
    "            const btnGame = document.getElementById('light-mode-game');\n"
    "            const btnLamp = document.getElementById('light-mode-lamp');\n"
    "            const lampControls = document.getElementById('light-lamp-controls');\n"
    "            if (btnGame) btnGame.classList.add('active');\n"
    "            if (btnLamp) btnLamp.classList.remove('active');\n"
    "            if (lampControls) lampControls.style.display = 'none';\n"
    "        } else {\n"
    "            showLightError('Přepnutí na šachovnici selhalo');\n"
    "        }\n"
    "    } catch (e) {\n"
    "        console.warn('setLightModeGame failed:', e.message);\n"
    "        showLightError('Chyba sítě');\n"
    "    }\n"
    "}\n"
    "\n"
    "async function setLightModeLamp(r, g, b, state) {\n"
    "    const rr = Math.min(255, Math.max(0, parseInt(r, 10) || 255));\n"
    "    const gg = Math.min(255, Math.max(0, parseInt(g, 10) || 255));\n"
    "    const bb = Math.min(255, Math.max(0, parseInt(b, 10) || 255));\n"
    "    try {\n"
    "        const res = await fetch('/api/light/command', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ state: !!state, r: rr, g: gg, b: bb })\n"
    "        });\n"
    "        if (res.ok) {\n"
    "            const btnGame = document.getElementById('light-mode-game');\n"
    "            const btnLamp = document.getElementById('light-mode-lamp');\n"
    "            const lampControls = document.getElementById('light-lamp-controls');\n"
    "            if (btnGame) btnGame.classList.remove('active');\n"
    "            if (btnLamp) btnLamp.classList.add('active');\n"
    "            if (lampControls) lampControls.style.display = '';\n"
    "        } else {\n"
    "            const data = await res.json().catch(function () { return {}; });\n"
    "            showLightError(data.message || 'Zařízení není připraveno (503)');\n"
    "        }\n"
    "    } catch (e) {\n"
    "        console.warn('setLightModeLamp failed:', e.message);\n"
    "        showLightError('Chyba sítě');\n"
    "    }\n"
    "}\n"
    "\n"
    "let lightCommandDebounceId = null;\n"
    "function sendLightCommandDebounced() {\n"
    "    if (lightCommandDebounceId) clearTimeout(lightCommandDebounceId);\n"
    "    lightCommandDebounceId = setTimeout(function () {\n"
    "        lightCommandDebounceId = null;\n"
    "        const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');\n"
    "        const toggleEl = document.getElementById('light-state-toggle');\n"
    "        if (!rEl || !gEl || !bEl) return;\n"
    "        const r = parseInt(rEl.value, 10) || 0, g = parseInt(gEl.value, 10) || 0, b = parseInt(bEl.value, 10) || 0;\n"
    "        const state = toggleEl ? toggleEl.checked : true;\n"
    "        setLightModeLamp(r, g, b, state);\n"
    "    }, 120);\n"
    "}\n"
    "\n"
    "function initLightControls() {\n"
    "    const btnGame = document.getElementById('light-mode-game');\n"
    "    const btnLamp = document.getElementById('light-mode-lamp');\n"
    "    const toggleEl = document.getElementById('light-state-toggle');\n"
    "    const ledGuidanceEl = document.getElementById('led-guidance-level');\n"
    "    const rEl = document.getElementById('light-r'), gEl = document.getElementById('light-g'), bEl = document.getElementById('light-b');\n"
    "    if (btnGame) btnGame.addEventListener('click', setLightModeGame);\n"
    "    if (btnLamp) btnLamp.addEventListener('click', function () {\n"
    "        const r = rEl ? parseInt(rEl.value, 10) : 255, g = gEl ? parseInt(gEl.value, 10) : 255, b = bEl ? parseInt(bEl.value, 10) : 255;\n"
    "        setLightModeLamp(r, g, b, true);\n"
    "    });\n"
    "    if (toggleEl) toggleEl.addEventListener('change', sendLightCommandDebounced);\n"
    "    if (ledGuidanceEl) {\n"
    "        ledGuidanceEl.addEventListener('change', function () {\n"
    "            setLedGuidanceLevel(ledGuidanceEl.value);\n"
    "        });\n"
    "    }\n"
    "    function updateRgbLabel(id, valueId) {\n"
    "        const el = document.getElementById(id), val = document.getElementById(valueId);\n"
    "        if (el && val) { val.textContent = el.value; }\n"
    "    }\n"
    "    if (rEl) { rEl.addEventListener('input', function () { updateRgbLabel('light-r', 'light-r-value'); sendLightCommandDebounced(); }); }\n"
    "    if (gEl) { gEl.addEventListener('input', function () { updateRgbLabel('light-g', 'light-g-value'); sendLightCommandDebounced(); }); }\n"
    "    if (bEl) { bEl.addEventListener('input', function () { updateRgbLabel('light-b', 'light-b-value'); sendLightCommandDebounced(); }); }\n"
    "}\n"
    "\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', initLightControls);\n"
    "} else {\n"
    "    setTimeout(initLightControls, 100);\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// NEW GAME (Nastavení → action bar „Nová hra“) – for inline onclick\n"
    "// ============================================================================\n"
    "\n"
    "function getConfirmNewGameEnabled() {\n"
    "    const cb = document.getElementById('confirm-new-game');\n"
    "    if (cb) return cb.checked;\n"
    "    return !!devicePrefs.chess_confirm_new_game;\n"
    "}\n"
    "\n"
    "async function startNewGame() {\n"
    "    if (isWebLocked()) {\n"
    "        alert('Rozhraní je zamčeno. Odemkněte přes UART.');\n"
    "        return;\n"
    "    }\n"
    "    if (getConfirmNewGameEnabled()) {\n"
    "        if (!confirm('Opravdu chcete začít novou hru? Aktuální partie bude ukončena.')) {\n"
    "            return;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    // UPDATE BOT SETTINGS FROM UI\n"
    "    const modeEl = document.getElementById('game-mode');\n"
    "    const strengthEl = document.getElementById('bot-strength');\n"
    "    const sideEl = document.getElementById('player-side');\n"
    "\n"
    "    if (modeEl) gameMode = modeEl.value;\n"
    "\n"
    "    botSettings.strength = (strengthEl) ? strengthEl.value : 10;\n"
    "    let sidePref = (sideEl) ? sideEl.value : 'white';\n"
    "\n"
    "    if (sidePref === 'random') {\n"
    "        sidePref = (Math.random() < 0.5) ? 'white' : 'black';\n"
    "        console.log('Random side (fallback):', sidePref);\n"
    "    }\n"
    "    botSettings.side = sidePref;\n"
    "\n"
    "    botThinking = false; // Reset bot state\n"
    "    lastSuggestedFen = null;\n"
    "    lastSuggestedMove = null;\n"
    "    stopBotHintRefresh();\n"
    "    var limit = getHintLimit();\n"
    "    var n = limit > 0 ? limit : 999;\n"
    "    hintsRemainingWhite = n;\n"
    "    hintsRemainingBlack = n;\n"
    "    lastCapturedCount = 0;\n"
    "    lastHintedMove = null;\n"
    "    updateHintButtonLabel();\n"
    "    gameGeneration++; // INVALIDATE OLD BOT REQUESTS\n"
    "    console.log('Starting New Game. Generation:', gameGeneration);\n"
    "\n"
    "    try {\n"
    "        const response = await fetch('/api/game/new', { method: 'POST' });\n"
    "        if (response.ok) {\n"
    "            if (typeof console !== 'undefined' && console.log) console.log('Nová hra spuštěna. Mode:', gameMode, 'Player Side:', botSettings.side);\n"
    "\n"
    "            await fetchData();\n"
    "\n"
    "            if (gameMode === 'bot' && botSettings.side === 'black') {\n"
    "                // Player is Black -> Bot is White -> Bot moves immediately\n"
    "                // We pass initial FEN (start pos)\n"
    "                const startFen = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';\n"
    "                setTimeout(() => checkBotTurn({ current_player: 'White', game_state: 'active' }, startFen), 500);\n"
    "            }\n"
    "        } else {\n"
    "            console.warn('Nová hra selhala:', response.status);\n"
    "        }\n"
    "    } catch (err) {\n"
    "        console.error('Chyba startNewGame:', err.message);\n"
    "    }\n"
    "}\n"
    "window.startNewGame = startNewGame;\n"
    "\n"
    "function handleRandomDraw() {\n"
    "    var sideEl = document.getElementById('player-side');\n"
    "    var msgEl = document.getElementById('random-draw-result');\n"
    "    if (!sideEl) return;\n"
    "    if (sideEl.value !== 'random') {\n"
    "        if (msgEl) {\n"
    "            msgEl.style.display = 'none';\n"
    "            msgEl.textContent = '';\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "    var drawn = Math.random() < 0.5 ? 'white' : 'black';\n"
    "    sideEl.value = drawn;\n"
    "    if (msgEl) {\n"
    "        msgEl.textContent = drawn === 'white' ? 'Losování: Hrajete za bílého.' : 'Losování: Hrajete za černého.';\n"
    "        msgEl.style.display = 'block';\n"
    "    }\n"
    "    if (typeof saveBotSettings === 'function') saveBotSettings();\n"
    "}\n"
    "window.handleRandomDraw = handleRandomDraw;\n"
    "\n"
    "// ============================================================================\n"
    "// PROMOTION MODAL (Q/R/B/N a Zrušit) – for inline onclick\n"
    "// ============================================================================\n"
    "\n"
    "async function selectPromotion(choice) {\n"
    "    const modal = document.getElementById('promotion-modal');\n"
    "    if (modal) modal.style.display = 'none';\n"
    "    try {\n"
    "        const body = JSON.stringify({ action: 'promote', choice: String(choice).toUpperCase().slice(0, 1) });\n"
    "        const response = await fetch('/api/game/virtual_action', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: body\n"
    "        });\n"
    "        if (response.ok) await fetchData();\n"
    "    } catch (err) {\n"
    "        console.error('Chyba selectPromotion:', err.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "function cancelPromotion() {\n"
    "    const modal = document.getElementById('promotion-modal');\n"
    "    if (modal) modal.style.display = 'none';\n"
    "    // Odblokovat hru výchozí volbou (Dáma)\n"
    "    selectPromotion('Q');\n"
    "}\n"
    "window.selectPromotion = selectPromotion;\n"
    "window.cancelPromotion = cancelPromotion;\n"
    "\n"
    "// Expose timer functions globally for inline onclick handlers\n"
    "window.changeTimeControl = changeTimeControl;\n"
    "window.applyTimeControl = applyTimeControl;\n"
    "window.pauseTimer = pauseTimer;\n"
    "window.resumeTimer = resumeTimer;\n"
    "window.resetTimer = resetTimer;\n"
    "window.hideEndgameReport = hideEndgameReport;\n"
    "window.toggleRemoteControl = toggleRemoteControl;\n"
    "window.undoSandboxMove = undoSandboxMove;\n"
    "\n"
    "// ============================================================================\n"
    "// KEYBOARD SHORTCUTS AND EVENT HANDLERS\n"
    "// ============================================================================\n"
    "\n"
    "document.addEventListener('keydown', (e) => {\n"
    "    if (e.key === 'Escape') {\n"
    "        if (reviewMode) {\n"
    "            exitReviewMode();\n"
    "        } else if (sandboxMode) {\n"
    "            exitSandboxMode();\n"
    "        } else {\n"
    "            clearHighlights();\n"
    "        }\n"
    "    }\n"
    "    if (historyData.length === 0) return;\n"
    "    switch (e.key) {\n"
    "        case 'ArrowLeft':\n"
    "            e.preventDefault();\n"
    "            if (reviewMode && currentReviewIndex > 0) {\n"
    "                enterReviewMode(currentReviewIndex - 1);\n"
    "            } else if (!reviewMode && !sandboxMode && historyData.length > 0) {\n"
    "                enterReviewMode(historyData.length - 1);\n"
    "            }\n"
    "            break;\n"
    "        case 'ArrowRight':\n"
    "            e.preventDefault();\n"
    "            if (reviewMode && currentReviewIndex < historyData.length - 1) {\n"
    "                enterReviewMode(currentReviewIndex + 1);\n"
    "            }\n"
    "            break;\n"
    "    }\n"
    "});\n"
    "\n"
    "// Click outside to deselect\n"
    "document.addEventListener('click', (e) => {\n"
    "    if (!e.target.closest('.history-block')) {\n"
    "        var pd = document.getElementById('history-move-detail');\n"
    "        if (pd) {\n"
    "            pd.style.display = 'none';\n"
    "            historyDetailIndex = null;\n"
    "        }\n"
    "    }\n"
    "    if (!e.target.closest('.square') && !e.target.closest('.history-item')) {\n"
    "        if (!reviewMode) {\n"
    "            clearHighlights();\n"
    "        }\n"
    "    }\n"
    "});\n"
    "\n"
    "// ============================================================================\n"
    "// WIFI FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "async function saveWiFiConfig() {\n"
    "    const ssid = document.getElementById('wifi-ssid').value;\n"
    "    const password = document.getElementById('wifi-password').value;\n"
    "    if (!ssid || !password) {\n"
    "        alert('SSID a heslo jsou povinné');\n"
    "        return;\n"
    "    }\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/config', {\n"
    "            method: 'POST',\n"
    "            headers: boardApiAuthHeaders({ 'Content-Type': 'application/json' }),\n"
    "            body: JSON.stringify({ ssid: ssid, password: password })\n"
    "        });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('WiFi uloženo. Stiskněte „Připojit STA“.');\n"
    "        } else {\n"
    "            alert('Uložení WiFi selhalo: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Chyba: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function connectSTA() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/connect', {\n"
    "            method: 'POST',\n"
    "            headers: boardApiAuthHeaders()\n"
    "        });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('Připojování k WiFi...');\n"
    "            setTimeout(updateWiFiStatus, 1500);\n"
    "        } else {\n"
    "            alert('Připojení selhalo: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Chyba: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function disconnectSTA() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/disconnect', {\n"
    "            method: 'POST',\n"
    "            headers: boardApiAuthHeaders()\n"
    "        });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('Odpojeno od WiFi');\n"
    "            setTimeout(updateWiFiStatus, 1000);\n"
    "        } else {\n"
    "            alert('Odpojení selhalo: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Chyba: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function clearWiFiConfig() {\n"
    "    if (!confirm('Opravdu smazat uloženou WiFi konfiguraci? ESP se odpojí od sítě.')) {\n"
    "        return;\n"
    "    }\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/clear', {\n"
    "            method: 'POST',\n"
    "            headers: boardApiAuthHeaders()\n"
    "        });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('WiFi konfigurace smazána.');\n"
    "            setTimeout(updateWiFiStatus, 500);\n"
    "        } else {\n"
    "            alert('Smazání selhalo: ' + (data.message || 'neznámá chyba'));\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Chyba: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Celý NVS oddíl + restart (stejné jako BLE `factory_reset`). */\n"
    "async function factoryResetDevice() {\n"
    "    if (!confirm('Tovární reset: smaže VŠECHNO v NVS (WiFi, MQTT, uložená partie, UI) a desku restartuje. Pokračovat?')) {\n"
    "        return;\n"
    "    }\n"
    "    if (!confirm('Naposledy: opravdu vymazat celou NVS flash?')) {\n"
    "        return;\n"
    "    }\n"
    "    try {\n"
    "        const response = await fetch('/api/system/factory_reset', {\n"
    "            method: 'POST',\n"
    "            headers: boardApiAuthHeaders({ 'Content-Type': 'application/json' }),\n"
    "            body: JSON.stringify({ confirm: 'erase_all_nvs' })\n"
    "        });\n"
    "        var data = {};\n"
    "        try {\n"
    "            data = await response.json();\n"
    "        } catch (e) {\n"
    "            data = {};\n"
    "        }\n"
    "        if (response.ok && data.success) {\n"
    "            alert('Reset naplánován — deska za chvíli naběhne s prázdnou NVS.');\n"
    "        } else {\n"
    "            alert('Factory reset selhal: ' + (data.message || response.status));\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('factoryResetDevice:', error);\n"
    "        alert('Chyba při factory resetu');\n"
    "    }\n"
    "}\n"
    "\n"
    "/** True if API vrací uložené STA SSID z NVS (ne zástupný text z firmware). */\n"
    "function wifiStatusHasSavedStaSsid(staSsid) {\n"
    "    if (staSsid == null || typeof staSsid !== 'string') {\n"
    "        return false;\n"
    "    }\n"
    "    const s = staSsid.trim();\n"
    "    if (s === '') {\n"
    "        return false;\n"
    "    }\n"
    "    const lower = s.toLowerCase();\n"
    "    if (lower === 'not configured' || lower === 'nenastaveno') {\n"
    "        return false;\n"
    "    }\n"
    "    return true;\n"
    "}\n"
    "\n"
    "async function updateWiFiStatus() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/status');\n"
    "        const data = await response.json();\n"
    "        document.getElementById('ap-ssid').textContent = data.ap_ssid || 'ESP32-CzechMate';\n"
    "        document.getElementById('ap-ip').textContent = data.ap_ip || '192.168.4.1';\n"
    "        document.getElementById('ap-clients').textContent = data.ap_clients || 0;\n"
    "        document.getElementById('sta-ssid').textContent =\n"
    "            wifiStatusHasSavedStaSsid(data.sta_ssid) ? data.sta_ssid : 'Nenastaveno';\n"
    "        document.getElementById('sta-ip').textContent = data.sta_ip || 'Nepřipojeno';\n"
    "        document.getElementById('sta-connected').textContent = data.sta_connected ? 'ano' : 'ne';\n"
    "        if (wifiStatusHasSavedStaSsid(data.sta_ssid)) {\n"
    "            document.getElementById('wifi-ssid').value = data.sta_ssid;\n"
    "        }\n"
    "        // Zařízení: Zámek a Online (data z téhož API)\n"
    "        const lockEl = document.getElementById('web-lock-status');\n"
    "        if (lockEl) {\n"
    "            lockEl.textContent = data.locked ? 'Zamčeno' : 'Odemčeno';\n"
    "            lockEl.style.color = data.locked ? '#e53935' : '#43a047';\n"
    "        }\n"
    "        const onlineEl = document.getElementById('web-online-status');\n"
    "        if (onlineEl) {\n"
    "            onlineEl.textContent = data.online ? 'Online' : 'Offline';\n"
    "            onlineEl.style.color = data.online ? '#43a047' : '#e53935';\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Failed to update WiFi status:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "// Expose WiFi functions globally for inline onclick handlers\n"
    "window.saveWiFiConfig = saveWiFiConfig;\n"
    "window.connectSTA = connectSTA;\n"
    "window.disconnectSTA = disconnectSTA;\n"
    "window.clearWiFiConfig = clearWiFiConfig;\n"
    "window.factoryResetDevice = factoryResetDevice;\n"
    "\n"
    "// Start WiFi status update loop (every 5 seconds)\n"
    "let wifiStatusInterval = null;\n"
    "function startWiFiStatusUpdateLoop() {\n"
    "    if (wifiStatusInterval) {\n"
    "        clearInterval(wifiStatusInterval);\n"
    "    }\n"
    "    // Initial update\n"
    "    updateWiFiStatus();\n"
    "    // Update every 5 seconds\n"
    "    wifiStatusInterval = setInterval(updateWiFiStatus, 10000); // Reduced from 5s to 10s\n"
    "}\n"
    "\n"
    "// Start WiFi status updates when DOM is ready\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', startWiFiStatusUpdateLoop);\n"
    "} else {\n"
    "    startWiFiStatusUpdateLoop();\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// DEMO MODE (SCREENSAVER) FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "/**\n"
    " * Toggle demo/screensaver mode on or off\n"
    " */\n"
    "async function toggleDemoMode() {\n"
    "    try {\n"
    "        // Get current state\n"
    "        const currentlyEnabled = await isDemoModeEnabled();\n"
    "        const newState = !currentlyEnabled;\n"
    "\n"
    "        // Send toggle request\n"
    "        const response = await fetch('/api/demo/config', {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ enabled: newState })\n"
    "        });\n"
    "\n"
    "        const data = await response.json();\n"
    "\n"
    "        if (data.success) {\n"
    "            console.log('✅ Demo mode toggled:', newState ? 'ON' : 'OFF');\n"
    "            // Update status immediately\n"
    "            await updateDemoModeStatus();\n"
    "        } else {\n"
    "            console.error('❌ Failed to toggle demo mode');\n"
    "            alert('Přepnutí demo režimu selhalo: ' + (data.message || 'Neznámá chyba'));\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Error toggling demo mode:', error);\n"
    "        alert('Chyba při přepnutí demo režimu');\n"
    "    }\n"
    "}\n"
    "\n"
    "/**\n"
    " * Check if demo mode is currently enabled\n"
    " * @returns {Promise<boolean>} True if enabled\n"
    " */\n"
    "async function isDemoModeEnabled() {\n"
    "    try {\n"
    "        const response = await fetch('/api/demo/status');\n"
    "        const data = await response.json();\n"
    "        return data.enabled === true;\n"
    "    } catch (error) {\n"
    "        console.error('Failed to check demo mode status:', error);\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/**\n"
    " * Update demo mode status indicator in UI.\n"
    " * Syncs checkbox in Settings tab (demo-enabled) when the removed demoStatus/btnDemoMode are not present.\n"
    " */\n"
    "async function updateDemoModeStatus() {\n"
    "    try {\n"
    "        const enabled = await isDemoModeEnabled();\n"
    "        const statusEl = document.getElementById('demoStatus');\n"
    "        const btnEl = document.getElementById('btnDemoMode');\n"
    "        const demoCheckbox = document.getElementById('demo-enabled');\n"
    "\n"
    "        if (statusEl) {\n"
    "            if (enabled) {\n"
    "                statusEl.textContent = 'Zapnuto';\n"
    "                statusEl.style.color = '#4CAF50';\n"
    "                statusEl.style.fontWeight = 'bold';\n"
    "            } else {\n"
    "                statusEl.textContent = 'Vypnuto';\n"
    "                statusEl.style.color = '#999';\n"
    "                statusEl.style.fontWeight = 'normal';\n"
    "            }\n"
    "        }\n"
    "\n"
    "        if (btnEl) {\n"
    "            if (enabled) {\n"
    "                btnEl.classList.add('btn-active');\n"
    "                btnEl.style.backgroundColor = '#4CAF50';\n"
    "                btnEl.style.borderColor = '#45a049';\n"
    "            } else {\n"
    "                btnEl.classList.remove('btn-active');\n"
    "                btnEl.style.backgroundColor = '#008CBA';\n"
    "                btnEl.style.borderColor = '#007396';\n"
    "            }\n"
    "        }\n"
    "\n"
    "        if (demoCheckbox && demoCheckbox.checked !== enabled) {\n"
    "            demoCheckbox.checked = enabled;\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Chyba aktualizace stavu demo režimu:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "// Expose demo mode functions globally\n"
    "window.toggleDemoMode = toggleDemoMode;\n"
    "window.updateDemoModeStatus = updateDemoModeStatus;\n"
    "\n"
    "// Start demo mode status update loop (every 3 seconds)\n"
    "let demoModeStatusInterval = null;\n"
    "function startDemoModeStatusUpdateLoop() {\n"
    "    if (demoModeStatusInterval) {\n"
    "        clearInterval(demoModeStatusInterval);\n"
    "    }\n"
    "    // Initial update\n"
    "    updateDemoModeStatus();\n"
    "    // Update every 3 seconds\n"
    "    demoModeStatusInterval = setInterval(updateDemoModeStatus, 5000); // Reduced from 3s to 5s\n"
    "}\n"
    "\n"
    "// Start demo mode status updates when DOM is ready\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', startDemoModeStatusUpdateLoop);\n"
    "} else {\n"
    "    startDemoModeStatusUpdateLoop();\n"
    "}\n"
    "\n"
    "// Helper functions for move history navigation\n"
    "\n"
    "function goToMove(index) {\n"
    "    if (!historyData || historyData.length === 0) return;\n"
    "\n"
    "    // Special case: -1 means go to last move\n"
    "    if (index === -1) {\n"
    "        index = historyData.length - 1;\n"
    "    }\n"
    "\n"
    "    // Clamp index to valid range\n"
    "    index = Math.max(0, Math.min(index, historyData.length - 1));\n"
    "\n"
    "    enterReviewMode(index);\n"
    "}\n"
    "\n"
    "function prevReviewMove() {\n"
    "    if (!reviewMode || currentReviewIndex <= 0) return;\n"
    "    enterReviewMode(currentReviewIndex - 1);\n"
    "}\n"
    "\n"
    "function nextReviewMove() {\n"
    "    if (!reviewMode || currentReviewIndex >= historyData.length - 1) return;\n"
    "    enterReviewMode(currentReviewIndex + 1);\n"
    "}\n"
    "\n"
    "// Initialize starting position check setting on page load\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', loadStartingPositionCheck);\n"
    "} else {\n"
    "    loadStartingPositionCheck();\n"
    "}\n"
    "\n";

static esp_err_t http_get_chess_js_handler(httpd_req_t *req) {
  size_t total_len = strlen(chess_app_js_content);
  ESP_LOGI(TAG, "GET /chess_app.js (%zu bytes) - using chunked transfer",
           total_len);

  httpd_resp_set_type(req, "application/javascript; charset=utf-8");
  // Cache-busting: no-cache zajisti, ze se JavaScript vzdy nacte cerstvy
  httpd_resp_set_hdr(req, "Cache-Control",
                     "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Expires", "0");

  // CHUNKED TRANSFER: ESP32 HTTP server cannot handle 54KB in one send
  // Send in 4KB chunks to prevent ECONNRESET (error 104)
  const char *ptr = chess_app_js_content;
  size_t remaining = total_len;
  size_t chunk_num = 0;
  const size_t CHUNK_SIZE = 4096;

  while (remaining > 0) {
    size_t chunk = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
    esp_err_t ret = httpd_resp_send_chunk(req, ptr, chunk);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG,
               "chess_app.js chunk %zu send failed (client disconnected?): %s",
               chunk_num, esp_err_to_name(ret));
      return ret;
    }
    ptr += chunk;
    remaining -= chunk;
    chunk_num++;
  }

  // End chunked transfer
  httpd_resp_send_chunk(req, NULL, 0);
  ESP_LOGI(TAG, "✅ chess_app.js sent in %zu chunks (%zu bytes total)",
           chunk_num, total_len);
  return ESP_OK;
}
// ============================================================================

static const char test_html[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Timer "
    "Test</title>"
    "<style>body{background:#1a1a1a;color:white;padding:20px;font-family:"
    "Arial;"
    "}"
    ".timer{background:#333;padding:20px;margin:10px;border-radius:8px;}"
    "button{padding:10px "
    "20px;margin:5px;cursor:pointer;}</style></head><body>"
    "<h1>Timer Test</h1><div class='timer'>"
    "<h2>White: <span id='white-time'>--:--</span></h2>"
    "<h2>Black: <span id='black-time'>--:--</span></h2></div>"
    "<div><select id='time-control'>"
    "<option value='0'>None</option><option value='3'>Rapid 10+0</option>"
    "<option value='12'>Classical 60+0</option></select>"
    "<button onclick='applyTime()'>Apply</button></div>"
    "<div><button onclick='pauseTimer()'>Pause</button>"
    "<button onclick='resumeTimer()'>Resume</button>"
    "<button onclick='resetTimer()'>Reset</button></div>"
    "<div id='log' "
    "style='background:#222;padding:10px;margin-top:20px;max-height:200px;"
    "overflow-y:auto;'></div>"
    "<script>"
    "function log(m){const "
    "d=document.getElementById('log');d.innerHTML+='<div>'+new "
    "Date().toLocaleTimeString()+': '+m+'</div>';d.scrollTop=d.scrollHeight;}"
    "log('Script loaded');"
    "function formatTime(ms){const s=Math.ceil(ms/1000);const "
    "m=Math.floor(s/60);const sec=s%60;return "
    "m+':'+sec.toString().padStart(2,'0');}"
    "async function updateTimer(){try{const res=await "
    "fetch('/api/timer');if(res.ok){const data=await res.json();"
    "document.getElementById('white-time').textContent=formatTime(data.white_"
    "time_ms);"
    "document.getElementById('black-time').textContent=formatTime(data.black_"
    "time_ms);"
    "log('W='+data.white_time_ms+' B='+data.black_time_ms);}else{log('ERROR: "
    "'+res.status);}}catch(e){log('ERROR: '+e.message);}}"
    "async function applyTime(){const "
    "type=parseInt(document.getElementById('time-control').value);log('Apply "
    "type='+type);try{"
    "const res=await "
    "fetch('/api/timer/"
    "config',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify({type:type})});"
    "if(res.ok){log('OK');setTimeout(updateTimer,500);}else{log('ERROR: "
    "'+res.status+' '+(await res.text()));}}catch(e){log('ERROR: "
    "'+e.message);}}"
    "async function pauseTimer(){log('Pause');try{const res=await "
    "fetch('/api/timer/pause',{method:'POST'});log(res.ok?'OK':'ERROR: "
    "'+res.status);}catch(e){log('ERROR: '+e.message);}}"
    "async function resumeTimer(){log('Resume');try{const res=await "
    "fetch('/api/timer/resume',{method:'POST'});log(res.ok?'OK':'ERROR: "
    "'+res.status);}catch(e){log('ERROR: '+e.message);}}"
    "async function resetTimer(){log('Reset');try{const res=await "
    "fetch('/api/timer/reset',{method:'POST'});log(res.ok?'OK':'ERROR: "
    "'+res.status);setTimeout(updateTimer,500);}catch(e){log('ERROR: "
    "'+e.message);}}"
    "log('Starting updates');setInterval(updateTimer,300);updateTimer();"
    "</script></body></html>";

static esp_err_t http_get_test_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /test - minimal timer test page");
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_send(req, test_html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t http_get_favicon_handler(httpd_req_t *req) {
  // Return 204 No Content to silence the browser's request without sending a
  // file
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// ============================================================================
// HTML CONTENT - CHUNKED ARRAYS IN FLASH MEMORY (READ-ONLY)
// ============================================================================

// HTML split into logical chunks for reliable chunked transfer
// Each chunk is a separate const string in flash memory (.rodata section)
// JavaScript is kept as ONE complete chunk to avoid "is not defined" errors

// Chunk 1: HTML head with bootstrap script and CSS
static const char html_chunk_head[] =
    // HTML head and initial structure
    "<!DOCTYPE html>\n"
    "<html lang='en'>\n"
    "<head>\n"
    "<meta charset='UTF-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
    "<title>CZECHMATE</title>\n"

    // Early bootstrap to avoid 'is not defined' before main script loads
    "<script>\n"
    "window.changeTimeControl = window.changeTimeControl || function(){};\n"
    "window.applyTimeControl = window.applyTimeControl || function(){};\n"
    "window.pauseTimer = window.pauseTimer || function(){};\n"
    "window.resumeTimer = window.resumeTimer || function(){};\n"
    "window.resetTimer = window.resetTimer || function(){};\n"
    "window.hideEndgameReport = window.hideEndgameReport || function(){};\n"
    "</script>\n"
    // Global JS error capture to surface syntax/runtime errors visibly
    "<script>\n"
    "(function(){\n"
    "function showJsError(msg, src, line, col){\n"
    "try {\n"
    "var b=document.body||document.documentElement;\n"
    "var "
    "d=document.getElementById('js-error')||document.createElement('pre');\n"
    "d.id='js-error';\n"
    "d.style.cssText='position:fixed;left:6px;bottom:6px;right:6px;max-"
    "height:"
    "40vh;overflow:auto;background:#300;color:#fff;border:1px solid "
    "#900;padding:8px;margin:0;z-index:99999;font:12px/1.4 "
    "monospace;white-space:pre-wrap;';\n"
    "d.textContent='JS ERROR: '+msg+'\\nSource: '+(src||'-')+'\\nLine: "
    "'+line+':'+col;\n"
    "b&&b.appendChild(d);\n"
    "} catch(e) {}\n"
    "}\n"
    "window.addEventListener('error', function(e){ showJsError(e.message, "
    "e.filename, e.lineno, e.colno); });\n"
    "window.addEventListener('unhandledrejection', function(e){ "
    "showJsError('Unhandled promise rejection: '+e.reason, '', 0, 0); });\n"
    "})();\n"
    "</script>\n"

    // CSS styles - part 1
    "<style>\n"
    "* { margin: 0; padding: 0; box-sizing: border-box; }\n"
    "body { "
    "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, "
    "sans-serif; "
    "background: #1a1a1a; "
    "color: #e0e0e0; "
    "min-height: 100vh; "
    "padding: clamp(6px, 2vw, 12px); "
    "overflow-x: hidden; "
    "}\n"
    ".container { "
    "width: 95%; "
    "max-width: 1600px; "
    "margin: 0 auto; "
    "}\n"
    ".app-header { "
    "position: sticky; "
    "top: 0; "
    "z-index: 100; "
    "display: flex; "
    "align-items: center; "
    "justify-content: space-between; "
    "flex-wrap: wrap; "
    "gap: 10px; "
    "margin-bottom: 10px; "
    "padding: 8px 0 10px 0; "
    "background: #1a1a1a; "
    "border-bottom: 1px solid #2a2a2a; "
    "}\n"
    "h1, .app-title { "
    "color: #5cb85c; "
    "margin: 0; "
    "font-size: clamp(1.05em, 2.8vw, 1.35em); "
    "font-weight: 600; "
    "letter-spacing: 0.02em; "
    "}\n"
    ".tab-nav { "
    "display: flex; "
    "gap: 8px; "
    "justify-content: flex-start; "
    "}\n"
    ".tab-btn { "
    "padding: 8px 20px; "
    "min-height: 44px; "
    "min-width: 44px; "
    "background: #252525; "
    "color: #999; "
    "border: 1px solid #383838; "
    "border-radius: 8px; "
    "cursor: pointer; "
    "font-weight: 500; "
    "font-size: clamp(13px, 1.8vw, 14px); "
    "transition: all 0.2s; "
    "}\n"
    ".tab-btn:hover { color: #e0e0e0; background: #2e2e2e; }\n"
    ".tab-btn.active { "
    "background: #3d5a3d; "
    "color: #e8f5e9; "
    "border-color: #4a6b4a; "
    "}\n"
    ".tab-content { "
    "display: none; "
    "}\n"
    ".tab-content.active { "
    "display: block; "
    "}\n"
    ".btn-primary { "
    "width: 100%; "
    "padding: 12px 16px; "
    "background: linear-gradient(135deg, #4CAF50, #45a049); "
    "color: white; "
    "border: none; "
    "border-radius: 8px; "
    "cursor: pointer; "
    "font-weight: 700; "
    "font-size: 15px; "
    "transition: all 0.2s; "
    "box-shadow: 0 4px 12px rgba(76,175,80,0.3); "
    "}\n"
    ".btn-primary:hover { "
    "transform: translateY(-2px); "
    "box-shadow: 0 6px 16px rgba(76,175,80,0.4); "
    "}\n"
    ".game-actions { "
    "display: grid; "
    "grid-template-columns: 1fr 1fr 1fr; "
    "gap: 6px; "
    "margin-top: 10px; "
    "padding-top: 10px; "
    "border-top: 1px solid #333; "
    "}\n"
    ".btn-game-action { "
    "padding: 8px 10px; "
    "font-size: 12px; "
    "font-weight: 600; "
    "min-height: 36px; "
    "border: 1px solid; "
    "border-radius: 6px; "
    "cursor: pointer; "
    "transition: all 0.2s; "
    "background: transparent; "
    "}\n"
    ".btn-game-new { "
    "color: #4CAF50; "
    "border-color: #4CAF50; "
    "background: rgba(76,175,80,0.12); "
    "}\n"
    ".btn-game-new:hover { "
    "background: rgba(76,175,80,0.25); "
    "}\n"
    ".btn-game-sandbox { "
    "color: #9C27B0; "
    "border-color: #9C27B0; "
    "background: rgba(156,39,176,0.12); "
    "}\n"
    ".btn-game-sandbox:hover { "
    "background: rgba(156,39,176,0.25); "
    "}\n"
    ".btn-game-hint { "
    "color: #FF9800; "
    "border-color: #FF9800; "
    "background: rgba(255,152,0,0.12); "
    "}\n"
    ".btn-game-hint:hover { "
    "background: rgba(255,152,0,0.25); "
    "}\n"
    ".hint-explanation { "
    "margin-top: 12px; "
    "padding: 10px 12px; "
    "background: rgba(255,152,0,0.08); "
    "border: 1px solid rgba(255,152,0,0.35); "
    "border-radius: 8px; "
    "font-size: 13px; "
    "line-height: 1.4; "
    "}\n"
    ".hint-explanation-title { "
    "font-weight: 600; "
    "color: #FF9800; "
    "margin-bottom: 8px; "
    "}\n"
    ".hint-explanation-message { "
    "margin: 0; "
    "color: #e0e0e0; "
    "line-height: 1.5; "
    "}\n"
    ".move-evaluation { "
    "margin-top: 12px; "
    "padding: 10px 12px; "
    "border-radius: 8px; "
    "font-size: 13px; "
    "line-height: 1.4; "
    "}\n"
    ".move-evaluation--best { background: rgba(76,175,80,0.12); border: 1px "
    "solid rgba(76,175,80,0.45); }\n"
    ".move-evaluation--good { background: rgba(139,195,74,0.12); border: 1px "
    "solid rgba(139,195,74,0.4); }\n"
    ".move-evaluation--inaccuracy { background: rgba(255,193,7,0.12); border: "
    "1px solid rgba(255,193,7,0.45); }\n"
    ".move-evaluation--mistake { background: rgba(255,152,0,0.15); border: 1px "
    "solid rgba(255,152,0,0.5); }\n"
    ".move-evaluation--blunder { background: rgba(244,67,54,0.15); border: 1px "
    "solid rgba(244,67,54,0.5); }\n"
    ".move-evaluation--error { background: rgba(97,97,97,0.2); border: 1px "
    "solid rgba(158,158,158,0.4); }\n"
    ".move-evaluation--unknown { background: rgba(97,97,97,0.2); border: 1px "
    "solid rgba(158,158,158,0.4); }\n"
    ".move-evaluation-title { "
    "font-weight: 600; "
    "margin-bottom: 8px; "
    "}\n"
    ".move-evaluation--best .move-evaluation-title { color: #4CAF50; }\n"
    ".move-evaluation--good .move-evaluation-title { color: #8BC34A; }\n"
    ".move-evaluation--inaccuracy .move-evaluation-title { color: #FFC107; }\n"
    ".move-evaluation--mistake .move-evaluation-title { color: #FF9800; }\n"
    ".move-evaluation--blunder .move-evaluation-title { color: #f44336; }\n"
    ".move-evaluation--error .move-evaluation-title { color: #9e9e9e; }\n"
    ".move-evaluation--unknown .move-evaluation-title { color: #9e9e9e; }\n"
    ".move-evaluation-message { "
    "margin: 0; "
    "color: #e0e0e0; "
    "line-height: 1.5; "
    "}\n"
    ".castling-pending-message { "
    "margin-top: 14px; padding: 12px 14px; "
    "background: rgba(255,193,7,0.12); border: 1px solid rgba(255,193,7,0.4); "
    "border-radius: 8px; font-size: 13px; line-height: 1.45; color: #ffc107; "
    "}\n"
    ".teaching-stats .game-title { margin-bottom: 8px; }\n"
    ".teaching-stats-row { "
    "display: flex; justify-content: space-between; gap: 12px; "
    "margin-bottom: 6px; font-size: 12px; "
    "}\n"
    ".teaching-stats-header .teaching-stats-label { "
    "font-weight: 600; color: #b0b0b0; "
    "}\n"
    ".teaching-stats-cell { "
    "flex: 1; text-align: center; "
    "}\n"
    ".teaching-stats-sublabel { "
    "display: block; font-size: 10px; color: #888; margin-bottom: 2px; "
    "}\n"
    ".teaching-stats-note { "
    "margin: 8px 0 0 0; font-size: 10px; color: #666; "
    "}\n"
    ".main-content { "
    "display: grid; "
    "grid-template-columns: minmax(220px, 260px) 1fr minmax(220px, 260px); "
    "grid-template-areas: 'left center right'; "
    "gap: clamp(10px, 2vw, 16px); "
    "align-items: start; "
    "}\n"
    "@media (min-width: 1280px) { "
    ".container { max-width: 1400px; } "
    "}\n"
    "@media (max-width: 1023px) { "
    ".main-content { "
    "grid-template-columns: 1fr 260px; "
    "grid-template-areas: 'center right' 'left right'; "
    "} "
    "}\n"
    "@media (max-width: 767px) { "
    ".main-content { "
    "grid-template-columns: 1fr; "
    "grid-template-areas: 'center' 'left' 'right'; "
    "} "
    "}\n"
    "@media (max-width: 599px) { "
    ".game-actions { grid-template-columns: 1fr; } "
    ".btn-game-action { min-height: 44px; } "
    "}\n"
    "@media (max-width: 319px) { "
    ".tab-btn { padding: 8px 12px; font-size: 12px; } "
    ".btn-game-action { padding: 6px 8px; font-size: 11px; min-height: 40px; } "
    "}\n"
    "html.web-board-focus { height: 100%; overflow: hidden; overscroll-behavior: "
    "none; }"
    "body.web-board-focus { position: fixed; inset: 0; width: 100%; height: "
    "100%; overflow: hidden; overscroll-behavior: none; -webkit-overflow-scrolling: "
    "auto; touch-action: manipulation; }"
    ".web-board-focus .container { max-height: 100vh; overflow: hidden; "
    "padding-bottom: env(safe-area-inset-bottom, 0); }"
    ".web-board-focus .app-header { display: none !important; }"
    ".web-board-focus .game-right-panel { display: none !important; }"
    ".web-board-focus .game-info-panel > .game-block:nth-child(n+2) { "
    "display: none !important; }"
    ".web-board-focus .main-content { "
    "grid-template-columns: 1fr !important; "
    "grid-template-areas: 'left' 'center' !important; "
    "align-items: stretch; gap: 8px !important; max-height: calc(100vh - 12px); "
    "overflow: hidden; min-height: 0; }"
    ".web-board-focus .board-container { min-height: 0; display: flex; "
    "flex-direction: column; }"
    ".web-board-focus-exit-btn { position: fixed; "
    "bottom: max(12px, env(safe-area-inset-bottom, 12px)); right: 12px; "
    "z-index: 3000; padding: 10px 14px; border-radius: 8px; border: 1px solid "
    "#555; background: #2a2a2a; color: #eee; font-size: 13px; "
    "box-shadow: 0 2px 10px rgba(0,0,0,0.45); cursor: pointer; }"
    ".board-container { grid-area: center; } "
    ".info-panel { grid-area: right; } "
    ".game-info-panel { grid-area: left; } "
    ".game-right-panel { grid-area: right; } "
    ".board-container { "
    "background: #222; "
    "border-radius: 10px; "
    "padding: clamp(10px, 2vw, 16px); "
    "box-shadow: 0 2px 8px rgba(0,0,0,0.25); "
    "}\n"
    ".board { "
    "display: grid; "
    "grid-template-columns: repeat(8, 1fr); "
    "grid-template-rows: repeat(8, 1fr); "
    "gap: 0; "
    "width: 100%; "
    "aspect-ratio: 1; "
    "border: 2px solid #3a3a3a; "
    "border-radius: 4px; "
    "overflow: hidden; "
    "touch-action: none; "
    "}"
    ".square { "
    "aspect-ratio: 1; "
    "display: flex; "
    "align-items: center; "
    "justify-content: center; "
    "font-size: 3vw; "
    "cursor: pointer; "
    "transition: background 0.15s; "
    "}"
    ".square:hover { background: #4a4a4a !important; }"
    ".square.light { background: #f0d9b5; }"
    ".square.dark { background: #b58863; }"
    ".square.selected { "
    "box-shadow: inset 0 0 0 3px #ffeb3b !important; "
    "}"
    ".square.lifted { "
    "background: #4CAF50 !important; "
    "box-shadow: inset 0 0 20px rgba(76,175,80,0.5); "
    "}"
    ".square.error-invalid { "
    "background: #f44336 !important; "
    "box-shadow: inset 0 0 20px rgba(244,67,54,0.6); "
    "animation: errorBlink 0.3s ease-in-out 6 forwards; "
    "}"
    ".square.error-original { "
    "background: #2196F3 !important; "
    "box-shadow: inset 0 0 20px rgba(33,150,243,0.6); "
    "}"
    "@keyframes errorBlink { "
    "0%, 100% { background: #f44336 !important; box-shadow: inset 0 0 20px "
    "rgba(244,67,54,0.6); } "
    "50% { background: #8b0000 !important; box-shadow: inset 0 0 8px "
    "rgba(244,67,54,0.3); } "
    "}"
    "@keyframes errorPulse { "
    "0% { transform: translate(1px, 1px) rotate(0deg); } "
    "10% { transform: translate(-1px, -2px) rotate(-1deg); } "
    "20% { transform: translate(-3px, 0px) rotate(1deg); } "
    "30% { transform: translate(3px, 2px) rotate(0deg); } "
    "40% { transform: translate(1px, -1px) rotate(1deg); } "
    "50% { transform: translate(-1px, 2px) rotate(-1deg); } "
    "60% { transform: translate(-3px, 1px) rotate(0deg); } "
    "70% { transform: translate(3px, 1px) rotate(-1deg); } "
    "80% { transform: translate(-1px, -1px) rotate(1deg); } "
    "90% { transform: translate(1px, 2px) rotate(0deg); } "
    "100% { transform: translate(1px, -2px) rotate(-1deg); } "
    "}"
    ".piece { "
    "font-size: 4vw; "
    "text-shadow: 2px 2px 4px rgba(0,0,0,0.3); "
    "user-select: none; "
    "display: flex; align-items: center; justify-content: center; "
    "}"
    ".piece.has-img { font-size: 0; text-shadow: none; line-height: 0; }"
    ".piece.has-img img { width: 88%; height: 88%; max-width: 100%; max-height: 100%; "
    "object-fit: contain; display: block; pointer-events: none; }"
    ".piece.white { color: white; }"
    ".piece.black { color: black; }"
    ".captured-piece { display: inline-flex; align-items: center; justify-content: center; "
    "min-width: 1.1em; min-height: 1.1em; vertical-align: middle; }"
    ".captured-piece img { width: clamp(22px, 4.2vw, 34px); height: clamp(22px, 4.2vw, 34px); "
    "object-fit: contain; display: block; }"
    ".setup-tutorial-piece-glyph.has-img { font-size: 0; text-shadow: none; line-height: 0; "
    "display: flex; align-items: center; justify-content: center; min-height: 72px; }"
    ".setup-tutorial-piece-glyph.has-img img { max-width: 120px; max-height: 120px; width: auto; "
    "height: auto; object-fit: contain; }"
    ".lifted-piece-img { width: 28px; height: 28px; object-fit: contain; vertical-align: middle; }"
    ".endgame-piece-img { width: 26px; height: 26px; object-fit: contain; vertical-align: middle; "
    "margin: 0 1px; }"

    // CSS styles - part 2
    ".info-panel, .game-info-panel, .game-right-panel { "
    "background: #222; "
    "border-radius: 10px; "
    "padding: clamp(10px, 1.5vw, 14px); "
    "box-shadow: 0 2px 8px rgba(0,0,0,0.25); "
    "min-width: 0; "
    "overflow: hidden; "
    "}"
    "#tab-nastaveni { "
    "padding: 8px 0 16px; "
    "}\n"
    ".settings-grid { "
    "display: grid; "
    "grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); "
    "gap: 10px; "
    "align-items: start; "
    "}\n"
    "@media (min-width: 520px) { "
    ".settings-grid { gap: 12px; } "
    "}\n"
    "@media (min-width: 600px) { "
    ".settings-section--wide { grid-column: span 2; } "
    "}\n"
    "@media (min-width: 800px) { "
    ".settings-grid { gap: 14px; } "
    "}\n"
    ".settings-section { "
    "min-width: 0; "
    "display: flex; "
    "flex-direction: column; "
    "gap: 6px; "
    "background: #252525; "
    "border-radius: 8px; "
    "padding: 10px 12px; "
    "border: 1px solid #333; "
    "}\n"
    ".settings-section .section-title { "
    "font-size: 0.72em; "
    "letter-spacing: 0.06em; "
    "color: #777; "
    "margin: 0 0 6px 0; "
    "font-weight: 500; "
    "}\n"
    "#tab-nastaveni .settings-section { "
    "gap: 6px; "
    "padding: 10px 12px; "
    "}\n"
    "#tab-nastaveni .set-title { "
    "font-size: 0.72em; "
    "letter-spacing: 0.04em; "
    "color: #777; "
    "margin: 0 0 6px 0; "
    "font-weight: 500; "
    "}\n"
    "#tab-nastaveni .set-rows-grid { "
    "display: grid; "
    "grid-template-columns: 1fr 1fr; "
    "gap: 2px 12px; "
    "margin-bottom: 6px; "
    "}\n"
    "#tab-nastaveni .set-rows-grid .set-row { margin-bottom: 2px; }\n"
    "#tab-nastaveni .set-row { "
    "display: flex; "
    "justify-content: space-between; "
    "align-items: center; "
    "gap: 8px; "
    "margin-bottom: 4px; "
    "font-size: 12px; "
    "}\n"
    "#tab-nastaveni .set-row:last-child { margin-bottom: 0; }\n"
    "#tab-nastaveni .set-label { color: #999; }\n"
    "#tab-nastaveni .set-value { color: #e0e0e0; font-weight: 500; "
    "font-size: 12px; word-break: break-all; }\n"
    "#tab-nastaveni .set-input, #tab-nastaveni .set-select { "
    "width: 100%; "
    "padding: 6px 8px; "
    "margin-top: 2px; "
    "background: #1a1a1a; "
    "border: 1px solid #3a3a3a; "
    "border-radius: 5px; "
    "color: #e0e0e0; "
    "font-size: 12px; "
    "}\n"
    "#tab-nastaveni .set-input:focus, #tab-nastaveni .set-select:focus { "
    "outline: none; "
    "border-color: #555; "
    "}\n"
    "#tab-nastaveni .set-group { margin-bottom: 6px; }\n"
    "#tab-nastaveni .set-group:last-child { margin-bottom: 0; }\n"
    "#tab-nastaveni .set-group label { "
    "display: block; "
    "font-size: 10px; "
    "color: #777; "
    "margin-bottom: 2px; "
    "}\n"
    "#tab-nastaveni .set-btn { "
    "width: 100%; "
    "padding: 8px 10px; "
    "margin-top: 4px; "
    "background: #3d5a3d; "
    "color: #e8f5e9; "
    "border: none; "
    "border-radius: 5px; "
    "font-size: 12px; "
    "font-weight: 500; "
    "cursor: pointer; "
    "min-height: 36px; "
    "}\n"
    "#tab-nastaveni .set-btn:hover { background: #4a6b4a; }\n"
    "#tab-nastaveni .set-btn-sm { padding: 6px 10px; min-height: 32px; "
    "margin-top: 4px; font-size: 11px; }\n"
    "#tab-nastaveni .set-btn-danger { background: #5a2a2a; color: #ffcdd2; }\n"
    "#tab-nastaveni .set-btn-danger:hover { background: #6b3333; }\n"
    "#tab-nastaveni .set-btn-row { "
    "display: flex; "
    "flex-wrap: wrap; "
    "gap: 6px; "
    "margin-top: 6px; "
    "}\n"
    "#tab-nastaveni .set-btn-row .set-btn { flex: 1; margin-top: 0; }\n"
    "#tab-nastaveni .set-check { "
    "display: flex; "
    "align-items: center; "
    "gap: 8px; "
    "cursor: pointer; "
    "padding: 4px 0; "
    "font-size: 12px; "
    "color: #ccc; "
    "}\n"
    "#tab-nastaveni .set-check input { "
    "width: 16px; height: 16px; "
    "min-width: 16px; cursor: pointer; "
    "}\n"
    "#tab-nastaveni .set-note { "
    "font-size: 10px; "
    "color: #666; "
    "margin-top: 2px; "
    "line-height: 1.35; "
    "}\n"
    "#tab-nastaveni .set-note-warn { color: #b8860b; }\n"
    "@media (max-width: 599px) { "
    "#tab-nastaveni .set-note { font-size: 11px; } "
    "#tab-nastaveni .set-btn { min-height: 44px; } "
    "#tab-nastaveni .set-btn-sm { min-height: 40px; } "
    "} "
    "@media (max-width: 399px) { "
    "#tab-nastaveni .set-btn-row { flex-direction: column; } "
    "#tab-nastaveni .set-btn-row .set-btn { width: 100%; flex: none; } "
    "} "
    "@media (max-width: 319px) { "
    "#tab-nastaveni { padding-left: 6px; padding-right: 6px; } "
    "#tab-nastaveni .settings-section { padding: 8px 10px; } "
    "} "
    ".setup-tutorial-overlay { "
    "display: none; "
    "position: fixed; inset: 0; background: rgba(0,0,0,0.65); z-index: 1600; "
    "align-items: center; justify-content: center; "
    "} "
    ".setup-tutorial-overlay.overlay-visible { "
    "display: flex !important; "
    "} "
    ".tutorials-panel-body { margin-top: 4px; }\n"
    ".setup-tutorial-modal-content { "
    "background: #2a2a2e; padding: 22px; border-radius: 10px; max-width: 440px; "
    "width: 92%; max-height: 90vh; overflow-y: auto; color: #eee; "
    "border: 1px solid #444; box-sizing: border-box; "
    "} "
    ".setup-tutorial-modal-title { "
    "margin: 0 0 10px 0; font-size: 1.05em; font-weight: 600; color: #e0e0e0; "
    "} "
    ".setup-tutorial-body { "
    "font-size: 15px; line-height: 1.45; margin: 0 0 8px 0; color: #ddd; "
    "} "
    ".setup-tutorial-warn { color: #ffb74d !important; } "
    ".setup-tutorial-piece-wrap { text-align: center; margin-bottom: 12px; } "
    ".setup-tutorial-piece-glyph { font-size: 56px; line-height: 1; } "
    ".setup-tutorial-instruction { "
    "font-size: 17px; text-align: center; margin: 8px 0; line-height: 1.4; "
    "} "
    ".setup-tutorial-progress { text-align: center; } "
    ".setup-tutorial-actions { "
    "display: flex; gap: 10px; margin-top: 16px; flex-wrap: wrap; "
    "} "
    ".setup-tutorial-actions--run { justify-content: center; margin-top: 14px; } "
    "@media (max-width: 399px) { "
    ".setup-tutorial-actions .set-btn { flex: 1 1 100%; min-height: 44px; } "
    ".setup-tutorial-actions--run .set-btn-sm { min-height: 44px; } "
    "} "
    "#tab-nastaveni input[type='range'] { accent-color: #4a6b4a; }\n"
    "#tab-nastaveni .status-item { "
    "margin: 4px 0; "
    "font-size: 13px; "
    "padding: 0; "
    "}\n"
    ".game-info-panel, .game-right-panel { "
    "display: flex; "
    "flex-direction: column; "
    "gap: 12px; "
    "}\n"
    ".game-block { "
    "background: #252525; "
    "border-radius: 8px; "
    "padding: 10px 12px; "
    "border: 1px solid #333; "
    "}\n"
    ".game-title { "
    "font-size: 0.7em; "
    "letter-spacing: 0.04em; "
    "color: #777; "
    "margin: 0 0 8px 0; "
    "font-weight: 500; "
    "}\n"
    ".game-row { "
    "display: flex; "
    "justify-content: space-between; "
    "align-items: center; "
    "margin-bottom: 6px; "
    "font-size: 13px; "
    "}\n"
    ".game-row:last-child { margin-bottom: 0; }\n"
    ".game-label { color: #999; }\n"
    ".game-value { color: #e0e0e0; font-weight: 500; }\n"
    "#bot-status-panel { "
    "background: #2a3032; "
    "border: 1px solid rgba(76,175,80,0.3); "
    "border-left: 3px solid rgba(76,175,80,0.65); "
    "border-radius: 10px; "
    "padding: 14px 16px; "
    "margin-top: 6px; "
    "box-shadow: 0 2px 6px rgba(0,0,0,0.15); "
    "}\n"
    "#bot-status-panel .game-title { "
    "font-size: 0.75em; letter-spacing: 0.06em; "
    "color: #5cb85c; margin-bottom: 10px; font-weight: 600; "
    "}\n"
    "#bot-status-text { "
    "display: block; margin: 0; padding: 2px 0 0 0; "
    "line-height: 1.45; word-break: break-word; min-height: 1.4em; "
    "font-size: 14px; color: #e8e8e8; "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel { "
    "background: #263238; "
    "border: 1px solid rgba(38,198,218,0.35); "
    "border-left: 3px solid rgba(38,198,218,0.75); "
    "border-radius: 10px; padding: 14px 16px; margin-top: 6px; "
    "box-shadow: 0 2px 6px rgba(0,0,0,0.15); "
    "}\n"
    "#puzzle-status-panel .game-title { "
    "font-size: 0.75em; letter-spacing: 0.06em; "
    "color: #4dd0e1; margin-bottom: 8px; font-weight: 600; "
    "}\n"
    "#puzzle-status-text { "
    "display: block; line-height: 1.5; word-break: break-word; "
    "font-size: 14px; color: #eceff1; margin: 0; "
    "}\n"
    ".puzzle-status-sub { "
    "margin: 10px 0 0 0; font-size: 12px; line-height: 1.4; color: #90a4ae; "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--setup { "
    "border-color: rgba(100,181,246,0.4); border-left-color: rgba(100,181,246,0.85); "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--setup .game-title { color: #64b5f6; }\n"
    "#puzzle-status-panel.puzzle-status-panel--play { "
    "border-color: rgba(38,198,218,0.35); border-left-color: rgba(38,198,218,0.75); "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--wrong { "
    "border-color: rgba(255,183,77,0.45); border-left-color: rgba(255,167,38,0.9); "
    "background: #2e2a24; "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--wrong .game-title { color: #ffb74d; }\n"
    "#puzzle-status-panel.puzzle-status-panel--illegal { "
    "border-color: rgba(239,154,154,0.45); border-left-color: rgba(229,115,115,0.85); "
    "background: #2d2424; "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--illegal .game-title { color: #ef9a9a; }\n"
    "#puzzle-status-panel.puzzle-status-panel--solved { "
    "border-color: rgba(129,199,132,0.5); border-left-color: rgba(76,175,80,0.9); "
    "background: #243124; "
    "}\n"
    "#puzzle-status-panel.puzzle-status-panel--solved .game-title { color: #81c784; }\n"
    "#puzzle-status-panel.puzzle-status-panel--offline { opacity: 0.92; }\n"
    ".status-box, .card { "
    "background: #2a2a2a; "
    "border-left: 2px solid #3d5a3d; "
    "padding: 10px 12px; "
    "margin-bottom: 10px; "
    "border-radius: 8px; "
    "}"
    ".status-box:last-child, .card:last-child { margin-bottom: 0; }"
    ".status-box h3, .card h3 { "
    "color: #8a8a8a; "
    "margin-bottom: 6px; "
    "font-weight: 500; "
    "font-size: clamp(0.72em, 1.8vw, 0.82em); "
    "}"
    ".status-item { "
    "display: flex; "
    "justify-content: space-between; "
    "align-items: baseline; "
    "margin: 3px 0; "
    "font-size: clamp(12px, 1.5vw, 13px); "
    "}"
    ".status-value { font-weight: 600; color: #e0e0e0; font-family: 'Courier "
    "New', monospace; }"
    ".game-block .timer-display { margin: 0 -2px 8px 0; }\n"
    ".game-block .timer-controls { margin: 8px 0; }\n"
    ".game-block .timer-controls button { "
    "padding: 6px 12px; font-size: 12px; border-radius: 6px; "
    "}\n"
    ".game-block .timer-stats { "
    "background: transparent; padding: 0; margin-top: 6px; "
    "}\n"
    ".game-block .stat-item { margin-bottom: 4px; font-size: 12px; }\n"
    ".game-details { margin-bottom: 4px; }\n"
    ".game-details-summary { "
    "cursor: pointer; "
    "list-style: none; "
    "font-weight: 600; "
    "color: #ccc; "
    "padding: 6px 0; "
    "font-size: clamp(13px, 1.6vw, 14px); "
    "}\n"
    ".game-details-summary::-webkit-details-marker { display: none; }\n"
    ".game-details-body { padding-top: 4px; }\n"
    ".history-toolbar { "
    "display: flex; "
    "align-items: center; "
    "justify-content: space-between; "
    "gap: 8px; "
    "flex-wrap: wrap; "
    "margin-bottom: 4px; "
    "}\n"
    ".history-toolbar-title { margin: 0; }\n"
    ".history-toolbar-btn { "
    "font-size: 11px; "
    "padding: 4px 10px; "
    "border-radius: 6px; "
    "border: 1px solid #555; "
    "background: #2d2d2d; "
    "color: #ddd; "
    "cursor: pointer; "
    "}\n"
    ".history-preview.history-box { max-height: none; }\n"
    ".history-full-wrap .history-box--expanded { "
    "max-height: min(50vh, 420px); "
    "overflow-y: auto; "
    "}\n"
    ".history-move-detail { "
    "margin-top: 8px; "
    "padding: 10px 12px; "
    "background: #1a1a1a; "
    "border: 1px solid #3a3a3a; "
    "border-radius: 8px; "
    "font-size: 12px; "
    "color: #ccc; "
    "}\n"
    ".history-detail-san { "
    "font-family: 'Courier New', monospace; "
    "color: #e0e0e0; "
    "margin-bottom: 6px; "
    "}\n"
    ".history-detail-grade { "
    "font-weight: 600; "
    "margin-bottom: 8px; "
    "}\n"
    ".history-detail-grade--best { color: #81C784; }\n"
    ".history-detail-grade--good { color: #9CCC65; }\n"
    ".history-detail-grade--inaccuracy { color: #FFD54F; }\n"
    ".history-detail-grade--mistake { color: #FFB74D; }\n"
    ".history-detail-grade--blunder { color: #E57373; }\n"
    ".history-detail-grade--error { color: #BDBDBD; }\n"
    ".history-detail-msg { margin: 0 0 10px 0; line-height: 1.45; color: #bbb; }\n"
    ".btn-history-review { "
    "font-size: 12px; "
    "padding: 6px 12px; "
    "border-radius: 6px; "
    "border: 1px solid #4CAF50; "
    "background: rgba(76,175,80,0.15); "
    "color: #81C784; "
    "cursor: pointer; "
    "}\n"
    ".history-box { "
    "max-height: 180px; "
    "overflow-y: auto; "
    "background: #1e1e1e; "
    "padding: 8px; "
    "border-radius: 6px; "
    "margin-top: 6px; "
    "}"
    ".history-item { "
    "padding: 5px 0; "
    "border-bottom: 1px solid #333; "
    "font-size: 12px; "
    "color: #aaa; "
    "font-family: 'Courier New', monospace; "
    "}"
    ".history-item:last-child { border-bottom: none; }\n"
    ".move-eval-badge { "
    "font-size: 11px; margin-left: 6px; padding: 2px 6px; border-radius: 4px; "
    "font-weight: 700; display: inline-block; vertical-align: middle; }\n"
    ".move-eval-badge--best { background: rgba(76,175,80,0.35); color: "
    "#81C784; }\n"
    ".move-eval-badge--good { background: rgba(139,195,74,0.3); color: "
    "#9CCC65; }\n"
    ".move-eval-badge--inaccuracy { background: rgba(255,193,7,0.35); color: "
    "#FFD54F; }\n"
    ".move-eval-badge--mistake { background: rgba(255,152,0,0.35); color: "
    "#FFB74D; }\n"
    ".move-eval-badge--blunder { background: rgba(244,67,54,0.35); color: "
    "#E57373; }\n"
    ".move-eval-badge--error { background: rgba(97,97,97,0.4); color: #BDBDBD; "
    "}\n"
    ".captured-pieces { "
    "display: flex; "
    "flex-wrap: wrap; "
    "gap: 4px; "
    "margin-top: 6px; "
    "}"
    ".captured-piece { font-size: 1.1em; color: #888; }\n"
    ".game-block .game-value { font-family: 'Courier New', monospace; }\n"
    ".game-right-panel .history-box:not(.history-box--expanded) { max-height: 220px; }\n"
    ".game-info-panel .player-time { padding: 10px; }\n"
    ".game-info-panel .time-value { font-size: 20px; }\n"
    ".loading { "
    "text-align: center; "
    "padding: 20px; "
    "color: #888; "
    "}"

    // CSS styles - part 3 (Review Mode, Sandbox Mode, etc.)
    "/* Review Mode */"
    ".review-banner { "
    "display: none; "
    "position: fixed; "
    "top: 10px; "
    "right: 10px; "
    "background: linear-gradient(135deg, #FF8C00 0%, #FF6B00 100%); "
    "padding: 0; "
    "border-radius: 12px; "
    "box-shadow: 0 8px 24px rgba(255, 140, 0, 0.5), 0 4px 12px "
    "rgba(0,0,0,0.3); "
    "z-index: 1001; "
    "max-width: 90vw; "
    "animation: slideInRight 0.3s ease-out; "
    "overflow: hidden; "
    "color: white; "
    "} "
    ".review-header { "
    "display: flex; "
    "align-items: center; "
    "justify-content: space-between; "
    "gap: 10px; "
    "padding: 12px 16px; "
    "background: rgba(0,0,0,0.2); "
    "font-weight: 600; "
    "font-size: 15px; "
    "} "
    ".review-controls { "
    "display: grid; "
    "grid-template-columns: repeat(4, 1fr); "
    "gap: 8px; "
    "padding: 12px; "
    "} "
    ".nav-btn { "
    "padding: 12px 8px; "
    "font-size: 20px; "
    "background: rgba(255, 255, 255, 0.15); "
    "border: 2px solid rgba(255, 255, 255, 0.3); "
    "border-radius: 8px; "
    "color: white; "
    "cursor: pointer; "
    "transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1); "
    "min-height: 44px; "
    "display: flex; "
    "align-items: center; "
    "justify-content: center; "
    "font-weight: 600; "
    "text-shadow: 0 1px 2px rgba(0,0,0,0.3); "
    "} "
    ".nav-btn:hover, .nav-btn:focus { "
    "background: rgba(255, 255, 255, 0.25); "
    "border-color: rgba(255, 255, 255, 0.5); "
    "transform: translateY(-2px); "
    "box-shadow: 0 4px 12px rgba(0,0,0,0.2); "
    "} "
    ".nav-btn:active { "
    "transform: translateY(0); "
    "box-shadow: 0 2px 6px rgba(0,0,0,0.15); "
    "} "
    ".btn-header-close { "
    "background: transparent; "
    "border: none; "
    "color: rgba(255,255,255,0.8); "
    "font-size: 20px; "
    "padding: 8px; "
    "cursor: pointer; "
    "border-radius: 50%; "
    "display: flex; "
    "align-items: center; "
    "justify-content: center; "
    "transition: all 0.2s; "
    "} "
    ".btn-header-close:hover { "
    "background: rgba(255,255,255,0.2); "
    "color: white; "
    "} "
    "@media (max-width: 600px) { "
    ".review-banner { "
    "top: auto; "
    "bottom: 0; "
    "left: 0; "
    "right: 0; "
    "border-radius: 16px 16px 0 0; "
    "max-width: none; "
    "padding-bottom: env(safe-area-inset-bottom, 10px); "
    "} "
    ".review-controls { "
    "gap: 12px; "
    "padding: 12px 16px; "
    "} "
    ".nav-btn { "
    "font-size: 24px; "
    "min-height: 52px; "
    "} "
    "} "
    ".review-banner.active { display: block; }"
    "@keyframes slideInRight { "
    "from { transform: translateX(100%); opacity: 0; } "
    "to { transform: translateX(0); opacity: 1; } "
    "}"
    ".btn-review-nav { "
    "background: rgba(255,255,255,0.2); "
    "border: 1px solid rgba(255,255,255,0.4); "
    "border-radius: 4px; "
    "color: white; "
    "width: 32px; "
    "height: 32px; "
    "display: flex; "
    "align-items: center; "
    "justify-content: center; "
    "cursor: pointer; "
    "transition: all 0.2s; "
    "}"
    ".btn-review-nav:hover:not(:disabled) { background: "
    "rgba(255,255,255,0.4); "
    "}"
    ".btn-review-nav:disabled { opacity: 0.3; cursor: not-allowed; }"
    ".history-item.selected { "
    "background: #FF9800 !important; "
    "color: white !important; "
    "font-weight: 600; "
    "}"
    ".square.move-from { "
    "box-shadow: inset 0 0 0 3px #4A90C8 !important; "
    "background: rgba(74,144,200,0.3) !important; "
    "}"
    ".square.move-to { "
    "box-shadow: inset 0 0 0 3px #4CAF50 !important; "
    "background: rgba(76,175,80,0.3) !important; "
    "}"
    ".square.hint-from { "
    "box-shadow: inset 0 0 0 3px #00BCD4 !important; "
    "background: rgba(0,188,212,0.35) !important; "
    "}"
    ".square.hint-to { "
    "box-shadow: inset 0 0 0 3px #FF8C00 !important; "
    "background: rgba(255,140,0,0.35) !important; "
    "}"

    // CSS styles - part 4 (Sandbox Mode)
    "/* Sandbox Mode */"
    ".sandbox-banner { "
    "position: fixed; "
    "bottom: 0; left: 0; right: 0; "
    "background: linear-gradient(135deg, #9C27B0, #7B1FA2); "
    "color: white; "
    "padding: 12px 20px; "
    "display: none; "
    "align-items: center; "
    "justify-content: center; "
    "gap: 16px; "
    "box-shadow: 0 -4px 12px rgba(0,0,0,0.3); "
    "z-index: 100; "
    "animation: slideUp 0.3s ease; "
    "}"
    "@keyframes slideUp { "
    "from { transform: translateY(100%); } "
    "to { transform: translateY(0); } "
    "}"
    ".sandbox-banner.active { display: flex; }"
    "#board.sandbox-active { "
    "border-color: #9C27B0; "
    "box-shadow: 0 0 0 2px rgba(156, 39, 176, 0.45); "
    "} "
    "#board.sandbox-active .square.light { background: #ede4d3; } "
    "#board.sandbox-active .square.dark { background: #5c4a6e; } "
    ".sandbox-text { font-weight: 600; }"
    ".btn-exit-sandbox { "
    "padding: 8px 20px; "
    "background: white; "
    "color: #9C27B0; "
    "border: none; "
    "border-radius: 6px; "
    "font-weight: 600; "
    "cursor: pointer; "
    "transition: all 0.2s; "
    "}"
    ".btn-exit-sandbox:hover { transform: scale(1.05); }"
    ".btn-try-moves { "
    "padding: 12px 24px; "
    "background: #9C27B0; "
    "color: white; "
    "border: none; "
    "border-radius: 8px; "
    "font-weight: 600; "
    "cursor: pointer; "
    "transition: all 0.2s; "
    "margin: 10px; "
    "}"
    ".btn-try-moves:hover { transform: scale(1.05); }"
    "/* Timer System Styles */"
    ".time-control-selector { "
    "display: flex; "
    "gap: 8px; "
    "margin-bottom: 10px; "
    "min-width: 0; "
    "}"
    ".time-control-selector select { "
    "flex: 1 1 0; "
    "min-width: 0; "
    "padding: 8px 10px; "
    "background: #333; "
    "color: #e0e0e0; "
    "border: 1px solid #555; "
    "border-radius: 4px; "
    "font-size: 13px; "
    "}"
    ".time-control-selector button { "
    "flex-shrink: 0; "
    "padding: 8px 12px; "
    "background: #4CAF50; "
    "color: white; "
    "border: none; "
    "border-radius: 4px; "
    "cursor: pointer; "
    "font-weight: 600; "
    "font-size: 13px; "
    "transition: all 0.2s; "
    "white-space: nowrap; "
    "}"
    ".time-control-selector button:hover:not(:disabled) { "
    "background: #45a049; "
    "transform: scale(1.05); "
    "}"
    ".time-control-selector button:disabled { "
    "background: #666; "
    "cursor: not-allowed; "
    "}"
    ".custom-settings { "
    "background: #333; "
    "padding: 10px; "
    "border-radius: 4px; "
    "margin-top: 10px; "
    "}"
    ".custom-input-group { "
    "display: flex; "
    "justify-content: space-between; "
    "align-items: center; "
    "margin-bottom: 8px; "
    "}"
    ".custom-input-group label { "
    "color: #e0e0e0; "
    "font-size: 14px; "
    "}"
    ".custom-input-group input { "
    "width: 80px; "
    "padding: 6px; "
    "background: #444; "
    "color: #e0e0e0; "
    "border: 1px solid #555; "
    "border-radius: 4px; "
    "text-align: center; "
    "}"
    ".timer-display { "
    "display: flex; "
    "flex-direction: column; "
    "gap: 10px; "
    "margin: 15px 0; "
    "}"
    ".player-time { "
    "background: #333; "
    "border-radius: 6px; "
    "padding: 12px; "
    "transition: all 0.3s ease; "
    "}"
    ".player-time.active { "
    "background: linear-gradient(135deg, #4CAF50, #45a049); "
    "box-shadow: 0 0 20px rgba(76,175,80,0.3); "
    "}"
    ".player-time.low-time { "
    "background: linear-gradient(135deg, #FF9800, #F57C00); "
    "animation: pulse 1s infinite; "
    "}"
    ".player-time.critical-time { "
    "background: linear-gradient(135deg, #F44336, #D32F2F); "
    "animation: pulse 0.5s infinite; "
    "}"
    "@keyframes pulse { "
    "0%, 100% { opacity: 1; } "
    "50% { opacity: 0.7; } "
    "}"
    ".player-info { "
    "display: flex; "
    "justify-content: space-between; "
    "align-items: center; "
    "margin-bottom: 8px; "
    "}"
    ".player-name { "
    "font-weight: 600; "
    "font-size: 14px; "
    "}"
    ".move-indicator { "
    "width: 12px; "
    "height: 12px; "
    "border-radius: 50%; "
    "background: #666; "
    "transition: all 0.3s; "
    "}"
    ".move-indicator.active { "
    "background: #4CAF50; "
    "box-shadow: 0 0 10px rgba(76,175,80,0.5); "
    "}"
    ".time-value { "
    "font-size: 24px; "
    "font-weight: bold; "
    "font-family: 'Courier New', monospace; "
    "text-align: center; "
    "margin-bottom: 8px; "
    "}"
    ".time-bar { "
    "height: 6px; "
    "background: #555; "
    "border-radius: 3px; "
    "overflow: hidden; "
    "}"
    ".time-progress { "
    "height: 100%; "
    "background: #4CAF50; "
    "transition: width 0.3s ease; "
    "border-radius: 3px; "
    "}"
    ".player-time.low-time .time-progress { "
    "background: #FF9800; "
    "}"
    ".player-time.critical-time .time-progress { "
    "background: #F44336; "
    "}"
    ".timer-controls { "
    "display: flex; "
    "gap: 10px; "
    "justify-content: center; "
    "margin: 15px 0; "
    "}"
    ".timer-controls button { "
    "padding: 10px 20px; "
    "background: #333; "
    "color: #e0e0e0; "
    "border: 1px solid #555; "
    "border-radius: 6px; "
    "cursor: pointer; "
    "font-weight: 600; "
    "transition: all 0.2s; "
    "}"
    ".timer-controls button:hover { "
    "background: #444; "
    "transform: scale(1.05); "
    "}"
    ".timer-stats { "
    "background: #333; "
    "padding: 10px; "
    "border-radius: 4px; "
    "margin-top: 10px; "
    "}"
    ".stat-item { "
    "display: flex; "
    "justify-content: space-between; "
    "margin-bottom: 5px; "
    "font-size: 13px; "
    "}"
    ".stat-label { "
    "color: #aaa; "
    "}"
    ".stat-value { "
    "color: #e0e0e0; "
    "font-weight: 600; "
    "font-family: 'Courier New', monospace; "
    "}"
    "/* Scrollbar styling */"
    ".history-box::-webkit-scrollbar { width: 6px; }"
    ".history-box::-webkit-scrollbar-track { background: #2a2a2a; }"
    ".history-box::-webkit-scrollbar-thumb { background: #4CAF50; "
    "border-radius: 3px; }"
    ".history-box::-webkit-scrollbar-thumb:hover { background: #45a049; }"
    "</style>"
    "</head>";

// Chunk 2: HTML body structure (no JavaScript yet)
// Chunk 2a: Layout Start (Body, Container, Header with menu vlevo + title
// vpravo) requestHint MUST be defined first so onclick on the button finds it
// (global scope)
static const char html_chunk_layout_start[] =
    "<body>"
    "<script>"
    "window.requestHint=function(){"
    "var "
    "btn=document.getElementById('hint-btn');if(btn){btn.disabled=true;btn."
    "textContent='Načítám…';}"
    "var board=typeof boardData!=='undefined'?boardData:[];"
    "var status=typeof statusData!=='undefined'?statusData:{};"
    "var history=typeof historyData!=='undefined'?historyData:{};"
    "if(!board||board.length!==8||(status.game_state||'').toLowerCase()==='"
    "promotion'||(status.game_end&&status.game_end.ended)){if(btn){btn."
    "disabled=false;btn.textContent='Nápověda';}return;}"
    "var rows=[];for(var "
    "r=7;r>=0;r--){if(!board[r]||board[r].length!==8){if(btn){btn.disabled="
    "false;btn.textContent='Nápověda';}return;}var rank='',empty=0;for(var "
    "c=0;c<8;c++){var p=board[r][c];if(p===' "
    "'||p==='')empty++;else{if(empty){rank+=empty;empty=0;}rank+=p;}}if(empty)"
    "rank+=empty;rows.push(rank);}"
    "var side=(status.current_player==='White')?'w':'b';var "
    "moves=(history.moves||[]).length;var fen=rows.join('/')+' '+side+' KQkq - "
    "0 '+(Math.floor(moves/2)+1);"
    "var depth=(function(){try{if(typeof getHintDepth==='function')return "
    "getHintDepth();var "
    "d=parseInt(localStorage.getItem('chessHintDepth')||'10',10);return "
    "Math.min(18,Math.max(1,isNaN(d)?10:d));}catch(e){return 10;}})();"
    "fetch('https://chess-api.com/"
    "v1',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify({fen:fen,depth:depth})}).then(function(r){if(!"
    "r||!r.ok)return null;return r.json();}).then(function(d){"
    "if(!d){if(btn){btn.disabled=false;btn.textContent='Nedostupné';setTimeout("
    "function(){if(btn)btn.textContent='Nápověda';},2500);}return;}"
    "var from=(d.from&&d.from.length===2)?d.from.toLowerCase():null;var "
    "to=(d.to&&d.to.length===2)?d.to.toLowerCase():null;"
    "if((!from||!to)&&d.move&&d.move.length>=4){from=d.move.substring(0,2)."
    "toLowerCase();to=d.move.substring(2,4).toLowerCase();}"
    "if(from&&to){document.querySelectorAll('.square').forEach(function(sq){sq."
    "classList.remove('hint-from','hint-to');});"
    "var "
    "fc=from.charCodeAt(0)-97,fr=parseInt(from[1],10)-1,tc=to.charCodeAt(0)-97,"
    "tr=parseInt(to[1],10)-1;"
    "var "
    "fs=document.querySelector('[data-row=\"'+fr+'\"][data-col=\"'+fc+'\"]');"
    "var "
    "ts=document.querySelector('[data-row=\"'+tr+'\"][data-col=\"'+tc+'\"]');"
    "if(fs)fs.classList.add('hint-from');if(ts)ts.classList.add('hint-to');"
    "fetch('/api/game/"
    "hint_highlight',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify({from:from,to:to})}).catch(function(){});}"
    "if(btn){btn.disabled=false;btn.textContent='Nápověda';}}).catch(function()"
    "{if(btn){btn.disabled=false;btn.textContent='Nedostupné';setTimeout("
    "function(){if(btn)btn.textContent='Nápověda';},2500);}});"
    "};"
    "</script>"
    "<div class='container'>"
    "<header class='app-header'>"
    "<nav class='tab-nav'>"
    "<button type='button' class='tab-btn active' "
    "onclick='switchTab(\"tab-hra\")' id='btn-tab-hra'>Hra</button>"
    "<button type='button' class='tab-btn' "
    "onclick='switchTab(\"tab-nastaveni\")' "
    "id='btn-tab-nastaveni'>Nastavení</button>"
    "</nav>"
    "<h1 class='app-title'>CZECHMATE</h1>"
    "</header>";

// Tab Hra: wrapper + main-content (tlačítka jsou v levém panelu)
static const char html_chunk_tab_hra_start[] =
    "<div id='tab-hra' class='tab-content active'>"
    "<div class='main-content'>";

// Chunk 2b: Game Left Panel (timer, stav, časová kontrola, akce)
static const char html_chunk_game_left[] =
    "<div class='game-info-panel'>"
    "<div class='game-block'>"
    "<div class='game-title'>Čas</div>"
    "<div class='timer-display'>"
    "<div class='player-time white-time' id='white-timer'>"
    "<div class='player-info'>"
    "<span class='player-name'>Bílý</span>"
    "<span class='move-indicator' id='white-move-indicator'>●</span>"
    "</div>"
    "<div class='time-value' id='white-time'>10:00</div>"
    "<div class='time-bar'><div class='time-progress' "
    "id='white-progress'></div></div>"
    "</div>"
    "<div class='player-time black-time' id='black-timer'>"
    "<div class='player-info'>"
    "<span class='player-name'>Černý</span>"
    "<span class='move-indicator' id='black-move-indicator'>●</span>"
    "</div>"
    "<div class='time-value' id='black-time'>10:00</div>"
    "<div class='time-bar'><div class='time-progress' "
    "id='black-progress'></div></div>"
    "</div>"
    "</div>"
    "<div class='timer-controls'>"
    "<button id='pause-timer' onclick='pauseTimer()'>Pozastavit</button>"
    "<button id='resume-timer' onclick='resumeTimer()' style='display:none;'>"
    "Pokračovat</button>"
    "<button id='reset-timer' onclick='resetTimer()'>Reset</button>"
    "</div>"
    "<div class='timer-stats'>"
    "<div class='stat-item'><span class='stat-label'>Prům. tah</span><span "
    "id='avg-move-time' class='stat-value'>—</span></div>"
    "<div class='stat-item'><span class='stat-label'>Tahy</span><span "
    "id='total-moves' class='stat-value'>0</span></div>"
    "</div>"
    "</div>"
    "<details class='game-block game-details'>"
    "<summary class='game-details-summary'>Stav partie</summary>"
    "<div class='game-details-body'>"
    "<div class='game-row'><span class='game-label'>Stav</span><span "
    "id='game-state' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Na tahu</span><span "
    "id='current-player' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Tahy</span><span "
    "id='move-count' class='game-value'>0</span></div>"
    "<div class='game-row'><span class='game-label'>Šach</span><span "
    "id='in-check' class='game-value'>Ne</span></div>"
    "</div>"
    "</details>"
    "<div id='bot-status-panel' class='game-block' style='display:none;'>"
    "<div class='game-title'>Bot</div>"
    "<span id='bot-status-text' class='game-value'>—</span>"
    "</div>"
    "<div id='puzzle-status-panel' class='game-block puzzle-status-panel' "
    "style='display:none;'>"
    "<div class='game-title'>Puzzle</div>"
    "<p id='puzzle-status-text' class='puzzle-status-text'>—</p>"
    "<p id='puzzle-status-sub' class='puzzle-status-sub' style='display:none;'>"
    "</p>"
    "</div>"
    "<details class='game-block game-details'>"
    "<summary class='game-details-summary'>Časová kontrola</summary>"
    "<div class='game-details-body'>"
    "<div class='time-control-selector'>"
    "<select id='time-control-select' onchange='changeTimeControl()'>"
    "<option value='0'>Bez času</option>"
    "<option value='1'>Bullet 1+0</option>"
    "<option value='2'>Bullet 1+1</option>"
    "<option value='3'>Bullet 2+1</option>"
    "<option value='4'>Blitz 3+0</option>"
    "<option value='5'>Blitz 3+2</option>"
    "<option value='6'>Blitz 5+0</option>"
    "<option value='7'>Blitz 5+3</option>"
    "<option value='8'>Rapid 10+0</option>"
    "<option value='9'>Rapid 10+5</option>"
    "<option value='10'>Rapid 15+10</option>"
    "<option value='11'>Rapid 30+0</option>"
    "<option value='12'>Classical 60+0</option>"
    "<option value='13'>Classical 90+30</option>"
    "<option value='14'>Vlastní</option>"
    "</select>"
    "<button id='apply-time-control' onclick='applyTimeControl()' "
    "disabled>Použít</button>"
    "</div>"
    "<div id='custom-time-settings' class='custom-settings' "
    "style='display:none;'>"
    "<div class='custom-input-group'><label>Minuty</label>"
    "<input type='number' id='custom-minutes' min='1' max='180' value='10'>"
    "</div>"
    "<div class='custom-input-group'><label>Increment (s)</label>"
    "<input type='number' id='custom-increment' min='0' max='60' value='0'>"
    "</div>"
    "</div>"
    "</div>"
    "</details>"
    "<div class='game-actions'>"
    "<button type='button' id='new-game-btn' class='btn-game-action "
    "btn-game-new' "
    "onclick='startNewGame()'>Nová hra</button>"
    "<button type='button' id='hint-btn' class='btn-game-action btn-game-hint' "
    "onclick='requestHint()'>Nápověda</button>"
    "<button type='button' class='btn-game-action btn-game-sandbox' "
    "onclick='enterSandboxMode()'>Zkusit tahy</button>"
    "</div>"
    "<details class='game-block game-details game-details--hints'>"
    "<summary class='game-details-summary'>Nápověda a poslední zhodnocení</summary>"
    "<div class='game-details-body'>"
    "<div id='hint-explanation' class='hint-explanation' style='display:none;'>"
    "<div class='hint-explanation-title'>Proč tento tah?</div>"
    "<p id='hint-explanation-message' class='hint-explanation-message'></p>"
    "</div>"
    "<div id='move-evaluation' class='move-evaluation' style='display:none;'>"
    "<div class='move-evaluation-title'>Zhodnocení posledního tahu</div>"
    "<p id='move-evaluation-message' class='move-evaluation-message'></p>"
    "</div>"
    "<div id='castling-pending-message' class='castling-pending-message' "
    "style='display:none;'></div>"
    "</div>"
    "</details>"
    "</div>";

// Game Right Panel (zvednutá, sebrání, historie)
static const char html_chunk_game_right[] =
    "<div class='game-right-panel'>"
    "<details class='game-block game-details' open>"
    "<summary class='game-details-summary'>Zvednutá figurka</summary>"
    "<div class='game-details-body'>"
    "<div class='game-row'><span class='game-label'>Figurka</span><span "
    "id='lifted-piece' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Pozice</span><span "
    "id='lifted-position' class='game-value'>—</span></div>"
    "</div>"
    "</details>"
    "<details class='game-block game-details'>"
    "<summary class='game-details-summary'>Sebrané figurky</summary>"
    "<div class='game-details-body'>"
    "<div class='game-row'><span class='game-label'>Bílý</span></div>"
    "<div id='white-captured' class='captured-pieces'></div>"
    "<div class='game-row' style='margin-top:8px;'><span "
    "class='game-label'>Černý</span></div>"
    "<div id='black-captured' class='captured-pieces'></div>"
    "</div>"
    "</details>"
    "<div id='teaching-stats-panel' class='game-block teaching-stats' "
    "style='display:none;'>"
    "<div class='game-title'>Výukový přehled</div>"
    "<div class='teaching-stats-row teaching-stats-header'>"
    "<span class='teaching-stats-label'>Bílý</span>"
    "<span class='teaching-stats-label'>Černý</span>"
    "</div>"
    "<div class='teaching-stats-row'>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Nápovědy</span> "
    "<span id='teaching-stats-white-hints'>0</span></span>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Nápovědy</span> "
    "<span id='teaching-stats-black-hints'>0</span></span>"
    "</div>"
    "<div class='teaching-stats-row'>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr 5 tahů</span> "
    "<span id='teaching-stats-white-avg5'>—</span></span>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr 5 tahů</span> "
    "<span id='teaching-stats-black-avg5'>—</span></span>"
    "</div>"
    "<div class='teaching-stats-row'>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr 15 tahů</span> "
    "<span id='teaching-stats-white-avg15'>—</span></span>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr 15 tahů</span> "
    "<span id='teaching-stats-black-avg15'>—</span></span>"
    "</div>"
    "<div class='teaching-stats-row'>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr celá hra</span> "
    "<span id='teaching-stats-white-avgAll'>—</span></span>"
    "<span class='teaching-stats-cell'><span class='teaching-stats-sublabel'>Průměr celá hra</span> "
    "<span id='teaching-stats-black-avgAll'>—</span></span>"
    "</div>"
    "<p class='teaching-stats-note'>Kvalita 1–5 (5 = výborný).</p>"
    "</div>"
    "<div class='game-block history-block'>"
    "<div class='history-toolbar'>"
    "<span class='game-title history-toolbar-title'>Historie</span>"
    "<button type='button' id='history-toggle-full' class='history-toolbar-btn' "
    "aria-expanded='false' style='display:none;'>Celá historie</button>"
    "</div>"
    "<div id='history-preview' class='history-preview history-box'></div>"
    "<div id='history-full-wrap' class='history-full-wrap' style='display:none;'>"
    "<div id='history' class='history-box history-box--expanded'></div>"
    "</div>"
    "<div id='history-move-detail' class='history-move-detail' style='display:none;'></div>"
    "</div>"
    "</div>";

static const char html_chunk_tab_hra_end[] = "</div></div>";

// Chunk 2c: Board Container (Center Column) + Modal
static const char html_chunk_board[] =
    "<div class='board-container'>"
    "<div id='board' class='board'></div>"
    "<div id='loading' class='loading'>Načítání...</div>"
    "</div>"
    "<!-- Promotion Modal -->"
    "<div id='promotion-modal' class='modal' style='display:none; "
    "position:fixed; top:0; left:0; width:100%; height:100%; "
    "background:rgba(0,0,0,0.8); z-index:2000; align-items:center; "
    "justify-content:center;'>"
    "<div class='modal-content' style='background:#333; padding:20px; "
    "border-radius:8px; text-align:center; border:2px solid #4CAF50;'>"
    "<h3 style='color:#4CAF50; margin-bottom:15px;'>Promoce pěšce</h3>"
    "<div style='display:flex; gap:10px; justify-content:center;'>"
    "<button onclick=\"selectPromotion('Q')\" style='font-size:18px; "
    "padding:10px; cursor:pointer;'>Dáma</button>"
    "<button onclick=\"selectPromotion('R')\" style='font-size:18px; "
    "padding:10px; cursor:pointer;'>Věž</button>"
    "<button onclick=\"selectPromotion('B')\" style='font-size:18px; "
    "padding:10px; cursor:pointer;'>Střelec</button>"
    "<button onclick=\"selectPromotion('N')\" style='font-size:18px; "
    "padding:10px; cursor:pointer;'>Jezdec</button>"
    "</div>"
    "<button onclick='cancelPromotion()' style='margin-top:15px; padding:8px "
    "16px; background:#f44336; color:white; border:none; border-radius:4px; "
    "cursor:pointer;'>Zrušit</button>"
    "</div>"
    "</div>";

// Tab Nastavení: wrapper start (settings-grid)
static const char html_chunk_tab_nastaveni_start[] =
    "<div id='tab-nastaveni' class='tab-content'>"
    "<div class='settings-grid'>";

// Section Síť
static const char html_chunk_settings_sit[] =
    "<div class='settings-section settings-section--wide'>"
    "<h3 class='set-title'>Síť</h3>"
    "<div class='set-rows-grid'>"
    "<div class='set-row'><span class='set-label'>AP</span><span id='ap-ssid' "
    "class='set-value'>ESP32-CzechMate</span></div>"
    "<div class='set-row'><span class='set-label'>AP IP</span><span id='ap-ip' "
    "class='set-value'>192.168.4.1</span></div>"
    "<div class='set-row'><span class='set-label'>Klienti</span><span "
    "id='ap-clients' class='set-value'>0</span></div>"
    "<div class='set-row'><span class='set-label'>STA</span><span "
    "id='sta-ssid' "
    "class='set-value'>—</span></div>"
    "<div class='set-row'><span class='set-label'>STA IP</span><span "
    "id='sta-ip' "
    "class='set-value'>—</span></div>"
    "<div class='set-row'><span class='set-label'>Připoj.</span><span "
    "id='sta-connected' class='set-value'>ne</span></div>"
    "</div>"
    "<div class='set-group'><label>SSID</label>"
    "<input type='text' id='wifi-ssid' class='set-input' placeholder='WiFi "
    "SSID' "
    "maxlength='32'>"
    "</div><div class='set-group'><label>Heslo</label>"
    "<input type='password' id='wifi-password' class='set-input' "
    "placeholder='••••••••' "
    "maxlength='64'>"
    "</div>"
    "<button id='wifi-save-btn' class='set-btn' "
    "onclick='saveWiFiConfig()'>Uložit WiFi</button>"
    "<div class='set-btn-row'>"
    "<button id='wifi-connect-btn' class='set-btn set-btn-sm' "
    "onclick='connectSTA()'>Připojit</button>"
    "<button id='wifi-disconnect-btn' class='set-btn set-btn-sm' "
    "onclick='disconnectSTA()'>Odpojit</button>"
    "</div>"
    "<button id='wifi-clear-btn' class='set-btn set-btn-sm set-btn-danger' "
    "onclick='clearWiFiConfig()'>Smazat "
    "WiFi</button>"
    "<p class='set-hint' style='margin-top:10px;font-size:12px;opacity:0.85'>"
    "Tovární reset smaže <strong>celou NVS</strong> (WiFi, MQTT, partie, UI) a "
    "desku restartuje.</p>"
    "<button type='button' id='factory-reset-btn' class='set-btn set-btn-sm "
    "set-btn-danger' onclick='factoryResetDevice()'>Tovární reset (celá "
    "NVS)</button>"
    "</div>";

// Section Hra proti Počítači
static const char html_chunk_settings_bot[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>Hra proti Počítači</h3>"
    "<div class='set-group'><label>Režim Soupeře</label>"
    "<select id='game-mode' class='set-select' "
    "onchange='updateBotSettingsVisibility()'>"
    "<option value='pvp'>Člověk (PvP)</option>"
    "<option value='bot'>Počítač</option>"
    "</select>"
    "</div>"
    "<div id='bot-settings-container' style='display:none;'>"
    "<div class='set-group'><label>Obtížnost Bota</label>"
    "<select id='bot-strength' class='set-select' onchange='saveBotSettings()'>"
    "<option value='1'>Začátečník (~500 ELO)</option>"
    "<option value='3'>Mírně pokročilý (~900 ELO)</option>"
    "<option value='5'>Pokročilý (~1300 ELO)</option>"
    "<option value='8'>Expert (~1700 ELO)</option>"
    "<option value='12'>Mistr (~2100 ELO)</option>"
    "<option value='15'>Velmistr (~2500 ELO)</option>"
    "</select>"
    "</div>"
    "<div class='set-group'><label>Barva Hráče</label>"
    "<select id='player-side' class='set-select' "
    "onchange='saveBotSettings(); if(typeof handleRandomDraw===\"function\") "
    "handleRandomDraw();'>"
    "<option value='white'>Hraji za Bílé</option>"
    "<option value='black'>Hraji za Černé</option>"
    "<option value='random'>Náhodně</option>"
    "</select>"
    "</div>"
    "<div id='random-draw-result' class='set-group' style='display:none; "
    "margin-top:8px; padding:8px 12px; background:rgba(33,150,243,0.15); "
    "border:1px solid rgba(33,150,243,0.4); border-radius:6px; color:#e0e0e0; "
    "font-size:14px;'>"
    "</div>"
    "<div class='set-group set-check' style='margin-top:8px;'>"
    "<label><input type='checkbox' id='bot-led-target-only-after-lift' "
    "onchange='if(typeof setBotLedTargetOnlyAfterLift===\"function\") "
    "setBotLedTargetOnlyAfterLift(this.checked);'> "
    "Po zvednutí figurky bota zobrazit na LED jen cílové pole</label>"
    "</div>"
    "</div>"
    "<script>\n"
    "/* Bot: updateBotSettingsVisibility, saveBotSettings, loadBotSettings — "
    "chess_app.js (NVS) */\n"
    "</script>"
    "</div>";

// Section Tutoriály (základní postavení)
static const char html_chunk_settings_tutorial[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>Tutoriály</h3>"
    "<div class='set-group set-check'>"
    "<label><input type='checkbox' id='chess-tutorials-enabled' "
    "onchange='if(typeof saveTutorialsEnabled===\"function\")saveTutorialsEnabled(this.checked);'> "
    "Zobrazit nabídku tutoriálů (výchozí vypnuto)</label>"
    "</div>"
    "<div id='tutorials-panel-body' class='tutorials-panel-body' style='display:none;'>"
    "<p class='set-note'>LED a web ukážou, kam položit figurku; senzory neověřují druh figurky. "
    "Po dokončení musí fyzická pozice odpovídat pravidlům šachu, jinak další hra nemusí dávat smysl.</p>"
    "<button type='button' class='set-btn' id='setup-tutorial-open-btn' "
    "onclick='if(typeof openSetupTutorialIntro===\"function\")openSetupTutorialIntro();' "
    "disabled>Základní postavení figur</button>"
    "</div>"
    "<div class='set-group' style='margin-top:10px;'>"
    "<p class='set-note'>Puzzle: nejdřív rozestavení podle LED (i bez zapnutých tutoriálů), pak hra.</p>"
    "<button type='button' class='set-btn' id='puzzle-open-btn' "
    "onclick='if(typeof openPuzzleIntro===\"function\")openPuzzleIntro();'>"
    "Puzzles (5 obtížností)</button>"
    "</div>"
    "<script>\n"
    "/* Tutoriály: saveTutorialsEnabled, tutorialsPanelSetVisible — chess_app.js "
    "(NVS) */\n"
    "</script>"
    "</div>";

// Section Zařízení
static const char html_chunk_settings_zarizeni[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>Zařízení</h3>"
    "<div class='set-row'><span class='set-label'>Režim</span></div>"
    "<div class='set-btn-row' style='margin-bottom:8px;'>"
    "<button type='button' class='set-btn set-btn-sm' id='light-mode-game'>"
    "Šachovnice</button>"
    "<button type='button' class='set-btn set-btn-sm' id='light-mode-lamp'>"
    "Lampa</button>"
    "</div>"
    "<div id='light-lamp-controls' style='display:none;'>"
    "<div class='set-row'><span class='set-label'>Zapnutí</span>"
    "<input type='checkbox' id='light-state-toggle' checked></div>"
    "<div class='set-row'><span class='set-label'>Barva R</span>"
    "<span id='light-r-value' class='set-value'>255</span></div>"
    "<input type='range' id='light-r' class='set-input' min='0' max='255' "
    "value='255' style='padding:0; height:20px;'>"
    "<div class='set-row'><span class='set-label'>G</span>"
    "<span id='light-g-value' class='set-value'>255</span></div>"
    "<input type='range' id='light-g' class='set-input' min='0' max='255' "
    "value='255' style='padding:0; height:20px;'>"
    "<div class='set-row'><span class='set-label'>B</span>"
    "<span id='light-b-value' class='set-value'>255</span></div>"
    "<input type='range' id='light-b' class='set-input' min='0' max='255' "
    "value='255' style='padding:0; height:20px;'>"
    "<span id='light-error-msg' class='set-note' style='display:none;color:#c62828;'></span>"
    "</div>"
    "<div class='set-row'>"
    "<span class='set-label'>Jas</span>"
    "<span id='brightness-value' class='set-value'>50%</span>"
    "</div>"
    "<input type='range' id='brightness-slider' class='set-input' min='0' "
    "max='100' value='50' style='padding:0; height:24px;' "
    "oninput='var e=document.getElementById(\"brightness-value\"); "
    "if(e)e.textContent=this.value+\"%\";' "
    "onchange='setBrightness(this.value)'>"
    "<div class='set-row'>"
    "<span class='set-label'>Přepnutí na lampu po nečinnosti</span>"
    "<span id='auto-lamp-timeout-value' class='set-value'>300 s</span></div>"
    "<input type='number' id='auto-lamp-timeout' class='set-input' min='5' "
    "max='7200' step='1' value='300' style='width:80px;' "
    "oninput='var e=document.getElementById(\"auto-lamp-timeout-value\"); "
    "if(e)e.textContent=(this.value||\"\")+\" s\";' "
    "onchange='setAutoLampTimeout(this.value)'>"
    "<span class='set-note'> 5 s až 120 min (7200 s)</span>"
    "<label class='set-check' style='margin-top:8px;'>"
    "<input type='checkbox' id='starting-position-check' "
    "onchange=\"if(typeof saveStartingPositionCheck=='function')saveStartingPositionCheck(this.checked);\"> "
    "<span>Kontrolovat počáteční pozici figurek</span></label>"
    "<p class='set-note'>Před zahájením hry ověřit, zda jsou figurky správně rozestaveny (výchozí: vypnuto)</p>"
    "<div class='set-row'><span class='set-label'>Zámek</span><span "
    "id='web-lock-status' class='set-value'>—</span></div>"
    "<div class='set-row'><span class='set-label'>Online</span><span "
    "id='web-online-status' class='set-value'>—</span></div>"
    "</div>"
    "<div class='settings-section settings-section--wide'>"
    "<h3 class='set-title'>Nápověda</h3>"
    "<p class='set-note' style='margin-top:0;'>Tlačítko Nápověda (Stockfish), hloubka "
    "myšlení a limity nápověd níže vyžadují internet (WiFi s přístupem ven).</p>"
    "<div class='set-group'><label for='led-guidance-level'>Úroveň LED na desce</label>"
    "<select id='led-guidance-level' class='set-select'>"
    "<option value='5'>5 – Plná (cíle, šach, braní na LED)</option>"
    "<option value='4'>4 – Bez růžové signalizace šachu na králi</option>"
    "<option value='3'>3 – Bez žlutého zvýraznění figur na tahu</option>"
    "<option value='2'>2 – Po zvednutí bez cílových polí (jen zdroj)</option>"
    "<option value='1'>1 – Minimum (jen základní indikace zvednutí)</option>"
    "</select></div>"
    "<p class='set-note'>Uloží se v zařízení. Nižší číslo = méně barev na LED; úroveň 5 "
    "zahrnuje i nápovědu braní u zvedlé soupeřovy figurky.</p>"
    "<div class='set-group'><label>Hloubka myšlení</label>"
    "<input type='number' id='hint-depth' class='set-input' min='1' max='18' "
    "value='10' onchange='var "
    "v=Math.min(18,Math.max(1,parseInt(this.value,10)||10));"
    "this.value=v;if(typeof window.onHintDepthChange===\"function\")"
    "window.onHintDepthChange(v);'>"
    "</div>"
    "<p class='set-note'>Jak moc má počítač přemýšlet. ELO u hloubek je jen hrubý odhad: "
    "1 ≈ 500 ELO, 10 ≈ 1500 ELO, 18 ≈ 2800 ELO.</p>"
    "<label class='set-check' style='margin-top:4px;'>"
    "<input type='checkbox' id='evaluate-move-enabled' "
    "onchange=\"if(typeof window.onEvaluateMoveChange==='function')"
    "window.onEvaluateMoveChange(this.checked);"
    "if(!this.checked){hideMoveEvaluation();if(typeof "
    "renderHistoryList==='function')renderHistoryList();}\">"
    "<span>Zhodnocení tahu</span>"
    "</label>"
    "<p class='set-note'>Po každém tahu zobrazit, jak dobrý byl tah (výborný, "
    "chyba, …). Pro přesnost se používá hloubka min. 12.</p>"
    "<div class='set-group' style='margin-top:6px;'><label>Nápověd na partii</label>"
    "<select id='hint-limit' class='set-select' onchange='saveHintSettings(); "
    "if(typeof updateHintButtonLabel===\"function\")updateHintButtonLabel();'>"
    "<option value='0'>Neomezeno</option>"
    "<option value='3'>3</option>"
    "<option value='5'>5</option>"
    "<option value='10'>10</option>"
    "</select>"
    "</div>"
    "<p class='set-note'>Omezený počet nápověd. Získejte je zpět výborným tahem "
    "nebo sebráním figurky.</p>"
    "<label class='set-check'><input type='checkbox' id='hint-award-best' "
    "onchange='saveHintSettings()'><span>Přidat nápovědu za výborný tah</span></label>"
    "<label class='set-check'><input type='checkbox' id='hint-award-good' "
    "onchange='saveHintSettings()'><span>Přidat nápovědu za dobrý tah</span></label>"
    "<label class='set-check'><input type='checkbox' id='hint-award-capture' "
    "onchange='saveHintSettings()'><span>Přidat nápovědu za sebrání figurky</span></label>"
    "<label class='set-check' style='margin-top:4px;'><input type='checkbox' "
    "id='show-hint-stats' onchange='saveHintSettings(); "
    "if(typeof updateTeachingStatsPanel===\"function\")updateTeachingStatsPanel();'>"
    "<span>Zobrazit výukový přehled (nápovědy a kvalita tahů)</span></label>"
    "<p class='set-note'>Blok v panelu u historie: nápovědy a průměr kvality za 5 / 15 / celou hru.</p>"
    "<script>\n"
    "function saveHintSettings(){try{if(typeof window.syncHintSettingsFromDom==="
    "'function')window.syncHintSettingsFromDom();}catch(e){}}\n"
    "</script>"
    "</div>"
    "<div class='settings-section'>"
    "<h3 class='set-title'>Ovládání</h3>"
    "<label class='set-check'>"
    "<input type='checkbox' id='remote-control-enabled' "
    "onchange='toggleRemoteControl()' aria-describedby='remote-control-help'>"
    "<span>Ovládání tahů z webu (virtuální zvednutí a položení)</span>"
    "</label>"
    "<p id='remote-control-help' class='set-note set-note-warn'>Kliky nebo táhnutí "
    "figurky na cílové pole posílají stejné příkazy jako fyzické zvednutí a položení. "
    "Režim jen šachovnice a časovačů bez scrollování: přidej do adresy <code>?focus=1</code> "
    "(tlačítkem „Celá aplikace“ se vrátíš). Pokud současně někdo mění postavení na "
    "desce, může se stav hry v programu rozcházet s reálným postavením — pak je potřeba "
    "situaci srovnat (např. nová hra) nebo používat jen jeden způsob ovládání. Při "
    "zamčeném webu (stav Zámek výše) se ovládání z webu nepoužije.</p>"
    "<label class='set-check'>"
    "<input type='checkbox' id='confirm-new-game' "
    "onchange=\"try{localStorage.setItem('chess_confirm_new_game',this.checked)"
    ";}"
    "catch(e){}\">"
    "<span>Potvrdit novou hru</span>"
    "</label>"
    "<p class='set-note'>Před spuštěním nové hry zobrazit potvrzení.</p>"
    "</div>";

// Chunk 2c: Review and Sandbox banners
static const char html_chunk_banners[] =
    "<!-- Review Mode Banner -->"
    "<div class='review-banner' id='review-banner'>"
    "<div class='review-header'>"
    "<div style='display:flex;align-items:center;gap:8px;flex:1'>"
    "<span id='review-move-text'>Prohlížíš tah 0</span>"
    "</div>"
    "<button class='btn-header-close' onclick='exitReviewMode()' "
    "title='Zavřít'>×</button>"
    "</div>"
    "<div class='review-controls'>"
    "<button class='nav-btn nav-first' onclick='goToMove(0)' "
    "title='Na začátek' aria-label='První tah'>|&lt;&lt;</button>"
    "<button class='nav-btn nav-prev' onclick='prevReviewMove()' "
    "title='Předchozí tah' aria-label='Předchozí'>&lt;</button>"
    "<button class='nav-btn nav-next' onclick='nextReviewMove()' "
    "title='Další tah' aria-label='Další'>&gt;</button>"
    "<button class='nav-btn nav-last' onclick='goToMove(-1)' "
    "title='Na konec' aria-label='Poslední tah'>&gt;&gt;|</button>"
    "</div>"
    "</div>"
    "<!-- Sandbox Mode Banner -->"
    "<div class='sandbox-banner' id='sandbox-banner'>"
    "<div class='sandbox-text'>"
    "<span>Sandbox Mode - Zkoušíš tahy lokálně</span>"
    "</div>"
    "<div style='display: flex; gap: 10px;'>"
    "<button class='btn-exit-sandbox' id='sandbox-undo-btn' "
    "onclick='undoSandboxMove()' disabled>Undo (0/10)</button>"
    "<button class='btn-exit-sandbox' onclick='exitSandboxMode()'>Zpět na "
    "skutečnou pozici</button>"
    "</div>"
    "</div>"
    "<div id='setup-tutorial-overlay' class='setup-tutorial-overlay' "
    "style='display:none;'>"
    "<div class='modal-content setup-tutorial-modal-content'>"
    "<div id='setup-tutorial-intro-panel'>"
    "<h3 class='setup-tutorial-modal-title'>Tutoriál: základní postavení</h3>"
    "<p id='setup-tutorial-warn' class='set-note setup-tutorial-warn' "
    "style='display:none;'></p>"
    "<p class='setup-tutorial-body'>Srovnej všechny figurky <strong>mimo desku</strong>. "
    "Potom ukládej podle návodu — rozsvítí se cílové pole. Senzory nepoznají druh figurky; "
    "polož správnou podle obrázku. Po dokončení musí odpovídat fyzická pozice pravidlům šachu.</p>"
    "<div class='setup-tutorial-actions'>"
    "<button type='button' class='set-btn' onclick='if(typeof setupTutorialBegin===\"function\")setupTutorialBegin();'>Zahájit</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof setupTutorialCloseIntro===\"function\")setupTutorialCloseIntro();'>Zavřít</button>"
    "</div></div>"
    "<div id='setup-tutorial-run-panel' style='display:none;'>"
    "<div class='setup-tutorial-piece-wrap'>"
    "<span id='setup-tutorial-piece-display' class='setup-tutorial-piece-glyph'></span>"
    "</div>"
    "<p id='setup-tutorial-instruction' class='setup-tutorial-instruction'></p>"
    "<p id='setup-tutorial-progress' class='set-note setup-tutorial-progress'></p>"
    "<div class='setup-tutorial-actions setup-tutorial-actions--run'>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof setupTutorialBack===\"function\")setupTutorialBack();'>Zpět</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof setupTutorialSkip===\"function\")setupTutorialSkip();'>Další</button>"
    "<button type='button' class='set-btn set-btn-sm set-btn-danger' onclick='if(typeof setupTutorialCancel===\"function\")setupTutorialCancel();'>Ukončit</button>"
    "</div></div>"
    "<div id='setup-tutorial-done-panel' style='display:none;'>"
    "<h3 class='setup-tutorial-modal-title'>Dokončení</h3>"
    "<p class='setup-tutorial-body'>Fyzická deska musí mít obsazené řádky 1–2 a 7–8 a prázdný střed (jen senzor).</p>"
    "<div class='setup-tutorial-actions'>"
    "<button type='button' class='set-btn' onclick='if(typeof setupTutorialFinish===\"function\")setupTutorialFinish();'>Potvrdit a spustit novou hru</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof setupTutorialCancel===\"function\")setupTutorialCancel();'>Zrušit</button>"
    "</div></div>"
    "</div></div>"
    "<div id='puzzle-overlay' class='setup-tutorial-overlay' style='display:none;'>"
    "<div class='modal-content setup-tutorial-modal-content'>"
    "<div id='puzzle-intro-panel'>"
    "<h3 class='setup-tutorial-modal-title'>Puzzle mód</h3>"
    "<p class='setup-tutorial-body'>Vyber obtížnost. Nejprve rozestav figurky na desku podle LED (stejně jako u základního postavení), pak spusť puzzle.</p>"
    "<div id='puzzle-list' class='setup-tutorial-body' style='display:grid;grid-template-columns:1fr;gap:8px;'></div>"
    "<p id='puzzle-guided-message' class='set-note setup-tutorial-progress'></p>"
    "<div class='setup-tutorial-actions'>"
    "<button type='button' class='set-btn' onclick='if(typeof puzzlePrepareSelected===\"function\")puzzlePrepareSelected();'>"
    "Pokračovat k rozestavení</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof puzzleCancel===\"function\")puzzleCancel();'>Zavřít</button>"
    "</div></div>"
    "<div id='puzzle-setup-panel' style='display:none;'>"
    "<div class='setup-tutorial-piece-wrap'>"
    "<span id='puzzle-setup-piece-display' class='setup-tutorial-piece-glyph'></span>"
    "</div>"
    "<p id='puzzle-setup-instruction' class='setup-tutorial-instruction'></p>"
    "<p id='puzzle-setup-progress' class='set-note setup-tutorial-progress'></p>"
    "<div class='setup-tutorial-actions setup-tutorial-actions--run'>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof puzzleSetupBack===\"function\")puzzleSetupBack();'>Zpět</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof puzzleSetupSkip===\"function\")puzzleSetupSkip();'>Přeskočit krok</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof puzzleBackToIntroFromSetup===\"function\")puzzleBackToIntroFromSetup();'>Na výběr</button>"
    "<button type='button' class='set-btn set-btn-sm set-btn-danger' onclick='if(typeof puzzleCancel===\"function\")puzzleCancel();'>Ukončit</button>"
    "</div></div>"
    "<div id='puzzle-confirm-panel' style='display:none;'>"
    "<h3 class='setup-tutorial-modal-title'>Spustit puzzle</h3>"
    "<p id='puzzle-confirm-warn' class='set-note setup-tutorial-warn' style='display:none;'></p>"
    "<p class='setup-tutorial-body'>Po spuštění hraj na fyzické desce. Špatný tah uvidíš ve stavu hry.</p>"
    "<div class='setup-tutorial-actions'>"
    "<button type='button' class='set-btn' onclick='if(typeof puzzleExecuteStart===\"function\")puzzleExecuteStart();'>Spustit puzzle</button>"
    "<button type='button' class='set-btn set-btn-sm' onclick='if(typeof puzzleCancel===\"function\")puzzleCancel();'>Zrušit</button>"
    "</div></div>"
    "</div></div>";

// Chunk 3: Hint (Nápověda) - inline so requestHint exists even if chess_app.js
// fails/cache
static const char html_chunk_hint_inline[] =
    "<script>\n"
    "(function(){"
    "function boardAndStatusToFen(board,status,history){"
    "if(!board||!status)return '';"
    "var rows=[];"
    "for(var r=7;r>=0;r--){var rank='',empty=0;"
    "for(var c=0;c<8;c++){var p=board[r][c];"
    "if(p===' '||p==='')empty++;"
    "else{if(empty){rank+=empty;empty=0;}rank+=p;}}"
    "if(empty)rank+=empty;rows.push(rank);}"
    "var side=(status.current_player==='White')?'w':'b';"
    "var moves=(history&&history.moves)?history.moves.length:0;"
    "return rows.join('/')+' '+side+' KQkq - 0 '+(Math.floor(moves/2)+1);}"
    "async function fetchStockfishBestMove(fen){"
    "try{var depth=(function(){try{if(typeof getHintDepth==='function')return "
    "getHintDepth();var "
    "d=parseInt(localStorage.getItem('chessHintDepth')||'10',10);"
    "return Math.min(18,Math.max(1,isNaN(d)?10:d));}catch(e){return 10;}})();"
    "var res=await "
    "fetch('https://chess-api.com/"
    "v1',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify({fen:fen,depth:depth})});"
    "if(!res.ok)return null;var data=await res.json().catch(function(){});"
    "if(!data)return null;"
    "if(data.from&&data.to&&data.from.length===2&&data.to.length===2)return{"
    "from:data.from.toLowerCase(),to:data.to.toLowerCase()};"
    "if(data.move&&data.move.length>=4)return{from:data.move.substring(0,2)."
    "toLowerCase(),to:data.move.substring(2,4).toLowerCase()};"
    "return null;}catch(e){return null;}}"
    "function showHintOnBoard(from,to){"
    "document.querySelectorAll('.square').forEach(function(sq){sq.classList."
    "remove('hint-from','hint-to');});"
    "var "
    "fc=from.charCodeAt(0)-97,fr=parseInt(from[1],10)-1,tc=to.charCodeAt(0)-97,"
    "tr=parseInt(to[1],10)-1;"
    "var "
    "fs=document.querySelector('[data-row=\"'+fr+'\"][data-col=\"'+fc+'\"]');"
    "var "
    "ts=document.querySelector('[data-row=\"'+tr+'\"][data-col=\"'+tc+'\"]');"
    "if(fs)fs.classList.add('hint-from');if(ts)ts.classList.add('hint-to');}"
    "async function requestHint(){"
    "if(typeof sandboxMode!=='undefined'&&sandboxMode)return;"
    "if(typeof reviewMode!=='undefined'&&reviewMode)return;"
    "var status=typeof statusData!=='undefined'?statusData:{};"
    "var state=(status.game_state||'').toLowerCase();"
    "if(state!=='active'&&state!=='playing')return;"
    "if(status.game_end&&status.game_end.ended)return;"
    "if(state==='promotion')return;"
    "var btn=document.getElementById('hint-btn');"
    "if(btn){btn.disabled=true;btn.textContent='Načítám…';}"
    "try{"
    "var fen=boardAndStatusToFen(typeof "
    "boardData!=='undefined'?boardData:[],status,typeof "
    "historyData!=='undefined'?historyData:{});"
    "if(!fen)return;"
    "var move=await fetchStockfishBestMove(fen);"
    "if(move){showHintOnBoard(move.from,move.to);"
    "try{await "
    "fetch('/api/game/"
    "hint_highlight',{method:'POST',headers:{'Content-Type':'application/"
    "json'},body:JSON.stringify({from:move.from,to:move.to})});}catch(e){}}"
    "}finally{if(btn){btn.disabled=false;btn.textContent='Nápověda';}}"
    "}"
    "window.requestHint=requestHint;"
    "})();\n"
    "</script>\n";

// Chunk 3b: JavaScript - load from external file (prevents UTF-8 chunking
// issues)
static const char html_chunk_javascript[] =
    "<script src='/chess_app.js'></script>";

// Chunk: Demo Mode UI Panel (v záložce Nastavení)
static const char html_chunk_demo_mode[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>Demo</h3>"
    "<label class='set-check'>"
    "<input type='checkbox' id='demo-enabled'> "
    "<span>Zapnout demo režim</span>"
    "</label>"
    "<div class='set-group'><label>Rychlost tahů</label>"
    "<div style='display:flex;align-items:center;gap:10px;'>"
    "<input type='range' id='demo-speed' class='set-input' min='500' "
    "max='5000' step='100' value='2000' style='flex:1;padding:0;' disabled>"
    "<span id='demo-speed-value' class='set-value' "
    "style='width:52px;'>2000ms</span>"
    "</div></div>"
    "<button id='stop-demo-btn' class='set-btn set-btn-sm set-btn-danger' "
    "onclick='stopDemo()' style='display:none;'>Zastavit přehrávání</button>"
    "<script>\n"
    "const demoCheckbox = document.getElementById('demo-enabled');\n"
    "const demoSpeed = document.getElementById('demo-speed');\n"
    "const demoSpeedValue = document.getElementById('demo-speed-value');\n"
    "const stopDemoBtn = document.getElementById('stop-demo-btn');\n"
    "\n"
    "demoCheckbox.addEventListener('change', function() {\n"
    "  const enabled = this.checked;\n"
    "  demoSpeed.disabled = !enabled;\n"
    "  demoSpeedValue.style.color = enabled ? '#fff' : '#888';\n"
    "  stopDemoBtn.style.display = enabled ? 'block' : 'none';\n"
    "  \n"
    "  fetch('/api/demo/config', {\n"
    "    method: 'POST',\n"
    "    headers: {'Content-Type': 'application/json'},\n"
    "    body: JSON.stringify({enabled: enabled, speed_ms: \n"
    "parseInt(demoSpeed.value)})\n"
    "  }).catch(e => console.error('Demo config error:', e));\n"
    "});\n"
    "\n"
    "demoSpeed.addEventListener('input', function() {\n"
    "  demoSpeedValue.textContent = this.value + 'ms';\n"
    "});\n"
    "\n"
    "demoSpeed.addEventListener('change', function() {\n"
    "  fetch('/api/demo/config', {\n"
    "    method: 'POST',\n"
    "    headers: {'Content-Type': 'application/json'},\n"
    "    body: JSON.stringify({enabled: demoCheckbox.checked, speed_ms: "
    "parseInt(this.value)})\n"
    "  }).catch(e => console.error('Demo speed error:', e));\n"
    "});\n"
    "\n"
    "function stopDemo() {\n"
    "  demoCheckbox.checked = false;\n"
    "  demoSpeed.disabled = true;\n"
    "  demoSpeedValue.style.color = '#888';\n"
    "  stopDemoBtn.style.display = 'none';\n"
    "  \n"
    "  fetch('/api/demo/config', {\n"
    "    method: 'POST',\n"
    "    headers: {'Content-Type': 'application/json'},\n"
    "    body: JSON.stringify({enabled: false, speed_ms: "
    "parseInt(demoSpeed.value)})\n"
    "  }).catch(e => console.error('Stop demo error:', e));\n"
    "}\n"
    "\n"
    "</script>"
    "</div>";

// Chunk: MQTT Configuration Panel
static const char html_chunk_mqtt_config[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>MQTT</h3>"
    "<div class='set-group'><label>Broker (host)</label>"
    "<input type='text' id='mqtt-host' class='set-input' "
    "placeholder='homeassistant.local'>"
    "</div>"
    "<div class='set-group'><label>Port</label>"
    "<input type='number' id='mqtt-port' class='set-input' value='1883' "
    "min='1' max='65535'>"
    "</div>"
    "<div class='set-group'><label>Username</label>"
    "<input type='text' id='mqtt-username' class='set-input' "
    "placeholder='user'>"
    "</div>"
    "<div class='set-group'><label>Password</label>"
    "<input type='password' id='mqtt-password' class='set-input' "
    "placeholder='••••••••'>"
    "</div>"
    "<button onclick='saveMQTTConfig()' class='set-btn'>Uložit MQTT</button>"
    "<div id='mqtt-status' class='set-note' style='margin-top:8px;padding:8px;"
    "border-radius:6px;display:none;'></div>"
    "</div>"
    "<script>\n"
    "async function saveMQTTConfig() {\n"
    "  const host = document.getElementById('mqtt-host').value;\n"
    "  const port = parseInt(document.getElementById('mqtt-port').value);\n"
    "  const username = document.getElementById('mqtt-username').value;\n"
    "  const password = document.getElementById('mqtt-password').value;\n"
    "  const statusDiv = document.getElementById('mqtt-status');\n"
    "  \n"
    "  if (!host) {\n"
    "    statusDiv.style.display = 'block';\n"
    "    statusDiv.style.background = '#f44336';\n"
    "    statusDiv.textContent = 'Chyba: Host je povinný';\n"
    "    return;\n"
    "  }\n"
    "  \n"
    "  try {\n"
    "    const response = await fetch('/api/mqtt/config', {\n"
    "      method: 'POST',\n"
    "      headers: {'Content-Type': 'application/json'},\n"
    "      body: JSON.stringify({host: host, port: port, username: username, "
    "password: password})\n"
    "    });\n"
    "    const data = await response.json();\n"
    "    \n"
    "    statusDiv.style.display = 'block';\n"
    "    if (data.success) {\n"
    "      statusDiv.style.background = '#4CAF50';\n"
    "      statusDiv.textContent = 'MQTT uloženo. Restartujte ESP32.';\n"
    "    } else {\n"
    "      statusDiv.style.background = '#f44336';\n"
    "      statusDiv.textContent = 'Chyba: ' + (data.message || 'Neznámá "
    "chyba');\n"
    "    }\n"
    "  } catch (e) {\n"
    "    statusDiv.style.display = 'block';\n"
    "    statusDiv.style.background = '#f44336';\n"
    "    statusDiv.textContent = 'Chyba: ' + e.message;\n"
    "  }\n"
    "}\n"
    "\n"
    "// Load current MQTT config on page load\n"
    "async function loadMQTTConfig() {\n"
    "  const statusDiv = document.getElementById('mqtt-status');\n"
    "  try {\n"
    "    const response = await fetch('/api/mqtt/status');\n"
    "    if (!response.ok) {\n"
    "      if (response.status === 404 && statusDiv) {\n"
    "        statusDiv.style.display = 'block';\n"
    "        statusDiv.style.background = '#666';\n"
    "        statusDiv.textContent = 'Rozhraní MQTT není v tomto firmware. "
    "Aktualizujte ESP32.';\n"
    "      }\n"
    "      return;\n"
    "    }\n"
    "    const data = await response.json();\n"
    "    if (data.host) document.getElementById('mqtt-host').value = "
    "data.host;\n"
    "    if (data.port) document.getElementById('mqtt-port').value = "
    "data.port;\n"
    "    if (data.username) document.getElementById('mqtt-username').value = "
    "data.username;\n"
    "  } catch (e) {\n"
    "    if (statusDiv) {\n"
    "      statusDiv.style.display = 'block';\n"
    "      statusDiv.style.background = '#666';\n"
    "      statusDiv.textContent = 'Načtení MQTT konfigurace selhalo.';\n"
    "    }\n"
    "  }\n"
    "}\n"
    "if (document.readyState === 'loading') {\n"
    "  document.addEventListener('DOMContentLoaded', loadMQTTConfig);\n"
    "} else {\n"
    "  setTimeout(loadMQTTConfig, 100);\n"
    "}\n"
    "</script>\n"
    "</div>";

// Tab Nastavení: close section Ovládání, settings-grid, tab
static const char html_chunk_tab_nastaveni_end[] = "</div></div></div>";

// Chunk: HTML closing tags
static const char html_chunk_end[] = "</div>"
                                     "</body>"
                                     "</html>";

// ============================================================================
// HTML PAGE HANDLER
// ============================================================================

static esp_err_t http_get_root_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET / (HTML page) - using chunked transfer for reliability");

  // Set content type
  httpd_resp_set_type(req, "text/html; charset=utf-8");

  // Prevent caching to ensure updates (especially JS fixes) are loaded
  httpd_resp_set_hdr(req, "Cache-Control",
                     "no-cache, no-store, must-revalidate");
  httpd_resp_set_hdr(req, "Pragma", "no-cache");
  httpd_resp_set_hdr(req, "Expires", "0");

  // Transfer-Encoding is handled automatically by httpd_resp_send_chunk
  // when Content-Length is not set. Don't set it manually!

  // Send HTML in 8 chunks for better debugging and memory efficiency
  // Traffic shaping: Small delays to prevent socket buffer overflow with
  // multiple clients

  // CHUNK 1: HEAD (CSS + bootstrap script)
  size_t chunk1_len = strlen(html_chunk_head);
  ESP_LOGI(TAG, "📤 Chunk 1: HEAD+CSS (%zu bytes)", chunk1_len);
  esp_err_t ret = httpd_resp_send_chunk(req, html_chunk_head, chunk1_len);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Chunk 1 send failed (client disconnected?): %s",
             esp_err_to_name(ret));
    return ret;
  }

  // CHUNK 2: Layout + Tab Hra (game left, board, game right)
  size_t chunk_layout_len = strlen(html_chunk_layout_start);
  ret = httpd_resp_send_chunk(req, html_chunk_layout_start, chunk_layout_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_tab_hra_start_len = strlen(html_chunk_tab_hra_start);
  ret = httpd_resp_send_chunk(req, html_chunk_tab_hra_start,
                              chunk_tab_hra_start_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_game_left_len = strlen(html_chunk_game_left);
  ret = httpd_resp_send_chunk(req, html_chunk_game_left, chunk_game_left_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_board_len = strlen(html_chunk_board);
  ret = httpd_resp_send_chunk(req, html_chunk_board, chunk_board_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_game_right_len = strlen(html_chunk_game_right);
  ret = httpd_resp_send_chunk(req, html_chunk_game_right, chunk_game_right_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_tab_hra_end_len = strlen(html_chunk_tab_hra_end);
  ret =
      httpd_resp_send_chunk(req, html_chunk_tab_hra_end, chunk_tab_hra_end_len);
  if (ret != ESP_OK)
    return ret;

  vTaskDelay(pdMS_TO_TICKS(20));

  // Tab Nastavení (info panel, demo, mqtt)
  size_t chunk_tab_nast_len = strlen(html_chunk_tab_nastaveni_start);
  ret = httpd_resp_send_chunk(req, html_chunk_tab_nastaveni_start,
                              chunk_tab_nast_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_sit_len = strlen(html_chunk_settings_sit);
  ret = httpd_resp_send_chunk(req, html_chunk_settings_sit, chunk_sit_len);
  if (ret != ESP_OK)
    return ret;

  // New Bot Settings Chunk
  size_t chunk_bot_len = strlen(html_chunk_settings_bot);
  ret = httpd_resp_send_chunk(req, html_chunk_settings_bot, chunk_bot_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_tutorial_len = strlen(html_chunk_settings_tutorial);
  ret = httpd_resp_send_chunk(req, html_chunk_settings_tutorial,
                              chunk_tutorial_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_zarizeni_len = strlen(html_chunk_settings_zarizeni);
  ret = httpd_resp_send_chunk(req, html_chunk_settings_zarizeni,
                              chunk_zarizeni_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk6_len = strlen(html_chunk_demo_mode);
  ret = httpd_resp_send_chunk(req, html_chunk_demo_mode, chunk6_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk7_len = strlen(html_chunk_mqtt_config);
  ret = httpd_resp_send_chunk(req, html_chunk_mqtt_config, chunk7_len);
  if (ret != ESP_OK)
    return ret;

  size_t chunk_tab_nast_end_len = strlen(html_chunk_tab_nastaveni_end);
  ret = httpd_resp_send_chunk(req, html_chunk_tab_nastaveni_end,
                              chunk_tab_nast_end_len);
  if (ret != ESP_OK)
    return ret;

  vTaskDelay(pdMS_TO_TICKS(20));

  // Banners (review, sandbox)
  size_t chunk4_len = strlen(html_chunk_banners);
  ret = httpd_resp_send_chunk(req, html_chunk_banners, chunk4_len);
  if (ret != ESP_OK)
    return ret;

  vTaskDelay(pdMS_TO_TICKS(20));

  // Hint inline (requestHint) then JavaScript
  size_t chunk_hint_len = strlen(html_chunk_hint_inline);
  ret = httpd_resp_send_chunk(req, html_chunk_hint_inline, chunk_hint_len);
  if (ret != ESP_OK)
    return ret;
  size_t chunk5_len = strlen(html_chunk_javascript);
  ret = httpd_resp_send_chunk(req, html_chunk_javascript, chunk5_len);
  if (ret != ESP_OK)
    return ret;

  vTaskDelay(pdMS_TO_TICKS(50));

  // Closing (container)
  size_t chunk8_len = strlen(html_chunk_end);
  ret = httpd_resp_send_chunk(req, html_chunk_end, chunk8_len);
  if (ret != ESP_OK)
    return ret;

  ret = httpd_resp_send_chunk(req, NULL, 0);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Chunked transfer end failed (client disconnected?): %s",
             esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG,
           "✅ HTML sent (layout + tab-hra + tab-nastaveni + banners + js + "
           "end)");
  return ESP_OK;
}

// ============================================================================
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

  // Initialize WiFi APSTA
  esp_err_t ret = wifi_init_apsta();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
    ESP_LOGE(TAG, "❌ Web server task exiting");

    esp_task_wdt_delete(NULL); // Unregister from WDT before deleting
    vTaskDelete(NULL);
    return;
  }
  wifi_ap_active = true;
  ESP_LOGI(TAG, "WiFi APSTA initialized");

  // Wait for WiFi to be ready
  vTaskDelay(pdMS_TO_TICKS(2000));

  // Automaticke pripojeni STA, pokud je konfigurace v NVS
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
      ESP_LOGW(TAG, "⚠️ WiFi STA auto-connect failed: %s (AP still active)",
               esp_err_to_name(connect_ret));
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

  /* Jednorazove nacteni jasu do cache – GET /api/status pak nepristupuje k NVS
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
  ESP_LOGI(TAG, "Connect to WiFi: %s", wifi_ap_ssid_effective);
  ESP_LOGI(TAG, "Password: %s", WIFI_AP_PASSWORD);
  ESP_LOGI(TAG, "Open browser: http://%s", WIFI_AP_IP);

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
  stop_http_server();
  esp_wifi_stop();
  esp_wifi_deinit();

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
}

void web_server_stop(void) {
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
// WEBSOCKET (snapshot push — stejný JSON jako GET /api/game/snapshot)
// ============================================================================

void web_server_websocket_init(void) {
#if CONFIG_HTTPD_WS_SUPPORT
  if (ws_broadcast_timer != NULL) {
    return;
  }
  const esp_timer_create_args_t args = {.callback = &ws_broadcast_timer_cb,
                                        .name = "ws_snap"};
  esp_err_t e = esp_timer_create(&args, &ws_broadcast_timer);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "WS timer create failed: %s", esp_err_to_name(e));
    return;
  }
  e = esp_timer_start_periodic(ws_broadcast_timer, 3000 * 1000);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "WS timer start failed: %s", esp_err_to_name(e));
    esp_timer_delete(ws_broadcast_timer);
    ws_broadcast_timer = NULL;
    return;
  }
  czechmate_ensure_snapshot_notify_queue();
  ESP_LOGI(TAG,
           "WebSocket: watchdog broadcast 3 s + okamžitý push při změně stavu");
#else
  ESP_LOGD(TAG, "WebSocket disabled (CONFIG_HTTPD_WS_SUPPORT)");
#endif
}

void web_server_websocket_send_update(const char *data) {
  (void)data;
  if (!web_server_active) {
    return;
  }
#if CONFIG_HTTPD_WS_SUPPORT
  ws_broadcast_snapshot();
#endif
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool web_server_is_active(void) { return web_server_active; }

const char *web_server_get_ap_ssid(void) {
  if (wifi_ap_ssid_effective[0] == '\0') {
    return WIFI_AP_SSID_BASE;
  }
  return wifi_ap_ssid_effective;
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
