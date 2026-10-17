/**
 * @file game_gesture.c
 * @brief Applies gesture results: lamp/game commit, new game, pairing, sleep.
 *
 * The wave only previews colors. ha_light_request_mode() runs after the wave
 * covers every square, so a reset mid-wave leaves the previous mode.
 */

#include "freertos_chess.h"
#include "game_gesture.h"

#include "sdkconfig.h"

#include "button_gesture.h"
#include "hall_i2c_squares.h"
#include "chess_gameplay_policy.h"
#include "game_task.h"
#include "game_task_internal.h"
#include "led_gesture_anim.h"
#include "led_mapping.h"

#include "../ble_task/include/ble_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "GAME_GESTURE";

enum {
  DEMO_OFF = 0,
  DEMO_MOVE,
  DEMO_END,
  DEMO_LAMP,
  DEMO_COLOR,
  DEMO_PAIR,
  DEMO_CLOSE
};

/* Scholar's mate. Stored in flash, not as a saved demo state. */
static const uint8_t k_demo_game[][4] = {
    {1, 4, 3, 4}, /* e2 e4 */
    {6, 4, 4, 4}, /* e7 e5 */
    {0, 5, 3, 2}, /* f1 c4 */
    {7, 1, 5, 2}, /* b8 c6 */
    {0, 3, 4, 7}, /* d1 h5 */
    {7, 6, 5, 5}, /* g8 f6 */
    {4, 7, 6, 5}, /* h5 f7 mate */
};

static uint8_t s_demo_phase;
static uint8_t s_demo_color;
static uint8_t s_demo_ply;
static bool s_demo_sent;
static TickType_t s_demo_until;

static const uint8_t k_demo_colors[][3] = {
    {255, 0, 0},   /* red */
    {0, 255, 0},   /* green */
    {0, 0, 255},   /* blue */
    {255, 255, 0}, /* yellow */
};

static void demo_show_stop_leds(void) {
  led_gesture_stop_pair_pulse();
  led_stop_endgame_animation();
  led_set_lamp_hold(false);
}

static void demo_play_move(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                           uint8_t to_col) {
  static uint8_t to_led;
  to_led = chess_pos_to_led_index(to_row, to_col);
  led_command_t cmd = {
      .type = LED_CMD_ANIM_MOVE_PATH,
      .led_index = chess_pos_to_led_index(from_row, from_col),
      .red = 0,
      .green = 0,
      .blue = 255,
      .duration_ms = 0,
      .data = &to_led,
  };
  led_execute_command_new(&cmd);
}

static void demo_play_end(void) {
  led_command_t cmd = {
      .type = LED_CMD_ANIM_ENDGAME,
      .led_index = chess_pos_to_led_index(0, 4),
      .red = 0,
      .green = 0,
      .blue = 0,
      .duration_ms = 0,
      .data = NULL,
  };
  led_execute_command_new(&cmd);
}

static void demo_paint(uint8_t index) {
  led_set_lamp_hold(true);
  led_set_ha_color(k_demo_colors[index][0], k_demo_colors[index][1],
                   k_demo_colors[index][2], 255);
}

static bool demo_wave(bool inward, uint8_t index) {
  uint32_t color = ((uint32_t)k_demo_colors[index][0] << 16) |
                   ((uint32_t)k_demo_colors[index][1] << 8) |
                   (uint32_t)k_demo_colors[index][2];
  uint32_t colors[64];
  for (int i = 0; i < 64; i++) {
    colors[i] = inward ? 0 : color;
  }
  return led_gesture_start_wave(colors, inward, color);
}

static void demo_show_tick(void) {
  if (s_demo_phase == DEMO_OFF) {
    return;
  }
  TickType_t now = xTaskGetTickCount();
  switch (s_demo_phase) {
  case DEMO_MOVE:
    if ((int32_t)(now - s_demo_until) < 0) {
      break;
    }
    led_set_lamp_hold(false);
    if (s_demo_ply >= (uint8_t)(sizeof(k_demo_game) / sizeof(k_demo_game[0]))) {
      s_demo_sent = false;
      s_demo_phase = DEMO_END;
      break;
    }
    demo_play_move(k_demo_game[s_demo_ply][0], k_demo_game[s_demo_ply][1],
                   k_demo_game[s_demo_ply][2], k_demo_game[s_demo_ply][3]);
    s_demo_ply++;
    s_demo_until = xTaskGetTickCount() + pdMS_TO_TICKS(900);
    break;
  case DEMO_END:
    if (!s_demo_sent) {
      demo_play_end();
      s_demo_sent = true;
      s_demo_until = now + pdMS_TO_TICKS(5500);
      break;
    }
    if ((int32_t)(now - s_demo_until) < 0) {
      break;
    }
    led_stop_endgame_animation();
    s_demo_sent = false;
    s_demo_phase = DEMO_LAMP;
    break;
  case DEMO_LAMP:
    if (!s_demo_sent) {
      if (!demo_wave(false, 0)) {
        break;
      }
      s_demo_sent = true;
      break;
    }
    if (led_gesture_anim_busy()) {
      break;
    }
    (void)led_gesture_take_result();
    demo_paint(0);
    s_demo_color = 1;
    s_demo_sent = false;
    s_demo_until = now + pdMS_TO_TICKS(1500);
    s_demo_phase = DEMO_COLOR;
    break;
  case DEMO_COLOR:
    if ((int32_t)(now - s_demo_until) < 0) {
      break;
    }
    if (s_demo_color < (uint8_t)(sizeof(k_demo_colors) / sizeof(k_demo_colors[0]))) {
      demo_paint(s_demo_color);
      s_demo_color++;
      s_demo_until = now + pdMS_TO_TICKS(1500);
      break;
    }
    if (!led_gesture_start_pair_pulse()) {
      break;
    }
    s_demo_until = now + pdMS_TO_TICKS(3500);
    s_demo_phase = DEMO_PAIR;
    break;
  case DEMO_PAIR:
    if ((int32_t)(now - s_demo_until) < 0) {
      break;
    }
    led_gesture_stop_pair_pulse();
    s_demo_sent = false;
    s_demo_phase = DEMO_CLOSE;
    break;
  case DEMO_CLOSE:
    if (!s_demo_sent) {
      if (!demo_wave(true, (uint8_t)(sizeof(k_demo_colors) / sizeof(k_demo_colors[0]) - 1))) {
        break;
      }
      s_demo_sent = true;
      break;
    }
    if (led_gesture_anim_busy()) {
      break;
    }
    (void)led_gesture_take_result();
    led_set_lamp_hold(false);
    s_demo_sent = false;
    s_demo_ply = 0;
    s_demo_until = 0;
    s_demo_phase = DEMO_MOVE;
    break;
  default:
    s_demo_phase = DEMO_OFF;
    break;
  }
}

void game_demo_show_set(bool on) {
  if (!on) {
    if (s_demo_phase == DEMO_OFF) {
      return;
    }
    s_demo_phase = DEMO_OFF;
    s_demo_sent = false;
    demo_show_stop_leds();
    ESP_LOGI(TAG, "demo show off");
    return;
  }
  if (s_demo_phase != DEMO_OFF) {
    return;
  }
  demo_show_stop_leds();
  s_demo_sent = false;
  s_demo_color = 0;
  s_demo_ply = 0;
  s_demo_until = 0;
  s_demo_phase = DEMO_MOVE;
  ESP_LOGI(TAG, "demo show on");
}

bool game_demo_show_active(void) { return s_demo_phase != DEMO_OFF; }

#if CONFIG_CHESS_GESTURE_BUTTON

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

static bool s_wave_wait;
static bool s_opening_wave;
static bool s_opening_wave;
static bool s_pair_pulse_on;
static ha_mode_t s_pending_mode;

static uint32_t rgb_of(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void fill_checker(uint32_t out[64]) {
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      uint32_t color = ((row + col) & 1u) ? 0xFFFFFFu : 0u;
      out[chess_pos_to_led_index(row, col)] = color;
    }
  }
}

static void fill_game_colors(uint32_t out[64]) {
  /* Quiet board only. Movable-piece yellow, lift highlights, and destination
   * greens belong to the game LED refresh after the wave commits. */
  if (!game_active) {
    fill_checker(out);
    return;
  }
  memset(out, 0, 64 * sizeof(uint32_t));
}

static uint32_t lamp_rgb(void) {
  uint8_t r = 255, g = 255, b = 255, bright = 255;
  bool on = true;
  ha_light_get_state(&r, &g, &b, &bright, &on);
  if (!on) {
    return 0;
  }
  return rgb_of((uint8_t)((r * bright) / 255), (uint8_t)((g * bright) / 255),
                (uint8_t)((b * bright) / 255));
}

static void fill_mode_colors(ha_mode_t mode, uint32_t out[64]) {
  if (mode == HA_MODE_HA) {
    uint32_t color = lamp_rgb();
    for (int i = 0; i < 64; i++) {
      out[i] = color;
    }
    return;
  }
  fill_game_colors(out);
}

static void gesture_power_off(void) {
  gpio_num_t pin = (gpio_num_t)CONFIG_CHESS_GESTURE_BUTTON_GPIO;
  led_set_forced_blackout(true);
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    led_set_pixel_internal((uint8_t)i, 0, 0, 0);
  }
  led_force_immediate_update();
  ha_light_persist_mode();

  /* The hold that finished the chase must be released first, otherwise a
   * level-low wake fires immediately. */
  while (led_gesture_button_down()) {
    (void)esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  vTaskDelay(pdMS_TO_TICKS(25));

  gpio_wakeup_disable(pin);
  gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  if ((int)BUTTON_RESET != (int)pin &&
      !chess_gpio_pin_is_stm32_nrst_output((int)BUTTON_RESET)) {
    gpio_wakeup_disable(BUTTON_RESET);
    gpio_wakeup_enable(BUTTON_RESET, GPIO_INTR_LOW_LEVEL);
  }
  esp_sleep_enable_gpio_wakeup();

#if ((SOC_GPIO_DEEP_SLEEP_WAKE_VALID_GPIO_MASK) & \
     (1ULL << CONFIG_CHESS_GESTURE_BUTTON_GPIO))
  ESP_LOGI(TAG, "[STAGING] power off: deep sleep, wake GPIO%d low",
           CONFIG_CHESS_GESTURE_BUTTON_GPIO);
  esp_deep_sleep_enable_gpio_wakeup(1ULL << CONFIG_CHESS_GESTURE_BUTTON_GPIO,
                                    ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
#else
  /* ESP32-C6 deep-sleep GPIO wake is only GPIO0-7. IO23 is not one of them
   * (and GPIO7 is the LED data pin). The 5 V rail has no load switch, so
   * "off" is LEDs dark plus light sleep. The next press wakes the CPU and
   * esp_restart() boots like a power-on. USB power stays connected. */
  ESP_LOGI(TAG,
           "[STAGING] power off: light sleep (GPIO%d cannot deep-sleep-wake), "
           "USB 5 V stays on",
           CONFIG_CHESS_GESTURE_BUTTON_GPIO);
  (void)esp_wifi_stop();
  esp_err_t sl = esp_light_sleep_start();
  /* USB-Serial/JTAG wakes light sleep at once while the button is still
   * released. That used to fall through into esp_restart(), so "off" never
   * stuck and the next press was not a power-on. */
  bool button_woke = (sl == ESP_OK && led_gesture_button_down());
  if (!button_woke) {
    if (sl != ESP_OK) {
      ESP_LOGW(TAG, "light sleep rejected (%s) — staying dark until SW1",
               esp_err_to_name(sl));
    } else {
      ESP_LOGW(TAG, "spurious wake — staying dark until SW1");
    }
    gpio_wakeup_disable(pin);
    for (;;) {
      (void)esp_task_wdt_reset();
      if (led_gesture_button_down()) {
        vTaskDelay(pdMS_TO_TICKS(25));
        if (led_gesture_button_down()) {
          while (led_gesture_button_down()) {
            (void)esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(25));
          }
          vTaskDelay(pdMS_TO_TICKS(25));
          ESP_LOGI(TAG, "[STAGING] power on from GPIO%d, restart",
                   CONFIG_CHESS_GESTURE_BUTTON_GPIO);
          esp_restart();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(25));
    }
  }
  while (led_gesture_button_down()) {
    (void)esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  ESP_LOGI(TAG, "[STAGING] wake on GPIO%d, restarting as power-on",
           CONFIG_CHESS_GESTURE_BUTTON_GPIO);
  esp_restart();
#endif
}

static bool gesture_in_puzzle(void) {
  return game_is_puzzle_active() || game_is_puzzle_setup_active();
}

static void gesture_next_puzzle(void) {
  uint8_t id = puzzle_active_id != 0 ? puzzle_active_id : puzzle_setup_id;
  uint8_t next = 1;
  if (id >= 1 && id < 5) {
    next = (uint8_t)(id + 1);
  }
  if (!game_puzzle_start(next)) {
    ESP_LOGW(TAG, "next puzzle %u did not start", (unsigned)next);
  }
}

static void gesture_begin_opening(void) {
  uint32_t colors[64];
  for (int i = 0; i < 64; i++) {
    uint8_t t = (uint8_t)((i * 255) / 63);
    colors[i] = ((uint32_t)(40 + t / 4) << 16) | ((uint32_t)(80 + t / 3) << 8) |
                (uint32_t)(180 + t / 5);
  }
  if (led_gesture_start_wave(colors, false, 0x2864C8u)) {
    s_opening_wave = true;
    s_wave_wait = true;
    return;
  }
  (void)game_opening_ensure_and_start();
}

static void handle_event(gesture_event_t ev) {
  if (gesture_in_puzzle() && ev == GESTURE_SNAKE_START) {
    gesture_next_puzzle();
    return;
  }
  if (gesture_in_puzzle() && ev == GESTURE_COLOR) {
    gesture_begin_opening();
    return;
  }
  if (ev == GESTURE_DEMO) {
    game_demo_show_set(!game_demo_show_active());
    return;
  }
  if (game_demo_show_active() && ev != GESTURE_NONE) {
    game_demo_show_set(false);
    return;
  }
  if (ev == GESTURE_SNAKE_START || ev == GESTURE_SHUTDOWN_START) {
    if (!led_gesture_anim_busy()) {
      bool started = (ev == GESTURE_SNAKE_START) ? led_gesture_start_snake()
                                                 : led_gesture_start_shutdown();
      if (!started) {
        ESP_LOGW(TAG, "gesture animation did not start");
      }
    }
    return;
  }
  if (led_gesture_anim_busy() && ev != GESTURE_NONE) {
    ESP_LOGI(TAG, "[STAGING] gesture dropped, animation busy");
    return;
  }
  switch (ev) {
  case GESTURE_TOGGLE: {
    if (hall_magnet_bench_active()) {
      hall_magnet_bench_capture();
      break;
    }
    /* One click while pairing is open closes the window. The lamp wave must
     * not start, or it fights the blue pulse. */
    if (ble_task_pairing_window_is_open() &&
        ble_task_claim_pairing_success()) {
      ESP_LOGI(TAG, "[STAGING] single click closes pairing");
      ble_task_finish_pairing_window();
      break;
    }
    ha_mode_t next =
        (ha_light_get_mode() == HA_MODE_HA) ? HA_MODE_GAME : HA_MODE_HA;
    uint32_t colors[64];
    uint32_t lamp = lamp_rgb();
    fill_mode_colors(next, colors);
    /* Opening spreads the last lamp color. Closing shrinks that same color. */
    bool inward = (next == HA_MODE_GAME);
    if (!led_gesture_start_wave(colors, inward, lamp)) {
      ESP_LOGW(TAG, "wave did not start");
      return;
    }
    s_pending_mode = next;
    s_wave_wait = true;
    ESP_LOGI(TAG, "[STAGING] wave preview %s -> %s (commit after cover)",
             next == HA_MODE_HA ? "game" : "lamp",
             next == HA_MODE_HA ? "lamp" : "game");
    break;
  }
  case GESTURE_COLOR: {
    if (ha_light_get_mode() != HA_MODE_HA) {
      ESP_LOGI(TAG, "[STAGING] double click ignored outside lamp");
      break;
    }
    static const uint8_t k_lamp_colors[][3] = {
        {255, 255, 255}, /* daylight white */
        {255, 214, 170}, /* warm white */
        {0, 0, 255},     /* blue */
        {255, 0, 0},     /* red */
    };
    uint8_t r = 0, g = 0, b = 0, bright = 255;
    bool on = true;
    ha_light_get_state(&r, &g, &b, &bright, &on);
    unsigned next = 0;
    for (unsigned i = 0; i < 4u; i++) {
      if (r == k_lamp_colors[i][0] && g == k_lamp_colors[i][1] &&
          b == k_lamp_colors[i][2]) {
        next = (i + 1u) % 4u;
        break;
      }
    }
    if (!ha_light_request_web_lamp(true, k_lamp_colors[next][0],
                                   k_lamp_colors[next][1],
                                   k_lamp_colors[next][2])) {
      ESP_LOGW(TAG, "lamp color queue full");
      break;
    }
    ESP_LOGI(TAG, "[STAGING] lamp color %u RGB(%u,%u,%u)", next,
             k_lamp_colors[next][0], k_lamp_colors[next][1],
             k_lamp_colors[next][2]);
    break;
  }
  case GESTURE_PAIR:
    ble_task_request_pairing();
    break;
  case GESTURE_SNAKE_START:
    if (!led_gesture_start_snake()) {
      ESP_LOGW(TAG, "snake did not start");
    }
    break;
  case GESTURE_SHUTDOWN_START:
    if (!led_gesture_start_shutdown()) {
      ESP_LOGW(TAG, "shutdown chase did not start");
    }
    break;
  case GESTURE_NONE:
  default:
    break;
  }
}

static void handle_result(led_gesture_result_t result) {
  switch (result) {
  case LED_GESTURE_WAVE_DONE:
    if (!s_wave_wait) {
      break;
    }
    s_wave_wait = false;
    if (s_opening_wave) {
      s_opening_wave = false;
      (void)game_opening_ensure_and_start();
      break;
    }
    if (!ha_light_request_mode(s_pending_mode)) {
      ESP_LOGW(TAG, "mode commit queue full");
    } else {
      ESP_LOGI(TAG, "[STAGING] mode committed: %s",
               s_pending_mode == HA_MODE_HA ? "lamp" : "game");
    }
    break;
  case LED_GESTURE_SNAKE_DONE:
    if (!button_gesture_is_down()) {
      ESP_LOGI(TAG, "[STAGING] snake ended after release, no new game");
      break;
    }
    if (ha_light_get_mode() == HA_MODE_HA) {
      (void)ha_light_request_mode(HA_MODE_GAME);
    }
    ESP_LOGI(TAG, "[STAGING] new game (snake finished while held)");
    game_start_new_game();
    break;
  case LED_GESTURE_SNAKE_CANCELLED:
    ESP_LOGI(TAG, "[STAGING] new game cancelled");
    break;
  case LED_GESTURE_SHUTDOWN_DONE:
    gesture_power_off();
    break;
  case LED_GESTURE_SHUTDOWN_CANCELLED:
    ESP_LOGI(TAG, "[STAGING] power off cancelled");
    break;
  case LED_GESTURE_PULSE_DONE:
  case LED_GESTURE_RESULT_NONE:
  default:
    break;
  }
}

void game_gesture_poll(void) {
  QueueHandle_t q = button_gesture_queue();
  if (q != NULL && !s_wave_wait && !led_gesture_anim_busy()) {
    gesture_event_t ev;
    if (xQueueReceive(q, &ev, 0) == pdTRUE) {
      handle_event(ev);
    }
  }
  if (!game_demo_show_active()) {
    bool want_pulse = ble_task_pairing_pulse_wanted();
    if (want_pulse != s_pair_pulse_on) {
      if (want_pulse) {
        (void)led_gesture_start_pair_pulse();
        ESP_LOGI(TAG, "[STAGING] pairing pulse on");
      } else {
        led_gesture_stop_pair_pulse();
        ESP_LOGI(TAG, "[STAGING] pairing pulse off");
      }
      s_pair_pulse_on = want_pulse;
    }
  }
  demo_show_tick();
  handle_result(led_gesture_take_result());
}

#else /* !CONFIG_CHESS_GESTURE_BUTTON */

void game_gesture_poll(void) {
  demo_show_tick();
  (void)led_gesture_take_result();
}

#endif
