/**
 * @file uart_parse.h
 * @brief Command lookup and dispatch.
 */
#ifndef UART_PARSE_H
#define UART_PARSE_H

#include "uart_task.h"

const uart_command_t *find_command(const char *command);
command_result_t execute_command(const char *command, const char *args);
void uart_parse_command(const char *input);

#endif /* UART_PARSE_H */
