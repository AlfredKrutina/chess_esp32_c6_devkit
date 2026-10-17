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
void uart_task_wdt_set_subscribed(bool subscribed);

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

command_result_t uart_cmd_help(const char* args);
command_result_t uart_cmd_status(const char* args);
command_result_t uart_cmd_clear(const char* args);
command_result_t uart_cmd_reset(const char* args);
command_result_t uart_cmd_ledtest(const char* args);
command_result_t uart_cmd_hall(const char* args);
command_result_t uart_cmd_hall_stop(const char* args);
command_result_t uart_cmd_mag(const char* args);
command_result_t uart_cmd_stm32(const char* args);
command_result_t uart_cmd_log(const char* args);

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

#ifdef __cplusplus
}
#endif

#endif // UART_TASK_LINENOISE_H
