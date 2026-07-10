/**
 * @file game_snapshot.h
 * @brief NVS game snapshot save/load and boot restore logic.
 */

#ifndef GAME_SNAPSHOT_H
#define GAME_SNAPSHOT_H

#include "esp_err.h"
#include <stdbool.h>

/** Erase snapshot + boot tracker keys (new game). */
void game_snapshot_erase_nvs(void);

/** Clear save/restore failure flags after starting a fresh game. */
void game_snapshot_reset_failure_flags(void);

/** True if NVS holds a full or minimal snapshot with valid CRC. */
bool game_snapshot_nvs_has_valid(void);

/**
 * Boot path after game_initialize_board(): boot tracker, load NVS or force new game.
 * May call game_start_new_game(), game_matrix_guard_check_resync_after_restore().
 */
void game_snapshot_restore_on_boot(void);

/** Save snapshot to NVS and update boot tracker after a valid move. */
void game_snapshot_persist_after_valid_move(void);

bool game_was_snapshot_loaded_on_boot(void);
bool game_is_snapshot_fallback_used(void);
bool game_has_snapshot_restore_failure(void);
bool game_has_snapshot_save_failure(void);
bool game_was_boot_new_game_triggered(void);

#endif /* GAME_SNAPSHOT_H */
