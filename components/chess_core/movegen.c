/**
 * @file movegen.c
 * @brief Attack scan and legality. Trials stay on a static copy of position_t.
 */

#include "position_internal.h"

#include <string.h>

static int on_board(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8;
}

static int is_white_piece(uint8_t piece) {
  return piece >= POS_WHITE_PAWN && piece <= POS_WHITE_KING;
}

static int is_black_piece(uint8_t piece) {
  return piece >= POS_BLACK_PAWN && piece <= POS_BLACK_KING;
}

bool position_sq_attacked(const uint8_t board[8][8], int row, int col, int by_white) {
  static const int knight[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                   {1, -2},  {1, 2},  {2, -1},  {2, 1}};
  static const int king[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                 {0, 1},   {1, -1}, {1, 0},  {1, 1}};
  static const int bishop[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  static const int rook[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  uint8_t pawn = by_white ? POS_WHITE_PAWN : POS_BLACK_PAWN;
  uint8_t knight_p = by_white ? POS_WHITE_KNIGHT : POS_BLACK_KNIGHT;
  uint8_t bishop_p = by_white ? POS_WHITE_BISHOP : POS_BLACK_BISHOP;
  uint8_t rook_p = by_white ? POS_WHITE_ROOK : POS_BLACK_ROOK;
  uint8_t queen_p = by_white ? POS_WHITE_QUEEN : POS_BLACK_QUEEN;
  uint8_t king_p = by_white ? POS_WHITE_KING : POS_BLACK_KING;
  int pawn_dir = by_white ? 1 : -1;
  int pawn_row = row - pawn_dir;
  int i;

  if (on_board(pawn_row, col - 1) && board[pawn_row][col - 1] == pawn) {
    return true;
  }
  if (on_board(pawn_row, col + 1) && board[pawn_row][col + 1] == pawn) {
    return true;
  }

  for (i = 0; i < 8; i++) {
    int nr = row + knight[i][0];
    int nc = col + knight[i][1];
    if (on_board(nr, nc) && board[nr][nc] == knight_p) {
      return true;
    }
  }

  for (i = 0; i < 4; i++) {
    int nr = row + bishop[i][0];
    int nc = col + bishop[i][1];
    while (on_board(nr, nc)) {
      uint8_t piece = board[nr][nc];
      if (piece != POS_EMPTY) {
        if (piece == bishop_p || piece == queen_p) {
          return true;
        }
        break;
      }
      nr += bishop[i][0];
      nc += bishop[i][1];
    }
  }

  for (i = 0; i < 4; i++) {
    int nr = row + rook[i][0];
    int nc = col + rook[i][1];
    while (on_board(nr, nc)) {
      uint8_t piece = board[nr][nc];
      if (piece != POS_EMPTY) {
        if (piece == rook_p || piece == queen_p) {
          return true;
        }
        break;
      }
      nr += rook[i][0];
      nc += rook[i][1];
    }
  }

  for (i = 0; i < 8; i++) {
    int nr = row + king[i][0];
    int nc = col + king[i][1];
    if (on_board(nr, nc) && board[nr][nc] == king_p) {
      return true;
    }
  }

  return false;
}

bool position_mover_in_check(const position_t *p, uint8_t side) {
  uint8_t king = (side == 0) ? POS_WHITE_KING : POS_BLACK_KING;
  int by_white = (side == 0) ? 0 : 1;
  int row;
  int col;

  if (p == NULL) {
    return false;
  }
  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      if (p->board[row][col] == king) {
        return position_sq_attacked(p->board, row, col, by_white);
      }
    }
  }
  return false;
}

bool position_legal(const position_t *p, position_move_t mv) {
  position_t *trial = position_core_trial();

  if (p == NULL) {
    return false;
  }
  *trial = *p;
  return position_apply(trial, mv);
}

static int own_piece(uint8_t piece, uint8_t side) {
  if (side == 0) {
    return is_white_piece(piece);
  }
  return is_black_piece(piece);
}

bool position_has_legal_move(const position_t *p) {
  static const int knight[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                   {1, -2},  {1, 2},  {2, -1},  {2, 1}};
  static const int king[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                 {0, 1},   {1, -1}, {1, 0},  {1, 1}};
  static const int bishop[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
  static const int rook[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  int row;
  int col;

  if (p == NULL) {
    return false;
  }

  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      uint8_t piece = p->board[row][col];
      position_move_t mv;
      int i;
      int dir_n = 0;
      const int (*dirs)[2] = NULL;

      if (!own_piece(piece, p->side)) {
        continue;
      }
      mv.from = (uint8_t)(row * 8 + col);
      mv.promo = POS_PROMO_NONE;

      if (piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN) {
        int dir = (piece == POS_WHITE_PAWN) ? 1 : -1;
        int tr = row + dir;
        int promo_row = (piece == POS_WHITE_PAWN) ? 7 : 0;
        int dc;

        if (on_board(tr, col) && p->board[tr][col] == POS_EMPTY) {
          mv.to = (uint8_t)(tr * 8 + col);
          if (tr == promo_row) {
            static const position_promo_t promos[4] = {
                POS_PROMO_Q, POS_PROMO_R, POS_PROMO_B, POS_PROMO_N};
            for (i = 0; i < 4; i++) {
              mv.promo = promos[i];
              if (position_legal(p, mv)) {
                return true;
              }
            }
            mv.promo = POS_PROMO_NONE;
          } else if (position_legal(p, mv)) {
            return true;
          }
          if ((piece == POS_WHITE_PAWN && row == 1) ||
              (piece == POS_BLACK_PAWN && row == 6)) {
            int jump = row + 2 * dir;
            if (p->board[jump][col] == POS_EMPTY) {
              mv.to = (uint8_t)(jump * 8 + col);
              mv.promo = POS_PROMO_NONE;
              if (position_legal(p, mv)) {
                return true;
              }
            }
          }
        }
        for (dc = -1; dc <= 1; dc += 2) {
          int tc = col + dc;
          if (!on_board(tr, tc)) {
            continue;
          }
          mv.to = (uint8_t)(tr * 8 + tc);
          if (tr == promo_row) {
            static const position_promo_t promos[4] = {
                POS_PROMO_Q, POS_PROMO_R, POS_PROMO_B, POS_PROMO_N};
            for (i = 0; i < 4; i++) {
              mv.promo = promos[i];
              if (position_legal(p, mv)) {
                return true;
              }
            }
            mv.promo = POS_PROMO_NONE;
          } else if (position_legal(p, mv)) {
            return true;
          }
        }
        continue;
      }

      if (piece == POS_WHITE_KNIGHT || piece == POS_BLACK_KNIGHT) {
        for (i = 0; i < 8; i++) {
          int tr = row + knight[i][0];
          int tc = col + knight[i][1];
          if (!on_board(tr, tc)) {
            continue;
          }
          mv.to = (uint8_t)(tr * 8 + tc);
          if (position_legal(p, mv)) {
            return true;
          }
        }
        continue;
      }

      if (piece == POS_WHITE_KING || piece == POS_BLACK_KING) {
        for (i = 0; i < 8; i++) {
          int tr = row + king[i][0];
          int tc = col + king[i][1];
          if (!on_board(tr, tc)) {
            continue;
          }
          mv.to = (uint8_t)(tr * 8 + tc);
          if (position_legal(p, mv)) {
            return true;
          }
        }
        mv.to = (uint8_t)(row * 8 + 6);
        if (position_legal(p, mv)) {
          return true;
        }
        mv.to = (uint8_t)(row * 8 + 2);
        if (position_legal(p, mv)) {
          return true;
        }
        continue;
      }

      if (piece == POS_WHITE_BISHOP || piece == POS_BLACK_BISHOP) {
        dirs = bishop;
        dir_n = 4;
      } else if (piece == POS_WHITE_ROOK || piece == POS_BLACK_ROOK) {
        dirs = rook;
        dir_n = 4;
      } else {
        dirs = bishop;
        dir_n = 4;
      }

      for (i = 0; i < dir_n; i++) {
        int tr = row + dirs[i][0];
        int tc = col + dirs[i][1];
        while (on_board(tr, tc)) {
          mv.to = (uint8_t)(tr * 8 + tc);
          if (position_legal(p, mv)) {
            return true;
          }
          if (p->board[tr][tc] != POS_EMPTY) {
            break;
          }
          tr += dirs[i][0];
          tc += dirs[i][1];
        }
      }
      if (piece == POS_WHITE_QUEEN || piece == POS_BLACK_QUEEN) {
        for (i = 0; i < 4; i++) {
          int tr = row + rook[i][0];
          int tc = col + rook[i][1];
          while (on_board(tr, tc)) {
            mv.to = (uint8_t)(tr * 8 + tc);
            if (position_legal(p, mv)) {
              return true;
            }
            if (p->board[tr][tc] != POS_EMPTY) {
              break;
            }
            tr += rook[i][0];
            tc += rook[i][1];
          }
        }
      }
    }
  }
  return false;
}
