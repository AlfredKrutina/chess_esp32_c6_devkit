/**
 * @file game_hall_restore_guide.c
 * @brief Match engine pieces to Hall readings and pick up to 3 restore paths.
 */

#include "game_hall_restore_guide.h"
#include "game_hall_match.h"
#include "game_squares_guard.h"
#include "game_task_internal.h"
#include "chess_gameplay_policy.h"
#include "game_task.h"
#include "game_physical_undo.h"

#include "../led_task/include/led_board_restore_guide.h"
#include "game_gesture.h"
#include "../led_task/include/led_gesture_anim.h"
#include "../led_task/include/led_task.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "../squares_task/include/squares_task.h"

#include "esp_log.h"
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
  uint8_t path[LED_RESTORE_GUIDE_MAX_PATH_LEN];
  uint8_t path_len;
  bool swap_corridor;
} restore_job_t;

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

static void remember_digest(const uint8_t digest[64]) {
  memcpy(s_last_sensor_digest, digest, 64);
  s_digest_valid = true;
}

static bool mode_blocks_guide(void) {
  if (game_task_squares_guard_mode_conflict_active()) {
    return true;
  }
  if (piece_lifted && lifted_piece != PIECE_EMPTY) {
    return true;
  }
  if (game_physical_undo_is_active() || game_ep_capture_waiting() ||
      castling_state.in_progress || castle_animation_active ||
      promotion_state.pending) {
    return true;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
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

static bool collect_board(hall_piece_slot_t *expect, int *expect_n,
                          hall_piece_slot_t *phys, int *phys_n,
                          const uint8_t occ[64]) {
  *expect_n = 0;
  *phys_n = 0;
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t p = board[sq / 8][sq % 8];
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
      if (!square_phys_identity(sq, &t, &c)) {
        continue;
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
                        uint8_t missing[LED_RESTORE_GUIDE_MAX_MISSING],
                        uint8_t *missing_n, uint8_t extra[LED_RESTORE_GUIDE_MAX_EXTRA],
                        uint8_t *extra_n) {
  if (e_count == 0) {
    for (int i = 0; i < p_count; i++) {
      if (*extra_n < LED_RESTORE_GUIDE_MAX_EXTRA) {
        extra[(*extra_n)++] = phys[i].phys_sq;
      }
    }
    return;
  }
  if (p_count == 0) {
    for (int i = 0; i < e_count; i++) {
      if (*missing_n < LED_RESTORE_GUIDE_MAX_MISSING) {
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
          from, home, jobs[*job_n].path, LED_RESTORE_GUIDE_MAX_PATH_LEN);
    } else {
      jobs[*job_n].path_len = (uint8_t)build_manhattan_path(
          from, home, jobs[*job_n].path, LED_RESTORE_GUIDE_MAX_PATH_LEN, false);
    }
    jobs[*job_n].swap_corridor = false;
    (*job_n)++;
  }

  for (int ei = 0; ei < e_count; ei++) {
    if (!expect_used[ei] && *missing_n < LED_RESTORE_GUIDE_MAX_MISSING) {
      missing[(*missing_n)++] = expect[ei].home_sq;
    }
  }
  for (int pi = 0; pi < p_count; pi++) {
    if (!phys_used[pi] && *extra_n < LED_RESTORE_GUIDE_MAX_EXTRA) {
      extra[(*extra_n)++] = phys[pi].phys_sq;
    }
  }
}

static void match_all(const hall_piece_slot_t *expect, int expect_n,
                      const hall_piece_slot_t *phys, int phys_n,
                      restore_job_t *jobs, int *job_n,
                      uint8_t missing[LED_RESTORE_GUIDE_MAX_MISSING],
                      uint8_t *missing_n, uint8_t extra[LED_RESTORE_GUIDE_MAX_EXTRA],
                      uint8_t *extra_n) {
  *job_n = 0;
  *missing_n = 0;
  *extra_n = 0;

  for (int t = HALL_PIECE_TYPE_PAWN; t <= HALL_PIECE_TYPE_KING; t++) {
    for (int c = HALL_PIECE_COLOR_WHITE; c <= HALL_PIECE_COLOR_BLACK; c++) {
      int e_idx[MAX_PIECES];
      int p_idx[MAX_PIECES];
      int ec = 0;
      int pc = 0;
      for (int i = 0; i < expect_n; i++) {
        if (expect[i].type == (hall_piece_type_t)t &&
            expect[i].color == (hall_piece_color_t)c) {
          e_idx[ec++] = i;
        }
      }
      for (int i = 0; i < phys_n; i++) {
        if (phys[i].type == (hall_piece_type_t)t &&
            phys[i].color == (hall_piece_color_t)c) {
          p_idx[pc++] = i;
        }
      }
      hall_piece_slot_t e_slice[MAX_PIECES];
      hall_piece_slot_t p_slice[MAX_PIECES];
      for (int i = 0; i < ec; i++) {
        e_slice[i] = expect[e_idx[i]];
      }
      for (int i = 0; i < pc; i++) {
        p_slice[i] = phys[p_idx[i]];
      }
      match_group(e_slice, ec, p_slice, pc, jobs, job_n, missing, missing_n,
                  extra, extra_n);
    }
  }
}

static bool is_single_lift(int job_n, uint8_t missing_n, uint8_t extra_n) {
  return missing_n == 1 && extra_n == 0 && job_n == 0;
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
              end_a, end_b, jobs[i].path, LED_RESTORE_GUIDE_MAX_PATH_LEN);
          jobs[i].swap_corridor = false;
        } else {
          jobs[i].path_len = (uint8_t)build_manhattan_path(
              end_a, end_b, jobs[i].path, LED_RESTORE_GUIDE_MAX_PATH_LEN, false);
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
          job->from_sq, job->home_sq, job->path, LED_RESTORE_GUIDE_MAX_PATH_LEN);
    }
    return;
  }
  uint8_t from = job->path[0];
  uint8_t to = job->path[job->path_len - 1];
  if (from >= 64 || to >= 64) {
    job->path_len = 0;
    return;
  }
  uint8_t row_first[LED_RESTORE_GUIDE_MAX_PATH_LEN];
  uint8_t col_first[LED_RESTORE_GUIDE_MAX_PATH_LEN];
  int row_n = build_manhattan_path(from, to, row_first,
                                   LED_RESTORE_GUIDE_MAX_PATH_LEN, false);
  int col_n = build_manhattan_path(from, to, col_first,
                                   LED_RESTORE_GUIDE_MAX_PATH_LEN, true);
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
                               led_restore_guide_plan_t *plan) {
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

  int best_jobs[LED_RESTORE_GUIDE_MAX_PATHS];
  int best_count = 0;
  static int s_path_rotate;
  int start = (cand_n > 0) ? (s_path_rotate % cand_n) : 0;
  if (cand_n > LED_RESTORE_GUIDE_MAX_PATHS) {
    s_path_rotate++;
  }

  for (int step = 0; step < cand_n && best_count < LED_RESTORE_GUIDE_MAX_PATHS;
       step++) {
    int job_i = candidates[(start + step) % cand_n];
    int trial[LED_RESTORE_GUIDE_MAX_PATHS];
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
  return game_hall_engine_to_sensor(board[sq / 8][sq % 8], t, c);
}

static void push_setup_mark(led_restore_guide_plan_t *plan, uint8_t sq,
                            uint8_t type, uint8_t color, uint8_t role) {
  if (sq >= 64 || plan->mark_count >= LED_RESTORE_GUIDE_MAX_MARKS) {
    return;
  }
  for (uint8_t i = 0; i < plan->mark_count; i++) {
    if (plan->marks[i].square == sq) {
      return;
    }
  }
  led_restore_guide_mark_t *mark = &plan->marks[plan->mark_count++];
  mark->square = sq;
  mark->type = type;
  mark->color = color;
  mark->role = role;
}

static void fill_setup_marks(const restore_job_t *jobs, int job_n,
                             const uint8_t *missing, uint8_t missing_n,
                             const uint8_t *extra, uint8_t extra_n,
                             led_restore_guide_plan_t *plan) {
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
    push_setup_mark(plan, jobs[i].home_sq, (uint8_t)t, (uint8_t)c,
                    LED_RESTORE_MARK_HOME);
  }
  for (uint8_t i = 0; i < missing_n; i++) {
    hall_piece_type_t t = HALL_PIECE_TYPE_PAWN;
    hall_piece_color_t c = HALL_PIECE_COLOR_WHITE;
    engine_mark(missing[i], &t, &c);
    push_setup_mark(plan, missing[i], (uint8_t)t, (uint8_t)c,
                    LED_RESTORE_MARK_HOME);
  }
  for (int i = 0; i < job_n; i++) {
    hall_piece_type_t t = jobs[i].type;
    hall_piece_color_t c = HALL_PIECE_COLOR_WHITE;
    hall_piece_type_t et;
    hall_piece_color_t ec;
    if (engine_mark(jobs[i].home_sq, &et, &ec)) {
      t = et;
      c = ec;
    }
    push_setup_mark(plan, jobs[i].from_sq, (uint8_t)t, (uint8_t)c,
                    LED_RESTORE_MARK_SITTING);
  }
  for (uint8_t i = 0; i < extra_n; i++) {
    push_setup_mark(plan, extra[i], 0, 0, LED_RESTORE_MARK_EXTRA);
  }
}

void game_hall_restore_guide_request_sync(void) { s_sync_requested = true; }

bool game_hall_restore_guide_take_sync_request(void) {
  if (!s_sync_requested) {
    return false;
  }
  s_sync_requested = false;
  return true;
}

void game_hall_restore_guide_note_stable_squares(const uint8_t occ[64]) {
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
  return s_guide_led_active || led_board_restore_guide_is_active();
}

void game_hall_restore_guide_stop_leds(void) {
  s_guide_led_active = false;
  led_board_restore_guide_clear();
}

void game_hall_restore_guide_sync(void) {
  if (game_demo_show_active()) {
    return;
  }
#if HALL_DEBUG_LIGHT_WHERE_PIECE
  game_hall_restore_guide_stop_leds();
  return;
#endif
  if (led_lamp_board_held() || led_gesture_anim_busy()) {
    return;
  }
  if (!hall_i2c_squares_readings_usable()) {
    return;
  }
  uint8_t occ[64];
  uint8_t digest[64];
  hall_piece_slot_t expect[MAX_PIECES];
  hall_piece_slot_t phys[MAX_PIECES];
  restore_job_t jobs[MAX_JOBS];
  uint8_t missing[LED_RESTORE_GUIDE_MAX_MISSING];
  uint8_t extra[LED_RESTORE_GUIDE_MAX_EXTRA];
  led_restore_guide_plan_t plan;
  int expect_n = 0;
  int phys_n = 0;
  int job_n = 0;
  uint8_t missing_n = 0;
  uint8_t extra_n = 0;
  bool has_led_work = false;

  if (s_in_sync) {
    return;
  }
  s_in_sync = true;

  if (!chess_policy_squares_guard_enabled() || !chess_policy_squares_guard_led_enabled()) {
    game_hall_restore_guide_stop_leds();
    goto out;
  }
  if (mode_blocks_guide()) {
    game_hall_restore_guide_stop_leds();
    goto out;
  }

  squares_get_state(occ);
  digest_sensors(occ, digest);
  if (s_digest_valid && sensors_unchanged(digest)) {
    goto out;
  }

  /* Empty board ends dark. Lights that were on fade out instead of snapping off. */
  if (occupancy_empty(occ)) {
    if (led_board_restore_guide_is_active()) {
      led_board_restore_guide_fade_out();
      s_guide_led_active = false;
    } else {
      led_clear_board_only();
    }
    remember_digest(digest);
    goto out;
  }

  if (!collect_board(expect, &expect_n, phys, &phys_n, occ)) {
    goto out;
  }

  match_all(expect, expect_n, phys, phys_n, jobs, &job_n, missing, &missing_n,
            extra, &extra_n);

  if (board_fully_aligned(job_n, missing_n, extra_n)) {
    game_hall_restore_guide_stop_leds();
    remember_digest(digest);
    goto out;
  }

  if (is_single_lift(job_n, missing_n, extra_n)) {
    game_hall_restore_guide_stop_leds();
    remember_digest(digest);
    goto out;
  }

  merge_swap_pairs(jobs, &job_n);

  memset(&plan, 0, sizeof(plan));
  pick_display_paths(jobs, job_n, missing, missing_n, &plan);
  plan.missing_count = missing_n;
  memcpy(plan.missing_homes, missing, missing_n);
  plan.extra_count = extra_n;
  memcpy(plan.extra_squares, extra, extra_n);
  fill_setup_marks(jobs, job_n, missing, missing_n, extra, extra_n, &plan);

  has_led_work = plan.mark_count > 0;
  if (!has_led_work) {
    game_hall_restore_guide_stop_leds();
    remember_digest(digest);
    goto out;
  }

  plan.active = true;
  ensure_squares_pause();
  led_board_restore_guide_apply(&plan);
  s_guide_led_active = true;
  remember_digest(digest);

  ESP_LOGI(TAG,
           "[STAGING] restore guide: paths=%u missing=%u extra=%u jobs=%d",
           (unsigned)plan.path_count, (unsigned)missing_n, (unsigned)extra_n,
           job_n);
out:
  s_in_sync = false;
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
