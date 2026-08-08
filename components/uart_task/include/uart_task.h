/**
 * @file uart_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - UART Task Header
 * 
 * This header defines the UART task interface:
 * - USB Serial JTAG console interface
 * - Character input without blocking
 * - Parsing and executing the command
 * - System testing and diagnostics
 * - Formatting and output of the response
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-01-27
 * 
 * @details
 * UART Task processes all communication with the user via USB Serial JTAG.
 * Provides a line terminal with history, auto-completion and color output.
 * 
 * Klicova improvements:
 * - Non-blocking I/O
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
#include "uart_queue_message.h"

#ifdef __cplusplus
extern "C" {
#endif

// Predni deklarace
// Poznamka: chess_move_command_t je definovan v chess_types.h

// ============================================================================
// COMMAND RESULT TYPES
// ============================================================================

/**
 * @brief The result types of the command
 */
typedef enum {
    CMD_SUCCESS = 0,                  ///< Done successfully
    CMD_ERROR_INVALID_SYNTAX = -1,    ///< Neplatna syntaxe
    CMD_ERROR_INVALID_PARAMETER = -2, ///< Neplatny parameter
    CMD_ERROR_SYSTEM_ERROR = -3,      ///< System error
    CMD_ERROR_NOT_FOUND = -4          ///< Command not found
} command_result_t;

/**
 * @brief Command handler type (command processing function)
 */
typedef command_result_t (*command_handler_t)(const char* args);

// ============================================================================
// TYPY UART ZPRAV (uart_message_t v uart_queue_message.h)
// ============================================================================

/**
 * @brief UART command structure
 */
typedef struct {
    const char* name;             ///< Command name
    command_handler_t handler;    ///< Function for processing
    const char* description;      ///< Command description
    const char* usage;            ///< Use command
    bool requires_args;           ///< Vyzaduje argumenty?
    const char* aliases[5];       ///< Max 5 aliasu
} uart_command_t;

// ============================================================================
// EXTERNI PROMENNE
// ============================================================================

/** @brief Queue for UART output */
extern QueueHandle_t uart_output_queue;

// ============================================================================
// PROTOTYPY HLAVNICH FUNKCI
// ============================================================================

/**
 * @brief The main functions of the UART task
 * 
 * @param pvParameters Task parameters (not used)
 */
void uart_task_start(void *pvParameters);

// ============================================================================
// FEATURE FOR LINE-BASED INPUT
// ============================================================================

/**
 * @brief Write string immediately to UART
 * 
 * @param str The string to write
 */
void uart_write_string_immediate(const char* str);

/**
 * @brief Write character immediately to UART
 * 
 * @param c The character to write
 */
void uart_write_char_immediate(char c);

// ============================================================================
// FUNCTIONS FOR COMMANDS
// ============================================================================

/** @brief Display help */
command_result_t uart_cmd_help(const char* args);
/** @brief Display verbose */
command_result_t uart_cmd_verbose(const char* args);
/** @brief Display quiet */
command_result_t uart_cmd_quiet(const char* args);
/** @brief Show status */
command_result_t uart_cmd_status(const char* args);
/** @brief Display version */
command_result_t uart_cmd_version(const char* args);
/** @brief Display memory */
command_result_t uart_cmd_memory(const char* args);
/** @brief Show history */
command_result_t uart_cmd_history(const char* args);
/** @brief Unified CLI panel — see also `HELP CLI` */
command_result_t uart_cmd_cli(const char* args);
void uart_cli_print_help(void);
/** @brief Display clear */
command_result_t uart_cmd_clear(const char* args);
/** @brief Show reset */
command_result_t uart_cmd_reset(const char* args);
/** @brief Show move */
command_result_t uart_cmd_move(const char* args);
/** @brief Command up (raise the figure) */
command_result_t uart_cmd_up(const char* args);
/** @brief Command dn (place figure) */
command_result_t uart_cmd_dn(const char* args);
/** @brief Show led_board */
command_result_t uart_cmd_led_board(const char* args);
/** @brief Board display */
command_result_t uart_cmd_board(const char* args);
/** @brief Showing game_new */
command_result_t uart_cmd_game_new(const char* args);
/** @brief Showing game_reset */
command_result_t uart_cmd_game_reset(const char* args);
/** @brief Display show_moves */
command_result_t uart_cmd_show_moves(const char* args);
/** @brief Display undo */
command_result_t uart_cmd_undo(const char* args);
/** @brief Show game_history */
command_result_t uart_cmd_game_history(const char* args);
/** @brief Benchmark display */
command_result_t uart_cmd_benchmark(const char* args);
/** @brief Display show_tasks */
command_result_t uart_cmd_show_tasks(const char* args);
/** @brief Show self_test */
command_result_t uart_cmd_self_test(const char* args);
/** @brief Display test_game */
command_result_t uart_cmd_test_game(const char* args);
/** @brief Show debug_status */
command_result_t uart_cmd_debug_status(const char* args);
/** @brief Show debug_game */
command_result_t uart_cmd_debug_game(const char* args);
/** @brief Show debug_board */
command_result_t uart_cmd_debug_board(const char* args);
/** @brief Show memcheck */
command_result_t uart_cmd_memcheck(const char* args);
/** @brief Display show_mutexes */
command_result_t uart_cmd_show_mutexes(const char* args);
/** @brief Display show_fifos */
command_result_t uart_cmd_show_fifos(const char* args);

/* Game / matrix handlers (uart_handlers_game.c) */
command_result_t uart_cmd_castle(const char* args);
command_result_t uart_cmd_promote(const char* args);
command_result_t uart_cmd_guard_clear(const char* args);
command_result_t uart_cmd_start_pos_check(const char* args);
command_result_t uart_cmd_matrixtest(const char* args);

/* System / demo handlers (uart_task.c) */
command_result_t uart_cmd_eval(const char* args);
command_result_t uart_cmd_ledtest(const char* args);
command_result_t uart_cmd_performance(const char* args);
command_result_t uart_cmd_config(const char* args);
command_result_t uart_cmd_demo(const char* args);
command_result_t uart_cmd_demo_status(const char* args);
command_result_t uart_cmd_component_off(const char* args);
command_result_t uart_cmd_component_on(const char* args);
command_result_t uart_cmd_endgame_white(const char* args);
command_result_t uart_cmd_endgame_black(const char* args);
command_result_t uart_cmd_endgame_wave(const char* args);
command_result_t uart_cmd_endgame_circles(const char* args);
command_result_t uart_cmd_endgame_cascade(const char* args);
command_result_t uart_cmd_endgame_fireworks(const char* args);
command_result_t uart_cmd_endgame_draw_spiral(const char* args);
command_result_t uart_cmd_endgame_draw_pulse(const char* args);
command_result_t uart_cmd_stop_endgame(const char* args);
command_result_t uart_cmd_test_move_anim(const char* args);
command_result_t uart_cmd_test_player_anim(const char* args);
command_result_t uart_cmd_test_castle_anim(const char* args);
command_result_t uart_cmd_test_promote_anim(const char* args);
command_result_t uart_cmd_test_endgame_anim(const char* args);

/* Timer / WiFi / web / MQTT handlers (uart_handlers_wifi.c) */
command_result_t uart_cmd_timer(const char* args);
command_result_t uart_cmd_timer_config(const char* args);
command_result_t uart_cmd_timer_pause(const char* args);
command_result_t uart_cmd_timer_resume(const char* args);
command_result_t uart_cmd_timer_reset(const char* args);
command_result_t uart_cmd_wifi(const char* args);
command_result_t uart_cmd_wifi_connect(const char* args);
command_result_t uart_cmd_wifi_disconnect(const char* args);
command_result_t uart_cmd_wifi_status(const char* args);
command_result_t uart_cmd_wifi_clear(const char* args);
command_result_t uart_cmd_web_lock(const char* args);
command_result_t uart_cmd_web_status(const char* args);
command_result_t uart_cmd_api_token(const char* args);
command_result_t uart_cmd_ble(const char* args);
command_result_t uart_cmd_mqtt_config(const char* args);
command_result_t uart_cmd_mqtt_status(const char* args);
command_result_t uart_cmd_mqtt_test(const char* args);

// ============================================================================
// HELPFUL FUNCTIONS
// ============================================================================

void uart_send_error(const char* message);
void uart_send_formatted(const char* format, ...);
void uart_send_line(const char* str);

/**
 * @brief Show main help
 */
void uart_display_main_help(void);

/**
 * @brief Help for game commands
 */
void uart_cmd_help_game(void);

/**
 * @brief Help for system commands
 */
void uart_cmd_help_system(void);

/**
 * @brief Help pro zacatecniky
 */
void uart_cmd_help_beginner(void);

/**
 * @brief Help for debug commands
 */
void uart_cmd_help_debug(void);

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Display the stroke animation
 * 
 * @param from Source's notation
 * @param the target notation
 */
void uart_display_move_animation(const char* from, const char* to);

/**
 * @brief Display the expanded inbox
 */
void uart_display_enhanced_board(void);

/**
 * @brief Display the LED box
 */
void uart_display_led_board(void);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Validate stroke notation
 * 
 * @param move Move notation (eg "e2e4")
 * @return true if the notation is valid
 */
bool is_valid_move_notation(const char* move);

/**
 * @brief Validate array notation
 * 
 * @param square Square notation (eg "e2")
 * @return true if the notation is valid
 */
bool is_valid_square_notation(const char* square);

#ifdef __cplusplus
}
#endif

#endif // UART_TASK_LINENOISE_H
