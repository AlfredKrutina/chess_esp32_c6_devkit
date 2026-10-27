/**
 * @file led_board_restore_guide.c
 * @brief Per-piece setup glow. Empty homes stay at one steady level.
 */

#include "led_board_restore_guide.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"
#include "led_task.h"

#include "esp_timer.h"

#include <string.h>

#define EXTRA_LEVEL 120U
#define EXTRA_R 255U
#define EXTRA_G 36U
#define EXTRA_B 48U

#define REVEAL_MS 420U

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t level;
} guide_rgb_t;

static led_restore_guide_plan_t s_plan;
static bool s_anim_active;
static uint8_t s_gain[64];
static uint8_t s_on[64];
static uint8_t s_want[64];
static guide_rgb_t s_hold[64];
static guide_rgb_t s_sent[64];
static uint32_t s_last_ms;

static uint8_t square_to_led(uint8_t sq) {
  return chess_pos_to_led_index((uint8_t)(sq / 8u), (uint8_t)(sq % 8u));
}

static guide_rgb_t color_for_piece(uint8_t type, uint8_t color) {
  /* Hue stays saturated. The fourth number is that color's own brightness. */
  static const guide_rgb_t white_pieces[7] = {
      {0, 0, 0, 0},
      {0, 255, 0, 120},
      {255, 80, 0, 170},
      {0, 90, 255, 200},
      {255, 0, 160, 150},
      {255, 255, 230, 90},
      {255, 220, 0, 140},
  };
  static const guide_rgb_t black_pieces[7] = {
      {0, 0, 0, 0},
      {0, 40, 255, 90},
      {130, 0, 255, 80},
      {255, 0, 50, 70},
      {0, 170, 40, 48},
      {30, 0, 140, 110},
      {255, 50, 0, 64},
  };
  if (type < 1 || type > 6) {
    guide_rgb_t red = {EXTRA_R, EXTRA_G, EXTRA_B, EXTRA_LEVEL};
    return red;
  }
  if (color == 2) {
    return black_pieces[type];
  }
  return white_pieces[type];
}

static void scale_rgb(guide_rgb_t src, uint8_t level, uint8_t *r, uint8_t *g,
                      uint8_t *b) {
  *r = (uint8_t)(((uint16_t)src.r * level) / 255U);
  *g = (uint8_t)(((uint16_t)src.g * level) / 255U);
  *b = (uint8_t)(((uint16_t)src.b * level) / 255U);
}

static void paint_square(uint8_t sq) {
  uint8_t gain = s_gain[sq];
  uint8_t r = (uint8_t)(((uint16_t)s_hold[sq].r * gain) / 255U);
  uint8_t g = (uint8_t)(((uint16_t)s_hold[sq].g * gain) / 255U);
  uint8_t b = (uint8_t)(((uint16_t)s_hold[sq].b * gain) / 255U);
  if (s_sent[sq].r == r && s_sent[sq].g == g && s_sent[sq].b == b &&
      (s_on[sq] || (r == 0U && g == 0U && b == 0U))) {
    return;
  }
  s_sent[sq].r = r;
  s_sent[sq].g = g;
  s_sent[sq].b = b;
  led_set_pixel_internal(square_to_led(sq), r, g, b);
  s_on[sq] = (gain > 0U) ? 1U : 0U;
}

static unsigned reveal_step(uint32_t now) {
  uint32_t dt = (s_last_ms == 0U) ? 33U : (now - s_last_ms);
  s_last_ms = now;
  if (dt == 0U || dt > 80U) {
    dt = 33U;
  }
  unsigned step = (unsigned)((dt * 255U) / REVEAL_MS);
  if (step < 1U) {
    step = 1U;
  }
  return step;
}

void led_board_restore_guide_apply(const led_restore_guide_plan_t *plan) {
  if (plan == NULL || !plan->active || plan->mark_count == 0) {
    led_board_restore_guide_clear();
    return;
  }
  if (s_anim_active && s_plan.active &&
      s_plan.mark_count == plan->mark_count &&
      memcmp(s_plan.marks, plan->marks,
             (size_t)plan->mark_count * sizeof(plan->marks[0])) == 0) {
    return;
  }
  bool fresh = !s_anim_active;
  memcpy(&s_plan, plan, sizeof(s_plan));
  s_anim_active = true;
  if (fresh) {
    s_last_ms = 0;
  }
}

void led_board_restore_guide_fade_out(void) {
  if (!s_anim_active && !s_plan.active) {
    return;
  }
  s_plan.active = false;
  s_plan.mark_count = 0;
}

void led_board_restore_guide_clear(void) {
  led_board_restore_guide_fade_out();
}

bool led_board_restore_guide_is_active(void) { return s_anim_active; }

void led_board_restore_guide_yield(void) {
  memset(s_sent, 0xFF, sizeof(s_sent));
  memset(s_on, 0, sizeof(s_on));
}

void led_board_restore_guide_cancel(void) {
  memset(&s_plan, 0, sizeof(s_plan));
  memset(s_gain, 0, sizeof(s_gain));
  memset(s_on, 0, sizeof(s_on));
  memset(s_want, 0, sizeof(s_want));
  memset(s_sent, 0xFF, sizeof(s_sent));
  s_anim_active = false;
  s_last_ms = 0;
}

void led_board_restore_guide_tick(void) {
  if (led_gesture_anim_busy() || led_lamp_board_held()) {
    return;
  }
  if (!s_plan.active || s_plan.mark_count == 0) {
    if (!s_anim_active) {
      return;
    }
    uint32_t now = esp_timer_get_time() / 1000U;
    unsigned step = reveal_step(now);
    bool any = false;
    for (uint8_t sq = 0; sq < 64U; sq++) {
      if (s_gain[sq] > 0U) {
        unsigned gain = (unsigned)s_gain[sq];
        s_gain[sq] = (gain > step) ? (uint8_t)(gain - step) : 0U;
        paint_square(sq);
        if (s_gain[sq] > 0U) {
          any = true;
        }
      } else if (s_on[sq]) {
        led_set_pixel_internal(square_to_led(sq), 0, 0, 0);
        s_on[sq] = 0U;
      }
    }
    if (!any) {
      s_anim_active = false;
    }
    return;
  }
  s_anim_active = true;

  uint32_t now = esp_timer_get_time() / 1000U;
  unsigned step = reveal_step(now);
  memset(s_want, 0, sizeof(s_want));

  for (uint8_t i = 0; i < s_plan.mark_count; i++) {
    const led_restore_guide_mark_t *mark = &s_plan.marks[i];
    if (mark->square >= 64) {
      continue;
    }
    guide_rgb_t rgb = color_for_piece(mark->type, mark->color);
    uint8_t level = rgb.level;
    if (mark->role == LED_RESTORE_MARK_EXTRA) {
      rgb.r = EXTRA_R;
      rgb.g = EXTRA_G;
      rgb.b = EXTRA_B;
      level = EXTRA_LEVEL;
    }
    uint8_t sq = mark->square;
    s_want[sq] = 1U;
    scale_rgb(rgb, level, &s_hold[sq].r, &s_hold[sq].g, &s_hold[sq].b);
  }

  for (uint8_t sq = 0; sq < 64U; sq++) {
    if (s_want[sq]) {
      unsigned gain = (unsigned)s_gain[sq] + step;
      s_gain[sq] = (gain > 255U) ? 255U : (uint8_t)gain;
      paint_square(sq);
    } else if (s_gain[sq] > 0U) {
      unsigned gain = (unsigned)s_gain[sq];
      s_gain[sq] = (gain > step) ? (uint8_t)(gain - step) : 0U;
      paint_square(sq);
    } else if (s_on[sq]) {
      led_set_pixel_internal(square_to_led(sq), 0, 0, 0);
      s_on[sq] = 0U;
    }
  }
}
