/**
 * @file game_matrix_guard.h
 * @brief Matrix guard — pause play when physical board disagrees with logic.
 */

#ifndef GAME_MATRIX_GUARD_H
#define GAME_MATRIX_GUARD_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

void game_matrix_guard_handle_command(const chess_move_command_t *cmd);
void game_matrix_guard_clear_both_layers(void);
void game_matrix_guard_restore_after_clear(bool notify_clients);
void game_matrix_guard_render_leds(void);
void game_matrix_guard_try_clear_from_matrix(void);
void game_matrix_guard_check_resync_after_restore(void);

#endif /* GAME_MATRIX_GUARD_H */
