/**
 * @file led_board_restore_guide.c
 * @brief Orange bar + bright pip animation for restore paths (LED task thread).
 */

#include "led_board_restore_guide.h"
#include "led_task.h"

#include "esp_timer.h"

#include <string.h>

#define RESTORE_ORANGE_R 255U
#define RESTORE_ORANGE_G 140U
#define RESTORE_ORANGE_B 0U

#define RESTORE_PIP_MS 140U
#define RESTORE_MISSING_MS 500U
#define RESTORE_EXTRA_MS 800U
#define RESTORE_SWAP_PULSE_MS 1200U
#define RESTORE_SWAP_FLOOR_R 22U
#define RESTORE_SWAP_FLOOR_G 10U
#define RESTORE_SWAP_FLOOR_B 0U

static led_restore_guide_plan_t s_plan;
static bool s_anim_active;
static uint32_t s_last_pip_ms;
static uint8_t s_pip_phase[LED_RESTORE_GUIDE_MAX_PATHS];
static uint32_t s_last_missing_ms;
static bool s_missing_bright;
static uint32_t s_last_extra_ms;
static bool s_extra_bright;

static uint8_t scale_channel(uint8_t v, uint8_t num, uint8_t den) {
  return (uint8_t)(((uint16_t)v * num) / den);
}

static void orange_dim(uint8_t *r, uint8_t *g, uint8_t *b) {
  uint8_t paths = s_plan.path_count;
  if (paths == 0) {
    paths = 1;
  }
  uint8_t den = (uint8_t)(paths + 1U);
  *r = scale_channel(RESTORE_ORANGE_R, 1U, den);
  *g = scale_channel(RESTORE_ORANGE_G, 1U, den);
  *b = scale_channel(RESTORE_ORANGE_B, 1U, den);
}

static void orange_bright(uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = RESTORE_ORANGE_R;
  *g = RESTORE_ORANGE_G;
  *b = RESTORE_ORANGE_B;
}

static void swap_corridor_color(uint32_t now_ms, uint8_t *r, uint8_t *g,
                                uint8_t *b) {
  uint32_t phase = now_ms % RESTORE_SWAP_PULSE_MS;
  uint32_t tri = phase * 2U;
  if (tri > RESTORE_SWAP_PULSE_MS) {
    tri = RESTORE_SWAP_PULSE_MS * 2U - tri;
  }
  uint8_t bright = (uint8_t)((tri * 255U) / RESTORE_SWAP_PULSE_MS);
  *r = (uint8_t)(RESTORE_SWAP_FLOOR_R +
                 ((uint16_t)(RESTORE_ORANGE_R - RESTORE_SWAP_FLOOR_R) * bright) /
                     255U);
  *g = (uint8_t)(RESTORE_SWAP_FLOOR_G +
                 ((uint16_t)(RESTORE_ORANGE_G - RESTORE_SWAP_FLOOR_G) * bright) /
                     255U);
  *b = (uint8_t)(RESTORE_SWAP_FLOOR_B +
                 ((uint16_t)(RESTORE_ORANGE_B - RESTORE_SWAP_FLOOR_B) * bright) /
                     255U);
}

static bool square_on_any_path(uint8_t sq) {
  for (uint8_t p = 0; p < s_plan.path_count; p++) {
    const led_restore_guide_path_t *path = &s_plan.paths[p];
    for (uint8_t i = 0; i < path->len; i++) {
      if (path->squares[i] == sq) {
        return true;
      }
    }
  }
  return false;
}

void led_board_restore_guide_apply(const led_restore_guide_plan_t *plan) {
  if (plan == NULL) {
    led_board_restore_guide_clear();
    return;
  }
  memcpy(&s_plan, plan, sizeof(s_plan));
  s_anim_active = plan->active;
  if (s_anim_active) {
    memset(s_pip_phase, 0, sizeof(s_pip_phase));
    s_last_pip_ms = esp_timer_get_time() / 1000U;
    s_last_missing_ms = s_last_pip_ms;
    s_last_extra_ms = s_last_pip_ms;
    s_missing_bright = true;
    s_extra_bright = true;
  }
}

void led_board_restore_guide_clear(void) {
  memset(&s_plan, 0, sizeof(s_plan));
  s_anim_active = false;
}

bool led_board_restore_guide_is_active(void) { return s_anim_active; }

void led_board_restore_guide_tick(void) {
  if (!s_anim_active || !s_plan.active) {
    return;
  }
  if (s_plan.path_count == 0 && s_plan.missing_count == 0 &&
      s_plan.extra_count == 0) {
    s_anim_active = false;
    return;
  }

  uint32_t now = esp_timer_get_time() / 1000U;

  if (now - s_last_pip_ms >= RESTORE_PIP_MS) {
    s_last_pip_ms = now;
    for (uint8_t p = 0; p < s_plan.path_count; p++) {
      if (s_plan.paths[p].len > 0 && !s_plan.paths[p].pulse_corridor) {
        s_pip_phase[p] =
            (uint8_t)((s_pip_phase[p] + 1U) % s_plan.paths[p].len);
      }
    }
  }

  if (now - s_last_missing_ms >= RESTORE_MISSING_MS) {
    s_last_missing_ms = now;
    s_missing_bright = !s_missing_bright;
  }

  if (now - s_last_extra_ms >= RESTORE_EXTRA_MS) {
    s_last_extra_ms = now;
    s_extra_bright = !s_extra_bright;
  }

  if (!s_anim_active || !s_plan.active) {
    return;
  }

  led_clear_board_only();

  uint8_t dr = 0;
  uint8_t dg = 0;
  uint8_t db = 0;
  orange_dim(&dr, &dg, &db);

  for (uint8_t p = 0; p < s_plan.path_count; p++) {
    const led_restore_guide_path_t *path = &s_plan.paths[p];
    for (uint8_t i = 0; i < path->len; i++) {
      uint8_t sq = path->squares[i];
      if (sq >= 64) {
        continue;
      }
      uint8_t r = dr;
      uint8_t g = dg;
      uint8_t b = db;
      if (path->pulse_corridor) {
        swap_corridor_color(now, &r, &g, &b);
      } else if (i == s_pip_phase[p]) {
        orange_bright(&r, &g, &b);
      }
      led_set_pixel_internal(sq, r, g, b);
    }
  }

  for (uint8_t m = 0; m < s_plan.missing_count; m++) {
    uint8_t sq = s_plan.missing_homes[m];
    if (sq >= 64 || square_on_any_path(sq)) {
      continue;
    }
    if (s_missing_bright) {
      led_set_pixel_internal(sq, RESTORE_ORANGE_R, RESTORE_ORANGE_G,
                             RESTORE_ORANGE_B);
    } else {
      led_set_pixel_internal(sq, dr, dg, db);
    }
  }

  for (uint8_t e = 0; e < s_plan.extra_count; e++) {
    uint8_t sq = s_plan.extra_squares[e];
    if (sq >= 64 || square_on_any_path(sq)) {
      continue;
    }
    if (s_extra_bright) {
      led_set_pixel_internal(sq, 255, 0, 0);
    } else {
      led_set_pixel_internal(sq, 40, 0, 0);
    }
  }

}
