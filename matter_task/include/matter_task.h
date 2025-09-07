/*
 * ESP32-C6 Chess System v2.4 - Matter Task Header
 * 
 * This header defines the Matter task interface:
 * - Matter protocol integration
 * - Device initialization and management
 * - Protocol communication
 * - Network connectivity
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef MATTER_TASK_H
#define MATTER_TASK_H

#include "freertos_chess.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MATTER COMMAND TYPES
// ============================================================================

typedef enum {
    MATTER_CMD_INIT,             // Initialize Matter
    MATTER_CMD_START,            // Start Matter
    MATTER_CMD_STOP,             // Stop Matter
    MATTER_CMD_STATUS            // Get Matter status
} matter_command_type_t;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

// Main task function
void matter_task_start(void *pvParameters);

// Matter command processing
void matter_process_commands(void);
void matter_execute_command(uint8_t command);

// Matter control functions
void matter_initialize(void);
void matter_start(void);
void matter_stop(void);
void matter_get_status(void);

// Matter state update
void matter_update_state(void);

// Matter utility functions
bool matter_is_initialized(void);
bool matter_is_connected(void);
uint32_t matter_get_uptime(void);
void matter_send_data(uint8_t* data, size_t length);
void matter_receive_data(uint8_t* buffer, size_t max_length);

// External variables
extern QueueHandle_t matter_status_queue;
extern QueueHandle_t matter_command_queue;

// Matter device management
void matter_register_device(void);
void matter_unregister_device(void);
void matter_update_device_state(uint8_t state);

// Matter status functions
bool matter_is_task_running(void);
void matter_stop_task(void);
void matter_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_TASK_H
