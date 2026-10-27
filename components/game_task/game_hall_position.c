/**
 * @file game_hall_position.c
 * @brief Diff the Hall occupancy snapshot against the last legal position.
 *
 * Pickup and drop commands only wake this and leave an identity hint.
 * They do not commit, reject, or rewrite board[][].
 */

#include "game_hall_position.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "game_hall_led.h"
#include "game_hall_match.h"
#include "game_hall_restore_guide.h"
#include "game_physical_undo.h"
#include "game_task.h"
#include "game_task_internal.h"
#include "chess_gameplay_policy.h"

#include "hall_i2c_squares.h"
#include "squares_task.h"
#include "led_mapping.h"
#include "led_task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <string.h>

static const char *TAG = "HALL_POS";

#define HALL_POS_STABLE_MS 150U
#define HALL_POS_TAKEBACK_MS 600U
#define HALL_POS_DIFF_CAP 8
#define HALL_CONTRA_SCANS 4
#define HINT_CAP 8

static piece_t s_last_valid[8][8];
static uint8_t s_occ_digest[8];
static bool s_digest_valid;
static game_hall_pos_class_t s_class = GAME_HALL_POS_EQUAL;
static uint32_t s_class_since_ms;
static bool s_cyan_shown;
static bool s_guide_drawn;
static bool s_lift_shown;
static bool s_timer_held;
static bool s_in_service;
static bool s_attention;
static uint8_t s_cyan_sq[8];
static uint8_t s_cyan_n;
static uint8_t s_lift_sq;
static uint8_t s_eq_contra;
static uint8_t s_prev_contra;
static chess_move_t s_pending_move;
static bool s_pending_valid;
static uint64_t s_attn_logged;

typedef struct {
  uint8_t sq;
  piece_t piece;
  uint32_t seq;
  bool vacate;
} hint_t;

static hint_t s_hint[HINT_CAP];
static int s_hint_n;
static uint32_t s_hint_seq;

typedef struct {
  uint8_t sq;
  piece_t expect;
  uint8_t occ;
} hall_diff_t;

static bool hall_side_owns(piece_t piece) {
  bool white = piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING;
  bool black = piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING;
  if (!white && !black) {
    return false;
  }
  return current_player == PLAYER_WHITE ? white : black;
}

static bool reading_contradicts(uint8_t sq, piece_t expect) {
  if (expect == PIECE_EMPTY || !hall_i2c_squares_identity_settled(sq)) {
    return false;
  }
  hall_piece_type_t ht = squares_get_hall_piece_type(sq);
  hall_piece_color_t hc = squares_get_hall_piece_color(sq);
  if (ht == HALL_PIECE_TYPE_NONE || hc == HALL_PIECE_COLOR_NONE) {
    return false;
  }
  return !game_hall_identity_matches(expect, ht, hc);
}

/** Occupancy is the piece. Unsettled or king/queen band does not contradict. */
static bool square_tolerant(uint8_t sq, piece_t expect, uint8_t occ) {
  if (expect == PIECE_EMPTY) {
    return occ == 0;
  }
  if (occ == 0) {
    return false;
  }
  return !reading_contradicts(sq, expect);
}

static bool occupancy_equal(const piece_t expected[8][8], const uint8_t occ[64]) {
  for (uint8_t sq = 0; sq < 64; sq++) {
    bool want = expected[sq / 8][sq % 8] != PIECE_EMPTY;
    if ((occ[sq] != 0) != want) {
      return false;
    }
  }
  return true;
}

static bool identity_clear(const piece_t expected[8][8], const uint8_t occ[64]) {
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (!square_tolerant(sq, expected[sq / 8][sq % 8], occ[sq])) {
      return false;
    }
  }
  return true;
}

static void pack_occ(const uint8_t occ[64], uint8_t digest[8]) {
  memset(digest, 0, 8);
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (occ[sq] != 0) {
      digest[sq / 8] |= (uint8_t)(1U << (sq % 8));
    }
  }
}

static int gather_diffs(const uint8_t occ[64], hall_diff_t *out, int cap) {
  int n = 0;
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t expect = s_last_valid[sq / 8][sq % 8];
    if (square_tolerant(sq, expect, occ[sq])) {
      continue;
    }
    if (n < cap) {
      out[n].sq = sq;
      out[n].expect = expect;
      out[n].occ = occ[sq];
    }
    n++;
  }
  return n;
}

static void fill_cyan(const uint8_t occ[64]) {
  s_cyan_n = 0;
  for (uint8_t sq = 0; sq < 64 && s_cyan_n < 8; sq++) {
    if (!square_tolerant(sq, s_last_valid[sq / 8][sq % 8], occ[sq])) {
      s_cyan_sq[s_cyan_n++] = sq;
    }
  }
}

static bool same_color_reading(piece_t piece, hall_piece_color_t hc) {
  bool white = piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING;
  if (white) {
    return hc == HALL_PIECE_COLOR_WHITE;
  }
  return hc == HALL_PIECE_COLOR_BLACK;
}

static bool landing_accepts(piece_t mover, uint8_t dest_sq) {
  if (!hall_i2c_squares_identity_settled(dest_sq)) {
    return true;
  }
  hall_piece_type_t ht = squares_get_hall_piece_type(dest_sq);
  hall_piece_color_t hc = squares_get_hall_piece_color(dest_sq);
  if (ht == HALL_PIECE_TYPE_NONE || hc == HALL_PIECE_COLOR_NONE) {
    return true;
  }
  if (game_hall_identity_matches(mover, ht, hc)) {
    return true;
  }
  bool promo = (mover == PIECE_WHITE_PAWN && dest_sq / 8 == 7) ||
               (mover == PIECE_BLACK_PAWN && dest_sq / 8 == 0);
  if (!promo || !same_color_reading(mover, hc)) {
    return false;
  }
  return ht == HALL_PIECE_TYPE_QUEEN || ht == HALL_PIECE_TYPE_KING ||
         ht == HALL_PIECE_TYPE_ROOK || ht == HALL_PIECE_TYPE_BISHOP ||
         ht == HALL_PIECE_TYPE_KNIGHT;
}

static void hint_push(uint8_t sq, piece_t piece, bool vacate) {
  hint_t *h = &s_hint[s_hint_n % HINT_CAP];
  s_hint_n++;
  h->sq = sq;
  h->piece = piece;
  h->seq = ++s_hint_seq;
  h->vacate = vacate;
}

static piece_t hint_before_place(uint8_t dest) {
  int n = s_hint_n < HINT_CAP ? s_hint_n : HINT_CAP;
  uint32_t place_seq = 0;
  bool found_place = false;
  for (int i = 0; i < n; i++) {
    if (s_hint[i].vacate || s_hint[i].sq != dest) {
      continue;
    }
    if (!found_place || s_hint[i].seq >= place_seq) {
      place_seq = s_hint[i].seq;
      found_place = true;
    }
  }
  if (!found_place) {
    return PIECE_EMPTY;
  }
  piece_t best = PIECE_EMPTY;
  uint32_t best_seq = 0;
  for (int i = 0; i < n; i++) {
    if (!s_hint[i].vacate || s_hint[i].seq >= place_seq) {
      continue;
    }
    if (best == PIECE_EMPTY || s_hint[i].seq >= best_seq) {
      best_seq = s_hint[i].seq;
      best = s_hint[i].piece;
    }
  }
  return best;
}

static void note_hint_vs_mover(piece_t mover, uint8_t dest_sq) {
  static uint8_t s_logged_sq = 255;
  static piece_t s_logged_mover = PIECE_EMPTY;
  piece_t hinted = hint_before_place(dest_sq);
  if (hinted == PIECE_EMPTY || hinted == mover) {
    return;
  }
  if (s_logged_sq == dest_sq && s_logged_mover == mover) {
    return;
  }
  s_logged_sq = dest_sq;
  s_logged_mover = mover;
  ESP_LOGW(TAG, "sq %u identity hint piece %d, occupancy mover %d kept",
           (unsigned)dest_sq, (int)hinted, (int)mover);
}

static bool move_is_castle(const chess_move_t *move) {
  return (move->piece == PIECE_WHITE_KING || move->piece == PIECE_BLACK_KING) &&
         move->from_row == move->to_row &&
         (move->to_col > move->from_col ? move->to_col - move->from_col
                                        : move->from_col - move->to_col) == 2;
}

static bool try_castle(const uint8_t occ[64], chess_move_t *out) {
  static const uint8_t rank[2] = {0, 7};
  static const piece_t king[2] = {PIECE_WHITE_KING, PIECE_BLACK_KING};
  static const piece_t rook[2] = {PIECE_WHITE_ROOK, PIECE_BLACK_ROOK};
  for (int side = 0; side < 2; side++) {
    uint8_t row = rank[side];
    uint8_t k_home = (uint8_t)(row * 8 + 4);
    if (s_last_valid[row][4] != king[side] || occ[k_home] != 0) {
      continue;
    }
    static const uint8_t king_to[2] = {6, 2};
    static const uint8_t rook_from[2] = {7, 0};
    static const uint8_t rook_to[2] = {5, 3};
    for (int wing = 0; wing < 2; wing++) {
      uint8_t k_to = (uint8_t)(row * 8 + king_to[wing]);
      uint8_t r_from = (uint8_t)(row * 8 + rook_from[wing]);
      uint8_t r_to = (uint8_t)(row * 8 + rook_to[wing]);
      if (s_last_valid[row][rook_from[wing]] != rook[side] || occ[r_from] != 0 ||
          occ[k_to] == 0 || occ[r_to] == 0) {
        continue;
      }
      if (s_last_valid[k_to / 8][k_to % 8] != PIECE_EMPTY ||
          s_last_valid[r_to / 8][r_to % 8] != PIECE_EMPTY) {
        continue;
      }
      if (!landing_accepts(king[side], k_to) || !landing_accepts(rook[side], r_to)) {
        continue;
      }
      chess_move_t move = {.from_row = row,
                           .from_col = 4,
                           .to_row = row,
                           .to_col = king_to[wing],
                           .piece = king[side],
                           .captured_piece = PIECE_EMPTY};
      if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
        continue;
      }
      int extras = 0;
      uint8_t used[4] = {k_home, k_to, r_from, r_to};
      for (uint8_t sq = 0; sq < 64; sq++) {
        bool castle_sq = false;
        for (int u = 0; u < 4; u++) {
          if (sq == used[u]) {
            castle_sq = true;
          }
        }
        if (castle_sq) {
          continue;
        }
        if (!square_tolerant(sq, s_last_valid[sq / 8][sq % 8], occ[sq])) {
          extras++;
        }
      }
      if (extras != 0) {
        continue;
      }
      *out = move;
      return true;
    }
  }
  return false;
}

static bool try_en_passant(const hall_diff_t *diff, int n, chess_move_t *out) {
  if (n != 3) {
    return false;
  }
  int origin = -1;
  int dest = -1;
  int victim = -1;
  for (int i = 0; i < 3; i++) {
    if (occ[diff[i].sq] == 0 && hall_side_owns(diff[i].expect)) {
      origin = i;
    } else if (occ[diff[i].sq] == 0 && diff[i].expect != PIECE_EMPTY) {
      victim = i;
    } else if (occ[diff[i].sq] != 0 && diff[i].expect == PIECE_EMPTY) {
      dest = i;
    }
  }
  if (origin < 0 || dest < 0 || victim < 0) {
    return false;
  }
  piece_t mover = diff[origin].expect;
  uint8_t dest_sq = diff[dest].sq;
  if (!landing_accepts(mover, dest_sq)) {
    return false;
  }
  chess_move_t move = {.from_row = (uint8_t)(diff[origin].sq / 8),
                       .from_col = (uint8_t)(diff[origin].sq % 8),
                       .to_row = (uint8_t)(dest_sq / 8),
                       .to_col = (uint8_t)(dest_sq % 8),
                       .piece = mover,
                       .captured_piece = PIECE_EMPTY};
  if (!game_is_en_passant_possible(&move)) {
    return false;
  }
  uint8_t victim_sq = (uint8_t)(diff[victim].sq);
  if (victim_sq / 8 != move.from_row || victim_sq % 8 != move.to_col) {
    return false;
  }
  if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
    return false;
  }
  note_hint_vs_mover(mover, dest_sq);
  *out = move;
  return true;
}

static bool try_one_legal(const hall_diff_t *diff, const uint8_t occ[64],
                          chess_move_t *out) {
  int origin = -1;
  int dest = -1;
  for (int i = 0; i < 2; i++) {
    if (occ[diff[i].sq] == 0 && diff[i].expect != PIECE_EMPTY) {
      if (origin >= 0) {
        return false;
      }
      origin = i;
    } else if (occ[diff[i].sq] != 0) {
      dest = i;
    }
  }
  if (origin < 0 || dest < 0 || !hall_side_owns(diff[origin].expect)) {
    return false;
  }
  piece_t mover = diff[origin].expect;
  piece_t landing = diff[dest].expect;
  if (landing != PIECE_EMPTY && hall_side_owns(landing)) {
    return false;
  }
  uint8_t dest_sq = diff[dest].sq;
  if (occ[dest_sq] == 0 || !landing_accepts(mover, dest_sq)) {
    return false;
  }
  chess_move_t move = {.from_row = (uint8_t)(diff[origin].sq / 8),
                       .from_col = (uint8_t)(diff[origin].sq % 8),
                       .to_row = (uint8_t)(dest_sq / 8),
                       .to_col = (uint8_t)(dest_sq % 8),
                       .piece = mover,
                       .captured_piece = landing};
  if (move_is_castle(&move) || game_is_en_passant_possible(&move)) {
    return false;
  }
  if (board[move.from_row][move.from_col] != mover) {
    return false;
  }
  if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
    return false;
  }
  note_hint_vs_mover(mover, dest_sq);
  *out = move;
  return true;
}

static game_hall_pos_class_t classify(const uint8_t occ[64], chess_move_t *legal) {
  bool occ_now = occupancy_equal(s_last_valid, occ);
  bool id_now = identity_clear(s_last_valid, occ);
  s_attention = false;
  s_lift_sq = 255;
  if (occ_now && id_now) {
    s_eq_contra = 0;
    return GAME_HALL_POS_EQUAL;
  }
  if (occ_now) {
    if (s_eq_contra < 255) {
      s_eq_contra++;
    }
    if (s_eq_contra < HALL_CONTRA_SCANS) {
      return GAME_HALL_POS_EQUAL;
    }
    s_attention = true;
    return GAME_HALL_POS_RESTORE;
  }
  s_eq_contra = 0;

  piece_t previous[8][8];
  if (game_physical_undo_copy_previous(previous) &&
      occupancy_equal(previous, occ)) {
    if (identity_clear(previous, occ)) {
      s_prev_contra = 0;
      fill_cyan(occ);
      if (game_physical_undo_enabled()) {
        return GAME_HALL_POS_PREVIOUS;
      }
      return GAME_HALL_POS_RESTORE;
    }
    if (s_prev_contra < 255) {
      s_prev_contra++;
    }
    if (s_prev_contra < HALL_CONTRA_SCANS && game_physical_undo_enabled()) {
      fill_cyan(occ);
      return GAME_HALL_POS_PREVIOUS;
    }
  } else {
    s_prev_contra = 0;
  }

  hall_diff_t diff[HALL_POS_DIFF_CAP];
  int n = gather_diffs(occ, diff, HALL_POS_DIFF_CAP);
  if (n == 1 && diff[0].occ == 0 && hall_side_owns(diff[0].expect)) {
    s_lift_sq = diff[0].sq;
    return GAME_HALL_POS_MOVE_PARTIAL;
  }
  if (n == 4 && try_castle(occ, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  if (n == 3 && try_en_passant(diff, n, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  if (n == 2 && try_one_legal(diff, occ, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  return GAME_HALL_POS_RESTORE;
}

static bool special_mode_holds(void) {
  if (game_demo_show_active() || promotion_state.pending ||
      game_ep_capture_waiting() || game_is_opening_trainer_active() ||
      game_is_puzzle_active()) {
    return true;
  }
  return false;
}

static void release_timer(void) {
  if (!s_timer_held) {
    return;
  }
  s_timer_held = false;
  game_resume_timer();
}

static void hold_timer(void) {
  if (s_timer_held || !game_is_timer_active()) {
    return;
  }
  if (game_pause_timer() == ESP_OK) {
    s_timer_held = true;
  }
}

static bool commit_legal(const chess_move_t *move) {
  if (current_game_state == GAME_STATE_FINISHED || promotion_state.pending) {
    return false;
  }
  release_timer();
  return game_execute_move(move);
}

static void show_own_lift(uint8_t sq) {
  static uint8_t painted = 255;
  uint8_t row = (uint8_t)(sq / 8);
  uint8_t col = (uint8_t)(sq % 8);
  if (s_lift_shown && painted == sq) {
    return;
  }
  painted = sq;
  s_lift_shown = true;
  s_lift_sq = sq;
  game_hall_restore_guide_stop_leds();
  led_clear_board_only();
  led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0);
  if (!game_led_guidance_show_destinations()) {
    return;
  }
  move_suggestion_t suggestions[64];
  uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
  for (uint32_t i = 0; i < valid_moves; i++) {
    uint8_t led_index =
        chess_pos_to_led_index(suggestions[i].to_row, suggestions[i].to_col);
    if (suggestions[i].is_castling) {
      if (chess_policy_move_hints_castling_blue()) {
        led_set_pixel_safe(led_index, 0, 0, 255);
      }
      continue;
    }
    piece_t dest_piece = board[suggestions[i].to_row][suggestions[i].to_col];
    bool opponent = (current_player == PLAYER_WHITE &&
                     dest_piece >= PIECE_BLACK_PAWN &&
                     dest_piece <= PIECE_BLACK_KING) ||
                    (current_player == PLAYER_BLACK &&
                     dest_piece >= PIECE_WHITE_PAWN &&
                     dest_piece <= PIECE_WHITE_KING);
    if (suggestions[i].is_check) {
      led_set_pixel_safe(led_index, 255, 255, 255);
    } else if (opponent) {
      led_set_pixel_safe(led_index, 255, 165, 0);
    } else {
      led_set_pixel_safe(led_index, 0, 255, 0);
    }
  }
}

static void show_attention(const uint8_t occ[64]) {
  led_clear_board_only();
  game_hall_restore_guide_stop_leds();
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t expect = s_last_valid[sq / 8][sq % 8];
    if (square_tolerant(sq, expect, occ[sq])) {
      s_attn_logged &= ~((uint64_t)1 << sq);
      continue;
    }
    led_set_pixel_safe(chess_pos_to_led_index(sq / 8, sq % 8), 255, 40, 0);
    if ((s_attn_logged & ((uint64_t)1 << sq)) == 0) {
      s_attn_logged |= (uint64_t)1 << sq;
      ESP_LOGW(TAG, "sq %u Hall identity disagrees, engine piece %d kept",
               (unsigned)sq, (int)expect);
    }
  }
}

static void note_class(game_hall_pos_class_t kind) {
  if (kind == s_class) {
    return;
  }
  s_class = kind;
  s_class_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
  s_cyan_shown = false;
  s_guide_drawn = false;
  s_lift_shown = false;
  s_pending_valid = false;
  if (kind == GAME_HALL_POS_PREVIOUS) {
    game_send_response_to_uart(
        "Previous position — takeback if it stays still.", false, NULL);
  } else if (kind == GAME_HALL_POS_RESTORE && !s_attention) {
    game_send_response_to_uart(
        "Put the pieces back. The last legal position is unchanged.", false,
        NULL);
  }
}

static void paint_for_class(const uint8_t occ[64]) {
  if (s_class == GAME_HALL_POS_MOVE_PARTIAL && s_lift_sq < 64) {
    release_timer();
    show_own_lift(s_lift_sq);
    return;
  }
  if (s_class == GAME_HALL_POS_PREVIOUS) {
    hold_timer();
    if (!s_cyan_shown && s_cyan_n > 0) {
      game_hall_led_show_takeback(s_cyan_sq, s_cyan_n);
      s_cyan_shown = true;
    }
    return;
  }
  if (s_class == GAME_HALL_POS_RESTORE && s_attention) {
    hold_timer();
    show_attention(occ);
    s_guide_drawn = true;
  }
}

static void service_unchanged(uint32_t now_ms) {
  uint32_t held = (uint32_t)(now_ms - s_class_since_ms);
  if (s_class == GAME_HALL_POS_PREVIOUS && held >= HALL_POS_TAKEBACK_MS) {
    release_timer();
    if (game_undo_rewind_last_move()) {
      s_class = GAME_HALL_POS_EQUAL;
      s_pending_valid = false;
      s_attention = false;
      game_send_response_to_uart("Takeback complete.", false, NULL);
      chess_policy_refresh_idle_board_leds();
    } else {
      note_class(GAME_HALL_POS_RESTORE);
      hold_timer();
    }
    return;
  }
  if (s_class == GAME_HALL_POS_ONE_LEGAL && s_pending_valid &&
      held >= HALL_POS_STABLE_MS) {
    s_in_service = true;
    chess_move_t move = s_pending_move;
    s_pending_valid = false;
    if (!commit_legal(&move)) {
      note_class(GAME_HALL_POS_RESTORE);
      hold_timer();
    } else {
      s_class = GAME_HALL_POS_EQUAL;
      s_attention = false;
      if (!promotion_state.pending) {
        chess_policy_refresh_idle_board_leds();
      }
    }
    s_in_service = false;
    return;
  }
  if (s_class == GAME_HALL_POS_RESTORE && !s_attention && !s_guide_drawn &&
      held >= HALL_POS_STABLE_MS) {
    hold_timer();
    if (game_hall_restore_guide_show_play()) {
      s_guide_drawn = true;
    }
  }
}

static void arm_legal(const chess_move_t *legal) {
  if (!s_pending_valid ||
      memcmp(&s_pending_move, legal, sizeof(s_pending_move)) != 0) {
    s_class_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
  }
  s_pending_move = *legal;
  s_pending_valid = true;
}

void game_hall_snapshot_last_valid(void) {
  memcpy(s_last_valid, board, sizeof(s_last_valid));
  s_digest_valid = false;
  s_class = GAME_HALL_POS_EQUAL;
  s_guide_drawn = false;
  s_cyan_shown = false;
  s_lift_shown = false;
  s_pending_valid = false;
  s_attention = false;
  s_eq_contra = 0;
  s_prev_contra = 0;
  s_hint_n = 0;
  s_attn_logged = 0;
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  capture_in_progress = false;
  memset(s_hint, 0, sizeof(s_hint));
}

bool game_hall_positions_equal(const piece_t expected[8][8]) {
  uint8_t occ[64];
  if (expected == NULL || !hall_i2c_squares_readings_usable()) {
    return false;
  }
  squares_get_state(occ);
  return occupancy_equal(expected, occ) && identity_clear(expected, occ);
}

int game_hall_position_mismatch_count(void) {
  uint8_t occ[64];
  int n = 0;
  if (!hall_i2c_squares_readings_usable()) {
    return 64;
  }
  squares_get_state(occ);
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (square_tolerant(sq, s_last_valid[sq / 8][sq % 8], occ[sq])) {
      continue;
    }
    n++;
    if (n > 4) {
      return n;
    }
  }
  return n;
}

void game_hall_position_on_matrix_event(const chess_move_command_t *cmd) {
  uint8_t row = 0;
  uint8_t col = 0;
  const char *notation = NULL;
  if (cmd == NULL) {
    return;
  }
  if (cmd->type == GAME_CMD_PICKUP) {
    notation = cmd->from_notation;
  } else if (cmd->type == GAME_CMD_DROP) {
    notation = cmd->to_notation;
  }
  if (notation != NULL && convert_notation_to_coords(notation, &row, &col)) {
    uint8_t sq = (uint8_t)(row * 8 + col);
    if (cmd->type == GAME_CMD_PICKUP) {
      hint_push(sq, s_last_valid[row][col], true);
    } else {
      hint_push(sq, PIECE_EMPTY, false);
    }
  }
  game_hall_position_service();
}

bool game_hall_position_blocks_idle_leds(void) {
  return s_class != GAME_HALL_POS_EQUAL;
}

void game_hall_position_service(void) {
  uint8_t occ[64];
  uint8_t digest[8];
  chess_move_t legal;
  game_hall_pos_class_t kind;
  uint32_t now_ms;
  bool digest_changed;

  if (s_in_service) {
    return;
  }
  if (game_get_move_count() == 0 && !game_hall_restore_guide_in_play()) {
    return;
  }
  if (!hall_i2c_squares_readings_usable()) {
    return;
  }
  if (game_is_squares_guard_active() || special_mode_holds()) {
    return;
  }

  squares_get_state(occ);
  pack_occ(occ, digest);
  now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  digest_changed = !s_digest_valid || memcmp(digest, s_occ_digest, 8) != 0;
  if (!digest_changed) {
    memset(&legal, 0, sizeof(legal));
    kind = classify(occ, &legal);
    if (kind != s_class) {
      bool was_equal = s_class == GAME_HALL_POS_EQUAL;
      note_class(kind);
      if (kind == GAME_HALL_POS_ONE_LEGAL) {
        arm_legal(&legal);
      }
      paint_for_class(occ);
      if (kind == GAME_HALL_POS_EQUAL && !was_equal) {
        release_timer();
        if (game_hall_restore_guide_is_active()) {
          game_hall_restore_guide_stop_leds();
        }
        chess_policy_refresh_idle_board_leds();
      }
      return;
    }
    service_unchanged(now_ms);
    return;
  }

  memcpy(s_occ_digest, digest, 8);
  s_digest_valid = true;
  memset(&legal, 0, sizeof(legal));
  kind = classify(occ, &legal);
  {
    game_hall_pos_class_t prev_class = s_class;
    note_class(kind);
    if (kind == GAME_HALL_POS_ONE_LEGAL) {
      arm_legal(&legal);
    }
    if (kind == GAME_HALL_POS_EQUAL) {
      release_timer();
      bool guide_was = game_hall_restore_guide_is_active();
      if (guide_was) {
        game_hall_restore_guide_stop_leds();
      }
      if (guide_was || prev_class != GAME_HALL_POS_EQUAL) {
        chess_policy_refresh_idle_board_leds();
      }
      return;
    }
  }
  paint_for_class(occ);
  if (s_class == GAME_HALL_POS_RESTORE && !s_attention) {
    hold_timer();
  }
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
