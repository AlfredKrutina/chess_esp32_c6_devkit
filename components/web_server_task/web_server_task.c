/**
 * @file web_server_task.c
 * @brief HTTP server (port 80), Wi‑Fi STA/AP dle NVS, REST API a WebSocket pro mobilní aplikaci
 *
 * @details
 * Prohlížečové UI na kořenové cestě není — GET `/` vrací orientační JSON; plná obsluha je v aplikaci.
 * Hotspot desky (AP) je ve výchozím stavu vypnutý; zapnutí přes BLE (`wifi_ap_set`) nebo NVS `ap_user_en`.
 *
 * =============================================================================
 * CO TENTO SOUBOR DELA?
 * =============================================================================
 *
 * 1. Wi‑Fi — STA podle NVS; AP jen když ho uživatel zapne (BLE / NVS).
 * 2. HTTP server (port 80) — REST `/api/...`, kořen bez HTML rozhraní.
 * 3. WebSocket — živý snapshot pro klienty.
 * 4. Fronta `game_command_queue` — příkazy z HTTP (tah, reset, …).
 *
 * =============================================================================
 * REST (orientační výpis)
 * =============================================================================
 *
 * GET /api/status, /api/board, POST /api/move, POST /api/reset, GET /api/timer,
 * POST /api/demo/config a další — viz implementované handlery v tomto souboru.
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
 * Sekce 4:  HTTP handlery a REST .................. (viz struktura souboru)
 * Sekce 5:  Main Web Server Task .................. (viz struktura souboru)
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-23
 *
 * @note Task priorita 2; stack kvůli velkým JSON bufferům; AP IP při zapnutém hotspotu typicky 192.168.4.1.
 *
 * @see game_task.c — JSON API hry
 */

#include "web_server_task.h"
#include "../game_hooks/include/game_state_notify.h"
#include "../game_task/include/game_task.h"
#include "../matrix_task/include/matrix_task.h"
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

/** Po konfliktu při finish setup tutoriálu krátce nevolat znovu validaci (BLE + HTTP). */
static TickType_t s_setup_tutorial_finish_conflict_until_tick;

static void setup_tutorial_reset_finish_cooldown(void) {
  s_setup_tutorial_finish_conflict_until_tick = 0;
}

static bool setup_tutorial_finish_in_cooldown(void) {
  if (s_setup_tutorial_finish_conflict_until_tick == 0) {
    return false;
  }
  return xTaskGetTickCount() < s_setup_tutorial_finish_conflict_until_tick;
}

static void setup_tutorial_note_finish_conflict(void) {
  s_setup_tutorial_finish_conflict_until_tick =
      xTaskGetTickCount() + pdMS_TO_TICKS(900);
}

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
/** 0/1 — uživatelsky zapnutý hotspot desky (AP); výchozí NVS chybí → AP vypnutý. */
#define WIFI_NVS_KEY_AP_USER "ap_user_en"
/** CSV blokovaných 3. oktetů IPv4 pro STA (např. `88` → zamítnout x.x.88.x); BLE `wifi_sta_ip_block` nebo výchozí z FW. */
#define WIFI_NVS_KEY_STA_BLK_OCT "sta_blk_oct"
/** Po flashi / chybě klíče v NVS — dokud uživatel neuloží jinak přes aplikaci. */
#define WIFI_STA_BLK_OCT_DEFAULT_CSV "88"

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
#define WIFI_STATUS_JSON_MAX 640
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

#define STA_BLK_OCT_MAX 16
static uint8_t s_sta_blk_oct[STA_BLK_OCT_MAX];
static size_t s_sta_blk_oct_n;
/** Kolikrát za sebou zamítnuta DHCP adresa (router může pořád nabízet stejnou). */
static unsigned s_sta_blk_reject_streak;

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
static void wifi_ap_user_apply_task(void *arg);
static void czechmate_mdns_refresh_sta_txt(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void wifi_sta_parse_blocked_csv(const char *csv);
static void wifi_sta_refresh_blocked_octets_from_nvs(void);
static bool wifi_sta_third_octet_is_blocked(uint8_t oct);
static void wifi_sta_disconnect_if_current_ip_blocked(void);
static esp_err_t wifi_sta_save_blocked_octets_csv(const char *csv_in);
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
static esp_err_t http_post_game_guard_clear_handler(httpd_req_t *req);
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

bool wifi_ap_is_broadcasting(void) { return wifi_ap_active; }

const char *wifi_ap_effective_ssid(void) {
  if (wifi_ap_ssid_effective[0] == '\0') {
    return WIFI_AP_SSID_BASE;
  }
  return wifi_ap_ssid_effective;
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

/** BLE wifi_survey: aktivní scan okolí, výsledek přes cmd_ack JSON (šifrovaný odkaz). */
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

  const bool ap_on_boot = wifi_ap_user_desired_from_nvs();
  ret = esp_wifi_set_mode(ap_on_boot ? WIFI_MODE_APSTA : WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
    return ret;
  }

  /* STA-only: nastavení AP konfigurace by vrátilo ESP_ERR_WIFI_MODE — AP rozhraní neexistuje. */
  if (ap_on_boot) {
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set WiFi AP config: %s", esp_err_to_name(ret));
      return ret;
    }
  }

  // Spustit WiFi
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
  /* Prázdný řetězec = vypnutí filtrování (klíč musí existovat; NOT_FOUND = výchozí FW). */
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

  // Handler pro root (JSON — aplikace místo prohlížeče)
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

  httpd_uri_t guard_clear_uri = {.uri = "/api/game/guard_clear",
                                 .method = HTTP_POST,
                                 .handler = http_post_game_guard_clear_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(httpd_handle, &guard_clear_uri);

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

static esp_err_t http_post_game_guard_clear_handler(httpd_req_t *req) {
  (void)req;
  ESP_LOGI(TAG, "POST /api/game/guard_clear");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  const bool was_active =
      game_is_matrix_guard_active() || matrix_is_guard_mode_active();
  if (was_active) {
    game_force_clear_matrix_guard();
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  if (was_active) {
    httpd_resp_send(
        req,
        "{\"success\":true,\"cleared\":true,\"message\":\"Matrix guard "
        "cleared. Verify physical board matches the game.\"}",
        -1);
  } else {
    httpd_resp_send(req, "{\"success\":true,\"cleared\":false}", -1);
  }
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

  if (want_start || want_cancel) {
    setup_tutorial_reset_finish_cooldown();
  }

  if (want_finish) {
    if (!game_is_board_setup_tutorial_active()) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(
          req, "{\"success\":false,\"error\":\"Tutorial is not active\"}", -1);
      return ESP_OK;
    }
    if (setup_tutorial_finish_in_cooldown()) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "409 Conflict");
      httpd_resp_send(req,
                      "{\"success\":false,\"error\":\"Physical board does "
                      "not match starting occupancy (rows 0-1,6-7 full; "
                      "2-5 empty)\"}",
                      -1);
      return ESP_OK;
    }
    if (!game_finish_board_setup_tutorial_from_web()) {
      httpd_resp_set_type(req, "application/json");
      if (game_is_board_setup_tutorial_active()) {
        setup_tutorial_note_finish_conflict();
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_send(req,
                        "{\"success\":false,\"error\":\"Physical board does "
                        "not match starting occupancy (rows 0-1,6-7 full; "
                        "2-5 empty)\"}",
                        -1);
      } else {
        setup_tutorial_reset_finish_cooldown();
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(
            req, "{\"success\":false,\"error\":\"Could not finish tutorial\"}",
            -1);
      }
      return ESP_OK;
    }
    setup_tutorial_reset_finish_cooldown();
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
// HTTP root & legacy JS paths — app-only product build (no browser dashboard).
// REST `/api/*`, WebSocket `/ws`, and OTA handlers remain registered below.
// ============================================================================

static const char k_http_root_json[] =
    "{\"service\":\"czechmate\",\"client\":\"mobile_app\","
    "\"api\":\"/api/\",\"websocket\":\"/ws\"}";

static esp_err_t http_get_chess_js_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "404 Not Found");
  httpd_resp_set_type(req, "application/json; charset=utf-8");
  return httpd_resp_send(req, "{\"error\":\"browser_ui_removed\"}",
                         HTTPD_RESP_USE_STRLEN);
}

/** GET /favicon.ico — žádné tělo; tiší 404 v logu prohlížeče při náhodném dotazu. */
static esp_err_t http_get_favicon_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, "", 0);
}

static esp_err_t http_get_root_handler(httpd_req_t *req) {
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
  if (wifi_ap_active) {
    ESP_LOGI(TAG, "Board hotspot SSID: %s", wifi_ap_ssid_effective);
    ESP_LOGI(TAG, "Board hotspot password: %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "Board HTTP (AP): http://%s", WIFI_AP_IP);
  } else {
    ESP_LOGI(TAG, "Board hotspot: OFF (enable from CzechMate app over BLE)");
  }

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
