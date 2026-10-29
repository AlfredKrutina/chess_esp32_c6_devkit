/**
 * @file position.c
 * @brief Reset, apply, undo, and status. Check trials use a static copy.
 */

#include "position_internal.h"

#include <string.h>

/* See position_internal.h. One copy of each; do not alias them. */
static position_t s_core_trial;
static position_t s_core_apply;

position_t *position_core_trial(void) { return &s_core_trial; }

position_t *position_core_apply(void) { return &s_core_apply; }

void position_reset(position_t *p) {
  int col;

  if (p == NULL) {
    return;
  }

  memset(p, 0, sizeof(*p));
  p->castling = (uint8_t)(POS_CASTLE_WK | POS_CASTLE_WQ | POS_CASTLE_BK | POS_CASTLE_BQ);
  p->ep_square = POSITION_SQUARE_NONE;
  p->fullmove = 1;

  p->board[0][0] = POS_WHITE_ROOK;
  p->board[0][1] = POS_WHITE_KNIGHT;
  p->board[0][2] = POS_WHITE_BISHOP;
  p->board[0][3] = POS_WHITE_QUEEN;
  p->board[0][4] = POS_WHITE_KING;
  p->board[0][5] = POS_WHITE_BISHOP;
  p->board[0][6] = POS_WHITE_KNIGHT;
  p->board[0][7] = POS_WHITE_ROOK;
  p->board[7][0] = POS_BLACK_ROOK;
  p->board[7][1] = POS_BLACK_KNIGHT;
  p->board[7][2] = POS_BLACK_BISHOP;
  p->board[7][3] = POS_BLACK_QUEEN;
  p->board[7][4] = POS_BLACK_KING;
  p->board[7][5] = POS_BLACK_BISHOP;
  p->board[7][6] = POS_BLACK_KNIGHT;
  p->board[7][7] = POS_BLACK_ROOK;

  for (col = 0; col < 8; col++) {
    p->board[1][col] = POS_WHITE_PAWN;
    p->board[6][col] = POS_BLACK_PAWN;
  }
}

static int on_board(int row, int col) {
  return row >= 0 && row < 8 && col >= 0 && col < 8;
}

static int white_piece(uint8_t piece) {
  return piece >= POS_WHITE_PAWN && piece <= POS_WHITE_KING;
}

static int black_piece(uint8_t piece) {
  return piece >= POS_BLACK_PAWN && piece <= POS_BLACK_KING;
}

static int same_side(uint8_t piece, uint8_t side) {
  if (side == 0) {
    return white_piece(piece);
  }
  return black_piece(piece);
}

static uint8_t promo_piece(uint8_t pawn, position_promo_t promo) {
  int white = pawn == POS_WHITE_PAWN;
  switch (promo) {
  case POS_PROMO_R:
    return white ? POS_WHITE_ROOK : POS_BLACK_ROOK;
  case POS_PROMO_B:
    return white ? POS_WHITE_BISHOP : POS_BLACK_BISHOP;
  case POS_PROMO_N:
    return white ? POS_WHITE_KNIGHT : POS_BLACK_KNIGHT;
  case POS_PROMO_Q:
    return white ? POS_WHITE_QUEEN : POS_BLACK_QUEEN;
  default:
    return pawn;
  }
}

static void push_hist(position_t *p, position_move_t mv, uint8_t captured) {
  position_hist_t *h;

  if (p->hist_len >= POSITION_HIST_CAP) {
    memmove(&p->hist[0], &p->hist[1],
            sizeof(position_hist_t) * (POSITION_HIST_CAP - 1));
    p->hist_len = POSITION_HIST_CAP - 1;
  }
  h = &p->hist[p->hist_len++];
  h->mv = mv;
  h->captured = captured;
  h->castling_before = p->castling;
  h->ep_before = p->ep_square;
  h->side_before = p->side;
  h->halfmove_before = p->halfmove;
  h->fullmove_before = p->fullmove;
}

static void clear_right_on_square(position_t *p, int row, int col) {
  if (row == 0 && col == 0) {
    p->castling &= (uint8_t)~POS_CASTLE_WQ;
  } else if (row == 0 && col == 7) {
    p->castling &= (uint8_t)~POS_CASTLE_WK;
  } else if (row == 7 && col == 0) {
    p->castling &= (uint8_t)~POS_CASTLE_BQ;
  } else if (row == 7 && col == 7) {
    p->castling &= (uint8_t)~POS_CASTLE_BK;
  }
}

static void switch_side(position_t *p) {
  if (p->side == 1) {
    if (p->fullmove < 65535) {
      p->fullmove++;
    }
  }
  p->side ^= 1u;
}

static int try_castle(position_t *p, int fr, int fc, int tr, int tc) {
  int white = p->side == 0;
  int home = white ? 0 : 7;
  int kingside = tc == 6;
  int rook_from = kingside ? 7 : 0;
  int rook_to = kingside ? 5 : 3;
  uint8_t need = 0;
  uint8_t rook = white ? POS_WHITE_ROOK : POS_BLACK_ROOK;
  uint8_t king = white ? POS_WHITE_KING : POS_BLACK_KING;
  int by_white = white ? 0 : 1;
  int pass_col = kingside ? 5 : 3;

  if (fr != home || tr != home || fc != 4) {
    return 0;
  }
  if (!kingside && tc != 2) {
    return 0;
  }
  need = white ? (kingside ? POS_CASTLE_WK : POS_CASTLE_WQ)
               : (kingside ? POS_CASTLE_BK : POS_CASTLE_BQ);
  if ((p->castling & need) == 0) {
    return 0;
  }
  if (p->board[home][fc] != king || p->board[home][rook_from] != rook) {
    return 0;
  }
  if (kingside) {
    if (p->board[home][5] != POS_EMPTY || p->board[home][6] != POS_EMPTY) {
      return 0;
    }
  } else if (p->board[home][1] != POS_EMPTY || p->board[home][2] != POS_EMPTY ||
             p->board[home][3] != POS_EMPTY) {
    return 0;
  }
  if (position_sq_attacked(p->board, home, 4, by_white) ||
      position_sq_attacked(p->board, home, pass_col, by_white) ||
      position_sq_attacked(p->board, home, tc, by_white)) {
    return 0;
  }

  p->board[home][fc] = POS_EMPTY;
  p->board[home][rook_from] = POS_EMPTY;
  p->board[home][tc] = king;
  p->board[home][rook_to] = rook;
  if (white) {
    p->castling &= (uint8_t)~(POS_CASTLE_WK | POS_CASTLE_WQ);
  } else {
    p->castling &= (uint8_t)~(POS_CASTLE_BK | POS_CASTLE_BQ);
  }
  p->ep_square = POSITION_SQUARE_NONE;
  if (p->halfmove < 65535) {
    p->halfmove++;
  }
  switch_side(p);
  return 1;
}

static int slider_ok(const position_t *p, int fr, int fc, int tr, int tc, int bishop,
                     int rook) {
  int dr = tr - fr;
  int dc = tc - fc;
  int step_r = 0;
  int step_c = 0;
  int row;
  int col;

  if (dr != 0 && dc != 0 && (dr > 0 ? dr : -dr) == (dc > 0 ? dc : -dc)) {
    if (!bishop) {
      return 0;
    }
  } else if (dr == 0 || dc == 0) {
    if (!rook) {
      return 0;
    }
  } else {
    return 0;
  }
  if (dr != 0) {
    step_r = dr > 0 ? 1 : -1;
  }
  if (dc != 0) {
    step_c = dc > 0 ? 1 : -1;
  }
  row = fr + step_r;
  col = fc + step_c;
  while (row != tr || col != tc) {
    if (p->board[row][col] != POS_EMPTY) {
      return 0;
    }
    row += step_r;
    col += step_c;
  }
  return 1;
}

static int apply_on(position_t *p, position_move_t mv) {
  int fr;
  int fc;
  int tr;
  int tc;
  uint8_t piece;
  uint8_t dest;
  uint8_t captured = POS_EMPTY;
  int pawn_last;
  int pending_promo = 0;

  if (mv.from >= 64 || mv.to >= 64 || mv.from == mv.to) {
    return 0;
  }
  fr = mv.from / 8;
  fc = mv.from % 8;
  tr = mv.to / 8;
  tc = mv.to % 8;
  piece = p->board[fr][fc];
  dest = p->board[tr][tc];
  if (piece == POS_EMPTY || !same_side(piece, p->side)) {
    return 0;
  }
  if (dest != POS_EMPTY && same_side(dest, p->side)) {
    return 0;
  }

  if ((piece == POS_WHITE_KING || piece == POS_BLACK_KING) && fr == tr &&
      (tc - fc == 2 || fc - tc == 2)) {
    if (mv.promo != POS_PROMO_NONE) {
      return 0;
    }
    push_hist(p, mv, POS_EMPTY);
    if (!try_castle(p, fr, fc, tr, tc)) {
      p->hist_len--;
      return 0;
    }
    return 1;
  }

  if (piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN) {
    int dir = (piece == POS_WHITE_PAWN) ? 1 : -1;
    int start = (piece == POS_WHITE_PAWN) ? 1 : 6;
    pawn_last = (piece == POS_WHITE_PAWN) ? 7 : 0;
    if (tc == fc && dest == POS_EMPTY && tr == fr + dir) {
      /* quiet pawn step */
    } else if (tc == fc && dest == POS_EMPTY && fr == start && tr == fr + 2 * dir &&
               p->board[fr + dir][fc] == POS_EMPTY) {
      /* double step */
    } else if ((tc == fc + 1 || tc == fc - 1) && tr == fr + dir && dest != POS_EMPTY) {
      captured = dest;
    } else if ((tc == fc + 1 || tc == fc - 1) && tr == fr + dir && dest == POS_EMPTY &&
               mv.to == p->ep_square && p->ep_square < 64) {
      int vr = (p->side == 0) ? tr - 1 : tr + 1;
      uint8_t victim;
      uint8_t expect = (p->side == 0) ? POS_BLACK_PAWN : POS_WHITE_PAWN;
      if (!on_board(vr, tc)) {
        return 0;
      }
      victim = p->board[vr][tc];
      if (victim != expect) {
        return 0;
      }
      captured = (uint8_t)(victim | 0x80u);
      p->board[vr][tc] = POS_EMPTY;
    } else {
      return 0;
    }
    if (tr == pawn_last) {
      if (mv.promo == POS_PROMO_NONE) {
        pending_promo = 1;
      }
    } else if (mv.promo != POS_PROMO_NONE) {
      return 0;
    }
  } else if (mv.promo != POS_PROMO_NONE) {
    return 0;
  } else if (piece == POS_WHITE_KNIGHT || piece == POS_BLACK_KNIGHT) {
    int dr = tr - fr;
    int dc = tc - fc;
    if (dr < 0) {
      dr = -dr;
    }
    if (dc < 0) {
      dc = -dc;
    }
    if (!((dr == 1 && dc == 2) || (dr == 2 && dc == 1))) {
      return 0;
    }
    captured = dest;
  } else if (piece == POS_WHITE_BISHOP || piece == POS_BLACK_BISHOP) {
    if (!slider_ok(p, fr, fc, tr, tc, 1, 0)) {
      return 0;
    }
    captured = dest;
  } else if (piece == POS_WHITE_ROOK || piece == POS_BLACK_ROOK) {
    if (!slider_ok(p, fr, fc, tr, tc, 0, 1)) {
      return 0;
    }
    captured = dest;
  } else if (piece == POS_WHITE_QUEEN || piece == POS_BLACK_QUEEN) {
    if (!slider_ok(p, fr, fc, tr, tc, 1, 1)) {
      return 0;
    }
    captured = dest;
  } else if (piece == POS_WHITE_KING || piece == POS_BLACK_KING) {
    int dr = tr - fr;
    int dc = tc - fc;
    if (dr < 0) {
      dr = -dr;
    }
    if (dc < 0) {
      dc = -dc;
    }
    if (dr > 1 || dc > 1) {
      return 0;
    }
    captured = dest;
  } else {
    return 0;
  }

  push_hist(p, mv, captured);
  if (dest != POS_EMPTY) {
    clear_right_on_square(p, tr, tc);
  }
  if (piece == POS_WHITE_KING) {
    p->castling &= (uint8_t)~(POS_CASTLE_WK | POS_CASTLE_WQ);
  } else if (piece == POS_BLACK_KING) {
    p->castling &= (uint8_t)~(POS_CASTLE_BK | POS_CASTLE_BQ);
  } else if (piece == POS_WHITE_ROOK || piece == POS_BLACK_ROOK) {
    clear_right_on_square(p, fr, fc);
  }

  p->board[fr][fc] = POS_EMPTY;
  if (pending_promo || (piece != POS_WHITE_PAWN && piece != POS_BLACK_PAWN)) {
    p->board[tr][tc] = piece;
  } else if (piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN) {
    if ((piece == POS_WHITE_PAWN && tr == 7) || (piece == POS_BLACK_PAWN && tr == 0)) {
      p->board[tr][tc] = promo_piece(piece, mv.promo);
    } else {
      p->board[tr][tc] = piece;
    }
  }

  if (piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN || captured != POS_EMPTY) {
    p->halfmove = 0;
  } else if (p->halfmove < 65535) {
    p->halfmove++;
  }

  p->ep_square = POSITION_SQUARE_NONE;
  if ((piece == POS_WHITE_PAWN || piece == POS_BLACK_PAWN) &&
      (tr - fr == 2 || fr - tr == 2)) {
    p->ep_square = (uint8_t)(((fr + tr) / 2) * 8 + fc);
  }

  if (!pending_promo) {
    switch_side(p);
  }
  return 1;
}

bool position_apply(position_t *p, position_move_t mv) {
  position_t *scratch = position_core_apply();
  uint8_t mover;

  if (p == NULL) {
    return false;
  }
  *scratch = *p;
  mover = scratch->side;
  if (!apply_on(scratch, mv)) {
    return false;
  }
  if (position_mover_in_check(scratch, mover)) {
    return false;
  }
  *p = *scratch;
  return true;
}

bool position_undo(position_t *p) {
  position_hist_t h;
  int fr;
  int fc;
  int tr;
  int tc;
  uint8_t moved;

  if (p == NULL || p->hist_len == 0) {
    return false;
  }
  h = p->hist[p->hist_len - 1];
  p->hist_len--;
  fr = h.mv.from / 8;
  fc = h.mv.from % 8;
  tr = h.mv.to / 8;
  tc = h.mv.to % 8;
  moved = p->board[tr][tc];

  uint8_t cap = (uint8_t)(h.captured & 0x7fu);
  int ep_cap = (h.captured & 0x80u) != 0;

  if ((moved == POS_WHITE_KING || moved == POS_BLACK_KING) && fr == tr &&
      (tc - fc == 2 || fc - tc == 2)) {
    int rook_to = (tc == 6) ? 5 : 3;
    int rook_from = (tc == 6) ? 7 : 0;
    p->board[fr][fc] = moved;
    p->board[tr][tc] = POS_EMPTY;
    p->board[fr][rook_from] = p->board[fr][rook_to];
    p->board[fr][rook_to] = POS_EMPTY;
  } else if (ep_cap) {
    int vr = (h.side_before == 0) ? tr - 1 : tr + 1;
    p->board[tr][tc] = POS_EMPTY;
    p->board[fr][fc] = (h.side_before == 0) ? POS_WHITE_PAWN : POS_BLACK_PAWN;
    if (vr >= 0 && vr < 8) {
      p->board[vr][tc] = cap;
    }
  } else {
    if (h.mv.promo != POS_PROMO_NONE) {
      moved = (h.side_before == 0) ? POS_WHITE_PAWN : POS_BLACK_PAWN;
    } else if ((moved == POS_WHITE_PAWN && tr == 7) || (moved == POS_BLACK_PAWN && tr == 0)) {
      moved = (h.side_before == 0) ? POS_WHITE_PAWN : POS_BLACK_PAWN;
    }
    p->board[tr][tc] = cap;
    p->board[fr][fc] = moved;
  }

  p->castling = h.castling_before;
  p->ep_square = h.ep_before;
  p->side = h.side_before;
  p->halfmove = h.halfmove_before;
  p->fullmove = h.fullmove_before;
  return true;
}

static int insufficient(const position_t *p) {
  int white_pieces = 0;
  int black_pieces = 0;
  int white_minors = 0;
  int black_minors = 0;
  int white_bishop = 0;
  int black_bishop = 0;
  int white_knight = 0;
  int black_knight = 0;
  int white_color = 0;
  int black_color = 0;
  int row;
  int col;

  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      switch (p->board[row][col]) {
      case POS_WHITE_PAWN:
      case POS_WHITE_ROOK:
      case POS_WHITE_QUEEN:
        white_pieces++;
        break;
      case POS_WHITE_BISHOP:
        white_pieces++;
        white_minors++;
        white_bishop = 1;
        white_color = ((row + col) % 2 == 0);
        break;
      case POS_WHITE_KNIGHT:
        white_pieces++;
        white_minors++;
        white_knight = 1;
        break;
      case POS_BLACK_PAWN:
      case POS_BLACK_ROOK:
      case POS_BLACK_QUEEN:
        black_pieces++;
        break;
      case POS_BLACK_BISHOP:
        black_pieces++;
        black_minors++;
        black_bishop = 1;
        black_color = ((row + col) % 2 == 0);
        break;
      case POS_BLACK_KNIGHT:
        black_pieces++;
        black_minors++;
        black_knight = 1;
        break;
      default:
        break;
      }
    }
  }

  if (white_pieces == 0 && black_pieces == 0) {
    return 1;
  }
  if ((white_pieces == 1 && black_pieces == 0) ||
      (white_pieces == 0 && black_pieces == 1)) {
    return 1;
  }
  if (white_pieces == 1 && black_pieces == 1 && white_bishop && black_bishop &&
      white_color == black_color) {
    return 1;
  }
  if (white_pieces == 1 && black_pieces == 1 && white_knight && black_knight) {
    return 1;
  }
  if ((white_pieces == 2 && black_pieces == 0 && white_minors == 2 && white_knight) ||
      (white_pieces == 0 && black_pieces == 2 && black_minors == 2 && black_knight)) {
    return 1;
  }
  return 0;
}

static uint32_t pos_hash(const position_t *p) {
  uint32_t hash = 0;
  int row;
  int col;

  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      uint8_t piece = p->board[row][col];
      if (piece != POS_EMPTY) {
        uint32_t piece_hash = ((uint32_t)piece << 16) | ((uint32_t)row << 8) | (uint32_t)col;
        hash = ((hash << 5) + hash) ^ piece_hash;
      }
    }
  }
  hash = ((hash << 5) + hash) ^ ((uint32_t)p->side << 24);
  hash = ((hash << 5) + hash) ^ p->castling;
  if (p->ep_square < 64) {
    hash = ((hash << 5) + hash) ^ p->ep_square;
  }
  return hash;
}

static int repeated(const position_t *p) {
  position_t *replay = position_core_trial();
  uint32_t target;
  int count = 1;

  *replay = *p;
  target = pos_hash(replay);
  while (replay->hist_len > 0) {
    if (!position_undo(replay)) {
      break;
    }
    if (pos_hash(replay) == target) {
      count++;
      if (count >= 3) {
        return 1;
      }
    }
  }
  return 0;
}

position_status_t position_status(const position_t *p) {
  position_status_t status;
  int in_check;
  int has_move;

  memset(&status, 0, sizeof(status));
  if (p == NULL) {
    return status;
  }
  in_check = position_mover_in_check(p, p->side);
  has_move = position_has_legal_move(p);
  status.in_check = in_check ? true : false;
  status.checkmate = (in_check && !has_move) ? true : false;
  status.stalemate = (!in_check && !has_move) ? true : false;
  status.fifty_move = (p->halfmove >= 100) ? true : false;
  status.repetition = repeated(p) ? true : false;
  status.insufficient_material = insufficient(p) ? true : false;
  return status;
}
