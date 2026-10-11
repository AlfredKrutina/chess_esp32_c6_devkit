/**
 * @file game_hall_restore_guide.h
 * @brief Hall type/color board-restore matching and guide orchestration.
 */

#ifndef GAME_HALL_RESTORE_GUIDE_H
#define GAME_HALL_RESTORE_GUIDE_H

#include <stdbool.h>
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

bool game_hall_restore_guide_is_active(void);

/** Recompute plan from stable sensors; may enter/exit pause + LED guide. */
void game_hall_restore_guide_sync(void);

/** After matrix scan: compare stable occ/type/color fingerprint. */
void game_hall_restore_guide_note_stable_squares(const uint8_t occ[64]);

/** Matrix scan: stable occ/type/color changed — sync soon (game_task). */
void game_hall_restore_guide_request_sync(void);

/** Consume a pending matrix-triggered sync request. */
bool game_hall_restore_guide_take_sync_request(void);

/** Stop guide LEDs and internal state (guard layer may remain). */
void game_hall_restore_guide_stop_leds(void);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_RESTORE_GUIDE_H */
