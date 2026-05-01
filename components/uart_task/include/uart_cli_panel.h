/**
 * @file uart_cli_panel.h
 * @brief Jednotný příkazový panel přes sériovou konzoli (`CLI …`).
 */
#ifndef UART_CLI_PANEL_H
#define UART_CLI_PANEL_H

#include "uart_task.h"

command_result_t uart_cmd_cli(const char *args);
void uart_cli_print_help(void);

#endif
