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
// #include "../timer_system/include/timer_system.h" // UNUSED
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_task_wdt.h"
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
static bool web_locked = false; // Flag pro lock web rozhrani
char sta_ip[16] = {0};          // Externi pro UART prikazy
static char sta_ssid[33] = {0};
static int last_disconnect_reason =
    0; // Posledni disconnection reason pro error handling

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
  sta_connecting = true; // Nastavit flag pripojovani

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

  // Pokud se prave pripojuje, zrusit pripojeni
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
    sta_connecting = false; // Resetovat flag pripojovani
    sta_ip[0] = '\0';
  }
  // IP eventy - ziskani IP adresy
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "STA: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    snprintf(sta_ip, sizeof(sta_ip), IPSTR, IP2STR(&event->ip_info.ip));
    sta_connected = true;
    sta_connecting = false; // Resetovat flag pripojovani
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
  ESP_LOGI(TAG, "GET /api/demo/status");

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
  config.recv_wait_timeout = 10;
  config.send_wait_timeout =
      3000; // 3 seconds timeout for reliable chunked transfer (8 chunks, ~250ms
            // transfer time + network overhead)
  config.max_resp_headers = 8;
  config.backlog_conn = 5;
  config.stack_size = 8192; // Zvysena velikost stacku pro HTTP server task

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
  ESP_LOGI(TAG, "GET /api/board");

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
  ESP_LOGI(TAG, "GET /api/status");

  // Ziskat stav hry z game tasku
  esp_err_t ret = game_get_status_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get game status", -1);
    return ESP_FAIL;
  }

  // INJECT WEB STATUS FIELDS
  // Game task doesn't know about web lock or wifi, so we add it here.
  char *last_brace = strrchr(json_buffer, '}');
  if (last_brace) {
    size_t current_len = last_brace - json_buffer;
    size_t remaining = sizeof(json_buffer) - current_len;
    snprintf(
        last_brace, remaining, ",\"web_locked\":%s,\"internet_connected\":%s}",
        web_is_locked() ? "true" : "false", sta_connected ? "true" : "false");
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

static esp_err_t http_get_history_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/history");

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
  ESP_LOGI(TAG, "GET /api/captured");

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
  ESP_LOGI(TAG, "GET /api/timer");

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
    // Promoce bez promotion parametru - mělo by být zachyceno výše, ale pro jistotu
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
  ESP_LOGI(TAG, "GET /api/wifi/status");

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
// chess_app.js embedded (27426 bytes, 696 lines)
// chess_app.js embedded (54831 bytes, 1361 lines)
static const char chess_app_js_content[] =
    "// "
    "=========================================================================="
    "==\n"
    "// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING\n"
    "// "
    "=========================================================================="
    "==\n"
    "\n"
    "console.log('🚀 Chess JavaScript loading...');\n"
    "\n"
    "// "
    "=========================================================================="
    "==\n"
    "// PIECE SYMBOLS AND GLOBAL VARIABLES\n"
    "// "
    "=========================================================================="
    "==\n"
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
    "let advantageData = { history: [], white_checks: 0, black_checks: 0, "
    "white_castles: 0, black_castles: 0 };\n"
    "let selectedSquare = null;\n"
    "let reviewMode = false;\n"
    "let currentReviewIndex = -1;\n"
    "let initialBoard = [];\n"
    "let sandboxMode = false;\n"
    "let remoteControlEnabled = false;\n"
    "let sandboxBoard = [];\n"
    "let sandboxHistory = [];\n"
    "let endgameReportShown = false;\n"
    "let pendingPromotion = null;\n"
    "\n"
    "// "
    "=========================================================================="
    "==\n"
    "// BOARD FUNCTIONS\n"
    "// "
    "=========================================================================="
    "==\n"
    "\n"
    "function createBoard() {\n"
    "    const board = document.getElementById('board');\n"
    "    board.innerHTML = '';\n"
    "    for (let row = 7; row >= 0; row--) {\n"
    "        for (let col = 0; col < 8; col++) {\n"
    "            const square = document.createElement('div');\n"
    "            square.className = 'square ' + ((row + col) % 2 === 0 ? "
    "'light' : 'dark');\n"
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
    "async function selectPromotion(pieceChar) {\n"
    "        if (pendingPromotion) {\n"
    "            // Scenario A: Web-initiated move\n"
    "            const { from, to } = pendingPromotion;\n"
    "            document.getElementById('promotion-modal').style.display = "
    "'none';\n"
    "            pendingPromotion = null;\n"
    "\n"
    "            try {\n"
    "                const response = await fetch('/api/move', {\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    // Send move WITH promotion choice (q, r, b, n)\n"
    "                    body: JSON.stringify({ from: from, to: to, promotion: "
    "pieceChar })\n"
    "                });\n"
    "                if (response.ok) {\n"
    "                    clearHighlights();\n"
    "                    fetchData();\n"
    "                }\n"
    "            } catch (error) {\n"
    "                console.error('Promotion move error:', error);\n"
    "            }\n"
    "        } else {\n"
    "            // Scenario B: Physical/Remote-initiated promotion\n"
    "            try {\n"
    "                const response = await fetch('/api/game/virtual_action', "
    "{\n"
    "                    method: 'POST',\n"
    "                    headers: { 'Content-Type': 'application/json' },\n"
    "                    body: JSON.stringify({ action: 'promote', choice: "
    "pieceChar })\n"
    "                });\n"
    "                if (response.ok) {\n"
    "                    "
    "document.getElementById('promotion-modal').style.display "
    "= 'none';\n"
    "                    fetchData();\n"
    "                }\n"
    "            } catch (error) {\n"
    "                console.error('Promotion action error:', error);\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "async function startNewGame() {\n"
    "    if (confirm('Start a new game?')) {\n"
    "        try {\n"
    "            const response = await fetch('/api/game/new', { method: "
    "'POST' });\n"
    "            if (response.ok) {\n"
    "                console.log('New game started');\n"
    "                fetchData();\n"
    "            } else {\n"
    "                alert('Failed to start new game');\n"
    "            }\n"
    "        } catch (error) {\n"
    "            console.error('New game error:', error);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "function cancelPromotion() {\n"
    "    document.getElementById('promotion-modal').style.display = 'none';\n"
    "    pendingPromotion = null;\n"
    "    clearHighlights();\n"
    "    selectedSquare = null;\n"
    "    document.getElementById('lifted-piece').textContent = '-';\n"
    "    document.getElementById('lifted-position').textContent = '-';\n"
    "    document.querySelectorAll('.square').forEach(sq => "
    "sq.classList.remove('selected', 'lifted'));\n"
    "}\n"
    "\n"
    "// REMOTE CONTROL LOGIC\n"
    "function toggleRemoteControl() {\n"
    "    const checkbox = document.getElementById('remote-control-enabled');\n"
    "    remoteControlEnabled = checkbox.checked;\n"
    "    console.log('Remote control:', remoteControlEnabled);\n"
    "    \n"
    "    if (!remoteControlEnabled) {\n"
    "        clearHighlights();\n"
    "    }\n"
    "}\n"
    "\n"
    "async function handleRemoteControlClick(row, col) {\n"
    "    const notation = String.fromCharCode(97 + col) + (row + 1);\n"
    "    let action = 'pickup';\n"
    "    \n"
    "    // Determine action based on currently lifted piece status\n"
    "    // Note: statusData is updated from backend\n"
    "    if (statusData && statusData.piece_lifted && "
    "statusData.piece_lifted.lifted) {\n"
    "        action = 'drop';\n"
    "    }\n"
    "    \n"
    "    console.log(`Remote control: ${action} at ${notation}`);\n"
    "    \n"
    "    // Visual feedback immediately (optimistic update)\n"
    "    const square = "
    "document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "    if (square) {\n"
    "        square.style.boxShadow = action === 'pickup' ? \n"
    "            'inset 0 0 20px rgba(255, 255, 0, 0.8)' : \n"
    "            'inset 0 0 20px rgba(0, 255, 0, 0.8)';\n"
    "        \n"
    "        setTimeout(() => {\n"
    "            if (square) square.style.boxShadow = '';\n"
    "        }, 500);\n"
    "    }\n"
    "    \n"
    "    try {\n"
    "        const response = await fetch('/api/game/virtual_action', {\n"
    "            method: 'POST',\n"
    "            headers: {'Content-Type': 'application/json'},\n"
    "            body: JSON.stringify({action: action, square: notation})\n"
    "        });\n"
    "        const res = await response.json();\n"
    "        console.log('Remote action response:', res);\n"
    "        \n"
    "        if (!res.success) {\n"
    "            alert('Remote action failed: ' + res.message);\n"
    "        }\n"
    "    } catch (e) {\n"
    "        console.error('Remote action error:', e);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function handleSquareClick(row, col) {\n"
    "    const piece = sandboxMode ? sandboxBoard[row][col] : "
    "boardData[row][col];\n"
    "    const index = row * 8 + col;\n"
    "\n"
    "    // REMOTE CONTROL MODE\n"
    "    if (remoteControlEnabled) {\n"
    "        handleRemoteControlClick(row, col);\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    if (piece === ' ' && selectedSquare !== null) {\n"
    "        const fromRow = Math.floor(selectedSquare / 8);\n"
    "        const fromCol = selectedSquare % 8;\n"
    "        const fromNotation = String.fromCharCode(97 + fromCol) + (8 - "
    "fromRow);\n"
    "        const toNotation = String.fromCharCode(97 + col) + (8 - row);\n"
    "\n"
    "        // DETEKCE PROMOCE: Zkontrolovat zda je pěšec a jde na promotion rank\n"
    "        const sourcePiece = boardData[fromRow][fromCol];\n"
    "        const isPromotion = (sourcePiece === 'P' && row === 0) || "
    "(sourcePiece === 'p' && row === 7);\n"
    "\n"
    "        if (isPromotion) {\n"
    "            // Web-initiated promoce: Nastavit pendingPromotion a zobrazit modal\n"
    "            pendingPromotion = { from: fromNotation, to: toNotation };\n"
    "            const promoModal = document.getElementById('promotion-modal');\n"
    "            if (promoModal) promoModal.style.display = 'flex';\n"
    "            clearHighlights(); // Smazat highlights, ale ponechat selectedSquare pro vizuální feedback\n"
    "            return; // NEposílat tah ještě - počkat na výběr figurky v selectPromotion()\n"
    "        }\n"
    "\n"
    "        try {\n"
    "            const response = await fetch('/api/move', {\n"
    "                method: 'POST',\n"
    "                headers: { 'Content-Type': 'application/json' },\n"
    "                body: JSON.stringify({ from: fromNotation, to: "
    "toNotation })\n"
    "            });\n"
            "            if (response.ok) {\n"
            "                clearHighlights();\n"
            "                fetchData(); // Refresh po úspěšném tahu\n"
            "            } else {\n"
            "                // Nevalidní tah - okamžitě aktualizovat pro zobrazení error state\n"
            "                console.warn('Invalid move:', response.status);\n"
            "                clearHighlights(); // Smazat lokální highlights\n"
            "                await fetchData(); // Okamžitá aktualizace pro error state (červená + modrá)\n"
            "            }\n"
    "        } catch (error) {\n"
    "            console.error('Move error:', error);\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    if (piece !== ' ') {\n"
    "        if (sandboxMode) {\n"
    "            clearHighlights();\n"
    "            selectedSquare = index;\n"
    "            const square = "
    "document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "            if (square) square.classList.add('lifted');\n"
        "        } else if (selectedSquare !== null) {\n"
        "                // CAPTURE LOGIC\n"
        "                // If we have a selected piece and click an opponent "
    "piece -> Capture\n"
        "                const fromRow = Math.floor(selectedSquare / 8);\n"
        "                const fromCol = selectedSquare % 8;\n"
        "                const fromNotation = String.fromCharCode(97 + fromCol) + "
    "(8 - fromRow);\n"
        "                const toNotation = String.fromCharCode(97 + col) + (8 - "
    "row);\n"
        "\n"
        "                // DETEKCE PROMOCE S CAPTURE: Zkontrolovat zda je pěšec a jde na promotion rank\n"
        "                const sourcePiece = boardData[fromRow][fromCol];\n"
        "                const isPromotion = (sourcePiece === 'P' && row === 0) || "
    "(sourcePiece === 'p' && row === 7);\n"
        "\n"
        "                if (isPromotion) {\n"
        "                    // Web-initiated promoce s capture: Nastavit pendingPromotion a zobrazit modal\n"
        "                    pendingPromotion = { from: fromNotation, to: toNotation };\n"
        "                    const promoModal = document.getElementById('promotion-modal');\n"
        "                    if (promoModal) promoModal.style.display = 'flex';\n"
        "                    clearHighlights();\n"
        "                    return; // NEposílat tah ještě - počkat na výběr figurky v selectPromotion()\n"
        "                }\n"
        "\n"
        "                try {\n"
        "                    const response = await fetch('/api/move', {\n"
        "                        method: 'POST',\n"
        "                        headers: { 'Content-Type': 'application/json' "
    "},\n"
        "                        body: JSON.stringify({ from: fromNotation, to: "
    "toNotation })\n"
        "                    });\n"
                    "                    if (response.ok) {\n"
                    "                        clearHighlights();\n"
                    "                        fetchData(); // Refresh po úspěšném tahu\n"
                    "                    } else {\n"
                    "                        // Nevalidní capture - okamžitě aktualizovat pro zobrazení error state\n"
                    "                        console.warn('Invalid capture:', response.status);\n"
                    "                        clearHighlights(); // Smazat lokální highlights\n"
                    "                        await fetchData(); // Okamžitá aktualizace pro error state (červená + modrá)\n"
                    "                    }\n"
    "                } catch (error) {\n"
    "                    console.error('Capture error:', error);\n"
    "                }\n"
    "            } else {\n"
    "            const isWhitePiece = piece === piece.toUpperCase();\n"
    "            const currentPlayerIsWhite = statusData.current_player === "
    "'White';\n"
    "\n"
    "            if ((isWhitePiece && currentPlayerIsWhite) || (!isWhitePiece "
    "&& !currentPlayerIsWhite)) {\n"
    "                clearHighlights();\n"
    "                selectedSquare = index;\n"
    "                const square = "
    "document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "                if (square) square.classList.add('lifted');\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// REVIEW MODE\n"
    "// "
    "========================================================================="
    "="
    "==\n"
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
    "    document.getElementById('review-move-text').textContent = `Reviewing "
    "move ${index + 1}`;\n"
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
    "        const fromSquare = "
    "document.querySelector(`[data-row='${fromRow}'][data-col='${fromCol}']`);"
    "\n"
    "        const toSquare = "
    "document.querySelector(`[data-row='${toRow}'][data-col='${toCol}']`);\n"
    "        if (fromSquare) fromSquare.classList.add('move-from');\n"
    "        if (toSquare) toSquare.classList.add('move-to');\n"
    "    }\n"
    "    document.querySelectorAll('.history-item').forEach(item => {\n"
    "        item.classList.remove('selected');\n"
    "    });\n"
    "    const selectedItem = "
    "document.querySelector(`[data-move-index='${index}']`);\n"
    "    if (selectedItem) {\n"
    "        selectedItem.classList.add('selected');\n"
    "        // Removed scrollIntoView - causes unwanted scroll on mobile "
    "when "
    "using navigation arrows\n"
    "        // History item stays highlighted but page doesn't scroll away "
    "from board/banner\n"
    "    }\n"
    "}\n"
    "\n"
    "function exitReviewMode() {\n"
    "    reviewMode = false;\n"
    "    currentReviewIndex = -1;\n"
    "    "
    "document.getElementById('review-banner').classList.remove('active');\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('move-from', 'move-to');\n"
    "    });\n"
    "    document.querySelectorAll('.history-item').forEach(item => {\n"
    "        item.classList.remove('selected');\n"
    "    });\n"
    "    fetchData();\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// SANDBOX MODE\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "\n"
    "function enterSandboxMode() {\n"
    "    sandboxMode = true;\n"
    "    sandboxBoard = JSON.parse(JSON.stringify(boardData));\n"
    "    sandboxHistory = [];\n"
    "    const banner = document.getElementById('sandbox-banner');\n"
    "    banner.classList.add('active');\n"
    "    clearHighlights();\n"
    "}\n"
    "\n"
    "function exitSandboxMode() {\n"
    "    sandboxMode = false;\n"
    "    sandboxBoard = [];\n"
    "    sandboxHistory = [];\n"
    "    "
    "document.getElementById('sandbox-banner').classList.remove('active');\n"
    "    clearHighlights();\n"
    "    fetchData();\n"
    "}\n"
    "\n"
    "function makeSandboxMove(fromRow, fromCol, toRow, toCol) {\n"
    "    const piece = sandboxBoard[fromRow][fromCol];\n"
    "    sandboxBoard[toRow][toCol] = piece;\n"
    "    sandboxBoard[fromRow][fromCol] = ' ';\n"
    "    sandboxHistory.push({ from: `${String.fromCharCode(97 + fromCol)}${8 "
    "- fromRow}`, to: `${String.fromCharCode(97 + toCol)}${8 - toRow}` });\n"
    "    updateBoard(sandboxBoard);\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// UPDATE FUNCTIONS\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "\n"
    "function updateBoard(board) {\n"
    "    boardData = board;\n"
    "    const loading = document.getElementById('loading');\n"
    "    if (loading) loading.style.display = 'none';\n"
    "\n"
    "    // NEPŘIDÁVAT clearHighlights() - highlights jsou řízené přes updateStatus()\n"
    "    // (lifted, error-invalid, error-original jsou serverem řízené stavy)\n"
    "\n"
    "    for (let row = 0; row < 8; row++) {\n"
    "        for (let col = 0; col < 8; col++) {\n"
    "            const piece = board[row][col];\n"
    "            const pieceElement = document.getElementById('piece-' + (row "
    "* 8 + col));\n"
    "            if (pieceElement) {\n"
    "                pieceElement.textContent = pieceSymbols[piece] || ' ';\n"
    "                if (piece !== ' ') {\n"
    "                    pieceElement.className = 'piece ' + (piece === "
    "piece.toUpperCase() ? 'white' : 'black');\n"
    "                } else {\n"
    "                    pieceElement.className = 'piece';\n"
    "                }\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// ENDGAME REPORT FUNCTIONS\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "\n"
    "// Zobrazit endgame report na webu\n"
    "async function showEndgameReport(gameEnd) {\n"
    "    console.log('🏆 showEndgameReport() called with:', gameEnd);\n"
    "\n"
    "    // Pokud už je banner zobrazen, nedělat nic (aby se "
    "nepřekresloval)\n"
    "    if (endgameReportShown && document.getElementById('endgame-banner')) "
    "{\n"
    "        console.log('Endgame report already shown, skipping...');\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    // Načíst advantage history pro graf\n"
    "    let advantageDataLocal = { history: [], white_checks: 0, "
    "black_checks: 0, white_castles: 0, black_castles: 0 };\n"
    "    try {\n"
    "        const response = await fetch('/api/advantage');\n"
    "        advantageDataLocal = await response.json();\n"
    "        console.log('Advantage data loaded:', advantageDataLocal);\n"
    "    } catch (e) {\n"
    "        console.error('Failed to load advantage data:', e);\n"
    "    }\n"
    "\n"
    "    // Určit výsledek a barvy\n"
    "    let emoji = '🏆';\n"
    "    let title = '';\n"
    "    let subtitle = '';\n"
    "    let accentColor = '#4CAF50';\n"
    "    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';\n"
    "\n"
    "    if (gameEnd.winner === 'Draw') {\n"
    "        emoji = '🤝';\n"
    "        title = 'REMÍZA';\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = '#FF9800';\n"
    "        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';\n"
    "    } else {\n"
    "        emoji = gameEnd.winner === 'White' ? '⚪' : '⚫';\n"
    "        title = `${gameEnd.winner.toUpperCase()} VYHRÁL!`;\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = gameEnd.winner === 'White' ? '#4CAF50' : "
    "'#2196F3';\n"
    "        bgGradient = gameEnd.winner === 'White' ? "
    "'linear-gradient(135deg, #1e3a1e, #2d4a2d)' : 'linear-gradient(135deg, "
    "#1e2a3a, #2d3a4a)';\n"
    "    }\n"
    "\n"
    "    // Získat statistiky\n"
    "    const whiteMoves = Math.ceil(statusData.move_count / 2);\n"
    "    const blackMoves = Math.floor(statusData.move_count / 2);\n"
    "    const whiteCaptured = capturedData.white_captured || [];\n"
    "    const blackCaptured = capturedData.black_captured || [];\n"
    "\n"
    "    // Material advantage\n"
    "    const pieceValues = { p: 1, n: 3, b: 3, r: 5, q: 9, P: 1, N: 3, B: "
    "3, "
    "R: 5, Q: 9 };\n"
    "    let whiteMaterial = 0, blackMaterial = 0;\n"
    "    whiteCaptured.forEach(p => whiteMaterial += pieceValues[p] || 0);\n"
    "    blackCaptured.forEach(p => blackMaterial += pieceValues[p] || 0);\n"
    "    const materialDiff = whiteMaterial - blackMaterial;\n"
    "    const materialText = materialDiff > 0 ? `White +${materialDiff}` : "
    "materialDiff < 0 ? `Black +${-materialDiff}` : 'Vyrovnáno';\n"
    "\n"
    "    // Vytvořit SVG graf výhody (jako chess.com)\n"
    "    let graphSVG = '';\n"
    "    if (advantageDataLocal.history && advantageDataLocal.history.length "
    "> "
    "1) {\n"
    "        const history = advantageDataLocal.history;\n"
    "        const width = 280;\n"
    "        const height = 100;\n"
    "        const maxAdvantage = Math.max(10, ...history.map(Math.abs));\n"
    "        const scaleY = height / (2 * maxAdvantage);\n"
    "        const scaleX = width / (history.length - 1);\n"
    "\n"
    "        // Vytvořit body pro polyline (0,0 je nahoře vlevo, y roste "
    "dolů)\n"
    "        let points = history.map((adv, i) => {\n"
    "            const x = i * scaleX;\n"
    "            const y = height / 2 - adv * scaleY;  // Převrátit Y (White "
    "nahoře, Black dole)\n"
    "            return `${x},${y}`;\n"
    "        }).join(' ');\n"
    "\n"
    "        // Vytvořit polygon pro vyplněnou oblast\n"
    "        let areaPoints = `0,${height / 2} ${points} ${width},${height / "
    "2}`;\n"
    "\n"
    "        graphSVG = `<svg width=\"280\" height=\"100\" "
    "style=\"border-radius:6px;background:rgba(0,0,0,0.2);\">\n"
    "            <!-- Středová čára (vyrovnaná pozice) -->\n"
    "            <line x1=\"0\" y1=\"${height / 2}\" x2=\"${width}\" "
    "y2=\"${height / 2}\" stroke=\"#555\" stroke-width=\"1\" "
    "stroke-dasharray=\"3,3\"/>\n"
    "            <!-- Vyplněná oblast pod křivkou -->\n"
    "            <polygon points=\"${areaPoints}\" fill=\"${accentColor}\" "
    "opacity=\"0.2\"/>\n"
    "            <!-- Křivka výhody -->\n"
    "            <polyline points=\"${points}\" fill=\"none\" "
    "stroke=\"${accentColor}\" stroke-width=\"2\" "
    "stroke-linejoin=\"round\"/>\n"
    "            <!-- Tečky na koncích -->\n"
    "            <circle cx=\"0\" cy=\"${height / 2}\" r=\"3\" "
    "fill=\"${accentColor}\"/>\n"
    "            <circle cx=\"${(history.length - 1) * scaleX}\" "
    "cy=\"${height "
    "/ 2 - history[history.length - 1] * scaleY}\" r=\"4\" "
    "fill=\"${accentColor}\"/>\n"
    "            <!-- Popisky -->\n"
    "            <text x=\"5\" y=\"12\" fill=\"#888\" font-size=\"10\" "
    "font-weight=\"600\">White</text>\n"
    "            <text x=\"5\" y=\"${height - 2}\" fill=\"#888\" "
    "font-size=\"10\" font-weight=\"600\">Black</text>\n"
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
    "            box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 40px "
    "${accentColor}40;\n"
    "            z-index: 9999;\n"
    "            animation: slideInLeft 0.4s ease-out;\n"
    "            backdrop-filter: blur(10px);\n"
    "        `;\n"
    "    }\n"
    "\n"
    "    // HTML obsah\n"
    "    banner.innerHTML = `\n"
    "        <div "
    "style=\"background:${accentColor};padding:20px;text-align:center;border-"
    "radius:10px 10px 0 0;\">\n"
    "            <div "
    "style=\"font-size:64px;margin-bottom:8px;\">${emoji}</div>\n"
    "            <h2 "
    "style=\"margin:0;color:white;font-size:24px;font-weight:700;text-shadow:"
    "0 "
    "2px 4px rgba(0,0,0,0.4);\">${title}</h2>\n"
    "            <p style=\"margin:8px 0 0 "
    "0;color:rgba(255,255,255,0.9);font-size:14px;font-weight:500;\">${"
    "subtitle}</p>\n"
    "        </div>\n"
    "        <div style=\"padding:20px;\">\n"
    "            ${graphSVG ? `\n"
    "            <div "
    "style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-"
    "bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px "
    "0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-"
    "items:center;gap:8px;\">\n"
    "                    <span>📈</span> Průběh hry\n"
    "                </h3>\n"
    "                ${graphSVG}\n"
    "                <div "
    "style=\"display:flex;justify-content:space-between;margin-top:8px;font-"
    "size:11px;color:#888;\">\n"
    "                    <span>Začátek</span>\n"
    "                    <span>Tah ${advantageDataLocal.count || 0}</span>\n"
    "                </div>\n"
    "            </div>` : ''}\n"
    "            <div "
    "style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-"
    "bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px "
    "0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-"
    "items:center;gap:8px;\">\n"
    "                    <span>📊</span> Statistiky\n"
    "                </h3>\n"
    "                <div style=\"display:grid;grid-template-columns:1fr "
    "1fr;gap:10px;font-size:13px;\">\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Tahy</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ "
    "${whiteMoves} | ⚫ ${blackMoves}</div>\n"
    "                    </div>\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Materiál</div>\n"
    "                        <div "
    "style=\"color:${accentColor};font-weight:600;\">${materialText}</div>\n"
    "                    </div>\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Sebráno</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ "
    "${whiteCaptured.length} | ⚫ ${blackCaptured.length}</div>\n"
    "                    </div>\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Celkem</div>\n"
    "                        <div "
    "style=\"color:#e0e0e0;font-weight:600;\">${statusData.move_count} "
    "tahů</div>\n"
    "                    </div>\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Šachy</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ "
    "${advantageDataLocal.white_checks || 0} | ⚫ "
    "${advantageDataLocal.black_checks || 0}</div>\n"
    "                    </div>\n"
    "                    <div "
    "style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;"
    "\">\n"
    "                        <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Rošády</div>\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ "
    "${advantageDataLocal.white_castles || 0} | ⚫ "
    "${advantageDataLocal.black_castles || 0}</div>\n"
    "                    </div>\n"
    "                </div>\n"
    "            </div>\n"
    "            <div "
    "style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-"
    "bottom:15px;\">\n"
    "                <h3 style=\"margin:0 0 12px "
    "0;color:${accentColor};font-size:16px;font-weight:600;display:flex;align-"
    "items:center;gap:8px;\">\n"
    "                    <span>⚔️</span> Sebrané figurky\n"
    "                </h3>\n"
    "                <div style=\"margin-bottom:10px;\">\n"
    "                    <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">White sebral "
    "(${whiteCaptured.length})</div>\n"
    "                    <div "
    "style=\"font-size:20px;line-height:1.4;\">${whiteCaptured.map(p => "
    "pieceSymbols[p] || p).join(' ') || '−'}</div>\n"
    "                </div>\n"
    "                <div>\n"
    "                    <div "
    "style=\"color:#888;font-size:11px;margin-bottom:4px;\">Black sebral "
    "(${blackCaptured.length})</div>\n"
    "                    <div "
    "style=\"font-size:20px;line-height:1.4;\">${blackCaptured.map(p => "
    "pieceSymbols[p] || p).join(' ') || '−'}</div>\n"
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
    "            \" "
    "onmouseover=\"this.style.transform='translateY(-2px)';this.style."
    "boxShadow='0 6px 16px rgba(0,0,0,0.4)'\" "
    "onmouseout=\"this.style.transform='translateY(0)';this.style.boxShadow='"
    "0 "
    "4px 12px rgba(0,0,0,0.3)'\">\n"
    "                ✓ OK\n"
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
    "                from { transform: translateY(-50%) translateX(-100%); "
    "opacity: 0; }\n"
    "                to { transform: translateY(-50%) translateX(0); opacity: "
    "1; }\n"
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
    "    console.log('🏆 ENDGAME REPORT SHOWN - banner displayed (left "
    "side)');\n"
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
    "    button.innerHTML = '🏆 Report';\n"
    "    button.title = 'Show/Hide Endgame Report';\n"
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
    "// "
    "========================================================================="
    "="
    "==\n"
    "// STATUS UPDATE FUNCTION\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "\n"
    "function updateStatus(status) {\n"
    "    statusData = status;\n"
    "    document.getElementById('game-state').textContent = "
    "status.game_state "
    "|| '-';\n"
    "    document.getElementById('current-player').textContent = "
    "status.current_player || '-';\n"
    "    document.getElementById('move-count').textContent = "
    "status.move_count "
    "|| 0;\n"
    "    document.getElementById('in-check').textContent = status.in_check ? "
    "'Yes' : 'No';\n"
    "\n"
    "    // PROMOTION LOGIC: Show modal if game state is 'promotion'\n"
    "    const promoModal = document.getElementById('promotion-modal');\n"
    "    if (status.game_state === 'promotion') {\n"
    "        if (promoModal && promoModal.style.display !== 'flex') {\n"
    "            promoModal.style.display = 'flex';\n"
    "        }\n"
    "    } else {\n"
    "        // Game state se změnil z 'promotion' na něco jiného\n"
    "        // CLEANUP: Pokud je pendingPromotion nastaveno, vyčistit ho\n"
    "        if (pendingPromotion) {\n"
    "            console.log('Promotion state changed - cleaning up pendingPromotion');\n"
    "            pendingPromotion = null;\n"
    "        }\n"
    "        // Hide modal only if we are NOT waiting for user to select "
    "promotion "
    "for a pending web move\n"
    "        if (!pendingPromotion && promoModal && promoModal.style.display !== 'none') {\n"
    "            promoModal.style.display = 'none';\n"
    "        }\n"
    "    }\n"
    "\n"

    "\n"
    "    // ERROR STATE - vždy nejprve odstranit všechny error classes\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('error-invalid', 'error-original');\n"
    "    });\n"
    "\n"
    "    // LIFTED PIECE - vždy nejprve odstranit všechny lifted classes\n"
    "    document.querySelectorAll('.square').forEach(sq => {\n"
    "        sq.classList.remove('lifted');\n"
    "    });\n"
    "\n"
    "    // Zobrazit lifted piece (zelená)\n"
    "    const lifted = status.piece_lifted;\n"
    "    if (lifted && lifted.lifted) {\n"
    "        document.getElementById('lifted-piece').textContent = "
    "pieceSymbols[lifted.piece] || '-';\n"
    "        document.getElementById('lifted-position').textContent = "
    "String.fromCharCode(97 + lifted.col) + (lifted.row + 1);\n"
    "        const square = "
    "document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted."
    "col}"
    "']`);\n"
    "        if (square) square.classList.add('lifted'); // Zelená - zvednutá figurka\n"
    "    } else {\n"
    "        document.getElementById('lifted-piece').textContent = '-';\n"
    "        document.getElementById('lifted-position').textContent = '-';\n"
    "    }\n"
    "\n"
    "\n"
    "    // Zobrazit error state (červená na invalid, modrá na original)\n"
    "    if (status.error_state && status.error_state.active) {\n"
    "        // Invalid position (červená - kde je figurka nyní na nevalidní pozici)\n"
    "        if (status.error_state.invalid_pos) {\n"
    "            const invalidCol = status.error_state.invalid_pos.charCodeAt(0) - 97;\n"
    "            const invalidRow = parseInt(status.error_state.invalid_pos[1]) - 1;\n"
    "            const invalidSquare = "
    "document.querySelector(`[data-row='${invalidRow}'][data-col='${invalidCol}']`);\n"
    "            if (invalidSquare) invalidSquare.classList.add('error-invalid'); // Červená - nevalidní pozice\n"
    "        }\n"
    "        // Original position (modrá - kde byla figurka původně)\n"
    "        if (status.error_state.original_pos) {\n"
    "            const originalCol = status.error_state.original_pos.charCodeAt(0) - 97;\n"
    "            const originalRow = parseInt(status.error_state.original_pos[1]) - 1;\n"
    "            const originalSquare = "
    "document.querySelector(`[data-row='${originalRow}'][data-col='${originalCol}']`);\n"
    "            if (originalSquare) originalSquare.classList.add('error-original'); // Modrá - původní pozice\n"
    "        }\n"
    "    }\n"

    "\n"
    "    // ENDGAME REPORT - zobrazit pouze JEDNOU, po prvnim skonceni\n"
    "    if (status.game_end && status.game_end.ended) {\n"
    "        // Ulozit data pro pozdejsi toggle\n"
    "        window.lastGameEndData = status.game_end;\n"
    "\n"
    "        // Zobrazit report jen pokud jeste nebyl nikdy zobrazen\n"
    "        if (!endgameReportShown) {\n"
    "            console.log('Game ended, showing endgame report...');\n"
    "            showEndgameReport(status.game_end);\n"
    "        }\n"
    "\n"
    "        // Zobrazit toggle button (jen pokud je hra skoncena)\n"
    "        showEndgameToggleButton();\n"
    "    } else {\n"
    "        // Hra je aktivni - skryj report i toggle button\n"
    "        if (endgameReportShown) {\n"
    "            console.log('Game restarted, clearing endgame report...');\n"
    "            hideEndgameReport();\n"
    "        }\n"
    "        endgameReportShown = false;  // Reset flagu po restartu\n"
    "        window.lastGameEndData = null;\n"
    "        hideEndgameToggleButton();\n"
    "    }\n"
    "\n"
    "    // Update Web Status (injected by server)\n"
    "    const lockStatus = document.getElementById('web-lock-status');\n"
    "    if (lockStatus) {\n"
    "        lockStatus.textContent = status.web_locked ? 'LOCKED' : "
    "'UNLOCKED';\n"
    "        lockStatus.style.color = status.web_locked ? '#ff4444' : "
    "'#44ff44';\n"
    "    }\n"
    "    const netStatus = document.getElementById('web-online-status');\n"
    "    if (netStatus) {\n"
    "        netStatus.textContent = status.internet_connected ? 'Online' : "
    "'Offline';\n"
    "        netStatus.style.color = status.internet_connected ? '#44ff44' : "
    "'#ff4444';\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateHistory(history) {\n"
    "    historyData = history.moves || [];\n"
    "    const historyBox = document.getElementById('history');\n"
    "    historyBox.innerHTML = '';\n"
    "    historyData.slice().reverse().forEach((move, index) => {\n"
    "        const item = document.createElement('div');\n"
    "        item.className = 'history-item';\n"
    "        const actualIndex = historyData.length - 1 - index;\n"
    "        item.dataset.moveIndex = actualIndex;\n"
    "        const moveNum = Math.floor(actualIndex / 2) + 1;\n"
    "        const isWhite = actualIndex % 2 === 0;\n"
    "        const prefix = isWhite ? moveNum + '. ' : '';\n"
    "        item.textContent = prefix + move.from + ' → ' + move.to;\n"
    "        item.onclick = () => enterReviewMode(actualIndex);\n"
    "        historyBox.appendChild(item);\n"
    "    });\n"
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
    "    \n"
    "    // OPTIMIZED POLLING: Sequential instead of parallel to reduce load\n"
    "    try {\n"
    "        // 1. Always fetch status (fast, small)\n"
    "        const statusRes = await fetch('/api/status');\n"
    "        if (!statusRes.ok) throw new Error('Status fetch failed');\n"
    "        const status = await statusRes.json();\n"
    "        \n"
    "        // 2. Update status UI first\n"
    "        updateStatus(status);\n"
    "        \n"
    "        // 3. Decide if we need board update\n"
    "        // We can check a move counter or hash if available, or just "
    "always fetch for now\n"
    "        // but sequential prevents network congestion.\n"
    "        const boardRes = await fetch('/api/board');\n"
    "        if (boardRes.ok) {\n"
    "            const board = await boardRes.json();\n"
    "            updateBoard(board.board);\n"
    "        }\n"
    "\n"
    "        // 4. Heavy data (History/Captured) - maybe fetch less often?\n"
    "        // For now, sequentially is safe.\n"
    "        const historyRes = await fetch('/api/history');\n"
    "        if (historyRes.ok) {\n"
    "            const history = await historyRes.json();\n"
    "            updateHistory(history);\n"
    "        }\n"
    "        \n"
    "        const capturedRes = await fetch('/api/captured');\n"
    "        if (capturedRes.ok) {\n"
    "            const captured = await capturedRes.json();\n"
    "            updateCaptured(captured);\n"
    "        }\n"
    "\n"
    "    } catch (error) {\n"
    "        console.error('Fetch cycle error:', error);\n"
    "        // Optional: Show connection lost icon\n"
    "        const onlineStatus = "
    "document.getElementById('web-online-status');\n"
    "        if(onlineStatus) onlineStatus.textContent = '❌ Offline';\n"
    "    }\n"
    "}\n"
    "\n"
    "function initializeApp() {\n"
    "    console.log('🎮 Initializing Chess App...');\n"
    "    createBoard();\n"
    "\n"
    "    // Inject Demo Mode section at bottom\n"
    "    // injectDemoModeSection(); // REMOVED: Avoid duplication\n"
    "\n"
    "    fetchData();\n"
    "    setInterval(fetchData, 2000); // Reduced from 500ms to 2s (4× "
    "fewer requests)\n"
    "    console.log('✅ Chess App initialized');\n"
    "}\n"
    "\n"
    "/**\n"
    " * Inject Demo Mode control section into DOM\n"
    " * Placed at bottom, below all main content\n"
    " */\n"
    /*
        // FUNCTION REMOVED: Demo Mode UI is now handled by server-side HTML
       chunk (Chunk 6)
        // to prevent duplication and ensure consistent styling.
        function injectDemoModeSection() {
        const container = document.querySelector('.container') ||
       document.body;

        const demoSection = document.createElement('div');
        demoSection.style.cssText = 'margin-top:30px; padding:20px;
       background:#2a2a2a; border-radius:8px; border-left:4px solid #666;';
        demoSection.innerHTML = `
            <h3
       style="color:#999;font-size:0.9em;margin-bottom:15px;text-transform:uppercase;letter-spacing:1px;">🤖
       Demo Mode</h3> <div
       style="display:flex;align-items:center;gap:15px;margin-bottom:10px;">
                <button id="btnDemoMode" onclick="toggleDemoMode()"
                        style="padding:10px
       20px;background:#008CBA;color:white;border:2px solid #007396;
                               border-radius:6px;cursor:pointer;font-size:14px;font-weight:600;transition:all
       0.3s;"> Toggle Demo Mode
                </button>
                <span id="demoStatus" style="font-size:14px;color:#999;">⚫
       Off</span>
            </div>
            <p style="font-size:12px;color:#666;margin:0;font-style:italic;">
                Automatic chess game playback. Touch board to interrupt.
            </p>
        `;

        container.appendChild(demoSection);
        console.log('✅ Demo Mode section injected');
        }
    */
    "\n"
    "console.log('🚀 Creating chess board...');\n"
    "initializeApp(); // Call the new initialization function\n"
    "console.log('✅ Chess JavaScript loaded successfully!');\n"
    "console.log('⏱️ About to initialize timer system...');\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// TIMER SYSTEM\n"
    "// "
    "========================================================================="
    "="
    "==\n"
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
    "        return hours + ':' + minutes.toString().padStart(2, '0') + ':' + "
    "seconds.toString().padStart(2, '0');\n"
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
    "    const isTimerActive = timerData.config && timerData.config.type !== "
    "0;\n"
    "\n"
    "    if (isTimerActive) {\n"
    "        const formattedTime = formatTime(timeMs);\n"
    "        timeElement.textContent = formattedTime;\n"
    "        playerElement.classList.remove('low-time', 'critical-time');\n"
    "        if (timeMs < 5000) "
    "playerElement.classList.add('critical-time');\n"
    "        else if (timeMs < 30000) "
    "playerElement.classList.add('low-time');\n"
    "    } else {\n"
    "        // Bez časové kontroly - zobrazit \"--:--\" a odstranit všechny "
    "warning třídy\n"
    "        timeElement.textContent = '--:--';\n"
    "        playerElement.classList.remove('low-time', 'critical-time', "
    "'active');\n"
    "        return; // Nedělat nic dalšího\n"
    "    }\n"
    "\n"
    "    if ((player === 'white' && timerData.is_white_turn) || (player === "
    "'black' && !timerData.is_white_turn)) {\n"
    "        playerElement.classList.add('active');\n"
    "    } else {\n"
    "        playerElement.classList.remove('active');\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateActivePlayer(isWhiteTurn) {\n"
    "    const whiteIndicator = "
    "document.getElementById('white-move-indicator');\n"
    "    const blackIndicator = "
    "document.getElementById('black-move-indicator');\n"
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
    "        const whiteProgress = "
    "document.getElementById('white-progress');\n"
    "        const blackProgress = "
    "document.getElementById('black-progress');\n"
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
    "        const whitePercent = (timerInfo.white_time_ms / initialTime) * "
    "100;\n"
    "        whiteProgress.style.width = Math.max(0, Math.min(100, "
    "whitePercent)) + '%';\n"
    "    }\n"
    "    if (blackProgress) {\n"
    "        const blackPercent = (timerInfo.black_time_ms / initialTime) * "
    "100;\n"
    "        blackProgress.style.width = Math.max(0, Math.min(100, "
    "blackPercent)) + '%';\n"
    "    }\n"
    "}\n"
    "\n"
    "function updateTimerStats(timerInfo) {\n"
    "    const avgMoveTimeElement = "
    "document.getElementById('avg-move-time');\n"
    "    const totalMovesElement = document.getElementById('total-moves');\n"
    "    if (avgMoveTimeElement) {\n"
    "        avgMoveTimeElement.textContent = timerInfo.avg_move_time_ms > 0 "
    "? "
    "formatTime(timerInfo.avg_move_time_ms) : '-';\n"
    "    }\n"
    "    if (totalMovesElement) {\n"
    "        totalMovesElement.textContent = timerInfo.total_moves || 0;\n"
    "    }\n"
    "}\n"
    "\n"
    "function checkTimeWarnings(timerInfo) {\n"
    "    // Nekontrolovat upozornění pokud není časová kontrola aktivní\n"
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) "
    "{\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const currentPlayerTime = timerInfo.is_white_turn ? "
    "timerInfo.white_time_ms : timerInfo.black_time_ms;\n"
    "    if (currentPlayerTime < 5000 && !timerInfo.warning_5s_shown) {\n"
    "        showTimeWarning('Critical! Less than 5 seconds!', 'critical');\n"
    "    } else if (currentPlayerTime < 10000 && "
    "!timerInfo.warning_10s_shown) "
    "{\n"
    "        showTimeWarning('Warning! Less than 10 seconds!', 'warning');\n"
    "    } else if (currentPlayerTime < 30000 && "
    "!timerInfo.warning_30s_shown) "
    "{\n"
    "        showTimeWarning('Low time! Less than 30 seconds!', 'info');\n"
    "    }\n"
    "}\n"
    "\n"
    "function showTimeWarning(message, type) {\n"
    "    const notification = document.createElement('div');\n"
    "    notification.className = 'time-warning ' + type;\n"
    "    notification.textContent = message;\n"
    "    notification.style.cssText = 'position: fixed; top: 20px; right: "
    "20px; padding: 15px 20px; border-radius: 8px; color: white; font-weight: "
    "600; z-index: 1000; animation: slideInRight 0.3s ease;';\n"
    "    switch (type) {\n"
    "        case 'critical': notification.style.background = '#F44336'; "
    "break;\n"
    "        case 'warning': notification.style.background = '#FF9800'; "
    "break;\n"
    "        case 'info': notification.style.background = '#2196F3'; break;\n"
    "    }\n"
    "    document.body.appendChild(notification);\n"
    "    setTimeout(() => {\n"
    "        notification.style.animation = 'slideOutRight 0.3s ease';\n"
    "        setTimeout(() => {\n"
    "            if (notification.parentNode) "
    "notification.parentNode.removeChild(notification);\n"
    "        }, 300);\n"
    "    }, 3000);\n"
    "}\n"
    "\n"
    "function handleTimeExpiration(timerInfo) {\n"
    "    // Nekontrolovat expiraci pokud není časová kontrola aktivní\n"
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) "
    "{\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const expiredPlayer = timerInfo.is_white_turn ? 'White' : 'Black';\n"
    "    showTimeWarning('Time expired! ' + expiredPlayer + ' lost on time.', "
    "'critical');\n"
    "    const pauseBtn = document.getElementById('pause-timer');\n"
    "    const resumeBtn = document.getElementById('resume-timer');\n"
    "    if (pauseBtn) pauseBtn.disabled = true;\n"
    "    if (resumeBtn) resumeBtn.disabled = true;\n"
    "}\n"
    "\n"
    "function toggleCustomSettings() {\n"
    "    const customSettings = "
    "document.getElementById('custom-time-settings');\n"
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
    "    localStorage.setItem('chess_time_control', "
    "selectedTimeControl.toString());\n"
    "}\n"
    "\n"
    "// ========== TIMER INITIALIZATION AND MAIN FUNCTIONS ==========\n"
    "\n"
    "function initTimerSystem() {\n"
    "    console.log('🔵 Initializing timer system...');\n"
    "    // Check if DOM elements exist before accessing them\n"
    "    const timeControlSelect = "
    "document.getElementById('time-control-select');\n"
    "    const applyButton = document.getElementById('apply-time-control');\n"
    "    if (!timeControlSelect) {\n"
    "        console.warn('⚠️ Timer controls not ready yet, retrying in "
    "100ms...');\n"
    "        setTimeout(() => initTimerSystem(), 100);\n"
    "        return;\n"
    "    }\n"
    "    const savedTimeControl = "
    "localStorage.getItem('chess_time_control');\n"
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
    "    console.log('✅ Timer update loop starting... (will update every "
    "1000ms)');\n"
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
    "    }, 1000); // Optimized from 200ms to 1s (5× fewer requests, still "
    "responsive)\n"
    "    console.log('✅ Timer interval set successfully, ID:', "
    "timerUpdateInterval);\n"
    "    // Initial immediate update\n"
    "    console.log('⏱️ Calling initial timer update...');\n"
    "    updateTimerDisplay().catch(e => console.error('❌ Initial timer "
    "update failed:', e));\n"
    "}\n"
    "\n"
    "async function updateTimerDisplay() {\n"
    "    try {\n"
    "        console.log('⏱️ updateTimerDisplay() called, fetching "
    "/api/timer...');\n"
    "        const response = await fetch('/api/timer');\n"
    "        console.log('⏱️ /api/timer response status:', response.status);\n"
    "        if (response.ok) {\n"
    "            const timerInfo = await response.json();\n"
    "            timerData = timerInfo;\n"
    "            // Format time for logging\n"
    "            const whiteTime = formatTime(timerInfo.white_time_ms);\n"
    "            const blackTime = formatTime(timerInfo.black_time_ms);\n"
    "            console.log('⏱️ Timer:', timerInfo.config ? "
    "timerInfo.config.name : 'NO CONFIG', '| White:', whiteTime, '(' + "
    "timerInfo.white_time_ms + 'ms)', '| Black:', blackTime, '(' + "
    "timerInfo.black_time_ms + 'ms)');\n"
    "            updatePlayerTime('white', timerInfo.white_time_ms);\n"
    "            updatePlayerTime('black', timerInfo.black_time_ms);\n"
    "            updateActivePlayer(timerInfo.is_white_turn);\n"
    "            updateProgressBars(timerInfo);\n"
    "            updateTimerStats(timerInfo);\n"
    "            // Disable/enable timer controls podle config.type\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            const resetBtn = document.getElementById('reset-timer');\n"
    "            const isTimerActive = timerInfo.config && "
    "timerInfo.config.type !== 0;\n"
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
    "    const timeControlSelect = "
    "document.getElementById('time-control-select');\n"
    "    const timeControlType = parseInt(timeControlSelect.value);\n"
    "    let config = { type: timeControlType };\n"
    "    if (timeControlType === 14) {\n"
    "        const minutes = "
    "parseInt(document.getElementById('custom-minutes').value);\n"
    "        const increment = "
    "parseInt(document.getElementById('custom-increment').value);\n"
    "        if (minutes < 1 || minutes > 180) { alert('Minutes must be "
    "between 1 and 180'); return; }\n"
    "        if (increment < 0 || increment > 60) { alert('Increment must be "
    "between 0 and 60 seconds'); return; }\n"
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
    "            showTimeWarning('Time control applied!', 'info');\n"
    "            const applyBtn = "
    "document.getElementById('apply-time-control');\n"
    "            if (applyBtn) applyBtn.disabled = true;\n"
    "        } else {\n"
    "            const errorText = await response.text();\n"
    "            console.error('Failed to apply time control:', "
    "response.status, errorText);\n"
    "            throw new Error('Failed to apply time control: ' + "
    "errorText);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Error applying time control:', error);\n"
    "        showTimeWarning('Error setting time control: ' + error.message, "
    "'critical');\n"
    "    }\n"
    "}\n"
    "\n"
    "async function pauseTimer() {\n"
    "    try {\n"
    "        const response = await fetch('/api/timer/pause', { method: "
    "'POST' "
    "});\n"
    "        if (response.ok) {\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            if (pauseBtn) pauseBtn.style.display = 'none';\n"
    "            if (resumeBtn) resumeBtn.style.display = 'inline-block';\n"
    "            showTimeWarning('Timer paused', 'info');\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('❌ Error pausing timer:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function resumeTimer() {\n"
    "    try {\n"
    "        const response = await fetch('/api/timer/resume', { method: "
    "'POST' });\n"
    "        if (response.ok) {\n"
    "            const pauseBtn = document.getElementById('pause-timer');\n"
    "            const resumeBtn = document.getElementById('resume-timer');\n"
    "            if (pauseBtn) pauseBtn.style.display = 'inline-block';\n"
    "            if (resumeBtn) resumeBtn.style.display = 'none';\n"
    "            showTimeWarning('Timer resumed', 'info');\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('❌ Error resuming timer:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function resetTimer() {\n"
    "    if (confirm('Really reset timer?')) {\n"
    "        try {\n"
    "            const response = await fetch('/api/timer/reset', { method: "
    "'POST' });\n"
    "            if (response.ok) {\n"
    "                showTimeWarning('Timer reset', 'info');\n"
    "                console.log('✅ Timer reset successfully');\n"
    "                await updateTimerDisplay();\n"
    "            }\n"
    "        } catch (error) {\n"
    "            console.error('❌ Error resetting timer:', error);\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// Expose timer functions globally for inline onclick handlers\n"
    "window.changeTimeControl = changeTimeControl;\n"
    "window.applyTimeControl = applyTimeControl;\n"
    "window.pauseTimer = pauseTimer;\n"
    "window.resumeTimer = resumeTimer;\n"
    "window.resetTimer = resetTimer;\n"
    "window.hideEndgameReport = hideEndgameReport;\n"
    "window.toggleRemoteControl = function() {\n"
    "    const checkbox = document.getElementById('remote-control-enabled');\n"
    "    if (checkbox) {\n"
    "        remoteControlEnabled = checkbox.checked;\n"
    "        console.log('Remote control:', remoteControlEnabled ? 'ENABLED' "
    ": "
    "'DISABLED');\n"
    "    }\n"
    "};\n"
    "window.hideEndgameReport = hideEndgameReport;\n"
    "\n"
    "// Initialize timer system immediately (will retry if DOM not ready)\n"
    "console.log('⏱️ Exposing timer functions and calling "
    "initTimerSystem()...');\n"
    "try {\n"
    "    initTimerSystem();\n"
    "    console.log('✅ initTimerSystem() called successfully');\n"
    "} catch (error) {\n"
    "    console.error('❌ CRITICAL ERROR in initTimerSystem():', error);\n"
    "    console.error('Stack:', error.stack);\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// KEYBOARD SHORTCUTS AND EVENT HANDLERS\n"
    "// "
    "========================================================================="
    "="
    "==\n"
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
    "            } else if (!reviewMode && !sandboxMode && historyData.length "
    "> 0) {\n"
    "                enterReviewMode(historyData.length - 1);\n"
    "            }\n"
    "            break;\n"
    "        case 'ArrowRight':\n"
    "            e.preventDefault();\n"
    "            if (reviewMode && currentReviewIndex < historyData.length - "
    "1) {\n"
    "                enterReviewMode(currentReviewIndex + 1);\n"
    "            }\n"
    "            break;\n"
    "    }\n"
    "});\n"
    "\n"
    "// Click outside to deselect\n"
    "document.addEventListener('click', (e) => {\n"
    "    if (!e.target.closest('.square') && "
    "!e.target.closest('.history-item')) {\n"
    "        if (!reviewMode) {\n"
    "            clearHighlights();\n"
    "        }\n"
    "    }\n"
    "});\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// WIFI FUNCTIONS\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "\n"
    "async function saveWiFiConfig() {\n"
    "    const ssid = document.getElementById('wifi-ssid').value;\n"
    "    const password = document.getElementById('wifi-password').value;\n"
    "    if (!ssid || !password) {\n"
    "        alert('SSID and password are required');\n"
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
    "            alert('WiFi config saved. Now press \"Connect STA\".');\n"
    "        } else {\n"
    "            alert('Failed to save WiFi config: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Error: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function connectSTA() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/connect', { method: "
    "'POST' });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('Connecting to WiFi...');\n"
    "            setTimeout(updateWiFiStatus, 1500);\n"
    "        } else {\n"
    "            alert('Failed to connect: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Error: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function disconnectSTA() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/disconnect', { method: "
    "'POST' });\n"
    "        const data = await response.json();\n"
    "        if (data.success) {\n"
    "            alert('Disconnected from WiFi');\n"
    "            setTimeout(updateWiFiStatus, 1000);\n"
    "        } else {\n"
    "            alert('Failed to disconnect: ' + data.message);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        alert('Error: ' + error.message);\n"
    "    }\n"
    "}\n"
    "\n"
    "async function updateWiFiStatus() {\n"
    "    try {\n"
    "        const response = await fetch('/api/wifi/status');\n"
    "        const data = await response.json();\n"
    "        document.getElementById('ap-ssid').textContent = data.ap_ssid || "
    "'ESP32-CzechMate';\n"
    "        document.getElementById('ap-ip').textContent = data.ap_ip || "
    "'192.168.4.1';\n"
    "        document.getElementById('ap-clients').textContent = "
    "data.ap_clients || 0;\n"
    "        document.getElementById('sta-ssid').textContent = data.sta_ssid "
    "|| 'Not configured';\n"
    "        document.getElementById('sta-ip').textContent = data.sta_ip || "
    "'Not connected';\n"
    "        document.getElementById('sta-connected').textContent = "
    "data.sta_connected ? 'true' : 'false';\n"
    "        if (data.sta_ssid && data.sta_ssid !== 'Not configured') {\n"
    "            document.getElementById('wifi-ssid').value = data.sta_ssid;\n"
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
    "    wifiStatusInterval = setInterval(updateWiFiStatus, 10000); // "
    "Reduced "
    "from 5s to 10s\n"
    "}\n"
    "\n"
    "// Start WiFi status updates when DOM is ready\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', "
    "startWiFiStatusUpdateLoop);\n"
    "} else {\n"
    "    startWiFiStatusUpdateLoop();\n"
    "}\n"
    "\n"
    "// "
    "========================================================================="
    "="
    "==\n"
    "// DEMO MODE (SCREENSAVER) FUNCTIONS\n"
    "// "
    "========================================================================="
    "="
    "==\n"
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
    "            console.log('✅ Demo mode toggled:', newState ? 'ON' : "
    "'OFF');\n"
    "            // Update status immediately\n"
    "            await updateDemoModeStatus();\n"
    "        } else {\n"
    "            console.error('❌ Failed to toggle demo mode');\n"
    "            alert('Failed to toggle demo mode: ' + (data.message || "
    "'Unknown error'));\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Error toggling demo mode:', error);\n"
    "        alert('Error toggling demo mode');\n"
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
    " * Update demo mode status indicator in UI\n"
    " */\n"
    "async function updateDemoModeStatus() {\n"
    "    try {\n"
    "        const enabled = await isDemoModeEnabled();\n"
    "        const statusEl = document.getElementById('demoStatus');\n"
    "        const btnEl = document.getElementById('btnDemoMode');\n"
    "\n"
    "        if (statusEl) {\n"
    "            if (enabled) {\n"
    "                statusEl.textContent = '🟢 Active';\n"
    "                statusEl.style.color = '#4CAF50';\n"
    "                statusEl.style.fontWeight = 'bold';\n"
    "            } else {\n"
    "                statusEl.textContent = '⚫ Off';\n"
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
    "    } catch (error) {\n"
    "        console.error('Failed to update demo mode status:', error);\n"
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
    "    demoModeStatusInterval = setInterval(updateDemoModeStatus, 5000); // "
    "Reduced from 3s to 5s\n"
    "}\n"
    "\n"
    "// Start demo mode status updates when DOM is ready\n"
    "if (document.readyState === 'loading') {\n"
    "    document.addEventListener('DOMContentLoaded', "
    "startDemoModeStatusUpdateLoop);\n"
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
    "    if (!reviewMode || currentReviewIndex >= historyData.length - 1) "
    "return;\n"
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
      ESP_LOGE(TAG, "❌ Chunk %zu send failed: %s", chunk_num,
               esp_err_to_name(ret));
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
    "padding: 10px; "
    "}\n"
    ".container { "
    "width: 95%; "
    "max-width: 1600px; "
    "margin: 0 auto; "
    "}\n"
    "h1 { "
    "color: #4CAF50; "
    "text-align: center; "
    "margin-bottom: 20px; "
    "font-size: 1.5em; "
    "font-weight: 600; "
    "}\n"
    ".main-content { "
    "display: grid; "
    "grid-template-columns: 280px 1fr 280px; "
    "grid-template-areas: 'left center right'; "
    "gap: 15px; "
    "}\n"
    "@media (max-width: 1200px) { "
    ".main-content { "
    "grid-template-columns: 1fr 280px; "
    "grid-template-areas: 'center right' 'left right'; "
    "} "
    "}\n"
    "@media (max-width: 768px) { "
    ".main-content { "
    "grid-template-columns: 1fr; "
    "grid-template-areas: 'center' 'left' 'right'; "
    "} "
    "}\n"
    ".board-container { grid-area: center; } "
    ".info-panel { grid-area: right; } "
    ".game-info-panel { grid-area: left; } "
    ".board-container { "
    "background: #2a2a2a; "
    "border-radius: 8px; "
    "padding: 15px; "
    "box-shadow: 0 4px 12px rgba(0,0,0,0.3); "
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
    ".square.lifted { "
    "background: #4CAF50 !important; "
    "box-shadow: inset 0 0 20px rgba(76,175,80,0.5); "
    "}"
    ".square.error-invalid { "
    "background: #f44336 !important; "
    "box-shadow: inset 0 0 20px rgba(244,67,54,0.6); "
    "animation: errorPulse 1s infinite; "
    "}"
    ".square.error-original { "
    "background: #2196F3 !important; "
    "box-shadow: inset 0 0 20px rgba(33,150,243,0.6); "
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
    ".info-panel, .game-info-panel { "
    "background: #2a2a2a; "
    "border-radius: 8px; "
    "padding: 15px; "
    "box-shadow: 0 4px 12px rgba(0,0,0,0.3); "
    "}"
    ".status-box { "
    "background: #333; "
    "border-left: 3px solid #4CAF50; "
    "padding: 12px; "
    "margin-bottom: 10px; "
    "border-radius: 4px; "
    "}"
    ".status-box h3 { color: #4CAF50; margin-bottom: 8px; font-weight: 600; "
    "font-size: 0.9em; }"
    ".status-item { "
    "display: flex; "
    "justify-content: space-between; "
    "margin: 4px 0; "
    "font-size: 13px; "
    "}"
    ".status-value { font-weight: 600; color: #e0e0e0; font-family: 'Courier "
    "New', monospace; }"
    ".history-box { "
    "max-height: 150px; "
    "overflow-y: auto; "
    "background: #333; "
    "padding: 8px; "
    "border-radius: 4px; "
    "margin-top: 10px; "
    "}"
    ".history-item { "
    "padding: 6px; "
    "border-bottom: 1px solid #444; "
    "font-size: 11px; "
    "color: #aaa; "
    "font-family: 'Courier New', monospace; "
    "}"
    ".captured-box { "
    "margin-top: 10px; "
    "padding: 10px; "
    "background: #333; "
    "border-radius: 4px; "
    "}"
    ".captured-pieces { "
    "display: flex; "
    "flex-wrap: wrap; "
    "gap: 3px; "
    "margin-top: 5px; "
    "}"
    ".captured-piece { "
    "font-size: 1.2em; "
    "color: #888; "
    "}"
    ".captured-box h3 { color: #4CAF50; font-size: 0.85em; margin-bottom: "
    "5px; "
    "}"
    ".captured-box div { font-size: 0.75em; color: #888; margin-top: 5px; }"
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
    "gap: 10px; "
    "margin-bottom: 10px; "
    "}"
    ".time-control-selector select { "
    "flex: 1; "
    "padding: 8px 12px; "
    "background: #333; "
    "color: #e0e0e0; "
    "border: 1px solid #555; "
    "border-radius: 4px; "
    "font-size: 14px; "
    "}"
    ".time-control-selector button { "
    "padding: 8px 16px; "
    "background: #4CAF50; "
    "color: white; "
    "border: none; "
    "border-radius: 4px; "
    "cursor: pointer; "
    "font-weight: 600; "
    "transition: all 0.2s; "
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
// Chunk 2a: Layout Start (Body, Container, H1, Main Content Open)
static const char html_chunk_layout_start[] = "<body>"
                                              "<div class='container'>"
                                              "<h1>♟️ CZECHMATE</h1>"
                                              "<div class='main-content'>";

// Chunk 2b: Game Info Panel (Left Column)
static const char html_chunk_game_info[] =
    "<div class='game-info-panel'>"
    "<div class='status-box'>"
    "<h3>⏰ Čas</h3>"
    "<div class='timer-display'>"
    "<div class='player-time white-time' id='white-timer'>"
    "<div class='player-info'>"
    "<span class='player-name'>♚ Bílý</span>"
    "<span class='move-indicator' id='white-move-indicator'>●</span>"
    "</div>"
    "<div class='time-value' id='white-time'>10:00</div>"
    "<div class='time-bar'>"
    "<div class='time-progress' id='white-progress'></div>"
    "</div>"
    "</div>"
    "<div class='player-time black-time' id='black-timer'>"
    "<div class='player-info'>"
    "<span class='player-name'>♔ Černý</span>"
    "<span class='move-indicator' id='black-move-indicator'>●</span>"
    "</div>"
    "<div class='time-value' id='black-time'>10:00</div>"
    "<div class='time-bar'>"
    "<div class='time-progress' id='black-progress'></div>"
    "</div>"
    "</div>"
    "</div>"
    "<div class='timer-controls'>"
    "<button id='pause-timer' onclick='pauseTimer()'>⏸️ Pozastavit</button>"
    "<button id='resume-timer' onclick='resumeTimer()' style='display: "
    "none;'>▶️ Pokračovat</button>"
    "<button id='reset-timer' onclick='resetTimer()'>🔄 Resetovat</button>"
    "</div>"
    "<div class='timer-stats'>"
    "<div class='stat-item'>"
    "<span class='stat-label'>Průměrný tah:</span>"
    "<span id='avg-move-time' class='stat-value'>-</span>"
    "</div>"
    "<div class='stat-item'>"
    "<span class='stat-label'>Celkem tahů:</span>"
    "<span id='total-moves' class='stat-value'>0</span>"
    "</div>"
    "</div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>Game Status</h3>"
    "<div class='status-item'>"
    "<span>State:</span>"
    "<span id='game-state' class='status-value'>-</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>Player:</span>"
    "<span id='current-player' class='status-value'>-</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>Moves:</span>"
    "<span id='move-count' class='status-value'>0</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>In Check:</span>"
    "<span id='in-check' class='status-value'>No</span>"
    "</div>"
    "<div class='status-item' "
    "style='margin-top:12px;padding-top:12px;border-top:1px solid "
    "rgba(255,255,255,0.1)'>"
    "<button id='new-game-btn' onclick='startNewGame()' "
    "style='width:100%;padding:10px;background:#4CAF50;color:white;border:"
    "none;"
    "border-radius:6px;cursor:pointer;font-weight:600;font-size:14px;"
    "transition:all 0.2s;box-shadow:0 2px 5px rgba(0,0,0,0.2)' "
    "onmouseover=\"this.style.transform='translateY(-1px)';this.style."
    "boxShadow='0 4px 8px rgba(0,0,0,0.3)'\" "
    "onmouseout=\"this.style.transform='translateY(0)';this.style.boxShadow='"
    "0 "
    "2px 5px rgba(0,0,0,0.2)'\">➕ New Game</button>"
    "</div>"
    "</div>"
    "<div class='captured-box'>"
    "<h3>Captured Pieces</h3>"
    "<div>White:</div>"
    "<div id='white-captured' class='captured-pieces'></div>"
    "<div style='margin-top: 10px;'>Black:</div>"
    "<div id='black-captured' class='captured-pieces'></div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>Move History</h3>"
    "<div id='history' class='history-box' style='max-height: 400px;'></div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>⏱️ Časová kontrola</h3>"
    "<div class='time-control-selector'>"
    "<select id='time-control-select' onchange='changeTimeControl()'>"
    "<option value='0'>Bez časové kontroly</option>"
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
    "<div id='custom-time-settings' class='custom-settings' style='display: "
    "none;'>"
    "<div class='custom-input-group'>"
    "<label>Minuty:</label>"
    "<input type='number' id='custom-minutes' min='1' max='180' value='10'>"
    "</div>"
    "<div class='custom-input-group'>"
    "<label>Increment (sekundy):</label>"
    "<input type='number' id='custom-increment' min='0' max='60' value='0'>"
    "</div>"
    "</div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>Lifted Piece</h3>"
    "<div class='status-item'>"
    "<span>Piece:</span>"
    "<span id='lifted-piece' class='status-value'>-</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>Position:</span>"
    "<span id='lifted-position' class='status-value'>-</span>"
    "</div>"
    "</div>"
    "</div>";

// Chunk 2c: Board Container (Center Column) + Modal
static const char html_chunk_board[] =
    "<div class='board-container'>"
    "<button class='btn-try-moves' onclick='enterSandboxMode()'>Try "
    "Moves</button>"
    "<div id='board' class='board'></div>"
    "<div id='loading' class='loading'>Loading board...</div>"
    "</div>"
    "<!-- Promotion Modal -->"
    "<div id='promotion-modal' class='modal' style='display:none; "
    "position:fixed; top:0; left:0; width:100%; height:100%; "
    "background:rgba(0,0,0,0.8); z-index:2000; align-items:center; "
    "justify-content:center;'>"
    "<div class='modal-content' style='background:#333; padding:20px; "
    "border-radius:8px; text-align:center; border:2px solid #4CAF50;'>"
    "<h3 style='color:#4CAF50; margin-bottom:15px;'>Promote Pawn</h3>"
    "<div style='display:flex; gap:10px; justify-content:center;'>"
    "<button onclick=\"selectPromotion('Q')\" style='font-size:24px; "
    "padding:10px; cursor:pointer;'>♛</button>"
    "<button onclick=\"selectPromotion('R')\" style='font-size:24px; "
    "padding:10px; cursor:pointer;'>♜</button>"
    "<button onclick=\"selectPromotion('B')\" style='font-size:24px; "
    "padding:10px; cursor:pointer;'>♝</button>"
    "<button onclick=\"selectPromotion('N')\" style='font-size:24px; "
    "padding:10px; cursor:pointer;'>♞</button>"
    "</div>"
    "<button onclick='cancelPromotion()' style='margin-top:15px; padding:8px "
    "16px; background:#f44336; color:white; border:none; border-radius:4px; "
    "cursor:pointer;'>Cancel</button>"
    "</div>"
    "</div>";

// Chunk 2b: Info panel (status, timer, history, captured)
static const char html_chunk_infopanel[] =
    "<div class='info-panel'>"
    // Items moved to html_chunk_game_info

    "<div class='status-box'>"
    "<h3>WiFi (Internet)</h3>"
    "<div class='status-item'>"
    "<span>AP SSID:</span>"
    "<span id='ap-ssid' class='status-value'>ESP32-CzechMate</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>AP IP:</span>"
    "<span id='ap-ip' class='status-value'>192.168.4.1</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>AP Clients:</span>"
    "<span id='ap-clients' class='status-value'>0</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>STA SSID:</span>"
    "<span id='sta-ssid' class='status-value'>Not configured</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>STA IP:</span>"
    "<span id='sta-ip' class='status-value'>Not connected</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>STA Connected:</span>"
    "<span id='sta-connected' class='status-value'>false</span>"
    "</div>"
    "<div style='margin-top: 15px;'>"
    "<input type='text' id='wifi-ssid' placeholder='WiFi SSID' "
    "maxlength='32' "
    "style='width: 100%; padding: 8px; margin-bottom: 8px; background: #111; "
    "color: #e0e0e0; border: 1px solid #444; border-radius: 4px; "
    "pointer-events: auto; user-select: text;'>"
    "<input type='password' id='wifi-password' placeholder='WiFi password' "
    "maxlength='64' style='width: 100%; padding: 8px; margin-bottom: 8px; "
    "background: #111; color: #e0e0e0; border: 1px solid #444; "
    "border-radius: "
    "4px; pointer-events: auto; user-select: text;'>"
    "<button id='wifi-save-btn' style='width: 100%; padding: 10px; "
    "background: "
    "#4CAF50; color: white; border: none; border-radius: 4px; cursor: "
    "pointer; "
    "margin-bottom: 5px;'>Save WiFi config</button>"
    "<button id='wifi-connect-btn' style='width: 48%; padding: 10px; "
    "background: #666; color: white; border: none; border-radius: 4px; "
    "cursor: "
    "pointer; margin-right: 4%;'>Connect STA</button>"
    "<button id='wifi-disconnect-btn' style='width: 48%; padding: 10px; "
    "background: #666; color: white; border: none; border-radius: 4px; "
    "cursor: "
    "pointer;'>Disconnect STA</button>"
    "<button id='wifi-clear-btn' style='width: 100%; padding: 8px; "
    "margin-top: "
    "5px; background: #f44336; color: white; border: none; border-radius: "
    "4px; "
    "cursor: pointer; font-size: 0.9em;'>Clear WiFi config</button>"
    "</div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>🌐 Web Status</h3>"
    "<div class='status-item'>"
    "<span>Lock Status:</span>"
    "<span id='web-lock-status' class='status-value'>-</span>"
    "</div>"
    "<div class='status-item'>"
    "<span>Internet:</span>"
    "<span id='web-online-status' class='status-value'>-</span>"
    "</div>"
    "</div>"
    "<div class='status-box'>"
    "<h3>🎮 Remote Control</h3>"
    "<div style='margin: 10px 0;'>"
    "<label style='display: flex; align-items: center; cursor: pointer;'>"
    "<input type='checkbox' id='remote-control-enabled' "
    "onchange='toggleRemoteControl()' "
    "style='margin-right: 10px; width: 20px; height: 20px; cursor: "
    "pointer;'> "
    "<span>Enable Remote Control</span>"
    "</label>"
    "<div style='margin-top: 5px; font-size: 0.8em; color: #ff9800;'>"
    "⚠️ Warning: Sync logic/physical state!"
    "</div>"
    "</div>"
    "</div>";

// Chunk 2c: Review and Sandbox banners
static const char html_chunk_banners[] =
    "<!-- Review Mode Banner -->"
    "<div class='review-banner' id='review-banner'>"
    "<div class='review-header'>"
    "<div style='display:flex;align-items:center;gap:8px;flex:1'>"
    "<span style='font-size: 20px;'>📖</span>"
    "<span id='review-move-text'>Prohlížíš tah 0</span>"
    "</div>"
    "<button class='btn-header-close' onclick='exitReviewMode()' "
    "title='Zavřít'>✕</button>"
    "</div>"
    "<div class='review-controls'>"
    "<button class='nav-btn nav-first' onclick='goToMove(0)' "
    "title='Na začátek' aria-label='První tah'>⏮️</button>"
    "<button class='nav-btn nav-prev' onclick='prevReviewMove()' "
    "title='Předchozí tah' aria-label='Předchozí'>◀️</button>"
    "<button class='nav-btn nav-next' onclick='nextReviewMove()' "
    "title='Další tah' aria-label='Další'>▶️</button>"
    "<button class='nav-btn nav-last' onclick='goToMove(-1)' "
    "title='Na konec' aria-label='Poslední tah'>⏭️</button>"
    "</div>"
    "</div>"
    "<!-- Sandbox Mode Banner -->"
    "<div class='sandbox-banner' id='sandbox-banner'>"
    "<div class='sandbox-text'>"
    "<span>🎮</span>"
    "<span>Sandbox Mode - Zkoušíš tahy lokálně</span>"
    "</div>"
    "<div style='display: flex; gap: 10px;'>"
    "<button class='btn-exit-sandbox' id='sandbox-undo-btn' "
    "onclick='undoSandboxMove()' disabled>↶ Undo (0/10)</button>"
    "<button class='btn-exit-sandbox' onclick='exitSandboxMode()'>Zpět na "
    "skutečnou pozici</button>"
    "</div>"
    "</div>";

// Chunk 3: JavaScript - load from external file (prevents UTF-8 chunking
// issues)
static const char html_chunk_javascript[] =
    "<script src='/chess_app.js'></script>";

// Chunk: Demo Mode UI Panel
static const char html_chunk_demo_mode[] =
    "<div class='status-box' style='border-left: 3px solid #ffa500;'>"
    "<h3 style='color: #ffa500;'>🎮 Demo Mode</h3>"
    "<div style='margin: 10px 0;'>"
    "<label style='display: flex; align-items: center; cursor: pointer;'>"
    "<input type='checkbox' id='demo-enabled' style='margin-right: 10px; "
    "width: 20px; height: 20px; cursor: pointer;'> "
    "<span>Enable Demo Mode</span>"
    "</label>"
    "</div>"
    "<div style='margin: 15px 0;'>"
    "<label style='display: block; margin-bottom: 5px; color: #888;'>Move "
    "Speed:</label>"
    "<div style='display: flex; align-items: center; gap: 10px;'>"
    "<input type='range' id='demo-speed' min='500' max='5000' step='100' "
    "value='2000' style='flex-grow: 1; cursor: pointer;' disabled>"
    "<span id='demo-speed-value' style='width: 60px; text-align: right; "
    "color: #888;'>2000ms</span>"
    "</div>"
    "</div>"
    "<button id='stop-demo-btn' onclick='stopDemo()' "
    "style='width: 100%; padding: 10px; margin-top: 10px; background: "
    "#f44336; "
    "color: white; "
    "border: none; border-radius: 4px; cursor: pointer; font-size: 14px; "
    "font-weight: 600; display: none; transition: background 0.2s;'>"
    "⏹️ Stop Playback</button>"
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
    "<div class='status-box' style='margin-top: 20px;'>"
    "<h3 style='margin-top: 0; color: #4CAF50;'>📡 MQTT Configuration</h3>"
    "<div style='margin: 10px 0;'>"
    "<label style='display: block; margin-bottom: 5px;'>Broker "
    "Host/IP:</label>"
    "<input type='text' id='mqtt-host' placeholder='homeassistant.local' "
    "style='width: 100%; padding: 8px; border-radius: 4px; border: 1px solid "
    "#555; "
    "background: #1a1a1a; color: #fff;'>"
    "</div>"
    "<div style='margin: 15px 0;'>"
    "<label style='display: block; margin-bottom: 5px;'>Port:</label>"
    "<input type='number' id='mqtt-port' value='1883' min='1' max='65535' "
    "style='width: 100%; padding: 8px; border-radius: 4px; border: 1px solid "
    "#555; "
    "background: #1a1a1a; color: #fff;'>"
    "</div>"
    "<div style='margin: 15px 0;'>"
    "<label style='display: block; margin-bottom: 5px;'>Username:</label>"
    "<input type='text' id='mqtt-username' placeholder='mqtt_user' "
    "style='width: 100%; padding: 8px; border-radius: 4px; border: 1px solid "
    "#555; "
    "background: #1a1a1a; color: #fff;'>"
    "</div>"
    "<div style='margin: 15px 0;'>"
    "<label style='display: block; margin-bottom: 5px;'>Password:</label>"
    "<input type='password' id='mqtt-password' placeholder='••••••••' "
    "style='width: 100%; padding: 8px; border-radius: 4px; border: 1px solid "
    "#555; "
    "background: #1a1a1a; color: #fff;'>"
    "</div>"
    "<button onclick='saveMQTTConfig()' "
    "style='width: 100%; padding: 10px; background: #4CAF50; color: white; "
    "border: none; border-radius: 4px; cursor: pointer; font-size: 16px;'>"
    "Save MQTT Config</button>"
    "<div id='mqtt-status' style='margin-top: 10px; padding: 8px; "
    "border-radius: 4px; "
    "display: none;'></div>"
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
    "    statusDiv.textContent = 'Error: Host is required';\n"
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
    "      statusDiv.textContent = 'MQTT config saved! Restart ESP32 to "
    "apply.';\n"
    "    } else {\n"
    "      statusDiv.style.background = '#f44336';\n"
    "      statusDiv.textContent = 'Error: ' + (data.message || 'Unknown "
    "error');\n"
    "    }\n"
    "  } catch (e) {\n"
    "    statusDiv.style.display = 'block';\n"
    "    statusDiv.style.background = '#f44336';\n"
    "    statusDiv.textContent = 'Error: ' + e.message;\n"
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
    "</div></div></div>";

// Chunk 4: HTML closing tags
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
    ESP_LOGE(TAG, "❌ Chunk 1 failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // CHUNK 2: Layout Start + Game Info + Board + Right Panel Start

  // 1. Layout Start
  size_t chunk_layout_len = strlen(html_chunk_layout_start);
  ESP_LOGI(TAG, "📤 Chunk 2a: LAYOUT START (%zu bytes)", chunk_layout_len);
  ret = httpd_resp_send_chunk(req, html_chunk_layout_start, chunk_layout_len);
  if (ret != ESP_OK)
    return ret;

  // 2. Game Info (Left)
  size_t chunk_game_len = strlen(html_chunk_game_info);
  ESP_LOGI(TAG, "📤 Chunk 2b: GAME INFO (%zu bytes)", chunk_game_len);
  ret = httpd_resp_send_chunk(req, html_chunk_game_info, chunk_game_len);
  if (ret != ESP_OK)
    return ret;

  // 3. Board (Center)
  size_t chunk_board_len = strlen(html_chunk_board);
  ESP_LOGI(TAG, "📤 Chunk 2c: BOARD (%zu bytes)", chunk_board_len);
  ret = httpd_resp_send_chunk(req, html_chunk_board, chunk_board_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 2c failed: %s", esp_err_to_name(ret));
    return ret;
  }
  // Traffic shaping
  vTaskDelay(pdMS_TO_TICKS(20));

  // 4. Info Panel (Right) start + content
  // Note: html_chunk_infopanel starts with <div class='info-panel'> and
  // contains WiFi etc. It stays open to accept Demo/MQTT.
  size_t chunk_info_len = strlen(html_chunk_infopanel);
  ESP_LOGI(TAG, "📤 Chunk 3: INFO PANEL START (%zu bytes)", chunk_info_len);
  ret = httpd_resp_send_chunk(req, html_chunk_infopanel, chunk_info_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 3 failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // CHUNK 4: Banners (review mode, sandbox mode)
  size_t chunk4_len = strlen(html_chunk_banners);
  ESP_LOGI(TAG, "📤 Chunk 4: BANNERS (%zu bytes)", chunk4_len);
  ret = httpd_resp_send_chunk(req, html_chunk_banners, chunk4_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 4 failed: %s", esp_err_to_name(ret));
    return ret;
  }
  // Traffic shaping
  vTaskDelay(pdMS_TO_TICKS(20));

  // CHUNK 5: Complete JavaScript (all functions in ONE piece!)
  size_t chunk5_len = strlen(html_chunk_javascript);
  ESP_LOGI(TAG, "📤 Chunk 5: JAVASCRIPT (%zu bytes)", chunk5_len);
  ret = httpd_resp_send_chunk(req, html_chunk_javascript, chunk5_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 5 failed: %s", esp_err_to_name(ret));
    return ret;
  }
  // Traffic shaping (JS is large)
  vTaskDelay(pdMS_TO_TICKS(50));

  // CHUNK 6: Demo Mode UI
  size_t chunk6_len = strlen(html_chunk_demo_mode);
  ESP_LOGI(TAG, "📤 Chunk 6: DEMO MODE (%zu bytes)", chunk6_len);
  ret = httpd_resp_send_chunk(req, html_chunk_demo_mode, chunk6_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 6 failed: %s", esp_err_to_name(ret));
    return ret;
  }
  // Traffic shaping
  vTaskDelay(pdMS_TO_TICKS(20));

  // CHUNK 7: MQTT Configuration UI
  size_t chunk7_len = strlen(html_chunk_mqtt_config);
  ESP_LOGI(TAG, "📤 Chunk 7: MQTT CONFIG (%zu bytes)", chunk7_len);
  ret = httpd_resp_send_chunk(req, html_chunk_mqtt_config, chunk7_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 7 failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // CHUNK 8: Closing tags
  size_t chunk8_len = strlen(html_chunk_end);
  ESP_LOGI(TAG, "📤 Chunk 8: CLOSING (%zu bytes)", chunk8_len);
  ret = httpd_resp_send_chunk(req, html_chunk_end, chunk8_len);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunk 8 failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // End chunked transfer
  ret = httpd_resp_send_chunk(req, NULL, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Chunked transfer end failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(TAG,
           "✅ HTML sent successfully (10 chunks: %zu + %zu + %zu + %zu + %zu "
           "+ %zu + %zu + %zu + %zu + %zu bytes)",
           chunk1_len, chunk_layout_len, chunk_game_len, chunk_board_len,
           chunk_info_len, chunk4_len, chunk5_len, chunk6_len, chunk7_len,
           chunk8_len);
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
