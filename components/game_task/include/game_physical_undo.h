/**
 * @file game_physical_undo.h
 * @brief Physical board takeback (lift last mover, restore position, then rewind engine).
 */

#ifndef GAME_PHYSICAL_UNDO_H
#define GAME_PHYSICAL_UNDO_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

void game_move_undo_meta_init_record(uint32_t history_slot, move_type_t kind,
                                     const chess_move_t *move);

void game_move_undo_meta_on_promotion_completed(piece_t promoted_piece);

bool game_undo_rewind_last_move(void);

bool game_physical_undo_enabled(void);

void game_physical_undo_reset(void);

bool game_physical_undo_is_active(void);

/**
 * @return true if the pickup was consumed (undo flow).
 */
bool game_physical_undo_handle_pickup(const chess_move_command_t *cmd,
                                      uint8_t from_row, uint8_t from_col,
                                      piece_t piece);

/**
 * @return true if drop was fully handled (caller should return).
 */
bool game_physical_undo_handle_drop(const chess_move_command_t *cmd,
                                    uint8_t to_row, uint8_t to_col);

void game_physical_undo_tick(void);

#endif /* GAME_PHYSICAL_UNDO_H */
