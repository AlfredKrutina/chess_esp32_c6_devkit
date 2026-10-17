/**
 * @file led_board_restore_guide.c
 * @brief Per-piece setup glow. Homes breathe in piece color, no path bars.
 */

#include "led_board_restore_guide.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"
#include "led_task.h"

#include "esp_timer.h"

#include <string.h>

#define WHITE_FLOOR 170U
#define WHITE_HOME_PEAK 255U
#define WHITE_SIT_PEAK 215U
#define BLACK_FLOOR 16U
#define BLACK_HOME_PEAK 120U
#define BLACK_SIT_PEAK 64U
#define EXTRA_R 255U
#define EXTRA_G 36U
#define EXTRA_B 48U

#define REVEAL_MS 420U

typedef struct {
  uint8_t r;
  uint8_t g;
  uint8_t b;
} guide_rgb_t;

static led_restore_guide_plan_t s_plan;
static bool s_anim_active;
static uint8_t s_gain[64];
static uint8_t s_on[64];
static uint8_t s_want[64];
static guide_rgb_t s_hold[64];
static uint32_t s_last_ms;

static uint8_t square_to_led(uint8_t sq) {
  return chess_pos_to_led_index((uint8_t)(sq / 8u), (uint8_t)(sq % 8u));
}

static uint32_t period_for_type(uint8_t type) {
  switch (type) {
  case 1:
    return 2000U;
  case 2:
    return 2400U;
  case 3:
    return 2600U;
  case 4:
    return 2300U;
  case 5:
    return 2800U;
  case 6:
    return 3200U;
  default:
    return 2200U;
  }
}

static guide_rgb_t color_for_piece(uint8_t type, uint8_t color) {
  /* White hues stay saturated so pawn, knight and bishop stay apart.
   * Black uses another hue and stays dark. */
  static const guide_rgb_t white_pieces[7] = {
      {0, 0, 0},       {0, 255, 0},     {255, 80, 0},    {0, 90, 255},
      {255, 0, 160},   {255, 255, 230}, {255, 220, 0},
  };
  static const guide_rgb_t black_pieces[7] = {
      {0, 0, 0},       {0, 40, 255},   {130, 0, 255},  {255, 0, 50},
      {0, 170, 40},    {30, 0, 140},   {255, 50, 0},
  };
  if (type < 1 || type > 6) {
    guide_rgb_t red = {EXTRA_R, EXTRA_G, EXTRA_B};
    return red;
  }
  if (color == 2) {
    return black_pieces[type];
  }
  return white_pieces[type];
}

static uint8_t smoothstep_u8(uint8_t x) {
  uint32_t v = (uint32_t)x;
  uint32_t s = (v * v * (765U - 2U * v)) / (255U * 255U);
  return (uint8_t)s;
}

static uint8_t breathe_level(uint32_t now_ms, uint32_t period_ms, uint32_t phase_ms,
                             uint8_t floor_lv, uint8_t peak_lv) {
  if (period_ms < 2U) {
    return peak_lv;
  }
  uint32_t t = (now_ms + phase_ms) % period_ms;
  uint32_t half = period_ms / 2U;
  uint32_t up = (t < half) ? t : (period_ms - t);
  uint8_t x = (uint8_t)((up * 255U) / half);
  uint8_t s = smoothstep_u8(x);
  return (uint8_t)(floor_lv + ((uint16_t)(peak_lv - floor_lv) * s) / 255U);
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
  bool fresh = !s_anim_active;
  memcpy(&s_plan, plan, sizeof(s_plan));
  s_anim_active = true;
  if (fresh) {
    memset(s_gain, 0, sizeof(s_gain));
    memset(s_on, 0, sizeof(s_on));
    memset(s_hold, 0, sizeof(s_hold));
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
  bool was_lit = s_anim_active || s_plan.active;
  s_plan.active = false;
  s_plan.mark_count = 0;
  s_anim_active = false;
  memset(s_gain, 0, sizeof(s_gain));
  memset(s_on, 0, sizeof(s_on));
  if (was_lit) {
    led_clear_board_only();
  }
}

bool led_board_restore_guide_is_active(void) { return s_anim_active; }

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
    guide_rgb_t rgb;
    uint32_t period;
    uint32_t phase;
    uint8_t peak;
    if (mark->role == LED_RESTORE_MARK_EXTRA) {
      rgb.r = EXTRA_R;
      rgb.g = EXTRA_G;
      rgb.b = EXTRA_B;
      period = 1800U;
      phase = (uint32_t)mark->square * 70U;
      peak = 220U;
    } else {
      rgb = color_for_piece(mark->type, mark->color);
      period = period_for_type(mark->type);
      phase = (uint32_t)mark->square * 90U;
      if (mark->color == 2) {
        peak = (mark->role == LED_RESTORE_MARK_SITTING) ? BLACK_SIT_PEAK
                                                        : BLACK_HOME_PEAK;
      } else {
        peak = (mark->role == LED_RESTORE_MARK_SITTING) ? WHITE_SIT_PEAK
                                                        : WHITE_HOME_PEAK;
      }
      if (mark->role == LED_RESTORE_MARK_SITTING) {
        phase += period / 2U;
      }
    }
    uint8_t floor_lv;
    if (mark->role == LED_RESTORE_MARK_EXTRA) {
      floor_lv = 40U;
    } else if (mark->color == 2) {
      floor_lv = BLACK_FLOOR;
    } else {
      floor_lv = WHITE_FLOOR;
    }
    uint8_t level = breathe_level(now, period, phase, floor_lv, peak);
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
