/**
 * @file game_squares_guard.h
 * @brief Pause play when the physical board disagrees with the logical position.
 *
 * JSON keys and Kconfig stay matrix_guard_* / CONFIG_CHESS_MATRIX_*. This module
 * only decides pause, LED layers, and resync. It does not merge reed GPIO with
 * Hall I2C.
 */
#ifndef GAME_SQUARES_GUARD_H
#define GAME_SQUARES_GUARD_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Handle GAME_CMD_SQUARES_GUARD.
 *
 * Action 0 clears the pause and notifies clients. Any other action copies
 * the lifted and dropped masks and enters the pause, unless the guard is
 * disabled or a special mode is active (those paths abort the baseline and
 * do not stay paused). NULL or a different command type is ignored.
 * @param cmd Command from the game queue.
 */
void game_squares_guard_handle_command(const chess_move_command_t *cmd);

/**
 * @brief Drop both pause masks and the resync-required flag.
 *
 * Does not redraw LEDs and does not notify clients.
 */
void game_squares_guard_clear_both_layers(void);

/**
 * @brief Clear the pause, stop Hall-guide LEDs, and redraw movable hints.
 * @param notify_clients true: bump the game revision so web/BLE clients refresh.
 */
void game_squares_guard_restore_after_clear(bool notify_clients);

/**
 * @brief Paint the active guard overlay through led_task.
 *
 * Returns immediately when the pause is inactive or guard LEDs are disabled.
 */
void game_squares_guard_render_leds(void);

/**
 * @brief Leave the pause when auto-clear is enabled and the board matches again.
 *
 * No-op when auto-clear is off or the pause is not active. Hall builds ask
 * the restore guide to recompute first.
 */
void game_squares_guard_try_clear_from_matrix(void);

/**
 * @brief After an NVS restore, pause if sensors still disagree with logic.
 *
 * No-op until Hall readings are usable, or when guard / NVS resync is disabled.
 */
void game_squares_guard_check_resync_after_restore(void);

/**
 * @brief Enter the guard pause because the Hall restore guide saw a global mismatch.
 *
 * No-op when the guard is disabled or a pause is already active.
 */
void game_squares_guard_enter_pause_from_hall_guide(void);

#endif /* GAME_SQUARES_GUARD_H */
