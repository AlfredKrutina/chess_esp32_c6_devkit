/**
 * @file game_task_internal.h
 * @brief Internal API between game_task.c and sibling source files in this component.
 */

#ifndef GAME_TASK_INTERNAL_H
#define GAME_TASK_INTERNAL_H

#include "chess_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "game_task.h"
#include <stdbool.h>
#include <stdint.h>

#define GAME_TASK_MAX_MOVES_HISTORY 200

extern piece_t board[8][8];
extern bool resync_required_after_restore;

extern game_state_t current_game_state;
extern player_t current_player;
extern uint32_t move_count;
extern bool game_active;

extern bool white_king_moved;
extern bool white_rook_a_moved;
extern bool white_rook_h_moved;
extern bool black_king_moved;
extern bool black_rook_a_moved;
extern bool black_rook_h_moved;

extern bool en_passant_available;
extern uint8_t en_passant_target_row;
extern uint8_t en_passant_target_col;
extern uint8_t en_passant_victim_row;
extern uint8_t en_passant_victim_col;

typedef struct {
  bool pending;
  uint8_t square_row;
  uint8_t square_col;
  player_t player;
} game_task_promotion_state_t;

extern game_task_promotion_state_t promotion_state;

extern chess_move_t move_history[GAME_TASK_MAX_MOVES_HISTORY];
extern uint32_t history_index;

extern uint32_t white_time_total;
extern uint32_t black_time_total;

/** True when matrix guard must not pause normal play (tutorial, castling, …). */
bool game_task_matrix_guard_mode_conflict_active(void);

/** Clear transient lift state when matrix guard activates. */
void game_task_matrix_guard_freeze_move_flow(void);

typedef struct {
  bool in_progress;
  uint8_t rook_from_row;
  uint8_t rook_from_col;
  uint8_t rook_to_row;
  uint8_t rook_to_col;
  player_t player;
  bool is_kingside;
} castling_state_t;

extern castling_state_t castling_state;
extern bool piece_moved[8][8];

typedef struct {
  bool active;
  player_t player;
  uint8_t king_row;
  uint8_t king_col;
  TimerHandle_t main_timer;
  TimerHandle_t animation_timer;
  uint64_t start_time_ms;
  uint8_t last_countdown_sec;
} king_resignation_state_t;

extern king_resignation_state_t resignation_state;

extern bool has_last_move;
extern uint8_t last_move_from_row;
extern uint8_t last_move_from_col;
extern uint8_t last_move_to_row;
extern uint8_t last_move_to_col;

void game_check_promotion_needed(void);

void game_bump_revision_and_notify(void);
