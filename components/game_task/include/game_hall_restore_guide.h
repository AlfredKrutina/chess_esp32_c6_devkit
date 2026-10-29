/**
 * @file game_hall_restore_guide.h
 * @brief Hall type/color board-restore matching and the LED guide around it.
 *
 * Compiled only for CONFIG_CHESS_MATRIX_INPUT_I2C_HALL. Uses the uncalibrated
 * Hall POC (see hall_i2c_squares.h). LED output goes through led_task.
 */
#ifndef GAME_HALL_RESTORE_GUIDE_H
#define GAME_HALL_RESTORE_GUIDE_H

#include "sdkconfig.h"
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

/**
 * @brief Whether the restore-guide LED overlay is currently on.
 * @return true while guide LEDs are active.
 */
bool game_hall_restore_guide_is_active(void);

/**
 * @brief Recompute the plan from stable sensors.
 *
 * May enter or leave the squares-guard pause and the LED guide. Returns
 * immediately when Hall readings are not usable yet.
 */
void game_hall_restore_guide_sync(void);

/**
 * @brief Note a matrix scan and request sync if the occ/type/color digest changed.
 * @param occ 64 occupancy bytes. Must be non-NULL.
 */
void game_hall_restore_guide_note_stable_squares(const uint8_t occ[64]);

/** @brief Ask game_task to call game_hall_restore_guide_sync() soon. */
void game_hall_restore_guide_request_sync(void);

/** @brief Drop the sensor digest and sync even if the pieces did not move. */
void game_hall_restore_guide_force_resync(void);

/** @brief Opening colors are done. A lifted piece is a move, not setup. */
void game_hall_restore_guide_end_for_play(void);

/** Start the clock once setup has handed the board to an active game. */
void game_hall_opening_clock_on_play(void);

/** @brief True after the opening guide has been retired for this game. */
bool game_hall_restore_guide_in_play(void);

/**
 * @brief Draw restore paths from the sensors back to board[][] during a game.
 * Does not arm the matrix-guard baseline. @return true when a plan was applied
 * or the board is already aligned.
 */
bool game_hall_restore_guide_show_play(void);

/**
 * @brief Guide sensors onto the current board[][] without retiring the opening.
 * Debounced like the opening sync. Does not start the clock.
 * @return true when the pieces already match.
 */
bool game_hall_restore_guide_show_target(void);

/**
 * @brief Live in-play guide back to board[][].
 *
 * Same jobs and paths as the opening sync, debounced the same way.
 * Does not retire the opening, start the clock, or fade an empty board.
 */
void game_hall_restore_guide_sync_play(void);

/**
 * @brief Light every starting home while the board is empty.
 *
 * Holds about 5 s. A piece placed on the board ends the hold and the normal
 * guide takes over. If the board stays empty, the glow fades out.
 * @return true when the opening glow is showing.
 */
bool game_hall_restore_guide_show_opening(void);

/**
 * @brief Take and clear a pending matrix-triggered sync request.
 * @return true if a request was pending.
 */
bool game_hall_restore_guide_take_sync_request(void);

/**
 * @brief Turn guide LEDs off and clear guide-local state.
 *
 * The squares-guard pause layer is left as-is.
 */
void game_hall_restore_guide_stop_leds(void);

/** Drop the guide flag without fading squares to black. */
void game_hall_restore_guide_release(void);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_RESTORE_GUIDE_H */
