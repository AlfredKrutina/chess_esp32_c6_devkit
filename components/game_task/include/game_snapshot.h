/**
 * @file game_snapshot.h
 * @brief NVS game snapshot save/load and boot restore logic.
 */

#ifndef GAME_SNAPSHOT_H
#define GAME_SNAPSHOT_H

#include "esp_err.h"
#include <stdbool.h>

/** @brief Erase snapshot and boot-tracker NVS keys (new game). */
void game_snapshot_erase_nvs(void);

/** @brief Clear save/restore failure flags after starting a fresh game. */
void game_snapshot_reset_failure_flags(void);

/**
 * @brief Whether NVS holds a full or minimal snapshot with a valid CRC.
 * @return true when at least one snapshot blob checks out.
 */
bool game_snapshot_nvs_has_valid(void);

/**
 * Boot path after game_initialize_board(): boot tracker, load NVS or force new game.
 * May call game_start_new_game(), game_squares_guard_check_resync_after_restore().
 */
void game_snapshot_restore_on_boot(void);

/** Save snapshot to NVS and update boot tracker after a valid move. */
void game_snapshot_persist_after_valid_move(void);

/**
 * After boot animation: once Hall (or reed) readings are usable, compare them
 * to the restored board and, if they match, start the paused clock.
 */
void game_snapshot_service_after_boot(void);

/**
 * @brief Whether sensors are ready to compare with a restored board.
 * @return false while Hall has no trustworthy baseline. Reed builds return true.
 */
bool game_snapshot_sensors_ready(void);

/**
 * @brief Whether this boot applied an NVS snapshot to the live game.
 * @return true after a successful full or minimal restore.
 */
bool game_was_snapshot_loaded_on_boot(void);

/**
 * @brief Whether the last save or restore used the minimal snapshot.
 *
 * Set when the full blob cannot be written or is not the blob that was loaded.
 * @return true if the minimal snapshot was the one used.
 */
bool game_is_snapshot_fallback_used(void);

/** @return true if the last restore attempt failed. */
bool game_has_snapshot_restore_failure(void);

/** @return true if the last snapshot save failed. */
bool game_has_snapshot_save_failure(void);

/**
 * @brief Whether the boot tracker forced a new game (two boots, no move).
 * @return true when that rule fired and a valid snapshot was not present.
 */
bool game_was_boot_new_game_triggered(void);

#endif /* GAME_SNAPSHOT_H */
