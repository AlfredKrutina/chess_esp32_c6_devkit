/**
 * @file uart_parse.h
 * @brief Command parsing and game-task dispatch (internal to uart_task component).
 */

#ifndef UART_PARSE_H
#define UART_PARSE_H

#include "uart_task.h"
#include <stdbool.h>
#include <stddef.h>

const uart_command_t *find_command(const char *command);
command_result_t execute_command(const char *command, const char *args);
void uart_parse_command(const char *input);

bool parse_move_notation(const char *input, char *from, char *to);
bool validate_chess_squares(const char *from, const char *to);
bool send_to_game_task(const chess_move_command_t *move_cmd);
bool send_to_game_task_with_response(const chess_move_command_t *move_cmd,
                                     char *response_buffer, size_t buffer_size,
                                     uint32_t timeout_ms);
bool is_valid_move_notation(const char *move);

#endif /* UART_PARSE_H */
