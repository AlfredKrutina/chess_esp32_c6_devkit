/**
 * @file game_board_core.h
 * @brief Board initialization, FEN load, and piece helpers (internal + public API in game_task.h).
 */

#ifndef GAME_BOARD_CORE_H
#define GAME_BOARD_CORE_H

#include "chess_types.h"
#include <stdbool.h>

bool game_load_position_from_fen(const char *fen, player_t *active_player);

#endif /* GAME_BOARD_CORE_H */
