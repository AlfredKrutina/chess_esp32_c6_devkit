/**
 * @file game_move_validate.h
 * @brief Move validation (internal to game_task component).
 */

#ifndef GAME_MOVE_VALIDATE_H
#define GAME_MOVE_VALIDATE_H

#include "chess_types.h"
#include <stdbool.h>

bool game_is_king_in_check(player_t player);

#endif /* GAME_MOVE_VALIDATE_H */
