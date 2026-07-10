/**
 * @file game_opening_trainer.c
 * @brief Interactive opening line trainer — multi-ply UCI validation + virtual opponent.
 */

#include "game_task_internal.h"
#include "game_board_core.h"
#include "game_task.h"

#include "../led_task/include/led_task.h"
#include "../matrix_task/include/matrix_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_OPENING";

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
  OPENING_FEEDBACK_COMPLETE,
  OPENING_FEEDBACK_CHECKPOINT
} opening_feedback_t;

typedef enum {
  OPENING_MODE_LEARN = 0,
  OPENING_MODE_DRILL,
  OPENING_MODE_TIMED,
  OPENING_MODE_MIRROR
} opening_mode_t;

typedef struct {
  bool active;
  bool setup_phase;
  opening_mode_t mode;
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
  opening_feedback_t feedback;
  player_t player_side;
  bool awaiting_checkpoint_ack;
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
  if (!convert_notation_to_coords(from_sq, &from_row, &from_col) ||
      !convert_notation_to_coords(to_sq, &to_row, &to_col)) {
    return;
  }
  uint8_t from_led = chess_pos_to_led_index(from_row, from_col);
  uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
  led_command_t move_path_cmd = {
      .type = LED_CMD_ANIM_MOVE_PATH,
      .led_index = from_led,
      .red = 0,
      .green = 120,
      .blue = 255,
      .duration_ms = 800,
      .data = &to_led,
  };
  led_execute_command_new(&move_path_cmd);
}

static void opening_show_hint_led(void) {
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack) {
    return;
  }
  if (opening_state.expected_from[0] == '\0' || opening_state.expected_to[0] == '\0') {
    return;
  }
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  uint8_t from_led = 64;
  if (convert_notation_to_coords(opening_state.expected_from, &from_row, &from_col)) {
    from_led = chess_pos_to_led_index(from_row, from_col);
  }
  if (!convert_notation_to_coords(opening_state.expected_to, &to_row, &to_col)) {
    return;
  }
  uint8_t to_led = chess_pos_to_led_index(to_row, to_col);
  led_command_t hint_cmd = {
      .type = LED_CMD_HIGHLIGHT_HINT,
      .led_index = from_led,
      .data = &to_led,
  };
  led_execute_command_new(&hint_cmd);
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
  if (!convert_notation_to_coords(from, &from_row, &from_col) ||
      !convert_notation_to_coords(to, &to_row, &to_col)) {
    return false;
  }
  piece_t piece = board[from_row][from_col];
  if (piece == PIECE_EMPTY) {
    return false;
  }
  chess_move_t move = {
      .from_row = from_row,
      .from_col = from_col,
      .to_row = to_row,
      .to_col = to_col,
      .piece = piece,
      .captured_piece = board[to_row][to_col],
      .timestamp = esp_timer_get_time() / 1000,
  };
  if (game_is_valid_move(&move) != MOVE_ERROR_NONE) {
    return false;
  }
  return game_execute_move(&move);
}

static void opening_run_virtual_until_player(void) {
  while (opening_state.active &&
         opening_state.ply_index < opening_state.line_uci_count &&
         !opening_is_player_ply(opening_state.ply_index)) {
    const char *uci = opening_state.line_uci[opening_state.ply_index];
    char from[3], to[3];
    opening_parse_uci(uci, from, to);
    if (game_opening_apply_uci(uci)) {
      strncpy(opening_state.last_opponent_uci, uci, sizeof(opening_state.last_opponent_uci) - 1);
      opening_play_move_led(from, to);
      STAGING_LOGI(TAG, "virtual reply ply=%u uci=%s", (unsigned)opening_state.ply_index, uci);
    } else {
      ESP_LOGE(TAG, "virtual uci failed: %s", uci);
      break;
    }
    opening_state.ply_index++;
    if (opening_is_checkpoint_ply(opening_state.ply_index - 1)) {
      opening_state.awaiting_checkpoint_ack = true;
      opening_state.feedback = OPENING_FEEDBACK_CHECKPOINT;
      return;
    }
  }
  opening_state.player_ply_index = opening_count_player_ply_index(opening_state.ply_index);
  opening_update_expected_move();
  opening_show_hint_led();
}

void game_opening_cancel(void) {
  bool was = opening_state.active || opening_state.setup_phase;
  memset(&opening_state, 0, sizeof(opening_state));
  if (was) {
    STAGING_LOGI(TAG, "opening cancelled");
  }
}

bool game_is_opening_trainer_active(void) { return opening_state.active; }

bool game_is_opening_trainer_setup_active(void) { return opening_state.setup_phase; }

bool game_opening_validate_checkpoint_physical(void) {
  uint8_t matrix[64] = {0};
  matrix_get_state(matrix);
  for (uint8_t i = 0; i < 64; i++) {
    uint8_t row = i / 8;
    uint8_t col = i % 8;
    uint8_t expected = (board[row][col] == PIECE_EMPTY) ? 0 : 1;
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
                              uint8_t player_side_white) {
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
  opening_state.player_side = player_side_white ? PLAYER_WHITE : PLAYER_BLACK;
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  STAGING_LOGI(TAG, "load line=%s plies=%u", line_id, (unsigned)line_uci_count);
  return true;
}

bool game_opening_start(void) {
  if (opening_state.line_uci_count == 0) {
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
  opening_state.feedback = OPENING_FEEDBACK_NONE;
  opening_state.last_opponent_uci[0] = '\0';
  led_clear_board_only();

  opening_run_virtual_until_player();
  STAGING_LOGI(TAG, "started line=%s ply=%u", opening_state.line_id,
               (unsigned)opening_state.ply_index);
  return true;
}

bool game_opening_hint(void) {
  if (!opening_state.active || opening_state.awaiting_checkpoint_ack) {
    return false;
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
  if (!opening_is_player_ply(opening_state.ply_index)) {
    return false;
  }
  char from_notation[3] = {0};
  char to_notation[3] = {0};
  snprintf(from_notation, sizeof(from_notation), "%c%d", 'a' + from_col, from_row + 1);
  snprintf(to_notation, sizeof(to_notation), "%c%d", 'a' + to_col, to_row + 1);
  return (strcmp(from_notation, opening_state.expected_from) == 0 &&
          strcmp(to_notation, opening_state.expected_to) == 0);
}

void game_opening_advance_after_correct(void) {
  if (!opening_state.active) {
    return;
  }
  opening_state.feedback = OPENING_FEEDBACK_CORRECT;
  opening_state.ply_index++;
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    led_command_t end_cmd = {.type = LED_CMD_ANIM_ENDGAME, .duration_ms = 1500};
    led_execute_command_new(&end_cmd);
    STAGING_LOGI(TAG, "line complete id=%s", opening_state.line_id);
    return;
  }
  if (opening_is_checkpoint_ply(opening_state.ply_index - 1)) {
    opening_state.awaiting_checkpoint_ack = true;
    opening_state.feedback = OPENING_FEEDBACK_CHECKPOINT;
    return;
  }
  opening_run_virtual_until_player();
  if (opening_state.ply_index >= opening_state.line_uci_count) {
    opening_state.active = false;
    opening_state.feedback = OPENING_FEEDBACK_COMPLETE;
    led_command_t end_cmd = {.type = LED_CMD_ANIM_ENDGAME, .duration_ms = 1500};
    led_execute_command_new(&end_cmd);
  } else if (!opening_state.awaiting_checkpoint_ack) {
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
  opening_state.feedback = OPENING_FEEDBACK_WRONG;
  return true;
}

bool game_opening_on_illegal_player_move(void) {
  if (!opening_state.active) {
    return false;
  }
  opening_state.feedback = OPENING_FEEDBACK_ILLEGAL;
  return true;
}

const char *game_opening_feedback_key(void) {
  switch (opening_state.feedback) {
  case OPENING_FEEDBACK_CORRECT:
    return "correct";
  case OPENING_FEEDBACK_WRONG:
    return "wrong";
  case OPENING_FEEDBACK_ILLEGAL:
    return "illegal";
  case OPENING_FEEDBACK_COMPLETE:
    return "complete";
  case OPENING_FEEDBACK_CHECKPOINT:
    return "checkpoint";
  default:
    return "none";
  }
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

bool game_opening_status_needs_matrix(void) {
  return opening_state.awaiting_checkpoint_ack || opening_state.setup_phase;
}

void game_opening_export_status_json(char *buf, size_t buf_size, size_t *offset) {
  if (buf == NULL || offset == NULL || buf_size == 0) {
    return;
  }
  if (!opening_state.active && !opening_state.setup_phase) {
    return;
  }
  int n = snprintf(
      buf + *offset, buf_size - *offset,
      ",\"opening_training\":{\"active\":%s,\"setup_phase\":%s,\"mode\":\"%s\","
      "\"line_id\":\"%s\",\"ply_index\":%u,\"ply_total\":%u,"
      "\"player_ply_index\":%u,\"player_ply_total\":%u,"
      "\"player_side\":\"%s\",\"feedback\":\"%s\","
      "\"expected_from\":\"%s\",\"expected_to\":\"%s\","
      "\"last_opponent_uci\":\"%s\","
      "\"checkpoint_required\":%s,\"awaiting_checkpoint_ack\":%s,"
      "\"physical_synced\":%s}",
      opening_state.active ? "true" : "false",
      opening_state.setup_phase ? "true" : "false", game_opening_mode_key(),
      opening_state.line_id, (unsigned)opening_state.ply_index,
      (unsigned)opening_state.line_uci_count,
      (unsigned)opening_state.player_ply_index,
      (unsigned)opening_state.player_ply_count,
      opening_state.player_side == PLAYER_WHITE ? "white" : "black",
      game_opening_feedback_key(), opening_state.expected_from,
      opening_state.expected_to, opening_state.last_opponent_uci,
      opening_state.awaiting_checkpoint_ack ? "true" : "false",
      opening_state.awaiting_checkpoint_ack ? "true" : "false",
      game_opening_validate_checkpoint_physical() ? "true" : "false");
  if (n > 0 && (size_t)n < buf_size - *offset) {
    *offset += (size_t)n;
  }
}
