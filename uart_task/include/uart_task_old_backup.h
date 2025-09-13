/*
 * ESP32-C6 Chess System v2.4 - Improved UART Task Header
 * 
 * This header defines the improved UART task interface:
 * - USB Serial JTAG console interface with proper echo
 * - Non-blocking character input
 * - Command parsing and execution
 * - System testing and diagnostics
 * - Response formatting and output
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
 * 
 * Improvements:
 * - Fixed echo functionality
 * - Non-blocking I/O
 * - Better error handling
 * - Simplified input processing
 */

#ifndef UART_TASK_IMPROVED_H
#define UART_TASK_IMPROVED_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

// Forward declarations
// Note: chess_move_command_t is defined in chess_types.h

// Task function signature
void uart_task_start(void *pvParameters);

// Line-based input functions
void uart_send_line(const char* str);
void uart_send_string(const char* str);
void uart_send_formatted(const char* format, ...);
void uart_parse_command(const char* input);
void uart_process_input(char c);

// Move validation functions
bool is_valid_move_notation(const char* move);

// Memory and health monitoring
esp_err_t uart_check_memory_health(void);

// ============================================================================
// UART MESSAGE QUEUE SYSTEM
// ============================================================================

typedef enum {
    UART_MSG_NORMAL,
    UART_MSG_ERROR,
    UART_MSG_WARNING,
    UART_MSG_SUCCESS,
    UART_MSG_INFO,
    UART_MSG_DEBUG
} uart_msg_type_t;

typedef struct {
    uart_msg_type_t type;
    char message[256];
    bool add_newline;
} uart_message_t;

// UART message queue for centralized output
extern QueueHandle_t uart_output_queue;

/**
 * @brief Send message to UART output queue (thread-safe)
 * @param type Message type (determines color)
 * @param add_newline Whether to add newline
 * @param format Format string
 * @param ... Format arguments
 */
void uart_queue_message(uart_msg_type_t type, bool add_newline, const char* format, ...);

// Command result enumeration
typedef enum {
    CMD_SUCCESS = 0,
    CMD_ERROR_INVALID_SYNTAX,
    CMD_ERROR_INVALID_PARAMETER,
    CMD_ERROR_SYSTEM_ERROR,
    CMD_ERROR_NOT_IMPLEMENTED
} command_result_t;

// Command handler function type
typedef command_result_t (*command_handler_t)(const char* args);

// ============================================================================
// ANSI COLOR AND FORMATTING FUNCTIONS
// ============================================================================

// ANSI Color and formatting functions
void uart_send_colored(const char* color, const char* message);
void uart_send_colored_line(const char* color, const char* message);
void uart_send_error(const char* message);
void uart_send_success(const char* message);
void uart_send_warning(const char* message);
void uart_send_info(const char* message);
void uart_send_move(const char* message);
void uart_send_status(const char* message);
void uart_send_debug(const char* message);
void uart_send_help(const char* message);

// ============================================================================
// ECHO CONTROL
// ============================================================================

/**
 * @brief Enable character echo
 * @param enabled true to enable echo, false to disable
 */
void uart_set_echo_enabled(bool enabled);

/**
 * @brief Get current echo status
 * @return true if echo is enabled, false otherwise
 */
bool uart_get_echo_enabled(void);

/**
 * @brief Test echo functionality
 */
void uart_test_echo(void);

#endif // UART_TASK_IMPROVED_H
