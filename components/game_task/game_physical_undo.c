/**
 * @file game_physical_undo.c
 * @brief Physical takeback: LEDs, delayed engine rewind, undo metadata.
 */

#include "game_physical_undo.h"
#include "game_task_internal.h"
#include "game_task.h"
#include "game_squares_guard.h"
#include "game_snapshot.h"
#include "chess_gameplay_policy.h"
#include "game_hall_match.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_restore_guide.h"
#include "game_hall_position.h"
#endif

#include "../config_manager/include/config_manager.h"
#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>

static const char *TAG = "PHYS_UNDO";

move_undo_meta_t move_history_meta[GAME_TASK_MAX_MOVES_HISTORY];

typedef enum {
  PHYS_UNDO_IDLE = 0,
  PHYS_UNDO_ARMED,
  PHYS_UNDO_PENDING,
} phys_undo_phase_t;

typedef struct {
  phys_undo_phase_t phase;
  chess_move_t move;
  move_type_t kind;
  move_undo_meta_t meta;
  uint8_t lift_row;
  uint8_t lift_col;
  piece_t lift_piece;
  uint8_t blue_row;
  uint8_t blue_col;
  bool undo_castle_rook_lift;
  bool stray_valid;
  uint8_t stray_row;
  uint8_t stray_col;
  piece_t target_board[8][8];
  uint32_t pending_blink_phase;
} physical_undo_state_t;

static physical_undo_state_t s_undo;

static void undo_pop_last_capture_list(piece_t captured, bool mover_is_white) {
  if (captured == PIECE_EMPTY) {
    return;
  }
  if (mover_is_white) {
    if (white_captured_index > 0) {
      white_captured_index--;
      if (white_captured_count > 0) {
        white_captured_count--;
      }
      if (white_captures > 0) {
        white_captures--;
      }
    }
  } else {
    if (black_captured_index > 0) {
      black_captured_index--;
      if (black_captured_count > 0) {
        black_captured_count--;
      }
      if (black_captures > 0) {
        black_captures--;
      }
    }
  }
}

void game_move_undo_meta_init_record(uint32_t history_slot, move_type_t kind,
                                     const chess_move_t *move) {
  if (history_slot >= GAME_TASK_MAX_MOVES_HISTORY || move == NULL) {
    return;
  }
  move_undo_meta_t *meta = &move_history_meta[history_slot];
  meta->side_before = current_player;
  meta->white_king_moved = white_king_moved;
  meta->white_rook_a_moved = white_rook_a_moved;
  meta->white_rook_h_moved = white_rook_h_moved;
  meta->black_king_moved = black_king_moved;
  meta->black_rook_a_moved = black_rook_a_moved;
  meta->black_rook_h_moved = black_rook_h_moved;
  meta->en_passant_available = en_passant_available;
  meta->en_passant_target_row = en_passant_target_row;
  meta->en_passant_target_col = en_passant_target_col;
  meta->en_passant_victim_row = en_passant_victim_row;
  meta->en_passant_victim_col = en_passant_victim_col;
  meta->promoted_to = PIECE_EMPTY;
  meta->rook_home_col = 0;
  meta->rook_castle_col = 0;
  meta->capture_return_piece = PIECE_EMPTY;
  meta->capture_return_row = move->to_row;
  meta->capture_return_col = move->to_col;

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    meta->rook_home_col = (move->to_col > move->from_col) ? 7u : 0u;
    meta->rook_castle_col = (move->to_col > move->from_col)
                                ? (uint8_t)(move->to_col - 1)
                                : (uint8_t)(move->to_col + 1);
  }

  if (kind == MOVE_TYPE_EN_PASSANT) {
    meta->capture_return_row = en_passant_victim_row;
    meta->capture_return_col = en_passant_victim_col;
    meta->capture_return_piece =
        (move->piece == PIECE_WHITE_PAWN) ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
  } else if (kind == MOVE_TYPE_CAPTURE) {
    meta->capture_return_piece = move->captured_piece;
  } else if (kind == MOVE_TYPE_PROMOTION) {
    meta->capture_return_row = move->to_row;
    meta->capture_return_col = move->to_col;
    meta->capture_return_piece = PIECE_EMPTY;
  }
}

void game_move_undo_meta_on_promotion_completed(piece_t promoted_piece) {
  if (history_index == 0) {
    return;
  }
  move_history_meta[history_index - 1].promoted_to = promoted_piece;
}

bool game_physical_undo_enabled(void) {
  return config_ui_prefs_get_chess_physical_takeback();
}

void game_physical_undo_reset(void) {
  memset(&s_undo, 0, sizeof(s_undo));
}

bool game_physical_undo_is_active(void) {
  return s_undo.phase != PHYS_UNDO_IDLE;
}

static bool game_physical_undo_mode_blocks_offer(void) {
  if (castling_state.in_progress || castle_animation_active ||
      guided_capture_state.active ||
      error_recovery_state.waiting_for_move_correction ||
      resignation_state.active ||
      current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP ||
      current_game_state == GAME_STATE_WAITING_FOR_RETURN ||
      puzzle_active || puzzle_setup_active || board_setup_tutorial_active ||
      game_is_opening_trainer_active()) {
    return true;
  }
  return false;
}

static bool game_physical_undo_load_last(chess_move_t *m, move_type_t *kind,
                                       move_undo_meta_t *meta) {
  if (history_index == 0) {
    return false;
  }
  uint32_t idx = history_index - 1;
  *m = move_history[idx];
  *kind = move_history_kind[idx];
  *meta = move_history_meta[idx];
  return true;
}

static piece_t game_physical_undo_rook_for_king(piece_t king) {
  return (king == PIECE_WHITE_KING) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
}

static bool game_physical_undo_lift_matches(const chess_move_t *m,
                                            move_type_t kind,
                                            const move_undo_meta_t *meta,
                                            uint8_t row, uint8_t col,
                                            piece_t piece) {
  piece_t on_board = board[row][col];
  if (on_board == PIECE_EMPTY && piece == PIECE_EMPTY) {
    return false;
  }

  if (row == m->to_row && col == m->to_col) {
    if (kind == MOVE_TYPE_PROMOTION && meta->promoted_to != PIECE_EMPTY) {
      return piece == meta->promoted_to || piece == m->piece;
    }
    return piece == on_board && piece != PIECE_EMPTY;
  }

  if ((kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) &&
      row == m->from_row && col == meta->rook_castle_col) {
    piece_t rook = game_physical_undo_rook_for_king(m->piece);
    return piece == rook && board[row][col] == rook;
  }
  return false;
}

static void game_physical_undo_build_target_board(piece_t target[8][8],
                                                  const chess_move_t *m,
                                                  move_type_t kind,
                                                  const move_undo_meta_t *meta) {
  memcpy(target, board, sizeof(piece_t) * 64);

  bool mover_white =
      (m->piece >= PIECE_WHITE_PAWN && m->piece <= PIECE_WHITE_KING);
  uint8_t fr = m->from_row;
  uint8_t fc = m->from_col;
  uint8_t tr = m->to_row;
  uint8_t tc = m->to_col;

  switch (kind) {
  case MOVE_TYPE_EN_PASSANT:
    target[tr][tc] = PIECE_EMPTY;
    target[fr][fc] = m->piece;
    target[meta->capture_return_row][meta->capture_return_col] =
        meta->capture_return_piece;
    break;
  case MOVE_TYPE_PROMOTION:
    target[tr][tc] = PIECE_EMPTY;
    target[fr][fc] = m->piece;
    break;
  default:
    target[tr][tc] = m->captured_piece;
    target[fr][fc] = m->piece;
    break;
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    piece_t rook = game_physical_undo_rook_for_king(m->piece);
    target[fr][meta->rook_castle_col] = PIECE_EMPTY;
    target[fr][meta->rook_home_col] = rook;
  }

  (void)mover_white;
}

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
static bool game_physical_undo_target_matches_hall(const piece_t target[8][8]) {
  return game_hall_positions_equal(target);
}
#else
static bool game_physical_undo_target_matches_reed(const piece_t target[8][8],
                                                   const move_undo_meta_t *meta,
                                                   move_type_t kind) {
  uint8_t phys[64] = {0};
  squares_get_state(phys);
  for (uint8_t sq = 0; sq < 64; sq++) {
    piece_t expect = target[sq / 8][sq % 8];
    uint8_t want_occ = (expect != PIECE_EMPTY) ? 1 : 0;
    if (phys[sq] != want_occ) {
      return false;
    }
  }
  if (kind == MOVE_TYPE_CAPTURE || kind == MOVE_TYPE_EN_PASSANT) {
    uint8_t sq = (uint8_t)(meta->capture_return_row * 8 + meta->capture_return_col);
    if (phys[sq] == 0) {
      return false;
    }
  }
  (void)meta;
  return true;
}
#endif

bool game_physical_undo_copy_previous(piece_t dest[8][8]) {
  chess_move_t m;
  move_type_t kind;
  move_undo_meta_t meta;
  if (dest == NULL || !game_physical_undo_load_last(&m, &kind, &meta)) {
    return false;
  }
  game_physical_undo_build_target_board(dest, &m, kind, &meta);
  return true;
}

static bool game_physical_undo_physical_matches_target(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return game_physical_undo_target_matches_hall(s_undo.target_board);
#else
  return game_physical_undo_target_matches_reed(s_undo.target_board, &s_undo.meta,
                                                s_undo.kind);
#endif
}

static void game_physical_undo_show_armed_leds(void) {
  led_clear_board_only();
  led_set_pixel_safe(chess_pos_to_led_index(s_undo.lift_row, s_undo.lift_col),
                     255, 255, 0);
  if (chess_policy_error_recovery_led_valid_blue()) {
    led_set_pixel_safe(
        chess_pos_to_led_index(s_undo.blue_row, s_undo.blue_col), 0, 0, 255);
  }
}

static void game_physical_undo_show_pending_leds(void) {
  led_clear_board_only();
  const chess_move_t *m = &s_undo.move;
  const move_undo_meta_t *meta = &s_undo.meta;

  if (chess_policy_error_recovery_led_valid_blue()) {
    led_set_pixel_safe(chess_pos_to_led_index(m->from_row, m->from_col), 0, 0,
                       255);
  }

  if (s_undo.kind == MOVE_TYPE_CASTLE_KING ||
      s_undo.kind == MOVE_TYPE_CASTLE_QUEEN) {
    piece_t rook = game_physical_undo_rook_for_king(m->piece);
    if (board[m->from_row][meta->rook_castle_col] == rook) {
      if (chess_policy_error_recovery_led_valid_blue()) {
        led_set_pixel_safe(
            chess_pos_to_led_index(m->from_row, meta->rook_home_col), 0, 0,
            255);
      }
    }
  }

  if (meta->capture_return_piece != PIECE_EMPTY) {
    uint8_t rr = meta->capture_return_row;
    uint8_t rc = meta->capture_return_col;
    piece_t expect = s_undo.target_board[rr][rc];
    uint8_t phys[64] = {0};
    squares_get_state(phys);
    uint8_t sq = (uint8_t)(rr * 8 + rc);
    bool ok = false;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    ok = game_hall_square_agrees_with_logic(sq, expect, phys[sq]);
#else
    bool need_piece = (expect != PIECE_EMPTY);
    ok = (!need_piece && phys[sq] == 0) || (need_piece && phys[sq] != 0);
#endif
    if (!ok && chess_policy_error_recovery_led_red_blink()) {
      s_undo.pending_blink_phase++;
      bool on = (s_undo.pending_blink_phase / 8U) % 2U == 0U;
      if (on) {
        led_set_pixel_safe(chess_pos_to_led_index(rr, rc), 255, 0, 0);
      }
    }
  }
}

static bool game_physical_undo_try_complete_engine(void) {
  if (!game_physical_undo_physical_matches_target()) {
    game_physical_undo_show_pending_leds();
    return false;
  }
  if (!game_undo_rewind_last_move()) {
    ESP_LOGW(TAG, "physical complete but engine undo failed");
    game_physical_undo_reset();
    return false;
  }
  ESP_LOGI(TAG, "Physical takeback complete — engine rewound");
  game_physical_undo_reset();
  return true;
}

bool game_undo_rewind_last_move(void) {
  if (history_index == 0) {
    ESP_LOGW(TAG, "Undo: no moves in history");
    return false;
  }

  uint32_t idx = history_index - 1;
  chess_move_t m = move_history[idx];
  move_type_t kind = move_history_kind[idx];
  move_undo_meta_t meta = move_history_meta[idx];

  bool mover_white =
      (m.piece >= PIECE_WHITE_PAWN && m.piece <= PIECE_WHITE_KING);
  uint8_t fr = m.from_row;
  uint8_t fc = m.from_col;
  uint8_t tr = m.to_row;
  uint8_t tc = m.to_col;

  switch (kind) {
  case MOVE_TYPE_EN_PASSANT:
    board[tr][tc] = PIECE_EMPTY;
    board[fr][fc] = m.piece;
    board[meta.capture_return_row][meta.capture_return_col] =
        meta.capture_return_piece;
    break;
  case MOVE_TYPE_PROMOTION: {
    if (promotion_state.pending) {
      if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
          pdTRUE) {
        promotion_state.pending = false;
        promotion_state.square_row = 0;
        promotion_state.square_col = 0;
        xSemaphoreGiveRecursive(promotion_mutex);
      }
    }
    board[tr][tc] = PIECE_EMPTY;
    board[fr][fc] = m.piece;
    break;
  }
  default:
    board[tr][tc] = m.captured_piece;
    board[fr][fc] = m.piece;
    break;
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    piece_t rook_piece = game_physical_undo_rook_for_king(m.piece);
    if (board[fr][meta.rook_castle_col] == rook_piece) {
      board[fr][meta.rook_castle_col] = PIECE_EMPTY;
      board[fr][meta.rook_home_col] = rook_piece;
    }
  }

  if (kind == MOVE_TYPE_CAPTURE) {
    undo_pop_last_capture_list(m.captured_piece, mover_white);
  }

  castling_state.in_progress = false;

  white_king_moved = meta.white_king_moved;
  white_rook_a_moved = meta.white_rook_a_moved;
  white_rook_h_moved = meta.white_rook_h_moved;
  black_king_moved = meta.black_king_moved;
  black_rook_a_moved = meta.black_rook_a_moved;
  black_rook_h_moved = meta.black_rook_h_moved;
  en_passant_available = meta.en_passant_available;
  en_passant_target_row = meta.en_passant_target_row;
  en_passant_target_col = meta.en_passant_target_col;
  en_passant_victim_row = meta.en_passant_victim_row;
  en_passant_victim_col = meta.en_passant_victim_col;

  history_index--;
  if (move_count > 0) {
    move_count--;
  }
  if (advantage_history_count > 0) {
    advantage_history_count--;
  }

  if (mover_white) {
    if (white_moves_count > 0) {
      white_moves_count--;
    }
  } else {
    if (black_moves_count > 0) {
      black_moves_count--;
    }
  }

  current_player = meta.side_before;

  if (history_index > 0) {
    chess_move_t *p = &move_history[history_index - 1];
    last_move_from_row = p->from_row;
    last_move_from_col = p->from_col;
    last_move_to_row = p->to_row;
    last_move_to_col = p->to_col;
    has_last_move = true;
  } else {
    has_last_move = false;
  }

  if (current_game_state == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_ACTIVE;
    game_active = true;
  }

  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  game_end_timer_move();
  game_start_timer_move(current_player == PLAYER_WHITE);

  game_squares_guard_check_resync_after_restore();
  if (!game_is_squares_guard_active()) {
    led_clear_board_only();
    chess_policy_highlight_movable_if_enabled();
  }

  game_snapshot_persist_after_valid_move();
  game_bump_revision_and_notify();

  ESP_LOGI(TAG, "Undo applied (kind=%d), side to move: %s", (int)kind,
           current_player == PLAYER_WHITE ? "White" : "Black");
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_snapshot_last_valid();
#endif
  return true;
}

void game_physical_undo_tick(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* Hall takeback is a stable previous position, not a per-tick LED redraw. */
  return;
#else
  if (s_undo.phase == PHYS_UNDO_PENDING) {
    game_physical_undo_show_pending_leds();
  }
#endif
}

bool game_physical_undo_intercept_pickup(uint8_t row, uint8_t col) {
  if (s_undo.phase != PHYS_UNDO_ARMED || !s_undo.stray_valid) {
    return false;
  }
  if (row != s_undo.stray_row || col != s_undo.stray_col) {
    return false;
  }
  s_undo.stray_valid = false;
  piece_lifted = true;
  lifted_piece_row = row;
  lifted_piece_col = col;
  lifted_piece = s_undo.lift_piece;
  game_physical_undo_show_armed_leds();
  ESP_LOGI(TAG, "Takeback resumed from stray %c%d", 'a' + col, row + 1);
  return true;
}

static void game_physical_undo_arm_from_last(const chess_move_t *m,
                                             move_type_t kind,
                                             const move_undo_meta_t *meta,
                                             uint8_t from_row, uint8_t from_col,
                                             piece_t piece) {
  s_undo.phase = PHYS_UNDO_ARMED;
  s_undo.stray_valid = false;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_stop_leds();
#endif
  s_undo.move = *m;
  s_undo.kind = kind;
  s_undo.meta = *meta;
  s_undo.lift_row = from_row;
  s_undo.lift_col = from_col;
  s_undo.lift_piece = piece;
  s_undo.undo_castle_rook_lift =
      (from_row == m->from_row && from_col == meta->rook_castle_col);

  if (s_undo.undo_castle_rook_lift) {
    s_undo.blue_row = m->from_row;
    s_undo.blue_col = meta->rook_home_col;
  } else {
    s_undo.blue_row = m->from_row;
    s_undo.blue_col = m->from_col;
  }

  piece_lifted = true;
  lifted_piece_row = from_row;
  lifted_piece_col = from_col;
  lifted_piece = piece;
}

static bool game_physical_undo_drop_is_last_reverse(uint8_t to_row,
                                                    uint8_t to_col,
                                                    chess_move_t *m,
                                                    move_type_t *kind,
                                                    move_undo_meta_t *meta) {
  if (!piece_lifted || !game_physical_undo_load_last(m, kind, meta)) {
    return false;
  }
  if (!game_physical_undo_lift_matches(m, *kind, meta, lifted_piece_row,
                                       lifted_piece_col, lifted_piece)) {
    return false;
  }
  if ((*kind == MOVE_TYPE_CASTLE_KING || *kind == MOVE_TYPE_CASTLE_QUEEN) &&
      lifted_piece_row == m->from_row &&
      lifted_piece_col == meta->rook_castle_col) {
    return to_row == m->from_row && to_col == meta->rook_home_col;
  }
  return to_row == m->from_row && to_col == m->from_col;
}

bool game_physical_undo_handle_pickup(const chess_move_command_t *cmd,
                                      uint8_t from_row, uint8_t from_col,
                                      piece_t piece) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  (void)cmd;
  (void)from_row;
  (void)from_col;
  (void)piece;
  return false;
#else
  (void)cmd;

  if (s_undo.phase != PHYS_UNDO_IDLE && !game_physical_undo_enabled()) {
    game_physical_undo_reset();
    return false;
  }

  if (game_physical_undo_intercept_pickup(from_row, from_col)) {
    return true;
  }

  if (s_undo.phase == PHYS_UNDO_PENDING) {
    game_physical_undo_try_complete_engine();
    if (s_undo.phase == PHYS_UNDO_PENDING) {
      game_physical_undo_show_pending_leds();
    }
    return true;
  }

  if (!game_physical_undo_enabled() || game_physical_undo_mode_blocks_offer()) {
    return false;
  }

  chess_move_t m;
  move_type_t kind;
  move_undo_meta_t meta;
  if (!game_physical_undo_load_last(&m, &kind, &meta)) {
    return false;
  }

  if (!game_physical_undo_lift_matches(&m, kind, &meta, from_row, from_col,
                                       piece)) {
    return false;
  }

  game_physical_undo_arm_from_last(&m, kind, &meta, from_row, from_col, piece);
  game_physical_undo_show_armed_leds();
  ESP_LOGI(TAG, "Takeback armed: lift %c%d blue return %c%d",
           'a' + from_col, from_row + 1, 'a' + s_undo.blue_col,
           s_undo.blue_row + 1);
  return true;
#endif
}

bool game_physical_undo_handle_drop(const chess_move_command_t *cmd,
                                    uint8_t to_row, uint8_t to_col) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  (void)cmd;
  (void)to_row;
  (void)to_col;
  return false;
#else
  (void)cmd;

  if (s_undo.phase != PHYS_UNDO_IDLE && !game_physical_undo_enabled()) {
    game_physical_undo_reset();
    return false;
  }

  if (s_undo.phase == PHYS_UNDO_PENDING) {
    game_physical_undo_try_complete_engine();
    return true;
  }

  if (s_undo.phase == PHYS_UNDO_IDLE) {
    if (!game_physical_undo_enabled() ||
        game_physical_undo_mode_blocks_offer() || !piece_lifted) {
      return false;
    }
    chess_move_t rev_m;
    move_type_t rev_kind;
    move_undo_meta_t rev_meta;
    if (!game_physical_undo_drop_is_last_reverse(to_row, to_col, &rev_m,
                                                 &rev_kind, &rev_meta)) {
      return false;
    }
    game_physical_undo_arm_from_last(&rev_m, rev_kind, &rev_meta,
                                     lifted_piece_row, lifted_piece_col,
                                     lifted_piece);
  }

  if (s_undo.phase != PHYS_UNDO_ARMED || !piece_lifted) {
    return false;
  }

  if (to_row == s_undo.lift_row && to_col == s_undo.lift_col) {
    ESP_LOGI(TAG, "Takeback cancelled — piece returned to lift square");
    game_physical_undo_reset();
    piece_lifted = false;
    lifted_piece = PIECE_EMPTY;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    led_clear_board_only();
    chess_policy_highlight_movable_if_enabled();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    game_hall_restore_guide_request_sync();
#endif
    return true;
  }

  if (to_row == s_undo.blue_row && to_col == s_undo.blue_col) {
    s_undo.phase = PHYS_UNDO_PENDING;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    game_hall_restore_guide_stop_leds();
#endif
    game_physical_undo_build_target_board(s_undo.target_board, &s_undo.move,
                                          s_undo.kind, &s_undo.meta);
    piece_lifted = false;
    lifted_piece = PIECE_EMPTY;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    s_undo.pending_blink_phase = 0;

    if (s_undo.kind == MOVE_TYPE_PROMOTION && promotion_state.pending) {
      if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
          pdTRUE) {
        promotion_state.pending = false;
        promotion_state.square_row = 0;
        promotion_state.square_col = 0;
        xSemaphoreGiveRecursive(promotion_mutex);
      }
    }

    ESP_LOGI(TAG, "Takeback pending — restore all pieces before engine rewind");
    game_physical_undo_show_pending_leds();
    game_physical_undo_try_complete_engine();
    return true;
  }

  ESP_LOGI(TAG, "Takeback held — %c%d is not the previous square",
           'a' + to_col, to_row + 1);
  s_undo.stray_valid = true;
  s_undo.stray_row = to_row;
  s_undo.stray_col = to_col;
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  game_physical_undo_show_armed_leds();
  led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 0, 0);
  return true;
#endif
}
