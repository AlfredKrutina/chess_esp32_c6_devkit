/**
 * @file uart_task_internal.h
 * @brief Internal API between uart_task.c and sibling source files.
 */

#ifndef UART_TASK_INTERNAL_H
#define UART_TASK_INTERNAL_H

#include "esp_err.h"
#include <stdint.h>

extern uint32_t command_count;
extern uint32_t error_count;
extern uint32_t last_command_time;

esp_err_t uart_task_wdt_reset_safe(void);

#define SAFE_WDT_RESET() uart_task_wdt_reset_safe()

#endif /* UART_TASK_INTERNAL_H */
