/**
 * @file game_board_core.c
 * @brief Board setup, FEN parsing, and piece/position helpers.
 */

#include "game_board_core.h"
#include "game_hall_position.h"

#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_snapshot.h"
#include "position.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "GAME_BOARD";
/* Position mirror: side, castling rights, en passant. */
game_state_t current_game_state = GAME_STATE_IDLE;
player_t current_player = PLAYER_WHITE;
uint32_t move_count = 0;
bool game_active = false;
bool white_king_moved = false;
bool white_rook_a_moved = false; // a1 rook
bool white_rook_h_moved = false; // h1 rook
bool black_king_moved = false;
bool black_rook_a_moved = false; // a8 rook
bool black_rook_h_moved = false; // h8 rook
bool en_passant_available = false;
uint8_t en_passant_target_row = 0;
uint8_t en_passant_target_col = 0;
uint8_t en_passant_victim_row = 0;
uint8_t en_passant_victim_col = 0;
bool piece_moved[8][8] = {false};
uint32_t moves_without_capture = 0;
uint32_t max_moves_without_capture = 0;
uint32_t position_history_count = 0;

static position_t s_logic;
static position_t s_game_scratch_pos;
_Static_assert(sizeof(position_t) <= 4096, "position_t size");

void game_initialize_board(void) {
  ESP_LOGI(TAG, "Initializing enhanced chess board...");

  memset(piece_moved, 0, sizeof(piece_moved));
  game_logic_reset();
  current_game_state = GAME_STATE_ACTIVE;
  move_count = 0;

  memset(&castling_state, 0, sizeof(castling_state));

  ESP_LOGI(TAG, "Enhanced chess board initialized successfully");
  ESP_LOGI(TAG,
           "Initial position: White pieces at bottom, Black pieces at top");

  game_check_promotion_needed();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_snapshot_last_valid();
#endif
}

bool game_parse_fen(const char *fen, game_fen_position_t *out) {
  position_t *parsed = game_logic_scratch_position();
  int row;
  int col;
  int victim;

  if (fen == NULL || out == NULL || parsed == NULL ||
      !position_from_fen(parsed, fen)) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      out->squares[row][col] = (piece_t)parsed->board[row][col];
    }
  }
  out->side = (player_t)parsed->side;
  out->white_rook_h_moved = (parsed->castling & POS_CASTLE_WK) == 0;
  out->white_rook_a_moved = (parsed->castling & POS_CASTLE_WQ) == 0;
  out->white_king_moved = out->white_rook_h_moved && out->white_rook_a_moved;
  out->black_rook_h_moved = (parsed->castling & POS_CASTLE_BK) == 0;
  out->black_rook_a_moved = (parsed->castling & POS_CASTLE_BQ) == 0;
  out->black_king_moved = out->black_rook_h_moved && out->black_rook_a_moved;
  if (parsed->ep_square >= 64) {
    out->ep = false;
  } else {
    out->ep = true;
    out->ep_row = (uint8_t)(parsed->ep_square / 8);
    out->ep_col = (uint8_t)(parsed->ep_square % 8);
    victim = (parsed->side == 0) ? ((int)out->ep_row - 1) : ((int)out->ep_row + 1);
    if (victim >= 0 && victim <= 7) {
      out->victim_row = (uint8_t)victim;
      out->victim_col = out->ep_col;
    }
  }
  out->halfmove = parsed->halfmove;
  out->move_count = (parsed->fullmove == 0)
                        ? 0
                        : (uint32_t)(parsed->fullmove - 1) * 2U +
                              (parsed->side == 1 ? 1U : 0U);
  return true;
}

bool game_load_position_from_fen(const char *fen, player_t *active_player) {
  position_t *loaded = game_logic_scratch_position();

  if (active_player == NULL || loaded == NULL ||
      !position_from_fen(loaded, fen)) {
    return false;
  }
  s_logic = *loaded;
  game_session_snap_refresh();
  piece_moved[0][4] = white_king_moved;
  piece_moved[0][0] = white_rook_a_moved;
  piece_moved[0][7] = white_rook_h_moved;
  piece_moved[7][4] = black_king_moved;
  piece_moved[7][0] = black_rook_a_moved;
  piece_moved[7][7] = black_rook_h_moved;
  game_set_fifty_move_counter(s_logic.halfmove);
  moves_without_capture = s_logic.halfmove;
  move_count = (s_logic.fullmove == 0)
                   ? 0
                   : (uint32_t)(s_logic.fullmove - 1) * 2U +
                         (s_logic.side == 1 ? 1U : 0U);
  position_history_count = 0;
  game_add_position_to_history();
  *active_player = (player_t)s_logic.side;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_snapshot_last_valid();
#endif
  return true;
}

bool game_is_valid_position(int row, int col) {
  return (row >= 0 && row < 8 && col >= 0 && col < 8);
}

piece_t game_get_piece(int row, int col) {
  if (!game_is_valid_position(row, col)) {
    return PIECE_EMPTY;
  }
  const position_t *pos = game_logic_position();
  if (pos == NULL) {
    return PIECE_EMPTY;
  }
  return (piece_t)pos->board[row][col];
}

/* Only non-move store of s_logic.board. Move commits use position_apply. */
static void game_logic_store_square(int row, int col, uint8_t piece) {
  s_logic.board[row][col] = piece;
}

void game_set_piece(int row, int col, piece_t piece) {
  if (!game_is_valid_position(row, col)) {
    return;
  }
  game_logic_store_square(row, col, (uint8_t)piece);
}

bool game_is_empty(int row, int col) {
  return game_get_piece(row, col) == PIECE_EMPTY;
}

bool game_is_white_piece(piece_t piece) {
  return (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
}

bool game_is_black_piece(piece_t piece) {
  return (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
}

bool game_is_same_color(piece_t piece1, piece_t piece2) {
  if (piece1 == PIECE_EMPTY || piece2 == PIECE_EMPTY) {
    return false;
  }
  return (game_is_white_piece(piece1) && game_is_white_piece(piece2)) ||
         (game_is_black_piece(piece1) && game_is_black_piece(piece2));
}

bool game_is_opponent_piece(piece_t piece, player_t player) {
  if (piece == PIECE_EMPTY) {
    return false;
  }

  if (player == PLAYER_WHITE) {
    return game_is_black_piece(piece);
  }
  return game_is_white_piece(piece);
}

/* square 0..63: row = square/8 (rank 1 at row 0), col = square%8 (file a). */
static void game_square_to_row_col(uint8_t square, uint8_t *row, uint8_t *col) {
  *row = (uint8_t)(square / 8);
  *col = (uint8_t)(square % 8);
}

void game_logic_sync_globals_from_position(const position_t *p) {
  uint8_t ep_row;
  uint8_t ep_col;
  int victim_row;

  if (p == NULL) {
    return;
  }
  current_player = (player_t)p->side;
  white_rook_h_moved = (p->castling & POS_CASTLE_WK) == 0;
  white_rook_a_moved = (p->castling & POS_CASTLE_WQ) == 0;
  /* Castling rights imply the king has not moved. If both rights are gone
   * and the king left e1, it has moved. If both rooks moved but the king
   * is still on e1, keep the previous flag. */
  if ((p->castling & (POS_CASTLE_WK | POS_CASTLE_WQ)) != 0) {
    white_king_moved = false;
  } else if (p->board[0][4] != POS_WHITE_KING) {
    white_king_moved = true;
  }
  black_rook_h_moved = (p->castling & POS_CASTLE_BK) == 0;
  black_rook_a_moved = (p->castling & POS_CASTLE_BQ) == 0;
  if ((p->castling & (POS_CASTLE_BK | POS_CASTLE_BQ)) != 0) {
    black_king_moved = false;
  } else if (p->board[7][4] != POS_BLACK_KING) {
    black_king_moved = true;
  }
  if (p->ep_square >= 64) {
    en_passant_available = false;
    en_passant_target_row = 0;
    en_passant_target_col = 0;
    en_passant_victim_row = 0;
    en_passant_victim_col = 0;
  } else {
    game_square_to_row_col(p->ep_square, &ep_row, &ep_col);
    victim_row =
        (p->side == PLAYER_BLACK) ? ((int)ep_row + 1) : ((int)ep_row - 1);
    if (victim_row < 0 || victim_row > 7) {
      en_passant_available = false;
    } else {
      en_passant_available = true;
      en_passant_target_row = ep_row;
      en_passant_target_col = ep_col;
      en_passant_victim_row = (uint8_t)victim_row;
      en_passant_victim_col = ep_col;
    }
  }
}

static void game_logic_fill(position_t *p, bool clear_hist) {
  uint8_t castle = 0;
  uint32_t half;

  if (p == NULL) {
    return;
  }
  if (clear_hist) {
    memset(p, 0, sizeof(*p));
  }
  p->side = (uint8_t)current_player;
  if (!white_rook_h_moved) {
    castle |= POS_CASTLE_WK;
  }
  if (!white_rook_a_moved) {
    castle |= POS_CASTLE_WQ;
  }
  if (!black_rook_h_moved) {
    castle |= POS_CASTLE_BK;
  }
  if (!black_rook_a_moved) {
    castle |= POS_CASTLE_BQ;
  }
  p->castling = castle;
  if (!en_passant_available || en_passant_target_row > 7 || en_passant_target_col > 7) {
    p->ep_square = POSITION_SQUARE_NONE;
  } else {
    p->ep_square = (uint8_t)(en_passant_target_row * 8 + en_passant_target_col);
  }
  half = game_get_fifty_move_counter();
  p->halfmove = (uint16_t)(half > 65535u ? 65535u : half);
  p->fullmove = (uint16_t)(move_count / 2u + 1u);
  if (p->fullmove == 0) {
    p->fullmove = 1;
  }
  if (clear_hist) {
    p->hist_len = 0;
  }
}

static void game_logic_sync_clock(const position_t *p) {
  if (p == NULL) {
    return;
  }
  game_set_fifty_move_counter(p->halfmove);
  moves_without_capture = p->halfmove;
}

void game_logic_copy_globals(position_t *out) {
  if (out == NULL || game_logic_position() == NULL) {
    return;
  }
  *out = *game_logic_position();
}

void game_logic_adopt_globals(void) { game_logic_fill(&s_logic, false); }

position_t *game_logic_position(void) { return &s_logic; }

position_t *game_logic_scratch_position(void) { return &s_game_scratch_pos; }

void game_logic_reset(void) {
  position_reset(&s_logic);
  game_session_snap_refresh();
  game_logic_sync_clock(&s_logic);
}

bool game_logic_play(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                     uint8_t to_col, int promo) {
  position_move_t mv;

  if (castling_state.in_progress && from_row == castling_state.rook_from_row &&
      from_col == castling_state.rook_from_col &&
      to_row == castling_state.rook_to_row &&
      to_col == castling_state.rook_to_col) {
    game_session_snap_refresh();
    game_logic_sync_clock(&s_logic);
    return true;
  }
  if (castling_state.in_progress && s_logic.hist_len > 0) {
    (void)position_undo(&s_logic);
  }
  game_logic_fill(&s_logic, false);
  mv.from = (uint8_t)(from_row * 8u + from_col);
  mv.to = (uint8_t)(to_row * 8u + to_col);
  mv.promo = (position_promo_t)promo;
  if (mv.from >= 64 || mv.to >= 64 || !position_apply(&s_logic, mv)) {
    return false;
  }
  game_logic_sync_globals_from_position(&s_logic);
  game_session_snap_refresh();
  game_logic_sync_clock(&s_logic);
  return true;
}
