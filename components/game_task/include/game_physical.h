/**
 * @file game_physical.h
 * @brief Physical command processing (internal to game_task component).
 */

#ifndef GAME_PHYSICAL_H
#define GAME_PHYSICAL_H

#include "chess_types.h"

void game_process_pickup_command(const chess_move_command_t *cmd);
void game_process_castle_command(const chess_move_command_t *cmd);
void game_process_promote_command(const chess_move_command_t *cmd);

#endif /* GAME_PHYSICAL_H */
