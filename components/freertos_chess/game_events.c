/**
 * @file game_events.c
 * @brief Game event queue between session and LED task.
 */

#include "game_events.h"
#include "freertos_chess.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "GAME_EVENTS";

QueueHandle_t game_event_queue = NULL;

static volatile bool s_board_show;
static volatile bool s_lamp_held;
static volatile bool s_gesture_busy;
static volatile bool s_booting = true;
static volatile uint32_t s_board_picture_gen;

typedef struct {
  union {
    game_restore_plan_t restore;
    struct {
      game_evt_hint_pixel_t hints[GAME_EVT_HINT_MAX];
      game_evt_breathe_t breathes[GAME_EVT_BREATHE_MAX];
    } pixels;
    struct {
      uint8_t takeback_squares[8];
      uint8_t takeback_count;
    } takeback;
  } u;
} game_event_slot_t;

static game_event_slot_t s_slots[GAME_EVENT_QUEUE_DEPTH];
static uint8_t s_slot_seq;

_Static_assert(sizeof(game_event_msg_t) <= 16, "game_event_msg_t must be slim");

/* Last two queue slots stay free so reject and restore can still land. */
#define GAME_EVENT_CRITICAL_RESERVE 2

_Static_assert(GAME_EVENT_CRITICAL_RESERVE < GAME_EVENT_QUEUE_DEPTH,
               "critical reserve exceeds queue depth");

static bool event_is_critical(uint8_t kind) {
  return kind == (uint8_t)GAME_EVT_MOVE_REJECTED ||
         kind == (uint8_t)GAME_EVT_RESTORE_BOARD;
}

static uint8_t take_payload_slot(void) {
  uint8_t s = (uint8_t)(s_slot_seq % GAME_EVENT_QUEUE_DEPTH);
  s_slot_seq++;
  return s;
}

/* Payload must be copied only after a free slot exists. Critical may wait 10 ms. */
static bool queue_room(uint8_t kind) {
  UBaseType_t need = event_is_critical(kind)
                         ? 1u
                         : (UBaseType_t)(GAME_EVENT_CRITICAL_RESERVE + 1u);
  TickType_t wait = event_is_critical(kind) ? pdMS_TO_TICKS(10) : 0;
  TickType_t start;

  if (game_event_queue == NULL) {
    return false;
  }
  start = xTaskGetTickCount();
  for (;;) {
    if (uxQueueSpacesAvailable(game_event_queue) >= need) {
      return true;
    }
    if (wait == 0 || (TickType_t)(xTaskGetTickCount() - start) >= wait) {
      ESP_LOGW(TAG, "event queue full kind=%d", (int)kind);
      return false;
    }
    vTaskDelay(1);
  }
}

static bool post_msg(const game_event_msg_t *msg) {
  if (msg == NULL || game_event_queue == NULL) {
    return false;
  }
  if (msg->slot == GAME_EVENT_NO_SLOT) {
    bool critical = event_is_critical(msg->kind);
    if (!critical && uxQueueSpacesAvailable(game_event_queue) <=
                         GAME_EVENT_CRITICAL_RESERVE) {
      ESP_LOGW(TAG, "event queue full kind=%d", (int)msg->kind);
      return false;
    }
    if (xQueueSend(game_event_queue, msg,
                   critical ? pdMS_TO_TICKS(10) : 0) != pdTRUE) {
      ESP_LOGW(TAG, "event queue full kind=%d", (int)msg->kind);
      return false;
    }
    return true;
  }
  if (xQueueSend(game_event_queue, msg, 0) != pdTRUE) {
    s_slot_seq--;
    ESP_LOGW(TAG, "event queue full kind=%d", (int)msg->kind);
    return false;
  }
  return true;
}

void game_event_resolve(const game_event_msg_t *msg, game_event_t *out) {
  if (msg == NULL || out == NULL) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->kind = (game_event_kind_t)msg->kind;
  out->from = msg->from;
  out->to = msg->to;
  out->flags = msg->flags;

  switch (msg->kind) {
  case GAME_EVT_GAME_END:
    out->end_result = msg->aux0;
    break;
  case GAME_EVT_RESTORE_BOARD:
    out->restore_op = msg->aux0;
    if (msg->slot < GAME_EVENT_QUEUE_DEPTH) {
      const game_event_slot_t *sl = &s_slots[msg->slot];
      if (msg->aux0 == GAME_EVT_RESTORE_OP_TAKEBACK) {
        out->takeback_count = sl->u.takeback.takeback_count;
        memcpy(out->takeback_squares, sl->u.takeback.takeback_squares,
               sl->u.takeback.takeback_count);
      } else {
        out->restore = sl->u.restore;
      }
    }
    break;
  case GAME_EVT_HINT_SQUARES:
    out->fade_ms = msg->fade_ms;
    out->hint_lift_sq = msg->aux1;
    out->square_count = msg->aux0;
    if (msg->slot < GAME_EVENT_QUEUE_DEPTH && msg->aux0 > 0) {
      const game_event_slot_t *sl = &s_slots[msg->slot];
      memcpy(out->hints, sl->u.pixels.hints,
             (size_t)msg->aux0 * sizeof(game_evt_hint_pixel_t));
      if ((msg->flags & GAME_EVT_HINT_PATCH) == 0) {
        out->breathe_count = msg->reserved;
        if (out->breathe_count > GAME_EVT_BREATHE_MAX) {
          out->breathe_count = GAME_EVT_BREATHE_MAX;
        }
        memcpy(out->breathes, sl->u.pixels.breathes,
               (size_t)out->breathe_count * sizeof(game_evt_breathe_t));
      }
    }
    break;
  case GAME_EVT_LED_CTRL:
    out->flags = msg->flags;
    out->from = msg->from;
    out->to = msg->to;
    out->square_count = msg->aux0;
    out->end_result = msg->aux1;
    break;
  default:
    break;
  }
}

bool game_event_post(const game_event_t *ev) {
  if (ev == NULL) {
    return false;
  }
  switch (ev->kind) {
  case GAME_EVT_MOVE_ACCEPTED:
    game_event_post_move_accepted(ev->from, ev->to);
    return true;
  case GAME_EVT_MOVE_REJECTED:
    if (ev->flags & 2U) {
      game_event_post_move_rejected_clear();
    } else {
      game_event_post_move_rejected(ev->from, (ev->flags & 1U) != 0);
    }
    return true;
  case GAME_EVT_CHECK:
    game_event_post_check(ev->from);
    return true;
  case GAME_EVT_GAME_END:
    game_event_post_game_end(ev->from, ev->end_result);
    return true;
  case GAME_EVT_RESTORE_BOARD:
    if (ev->restore_op == GAME_EVT_RESTORE_OP_TAKEBACK &&
        ev->takeback_count > 0) {
      game_event_post_takeback(ev->takeback_squares, ev->takeback_count);
    } else {
      game_event_post_restore(
          ev->restore_op == GAME_EVT_RESTORE_OP_APPLY ? &ev->restore : NULL,
          ev->restore_op);
    }
    return true;
  default:
    return false;
  }
}

void game_event_post_move_rejected(uint8_t square, bool persist_red) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_MOVE_REJECTED,
      .from = square,
      .slot = GAME_EVENT_NO_SLOT,
  };
  if (persist_red) {
    msg.flags |= 1U;
  }
  (void)post_msg(&msg);
}

void game_event_post_move_rejected_clear(void) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_MOVE_REJECTED,
      .flags = 2U,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_check(uint8_t king_square) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_CHECK,
      .from = king_square,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_game_end(uint8_t king_square, uint8_t end_result) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_GAME_END,
      .from = king_square,
      .aux0 = end_result,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_move_accepted(uint8_t from, uint8_t to) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_MOVE_ACCEPTED,
      .from = from,
      .to = to,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_restore(const game_restore_plan_t *plan, uint8_t op) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_RESTORE_BOARD,
      .aux0 = op,
      .slot = GAME_EVENT_NO_SLOT,
  };
  if (plan != NULL) {
    if (!queue_room(GAME_EVT_RESTORE_BOARD)) {
      return;
    }
    uint8_t slot = take_payload_slot();
    s_slots[slot].u.restore = *plan;
    msg.slot = slot;
  }
  (void)post_msg(&msg);
}

void game_event_post_takeback(const uint8_t *squares, uint8_t count) {
  if (squares == NULL || count == 0) {
    game_event_post_restore(NULL, GAME_EVT_RESTORE_OP_TAKEBACK);
    return;
  }
  if (!queue_room(GAME_EVT_RESTORE_BOARD)) {
    return;
  }
  uint8_t slot = take_payload_slot();
  game_event_slot_t *sl = &s_slots[slot];
  sl->u.takeback.takeback_count = count > 8 ? 8 : count;
  memcpy(sl->u.takeback.takeback_squares, squares, sl->u.takeback.takeback_count);
  game_event_msg_t msg = {
      .kind = GAME_EVT_RESTORE_BOARD,
      .aux0 = GAME_EVT_RESTORE_OP_TAKEBACK,
      .slot = slot,
  };
  (void)post_msg(&msg);
}

void game_event_post_hints_clear(void) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_HINT_SQUARES,
      .aux1 = 255U,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_pixels(const game_evt_hint_pixel_t *px, uint8_t count,
                            const game_evt_breathe_t *breathes,
                            uint8_t breathe_count, uint16_t fade_ms,
                            bool patch, uint8_t hint_lift_sq) {
  uint8_t off = 0;
  bool first = true;
  if (breathe_count > GAME_EVT_BREATHE_MAX) {
    breathe_count = GAME_EVT_BREATHE_MAX;
  }
  do {
    game_event_msg_t msg = {
        .kind = GAME_EVT_HINT_SQUARES,
        .slot = GAME_EVENT_NO_SLOT,
    };
    uint8_t n = 0;
    uint8_t i;
    if (!first) {
      msg.flags = GAME_EVT_HINT_PATCH;
      msg.aux1 = 255U;
    } else {
      if (patch) {
        msg.flags = GAME_EVT_HINT_PATCH;
      }
      msg.aux1 = hint_lift_sq;
      msg.fade_ms = fade_ms;
      msg.reserved = breathe_count;
    }
    if (!queue_room(GAME_EVT_HINT_SQUARES)) {
      return;
    }
    uint8_t slot = take_payload_slot();
    game_event_slot_t *sl = &s_slots[slot];
    memset(sl->u.pixels.hints, 0, sizeof(sl->u.pixels.hints));
    memset(sl->u.pixels.breathes, 0, sizeof(sl->u.pixels.breathes));
    if (first && breathes != NULL) {
      for (i = 0; i < breathe_count; i++) {
        sl->u.pixels.breathes[i] = breathes[i];
      }
    }
    while (off < count && n < GAME_EVT_HINT_MAX) {
      if (px != NULL) {
        sl->u.pixels.hints[n] = px[off];
      }
      n++;
      off++;
    }
    msg.aux0 = n;
    msg.slot = slot;
    (void)post_msg(&msg);
    first = false;
  } while (off < count);
}

void game_event_post_board_anim(uint8_t anim, uint8_t led_a, uint8_t led_b) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_BOARD_ANIM,
      .from = led_a,
      .to = led_b,
      .flags = anim,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_led_ctrl(uint8_t op, uint8_t a, uint8_t b, uint8_t c,
                              uint8_t d) {
  game_event_msg_t msg = {
      .kind = GAME_EVT_LED_CTRL,
      .flags = op,
      .from = a,
      .to = b,
      .aux0 = c,
      .aux1 = d,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

static struct {
  bool active;
  uint8_t count;
  game_evt_hint_pixel_t px[GAME_EVT_HINT_MAX];
  uint16_t fade_ms;
} s_required_step_mirror;

void game_event_required_step_disarm(void) {
  memset(&s_required_step_mirror, 0, sizeof(s_required_step_mirror));
}

void game_event_post_required_step_clear(void) {
  game_event_required_step_disarm();
  game_event_msg_t msg = {
      .kind = GAME_EVT_HINT_SQUARES,
      .flags = GAME_EVT_HINT_REQUIRED_STEP,
      .aux1 = 254U,
      .slot = GAME_EVENT_NO_SLOT,
  };
  (void)post_msg(&msg);
}

void game_event_post_required_step_guide(const game_evt_hint_pixel_t *px,
                                         uint8_t count, uint16_t fade_ms) {
  uint8_t n = count > GAME_EVT_HINT_MAX ? GAME_EVT_HINT_MAX : count;
  s_required_step_mirror.active = (n > 0);
  s_required_step_mirror.count = n;
  s_required_step_mirror.fade_ms = fade_ms;
  if (px != NULL && n > 0) {
    memcpy(s_required_step_mirror.px, px, (size_t)n * sizeof(px[0]));
  }
  if (n == 0) {
    game_event_post_required_step_clear();
    return;
  }
  if (!queue_room(GAME_EVT_HINT_SQUARES)) {
    return;
  }
  uint8_t slot = take_payload_slot();
  game_event_slot_t *sl = &s_slots[slot];
  memset(sl->u.pixels.hints, 0, sizeof(sl->u.pixels.hints));
  memcpy(sl->u.pixels.hints, s_required_step_mirror.px, (size_t)n * sizeof(px[0]));
  game_event_msg_t msg = {
      .kind = GAME_EVT_HINT_SQUARES,
      .flags = GAME_EVT_HINT_REQUIRED_STEP,
      .aux0 = n,
      .fade_ms = fade_ms,
      .aux1 = 255U,
      .slot = slot,
  };
  (void)post_msg(&msg);
}

bool game_event_required_step_mirror_active(void) {
  return s_required_step_mirror.active && s_required_step_mirror.count > 0;
}

uint8_t game_event_required_step_mirror_copy(game_evt_hint_pixel_t *out,
                                             uint8_t max_out,
                                             uint16_t *fade_ms) {
  if (!s_required_step_mirror.active || out == NULL || max_out == 0) {
    return 0;
  }
  uint8_t n = s_required_step_mirror.count;
  if (n > max_out) {
    n = max_out;
  }
  memcpy(out, s_required_step_mirror.px, (size_t)n * sizeof(out[0]));
  if (fade_ms != NULL) {
    *fade_ms = s_required_step_mirror.fade_ms;
  }
  return n;
}

void game_led_overlay_report(bool board_show, bool lamp_held, bool gesture_busy,
                             bool booting) {
  s_board_show = board_show;
  s_lamp_held = lamp_held;
  s_gesture_busy = gesture_busy;
  s_booting = booting;
}

bool game_led_lamp_held(void) { return s_lamp_held; }

bool game_led_gesture_busy(void) { return s_gesture_busy; }

bool game_led_boot_active(void) { return s_booting; }

bool game_led_overlay_blocks_restore(void) {
  return s_board_show || s_lamp_held || s_gesture_busy;
}

uint32_t game_led_board_picture_gen(void) { return s_board_picture_gen; }

void game_led_board_picture_bump(void) { s_board_picture_gen++; }
