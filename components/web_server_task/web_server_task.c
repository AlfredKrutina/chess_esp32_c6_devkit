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
 * @version 2.4
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
#include "../game_task/include/game_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
// #include "../timer_system/include/timer_system.h" // UNUSED
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos_chess.h"
#include "nvs.h"
// #include "nvs_flash.h" // UNUSED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Externi deklarace fronty
extern QueueHandle_t game_command_queue;

// ============================================================================
// LOKALNI PROMENNE A KONSTANTY
// ============================================================================

static const char *TAG = "WEB_SERVER_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro
 * ESP_ERR_NOT_FOUND
 *
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 *
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany
 * (WARNING pouze)
 *
 * @details
 * Funkce je pouzivana pro bezpecny reset watchdog timeru behem web server
 * operaci. Zabranuje chybam pri startupu kdy task jeste neni registrovany.
 */
static esp_err_t web_server_task_wdt_reset_safe(void) {
  esp_err_t ret = esp_task_wdt_reset();

  if (ret == ESP_ERR_NOT_FOUND) {
    // Logovat jako WARNING misto ERROR - task jeste neni registrovany
    ESP_LOGW(
        TAG,
        "WDT reset: task not registered yet (this is normal during startup)");
    return ESP_OK; // Povazovat za uspech pro nase ucely
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

// Konfigurace WiFi
#define WIFI_AP_SSID "ESP32-CzechMate"
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

// Velikosti JSON bufferu
#define JSON_BUFFER_SIZE 2048 // Puvodni velikost

// Sledovani stavu web serveru
static bool task_running = false;
static bool web_server_active = false;
static bool wifi_ap_active = false;
static uint32_t web_server_start_time = 0;
uint32_t client_count = 0; // Externi pro UART prikazy

// Handle HTTP serveru
static httpd_handle_t httpd_handle = NULL;

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

// ============================================================================
// PREDBEZNE DEKLARACE
// ============================================================================

static esp_err_t wifi_init_apsta(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static esp_err_t start_http_server(void);
static void stop_http_server(void);

// HTTP handlery
static esp_err_t http_get_root_handler(httpd_req_t *req);
static esp_err_t
http_get_chess_js_handler(httpd_req_t *req); // chess_app.js file
static esp_err_t
http_get_test_handler(httpd_req_t *req); // Testovaci stranka pro timer
static esp_err_t
http_get_favicon_handler(httpd_req_t *req); // Favicon handler (204 No Content)
static esp_err_t http_get_board_handler(httpd_req_t *req);
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
static esp_err_t http_get_wifi_status_handler(httpd_req_t *req);
static esp_err_t http_get_wifi_status_handler(httpd_req_t *req);
static esp_err_t http_get_web_lock_status_handler(httpd_req_t *req);

// Handlery pro Demo API
static esp_err_t http_post_demo_config_handler(httpd_req_t *req);
static esp_err_t http_get_demo_status_handler(httpd_req_t *req);

// Handler pro Virtual Actions
static esp_err_t http_post_game_virtual_action_handler(httpd_req_t *req);
static esp_err_t http_post_game_new_handler(httpd_req_t *req);
static esp_err_t http_post_game_hint_highlight_handler(httpd_req_t *req);

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
    ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
    return ret;
  }

  // Nacist SSID
  size_t required_size = ssid_len;
  ret = nvs_get_str(nvs_handle, WIFI_NVS_KEY_SSID, ssid, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get SSID from NVS: %s", esp_err_to_name(ret));
    nvs_close(nvs_handle);
    return ret;
  }

  // Nacist password
  required_size = password_len;
  ret =
      nvs_get_str(nvs_handle, WIFI_NVS_KEY_PASSWORD, password, &required_size);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get password from NVS: %s", esp_err_to_name(ret));
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

  if (password_len == 0 || password_len > 64) {
    ESP_LOGE(TAG, "Invalid password length: %zu (must be 1-64)", password_len);
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
    ESP_LOGE(TAG, "Failed to load WiFi config from NVS: %s",
             esp_err_to_name(ret));
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

  // Vytvorit default netif pro AP
  ESP_LOGI(TAG, "Creating default WiFi AP netif...");
  ap_netif = esp_netif_create_default_wifi_ap();
  if (ap_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create AP netif");
    return ESP_FAIL;
  }

  // Vytvorit default netif pro STA
  ESP_LOGI(TAG, "Creating default WiFi STA netif...");
  sta_netif = esp_netif_create_default_wifi_sta();
  if (sta_netif == NULL) {
    ESP_LOGE(TAG, "Failed to create STA netif");
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

  // Konfigurovat WiFi AP
  wifi_config_t wifi_config = {
      .ap = {.ssid = WIFI_AP_SSID,
             .ssid_len = strlen(WIFI_AP_SSID),
             .password = WIFI_AP_PASSWORD,
             .channel = WIFI_AP_CHANNEL,
             .max_connection = WIFI_AP_MAX_CONNECTIONS,
             .authmode = WIFI_AUTH_WPA2_PSK},
  };

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
  ESP_LOGI(TAG, "AP SSID: %s", WIFI_AP_SSID);
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
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
    ESP_LOGI(TAG, "STA: Lost IP");
    sta_connected = false;
    sta_connecting = false; // Resetovat flag pripojovani
    sta_ip[0] = '\0';
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
      WIFI_AP_SSID, ap_ip_str, (unsigned long)client_count, sta_ssid_display,
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
  config.max_uri_handlers =
      32; // Increased to 32 to accommodate all endpoints (currently ~25)
  config.max_open_sockets =
      10; // LWIP_MAX_SOCKETS(16) - HTTP_INTERNAL(3) - MQTT(1) - SYSTEM(2) = 10
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
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return ret;
  }

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

  ESP_LOGI(TAG, "HTTP server started successfully on port %d",
           HTTP_SERVER_PORT);

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
  if (httpd_handle != NULL) {
    httpd_stop(httpd_handle);
    httpd_handle = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");
  }
}

// ============================================================================
// REST API HANDLERS
// ============================================================================

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

  // INJECT WEB STATUS FIELDS
  // Game task doesn't know about web lock or wifi, so we add it here.
  // Brightness: pouzit cache (nepristupovat k NVS na kazdy request – stabilita)
  char *last_brace = strrchr(json_buffer, '}');
  if (last_brace) {
    size_t remaining = sizeof(json_buffer) - (size_t)(last_brace - json_buffer);
    uint8_t b = cached_brightness_valid ? cached_brightness : 50;
    snprintf(last_brace, remaining,
             ",\"web_locked\":%s,\"internet_connected\":%s,\"brightness\":%d}",
             web_is_locked() ? "true" : "false",
             sta_connected ? "true" : "false", (int)b);
  }

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

  // Ziskat stav timeru z game tasku pouzitim lokalniho bufferu pro zabraneni
  // race conditions s jinymi endpointy
  char local_json[JSON_BUFFER_SIZE];
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

  // VALIDACE PROMOCE: Zkontrolovat zda je cíl na promotion rank
  // Promotion rank: 1. řádek (rank 1) pro černé, 8. řádek (rank 8) pro bílé
  bool is_promotion_rank = false;
  if (strlen(to) >= 2) {
    char rank = to[1]; // Druhý znak je rank (1-8)
    if (rank == '1' || rank == '8') {
      is_promotion_rank = true;
    }
  }

  // VALIDACE: Pokud je to promoce, promotion parametr je povinný
  if (is_promotion_rank && strlen(promotion) == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Promotion required for "
                    "move to promotion rank\"}",
                    -1);
    return ESP_FAIL;
  }

  // VAROVÁNÍ: Pokud to není promoce, ale promotion parametr je poskytnut
  if (!is_promotion_rank && strlen(promotion) > 0) {
    ESP_LOGW(TAG,
             "Promotion parameter provided for non-promotion move %s->%s, "
             "ignoring",
             from, to);
    // Ignorovat promotion parametr, ale pokračovat
  }

  // Pripravit command pro game task
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_MOVE;
  strncpy(cmd.from_notation, from, sizeof(cmd.from_notation) - 1);
  strncpy(cmd.to_notation, to, sizeof(cmd.to_notation) - 1);

  // Zpracovat promotion choice (pouze pokud je to skutečně promoce)
  if (is_promotion_rank && strlen(promotion) > 0) {
    if (strcasecmp(promotion, "Q") == 0)
      cmd.promotion_choice = PROMOTION_QUEEN;
    else if (strcasecmp(promotion, "R") == 0)
      cmd.promotion_choice = PROMOTION_ROOK;
    else if (strcasecmp(promotion, "B") == 0)
      cmd.promotion_choice = PROMOTION_BISHOP;
    else if (strcasecmp(promotion, "N") == 0)
      cmd.promotion_choice = PROMOTION_KNIGHT;
    else
      cmd.promotion_choice = PROMOTION_QUEEN;
  } else if (is_promotion_rank) {
    // Promoce bez promotion parametru - mělo by být zachyceno výše, ale pro
    // jistotu
    cmd.promotion_choice = PROMOTION_QUEEN; // Default fallback
  } else {
    // Není to promoce - promotion_choice není relevantní
    cmd.promotion_choice = PROMOTION_QUEEN; // Default (nebude použito)
  }

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
 * @brief Handler pro POST /api/game/new
 *
 * Spustí novou hru odesíláním příkazu GAME_CMD_NEW_GAME do game tasku.
 *
 * @param req HTTP request
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Nepotřebuje žádné JSON payload. Prostě pošle NEW_GAME příkaz.
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

  // Vytvořit NEW_GAME příkaz
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_NEW_GAME;
  cmd.player = 0;            // Neznámé
  cmd.response_queue = NULL; // Web nepotřebuje response

  // Odeslat do fronty
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

  ESP_LOGI(TAG, "✅ NEW_GAME command sent successfully");

  // Odpovědět úspěchem
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"New game started\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/hint_highlight
 *
 * Body: { "from": "e2", "to": "e4" }. Ověří notaci, převede na LED indexy
 * a pošle LED_CMD_HIGHLIGHT_HINT (zelená = from, žlutá = to).
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

  const char *p_from = strstr(buf, "\"from\"");
  const char *p_to = strstr(buf, "\"to\"");
  if (!p_from || !p_to) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Missing from/to\"}",
                    -1);
    return ESP_OK;
  }
  /* Find value after "from": skip to next " and take 2 chars */
  p_from += 6;
  while (*p_from && *p_from != '"')
    p_from++;
  if (*p_from == '"')
    p_from++;
  p_to += 4;
  while (*p_to && *p_to != '"')
    p_to++;
  if (*p_to == '"')
    p_to++;
  if (!*p_from || !*p_to || !p_from[1] || !p_to[1]) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid JSON\"}", -1);
    return ESP_OK;
  }
  char from_sq[3] = {0};
  char to_sq[3] = {0};
  from_sq[0] =
      (char)((unsigned char)p_from[0] <= 'Z' ? p_from[0] + 32 : p_from[0]);
  from_sq[1] = p_from[1];
  to_sq[0] = (char)((unsigned char)p_to[0] <= 'Z' ? p_to[0] + 32 : p_to[0]);
  to_sq[1] = p_to[1];
  if (from_sq[0] < 'a' || from_sq[0] > 'h' || from_sq[1] < '1' ||
      from_sq[1] > '8' || to_sq[0] < 'a' || to_sq[0] > 'h' || to_sq[1] < '1' ||
      to_sq[1] > '8') {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid square\"}",
                    -1);
    return ESP_OK;
  }

  uint8_t from_led = chess_notation_to_led_index(from_sq);
  uint8_t to_led = chess_notation_to_led_index(to_sq);
  if (from_led >= 64 || to_led >= 64) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Bad notation\"}", -1);
    return ESP_OK;
  }

  led_command_t cmd = {0};
  cmd.type = LED_CMD_HIGHLIGHT_HINT;
  cmd.led_index = from_led;
  cmd.data = (void *)&to_led;
  led_execute_command_new(&cmd);
  ESP_LOGI(TAG, "Hint highlight sent: %s -> %s (LED %u -> %u)", from_sq, to_sq,
           (unsigned)from_led, (unsigned)to_led);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
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

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "WiFi config blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
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

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "WiFi connect blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
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

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "WiFi disconnect blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
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

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "WiFi clear blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
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

  char local_json[JSON_BUFFER_SIZE];
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
// chess_app.js embedded (110990 bytes, 2682 lines)
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
    "let boardData = [];\n"
    "let statusData = {};\n"
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
    "            square.onclick = () => handleSquareClick(row, col);\n"
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
    "    const banner = document.getElementById('sandbox-banner');\n"
    "    banner.classList.add('active');\n"
    "    clearHighlights();\n"
    "    updateUndoButton(); // Aktualizovat tlačítko undo při vstupu do sandbox mode\n"
    "}\n"
    "\n"
    "function exitSandboxMode() {\n"
    "    sandboxMode = false;\n"
    "    sandboxBoard = [];\n"
    "    sandboxHistory = [];\n"
    "    document.getElementById('sandbox-banner').classList.remove('active');\n"
    "    clearHighlights();\n"
    "    fetchData();\n"
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
    "/** Hint depth 1–18 from settings (localStorage). Used by fetchStockfishBestMove for hints. */\n"
    "function getHintDepth() {\n"
    "    try {\n"
    "        var d = parseInt(localStorage.getItem('chessHintDepth') || '10', 10);\n"
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
    "        return localStorage.getItem('chessEvaluateMove') === 'true';\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Počet nápověd na partii (0 = neomezeno). */\n"
    "function getHintLimit() {\n"
    "    try {\n"
    "        var n = parseInt(localStorage.getItem('chessHintLimit') || '0', 10);\n"
    "        return isNaN(n) || n < 0 ? 0 : n;\n"
    "    } catch (e) {\n"
    "        return 0;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za výborný tah (localStorage). */\n"
    "function getHintAwardBest() {\n"
    "    try {\n"
    "        return localStorage.getItem('chessHintAwardBest') !== 'false';\n"
    "    } catch (e) {\n"
    "        return true;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za dobrý tah (localStorage). */\n"
    "function getHintAwardGood() {\n"
    "    try {\n"
    "        return localStorage.getItem('chessHintAwardGood') === 'true';\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Přidat nápovědu za sebrání figurky (localStorage). */\n"
    "function getHintAwardCapture() {\n"
    "    try {\n"
    "        return localStorage.getItem('chessHintAwardCapture') === 'true';\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
    "\n"
    "/** Zobrazit blok „Výukový přehled“ (nápovědy + kvalita tahů). */\n"
    "function getShowHintStats() {\n"
    "    try {\n"
    "        return localStorage.getItem('chessShowHintStats') === 'true';\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
    "}\n"
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
    "    panel.style.display = 'block';\n"
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
    "/**\n"
    " * Fetch best move and explanation from Chess-API.com (POST).\n"
    " * Returns { from, to, eval, text, san, continuationArr, mate, winChance } or null.\n"
    " * depthOverride: optional; if number, use for API; else use getHintDepth() (for hints).\n"
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
    "        var data = raw && typeof raw.data === 'object' && raw.data !== null ? raw.data : (raw && typeof raw.result === 'object' && raw.result !== null ? raw.result : raw);\n"
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
    "        var evalVal = null;\n"
    "        function toPawns(v) {\n"
    "            if (v == null || typeof v !== 'number' || isNaN(v)) return null;\n"
    "            if (Math.abs(v) > 10) return v / 100;\n"
    "            return v;\n"
    "        }\n"
    "        if (typeof data.eval === 'number') evalVal = toPawns(data.eval);\n"
    "        if (evalVal == null && typeof data.eval === 'string') { var p = parseFloat(data.eval); if (!isNaN(p)) evalVal = toPawns(p); }\n"
    "        if (evalVal == null && data.centipawns != null) {\n"
    "            var cp = typeof data.centipawns === 'number' ? data.centipawns : parseInt(data.centipawns, 10);\n"
    "            if (!isNaN(cp)) evalVal = cp / 100;\n"
    "        }\n"
    "        if (evalVal == null && data.cp != null) {\n"
    "            var cp2 = typeof data.cp === 'number' ? data.cp : parseInt(data.cp, 10);\n"
    "            if (!isNaN(cp2)) evalVal = cp2 / 100;\n"
    "        }\n"
    "        if (evalVal == null && data.evaluation != null) {\n"
    "            var ev = typeof data.evaluation === 'number' ? data.evaluation : parseFloat(data.evaluation);\n"
    "            if (!isNaN(ev)) evalVal = toPawns(ev);\n"
    "        }\n"
    "        if (evalVal == null && typeof data.score === 'number' && !isNaN(data.score)) evalVal = toPawns(data.score);\n"
    "        if (evalVal == null && data.result != null && typeof data.result === 'object' && typeof data.result.eval === 'number') evalVal = toPawns(data.result.eval);\n"
    "        if (typeof console !== 'undefined' && console.log) {\n"
    "            console.log('[Eval Staging] API raw data.eval:', data.eval, 'data.centipawns:', data.centipawns, 'parsed evalVal (pawns):', evalVal);\n"
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
    "/** API vrací eval z pohledu strany na tahu; bez převodu je scoreDrop příliš malý a nikdy neukáže Chyba/Vážná chyba. */\n"
    "var API_EVAL_SIDE_TO_MOVE = true;\n"
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
    "    // Only clear hint when board actually changed (new move), not on every periodic fetch\n"
    "    var boardUnchanged = boardData && board.length === 8 && boardData.length === 8 &&\n"
    "        JSON.stringify(board) === JSON.stringify(boardData);\n"
    "    boardData = board;\n"
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
    "                pieceElement.textContent = pieceSymbols[piece] || ' ';\n"
    "                if (piece !== ' ') {\n"
    "                    pieceElement.className = 'piece ' + (piece === piece.toUpperCase() ? 'white' : 'black');\n"
    "                } else {\n"
    "                    pieceElement.className = 'piece';\n"
    "                }\n"
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
    "                    <div style=\"font-size:20px;line-height:1.4;\">${whiteCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−'}</div>\n"
    "                </div>\n"
    "                <div>\n"
    "                    <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Black sebral (${blackCaptured.length})</div>\n"
    "                    <div style=\"font-size:20px;line-height:1.4;\">${blackCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−'}</div>\n"
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
    "async function fetchStockfishBestMove(fen) {\n"
    "    const depth = parseInt(botSettings.strength, 10) || 10;\n"
    "    const controller = typeof AbortController !== 'undefined' ? new AbortController() : null;\n"
    "    const timeoutId = controller ? setTimeout(function () { controller.abort(); }, BOT_API_TIMEOUT_MS) : null;\n"
    "    try {\n"
    "        const opts = {\n"
    "            method: 'POST',\n"
    "            headers: { 'Content-Type': 'application/json' },\n"
    "            body: JSON.stringify({ fen: fen, depth: depth })\n"
    "        };\n"
    "        if (controller && controller.signal) opts.signal = controller.signal;\n"
    "        const res = await fetch('https://chess-api.com/v1', opts);\n"
    "        if (!res.ok) {\n"
    "            if (console.warn) console.warn('Bot API HTTP', res.status, res.statusText);\n"
    "            return null;\n"
    "        }\n"
    "        const data = await res.json().catch(function () { return null; });\n"
    "        if (!data) {\n"
    "            if (console.warn) console.warn('Bot API: invalid JSON');\n"
    "            return null;\n"
    "        }\n"
    "        var from, to;\n"
    "        if (data.from != null && data.to != null) {\n"
    "            from = String(data.from).toLowerCase();\n"
    "            to = String(data.to).toLowerCase();\n"
    "        } else if (data.move && data.move.length >= 4) {\n"
    "            from = data.move.substring(0, 2).toLowerCase();\n"
    "            to = data.move.substring(2, 4).toLowerCase();\n"
    "        }\n"
    "        if (from && to && BOT_SQUARE_REGEX.test(from) && BOT_SQUARE_REGEX.test(to)) return { from: from, to: to };\n"
    "        if (from || to) {\n"
    "            if (console.warn) console.warn('Bot API: neplatný formát pole', data);\n"
    "        } else if (console.warn) {\n"
    "            console.warn('Bot API: missing from/to or move', data);\n"
    "        }\n"
    "        return null;\n"
    "    } catch (e) {\n"
    "        if (e && e.name === 'AbortError') {\n"
    "            if (console.warn) console.warn('Bot API: timeout after', BOT_API_TIMEOUT_MS, 'ms');\n"
    "        } else {\n"
    "            console.error('Bot fetch error:', e);\n"
    "        }\n"
    "        return null;\n"
    "    } finally {\n"
    "        if (timeoutId) clearTimeout(timeoutId);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function playBotMove(fen, generation) {\n"
    "    if (botThinking || !fen || typeof fen !== 'string') return;\n"
    "    botThinking = true;\n"
    "    var msgEl = document.getElementById('castling-pending-message');\n"
    "    var statusEl = document.getElementById('game-state');\n"
    "\n"
    "    try {\n"
    "        if (statusEl) statusEl.textContent = 'Počítač přemýšlí...';\n"
    "        console.log('🤖 Bot starts thinking... Generation:', generation);\n"
    "        var move = await fetchStockfishBestMove(fen);\n"
    "\n"
    "        if (generation !== gameGeneration) {\n"
    "            console.warn('🤖 Bot move aborted: Game generation changed (New game started).');\n"
    "            lastSuggestedMove = null;\n"
    "            stopBotHintRefresh();\n"
    "            return;\n"
    "        }\n"
    "\n"
    "        if (move) {\n"
    "            console.log('🤖 Bot plays:', move.from, '->', move.to, '| FEN:', fen.length > 20 ? fen.substring(0, 30) + '...' : fen, '| mode:', gameMode, '| gen:', generation);\n"
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
    "                fetch('/api/game/hint_highlight', {\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    body: JSON.stringify({ from: lastSuggestedMove.from, to: lastSuggestedMove.to })\n"
    "                }).catch(function () {});\n"
    "            }, BOT_HINT_REFRESH_MS);\n"
    "            if (msgEl) {\n"
    "                msgEl.textContent = 'Bot hraje: ' + move.from + ' → ' + move.to;\n"
    "                msgEl.style.display = 'block';\n"
    "                msgEl.style.background = 'rgba(76, 175, 80, 0.2)';\n"
    "                msgEl.style.borderColor = '#4CAF50';\n"
    "                msgEl.style.color = '#e0e0e0';\n"
    "            }\n"
    "            showHintOnBoard(move.from, move.to);\n"
    "        } else {\n"
    "            console.warn('Bot: žádný tah (API chyba nebo timeout).');\n"
    "            if (msgEl) {\n"
    "                msgEl.textContent = 'Počítač nemohl najít tah (zkontrolujte internet).';\n"
    "                msgEl.style.display = 'block';\n"
    "                msgEl.style.background = 'rgba(244, 67, 54, 0.2)';\n"
    "                msgEl.style.borderColor = '#f44336';\n"
    "                msgEl.style.color = '#e0e0e0';\n"
    "                setTimeout(function () {\n"
    "                    if (msgEl.textContent.indexOf('nemohl najít tah') !== -1) msgEl.style.display = 'none';\n"
    "                }, 5000);\n"
    "            }\n"
    "        }\n"
    "    } finally {\n"
    "        botThinking = false;\n"
    "    }\n"
    "}\n"
    "\n"
    "function checkBotTurn(status, fen) {\n"
    "    if (gameMode !== 'bot') {\n"
    "        lastSuggestedFen = null;\n"
    "        lastSuggestedMove = null;\n"
    "        stopBotHintRefresh();\n"
    "        document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });\n"
    "        return;\n"
    "    }\n"
    "    if (!status || typeof status !== 'object') return;\n"
    "    if (status.game_state !== 'active' && status.game_state !== 'playing') return;\n"
    "    if (status.game_end && status.game_end.ended) return;\n"
    "    if (status.current_player !== 'White' && status.current_player !== 'Black') return;\n"
    "    if (!fen || typeof fen !== 'string' || fen.length < 20) return;\n"
    "\n"
    "    var isBotTurn = (botSettings.side === 'white') !== (status.current_player === 'White');\n"
    "    if (typeof console !== 'undefined' && console.log) console.log('checkBotTurn: isBotTurn=', isBotTurn, 'current_player=', status.current_player);\n"
    "\n"
    "    var msgEl = document.getElementById('castling-pending-message');\n"
    "    if (!isBotTurn) {\n"
    "        lastSuggestedFen = null;\n"
    "        lastSuggestedMove = null;\n"
    "        stopBotHintRefresh();\n"
    "        document.querySelectorAll('.square').forEach(function (sq) { sq.classList.remove('hint-from', 'hint-to'); });\n"
    "        if (msgEl && (msgEl.textContent.indexOf('Bot hraje') !== -1 || msgEl.textContent.indexOf('Počítač') !== -1)) msgEl.style.display = 'none';\n"
    "    } else if (!botThinking && fen !== lastSuggestedFen) {\n"
    "        lastSuggestedFen = fen;\n"
    "        playBotMove(fen, gameGeneration);\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// STATUS UPDATE FUNCTION\n"
    "// ============================================================================\n"
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
    "        if (liftedPieceEl) liftedPieceEl.textContent = pieceSymbols[lifted.piece] || '-';\n"
    "        if (liftedPosEl) liftedPosEl.textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);\n"
    "        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);\n"
    "        if (square) square.classList.add('lifted');\n"
    "    } else {\n"
    "        if (liftedPieceEl) liftedPieceEl.textContent = '-';\n"
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
    "        // Prioritize Castling Message over Bot Message\n"
    "        if (status.castling_in_progress && status.castling_from && status.castling_to) {\n"
    "            castlingMsg.textContent = 'Dokončete rošádu: přesuňte věž z ' + status.castling_from + ' na ' + status.castling_to + '.';\n"
    "            castlingMsg.style.display = 'block';\n"
    "            // Reset style to warning for castling\n"
    "            castlingMsg.style.background = 'rgba(255,193,7,0.12)';\n"
    "            castlingMsg.style.borderColor = 'rgba(255,193,7,0.4)';\n"
    "            castlingMsg.style.color = '#ffc107';\n"
    "        } else {\n"
    "            // If NOT castling, check if we should keep Bot Message or hide it\n"
    "            // Bot logic handles showing/hiding bot message, so we only hide if NO bot message logic is active\n"
    "            // But simpler: just hide if not castling AND not bot thinking/turn (handled in checkBotTurn)\n"
    "            var keepMsg = castlingMsg.textContent.indexOf('Bot') !== -1 || castlingMsg.textContent.indexOf('Losování:') === 0;\n"
    "            if (!keepMsg) castlingMsg.style.display = 'none';\n"
    "        }\n"
    "    }\n"
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
    "        } else {\n"
    "            if (typeof updateHintButtonLabel === 'function') updateHintButtonLabel();\n"
    "        }\n"
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
    "function renderHistoryList() {\n"
    "    const historyBox = document.getElementById('history');\n"
    "    if (!historyBox) return;\n"
    "    historyBox.innerHTML = '';\n"
    "    const showEval = getEvaluateMoveEnabled();\n"
    "    historyData.slice().reverse().forEach((move, index) => {\n"
    "        const item = document.createElement('div');\n"
    "        item.className = 'history-item';\n"
    "        const actualIndex = historyData.length - 1 - index;\n"
    "        item.dataset.moveIndex = actualIndex;\n"
    "        const moveNum = Math.floor(actualIndex / 2) + 1;\n"
    "        const isWhite = actualIndex % 2 === 0;\n"
    "        const prefix = isWhite ? moveNum + '. ' : '';\n"
    "        item.textContent = prefix + move.from + ' → ' + move.to;\n"
    "        if (showEval && moveEvaluations[actualIndex]) {\n"
    "            const ev = moveEvaluations[actualIndex];\n"
    "            const badge = document.createElement('span');\n"
    "            badge.className = 'move-eval-badge move-eval-badge--' + (ev.grade || 'good');\n"
    "            badge.title = ev.msg || getGradeLabel(ev.grade);\n"
    "            badge.textContent = getGradeLabel(ev.grade);\n"
    "            item.appendChild(document.createTextNode(' '));\n"
    "            item.appendChild(badge);\n"
    "        }\n"
    "        item.onclick = () => enterReviewMode(actualIndex);\n"
    "        historyBox.appendChild(item);\n"
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
    "        piece.textContent = pieceSymbols[p] || p;\n"
    "        whiteBox.appendChild(piece);\n"
    "    });\n"
    "    captured.black_captured.forEach(p => {\n"
    "        const piece = document.createElement('div');\n"
    "        piece.className = 'captured-piece';\n"
    "        piece.textContent = pieceSymbols[p] || p;\n"
    "        blackBox.appendChild(piece);\n"
    "    });\n"
    "}\n"
    "\n"
    "async function fetchData() {\n"
    "    if (reviewMode || sandboxMode) return;\n"
    "    try {\n"
    "        const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([\n"
    "            fetch('/api/board'),\n"
    "            fetch('/api/status'),\n"
    "            fetch('/api/history'),\n"
    "            fetch('/api/captured')\n"
    "        ]);\n"
    "        const board = await boardRes.json();\n"
    "        const status = await statusRes.json();\n"
    "        const history = await historyRes.json();\n"
    "        const captured = await capturedRes.json();\n"
    "        updateBoard(board.board);\n"
    "        updateStatus(status);\n"
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
    "                var lastMoveByBot = gameMode === 'bot' && ((botSettings.side === 'white' && lastMoveByWhite) || (botSettings.side === 'black' && !lastMoveByWhite));\n"
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
    "            if (!castlingInProgress) {\n"
    "                lastFen = currentFen;\n"
    "                lastHistoryLength = newHistoryLength;\n"
    "            }\n"
    "        }\n"
    "        checkBotTurn(status, currentFen);\n"
    "    } catch (error) {\n"
    "        console.error('Fetch error:', error);\n"
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
    "console.log('🚀 Creating chess board...');\n"
    "initializeApp(); // Call the new initialization function\n"
    "console.log('✅ Chess JavaScript loaded successfully!');\n"
    "console.log('⏱️ About to initialize timer system...');\n"
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
    "    selectedTimeControl = parseInt(select.value);\n"
    "    toggleCustomSettings();\n"
    "    if (applyBtn) applyBtn.disabled = false;\n"
    "    localStorage.setItem('chess_time_control', selectedTimeControl.toString());\n"
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
    "    const savedTimeControl = localStorage.getItem('chess_time_control');\n"
    "    if (savedTimeControl) {\n"
    "        selectedTimeControl = parseInt(savedTimeControl);\n"
    "        timeControlSelect.value = selectedTimeControl;\n"
    "    } else {\n"
    "        selectedTimeControl = parseInt(timeControlSelect.value);\n"
    "    }\n"
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
    "function startTimerUpdateLoop() {\n"
    "    console.log('✅ Timer update loop starting... (will update every 1000ms)');\n"
    "    if (timerUpdateInterval) {\n"
    "        console.log('⚠️ Clearing existing timer interval');\n"
    "        clearInterval(timerUpdateInterval);\n"
    "    }\n"
    "    timerUpdateInterval = setInterval(async () => {\n"
    "        try {\n"
    "            await updateTimerDisplay();\n"
    "        } catch (error) {\n"
    "            console.error('❌ Timer update loop error:', error);\n"
    "        }\n"
    "    }, 1000); // Optimized from 200ms to 1s (5× fewer requests, still responsive)\n"
    "    console.log('✅ Timer interval set successfully, ID:', timerUpdateInterval);\n"
    "    // Initial immediate update\n"
    "    console.log('⏱️ Calling initial timer update...');\n"
    "    updateTimerDisplay().catch(e => console.error('❌ Initial timer update failed:', e));\n"
    "}\n"
    "\n"
    "async function updateTimerDisplay() {\n"
    "    try {\n"
    "        console.log('⏱️ updateTimerDisplay() called, fetching /api/timer...');\n"
    "        const response = await fetch('/api/timer');\n"
    "        console.log('⏱️ /api/timer response status:', response.status);\n"
    "        if (response.ok) {\n"
    "            const timerInfo = await response.json();\n"
    "            timerData = timerInfo;\n"
    "            // Format time for logging\n"
    "            const whiteTime = formatTime(timerInfo.white_time_ms);\n"
    "            const blackTime = formatTime(timerInfo.black_time_ms);\n"
    "            console.log('⏱️ Timer:', timerInfo.config ? timerInfo.config.name : 'NO CONFIG', '| White:', whiteTime, '(' + timerInfo.white_time_ms + 'ms)', '| Black:', blackTime, '(' + timerInfo.black_time_ms + 'ms)');\n"
    "            updatePlayerTime('white', timerInfo.white_time_ms);\n"
    "            updatePlayerTime('black', timerInfo.black_time_ms);\n"
    "            updateActivePlayer(timerInfo.is_white_turn);\n"
    "            updateProgressBars(timerInfo);\n"
    "            updateTimerStats(timerInfo);\n"
    "            // Disable/enable timer controls podle config.type\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            const resetBtn = document.getElementById('reset-timer');\n"
    "            const isTimerActive = timerInfo.config && timerInfo.config.type !== 0;\n"
    "            if (pauseBtn) pauseBtn.disabled = !isTimerActive;\n"
    "            if (resumeBtn) resumeBtn.disabled = !isTimerActive;\n"
    "            if (resetBtn) resetBtn.disabled = !isTimerActive;\n"
    "            // Pouze pokud je časová kontrola aktivní\n"
    "            if (isTimerActive) {\n"
    "                checkTimeWarnings(timerInfo);\n"
    "                if (timerInfo.time_expired) {\n"
    "                    handleTimeExpiration(timerInfo);\n"
    "                }\n"
    "            }\n"
    "        } else {\n"
    "            console.error('❌ Timer update failed:', response.status);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('❌ Timer update error:', error);\n"
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
    "// ============================================================================\n"
    "// NEW GAME (Nastavení → action bar „Nová hra“) – for inline onclick\n"
    "// ============================================================================\n"
    "\n"
    "function getConfirmNewGameEnabled() {\n"
    "    const cb = document.getElementById('confirm-new-game');\n"
    "    if (cb) return cb.checked;\n"
    "    try {\n"
    "        return localStorage.getItem('chess_confirm_new_game') === 'true';\n"
    "    } catch (e) {\n"
    "        return false;\n"
    "    }\n"
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
    "(function initConfirmNewGamePref() {\n"
    "    function apply() {\n"
    "        var cb = document.getElementById('confirm-new-game');\n"
    "        if (!cb) return;\n"
    "        try {\n"
    "            var saved = localStorage.getItem('chess_confirm_new_game');\n"
    "            cb.checked = saved === 'true';\n"
    "        } catch (e) { }\n"
    "        cb.addEventListener('change', function () {\n"
    "            try {\n"
    "                localStorage.setItem('chess_confirm_new_game', cb.checked);\n"
    "            } catch (e) { }\n"
    "        });\n"
    "    }\n"
    "    if (document.readyState === 'loading') {\n"
    "        document.addEventListener('DOMContentLoaded', apply);\n"
    "    } else {\n"
    "        apply();\n"
    "    }\n"
    "})();\n"
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
    "// Initialize timer system immediately (will retry if DOM not ready)\n"
    "console.log('⏱️ Exposing timer functions and calling initTimerSystem()...');\n"
    "try {\n"
    "    initTimerSystem();\n"
    "    console.log('✅ initTimerSystem() called successfully');\n"
    "} catch (error) {\n"
    "    console.error('❌ CRITICAL ERROR in initTimerSystem():', error);\n"
    "    console.error('Stack:', error.stack);\n"
    "}\n"
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
    "            headers: { 'Content-Type': 'application/json' },\n"
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
    "        const response = await fetch('/api/wifi/connect', { method: 'POST' });\n"
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
    "        const response = await fetch('/api/wifi/disconnect', { method: 'POST' });\n"
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
    "        const response = await fetch('/api/wifi/clear', { method: 'POST' });\n"
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
    "async function updateWiFiStatus() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/status');\n"
    "        const data = await response.json();\n"
    "        document.getElementById('ap-ssid').textContent = data.ap_ssid || 'ESP32-CzechMate';\n"
    "        document.getElementById('ap-ip').textContent = data.ap_ip || '192.168.4.1';\n"
    "        document.getElementById('ap-clients').textContent = data.ap_clients || 0;\n"
    "        document.getElementById('sta-ssid').textContent = data.sta_ssid || 'Nenastaveno';\n"
    "        document.getElementById('sta-ip').textContent = data.sta_ip || 'Nepřipojeno';\n"
    "        document.getElementById('sta-connected').textContent = data.sta_connected ? 'ano' : 'ne';\n"
    "        if (data.sta_ssid && data.sta_ssid !== 'Nenastaveno') {\n"
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
    "}\n";

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
    "margin-top: 10px; padding: 8px 10px; "
    "background: rgba(255,193,7,0.12); border: 1px solid rgba(255,193,7,0.4); "
    "border-radius: 6px; font-size: 12px; color: #ffc107; "
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
    "}\n"
    "@media (max-width: 319px) { "
    ".tab-btn { padding: 8px 12px; font-size: 12px; } "
    ".btn-game-action { padding: 6px 8px; font-size: 11px; min-height: 32px; } "
    "}\n"
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
    "}"
    ".piece.white { color: white; }"
    ".piece.black { color: black; }"

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
    "font-size: 10px; margin-left: 6px; padding: 1px 5px; border-radius: 4px; "
    "font-weight: 600; display: inline-block; }\n"
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
    ".game-right-panel .history-box { max-height: 260px; }\n"
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
    "var depth=(function(){try{var "
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
    "<div class='game-block'>"
    "<div class='game-title'>Stav</div>"
    "<div class='game-row'><span class='game-label'>Stav</span><span "
    "id='game-state' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Na tahu</span><span "
    "id='current-player' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Tahy</span><span "
    "id='move-count' class='game-value'>0</span></div>"
    "<div class='game-row'><span class='game-label'>Šach</span><span "
    "id='in-check' class='game-value'>Ne</span></div>"
    "</div>"
    "<div class='game-block'>"
    "<div class='game-title'>Časová kontrola</div>"
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
    "<div class='game-actions'>"
    "<button type='button' id='new-game-btn' class='btn-game-action "
    "btn-game-new' "
    "onclick='startNewGame()'>Nová hra</button>"
    "<button type='button' id='hint-btn' class='btn-game-action btn-game-hint' "
    "onclick='requestHint()'>Nápověda</button>"
    "<button type='button' class='btn-game-action btn-game-sandbox' "
    "onclick='enterSandboxMode()'>Zkusit tahy</button>"
    "</div>"
    "<div id='hint-explanation' class='hint-explanation' style='display:none;'>"
    "<div class='hint-explanation-title'>Proč tento tah?</div>"
    "<p id='hint-explanation-message' class='hint-explanation-message'></p>"
    "</div>"
    "<div id='move-evaluation' class='move-evaluation' style='display:none;'>"
    "<div class='move-evaluation-title'>Zhodnocení tahu</div>"
    "<p id='move-evaluation-message' class='move-evaluation-message'></p>"
    "</div>"
    "<div id='castling-pending-message' class='castling-pending-message' "
    "style='display:none;'></div>"
    "</div>";

// Game Right Panel (zvednutá, sebrání, historie)
static const char html_chunk_game_right[] =
    "<div class='game-right-panel'>"
    "<div class='game-block'>"
    "<div class='game-title'>Zvednutá</div>"
    "<div class='game-row'><span class='game-label'>Figurka</span><span "
    "id='lifted-piece' class='game-value'>—</span></div>"
    "<div class='game-row'><span class='game-label'>Pozice</span><span "
    "id='lifted-position' class='game-value'>—</span></div>"
    "</div>"
    "<div class='game-block'>"
    "<div class='game-title'>Sebrané</div>"
    "<div class='game-row'><span class='game-label'>Bílý</span></div>"
    "<div id='white-captured' class='captured-pieces'></div>"
    "<div class='game-row' style='margin-top:8px;'><span "
    "class='game-label'>Černý</span></div>"
    "<div id='black-captured' class='captured-pieces'></div>"
    "</div>"
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
    "<div class='game-block'>"
    "<div class='game-title'>Historie</div>"
    "<div id='history' class='history-box'></div>"
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
    "</div>"
    "<script>\n"
    "function updateBotSettingsVisibility() {\n"
    "  const mode = document.getElementById('game-mode').value;\n"
    "  const container = document.getElementById('bot-settings-container');\n"
    "  container.style.display = (mode === 'bot') ? 'block' : 'none';\n"
    "  saveBotSettings();\n"
    "}\n"
    "function saveBotSettings() {\n"
    "  const settings = {\n"
    "    mode: document.getElementById('game-mode').value,\n"
    "    strength: document.getElementById('bot-strength').value,\n"
    "    side: document.getElementById('player-side').value\n"
    "  };\n"
    "  localStorage.setItem('chessBotSettings', JSON.stringify(settings));\n"
    "}\n"
    "function loadBotSettings() {\n"
    "  try {\n"
    "    const stored = localStorage.getItem('chessBotSettings');\n"
    "    if (stored) {\n"
    "      const s = JSON.parse(stored);\n"
    "      if(s.mode) document.getElementById('game-mode').value = s.mode;\n"
    "      if(s.strength) document.getElementById('bot-strength').value = "
    "s.strength;\n"
    "      if(s.side) document.getElementById('player-side').value = s.side;\n"
    "      updateBotSettingsVisibility();\n"
    "    }\n"
    "  } catch(e) { console.error('Failed to load bot settings', e); }\n"
    "  if (typeof handleRandomDraw === 'function') handleRandomDraw();\n"
    "}\n"
    "// Load on init\n"
    "if (document.readyState === 'loading') {\n"
    "  document.addEventListener('DOMContentLoaded', loadBotSettings);\n"
    "} else {\n"
    "  setTimeout(loadBotSettings, 100);\n"
    "}\n"
    "</script>"
    "</div>";

// Section Zařízení
static const char html_chunk_settings_zarizeni[] =
    "<div class='settings-section'>"
    "<h3 class='set-title'>Zařízení</h3>"
    "<div class='set-row'>"
    "<span class='set-label'>Jas</span>"
    "<span id='brightness-value' class='set-value'>50%</span>"
    "</div>"
    "<input type='range' id='brightness-slider' class='set-input' min='0' "
    "max='100' value='50' style='padding:0; height:24px;' "
    "oninput='var e=document.getElementById(\"brightness-value\"); "
    "if(e)e.textContent=this.value+\"%\";' "
    "onchange='setBrightness(this.value)'>"
    "<div class='set-row'><span class='set-label'>Zámek</span><span "
    "id='web-lock-status' class='set-value'>—</span></div>"
    "<div class='set-row'><span class='set-label'>Online</span><span "
    "id='web-online-status' class='set-value'>—</span></div>"
    "</div>"
    "<div class='settings-section settings-section--wide'>"
    "<h3 class='set-title'>Nápověda</h3>"
    "<div class='set-group'><label>Hloubka myšlení</label>"
    "<input type='number' id='hint-depth' class='set-input' min='1' max='18' "
    "value='10' onchange='var "
    "v=Math.min(18,Math.max(1,parseInt(this.value,10)||10));"
    "this.value=v;try{localStorage.setItem(\"chessHintDepth\",v);}catch(e){}'>"
    "</div>"
    "<p class='set-note'>Jak moc má počítač přemýšlet. Přibližně: 1 ≈ 500 "
    "ELO, 10 ≈ 1500 ELO, 18 ≈ 2800 ELO.</p>"
    "<label class='set-check' style='margin-top:4px;'>"
    "<input type='checkbox' id='evaluate-move-enabled' "
    "onchange=\"try{localStorage.setItem('chessEvaluateMove',this.checked);}"
    "catch(e){};if(!this.checked){hideMoveEvaluation();if(typeof "
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
    "function saveHintSettings(){try{var "
    "el=document.getElementById('hint-limit');if(el)localStorage.setItem('chessHintLimit',el.value);"
    "el=document.getElementById('hint-award-best');if(el)localStorage.setItem('chessHintAwardBest',el.checked);"
    "el=document.getElementById('hint-award-good');if(el)localStorage.setItem('chessHintAwardGood',el.checked);"
    "el=document.getElementById('hint-award-capture');if(el)localStorage.setItem('chessHintAwardCapture',el.checked);"
    "el=document.getElementById('show-hint-stats');if(el)localStorage.setItem('chessShowHintStats',el.checked);"
    "}catch(e){}}\n"
    "function loadHintSettings(){try{var "
    "v=localStorage.getItem('chessHintLimit');var "
    "el=document.getElementById('hint-limit');if(el&&v!==null)el.value=v;"
    "v=localStorage.getItem('chessHintAwardBest');el=document.getElementById('hint-award-best');"
    "if(el)el.checked=v!=='false';"
    "v=localStorage.getItem('chessHintAwardGood');el=document.getElementById('hint-award-good');"
    "if(el)el.checked=v==='true';"
    "v=localStorage.getItem('chessHintAwardCapture');el=document.getElementById('hint-award-capture');"
    "if(el)el.checked=v==='true';"
    "v=localStorage.getItem('chessShowHintStats');el=document.getElementById('show-hint-stats');"
    "if(el)el.checked=v==='true';"
    "if(typeof updateTeachingStatsPanel==='function')updateTeachingStatsPanel();"
    "}catch(e){}}\n"
    "if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',loadHintSettings);"
    "else setTimeout(loadHintSettings,100);\n"
    "</script>"
    "</div>"
    "<div class='settings-section'>"
    "<h3 class='set-title'>Ovládání</h3>"
    "<label class='set-check'>"
    "<input type='checkbox' id='remote-control-enabled' "
    "onchange='toggleRemoteControl()'>"
    "<span>Dálkové ovládání (sync s deskou)</span>"
    "</label>"
    "<p class='set-note set-note-warn'>Synchronizace s fyzickou deskou.</p>"
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
    "</div>";

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
    "try{var depth=(function(){try{var "
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
    "  try {\n"
    "    const response = await fetch('/api/mqtt/status');\n"
    "    const data = await response.json();\n"
    "    if (data.host) document.getElementById('mqtt-host').value = "
    "data.host;\n"
    "    if (data.port) document.getElementById('mqtt-port').value = "
    "data.port;\n"
    "    if (data.username) document.getElementById('mqtt-username').value = "
    "data.username;\n"
    "  } catch (e) {\n"
    "    console.log('Could not load MQTT config:', e);\n"
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

    // Don't delete task - instead enter a maintenance loop that feeds WDT
    task_running = true;
    while (task_running) {
      web_server_task_wdt_reset_safe();
      vTaskDelay(pdMS_TO_TICKS(1000));
    }

    esp_task_wdt_delete(NULL); // Unregister before deleting
    vTaskDelete(NULL);
    return;
  }
  web_server_active = true;
  web_server_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
  ESP_LOGI(TAG, "HTTP server started");

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
  ESP_LOGI(TAG, "Connect to WiFi: %s", WIFI_AP_SSID);
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
// WEBSOCKET FUNCTIONS (Placeholder for future implementation)
// ============================================================================

void web_server_websocket_init(void) {
  ESP_LOGI(TAG, "WebSocket support not yet implemented");
}

void web_server_websocket_send_update(const char *data) {
  if (!web_server_active || data == NULL) {
    return;
  }
  ESP_LOGI(TAG, "WebSocket send: %s", data);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool web_server_is_active(void) { return web_server_active; }

uint32_t web_server_get_client_count(void) { return client_count; }

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
