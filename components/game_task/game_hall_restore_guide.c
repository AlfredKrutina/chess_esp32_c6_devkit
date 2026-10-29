/**
 * @file game_hall_restore_guide.c
 * @brief Match engine pieces to Hall readings and pick up to 3 restore paths.
 */

#include "game_hall_restore_guide.h"
#include "game_hall_position.h"
#include "game_hall_match.h"
#include "game_squares_guard.h"
#include "game_policy.h"
#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_physical_undo.h"

#include "game_events.h"
#include "game_gesture.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "../squares_task/include/squares_task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

static const char *TAG = "HALL_RESTORE";

#define MAX_PIECES 32
#define MAX_JOBS 32
typedef struct {
  uint8_t home_sq;
  uint8_t phys_sq;
  hall_piece_type_t type;
  hall_piece_color_t color;
} hall_piece_slot_t;

typedef struct {
  uint8_t from_sq;
  uint8_t home_sq;
  hall_piece_type_t type;
  uint8_t path[GAME_EVT_RESTORE_MAX_PATH_LEN];
  uint8_t path_len;
  bool swap_corridor;
} restore_job_t;

static hall_piece_slot_t s_compose_expect[MAX_PIECES];
static hall_piece_slot_t s_compose_phys[MAX_PIECES];
/* match_all scratch; game task only, branches do not nest */
static int s_match_e_idx[MAX_PIECES];
static int s_match_p_idx[MAX_PIECES];
static hall_piece_slot_t s_match_e_slice[MAX_PIECES];
static hall_piece_slot_t s_match_p_slice[MAX_PIECES];
static restore_job_t s_compose_jobs[MAX_JOBS];
static uint8_t s_compose_missing[GAME_EVT_RESTORE_MAX_MISSING];
static uint8_t s_compose_extra[GAME_EVT_RESTORE_MAX_EXTRA];
static game_restore_plan_t s_restore_work_plan;

static bool s_guide_led_active;
static uint8_t s_last_sensor_digest[64];
static volatile bool s_sync_requested;
/** Set while sync() runs so a clear-from-matrix callback cannot re-enter it. */
static bool s_in_sync;

static int build_manhattan_path(uint8_t from, uint8_t to, uint8_t *out, int cap,
                                bool columns_first) {
  if (cap < 1 || from >= 64 || to >= 64) {
    return 0;
  }
  int len = 0;
  out[len++] = from;
  int r = (int)(from / 8);
  int c = (int)(from % 8);
  int tr = (int)(to / 8);
  int tc = (int)(to % 8);
  if (columns_first) {
    while (c != tc && len < cap) {
      c += (tc > c) ? 1 : -1;
      out[len++] = (uint8_t)(r * 8 + c);
    }
    while (r != tr && len < cap) {
      r += (tr > r) ? 1 : -1;
      out[len++] = (uint8_t)(r * 8 + c);
    }
  } else {
    while (r != tr && len < cap) {
      r += (tr > r) ? 1 : -1;
      out[len++] = (uint8_t)(r * 8 + c);
    }
    while (c != tc && len < cap) {
      c += (tc > c) ? 1 : -1;
      out[len++] = (uint8_t)(r * 8 + c);
    }
  }
  return len;
}

static int build_knight_path(uint8_t from, uint8_t to, uint8_t *out, int cap) {
  if (cap < 2 || from >= 64 || to >= 64 || from == to) {
    return 0;
  }
  int r = (int)(from / 8);
  int c = (int)(from % 8);
  int tr = (int)(to / 8);
  int tc = (int)(to % 8);
  int dr = tr - r;
  int dc = tc - c;
  int adr = (dr < 0) ? -dr : dr;
  int adc = (dc < 0) ? -dc : dc;
  out[0] = from;
  if (((adr == 2 && adc == 1) || (adr == 1 && adc == 2)) && cap >= 3) {
    int sr = (dr > 0) ? 1 : ((dr < 0) ? -1 : 0);
    int sc = (dc > 0) ? 1 : ((dc < 0) ? -1 : 0);
    if (adr == 2) {
      out[1] = (uint8_t)((r + 2 * sr) * 8 + c);
    } else {
      out[1] = (uint8_t)(r * 8 + (c + 2 * sc));
    }
    out[2] = to;
    return 3;
  }
  out[1] = to;
  return 2;
}

static int manhattan_steps(uint8_t a, uint8_t b) {
  int ar = (int)(a / 8);
  int ac = (int)(a % 8);
  int br = (int)(b / 8);
  int bc = (int)(b % 8);
  int dr = ar - br;
  int dc = ac - bc;
  if (dr < 0) {
    dr = -dr;
  }
  if (dc < 0) {
    dc = -dc;
  }
  return dr + dc;
}

static bool paths_overlap(const uint8_t *a, uint8_t a_len, const uint8_t *b,
                          uint8_t b_len) {
  for (uint8_t i = 0; i < a_len; i++) {
    for (uint8_t j = 0; j < b_len; j++) {
      if (a[i] == b[j]) {
        return true;
      }
    }
  }
  return false;
}

static void digest_sensors(const uint8_t occ[64], uint8_t out[64]) {
  for (uint8_t sq = 0; sq < 64; sq++) {
    hall_piece_type_t ht = squares_get_hall_piece_type(sq);
    hall_piece_color_t hc = squares_get_hall_piece_color(sq);
    out[sq] = (uint8_t)((occ[sq] ? 1U : 0U) | ((uint8_t)ht << 1) |
                        ((uint8_t)hc << 5));
  }
}

static bool occupancy_empty(const uint8_t occ[64]) {
  for (int i = 0; i < 64; i++) {
    if (occ[i] != 0) {
      return false;
    }
  }
  return true;
}

static bool sensors_unchanged(const uint8_t digest[64]) {
  return memcmp(digest, s_last_sensor_digest, 64) == 0;
}

static bool s_digest_valid;
static uint32_t s_play_gen;
static bool s_opening_done;
static bool s_target_aligned;
static bool s_opening_clock_started;
static uint32_t s_opening_deadline_ms;
static uint8_t s_pending_digest[64];
static uint32_t s_pending_since_ms;
static bool s_pending_valid;
#define GUIDE_STABLE_MS 150U

static bool occupancy_same(const uint8_t a[64], const uint8_t b[64]) {
  for (int i = 0; i < 64; i++) {
    if ((a[i] ^ b[i]) & 1U) {
      return false;
    }
  }
  return true;
}

static void remember_digest(const uint8_t digest[64]) {
  memcpy(s_last_sensor_digest, digest, 64);
  s_digest_valid = true;
}

static bool initial_setup_guide(void) {
  /* Piece colors are the opening guide only. A game that already has moves,
   * and the demo, draw their own lights. Once the pieces have sat correctly,
   * or the player has lifted one to move, those colors stay off. */
  if (game_is_puzzle_setup_active() || game_is_puzzle_active()) {
    return false;
  }
  if (s_opening_done || game_demo_show_active()) {
    return false;
  }
  if (current_game_state == GAME_STATE_IDLE ||
      current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    return true;
  }
  /* Position check is often off, so the game is already ACTIVE before the
   * pieces are placed. No moves yet means this is still the opening. */
  return current_game_state == GAME_STATE_ACTIVE && game_get_move_count() == 0;
}

static bool mode_blocks_guide(void) {
  if (game_task_squares_guard_mode_conflict_active()) {
    return true;
  }
  /* A piece in the hand must not abort the guide. The last piece lifted
   * is exactly that case, and the empty-board lights never got drawn. */
  if (game_physical_undo_is_active() || game_ep_capture_waiting() ||
      castling_state.in_progress || castle_animation_active ||
      (*game_state_promotion_state()).pending) {
    return true;
  }
  if (current_game_state != GAME_STATE_IDLE &&
      current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_WAITING_FOR_BOARD_SETUP &&
      current_game_state != GAME_STATE_FINISHED &&
      current_game_state != GAME_STATE_WAITING_FOR_RETURN) {
    return true;
  }
  return false;
}

static bool square_phys_identity(uint8_t sq, hall_piece_type_t *ht,
                                 hall_piece_color_t *hc) {
  *ht = squares_get_hall_piece_type(sq);
  *hc = squares_get_hall_piece_color(sq);
  return *ht != HALL_PIECE_TYPE_NONE && *hc != HALL_PIECE_COLOR_NONE;
}

/* Neighbor crosstalk on crowded home ranks can shift Hall energy one band. */
static bool hall_home_band_match(hall_piece_type_t home_t,
                                 hall_piece_type_t seen_t) {
  if (home_t == seen_t) {
    return true;
  }
  if (home_t == HALL_PIECE_TYPE_PAWN && seen_t == HALL_PIECE_TYPE_KNIGHT) {
    return true;
  }
  if (home_t == HALL_PIECE_TYPE_KNIGHT &&
      (seen_t == HALL_PIECE_TYPE_PAWN || seen_t == HALL_PIECE_TYPE_BISHOP)) {
    return true;
  }
  if (home_t == HALL_PIECE_TYPE_BISHOP &&
      (seen_t == HALL_PIECE_TYPE_KNIGHT || seen_t == HALL_PIECE_TYPE_ROOK)) {
    return true;
  }
  return false;
}

/* Home light goes out when the expected piece is seated on its home square.
 * Unnamed readings count while occ is set (matches collect_board). King and
 * queen share one band; same-color neighbor-band tolerance on home ranks. */
static bool sensor_is_this_piece(uint8_t sq, hall_piece_type_t want_t,
                                 hall_piece_color_t want_c,
                                 const uint8_t occ[64]) {
  hall_piece_type_t t;
  hall_piece_color_t c;
  bool named = square_phys_identity(sq, &t, &c);
  if (occ[sq] != 0 && !named) {
    return true;
  }
  if (!named || c != want_c) {
    return false;
  }
  if (t == want_t) {
    return true;
  }
  bool want_royal = want_t == HALL_PIECE_TYPE_KING ||
                    want_t == HALL_PIECE_TYPE_QUEEN;
  bool seen_royal = t == HALL_PIECE_TYPE_KING || t == HALL_PIECE_TYPE_QUEEN;
  if (want_royal && seen_royal) {
    return true;
  }
  return hall_home_band_match(want_t, t);
}

static bool collect_board(hall_piece_slot_t *expect, int *expect_n,
                          hall_piece_slot_t *phys, int *phys_n,
                          const uint8_t occ[64]) {
  *expect_n = 0;
  *phys_n = 0;
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t p = game_get_piece((int)(sq / 8), (int)(sq % 8));
    if (p != PIECE_EMPTY) {
      hall_piece_type_t t;
      hall_piece_color_t c;
      if (!game_hall_engine_to_sensor(p, &t, &c)) {
        continue;
      }
      if (*expect_n >= MAX_PIECES) {
        return false;
      }
      expect[*expect_n].home_sq = sq;
      expect[*expect_n].phys_sq = 255;
      expect[*expect_n].type = t;
      expect[*expect_n].color = c;
      (*expect_n)++;
    }
    if (occ[sq] != 0) {
      hall_piece_type_t t;
      hall_piece_color_t c;
      bool named = square_phys_identity(sq, &t, &c);
      piece_t home = game_get_piece((int)(sq / 8), (int)(sq % 8));
      hall_piece_type_t home_t = HALL_PIECE_TYPE_NONE;
      hall_piece_color_t home_c = HALL_PIECE_COLOR_NONE;
      bool has_home = home != PIECE_EMPTY &&
                      game_hall_engine_to_sensor(home, &home_t, &home_c);
      if (!named) {
        if (!has_home) {
          t = HALL_PIECE_TYPE_NONE;
          c = HALL_PIECE_COLOR_NONE;
        } else {
          t = home_t;
          c = home_c;
        }
      } else if (has_home && c == home_c && hall_home_band_match(home_t, t)) {
        t = home_t;
      }
      if (*phys_n >= MAX_PIECES) {
        return false;
      }
      phys[*phys_n].home_sq = 255;
      phys[*phys_n].phys_sq = sq;
      phys[*phys_n].type = t;
      phys[*phys_n].color = c;
      (*phys_n)++;
    }
  }
  return true;
}

static void match_group(const hall_piece_slot_t *expect, int e_count,
                        const hall_piece_slot_t *phys, int p_count,
                        restore_job_t *jobs, int *job_n,
                        uint8_t missing[GAME_EVT_RESTORE_MAX_MISSING],
                        uint8_t *missing_n, uint8_t extra[GAME_EVT_RESTORE_MAX_EXTRA],
                        uint8_t *extra_n) {
  if (e_count == 0) {
    for (int i = 0; i < p_count; i++) {
      if (*extra_n < GAME_EVT_RESTORE_MAX_EXTRA) {
        extra[(*extra_n)++] = phys[i].phys_sq;
      }
    }
    return;
  }
  if (p_count == 0) {
    for (int i = 0; i < e_count; i++) {
      if (*missing_n < GAME_EVT_RESTORE_MAX_MISSING) {
        missing[(*missing_n)++] = expect[i].home_sq;
      }
    }
    return;
  }

  /* Repeated shortest pair. Identical pieces have no left/right identity,
   * and a full search of eight pawns can stall the game task. */
  bool expect_used[MAX_PIECES] = {false};
  bool phys_used[MAX_PIECES] = {false};
  int pair_n = (e_count < p_count) ? e_count : p_count;
  for (int n = 0; n < pair_n; n++) {
    int best_e = -1;
    int best_p = -1;
    int best_d = 1 << 30;
    for (int ei = 0; ei < e_count; ei++) {
      if (expect_used[ei]) {
        continue;
      }
      for (int pi = 0; pi < p_count; pi++) {
        if (phys_used[pi]) {
          continue;
        }
        int d = manhattan_steps(phys[pi].phys_sq, expect[ei].home_sq);
        if (d < best_d) {
          best_d = d;
          best_e = ei;
          best_p = pi;
        }
      }
    }
    if (best_e < 0 || best_p < 0) {
      break;
    }
    expect_used[best_e] = true;
    phys_used[best_p] = true;
    uint8_t from = phys[best_p].phys_sq;
    uint8_t home = expect[best_e].home_sq;
    if (from == home || *job_n >= MAX_JOBS) {
      continue;
    }
    jobs[*job_n].from_sq = from;
    jobs[*job_n].home_sq = home;
    jobs[*job_n].type = expect[best_e].type;
    if (expect[best_e].type == HALL_PIECE_TYPE_KNIGHT) {
      jobs[*job_n].path_len = (uint8_t)build_knight_path(
          from, home, jobs[*job_n].path, GAME_EVT_RESTORE_MAX_PATH_LEN);
    } else {
      jobs[*job_n].path_len = (uint8_t)build_manhattan_path(
          from, home, jobs[*job_n].path, GAME_EVT_RESTORE_MAX_PATH_LEN, false);
    }
    jobs[*job_n].swap_corridor = false;
    (*job_n)++;
  }

  for (int ei = 0; ei < e_count; ei++) {
    if (!expect_used[ei] && *missing_n < GAME_EVT_RESTORE_MAX_MISSING) {
      missing[(*missing_n)++] = expect[ei].home_sq;
    }
  }
  for (int pi = 0; pi < p_count; pi++) {
    if (!phys_used[pi] && *extra_n < GAME_EVT_RESTORE_MAX_EXTRA) {
      extra[(*extra_n)++] = phys[pi].phys_sq;
    }
  }
}

/* Same square in missing[] and extra[]: piece on its home, crosstalk mis-band. */
static void cancel_home_crosstalk_pairs(
    uint8_t missing[GAME_EVT_RESTORE_MAX_MISSING], uint8_t *missing_n,
    uint8_t extra[GAME_EVT_RESTORE_MAX_EXTRA], uint8_t *extra_n) {
  for (uint8_t mi = 0; mi < *missing_n;) {
    uint8_t sq = missing[mi];
    bool removed = false;
    for (uint8_t xi = 0; xi < *extra_n; xi++) {
      if (extra[xi] != sq) {
        continue;
      }
      piece_t p = game_get_piece((int)(sq / 8), (int)(sq % 8));
      hall_piece_type_t et;
      hall_piece_color_t ec;
      if (p == PIECE_EMPTY || !game_hall_engine_to_sensor(p, &et, &ec)) {
        break;
      }
      hall_piece_color_t pc = squares_get_hall_piece_color(sq);
      if (pc != HALL_PIECE_COLOR_NONE && pc != ec) {
        break;
      }
      for (uint8_t j = mi + 1; j < *missing_n; j++) {
        missing[j - 1] = missing[j];
      }
      (*missing_n)--;
      for (uint8_t j = xi + 1; j < *extra_n; j++) {
        extra[j - 1] = extra[j];
      }
      (*extra_n)--;
      removed = true;
      break;
    }
    if (!removed) {
      mi++;
    }
  }
}

static void match_all(const hall_piece_slot_t *expect, int expect_n,
                      const hall_piece_slot_t *phys, int phys_n,
                      restore_job_t *jobs, int *job_n,
                      uint8_t missing[GAME_EVT_RESTORE_MAX_MISSING],
                      uint8_t *missing_n, uint8_t extra[GAME_EVT_RESTORE_MAX_EXTRA],
                      uint8_t *extra_n) {
  *job_n = 0;
  *missing_n = 0;
  *extra_n = 0;

  for (int t = HALL_PIECE_TYPE_PAWN; t <= HALL_PIECE_TYPE_KING; t++) {
    if (t == HALL_PIECE_TYPE_QUEEN || t == HALL_PIECE_TYPE_KING) {
      continue;
    }
    for (int c = HALL_PIECE_COLOR_WHITE; c <= HALL_PIECE_COLOR_BLACK; c++) {
      int ec = 0;
      int pc = 0;
      for (int i = 0; i < expect_n; i++) {
        if (expect[i].type == (hall_piece_type_t)t &&
            expect[i].color == (hall_piece_color_t)c) {
          s_match_e_idx[ec++] = i;
        }
      }
      for (int i = 0; i < phys_n; i++) {
        if (phys[i].type == (hall_piece_type_t)t &&
            phys[i].color == (hall_piece_color_t)c) {
          s_match_p_idx[pc++] = i;
        }
      }
      for (int i = 0; i < ec; i++) {
        s_match_e_slice[i] = expect[s_match_e_idx[i]];
      }
      for (int i = 0; i < pc; i++) {
        s_match_p_slice[i] = phys[s_match_p_idx[i]];
      }
      match_group(s_match_e_slice, ec, s_match_p_slice, pc, jobs, job_n,
                  missing, missing_n, extra, extra_n);
    }
  }

  /* Queen and king energies overlap. A royal piece already on a royal home
   * of the same color belongs to that home, whichever of the two the sensor
   * names. */
  for (int c = HALL_PIECE_COLOR_WHITE; c <= HALL_PIECE_COLOR_BLACK; c++) {
    int ec = 0;
    int pc = 0;
    for (int i = 0; i < expect_n; i++) {
      if (expect[i].color == (hall_piece_color_t)c &&
          (expect[i].type == HALL_PIECE_TYPE_QUEEN ||
           expect[i].type == HALL_PIECE_TYPE_KING)) {
        s_match_e_slice[ec++] = expect[i];
      }
    }
    for (int i = 0; i < phys_n; i++) {
      if (phys[i].color == (hall_piece_color_t)c &&
          (phys[i].type == HALL_PIECE_TYPE_QUEEN ||
           phys[i].type == HALL_PIECE_TYPE_KING)) {
        s_match_p_slice[pc++] = phys[i];
      }
    }
    match_group(s_match_e_slice, ec, s_match_p_slice, pc, jobs, job_n,
                missing, missing_n, extra, extra_n);
  }

  cancel_home_crosstalk_pairs(missing, missing_n, extra, extra_n);

  for (int i = 0; i < phys_n; i++) {
    if (phys[i].type != HALL_PIECE_TYPE_NONE ||
        phys[i].color != HALL_PIECE_COLOR_NONE) {
      continue;
    }
    if (*extra_n < GAME_EVT_RESTORE_MAX_EXTRA) {
      extra[(*extra_n)++] = phys[i].phys_sq;
    }
  }
}

static bool board_fully_aligned(int job_n, uint8_t missing_n, uint8_t extra_n) {
  return job_n == 0 && missing_n == 0 && extra_n == 0;
}

static bool set_paths_conflict(const int *idx, int count,
                               const restore_job_t *jobs) {
  for (int i = 0; i < count; i++) {
    for (int j = i + 1; j < count; j++) {
      if (paths_overlap(jobs[idx[i]].path, jobs[idx[i]].path_len,
                        jobs[idx[j]].path, jobs[idx[j]].path_len)) {
        return true;
      }
    }
  }
  return false;
}

/** Two misplaced pieces on each other's homes → one pulsing corridor between them. */
static void merge_swap_pairs(restore_job_t *jobs, int *job_n) {
  for (int i = 0; i < *job_n; i++) {
    if (jobs[i].swap_corridor) {
      continue;
    }
    for (int j = i + 1; j < *job_n; j++) {
      if (jobs[j].swap_corridor) {
        continue;
      }
      if (jobs[i].from_sq == jobs[j].home_sq &&
          jobs[j].from_sq == jobs[i].home_sq) {
        uint8_t end_a = jobs[i].from_sq;
        uint8_t end_b = jobs[j].from_sq;
        bool knight = jobs[i].type == HALL_PIECE_TYPE_KNIGHT ||
                      jobs[j].type == HALL_PIECE_TYPE_KNIGHT;
        jobs[i].home_sq = end_b;
        jobs[i].type = knight ? HALL_PIECE_TYPE_KNIGHT : jobs[i].type;
        if (knight) {
          jobs[i].path_len = (uint8_t)build_knight_path(
              end_a, end_b, jobs[i].path, GAME_EVT_RESTORE_MAX_PATH_LEN);
          jobs[i].swap_corridor = false;
        } else {
          jobs[i].path_len = (uint8_t)build_manhattan_path(
              end_a, end_b, jobs[i].path, GAME_EVT_RESTORE_MAX_PATH_LEN, false);
          jobs[i].swap_corridor = true;
        }
        if (j < *job_n - 1) {
          jobs[j] = jobs[*job_n - 1];
        }
        (*job_n)--;
        j--;
      }
    }
  }
}

static bool path_crosses_missing_home(const restore_job_t *job,
                                      const uint8_t *missing,
                                      uint8_t missing_n) {
  for (uint8_t i = 0; i < job->path_len; i++) {
    for (uint8_t m = 0; m < missing_n; m++) {
      if (job->path[i] == missing[m]) {
        return true;
      }
    }
  }
  return false;
}

/** Row-then-column, or column-then-row when the first route covers an empty home. */
static void prefer_path_clear_of_missing(restore_job_t *job,
                                         const uint8_t *missing,
                                         uint8_t missing_n) {
  if (job->path_len == 0 || job->type == HALL_PIECE_TYPE_KNIGHT) {
    if (job->type == HALL_PIECE_TYPE_KNIGHT) {
      job->path_len = (uint8_t)build_knight_path(
          job->from_sq, job->home_sq, job->path, GAME_EVT_RESTORE_MAX_PATH_LEN);
    }
    return;
  }
  uint8_t from = job->path[0];
  uint8_t to = job->path[job->path_len - 1];
  if (from >= 64 || to >= 64) {
    job->path_len = 0;
    return;
  }
  uint8_t row_first[GAME_EVT_RESTORE_MAX_PATH_LEN];
  uint8_t col_first[GAME_EVT_RESTORE_MAX_PATH_LEN];
  int row_n = build_manhattan_path(from, to, row_first,
                                   GAME_EVT_RESTORE_MAX_PATH_LEN, false);
  int col_n = build_manhattan_path(from, to, col_first,
                                   GAME_EVT_RESTORE_MAX_PATH_LEN, true);
  restore_job_t probe = *job;
  if (row_n > 0) {
    probe.path_len = (uint8_t)row_n;
    memcpy(probe.path, row_first, (size_t)row_n);
    if (!path_crosses_missing_home(&probe, missing, missing_n)) {
      job->path_len = probe.path_len;
      memcpy(job->path, probe.path, (size_t)row_n);
      return;
    }
  }
  if (col_n > 0) {
    probe.path_len = (uint8_t)col_n;
    memcpy(probe.path, col_first, (size_t)col_n);
    if (!path_crosses_missing_home(&probe, missing, missing_n)) {
      job->path_len = probe.path_len;
      memcpy(job->path, probe.path, (size_t)col_n);
      return;
    }
  }
  job->path_len = 0;
}

static void pick_display_paths(restore_job_t *jobs, int job_n,
                               const uint8_t *missing, uint8_t missing_n,
                               game_restore_plan_t *plan) {
  int candidates[MAX_JOBS];
  int cand_n = 0;
  for (int i = 0; i < job_n; i++) {
    prefer_path_clear_of_missing(&jobs[i], missing, missing_n);
    if (jobs[i].path_len > 0 &&
        (jobs[i].type == HALL_PIECE_TYPE_KNIGHT ||
         !path_crosses_missing_home(&jobs[i], missing, missing_n))) {
      candidates[cand_n++] = i;
    }
  }

  int best_jobs[GAME_EVT_RESTORE_MAX_PATHS];
  int best_count = 0;
  static int s_path_rotate;
  int start = (cand_n > 0) ? (s_path_rotate % cand_n) : 0;
  if (cand_n > GAME_EVT_RESTORE_MAX_PATHS) {
    s_path_rotate++;
  }

  for (int step = 0; step < cand_n && best_count < GAME_EVT_RESTORE_MAX_PATHS;
       step++) {
    int job_i = candidates[(start + step) % cand_n];
    int trial[GAME_EVT_RESTORE_MAX_PATHS];
    for (int k = 0; k < best_count; k++) {
      trial[k] = best_jobs[k];
    }
    trial[best_count] = job_i;
    if (!set_paths_conflict(trial, best_count + 1, jobs)) {
      best_jobs[best_count++] = job_i;
    }
  }

  plan->path_count = (uint8_t)best_count;
  for (int i = 0; i < best_count; i++) {
    const restore_job_t *job = &jobs[best_jobs[i]];
    plan->paths[i].len = job->path_len;
    plan->paths[i].pulse_corridor = job->swap_corridor;
    memcpy(plan->paths[i].squares, job->path, job->path_len);
  }
}

static bool engine_mark(uint8_t sq, hall_piece_type_t *t, hall_piece_color_t *c) {
  if (sq >= 64) {
    return false;
  }
  return game_hall_engine_to_sensor(
      game_get_piece((int)(sq / 8), (int)(sq % 8)), t, c);
}

static void push_setup_mark(game_restore_plan_t *plan, uint8_t sq,
                            uint8_t type, uint8_t color, uint8_t role) {
  if (sq >= 64 || plan->mark_count >= GAME_EVT_RESTORE_MAX_MARKS) {
    return;
  }
  for (uint8_t i = 0; i < plan->mark_count; i++) {
    if (plan->marks[i].square == sq) {
      return;
    }
  }
  game_restore_mark_t *mark = &plan->marks[plan->mark_count++];
  mark->square = sq;
  mark->type = type;
  mark->color = color;
  mark->role = role;
}

static void fill_setup_marks(const restore_job_t *jobs, int job_n,
                             const uint8_t *missing, uint8_t missing_n,
                             const uint8_t *extra, uint8_t extra_n,
                             const uint8_t occ[64],
                             game_restore_plan_t *plan) {
  plan->mark_count = 0;
  for (int i = 0; i < job_n; i++) {
    hall_piece_type_t t = jobs[i].type;
    hall_piece_color_t c = HALL_PIECE_COLOR_WHITE;
    hall_piece_type_t et;
    hall_piece_color_t ec;
    if (engine_mark(jobs[i].home_sq, &et, &ec)) {
      t = et;
      c = ec;
    }
    if (sensor_is_this_piece(jobs[i].home_sq, t, c, occ)) {
      continue;
    }
    push_setup_mark(plan, jobs[i].home_sq, (uint8_t)t, (uint8_t)c,
                    GAME_EVT_RESTORE_MARK_HOME);
  }
  for (uint8_t i = 0; i < missing_n; i++) {
    hall_piece_type_t t = HALL_PIECE_TYPE_PAWN;
    hall_piece_color_t c = HALL_PIECE_COLOR_WHITE;
    if (!engine_mark(missing[i], &t, &c) ||
        sensor_is_this_piece(missing[i], t, c, occ)) {
      continue;
    }
    push_setup_mark(plan, missing[i], (uint8_t)t, (uint8_t)c,
                    GAME_EVT_RESTORE_MARK_HOME);
  }
  for (uint8_t i = 0; i < extra_n; i++) {
    if (occ[extra[i]] != 0) {
      continue;
    }
    push_setup_mark(plan, extra[i], 0, 0, GAME_EVT_RESTORE_MARK_EXTRA);
  }
  /* Ranks 3–6 are empty in the start position. A piece on an engine-empty
   * square there is lit in its own color, including king and queen.
   * This does not wait for the setup state: a new game often goes
   * straight to active and that gate never ran. */
  for (uint8_t sq = 0; sq < 64; sq++) {
    uint8_t rank = (uint8_t)(sq / 8U);
    if (rank < 2U || rank > 5U || occ[sq] == 0) {
      continue;
    }
    if (game_get_piece((int)(sq / 8), (int)(sq % 8)) != PIECE_EMPTY) {
      continue;
    }
    hall_piece_type_t t = squares_get_hall_piece_type(sq);
    hall_piece_color_t c = squares_get_hall_piece_color(sq);
    if (c == HALL_PIECE_COLOR_NONE || t == HALL_PIECE_TYPE_NONE) {
      continue;
    }
    push_setup_mark(plan, sq, (uint8_t)t, (uint8_t)c, GAME_EVT_RESTORE_MARK_HOME);
  }
}

void game_hall_restore_guide_request_sync(void) { s_sync_requested = true; }

void game_hall_restore_guide_stop_leds(void);

static void opening_clock_on_play(void) {
  if (s_opening_clock_started) {
    return;
  }
  if (!s_opening_done || game_get_move_count() != 0) {
    return;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_PLAYING) {
    return;
  }
  s_opening_clock_started = true;
  if (game_is_timer_active()) {
    (void)game_start_timer_move(current_player == PLAYER_WHITE);
    ESP_LOGI(TAG, "Clock started after opening setup");
  }
}

void game_hall_opening_clock_on_play(void) { opening_clock_on_play(); }

void game_hall_restore_guide_end_for_play(void) {
  s_opening_done = true;
  game_hall_restore_guide_stop_leds();
  opening_clock_on_play();
}

bool game_hall_restore_guide_in_play(void) { return s_opening_done; }

void game_hall_restore_guide_force_resync(void) {
  s_opening_done = false;
  s_opening_clock_started = false;
  s_digest_valid = false;
  s_sync_requested = true;
  s_target_aligned = false;
}

static bool apply_opening_homes(void) {
  game_restore_plan_t *plan = &s_restore_work_plan;

  memset(plan, 0, sizeof(*plan));
  plan->active = true;
  for (uint8_t sq = 0; sq < 64; sq++) {
    hall_piece_type_t type = HALL_PIECE_TYPE_NONE;
    hall_piece_color_t color = HALL_PIECE_COLOR_NONE;
    if (!engine_mark(sq, &type, &color)) {
      continue;
    }
    push_setup_mark(plan, sq, (uint8_t)type, (uint8_t)color,
                    GAME_EVT_RESTORE_MARK_HOME);
  }
  if (plan->mark_count == 0) {
    return false;
  }
  game_event_post_restore(plan, GAME_EVT_RESTORE_OP_APPLY);
  s_guide_led_active = true;
  s_opening_deadline_ms = (uint32_t)(esp_timer_get_time() / 1000) + 10000U;
  ESP_LOGI(TAG, "[STAGING] opening setup guide, %u homes, 10s",
           (unsigned)plan->mark_count);
  return true;
}

bool game_hall_restore_guide_show_opening(void) {
  uint8_t occ[64];

  if (!hall_i2c_squares_readings_usable()) {
    return false;
  }
  squares_get_state(occ);
  if (!occupancy_empty(occ)) {
    return false;
  }
  s_digest_valid = false;
  return apply_opening_homes();
}

bool game_hall_restore_guide_take_sync_request(void) {
  if (!s_sync_requested) {
    return false;
  }
  s_sync_requested = false;
  return true;
}

void game_hall_restore_guide_note_stable_squares(const uint8_t occ[64]) {
  if (s_opening_done || occ == NULL) {
    return;
  }
  uint8_t digest[64];
  digest_sensors(occ, digest);
  if (memcmp(digest, s_last_sensor_digest, 64) != 0) {
    game_hall_restore_guide_request_sync();
  }
}

static void ensure_squares_pause(void) {
  if (!game_is_squares_guard_active()) {
    game_squares_guard_enter_pause_from_hall_guide();
  }
}

bool game_hall_restore_guide_is_active(void) {
  return s_guide_led_active;
}

void game_hall_restore_guide_stop_leds(void) {
  s_guide_led_active = false;
  game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_CLEAR);
}

void game_hall_restore_guide_release(void) {
  s_guide_led_active = false;
  game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_CANCEL);
}

static bool guide_digest_settled(const uint8_t digest[64], uint32_t now_ms) {
  if (!s_pending_valid || !occupancy_same(digest, s_pending_digest)) {
    memcpy(s_pending_digest, digest, 64);
    s_pending_valid = true;
    s_pending_since_ms = now_ms;
    return false;
  }
  memcpy(s_pending_digest, digest, 64);
  return (uint32_t)(now_ms - s_pending_since_ms) >= GUIDE_STABLE_MS;
}

static bool play_sensors_blocked(void) {
  if (game_demo_show_active() || !hall_i2c_squares_readings_usable() ||
      mode_blocks_guide()) {
    return true;
  }
  return false;
}

/** Jobs and marks against board[][]. aligned is true when nothing is left to move. */
static bool compose_restore_plan(const uint8_t occ[64],
                                 game_restore_plan_t *plan, bool *aligned) {
  hall_piece_slot_t *expect = s_compose_expect;
  hall_piece_slot_t *phys = s_compose_phys;
  restore_job_t *jobs = s_compose_jobs;
  uint8_t *missing = s_compose_missing;
  uint8_t *extra = s_compose_extra;
  int expect_n = 0;
  int phys_n = 0;
  int job_n = 0;
  uint8_t missing_n = 0;
  uint8_t extra_n = 0;

  *aligned = false;
  memset(plan, 0, sizeof(*plan));
  if (!collect_board(expect, &expect_n, phys, &phys_n, occ)) {
    return false;
  }
  match_all(expect, expect_n, phys, phys_n, jobs, &job_n, missing, &missing_n,
            extra, &extra_n);
  if (board_fully_aligned(job_n, missing_n, extra_n)) {
    *aligned = true;
    return true;
  }
  merge_swap_pairs(jobs, &job_n);
  pick_display_paths(jobs, job_n, missing, missing_n, plan);
  plan->missing_count = missing_n;
  memcpy(plan->missing_homes, missing, missing_n);
  plan->extra_count = extra_n;
  memcpy(plan->extra_squares, extra, extra_n);
  fill_setup_marks(jobs, job_n, missing, missing_n, extra, extra_n, occ, plan);
  return true;
}

/** Opening pieces already match the engine. Drop the setup guide and paint idle LEDs. */
static void handover_opening_aligned(const uint8_t digest[64]) {
  s_opening_done = true;
  (void)game_squares_guard_release_hall_hold();
  game_hall_restore_guide_release();
  game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
  opening_clock_on_play();
  remember_digest(digest);
  game_hall_position_arm_idle_handover();
  game_policy_refresh_idle_board_leds();
}

void game_hall_restore_guide_sync(void) {
  if (s_opening_done || game_is_puzzle_setup_active() || game_is_puzzle_active()) {
    s_sync_requested = false;
    return;
  }
  if (game_demo_show_active()) {
    s_guide_led_active = false;
    game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_CANCEL);
    return;
  }
#if HALL_DEBUG_LIGHT_WHERE_PIECE
  game_hall_restore_guide_stop_leds();
  return;
#endif
  if (!hall_i2c_squares_readings_usable()) {
    return;
  }
  uint8_t occ[64];
  uint8_t digest[64];
  game_restore_plan_t *plan = &s_restore_work_plan;
  bool aligned = false;

  if (s_in_sync) {
    return;
  }
  s_in_sync = true;

  if (!game_policy_squares_guard_enabled() || !game_policy_squares_guard_led_enabled()) {
    game_hall_restore_guide_stop_leds();
    goto out;
  }
  if (!initial_setup_guide()) {
    game_hall_restore_guide_stop_leds();
    goto out;
  }
  if (mode_blocks_guide()) {
    game_hall_restore_guide_stop_leds();
    goto out;
  }

  squares_get_state(occ);
  digest_sensors(occ, digest);
  /* New Game clears the digest, so the first sync always fails the 150 ms
   * settle. Pieces already on their homes must light movable LEDs now. */
  if (!s_guide_led_active &&
      compose_restore_plan(occ, plan, &aligned) && aligned) {
    handover_opening_aligned(digest);
    goto out;
  }
  if (!guide_digest_settled(digest, (uint32_t)(esp_timer_get_time() / 1000))) {
    goto out;
  }
  if (s_opening_deadline_ms != 0) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (!occupancy_empty(occ)) {
      s_opening_deadline_ms = 0;
    } else if ((int32_t)(now_ms - s_opening_deadline_ms) >= 0) {
      s_opening_deadline_ms = 0;
      game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_FADE_OUT);
      s_guide_led_active = false;
      remember_digest(digest);
      ESP_LOGI(TAG, "[STAGING] opening setup guide faded, board still empty");
      goto out;
    } else {
      remember_digest(digest);
      goto out;
    }
  }
  if (s_digest_valid && sensors_unchanged(digest)) {
    goto out;
  }

  /* Last piece off the board: light every engine home and hold 10 s.
   * This used to fade immediately, and only the new-game gesture on an
   * already empty board ever started the timer. */
  if (occupancy_empty(occ)) {
    if (apply_opening_homes()) {
      remember_digest(digest);
      goto out;
    }
    if (s_guide_led_active) {
      game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_FADE_OUT);
      s_guide_led_active = false;
    }
    remember_digest(digest);
    goto out;
  }

  if (!compose_restore_plan(occ, plan, &aligned)) {
    goto out;
  }
  if (aligned) {
    handover_opening_aligned(digest);
    goto out;
  }
  if (plan->mark_count == 0) {
    game_hall_restore_guide_stop_leds();
    remember_digest(digest);
    goto out;
  }

  plan->active = true;
  ensure_squares_pause();
  game_event_post_restore(plan, GAME_EVT_RESTORE_OP_APPLY);
  s_guide_led_active = true;
  remember_digest(digest);

  ESP_LOGI(TAG, "[STAGING] restore guide: paths=%u missing=%u extra=%u",
           (unsigned)plan->path_count, (unsigned)plan->missing_count,
           (unsigned)plan->extra_count);
out:
  s_in_sync = false;
}

static bool apply_composed_plan(const uint8_t occ[64]) {
  game_restore_plan_t *plan = &s_restore_work_plan;
  bool aligned = false;

  if (!compose_restore_plan(occ, plan, &aligned)) {
    return false;
  }
  if (aligned) {
    (void)game_squares_guard_release_hall_hold();
    game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
    game_hall_restore_guide_release();
    game_hall_position_arm_idle_handover();
    game_policy_refresh_idle_board_leds();
    return true;
  }
  if (plan->mark_count == 0 && plan->path_count == 0) {
    s_guide_led_active = false;
    game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_BLANK);
    return true;
  }
  plan->active = true;
  if (s_opening_done &&
      (*game_state_error_recovery_state()).waiting_for_move_correction) {
    game_stop_error_blink();
    game_reset_error_recovery_state();
  }
  game_event_post_restore(plan, GAME_EVT_RESTORE_OP_APPLY);
  s_guide_led_active = true;
  return true;
}

bool game_hall_restore_guide_show_target(void) {
  uint8_t occ[64];
  uint8_t digest[64];
  game_restore_plan_t *plan = &s_restore_work_plan;
  bool aligned = false;
  uint32_t now_ms;

  if (s_in_sync || !hall_i2c_squares_readings_usable() || mode_blocks_guide()) {
    return s_target_aligned;
  }
  squares_get_state(occ);
  digest_sensors(occ, digest);
  now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  if (!guide_digest_settled(digest, now_ms)) {
    return s_target_aligned;
  }
  if (s_digest_valid && sensors_unchanged(digest)) {
    return s_target_aligned;
  }
  s_in_sync = true;
  if (!compose_restore_plan(occ, plan, &aligned)) {
    s_in_sync = false;
    return s_target_aligned;
  }
  remember_digest(digest);
  if (aligned) {
    if (s_guide_led_active) {
      game_hall_restore_guide_release();
    }
    s_target_aligned = true;
    s_in_sync = false;
    return true;
  }
  plan->active = true;
  game_event_post_restore(plan, GAME_EVT_RESTORE_OP_APPLY);
  s_guide_led_active = true;
  s_target_aligned = false;
  s_in_sync = false;
  return false;
}

bool game_hall_restore_guide_show_play(void) {
  uint8_t occ[64];

  if (s_in_sync || !s_opening_done || play_sensors_blocked()) {
    return false;
  }
  s_in_sync = true;
  squares_get_state(occ);
  if (!apply_composed_plan(occ)) {
    s_in_sync = false;
    return false;
  }
  s_in_sync = false;
  return true;
}

void game_hall_restore_guide_sync_play(void) {
  uint8_t occ[64];
  uint8_t digest[64];
  uint32_t now_ms;

  if (s_in_sync || !s_opening_done || play_sensors_blocked()) {
    return;
  }
#if HALL_DEBUG_LIGHT_WHERE_PIECE
  return;
#endif
  squares_get_state(occ);
  digest_sensors(occ, digest);
  now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  if (!guide_digest_settled(digest, now_ms)) {
    return;
  }
  if (s_digest_valid && sensors_unchanged(digest) &&
      s_play_gen == game_led_board_picture_gen()) {
    return;
  }
  s_in_sync = true;
  if (!apply_composed_plan(occ)) {
    s_in_sync = false;
    return;
  }
  remember_digest(digest);
  s_play_gen = game_led_board_picture_gen();
  s_in_sync = false;
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
