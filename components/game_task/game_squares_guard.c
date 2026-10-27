/**
 * @file game_squares_guard.c
 * @brief Matrix guard — LED feedback and pause when sensors disagree with logic.
 */

#include "game_squares_guard.h"
#include "game_task_internal.h"
#include "chess_gameplay_policy.h"
#include "game_hall_match.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_restore_guide.h"
#include "game_hall_position.h"
#endif

#include "freertos_chess.h"
#include "game_task.h"
#include "game_gesture.h"
#include "led_mapping.h"
#include "led_task.h"
#include "hall_i2c_squares.h"
#include "squares_task.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "GAME_SQUARES_GUARD";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

typedef struct {
  bool active;
  uint32_t lifted_mask_low;
  uint32_t lifted_mask_high;
  uint32_t dropped_mask_low;
  uint32_t dropped_mask_high;
  uint8_t conflict_count;
} squares_guard_pause_state_t;

static squares_guard_pause_state_t squares_guard_pause_state = {0};

static uint8_t game_count_mask_bits(uint32_t low, uint32_t high) {
  uint8_t count = 0;
  while (low != 0) {
    count += (uint8_t)(low & 1U);
    low >>= 1;
  }
  while (high != 0) {
    count += (uint8_t)(high & 1U);
    high >>= 1;
  }
  return count;
}

static void game_board_to_occupancy(uint8_t out[64]) {
  if (out == NULL) {
    return;
  }
  for (uint8_t i = 0; i < 64; i++) {
    out[i] = (((uint8_t)board[i / 8][i % 8]) == PIECE_EMPTY) ? 0 : 1;
  }
}

static bool game_squares_sensor_matches_engine(uint8_t square,
                                              const uint8_t phys[64]) {
  piece_t piece = board[square / 8][square % 8];
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return game_hall_square_agrees_with_logic(square, piece, phys[square]);
#else
  uint8_t expected_occ = (piece == PIECE_EMPTY) ? 0 : 1;
  return phys[square] == expected_occ;
#endif
}

bool game_is_squares_guard_active(void) {
  if (!chess_policy_squares_guard_enabled()) {
    return false;
  }
  return squares_guard_pause_state.active;
}

uint8_t game_get_squares_guard_conflict_count(void) {
  return squares_guard_pause_state.conflict_count;
}

uint32_t game_get_squares_guard_lifted_mask_low(void) {
  return squares_guard_pause_state.lifted_mask_low;
}

uint32_t game_get_squares_guard_lifted_mask_high(void) {
  return squares_guard_pause_state.lifted_mask_high;
}

uint32_t game_get_squares_guard_dropped_mask_low(void) {
  return squares_guard_pause_state.dropped_mask_low;
}

uint32_t game_get_squares_guard_dropped_mask_high(void) {
  return squares_guard_pause_state.dropped_mask_high;
}

void game_force_clear_squares_guard(void) {
  if (!squares_guard_pause_state.active && !squares_is_guard_mode_active()) {
    return;
  }
  game_squares_guard_restore_after_clear(true);
  STAGING_LOGI(TAG, "Matrix guard force-cleared (game + matrix)");
}

void game_squares_guard_clear_both_layers(void) {
  squares_guard_pause_state.active = false;
  squares_guard_pause_state.lifted_mask_low = 0;
  squares_guard_pause_state.lifted_mask_high = 0;
  squares_guard_pause_state.dropped_mask_low = 0;
  squares_guard_pause_state.dropped_mask_high = 0;
  squares_guard_pause_state.conflict_count = 0;
  resync_required_after_restore = false;
  squares_abort_ambiguous_guard_baseline();
}

void game_squares_guard_restore_after_clear(bool notify_clients) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_stop_leds();
#endif
  game_squares_guard_clear_both_layers();
  led_clear_board_only();
  chess_policy_highlight_movable_if_enabled();
  if (notify_clients) {
    game_bump_revision_and_notify();
  }
}

void game_squares_guard_render_leds(void) {
  if (game_demo_show_active()) {
    return;
  }
#if HALL_DEBUG_LIGHT_WHERE_PIECE
  return;
#endif
  if (!squares_guard_pause_state.active) {
    return;
  }
  if (!chess_policy_squares_guard_led_enabled()) {
    return;
  }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* Yellow and blue here mean "a piece should stand on this square".
   * On Hall that locked the board after boot. The opening guide draws
   * those homes instead, and only before the first move. */
  game_hall_restore_guide_sync();
  return;
#else
  led_clear_board_only();
  uint8_t phys[64] = {0};
  squares_get_state(phys);

  bool any_piece = false;
  for (uint8_t i = 0; i < 64; i++) {
    if (phys[i] != 0) {
      any_piece = true;
      break;
    }
  }
  if (!any_piece) {
    return;
  }

  for (uint8_t square = 0; square < 64; square++) {
    uint8_t row = square / 8;
    uint8_t col = square % 8;
    piece_t piece = board[row][col];
    if (game_squares_sensor_matches_engine(square, phys)) {
      continue;
    }

    if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
      if (chess_policy_mg_led_white_yellow()) {
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0);
      }
    } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
      if (chess_policy_mg_led_black_blue()) {
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0, 255);
      }
    } else if (phys[square] != 0) {
      if (chess_policy_mg_led_ghost_orange()) {
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 140, 0);
      }
    } else if (chess_policy_mg_led_missing_white()) {
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 220, 220, 220);
    }
  }
#endif
}

void game_squares_guard_handle_command(const chess_move_command_t *cmd) {
  if (cmd == NULL || cmd->type != GAME_CMD_SQUARES_GUARD) {
    return;
  }

  uint8_t action = cmd->timer_data.squares_guard.action;
  if (action == 0) {
    game_squares_guard_restore_after_clear(true);
    STAGING_LOGI(TAG, "Matrix guard cleared");
    return;
  }

  if (!chess_policy_squares_guard_enabled()) {
    STAGING_LOGI(TAG, "Matrix guard command ignored (disabled in menuconfig)");
    squares_abort_ambiguous_guard_baseline();
    return;
  }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* In play, a multi-square anomaly must not freeze occupancy. The Hall
   * guide walks the pieces back to the last legal position. */
  if (hall_i2c_squares_readings_usable() &&
      game_hall_restore_guide_in_play() &&
      !game_task_squares_guard_mode_conflict_active() &&
      (current_game_state == GAME_STATE_ACTIVE ||
       current_game_state == GAME_STATE_PLAYING)) {
    squares_abort_ambiguous_guard_baseline();
    game_hall_position_enter_chaos_restore();
    if (game_hall_position_in_chaos_restore()) {
      game_squares_guard_enter_pause_from_hall_guide();
    }
    return;
  }
  if (hall_i2c_squares_readings_usable() &&
      game_hall_position_mismatch_count() <= 4) {
    STAGING_LOGI(TAG, "Matrix guard left to Hall restore (mismatch<=4)");
    squares_abort_ambiguous_guard_baseline();
    return;
  }
#endif

  if (game_task_squares_guard_mode_conflict_active()) {
    STAGING_LOGI(TAG,
                 "Matrix guard ignored (special mode) — clearing matrix local "
                 "guard so UP/DN continues");
    if (squares_guard_pause_state.active) {
      memset(&squares_guard_pause_state, 0, sizeof(squares_guard_pause_state));
      resync_required_after_restore = false;
    }
    squares_abort_ambiguous_guard_baseline();
    return;
  }

  squares_guard_pause_state.active = true;
  squares_guard_pause_state.lifted_mask_low =
      cmd->timer_data.squares_guard.lifted_mask_low;
  squares_guard_pause_state.lifted_mask_high =
      cmd->timer_data.squares_guard.lifted_mask_high;
  squares_guard_pause_state.dropped_mask_low =
      cmd->timer_data.squares_guard.dropped_mask_low;
  squares_guard_pause_state.dropped_mask_high =
      cmd->timer_data.squares_guard.dropped_mask_high;
  squares_guard_pause_state.conflict_count = game_count_mask_bits(
      squares_guard_pause_state.lifted_mask_low |
          squares_guard_pause_state.dropped_mask_low,
      squares_guard_pause_state.lifted_mask_high |
          squares_guard_pause_state.dropped_mask_high);

  if (chess_policy_squares_guard_should_freeze()) {
    game_task_squares_guard_freeze_move_flow();
  }

  uint8_t expected[64];
  game_board_to_occupancy(expected);
  squares_guard_apply_expected_occupancy(expected);

  STAGING_LOGI(TAG, "Matrix guard active, conflict_count=%u",
               squares_guard_pause_state.conflict_count);
  game_squares_guard_render_leds();
}

void game_squares_guard_check_resync_after_restore(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* Restoring a saved game must not paint blue on every unseen black home
   * and then refuse lifts until those homes read as occupied. */
  return;
#endif
  if (!chess_policy_squares_guard_enabled() ||
      !chess_policy_squares_guard_nvs_resync_enabled()) {
    return;
  }
  if (game_is_opening_trainer_active() || game_is_opening_trainer_setup_active()) {
    return;
  }
  uint8_t squares_state_now[64] = {0};
  squares_get_state(squares_state_now);

  uint32_t lifted_low = 0, lifted_high = 0;
  uint8_t conflicts = 0;
  for (uint8_t i = 0; i < 64; i++) {
    if (game_squares_sensor_matches_engine(i, squares_state_now)) {
      continue;
    }
    if (i < 32) {
      lifted_low |= (1UL << i);
    } else {
      lifted_high |= (1UL << (i - 32));
    }
    conflicts++;
  }

  if (conflicts == 0) {
    resync_required_after_restore = false;
    return;
  }

  squares_guard_pause_state.active = true;
  squares_guard_pause_state.lifted_mask_low = lifted_low;
  squares_guard_pause_state.lifted_mask_high = lifted_high;
  squares_guard_pause_state.dropped_mask_low = 0;
  squares_guard_pause_state.dropped_mask_high = 0;
  squares_guard_pause_state.conflict_count = conflicts;
  resync_required_after_restore = true;

  uint8_t expected[64];
  game_board_to_occupancy(expected);
  squares_guard_apply_expected_occupancy(expected);

  game_squares_guard_render_leds();
}

void game_squares_guard_try_clear_from_matrix(void) {
  if (!chess_policy_squares_guard_auto_clear_enabled()) {
    return;
  }
  if (!squares_guard_pause_state.active) {
    return;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_sync();
  if (game_hall_restore_guide_is_active()) {
    return;
  }
#endif
  uint8_t squares_state_now[64] = {0};
  squares_get_state(squares_state_now);
  for (uint8_t i = 0; i < 64; i++) {
    if (!game_squares_sensor_matches_engine(i, squares_state_now)) {
      return;
    }
  }

  game_squares_guard_restore_after_clear(true);
  STAGING_LOGI(TAG, "Matrix guard cleared (game + matrix layers synced)");
}

void game_squares_guard_enter_pause_from_hall_guide(void) {
  if (!chess_policy_squares_guard_enabled()) {
    return;
  }
  if (squares_guard_pause_state.active) {
    return;
  }
  squares_guard_pause_state.active = true;
  squares_guard_pause_state.lifted_mask_low = 0;
  squares_guard_pause_state.lifted_mask_high = 0;
  squares_guard_pause_state.dropped_mask_low = 0;
  squares_guard_pause_state.dropped_mask_high = 0;
  squares_guard_pause_state.conflict_count = 1;

  /* Do not call squares_guard_apply_expected_occupancy(). Occupancy often
   * already matches, and the scan would send action=0, which clears this
   * hold and the guide enters it again. */
  if (chess_policy_squares_guard_should_freeze()) {
    game_task_squares_guard_freeze_move_flow();
  }
  STAGING_LOGI(TAG, "Matrix guard entered from Hall restore guide");
}

bool game_squares_guard_release_hall_hold(void) {
  if (!squares_guard_pause_state.active) {
    return false;
  }
  if ((squares_guard_pause_state.lifted_mask_low |
       squares_guard_pause_state.lifted_mask_high |
       squares_guard_pause_state.dropped_mask_low |
       squares_guard_pause_state.dropped_mask_high) != 0) {
    return false;
  }
  memset(&squares_guard_pause_state, 0, sizeof(squares_guard_pause_state));
  return true;
}
