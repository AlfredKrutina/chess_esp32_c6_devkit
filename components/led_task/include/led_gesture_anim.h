/**
 * @file led_gesture_anim.h
 * @brief Non-blocking board animations for the V2 gesture button.
 *
 * Frames advance from the LED task tick. Callers only start, signal release,
 * and collect a completion code.
 */
#ifndef LED_GESTURE_ANIM_H
#define LED_GESTURE_ANIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LED_GESTURE_RESULT_NONE = 0,
  LED_GESTURE_WAVE_DONE,
  LED_GESTURE_SNAKE_DONE,
  LED_GESTURE_SNAKE_CANCELLED,
  LED_GESTURE_SHUTDOWN_DONE,
  LED_GESTURE_SHUTDOWN_CANCELLED,
  LED_GESTURE_PULSE_DONE
} led_gesture_result_t;

bool led_gesture_anim_busy(void);
void led_gesture_anim_tick(void);
led_gesture_result_t led_gesture_take_result(void);

/**
 * Wave from the physical button (edge between a1 and b1, just outside rank 1).
 * `new_colors` is 64 LED-index colors for the mode being revealed.
 * The mode flag itself is not changed here.
 */
bool led_gesture_start_wave(const uint32_t new_colors[64]);

/** Boot-style green fill along LED index order. Reverse on early release. */
bool led_gesture_start_snake(void);

/** Chase along the black player's squares, then the board goes dark. */
bool led_gesture_start_shutdown(void);

/** One blue pulse, then the previous board colors return. */
bool led_gesture_start_pair_pulse(void);

/**
 * Button went up. Cancels a forward snake (plays it backwards) or a shutdown
 * that has not finished. A release during the reverse does nothing.
 */
void led_gesture_note_release(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_GESTURE_ANIM_H */
