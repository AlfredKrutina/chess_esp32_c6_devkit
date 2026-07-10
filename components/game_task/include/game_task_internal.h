/**
 * @file game_task_internal.h
 * @brief Internal API between game_task.c and sibling source files in this component.
 */

#ifndef GAME_TASK_INTERNAL_H
#define GAME_TASK_INTERNAL_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

extern piece_t board[8][8];
extern bool resync_required_after_restore;

/** True when matrix guard must not pause normal play (tutorial, castling, …). */
bool game_task_matrix_guard_mode_conflict_active(void);

/** Clear transient lift state when matrix guard activates. */
void game_task_matrix_guard_freeze_move_flow(void);

void game_bump_revision_and_notify(void);

#endif /* GAME_TASK_INTERNAL_H */
