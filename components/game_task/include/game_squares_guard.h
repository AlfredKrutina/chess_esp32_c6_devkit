/**
 * @file game_squares_guard.h
 * @brief Matrix guard — pause play when physical board disagrees with logic.
 */

#ifndef GAME_SQUARES_GUARD_H
#define GAME_SQUARES_GUARD_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

void game_squares_guard_handle_command(const chess_move_command_t *cmd);
void game_squares_guard_clear_both_layers(void);
void game_squares_guard_restore_after_clear(bool notify_clients);
void game_squares_guard_render_leds(void);
void game_squares_guard_try_clear_from_matrix(void);
void game_squares_guard_check_resync_after_restore(void);

/** Pause play when Hall restore guide detects global misalignment (e.g. swaps). */
void game_squares_guard_enter_pause_from_hall_guide(void);

#endif /* GAME_SQUARES_GUARD_H */
