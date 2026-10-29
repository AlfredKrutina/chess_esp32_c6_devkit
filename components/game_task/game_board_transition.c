/**
 * @file game_board_transition.c
 * @brief Bridge board machine output to position_t and post-move side effects.
 */

#include "game_board_transition.h"

#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_mode.h"
#include "game_move_exec.h"
#include "game_policy.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "game_physical_undo.h"
#include "game_snapshot.h"

void game_board_transition_reset(void) { board_machine_reset(game_board_machine()); }

void game_board_on_lift_square(uint8_t row, uint8_t col) {
  position_t *scratch_pos = game_logic_scratch_position();
  uint8_t square = (uint8_t)(row * 8u + col);
  game_logic_copy_globals(scratch_pos);
  (void)board_machine_lift(game_board_machine(), scratch_pos, square);
  game_mode_call_lift(scratch_pos, square);
}

board_machine_out_t game_board_on_drop_squares(uint8_t from_row, uint8_t from_col,
                                             uint8_t to_row, uint8_t to_col,
                                             bool has_from) {
  position_t *scratch_pos = game_logic_scratch_position();
  game_logic_copy_globals(scratch_pos);
  uint8_t from = has_from ? (uint8_t)(from_row * 8u + from_col)
                          : game_board_machine()->lifted_sq;
  uint8_t to = (uint8_t)(to_row * 8u + to_col);
  game_mode_call_drop(scratch_pos, from, to, has_from);
  board_machine_out_t out = board_machine_drop(game_board_machine(), scratch_pos,
                                              from, to, has_from);
  if (game_mode_take_drop_reject() &&
      (out.kind == BM_OUT_MOVE || out.kind == BM_OUT_NEED_PROMO)) {
    board_machine_t *machine = game_board_machine();
    machine->last_reject = BM_REJECT_ILLEGAL;
    machine->state = BM_STATE_LIFTED;
    machine->lifted_sq = from;
    out.kind = BM_OUT_REJECT;
    out.reason = BM_REJECT_ILLEGAL;
  }
  return out;
}

game_board_move_class_t game_board_classify_move(const chess_move_t *move) {
  if (!move) {
    return GAME_BOARD_MOVE_QUIET;
  }
  piece_t sp = move->piece;
  if ((sp == PIECE_WHITE_KING || sp == PIECE_BLACK_KING) &&
      ((int)move->to_col - (int)move->from_col == 2 ||
       (int)move->to_col - (int)move->from_col == -2)) {
    return GAME_BOARD_MOVE_CASTLE_KING;
  }
  if (castling_state.in_progress &&
      move->from_row == castling_state.rook_from_row &&
      move->from_col == castling_state.rook_from_col &&
      move->to_row == castling_state.rook_to_row &&
      move->to_col == castling_state.rook_to_col) {
    return GAME_BOARD_MOVE_CASTLE_ROOK;
  }
  if ((sp == PIECE_WHITE_PAWN && move->to_row == 7) ||
      (sp == PIECE_BLACK_PAWN && move->to_row == 0)) {
    return GAME_BOARD_MOVE_PROMO_ENTRY;
  }
  if (move->captured_piece != PIECE_EMPTY) {
    return GAME_BOARD_MOVE_CAPTURE;
  }
  if (en_passant_available && move->to_row == en_passant_target_row &&
      move->to_col == en_passant_target_col) {
    return GAME_BOARD_MOVE_EN_PASSANT;
  }
  return GAME_BOARD_MOVE_QUIET;
}

static void pos_move_to_chess(const position_move_t *pm, chess_move_t *out) {
  out->from_row = pm->from / 8u;
  out->from_col = pm->from % 8u;
  out->to_row = pm->to / 8u;
  out->to_col = pm->to % 8u;
  out->piece = game_board_piece_at(out->from_row, out->from_col);
  out->captured_piece = game_board_piece_at(out->to_row, out->to_col);
  out->timestamp = esp_timer_get_time() / 1000;
}

bool game_board_apply_machine_move(const board_machine_out_t *out,
                                   chess_move_t *move_out) {
  if (!out || !move_out) {
    return false;
  }
  /* Quiet, capture, castle, and promo entry all go through game_execute_move. */
  if (out->kind != BM_OUT_NEED_PROMO && out->kind != BM_OUT_MOVE) {
    return false;
  }
  pos_move_to_chess(&out->move, move_out);
  if (!game_execute_move(move_out)) {
    return false;
  }
  game_mode_call_move_applied(game_logic_position(), out->move);
  return true;
}

static position_promo_t char_to_promo(char c) {
  switch (c) {
  case 'r':
  case 'R':
    return POS_PROMO_R;
  case 'b':
  case 'B':
    return POS_PROMO_B;
  case 'n':
  case 'N':
    return POS_PROMO_N;
  case 'q':
  case 'Q':
  default:
    return POS_PROMO_Q;
  }
}

bool game_board_apply_promotion_piece(char piece_char) {
  if (!(*game_state_promotion_state()).pending) {
    return false;
  }
  uint8_t row = (*game_state_promotion_state()).square_row;
  uint8_t col = (*game_state_promotion_state()).square_col;
  position_t *logic = game_logic_position();
  position_promo_t pr = char_to_promo(piece_char);
  uint8_t p = logic->board[row][col];
  uint8_t promoted = POS_EMPTY;
  if (p == POS_WHITE_PAWN) {
    switch (pr) {
    case POS_PROMO_R:
      promoted = POS_WHITE_ROOK;
      break;
    case POS_PROMO_B:
      promoted = POS_WHITE_BISHOP;
      break;
    case POS_PROMO_N:
      promoted = POS_WHITE_KNIGHT;
      break;
    default:
      promoted = POS_WHITE_QUEEN;
      break;
    }
  } else if (p == POS_BLACK_PAWN) {
    switch (pr) {
    case POS_PROMO_R:
      promoted = POS_BLACK_ROOK;
      break;
    case POS_PROMO_B:
      promoted = POS_BLACK_BISHOP;
      break;
    case POS_PROMO_N:
      promoted = POS_BLACK_KNIGHT;
      break;
    default:
      promoted = POS_BLACK_QUEEN;
      break;
    }
  } else {
    return false;
  }
  game_set_piece((int)row, (int)col, (piece_t)promoted);
  logic->side = (uint8_t)(((*game_state_promotion_state()).player == PLAYER_WHITE)
                              ? PLAYER_BLACK
                              : PLAYER_WHITE);
  game_session_snap_refresh();
  game_set_fifty_move_counter(logic->halfmove);
  moves_without_capture = logic->halfmove;
  return true;
}

piece_t game_board_piece_at(uint8_t row, uint8_t col) {
  const position_t *logic = game_logic_position();
  if (logic != NULL) {
    return (piece_t)logic->board[row][col];
  }
  return game_get_piece(row, col);
}

bool game_board_undo_promotion_choice(void) {
  position_t *logic = game_logic_position();
  if (logic == NULL) {
    return false;
  }
  game_logic_copy_globals(logic);
  uint8_t row = (*game_state_promotion_state()).square_row;
  uint8_t col = (*game_state_promotion_state()).square_col;
  game_set_piece((int)row, (int)col,
                 ((*game_state_promotion_state()).player == PLAYER_WHITE)
                     ? PIECE_WHITE_PAWN
                     : PIECE_BLACK_PAWN);
  logic->side = (uint8_t)(*game_state_promotion_state()).player;
  game_session_snap_refresh();
  (*game_state_promotion_state()).pending = true;
  return true;
}

bool game_board_apply_uart_castle(bool kingside) {
  uint8_t king_row = (current_player == PLAYER_WHITE) ? 0u : 7u;
  uint8_t king_to_col = kingside ? 6u : 2u;
  position_move_t mv = {.from = (uint8_t)(king_row * 8u + 4u),
                        .to = (uint8_t)(king_row * 8u + king_to_col),
                        .promo = POS_PROMO_NONE};
  position_t *logic = game_logic_position();
  game_logic_copy_globals(logic);
  if (!position_apply(logic, mv)) {
    return false;
  }
  game_session_snap_refresh();
  game_set_fifty_move_counter(logic->halfmove);
  moves_without_capture = logic->halfmove;
  current_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
  return true;
}

bool game_board_apply_uart_promotion(uint8_t row, uint8_t col, char piece_char) {
  position_t *logic = game_logic_position();
  position_promo_t pr = char_to_promo(piece_char);
  if (logic == NULL) {
    return false;
  }
  game_logic_copy_globals(logic);
  uint8_t p = logic->board[row][col];
  uint8_t promoted = POS_EMPTY;
  if (p == POS_WHITE_PAWN) {
    switch (pr) {
    case POS_PROMO_R:
      promoted = POS_WHITE_ROOK;
      break;
    case POS_PROMO_B:
      promoted = POS_WHITE_BISHOP;
      break;
    case POS_PROMO_N:
      promoted = POS_WHITE_KNIGHT;
      break;
    default:
      promoted = POS_WHITE_QUEEN;
      break;
    }
  } else if (p == POS_BLACK_PAWN) {
    switch (pr) {
    case POS_PROMO_R:
      promoted = POS_BLACK_ROOK;
      break;
    case POS_PROMO_B:
      promoted = POS_BLACK_BISHOP;
      break;
    case POS_PROMO_N:
      promoted = POS_BLACK_KNIGHT;
      break;
    default:
      promoted = POS_BLACK_QUEEN;
      break;
    }
  } else {
    return false;
  }
  game_set_piece((int)row, (int)col, (piece_t)promoted);
  game_session_snap_refresh();
  game_set_fifty_move_counter(logic->halfmove);
  moves_without_capture = logic->halfmove;
  return true;
}

void game_board_run_after_reject(uint8_t to_row, uint8_t to_col) {
  (void)to_row;
  (void)to_col;
  /* board[] stays at last publish; machine keeps error state via error_recovery */
}

