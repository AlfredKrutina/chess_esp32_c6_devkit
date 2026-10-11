/**
 * @file game_gesture.c
 * @brief Applies gesture results: lamp/game commit, new game, pairing, sleep.
 *
 * The wave only previews colors. ha_light_request_mode() runs after the wave
 * covers every square, so a reset mid-wave leaves the previous mode.
 */

#include "game_gesture.h"

#include "sdkconfig.h"

#include "button_gesture.h"
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

#include <string.h>

#if CONFIG_CHESS_GESTURE_BUTTON

#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

static const char *TAG = "GAME_GESTURE";

static bool s_wave_wait;
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
  memset(out, 0, 64 * sizeof(uint32_t));
  if (!game_active) {
    fill_checker(out);
    return;
  }
  if (piece_lifted) {
    out[chess_pos_to_led_index(lifted_piece_row, lifted_piece_col)] = 0xFFFF00u;
    move_suggestion_t suggestions[32];
    uint32_t n = game_get_available_moves(lifted_piece_row, lifted_piece_col,
                                          suggestions, 32);
    for (uint32_t i = 0; i < n; i++) {
      piece_t dest = board[suggestions[i].to_row][suggestions[i].to_col];
      bool enemy = (current_player == PLAYER_WHITE && dest >= PIECE_BLACK_PAWN &&
                    dest <= PIECE_BLACK_KING) ||
                   (current_player == PLAYER_BLACK && dest >= PIECE_WHITE_PAWN &&
                    dest <= PIECE_WHITE_KING);
      out[chess_pos_to_led_index(suggestions[i].to_row, suggestions[i].to_col)] =
          enemy ? 0xFFA500u : 0x00FF00u;
    }
    return;
  }
  if (castling_state.in_progress ||
      !chess_policy_move_hints_movable_yellow() ||
      !game_led_guidance_show_movable_yellow()) {
    return;
  }
  (void)esp_task_wdt_reset();
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      bool mine = (current_player == PLAYER_WHITE && piece >= PIECE_WHITE_PAWN &&
                   piece <= PIECE_WHITE_KING) ||
                  (current_player == PLAYER_BLACK && piece >= PIECE_BLACK_PAWN &&
                   piece <= PIECE_BLACK_KING);
      if (!mine) {
        continue;
      }
      move_suggestion_t one;
      if (game_get_available_moves(row, col, &one, 1) > 0) {
        out[chess_pos_to_led_index(row, col)] = 0xFFFF00u;
      }
    }
  }
}

static void fill_mode_colors(ha_mode_t mode, uint32_t out[64]) {
  if (mode == HA_MODE_HA) {
    uint8_t r = 255, g = 255, b = 255, bright = 255;
    bool on = true;
    ha_light_get_state(&r, &g, &b, &bright, &on);
    uint32_t color = 0;
    if (on) {
      color = rgb_of((uint8_t)((r * bright) / 255),
                     (uint8_t)((g * bright) / 255),
                     (uint8_t)((b * bright) / 255));
    }
    for (int i = 0; i < 64; i++) {
      out[i] = color;
    }
    return;
  }
  fill_game_colors(out);
}

static void gesture_power_off(void) {
  gpio_num_t pin = (gpio_num_t)CONFIG_CHESS_GESTURE_BUTTON_GPIO;
  for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    led_set_pixel_internal((uint8_t)i, 0, 0, 0);
  }
  led_force_immediate_update();
  ha_light_persist_mode();

  /* The hold that finished the chase must be released first, otherwise a
   * level-low wake fires immediately. */
  while (gpio_get_level(pin) == 0) {
    (void)esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  vTaskDelay(pdMS_TO_TICKS(25));

  gpio_wakeup_disable(pin);
  gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
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
  if (sl != ESP_OK) {
    ESP_LOGE(TAG, "light sleep rejected: %s", esp_err_to_name(sl));
    (void)esp_wifi_start();
    gpio_wakeup_disable(pin);
    if (ha_light_get_mode() == HA_MODE_HA) {
      (void)ha_light_request_mode(HA_MODE_HA);
    } else {
      game_refresh_leds();
    }
    return;
  }
  while (gpio_get_level(pin) == 0) {
    (void)esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(25));
  }
  ESP_LOGI(TAG, "[STAGING] wake on GPIO%d, restarting as power-on",
           CONFIG_CHESS_GESTURE_BUTTON_GPIO);
  esp_restart();
#endif
}

static void handle_event(gesture_event_t ev) {
  if (led_gesture_anim_busy() && ev != GESTURE_NONE) {
    ESP_LOGI(TAG, "[STAGING] gesture dropped, animation busy");
    return;
  }
  switch (ev) {
  case GESTURE_TOGGLE: {
    ha_mode_t next =
        (ha_light_get_mode() == HA_MODE_HA) ? HA_MODE_GAME : HA_MODE_HA;
    uint32_t colors[64];
    fill_mode_colors(next, colors);
    if (!led_gesture_start_wave(colors)) {
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
  case GESTURE_PAIR:
    ble_task_request_pairing();
    if (!led_gesture_start_pair_pulse()) {
      ESP_LOGW(TAG, "pair pulse did not start");
    }
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
  if (q != NULL) {
    gesture_event_t ev;
    while (xQueueReceive(q, &ev, 0) == pdTRUE) {
      handle_event(ev);
    }
  }
  handle_result(led_gesture_take_result());
}

#else /* !CONFIG_CHESS_GESTURE_BUTTON */

void game_gesture_poll(void) {
  (void)led_gesture_take_result();
}

#endif
