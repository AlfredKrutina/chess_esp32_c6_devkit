/**
 * @file game_board_core.c
 * @brief Board setup, FEN parsing, and piece/position helpers.
 */

#include "game_board_core.h"
#include "game_task_internal.h"
#include "game_hall_position.h"

#include "game_task.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "GAME_BOARD";

void game_initialize_board(void) {
  ESP_LOGI(TAG, "Initializing enhanced chess board...");

  memset(board, 0, sizeof(board));
  memset(piece_moved, 0, sizeof(piece_moved));

  board[0][0] = PIECE_WHITE_ROOK;
  board[0][1] = PIECE_WHITE_KNIGHT;
  board[0][2] = PIECE_WHITE_BISHOP;
  board[0][3] = PIECE_WHITE_QUEEN;
  board[0][4] = PIECE_WHITE_KING;
  board[0][5] = PIECE_WHITE_BISHOP;
  board[0][6] = PIECE_WHITE_KNIGHT;
  board[0][7] = PIECE_WHITE_ROOK;

  for (int col = 0; col < 8; col++) {
    board[1][col] = PIECE_WHITE_PAWN;
  }

  for (int col = 0; col < 8; col++) {
    board[6][col] = PIECE_BLACK_PAWN;
  }

  board[7][0] = PIECE_BLACK_ROOK;
  board[7][1] = PIECE_BLACK_KNIGHT;
  board[7][2] = PIECE_BLACK_BISHOP;
  board[7][3] = PIECE_BLACK_QUEEN;
  board[7][4] = PIECE_BLACK_KING;
  board[7][5] = PIECE_BLACK_BISHOP;
  board[7][6] = PIECE_BLACK_KNIGHT;
  board[7][7] = PIECE_BLACK_ROOK;

  current_player = PLAYER_WHITE;
  current_game_state = GAME_STATE_ACTIVE;
  move_count = 0;

  white_king_moved = false;
  white_rook_a_moved = false;
  white_rook_h_moved = false;
  black_king_moved = false;
  black_rook_a_moved = false;
  black_rook_h_moved = false;

  memset(&castling_state, 0, sizeof(castling_state));

  ESP_LOGI(TAG, "Enhanced chess board initialized successfully");
  ESP_LOGI(TAG,
           "Initial position: White pieces at bottom, Black pieces at top");

  game_check_promotion_needed();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_snapshot_last_valid();
#endif
}

static bool game_parse_piece_from_fen(char c, piece_t *out_piece) {
  if (out_piece == NULL) {
    return false;
  }
  switch (c) {
  case 'P':
    *out_piece = PIECE_WHITE_PAWN;
    return true;
  case 'N':
    *out_piece = PIECE_WHITE_KNIGHT;
    return true;
  case 'B':
    *out_piece = PIECE_WHITE_BISHOP;
    return true;
  case 'R':
    *out_piece = PIECE_WHITE_ROOK;
    return true;
  case 'Q':
    *out_piece = PIECE_WHITE_QUEEN;
    return true;
  case 'K':
    *out_piece = PIECE_WHITE_KING;
    return true;
  case 'p':
    *out_piece = PIECE_BLACK_PAWN;
    return true;
  case 'n':
    *out_piece = PIECE_BLACK_KNIGHT;
    return true;
  case 'b':
    *out_piece = PIECE_BLACK_BISHOP;
    return true;
  case 'r':
    *out_piece = PIECE_BLACK_ROOK;
    return true;
  case 'q':
    *out_piece = PIECE_BLACK_QUEEN;
    return true;
  case 'k':
    *out_piece = PIECE_BLACK_KING;
    return true;
  default:
    return false;
  }
}

static bool fen_take_token(const char **pp, char *tok, size_t cap) {
  const char *p = *pp;
  while (*p == ' ') {
    p++;
  }
  if (*p == '\0') {
    *pp = p;
    return false;
  }
  size_t n = 0;
  while (*p != '\0' && *p != ' ') {
    if (n + 1 >= cap) {
      return false;
    }
    tok[n++] = *p++;
  }
  tok[n] = '\0';
  *pp = p;
  return true;
}

static bool fen_parse_u32(const char *s, uint32_t *out, uint32_t maxv) {
  if (s == NULL || *s < '0' || *s > '9') {
    return false;
  }
  uint32_t v = 0;
  for (; *s != '\0'; s++) {
    if (*s < '0' || *s > '9') {
      return false;
    }
    v = v * 10U + (uint32_t)(*s - '0');
    if (v > maxv) {
      return false;
    }
  }
  *out = v;
  return true;
}

static void fen_infer_castling(game_fen_position_t *out) {
  bool wk = out->squares[0][4] == PIECE_WHITE_KING &&
            out->squares[0][7] == PIECE_WHITE_ROOK;
  bool wq = out->squares[0][4] == PIECE_WHITE_KING &&
            out->squares[0][0] == PIECE_WHITE_ROOK;
  bool bk = out->squares[7][4] == PIECE_BLACK_KING &&
            out->squares[7][7] == PIECE_BLACK_ROOK;
  bool bq = out->squares[7][4] == PIECE_BLACK_KING &&
            out->squares[7][0] == PIECE_BLACK_ROOK;
  out->white_rook_h_moved = !wk;
  out->white_rook_a_moved = !wq;
  out->white_king_moved = !(wk || wq);
  out->black_rook_h_moved = !bk;
  out->black_rook_a_moved = !bq;
  out->black_king_moved = !(bk || bq);
}

static bool fen_apply_castling_token(game_fen_position_t *out, const char *tok) {
  out->white_king_moved = true;
  out->white_rook_a_moved = true;
  out->white_rook_h_moved = true;
  out->black_king_moved = true;
  out->black_rook_a_moved = true;
  out->black_rook_h_moved = true;
  if (tok[0] == '-' && tok[1] == '\0') {
    return true;
  }
  bool seen_k = false, seen_q = false, seen_kk = false, seen_qq = false;
  for (const char *c = tok; *c != '\0'; c++) {
    bool ok_piece = false;
    if (*c == 'K' && !seen_k) {
      seen_k = true;
      ok_piece = out->squares[0][4] == PIECE_WHITE_KING &&
                 out->squares[0][7] == PIECE_WHITE_ROOK;
      out->white_rook_h_moved = false;
      out->white_king_moved = false;
    } else if (*c == 'Q' && !seen_q) {
      seen_q = true;
      ok_piece = out->squares[0][4] == PIECE_WHITE_KING &&
                 out->squares[0][0] == PIECE_WHITE_ROOK;
      out->white_rook_a_moved = false;
      out->white_king_moved = false;
    } else if (*c == 'k' && !seen_kk) {
      seen_kk = true;
      ok_piece = out->squares[7][4] == PIECE_BLACK_KING &&
                 out->squares[7][7] == PIECE_BLACK_ROOK;
      out->black_rook_h_moved = false;
      out->black_king_moved = false;
    } else if (*c == 'q' && !seen_qq) {
      seen_qq = true;
      ok_piece = out->squares[7][4] == PIECE_BLACK_KING &&
                 out->squares[7][0] == PIECE_BLACK_ROOK;
      out->black_rook_a_moved = false;
      out->black_king_moved = false;
    }
    if (!ok_piece) {
      return false;
    }
  }
  return tok[0] != '\0';
}

static bool fen_apply_ep_token(game_fen_position_t *out, const char *tok) {
  out->ep = false;
  if (tok[0] == '-' && tok[1] == '\0') {
    return true;
  }
  if (tok[0] < 'a' || tok[0] > 'h' || tok[2] != '\0') {
    return false;
  }
  int col = tok[0] - 'a';
  int rank = tok[1] - '0';
  if (out->side == PLAYER_WHITE) {
    if (rank != 6) {
      return false;
    }
    if (out->squares[5][col] != PIECE_EMPTY ||
        out->squares[4][col] != PIECE_BLACK_PAWN) {
      return false;
    }
    out->ep = true;
    out->ep_row = 5;
    out->ep_col = (uint8_t)col;
    out->victim_row = 4;
    out->victim_col = (uint8_t)col;
    return true;
  }
  if (rank != 3) {
    return false;
  }
  if (out->squares[2][col] != PIECE_EMPTY ||
      out->squares[3][col] != PIECE_WHITE_PAWN) {
    return false;
  }
  out->ep = true;
  out->ep_row = 2;
  out->ep_col = (uint8_t)col;
  out->victim_row = 3;
  out->victim_col = (uint8_t)col;
  return true;
}

static void fen_commit_meta(const game_fen_position_t *pos) {
  current_player = pos->side;
  white_king_moved = pos->white_king_moved;
  white_rook_a_moved = pos->white_rook_a_moved;
  white_rook_h_moved = pos->white_rook_h_moved;
  black_king_moved = pos->black_king_moved;
  black_rook_a_moved = pos->black_rook_a_moved;
  black_rook_h_moved = pos->black_rook_h_moved;
  piece_moved[0][4] = pos->white_king_moved;
  piece_moved[0][0] = pos->white_rook_a_moved;
  piece_moved[0][7] = pos->white_rook_h_moved;
  piece_moved[7][4] = pos->black_king_moved;
  piece_moved[7][0] = pos->black_rook_a_moved;
  piece_moved[7][7] = pos->black_rook_h_moved;
  en_passant_available = pos->ep;
  en_passant_target_row = pos->ep_row;
  en_passant_target_col = pos->ep_col;
  en_passant_victim_row = pos->victim_row;
  en_passant_victim_col = pos->victim_col;
  game_set_fifty_move_counter(pos->halfmove);
  moves_without_capture = pos->halfmove;
  move_count = pos->move_count;
  position_history_count = 0;
  game_add_position_to_history();
}

bool game_parse_fen(const char *fen, game_fen_position_t *out) {
  if (fen == NULL || out == NULL) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  int row = 7;
  int col = 0;
  const char *p = fen;
  while (*p != '\0' && *p != ' ') {
    if (*p == '/') {
      if (col != 8) {
        return false;
      }
      row--;
      col = 0;
      p++;
      continue;
    }
    if (*p >= '1' && *p <= '8') {
      col += (*p - '0');
      if (col > 8) {
        return false;
      }
      p++;
      continue;
    }
    piece_t piece = PIECE_EMPTY;
    if (!game_parse_piece_from_fen(*p, &piece)) {
      return false;
    }
    if (row < 0 || row > 7 || col < 0 || col > 7) {
      return false;
    }
    out->squares[row][col] = piece;
    col++;
    p++;
  }
  if (row != 0 || col != 8 || *p != ' ') {
    return false;
  }
  p++;
  if (*p == 'w') {
    out->side = PLAYER_WHITE;
  } else if (*p == 'b') {
    out->side = PLAYER_BLACK;
  } else {
    return false;
  }
  p++;
  if (*p != '\0' && *p != ' ') {
    return false;
  }

  int kings_w = 0;
  int kings_b = 0;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      piece_t piece = out->squares[r][c];
      if (piece == PIECE_WHITE_KING) {
        kings_w++;
      } else if (piece == PIECE_BLACK_KING) {
        kings_b++;
      } else if ((piece == PIECE_WHITE_PAWN || piece == PIECE_BLACK_PAWN) &&
                 (r == 0 || r == 7)) {
        return false;
      }
    }
  }
  if (kings_w != 1 || kings_b != 1) {
    return false;
  }

  char tok[16];
  bool have_castle = fen_take_token(&p, tok, sizeof(tok));
  if (!have_castle) {
    fen_infer_castling(out);
    return true;
  }
  if (!fen_apply_castling_token(out, tok)) {
    return false;
  }
  if (!fen_take_token(&p, tok, sizeof(tok))) {
    return true;
  }
  if (!fen_apply_ep_token(out, tok)) {
    return false;
  }
  if (!fen_take_token(&p, tok, sizeof(tok))) {
    return true;
  }
  if (!fen_parse_u32(tok, &out->halfmove, 999U)) {
    return false;
  }
  if (!fen_take_token(&p, tok, sizeof(tok))) {
    return true;
  }
  uint32_t fullmove = 0;
  if (!fen_parse_u32(tok, &fullmove, 9999U) || fullmove < 1U) {
    return false;
  }
  out->move_count =
      (fullmove - 1U) * 2U + (out->side == PLAYER_BLACK ? 1U : 0U);
  while (*p == ' ') {
    p++;
  }
  return *p == '\0';
}

bool game_load_position_from_fen(const char *fen, player_t *active_player) {
  game_fen_position_t pos;
  if (active_player == NULL || !game_parse_fen(fen, &pos)) {
    return false;
  }
  memcpy(board, pos.squares, sizeof(board));
  fen_commit_meta(&pos);
  *active_player = pos.side;
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
  return board[row][col];
}

void game_set_piece(int row, int col, piece_t piece) {
  if (!game_is_valid_position(row, col)) {
    return;
  }
  board[row][col] = piece;
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
