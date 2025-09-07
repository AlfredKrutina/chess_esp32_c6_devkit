/*
 * ESP32-C6 Chess System v2.4 - Web Server Task Header
 * 
 * This header defines the web server task interface:
 * - Web server control functions
 * - HTTP request handlers
 * - WebSocket functions
 * - Configuration and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef WEB_SERVER_TASK_H
#define WEB_SERVER_TASK_H

#include "freertos_chess.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// WEB SERVER COMMAND TYPES
// ============================================================================

typedef enum {
    WEB_CMD_START_SERVER,        // Start web server
    WEB_CMD_STOP_SERVER,         // Stop web server
    WEB_CMD_GET_STATUS,          // Get server status
    WEB_CMD_SET_CONFIG           // Set server configuration
} web_command_type_t;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

// Main task function
void web_server_task_start(void *pvParameters);

// Command processing
void web_server_process_commands(void);
void web_server_execute_command(uint8_t command);

// Web server control
void web_server_start(void);
void web_server_stop(void);
void web_server_get_status(void);
void web_server_set_config(void);

// State management
void web_server_update_state(void);

// HTTP request handlers
void web_server_handle_root(void);
void web_server_handle_api_status(void);
void web_server_handle_api_board(void);
void web_server_handle_api_move(void);

// WebSocket functions
void web_server_websocket_init(void);
void web_server_websocket_send_update(const char* data);

// Utility functions
bool web_server_is_active(void);
uint32_t web_server_get_client_count(void);
uint32_t web_server_get_uptime(void);
void web_server_log_request(const char* method, const char* path);
void web_server_log_error(const char* error_message);

// Configuration functions
void web_server_set_port(uint16_t port);
void web_server_set_max_clients(uint32_t max_clients);
void web_server_enable_ssl(bool enable);

// Status and control functions
bool web_server_is_task_running(void);
void web_server_stop_task(void);
void web_server_reset(void);

// External variables
extern QueueHandle_t web_server_status_queue;
extern QueueHandle_t web_server_command_queue;

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_TASK_H
