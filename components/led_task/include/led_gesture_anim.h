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

/** Held SW1. The LED task lights rank 1 and rank 8 until a gesture animation starts. */
void led_gesture_set_button_held(bool held);

/** Active-low. True for the gesture GPIO or for GPIO15 when it is not an NRST pin. */
bool led_gesture_button_down(void);

/**
 * @brief Take the completion code and clear it.
 * @return The latched result, or LED_GESTURE_RESULT_NONE if nothing finished.
 */
led_gesture_result_t led_gesture_take_result(void);

/**
 * @brief Wave between the current board and new_colors.
 *
 * Outward (inward == false) spreads from the button on h1 and covers the
 * board. Inward plays that wave backwards: the current picture shrinks back
 * into h1 and new_colors show from the far corner. The mode flag itself is
 * not changed here. Fails if new_colors is NULL or another gesture is already
 * running.
 * @param new_colors 64 chess-order colors (index = row * 8 + col, row 0 = rank 1,
 *                   col 0 = file a) for the picture left when the wave ends.
 * @param inward true to run the wave from the far corner back to h1.
 * @param cover_rgb Color of the light disk. Outward this is what spreads.
 *                  Inward this is what shrinks back into h1.
 * @return true if the wave started.
 */
bool led_gesture_start_wave(const uint32_t new_colors[64], bool inward,
                            uint32_t cover_rgb);

/**
 * @brief Smooth sweep from h1 that wipes the board to black. Reverse on early release.
 * A second call while this sweep is running replays it once from h1.
 * @return true if the snake started or restarted. false if another gesture is running.
 */
bool led_gesture_start_snake(void);

/** @brief True while the new-game sweep is moving forward or back. */
bool led_gesture_snake_active(void);

/**
 * @brief Red power symbol, then the ring goes out and the bar sinks.
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
 * Cancels a forward snake or a shutdown by playing it backwards from the
 * current frame. Neither powers off or starts a game. A release during the
 * reverse does nothing. Wave and pulse ignore
 * the latch. The flag is stored even if the LED phase has not started yet.
 */
void led_gesture_note_release(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_GESTURE_ANIM_H */
