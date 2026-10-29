/**
 * @file game_board_machine.h
 * @brief Physical board state machine (lift/drop); does not write position_t.
 */

#ifndef GAME_BOARD_MACHINE_H
#define GAME_BOARD_MACHINE_H

#include "chess_types.h"
#include "position.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BM_STATE_IDLE = 0,
  BM_STATE_LIFTED,
  BM_STATE_WAIT_CAPTURE,
  BM_STATE_WAIT_ROOK,
  BM_STATE_WAIT_PROMO,
} board_machine_state_t;

typedef enum {
  BM_OUT_NONE = 0,
  BM_OUT_MOVE,
  BM_OUT_REJECT,
  BM_OUT_NEED_PROMO,
} board_machine_out_kind_t;

typedef enum {
  BM_REJECT_NONE = 0,
  BM_REJECT_ILLEGAL,
  BM_REJECT_NO_LIFT,
  BM_REJECT_WRONG_CAPTURE_STEP,
  BM_REJECT_WRONG_ROOK_STEP,
  BM_REJECT_PROMO_PENDING,
} board_machine_reject_t;

typedef struct {
  board_machine_state_t state;
  uint8_t lifted_sq;
  uint8_t capture_victim_sq;
  uint8_t rook_from_sq;
  uint8_t rook_to_sq;
  uint8_t promo_sq;
  board_machine_reject_t last_reject;
} board_machine_t;

typedef struct {
  board_machine_out_kind_t kind;
  position_move_t move;
  board_machine_reject_t reason;
} board_machine_out_t;

void board_machine_reset(board_machine_t *m);

board_machine_out_t board_machine_lift(board_machine_t *m, const position_t *pos,
                                       uint8_t square);

board_machine_out_t board_machine_drop(board_machine_t *m, const position_t *pos,
                                       uint8_t from, uint8_t to,
                                       bool has_from);

void board_machine_on_capture_victim_lifted(board_machine_t *m, uint8_t victim_sq);

void board_machine_on_castle_king_applied(board_machine_t *m, uint8_t rook_from,
                                          uint8_t rook_to);

void board_machine_on_promo_pawn_applied(board_machine_t *m, uint8_t promo_sq);

board_machine_t *game_board_machine(void);

/** Sync piece_lifted, lifted_* globals, and m->lifted_sq for rules/LED paths. */
void game_board_set_piece_lifted(bool lifted, uint8_t row, uint8_t col,
                                 piece_t piece);

/* In-progress castle on the physical board (not the castling-rights bits). */
typedef struct {
  bool in_progress;
  uint8_t rook_from_row;
  uint8_t rook_from_col;
  uint8_t rook_to_row;
  uint8_t rook_to_col;
  player_t player;
  bool is_kingside;
} castling_state_t;

extern castling_state_t castling_state;

extern bool piece_lifted;
extern uint8_t lifted_piece_row;
extern uint8_t lifted_piece_col;
extern piece_t lifted_piece;

extern bool capture_in_progress;
extern uint8_t capture_target_row;
extern uint8_t capture_target_col;
extern piece_t capture_removed_piece;

extern bool opponent_piece_moved;
extern piece_t opponent_piece_type;
extern uint8_t opponent_original_row;
extern uint8_t opponent_original_col;
extern uint8_t opponent_current_row;
extern uint8_t opponent_current_col;

extern bool castle_animation_active;
extern uint8_t rook_from_row;
extern uint8_t rook_from_col;
extern uint8_t rook_to_row;
extern uint8_t rook_to_col;

#define GUIDED_CAPTURE_MAX_ATTACKERS 16

typedef struct {
  bool active;
  uint8_t target_row;
  uint8_t target_col;
  piece_t target_piece;
  uint8_t attacker_count;
  uint8_t attacker_rows[GUIDED_CAPTURE_MAX_ATTACKERS];
  uint8_t attacker_cols[GUIDED_CAPTURE_MAX_ATTACKERS];
  piece_t attacker_pieces[GUIDED_CAPTURE_MAX_ATTACKERS];
} guided_capture_state_t;

extern guided_capture_state_t guided_capture_state;

#endif /* GAME_BOARD_MACHINE_H */
