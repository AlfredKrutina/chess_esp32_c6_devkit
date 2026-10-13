/**
 * @file game_puzzle.c
 * @brief Puzzle session: physical setup, gated start, line check, LED feedback.
 *
 * The app stages a FEN plus a solver-first UCI line (solver, opponent, solver…).
 * Play does not begin until the matrix matches that FEN. Opponent plies are
 * shown on LEDs and must be executed on the board so logic and wood stay together.
 */

#include "game_task_internal.h"
#include "game_board_core.h"
#include "game_task.h"
#include "../squares_task/include/squares_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "game_hall_match.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_PUZZLE";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

#define PUZZLE_FEN_MAX 120
#define PUZZLE_TITLE_MAX 48
#define PUZZLE_MAX_PLIES 12
#define PUZZLE_UCI_MAX 8

/** Apply the line already staged by HTTP/BLE. */
#define PUZZLE_CMD_APPLY_SETUP 210U
/** Begin play if the staged FEN matches the matrix. */
#define PUZZLE_CMD_START_STAGED 211U

static const game_puzzle_definition_t game_puzzles[] = {
    {1, 1, "Mate in 1 – Queen on the back rank",
     "Classic motif: open f-file, queen delivers mate on f8.",
     "7k/7p/8/8/8/8/5Q2/6K1 w - - 0 1", "f2", "f8"},
    {2, 2, "Mate in 1 – Queen along the file",
     "Attack along the file: queen rises from b2 to b8.",
     "6k1/5ppp/8/8/8/8/1Q6/6K1 w - - 0 1", "b2", "b8"},
    {3, 3, "Mate in 1 – Rook takes rook",
     "Back-rank tactic: white rook takes black on e8 and mates the king.",
     "4r1k1/5ppp/8/8/8/8/4R3/4K3 w - - 0 1", "e2", "e8"},
    {4, 4, "Mate in 1 – Scholar's mate",
     "Well-known demo position: bishop on c4, queen on h5 — mate on f7.",
     "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1",
     "h5", "f7"},
    {5, 5, "Mate in 1 – Queen to d8",
     "Central strike: queen from d4 delivers mate on d8.",
     "6k1/5ppp/8/8/3Q4/8/6PP/6K1 w - - 0 1", "d4", "d8"},
};

bool puzzle_active = false;
bool puzzle_setup_active = false;
uint8_t puzzle_setup_id = 0;
uint8_t puzzle_active_id = 0;
uint8_t puzzle_solution_from_row = 0;
uint8_t puzzle_solution_from_col = 0;
uint8_t puzzle_solution_to_row = 0;
uint8_t puzzle_solution_to_col = 0;
puzzle_feedback_t puzzle_feedback = PUZZLE_FEEDBACK_NONE;

static bool puzzle_session_loaded = false;
static bool puzzle_solved = false;
static bool puzzle_awaiting_opponent = false;
static bool puzzle_sync_hold = false;
static bool puzzle_setup_match_latched = false;
static char puzzle_fen[PUZZLE_FEN_MAX];
static char puzzle_title[PUZZLE_TITLE_MAX];
static char puzzle_solution[PUZZLE_MAX_PLIES][PUZZLE_UCI_MAX];
static uint8_t puzzle_solution_count = 0;
static uint8_t puzzle_ply = 0;
static char puzzle_expected_from[3];
static char puzzle_expected_to[3];
static char puzzle_last_uci[PUZZLE_UCI_MAX];
static uint16_t puzzle_session = 0;

static bool game_fen_expected_occupancy_64(const char *fen, uint8_t exp[64]) {
  memset(exp, 0, 64);
  if (fen == NULL) {
    return false;
  }
  int row = 7;
  int col = 0;
  const char *p = fen;
  while (*p != '\0' && *p != ' ') {
    if (*p == '/') {
      if (col != 8) {
        return false;
      }
      row--;
      col = 0;
      p++;
      continue;
    }
    if (*p >= '1' && *p <= '8') {
      col += (*p - '0');
      if (col > 8) {
        return false;
      }
      p++;
      continue;
    }
    if (row < 0 || row > 7 || col < 0 || col > 7) {
      return false;
    }
    exp[row * 8 + col] = 1;
    col++;
    p++;
  }
  return (row == 0 && col == 8 && *p == ' ');
}

bool game_physical_squares_matches_fen(const char *fen) {
  uint8_t exp[64];
  if (!game_fen_expected_occupancy_64(fen, exp)) {
    return false;
  }
  uint8_t phys[64];
  squares_get_state(phys);
  for (int i = 0; i < 64; i++) {
    uint8_t want = exp[i] ? (uint8_t)1 : (uint8_t)0;
    uint8_t got = phys[i] ? (uint8_t)1 : (uint8_t)0;
    if (want != got) {
      return false;
    }
  }
  return true;
}

bool game_puzzle_physical_matches_fen(const char *fen) {
  return game_physical_squares_matches_fen(fen);
}

static bool puzzle_uci_ok(const char *uci) {
  if (uci == NULL) {
    return false;
  }
  size_t n = strlen(uci);
  if (n != 4 && n != 5) {
    return false;
  }
  if (uci[0] < 'a' || uci[0] > 'h' || uci[2] < 'a' || uci[2] > 'h') {
    return false;
  }
  if (uci[1] < '1' || uci[1] > '8' || uci[3] < '1' || uci[3] > '8') {
    return false;
  }
  if (n == 5) {
    char p = uci[4];
    if (p != 'q' && p != 'r' && p != 'b' && p != 'n') {
      return false;
    }
  }
  return true;
}

static void puzzle_clear_expected(void) {
  puzzle_expected_from[0] = '\0';
  puzzle_expected_to[0] = '\0';
  puzzle_solution_from_row = 0;
  puzzle_solution_from_col = 0;
  puzzle_solution_to_row = 0;
  puzzle_solution_to_col = 0;
}

static void puzzle_forget_session(void) {
  puzzle_session_loaded = false;
  puzzle_solved = false;
  puzzle_awaiting_opponent = false;
  puzzle_sync_hold = false;
  puzzle_setup_match_latched = false;
  puzzle_fen[0] = '\0';
  puzzle_title[0] = '\0';
  puzzle_solution_count = 0;
  puzzle_ply = 0;
  puzzle_last_uci[0] = '\0';
  memset(puzzle_solution, 0, sizeof(puzzle_solution));
  puzzle_clear_expected();
}

static void puzzle_bind_expected_squares(void) {
  puzzle_clear_expected();
  if (puzzle_ply >= puzzle_solution_count) {
    return;
  }
  const char *uci = puzzle_solution[puzzle_ply];
  puzzle_expected_from[0] = uci[0];
  puzzle_expected_from[1] = uci[1];
  puzzle_expected_from[2] = '\0';
  puzzle_expected_to[0] = uci[2];
  puzzle_expected_to[1] = uci[3];
  puzzle_expected_to[2] = '\0';
  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!convert_notation_to_coords(puzzle_expected_from, &from_row, &from_col) ||
      !convert_notation_to_coords(puzzle_expected_to, &to_row, &to_col)) {
    puzzle_clear_expected();
    return;
  }
  puzzle_solution_from_row = from_row;
  puzzle_solution_from_col = from_col;
  puzzle_solution_to_row = to_row;
  puzzle_solution_to_col = to_col;
}

static void puzzle_show_play_leds(bool reveal_destination) {
  led_clear_board_only();
  if (puzzle_expected_from[0] == '\0') {
    return;
  }
  uint8_t row = 0, col = 0;
  if (convert_notation_to_coords(puzzle_expected_from, &row, &col)) {
    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 210, 0);
  }
  if (!reveal_destination || puzzle_expected_to[0] == '\0') {
    return;
  }
  if (convert_notation_to_coords(puzzle_expected_to, &row, &col)) {
    led_set_pixel_safe(chess_pos_to_led_index(row, col), 40, 90, 255);
  }
}

static bool puzzle_square_disagrees(uint8_t index, uint8_t want_occ,
                                    const uint8_t phys[64]) {
  uint8_t got = phys[index] ? (uint8_t)1 : (uint8_t)0;
  if (want_occ != got) {
    return true;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (puzzle_active && !puzzle_setup_active) {
    uint8_t row = index / 8;
    uint8_t col = index % 8;
    if (!game_hall_square_agrees_with_logic(index, board[row][col], phys[index])) {
      return true;
    }
  }
#else
  (void)want_occ;
#endif
  return false;
}

static bool puzzle_paint_target(const uint8_t exp[64]) {
  uint8_t phys[64];
  squares_get_state(phys);
  led_clear_board_only();
  bool match = true;
  for (int i = 0; i < 64; i++) {
    uint8_t want = exp[i] ? (uint8_t)1 : (uint8_t)0;
    uint8_t row = (uint8_t)(i / 8);
    uint8_t col = (uint8_t)(i % 8);
    uint8_t led = chess_pos_to_led_index(row, col);
    if (!puzzle_square_disagrees((uint8_t)i, want, phys)) {
      if (want) {
        led_set_pixel_safe(led, 0, 90, 0);
      }
      continue;
    }
    match = false;
    uint8_t got = phys[i] ? (uint8_t)1 : (uint8_t)0;
    if (want && !got) {
      led_set_pixel_safe(led, 210, 210, 210);
    } else {
      led_set_pixel_safe(led, 255, 0, 0);
    }
  }
  return match;
}

static void puzzle_expected_occupancy_from_logic(uint8_t exp[64]) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      exp[row * 8 + col] = (board[row][col] == PIECE_EMPTY) ? 0 : 1;
    }
  }
}

const game_puzzle_definition_t *game_get_puzzle_definition(uint8_t puzzle_id) {
  for (size_t i = 0; i < (sizeof(game_puzzles) / sizeof(game_puzzles[0])); i++) {
    if (game_puzzles[i].id == puzzle_id) {
      return &game_puzzles[i];
    }
  }
  return NULL;
}

bool game_puzzle_stage_line(const char *fen, const char *title,
                            const char solution[][PUZZLE_UCI_MAX],
                            uint8_t count) {
  uint8_t exp[64];
  if (fen == NULL || !game_fen_expected_occupancy_64(fen, exp)) {
    return false;
  }
  if (count > PUZZLE_MAX_PLIES) {
    return false;
  }
  if (solution == NULL && count > 0) {
    return false;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (!puzzle_uci_ok(solution[i])) {
      return false;
    }
  }

  puzzle_active = false;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_active_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_solved = false;
  puzzle_awaiting_opponent = false;
  puzzle_sync_hold = false;
  puzzle_setup_match_latched = false;
  puzzle_ply = 0;
  puzzle_last_uci[0] = '\0';
  puzzle_clear_expected();

  strncpy(puzzle_fen, fen, sizeof(puzzle_fen) - 1);
  puzzle_fen[sizeof(puzzle_fen) - 1] = '\0';
  puzzle_title[0] = '\0';
  if (title != NULL) {
    strncpy(puzzle_title, title, sizeof(puzzle_title) - 1);
    puzzle_title[sizeof(puzzle_title) - 1] = '\0';
  }
  memset(puzzle_solution, 0, sizeof(puzzle_solution));
  puzzle_solution_count = count;
  for (uint8_t i = 0; i < count; i++) {
    strncpy(puzzle_solution[i], solution[i], PUZZLE_UCI_MAX - 1);
  }
  puzzle_session++;
  if (puzzle_session == 0) {
    puzzle_session = 1;
  }
  puzzle_session_loaded = true;
  STAGING_LOGI(TAG, "puzzle: staged session=%u plies=%u",
               (unsigned)puzzle_session, (unsigned)count);
  return true;
}

bool game_puzzle_stage_builtin(uint8_t puzzle_id) {
  const game_puzzle_definition_t *puzzle = game_get_puzzle_definition(puzzle_id);
  if (puzzle == NULL) {
    return false;
  }
  char uci[PUZZLE_UCI_MAX];
  snprintf(uci, sizeof(uci), "%s%s", puzzle->solution_from, puzzle->solution_to);
  char line[1][PUZZLE_UCI_MAX];
  strncpy(line[0], uci, PUZZLE_UCI_MAX - 1);
  line[0][PUZZLE_UCI_MAX - 1] = '\0';
  if (!game_puzzle_stage_line(puzzle->fen, puzzle->title, line, 1)) {
    return false;
  }
  puzzle_active_id = puzzle_id;
  puzzle_setup_id = puzzle_id;
  return true;
}

static void puzzle_enter_empty_setup(void) {
  if (game_is_board_setup_tutorial_active()) {
    game_exit_board_setup_tutorial(false);
  }
  game_opening_cancel();
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  puzzle_setup_active = true;
  puzzle_active = false;
  puzzle_solved = false;
  puzzle_awaiting_opponent = false;
  puzzle_sync_hold = false;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_setup_match_latched = false;
}

bool game_puzzle_apply_staged_setup(void) {
  if (!puzzle_session_loaded || puzzle_fen[0] == '\0') {
    return false;
  }
  uint8_t keep_id = puzzle_setup_id != 0 ? puzzle_setup_id : puzzle_active_id;
  puzzle_enter_empty_setup();
  puzzle_setup_id = keep_id;
  uint8_t exp[64];
  if (!game_fen_expected_occupancy_64(puzzle_fen, exp)) {
    return false;
  }
  bool match = puzzle_paint_target(exp);
  puzzle_setup_match_latched = match;
  puzzle_feedback = match ? PUZZLE_FEEDBACK_READY : PUZZLE_FEEDBACK_NONE;
  STAGING_LOGI(TAG, "puzzle: setup session=%u match=%d",
               (unsigned)puzzle_session, match ? 1 : 0);
  game_bump_revision_and_notify();
  return true;
}

bool game_puzzle_enter_setup(uint8_t puzzle_id) {
  if (!game_puzzle_stage_builtin(puzzle_id)) {
    return false;
  }
  return game_puzzle_apply_staged_setup();
}

bool game_is_puzzle_setup_active(void) { return puzzle_setup_active; }

bool game_puzzle_blocks_new_game(void) {
  return puzzle_session_loaded || puzzle_active || puzzle_setup_active;
}

static bool puzzle_sensors_match_logic(void) {
  uint8_t exp[64];
  uint8_t phys[64];
  puzzle_expected_occupancy_from_logic(exp);
  squares_get_state(phys);
  for (int i = 0; i < 64; i++) {
    if (puzzle_square_disagrees((uint8_t)i, exp[i], phys)) {
      return false;
    }
  }
  return true;
}

bool game_puzzle_try_start_staged(void) {
  if (!puzzle_session_loaded || puzzle_fen[0] == '\0') {
    return false;
  }
  if (puzzle_active && !puzzle_solved && !puzzle_sync_hold) {
    return true;
  }
  if (!game_physical_squares_matches_fen(puzzle_fen)) {
    if (!puzzle_setup_active) {
      game_puzzle_apply_staged_setup();
    } else {
      uint8_t exp[64];
      if (game_fen_expected_occupancy_64(puzzle_fen, exp)) {
        puzzle_setup_match_latched = puzzle_paint_target(exp);
      }
    }
    STAGING_LOGI(TAG, "puzzle: start blocked session=%u (occupancy)",
                 (unsigned)puzzle_session);
    return false;
  }
  if (puzzle_solution_count == 0) {
    char fen_copy[PUZZLE_FEN_MAX];
    strncpy(fen_copy, puzzle_fen, sizeof(fen_copy) - 1);
    fen_copy[sizeof(fen_copy) - 1] = '\0';
    game_puzzle_cancel();
    game_start_new_game_from_fen(fen_copy);
    STAGING_LOGI(TAG, "puzzle: position only (no solution line)");
    return true;
  }

  uint8_t keep_id = puzzle_active_id != 0 ? puzzle_active_id : puzzle_setup_id;
  game_reset_game();

  player_t fen_player = PLAYER_WHITE;
  if (!game_load_position_from_fen(puzzle_fen, &fen_player)) {
    ESP_LOGE(TAG, "puzzle: failed to load staged FEN");
    puzzle_session_loaded = false;
    return false;
  }

  current_player = fen_player;
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  game_start_time = esp_timer_get_time() / 1000;
  last_move_time = game_start_time;
  board_setup_tutorial_active = false;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_active = true;
  puzzle_active_id = keep_id;
  puzzle_solved = false;
  puzzle_awaiting_opponent = false;
  puzzle_ply = 0;
  puzzle_last_uci[0] = '\0';
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_bind_expected_squares();
  if (puzzle_expected_from[0] == '\0') {
    ESP_LOGE(TAG, "puzzle: solution square parse failed");
    puzzle_active = false;
    return false;
  }

  puzzle_sync_hold = !puzzle_sensors_match_logic();
  if (puzzle_sync_hold) {
    uint8_t exp[64];
    puzzle_expected_occupancy_from_logic(exp);
    puzzle_paint_target(exp);
    STAGING_LOGI(TAG, "puzzle: started but sensor hold session=%u",
                 (unsigned)puzzle_session);
  } else {
    puzzle_show_play_leds(false);
    STAGING_LOGI(TAG, "puzzle: play session=%u plies=%u",
                 (unsigned)puzzle_session, (unsigned)puzzle_solution_count);
  }
  game_bump_revision_and_notify();
  return true;
}

bool game_puzzle_start(uint8_t puzzle_id) {
  if (!game_puzzle_stage_builtin(puzzle_id)) {
    return false;
  }
  return game_puzzle_try_start_staged();
}

void game_puzzle_cancel(void) {
  bool was = puzzle_active || puzzle_setup_active || puzzle_session_loaded;
  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_forget_session();
  if (was) {
    led_clear_board_only();
    STAGING_LOGI(TAG, "puzzle: cancelled");
    game_bump_revision_and_notify();
  }
}

bool game_is_puzzle_active(void) { return puzzle_active; }

bool game_puzzle_is_solved(void) { return puzzle_active && puzzle_solved; }

bool game_puzzle_awaiting_opponent(void) {
  return puzzle_active && puzzle_awaiting_opponent && !puzzle_solved &&
         !puzzle_sync_hold;
}

bool game_puzzle_sync_hold(void) {
  return (puzzle_setup_active && puzzle_session_loaded &&
          !puzzle_setup_match_latched) ||
         (puzzle_active && puzzle_sync_hold);
}

bool game_puzzle_drop_is_expected(uint8_t from_row, uint8_t from_col,
                                 uint8_t to_row, uint8_t to_col) {
  if (!puzzle_active || puzzle_solved || puzzle_sync_hold) {
    return false;
  }
  return from_row == puzzle_solution_from_row &&
         from_col == puzzle_solution_from_col &&
         to_row == puzzle_solution_to_row && to_col == puzzle_solution_to_col;
}

void game_puzzle_after_accepted_move(void) {
  if (!puzzle_active || puzzle_solved) {
    return;
  }
  if (puzzle_expected_from[0] != '\0' && puzzle_expected_to[0] != '\0') {
    const char *uci = puzzle_solution[puzzle_ply];
    strncpy(puzzle_last_uci, uci, sizeof(puzzle_last_uci) - 1);
    puzzle_last_uci[sizeof(puzzle_last_uci) - 1] = '\0';
  }
  puzzle_ply++;
  puzzle_sync_hold = false;
  if (puzzle_ply >= puzzle_solution_count) {
    puzzle_solved = true;
    puzzle_awaiting_opponent = false;
    puzzle_feedback = PUZZLE_FEEDBACK_SOLVED;
    puzzle_clear_expected();
    led_clear_board_only();
    STAGING_LOGI(TAG, "puzzle: solved session=%u", (unsigned)puzzle_session);
    game_bump_revision_and_notify();
    return;
  }
  puzzle_bind_expected_squares();
  if ((puzzle_ply % 2U) == 1U) {
    puzzle_awaiting_opponent = true;
    puzzle_feedback = PUZZLE_FEEDBACK_OPPONENT;
    puzzle_show_play_leds(true);
    STAGING_LOGI(TAG, "puzzle: opponent ply=%u %s", (unsigned)puzzle_ply,
                 puzzle_last_uci);
  } else {
    puzzle_awaiting_opponent = false;
    puzzle_feedback = PUZZLE_FEEDBACK_NONE;
    puzzle_show_play_leds(false);
    STAGING_LOGI(TAG, "puzzle: solver ply=%u", (unsigned)puzzle_ply);
  }
  game_bump_revision_and_notify();
}

bool game_puzzle_hint(void) {
  if (!puzzle_active || puzzle_solved || puzzle_sync_hold ||
      puzzle_expected_to[0] == '\0') {
    return false;
  }
  puzzle_show_play_leds(true);
  STAGING_LOGI(TAG, "puzzle: hint %s%s", puzzle_expected_from, puzzle_expected_to);
  return true;
}

void game_puzzle_service_leds(void) {
  if (error_recovery_state.waiting_for_move_correction || piece_lifted) {
    return;
  }
  if (puzzle_setup_active && puzzle_fen[0] != '\0') {
    uint8_t exp[64];
    if (!game_fen_expected_occupancy_64(puzzle_fen, exp)) {
      return;
    }
    bool match = puzzle_paint_target(exp);
    if (match != puzzle_setup_match_latched) {
      puzzle_setup_match_latched = match;
      puzzle_feedback = match ? PUZZLE_FEEDBACK_READY : PUZZLE_FEEDBACK_NONE;
      STAGING_LOGI(TAG, "puzzle: setup match -> %d", match ? 1 : 0);
      game_bump_revision_and_notify();
    }
    return;
  }
  if (!puzzle_active || puzzle_solved) {
    return;
  }
  bool match = puzzle_sensors_match_logic();
  if (!match) {
    uint8_t exp[64];
    puzzle_expected_occupancy_from_logic(exp);
    puzzle_paint_target(exp);
    if (!puzzle_sync_hold) {
      puzzle_sync_hold = true;
      STAGING_LOGI(TAG, "puzzle: sync hold (board drifted)");
      game_bump_revision_and_notify();
    }
    return;
  }
  if (puzzle_sync_hold) {
    puzzle_sync_hold = false;
    puzzle_show_play_leds(puzzle_awaiting_opponent);
    STAGING_LOGI(TAG, "puzzle: sync hold cleared");
    game_bump_revision_and_notify();
  }
}

bool game_puzzle_suppress_move_hints(void) {
  return puzzle_active &&
         (puzzle_solved || puzzle_awaiting_opponent || puzzle_sync_hold);
}

const char *game_puzzle_staged_fen(void) { return puzzle_fen; }

const char *game_puzzle_title(void) { return puzzle_title; }

uint8_t game_puzzle_ply(void) { return puzzle_ply; }

uint8_t game_puzzle_ply_total(void) { return puzzle_solution_count; }

const char *game_puzzle_expected_from(void) { return puzzle_expected_from; }

const char *game_puzzle_expected_to(void) { return puzzle_expected_to; }

const char *game_puzzle_last_uci(void) { return puzzle_last_uci; }

uint16_t game_puzzle_session_id(void) { return puzzle_session; }

bool game_puzzle_status_physical_match(void) {
  if (puzzle_setup_active && puzzle_fen[0] != '\0') {
    return game_physical_squares_matches_fen(puzzle_fen);
  }
  if (puzzle_active) {
    return puzzle_sensors_match_logic();
  }
  return false;
}

const char *game_puzzle_feedback_key(void) {
  if (puzzle_active && puzzle_sync_hold) {
    return "sync";
  }
  switch (puzzle_feedback) {
  case PUZZLE_FEEDBACK_WRONG:
    return "wrong";
  case PUZZLE_FEEDBACK_SOLVED:
    return "solved";
  case PUZZLE_FEEDBACK_ILLEGAL:
    return "illegal";
  case PUZZLE_FEEDBACK_OPPONENT:
    return "opponent";
  case PUZZLE_FEEDBACK_READY:
    return "ready";
  case PUZZLE_FEEDBACK_NONE:
  default:
    return "none";
  }
}

const char *game_puzzle_feedback_message(void) {
  if (puzzle_setup_active && !puzzle_setup_match_latched) {
    return "Green is done, white is missing, red must be cleared.";
  }
  if (puzzle_active && puzzle_sync_hold) {
    return "Board does not match the puzzle. Fix the lit squares before moving.";
  }
  switch (puzzle_feedback) {
  case PUZZLE_FEEDBACK_WRONG:
    return "This is not a correct solution, return the figure to the original square.";
  case PUZZLE_FEEDBACK_SOLVED:
    return "Correct! Puzzle solved.";
  case PUZZLE_FEEDBACK_ILLEGAL:
    return "Illegal move, try another.";
  case PUZZLE_FEEDBACK_OPPONENT:
    return "Opponent move: yellow piece goes to the blue square.";
  case PUZZLE_FEEDBACK_READY:
    return "Position matches. Puzzle can start.";
  case PUZZLE_FEEDBACK_NONE:
  default:
    return "";
  }
}
