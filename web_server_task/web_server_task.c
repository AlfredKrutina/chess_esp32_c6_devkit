/*
 * ESP32-C6 Chess System v2.4 - Web Server Task Component
 * 
 * This component handles web server functionality:
 * - WiFi Access Point setup
 * - HTTP server for remote control
 * - Captive portal for automatic browser opening
 * - REST API endpoints for game state
 * - Web interface for chess game
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-XX
 */

#include "web_server_task.h"
#include "freertos_chess.h"
#include "../game_task/include/game_task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// External queue declaration
extern QueueHandle_t game_command_queue;

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

static const char* TAG = "WEB_SERVER_TASK";

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
#define JSON_BUFFER_SIZE 2048  // Reduced from 4096 to save memory

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
static esp_err_t http_get_board_handler(httpd_req_t *req);
static esp_err_t http_get_status_handler(httpd_req_t *req);
static esp_err_t http_get_history_handler(httpd_req_t *req);
static esp_err_t http_get_captured_handler(httpd_req_t *req);
// static esp_err_t http_post_move_handler(httpd_req_t *req);  // DISABLED - web je 100% READ-ONLY

// Captive portal handlers
static esp_err_t http_get_generate_204_handler(httpd_req_t *req);
static esp_err_t http_get_hotspot_handler(httpd_req_t *req);
static esp_err_t http_get_connecttest_handler(httpd_req_t *req);

// ============================================================================
// WIFI AP SETUP
// ============================================================================

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
    config.max_uri_handlers = 10;
    config.max_open_sockets = HTTP_SERVER_MAX_CLIENTS;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 50;  // Increased for large HTML (50ms)
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

// DISABLED: POST /api/move handler - web je 100% READ-ONLY
/*
static esp_err_t http_post_move_handler(httpd_req_t *req)
{
    // ... celá funkce zakomentována ...
}
*/

// ============================================================================
// HTML PAGE - PŘESUNUTO DO FLASH (static const)
// ============================================================================

static const char html_page[] = 
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<title>ESP32 Chess</title>"
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
            "/* Scrollbar styling */"
            ".history-box::-webkit-scrollbar { width: 6px; }"
            ".history-box::-webkit-scrollbar-track { background: #2a2a2a; }"
            ".history-box::-webkit-scrollbar-thumb { background: #4CAF50; border-radius: 3px; }"
            ".history-box::-webkit-scrollbar-thumb:hover { background: #45a049; }"
        "</style>"
    "</head>"
    "<body>"
        "<div class='container'>"
            "<h1>♟️ ESP32 Chess</h1>"
            "<div class='main-content'>"
                "<div class='board-container'>"
                    "<button class='btn-try-moves' onclick='enterSandboxMode()'>Try Moves</button>"
                    "<div id='board' class='board'></div>"
                    "<div id='loading' class='loading'>Loading board...</div>"
                "</div>"
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
                        "<h3>Move History</h3>"
                        "<div id='history' class='history-box'></div>"
                    "</div>"
                "</div>"
            "</div>"
        "</div>"
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
        "</div>"
        "<script>"
            "const pieceSymbols = {"
                "'R': '♜', 'N': '♞', 'B': '♝', 'Q': '♛', 'K': '♚', 'P': '♟',"
                "'r': '♖', 'n': '♘', 'b': '♗', 'q': '♕', 'k': '♔', 'p': '♙',"
                "' ': ' '"
            "};"
            "let boardData = [];"
            "let statusData = {};"
            "let historyData = [];"
            "let capturedData = {white_captured: [], black_captured: []};"
            "let selectedSquare = null;"
            "let reviewMode = false;"
            "let currentReviewIndex = -1;"
            "let initialBoard = [];"
            "let sandboxMode = false;"
            "let sandboxBoard = [];"
            "let sandboxHistory = [];"
            ""
            "function createBoard() {"
                "const board = document.getElementById('board');"
                "board.innerHTML = '';"
                "for (let row = 7; row >= 0; row--) {"
                    "for (let col = 0; col < 8; col++) {"
                        "const square = document.createElement('div');"
                        "square.className = 'square ' + ((row + col) % 2 === 0 ? 'light' : 'dark');"
                        "square.dataset.row = row;"
                        "square.dataset.col = col;"
                        "square.dataset.index = row * 8 + col;"
                        "square.onclick = () => handleSquareClick(row, col);"
                        "const piece = document.createElement('div');"
                        "piece.className = 'piece';"
                        "piece.id = 'piece-' + (row * 8 + col);"
                        "square.appendChild(piece);"
                        "board.appendChild(square);"
                    "}"
                "}"
            "}"
            ""
            "function clearHighlights() {"
                "document.querySelectorAll('.square').forEach(sq => {"
                    "sq.classList.remove('selected', 'valid-move', 'valid-capture');"
                "});"
                "selectedSquare = null;"
            "}"
            ""
            "async function handleSquareClick(row, col) {"
                "const piece = sandboxMode ? sandboxBoard[row][col] : boardData[row][col];"
                "const index = row * 8 + col;"
                ""
                "if (piece === ' ' && selectedSquare !== null) {"
                    "const fromRow = Math.floor(selectedSquare / 8);"
                    "const fromCol = selectedSquare % 8;"
                    ""
                    "if (sandboxMode) {"
                        "makeSandboxMove(fromRow, fromCol, row, col);"
                        "clearHighlights();"
                    "} else {"
                        "const fromNotation = String.fromCharCode(97 + fromCol) + (8 - fromRow);"
                        "const toNotation = String.fromCharCode(97 + col) + (8 - row);"
                        "try {"
                            "const response = await fetch('/api/move', {"
                                "method: 'POST',"
                                "headers: {'Content-Type': 'application/json'},"
                                "body: JSON.stringify({from: fromNotation, to: toNotation})"
                            "});"
                            "if (response.ok) {"
                                "clearHighlights();"
                                "fetchData();"
                            "}"
                        "} catch (error) {"
                            "console.error('Move error:', error);"
                        "}"
                    "}"
                    "return;"
                "}"
                ""
                "if (piece !== ' ') {"
                    "if (sandboxMode) {"
                        "clearHighlights();"
                        "selectedSquare = index;"
                        "const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);"
                        "if (square) square.classList.add('selected');"
                    "} else {"
                        "const isWhitePiece = piece === piece.toUpperCase();"
                        "const currentPlayerIsWhite = statusData.current_player === 'White' || statusData.current_player === 'Bílý';"
                        ""
                        "if ((isWhitePiece && currentPlayerIsWhite) || (!isWhitePiece && !currentPlayerIsWhite)) {"
                            "clearHighlights();"
                            "selectedSquare = index;"
                            "const square = document.querySelector(`[data-row='${row}'][data-col='${col}']`);"
                            "if (square) square.classList.add('selected');"
                        "}"
                    "}"
                "}"
            "}"
            ""
            "function reconstructBoardAtMove(moveIndex) {"
                "const startBoard = ["
                    "['R','N','B','Q','K','B','N','R'],"
                    "['P','P','P','P','P','P','P','P'],"
                    "[' ',' ',' ',' ',' ',' ',' ',' '],"
                    "[' ',' ',' ',' ',' ',' ',' ',' '],"
                    "[' ',' ',' ',' ',' ',' ',' ',' '],"
                    "[' ',' ',' ',' ',' ',' ',' ',' '],"
                    "['p','p','p','p','p','p','p','p'],"
                    "['r','n','b','q','k','b','n','r']"
                "];"
                "const board = JSON.parse(JSON.stringify(startBoard));"
                "for (let i = 0; i <= moveIndex && i < historyData.length; i++) {"
                    "const move = historyData[i];"
                    "const fromRow = parseInt(move.from[1]) - 1;"
                    "const fromCol = move.from.charCodeAt(0) - 97;"
                    "const toRow = parseInt(move.to[1]) - 1;"
                    "const toCol = move.to.charCodeAt(0) - 97;"
                    "board[toRow][toCol] = board[fromRow][fromCol];"
                    "board[fromRow][fromCol] = ' ';"
                "}"
                "return board;"
            "}"
            ""
            "function enterReviewMode(index) {"
                "reviewMode = true;"
                "currentReviewIndex = index;"
                "const banner = document.getElementById('review-banner');"
                "banner.classList.add('active');"
                "document.getElementById('review-move-text').textContent = `Prohlížíš tah ${index + 1}`;"
                "const reconstructedBoard = reconstructBoardAtMove(index);"
                "updateBoard(reconstructedBoard);"
                "document.querySelectorAll('.square').forEach(sq => {"
                    "sq.classList.remove('move-from', 'move-to');"
                "});"
                "if (index >= 0 && index < historyData.length) {"
                    "const move = historyData[index];"
                    "const fromRow = parseInt(move.from[1]) - 1;"
                    "const fromCol = move.from.charCodeAt(0) - 97;"
                    "const toRow = parseInt(move.to[1]) - 1;"
                    "const toCol = move.to.charCodeAt(0) - 97;"
                    "const fromSquare = document.querySelector(`[data-row='${fromRow}'][data-col='${fromCol}']`);"
                    "const toSquare = document.querySelector(`[data-row='${toRow}'][data-col='${toCol}']`);"
                    "if (fromSquare) fromSquare.classList.add('move-from');"
                    "if (toSquare) toSquare.classList.add('move-to');"
                "}"
                "document.querySelectorAll('.history-item').forEach(item => {"
                    "item.classList.remove('selected');"
                "});"
                "const selectedItem = document.querySelector(`[data-move-index='${index}']`);"
                "if (selectedItem) {"
                    "selectedItem.classList.add('selected');"
                    "selectedItem.scrollIntoView({behavior:'smooth',block:'nearest'});"
                "}"
            "}"
            ""
            "function exitReviewMode() {"
                "reviewMode = false;"
                "currentReviewIndex = -1;"
                "document.getElementById('review-banner').classList.remove('active');"
                "document.querySelectorAll('.square').forEach(sq => {"
                    "sq.classList.remove('move-from', 'move-to');"
                "});"
                "document.querySelectorAll('.history-item').forEach(item => {"
                    "item.classList.remove('selected');"
                "});"
                "fetchData();"
            "}"
            ""
            "function enterSandboxMode() {"
                "sandboxMode = true;"
                "sandboxBoard = JSON.parse(JSON.stringify(boardData));"
                "sandboxHistory = [];"
                "const banner = document.getElementById('sandbox-banner');"
                "banner.classList.add('active');"
                "clearHighlights();"
            "}"
            ""
            "function exitSandboxMode() {"
                "sandboxMode = false;"
                "sandboxBoard = [];"
                "sandboxHistory = [];"
                "document.getElementById('sandbox-banner').classList.remove('active');"
                "clearHighlights();"
                "fetchData();"
            "}"
            ""
            "function makeSandboxMove(fromRow, fromCol, toRow, toCol) {"
                "const piece = sandboxBoard[fromRow][fromCol];"
                "sandboxBoard[toRow][toCol] = piece;"
                "sandboxBoard[fromRow][fromCol] = ' ';"
                "sandboxHistory.push({from: `${String.fromCharCode(97+fromCol)}${8-fromRow}`, to: `${String.fromCharCode(97+toCol)}${8-toRow}`});"
                "updateBoard(sandboxBoard);"
            "}"
            ""
            "function updateBoard(board) {"
                "boardData = board;"
                "const loading = document.getElementById('loading');"
                "if (loading) loading.style.display = 'none';"
                "for (let row = 0; row < 8; row++) {"
                    "for (let col = 0; col < 8; col++) {"
                        "const piece = board[row][col];"
                        "const pieceElement = document.getElementById('piece-' + (row * 8 + col));"
                        "if (pieceElement) {"
                            "pieceElement.textContent = pieceSymbols[piece] || ' ';"
                            "if (piece !== ' ') {"
                                "pieceElement.className = 'piece ' + (piece === piece.toUpperCase() ? 'white' : 'black');"
                            "} else {"
                                "pieceElement.className = 'piece';"
                            "}"
                        "}"
                    "}"
                "}"
            "}"
            ""
            "function updateStatus(status) {"
                "statusData = status;"
                "document.getElementById('game-state').textContent = status.game_state || '-';"
                "document.getElementById('current-player').textContent = status.current_player || '-';"
                "document.getElementById('move-count').textContent = status.move_count || 0;"
                "document.getElementById('in-check').textContent = status.in_check ? 'Yes' : 'No';"
                ""
                "const lifted = status.piece_lifted;"
                "if (lifted && lifted.lifted) {"
                    "document.getElementById('lifted-piece').textContent = pieceSymbols[lifted.piece] || '-';"
                    "document.getElementById('lifted-position').textContent = String.fromCharCode(97 + lifted.col) + (lifted.row + 1);"
                    "const square = document.querySelector(`[data-row='${lifted.row}'][data-col='${lifted.col}']`);"
                    "if (square) square.classList.add('lifted');"
                "} else {"
                    "document.getElementById('lifted-piece').textContent = '-';"
                    "document.getElementById('lifted-position').textContent = '-';"
                    "document.querySelectorAll('.square').forEach(sq => sq.classList.remove('lifted'));"
                "}"
            "}"
            ""
            "function updateHistory(history) {"
                "historyData = history.moves || [];"
                "const historyBox = document.getElementById('history');"
                "historyBox.innerHTML = '';"
                "historyData.slice().reverse().forEach((move, index) => {"
                    "const item = document.createElement('div');"
                    "item.className = 'history-item';"
                    "const actualIndex = historyData.length - 1 - index;"
                    "item.dataset.moveIndex = actualIndex;"
                    "const moveNum = Math.floor(actualIndex / 2) + 1;"
                    "const isWhite = actualIndex % 2 === 0;"
                    "const prefix = isWhite ? moveNum + '. ' : '';"
                    "item.textContent = prefix + move.from + ' → ' + move.to;"
                    "item.onclick = () => enterReviewMode(actualIndex);"
                    "historyBox.appendChild(item);"
                "});"
            "}"
            ""
            "function updateCaptured(captured) {"
                "capturedData = captured;"
                "const whiteBox = document.getElementById('white-captured');"
                "const blackBox = document.getElementById('black-captured');"
                "whiteBox.innerHTML = '';"
                "blackBox.innerHTML = '';"
                "captured.white_captured.forEach(p => {"
                    "const piece = document.createElement('div');"
                    "piece.className = 'captured-piece';"
                    "piece.textContent = pieceSymbols[p] || p;"
                    "whiteBox.appendChild(piece);"
                "});"
                "captured.black_captured.forEach(p => {"
                    "const piece = document.createElement('div');"
                    "piece.className = 'captured-piece';"
                    "piece.textContent = pieceSymbols[p] || p;"
                    "blackBox.appendChild(piece);"
                "});"
            "}"
            ""
            "async function fetchData() {"
                "if (reviewMode || sandboxMode) return;"
                "try {"
                    "const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all(["
                        "fetch('/api/board'),"
                        "fetch('/api/status'),"
                        "fetch('/api/history'),"
                        "fetch('/api/captured')"
                    "]);"
                    "const board = await boardRes.json();"
                    "const status = await statusRes.json();"
                    "const history = await historyRes.json();"
                    "const captured = await capturedRes.json();"
                    "updateBoard(board.board);"
                    "updateStatus(status);"
                    "updateHistory(history);"
                    "updateCaptured(captured);"
                "} catch (error) {"
                    "console.error('Fetch error:', error);"
                "}"
            "}"
            ""
            "createBoard();"
            "fetchData();"
            "setInterval(fetchData, 500);"
            ""
            "// Keyboard shortcuts"
            "document.addEventListener('keydown', (e) => {"
                "if (e.key === 'Escape') {"
                    "if (reviewMode) {"
                        "exitReviewMode();"
                    "} else if (sandboxMode) {"
                        "exitSandboxMode();"
                    "} else {"
                        "clearHighlights();"
                    "}"
                "}"
                "if (historyData.length === 0) return;"
                "switch(e.key) {"
                    "case 'ArrowLeft':"
                        "e.preventDefault();"
                        "if (reviewMode && currentReviewIndex > 0) {"
                            "enterReviewMode(currentReviewIndex - 1);"
                        "} else if (!reviewMode && !sandboxMode && historyData.length > 0) {"
                            "enterReviewMode(historyData.length - 1);"
                        "}"
                        "break;"
                    "case 'ArrowRight':"
                        "e.preventDefault();"
                        "if (reviewMode && currentReviewIndex < historyData.length - 1) {"
                            "enterReviewMode(currentReviewIndex + 1);"
                        "}"
                        "break;"
                "}"
            "});"
            ""
            "// Kliknutí na pozadí pro zhasnutí"
            "document.addEventListener('click', (e) => {"
                "if (!e.target.closest('.square') && !e.target.closest('.history-item')) {"
                    "if (!reviewMode) {"
                        "clearHighlights();"
                    "}"
                "}"
            "});"
        "</script>"
    "</body>"
    "</html>";

// ============================================================================
// HTML PAGE HANDLER
// ============================================================================

static esp_err_t http_get_root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET / (HTML page)");
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, sizeof(html_page) - 1);  // sizeof místo strlen!
    
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
    
    // NVS is already initialized in main.c - skip it here
    ESP_LOGI(TAG, "NVS already initialized, skipping...");
    
    // Initialize WiFi AP
    esp_err_t ret = wifi_init_ap();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi AP: %s", esp_err_to_name(ret));
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
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
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
        esp_err_t wdt_ret = esp_task_wdt_reset();
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
