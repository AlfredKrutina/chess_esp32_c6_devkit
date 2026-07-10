/**
 * @file game_task_internal.h
 * @brief Internal API between game_task.c and sibling source files in this component.
 */

#ifndef GAME_TASK_INTERNAL_H
#define GAME_TASK_INTERNAL_H

#include "chess_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
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
extern move_type_t move_history_kind[GAME_TASK_MAX_MOVES_HISTORY];
extern uint32_t history_index;

extern uint32_t white_time_total;
extern uint32_t black_time_total;

extern uint32_t last_move_time;
extern uint32_t white_moves_count;
extern uint32_t black_moves_count;
extern uint32_t white_castles;
extern uint32_t black_castles;

extern game_state_t game_result;
extern game_result_type_t current_result_type;

typedef enum {
  LAST_MOVE_NORMAL = 0,
  LAST_MOVE_EN_PASSANT = 1,
  LAST_MOVE_CASTLING = 2,
  LAST_MOVE_PROMOTION = 3,
  LAST_MOVE_DISCOVERED = 4
} last_move_type_t;

extern last_move_type_t last_move_type;

typedef struct {
  bool has_invalid_piece;
  uint8_t invalid_row;
  uint8_t invalid_col;
  uint8_t original_valid_row;
  uint8_t original_valid_col;
  piece_t piece_type;
  bool waiting_for_move_correction;
  uint8_t error_count;
} game_task_error_recovery_t;

extern game_task_error_recovery_t error_recovery_state;

extern SemaphoreHandle_t promotion_mutex;
extern bool s_uart_move_immediate_promotion;
extern promotion_choice_t s_uart_move_immediate_promotion_piece;

void game_add_captured_piece(piece_t piece);
void game_update_endgame_statistics(game_result_type_t result_type);
void game_print_endgame_report_uart(game_result_type_t result_type);
void game_record_material_advantage(void);
bool game_led_guidance_show_check_anim(void);

uint32_t game_generate_legal_moves(player_t player);

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

/* --- Physical / lift-drop flow (game_physical.c) --- */

extern bool piece_lifted;
extern uint8_t lifted_piece_row;
extern uint8_t lifted_piece_col;
extern piece_t lifted_piece;

extern bool capture_in_progress;
extern uint8_t capture_target_row;
extern uint8_t capture_target_col;
extern piece_t capture_removed_piece;

extern bool opponent_piece_moved;
extern piece_t opponent_piece_type;
extern uint8_t opponent_original_row;
extern uint8_t opponent_original_col;
extern uint8_t opponent_current_row;
extern uint8_t opponent_current_col;

extern bool castle_animation_active;
extern uint8_t rook_from_row;
extern uint8_t rook_from_col;
extern uint8_t rook_to_row;
extern uint8_t rook_to_col;

#define GUIDED_CAPTURE_MAX_ATTACKERS 16

typedef struct {
  bool active;
  uint8_t target_row;
  uint8_t target_col;
  piece_t target_piece;
  uint8_t attacker_count;
  uint8_t attacker_rows[GUIDED_CAPTURE_MAX_ATTACKERS];
  uint8_t attacker_cols[GUIDED_CAPTURE_MAX_ATTACKERS];
  piece_t attacker_pieces[GUIDED_CAPTURE_MAX_ATTACKERS];
} guided_capture_state_t;

extern guided_capture_state_t guided_capture_state;

typedef enum {
  PUZZLE_FEEDBACK_NONE = 0,
  PUZZLE_FEEDBACK_WRONG = 1,
  PUZZLE_FEEDBACK_SOLVED = 2,
  PUZZLE_FEEDBACK_ILLEGAL = 3
} puzzle_feedback_t;

extern bool puzzle_active;
extern uint8_t puzzle_active_id;
extern uint8_t puzzle_solution_from_row;
extern uint8_t puzzle_solution_from_col;
extern uint8_t puzzle_solution_to_row;
extern uint8_t puzzle_solution_to_col;
extern puzzle_feedback_t puzzle_feedback;

extern bool auto_new_game_blocked_until_move;

extern const char *piece_symbols[];

void game_stop_error_blink(void);
void game_update_promotion_anchor_led(void);

void game_send_response_to_uart(const char *message, bool is_error,
                                QueueHandle_t response_queue);
bool game_cmd_is_matrix_origin(const chess_move_command_t *cmd);
bool game_led_guidance_show_destinations(void);
bool game_led_guidance_show_movable_yellow(void);

void resignation_start(player_t player, uint8_t row, uint8_t col);
void resignation_stop(bool finalize);

void game_reset_guided_capture_state(void);
void game_show_guided_capture_leds(void);

void game_handle_invalid_move(move_error_t error, const chess_move_t *move);

#endif /* GAME_TASK_INTERNAL_H */
