/**
 * @file led_gesture_anim.h
 * @brief Non-blocking board animations for the V2 gesture button.
 *
 * Frames advance from the LED task tick. Callers only start, signal release,
 * and collect a completion code. Pixel writes stay inside led_task.
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

/**
 * @brief Whether a gesture animation is running.
 * @return true when the phase is not idle.
 */
bool led_gesture_anim_busy(void);

/** @brief Advance one frame. Called from the LED task. */
void led_gesture_anim_tick(void);

/**
 * @brief Take the completion code and clear it.
 * @return The latched result, or LED_GESTURE_RESULT_NONE if nothing finished.
 */
led_gesture_result_t led_gesture_take_result(void);

/**
 * @brief Wave from the physical button (edge between a1 and b1, just outside rank 1).
 *
 * The mode flag itself is not changed here. Fails if new_colors is NULL or
 * another gesture is already running.
 * @param new_colors 64 LED-index colors for the mode being revealed.
 * @return true if the wave started.
 */
bool led_gesture_start_wave(const uint32_t new_colors[64]);

/**
 * @brief Boot-style green fill along LED index order. Reverse on early release.
 * @return true if the snake started (idle). false if another gesture is running.
 */
bool led_gesture_start_snake(void);

/**
 * @brief Chase along the black player's squares, then the board goes dark.
 * @return true if shutdown started. false if another gesture is running.
 */
bool led_gesture_start_shutdown(void);

/**
 * @brief Calm blue pulse for as long as the pairing window is open.
 *
 * Does not take the exclusive gesture phase, so a lamp toggle or another
 * triple-click still runs. Stop restores the board picture from when the
 * pulse last had the squares to itself.
 * @return true once the pulse is requested.
 */
bool led_gesture_start_pair_pulse(void);

/** @brief End the pairing pulse and give the squares back to the game. */
void led_gesture_stop_pair_pulse(void);

/**
 * @brief Button went up.
 *
 * Cancels a forward snake (plays it backwards) or a shutdown that has not
 * finished. A release during the reverse does nothing. Wave and pulse ignore
 * the latch. The flag is stored even if the LED phase has not started yet.
 */
void led_gesture_note_release(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_GESTURE_ANIM_H */
