/**
 * @file position_internal.h
 * @brief Helpers shared inside chess_core. Not a public header.
 */

#ifndef POSITION_INTERNAL_H
#define POSITION_INTERNAL_H

#include "position.h"

#include <stdbool.h>

bool position_sq_attacked(const uint8_t board[8][8], int row, int col, int by_white);
bool position_mover_in_check(const position_t *p, uint8_t side);
bool position_has_legal_move(const position_t *p);

/* trial: FEN parse, position_legal, repetition replay (those calls are not nested).
 * apply: position_apply only. position_legal holds trial across that call. */
position_t *position_core_trial(void);
position_t *position_core_apply(void);

#endif /* POSITION_INTERNAL_H */
