/**
 * @file game_physical.h
 * @brief Physical move commands. Implementation stays in game_physical.c.
 *
 * Pickup, castling, and promotion rules are not re-split here. LED writes
 * from that file go through led_task.
 */
#ifndef GAME_PHYSICAL_H
#define GAME_PHYSICAL_H

#include "chess_types.h"

/**
 * @brief Handle a piece lift (board UP or UART pickup).
 * @param cmd Command with the source square in from_notation. NULL is ignored.
 */
void game_process_pickup_command(const game_cmd_t *cmd);

/**
 * @brief Handle a UART CASTLE command.
 * @param cmd Direction is in to_notation. NULL is ignored.
 */
void game_process_castle_command(const game_cmd_t *cmd);

/**
 * @brief Handle a UART PROMOTE command (square and piece choice).
 * @param cmd from_notation is the square, to_notation is the piece. NULL is ignored.
 */
void game_process_promote_command(const game_cmd_t *cmd);

#endif /* GAME_PHYSICAL_H */
