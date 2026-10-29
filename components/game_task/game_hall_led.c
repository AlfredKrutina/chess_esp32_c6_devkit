/**
 * @file game_hall_led.c
 * @brief Takeback overlay via game events.
 */

#include "game_hall_led.h"

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

#include "game_events.h"

void game_hall_led_show_takeback(const uint8_t *squares, uint8_t count) {
  game_event_post_takeback(squares, count);
}

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */
