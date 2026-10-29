/**
 * @file game_hall_position.c
 * @brief Diff the Hall occupancy snapshot against the last legal position.
 *
 * Pickup and drop commands only wake this and leave an identity hint.
 * They do not commit, reject, or rewrite board[][].
 */

#include "game_hall_position.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "game_gesture.h"
#include "game_hall_led.h"
#include "game_hall_match.h"
#include "game_hall_restore_guide.h"
#include "game_squares_guard.h"
#include "game_physical_undo.h"
#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "position.h"
#include "game_board_transition.h"
#include "game_policy.h"

#include "hall_i2c_squares.h"
#include "squares_task.h"
#include "game_events.h"
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
#define HALL_CONTRA_MS 700U
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
static uint32_t s_eq_contra_ms;
static uint8_t s_prev_contra;
static chess_move_t s_pending_move;
static bool s_pending_valid;
static bool s_takeback_off_said;
static bool s_resign_ring;
static bool s_resign_latched;
static bool s_chaos_restore;
static bool s_idle_handover;
static bool s_chaos_match;
static uint32_t s_chaos_match_since_ms;
static bool s_castle_paint;
static uint8_t s_castle_kh, s_castle_kt, s_castle_rh, s_castle_rt;
/* Rook slid to the castle transit square and may still become a castle. */
static bool s_conv_armed;
static bool s_conv_commit;
static uint8_t s_conv_row;
static uint8_t s_conv_kto;
static uint8_t s_conv_rfrom;
static uint8_t s_conv_rto;
static bool s_ep_paint;
static uint8_t s_ep_from, s_ep_to, s_ep_victim;
static bool s_cap_paint;
static uint8_t s_cap_from;
static uint8_t s_cap_victim;

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
  hall_piece_type_t ht;
  hall_piece_color_t hc;
  bool white;
  bool royal;
  if (expect == PIECE_EMPTY || !hall_i2c_squares_identity_settled(sq)) {
    return false;
  }
  ht = squares_get_hall_piece_type(sq);
  hc = squares_get_hall_piece_color(sq);
  if (ht == HALL_PIECE_TYPE_NONE || hc == HALL_PIECE_COLOR_NONE) {
    return false;
  }
  /* King and queen share one band. That swap is not a contradiction. */
  white = expect >= PIECE_WHITE_PAWN && expect <= PIECE_WHITE_KING;
  royal = expect == PIECE_WHITE_KING || expect == PIECE_WHITE_QUEEN ||
          expect == PIECE_BLACK_KING || expect == PIECE_BLACK_QUEEN;
  if (royal && (ht == HALL_PIECE_TYPE_KING || ht == HALL_PIECE_TYPE_QUEEN) &&
      ((white && hc == HALL_PIECE_COLOR_WHITE) ||
       (!white && hc == HALL_PIECE_COLOR_BLACK))) {
    return false;
  }
  return !game_hall_identity_matches(expect, ht, hc);
}

/** Occupancy only. A wrong Hall type on a full square is not a moved piece. */
static bool occ_ok(piece_t expect, uint8_t occ) {
  if (expect == PIECE_EMPTY) {
    return occ == 0;
  }
  return occ != 0;
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
    if (occ_ok(expect, occ[sq])) {
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
    if (!occ_ok(s_last_valid[sq / 8][sq % 8], occ[sq])) {
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

static bool try_en_passant(const hall_diff_t *diff, int n, const uint8_t occ[64],
                          chess_move_t *out) {
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
  if (game_get_piece(move.from_row, move.from_col) != mover) {
    return false;
  }
  if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
    return false;
  }
  note_hint_vs_mover(mover, dest_sq);
  *out = move;
  return true;
}

static bool opponent_piece(piece_t piece) {
  if (piece == PIECE_EMPTY || hall_side_owns(piece)) {
    return false;
  }
  return (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) ||
         (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
}

static bool diffs_only_on(const uint8_t occ[64], const uint8_t *sqs, int ns) {
  uint8_t sq;
  for (sq = 0; sq < 64; sq++) {
    bool used = false;
    int i;
    if (square_tolerant(sq, s_last_valid[sq / 8][sq % 8], occ[sq])) {
      continue;
    }
    for (i = 0; i < ns; i++) {
      if (sqs[i] == sq) {
        used = true;
      }
    }
    if (!used) {
      return false;
    }
  }
  return true;
}

static bool castle_wing_legal(int side, int wing, uint8_t *kh, uint8_t *kt,
                              uint8_t *rh, uint8_t *rt) {
  static const uint8_t rank[2] = {0, 7};
  static const piece_t king[2] = {PIECE_WHITE_KING, PIECE_BLACK_KING};
  static const piece_t rook[2] = {PIECE_WHITE_ROOK, PIECE_BLACK_ROOK};
  static const uint8_t king_to[2] = {6, 2};
  static const uint8_t rook_from[2] = {7, 0};
  static const uint8_t rook_to[2] = {5, 3};
  uint8_t row = rank[side];
  chess_move_t move;
  if (s_last_valid[row][4] != king[side] ||
      s_last_valid[row][rook_from[wing]] != rook[side]) {
    return false;
  }
  *kh = (uint8_t)(row * 8 + 4);
  *kt = (uint8_t)(row * 8 + king_to[wing]);
  *rh = (uint8_t)(row * 8 + rook_from[wing]);
  *rt = (uint8_t)(row * 8 + rook_to[wing]);
  if (s_last_valid[*kt / 8][*kt % 8] != PIECE_EMPTY ||
      s_last_valid[*rt / 8][*rt % 8] != PIECE_EMPTY) {
    return false;
  }
  memset(&move, 0, sizeof(move));
  move.from_row = row;
  move.from_col = 4;
  move.to_row = row;
  move.to_col = king_to[wing];
  move.piece = king[side];
  return game_is_valid_move(&move) == MOVE_ERROR_NONE;
}

static bool try_castle_partial(const uint8_t occ[64]) {
  int side;
  int wing;
  for (side = 0; side < 2; side++) {
    for (wing = 0; wing < 2; wing++) {
      uint8_t kh, kt, rh, rt;
      uint8_t used[4];
      bool king_home, king_dest, rook_home, rook_dest;
      if (!castle_wing_legal(side, wing, &kh, &kt, &rh, &rt)) {
        continue;
      }
      /* Opponent's king must not lock the scan into a castle guide. */
      if (!hall_side_owns(side == 0 ? PIECE_WHITE_KING : PIECE_BLACK_KING)) {
        continue;
      }
      king_home = occ[kh] != 0;
      king_dest = occ[kt] != 0;
      rook_home = occ[rh] != 0;
      rook_dest = occ[rt] != 0;
      if ((king_home && king_dest) || (rook_home && rook_dest)) {
        continue;
      }
      if (king_home && rook_home) {
        continue;
      }
      /* Rook-first is a normal rook move. King-first keeps this wait. */
      if (king_home) {
        continue;
      }
      /* Both pieces seated: try_castle commits once. */
      if (king_dest && rook_dest) {
        continue;
      }
      used[0] = kh;
      used[1] = kt;
      used[2] = rh;
      used[3] = rt;
      {
        uint8_t sq;
        bool only = true;
        for (sq = 0; sq < 64 && only; sq++) {
          int i;
          bool on = false;
          if (occ_ok(s_last_valid[sq / 8][sq % 8], occ[sq])) {
            continue;
          }
          for (i = 0; i < 4; i++) {
            if (used[i] == sq) {
              on = true;
            }
          }
          if (!on) {
            only = false;
          }
        }
        if (!only) {
          continue;
        }
      }
      s_castle_paint = true;
      s_castle_kh = kh;
      s_castle_kt = kt;
      s_castle_rh = rh;
      s_castle_rt = rt;
      return true;
    }
  }
  return false;
}

static bool try_ep_partial(const uint8_t occ[64]) {
  uint8_t from = 255;
  uint8_t to;
  uint8_t victim;
  uint8_t sq;
  uint8_t used[3];
  bool pawn_home, pawn_dest, victim_here;
  if (!en_passant_available) {
    return false;
  }
  to = (uint8_t)(en_passant_target_row * 8 + en_passant_target_col);
  victim = (uint8_t)(en_passant_victim_row * 8 + en_passant_victim_col);
  for (sq = 0; sq < 64; sq++) {
    piece_t pawn = s_last_valid[sq / 8][sq % 8];
    chess_move_t move;
    if (pawn != PIECE_WHITE_PAWN && pawn != PIECE_BLACK_PAWN) {
      continue;
    }
    if (!hall_side_owns(pawn)) {
      continue;
    }
    memset(&move, 0, sizeof(move));
    move.from_row = (uint8_t)(sq / 8);
    move.from_col = (uint8_t)(sq % 8);
    move.to_row = en_passant_target_row;
    move.to_col = en_passant_target_col;
    move.piece = pawn;
    if (!game_is_en_passant_possible(&move)) {
      continue;
    }
    if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
      continue;
    }
    from = sq;
    break;
  }
  if (from == 255) {
    return false;
  }
  pawn_home = occ[from] != 0;
  pawn_dest = occ[to] != 0;
  victim_here = occ[victim] != 0;
  /* Only the landed partial: capturer on the empty square, victim still there. */
  if (pawn_home || !pawn_dest || !victim_here) {
    return false;
  }
  if (!landing_accepts(s_last_valid[from / 8][from % 8], to)) {
    return false;
  }
  used[0] = from;
  used[1] = to;
  used[2] = victim;
  if (!diffs_only_on(occ, used, 3)) {
    return false;
  }
  s_ep_paint = true;
  s_ep_from = from;
  s_ep_to = to;
  s_ep_victim = victim;
  return true;
}

static bool legal_capture(uint8_t from, uint8_t to) {
  chess_move_t move;
  piece_t mover = s_last_valid[from / 8][from % 8];
  piece_t landing = s_last_valid[to / 8][to % 8];
  if (!hall_side_owns(mover) || !opponent_piece(landing)) {
    return false;
  }
  memset(&move, 0, sizeof(move));
  move.from_row = (uint8_t)(from / 8);
  move.from_col = (uint8_t)(from % 8);
  move.to_row = (uint8_t)(to / 8);
  move.to_col = (uint8_t)(to % 8);
  move.piece = mover;
  move.captured_piece = landing;
  if (move_is_castle(&move) || game_is_en_passant_possible(&move)) {
    return false;
  }
  return game_is_valid_move(&move) == MOVE_ERROR_NONE;
}

static bool try_capture_partial(const hall_diff_t *diff, int n,
                                const uint8_t occ[64]) {
  int own = -1;
  int victim = -1;
  int i;
  if (n < 1 || n > 2) {
    return false;
  }
  for (i = 0; i < n; i++) {
    if (occ[diff[i].sq] == 0 && hall_side_owns(diff[i].expect)) {
      if (own >= 0) {
        return false;
      }
      own = i;
    } else if (occ[diff[i].sq] == 0 && opponent_piece(diff[i].expect)) {
      if (victim >= 0) {
        return false;
      }
      victim = i;
    } else {
      return false;
    }
  }
  if (victim < 0) {
    return false;
  }
  if (own >= 0) {
    if (!legal_capture(diff[own].sq, diff[victim].sq)) {
      return false;
    }
    s_cap_paint = true;
    s_cap_from = diff[own].sq;
    s_cap_victim = diff[victim].sq;
    return true;
  }
  for (i = 0; i < 64; i++) {
    if (!legal_capture((uint8_t)i, diff[victim].sq)) {
      continue;
    }
    s_cap_paint = true;
    s_cap_from = 255;
    s_cap_victim = diff[victim].sq;
    return true;
  }
  return false;
}

static bool hint_latest(uint8_t sq, bool vacate, uint32_t *seq_out) {
  int n = s_hint_n < HINT_CAP ? s_hint_n : HINT_CAP;
  int i;
  bool found = false;
  uint32_t best = 0;
  for (i = 0; i < n; i++) {
    if (s_hint[i].sq != sq || s_hint[i].vacate != vacate) {
      continue;
    }
    if (!found || s_hint[i].seq >= best) {
      best = s_hint[i].seq;
      found = true;
    }
  }
  if (!found) {
    return false;
  }
  *seq_out = best;
  return true;
}

/** Victim square emptied, then filled again after this piece was lifted. */
static bool hint_refilled_after_lift(uint8_t from, uint8_t dest) {
  uint32_t lift_seq = 0;
  uint32_t vac_seq = 0;
  uint32_t place_seq = 0;
  if (!hint_latest(from, true, &lift_seq) ||
      !hint_latest(dest, true, &vac_seq) ||
      !hint_latest(dest, false, &place_seq)) {
    return false;
  }
  return place_seq > vac_seq && place_seq > lift_seq;
}

static bool dest_settled(uint8_t sq, hall_piece_color_t *hc) {
  hall_piece_type_t ht;
  if (!hall_i2c_squares_identity_settled(sq)) {
    return false;
  }
  ht = squares_get_hall_piece_type(sq);
  *hc = squares_get_hall_piece_color(sq);
  return ht != HALL_PIECE_TYPE_NONE && *hc != HALL_PIECE_COLOR_NONE;
}

/** Settled landing shows the mover's colour and a compatible type. */
static bool mover_landed(piece_t mover, uint8_t sq) {
  hall_piece_color_t hc = HALL_PIECE_COLOR_NONE;
  if (!dest_settled(sq, &hc) || !same_color_reading(mover, hc)) {
    return false;
  }
  return landing_accepts(mover, sq);
}

static bool arm_capture(uint8_t from, uint8_t to, chess_move_t *out) {
  piece_t mover = s_last_valid[from / 8][from % 8];
  chess_move_t move;
  memset(&move, 0, sizeof(move));
  move.from_row = (uint8_t)(from / 8);
  move.from_col = (uint8_t)(from % 8);
  move.to_row = (uint8_t)(to / 8);
  move.to_col = (uint8_t)(to % 8);
  move.piece = mover;
  move.captured_piece = s_last_valid[to / 8][to % 8];
  if (game_get_piece(move.from_row, move.from_col) != mover) {
    return false;
  }
  note_hint_vs_mover(mover, to);
  *out = move;
  return true;
}

/**
 * A finished capture keeps the victim square occupied, so the occupancy diff
 * is only the empty source. Colour (K/Q band included) or one vacate+refill
 * hint names the landing. An open reading waits; a settled opponent does not.
 */
static bool try_landed_capture(uint8_t from, const uint8_t occ[64],
                               chess_move_t *out) {
  piece_t mover = s_last_valid[from / 8][from % 8];
  int confirmed = 0;
  int hinted = 0;
  int waiting = 0;
  uint8_t confirmed_to = 255;
  uint8_t hinted_to = 255;
  uint8_t wait_to = 255;
  uint8_t to;
  bool armed = false;

  for (to = 0; to < 64; to++) {
    hall_piece_color_t hc = HALL_PIECE_COLOR_NONE;
    bool settled;
    bool refilled;
    uint32_t vac_seq = 0;
    uint32_t place_seq = 0;
    bool vac;
    bool place;
    if (to == from || occ[to] == 0 || !legal_capture(from, to)) {
      continue;
    }
    settled = dest_settled(to, &hc);
    if (mover_landed(mover, to)) {
      confirmed++;
      confirmed_to = to;
      continue;
    }
    refilled = hint_refilled_after_lift(from, to);
    if (refilled && (!settled || same_color_reading(mover, hc))) {
      hinted++;
      hinted_to = to;
      continue;
    }
    if (settled && same_color_reading(mover, hc)) {
      waiting++;
      wait_to = to;
      continue;
    }
    if (settled) {
      continue;
    }
    vac = hint_latest(to, true, &vac_seq);
    place = hint_latest(to, false, &place_seq);
    if (vac && (!place || place_seq < vac_seq)) {
      waiting++;
      wait_to = to;
    }
  }

  if (confirmed == 1 && hinted == 0) {
    armed = arm_capture(from, confirmed_to, out);
  } else if (confirmed == 0 && hinted == 1) {
    armed = arm_capture(from, hinted_to, out);
  }
  if (armed) {
    return true;
  }
  if (confirmed > 0 || hinted > 0 || waiting > 0) {
    s_cap_paint = true;
    s_cap_from = from;
    s_cap_victim = 255;
    if (confirmed + hinted + waiting == 1) {
      if (confirmed == 1) {
        s_cap_victim = confirmed_to;
      } else if (hinted == 1) {
        s_cap_victim = hinted_to;
      } else {
        s_cap_victim = wait_to;
      }
    }
    return false;
  }
  return false;
}

static bool conv_squares_only(const uint8_t occ[64]) {
  uint8_t kh = (uint8_t)(s_conv_row * 8 + 4);
  uint8_t kt = (uint8_t)(s_conv_row * 8 + s_conv_kto);
  uint8_t sq;
  for (sq = 0; sq < 64; sq++) {
    if (occ_ok(s_last_valid[sq / 8][sq % 8], occ[sq])) {
      continue;
    }
    if (sq != kh && sq != kt) {
      return false;
    }
  }
  return true;
}

static void clear_rook_convert(void) {
  s_conv_armed = false;
  s_conv_commit = false;
  s_castle_paint = false;
}

void game_hall_position_clear_castle_wait(void) { clear_rook_convert(); }

/* King of the side that just slid the rook is travelling to its castle square. */
static bool try_rook_convert(const uint8_t occ[64], chess_move_t *legal) {
  uint8_t kh;
  uint8_t kt;
  uint8_t rhome;
  piece_t king;
  if (!s_conv_armed) {
    return false;
  }
  /* Rook back on its home square is not a rook-first castle. */
  rhome = (uint8_t)(s_conv_row * 8 + s_conv_rfrom);
  if (rhome < 64 && occ[rhome] != 0) {
    clear_rook_convert();
    return false;
  }
  if (!conv_squares_only(occ)) {
    return false;
  }
  kh = (uint8_t)(s_conv_row * 8 + 4);
  kt = (uint8_t)(s_conv_row * 8 + s_conv_kto);
  if (occ[kh] != 0) {
    return false;
  }
  king = s_last_valid[s_conv_row][4];
  s_castle_kh = kh;
  s_castle_kt = kt;
  s_castle_rh = (uint8_t)(s_conv_row * 8 + s_conv_rfrom);
  s_castle_rt = (uint8_t)(s_conv_row * 8 + s_conv_rto);
  s_conv_commit = (occ[kt] != 0);
  if (s_conv_commit) {
    s_castle_paint = false;
    memset(legal, 0, sizeof(*legal));
    legal->from_row = s_conv_row;
    legal->from_col = 4;
    legal->to_row = s_conv_row;
    legal->to_col = s_conv_kto;
    legal->piece = king;
    return true;
  }
  s_castle_paint = true;
  return true;
}

static bool rook_move_can_become_castle(const chess_move_t *move) {
  chess_move_t castle;
  bool kingside;
  bool queenside;
  piece_t king;
  if (move->captured_piece != PIECE_EMPTY || move->from_row != move->to_row) {
    return false;
  }
  if (move->piece != PIECE_WHITE_ROOK && move->piece != PIECE_BLACK_ROOK) {
    return false;
  }
  kingside = move->from_col == 7 && move->to_col == 5;
  queenside = move->from_col == 0 && move->to_col == 3;
  if (!kingside && !queenside) {
    return false;
  }
  king = (move->piece == PIECE_WHITE_ROOK) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
  if (game_get_piece(move->from_row, 4) != king) {
    return false;
  }
  memset(&castle, 0, sizeof(castle));
  castle.from_row = move->from_row;
  castle.from_col = 4;
  castle.to_row = move->from_row;
  castle.to_col = kingside ? 6 : 2;
  castle.piece = king;
  if (game_is_valid_move(&castle) != MOVE_ERROR_NONE) {
    return false;
  }
  s_conv_row = move->from_row;
  s_conv_kto = castle.to_col;
  s_conv_rfrom = move->from_col;
  s_conv_rto = move->to_col;
  return true;
}

static bool commit_rook_as_castle(void) {
  chess_move_t castle;
  chess_move_t rook;
  if (!s_conv_armed || history_index == 0) {
    return false;
  }
  memset(&castle, 0, sizeof(castle));
  castle.from_row = s_conv_row;
  castle.from_col = 4;
  castle.to_row = s_conv_row;
  castle.to_col = s_conv_kto;
  castle.piece = s_last_valid[s_conv_row][4];
  rook = move_history[history_index - 1];
  s_conv_armed = false;
  s_conv_commit = false;
  if (!game_undo_rewind_last_move()) {
    return false;
  }
  if (game_execute_move(&castle)) {
    return true;
  }
  (void)game_execute_move(&rook);
  return false;
}

static game_hall_pos_class_t classify(const uint8_t occ[64], chess_move_t *legal) {
  /* A legal-looking shape while pieces are scrambled must not commit. */
  if (s_chaos_restore) {
    s_attention = false;
    s_eq_contra_ms = 0;
    s_lift_sq = 255;
    s_castle_paint = false;
    s_ep_paint = false;
    s_cap_paint = false;
    return GAME_HALL_POS_RESTORE;
  }
  bool occ_now = occupancy_equal(s_last_valid, occ);
  bool id_now = identity_clear(s_last_valid, occ);
  s_attention = false;
  s_lift_sq = 255;
  s_conv_commit = false;
  s_castle_paint = false;
  s_ep_paint = false;
  s_cap_paint = false;
  if (occ_now && id_now) {
    s_eq_contra_ms = 0;
    clear_rook_convert();
    return GAME_HALL_POS_EQUAL;
  }
  if (occ_now) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (s_eq_contra_ms == 0U) {
      s_eq_contra_ms = now_ms == 0U ? 1U : now_ms;
    }
    if ((uint32_t)(now_ms - s_eq_contra_ms) < HALL_CONTRA_MS) {
      return GAME_HALL_POS_EQUAL;
    }
    return GAME_HALL_POS_RESTORE;
  }
  s_eq_contra_ms = 0;

  piece_t previous[8][8];
  if (game_physical_undo_copy_previous(previous) &&
      occupancy_equal(previous, occ)) {
    if (!game_physical_undo_enabled()) {
      if (!s_takeback_off_said) {
        s_takeback_off_said = true;
        game_cmd_reply("Takeback is off", false, NULL);
      }
      return GAME_HALL_POS_RESTORE;
    }
    s_takeback_off_said = false;
    if (identity_clear(previous, occ)) {
      s_prev_contra = 0;
      clear_rook_convert();
      fill_cyan(occ);
      return GAME_HALL_POS_PREVIOUS;
    }
    if (s_prev_contra < 255) {
      s_prev_contra++;
    }
    if (s_prev_contra < HALL_CONTRA_SCANS) {
      clear_rook_convert();
      fill_cyan(occ);
      return GAME_HALL_POS_PREVIOUS;
    }
  } else {
    s_prev_contra = 0;
    s_takeback_off_said = false;
  }

  if (try_rook_convert(occ, legal)) {
    return s_conv_commit ? GAME_HALL_POS_ONE_LEGAL : GAME_HALL_POS_CASTLE_PARTIAL;
  }

  hall_diff_t diff[HALL_POS_DIFF_CAP];
  int n = gather_diffs(occ, diff, HALL_POS_DIFF_CAP);
  if (n == 1 && diff[0].occ == 0 && hall_side_owns(diff[0].expect)) {
    /* Capture dest stays occupied, so this is the whole occupancy diff. */
    if (try_landed_capture(diff[0].sq, occ, legal)) {
      return GAME_HALL_POS_ONE_LEGAL;
    }
    if (s_cap_paint) {
      return GAME_HALL_POS_CAPTURE_PARTIAL;
    }
    s_lift_sq = diff[0].sq;
    return GAME_HALL_POS_MOVE_PARTIAL;
  }
  if (n == 4 && try_castle(occ, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  if (n == 3 && try_en_passant(diff, n, occ, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  /* King has left home and the rook has not finished. */
  if (try_castle_partial(occ)) {
    return GAME_HALL_POS_CASTLE_PARTIAL;
  }
  if (n == 2 && try_one_legal(diff, occ, legal)) {
    return GAME_HALL_POS_ONE_LEGAL;
  }
  if (try_ep_partial(occ)) {
    return GAME_HALL_POS_EP_PARTIAL;
  }
  if (try_capture_partial(diff, n, occ)) {
    return GAME_HALL_POS_CAPTURE_PARTIAL;
  }
  return GAME_HALL_POS_RESTORE;
}

static bool special_mode_holds(void) {
  if (game_demo_show_active() || (*game_state_promotion_state()).pending ||
      game_ep_capture_waiting() || game_is_opening_trainer_active() ||
      game_is_puzzle_active()) {
    return true;
  }
  return false;
}

static bool chaos_context_ok(void) {
  if (!game_hall_restore_guide_in_play()) {
    return false;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_PLAYING) {
    return false;
  }
  if (special_mode_holds() || game_task_squares_guard_mode_conflict_active()) {
    return false;
  }
  return true;
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

static void clear_partial_and_lift_state(void) {
  s_hint_n = 0;
  s_lift_sq = 255;
  s_lift_shown = false;
  s_castle_paint = false;
  s_ep_paint = false;
  s_cap_paint = false;
  s_pending_valid = false;
  game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
}

static void exit_chaos_restore(void) {
  s_chaos_restore = false;
  s_chaos_match = false;
  s_attention = false;
  s_pending_valid = false;
  s_guide_drawn = false;
  s_lift_shown = false;
  s_class = GAME_HALL_POS_EQUAL;
  s_class_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
  (void)game_squares_guard_release_hall_hold();
  game_hall_restore_guide_release();
  release_timer();
  game_policy_refresh_idle_board_leds();
  ESP_LOGI(TAG, "Board restored");
}

static bool commit_legal(const chess_move_t *move) {
  if (current_game_state == GAME_STATE_FINISHED || (*game_state_promotion_state()).pending) {
    return false;
  }
  release_timer();
  board_machine_out_t bout = game_board_on_drop_squares(
      move->from_row, move->from_col, move->to_row, move->to_col, true);
  chess_move_t applied = *move;
  if (bout.kind == BM_OUT_MOVE || bout.kind == BM_OUT_NEED_PROMO) {
    return game_board_apply_machine_move(&bout, &applied);
  }
  return false;
}

static void show_own_lift(uint8_t sq) {
  static uint8_t painted = 255;
  if (!s_lift_shown) {
    painted = 255;
  }
  if (s_lift_shown && painted == sq) {
    return;
  }
  painted = sq;
  s_lift_shown = true;
  s_lift_sq = sq;
  /* Hold the previous frame across move generation, then snap the full overlay. */
  game_hall_restore_guide_stop_leds();
  {
    uint8_t row = (uint8_t)(sq / 8);
    uint8_t col = (uint8_t)(sq % 8);
    piece_t piece = s_last_valid[row][col];
    game_board_on_lift_square(row, col);
    game_board_set_piece_lifted(true, row, col, piece);
  }
  game_event_publish_lift_hints(sq);
}

static void show_castle_partial(const uint8_t occ[64]) {
  static uint8_t sig = 0xFF;
  uint8_t now = (uint8_t)((occ[s_castle_kh] ? 1U : 0U) |
                          (occ[s_castle_kt] ? 2U : 0U) |
                          (occ[s_castle_rh] ? 4U : 0U) |
                          (occ[s_castle_rt] ? 8U : 0U));
  if (!s_lift_shown) {
    sig = 0xFF;
  }
  if (s_lift_shown && sig == now) {
    return;
  }
  sig = now;
  s_lift_shown = true;
  /* Both pieces already sit on the castle squares. Leave the guide until
   * the one commit crossfades it. Do not recapture this frame. */
  if (occ[s_castle_kt] != 0 && occ[s_castle_rt] != 0) {
    return;
  }
  game_led_frame_begin(GAME_EVT_HINT_FADE_MS, false);
  game_hall_restore_guide_release();
  if (occ[s_castle_rt] == 0) {
    game_led_frame_breathe((uint8_t)(s_castle_rh / 8), (uint8_t)(s_castle_rh % 8),
                           255, 255, 0, GAME_EVT_BREATHE_SQ, 48, 0, 0);
    game_led_frame_sq((uint8_t)(s_castle_rt / 8), (uint8_t)(s_castle_rt % 8), 0,
                      255, 0);
  }
  if (occ[s_castle_kt] == 0) {
    game_led_frame_breathe((uint8_t)(s_castle_kh / 8), (uint8_t)(s_castle_kh % 8),
                           255, 255, 0, GAME_EVT_BREATHE_SQ, 48, 0, 0);
    if (game_policy_move_hints_castling_blue()) {
      game_led_frame_sq((uint8_t)(s_castle_kt / 8), (uint8_t)(s_castle_kt % 8),
                        0, 0, 255);
    } else {
      game_led_frame_sq((uint8_t)(s_castle_kt / 8), (uint8_t)(s_castle_kt % 8),
                        0, 255, 0);
    }
  }
  game_led_frame_end();
}

static void show_ep_partial(const uint8_t occ[64]) {
  (void)occ;
  if (s_lift_shown) {
    return;
  }
  s_lift_shown = true;
  /* Full frame: only the pawn that must leave the board. */
  game_led_frame_begin(0, false);
  game_hall_restore_guide_stop_leds();
  game_led_frame_sq((uint8_t)(s_ep_victim / 8), (uint8_t)(s_ep_victim % 8), 255,
                    80, 0);
  game_led_frame_end();
}

static void show_capture_partial(void) {
  uint8_t sq;
  static uint8_t painted_victim = 255;
  if (s_cap_from < 64) {
    show_own_lift(s_cap_from);
    return;
  }
  if (!s_lift_shown) {
    painted_victim = 255;
  }
  if (s_lift_shown && painted_victim == s_cap_victim) {
    return;
  }
  painted_victim = s_cap_victim;
  s_lift_shown = true;
  game_led_frame_begin(0, false);
  game_hall_restore_guide_stop_leds();
  game_led_frame_sq((uint8_t)(s_cap_victim / 8), (uint8_t)(s_cap_victim % 8),
                    255, 165, 0);
  for (sq = 0; sq < 64; sq++) {
    if (!legal_capture(sq, s_cap_victim)) {
      continue;
    }
    game_led_frame_sq((uint8_t)(sq / 8), (uint8_t)(sq % 8), 255, 255, 0);
  }
  game_led_frame_end();
}

static void note_class(game_hall_pos_class_t kind) {
  game_hall_pos_class_t prev = s_class;
  if (kind == prev) {
    return;
  }
  if (prev == GAME_HALL_POS_MOVE_PARTIAL && s_resign_ring) {
    s_resign_ring = false;
    s_resign_latched = false;
    game_event_post_led_ctrl(GAME_EVT_CTRL_RESIGN_CANCEL, 0, 0, 0, 0);
  }
  if (prev == GAME_HALL_POS_PREVIOUS) {
    game_event_post_led_ctrl(GAME_EVT_CTRL_TAKEBACK_CLEAR, 0, 0, 0, 0);
  }
  s_class = kind;
  s_class_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
  s_cyan_shown = false;
  s_guide_drawn = false;
  s_lift_shown = false;
  s_pending_valid = false;
  if (prev == GAME_HALL_POS_EP_PARTIAL &&
      (kind == GAME_HALL_POS_ONE_LEGAL || kind == GAME_HALL_POS_RESTORE)) {
    game_led_frame_begin(0, false);
    game_led_frame_end();
  }
  if (kind == GAME_HALL_POS_PREVIOUS) {
    game_cmd_reply(
        "Previous position � takeback if it stays still.", false, NULL);
  } else if (kind == GAME_HALL_POS_RESTORE && !s_attention) {
    game_cmd_reply(
        "Put the pieces back. The last legal position is unchanged.", false,
        NULL);
  }
}

void game_hall_position_enter_chaos_restore(void) {
  if (s_chaos_restore || !chaos_context_ok()) {
    return;
  }
  if ((*game_state_error_recovery_state()).waiting_for_move_correction) {
    game_stop_error_blink();
    game_reset_error_recovery_state();
  }
  s_chaos_restore = true;
  s_chaos_match = false;
  s_attention = false;
  hold_timer();
  note_class(GAME_HALL_POS_RESTORE);
  s_attention = false;
  s_resign_ring = false;
  s_resign_latched = false;
  game_event_post_led_ctrl(GAME_EVT_CTRL_RESIGN_STOP, 0, 0, 0, 0);
  game_event_post_led_ctrl(GAME_EVT_CTRL_TAKEBACK_CLEAR, 0, 0, 0, 0);
  s_lift_shown = false;
  ESP_LOGI(TAG, "Board scrambled � restore guide for last valid position");
  (void)game_hall_restore_guide_show_play();
}

static void paint_for_class(const uint8_t occ[64]) {
  if (s_class == GAME_HALL_POS_MOVE_PARTIAL && s_lift_sq < 64) {
    release_timer();
    if (!s_resign_ring) {
      show_own_lift(s_lift_sq);
    }
    return;
  }
  if (s_class == GAME_HALL_POS_CASTLE_PARTIAL && s_castle_paint) {
    release_timer();
    show_castle_partial(occ);
    return;
  }
  if (s_class == GAME_HALL_POS_EP_PARTIAL && s_ep_paint) {
    release_timer();
    show_ep_partial(occ);
    return;
  }
  if (s_class == GAME_HALL_POS_CAPTURE_PARTIAL && s_cap_paint) {
    release_timer();
    show_capture_partial();
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
}

static void service_unchanged(uint32_t now_ms) {
  uint32_t held = (uint32_t)(now_ms - s_class_since_ms);
  if (!s_chaos_restore && s_class == GAME_HALL_POS_MOVE_PARTIAL && s_lift_sq < 64 &&
      current_game_state != GAME_STATE_FINISHED) {
    piece_t lifted = s_last_valid[s_lift_sq / 8][s_lift_sq % 8];
    bool king = (lifted == PIECE_WHITE_KING || lifted == PIECE_BLACK_KING) &&
                hall_side_owns(lifted);
    if (king) {
      release_timer();
      if (held >= 10000U) {
        if (!s_resign_latched) {
          s_resign_latched = true;
          s_resign_ring = false;
          game_event_post_led_ctrl(GAME_EVT_CTRL_RESIGN_STOP, 0, 0, 0, 0);
          game_resign_from_king_hold(current_player);
        }
        return;
      }
      if (held >= 4000U && !s_resign_ring) {
        s_resign_ring = true;
        game_led_clear_board();
        game_event_post_led_ctrl(GAME_EVT_CTRL_RESIGN_START,
                                 (uint8_t)(s_lift_sq / 8),
                                 (uint8_t)(s_lift_sq % 8),
                                 current_player == PLAYER_WHITE, 0);
        game_cmd_reply("Hold the king to resign.", false, NULL);
      }
    }
  }
  if (!s_chaos_restore && s_class == GAME_HALL_POS_PREVIOUS &&
      held >= HALL_POS_TAKEBACK_MS) {
    uint8_t flash_n = s_cyan_n;
    uint8_t flash_sq[8];
    uint8_t fi;
    if (flash_n > 8) {
      flash_n = 8;
    }
    memcpy(flash_sq, s_cyan_sq, flash_n);
    release_timer();
    game_event_post_led_ctrl(GAME_EVT_CTRL_TAKEBACK_CLEAR, 0, 0, 0, 0);
    {
      bool castle_undo = false;
      if (history_index > 0) {
        move_type_t uk = move_history_kind[history_index - 1];
        castle_undo = (uk == MOVE_TYPE_CASTLE_KING || uk == MOVE_TYPE_CASTLE_QUEEN);
      }
      if (game_undo_rewind_last_move()) {
        s_class = GAME_HALL_POS_EQUAL;
        s_pending_valid = false;
        s_attention = false;
        clear_rook_convert();
        game_cmd_reply("Takeback complete.", false, NULL);
        /* Full idle frame. Do not pulse the empty squares between king and rook. */
        game_event_post_led_ctrl(GAME_EVT_CTRL_TRAIL_CANCEL, 0, 0, 0, 0);
        game_policy_refresh_idle_board_leds();
        if (!castle_undo) {
          for (fi = 0; fi < flash_n; fi++) {
            game_event_post_led_ctrl(
                GAME_EVT_CTRL_PULSE,
                chess_pos_to_led_index(flash_sq[fi] / 8, flash_sq[fi] % 8), 0,
                255, 255);
          }
        }
      } else {
        note_class(GAME_HALL_POS_RESTORE);
        hold_timer();
      }
    }
    return;
  }
  if (!s_chaos_restore && s_class == GAME_HALL_POS_ONE_LEGAL && s_pending_valid &&
      held >= HALL_POS_STABLE_MS) {
    s_in_service = true;
    chess_move_t move = s_pending_move;
    bool convert = s_conv_commit;
    bool arm_rook = false;
    s_pending_valid = false;
    if (!convert) {
      arm_rook = rook_move_can_become_castle(&move);
    }
    if (convert ? !commit_rook_as_castle() : !commit_legal(&move)) {
      s_conv_armed = false;
      s_conv_commit = false;
      note_class(GAME_HALL_POS_RESTORE);
      hold_timer();
    } else {
      s_class = GAME_HALL_POS_EQUAL;
      s_attention = false;
      s_conv_commit = false;
      /* Snapshot cleared convert. A castle must not arm it again. */
      s_conv_armed = (convert || move_is_castle(&move)) ? false : arm_rook;
      s_castle_paint = false;
      game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
      if (!(*game_state_promotion_state()).pending) {
        if (convert) {
          /* Undo clears the captured trail. Fade straight to the next hints. */
          game_event_post_led_ctrl(GAME_EVT_CTRL_TRAIL_CANCEL, 0, 0, 0, 0);
          game_policy_refresh_idle_board_leds();
        } else {
          game_event_post_led_ctrl(GAME_EVT_CTRL_COLLAPSE, 0, 0, 0, 0);
          game_policy_refresh_idle_board_leds();
          game_led_flush();
          game_event_post_board_anim(
              GAME_EVT_ANIM_MOVE_PATH,
              chess_pos_to_led_index(move.from_row, move.from_col),
              chess_pos_to_led_index(move.to_row, move.to_col));
        }
      } else {
        game_event_post_led_ctrl(GAME_EVT_CTRL_TRAIL_CANCEL, 0, 0, 0, 0);
      }
    }
    s_in_service = false;
    return;
  }
  if (!s_chaos_restore && s_class == GAME_HALL_POS_RESTORE &&
      held >= HALL_POS_STABLE_MS) {
    hold_timer();
    if (!chaos_context_ok()) {
      return;
    }
    if ((*game_state_error_recovery_state()).waiting_for_move_correction) {
      game_stop_error_blink();
      game_reset_error_recovery_state();
    }
    if (game_hall_position_mismatch_count() > 4) {
      game_hall_position_enter_chaos_restore();
      return;
    }
    game_hall_restore_guide_sync_play();
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
  const position_t *pos = game_logic_position();
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      s_last_valid[row][col] =
          (pos != NULL) ? (piece_t)pos->board[row][col] : PIECE_EMPTY;
    }
  }
  if (s_chaos_restore) {
    s_chaos_restore = false;
    s_chaos_match = false;
    game_hall_restore_guide_stop_leds();
    (void)game_squares_guard_release_hall_hold();
    release_timer();
  }
  s_digest_valid = false;
  s_class = GAME_HALL_POS_EQUAL;
  s_resign_ring = false;
  s_resign_latched = false;
  s_takeback_off_said = false;
  game_event_post_led_ctrl(GAME_EVT_CTRL_RESIGN_STOP, 0, 0, 0, 0);
  game_event_post_led_ctrl(GAME_EVT_CTRL_TAKEBACK_CLEAR, 0, 0, 0, 0);
  s_guide_drawn = false;
  s_cyan_shown = false;
  s_lift_shown = false;
  s_pending_valid = false;
  s_attention = false;
  clear_rook_convert();
  s_eq_contra_ms = 0;
  s_prev_contra = 0;
  s_hint_n = 0;
  game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
  capture_in_progress = false;
  memset(s_hint, 0, sizeof(s_hint));
  game_snapshot_publish_hall_empty();
}

bool game_hall_positions_equal(const piece_t expected[8][8]) {
  uint8_t occ[64];
  if (expected == NULL || !hall_i2c_squares_readings_usable()) {
    return false;
  }
  squares_get_state(occ);
  return occupancy_equal(expected, occ) && identity_clear(expected, occ);
}

bool game_hall_sensors_match_last_valid(void) {
  return game_hall_positions_equal(s_last_valid);
}

int game_hall_position_mismatch_count(void) {
  uint8_t occ[64];
  int n = 0;
  if (!hall_i2c_squares_readings_usable()) {
    return 64;
  }
  squares_get_state(occ);
  for (uint8_t sq = 0; sq < 64; sq++) {
    if (occ_ok(s_last_valid[sq / 8][sq % 8], occ[sq])) {
      continue;
    }
    n++;
    if (n > 4) {
      return n;
    }
  }
  return n;
}

void game_hall_position_on_matrix_event(const game_cmd_t *cmd) {
  uint8_t row = 0;
  uint8_t col = 0;
  const char *notation = NULL;
  if (cmd == NULL) {
    return;
  }
  if (cmd->kind == GCMD_BOARD_LIFT) {
    notation = game_cmd_from(cmd);
  } else if (cmd->kind == GCMD_BOARD_DROP) {
    notation = game_cmd_to(cmd);
  }
  if (notation != NULL && game_notation_to_coords(notation, &row, &col)) {
    uint8_t sq = (uint8_t)(row * 8 + col);
    if (cmd->kind == GCMD_BOARD_LIFT) {
      hint_push(sq, s_last_valid[row][col], true);
    } else {
      hint_push(sq, PIECE_EMPTY, false);
    }
  }
  game_hall_position_service();
}

void game_hall_position_arm_idle_handover(void) { s_idle_handover = true; }

bool game_hall_position_blocks_idle_leds(void) {
  if (s_idle_handover) {
    s_idle_handover = false;
    return false;
  }
  return s_chaos_restore || s_class != GAME_HALL_POS_EQUAL;
}

bool game_hall_position_in_chaos_restore(void) { return s_chaos_restore; }

static void service_chaos(void) {
  uint8_t occ[64];
  uint8_t digest[8];
  uint32_t now_ms;
  bool digest_changed;

  squares_get_state(occ);
  pack_occ(occ, digest);
  now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  digest_changed = !s_digest_valid || memcmp(digest, s_occ_digest, 8) != 0;
  if (digest_changed) {
    memcpy(s_occ_digest, digest, 8);
    s_digest_valid = true;
  }
  if (s_class != GAME_HALL_POS_RESTORE) {
    note_class(GAME_HALL_POS_RESTORE);
  }
  s_attention = false;
  hold_timer();
  if (!game_hall_positions_equal(s_last_valid)) {
    s_chaos_match = false;
  } else if (digest_changed || !s_chaos_match) {
    s_chaos_match = true;
    s_chaos_match_since_ms = now_ms;
  } else if ((uint32_t)(now_ms - s_chaos_match_since_ms) >= HALL_POS_STABLE_MS) {
    exit_chaos_restore();
    return;
  }
  game_hall_restore_guide_sync_play();
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
  /* First lift or place after the result: drop the wave and open a new game
   * the same way boot does. The clock and white's hints wait for the start
   * position. Occupancy only � a brief identity glitch must not end the wave. */
  if (current_game_state == GAME_STATE_FINISHED) {
    uint8_t end_occ[64];
    if (!hall_i2c_squares_readings_usable()) {
      return;
    }
    squares_get_state(end_occ);
    if (occupancy_equal(s_last_valid, end_occ)) {
      return;
    }
    ESP_LOGI(TAG, "End position changed � opening setup");
    game_start_new_game();
    return;
  }
  if (game_get_move_count() == 0 && !game_hall_restore_guide_in_play()) {
    return;
  }
  if (!hall_i2c_squares_readings_usable()) {
    return;
  }
  if (s_chaos_restore) {
    if (!chaos_context_ok()) {
      s_chaos_restore = false;
      s_chaos_match = false;
      game_hall_restore_guide_stop_leds();
      (void)game_squares_guard_release_hall_hold();
      release_timer();
      return;
    }
    service_chaos();
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
        game_event_post_led_ctrl(GAME_EVT_CTRL_TRAIL_CANCEL, 0, 0, 0, 0);
        if (game_hall_restore_guide_is_active()) {
          game_hall_restore_guide_release();
        }
        clear_partial_and_lift_state();
        game_hall_position_arm_idle_handover();
        game_policy_refresh_idle_board_leds();
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
      game_event_post_led_ctrl(GAME_EVT_CTRL_TRAIL_CANCEL, 0, 0, 0, 0);
      bool guide_was = game_hall_restore_guide_is_active();
      if (guide_was) {
        game_hall_restore_guide_release();
      }
      clear_partial_and_lift_state();
      if (guide_was || prev_class != GAME_HALL_POS_EQUAL) {
        game_hall_position_arm_idle_handover();
        game_policy_refresh_idle_board_leds();
      }
      return;
    }
  }
  paint_for_class(occ);
  if (s_class == GAME_HALL_POS_RESTORE) {
    hold_timer();
  }
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
