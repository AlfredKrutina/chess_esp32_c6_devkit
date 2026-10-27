/**
 * @file game_hall_led.c
 * @brief Takeback overlay. Called once per digest, never from the 25 ms idle path.
 */

#include "game_hall_led.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "led_mapping.h"
#include "led_task.h"

void game_hall_led_show_takeback(const uint8_t *squares, uint8_t count) {
  uint8_t leds[8];
  uint8_t n = 0;
  uint8_t i;
  if (squares == NULL || count == 0) {
    led_takeback_clear();
    return;
  }
  led_target_begin();
  for (i = 0; i < count && n < 8; i++) {
    uint8_t led;
    if (squares[i] >= 64) {
      continue;
    }
    led = chess_pos_to_led_index(squares[i] / 8, squares[i] % 8);
    leds[n++] = led;
    led_target_set(led, 0, 255, 255, LED_VIS_BREATHE);
  }
  led_target_end();
  led_takeback_arm(leds, n);
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
