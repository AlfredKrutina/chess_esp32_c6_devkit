/**
 * @file led_gesture_anim.c
 * @brief Wave, new-game sweep, shutdown power symbol, and pairing pulse.
 *
 * Pictures and glyphs are chess coordinates: row 0 is rank 1, col 0 is the
 * a-file, and a square's slot is row * 8 + col. The strip index is only the
 * value chess_pos_to_led_index() returns at the moment of the pixel write.
 * That function does not mirror the file. See context/firmware/LED_MAP.md.
 *
 * The physical button sits on h1 (row 0, col 7). The wave origin is the
 * center of that square. The new-game sweep starts there and crawls
 * h1→a1, then a2→h2, wiping the picture to black behind it.
 *
 * LED 72 (reset / gesture button) follows the same phase: orange while held,
 * gold through the wave, amber along the new-game sweep, red power symbol
 * on shutdown, light blue during pairing with the rune as the dark mark. Edges ease instead of popping one
 * square per frame.
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
static bool s_shut_back;
static bool s_wave_in;
static uint32_t s_cover;
static uint8_t s_rev_alt;
static bool s_have_old;
static bool s_pair_hold;
static bool s_pair_captured;
static bool s_pair_yield;
static bool s_pair_stop;
static uint8_t s_pair_step;
static uint16_t s_snake_q8;
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

/** Chess slot. row 0 = rank 1, col 0 = file a. Not a strip index. */
static uint8_t sq_of(uint8_t row, uint8_t col) {
  return (uint8_t)((row << 3) | col);
}

static void paint_square(uint8_t row, uint8_t col, uint32_t rgb) {
  paint(chess_pos_to_led_index(row, col), rgb);
}

static void paint_squares(const uint32_t squares[64]) {
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      paint_square(row, col, squares[sq_of(row, col)]);
    }
  }
}

static void restore_board(void) { paint_squares(s_old); }

/** Lamp white sits in the top byte. The wave paints RGB, so fold it back. */
static uint32_t visible_rgb(uint32_t state) {
  unsigned w = (state >> 24) & 0xFFu;
  unsigned r = ((state >> 16) & 0xFFu) + w;
  unsigned g = ((state >> 8) & 0xFFu) + w;
  unsigned b = (state & 0xFFu) + w;
  if (r > 255u) {
    r = 255u;
  }
  if (g > 255u) {
    g = 255u;
  }
  if (b > 255u) {
    b = 255u;
  }
  return (r << 16) | (g << 8) | b;
}

static void capture_board(void) {
  uint32_t all[73];
  memset(all, 0, sizeof(all));
  led_get_all_states(all, 73);
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint8_t led = chess_pos_to_led_index(row, col);
      s_old[sq_of(row, col)] = visible_rgb(all[led]);
    }
  }
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
    s_shut_back = false;
    s_rev_alt = 0;
    s_snake_q8 = 0;
    s_have_old = false;
    ok = true;
  }
  taskEXIT_CRITICAL(&s_mux);
  return ok;
}

/** Half-square units. Origin is h1 (14, 0). Square (col,row) center is (2*col, 2*row). */
static int dist2_half(uint8_t row, uint8_t col) {
  int dx = (int)col * 2 - 14;
  int dy = (int)row * 2;
  return dx * dx + dy * dy;
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

static void tick_wave(void) {
  if (!s_have_old) {
    capture_board();
  }
  /* Farthest square from h1 is a8 at 14^2+14^2 = 392.
   * Outward radius 20 covers it. Inward starts past that (23) and shrinks
   * through h1 (radius -3) so the same crest runs backwards. */
  const int radius = s_wave_in ? (23 - (int)s_step) : (int)s_step;
  if (s_wave_in && radius <= -3) {
    paint_squares(s_new);
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
      uint8_t sq = sq_of(row, col);
      /* Disk interior is the spreading color. Inward, that disk is the lamp
       * shrinking toward h1 and the outside is the picture being revealed. */
      uint32_t outside = s_wave_in ? s_new[sq] : s_old[sq];
      uint32_t inside = s_wave_in ? s_old[sq] : s_new[sq];
      uint32_t crest = s_cover;
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
      paint_square(row, col, color);
    }
  }
  if (!s_wave_in && covered && radius >= 20) {
    paint_squares(s_new);
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

/* Head position is in 1/256 of a square. The crest eases in over 3 squares
 * and the tail fades out over 5, so the wipe is a soft brush. The head must
 * travel past a8 until that tail leaves the board black. ~3.4 s at 33 ms. */
#define SWEEP_AHEAD_Q8 768
#define SWEEP_BEHIND_Q8 1280
#define SWEEP_CREST 0xFFE6B0u
#define SNAKE_Q8_END (64u * 256u + SWEEP_BEHIND_Q8)
#define SNAKE_Q8_FWD 170u

static uint8_t sweep_glow(int dist_q8) {
  int reach = (dist_q8 >= 0) ? SWEEP_AHEAD_Q8 : SWEEP_BEHIND_Q8;
  int ad = dist_q8 < 0 ? -dist_q8 : dist_q8;
  if (ad >= reach) {
    return 0;
  }
  uint8_t t = (uint8_t)((ad * 255) / reach);
  uint8_t eased = smoothstep_u8(smoothstep_u8(t));
  return (uint8_t)(255u - eased);
}

/** Ahead of the head the saved picture stays. Behind it the square is black. */
static void paint_new_game_sweep(uint16_t q8) {
  int head = (int)q8;
  for (unsigned i = 0; i < 64u; i++) {
    uint8_t row;
    uint8_t col;
    snake_square((uint8_t)i, &row, &col);
    int dist = (int)i * 256 + 128 - head;
    uint32_t base = (dist > 0) ? s_old[sq_of(row, col)] : 0;
    paint_square(row, col, lerp_rgb(base, SWEEP_CREST, sweep_glow(dist)));
  }
}

static uint32_t sweep_button_rgb(uint16_t q8, bool reverse) {
  unsigned board = 64u * 256u;
  unsigned level;
  if (reverse) {
    level = (q8 >= board) ? 255u : ((unsigned)q8 * 255u) / board;
  } else if (q8 <= board) {
    level = 255u;
  } else {
    unsigned tail = (unsigned)SNAKE_Q8_END - board;
    unsigned gone = (unsigned)q8 - board;
    level = (gone >= tail) ? 0u : 255u - (gone * 255u) / tail;
  }
  return lerp_rgb(0, SWEEP_CREST, smoothstep_u8((uint8_t)level));
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
    for (uint8_t row = 0; row < 8; row++) {
      for (uint8_t col = 0; col < 8; col++) {
        paint_square(row, col, 0);
      }
    }
    ESP_LOGI(TAG, "[STAGING] new-game sweep finished");
    post_result(LED_GESTURE_SNAKE_DONE);
    return;
  }
  paint_new_game_sweep(s_snake_q8);
  uint32_t next = (uint32_t)s_snake_q8 + SNAKE_Q8_FWD;
  if (next > SNAKE_Q8_END) {
    next = SNAKE_Q8_END;
  }
  s_snake_q8 = (uint16_t)next;
}

static void tick_snake_reverse(void) {
  taskENTER_CRITICAL(&s_mux);
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  /* Same step as the way out. At the button the saved picture comes back
   * and the new game does not start. */
  if (s_snake_q8 == 0) {
    restore_board();
    ESP_LOGI(TAG, "[STAGING] snake reverse finished, game not started");
    post_result(LED_GESTURE_SNAKE_CANCELLED);
    return;
  }
  if (s_snake_q8 <= SNAKE_Q8_FWD) {
    s_snake_q8 = 0;
  } else {
    s_snake_q8 = (uint16_t)(s_snake_q8 - SNAKE_Q8_FWD);
  }
  paint_new_game_sweep(s_snake_q8);
}

/** IEC 5009, white's view (row 7 is rank 8). The bar stands in the top gap
 * and does not touch the ring. */
#define SHUT_RED 0xFF1408u
#define SHUT_FADE 12
#define SHUT_HOLD_END 26
#define SHUT_RING_N 16
#define SHUT_RING_FRAMES 48
#define SHUT_RING_END (SHUT_HOLD_END + SHUT_RING_FRAMES)
#define SHUT_STEM_N 8
#define SHUT_STEM_FRAMES 28
#define SHUT_STEM_END (SHUT_RING_END + SHUT_STEM_FRAMES)
#define SHUT_DARK 8
#define SHUT_DONE (SHUT_STEM_END + SHUT_DARK)
#define SHUT_TAIL_Q8 512

/* Chess {row, col}. Row 0 is rank 1, col 0 is the a-file. Clockwise from
 * the right shoulder of the gap (f7), then the bar, tip first (d8, e8). */
static const uint8_t s_shut_ring[SHUT_RING_N][2] = {
    {6, 5}, {5, 6}, {4, 7}, {3, 7}, {2, 7}, {1, 6}, {0, 5}, {0, 4},
    {0, 3}, {0, 2}, {1, 1}, {2, 0}, {3, 0}, {4, 0}, {5, 1}, {6, 2},
};
static const uint8_t s_shut_stem[SHUT_STEM_N][2] = {
    {7, 3}, {7, 4}, {5, 3}, {5, 4}, {4, 3}, {4, 4}, {3, 3}, {3, 4},
};

/** Brightness left as an extinguish head passes this point. 255 = still full. */
static uint8_t shut_trail(int head_q8, int index) {
  int d = head_q8 - index * 256;
  if (d <= 0) {
    return 255;
  }
  if (d >= SHUT_TAIL_Q8) {
    return 0;
  }
  uint8_t t = (uint8_t)((d * 255) / SHUT_TAIL_Q8);
  return (uint8_t)(255u - smoothstep_u8(t));
}

/** 0 = this square is not part of the lit symbol right now. 255 = full red. */
static uint8_t shut_glyph_level(uint8_t row, uint8_t col, uint8_t frame) {
  int ring_i = -1;
  int stem_i = -1;
  for (int i = 0; i < SHUT_RING_N; i++) {
    if (s_shut_ring[i][0] == row && s_shut_ring[i][1] == col) {
      ring_i = i;
    }
  }
  for (int i = 0; i < SHUT_STEM_N; i++) {
    if (s_shut_stem[i][0] == row && s_shut_stem[i][1] == col) {
      stem_i = i;
    }
  }
  if (ring_i < 0 && stem_i < 0) {
    return 0;
  }
  if (frame < SHUT_HOLD_END) {
    return 255;
  }
  if (frame < SHUT_RING_END) {
    if (stem_i >= 0) {
      return 255;
    }
    int span = SHUT_RING_N * 256 + SHUT_TAIL_Q8;
    int head = ((int)frame - SHUT_HOLD_END) * span / SHUT_RING_FRAMES;
    return shut_trail(head, ring_i);
  }
  if (frame < SHUT_STEM_END && stem_i >= 0) {
    int span = SHUT_STEM_N * 256 + SHUT_TAIL_Q8;
    int head = ((int)frame - SHUT_RING_END) * span / SHUT_STEM_FRAMES;
    return shut_trail(head, stem_i);
  }
  return 0;
}

static uint8_t shut_intro(uint8_t frame) {
  if (frame >= SHUT_FADE) {
    return 255;
  }
  return smoothstep_u8((uint8_t)((unsigned)frame * 255u / (SHUT_FADE - 1u)));
}

static void paint_shutdown(uint8_t frame) {
  uint8_t intro = shut_intro(frame);
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint8_t sq = sq_of(row, col);
      uint32_t bg = (frame >= SHUT_FADE) ? 0 : lerp_rgb(s_old[sq], 0, intro);
      uint8_t level = shut_glyph_level(row, col, frame);
      if (level == 0) {
        paint_square(row, col, bg);
      } else {
        uint8_t t = (uint8_t)(((unsigned)level * intro) / 255u);
        paint_square(row, col, lerp_rgb(bg, SHUT_RED, t));
      }
    }
  }
}

static uint32_t shutdown_button_rgb(uint8_t frame) {
  if (frame >= SHUT_STEM_END) {
    return 0;
  }
  if (frame >= SHUT_RING_END) {
    unsigned span = (unsigned)(SHUT_STEM_END - SHUT_RING_END);
    unsigned num = (unsigned)(frame - SHUT_RING_END) * 255u / span;
    return lerp_rgb(SHUT_RED, 0, smoothstep_u8((uint8_t)num));
  }
  return lerp_rgb(0, SHUT_RED, shut_intro(frame));
}

static void tick_shutdown(void) {
  if (!s_have_old) {
    capture_board();
  }
  bool released = false;
  taskENTER_CRITICAL(&s_mux);
  released = s_release;
  s_release = false;
  taskEXIT_CRITICAL(&s_mux);
  if (released && !s_shut_back) {
    s_shut_back = true;
  }
  /* Release runs the same frames backwards and does not power off. */
  if (s_shut_back) {
    if (s_step == 0) {
      restore_board();
      s_shut_back = false;
      ESP_LOGI(TAG, "[STAGING] shutdown reversed, power stays on");
      post_result(LED_GESTURE_SHUTDOWN_CANCELLED);
      return;
    }
    s_step--;
    paint_shutdown(s_step);
    return;
  }
  if (s_step >= SHUT_DONE) {
    for (uint8_t row = 0; row < 8; row++) {
      for (uint8_t col = 0; col < 8; col++) {
        paint_square(row, col, 0);
      }
    }
    s_btn_leave_dark = true;
    ESP_LOGI(TAG, "[STAGING] shutdown power symbol finished");
    post_result(LED_GESTURE_SHUTDOWN_DONE);
    return;
  }
  paint_shutdown(s_step);
  s_step++;
}

/** Light-blue field. amount is 0..140. The picture underneath stays saved. */
static uint32_t pair_rgb(uint8_t amount) {
  unsigned r = 48u + ((36u * (unsigned)amount) / 140u);
  unsigned g = 150u + ((55u * (unsigned)amount) / 140u);
  unsigned b = 210u + ((45u * (unsigned)amount) / 140u);
  return (r << 16) | (g << 8) | b;
}

/** Dark rune, the inverse of the field, so the mark reads as ink. */
static uint32_t pair_logo_rgb(uint8_t amount) {
  unsigned b = 6u + ((24u * (unsigned)amount) / 140u);
  return b;
}

/* Bluetooth rune in chess coordinates. {row, col}, row 0 = rank 1,
 * col 0 = file a. Rank 8 is the far side, so the mark stands upright
 * for white. Stem on the d-file, right tips on the g-file. */
static const uint8_t s_ble_logo[][2] = {
    {7, 3}, {6, 3}, {6, 4}, {5, 1}, {5, 3}, {5, 5}, {4, 2}, {4, 3}, {4, 6},
    {3, 3}, {3, 4}, {3, 5}, {2, 2}, {2, 3}, {2, 6}, {1, 1}, {1, 3}, {1, 5},
    {0, 3}, {0, 4},
};

/** ~3 s breathe. The board is pairing blue until the pulse stops. */
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
  uint32_t color = pair_rgb(amount);
  uint32_t logo = pair_logo_rgb(amount);
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      paint_square(row, col, color);
    }
  }
  for (unsigned i = 0; i < sizeof(s_ble_logo) / sizeof(s_ble_logo[0]); i++) {
    paint_square(s_ble_logo[i][0], s_ble_logo[i][1], logo);
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
    color = s_wave_in ? lerp_rgb(s_cover, 0, t) : lerp_rgb(0, s_cover, t);
    own = true;
  } else if (s_phase == PH_SNAKE_FWD || s_phase == PH_SNAKE_REV) {
    color = sweep_button_rgb(s_snake_q8, s_phase == PH_SNAKE_REV);
    own = true;
  } else if (s_phase == PH_SHUTDOWN) {
    uint8_t frame = (s_shut_back || s_step == 0) ? s_step : (uint8_t)(s_step - 1);
    color = shutdown_button_rgb(frame);
    own = true;
  } else if (s_pair_hold && s_pair_captured && s_phase == PH_IDLE) {
    color = pair_rgb(s_pair_amount);
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

bool led_gesture_start_wave(const uint32_t new_colors[64], bool inward,
                            uint32_t cover_rgb) {
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
  s_cover = cover_rgb;
  s_phase = PH_WAVE;
  s_release = false;
  s_step = 0;
  s_wave_in = inward;
  s_rev_alt = 0;
  s_have_old = false;
  if (inward) {
    for (int i = 0; i < 64; i++) {
      s_old[i] = cover_rgb;
    }
    s_have_old = true;
  }
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
