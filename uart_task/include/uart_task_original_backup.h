/*
 * ESP32-C6 Chess System v2.4 - UART Task Header
 * 
 * This header defines the UART task interface:
 * - USB Serial JTAG console interface
 * - Command parsing and execution
 * - System testing and diagnostics
 * - Response formatting and output
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef UART_TASK_H
#define UART_TASK_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/queue.h"

// Forward declarations - removed conflicting system_config_t
// Note: chess_move_command_t is defined in chess_types.h

// Task function signature
void uart_task_start(void *pvParameters);

// Line-based input functions
void uart_send_line(const char* str);
void uart_send_string(const char* str);
void uart_send_formatted(const char* format, ...);
void uart_parse_command(const char* input);
void uart_process_input(char c);

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

// Command functions (new signature with args)
command_result_t uart_cmd_help(const char* args);
command_result_t uart_cmd_status(const char* args);
command_result_t uart_cmd_version(const char* args);
command_result_t uart_cmd_memory(const char* args);
command_result_t uart_cmd_verbose(const char* args);
command_result_t uart_cmd_quiet(const char* args);
command_result_t uart_cmd_history(const char* args);

command_result_t uart_cmd_clear(const char* args);
command_result_t uart_cmd_reset(const char* args);

// Game command functions
command_result_t uart_cmd_move(const char* args);
command_result_t uart_cmd_board(const char* args);
command_result_t uart_cmd_game_new(const char* args);
command_result_t uart_cmd_game_reset(const char* args);
command_result_t uart_cmd_show_moves(const char* args);
command_result_t uart_cmd_undo(const char* args);
command_result_t uart_cmd_game_history(const char* args);

// Move parsing functions
bool parse_move_notation(const char* input, char* from, char* to);
bool validate_chess_squares(const char* from, const char* to);
// send_to_game_task declaration moved to implementation - uses chess_move_command_t

// Legacy command functions (kept for compatibility)
void uart_cmd_led_test(void);
void uart_parse_led_set(const char* input);
void uart_cmd_led_board(void);
void uart_cmd_led_clear(void);
void uart_cmd_matrix_status(void);
void uart_parse_matrix_move(const char* input);
void uart_cmd_button_status(void);
void uart_parse_button_test(const char* input);
void uart_parse_game_state(const char* input);

// ============================================================================
// NEW CONTROL COMMANDS DECLARATIONS
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

// Advanced help system
void uart_cmd_help_game(void);
void uart_cmd_help_system(void);
void uart_cmd_help_beginner(void);
void uart_cmd_help_debug(void);
void uart_cmd_quickstart(void);

// Screen and display control
void uart_cmd_show_board(void);
void uart_cmd_auto_display(const char* arg);
void uart_cmd_auto_display_status(void);

// LED control and status
void uart_cmd_led_status(void);
void uart_cmd_led_enable(void);
void uart_cmd_led_disable(void);
void uart_cmd_led_status_detailed(void);
void uart_cmd_led_status_compact(void);
void uart_cmd_led_show_active(void);
void uart_cmd_led_show_changes(void);

// Matrix and scanning control
void uart_cmd_scan_status(void);
void uart_cmd_scan_enable(void);
void uart_cmd_scan_disable(void);

// Animation and system control
void uart_cmd_animation_trigger(void);
void uart_cmd_screen_saver_trigger(void);
void uart_cmd_sleep(void);
void uart_cmd_anim_with_id(const char* anim_id);

// Game control and statistics
void uart_cmd_show_valid_moves(void);
void uart_cmd_show_move_history(void);
void uart_cmd_game_stats(void);
void uart_cmd_material_score(void);
void uart_cmd_timer_control(bool enabled);
void uart_cmd_save_game(const char* game_name);
void uart_cmd_load_game(const char* game_name);
void uart_cmd_export_pgn(void);

// Demo mode control
void uart_cmd_demo_mode(bool enabled);
void uart_cmd_demo_speed(int speed);

// Debugging and testing
command_result_t uart_cmd_debug_status(const char* args);
command_result_t uart_cmd_debug_game(const char* args);
command_result_t uart_cmd_debug_board(const char* args);
command_result_t uart_cmd_self_test(const char* args);

command_result_t uart_cmd_test_game(const char* args);
command_result_t uart_cmd_benchmark(const char* args);
command_result_t uart_cmd_memcheck(const char* args);
command_result_t uart_cmd_show_tasks(const char* args);

// Echo control commands
command_result_t uart_cmd_echo_on(const char* args);
command_result_t uart_cmd_echo_off(const char* args);
command_result_t uart_cmd_echo_test(const char* args);

// System control and utilities
void uart_cmd_system_reset(void);
void uart_cmd_loglevel(const char* level);
void uart_cmd_set_verbose(const char* mode);
void uart_cmd_quiet_mode(void);

// Alias functions
void uart_cmd_gamestat(void);
void uart_cmd_sysstat(void);
void uart_cmd_test(void);

// Test functions (declared above with correct signatures)

// New chess display commands
void uart_cmd_show_board(void);
void uart_cmd_show_valid_moves(void);
void uart_cmd_show_move_history(void);
void uart_cmd_auto_display(const char* mode);
void uart_cmd_auto_display_status(void);

// New game statistics and control commands
void uart_cmd_game_stats(void);
void uart_cmd_material_score(void);
void uart_cmd_timer_control(bool enabled);
void uart_cmd_save_game(const char* game_name);
void uart_cmd_load_game(const char* game_name);
void uart_cmd_export_pgn(void);

// ============================================================================
// UX ENHANCEMENT FUNCTIONS
// ============================================================================

/**
 * @brief Display impressive welcome logo with ANSI colors
 */
void uart_send_welcome_logo(void);

/**
 * @brief Show animated progress bar
 * @param label Progress label
 * @param max_value Maximum value (100 = 100%)
 * @param duration_ms Duration in milliseconds
 */
void uart_show_progress_bar(const char* label, uint32_t max_value, uint32_t duration_ms);

/**
 * @brief Centralized chess board display function with consistent colors
 */
void uart_display_chess_board(void);

/**
 * @brief Display enhanced chess board with animations and visual effects
 * @deprecated Use uart_display_chess_board() for consistent display
 */
void uart_display_enhanced_board(void);

/**
 * @brief Display animated move visualization
 * @param from Starting square
 * @param to Destination square
 */
void uart_display_move_animation(const char* from, const char* to);

/**
 * @brief Display main help menu with categories
 */
void uart_display_main_help(void);

// Comprehensive help system
void uart_cmd_help_game(void);
void uart_cmd_help_system(void);
void uart_cmd_help_beginner(void);
void uart_cmd_help_debug(void);
void uart_cmd_quickstart(void);

// Colored output functions
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

// Demo mode and debugging functions (declared above with correct signatures)
void uart_cmd_demo_mode(bool enabled);
void uart_cmd_demo_speed(int speed);

// Utility functions
void uart_show_history(void);
int uart_chess_to_index(const char* square);
void uart_process_input(char c);



// Chess move helper functions
bool uart_validate_chess_square(const char* square);
void uart_chess_square_to_coords(const char* square, int* row, int* col);

#endif // UART_TASK_H