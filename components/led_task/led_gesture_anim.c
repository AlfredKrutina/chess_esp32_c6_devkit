/**
 * @file led_gesture_anim.c
 * @brief Wave, boot snake, shutdown chase, and pairing pulse.
 *
 * Square orientation follows chess_pos_to_led_index(): row 0 is rank 1
 * (e1 is row 0, col 4), col 0 is the a-file. The column mirror inside that
 * function is the wiring map; this file does not flip it.
 *
 * The physical button sits just outside the board on the edge between a1
 * (row 0, col 0) and b1 (row 0, col 1). The wave origin is the midpoint of
 * that edge, half a square off the board: (file 0.5, rank -0.5).
 */

#include "led_gesture_anim.h"

#include "led_mapping.h"
#include "led_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include <string.h>

static const char *TAG = "LED_GESTURE";

typedef enum {
  PH_IDLE = 0,
  PH_WAVE,
  PH_SNAKE_FWD,
  PH_SNAKE_REV,
  PH_SHUTDOWN,
  PH_PULSE
} phase_t;

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static phase_t s_phase;
static bool s_release;
static led_gesture_result_t s_result;
static uint32_t s_old[64];
static uint32_t s_new[64];
static uint8_t s_step;
static uint8_t s_rev_alt;
static bool s_have_old;
static uint8_t s_black_leds[16];
static bool s_black_ready;

static void post_result(led_gesture_result_t result) {
  s_result = result;
  s_phase = PH_IDLE;
  s_release = false;
}

static void paint(uint8_t index, uint32_t rgb) {
  led_set_pixel_internal(index, (uint8_t)((rgb >> 16) & 0xFFu),
                         (uint8_t)((rgb >> 8) & 0xFFu),
                         (uint8_t)(rgb & 0xFFu));
}

static void restore_board(void) {
  for (int i = 0; i < 64; i++) {
    paint((uint8_t)i, s_old[i]);
  }
}

static void capture_board(void) {
  uint32_t all[73];
  memset(all, 0, sizeof(all));
  led_get_all_states(all, 73);
  memcpy(s_old, all, sizeof(s_old));
  s_have_old = true;
}

static bool begin_phase(phase_t phase, bool keep_release) {
  bool ok = false;
  taskENTER_CRITICAL(&s_mux);
  if (s_phase == PH_IDLE) {
    s_phase = phase;
    if (!keep_release) {
      s_release = false;
    }
    s_step = 0;
    s_rev_alt = 0;
    s_have_old = false;
    ok = true;
  }
  taskEXIT_CRITICAL(&s_mux);
  return ok;
}

static void ensure_black_order(void) {
  if (s_black_ready) {
    return;
  }
  uint8_t n = 0;
  for (uint8_t row = 6; row <= 7; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      s_black_leds[n++] = chess_pos_to_led_index(row, col);
    }
  }
  for (int i = 1; i < 16; i++) {
    uint8_t v = s_black_leds[i];
    int j = i;
    while (j > 0 && s_black_leds[j - 1] > v) {
      s_black_leds[j] = s_black_leds[j - 1];
      j--;
    }
    s_black_leds[j] = v;
  }
  s_black_ready = true;
}

/** Half-square units. Origin is (1, -1), square (col,row) center is (2*col, 2*row). */
static int dist2_half(uint8_t row, uint8_t col) {
  int dx = (int)col * 2 - 1;
  int dy = (int)row * 2 + 1;
  return dx * dx + dy * dy;
}

static uint8_t crest_channel(uint8_t base) {
  unsigned v = (unsigned)base / 3u + 210u;
  if (v > 255u) {
    v = 255u;
  }
  return (uint8_t)v;
}

static void tick_wave(void) {
  if (!s_have_old) {
    capture_board();
  }
  /* 19^2 = 361 does not cover h8 (394). 20^2 = 400 does. */
  const int radius = (int)s_step;
  const int radius2 = radius * radius;
  const int crest_lo = (radius > 2) ? (radius - 2) * (radius - 2) : -1;
  bool covered = true;
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      int d2 = dist2_half(row, col);
      uint8_t led = chess_pos_to_led_index(row, col);
      uint32_t color;
      if (d2 > radius2) {
        color = s_old[led];
        covered = false;
      } else if (d2 > crest_lo) {
        uint32_t base = s_new[led];
        color = ((uint32_t)crest_channel((uint8_t)((base >> 16) & 0xFFu))
                  << 16) |
                 ((uint32_t)crest_channel((uint8_t)((base >> 8) & 0xFFu))
                  << 8) |
                 (uint32_t)crest_channel((uint8_t)(base & 0xFFu));
      } else {
        color = s_new[led];
      }
      paint(led, color);
    }
  }
  if (covered && radius >= 20) {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, s_new[i]);
    }
    ESP_LOGI(TAG, "[STAGING] wave covered the board");
    post_result(LED_GESTURE_WAVE_DONE);
    return;
  }
  if (s_step < 250) {
    s_step++;
  }
}

static void paint_snake(uint8_t head) {
  for (uint8_t i = 0; i < 64; i++) {
    if (i < head) {
      uint32_t color = (i + 1u == head) ? 0x00FF00u : 0x008000u;
      paint(i, color);
    } else {
      paint(i, s_old[i]);
    }
  }
}

static void tick_snake_forward(void) {
  if (!s_have_old) {
    capture_board();
  }
  bool released = false;
  taskENTER_CRITICAL(&s_mux);
  released = s_release;
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  if (released) {
    s_phase = PH_SNAKE_REV;
    s_rev_alt = 0;
    ESP_LOGI(TAG, "[STAGING] snake reverse (released early)");
    return;
  }
  if (s_step >= 64) {
    ESP_LOGI(TAG, "[STAGING] snake reached the end");
    post_result(LED_GESTURE_SNAKE_DONE);
    return;
  }
  s_step++;
  paint_snake(s_step);
}

static void tick_snake_reverse(void) {
  taskENTER_CRITICAL(&s_mux);
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  uint8_t back = (uint8_t)(2u + (s_rev_alt & 1u));
  s_rev_alt++;
  if (s_step <= back) {
    s_step = 0;
  } else {
    s_step = (uint8_t)(s_step - back);
  }
  if (s_step == 0) {
    restore_board();
    ESP_LOGI(TAG, "[STAGING] snake reverse finished, game not started");
    post_result(LED_GESTURE_SNAKE_CANCELLED);
    return;
  }
  paint_snake(s_step);
}

static void tick_shutdown(void) {
  if (!s_have_old) {
    capture_board();
    ensure_black_order();
  }
  bool released = false;
  taskENTER_CRITICAL(&s_mux);
  released = s_release;
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  if (released) {
    restore_board();
    ESP_LOGI(TAG, "[STAGING] shutdown cancelled, lights restored");
    post_result(LED_GESTURE_SHUTDOWN_CANCELLED);
    return;
  }
  /* 0..15 chase, 16..17 whole board dark, then done. */
  if (s_step >= 18) {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, 0);
    }
    ESP_LOGI(TAG, "[STAGING] shutdown chase finished");
    post_result(LED_GESTURE_SHUTDOWN_DONE);
    return;
  }
  if (s_step < 16) {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, s_old[i]);
    }
    for (uint8_t i = 0; i < s_step; i++) {
      paint(s_black_leds[i], 0);
    }
    paint(s_black_leds[s_step], 0xDCDCDCu);
  } else {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, 0);
    }
  }
  s_step++;
}

static void tick_pulse(void) {
  if (!s_have_old) {
    capture_board();
  }
  static const uint8_t k_blue[6] = {40, 120, 255, 180, 70, 0};
  if (s_step >= 6) {
    restore_board();
    post_result(LED_GESTURE_PULSE_DONE);
    return;
  }
  uint8_t b = k_blue[s_step];
  for (int i = 0; i < 64; i++) {
    paint((uint8_t)i, (uint32_t)b);
  }
  s_step++;
}

bool led_gesture_anim_busy(void) { return s_phase != PH_IDLE; }

void led_gesture_anim_tick(void) {
  switch (s_phase) {
  case PH_WAVE:
    tick_wave();
    break;
  case PH_SNAKE_FWD:
    tick_snake_forward();
    break;
  case PH_SNAKE_REV:
    tick_snake_reverse();
    break;
  case PH_SHUTDOWN:
    tick_shutdown();
    break;
  case PH_PULSE:
    tick_pulse();
    break;
  case PH_IDLE:
  default:
    break;
  }
}

led_gesture_result_t led_gesture_take_result(void) {
  led_gesture_result_t result;
  taskENTER_CRITICAL(&s_mux);
  result = s_result;
  s_result = LED_GESTURE_RESULT_NONE;
  taskEXIT_CRITICAL(&s_mux);
  return result;
}

bool led_gesture_start_wave(const uint32_t new_colors[64]) {
  if (new_colors == NULL) {
    return false;
  }
  taskENTER_CRITICAL(&s_mux);
  if (s_phase != PH_IDLE) {
    taskEXIT_CRITICAL(&s_mux);
    return false;
  }
  memcpy(s_new, new_colors, sizeof(s_new));
  s_phase = PH_WAVE;
  s_release = false;
  s_step = 0;
  s_rev_alt = 0;
  s_have_old = false;
  taskEXIT_CRITICAL(&s_mux);
  return true;
}

bool led_gesture_start_snake(void) {
  return begin_phase(PH_SNAKE_FWD, true);
}

bool led_gesture_start_shutdown(void) {
  return begin_phase(PH_SHUTDOWN, true);
}

bool led_gesture_start_pair_pulse(void) {
  return begin_phase(PH_PULSE, false);
}

void led_gesture_note_release(void) {
  /* Latched even if the LED phase has not started yet (game task is slower
   * than the button scan). Wave, pulse, and reverse ignore the latch. */
  taskENTER_CRITICAL(&s_mux);
  s_release = true;
  taskEXIT_CRITICAL(&s_mux);
}
