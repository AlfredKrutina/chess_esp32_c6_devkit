/**
 * @file fen.c
 * @brief FEN load and save. A failed parse leaves the destination unchanged.
 */

#include "position.h"
#include "position_internal.h"

#include <stdio.h>
#include <string.h>

static int take_token(const char **pp, char *tok, size_t cap) {
  const char *p;
  size_t n = 0;

  if (pp == NULL || *pp == NULL || tok == NULL || cap < 2) {
    return 0;
  }
  p = *pp;
  while (*p == ' ') {
    p++;
  }
  if (*p == '\0') {
    tok[0] = '\0';
    *pp = p;
    return 0;
  }
  while (*p != '\0' && *p != ' ' && n + 1 < cap) {
    tok[n++] = *p++;
  }
  tok[n] = '\0';
  *pp = p;
  return 1;
}

static int parse_u32(const char *s, unsigned *out, unsigned maxv) {
  unsigned v = 0;

  if (s == NULL || *s == '\0' || out == NULL) {
    return 0;
  }
  for (; *s != '\0'; s++) {
    if (*s < '0' || *s > '9') {
      return 0;
    }
    v = v * 10u + (unsigned)(*s - '0');
    if (v > maxv) {
      return 0;
    }
  }
  *out = v;
  return 1;
}

static uint8_t piece_from_fen(char c) {
  switch (c) {
  case 'P':
    return POS_WHITE_PAWN;
  case 'N':
    return POS_WHITE_KNIGHT;
  case 'B':
    return POS_WHITE_BISHOP;
  case 'R':
    return POS_WHITE_ROOK;
  case 'Q':
    return POS_WHITE_QUEEN;
  case 'K':
    return POS_WHITE_KING;
  case 'p':
    return POS_BLACK_PAWN;
  case 'n':
    return POS_BLACK_KNIGHT;
  case 'b':
    return POS_BLACK_BISHOP;
  case 'r':
    return POS_BLACK_ROOK;
  case 'q':
    return POS_BLACK_QUEEN;
  case 'k':
    return POS_BLACK_KING;
  default:
    return 0xFF;
  }
}

static char fen_char(uint8_t piece) {
  switch (piece) {
  case POS_WHITE_PAWN:
    return 'P';
  case POS_WHITE_KNIGHT:
    return 'N';
  case POS_WHITE_BISHOP:
    return 'B';
  case POS_WHITE_ROOK:
    return 'R';
  case POS_WHITE_QUEEN:
    return 'Q';
  case POS_WHITE_KING:
    return 'K';
  case POS_BLACK_PAWN:
    return 'p';
  case POS_BLACK_KNIGHT:
    return 'n';
  case POS_BLACK_BISHOP:
    return 'b';
  case POS_BLACK_ROOK:
    return 'r';
  case POS_BLACK_QUEEN:
    return 'q';
  case POS_BLACK_KING:
    return 'k';
  default:
    return '?';
  }
}

static void infer_castling(position_t *out) {
  out->castling = 0;
  if (out->board[0][4] == POS_WHITE_KING && out->board[0][7] == POS_WHITE_ROOK) {
    out->castling |= POS_CASTLE_WK;
  }
  if (out->board[0][4] == POS_WHITE_KING && out->board[0][0] == POS_WHITE_ROOK) {
    out->castling |= POS_CASTLE_WQ;
  }
  if (out->board[7][4] == POS_BLACK_KING && out->board[7][7] == POS_BLACK_ROOK) {
    out->castling |= POS_CASTLE_BK;
  }
  if (out->board[7][4] == POS_BLACK_KING && out->board[7][0] == POS_BLACK_ROOK) {
    out->castling |= POS_CASTLE_BQ;
  }
}

static int apply_castle_token(position_t *out, const char *tok) {
  int seen_k = 0;
  int seen_q = 0;
  int seen_kk = 0;
  int seen_qq = 0;
  const char *c;

  out->castling = 0;
  if (tok[0] == '-' && tok[1] == '\0') {
    return 1;
  }
  for (c = tok; *c != '\0'; c++) {
    int ok = 0;
    if (*c == 'K' && !seen_k) {
      seen_k = 1;
      ok = out->board[0][4] == POS_WHITE_KING && out->board[0][7] == POS_WHITE_ROOK;
      out->castling |= POS_CASTLE_WK;
    } else if (*c == 'Q' && !seen_q) {
      seen_q = 1;
      ok = out->board[0][4] == POS_WHITE_KING && out->board[0][0] == POS_WHITE_ROOK;
      out->castling |= POS_CASTLE_WQ;
    } else if (*c == 'k' && !seen_kk) {
      seen_kk = 1;
      ok = out->board[7][4] == POS_BLACK_KING && out->board[7][7] == POS_BLACK_ROOK;
      out->castling |= POS_CASTLE_BK;
    } else if (*c == 'q' && !seen_qq) {
      seen_qq = 1;
      ok = out->board[7][4] == POS_BLACK_KING && out->board[7][0] == POS_BLACK_ROOK;
      out->castling |= POS_CASTLE_BQ;
    }
    if (!ok) {
      return 0;
    }
  }
  return tok[0] != '\0';
}

static int apply_ep_token(position_t *out, const char *tok) {
  int col;
  int rank;

  out->ep_square = POSITION_SQUARE_NONE;
  if (tok[0] == '-' && tok[1] == '\0') {
    return 1;
  }
  if (tok[0] < 'a' || tok[0] > 'h' || tok[2] != '\0') {
    return 0;
  }
  col = tok[0] - 'a';
  rank = tok[1] - '0';
  if (out->side == 0) {
    if (rank != 6) {
      return 0;
    }
    if (out->board[5][col] != POS_EMPTY || out->board[4][col] != POS_BLACK_PAWN) {
      return 0;
    }
    out->ep_square = (uint8_t)(5 * 8 + col);
    return 1;
  }
  if (rank != 3) {
    return 0;
  }
  if (out->board[2][col] != POS_EMPTY || out->board[3][col] != POS_WHITE_PAWN) {
    return 0;
  }
  out->ep_square = (uint8_t)(2 * 8 + col);
  return 1;
}

bool position_from_fen(position_t *p, const char *fen) {
  position_t *out = position_core_trial();
  const char *cursor;
  int row = 7;
  int col = 0;
  int kings_w = 0;
  int kings_b = 0;
  char tok[16];
  unsigned half = 0;
  unsigned full = 1;
  int r;
  int c;

  if (p == NULL || fen == NULL) {
    return false;
  }
  memset(out, 0, sizeof(*out));
  out->ep_square = POSITION_SQUARE_NONE;
  out->fullmove = 1;
  cursor = fen;
  while (*cursor != '\0' && *cursor != ' ') {
    if (*cursor == '/') {
      if (col != 8) {
        return false;
      }
      row--;
      col = 0;
      cursor++;
      continue;
    }
    if (*cursor >= '1' && *cursor <= '8') {
      col += (*cursor - '0');
      if (col > 8) {
        return false;
      }
      cursor++;
      continue;
    }
    if (row < 0 || row > 7 || col > 7) {
      return false;
    }
    {
      uint8_t piece = piece_from_fen(*cursor);
      if (piece == 0xFF) {
        return false;
      }
      out->board[row][col] = piece;
    }
    col++;
    cursor++;
  }
  if (row != 0 || col != 8 || *cursor != ' ') {
    return false;
  }
  cursor++;
  if (*cursor == 'w') {
    out->side = 0;
  } else if (*cursor == 'b') {
    out->side = 1;
  } else {
    return false;
  }
  cursor++;
  if (*cursor != '\0' && *cursor != ' ') {
    return false;
  }

  for (r = 0; r < 8; r++) {
    for (c = 0; c < 8; c++) {
      uint8_t piece = out->board[r][c];
      if (piece == POS_WHITE_KING) {
        kings_w++;
      } else if (piece == POS_BLACK_KING) {
        kings_b++;
      } else if ((piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN) &&
                 (r == 0 || r == 7)) {
        return false;
      }
    }
  }
  if (kings_w != 1 || kings_b != 1) {
    return false;
  }

  if (!take_token(&cursor, tok, sizeof(tok))) {
    infer_castling(out);
    *p = *out;
    return true;
  }
  if (!apply_castle_token(out, tok)) {
    return false;
  }
  if (!take_token(&cursor, tok, sizeof(tok))) {
    *p = *out;
    return true;
  }
  if (!apply_ep_token(out, tok)) {
    return false;
  }
  if (!take_token(&cursor, tok, sizeof(tok))) {
    *p = *out;
    return true;
  }
  if (!parse_u32(tok, &half, 999u)) {
    return false;
  }
  out->halfmove = (uint16_t)half;
  if (!take_token(&cursor, tok, sizeof(tok))) {
    *p = *out;
    return true;
  }
  if (!parse_u32(tok, &full, 9999u) || full < 1u) {
    return false;
  }
  out->fullmove = (uint16_t)full;
  while (*cursor == ' ') {
    cursor++;
  }
  if (*cursor != '\0') {
    return false;
  }
  *p = *out;
  return true;
}

bool position_to_fen(const position_t *p, char *out, size_t out_cap) {
  char board[96];
  char castle[8];
  char ep[4];
  int row;
  int n = 0;
  int ci = 0;

  if (out != NULL && out_cap > 0) {
    out[0] = '\0';
  }
  if (p == NULL || out == NULL || out_cap < 128) {
    return false;
  }

  board[0] = '\0';
  for (row = 7; row >= 0; row--) {
    int col;
    int empty = 0;
    for (col = 0; col < 8; col++) {
      uint8_t piece = p->board[row][col];
      if (piece == POS_EMPTY) {
        empty++;
        continue;
      }
      if (empty > 0) {
        board[n++] = (char)('0' + empty);
        empty = 0;
      }
      board[n++] = fen_char(piece);
    }
    if (empty > 0) {
      board[n++] = (char)('0' + empty);
    }
    if (row > 0) {
      board[n++] = '/';
    }
  }
  board[n] = '\0';

  if (p->castling & POS_CASTLE_WK) {
    castle[ci++] = 'K';
  }
  if (p->castling & POS_CASTLE_WQ) {
    castle[ci++] = 'Q';
  }
  if (p->castling & POS_CASTLE_BK) {
    castle[ci++] = 'k';
  }
  if (p->castling & POS_CASTLE_BQ) {
    castle[ci++] = 'q';
  }
  if (ci == 0) {
    castle[ci++] = '-';
  }
  castle[ci] = '\0';

  if (p->ep_square >= 64) {
    ep[0] = '-';
    ep[1] = '\0';
  } else {
    ep[0] = (char)('a' + (p->ep_square % 8));
    ep[1] = (char)('1' + (p->ep_square / 8));
    ep[2] = '\0';
  }

  if (snprintf(out, out_cap, "%s %c %s %s %u %u", board, p->side == 0 ? 'w' : 'b',
               castle, ep, (unsigned)p->halfmove,
               (unsigned)(p->fullmove == 0 ? 1 : p->fullmove)) < 0) {
    out[0] = '\0';
    return false;
  }
  return true;
}
