/**
 * @file game_hall_led.c
 * @brief Takeback overlay. Called once per digest, never from the 25 ms idle path.
 */

#include "game_hall_led.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "led_mapping.h"
#include "led_task.h"

void game_hall_led_show_takeback(const uint8_t *squares, uint8_t count) {
  if (squares == NULL || count == 0) {
    return;
  }
  led_clear_board_only();
  for (uint8_t i = 0; i < count; i++) {
    if (squares[i] >= 64) {
      continue;
    }
    led_set_pixel_safe(chess_pos_to_led_index(squares[i] / 8, squares[i] % 8),
                       0, 255, 255);
  }
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
