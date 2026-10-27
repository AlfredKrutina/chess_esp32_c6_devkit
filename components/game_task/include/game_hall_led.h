/**
 * @file game_hall_led.h
 * @brief One-shot Hall overlays that are not move hints or the restore guide.
 */
#ifndef GAME_HALL_LED_H
#define GAME_HALL_LED_H

#include "sdkconfig.h"
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

/** Cyan on the squares of a takeback that is waiting out its stable window. */
void game_hall_led_show_takeback(const uint8_t *squares, uint8_t count);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_LED_H */
