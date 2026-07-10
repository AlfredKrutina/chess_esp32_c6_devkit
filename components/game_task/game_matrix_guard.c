/**
 * @file game_matrix_guard.c
 * @brief Matrix guard — LED feedback and pause when sensors disagree with logic.
 */

#include "game_matrix_guard.h"
#include "game_task_internal.h"

#include "freertos_chess.h"
#include "game_task.h"
#include "led_task.h"
#include "matrix_task.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "GAME_MATRIX_GUARD";

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
} matrix_guard_pause_state_t;

static matrix_guard_pause_state_t matrix_guard_pause_state = {0};

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

bool game_is_matrix_guard_active(void) { return matrix_guard_pause_state.active; }

uint8_t game_get_matrix_guard_conflict_count(void) {
  return matrix_guard_pause_state.conflict_count;
}

uint32_t game_get_matrix_guard_lifted_mask_low(void) {
  return matrix_guard_pause_state.lifted_mask_low;
}

uint32_t game_get_matrix_guard_lifted_mask_high(void) {
  return matrix_guard_pause_state.lifted_mask_high;
}

uint32_t game_get_matrix_guard_dropped_mask_low(void) {
  return matrix_guard_pause_state.dropped_mask_low;
}

uint32_t game_get_matrix_guard_dropped_mask_high(void) {
  return matrix_guard_pause_state.dropped_mask_high;
}

void game_force_clear_matrix_guard(void) {
  if (!matrix_guard_pause_state.active && !matrix_is_guard_mode_active()) {
    return;
  }
  game_matrix_guard_restore_after_clear(true);
  STAGING_LOGI(TAG, "Matrix guard force-cleared (game + matrix)");
}

void game_matrix_guard_clear_both_layers(void) {
  matrix_guard_pause_state.active = false;
  matrix_guard_pause_state.lifted_mask_low = 0;
  matrix_guard_pause_state.lifted_mask_high = 0;
  matrix_guard_pause_state.dropped_mask_low = 0;
  matrix_guard_pause_state.dropped_mask_high = 0;
  matrix_guard_pause_state.conflict_count = 0;
  resync_required_after_restore = false;
  matrix_abort_ambiguous_guard_baseline();
}

void game_matrix_guard_restore_after_clear(bool notify_clients) {
  game_matrix_guard_clear_both_layers();
  led_clear_board_only();
  game_highlight_movable_pieces();
  if (notify_clients) {
    game_bump_revision_and_notify();
  }
}

void game_matrix_guard_render_leds(void) {
  if (!matrix_guard_pause_state.active) {
    return;
  }

  led_clear_board_only();
  uint8_t phys[64] = {0};
  matrix_get_state(phys);

  for (uint8_t square = 0; square < 64; square++) {
    uint8_t row = square / 8;
    uint8_t col = square % 8;
    piece_t piece = board[row][col];
    uint8_t expected_occ = (piece == PIECE_EMPTY) ? 0 : 1;
    if (phys[square] == expected_occ) {
      continue;
    }

    if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0);
    } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0, 255);
    } else if (phys[square] != 0) {
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 140, 0);
    } else {
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 220, 220, 220);
    }
  }
}

void game_matrix_guard_handle_command(const chess_move_command_t *cmd) {
  if (cmd == NULL || cmd->type != GAME_CMD_MATRIX_GUARD) {
    return;
  }

  uint8_t action = cmd->timer_data.matrix_guard.action;
  if (action == 0) {
    game_matrix_guard_restore_after_clear(true);
    STAGING_LOGI(TAG, "Matrix guard cleared");
    return;
  }

  if (game_task_matrix_guard_mode_conflict_active()) {
    STAGING_LOGI(TAG,
                 "Matrix guard ignored (special mode) — clearing matrix local "
                 "guard so UP/DN continues");
    if (matrix_guard_pause_state.active) {
      memset(&matrix_guard_pause_state, 0, sizeof(matrix_guard_pause_state));
      resync_required_after_restore = false;
    }
    matrix_abort_ambiguous_guard_baseline();
    return;
  }

  matrix_guard_pause_state.active = true;
  matrix_guard_pause_state.lifted_mask_low =
      cmd->timer_data.matrix_guard.lifted_mask_low;
  matrix_guard_pause_state.lifted_mask_high =
      cmd->timer_data.matrix_guard.lifted_mask_high;
  matrix_guard_pause_state.dropped_mask_low =
      cmd->timer_data.matrix_guard.dropped_mask_low;
  matrix_guard_pause_state.dropped_mask_high =
      cmd->timer_data.matrix_guard.dropped_mask_high;
  matrix_guard_pause_state.conflict_count = game_count_mask_bits(
      matrix_guard_pause_state.lifted_mask_low |
          matrix_guard_pause_state.dropped_mask_low,
      matrix_guard_pause_state.lifted_mask_high |
          matrix_guard_pause_state.dropped_mask_high);

  game_task_matrix_guard_freeze_move_flow();

  uint8_t expected[64];
  game_board_to_occupancy(expected);
  matrix_guard_apply_expected_occupancy(expected);

  STAGING_LOGI(TAG, "Matrix guard active, conflict_count=%u",
               matrix_guard_pause_state.conflict_count);
  game_matrix_guard_render_leds();
}

void game_matrix_guard_check_resync_after_restore(void) {
  uint8_t matrix_state_now[64] = {0};
  matrix_get_state(matrix_state_now);

  uint32_t lifted_low = 0, lifted_high = 0;
  uint8_t conflicts = 0;
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t expected = (((uint8_t)board[i / 8][i % 8]) == PIECE_EMPTY) ? 0 : 1;
    if (matrix_state_now[i] != expected) {
      if (i < 32) {
        lifted_low |= (1UL << i);
      } else {
        lifted_high |= (1UL << (i - 32));
      }
      conflicts++;
    }
  }

  if (conflicts == 0) {
    resync_required_after_restore = false;
    return;
  }

  matrix_guard_pause_state.active = true;
  matrix_guard_pause_state.lifted_mask_low = lifted_low;
  matrix_guard_pause_state.lifted_mask_high = lifted_high;
  matrix_guard_pause_state.dropped_mask_low = 0;
  matrix_guard_pause_state.dropped_mask_high = 0;
  matrix_guard_pause_state.conflict_count = conflicts;
  resync_required_after_restore = true;

  uint8_t expected[64];
  game_board_to_occupancy(expected);
  matrix_guard_apply_expected_occupancy(expected);

  game_matrix_guard_render_leds();
}

void game_matrix_guard_try_clear_from_matrix(void) {
  if (!matrix_guard_pause_state.active) {
    return;
  }
  uint8_t matrix_state_now[64] = {0};
  matrix_get_state(matrix_state_now);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t expected = (((uint8_t)board[i / 8][i % 8]) == PIECE_EMPTY) ? 0 : 1;
    if (matrix_state_now[i] != expected) {
      return;
    }
  }

  game_matrix_guard_restore_after_clear(true);
  STAGING_LOGI(TAG, "Matrix guard cleared (game + matrix layers synced)");
}
