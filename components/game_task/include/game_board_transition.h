/**
 * @file game_board_transition.h
 * @brief Apply board machine output via position_t and post-move hooks.
 */

#ifndef GAME_BOARD_TRANSITION_H
#define GAME_BOARD_TRANSITION_H

#include "game_board_machine.h"
#include "chess_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  GAME_BOARD_MOVE_QUIET = 0,
  GAME_BOARD_MOVE_CAPTURE,
  GAME_BOARD_MOVE_EN_PASSANT,
  GAME_BOARD_MOVE_CASTLE_KING,
  GAME_BOARD_MOVE_CASTLE_ROOK,
  GAME_BOARD_MOVE_PROMO_ENTRY,
} game_board_move_class_t;

void game_board_transition_reset(void);

void game_board_on_lift_square(uint8_t row, uint8_t col);

board_machine_out_t game_board_on_drop_squares(uint8_t from_row, uint8_t from_col,
                                             uint8_t to_row, uint8_t to_col,
                                             bool has_from);

game_board_move_class_t game_board_classify_move(const chess_move_t *move);

bool game_board_apply_machine_move(const board_machine_out_t *out,
                                   chess_move_t *move_out);

bool game_board_apply_promotion_piece(char piece_char);

void game_board_run_after_reject(uint8_t to_row, uint8_t to_col);

piece_t game_board_piece_at(uint8_t row, uint8_t col);

bool game_board_undo_promotion_choice(void);

bool game_board_apply_uart_castle(bool kingside);

bool game_board_apply_uart_promotion(uint8_t row, uint8_t col, char piece_char);

#endif /* GAME_BOARD_TRANSITION_H */
