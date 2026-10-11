/**
 * @file button_gesture.h
 * @brief One-button gesture decoder for the V2 hall PCB (GPIO23).
 *
 * The reed image keeps GPIO15 reset behaviour. This decoder is idle unless
 * CONFIG_CHESS_GESTURE_BUTTON is set.
 */
#ifndef BUTTON_GESTURE_H
#define BUTTON_GESTURE_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  GESTURE_NONE = 0,
  GESTURE_TOGGLE,        ///< One short click, then quiet: game <-> lamp
  GESTURE_PAIR,          ///< Three short clicks: BLE pairing
  GESTURE_SNAKE_START,   ///< Hold from idle: new-game snake
  GESTURE_SHUTDOWN_START ///< Click, then hold: power off
} gesture_event_t;

void button_gesture_init(void);
void button_gesture_poll(void);

/** Stable, debounced level. True while the gesture button is held. */
bool button_gesture_is_down(void);

/** Queue of gesture_event_t. NULL when the gesture button is not enabled. */
QueueHandle_t button_gesture_queue(void);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_GESTURE_H */
