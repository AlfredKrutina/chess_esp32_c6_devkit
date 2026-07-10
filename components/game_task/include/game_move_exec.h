/**
 * @file game_move_exec.h
 * @brief Move execution (internal to game_task component).
 */

#ifndef GAME_MOVE_EXEC_H
#define GAME_MOVE_EXEC_H

#include "chess_types.h"
#include <stdbool.h>

game_state_t game_analyze_position(player_t player);
bool game_execute_move_enhanced(chess_move_extended_t *move);

#endif /* GAME_MOVE_EXEC_H */
