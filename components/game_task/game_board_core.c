/**
 * @file game_board_core.c
 * @brief Board setup, FEN parsing, and piece/position helpers.
 */

#include "game_board_core.h"
#include "game_task_internal.h"

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

bool game_load_position_from_fen(const char *fen, player_t *active_player) {
  if (fen == NULL || active_player == NULL) {
    return false;
  }

  memset(board, 0, sizeof(board));

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
    board[row][col] = piece;
    col++;
    p++;
  }

  if (row != 0 || col != 8 || *p != ' ') {
    return false;
  }

  p++;
  if (*p == 'w') {
    *active_player = PLAYER_WHITE;
  } else if (*p == 'b') {
    *active_player = PLAYER_BLACK;
  } else {
    return false;
  }
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
