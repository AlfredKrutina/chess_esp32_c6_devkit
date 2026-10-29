/**
 * @file led_board_restore_guide.c
 * @brief Per-piece setup glow. The LED task breathes the targets.
 */

#include "led_board_restore_guide.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"
#include "led_task.h"

#include <string.h>

#define EXTRA_LEVEL 120U
#define EXTRA_R 255U
#define EXTRA_G 36U
#define EXTRA_B 48U

/* HOME squares only. White barely moves; black swings more. Never black. */
#define GUIDE_BREATHE_PERIOD_MS 2400U
#define GUIDE_BREATHE_JITTER_MS 480U
#define GUIDE_WHITE_FLOOR 217U /* ~85% */
#define GUIDE_BLACK_FLOOR 89U  /* ~35% */
#define GUIDE_FADE_MS 360U

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t level;
} guide_rgb_t;

static led_restore_guide_plan_t s_plan;
static uint64_t s_mask;
static bool s_need_push;
static uint32_t s_pushed_gen;

static uint8_t square_to_led(uint8_t sq) {
  return chess_pos_to_led_index((uint8_t)(sq / 8u), (uint8_t)(sq % 8u));
}

static uint32_t guide_hash(uint8_t sq) {
  return (uint32_t)sq * 37U + ((uint32_t)sq >> 3) * 11U;
}

static uint16_t guide_period_ms(uint8_t sq) {
  uint32_t span = GUIDE_BREATHE_JITTER_MS;
  uint32_t base = GUIDE_BREATHE_PERIOD_MS - span / 2U;
  return (uint16_t)(base + (guide_hash(sq) % (span + 1U)));
}

static uint16_t guide_phase_ms(uint8_t sq, uint16_t period) {
  if (period == 0U) {
    return 0U;
  }
  return (uint16_t)(guide_hash(sq) % period);
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

static void push_plan(void) {
  uint8_t i;
  s_mask = 0;
  led_target_begin_fade(GUIDE_FADE_MS);
  for (i = 0; i < s_plan.mark_count; i++) {
    const led_restore_guide_mark_t *mark = &s_plan.marks[i];
    guide_rgb_t rgb;
    uint8_t level;
    uint8_t led;
    uint8_t r, g, b;
    if (mark->square >= 64) {
      continue;
    }
    rgb = color_for_piece(mark->type, mark->color);
    level = rgb.level;
    if (mark->role == LED_RESTORE_MARK_EXTRA) {
      rgb.r = EXTRA_R;
      rgb.g = EXTRA_G;
      rgb.b = EXTRA_B;
      level = EXTRA_LEVEL;
    }
    led = square_to_led(mark->square);
    if (led >= 64) {
      continue;
    }
    scale_rgb(rgb, level, &r, &g, &b);
    s_mask |= (uint64_t)1 << led;
    if (mark->role == LED_RESTORE_MARK_HOME) {
      uint16_t period = guide_period_ms(mark->square);
      uint8_t floor =
          (mark->color == 2U) ? GUIDE_BLACK_FLOOR : GUIDE_WHITE_FLOOR;
      led_target_set_breathe_sq(led, r, g, b, floor, period,
                                guide_phase_ms(mark->square, period),
                                GUIDE_FADE_MS);
    } else {
      led_target_set_breathe_sq(led, r, g, b, 0, 0, 0, GUIDE_FADE_MS);
    }
  }
  led_target_end();
  s_need_push = false;
}

void led_board_restore_guide_apply(const led_restore_guide_plan_t *plan) {
  if (plan == NULL || !plan->active || plan->mark_count == 0) {
    led_board_restore_guide_clear();
    return;
  }
  bool overlay_blocks = led_gesture_anim_busy() || led_lamp_board_held();
  if (!overlay_blocks && s_plan.active && s_plan.mark_count == plan->mark_count &&
      memcmp(s_plan.marks, plan->marks,
             (size_t)plan->mark_count * sizeof(plan->marks[0])) == 0 &&
      s_pushed_gen == led_board_picture_gen()) {
    return;
  }
  memcpy(&s_plan, plan, sizeof(s_plan));
  if (overlay_blocks) {
    s_need_push = true;
    return;
  }
  push_plan();
  s_pushed_gen = led_board_picture_gen();
}

void led_board_restore_guide_blank(void) {
  memset(&s_plan, 0, sizeof(s_plan));
  s_mask = 0U;
  s_need_push = false;
  led_target_begin_fade(GUIDE_FADE_MS);
  led_target_end();
  s_pushed_gen = led_board_picture_gen();
}

void led_board_restore_guide_fade_out(void) {
  uint64_t mask;
  uint8_t i;
  if (!s_plan.active && s_mask == 0U) {
    return;
  }
  s_plan.active = false;
  s_plan.mark_count = 0;
  s_need_push = false;
  /* A caller picture (lift, idle) owns the frame. Do not fade under it. */
  if (led_target_is_open()) {
    s_mask = 0U;
    return;
  }
  mask = s_mask;
  s_mask = 0U;
  for (i = 0; i < 64U; i++) {
    if ((mask & ((uint64_t)1 << i)) == 0U) {
      continue;
    }
    led_target_set_breathe_sq(i, 0, 0, 0, 0, 0, 0, GUIDE_FADE_MS);
  }
}

void led_board_restore_guide_clear(void) {
  led_board_restore_guide_fade_out();
}

bool led_board_restore_guide_is_active(void) { return s_plan.active; }

void led_board_restore_guide_yield(void) {
  if (s_plan.active) {
    s_need_push = true;
  }
}

void led_board_restore_guide_cancel(void) {
  memset(&s_plan, 0, sizeof(s_plan));
  s_mask = 0U;
  s_need_push = false;
}

void led_board_restore_guide_tick(void) {
  if (led_gesture_anim_busy() || led_lamp_board_held()) {
    return;
  }
  if (!s_need_push || !s_plan.active || led_target_is_open()) {
    return;
  }
  push_plan();
}
