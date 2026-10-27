/**
 * @brief Lift-order hand. The newest slot is the last one pushed.
 *
 * Hall drops do not assume the last lift is the piece that landed.
 */

#include "game_hall_hand.h"

#include "esp_log.h"
#include "esp_timer.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_match.h"
#include "game_task_internal.h"
#include "../squares_task/include/squares_task.h"
#endif

typedef struct {
  piece_t piece;
  uint8_t row;
  uint8_t col;
  uint32_t at_ms;
} hand_slot_t;

static hand_slot_t s_hand[GAME_HALL_HAND_MAX];
static int s_count;
static piece_t s_captured[8];
static int s_captured_n;

void game_hall_hand_reset(void) {
  s_count = 0;
  s_captured_n = 0;
}

bool game_hall_hand_push(piece_t piece, uint8_t row, uint8_t col) {
  if (piece == PIECE_EMPTY || s_count >= GAME_HALL_HAND_MAX) {
    return false;
  }
  for (int i = 0; i < s_count; i++) {
    if (s_hand[i].row == row && s_hand[i].col == col) {
      return true;
    }
  }
  s_hand[s_count].piece = piece;
  s_hand[s_count].row = row;
  s_hand[s_count].col = col;
  s_hand[s_count].at_ms = (uint32_t)(esp_timer_get_time() / 1000);
  s_count++;
  return true;
}

int game_hall_hand_count(void) { return s_count; }

bool game_hall_hand_get(int index, piece_t *piece, uint8_t *row, uint8_t *col) {
  if (index < 0 || index >= s_count || piece == NULL || row == NULL ||
      col == NULL) {
    return false;
  }
  *piece = s_hand[index].piece;
  *row = s_hand[index].row;
  *col = s_hand[index].col;
  return true;
}

void game_hall_hand_remove_at(int index) {
  if (index < 0 || index >= s_count) {
    return;
  }
  for (int i = index; i < s_count - 1; i++) {
    s_hand[i] = s_hand[i + 1];
  }
  s_count--;
}

void game_hall_hand_remove_origin(uint8_t row, uint8_t col) {
  for (int i = 0; i < s_count; i++) {
    if (s_hand[i].row == row && s_hand[i].col == col) {
      game_hall_hand_remove_at(i);
      return;
    }
  }
}

void game_hall_hand_set_piece(int index, piece_t piece) {
  if (index < 0 || index >= s_count || piece == PIECE_EMPTY) {
    return;
  }
  s_hand[index].piece = piece;
}

void game_hall_hand_note_captured(piece_t piece) {
  if (piece == PIECE_EMPTY || s_captured_n >= 8) {
    return;
  }
  s_captured[s_captured_n++] = piece;
}

piece_t game_hall_hand_last_captured(void) {
  if (s_captured_n <= 0) {
    return PIECE_EMPTY;
  }
  return s_captured[s_captured_n - 1];
}

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

static const char *TAG = "GAME_HALL_HAND";

static bool hall_is_royal(hall_piece_type_t type) {
  return type == HALL_PIECE_TYPE_KING || type == HALL_PIECE_TYPE_QUEEN;
}

static int hall_count_play(piece_t piece) {
  bool held_origin[64] = {0};
  int n = 0;
  for (int i = 0; i < s_count; i++) {
    held_origin[s_hand[i].row * 8 + s_hand[i].col] = true;
    if (s_hand[i].piece == piece) {
      n++;
    }
  }
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (held_origin[sq]) {
      continue;
    }
    if (board[sq / 8][sq % 8] == piece) {
      n++;
    }
  }
  return n;
}

static int hall_count_captured(piece_t piece) {
  int n = 0;
  for (int i = 0; i < s_captured_n; i++) {
    if (s_captured[i] == piece) {
      n++;
    }
  }
  return n;
}

static int hall_accounted(piece_t piece) {
  return hall_count_play(piece) + hall_count_captured(piece);
}

static piece_t hall_pawn_of(hall_piece_color_t color) {
  return color == HALL_PIECE_COLOR_WHITE ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;
}

/* Officers can exceed the starting pair by one per promoted pawn. */
static int hall_type_limit(piece_t piece) {
  hall_piece_type_t type;
  hall_piece_color_t color;
  if (!game_hall_engine_to_sensor(piece, &type, &color)) {
    return 0;
  }
  if (type == HALL_PIECE_TYPE_KING) {
    return 1;
  }
  if (type == HALL_PIECE_TYPE_PAWN) {
    return 8;
  }
  int missing_pawns = 8 - hall_accounted(hall_pawn_of(color));
  if (missing_pawns < 0) {
    missing_pawns = 0;
  }
  int base = (type == HALL_PIECE_TYPE_QUEEN) ? 1 : 2;
  return base + missing_pawns;
}

static bool hall_reading_known(hall_piece_type_t type, hall_piece_color_t color) {
  return hall_i2c_squares_readings_usable() && type != HALL_PIECE_TYPE_NONE &&
         color != HALL_PIECE_COLOR_NONE;
}

/* 2 = exact or same-color K/Q band, 1 = pawn promotion wildcard, 0 = no. */
static int hall_match_rank(piece_t piece, hall_piece_type_t ht,
                           hall_piece_color_t hc, uint8_t to_row) {
  hall_piece_type_t want;
  hall_piece_color_t want_color;
  if (!game_hall_engine_to_sensor(piece, &want, &want_color) || hc != want_color) {
    return 0;
  }
  if (ht == want || (hall_is_royal(ht) && hall_is_royal(want))) {
    return 2;
  }
  bool promo = (piece == PIECE_WHITE_PAWN && to_row == 7) ||
               (piece == PIECE_BLACK_PAWN && to_row == 0);
  if (!promo) {
    return 0;
  }
  if (ht == HALL_PIECE_TYPE_QUEEN || ht == HALL_PIECE_TYPE_KING ||
      ht == HALL_PIECE_TYPE_ROOK || ht == HALL_PIECE_TYPE_BISHOP ||
      ht == HALL_PIECE_TYPE_KNIGHT) {
    return 1;
  }
  return 0;
}

static bool hall_legal_landing(piece_t piece, uint8_t from_row, uint8_t from_col,
                               uint8_t to_row, uint8_t to_col) {
  if (from_row == to_row && from_col == to_col) {
    return false;
  }
  chess_move_t move = {.from_row = from_row,
                       .from_col = from_col,
                       .to_row = to_row,
                       .to_col = to_col,
                       .piece = piece,
                       .captured_piece = board[to_row][to_col]};
  return game_is_valid_move(&move) == MOVE_ERROR_NONE;
}

/* Prefer the piece's own square, then a legal destination, then the newest lift. */
static int hall_fallback_index(const int *cands, int nc, uint8_t to_row,
                               uint8_t to_col) {
  int best = -1;
  int best_score = -1;
  for (int n = 0; n < nc; n++) {
    int i = cands[n];
    int score = 0;
    if (s_hand[i].row == to_row && s_hand[i].col == to_col) {
      score = 1000;
    } else if (hall_legal_landing(s_hand[i].piece, s_hand[i].row, s_hand[i].col,
                                  to_row, to_col)) {
      score = 200;
    }
    if (score > best_score || (score == best_score && i > best)) {
      best = i;
      best_score = score;
    }
  }
  return best;
}

typedef enum {
  HALL_MAP_PIECE = 0,
  HALL_MAP_AMBIGUOUS = 1,
  HALL_MAP_IMPOSSIBLE = 2,
} hall_map_t;

static hall_map_t hall_map_reading(hall_piece_type_t ht, hall_piece_color_t hc,
                                   piece_t *out) {
  if (hc != HALL_PIECE_COLOR_WHITE && hc != HALL_PIECE_COLOR_BLACK) {
    return HALL_MAP_AMBIGUOUS;
  }
  piece_t piece = PIECE_EMPTY;
  if (hc == HALL_PIECE_COLOR_WHITE) {
    switch (ht) {
    case HALL_PIECE_TYPE_PAWN:
      piece = PIECE_WHITE_PAWN;
      break;
    case HALL_PIECE_TYPE_KNIGHT:
      piece = PIECE_WHITE_KNIGHT;
      break;
    case HALL_PIECE_TYPE_BISHOP:
      piece = PIECE_WHITE_BISHOP;
      break;
    case HALL_PIECE_TYPE_ROOK:
      piece = PIECE_WHITE_ROOK;
      break;
    default:
      break;
    }
  } else {
    switch (ht) {
    case HALL_PIECE_TYPE_PAWN:
      piece = PIECE_BLACK_PAWN;
      break;
    case HALL_PIECE_TYPE_KNIGHT:
      piece = PIECE_BLACK_KNIGHT;
      break;
    case HALL_PIECE_TYPE_BISHOP:
      piece = PIECE_BLACK_BISHOP;
      break;
    case HALL_PIECE_TYPE_ROOK:
      piece = PIECE_BLACK_ROOK;
      break;
    default:
      break;
    }
  }
  if (hall_is_royal(ht)) {
    piece_t king =
        hc == HALL_PIECE_COLOR_WHITE ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    piece_t queen =
        hc == HALL_PIECE_COLOR_WHITE ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;
    bool king_ok = hall_accounted(king) < hall_type_limit(king);
    bool queen_ok = hall_accounted(queen) < hall_type_limit(queen);
    if (king_ok && !queen_ok) {
      *out = king;
      return HALL_MAP_PIECE;
    }
    if (queen_ok && !king_ok) {
      *out = queen;
      return HALL_MAP_PIECE;
    }
    if (!king_ok && !queen_ok) {
      return HALL_MAP_IMPOSSIBLE;
    }
    return HALL_MAP_AMBIGUOUS;
  }
  if (piece == PIECE_EMPTY) {
    return HALL_MAP_AMBIGUOUS;
  }
  if (hall_accounted(piece) >= hall_type_limit(piece)) {
    return HALL_MAP_IMPOSSIBLE;
  }
  *out = piece;
  return HALL_MAP_PIECE;
}

static void hall_fill_pick(int index, bool corrected, bool contradicted,
                           bool sensor_hit, game_hall_pick_t *out) {
  out->index = index;
  out->piece = s_hand[index].piece;
  out->row = s_hand[index].row;
  out->col = s_hand[index].col;
  out->corrected = corrected;
  out->contradicted = contradicted;
  out->sensor_hit = sensor_hit;
}

static bool hall_read_square(uint8_t sq, bool allow_last, hall_piece_type_t *ht,
                             hall_piece_color_t *hc) {
  *ht = squares_get_hall_piece_type(sq);
  *hc = squares_get_hall_piece_color(sq);
  if (*ht != HALL_PIECE_TYPE_NONE) {
    return true;
  }
  if (!allow_last) {
    return false;
  }
  *ht = squares_get_hall_last_occupied_type(sq);
  *hc = squares_get_hall_last_occupied_color(sq);
  return *ht != HALL_PIECE_TYPE_NONE;
}

game_hall_pick_status_t game_hall_hand_pick_drop(uint8_t to_row, uint8_t to_col,
                                                game_hall_pick_t *out) {
  if (out == NULL || s_count <= 0 || to_row > 7 || to_col > 7) {
    return GAME_HALL_PICK_NONE;
  }
  uint8_t sq = (uint8_t)(to_row * 8 + to_col);
  hall_piece_type_t ht = HALL_PIECE_TYPE_NONE;
  hall_piece_color_t hc = HALL_PIECE_COLOR_NONE;
  hall_read_square(sq, false, &ht, &hc);
  bool known = hall_reading_known(ht, hc);

  int all[GAME_HALL_HAND_MAX];
  int exact[GAME_HALL_HAND_MAX];
  int wild[GAME_HALL_HAND_MAX];
  int n_exact = 0;
  int n_wild = 0;
  for (int i = 0; i < s_count; i++) {
    all[i] = i;
    if (!known) {
      continue;
    }
    int rank = hall_match_rank(s_hand[i].piece, ht, hc, to_row);
    if (rank == 2) {
      exact[n_exact++] = i;
    } else if (rank == 1) {
      wild[n_wild++] = i;
    }
  }

  const int *cands = all;
  int nc = s_count;
  bool contradicted = false;
  bool corrected = false;
  if (known && n_exact == 1) {
    hall_fill_pick(exact[0], false, false, true, out);
    ESP_LOGI(TAG, "drop %c%d: sensor matched held piece %d", 'a' + to_col,
             to_row + 1, (int)out->piece);
    return GAME_HALL_PICK_OK;
  }
  if (known && n_exact > 1) {
    cands = exact;
    nc = n_exact;
  } else if (known && n_exact == 0 && n_wild == 1) {
    hall_fill_pick(wild[0], false, false, true, out);
    ESP_LOGI(TAG, "drop %c%d: promotion reading, held pawn kept", 'a' + to_col,
             to_row + 1);
    return GAME_HALL_PICK_OK;
  } else if (known && n_exact == 0 && n_wild > 1) {
    cands = wild;
    nc = n_wild;
  } else if (known && n_exact == 0 && n_wild == 0) {
    piece_t seen = PIECE_EMPTY;
    hall_map_t mapped = hall_map_reading(ht, hc, &seen);
    if (mapped == HALL_MAP_PIECE) {
      contradicted = true;
      ESP_LOGW(TAG,
               "drop %c%d: Hall says piece %d, engine identity kept",
               'a' + to_col, to_row + 1, (int)seen);
    } else if (mapped == HALL_MAP_IMPOSSIBLE) {
      contradicted = true;
      ESP_LOGW(TAG, "drop %c%d: Hall [%s %s] matches no plausible piece",
               'a' + to_col, to_row + 1, squares_get_hall_piece_color_name(hc),
               squares_get_hall_piece_type_name(ht));
    }
  }

  int idx = hall_fallback_index(cands, nc, to_row, to_col);
  if (idx < 0) {
    return GAME_HALL_PICK_NONE;
  }
  hall_fill_pick(idx, corrected, contradicted, false, out);
  ESP_LOGI(TAG, "drop %c%d: rules chose piece %d from %c%d (contradict=%d)",
           'a' + to_col, to_row + 1, (int)out->piece, 'a' + out->col,
           out->row + 1, contradicted ? 1 : 0);
  return GAME_HALL_PICK_OK;
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
