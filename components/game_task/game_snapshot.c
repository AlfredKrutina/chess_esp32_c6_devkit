/**
 * @file game_snapshot.c
 * @brief NVS persistence for game state and boot tracker.
 */

#include "game_snapshot.h"
#include "game_matrix_guard.h"
#include "game_task_internal.h"

#include "../config_manager/include/config_manager.h"
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

static bool snapshot_loaded_on_boot = false;
static bool snapshot_restore_failed = false;
static bool snapshot_save_failed = false;
static bool snapshot_fallback_used = false;
static bool boot_new_game_triggered = false;

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
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN);
  (void)config_erase_key_from_nvs(CONFIG_NVS_KEY_BOOT_TRACKER);
}

void game_snapshot_reset_failure_flags(void) {
  snapshot_save_failed = false;
  snapshot_restore_failed = false;
}

static esp_err_t game_save_snapshot_to_nvs(void) {
  game_snapshot_full_t full = {0};
  full.version = GAME_SNAPSHOT_VERSION;
  full.format = 2;
  game_snapshot_fill_board(full.board);
  full.current_player = (uint8_t)current_player;
  full.game_state = (uint8_t)current_game_state;
  full.move_count = move_count;
  full.white_king_moved = (uint8_t)white_king_moved;
  full.white_rook_a_moved = (uint8_t)white_rook_a_moved;
  full.white_rook_h_moved = (uint8_t)white_rook_h_moved;
  full.black_king_moved = (uint8_t)black_king_moved;
  full.black_rook_a_moved = (uint8_t)black_rook_a_moved;
  full.black_rook_h_moved = (uint8_t)black_rook_h_moved;
  full.en_passant_available = (uint8_t)en_passant_available;
  full.en_passant_target_row = en_passant_target_row;
  full.en_passant_target_col = en_passant_target_col;
  full.en_passant_victim_row = en_passant_victim_row;
  full.en_passant_victim_col = en_passant_victim_col;
  full.promotion_pending = (uint8_t)promotion_state.pending;
  full.promotion_row = promotion_state.square_row;
  full.promotion_col = promotion_state.square_col;
  full.promotion_player = (uint8_t)promotion_state.player;
  full.white_time_total = white_time_total;
  full.black_time_total = black_time_total;
  full.white_remaining_ms = game_get_remaining_time(true);
  full.black_remaining_ms = game_get_remaining_time(false);
  full.history_count = (history_index > GAME_SNAPSHOT_HISTORY_CAP)
                           ? GAME_SNAPSHOT_HISTORY_CAP
                           : (uint8_t)history_index;
  if (full.history_count > 0) {
    uint32_t start = history_index - full.history_count;
    for (uint8_t i = 0; i < full.history_count; i++) {
      full.history[i] = move_history[start + i];
    }
  }
  full.crc32 = game_crc32_simple(((const uint8_t *)&full) + 8, sizeof(full) - 8);

  esp_err_t ret = config_save_blob_to_nvs(CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL, &full,
                                          sizeof(full));
  if (ret == ESP_OK) {
    snapshot_fallback_used = false;
    snapshot_save_failed = false;
    return ESP_OK;
  }

  game_snapshot_min_t min = {0};
  min.version = GAME_SNAPSHOT_VERSION;
  min.format = 1;
  memcpy(min.board, full.board, sizeof(min.board));
  min.current_player = full.current_player;
  min.game_state = full.game_state;
  min.move_count = full.move_count;
  min.white_king_moved = full.white_king_moved;
  min.white_rook_a_moved = full.white_rook_a_moved;
  min.white_rook_h_moved = full.white_rook_h_moved;
  min.black_king_moved = full.black_king_moved;
  min.black_rook_a_moved = full.black_rook_a_moved;
  min.black_rook_h_moved = full.black_rook_h_moved;
  min.en_passant_available = full.en_passant_available;
  min.en_passant_target_row = full.en_passant_target_row;
  min.en_passant_target_col = full.en_passant_target_col;
  min.en_passant_victim_row = full.en_passant_victim_row;
  min.en_passant_victim_col = full.en_passant_victim_col;
  min.promotion_pending = full.promotion_pending;
  min.promotion_row = full.promotion_row;
  min.promotion_col = full.promotion_col;
  min.promotion_player = full.promotion_player;
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
    history_index = full.history_count;
    if (history_index > 0) {
      memcpy(move_history, full.history, sizeof(chess_move_t) * history_index);
    }
    snapshot_loaded_on_boot = true;
    snapshot_fallback_used = false;
    game_active = (current_game_state != GAME_STATE_IDLE &&
                   current_game_state != GAME_STATE_FINISHED);
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
    game_active = (current_game_state != GAME_STATE_IDLE &&
                   current_game_state != GAME_STATE_FINISHED);
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
  esp_err_t ret = game_save_snapshot_to_nvs();
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Game snapshot save failed: %s", esp_err_to_name(ret));
  }
  game_boot_tracker_update_on_move();
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
  } else {
    esp_err_t restore_ret = game_load_snapshot_from_nvs();
    if (restore_ret == ESP_OK) {
      ESP_LOGI(TAG, "Game snapshot restored from NVS");
      game_matrix_guard_check_resync_after_restore();
    } else {
      ESP_LOGI(TAG, "No valid snapshot found, using fresh board");
    }
  }
}
