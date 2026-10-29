/**
 * @file led_game_events.c
 * @brief Draw session game events on the board LEDs.
 */

#include "game_events.h"
#include "freertos_chess.h"
#include "led_board_restore_guide.h"
#include "led_required_step_guide.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"
#include "led_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static uint8_t s_error_led = 255U;

static void apply_restore(const game_event_t *ev) {
  switch (ev->restore_op) {
  case GAME_EVT_RESTORE_OP_APPLY:
    led_board_restore_guide_apply((const led_restore_guide_plan_t *)&ev->restore);
    game_led_board_picture_bump();
    break;
  case GAME_EVT_RESTORE_OP_CLEAR:
    led_board_restore_guide_clear();
    break;
  case GAME_EVT_RESTORE_OP_CANCEL:
    led_board_restore_guide_cancel();
    break;
  case GAME_EVT_RESTORE_OP_FADE_OUT:
    led_board_restore_guide_fade_out();
    break;
  case GAME_EVT_RESTORE_OP_BLANK:
    led_board_restore_guide_blank();
    break;
  case GAME_EVT_RESTORE_OP_TAKEBACK: {
    uint8_t leds[8];
    uint8_t n = 0;
    uint8_t i;
    if (ev->takeback_count == 0) {
      led_takeback_clear();
      return;
    }
    led_target_begin();
    for (i = 0; i < ev->takeback_count && n < 8; i++) {
      uint8_t sq = ev->takeback_squares[i];
      uint8_t led;
      if (sq >= 64) {
        continue;
      }
      led = chess_pos_to_led_index(sq / 8, sq % 8);
      leds[n++] = led;
      led_target_set(led, 0, 255, 255, LED_VIS_BREATHE);
    }
    led_target_end();
    led_takeback_arm(leds, n);
    break;
  }
  default:
    break;
  }
}

static void paint_square(uint8_t square, uint8_t r, uint8_t g, uint8_t b,
                         led_vis_mode_t mode) {
  uint8_t row;
  uint8_t col;
  if (square >= 64) {
    return;
  }
  row = (uint8_t)(square / 8);
  col = (uint8_t)(square % 8);
  led_target_set(chess_pos_to_led_index(row, col), r, g, b, mode);
}

static void apply_hints(const game_event_t *ev) {
  uint8_t i;
  bool required = (ev->flags & GAME_EVT_HINT_REQUIRED_STEP) != 0;
  if (required && ev->hint_lift_sq == 254U && ev->square_count == 0) {
    led_required_step_guide_clear();
    return;
  }
  bool patch = (ev->flags & GAME_EVT_HINT_PATCH) != 0;
  if (!patch && !required) {
    led_required_step_guide_yield();
  }
  if (!patch) {
    led_move_trail_cancel();
    if (ev->fade_ms != 0) {
      led_target_begin_fade(ev->fade_ms);
    } else {
      led_target_begin();
    }
  }
  if (ev->hint_lift_sq < 64) {
    paint_square(ev->hint_lift_sq, 255, 255, 0, LED_VIS_SNAP);
  }
  for (i = 0; i < ev->square_count && i < GAME_EVT_HINT_MAX; i++) {
    const game_evt_hint_pixel_t *h = &ev->hints[i];
    paint_square(h->square, h->r, h->g, h->b, LED_VIS_SNAP);
  }
  for (i = 0; i < ev->breathe_count && i < GAME_EVT_BREATHE_MAX; i++) {
    const game_evt_breathe_t *b = &ev->breathes[i];
    uint8_t row;
    uint8_t col;
    uint8_t led;
    if (b->square >= 64) {
      continue;
    }
    row = (uint8_t)(b->square / 8);
    col = (uint8_t)(b->square % 8);
    led = chess_pos_to_led_index(row, col);
    if (b->mode == GAME_EVT_BREATHE_SQ) {
      led_target_set_breathe_sq(led, b->r, b->g, b->b, b->floor, b->period_ms,
                                b->phase_ms, ev->fade_ms);
    } else {
      paint_square(b->square, b->r, b->g, b->b, LED_VIS_BREATHE);
    }
  }
  if (!patch) {
    led_target_end();
    if (ev->hint_lift_sq < 64) {
      led_move_collapse_capture();
    }
  }
  if (required) {
    led_required_step_plan_t plan;
    plan.active = true;
    plan.count = ev->square_count;
    plan.fade_ms = ev->fade_ms;
    if (plan.count > LED_REQUIRED_STEP_MAX_PX) {
      plan.count = LED_REQUIRED_STEP_MAX_PX;
    }
    for (i = 0; i < plan.count; i++) {
      plan.px[i] = ev->hints[i];
    }
    led_required_step_guide_apply(&plan);
  }
}

static void apply_board_anim(const game_event_t *ev) {
  static uint8_t to_led;
  led_command_t cmd = {0};
  to_led = ev->to;
  cmd.led_index = ev->from;
  cmd.data = &to_led;
  switch (ev->flags) {
  case GAME_EVT_ANIM_MOVE_PATH:
    cmd.type = LED_CMD_ANIM_MOVE_PATH;
    led_execute_command_new(&cmd);
    break;
  case GAME_EVT_ANIM_CASTLE:
    cmd.type = LED_CMD_ANIM_CASTLE;
    cmd.red = 255;
    cmd.green = 215;
    led_execute_command_new(&cmd);
    break;
  case GAME_EVT_ANIM_PROMOTE:
    cmd.type = LED_CMD_ANIM_PROMOTE;
    cmd.red = 255;
    cmd.green = 215;
    cmd.data = NULL;
    led_execute_command_new(&cmd);
    break;
  case GAME_EVT_ANIM_HINT_PAIR:
    cmd.type = LED_CMD_HIGHLIGHT_HINT;
    led_execute_command_new(&cmd);
    break;
  default:
    break;
  }
}

static void apply_led_ctrl(const game_event_t *ev) {
  switch (ev->flags) {
  case GAME_EVT_CTRL_STOP_ENDGAME:
    led_stop_endgame_animation();
    break;
  case GAME_EVT_CTRL_RESIGN_CANCEL:
    led_resign_ring_cancel();
    break;
  case GAME_EVT_CTRL_RELEASE_HOLD:
    led_release_direct_hold();
    break;
  case GAME_EVT_CTRL_ARM_HANDOVER:
    led_arm_direct_handover();
    break;
  case GAME_EVT_CTRL_LAMP:
    led_set_lamp_hold(ev->from != 0);
    break;
  case GAME_EVT_CTRL_BLACKOUT: {
    int i;
    led_set_forced_blackout(true);
    for (i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
      led_set_pixel_internal((uint8_t)i, 0, 0, 0);
    }
    led_force_immediate_update();
    break;
  }
  case GAME_EVT_CTRL_HA_COLOR:
    led_set_ha_color(ev->from, ev->to, ev->square_count, ev->end_result);
    break;
  case GAME_EVT_CTRL_FORCE_UPDATE:
    led_force_immediate_update();
    break;
  case GAME_EVT_CTRL_CLEAR_ALL:
    led_clear_all_safe();
    break;
  case GAME_EVT_CTRL_TRAIL_CANCEL:
    led_move_trail_cancel();
    break;
  case GAME_EVT_CTRL_TAKEBACK_CLEAR:
    led_takeback_clear();
    break;
  case GAME_EVT_CTRL_RESIGN_STOP:
    led_resign_ring_stop();
    break;
  case GAME_EVT_CTRL_RESIGN_START:
    led_resign_ring_start(ev->from, ev->to, ev->square_count != 0);
    break;
  case GAME_EVT_CTRL_SHOW_BOARD:
    led_show_chess_board();
    break;
  case GAME_EVT_CTRL_COLLAPSE:
    led_move_collapse_capture();
    break;
  case GAME_EVT_CTRL_PULSE:
    led_target_pulse(ev->from, ev->to, ev->square_count, ev->end_result);
    break;
  default:
    break;
  }
}

static void apply_move_rejected(const game_event_t *ev) {
  if (ev->flags & 2U) {
    if (s_error_led < 64U) {
      led_target_pulse_cancel(s_error_led);
      s_error_led = 255U;
    }
    led_target_begin();
    led_target_end();
    return;
  }
  if (ev->from >= 64) {
    return;
  }
  uint8_t led = chess_pos_to_led_index((uint8_t)(ev->from / 8),
                                       (uint8_t)(ev->from % 8));
  if (s_error_led < 64U && s_error_led != led) {
    led_target_pulse_cancel(s_error_led);
  }
  s_error_led = led;
  led_target_begin();
  if (ev->flags & 1U) {
    led_target_set(led, 255, 0, 0, LED_VIS_SNAP);
  }
  led_target_end();
  if (!(ev->flags & 1U)) {
    led_target_pulse(led, 255, 0, 0);
  }
}

static void apply_check(const game_event_t *ev) {
  led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                             .led_index = ev->from,
                             .duration_ms = 0};
  led_execute_command_new(&check_cmd);
}

static void apply_game_end(const game_event_t *ev) {
  led_command_t end_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                           .led_index = ev->from,
                           .duration_ms = 0};
  (void)ev->end_result;
  led_execute_command_new(&end_cmd);
}

static game_event_t s_led_game_ev;

void led_game_events_process(void) {
  game_event_msg_t msg;
  while (game_event_queue != NULL &&
         xQueueReceive(game_event_queue, &msg, 0) == pdTRUE) {
    game_event_resolve(&msg, &s_led_game_ev);
    const game_event_t *ev = &s_led_game_ev;
    switch (ev->kind) {
    case GAME_EVT_MOVE_REJECTED:
      apply_move_rejected(ev);
      break;
    case GAME_EVT_CHECK:
      apply_check(ev);
      break;
    case GAME_EVT_GAME_END:
      apply_game_end(ev);
      break;
    case GAME_EVT_RESTORE_BOARD:
      apply_restore(ev);
      break;
    case GAME_EVT_HINT_SQUARES:
      apply_hints(ev);
      break;
    case GAME_EVT_BOARD_ANIM:
      apply_board_anim(ev);
      break;
    case GAME_EVT_LED_CTRL:
      apply_led_ctrl(ev);
      break;
    case GAME_EVT_MOVE_ACCEPTED:
    default:
      break;
    }
  }
}

void led_game_events_report_overlay(void) {
  game_led_overlay_report(led_board_show_active(), led_lamp_board_held(),
                          led_gesture_anim_busy(), led_is_booting());
}
