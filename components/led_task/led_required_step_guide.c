/**
 * @file led_required_step_guide.c
 * @brief Re-applies mandatory-step colors after droppable hint events.
 */

#include "led_required_step_guide.h"
#include "game_events.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"
#include "led_task.h"

#include <string.h>

#define REQ_STEP_FADE_MS 180U

static led_required_step_plan_t s_plan;
static bool s_need_push;
static uint32_t s_pushed_gen;

static void push_plan(void) {
  uint8_t i;
  if (!s_plan.active || s_plan.count == 0) {
    return;
  }
  if (s_plan.fade_ms != 0) {
    led_target_begin_fade(s_plan.fade_ms);
  } else {
    led_target_begin_fade(REQ_STEP_FADE_MS);
  }
  for (i = 0; i < s_plan.count && i < LED_REQUIRED_STEP_MAX_PX; i++) {
    const game_evt_hint_pixel_t *h = &s_plan.px[i];
    uint8_t row;
    uint8_t col;
    uint8_t led;
    if (h->square >= 64) {
      continue;
    }
    row = (uint8_t)(h->square / 8u);
    col = (uint8_t)(h->square % 8u);
    led = chess_pos_to_led_index(row, col);
    led_target_set(led, h->r, h->g, h->b, LED_VIS_SNAP);
  }
  led_target_end();
  s_need_push = false;
  s_pushed_gen = led_board_picture_gen();
}

void led_required_step_guide_apply(const led_required_step_plan_t *plan) {
  if (plan == NULL || !plan->active || plan->count == 0) {
    led_required_step_guide_clear();
    return;
  }
  bool overlay_blocks = led_gesture_anim_busy() || led_lamp_board_held();
  if (!overlay_blocks && s_plan.active && s_plan.count == plan->count &&
      s_plan.fade_ms == plan->fade_ms &&
      memcmp(s_plan.px, plan->px,
             (size_t)plan->count * sizeof(plan->px[0])) == 0 &&
      s_pushed_gen == led_board_picture_gen()) {
    return;
  }
  memcpy(&s_plan, plan, sizeof(s_plan));
  if (overlay_blocks) {
    s_need_push = true;
    return;
  }
  push_plan();
}

void led_required_step_guide_clear(void) {
  /* Do not patch squares black. The next full frame owns the picture. */
  memset(&s_plan, 0, sizeof(s_plan));
  s_need_push = false;
}

bool led_required_step_guide_is_active(void) { return s_plan.active; }

void led_required_step_guide_yield(void) {
  if (s_plan.active) {
    s_need_push = true;
  }
}

void led_required_step_guide_sync_mirror(void) {
  game_evt_hint_pixel_t px[LED_REQUIRED_STEP_MAX_PX];
  uint16_t fade_ms = 0;
  uint8_t n;
  led_required_step_plan_t plan;
  if (!game_event_required_step_mirror_active()) {
    return;
  }
  n = game_event_required_step_mirror_copy(px, LED_REQUIRED_STEP_MAX_PX, &fade_ms);
  if (n == 0) {
    return;
  }
  plan.active = true;
  plan.count = n;
  plan.fade_ms = fade_ms;
  memcpy(plan.px, px, (size_t)n * sizeof(px[0]));
  led_required_step_guide_apply(&plan);
}

void led_required_step_guide_tick(void) {
  if (led_gesture_anim_busy() || led_lamp_board_held()) {
    return;
  }
  if (!game_event_required_step_mirror_active()) {
    s_plan.active = false;
    s_plan.count = 0;
    s_need_push = false;
    return;
  }
  if (!s_plan.active) {
    led_required_step_guide_sync_mirror();
  }
  if (!s_need_push || !s_plan.active || led_target_is_open()) {
    return;
  }
  push_plan();
}
