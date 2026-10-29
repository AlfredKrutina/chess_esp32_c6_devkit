/**
 * @file game_snapshot.h
 * @brief NVS game snapshot save/load and boot restore logic.
 */

#ifndef GAME_SNAPSHOT_H
#define GAME_SNAPSHOT_H

#include "esp_err.h"
#include "position.h"
#include "chess_types.h"
#include "game_physical_undo.h"
#include <stdbool.h>

/** @brief Erase snapshot and boot-tracker NVS keys (new game). */
void game_snapshot_erase_nvs(void);

/** @brief Clear save/restore failure flags after starting a fresh game. */
void game_snapshot_reset_failure_flags(void);

/** Push the live board's empty map and piece types into the Hall driver. */
void game_snapshot_publish_hall_empty(void);

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

/** Board header only (no position undo hist). Same layout as position_t prefix. */
typedef struct {
  uint8_t board[8][8];
  uint8_t side;
  uint8_t castling;
  uint8_t ep_square;
  uint16_t halfmove;
  uint16_t fullmove;
} game_snap_pos_t;

/** Session snapshot for HTTP/UART reads (position, mode, clocks, web history). */
typedef struct {
  game_snap_pos_t pos;
  position_status_t pos_status;
  char fen[100];
  uint8_t mode; /* 0 none, 1 puzzle, 2 opening, 3 tutorial, 4 guard */
  uint8_t timer_fields_ready;
  uint32_t white_time_total;
  uint32_t black_time_total;
  uint32_t white_remaining_ms;
  uint32_t black_remaining_ms;
  uint32_t move_count;
  uint8_t game_state;
  uint32_t history_len;
  chess_move_t history_moves[POSITION_HIST_CAP];
} game_session_snap_t;

/** Thread-safe copy of the last committed session snapshot. */
void game_session_snap_copy(game_session_snap_t *out);

/** Hold s_session_snap_mutex; pair with game_session_snap_release(). */
const game_session_snap_t *game_session_snap_acquire(void);

/** Release after game_session_snap_acquire(). */
void game_session_snap_release(void);

/** Rebuild session snapshot from live game state (game task thread). */
void game_session_snap_refresh(void);

extern chess_move_t move_history[GAME_TASK_MAX_MOVES_HISTORY];
extern move_type_t move_history_kind[GAME_TASK_MAX_MOVES_HISTORY];
extern uint32_t history_index;

extern bool has_last_move;
extern uint8_t last_move_from_row;
extern uint8_t last_move_from_col;
extern uint8_t last_move_to_row;
extern uint8_t last_move_to_col;

extern bool game_saved;
extern char saved_game_name[32];

#endif /* GAME_SNAPSHOT_H */
