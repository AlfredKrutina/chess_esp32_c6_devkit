/*
 * ESP32-C6 Chess System v2.4 - UART Task Header with Linenoise Echo
 * 
 * This header defines the UART task interface with proper echo support:
 * - USB Serial JTAG console interface with linenoise echo
 * - Non-blocking character input with immediate echo
 * - Command parsing and execution
 * - System testing and diagnostics
 * - Response formatting and output
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-01-27
 * 
 * Key improvements:
 * - Uses ESP-IDF's linenoise component for proper echo
 * - Non-blocking I/O with built-in echo support
 * - Better error handling
 * - Simplified input processing
 */

#ifndef UART_TASK_LINENOISE_H
#define UART_TASK_LINENOISE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "chess_types.h"

// Forward declarations
// Note: chess_move_command_t is defined in chess_types.h

// Command result types
typedef enum {
    CMD_SUCCESS = 0,
    CMD_ERROR_INVALID_SYNTAX = -1,
    CMD_ERROR_INVALID_PARAMETER = -2,
    CMD_ERROR_SYSTEM_ERROR = -3,
    CMD_ERROR_NOT_FOUND = -4
} command_result_t;

// Command handler function type
typedef command_result_t (*command_handler_t)(const char* args);

// UART message types
typedef enum {
    UART_MSG_NORMAL = 0,
    UART_MSG_ERROR = 1,
    UART_MSG_WARNING = 2,
    UART_MSG_SUCCESS = 3,
    UART_MSG_INFO = 4,
    UART_MSG_DEBUG = 5
} uart_msg_type_t;

// UART message structure
typedef struct {
    uart_msg_type_t type;
    bool add_newline;
    char message[256];
} uart_message_t;

// UART command structure
typedef struct {
    const char* name;
    command_handler_t handler;
    const char* description;
    const char* usage;
    bool requires_args;
    const char* aliases[5];  // Up to 5 aliases
} uart_command_t;

// External queue declarations
extern QueueHandle_t uart_output_queue;

// Task function signature
void uart_task_start(void *pvParameters);

// Line-based input functions
void uart_write_string_immediate(const char* str);
void uart_write_char_immediate(char c);

// Echo control functions
void uart_set_echo_enabled(bool enabled);
bool uart_get_echo_enabled(void);

// Test functions
void uart_test_echo(void);

// Genius compatibility - no complex response system needed

// Command functions
command_result_t uart_cmd_help(const char* args);
command_result_t uart_cmd_verbose(const char* args);
command_result_t uart_cmd_quiet(const char* args);
command_result_t uart_cmd_status(const char* args);
command_result_t uart_cmd_version(const char* args);
command_result_t uart_cmd_memory(const char* args);
command_result_t uart_cmd_history(const char* args);
command_result_t uart_cmd_clear(const char* args);
command_result_t uart_cmd_reset(const char* args);
command_result_t uart_cmd_move(const char* args);
command_result_t uart_cmd_up(const char* args);
command_result_t uart_cmd_dn(const char* args);
command_result_t uart_cmd_led_board(const char* args);
command_result_t uart_cmd_board(const char* args);
void uart_display_led_board(void);
command_result_t uart_cmd_game_new(const char* args);
command_result_t uart_cmd_game_reset(const char* args);
command_result_t uart_cmd_show_moves(const char* args);
command_result_t uart_cmd_undo(const char* args);
command_result_t uart_cmd_game_history(const char* args);
command_result_t uart_cmd_benchmark(const char* args);
command_result_t uart_cmd_show_tasks(const char* args);
command_result_t uart_cmd_self_test(const char* args);
command_result_t uart_cmd_test_game(const char* args);
command_result_t uart_cmd_debug_status(const char* args);
command_result_t uart_cmd_debug_game(const char* args);
command_result_t uart_cmd_debug_board(const char* args);
command_result_t uart_cmd_memcheck(const char* args);
command_result_t uart_cmd_show_mutexes(const char* args);
command_result_t uart_cmd_show_fifos(const char* args);

// Help functions
void uart_display_main_help(void);
void uart_cmd_help_game(void);
void uart_cmd_help_system(void);
void uart_cmd_help_beginner(void);
void uart_cmd_help_debug(void);

// Display functions
void uart_display_move_animation(const char* from, const char* to);
void uart_display_enhanced_board(void);

// Utility functions
bool is_valid_move_notation(const char* move);
bool is_valid_square_notation(const char* square);

#endif // UART_TASK_LINENOISE_H
