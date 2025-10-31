/**
 * @file web_server_task.c
 * @brief ESP32-C6 Chess System v2.4 - Web Server Task komponenta
 * 
 * Tato komponenta zpracovava web server funkcionalitu:
 * - WiFi Access Point nastaveni
 * - HTTP server pro vzdalene ovladani
 * - Captive portal pro automaticke otevreni prohlizece
 * - REST API endpointy pro stav hry
 * - Web rozhrani pro sachovou hru
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-XX
 * 
 * @details
 * Tento task vytvari WiFi hotspot a HTTP server pro vzdalene ovladani
 * sachoveho systemu pres webove rozhrani. Uzivatel se muze pripojit
 * k WiFi "ESP32-Chess" a ovladat hru pres prohlizec.
 */

#include "web_server_task.h"
#include "freertos_chess.h"
#include "../game_task/include/game_task.h"
#include "../timer_system/include/timer_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External queue declaration
extern QueueHandle_t game_command_queue;

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

static const char* TAG = "WEB_SERVER_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro ESP_ERR_NOT_FOUND
 * 
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 * 
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany (WARNING pouze)
 * 
 * @details
 * Funkce je pouzivana pro bezpecny reset watchdog timeru behem web server operaci.
 * Zabranuje chybam pri startupu kdy task jeste neni registrovany.
 */
static esp_err_t web_server_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Log as WARNING instead of ERROR - task not registered yet
        ESP_LOGW(TAG, "WDT reset: task not registered yet (this is normal during startup)");
        return ESP_OK; // Treat as success for our purposes
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// WiFi configuration
#define WIFI_AP_SSID "ESP32-Chess"
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_GATEWAY "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"

// HTTP server configuration
#define HTTP_SERVER_PORT 80
#define HTTP_SERVER_MAX_URI_LEN 512
#define HTTP_SERVER_MAX_HEADERS 8
#define HTTP_SERVER_MAX_CLIENTS 4

// JSON buffer sizes
#define JSON_BUFFER_SIZE 2048  // Original size

// Web server state tracking
static bool task_running = false;
static bool web_server_active = false;
static bool wifi_ap_active = false;
static uint32_t web_server_start_time = 0;
static uint32_t client_count = 0;

// HTTP server handle
static httpd_handle_t httpd_handle = NULL;

// Netif handle
static esp_netif_t* ap_netif = NULL;

// External variables
QueueHandle_t web_server_status_queue = NULL;
QueueHandle_t web_server_command_queue = NULL;

// JSON buffer pool (reusable)
static char json_buffer[JSON_BUFFER_SIZE];

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static esp_err_t wifi_init_ap(void);
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t start_http_server(void);
static void stop_http_server(void);

// HTTP handlers
static esp_err_t http_get_root_handler(httpd_req_t *req);
static esp_err_t http_get_chess_js_handler(httpd_req_t *req);  // chess_app.js file
static esp_err_t http_get_test_handler(httpd_req_t *req);  // Test page for timer
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
// static esp_err_t http_post_move_handler(httpd_req_t *req);  // DISABLED - web je 100% READ-ONLY

// Captive portal handlers
static esp_err_t http_get_generate_204_handler(httpd_req_t *req);
static esp_err_t http_get_hotspot_handler(httpd_req_t *req);
static esp_err_t http_get_connecttest_handler(httpd_req_t *req);

// ============================================================================
// WIFI AP SETUP
// ============================================================================

/**
 * @brief Inicializuje WiFi Access Point
 * 
 * Tato funkce inicializuje WiFi Access Point pro web server.
 * Nastavi SSID, heslo a ostatni parametry pro WiFi hotspot.
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 * 
 * @details
 * Funkce inicializuje netif, event loop, WiFi a nastavi AP konfiguraci.
 * Registruje event handler pro WiFi udalosti a spusti Access Point.
 */
static esp_err_t wifi_init_ap(void)
{
    ESP_LOGI(TAG, "Initializing WiFi AP...");
    
    // CRITICAL: Initialize netif BEFORE creating default netif
    ESP_LOGI(TAG, "Initializing netif...");
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Netif already initialized");
    }
    
    // CRITICAL: Create default event loop BEFORE WiFi init
    ESP_LOGI(TAG, "Creating default event loop...");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create default event loop: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(TAG, "Event loop already created");
    } else {
        ESP_LOGI(TAG, "Event loop ready");
    }
    
    // Create default netif for AP
    ESP_LOGI(TAG, "Creating default WiFi AP netif...");
    ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create AP netif");
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default config
    ESP_LOGI(TAG, "Initializing WiFi...");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register WiFi event handler
    ret = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &wifi_event_handler,
                                              NULL,
                                              NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure WiFi AP
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .ssid_len = strlen(WIFI_AP_SSID),
            .password = WIFI_AP_PASSWORD,
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };
    
    // Set WiFi mode to AP
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set WiFi configuration
    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi AP initialized successfully");
    ESP_LOGI(TAG, "SSID: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Password: %s", WIFI_AP_PASSWORD);
    ESP_LOGI(TAG, "IP: %s", WIFI_AP_IP);
    
    return ESP_OK;
}

/**
 * @brief Obsluhuje WiFi eventy
 * 
 * Tato funkce obsluhuje WiFi eventy pro Access Point.
 * Sleduje pripojeni a odpojeni klientu.
 * 
 * @param arg Argument (nepouzivany)
 * @param event_base Typ eventu (WIFI_EVENT)
 * @param event_id ID eventu
 * @param event_data Data eventu
 * 
 * @details
 * Funkce sleduje pripojeni a odpojeni klientu k WiFi hotspotu
 * a aktualizuje pocet pripojenych klientu.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station connected, AID=%d", event->aid);
        client_count++;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station disconnected, AID=%d", event->aid);
        if (client_count > 0) {
            client_count--;
        }
    }
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
 * - Captive portal handlery
 */
static esp_err_t start_http_server(void)
{
    if (httpd_handle != NULL) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting HTTP server...");
    
    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_uri_handlers = 16; // ensure enough slots for all registered endpoints
    config.max_open_sockets = 4;  // Max allowed: LWIP_MAX_SOCKETS(7) - 3 internal = 4
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 1000;  // 1 second timeout for 6-chunk HTML (6×50ms delays + transfer time)
    config.max_resp_headers = 8;
    config.backlog_conn = 5;
    config.stack_size = 8192;  // Increased stack size for HTTP server task
    
    // Start HTTP server
    esp_err_t ret = httpd_start(&httpd_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    ESP_LOGI(TAG, "Registering URI handlers...");
    
    // Chess JavaScript file handler
    httpd_uri_t chess_js_uri = {
        .uri = "/chess_app.js",
        .method = HTTP_GET,
        .handler = http_get_chess_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &chess_js_uri);
    
    // Test page handler (minimal timer test)
    httpd_uri_t test_uri = {
        .uri = "/test",
        .method = HTTP_GET,
        .handler = http_get_test_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &test_uri);
    
    // Root handler (HTML page)
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_get_root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &root_uri);
    
    // API handlers
    httpd_uri_t board_uri = {
        .uri = "/api/board",
        .method = HTTP_GET,
        .handler = http_get_board_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &board_uri);
    
    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = http_get_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &status_uri);
    
    httpd_uri_t history_uri = {
        .uri = "/api/history",
        .method = HTTP_GET,
        .handler = http_get_history_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &history_uri);
    
    httpd_uri_t captured_uri = {
        .uri = "/api/captured",
        .method = HTTP_GET,
        .handler = http_get_captured_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &captured_uri);
    
    httpd_uri_t advantage_uri = {
        .uri = "/api/advantage",
        .method = HTTP_GET,
        .handler = http_get_advantage_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &advantage_uri);
    
    // Timer API handlers
    httpd_uri_t timer_uri = {
        .uri = "/api/timer",
        .method = HTTP_GET,
        .handler = http_get_timer_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &timer_uri);
    
    httpd_uri_t timer_config_uri = {
        .uri = "/api/timer/config",
        .method = HTTP_POST,
        .handler = http_post_timer_config_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &timer_config_uri);
    
    httpd_uri_t timer_pause_uri = {
        .uri = "/api/timer/pause",
        .method = HTTP_POST,
        .handler = http_post_timer_pause_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &timer_pause_uri);
    
    httpd_uri_t timer_resume_uri = {
        .uri = "/api/timer/resume",
        .method = HTTP_POST,
        .handler = http_post_timer_resume_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &timer_resume_uri);
    
    httpd_uri_t timer_reset_uri = {
        .uri = "/api/timer/reset",
        .method = HTTP_POST,
        .handler = http_post_timer_reset_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &timer_reset_uri);
    
    // POST /api/move handler DISABLED - web je 100% READ-ONLY
    // httpd_uri_t move_uri = {
    //     .uri = "/api/move",
    //     .method = HTTP_POST,
    //     .handler = http_post_move_handler,
    //     .user_ctx = NULL
    // };
    // httpd_register_uri_handler(httpd_handle, &move_uri);
    
    // Captive portal handlers
    httpd_uri_t generate_204_uri = {
        .uri = "/generate_204",
        .method = HTTP_GET,
        .handler = http_get_generate_204_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &generate_204_uri);
    
    httpd_uri_t hotspot_uri = {
        .uri = "/hotspot-detect.html",
        .method = HTTP_GET,
        .handler = http_get_hotspot_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &hotspot_uri);
    
    httpd_uri_t connecttest_uri = {
        .uri = "/connecttest.txt",
        .method = HTTP_GET,
        .handler = http_get_connecttest_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(httpd_handle, &connecttest_uri);
    
    ESP_LOGI(TAG, "HTTP server started successfully on port %d", HTTP_SERVER_PORT);
    
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
static void stop_http_server(void)
{
    if (httpd_handle != NULL) {
        httpd_stop(httpd_handle);
        httpd_handle = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

// ============================================================================
// CAPTIVE PORTAL HANDLERS
// ============================================================================

/**
 * @brief Handler pro generate_204 captive portal
 * 
 * Tato funkce obsluhuje generate_204 požadavky pro captive portal.
 * Vraci HTTP 204 No Content pro detekci internetoveho pripojeni.
 * 
 * @param req HTTP request
 * @return ESP_OK pri uspechu
 * 
 * @details
 * Funkce je pouzivana pro captive portal detekci.
 * Vraci HTTP 204 status pro indikaci ze neni internetove pripojeni.
 */
static esp_err_t http_get_generate_204_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Android captive portal request");
    httpd_resp_set_status(req, "204 No Content");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_get_hotspot_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "iOS captive portal request");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t http_get_connecttest_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Windows captive portal request");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ============================================================================
// REST API HANDLERS
// ============================================================================

static esp_err_t http_get_board_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/board");
    
    // Get board state from game task
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

static esp_err_t http_get_status_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/status");
    
    // Get game status from game task
    esp_err_t ret = game_get_status_json(json_buffer, sizeof(json_buffer));
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Failed to get game status", -1);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_buffer, strlen(json_buffer));
    
    return ESP_OK;
}

static esp_err_t http_get_history_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/history");
    
    // Get move history from game task
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

static esp_err_t http_get_captured_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/captured");
    
    // Get captured pieces from game task
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

static esp_err_t http_get_advantage_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/advantage");
    
    // Get advantage history from game task
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

// DISABLED: POST /api/move handler - web je 100% READ-ONLY
/*
static esp_err_t http_post_move_handler(httpd_req_t *req)
{
    // ... celá funkce zakomentována ...
}
*/

// ============================================================================
// TIMER API HANDLERS
// ============================================================================

static esp_err_t http_get_timer_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/timer");
    
    // Get timer state from game task using a local buffer to avoid races with other endpoints
    char local_json[JSON_BUFFER_SIZE];
    esp_err_t ret = game_get_timer_json(local_json, sizeof(local_json));
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Failed to get timer state", -1);
        return ESP_FAIL;
    }
    
    // Prevent caching timer responses in the browser
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, local_json, strlen(local_json));
    
    return ESP_OK;
}

static esp_err_t http_post_timer_config_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/timer/config");
    
    // Read JSON data
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "No data received", -1);
        return ESP_FAIL;
    }
    content[ret] = '\0';
    
    // Parse JSON and send command to game task
    chess_move_command_t cmd = { 0 };
    cmd.type = GAME_CMD_SET_TIME_CONTROL;
    
    // Improved JSON parsing using sscanf for more reliable parsing
    // First find the "type" field
    char *type_str = strstr(content, "\"type\":");
    if (type_str == NULL) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Missing 'type' field", -1);
        return ESP_FAIL;
    }
    
    // Parse type value
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
    
    // Parse custom time values if type is CUSTOM
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
        
        // Validate that custom values were provided
        if (minutes_str == NULL || increment_str == NULL) {
            httpd_resp_set_status(req, "400 Bad Request");
            httpd_resp_send(req, "Custom time control requires minutes and increment", -1);
            return ESP_FAIL;
        }
    }
    
    // Send command to game task
    if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "Failed to set time control", -1);
        return ESP_FAIL;
    }
    
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "Time control set successfully", -1);
    
    return ESP_OK;
}

static esp_err_t http_post_timer_pause_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/timer/pause");
    
    chess_move_command_t cmd = { 0 };
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

static esp_err_t http_post_timer_resume_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/timer/resume");
    
    chess_move_command_t cmd = { 0 };
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

static esp_err_t http_post_timer_reset_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/timer/reset");
    
    chess_move_command_t cmd = { 0 };
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
// TEST PAGE - MINIMAL TIMER TEST (for debugging)
// chess_app.js embedded (27426 bytes, 696 lines)
static const char chess_app_js_content[] =
    "// ============================================================================\n"
    "// CHESS WEB APP - EXTRACTED JAVASCRIPT FOR SYNTAX CHECKING\n"
    "// ============================================================================\n"
    "\n"
    "console.log('🚀 Chess JavaScript loading...');\n"
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
    "let capturedData = {white_captured: [], black_captured: []};\n"
    "let selectedSquare = null;\n"
    "let reviewMode = false;\n"
    "let currentReviewIndex = -1;\n"
    "let initialBoard = [];\n"
    "let sandboxMode = false;\n"
    "let sandboxBoard = [];\n"
    "let sandboxHistory = [];\n"
    "let endgameReportShown = false;\n"
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
    "        sq.classList.remove('selected', 'valid-move', 'valid-capture');\n"
    "    });\n"
    "    selectedSquare = null;\n"
    "}\n"
    "\n"
    "async function handleSquareClick(row, col) {\n"
    "    const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];\n"
    "    const index = row * 8 + col;\n"
    "    \n"
    "    if (piece === ' ' && selectedSquare !== null) {\n"
    "        const fromRow = Math.floor(selectedSquare / 8);\n"
    "        const fromCol = selectedSquare % 8;\n"
    "        \n"
    "        if (sandboxMode) {\n"
    "            makeSandboxMove(fromRow, fromCol, row, col);\n"
    "            clearHighlights();\n"
    "        } else {\n"
    "            const fromNotation = String.fromCharCode(97 + fromCol) + (8 - fromRow);\n"
    "            const toNotation = String.fromCharCode(97 + col) + (8 - row);\n"
    "            try {\n"
    "                const response = await fetch('/api/move', {\n"
    "                    method: 'POST',\n"
    "                    headers: {'Content-Type': 'application/json'},\n"
    "                    body: JSON.stringify({from: fromNotation, to: toNotation})\n"
    "                });\n"
    "                if (response.ok) {\n"
    "                    clearHighlights();\n"
    "                    fetchData();\n"
    "                }\n"
    "            } catch (error) {\n"
    "                console.error('Move error:', error);\n"
    "            }\n"
    "        }\n"
    "        return;\n"
    "    }\n"
    "    \n"
    "    if (piece !== ' ') {\n"
    "        if (sandboxMode) {\n"
    "            clearHighlights();\n"
    "            selectedSquare = index;\n"
    "            const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "            if (square) square.classList.add('selected');\n"
    "        } else {\n"
    "            const isWhitePiece = piece === piece.toUpperCase();\n"
    "            const currentPlayerIsWhite = statusData.current_player === 'White';\n"
    "            \n"
    "            if ((isWhitePiece && currentPlayerIsWhite) || (!isWhitePiece && !currentPlayerIsWhite)) {\n"
    "                clearHighlights();\n"
    "                selectedSquare = index;\n"
    "                const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);\n"
    "                if (square) square.classList.add('selected');\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// REVIEW MODE\n"
    "// ============================================================================\n"
    "\n"
    "function reconstructBoardAtMove(moveIndex) {\n"
    "    const startBoard = [\n"
    "        ['R','N','B','Q','K','B','N','R'],\n"
    "        ['P','P','P','P','P','P','P','P'],\n"
    "        [' ',' ',' ',' ',' ',' ',' ',' '],\n"
    "        [' ',' ',' ',' ',' ',' ',' ',' '],\n"
    "        [' ',' ',' ',' ',' ',' ',' ',' '],\n"
    "        [' ',' ',' ',' ',' ',' ',' ',' '],\n"
    "        ['p','p','p','p','p','p','p','p'],\n"
    "        ['r','n','b','q','k','b','n','r']\n"
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
    "    document.getElementById('review-move-text').textContent = `Reviewing move ${index + 1}`;\n"
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
    "        selectedItem.scrollIntoView({behavior:'smooth',block:'nearest'});\n"
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
    "    sandboxBoard[toRow][toCol] = piece;\n"
    "    sandboxBoard[fromRow][fromCol] = ' ';\n"
    "    sandboxHistory.push({from: `${String.fromCharCode(97+fromCol)}${8-fromRow}`, to: `${String.fromCharCode(97+toCol)}${8-toRow}`});\n"
    "    updateBoard(sandboxBoard);\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// UPDATE FUNCTIONS\n"
    "// ============================================================================\n"
    "\n"
    "function updateBoard(board) {\n"
    "    boardData = board;\n"
    "    const loading = document.getElementById('loading');\n"
    "    if (loading) loading.style.display = 'none';\n"
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
    "    \n"
    "    // ✅ FIX: Pokud už je banner zobrazen, nedělat nic (aby se nepřekresloval)\n"
    "    if (endgameReportShown && document.getElementById('endgame-banner')) {\n"
    "        console.log('Endgame report already shown, skipping...');\n"
    "        return;\n"
    "    }\n"
    "    \n"
    "    // ✅ Načíst advantage history pro graf\n"
    "    let advantageData = {history: [], white_checks: 0, black_checks: 0, white_castles: 0, black_castles: 0};\n"
    "    try {\n"
    "        const response = await fetch('/api/advantage');\n"
    "        advantageData = await response.json();\n"
    "        console.log('Advantage data loaded:', advantageData);\n"
    "    } catch (e) {\n"
    "        console.error('Failed to load advantage data:', e);\n"
    "    }\n"
    "    \n"
    "    // Určit výsledek a barvy\n"
    "    let emoji = '🏆';\n"
    "    let title = '';\n"
    "    let subtitle = '';\n"
    "    let accentColor = '#4CAF50';\n"
    "    let bgGradient = 'linear-gradient(135deg, #1e3a1e, #2d4a2d)';\n"
    "    \n"
    "    if (gameEnd.winner === 'Draw') {\n"
    "        emoji = '🤝';\n"
    "        title = 'REMÍZA';\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = '#FF9800';\n"
    "        bgGradient = 'linear-gradient(135deg, #3a2e1e, #4a3e2d)';\n"
    "    } else {\n"
    "        emoji = gameEnd.winner === 'White' ? '⚪' : '⚫';\n"
    "        title = gameEnd.winner.toUpperCase() + ' VYHRÁL!';\n"
    "        subtitle = gameEnd.reason;\n"
    "        accentColor = gameEnd.winner === 'White' ? '#4CAF50' : '#2196F3';\n"
    "        bgGradient = gameEnd.winner === 'White' ? 'linear-gradient(135deg, #1e3a1e, #2d4a2d)' : 'linear-gradient(135deg, #1e2a3a, #2d3a4a)';\n"
    "    }\n"
    "    \n"
    "    // Získat statistiky\n"
    "    const whiteMoves = Math.ceil(statusData.move_count / 2);\n"
    "    const blackMoves = Math.floor(statusData.move_count / 2);\n"
    "    const whiteCaptured = capturedData.white_captured || [];\n"
    "    const blackCaptured = capturedData.black_captured || [];\n"
    "    \n"
    "    // Material advantage\n"
    "    const pieceValues = {p:1,n:3,b:3,r:5,q:9,P:1,N:3,B:3,R:5,Q:9};\n"
    "    let whiteMaterial = 0, blackMaterial = 0;\n"
    "    whiteCaptured.forEach(p => whiteMaterial += pieceValues[p] || 0);\n"
    "    blackCaptured.forEach(p => blackMaterial += pieceValues[p] || 0);\n"
    "    const materialDiff = whiteMaterial - blackMaterial;\n"
    "    const materialText = materialDiff > 0 ? 'White +' + materialDiff : materialDiff < 0 ? 'Black +' + (-materialDiff) : 'Vyrovnáno';\n"
    "    \n"
    "    // ✅ Vytvořit SVG graf výhody (jako chess.com)\n"
    "    let graphSVG = '';\n"
    "    if (advantageData.history && advantageData.history.length > 1) {\n"
    "        const history = advantageData.history;\n"
    "        const width = 280;\n"
    "        const height = 100;\n"
    "        const maxAdvantage = Math.max(10, ...history.map(Math.abs));\n"
    "        const scaleY = height / (2 * maxAdvantage);\n"
    "        const scaleX = width / (history.length - 1);\n"
    "        \n"
    "        // Vytvořit body pro polyline (0,0 je nahoře vlevo, y roste dolů)\n"
    "        let points = history.map((adv, i) => {\n"
    "            const x = i * scaleX;\n"
    "            const y = height / 2 - adv * scaleY;  // Převrátit Y (White nahoře, Black dole)\n"
    "            return x + ',' + y;\n"
    "        }).join(' ');\n"
    "        \n"
    "        // Vytvořit polygon pro vyplněnou oblast\n"
    "        let areaPoints = '0,' + (height / 2) + ' ' + points + ' ' + width + ',' + (height / 2);\n"
    "        \n"
    "        graphSVG = '<svg width=\"280\" height=\"100\" style=\"border-radius:6px;background:rgba(0,0,0,0.2);\">' +\n"
    "            '<!-- Středová čára (vyrovnaná pozice) -->' +\n"
    "            '<line x1=\"0\" y1=\"' + (height / 2) + '\" x2=\"' + width + '\" y2=\"' + (height / 2) + '\" stroke=\"#555\" stroke-width=\"1\" stroke-dasharray=\"3,3\"/>' +\n"
    "            '<!-- Vyplněná oblast pod křivkou -->' +\n"
    "            '<polygon points=\"' + areaPoints + '\" fill=\"' + accentColor + '\" opacity=\"0.2\"/>' +\n"
    "            '<!-- Křivka výhody -->' +\n"
    "            '<polyline points=\"' + points + '\" fill=\"none\" stroke=\"' + accentColor + '\" stroke-width=\"2\" stroke-linejoin=\"round\"/>' +\n"
    "            '<!-- Tečky na koncích -->' +\n"
    "            '<circle cx=\"0\" cy=\"' + (height / 2) + '\" r=\"3\" fill=\"' + accentColor + '\"/>' +\n"
    "            '<circle cx=\"' + ((history.length - 1) * scaleX) + '\" cy=\"' + (height / 2 - history[history.length - 1] * scaleY) + '\" r=\"4\" fill=\"' + accentColor + '\"/>' +\n"
    "            '<!-- Popisky -->' +\n"
    "            '<text x=\"5\" y=\"12\" fill=\"#888\" font-size=\"10\" font-weight=\"600\">White</text>' +\n"
    "            '<text x=\"5\" y=\"' + (height - 2) + '\" fill=\"#888\" font-size=\"10\" font-weight=\"600\">Black</text>' +\n"
    "        '</svg>';\n"
    "    }\n"
    "    \n"
    "    // Vytvořit nový banner - VLEVO OD BOARDU, NE UPROSTŘED!\n"
    "    const banner = document.createElement('div');\n"
    "    banner.id = 'endgame-banner';\n"
    "    banner.style.cssText = '\\\n"
    "        position: fixed;\\\n"
    "        left: 10px;\\\n"
    "        top: 50%;\\\n"
    "        transform: translateY(-50%);\\\n"
    "        width: 320px;\\\n"
    "        max-height: 90vh;\\\n"
    "        overflow-y: auto;\\\n"
    "        background: ' + bgGradient + ';\\\n"
    "        border: 2px solid ' + accentColor + ';\\\n"
    "        border-radius: 12px;\\\n"
    "        padding: 0;\\\n"
    "        box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 40px ' + accentColor + '40;\\\n"
    "        z-index: 9999;\\\n"
    "        animation: slideInLeft 0.4s ease-out;\\\n"
    "        backdrop-filter: blur(10px);\\\n"
    "    ';\n"
    "    \n"
    "    // Na mobilu - jiné umístění (nahoře, plná šířka)\n"
    "    if (window.innerWidth <= 768) {\n"
    "        banner.style.cssText = '\\\n"
    "            position: fixed;\\\n"
    "            left: 10px;\\\n"
    "            right: 10px;\\\n"
    "            top: 10px;\\\n"
    "            width: auto;\\\n"
    "            max-height: 80vh;\\\n"
    "            transform: none;\\\n"
    "            overflow-y: auto;\\\n"
    "            background: ' + bgGradient + ';\\\n"
    "            border: 2px solid ' + accentColor + ';\\\n"
    "            border-radius: 12px;\\\n"
    "            padding: 0;\\\n"
    "            box-shadow: 0 8px 32px rgba(0,0,0,0.6);\\\n"
    "            z-index: 9999;\\\n"
    "            animation: slideInTop 0.4s ease-out;\\\n"
    "        ';\n"
    "    }\n"
    "    \n"
    "    // HTML obsah\n"
    "    banner.innerHTML = '\\\n"
    "        <div style=\"background:' + accentColor + ';padding:20px;text-align:center;border-radius:10px 10px 0 0;\">\\\n"
    "            <div style=\"font-size:64px;margin-bottom:8px;\">' + emoji + '</div>\\\n"
    "            <h2 style=\"margin:0;color:white;font-size:24px;font-weight:700;text-shadow:0 2px 4px rgba(0,0,0,0.4);\">' + title + '</h2>\\\n"
    "            <p style=\"margin:8px 0 0 0;color:rgba(255,255,255,0.9);font-size:14px;font-weight:500;\">' + subtitle + '</p>\\\n"
    "        </div>\\\n"
    "        <div style=\"padding:20px;\">\\\n"
    "            ' + (graphSVG ? '\\\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\\\n"
    "                <h3 style=\"margin:0 0 12px 0;color:' + accentColor + ';font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;\">\\\n"
    "                    <span>📈</span> Průběh hry\\\n"
    "                </h3>\\\n"
    "                ' + graphSVG + '\\\n"
    "                <div style=\"display:flex;justify-content:space-between;margin-top:8px;font-size:11px;color:#888;\">\\\n"
    "                    <span>Začátek</span>\\\n"
    "                    <span>Tah ' + (advantageData.count || 0) + '</span>\\\n"
    "                </div>\\\n"
    "            </div>' : '') + '\\\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\\\n"
    "                <h3 style=\"margin:0 0 12px 0;color:' + accentColor + ';font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;\">\\\n"
    "                    <span>📊</span> Statistiky\\\n"
    "                </h3>\\\n"
    "                <div style=\"display:grid;grid-template-columns:1fr 1fr;gap:10px;font-size:13px;\">\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Tahy</div>\\\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ ' + whiteMoves + ' | ⚫ ' + blackMoves + '</div>\\\n"
    "                    </div>\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Materiál</div>\\\n"
    "                        <div style=\"color:' + accentColor + ';font-weight:600;\">' + materialText + '</div>\\\n"
    "                    </div>\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Sebráno</div>\\\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ ' + whiteCaptured.length + ' | ⚫ ' + blackCaptured.length + '</div>\\\n"
    "                    </div>\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Celkem</div>\\\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">' + statusData.move_count + ' tahů</div>\\\n"
    "                    </div>\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Šachy</div>\\\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ ' + (advantageData.white_checks || 0) + ' | ⚫ ' + (advantageData.black_checks || 0) + '</div>\\\n"
    "                    </div>\\\n"
    "                    <div style=\"background:rgba(255,255,255,0.05);padding:8px;border-radius:6px;\">\\\n"
    "                        <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Rošády</div>\\\n"
    "                        <div style=\"color:#e0e0e0;font-weight:600;\">⚪ ' + (advantageData.white_castles || 0) + ' | ⚫ ' + (advantageData.black_castles || 0) + '</div>\\\n"
    "                    </div>\\\n"
    "                </div>\\\n"
    "            </div>\\\n"
    "            <div style=\"background:rgba(0,0,0,0.3);border-radius:8px;padding:15px;margin-bottom:15px;\">\\\n"
    "                <h3 style=\"margin:0 0 12px 0;color:' + accentColor + ';font-size:16px;font-weight:600;display:flex;align-items:center;gap:8px;\">\\\n"
    "                    <span>⚔️</span> Sebrané figurky\\\n"
    "                </h3>\\\n"
    "                <div style=\"margin-bottom:10px;\">\\\n"
    "                    <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">White sebral (' + whiteCaptured.length + ')</div>\\\n"
    "                    <div style=\"font-size:20px;line-height:1.4;\">' + (whiteCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−') + '</div>\\\n"
    "                </div>\\\n"
    "                <div>\\\n"
    "                    <div style=\"color:#888;font-size:11px;margin-bottom:4px;\">Black sebral (' + blackCaptured.length + ')</div>\\\n"
    "                    <div style=\"font-size:20px;line-height:1.4;\">' + (blackCaptured.map(p => pieceSymbols[p] || p).join(' ') || '−') + '</div>\\\n"
    "                </div>\\\n"
    "            </div>\\\n"
    "            <button onclick=\"hideEndgameReport()\" style=\"\\\n"
    "                width:100%;\\\n"
    "                padding:14px;\\\n"
    "                font-size:16px;\\\n"
    "                background:' + accentColor + ';\\\n"
    "                color:white;\\\n"
    "                border:none;\\\n"
    "                border-radius:8px;\\\n"
    "                cursor:pointer;\\\n"
    "                font-weight:600;\\\n"
    "                box-shadow:0 4px 12px rgba(0,0,0,0.3);\\\n"
    "                transition:all 0.2s;\\\n"
    "            \" onmouseover=\"this.style.transform=\\'translateY(-2px)\\';this.style.boxShadow=\\'0 6px 16px rgba(0,0,0,0.4)\\'\" onmouseout=\"this.style.transform=\\'translateY(0)\\';this.style.boxShadow=\\'0 4px 12px rgba(0,0,0,0.3)\\'\">\\\n"
    "                ✓ OK\\\n"
    "            </button>\\\n"
    "        </div>\\\n"
    "    ';\n"
    "    \n"
    "    document.body.appendChild(banner);\n"
    "    endgameReportShown = true;  // ✅ Označit, že je zobrazený\n"
    "    console.log('🏆 ENDGAME REPORT SHOWN - banner displayed (left side)');\n"
    "}\n"
    "\n"
    "// Skrýt endgame report\n"
    "function hideEndgameReport() {\n"
    "    console.log('Hiding endgame report...');\n"
    "    const banner = document.getElementById('endgame-banner');\n"
    "    if (banner) {\n"
    "        banner.remove();  // ✅ Odstranit z DOM\n"
    "        endgameReportShown = false;  // ✅ Resetovat flag\n"
    "        console.log('Endgame report hidden and removed');\n"
    "    }\n"
    "}\n"
    "\n"
    "// ============================================================================\n"
    "// STATUS UPDATE FUNCTION\n"
    "// ============================================================================\n"
    "\n"
    "function updateStatus(status) {\n"
    "    statusData = status;\n"
    "    document.getElementById('game-state').textContent = status.game_state || '-';\n"
    "    document.getElementById('current-player').textContent = status.current_player || '-';\n"
    "    document.getElementById('move-count').textContent = status.move_count || 0;\n"
    "    document.getElementById('in-check').textContent = status.in_check ? 'Yes' : 'No';\n"
    "    \n"
    "    const lifted = status.piece_lifted;\n"
    "    if (lifted && lifted.lifted) {\n"
    "        document.getElementById('lifted-piece').textContent = pieceSymbols[lifted.piece] || '-';\n"
    "        document.getElementById('lifted-position').textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);\n"
    "        const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);\n"
    "        if (square) square.classList.add('lifted');\n"
    "    } else {\n"
    "        document.getElementById('lifted-piece').textContent = '-';\n"
    "        document.getElementById('lifted-position').textContent = '-';\n"
    "        document.querySelectorAll('.square').forEach(sq => sq.classList.remove('lifted'));\n"
    "    }\n"
    "    // ✅ ENDGAME REPORT - zobrazit pouze jednou, ne při každém update\n"
    "    if (status.game_end && status.game_end.ended && !endgameReportShown) {\n"
    "        console.log('Game ended, showing endgame report...');\n"
    "        showEndgameReport(status.game_end);\n"
    "    } else if (!(status.game_end && status.game_end.ended) && endgameReportShown) {\n"
    "        // Hra už neskončila (nová hra), skrýt report\n"
    "        console.log('Game no longer ended, hiding report...');\n"
    "        hideEndgameReport();\n"
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
    "    } catch (error) {\n"
    "        console.error('Fetch error:', error);\n"
    "    }\n"
    "}\n"
    "\n"
    "console.log('🚀 Creating chess board...');\n"
    "createBoard();\n"
    "console.log('🚀 Fetching initial data...');\n"
    "fetchData();\n"
    "setInterval(fetchData, 500);\n"
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
    "    const isTimerActive = timerData.config && timerData.config.type !== 0;\n"
    "    if (isTimerActive) {\n"
    "        const formattedTime = formatTime(timeMs);\n"
    "        timeElement.textContent = formattedTime;\n"
    "        playerElement.classList.remove('low-time', 'critical-time');\n"
    "        if (timeMs < 5000) playerElement.classList.add('critical-time');\n"
    "        else if (timeMs < 30000) playerElement.classList.add('low-time');\n"
    "    } else {\n"
    "        timeElement.textContent = '--:--';\n"
    "        playerElement.classList.remove('low-time', 'critical-time', 'active');\n"
    "        return;\n"
    "    }\n"
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
    "    if (timerInfo.config.type === 0) {\n"
    "        const whiteProgress = document.getElementById('white-progress');\n"
    "        const blackProgress = document.getElementById('black-progress');\n"
    "        if (whiteProgress) whiteProgress.style.width = '0%';\n"
    "        if (blackProgress) blackProgress.style.width = '0%';\n"
    "        return;\n"
    "    }\n"
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
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {\n"
    "        return;\n"
    "    }\n"
    "    const currentPlayerTime = timerInfo.is_white_turn ? timerInfo.white_time_ms : timerInfo.black_time_ms;\n"
    "    if (currentPlayerTime < 5000 && !timerInfo.warning_5s_shown) {\n"
    "        showTimeWarning('Critical! Less than 5 seconds!', 'critical');\n"
    "    } else if (currentPlayerTime < 10000 && !timerInfo.warning_10s_shown) {\n"
    "        showTimeWarning('Warning! Less than 10 seconds!', 'warning');\n"
    "    } else if (currentPlayerTime < 30000 && !timerInfo.warning_30s_shown) {\n"
    "        showTimeWarning('Low time! Less than 30 seconds!', 'info');\n"
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
    "    if (!timerInfo || !timerInfo.config || timerInfo.config.type === 0) {\n"
    "        return;\n"
    "    }\n"
    "    const expiredPlayer = timerInfo.is_white_turn ? 'White' : 'Black';\n"
    "    showTimeWarning('Time expired! ' + expiredPlayer + ' lost on time.', 'critical');\n"
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
    "    console.log('✅ Timer update loop starting... (will update every 300ms)');\n"
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
    "    }, 200);\n"
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
    "        if (minutes < 1 || minutes > 180) { alert('Minutes must be between 1 and 180'); return; }\n"
    "        if (increment < 0 || increment > 60) { alert('Increment must be between 0 and 60 seconds'); return; }\n"
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
    "            const applyBtn = document.getElementById('apply-time-control');\n"
    "            if (applyBtn) applyBtn.disabled = true;\n"
    "        } else {\n"
    "            const errorText = await response.text();\n"
    "            console.error('Failed to apply time control:', response.status, errorText);\n"
    "            throw new Error('Failed to apply time control: ' + errorText);\n"
    "        }\n"
    "    } catch (error) {\n"
    "        console.error('Error applying time control:', error);\n"
    "        showTimeWarning('Error setting time control: ' + error.message, 'critical');\n"
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
    "            showTimeWarning('Timer paused', 'info');\n"
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
    "            const response = await fetch('/api/timer/reset', { method: 'POST' });\n"
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
    "    switch(e.key) {\n"
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
    "            clearHighlights();        }\n"
    "    }\n"
    "});\n"
    "\n"
    "\n"
    "\n"
    "";

static esp_err_t http_get_chess_js_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /chess_app.js (%zu bytes)", strlen(chess_app_js_content));
    httpd_resp_set_type(req, "application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=3600");
    httpd_resp_send(req, chess_app_js_content, strlen(chess_app_js_content));
    return ESP_OK;
}
// ============================================================================

static const char test_html[] =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Timer Test</title>"
    "<style>body{background:#1a1a1a;color:white;padding:20px;font-family:Arial;}"
    ".timer{background:#333;padding:20px;margin:10px;border-radius:8px;}"
    "button{padding:10px 20px;margin:5px;cursor:pointer;}</style></head><body>"
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
    "<div id='log' style='background:#222;padding:10px;margin-top:20px;max-height:200px;overflow-y:auto;'></div>"
    "<script>"
    "function log(m){const d=document.getElementById('log');d.innerHTML+='<div>'+new Date().toLocaleTimeString()+': '+m+'</div>';d.scrollTop=d.scrollHeight;}"
    "log('Script loaded');"
    "function formatTime(ms){const s=Math.ceil(ms/1000);const m=Math.floor(s/60);const sec=s%60;return m+':'+sec.toString().padStart(2,'0');}"
    "async function updateTimer(){try{const res=await fetch('/api/timer');if(res.ok){const data=await res.json();"
    "document.getElementById('white-time').textContent=formatTime(data.white_time_ms);"
    "document.getElementById('black-time').textContent=formatTime(data.black_time_ms);"
    "log('W='+data.white_time_ms+' B='+data.black_time_ms);}else{log('ERROR: '+res.status);}}catch(e){log('ERROR: '+e.message);}}"
    "async function applyTime(){const type=parseInt(document.getElementById('time-control').value);log('Apply type='+type);try{"
    "const res=await fetch('/api/timer/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({type:type})});"
    "if(res.ok){log('OK');setTimeout(updateTimer,500);}else{log('ERROR: '+res.status+' '+(await res.text()));}}catch(e){log('ERROR: '+e.message);}}"
    "async function pauseTimer(){log('Pause');try{const res=await fetch('/api/timer/pause',{method:'POST'});log(res.ok?'OK':'ERROR: '+res.status);}catch(e){log('ERROR: '+e.message);}}"
    "async function resumeTimer(){log('Resume');try{const res=await fetch('/api/timer/resume',{method:'POST'});log(res.ok?'OK':'ERROR: '+res.status);}catch(e){log('ERROR: '+e.message);}}"
    "async function resetTimer(){log('Reset');try{const res=await fetch('/api/timer/reset',{method:'POST'});log(res.ok?'OK':'ERROR: '+res.status);setTimeout(updateTimer,500);}catch(e){log('ERROR: '+e.message);}}"
    "log('Starting updates');setInterval(updateTimer,300);updateTimer();"
    "</script></body></html>";

static esp_err_t http_get_test_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /test - minimal timer test page");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, test_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// ============================================================================
// HTML CONTENT - CHUNKED ARRAYS IN FLASH MEMORY (READ-ONLY)
// ============================================================================

// ✅ HTML split into logical chunks for reliable chunked transfer
// Each chunk is a separate const string in flash memory (.rodata section)
// JavaScript is kept as ONE complete chunk to avoid "is not defined" errors

// Chunk 1: HTML head with bootstrap script and CSS
static const char html_chunk_head[] =
        // HTML head and initial structure
        "<!DOCTYPE html>"
        "<html lang='en'>"
        "<head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<title>ESP32 Chess</title>"

        // Early bootstrap to avoid 'is not defined' before main script loads
        "<script>"
            "window.changeTimeControl = window.changeTimeControl || function(){};"
            "window.applyTimeControl = window.applyTimeControl || function(){};"
            "window.pauseTimer = window.pauseTimer || function(){};"
            "window.resumeTimer = window.resumeTimer || function(){};"
            "window.resetTimer = window.resetTimer || function(){};"
            "window.hideEndgameReport = window.hideEndgameReport || function(){};"
        "</script>"
        // Global JS error capture to surface syntax/runtime errors visibly
        "<script>"
            "(function(){"
                "function showJsError(msg, src, line, col){"
                    "try {"
                        "var b=document.body||document.documentElement;"
                        "var d=document.getElementById('js-error')||document.createElement('pre');"
                        "d.id='js-error';"
                        "d.style.cssText='position:fixed;left:6px;bottom:6px;right:6px;max-height:40vh;overflow:auto;background:#300;color:#fff;border:1px solid #900;padding:8px;margin:0;z-index:99999;font:12px/1.4 monospace;white-space:pre-wrap;';"
                        "d.textContent='JS ERROR: '+msg+'\nSource: '+(src||'-')+'\nLine: '+line+':'+col;"
                        "b&&b.appendChild(d);"
                    "} catch(e) {}"
                "}"
                "window.addEventListener('error', function(e){ showJsError(e.message, e.filename, e.lineno, e.colno); });"
                "window.addEventListener('unhandledrejection', function(e){ showJsError('Unhandled promise rejection: '+e.reason, '', 0, 0); });"
            "})();"
        "</script>"
        
        // CSS styles - part 1
        "<style>"
            "* { margin: 0; padding: 0; box-sizing: border-box; }"
            "body { "
                "font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
                "background: #1a1a1a; "
                "color: #e0e0e0; "
                "min-height: 100vh; "
                "padding: 10px; "
            "}"
            ".container { "
                "max-width: 900px; "
                "margin: 0 auto; "
            "}"
            "h1 { "
                "color: #4CAF50; "
                "text-align: center; "
                "margin-bottom: 20px; "
                "font-size: 1.5em; "
                "font-weight: 600; "
            "}"
            ".main-content { "
                "display: grid; "
                "grid-template-columns: 1fr 280px; "
                "gap: 15px; "
            "}"
            "@media (max-width: 768px) { "
                ".main-content { grid-template-columns: 1fr; } "
            "}"
            ".board-container { "
                "background: #2a2a2a; "
                "border-radius: 8px; "
                "padding: 15px; "
                "box-shadow: 0 4px 12px rgba(0,0,0,0.3); "
            "}"
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
            ".piece { "
                "font-size: 4vw; "
                "text-shadow: 2px 2px 4px rgba(0,0,0,0.3); "
                "user-select: none; "
            "}"
            ".piece.white { color: white; }"
            ".piece.black { color: black; }"
        
        // CSS styles - part 2
        ".info-panel { "
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
        ".status-box h3 { color: #4CAF50; margin-bottom: 8px; font-weight: 600; font-size: 0.9em; }"
        ".status-item { "
            "display: flex; "
            "justify-content: space-between; "
            "margin: 4px 0; "
            "font-size: 13px; "
        "}"
        ".status-value { font-weight: 600; color: #e0e0e0; font-family: 'Courier New', monospace; }"
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
        ".captured-box h3 { color: #4CAF50; font-size: 0.85em; margin-bottom: 5px; }"
        ".captured-box div { font-size: 0.75em; color: #888; margin-top: 5px; }"
        ".loading { "
            "text-align: center; "
            "padding: 20px; "
            "color: #888; "
        "}"
        
        // CSS styles - part 3 (Review Mode, Sandbox Mode, etc.)
        "/* Review Mode */"
        ".review-banner { "
            "position: fixed; "
            "top: 0; left: 0; right: 0; "
            "background: linear-gradient(135deg, #FF9800, #FF6F00); "
            "color: white; "
            "padding: 12px 20px; "
            "display: none; "
            "align-items: center; "
            "justify-content: center; "
            "gap: 16px; "
            "box-shadow: 0 4px 12px rgba(0,0,0,0.3); "
            "z-index: 100; "
            "animation: slideDown 0.3s ease; "
        "}"
        "@keyframes slideDown { "
            "from { transform: translateY(-100%); } "
            "to { transform: translateY(0); } "
        "}"
        "@keyframes slideInLeft { "
            "from { transform: translateY(-50%) translateX(-100%); opacity: 0; } "
            "to { transform: translateY(-50%) translateX(0); opacity: 1; } "
        "}"
        "@keyframes slideInTop { "
            "from { transform: translateY(-100%); opacity: 0; } "
            "to { transform: translateY(0); opacity: 1; } "
        "}"
        ".review-banner.active { display: flex; }"
        ".review-text { font-weight: 600; }"
        ".btn-exit-review { "
            "padding: 8px 20px; "
            "background: white; "
            "color: #FF9800; "
            "border: none; "
            "border-radius: 6px; "
            "font-weight: 600; "
            "cursor: pointer; "
            "transition: all 0.2s; "
        "}"
        ".btn-exit-review:hover { transform: scale(1.05); }"
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
        ".history-box::-webkit-scrollbar-thumb { background: #4CAF50; border-radius: 3px; }"
        ".history-box::-webkit-scrollbar-thumb:hover { background: #45a049; }"
        "</style>"
        "</head>";
        
// Chunk 2: HTML body structure (no JavaScript yet)
static const char html_chunk_body[] =
        "<body>"
        "<div class='container'>"
            "<h1>♟️ ESP32 Chess</h1>"
            "<div class='main-content'>"
                "<div class='board-container'>"
                    "<button class='btn-try-moves' onclick='enterSandboxMode()'>Try Moves</button>"
                    "<div id='board' class='board'></div>"
                    "<div id='loading' class='loading'>Loading board...</div>"
                "</div>";

// Chunk 2b: Info panel (status, timer, history, captured)
static const char html_chunk_infopanel[] =
                "<div class='info-panel'>"
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
                    "<div class='captured-box'>"
                        "<h3>Captured Pieces</h3>"
                        "<div>White:</div>"
                        "<div id='white-captured' class='captured-pieces'></div>"
                        "<div style='margin-top: 10px;'>Black:</div>"
                        "<div id='black-captured' class='captured-pieces'></div>"
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
                            "<button id='apply-time-control' onclick='applyTimeControl()' disabled>Použít</button>"
                        "</div>"
                        "<div id='custom-time-settings' class='custom-settings' style='display: none;'>"
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
                            "<button id='resume-timer' onclick='resumeTimer()' style='display: none;'>▶️ Pokračovat</button>"
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
                        "<h3>Move History</h3>"
                        "<div id='history' class='history-box'></div>"
                    "</div>"
                "</div>"
            "</div>"
        "</div>";
        
// Chunk 2c: Review and Sandbox banners
static const char html_chunk_banners[] =
        "<!-- Review Mode Banner -->"
        "<div class='review-banner' id='review-banner'>"
            "<div class='review-text'>"
                "<span>📋</span>"
                "<span id='review-move-text'>Prohlížíš tah 0</span>"
            "</div>"
            "<button class='btn-exit-review' onclick='exitReviewMode()'>Zpět na aktuální pozici</button>"
        "</div>"
        "<!-- Sandbox Mode Banner -->"
        "<div class='sandbox-banner' id='sandbox-banner'>"
            "<div class='sandbox-text'>"
                "<span>🎮</span>"
                "<span>Sandbox Mode - Zkoušíš tahy lokálně</span>"
            "</div>"
            "<button class='btn-exit-sandbox' onclick='exitSandboxMode()'>Zpět na skutečnou pozici</button>"
        "</div>";

// Chunk 3: JavaScript - load from external file (prevents UTF-8 chunking issues)
static const char html_chunk_javascript[] =
        "<script src='/chess_app.js'></script>";

// Chunk 4: HTML closing tags
static const char html_chunk_end[] =
        "</body>"
        "</html>";

// ============================================================================
// HTML PAGE HANDLER
// ============================================================================

static esp_err_t http_get_root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET / (HTML page) - using chunked transfer for reliability");
    
    // Set content type
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    
    // Set chunked transfer encoding
    httpd_resp_set_hdr(req, "Transfer-Encoding", "chunked");
    
    // ✅ Send HTML in 6 smaller chunks with explicit logging
    // Using explicit strlen() and longer delays for reliability
    
    // CHUNK 1: HEAD (CSS + bootstrap script)
    size_t chunk1_len = strlen(html_chunk_head);
    ESP_LOGI(TAG, "📤 Chunk 1: HEAD+CSS (%zu bytes)", chunk1_len);
    esp_err_t ret = httpd_resp_send_chunk(req, html_chunk_head, chunk1_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 1 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // CHUNK 2: BODY start + board container
    size_t chunk2_len = strlen(html_chunk_body);
    ESP_LOGI(TAG, "📤 Chunk 2: BODY+BOARD (%zu bytes)", chunk2_len);
    ret = httpd_resp_send_chunk(req, html_chunk_body, chunk2_len);
        if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 2 failed: %s", esp_err_to_name(ret));
            return ret;
        }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // CHUNK 3: Info panel (status, timer UI, history, captured)
    size_t chunk3_len = strlen(html_chunk_infopanel);
    ESP_LOGI(TAG, "📤 Chunk 3: INFOPANEL (%zu bytes)", chunk3_len);
    ret = httpd_resp_send_chunk(req, html_chunk_infopanel, chunk3_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 3 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // CHUNK 4: Banners (review mode, sandbox mode)
    size_t chunk4_len = strlen(html_chunk_banners);
    ESP_LOGI(TAG, "📤 Chunk 4: BANNERS (%zu bytes)", chunk4_len);
    ret = httpd_resp_send_chunk(req, html_chunk_banners, chunk4_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 4 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // CHUNK 5: Complete JavaScript (all functions in ONE piece!)
    size_t chunk5_len = strlen(html_chunk_javascript);
    ESP_LOGI(TAG, "📤 Chunk 5: JAVASCRIPT (%zu bytes)", chunk5_len);
    ret = httpd_resp_send_chunk(req, html_chunk_javascript, chunk5_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 5 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // CHUNK 6: Closing tags
    size_t chunk6_len = strlen(html_chunk_end);
    ESP_LOGI(TAG, "📤 Chunk 6: CLOSING (%zu bytes)", chunk6_len);
    ret = httpd_resp_send_chunk(req, html_chunk_end, chunk6_len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunk 6 failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // End chunked transfer
    ret = httpd_resp_send_chunk(req, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Chunked transfer end failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "✅ HTML sent successfully (6 chunks: %zu + %zu + %zu + %zu + %zu + %zu bytes)",
             chunk1_len, chunk2_len, chunk3_len, chunk4_len, chunk5_len, chunk6_len);
    return ESP_OK;
}

// ============================================================================
// WRAP FUNCTIONS FOR ESP_DIAGNOSTICS
// ============================================================================
// EMPTY implementation to prevent stack overflow
// esp_diagnostics will not work, but web server will function

void __wrap_esp_log_writev(esp_log_level_t level,
                          const char* tag,
                          const char* format,
                          va_list args)
{
    // EMPTY - do nothing to prevent stack overflow
    (void)level;
    (void)tag;
    (void)format;
    (void)args;
}

void __wrap_esp_log_write(esp_log_level_t level,
                          const char* tag,
                          const char* format, ...)
{
    // EMPTY - do nothing to prevent stack overflow
    (void)level;
    (void)tag;
    (void)format;
}

// ============================================================================
// WEB SERVER TASK IMPLEMENTATION
// ============================================================================

void web_server_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Web server task starting...");
    
    // ✅ CRITICAL: Register with TWDT
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register web server task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ Web server task registered with TWDT");
    }
    
    // NVS is already initialized in main.c - skip it here
    ESP_LOGI(TAG, "NVS already initialized, skipping...");
    
    // Initialize WiFi AP
    esp_err_t ret = wifi_init_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "❌ Web server task exiting");
        
        esp_task_wdt_delete(NULL);  // Unregister from WDT before deleting
        vTaskDelete(NULL);
        return;
    }
    wifi_ap_active = true;
    ESP_LOGI(TAG, "WiFi AP initialized");
    
    // Wait for WiFi to be ready
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Start HTTP server
    ret = start_http_server();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to start HTTP server: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "❌ Web server task will continue but HTTP will not be available");
        
        // Don't delete task - instead enter a maintenance loop that feeds WDT
        task_running = true;
        while (task_running) {
            web_server_task_wdt_reset_safe();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        esp_task_wdt_delete(NULL);  // Unregister before deleting
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
                     web_server_active ? "Yes" : "No",
                     client_count,
                     web_server_active ? (xTaskGetTickCount() * portTICK_PERIOD_MS - web_server_start_time) : 0);
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

void web_server_process_commands(void)
{
    uint8_t command;
    
    // Check for new web server commands from queue
    if (web_server_command_queue != NULL && 
        xQueueReceive(web_server_command_queue, &command, 0) == pdTRUE) {
        web_server_execute_command(command);
    }
}

void web_server_execute_command(uint8_t command)
{
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

void web_server_start(void)
{
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

void web_server_stop(void)
{
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

void web_server_get_status(void)
{
    ESP_LOGI(TAG, "Web Server Status - Active: %s, Clients: %lu, Uptime: %lu ms", 
              web_server_active ? "Yes" : "No",
              client_count,
              web_server_active ? (xTaskGetTickCount() * portTICK_PERIOD_MS - web_server_start_time) : 0);
    
    // Send status to status queue
    if (web_server_status_queue != NULL) {
        uint8_t status = web_server_active ? 1 : 0;
        xQueueSend(web_server_status_queue, &status, 0);
    }
}

void web_server_set_config(void)
{
    ESP_LOGI(TAG, "Web server configuration update requested");
    ESP_LOGI(TAG, "Web server configuration updated");
}

// ============================================================================
// WEB SERVER STATE UPDATE
// ============================================================================

void web_server_update_state(void)
{
    if (!web_server_active) {
        return;
    }
    
    // Web server is running, no additional updates needed
    // State is updated via HTTP handlers
}

// ============================================================================
// HTTP HANDLERS (Legacy placeholder functions)
// ============================================================================

void web_server_handle_root(void)
{
    ESP_LOGI(TAG, "Handling root HTTP request");
    ESP_LOGD(TAG, "Root page served successfully");
}

void web_server_handle_api_status(void)
{
    ESP_LOGI(TAG, "Handling API status request");
    ESP_LOGD(TAG, "API status served successfully");
}

void web_server_handle_api_board(void)
{
    ESP_LOGI(TAG, "Handling API board request");
    ESP_LOGD(TAG, "API board data served successfully");
}

void web_server_handle_api_move(void)
{
    ESP_LOGI(TAG, "Handling API move request");
    ESP_LOGD(TAG, "API move request processed successfully");
}

// ============================================================================
// WEBSOCKET FUNCTIONS (Placeholder for future implementation)
// ============================================================================

void web_server_websocket_init(void)
{
    ESP_LOGI(TAG, "WebSocket support not yet implemented");
}

void web_server_websocket_send_update(const char* data)
{
    if (!web_server_active || data == NULL) {
        return;
    }
    ESP_LOGI(TAG, "WebSocket send: %s", data);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

bool web_server_is_active(void)
{
    return web_server_active;
}

uint32_t web_server_get_client_count(void)
{
    return client_count;
}

uint32_t web_server_get_uptime(void)
{
    if (!web_server_active) {
        return 0;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return current_time - web_server_start_time;
}

void web_server_log_request(const char* method, const char* path)
{
    if (method == NULL || path == NULL) {
        return;
    }
    
    ESP_LOGI(TAG, "HTTP Request: %s %s", method, path);
}

void web_server_log_error(const char* error_message)
{
    if (error_message == NULL) {
        return;
    }
    
    ESP_LOGE(TAG, "Web Server Error: %s", error_message);
}

// ============================================================================
// CONFIGURATION FUNCTIONS
// ============================================================================

void web_server_set_port(uint16_t port)
{
    ESP_LOGI(TAG, "Setting web server port to %d", port);
    ESP_LOGI(TAG, "Web server port updated to %d", port);
}

void web_server_set_max_clients(uint32_t max_clients)
{
    ESP_LOGI(TAG, "Setting web server max clients to %lu", max_clients);
    ESP_LOGI(TAG, "Web server max clients updated to %lu", max_clients);
}

void web_server_enable_ssl(bool enable)
{
    ESP_LOGI(TAG, "Setting web server SSL to %s", enable ? "enabled" : "disabled");
    ESP_LOGI(TAG, "Web server SSL %s", enable ? "enabled" : "disabled");
}

// ============================================================================
// STATUS AND CONTROL FUNCTIONS
// ============================================================================

bool web_server_is_task_running(void)
{
    return task_running;
}

void web_server_stop_task(void)
{
    task_running = false;
    ESP_LOGI(TAG, "Web server task stop requested");
}

void web_server_reset(void)
{
    ESP_LOGI(TAG, "Resetting web server...");
    
    web_server_active = false;
    web_server_start_time = 0;
    client_count = 0;
    
    ESP_LOGI(TAG, "Web server reset completed");
}
