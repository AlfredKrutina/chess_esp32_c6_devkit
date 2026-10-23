/**
 * @file game_snapshot.c
 * @brief NVS persistence for game state and boot tracker.
 */

#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_hall_restore_guide.h"
#include "game_task_internal.h"

#include "../config_manager/include/config_manager.h"
#include "../timer_system/include/timer_system.h"
#include "hall_i2c_squares.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "GAME_SNAPSHOT";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

#define GAME_SNAPSHOT_VERSION 1
#define GAME_SNAPSHOT_VERSION_V2 2
#define GAME_SNAPSHOT_HISTORY_CAP 40
#define BOOT_WINDOW_SECONDS 60

typedef struct {
  uint32_t version;
  uint32_t crc32;
  uint8_t format;
  uint8_t board[64];
  uint8_t current_player;
  uint8_t game_state;
  uint32_t move_count;
  uint8_t white_king_moved;
  uint8_t white_rook_a_moved;
  uint8_t white_rook_h_moved;
  uint8_t black_king_moved;
  uint8_t black_rook_a_moved;
  uint8_t black_rook_h_moved;
  uint8_t en_passant_available;
  uint8_t en_passant_target_row;
  uint8_t en_passant_target_col;
  uint8_t en_passant_victim_row;
  uint8_t en_passant_victim_col;
  uint8_t promotion_pending;
  uint8_t promotion_row;
  uint8_t promotion_col;
  uint8_t promotion_player;
  uint32_t white_time_total;
  uint32_t black_time_total;
  uint32_t white_remaining_ms;
  uint32_t black_remaining_ms;
  uint8_t history_count;
  chess_move_t history[GAME_SNAPSHOT_HISTORY_CAP];
} game_snapshot_full_t;

typedef struct {
  uint32_t version;
  uint32_t crc32;
  uint8_t format;
  uint8_t board[64];
  uint8_t current_player;
  uint8_t game_state;
  uint32_t move_count;
  uint8_t white_king_moved;
  uint8_t white_rook_a_moved;
  uint8_t white_rook_h_moved;
  uint8_t black_king_moved;
  uint8_t black_rook_a_moved;
  uint8_t black_rook_h_moved;
  uint8_t en_passant_available;
  uint8_t en_passant_target_row;
  uint8_t en_passant_target_col;
  uint8_t en_passant_victim_row;
  uint8_t en_passant_victim_col;
  uint8_t promotion_pending;
  uint8_t promotion_row;
  uint8_t promotion_col;
  uint8_t promotion_player;
} game_snapshot_min_t;

typedef struct {
  uint32_t version;
  uint32_t crc32;
  uint32_t first_boot_epoch_s;
  uint8_t boot_counter_window;
  uint8_t moved_since_boot;
} game_boot_tracker_t;

#pragma pack(push, 1)
typedef struct {
  uint8_t from_row;
  uint8_t from_col;
  uint8_t to_row;
  uint8_t to_col;
  uint8_t piece;
  uint8_t captured;
  uint32_t timestamp;
  uint8_t kind;
  uint8_t side_before;
  uint8_t white_king_moved;
  uint8_t white_rook_a_moved;
  uint8_t white_rook_h_moved;
  uint8_t black_king_moved;
  uint8_t black_rook_a_moved;
  uint8_t black_rook_h_moved;
  uint8_t en_passant_available;
  uint8_t en_passant_target_row;
  uint8_t en_passant_target_col;
  uint8_t en_passant_victim_row;
  uint8_t en_passant_victim_col;
  uint8_t rook_home_col;
  uint8_t rook_castle_col;
  uint8_t promoted_to;
  uint8_t capture_return_piece;
  uint8_t capture_return_row;
  uint8_t capture_return_col;
} game_snapshot_hist_v2_t;

typedef struct {
  uint32_t version;
  uint32_t crc32;
  uint32_t seq;
  uint8_t format;
  uint8_t board[64];
  uint8_t current_player;
  uint8_t game_state;
  uint32_t move_count;
  uint8_t white_king_moved;
  uint8_t white_rook_a_moved;
  uint8_t white_rook_h_moved;
  uint8_t black_king_moved;
  uint8_t black_rook_a_moved;
  uint8_t black_rook_h_moved;
  uint8_t en_passant_available;
  uint8_t en_passant_target_row;
  uint8_t en_passant_target_col;
  uint8_t en_passant_victim_row;
  uint8_t en_passant_victim_col;
  uint8_t promotion_pending;
  uint8_t promotion_row;
  uint8_t promotion_col;
  uint8_t promotion_player;
  uint32_t white_time_total;
  uint32_t black_time_total;
  uint32_t white_remaining_ms;
  uint32_t black_remaining_ms;
  uint32_t fifty_move;
  uint32_t moves_without_capture;
  uint8_t result_type;
  uint8_t endgame_reason;
  uint8_t castle_in_progress;
  uint8_t castle_player;
  uint8_t castle_kingside;
  uint8_t rook_from_row;
  uint8_t rook_from_col;
  uint8_t rook_to_row;
  uint8_t rook_to_col;
  uint8_t history_count;
  game_snapshot_hist_v2_t history[GAME_SNAPSHOT_HISTORY_CAP];
} game_snapshot_v2_t;
#pragma pack(pop)

static bool snapshot_loaded_on_boot = false;
static bool snapshot_restore_failed = false;
static bool snapshot_save_failed = false;
static bool snapshot_fallback_used = false;
static bool boot_new_game_triggered = false;
static bool s_reconcile_pending = false;
static bool s_clock_held = false;

static uint32_t game_crc32_simple(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  if (data == NULL) {
    return h;
  }
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

static void game_snapshot_fill_board(uint8_t out_board[64]) {
  if (out_board == NULL) {
    return;
  }
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      out_board[row * 8 + col] = (uint8_t)board[row][col];
    }
  }
}

static void game_snapshot_apply_board(const uint8_t in_board[64]) {
  if (in_board == NULL) {
    return;
  }
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      board[row][col] = (piece_t)in_board[row * 8 + col];
    }
  }
}

bool game_was_snapshot_loaded_on_boot(void) { return snapshot_loaded_on_boot; }

bool game_is_snapshot_fallback_used(void) { return snapshot_fallback_used; }

bool game_has_snapshot_restore_failure(void) { return snapshot_restore_failed; }

bool game_has_snapshot_save_failure(void) { return snapshot_save_failed; }

bool game_was_boot_new_game_triggered(void) { return boot_new_game_triggered; }

void game_snapshot_erase_nvs(void) {
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL);
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_B);
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN);
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_BOOT_TRACKER);
}

void game_snapshot_reset_failure_flags(void) {
  snapshot_save_failed = false;
  snapshot_restore_failed = false;
}

static bool game_snapshot_side_mode_active(void) {
  return game_is_opening_trainer_active() ||
         game_is_opening_trainer_setup_active() || puzzle_active ||
         puzzle_setup_active || board_setup_tutorial_active;
}

static void game_snapshot_publish_hall_empty(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  uint8_t empty[64];
  uint8_t types[64];
  for (int i = 0; i < 64; i++) {
    piece_t p = board[i / 8][i % 8];
    uint8_t t = 0;
    if (p >= PIECE_WHITE_PAWN && p <= PIECE_WHITE_KING) {
      t = (uint8_t)p;
    } else if (p >= PIECE_BLACK_PAWN && p <= PIECE_BLACK_KING) {
      t = (uint8_t)(p - PIECE_WHITE_KING);
    }
    types[i] = t;
    empty[i] = (t == 0) ? 1u : 0u;
  }
  hall_i2c_squares_set_logical_empty(empty, types);
#else
  (void)0;
#endif
}

static void game_snapshot_hold_clocks(uint32_t white_ms, uint32_t black_ms) {
  if (!game_is_timer_active()) {
    s_clock_held = false;
    return;
  }
  (void)timer_restore_remaining_paused(white_ms, black_ms,
                                      current_player == PLAYER_WHITE);
  s_clock_held = (current_game_state == GAME_STATE_ACTIVE ||
                  current_game_state == GAME_STATE_PLAYING);
}

static void game_snapshot_mark_active_from_state(void) {
  game_active = (current_game_state == GAME_STATE_ACTIVE ||
                 current_game_state == GAME_STATE_PLAYING ||
                 current_game_state == GAME_STATE_PROMOTION ||
                 promotion_state.pending);
}

static bool game_snapshot_v2_sane(const game_snapshot_v2_t *snap) {
  if (snap == NULL || snap->current_player > PLAYER_BLACK) {
    return false;
  }
  if (snap->game_state > GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    return false;
  }
  if (snap->history_count > GAME_SNAPSHOT_HISTORY_CAP) {
    return false;
  }
  for (int i = 0; i < 64; i++) {
    if (snap->board[i] > PIECE_BLACK_KING) {
      return false;
    }
  }
  return true;
}

static bool game_snapshot_v2_crc_ok(const game_snapshot_v2_t *snap, size_t len) {
  if (snap == NULL || len != sizeof(*snap) ||
      snap->version != GAME_SNAPSHOT_VERSION_V2) {
    return false;
  }
  uint32_t crc =
      game_crc32_simple(((const uint8_t *)snap) + 8, sizeof(*snap) - 8);
  return crc == snap->crc32 && game_snapshot_v2_sane(snap);
}

static bool game_snapshot_load_v2_key(const char *key, game_snapshot_v2_t *out) {
  size_t len = sizeof(*out);
  esp_err_t ret = config_load_blob_from_nvs(key, out, &len);
  return ret == ESP_OK && game_snapshot_v2_crc_ok(out, len);
}

static void game_snapshot_apply_v2(const game_snapshot_v2_t *snap) {
  game_snapshot_apply_board(snap->board);
  current_player = (player_t)snap->current_player;
  current_game_state = (game_state_t)snap->game_state;
  move_count = snap->move_count;
  white_king_moved = snap->white_king_moved;
  white_rook_a_moved = snap->white_rook_a_moved;
  white_rook_h_moved = snap->white_rook_h_moved;
  black_king_moved = snap->black_king_moved;
  black_rook_a_moved = snap->black_rook_a_moved;
  black_rook_h_moved = snap->black_rook_h_moved;
  en_passant_available = snap->en_passant_available;
  en_passant_target_row = snap->en_passant_target_row;
  en_passant_target_col = snap->en_passant_target_col;
  en_passant_victim_row = snap->en_passant_victim_row;
  en_passant_victim_col = snap->en_passant_victim_col;
  promotion_state.pending = snap->promotion_pending;
  promotion_state.square_row = snap->promotion_row;
  promotion_state.square_col = snap->promotion_col;
  promotion_state.player = (player_t)snap->promotion_player;
  white_time_total = snap->white_time_total;
  black_time_total = snap->black_time_total;
  game_set_fifty_move_counter(snap->fifty_move);
  moves_without_capture = snap->moves_without_capture;
  current_result_type = (game_result_type_t)snap->result_type;
  current_endgame_reason = (endgame_reason_t)snap->endgame_reason;

  memset(&castling_state, 0, sizeof(castling_state));
  if (snap->castle_in_progress) {
    castling_state.in_progress = true;
    castling_state.player = (player_t)snap->castle_player;
    castling_state.is_kingside = snap->castle_kingside != 0;
    castling_state.rook_from_row = snap->rook_from_row;
    castling_state.rook_from_col = snap->rook_from_col;
    castling_state.rook_to_row = snap->rook_to_row;
    castling_state.rook_to_col = snap->rook_to_col;
  }

  memset(move_history, 0, sizeof(move_history));
  memset(move_history_kind, 0, sizeof(move_history_kind));
  memset(move_history_meta, 0, sizeof(move_history_meta));
  history_index = snap->history_count;
  for (uint8_t i = 0; i < snap->history_count; i++) {
    const game_snapshot_hist_v2_t *h = &snap->history[i];
    chess_move_t *m = &move_history[i];
    m->from_row = h->from_row;
    m->from_col = h->from_col;
    m->to_row = h->to_row;
    m->to_col = h->to_col;
    m->piece = (piece_t)h->piece;
    m->captured_piece = (piece_t)h->captured;
    m->timestamp = h->timestamp;
    move_history_kind[i] = (move_type_t)h->kind;
    move_undo_meta_t *meta = &move_history_meta[i];
    meta->side_before = (player_t)h->side_before;
    meta->white_king_moved = h->white_king_moved != 0;
    meta->white_rook_a_moved = h->white_rook_a_moved != 0;
    meta->white_rook_h_moved = h->white_rook_h_moved != 0;
    meta->black_king_moved = h->black_king_moved != 0;
    meta->black_rook_a_moved = h->black_rook_a_moved != 0;
    meta->black_rook_h_moved = h->black_rook_h_moved != 0;
    meta->en_passant_available = h->en_passant_available != 0;
    meta->en_passant_target_row = h->en_passant_target_row;
    meta->en_passant_target_col = h->en_passant_target_col;
    meta->en_passant_victim_row = h->en_passant_victim_row;
    meta->en_passant_victim_col = h->en_passant_victim_col;
    meta->rook_home_col = h->rook_home_col;
    meta->rook_castle_col = h->rook_castle_col;
    meta->promoted_to = (piece_t)h->promoted_to;
    meta->capture_return_piece = (piece_t)h->capture_return_piece;
    meta->capture_return_row = h->capture_return_row;
    meta->capture_return_col = h->capture_return_col;
  }
  if (history_index > 0) {
    chess_move_t *last = &move_history[history_index - 1];
    last_move_from_row = last->from_row;
    last_move_from_col = last->from_col;
    last_move_to_row = last->to_row;
    last_move_to_col = last->to_col;
    has_last_move = true;
  } else {
    has_last_move = false;
  }

  snapshot_restore_failed = false;
  snapshot_loaded_on_boot = true;
  snapshot_fallback_used = false;
  game_snapshot_mark_active_from_state();
  game_snapshot_hold_clocks(snap->white_remaining_ms, snap->black_remaining_ms);
}

static void game_snapshot_fill_v2(game_snapshot_v2_t *snap, uint32_t seq) {
  memset(snap, 0, sizeof(*snap));
  snap->version = GAME_SNAPSHOT_VERSION_V2;
  snap->seq = seq;
  snap->format = 2;
  game_snapshot_fill_board(snap->board);
  snap->current_player = (uint8_t)current_player;
  snap->game_state = (uint8_t)current_game_state;
  snap->move_count = move_count;
  snap->white_king_moved = (uint8_t)white_king_moved;
  snap->white_rook_a_moved = (uint8_t)white_rook_a_moved;
  snap->white_rook_h_moved = (uint8_t)white_rook_h_moved;
  snap->black_king_moved = (uint8_t)black_king_moved;
  snap->black_rook_a_moved = (uint8_t)black_rook_a_moved;
  snap->black_rook_h_moved = (uint8_t)black_rook_h_moved;
  snap->en_passant_available = (uint8_t)en_passant_available;
  snap->en_passant_target_row = en_passant_target_row;
  snap->en_passant_target_col = en_passant_target_col;
  snap->en_passant_victim_row = en_passant_victim_row;
  snap->en_passant_victim_col = en_passant_victim_col;
  snap->promotion_pending = (uint8_t)promotion_state.pending;
  snap->promotion_row = promotion_state.square_row;
  snap->promotion_col = promotion_state.square_col;
  snap->promotion_player = (uint8_t)promotion_state.player;
  snap->white_time_total = white_time_total;
  snap->black_time_total = black_time_total;
  snap->white_remaining_ms = game_get_remaining_time(true);
  snap->black_remaining_ms = game_get_remaining_time(false);
  snap->fifty_move = game_get_fifty_move_counter();
  snap->moves_without_capture = moves_without_capture;
  snap->result_type = (uint8_t)current_result_type;
  snap->endgame_reason = (uint8_t)current_endgame_reason;
  snap->castle_in_progress = (uint8_t)castling_state.in_progress;
  snap->castle_player = (uint8_t)castling_state.player;
  snap->castle_kingside = (uint8_t)castling_state.is_kingside;
  snap->rook_from_row = castling_state.rook_from_row;
  snap->rook_from_col = castling_state.rook_from_col;
  snap->rook_to_row = castling_state.rook_to_row;
  snap->rook_to_col = castling_state.rook_to_col;
  snap->history_count = (history_index > GAME_SNAPSHOT_HISTORY_CAP)
                            ? GAME_SNAPSHOT_HISTORY_CAP
                            : (uint8_t)history_index;
  if (snap->history_count > 0) {
    uint32_t start = history_index - snap->history_count;
    for (uint8_t i = 0; i < snap->history_count; i++) {
      const chess_move_t *m = &move_history[start + i];
      const move_undo_meta_t *meta = &move_history_meta[start + i];
      game_snapshot_hist_v2_t *h = &snap->history[i];
      h->from_row = m->from_row;
      h->from_col = m->from_col;
      h->to_row = m->to_row;
      h->to_col = m->to_col;
      h->piece = (uint8_t)m->piece;
      h->captured = (uint8_t)m->captured_piece;
      h->timestamp = m->timestamp;
      h->kind = (uint8_t)move_history_kind[start + i];
      h->side_before = (uint8_t)meta->side_before;
      h->white_king_moved = (uint8_t)meta->white_king_moved;
      h->white_rook_a_moved = (uint8_t)meta->white_rook_a_moved;
      h->white_rook_h_moved = (uint8_t)meta->white_rook_h_moved;
      h->black_king_moved = (uint8_t)meta->black_king_moved;
      h->black_rook_a_moved = (uint8_t)meta->black_rook_a_moved;
      h->black_rook_h_moved = (uint8_t)meta->black_rook_h_moved;
      h->en_passant_available = (uint8_t)meta->en_passant_available;
      h->en_passant_target_row = meta->en_passant_target_row;
      h->en_passant_target_col = meta->en_passant_target_col;
      h->en_passant_victim_row = meta->en_passant_victim_row;
      h->en_passant_victim_col = meta->en_passant_victim_col;
      h->rook_home_col = meta->rook_home_col;
      h->rook_castle_col = meta->rook_castle_col;
      h->promoted_to = (uint8_t)meta->promoted_to;
      h->capture_return_piece = (uint8_t)meta->capture_return_piece;
      h->capture_return_row = meta->capture_return_row;
      h->capture_return_col = meta->capture_return_col;
    }
  }
  snap->crc32 = game_crc32_simple(((const uint8_t *)snap) + 8, sizeof(*snap) - 8);
}

static esp_err_t game_save_snapshot_to_nvs(void) {
  /* Static: three v2 blobs do not fit on the 6 KB game task stack. */
  static game_snapshot_v2_t slot_a;
  static game_snapshot_v2_t slot_b;
  static game_snapshot_v2_t snap;
  bool have_a =
      game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &slot_a);
  bool have_b = game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_B, &slot_b);
  uint32_t best_seq = 0;
  int best_slot = 0;
  if (have_a && slot_a.seq >= best_seq) {
    best_seq = slot_a.seq;
    best_slot = 1;
  }
  if (have_b && slot_b.seq >= best_seq) {
    best_seq = slot_b.seq;
    best_slot = 2;
  }

  game_snapshot_fill_v2(&snap, best_seq + 1u);
  /* Write the slot that is not the current good copy. A cut mid-write leaves it. */
  const char *key = (best_slot == 1) ? CONFIG_NVS_KEY_GAME_SNAPSHOT_B
                                      : CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL;
  esp_err_t ret = config_save_blob_to_nvs(key, &snap, sizeof(snap));
  if (ret == ESP_OK) {
    snapshot_fallback_used = false;
    snapshot_save_failed = false;
    STAGING_LOGI(TAG, "snapshot v2 seq=%lu -> %s", (unsigned long)snap.seq, key);
    return ESP_OK;
  }

  game_snapshot_min_t min = {0};
  min.version = GAME_SNAPSHOT_VERSION;
  min.format = 1;
  memcpy(min.board, snap.board, sizeof(min.board));
  min.current_player = snap.current_player;
  min.game_state = snap.game_state;
  min.move_count = snap.move_count;
  min.white_king_moved = snap.white_king_moved;
  min.white_rook_a_moved = snap.white_rook_a_moved;
  min.white_rook_h_moved = snap.white_rook_h_moved;
  min.black_king_moved = snap.black_king_moved;
  min.black_rook_a_moved = snap.black_rook_a_moved;
  min.black_rook_h_moved = snap.black_rook_h_moved;
  min.en_passant_available = snap.en_passant_available;
  min.en_passant_target_row = snap.en_passant_target_row;
  min.en_passant_target_col = snap.en_passant_target_col;
  min.en_passant_victim_row = snap.en_passant_victim_row;
  min.en_passant_victim_col = snap.en_passant_victim_col;
  min.promotion_pending = snap.promotion_pending;
  min.promotion_row = snap.promotion_row;
  min.promotion_col = snap.promotion_col;
  min.promotion_player = snap.promotion_player;
  min.crc32 = game_crc32_simple(((const uint8_t *)&min) + 8, sizeof(min) - 8);

  ret = config_save_blob_to_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN, &min, sizeof(min));
  if (ret == ESP_OK) {
    snapshot_fallback_used = true;
    snapshot_save_failed = false;
  } else {
    snapshot_save_failed = true;
  }
  return ret;
}

bool game_snapshot_nvs_has_valid(void) {
  static game_snapshot_v2_t v2;
  if (game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &v2) ||
      game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_B, &v2)) {
    return true;
  }

  game_snapshot_full_t full = {0};
  size_t full_len = sizeof(full);
  esp_err_t ret =
      config_load_blob_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &full, &full_len);
  if (ret == ESP_OK && full_len == sizeof(full) &&
      full.version == GAME_SNAPSHOT_VERSION &&
      full.crc32 ==
          game_crc32_simple(((const uint8_t *)&full) + 8, sizeof(full) - 8)) {
    return true;
  }

  game_snapshot_min_t min = {0};
  size_t min_len = sizeof(min);
  ret = config_load_blob_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN, &min, &min_len);
  if (ret == ESP_OK && min_len == sizeof(min) && min.version == GAME_SNAPSHOT_VERSION &&
      min.crc32 ==
          game_crc32_simple(((const uint8_t *)&min) + 8, sizeof(min) - 8)) {
    return true;
  }
  return false;
}

static esp_err_t game_load_snapshot_from_nvs(void) {
  static game_snapshot_v2_t slot_a;
  static game_snapshot_v2_t slot_b;
  bool have_a =
      game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &slot_a);
  bool have_b = game_snapshot_load_v2_key(CONFIG_NVS_KEY_GAME_SNAPSHOT_B, &slot_b);
  if (have_a || have_b) {
    const game_snapshot_v2_t *best = NULL;
    if (have_a && have_b) {
      best = (slot_b.seq > slot_a.seq) ? &slot_b : &slot_a;
    } else {
      best = have_b ? &slot_b : &slot_a;
    }
    game_snapshot_apply_v2(best);
    ESP_LOGI(TAG, "Game snapshot v2 restored seq=%lu state=%u",
             (unsigned long)best->seq, (unsigned)best->game_state);
    return ESP_OK;
  }

  game_snapshot_full_t full = {0};
  size_t full_len = sizeof(full);
  esp_err_t ret =
      config_load_blob_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &full, &full_len);
  if (ret == ESP_OK && full_len == sizeof(full) &&
      full.version == GAME_SNAPSHOT_VERSION &&
      full.crc32 ==
          game_crc32_simple(((const uint8_t *)&full) + 8, sizeof(full) - 8)) {
    snapshot_restore_failed = false;
    game_snapshot_apply_board(full.board);
    current_player = (player_t)full.current_player;
    current_game_state = (game_state_t)full.game_state;
    move_count = full.move_count;
    white_king_moved = full.white_king_moved;
    white_rook_a_moved = full.white_rook_a_moved;
    white_rook_h_moved = full.white_rook_h_moved;
    black_king_moved = full.black_king_moved;
    black_rook_a_moved = full.black_rook_a_moved;
    black_rook_h_moved = full.black_rook_h_moved;
    en_passant_available = full.en_passant_available;
    en_passant_target_row = full.en_passant_target_row;
    en_passant_target_col = full.en_passant_target_col;
    en_passant_victim_row = full.en_passant_victim_row;
    en_passant_victim_col = full.en_passant_victim_col;
    promotion_state.pending = full.promotion_pending;
    promotion_state.square_row = full.promotion_row;
    promotion_state.square_col = full.promotion_col;
    promotion_state.player = (player_t)full.promotion_player;
    white_time_total = full.white_time_total;
    black_time_total = full.black_time_total;
    game_snapshot_hold_clocks(full.white_remaining_ms, full.black_remaining_ms);
    history_index = full.history_count;
    if (history_index > 0) {
      memcpy(move_history, full.history, sizeof(chess_move_t) * history_index);
    }
    snapshot_loaded_on_boot = true;
    snapshot_fallback_used = false;
    game_snapshot_mark_active_from_state();
    return ESP_OK;
  }

  game_snapshot_min_t min = {0};
  size_t min_len = sizeof(min);
  ret = config_load_blob_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN, &min, &min_len);
  if (ret == ESP_OK && min_len == sizeof(min) && min.version == GAME_SNAPSHOT_VERSION &&
      min.crc32 ==
          game_crc32_simple(((const uint8_t *)&min) + 8, sizeof(min) - 8)) {
    snapshot_restore_failed = false;
    game_snapshot_apply_board(min.board);
    current_player = (player_t)min.current_player;
    current_game_state = (game_state_t)min.game_state;
    move_count = min.move_count;
    white_king_moved = min.white_king_moved;
    white_rook_a_moved = min.white_rook_a_moved;
    white_rook_h_moved = min.white_rook_h_moved;
    black_king_moved = min.black_king_moved;
    black_rook_a_moved = min.black_rook_a_moved;
    black_rook_h_moved = min.black_rook_h_moved;
    en_passant_available = min.en_passant_available;
    en_passant_target_row = min.en_passant_target_row;
    en_passant_target_col = min.en_passant_target_col;
    en_passant_victim_row = min.en_passant_victim_row;
    en_passant_victim_col = min.en_passant_victim_col;
    promotion_state.pending = min.promotion_pending;
    promotion_state.square_row = min.promotion_row;
    promotion_state.square_col = min.promotion_col;
    promotion_state.player = (player_t)min.promotion_player;
    history_index = 0;
    snapshot_loaded_on_boot = true;
    snapshot_fallback_used = true;
    game_snapshot_mark_active_from_state();
    return ESP_OK;
  }

  snapshot_restore_failed = false;
  return ESP_ERR_NOT_FOUND;
}

static void game_boot_tracker_update_on_move(void) {
  game_boot_tracker_t trk = {0};
  size_t len = sizeof(trk);
  bool loaded =
      (config_load_blob_from_nvs(CONFIG_NVS_KEY_BOOT_TRACKER, &trk, &len) == ESP_OK &&
       len == sizeof(trk) && trk.version == GAME_SNAPSHOT_VERSION &&
       trk.crc32 == game_crc32_simple(((const uint8_t *)&trk) + 8, sizeof(trk) - 8));

  if (!loaded) {
    time_t now = time(NULL);
    trk.version = GAME_SNAPSHOT_VERSION;
    trk.first_boot_epoch_s = (now >= 1700000000) ? (uint32_t)now : 0u;
    trk.boot_counter_window = 1u;
  }
  trk.moved_since_boot = 1u;
  trk.crc32 = game_crc32_simple(((const uint8_t *)&trk) + 8, sizeof(trk) - 8);
  (void)config_save_blob_to_nvs(CONFIG_NVS_KEY_BOOT_TRACKER, &trk, sizeof(trk));
}

static bool game_boot_tracker_should_force_new_game(void) {
  time_t now = time(NULL);
  const bool wall_time_ok = (now >= 1700000000);
  const uint32_t now_s = wall_time_ok ? (uint32_t)now : 0u;

  game_boot_tracker_t trk = {0};
  size_t len = sizeof(trk);
  bool loaded =
      (config_load_blob_from_nvs(CONFIG_NVS_KEY_BOOT_TRACKER, &trk, &len) == ESP_OK &&
       len == sizeof(trk) && trk.version == GAME_SNAPSHOT_VERSION &&
       trk.crc32 == game_crc32_simple(((const uint8_t *)&trk) + 8, sizeof(trk) - 8));

  bool in_window = false;
  if (loaded) {
    if (wall_time_ok) {
      in_window = (now_s >= trk.first_boot_epoch_s &&
                   (now_s - trk.first_boot_epoch_s) <= BOOT_WINDOW_SECONDS);
    } else {
      in_window = true;
    }
  }

  bool force_new_game = false;
  if (wall_time_ok && loaded && trk.moved_since_boot == 0u && in_window &&
      trk.boot_counter_window == 1u) {
    force_new_game = true;
  }

  game_boot_tracker_t next = {0};
  next.version = GAME_SNAPSHOT_VERSION;
  next.moved_since_boot = 0u;
  if (loaded && in_window) {
    next.first_boot_epoch_s = trk.first_boot_epoch_s;
    next.boot_counter_window = trk.boot_counter_window + 1u;
  } else if (wall_time_ok) {
    next.first_boot_epoch_s = now_s;
    next.boot_counter_window = 1u;
  } else {
    next.first_boot_epoch_s = loaded ? trk.first_boot_epoch_s : 0u;
    next.boot_counter_window = 1u;
  }
  next.crc32 = game_crc32_simple(((const uint8_t *)&next) + 8, sizeof(next) - 8);
  (void)config_save_blob_to_nvs(CONFIG_NVS_KEY_BOOT_TRACKER, &next, sizeof(next));

  return force_new_game;
}

void game_snapshot_persist_after_valid_move(void) {
  if (game_snapshot_side_mode_active()) {
    STAGING_LOGI(TAG, "skip NVS: trainer/puzzle/tutorial must not replace the game");
    return;
  }
  s_clock_held = false;
  esp_err_t ret = game_save_snapshot_to_nvs();
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Game snapshot save failed: %s", esp_err_to_name(ret));
  } else {
    game_snapshot_publish_hall_empty();
  }
  game_boot_tracker_update_on_move();
}

bool game_snapshot_sensors_ready(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return hall_i2c_squares_readings_usable();
#else
  return true;
#endif
}

void game_snapshot_service_after_boot(void) {
  if (!game_snapshot_sensors_ready()) {
    return;
  }
  if (s_reconcile_pending) {
    s_reconcile_pending = false;
    game_squares_guard_check_resync_after_restore();
    ESP_LOGI(TAG, "Post-boot sensor reconcile done (guard=%d)",
             game_is_squares_guard_active() ? 1 : 0);
  }
  if (!s_clock_held) {
    return;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_PLAYING) {
    s_clock_held = false;
    return;
  }
  if (game_is_squares_guard_active()) {
    return;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (game_hall_restore_guide_is_active()) {
    return;
  }
#endif
  s_clock_held = false;
  if (game_is_timer_active()) {
    (void)game_start_timer_move(current_player == PLAYER_WHITE);
    ESP_LOGI(TAG, "Clock resumed after outage for %s",
             current_player == PLAYER_WHITE ? "white" : "black");
  }
}

void game_snapshot_restore_on_boot(void) {
  bool has_valid_snapshot = game_snapshot_nvs_has_valid();
  if (has_valid_snapshot) {
    boot_new_game_triggered = false;
    STAGING_LOGI(TAG,
                 "boot: valid NVS snapshot present — skipping force-new-game "
                 "heuristic");
  } else {
    boot_new_game_triggered = game_boot_tracker_should_force_new_game();
  }

  if (boot_new_game_triggered) {
    ESP_LOGW(TAG, "Boot rule triggered: forcing new game (2 boots / 1 minute)");
    game_start_new_game();
    snapshot_loaded_on_boot = false;
    s_reconcile_pending = false;
  } else {
    esp_err_t restore_ret = game_load_snapshot_from_nvs();
    if (restore_ret == ESP_OK) {
      ESP_LOGI(TAG, "Game snapshot restored from NVS");
      s_reconcile_pending = true;
    } else {
      ESP_LOGI(TAG, "No valid snapshot found, using fresh board");
      s_reconcile_pending = false;
    }
  }
  game_snapshot_publish_hall_empty();
}
