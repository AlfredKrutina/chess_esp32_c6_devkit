/**
 * @file led_gesture_anim.c
 * @brief Wave, boot snake, shutdown chase, and pairing pulse.
 *
 * Square orientation follows chess_pos_to_led_index(): row 0 is rank 1
 * (e1 is row 0, col 4), col 0 is the a-file. The column mirror inside that
 * function is the wiring map; this file does not flip it.
 *
 * The physical button sits on h1 (row 0, col 7). The wave origin is the
 * center of that square. The snake starts there and crawls h1→a1, then a2→h2.
 *
 * LED 72 (reset / gesture button) follows the same phase: orange while held,
 * gold through the wave, green along the snake, white-to-off on shutdown,
 * blue during pairing. Edges ease instead of popping one square per frame.
 */

#include "led_gesture_anim.h"

#include "freertos_chess.h"
#include "led_mapping.h"
#include "led_task.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
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
static bool s_wave_in;
static uint8_t s_rev_alt;
static bool s_have_old;
static bool s_pair_hold;
static bool s_pair_captured;
static bool s_pair_yield;
static bool s_pair_stop;
static uint8_t s_pair_step;
static uint8_t s_snake_pace;
static uint16_t s_snake_q8;
static uint8_t s_black_leds[16];
static bool s_black_ready;
static uint8_t s_pair_amount;
static uint8_t s_hold_level;
static uint32_t s_btn_rgb;
static bool s_btn_saved;
static bool s_btn_owned;
static bool s_btn_leave_dark;

#define GESTURE_BUTTON_LED 72

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
    if (s_pair_hold && s_pair_captured) {
      s_pair_yield = true;
    }
    s_phase = phase;
    if (!keep_release) {
      s_release = false;
    }
    s_step = 0;
    s_rev_alt = 0;
    s_snake_pace = 0;
    s_snake_q8 = 0;
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

/** Half-square units. Origin is h1 (14, 0). Square (col,row) center is (2*col, 2*row). */
static int dist2_half(uint8_t row, uint8_t col) {
  int dx = (int)col * 2 - 14;
  int dy = (int)row * 2;
  return dx * dx + dy * dy;
}

static uint8_t crest_channel(uint8_t base) {
  unsigned v = (unsigned)base / 3u + 210u;
  if (v > 255u) {
    v = 255u;
  }
  return (uint8_t)v;
}

static uint8_t smoothstep_u8(uint8_t x) {
  uint32_t v = (uint32_t)x;
  uint32_t s = (v * v * (765U - 2U * v)) / (255U * 255U);
  return (uint8_t)s;
}

static uint8_t lerp_u8(uint8_t a, uint8_t b, uint8_t t) {
  return (uint8_t)(((uint32_t)a * (255U - t) + (uint32_t)b * t) / 255U);
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint8_t t) {
  return ((uint32_t)lerp_u8((uint8_t)((a >> 16) & 0xFFu),
                            (uint8_t)((b >> 16) & 0xFFu), t)
          << 16) |
         ((uint32_t)lerp_u8((uint8_t)((a >> 8) & 0xFFu),
                            (uint8_t)((b >> 8) & 0xFFu), t)
          << 8) |
         (uint32_t)lerp_u8((uint8_t)(a & 0xFFu), (uint8_t)(b & 0xFFu), t);
}

static uint32_t crest_rgb(uint32_t base) {
  return ((uint32_t)crest_channel((uint8_t)((base >> 16) & 0xFFu)) << 16) |
         ((uint32_t)crest_channel((uint8_t)((base >> 8) & 0xFFu)) << 8) |
         (uint32_t)crest_channel((uint8_t)(base & 0xFFu));
}

static void tick_wave(void) {
  if (!s_have_old) {
    capture_board();
  }
  /* Farthest square from h1 is a8 at 14^2+14^2 = 392.
   * Outward radius 20 covers it. Inward starts past that (23) and shrinks
   * through h1 (radius -3) so the same crest runs backwards. */
  const int radius = s_wave_in ? (23 - (int)s_step) : (int)s_step;
  if (s_wave_in && radius <= -3) {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, s_new[i]);
    }
    ESP_LOGI(TAG, "[STAGING] wave uncovered the board");
    post_result(LED_GESTURE_WAVE_DONE);
    return;
  }
  const int radius2 = radius * radius;
  const int crest_lo = (radius > 2) ? (radius - 2) * (radius - 2) : -1;
  bool covered = true;
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      int d2 = dist2_half(row, col);
      uint8_t led = chess_pos_to_led_index(row, col);
      /* Disk interior is the spreading color. Inward, that disk is the lamp
       * shrinking toward h1 and the outside is the picture being revealed. */
      uint32_t outside = s_wave_in ? s_new[led] : s_old[led];
      uint32_t inside = s_wave_in ? s_old[led] : s_new[led];
      uint32_t crest = crest_rgb(inside);
      int outer = radius + 3;
      int outer2 = outer * outer;
      uint32_t color;
      if (d2 >= outer2) {
        color = outside;
        covered = false;
      } else if (d2 > radius2) {
        int span = outer2 - radius2;
        int t = (span > 0) ? ((outer2 - d2) * 255) / span : 255;
        if (t < 0) {
          t = 0;
        } else if (t > 255) {
          t = 255;
        }
        color = lerp_rgb(outside, crest, smoothstep_u8((uint8_t)t));
        covered = false;
      } else if (radius > 2 && d2 > crest_lo) {
        int span = radius2 - crest_lo;
        int t = (span > 0) ? ((radius2 - d2) * 255) / span : 255;
        if (t < 0) {
          t = 0;
        } else if (t > 255) {
          t = 255;
        }
        color = lerp_rgb(crest, inside, smoothstep_u8((uint8_t)t));
      } else {
        color = inside;
      }
      paint(led, color);
    }
  }
  if (!s_wave_in && covered && radius >= 20) {
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

/** Board snake: rank 1 h→a, rank 2 a→h, and so on. Head starts on h1. */
static void snake_square(uint8_t step, uint8_t *row, uint8_t *col) {
  uint8_t rank = (uint8_t)(step / 8u);
  uint8_t file = (uint8_t)(step % 8u);
  if ((rank & 1u) == 0u) {
    file = (uint8_t)(7u - file);
  }
  *row = rank;
  *col = file;
}

/* Same crawl as led_boot_animation_step (164/25 ms). Hold runs a bit faster:
 * 300 counts per 33 ms frame, about 1.8 s across the board. */
#define SNAKE_Q8_END (64u * 256u)
#define SNAKE_Q8_FWD 300u
#define SNAKE_Q8_REV 420u

static void paint_boot_snake(uint16_t q8) {
  unsigned head_sq = (unsigned)(q8 >> 8);
  uint8_t lead = smoothstep_u8((uint8_t)(q8 & 0xFFu));
  for (unsigned i = 0; i < 64u; i++) {
    uint8_t row;
    uint8_t col;
    snake_square((uint8_t)i, &row, &col);
    uint8_t g = 0;
    if (head_sq >= 64u || i + 2u < head_sq) {
      g = 128;
    } else if (i + 1u < head_sq) {
      g = 200;
    } else if (i < head_sq) {
      g = 255;
    } else if (i == head_sq && head_sq < 64u) {
      g = lead;
    }
    paint(chess_pos_to_led_index(row, col), (uint32_t)g << 8);
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
    ESP_LOGI(TAG, "[STAGING] snake reverse (released early)");
    return;
  }
  if (s_snake_q8 >= SNAKE_Q8_END) {
    ESP_LOGI(TAG, "[STAGING] snake reached the end");
    post_result(LED_GESTURE_SNAKE_DONE);
    return;
  }
  uint32_t next = (uint32_t)s_snake_q8 + SNAKE_Q8_FWD;
  if (next > SNAKE_Q8_END) {
    next = SNAKE_Q8_END;
  }
  s_snake_q8 = (uint16_t)next;
  paint_boot_snake(s_snake_q8);
}

static void tick_snake_reverse(void) {
  taskENTER_CRITICAL(&s_mux);
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  if (s_snake_q8 <= SNAKE_Q8_REV) {
    s_snake_q8 = 0;
    restore_board();
    ESP_LOGI(TAG, "[STAGING] snake reverse finished, game not started");
    post_result(LED_GESTURE_SNAKE_CANCELLED);
    return;
  }
  s_snake_q8 = (uint16_t)(s_snake_q8 - SNAKE_Q8_REV);
  paint_boot_snake(s_snake_q8);
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
  /* Each black-player square eases white, then out, over two frames.
   * After the last square the board stays dark for a short beat. */
  if (s_step >= 16) {
    for (int i = 0; i < 64; i++) {
      paint((uint8_t)i, 0);
    }
    s_snake_pace++;
    if (s_snake_pace >= 6) {
      s_btn_leave_dark = true;
      ESP_LOGI(TAG, "[STAGING] shutdown chase finished");
      post_result(LED_GESTURE_SHUTDOWN_DONE);
    }
    return;
  }
  uint8_t fade = (s_snake_pace == 0) ? 70 : 210;
  for (int i = 0; i < 64; i++) {
    paint((uint8_t)i, s_old[i]);
  }
  for (uint8_t i = 0; i < s_step; i++) {
    paint(s_black_leds[i], 0);
  }
  {
    uint8_t led = s_black_leds[s_step];
    uint32_t mid = lerp_rgb(s_old[led], 0xDCDCDCu, (uint8_t)(255u - fade));
    paint(led, lerp_rgb(mid, 0, fade));
  }
  s_snake_pace++;
  if (s_snake_pace >= 2) {
    paint(s_black_leds[s_step], 0);
    s_snake_pace = 0;
    s_step++;
  }
}

static uint8_t mix_channel(uint8_t base, uint8_t tint, uint8_t amount) {
  unsigned v = (unsigned)base * (255u - amount) + (unsigned)tint * amount;
  return (uint8_t)(v / 255u);
}

/** ~3 s breathe. Amount 0 shows the saved game colors. */
static void tick_pair_hold(void) {
  if (!s_pair_captured) {
    capture_board();
    s_pair_captured = true;
    s_pair_step = 0;
    return;
  }
  /* 0..47 rise, 48..95 fall. Smoothstep keeps the turn at the top soft. */
  uint8_t tri = s_pair_step < 48 ? s_pair_step : (uint8_t)(96 - s_pair_step);
  uint8_t shaped = smoothstep_u8((uint8_t)((unsigned)tri * 255u / 48u));
  uint8_t amount = (uint8_t)((unsigned)shaped * 140u / 255u);
  s_pair_amount = amount;
  for (int i = 0; i < 64; i++) {
    uint32_t base = s_old[i];
    uint32_t color =
        ((uint32_t)mix_channel((uint8_t)((base >> 16) & 0xFFu), 0, amount)
         << 16) |
        ((uint32_t)mix_channel((uint8_t)((base >> 8) & 0xFFu), 40, amount)
         << 8) |
        (uint32_t)mix_channel((uint8_t)(base & 0xFFu), 160, amount);
    paint((uint8_t)i, color);
  }
  s_pair_step++;
  if (s_pair_step >= 96) {
    s_pair_step = 0;
  }
}

bool led_gesture_anim_busy(void) { return s_phase != PH_IDLE; }

static volatile bool s_button_held;
static bool s_hold_lit;

static void hold_step(uint8_t target) {
  if (s_hold_level < target) {
    uint8_t room = (uint8_t)(target - s_hold_level);
    s_hold_level = (uint8_t)(s_hold_level + (room > 48u ? 48u : room));
  } else if (s_hold_level > target) {
    uint8_t room = (uint8_t)(s_hold_level - target);
    s_hold_level = (uint8_t)(s_hold_level - (room > 48u ? 48u : room));
  }
}

void led_gesture_set_button_held(bool held) { s_button_held = held; }

bool led_gesture_button_down(void) {
#if CONFIG_CHESS_GESTURE_BUTTON
  if (gpio_get_level((gpio_num_t)CONFIG_CHESS_GESTURE_BUTTON_GPIO) == 0) {
    return true;
  }
  /* This board's switch is the reset pin (log: Button ID 8), not IO23. */
  if ((int)BUTTON_RESET != CONFIG_CHESS_GESTURE_BUTTON_GPIO &&
      !chess_gpio_pin_is_stm32_nrst_output((int)BUTTON_RESET)) {
    return gpio_get_level(BUTTON_RESET) == 0;
  }
#else
  (void)0;
#endif
  return false;
}

static void hold_restore(void) { s_hold_lit = false; }

static void hold_preview(void) {
  if (s_phase != PH_IDLE) {
    return;
  }
  /* Boot still owns the board. A floating gesture pin reads as held and
   * would turn the button LED orange on top of the boot snake. */
  if (led_is_booting()) {
    if (s_hold_lit) {
      s_hold_level = 0;
      hold_restore();
    }
    return;
  }
  if (!s_button_held) {
    if (!s_hold_lit) {
      return;
    }
    hold_step(0);
    if (s_hold_level == 0) {
      hold_restore();
    }
    return;
  }
  if (!s_hold_lit) {
    s_hold_lit = true;
    s_hold_level = 0;
  }
  hold_step(255);
}

static void paint_gesture_button(void) {
  if (s_btn_leave_dark) {
    paint(GESTURE_BUTTON_LED, 0);
    s_btn_leave_dark = false;
    s_btn_owned = false;
    s_btn_saved = false;
    s_pair_amount = 0;
    return;
  }

  bool own = false;
  uint32_t color = 0;
  if (s_phase == PH_WAVE) {
    unsigned span = s_wave_in ? 26u : 20u;
    unsigned num = (unsigned)s_step * 255u / span;
    if (num > 255u) {
      num = 255u;
    }
    uint8_t t = smoothstep_u8((uint8_t)num);
    color = s_wave_in ? lerp_rgb(0xFFD646u, 0x402000u, t)
                      : lerp_rgb(0x402000u, 0xFFD646u, t);
    own = true;
  } else if (s_phase == PH_SNAKE_FWD || s_phase == PH_SNAKE_REV) {
    uint8_t t = smoothstep_u8(
        (uint8_t)(((unsigned)s_snake_q8 * 255u) / SNAKE_Q8_END));
    uint8_t g = (s_phase == PH_SNAKE_FWD)
                    ? (uint8_t)(40u + ((unsigned)t * 215u) / 255u)
                    : (uint8_t)(((unsigned)t * 180u) / 255u);
    color = (uint32_t)g << 8;
    own = true;
  } else if (s_phase == PH_SHUTDOWN) {
    uint8_t t = (s_step >= 16) ? 255 : (uint8_t)((unsigned)s_step * 255u / 16u);
    color = lerp_rgb(0xDCDCDCu, 0, smoothstep_u8(t));
    own = true;
  } else if (s_pair_hold && s_pair_captured && s_phase == PH_IDLE) {
    color = (uint32_t)((160u * s_pair_amount) / 140u);
    own = true;
  } else if (s_hold_level > 0) {
    color = lerp_rgb(s_btn_rgb, 0xFF6600u, smoothstep_u8(s_hold_level));
    own = true;
  }

  if (own) {
    if (!s_btn_saved) {
      uint32_t all[73];
      memset(all, 0, sizeof(all));
      led_get_all_states(all, 73);
      s_btn_rgb = all[GESTURE_BUTTON_LED];
      s_btn_saved = true;
    }
    paint(GESTURE_BUTTON_LED, color);
    s_btn_owned = true;
    return;
  }
  if (s_btn_owned) {
    paint(GESTURE_BUTTON_LED, s_btn_rgb);
    s_btn_owned = false;
    s_btn_saved = false;
    s_pair_amount = 0;
  }
}

void led_gesture_anim_tick(void) {
#if CONFIG_CHESS_GESTURE_BUTTON
  /* Follow the pin here too. The button task owns the gesture timing, but
   * the button LED must turn orange even if that task has not published yet. */
  s_button_held = led_gesture_button_down();
#endif
  if (s_phase != PH_IDLE && s_hold_lit) {
    s_hold_level = 0;
    hold_restore();
  }
  bool yield = false;
  bool stop = false;
  taskENTER_CRITICAL(&s_mux);
  yield = s_pair_yield;
  s_pair_yield = false;
  stop = s_pair_stop;
  s_pair_stop = false;
  taskEXIT_CRITICAL(&s_mux);
  if (yield && s_pair_captured) {
    restore_board();
    s_pair_captured = false;
    s_have_old = false;
  }
  if (stop && s_pair_captured && s_phase == PH_IDLE) {
    restore_board();
    s_pair_captured = false;
  }
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
  case PH_IDLE:
  default:
    break;
  }
  if (s_pair_hold && s_phase == PH_IDLE && !stop) {
    tick_pair_hold();
  }
  hold_preview();
  paint_gesture_button();
  if (s_phase != PH_IDLE || s_hold_lit || s_btn_owned) {
    led_force_immediate_update();
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

bool led_gesture_start_wave(const uint32_t new_colors[64], bool inward) {
  if (new_colors == NULL) {
    return false;
  }
  taskENTER_CRITICAL(&s_mux);
  if (s_phase != PH_IDLE) {
    taskEXIT_CRITICAL(&s_mux);
    return false;
  }
  if (s_pair_hold && s_pair_captured) {
    s_pair_yield = true;
  }
  memcpy(s_new, new_colors, sizeof(s_new));
  s_phase = PH_WAVE;
  s_release = false;
  s_step = 0;
  s_wave_in = inward;
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
  taskENTER_CRITICAL(&s_mux);
  s_pair_hold = true;
  s_pair_step = 0;
  if (s_phase != PH_IDLE) {
    s_pair_captured = false;
  }
  taskEXIT_CRITICAL(&s_mux);
  return true;
}

void led_gesture_stop_pair_pulse(void) {
  taskENTER_CRITICAL(&s_mux);
  s_pair_hold = false;
  s_pair_yield = false;
  if (s_pair_captured && s_phase == PH_IDLE) {
    s_pair_stop = true;
  } else {
    s_pair_captured = false;
  }
  taskEXIT_CRITICAL(&s_mux);
}

void led_gesture_note_release(void) {
  /* Latched even if the LED phase has not started yet (game task is slower
   * than the button scan). Wave, pulse, and reverse ignore the latch. */
  taskENTER_CRITICAL(&s_mux);
  s_release = true;
  taskEXIT_CRITICAL(&s_mux);
}
