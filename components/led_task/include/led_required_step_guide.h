/**
 * @file led_required_step_guide.h
 * @brief Persistent LED overlay while a mandatory physical step is pending.
 */

#ifndef LED_REQUIRED_STEP_GUIDE_H
#define LED_REQUIRED_STEP_GUIDE_H

#include "game_events.h"
#include <stdbool.h>
#include <stdint.h>

#define LED_REQUIRED_STEP_MAX_PX GAME_EVT_HINT_MAX

typedef struct {
  bool active;
  uint8_t count;
  game_evt_hint_pixel_t px[LED_REQUIRED_STEP_MAX_PX];
  uint16_t fade_ms;
} led_required_step_plan_t;

void led_required_step_guide_apply(const led_required_step_plan_t *plan);
void led_required_step_guide_clear(void);
bool led_required_step_guide_is_active(void);
void led_required_step_guide_yield(void);
void led_required_step_guide_tick(void);
void led_required_step_guide_sync_mirror(void);

#endif /* LED_REQUIRED_STEP_GUIDE_H */
