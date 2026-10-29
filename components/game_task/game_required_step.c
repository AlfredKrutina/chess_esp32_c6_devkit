/**
 * @file game_required_step.c
 * @brief Keeps mandatory-step LEDs alive and times out stuck latches.
 */

#include "game_required_step.h"
#include "game_task.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_hall_restore_guide.h"
#include "game_policy.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <string.h>

static const char *TAG = "GAME_REQ_STEP";

/** Idle limit before abandoning a stuck mandatory step (plan: game or restore). */
#define REQUIRED_STEP_IDLE_MS 120000U
#define REQUIRED_STEP_REPOST_MS 400U

static uint32_t s_step_since_ms;
static uint32_t s_last_repost_ms;
static bool s_timeout_armed;

static void add_px(game_evt_hint_pixel_t *px, uint8_t *n, uint8_t row,
                   uint8_t col, uint8_t r, uint8_t g, uint8_t b) {
  if (*n >= GAME_EVT_HINT_MAX || row >= 8 || col >= 8) {
    return;
  }
  px[*n].square = (uint8_t)(row * 8u + col);
  px[*n].r = r;
  px[*n].g = g;
  px[*n].b = b;
  (*n)++;
}

static bool guided_attacker_lifted(void) {
  if (!guided_capture_state.active || !piece_lifted ||
      lifted_piece == PIECE_EMPTY) {
    return false;
  }
  for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
    if (guided_capture_state.attacker_rows[i] == lifted_piece_row &&
        guided_capture_state.attacker_cols[i] == lifted_piece_col &&
        guided_capture_state.attacker_pieces[i] == lifted_piece) {
      return true;
    }
  }
  return false;
}

static uint8_t build_pixels(game_evt_hint_pixel_t *px) {
  uint8_t n = 0;

  if (capture_in_progress && piece_lifted && lifted_piece != PIECE_EMPTY) {
    add_px(px, &n, lifted_piece_row, lifted_piece_col, 255, 255, 0);
    add_px(px, &n, capture_target_row, capture_target_col, 128, 0, 128);
    return n;
  }
  if (guided_capture_state.active) {
    if (guided_attacker_lifted()) {
      add_px(px, &n, guided_capture_state.target_row,
             guided_capture_state.target_col, 128, 0, 128);
      return n;
    }
    add_px(px, &n, guided_capture_state.target_row,
           guided_capture_state.target_col, 128, 0, 128);
    if (game_get_guided_capture_hints_enabled() ||
        game_led_guidance_show_movable_yellow()) {
      for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
        add_px(px, &n, guided_capture_state.attacker_rows[i],
               guided_capture_state.attacker_cols[i], 255, 255, 0);
      }
    }
    return n;
  }
  if (castling_state.in_progress) {
    add_px(px, &n, castling_state.rook_from_row, castling_state.rook_from_col,
           255, 255, 0);
    add_px(px, &n, castling_state.rook_to_row, castling_state.rook_to_col, 0,
           255, 0);
    return n;
  }
  if (game_ep_capture_waiting()) {
    uint8_t er = 0;
    uint8_t ec = 0;
    if (game_ep_capture_victim_square(&er, &ec)) {
      add_px(px, &n, er, ec, 255, 80, 0);
    }
    return n;
  }
  if ((*game_state_promotion_state()).pending) {
    uint8_t row = (*game_state_promotion_state()).square_row;
    uint8_t col = (*game_state_promotion_state()).square_col;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    float t = (float)(now_ms % 1000U) / 1000.0f;
    float mix = 0.5f + 0.5f * sinf(t * 2.0f * 3.14159f);
    uint8_t r = (uint8_t)(128.0f * (1.0f - mix) + 255.0f * mix);
    uint8_t g = (uint8_t)(0.0f * (1.0f - mix) + 215.0f * mix);
    uint8_t b = (uint8_t)(128.0f * (1.0f - mix) + 0.0f * mix);
    add_px(px, &n, row, col, r, g, b);
    return n;
  }
  return 0;
}

bool game_required_step_blocks_restore(void) {
  return capture_in_progress || guided_capture_state.active ||
         castling_state.in_progress || game_ep_capture_waiting() ||
         (*game_state_promotion_state()).pending;
}

void game_required_step_clear(void) {
  s_timeout_armed = false;
  s_step_since_ms = 0;
  s_last_repost_ms = 0;
  game_event_post_required_step_clear();
}

static void arm_idle_timer(void) {
  if (!s_timeout_armed) {
    s_step_since_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s_timeout_armed = true;
  }
}

void game_required_step_refresh(void) {
  game_evt_hint_pixel_t px[GAME_EVT_HINT_MAX];
  uint8_t n = build_pixels(px);
  if (n == 0) {
    game_required_step_clear();
    return;
  }
  arm_idle_timer();
  game_event_post_required_step_guide(px, n, 180U);
}

static void timeout_abandon(void) {
  ESP_LOGW(TAG, "mandatory step idle timeout — entering restore");
  if (capture_in_progress) {
    capture_in_progress = false;
    capture_target_row = 0;
    capture_target_col = 0;
    capture_removed_piece = PIECE_EMPTY;
  }
  if (guided_capture_state.active) {
    game_reset_guided_capture_state();
  }
  if (castling_state.in_progress) {
    memset(&castling_state, 0, sizeof(castling_state));
  }
  if (game_ep_capture_waiting()) {
    game_ep_capture_reset();
  }
  game_board_set_piece_lifted(false, 0, 0, PIECE_EMPTY);
  game_required_step_clear();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* In-play plan against the committed board. Lamp hold returns false;
   * yield on release redraws later. Do not clear s_opening_done. */
  (void)game_hall_restore_guide_show_play();
#endif
  s_timeout_armed = false;
}

void game_required_step_service(void) {
  if (!game_required_step_blocks_restore()) {
    if (s_timeout_armed) {
      game_required_step_clear();
    }
    return;
  }

  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  if (s_timeout_armed && (*game_state_promotion_state()).pending) {
    /* Promotion choice may take longer; do not drop the pending latch. */
  } else if (s_timeout_armed &&
             (uint32_t)(now_ms - s_step_since_ms) >= REQUIRED_STEP_IDLE_MS) {
    timeout_abandon();
    return;
  }

  if (s_last_repost_ms == 0 ||
      (uint32_t)(now_ms - s_last_repost_ms) >= REQUIRED_STEP_REPOST_MS) {
    s_last_repost_ms = now_ms;
    game_required_step_refresh();
  }
}
