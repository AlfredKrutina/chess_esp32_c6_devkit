/**
 * @file game_hall_match.c
 * @brief Hall type/color agreement with chess engine piece_t.
 */

#include "game_hall_match.h"
#include "game_hall_hand.h"
#include "game_task_internal.h"
#include "../squares_task/include/squares_task.h"

#include "esp_log.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

static const char *TAG = "GAME_HALL";

bool game_hall_engine_to_sensor(piece_t piece, hall_piece_type_t *out_type,
                                hall_piece_color_t *out_color) {
  if (out_type == NULL || out_color == NULL || piece == PIECE_EMPTY) {
    return false;
  }

  if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
    *out_color = HALL_PIECE_COLOR_WHITE;
  } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
    *out_color = HALL_PIECE_COLOR_BLACK;
  } else {
    return false;
  }

  switch (piece) {
  case PIECE_WHITE_PAWN:
  case PIECE_BLACK_PAWN:
    *out_type = HALL_PIECE_TYPE_PAWN;
    break;
  case PIECE_WHITE_KNIGHT:
  case PIECE_BLACK_KNIGHT:
    *out_type = HALL_PIECE_TYPE_KNIGHT;
    break;
  case PIECE_WHITE_BISHOP:
  case PIECE_BLACK_BISHOP:
    *out_type = HALL_PIECE_TYPE_BISHOP;
    break;
  case PIECE_WHITE_ROOK:
  case PIECE_BLACK_ROOK:
    *out_type = HALL_PIECE_TYPE_ROOK;
    break;
  case PIECE_WHITE_QUEEN:
  case PIECE_BLACK_QUEEN:
    *out_type = HALL_PIECE_TYPE_QUEEN;
    break;
  case PIECE_WHITE_KING:
  case PIECE_BLACK_KING:
    *out_type = HALL_PIECE_TYPE_KING;
    break;
  default:
    return false;
  }
  return true;
}

bool game_hall_stable_reading_matches(piece_t engine_piece,
                                      hall_piece_type_t ht,
                                      hall_piece_color_t hc) {
  hall_piece_type_t want_type;
  hall_piece_color_t want_color;
  if (!game_hall_engine_to_sensor(engine_piece, &want_type, &want_color)) {
    return ht == HALL_PIECE_TYPE_NONE && hc == HALL_PIECE_COLOR_NONE;
  }
  return ht == want_type && hc == want_color;
}

static bool game_hall_reading_for_pickup(uint8_t sq, hall_piece_type_t *ht,
                                         hall_piece_color_t *hc) {
  *ht = squares_get_hall_piece_type(sq);
  *hc = squares_get_hall_piece_color(sq);
  if (*ht != HALL_PIECE_TYPE_NONE) {
    return true;
  }
  *ht = squares_get_hall_last_occupied_type(sq);
  *hc = squares_get_hall_last_occupied_color(sq);
  return *ht != HALL_PIECE_TYPE_NONE;
}

bool game_hall_validate_pickup_square(uint8_t row, uint8_t col,
                                      piece_t engine_piece) {
  if (engine_piece == PIECE_EMPTY) {
    return false;
  }

  uint8_t sq = (uint8_t)(row * 8 + col);
  hall_piece_type_t ht;
  hall_piece_color_t hc;
  if (!game_hall_reading_for_pickup(sq, &ht, &hc)) {
    ESP_LOGW(TAG, "pickup %u: no stable Hall identity (engine %d)", (unsigned)sq,
             (int)engine_piece);
    return true;
  }

  if (!game_hall_stable_reading_matches(engine_piece, ht, hc)) {
    /* game keeps the engine piece. A settled contradiction is attention, not a rename. */
  }
  return true;
}

bool game_hall_validate_drop_square(uint8_t row, uint8_t col,
                                    piece_t moving_piece) {
  if (moving_piece == PIECE_EMPTY) {
    return false;
  }

  uint8_t sq = (uint8_t)(row * 8 + col);
  hall_piece_type_t ht = squares_get_hall_piece_type(sq);
  hall_piece_color_t hc = squares_get_hall_piece_color(sq);

  if (ht == HALL_PIECE_TYPE_NONE) {
    ESP_LOGW(TAG, "drop %u: Hall still empty (not stable occupied)", (unsigned)sq);
    return false;
  }

  hall_piece_type_t want_type;
  hall_piece_color_t want_color;
  if (!game_hall_engine_to_sensor(moving_piece, &want_type, &want_color) ||
      hc != want_color) {
    ESP_LOGW(TAG, "drop %u: Hall [%s %s] != moving piece %d", (unsigned)sq,
             squares_get_hall_piece_color_name(hc),
             squares_get_hall_piece_type_name(ht), (int)moving_piece);
    return false;
  }

  if (ht != want_type) {
    ESP_LOGW(TAG, "drop %u: Hall type %s, kept %s by color", (unsigned)sq,
             squares_get_hall_piece_type_name(ht),
             squares_get_hall_piece_type_name(want_type));
  }
  return true;
}

bool game_hall_validate_promotion_entry_drop(uint8_t row, uint8_t col,
                                             piece_t moving_pawn) {
  if (game_hall_validate_drop_square(row, col, moving_pawn)) {
    return true;
  }

  piece_t options[4];
  if (moving_pawn == PIECE_WHITE_PAWN) {
    options[0] = PIECE_WHITE_QUEEN;
    options[1] = PIECE_WHITE_ROOK;
    options[2] = PIECE_WHITE_BISHOP;
    options[3] = PIECE_WHITE_KNIGHT;
  } else if (moving_pawn == PIECE_BLACK_PAWN) {
    options[0] = PIECE_BLACK_QUEEN;
    options[1] = PIECE_BLACK_ROOK;
    options[2] = PIECE_BLACK_BISHOP;
    options[3] = PIECE_BLACK_KNIGHT;
  } else {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    if (game_hall_validate_drop_square(row, col, options[i])) {
      return true;
    }
  }
  return false;
}

bool game_hall_validate_castling_rook_drop(uint8_t rook_to_row,
                                           uint8_t rook_to_col, piece_t rook,
                                           uint8_t king_row, uint8_t king_col,
                                           piece_t king) {
  if (!game_hall_validate_drop_square(rook_to_row, rook_to_col, rook)) {
    return false;
  }
  return game_hall_validate_drop_square(king_row, king_col, king);
}

bool game_hall_piece_for_promoting_player(hall_piece_type_t ht,
                                          hall_piece_color_t hc,
                                          player_t promoting_player,
                                          piece_t *out_piece) {
  if (out_piece == NULL || ht == HALL_PIECE_TYPE_NONE ||
      hc == HALL_PIECE_COLOR_NONE) {
    return false;
  }

  const hall_piece_color_t want_color = (promoting_player == PLAYER_WHITE)
                                            ? HALL_PIECE_COLOR_WHITE
                                            : HALL_PIECE_COLOR_BLACK;
  if (hc != want_color) {
    return false;
  }

  if (promoting_player == PLAYER_WHITE) {
    switch (ht) {
    case HALL_PIECE_TYPE_PAWN:
      *out_piece = PIECE_WHITE_PAWN;
      return true;
    case HALL_PIECE_TYPE_KNIGHT:
      *out_piece = PIECE_WHITE_KNIGHT;
      return true;
    case HALL_PIECE_TYPE_BISHOP:
      *out_piece = PIECE_WHITE_BISHOP;
      return true;
    case HALL_PIECE_TYPE_ROOK:
      *out_piece = PIECE_WHITE_ROOK;
      return true;
    case HALL_PIECE_TYPE_QUEEN:
    case HALL_PIECE_TYPE_KING:
      *out_piece = PIECE_WHITE_QUEEN;
      return true;
    default:
      return false;
    }
  }

  switch (ht) {
  case HALL_PIECE_TYPE_PAWN:
    *out_piece = PIECE_BLACK_PAWN;
    return true;
  case HALL_PIECE_TYPE_KNIGHT:
    *out_piece = PIECE_BLACK_KNIGHT;
    return true;
  case HALL_PIECE_TYPE_BISHOP:
    *out_piece = PIECE_BLACK_BISHOP;
    return true;
  case HALL_PIECE_TYPE_ROOK:
    *out_piece = PIECE_BLACK_ROOK;
    return true;
    case HALL_PIECE_TYPE_QUEEN:
    case HALL_PIECE_TYPE_KING:
      *out_piece = PIECE_BLACK_QUEEN;
      return true;
  default:
    return false;
  }
}

bool game_hall_logic_matches_stable_sensors(const uint8_t sensor_occ[64]) {
  if (sensor_occ == NULL) {
    return false;
  }
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t expect = board[sq / 8][sq % 8];
    if (!game_hall_square_agrees_with_logic(sq, expect, sensor_occ[sq])) {
      return false;
    }
  }
  return true;
}

bool game_hall_square_agrees_with_logic(uint8_t square, piece_t engine_piece,
                                        uint8_t sensor_occ) {
  if (engine_piece == PIECE_EMPTY) {
    return sensor_occ == 0;
  }
  if (sensor_occ == 0) {
    return false;
  }
  hall_piece_type_t ht = squares_get_hall_piece_type(square);
  hall_piece_color_t hc = squares_get_hall_piece_color(square);
  if (!hall_i2c_squares_identity_settled(square) || ht == HALL_PIECE_TYPE_NONE ||
      hc == HALL_PIECE_COLOR_NONE) {
    return true;
  }
  return game_hall_identity_matches(engine_piece, ht, hc);
}

static bool hall_piece_matches_reading(piece_t piece, hall_piece_type_t ht,
                                      hall_piece_color_t hc) {
  hall_piece_type_t want;
  hall_piece_color_t want_color;
  if (!game_hall_engine_to_sensor(piece, &want, &want_color) ||
      hc != want_color) {
    return false;
  }
  if (ht == want) {
    return true;
  }
  bool want_royal = want == HALL_PIECE_TYPE_KING || want == HALL_PIECE_TYPE_QUEEN;
  bool seen_royal = ht == HALL_PIECE_TYPE_KING || ht == HALL_PIECE_TYPE_QUEEN;
  return want_royal && seen_royal;
}

bool game_hall_identity_matches(piece_t piece, hall_piece_type_t ht,
                                hall_piece_color_t hc) {
  return hall_piece_matches_reading(piece, ht, hc);
}

int game_hall_relocated_origin(uint8_t placed, const uint8_t occ[64]) {
  if (occ == NULL || placed >= 64) {
    return -1;
  }
  hall_piece_type_t ht = squares_get_hall_piece_type(placed);
  hall_piece_color_t hc = squares_get_hall_piece_color(placed);
  if (ht == HALL_PIECE_TYPE_NONE || hc == HALL_PIECE_COLOR_NONE) {
    return -1;
  }

  uint8_t seen[64] = {0};
  int found = -1;
  int n = 0;
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (sq == placed || occ[sq] != 0) {
      continue;
    }
    piece_t piece = board[sq / 8][sq % 8];
    if (!hall_piece_matches_reading(piece, ht, hc)) {
      continue;
    }
    seen[sq] = 1;
    found = (int)sq;
    n++;
  }
  for (int i = 0; i < game_hall_hand_count(); i++) {
    piece_t piece;
    uint8_t row;
    uint8_t col;
    if (!game_hall_hand_get(i, &piece, &row, &col)) {
      continue;
    }
    uint8_t sq = (uint8_t)(row * 8 + col);
    if (sq >= 64 || sq == placed || occ[sq] != 0 || seen[sq]) {
      continue;
    }
    if (!hall_piece_matches_reading(piece, ht, hc)) {
      continue;
    }
    seen[sq] = 1;
    found = (int)sq;
    n++;
  }
  return (n == 1) ? found : -1;
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
