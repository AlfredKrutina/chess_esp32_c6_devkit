/**
 * @file game_board_machine.c
 * @brief Physical lift/drop state machine over a read-only position snapshot.
 */

#include "game_board_machine.h"

#include <string.h>

static board_machine_t s_machine;
/* Physical lift, capture, and castle-animation state. */
castling_state_t castling_state = {0};
bool castle_animation_active = false;
uint8_t rook_from_row, rook_from_col, rook_to_row, rook_to_col;
bool piece_lifted = false;
uint8_t lifted_piece_row = 0;
uint8_t lifted_piece_col = 0;
piece_t lifted_piece = PIECE_EMPTY;
bool capture_in_progress = false; // Is capture in progress?
uint8_t capture_target_row = 0;   // Where to place your own figure
uint8_t capture_target_col = 0;
piece_t capture_removed_piece = PIECE_EMPTY; // What figure was taken
guided_capture_state_t guided_capture_state = {0};
bool opponent_piece_moved = false;
piece_t opponent_piece_type = PIECE_EMPTY;
uint8_t opponent_original_row = 0;
uint8_t opponent_original_col = 0;
uint8_t opponent_current_row = 0;
uint8_t opponent_current_col = 0;


static board_machine_out_t out_none(void) {
  board_machine_out_t o = {0};
  return o;
}

static board_machine_out_t out_reject(board_machine_reject_t reason) {
  board_machine_out_t o = {0};
  o.kind = BM_OUT_REJECT;
  o.reason = reason;
  return o;
}

static bool sq_valid(uint8_t sq) { return sq < 64; }

static uint8_t piece_at(const position_t *pos, uint8_t sq) {
  return pos->board[sq / 8u][sq % 8u];
}

static bool side_owns(const position_t *pos, uint8_t sq) {
  uint8_t p = piece_at(pos, sq);
  if (p == POS_EMPTY) {
    return false;
  }
  bool white = p <= POS_WHITE_KING;
  return (pos->side == 0) ? white : !white;
}

static bool is_king_castle_delta(int fc, int tc) {
  return (tc - fc) == 2 || (tc - fc) == -2;
}

static bool move_needs_promo_choice(const position_t *pos, uint8_t from,
                                    uint8_t to) {
  uint8_t p = piece_at(pos, from);
  uint8_t tr = to / 8u;
  if (p == POS_WHITE_PAWN && tr == 7) {
    return true;
  }
  if (p == POS_BLACK_PAWN && tr == 0) {
    return true;
  }
  return false;
}

static bool is_quiet_legal(const position_t *pos, position_move_t mv) {
  if (!position_legal(pos, mv)) {
    return false;
  }
  uint8_t cap = piece_at(pos, mv.to);
  if (cap != POS_EMPTY) {
    return false;
  }
  uint8_t p = piece_at(pos, mv.from);
  if ((p == POS_WHITE_KING || p == POS_BLACK_KING) &&
      is_king_castle_delta((int)(mv.from % 8u), (int)(mv.to % 8u))) {
    return false;
  }
  if (move_needs_promo_choice(pos, mv.from, mv.to)) {
    return false;
  }
  if (pos->ep_square != POSITION_SQUARE_NONE && mv.to == pos->ep_square) {
    return false;
  }
  return true;
}

board_machine_t *game_board_machine(void) { return &s_machine; }

void game_board_set_piece_lifted(bool lifted, uint8_t row, uint8_t col,
                                 piece_t piece) {
  board_machine_t *m = game_board_machine();
  if (lifted) {
    piece_lifted = true;
    lifted_piece_row = row;
    lifted_piece_col = col;
    lifted_piece = piece;
    if (sq_valid((uint8_t)(row * 8u + col))) {
      m->lifted_sq = (uint8_t)(row * 8u + col);
      m->state = BM_STATE_LIFTED;
    }
    return;
  }
  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;
  m->lifted_sq = 0xFF;
  if (m->state == BM_STATE_LIFTED) {
    m->state = BM_STATE_IDLE;
  }
}

void board_machine_reset(board_machine_t *m) {
  if (!m) {
    return;
  }
  memset(m, 0, sizeof(*m));
  m->state = BM_STATE_IDLE;
}

void board_machine_on_capture_victim_lifted(board_machine_t *m, uint8_t victim_sq) {
  if (!m || !sq_valid(victim_sq)) {
    return;
  }
  m->state = BM_STATE_WAIT_CAPTURE;
  m->capture_victim_sq = victim_sq;
}

void board_machine_on_castle_king_applied(board_machine_t *m, uint8_t rook_from,
                                          uint8_t rook_to) {
  if (!m) {
    return;
  }
  m->state = BM_STATE_WAIT_ROOK;
  m->rook_from_sq = rook_from;
  m->rook_to_sq = rook_to;
}

void board_machine_on_promo_pawn_applied(board_machine_t *m, uint8_t promo_sq) {
  if (!m) {
    return;
  }
  m->state = BM_STATE_WAIT_PROMO;
  m->promo_sq = promo_sq;
}

board_machine_out_t board_machine_lift(board_machine_t *m, const position_t *pos,
                                       uint8_t square) {
  if (!m || !pos || !sq_valid(square)) {
    return out_reject(BM_REJECT_ILLEGAL);
  }
  m->last_reject = BM_REJECT_NONE;

  if (m->state == BM_STATE_WAIT_PROMO) {
    return out_none();
  }

  if (m->state == BM_STATE_WAIT_CAPTURE) {
    if (square == m->capture_victim_sq) {
      return out_none();
    }
    m->lifted_sq = square;
    m->state = BM_STATE_LIFTED;
    return out_none();
  }

  if (m->state == BM_STATE_WAIT_ROOK) {
    if (square == m->rook_from_sq) {
      m->lifted_sq = square;
      return out_none();
    }
    return out_none();
  }

  if (piece_at(pos, square) == POS_EMPTY) {
    return out_reject(BM_REJECT_ILLEGAL);
  }

  m->lifted_sq = square;
  m->state = BM_STATE_LIFTED;
  return out_none();
}

board_machine_out_t board_machine_drop(board_machine_t *m, const position_t *pos,
                                       uint8_t from, uint8_t to, bool has_from) {
  if (!m || !pos || !sq_valid(to)) {
    return out_reject(BM_REJECT_ILLEGAL);
  }

  uint8_t from_sq = has_from ? from : m->lifted_sq;
  if (!sq_valid(from_sq)) {
    m->last_reject = BM_REJECT_NO_LIFT;
    return out_reject(BM_REJECT_NO_LIFT);
  }

  if (m->state == BM_STATE_WAIT_PROMO) {
    if (to != m->promo_sq) {
      return out_reject(BM_REJECT_PROMO_PENDING);
    }
    return out_none();
  }

  if (m->state == BM_STATE_WAIT_ROOK) {
    position_move_t mv = {.from = m->rook_from_sq,
                          .to = m->rook_to_sq,
                          .promo = POS_PROMO_NONE};
    if (from_sq != m->rook_from_sq || to != m->rook_to_sq) {
      m->last_reject = BM_REJECT_WRONG_ROOK_STEP;
      return out_reject(BM_REJECT_WRONG_ROOK_STEP);
    }
    if (!position_legal(pos, mv)) {
      return out_reject(BM_REJECT_ILLEGAL);
    }
    board_machine_out_t o = {0};
    o.kind = BM_OUT_MOVE;
    o.move = mv;
    m->state = BM_STATE_IDLE;
    return o;
  }

  if (m->state == BM_STATE_WAIT_CAPTURE) {
    position_move_t mv = {.from = from_sq, .to = to, .promo = POS_PROMO_NONE};
    if (to == m->capture_victim_sq) {
      m->last_reject = BM_REJECT_WRONG_CAPTURE_STEP;
      return out_reject(BM_REJECT_WRONG_CAPTURE_STEP);
    }
    if (!position_legal(pos, mv)) {
      m->last_reject = BM_REJECT_ILLEGAL;
      return out_reject(BM_REJECT_ILLEGAL);
    }
    board_machine_out_t o = {0};
    o.kind = BM_OUT_MOVE;
    o.move = mv;
    m->state = BM_STATE_IDLE;
    m->capture_victim_sq = 0xFF;
    return o;
  }

  position_move_t mv = {.from = from_sq, .to = to, .promo = POS_PROMO_NONE};

  if (from_sq == to) {
    m->state = BM_STATE_IDLE;
    return out_none();
  }

  uint8_t p = piece_at(pos, from_sq);
  int fc = (int)(from_sq % 8u);
  int tc = (int)(to % 8u);
  if ((p == POS_WHITE_KING || p == POS_BLACK_KING) && is_king_castle_delta(fc, tc)) {
    if (!position_legal(pos, mv)) {
      m->last_reject = BM_REJECT_ILLEGAL;
      return out_reject(BM_REJECT_ILLEGAL);
    }
    board_machine_out_t o = {0};
    o.kind = BM_OUT_MOVE;
    o.move = mv;
    uint8_t row = from_sq / 8u;
    if (tc > fc) {
      board_machine_on_castle_king_applied(m, (uint8_t)(row * 8u + 7u),
                                         (uint8_t)(row * 8u + 5u));
    } else {
      board_machine_on_castle_king_applied(m, (uint8_t)(row * 8u),
                                         (uint8_t)(row * 8u + 3u));
    }
    return o;
  }

  if (move_needs_promo_choice(pos, from_sq, to)) {
    if (!position_legal(pos, mv)) {
      m->last_reject = BM_REJECT_ILLEGAL;
      return out_reject(BM_REJECT_ILLEGAL);
    }
    board_machine_out_t o = {0};
    o.kind = BM_OUT_NEED_PROMO;
    o.move = mv;
    board_machine_on_promo_pawn_applied(m, to);
    return o;
  }

  if (is_quiet_legal(pos, mv)) {
    board_machine_out_t o = {0};
    o.kind = BM_OUT_MOVE;
    o.move = mv;
    m->state = BM_STATE_IDLE;
    m->lifted_sq = 0xFF;
    return o;
  }

  if (position_legal(pos, mv)) {
    board_machine_out_t o = {0};
    o.kind = BM_OUT_MOVE;
    o.move = mv;
    m->state = BM_STATE_IDLE;
    m->lifted_sq = 0xFF;
    return o;
  }

  m->last_reject = BM_REJECT_ILLEGAL;
  return out_reject(BM_REJECT_ILLEGAL);
}
