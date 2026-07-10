/**
 * @file uart_task_internal.h
 * @brief Internal API between uart_task.c and sibling source files.
 */

#ifndef UART_TASK_INTERNAL_H
#define UART_TASK_INTERNAL_H

#include "chess_types.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

extern uint32_t command_count;
extern uint32_t error_count;
extern uint32_t last_command_time;
extern bool color_enabled;

esp_err_t uart_task_wdt_reset_safe(void);

#define SAFE_WDT_RESET() uart_task_wdt_reset_safe()

/* ANSI colors (shared by uart_task.c and handler modules) */
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"

#define COLOR_ERROR COLOR_RED COLOR_BOLD
#define COLOR_SUCCESS COLOR_GREEN COLOR_BOLD
#define COLOR_WARNING COLOR_YELLOW COLOR_BOLD
#define COLOR_INFO COLOR_CYAN
#define COLOR_MOVE COLOR_GREEN COLOR_BOLD
#define COLOR_STATUS COLOR_YELLOW
#define COLOR_DEBUG COLOR_MAGENTA
#define COLOR_HELP COLOR_BLUE COLOR_BOLD

void uart_send_colored_line(const char *color, const char *message);
void uart_send_error(const char *message);
void uart_send_success(const char *message);
void uart_send_warning(const char *message);
void uart_send_info(const char *message);
void uart_send_formatted(const char *format, ...);
void uart_send_line(const char *str);

const char *get_ascii_piece_symbol(piece_t piece);

#endif /* UART_TASK_INTERNAL_H */
