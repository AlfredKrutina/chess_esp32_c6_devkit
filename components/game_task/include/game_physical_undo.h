/**
 * @file game_physical_undo.h
 * @brief Physical takeback: lift the last mover, restore the piece, then rewind.
 *
 * Engine rewind of the last history entry is game_undo_rewind_last_move().
 * The physical phase machine does not replace that function.
 */
#ifndef GAME_PHYSICAL_UNDO_H
#define GAME_PHYSICAL_UNDO_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Store side-to-move, castling rights, and en-passant fields for one ply.
 *
 * Called when the move is appended to history. Out-of-range slot or NULL move
 * is ignored.
 * @param history_slot Index in move_history.
 * @param kind Move kind used to record rook columns or the captured square.
 * @param move Move just played.
 */
void game_move_undo_meta_init_record(uint32_t history_slot, move_type_t kind,
                                     const chess_move_t *move);

/**
 * @brief Record the piece chosen when a pending promotion completes.
 * @param promoted_piece Engine piece now standing on the promotion square.
 *        Ignored when history is empty.
 */
void game_move_undo_meta_on_promotion_completed(piece_t promoted_piece);

/**
 * @brief Rewind the engine by one history entry (UART undo and the end of a takeback).
 * @return false when history is empty; true after the position is restored.
 */
bool game_undo_rewind_last_move(void);

/**
 * @brief Whether the physical-takeback preference is on.
 * @return true when config_ui_prefs_get_chess_physical_takeback() is set.
 */
bool game_physical_undo_enabled(void);

/** @brief Clear the in-progress physical takeback. Does not rewind the engine. */
void game_physical_undo_reset(void);

/**
 * @brief Whether a physical takeback phase is in progress.
 * @return true when the phase is not idle.
 */
bool game_physical_undo_is_active(void);

/**
 * @brief Offer or continue a takeback when the lifted piece is the last mover.
 * @param cmd Pickup command. The square arguments are what the flow uses.
 * @param from_row Lifted square row.
 * @param from_col Lifted square column.
 * @param piece Piece the engine believes was lifted.
 * @return true if this pickup was consumed by the undo flow.
 */
bool game_physical_undo_handle_pickup(const chess_move_command_t *cmd,
                                      uint8_t from_row, uint8_t from_col,
                                      piece_t piece);

/**
 * @brief Place pieces during an active takeback drop.
 * @param cmd Drop command. The square arguments are what the flow uses.
 * @param to_row Drop row.
 * @param to_col Drop column.
 * @return true if the drop was fully handled and the caller should return.
 */
bool game_physical_undo_handle_drop(const chess_move_command_t *cmd,
                                    uint8_t to_row, uint8_t to_col);

/** @brief Refresh pending-takeback LEDs. Called from the game command cycle. */
void game_physical_undo_tick(void);

/**
 * @brief Continue a takeback when the last mover was set down off the return square.
 * @return true when this pickup belongs to the active takeback and the caller should stop.
 */
bool game_physical_undo_intercept_pickup(uint8_t row, uint8_t col);

/**
 * @brief Copy the position before the last history ply into dest.
 * @return false when history is empty or dest is NULL.
 */
bool game_physical_undo_copy_previous(piece_t dest[8][8]);

#endif /* GAME_PHYSICAL_UNDO_H */
