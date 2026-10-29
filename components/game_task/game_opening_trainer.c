/**
 * @file game_opening_trainer.c
 * @brief Interactive opening line trainer — multi-ply UCI validation + virtual opponent.
 */

#include "game_board_core.h"
#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_mode.h"
#include "game_board_transition.h"
#include "game_board_machine.h"

#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_OPENING";

static void opening_mode_bind(void);
static void opening_mode_unbind(void);

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

#define OPENING_MAX_PLIES 16
#define OPENING_MAX_PLAYER_PLIES 8
#define OPENING_MAX_CHECKPOINTS 4
#define OPENING_LINE_ID_MAX 48
#define OPENING_FEN_MAX 120

typedef enum {
  OPENING_FEEDBACK_NONE = 0,
  OPENING_FEEDBACK_CORRECT,
  OPENING_FEEDBACK_WRONG,
  OPENING_FEEDBACK_ILLEGAL,
  OPENING_FEEDBACK_MISTAKE_HINT,
  OPENING_FEEDBACK_COMPLETE,
  OPENING_FEEDBACK_CHECKPOINT,
  OPENING_FEEDBACK_OPPONENT_TURN
} opening_feedback_t;

typedef enum {
  OPENING_MODE_LEARN = 0,
  OPENING_MODE_DRILL,
  OPENING_MODE_TIMED,
  OPENING_MODE_MIRROR
} opening_mode_t;

typedef enum {
  OPENING_OPPONENT_VIRTUAL = 0,
  OPENING_OPPONENT_PHYSICAL
} opening_opponent_mode_t;

typedef struct {
  bool active;
  bool setup_phase;
  opening_mode_t mode;
  opening_opponent_mode_t opponent_mode;
  char line_id[OPENING_LINE_ID_MAX];
  char start_fen[OPENING_FEN_MAX];
  char line_uci[OPENING_MAX_PLIES][6];
  uint8_t line_uci_count;
  uint8_t player_ply_indices[OPENING_MAX_PLAYER_PLIES];
  uint8_t player_ply_count;
  uint8_t checkpoint_ply_indices[OPENING_MAX_CHECKPOINTS];
  uint8_t checkpoint_count;
  uint8_t ply_index;
  uint8_t player_ply_index;
  char expected_from[3];
  char expected_to[3];
  char last_opponent_uci[6];
  char last_wrong_uci[6];
  opening_feedback_t feedback;
  player_t player_side;
  bool awaiting_checkpoint_ack;
  bool awaiting_opponent_physical;
  uint8_t wrong_move_count;
} opening_trainer_state_t;

static opening_trainer_state_t opening_state;

static bool opening_is_player_ply(uint8_t ply) {
  for (uint8_t i = 0; i < opening_state.player_ply_count; i++) {
    if (opening_state.player_ply_indices[i] == ply) {
      return true;
    }
  }
  return false;
}

static bool opening_is_checkpoint_ply(uint8_t ply) {
  for (uint8_t i = 0; i < opening_state.checkpoint_count; i++) {
    if (opening_state.checkpoint_ply_indices[i] == ply) {
      return true;
    }
  }
  return false;
}

static bool opening_parse_uci(const char *uci, char *from, char *to) {
  if (uci == NULL || from == NULL || to == NULL) {
    return false;
  }
  size_t len = strlen(uci);
  if (len < 4) {
    return false;
  }
  from[0] = uci[0];
  from[1] = uci[1];
  from[2] = '\0';
  to[0] = uci[2];
  to[1] = uci[3];
  to[2] = '\0';
  return true;
}

static void opening_play_move_led(const char *from_sq, const char *to_sq) {
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!game_notation_to_coords(from_sq, &from_row, &from_col) ||
      !game_notation_to_coords(to_sq, &to_row, &to_col)) {
    return;
  }
  uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
  uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
  game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_MOVE_PATH, from_led, to_led);
}

static void opening_show_opponent_physical_led(bool piece_lifted) {
  if (!opening_state.active || !opening_state.awaiting_opponent_physical) {
    return;
  }
  if (opening_state.expected_from[0] == '\0' || opening_state.expected_to[0] == '\0') {
    return;
  }
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!game_notation_to_coords(opening_state.expected_to, &to_row, &to_col)) {
    return;
  }
  uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
  uint8_t from_led = 64;
  if (!piece_lifted &&
      game_notation_to_coords(opening_state.expected_from, &from_row, &from_col)) {
    from_led = chess_pos_to_led_index(from_row, from_col);
  }
  game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_HINT_PAIR, from_led, to_led);
}

static void opening_show_hint_led(void) {
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack ||
      opening_state.awaiting_opponent_physical) {
    return;
  }
  if (opening_state.expected_from[0] == '\0' || opening_state.expected_to[0] == '\0') {
    return;
  }
  uint8_t guidance = game_get_led_guidance_level();
  if (guidance == 0) {
    return;
  }
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!game_notation_to_coords(opening_state.expected_to, &to_row, &to_col)) {
    return;
  }
  uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
  bool to_only = (opening_state.mode == OPENING_MODE_DRILL ||
                  opening_state.mode == OPENING_MODE_TIMED || guidance == 1);
  uint8_t from_led = 64;
  if (!to_only && guidance >= 2 &&
      game_notation_to_coords(opening_state.expected_from, &from_row, &from_col)) {
    from_led = chess_pos_to_led_index(from_row, from_col);
  }
  game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_HINT_PAIR, from_led, to_led);
}

static void opening_update_expected_move(void) {
  opening_state.expected_from[0] = '\0';
  opening_state.expected_to[0] = '\0';
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    return;
  }
  opening_parse_uci(opening_state.line_uci[opening_state.ply_index],
                      opening_state.expected_from, opening_state.expected_to);
}

static uint8_t opening_count_player_ply_index(uint8_t ply) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < opening_state.player_ply_count; i++) {
    if (opening_state.player_ply_indices[i] < ply) {
      count++;
    } else if (opening_state.player_ply_indices[i] == ply) {
      return count;
    }
  }
  return count;
}

bool game_opening_apply_uci(const char *uci) {
  if (uci == NULL) {
    return false;
  }
  char from[3] = {0};
  char to[3] = {0};
  if (!opening_parse_uci(uci, from, to)) {
    return false;
  }
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!game_notation_to_coords(from, &from_row, &from_col) ||
      !game_notation_to_coords(to, &to_row, &to_col)) {
    return false;
  }
  piece_t piece = game_get_piece(from_row, from_col);
  if (piece == PIECE_EMPTY) {
    return false;
  }
  chess_move_t move = {
      .from_row = from_row,
      .from_col = from_col,
      .to_row = to_row,
      .to_col = to_col,
      .piece = piece,
      .captured_piece = game_get_piece(to_row, to_col),
      .timestamp = esp_timer_get_time() / 1000,
  };
  if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
    return false;
  }
  return game_execute_move(&move);
}

static bool opening_should_checkpoint_after_ply(uint8_t ply) {
  return opening_state.opponent_mode == OPENING_OPPONENT_VIRTUAL &&
         opening_is_checkpoint_ply(ply);
}

static void opening_prepare_player_turn(void) {
  opening_state.awaiting_opponent_physical = false;
  opening_state.player_ply_index =
      opening_count_player_ply_index(opening_state.ply_index);
  opening_update_expected_move();
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  opening_show_hint_led();
}

static bool opening_begin_opponent_physical_turn(void) {
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    return false;
  }
  const char *uci = opening_state.line_uci[opening_state.ply_index];
  opening_parse_uci(uci, opening_state.expected_from, opening_state.expected_to);
  strncpy(opening_state.last_opponent_uci, uci,
          sizeof(opening_state.last_opponent_uci) - 1);
  opening_state.awaiting_opponent_physical = true;
  opening_state.feedback = OPENING_FEEDBACK_OPPONENT_TURN;
  opening_show_opponent_physical_led(false);
  STAGING_LOGI(TAG, "opponent physical ply=%u uci=%s", (unsigned)opening_state.ply_index,
               uci);
  return true;
}

static void opening_run_until_player_turn(void) {
  while (opening_state.active &&
         opening_state.ply_index < opening_state.line_uci_count) {
    if (opening_is_player_ply(opening_state.ply_index)) {
      opening_prepare_player_turn();
      return;
    }

    if (opening_state.opponent_mode == OPENING_OPPONENT_PHYSICAL) {
      opening_begin_opponent_physical_turn();
      return;
    }

    const char *uci = opening_state.line_uci[opening_state.ply_index];
    char from[3], to[3];
    opening_parse_uci(uci, from, to);
    if (game_opening_apply_uci(uci)) {
      strncpy(opening_state.last_opponent_uci, uci,
              sizeof(opening_state.last_opponent_uci) - 1);
      opening_play_move_led(from, to);
      STAGING_LOGI(TAG, "virtual reply ply=%u uci=%s", (unsigned)opening_state.ply_index,
                   uci);
    } else {
      ESP_LOGE(TAG, "virtual uci failed: %s", uci);
      break;
    }
    opening_state.ply_index++;
    if (opening_should_checkpoint_after_ply(opening_state.ply_index - 1)) {
      opening_state.awaiting_checkpoint_ack = true;
      opening_state.feedback = OPENING_FEEDBACK_CHECKPOINT;
      return;
    }
  }

  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    game_led_flush();
  game_event_post_game_end(0, 0);
    STAGING_LOGI(TAG, "line complete id=%s", opening_state.line_id);
  }
}

void game_opening_cancel(void) {
  opening_mode_unbind();
  bool was = opening_state.active || opening_state.setup_phase;
  memset(&opening_state, 0, sizeof(opening_state));
  if (was) {
    STAGING_LOGI(TAG, "opening cancelled");
  }
}

bool game_is_opening_trainer_active(void) { return opening_state.active; }

bool game_is_opening_trainer_setup_active(void) { return opening_state.setup_phase; }

bool game_opening_awaiting_opponent_physical(void) {
  return opening_state.active && opening_state.awaiting_opponent_physical;
}

bool game_opening_is_physical_opponent_mode(void) {
  return opening_state.opponent_mode == OPENING_OPPONENT_PHYSICAL;
}

void game_opening_on_opponent_piece_lifted(void) {
  if (!opening_state.awaiting_opponent_physical) {
    return;
  }
  opening_show_opponent_physical_led(true);
}

bool game_opening_physical_matches_start(void) {
  if (opening_state.start_fen[0] == '\0') {
    return false;
  }
  return game_physical_squares_matches_fen(opening_state.start_fen);
}

bool game_opening_enter_setup_phase(void) {
  if (opening_state.line_uci_count == 0) {
    return false;
  }
  opening_state.setup_phase = true;
  opening_state.active = false;
  STAGING_LOGI(TAG, "setup_phase line=%s (physical mismatch)",
               opening_state.line_id);
  return true;
}

bool game_opening_validate_checkpoint_physical(void) {
  uint8_t matrix[64] = {0};
  squares_get_state(matrix);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t row = i / 8;
    uint8_t col = i % 8;
    uint8_t expected = (game_get_piece(row, col) == PIECE_EMPTY) ? 0 : 1;
    if (matrix[i] != expected) {
      return false;
    }
  }
  return true;
}

bool game_opening_load_config(const char *line_id, const char *start_fen,
                              const char line_uci[][6], uint8_t line_uci_count,
                              const uint8_t *player_ply_indices,
                              uint8_t player_ply_count,
                              const uint8_t *checkpoint_ply_indices,
                              uint8_t checkpoint_count, uint8_t mode,
                              uint8_t player_side_white,
                              uint8_t opponent_mode) {
  if (line_id == NULL || start_fen == NULL || line_uci == NULL ||
      line_uci_count == 0 || line_uci_count > OPENING_MAX_PLIES ||
      player_ply_count == 0 || player_ply_count > OPENING_MAX_PLAYER_PLIES) {
    return false;
  }
  game_opening_cancel();
  memset(&opening_state, 0, sizeof(opening_state));
  strncpy(opening_state.line_id, line_id, sizeof(opening_state.line_id) - 1);
  strncpy(opening_state.start_fen, start_fen, sizeof(opening_state.start_fen) - 1);
  opening_state.line_uci_count = line_uci_count;
  for (uint8_t i = 0; i < line_uci_count; i++) {
    strncpy(opening_state.line_uci[i], line_uci[i], sizeof(opening_state.line_uci[i]) - 1);
  }
  opening_state.player_ply_count = player_ply_count;
  for (uint8_t i = 0; i < player_ply_count; i++) {
    opening_state.player_ply_indices[i] = player_ply_indices[i];
  }
  opening_state.checkpoint_count =
      checkpoint_count > OPENING_MAX_CHECKPOINTS ? OPENING_MAX_CHECKPOINTS : checkpoint_count;
  for (uint8_t i = 0; i < opening_state.checkpoint_count; i++) {
    opening_state.checkpoint_ply_indices[i] = checkpoint_ply_indices[i];
  }
  opening_state.mode = (opening_mode_t)mode;
  opening_state.opponent_mode =
      opponent_mode == 1 ? OPENING_OPPONENT_PHYSICAL : OPENING_OPPONENT_VIRTUAL;
  opening_state.player_side = player_side_white ? PLAYER_WHITE : PLAYER_BLACK;
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  opening_state.wrong_move_count = 0;
  opening_state.last_wrong_uci[0] = '\0';
  opening_state.awaiting_opponent_physical = false;
  opening_state.setup_phase = false;
  STAGING_LOGI(TAG, "load line=%s plies=%u opponent=%s", line_id,
               (unsigned)line_uci_count,
               opening_state.opponent_mode == OPENING_OPPONENT_PHYSICAL
                   ? "physical"
                   : "virtual");
  return true;
}

bool game_opening_ensure_and_start(void) {
  if (opening_state.line_uci_count == 0) {
    static const char uci[][6] = {"e2e4", "e7e5", "g1f3", "b8c6", "f1c4"};
    static const uint8_t player_plies[] = {0, 2, 4};
    static const uint8_t checkpoint[] = {4};
    const char *fen =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    if (!game_opening_load_config("italian", fen, uci, 5, player_plies, 3,
                                  checkpoint, 1, 0, 1, 0)) {
      return false;
    }
  }
  if (game_opening_start()) {
    return true;
  }
  return game_opening_enter_setup_phase();
}

bool game_opening_start(void) {
  if (opening_state.line_uci_count == 0) {
    return false;
  }
  if (!game_opening_physical_matches_start()) {
    opening_state.setup_phase = true;
    opening_state.active = false;
    STAGING_LOGI(TAG, "start blocked — physical mismatch line=%s",
                 opening_state.line_id);
    return false;
  }
  if (game_is_board_setup_tutorial_active()) {
    game_exit_board_setup_tutorial(false);
  }
  game_puzzle_cancel();
  game_reset_game();

  player_t fen_player = PLAYER_WHITE;
  if (!game_load_position_from_fen(opening_state.start_fen, &fen_player)) {
    ESP_LOGE(TAG, "failed to load start FEN");
    return false;
  }

  current_player = fen_player;
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  opening_state.setup_phase = false;
  opening_state.active = true;
  opening_state.ply_index = 0;
  opening_state.player_ply_index = 0;
  opening_state.awaiting_checkpoint_ack = false;
  opening_state.awaiting_opponent_physical = false;
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  opening_state.last_opponent_uci[0] = '\0';
  opening_state.last_wrong_uci[0] = '\0';
  opening_state.last_wrong_uci[0] = '\0';
  opening_state.wrong_move_count = 0;
  game_led_clear_board();

  opening_run_until_player_turn();
  STAGING_LOGI(TAG, "started line=%s ply=%u", opening_state.line_id,
               (unsigned)opening_state.ply_index);
  opening_mode_bind();
  return true;
}

bool game_opening_hint(void) {
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack) {
    return false;
  }
  if (opening_state.awaiting_opponent_physical) {
    opening_show_opponent_physical_led(false);
    return true;
  }
  opening_show_hint_led();
  return true;
}

bool game_opening_checkpoint_ack(void) {
  if (!opening_state.active || !opening_state.awaiting_checkpoint_ack) {
    return false;
  }
  if (!game_opening_validate_checkpoint_physical()) {
    STAGING_LOGI(TAG, "checkpoint_ack rejected — physical mismatch");
    return false;
  }
  opening_state.awaiting_checkpoint_ack = false;
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  opening_update_expected_move();
  opening_show_hint_led();
  STAGING_LOGI(TAG, "checkpoint_ack OK ply=%u", (unsigned)opening_state.ply_index);
  return true;
}

bool game_opening_validate_expected_move(uint8_t from_row, uint8_t from_col,
                                         uint8_t to_row, uint8_t to_col) {
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack) {
    return false;
  }
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    return false;
  }
  char from_notation[4] = {0};
  char to_notation[4] = {0};
  if (!game_coords_to_notation(from_row, from_col, from_notation) ||
      !game_coords_to_notation(to_row, to_col, to_notation)) {
    return false;
  }
  if (opening_state.awaiting_opponent_physical) {
    return (strcmp(from_notation, opening_state.expected_from) == 0 &&
            strcmp(to_notation, opening_state.expected_to) == 0);
  }
  if (!opening_is_player_ply(opening_state.ply_index)) {
    return false;
  }
  return (strcmp(from_notation, opening_state.expected_from) == 0 &&
          strcmp(to_notation, opening_state.expected_to) == 0);
}

bool game_opening_validate_opponent_pickup(uint8_t from_row, uint8_t from_col) {
  if (!opening_state.awaiting_opponent_physical) {
    return false;
  }
  char from_notation[4] = {0};
  if (!game_coords_to_notation(from_row, from_col, from_notation)) {
    return false;
  }
  return strcmp(from_notation, opening_state.expected_from) == 0;
}

void game_opening_advance_after_opponent_physical(void) {
  if (!opening_state.active || !opening_state.awaiting_opponent_physical) {
    return;
  }
  opening_state.awaiting_opponent_physical = false;
  opening_state.last_wrong_uci[0] = '\0';
  opening_state.wrong_move_count = 0;
  opening_state.ply_index++;
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    game_led_flush();
  game_event_post_game_end(0, 0);
    STAGING_LOGI(TAG, "line complete id=%s", opening_state.line_id);
    return;
  }
  opening_run_until_player_turn();
}

void game_opening_advance_after_correct(void) {
  if (!opening_state.active) {
    return;
  }
  opening_state.last_wrong_uci[0] = '\0';
  opening_state.wrong_move_count = 0;
  opening_state.feedback = OPENING_FEEDBACK_CORRECT;
  opening_state.ply_index++;
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    game_led_flush();
  game_event_post_game_end(0, 0);
    STAGING_LOGI(TAG, "line complete id=%s", opening_state.line_id);
    return;
  }
  if (opening_should_checkpoint_after_ply(opening_state.ply_index - 1)) {
    opening_state.awaiting_checkpoint_ack = true;
    opening_state.feedback = OPENING_FEEDBACK_CHECKPOINT;
    return;
  }
  opening_run_until_player_turn();
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    game_led_flush();
  game_event_post_game_end(0, 0);
  } else if (!opening_state.awaiting_checkpoint_ack &&
             !opening_state.awaiting_opponent_physical) {
    opening_state.feedback = OPENING_FEEDBACK_NONE;
  }
}

bool game_opening_on_correct_player_move(uint8_t from_row, uint8_t from_col,
                                         uint8_t to_row, uint8_t to_col) {
  (void)from_row;
  (void)from_col;
  (void)to_row;
  (void)to_col;
  game_opening_advance_after_correct();
  return true;
}

bool game_opening_on_wrong_player_move(void) {
  if (!opening_state.active) {
    return false;
  }
  if (opening_state.wrong_move_count < 255) {
    opening_state.wrong_move_count++;
  }
  if (opening_state.wrong_move_count >= 3) {
    opening_state.feedback = OPENING_FEEDBACK_MISTAKE_HINT;
    opening_show_hint_led();
  } else {
    opening_state.feedback = OPENING_FEEDBACK_WRONG;
  }
  return true;
}

void game_opening_record_wrong_uci(uint8_t from_row, uint8_t from_col,
                                     uint8_t to_row, uint8_t to_col) {
  char from_sq[4] = {0};
  char to_sq[4] = {0};
  if (!game_coords_to_notation(from_row, from_col, from_sq) ||
      !game_coords_to_notation(to_row, to_col, to_sq)) {
    opening_state.last_wrong_uci[0] = '\0';
    return;
  }
  opening_state.last_wrong_uci[0] = from_sq[0];
  opening_state.last_wrong_uci[1] = from_sq[1];
  opening_state.last_wrong_uci[2] = to_sq[0];
  opening_state.last_wrong_uci[3] = to_sq[1];
  opening_state.last_wrong_uci[4] = '\0';
  STAGING_LOGI(TAG, "wrong uci recorded %s ply=%u", opening_state.last_wrong_uci,
               (unsigned)opening_state.ply_index);
}

bool game_opening_on_illegal_player_move(void) {
  if (!opening_state.active) {
    return false;
  }
  if (opening_state.wrong_move_count < 255) {
    opening_state.wrong_move_count++;
  }
  if (opening_state.wrong_move_count >= 3) {
    opening_state.feedback = OPENING_FEEDBACK_MISTAKE_HINT;
    opening_show_hint_led();
  } else {
    opening_state.feedback = OPENING_FEEDBACK_ILLEGAL;
  }
  return true;
}

static void opening_on_lift(const position_t *pos, uint8_t square) {
  (void)pos;
  (void)square;
  game_opening_on_opponent_piece_lifted();
}

static void opening_on_drop(const position_t *pos, uint8_t from, uint8_t to,
                            bool has_from) {
  uint8_t fr = from / 8u;
  uint8_t fc = from % 8u;
  uint8_t tr = to / 8u;
  uint8_t tc = to % 8u;
  position_move_t mv;
  (void)has_from;
  if (!opening_state.active) {
    return;
  }
  if (game_opening_validate_expected_move(fr, fc, tr, tc)) {
    return;
  }
  game_opening_record_wrong_uci(fr, fc, tr, tc);
  mv.from = from;
  mv.to = to;
  mv.promo = POS_PROMO_NONE;
  if (pos != NULL && position_legal(pos, mv)) {
    game_opening_on_wrong_player_move();
  } else {
    game_opening_on_illegal_player_move();
  }
  game_mode_reject_drop();
}

static void opening_on_move_applied(const position_t *pos, position_move_t mv);

static void opening_on_leds(const position_t *pos) {
  (void)pos;
}

static const game_mode_t s_opening_mode = {
    opening_on_lift,
    opening_on_drop,
    opening_on_move_applied,
    opening_on_leds,
};

static void opening_on_move_applied(const position_t *pos, position_move_t mv) {
  (void)pos;
  (void)mv;
  if (!opening_state.active || (*game_state_promotion_state()).pending) {
    return;
  }
  if (opening_state.awaiting_opponent_physical) {
    game_opening_advance_after_opponent_physical();
  } else {
    game_opening_advance_after_correct();
  }
  if ((*game_state_error_recovery_state()).waiting_for_move_correction) {
    game_reset_error_recovery_state();
  }
  if (!opening_state.active) {
    opening_mode_unbind();
  }
}

static void opening_mode_bind(void) { game_mode_set(&s_opening_mode); }

static void opening_mode_unbind(void) { game_mode_clear(&s_opening_mode); }

const char *game_opening_feedback_key(void) {
  switch (opening_state.feedback) {
  case OPENING_FEEDBACK_CORRECT:
    return "correct";
  case OPENING_FEEDBACK_WRONG:
    return "wrong";
  case OPENING_FEEDBACK_ILLEGAL:
    return "illegal";
  case OPENING_FEEDBACK_MISTAKE_HINT:
    return "mistake_hint";
  case OPENING_FEEDBACK_COMPLETE:
    return "complete";
  case OPENING_FEEDBACK_CHECKPOINT:
    return "checkpoint";
  case OPENING_FEEDBACK_OPPONENT_TURN:
    return "opponent_turn";
  default:
    return "none";
  }
}

const char *game_opening_opponent_mode_key(void) {
  return opening_state.opponent_mode == OPENING_OPPONENT_PHYSICAL ? "physical"
                                                                  : "virtual";
}

const char *game_opening_mode_key(void) {
  switch (opening_state.mode) {
  case OPENING_MODE_DRILL:
    return "drill";
  case OPENING_MODE_TIMED:
    return "timed";
  case OPENING_MODE_MIRROR:
    return "mirror";
  default:
    return "learn";
  }
}

bool game_opening_status_needs_squares(void) {
  return opening_state.awaiting_checkpoint_ack || opening_state.setup_phase;
}

static bool opening_export_active(void) {
  return opening_state.active || opening_state.setup_phase ||
         opening_state.line_uci_count > 0;
}

static void opening_fill_expected_occupied(uint8_t out[64]) {
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t row = i / 8;
    uint8_t col = i % 8;
    out[i] = (game_get_piece(row, col) == PIECE_EMPTY) ? 0 : 1;
  }
}

void game_opening_export_status_json(char *buf, size_t buf_size, size_t *offset) {
  if (buf == NULL || offset == NULL || buf_size == 0) {
    return;
  }
  if (!opening_export_active()) {
    return;
  }
  bool physical_match = game_opening_physical_matches_start();
  int n = snprintf(
      buf + *offset, buf_size - *offset,
      ",\"opening_training\":{\"active\":%s,\"setup_phase\":%s,\"mode\":\"%s\","
      "\"opponent_mode\":\"%s\",\"line_id\":\"%s\",\"ply_index\":%u,\"ply_total\":%u,"
      "\"player_ply_index\":%u,\"player_ply_total\":%u,"
      "\"player_side\":\"%s\",\"feedback\":\"%s\","
      "\"expected_from\":\"%s\",\"expected_to\":\"%s\","
      "\"last_opponent_uci\":\"%s\","
      "\"checkpoint_required\":%s,\"awaiting_checkpoint_ack\":%s,"
      "\"awaiting_opponent_physical\":%s,"
      "\"physical_synced\":%s,\"physical_match\":%s,"
      "\"wrong_move_count\":%u,\"last_wrong_uci\":\"%s\"",
      opening_state.active ? "true" : "false",
      opening_state.setup_phase ? "true" : "false", game_opening_mode_key(),
      game_opening_opponent_mode_key(),
      opening_state.line_id, (unsigned)opening_state.ply_index,
      (unsigned)opening_state.line_uci_count,
      (unsigned)opening_state.player_ply_index,
      (unsigned)opening_state.player_ply_count,
      opening_state.player_side == PLAYER_WHITE ? "white" : "black",
      game_opening_feedback_key(), opening_state.expected_from,
      opening_state.expected_to, opening_state.last_opponent_uci,
      opening_state.awaiting_checkpoint_ack ? "true" : "false",
      opening_state.awaiting_checkpoint_ack ? "true" : "false",
      opening_state.awaiting_opponent_physical ? "true" : "false",
      game_opening_validate_checkpoint_physical() ? "true" : "false",
      physical_match ? "true" : "false",
      (unsigned)opening_state.wrong_move_count,
      opening_state.last_wrong_uci[0] != '\0' ? opening_state.last_wrong_uci : "");
  if (n <= 0 || (size_t)n >= buf_size - *offset) {
    return;
  }
  *offset += (size_t)n;

  if (opening_state.awaiting_checkpoint_ack) {
    uint8_t expected[64];
    opening_fill_expected_occupied(expected);
    n = snprintf(buf + *offset, buf_size - *offset,
                 ",\"checkpoint_expected_occupied\":[");
    if (n <= 0 || (size_t)n >= buf_size - *offset) {
      return;
    }
    *offset += (size_t)n;
    for (int i = 0; i < 64; i++) {
      n = snprintf(buf + *offset, buf_size - *offset, "%s%u",
                   (i > 0) ? "," : "", (unsigned)expected[i]);
      if (n <= 0 || (size_t)n >= buf_size - *offset) {
        return;
      }
      *offset += (size_t)n;
    }
    n = snprintf(buf + *offset, buf_size - *offset, "]");
    if (n <= 0 || (size_t)n >= buf_size - *offset) {
      return;
    }
    *offset += (size_t)n;
  }

  n = snprintf(buf + *offset, buf_size - *offset, "}");
  if (n > 0 && (size_t)n < buf_size - *offset) {
    *offset += (size_t)n;
  }
}

void game_opening_service_hall_move(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  static uint32_t s_since;
  static bool s_armed;
  uint8_t occ[64];
  uint8_t fr = 0;
  uint8_t fc = 0;
  uint8_t tr = 0;
  uint8_t tc = 0;
  uint32_t now;
  piece_t mover;
  chess_move_t move;
  bool castle;
  bool ep;
  uint8_t rook_from = 0;
  uint8_t rook_to = 0;
  uint8_t sq;
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack ||
      opening_state.expected_from[0] == '\0' ||
      !hall_i2c_squares_readings_usable()) {
    s_armed = false;
    return;
  }
  if (!game_notation_to_coords(opening_state.expected_from, &fr, &fc) ||
      !game_notation_to_coords(opening_state.expected_to, &tr, &tc)) {
    s_armed = false;
    return;
  }
  if (!game_opening_validate_expected_move(fr, fc, tr, tc)) {
    s_armed = false;
    return;
  }
  mover = game_get_piece(fr, fc);
  if (mover == PIECE_EMPTY) {
    s_armed = false;
    return;
  }
  castle = (mover == PIECE_WHITE_KING || mover == PIECE_BLACK_KING) && fr == tr &&
           (fc > tc ? fc - tc : tc - fc) == 2;
  ep = en_passant_available && tr == en_passant_target_row &&
       tc == en_passant_target_col &&
       (mover == PIECE_WHITE_PAWN || mover == PIECE_BLACK_PAWN);
  if (castle) {
    rook_from = (uint8_t)(fr * 8 + (tc > fc ? 7 : 0));
    rook_to = (uint8_t)(fr * 8 + (tc > fc ? 5 : 3));
  }
  squares_get_state(occ);
  for (sq = 0; sq < 64; sq++) {
    bool want = game_get_piece(sq / 8, sq % 8) != PIECE_EMPTY;
    if (sq == (uint8_t)(fr * 8 + fc)) {
      want = false;
    }
    if (sq == (uint8_t)(tr * 8 + tc)) {
      want = true;
    }
    if (castle && sq == rook_from) {
      want = false;
    }
    if (castle && sq == rook_to) {
      want = true;
    }
    if (ep &&
        sq == (uint8_t)(en_passant_victim_row * 8 + en_passant_victim_col)) {
      want = false;
    }
    if ((occ[sq] != 0) != want) {
      s_armed = false;
      return;
    }
  }
  now = (uint32_t)(esp_timer_get_time() / 1000);
  if (!s_armed) {
    s_armed = true;
    s_since = now;
    return;
  }
  if ((uint32_t)(now - s_since) < 150U) {
    return;
  }
  s_armed = false;
  memset(&move, 0, sizeof(move));
  move.from_row = fr;
  move.from_col = fc;
  move.to_row = tr;
  move.to_col = tc;
  move.piece = mover;
  move.captured_piece = game_get_piece(tr, tc);
  {
    board_machine_out_t bout = game_board_on_drop_squares(fr, fc, tr, tc, true);
    if ((bout.kind != BM_OUT_MOVE && bout.kind != BM_OUT_NEED_PROMO) ||
        !game_board_apply_machine_move(&bout, &move)) {
      return;
    }
  }
  if (castle) {
    board_machine_reset(game_board_machine());
  }
  game_cmd_reply("Opening move accepted.", false, NULL);
#endif
}
