/**
 * @file web_server_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - Web Server Task Header
 *
 * This header defines the interface for the web server task:
 * - Functions for controlling the web server
 * - HTTP request handlers
 * - WebSocket functionality
 * - Configuration and function status
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * Web Server Task manages WiFi Access Point and HTTP server for remote
 * control of the sach system. The user can connect to a WiFi hotspot
 * "ESP32-CzechMate" (or ESP32-CzechMate_1...) and control the game via a web browser.
 *
 * WiFi configuration:
 * - SSID: ESP32-CzechMate or ESP32-CzechMate_N (car according to surrounding boards)
 * - Password: 12345678
 * - IP: 192.168.4.1
 * - Channel: 1
 * - Max connections: 4
 *
 * HTTP Endpoints:
 * - GET / - Main page with web interface
 * - GET /api/board - Current state of the inbox (JSON)
 * - GET /api/status - Game status (JSON)
 * - GET /api/history - Pull history (JSON)
 * - GET /api/captured - Captured figures (JSON)
 * - GET /api/advantage - Material advantage chart (JSON)
 * - GET /api/timer - Status of the time system (JSON)
 * - POST /api/timer/config - Time system configuration
 * - POST /api/timer/pause - Pause the timer
 * - POST /api/timer/resume - Timer resume
 * - POST /api/timer/reset - Timer reset
 *
 * HTTP authentication (Bearer token and web lock) — see URI overview
 * @ref board_api_auth.h (board_api_auth.h).
 */

#ifndef WEB_SERVER_TASK_H
#define WEB_SERVER_TASK_H

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos_chess.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// WEB SERVER COMMAND TYPES
// ============================================================================

/**
 * @brief Web server command types
 */
typedef enum {
  WEB_CMD_START_SERVER, ///< Start web server
  WEB_CMD_STOP_SERVER,  ///< Zastav web server
  WEB_CMD_GET_STATUS,   ///< Ziskej status serveru
  WEB_CMD_SET_CONFIG    ///< Nastav konfiguraci serveru
} web_command_type_t;

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief The main functions of the Web Server task
 *
 * Initializes the WiFi AP, starts the HTTP server and processes requests.
 *
 * @param pvParameters Task parameters (not used)
 */
void web_server_task_start(void *pvParameters);

// Order processing

/**
 * @brief Process web server commands from the queue
 */
void web_server_process_commands(void);

/**
 * @brief Sends pending game snapshot (WS + BLE) from queue — call from web
 * server task loop; must not from game_task.
 */
void web_server_process_snapshot_notify_queue(void);

/**
 * @brief Execute the web server command
 *
 * @param command The command to execute (web_command_type_t)
 */
void web_server_execute_command(uint8_t command);

/**
 * @brief Enqueues command to web_server task (safe from UART/other task).
 */
esp_err_t web_server_enqueue_command(web_command_type_t cmd);

// Functions for controlling the web server

/**
 * @brief Start the web server
 *
 * Initializes the WiFi AP and start the HTTP server.
 */
void web_server_start(void);

/**
 * @brief Zastav web server
 *
 * Stop HTTP server and WiFi AP.
 */
void web_server_stop(void);

/**
 * @brief Get the web server status
 *
 * Lists server status (active, number of clients, uptime).
 */
void web_server_get_status(void);

/**
 * @brief Set the web server configuration
 *
 * Zmeni konfiguraci serveru (port, max klientu, SSL).
 */
void web_server_set_config(void);

// Status management

/**
 * @brief Update the web server status
 *
 * Periodically updates the server's internal status.
 */
void web_server_update_state(void);

// HTTP request handlers

/**
 * @brief Process GET / (main page)
 */
void web_server_handle_root(void);

/**
 * @brief Handle GET /api/status (game state)
 */
void web_server_handle_api_status(void);

/**
 * @brief Process GET /api/board (box)
 */
void web_server_handle_api_board(void);

/**
 * @brief Zpracuj POST /api/move (tah)
 */
void web_server_handle_api_move(void);

// WebSocket function

/**
 * @brief Initialize the WebSocket
 */
void web_server_websocket_init(void);

/**
 * @brief Posli aktualizaci pres WebSocket
 *
 * @param data Data k poslani (JSON string)
 */
void web_server_websocket_send_update(const char *data);

// Utility function

/**
 * @brief Verify if the web server is active
 *
 * @return true if the server is running
 */
bool web_server_is_active(void);

/**
 * @brief Current SSID WiFi AP after start (e.g. ESP32-CzechMate or ESP32-CzechMate_1).
 */
const char *web_server_get_ap_ssid(void);

/**
 * @brief Get the number of connected clients
 *
 * @return The number of currently connected clients
 */
uint32_t web_server_get_client_count(void);

/**
 * @brief Return the last error when starting the HTTP server (ESP_OK if no error)
 */
esp_err_t web_server_get_last_http_error(void);

/**
 * @brief Get server uptime
 *
 * @return Uptime in milliseconds since startup
 */
uint32_t web_server_get_uptime(void);

/**
 * @brief Zaloguj HTTP pozadavek
 *
 * @param method HTTP metoda (GET, POST, atd.)
 * @param path Cesta pozadavku (/api/board, atd.)
 */
void web_server_log_request(const char *method, const char *path);

/**
 * @brief Zaloguj chybu web serveru
 *
 * @param error_message Chybova zprava
 */
void web_server_log_error(const char *error_message);

// Configuration functions

/**
 * @brief Set server port
 *
 * @param port Cislo portu (80 = HTTP standard)
 */
void web_server_set_port(uint16_t port);

/**
 * @brief Set maximum client
 *
 * @param max_clients Maximum number of simultaneously connected clients
 */
void web_server_set_max_clients(uint32_t max_clients);

/**
 * @brief Zapni/vypni SSL
 *
 * @param enable true pro zapnuti SSL (HTTPS)
 */
void web_server_enable_ssl(bool enable);

// Functions for status and control

/**
 * @brief Check if the web server task is running
 *
 * @return true if the task is running
 */
bool web_server_is_task_running(void);

/**
 * @brief Zastav web server task
 *
 * Ukonci web server task a uvolni prostredky.
 */
void web_server_stop_task(void);

/**
 * @brief Resetuj web server
 *
 * Performs a complete reset of the web server.
 */
void web_server_reset(void);

// WiFi function for external use (UART commands)
/**
 * @brief Save WiFi STA configuration to NVS
 *
 * @param ssid SSID WiFi site (max 32 characters)
 * @param password WiFi site password (max 64 characters)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_save_config_to_nvs(const char *ssid, const char *password);

/**
 * @brief I connect the ESP32 to the WiFi site as a Station
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_connect_sta(void);

/**
 * @brief Disconnect the ESP32 from the WiFi site
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_disconnect_sta(void);

/**
 * @brief Clears WiFi STA configuration from NVS
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t wifi_clear_config_from_nvs(void);

/**
 * @brief Determine if STA is connected
 *
 * @return true if STA is connected, false otherwise
 */
bool wifi_is_sta_connected(void);

/**
 * @brief Determine if the web interface is locked
 *
 * @return true if locked, false if unlocked
 */
bool web_is_locked(void);

/**
 * @brief Set lock state and save to NVS
 *
 * @param locked True for lock, false for unlock
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_lock_set(bool locked);

/**
 * @brief Nacte lock status from NVS
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_lock_load_from_nvs(void);

/**
 * @brief Create WiFi STA configuration from NVS (for external use)
 *
 * @param ssid Buffer for SSID (minimum 33 characters)
 * @param ssid_len Buffer size for SSID
 * @param password Buffer for the password (minimum 65 characters)
 * @param password_len Buffer size for the password
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if the configuration does not exist
 */
esp_err_t wifi_load_config_from_nvs(char *ssid, size_t ssid_len, char *password,
                                    size_t password_len);

/**
 * @brief Same JSON as GET /api/game/snapshot (for BLE GATT read / internal
 * use).
 */
esp_err_t web_server_build_game_snapshot_json(char *out, size_t cap,
                                              size_t *out_len);

/**
 * @brief Same JSON as GET /api/game/snapshot to internal `snapshot_buffer`.
 *
 * Saves ~20KB of RAM over the second static field in BLE — the mutex is inside the build.
 * Valid pointer only until the next call of any snapshot build (HTTP/BLE).
 */
esp_err_t web_server_build_game_snapshot_json_shared(char **out_json,
                                                     size_t *out_len);

/**
 * @brief Process UTF-8 JSON from BLE GATT notation to a command characteristic
 * (cmd: ping, hint_highlight, hint_clear, brightness — same behavior as
 * REST where indicated).
 */
esp_err_t web_server_ble_command_dispatch(const char *json, size_t json_len);

/** True if dispatch has already sent its own cmd_ack (e.g. ota_ble_status). */
bool web_server_ble_dispatch_custom_ack_was_sent(void);

/**
 * @brief Extracts the string from the "cmd" field of the BLE JSON (for confirmation notifications).
 * @return true if cmd was found and copied to cmd_out
 */
bool web_server_ble_extract_cmd_for_ack(const char *json, char *cmd_out,
                                        size_t cmd_out_sz);

/**
 * @brief Get the current IP address of the STA interface
 *
 * @param buffer Buffer for IP address (min 16 characters)
 * @param max_len Buffer size
 * @return ESP_OK on success
 */
esp_err_t wifi_get_sta_ip(char *buffer, size_t max_len);

/**
 * @brief Get the current SSID of the STA interface
 *
 * @param buffer Buffer for SSID (min 33 characters)
 * @param max_len Buffer size
 * @return ESP_OK on success
 */
esp_err_t wifi_get_sta_ssid(char *buffer, size_t max_len);

/** True if the board is currently broadcasting a user hotspot (AP). */
bool wifi_ap_is_broadcasting(void);

/** Hotspot SSID (CzechMate convention), valid even if the AP is not currently broadcasting. */
const char *wifi_ap_effective_ssid(void);

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/** @brief Fronta pro web server status */
extern QueueHandle_t web_server_status_queue;
/** @brief Queue for web server commands */
extern QueueHandle_t web_server_command_queue;

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_TASK_H
