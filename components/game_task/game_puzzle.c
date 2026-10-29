/**
 * @file game_puzzle.c
 * @brief Puzzle session: physical setup, gated start, line check, LED feedback.
 *
 * The app stages a FEN plus a solver-first UCI line (solver, opponent, solver…).
 * Play does not begin until the matrix matches that FEN. Opponent plies are
 * shown on LEDs and must be executed on the board so logic and wood stay together.
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
#include "../squares_task/include/squares_task.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "game_hall_match.h"
#include "game_hall_restore_guide.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_PUZZLE";

static void puzzle_mode_bind(void);
static void puzzle_mode_unbind(void);

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

static bool s_puzzle_active = false;

bool *game_state_puzzle_active(void) { return &s_puzzle_active; }
#define puzzle_active (*game_state_puzzle_active())
static bool s_puzzle_setup_active = false;

bool *game_state_puzzle_setup_active(void) { return &s_puzzle_setup_active; }
#define puzzle_setup_active (*game_state_puzzle_setup_active())
static uint8_t s_puzzle_setup_id = 0;

uint8_t *game_state_puzzle_setup_id(void) { return &s_puzzle_setup_id; }
#define puzzle_setup_id (*game_state_puzzle_setup_id())
static uint8_t s_puzzle_active_id = 0;

uint8_t *game_state_puzzle_active_id(void) { return &s_puzzle_active_id; }
#define puzzle_active_id (*game_state_puzzle_active_id())
static uint8_t s_puzzle_solution_from_row = 0;

uint8_t *game_state_puzzle_solution_from_row(void) { return &s_puzzle_solution_from_row; }
#define puzzle_solution_from_row (*game_state_puzzle_solution_from_row())
static uint8_t s_puzzle_solution_from_col = 0;

uint8_t *game_state_puzzle_solution_from_col(void) { return &s_puzzle_solution_from_col; }
#define puzzle_solution_from_col (*game_state_puzzle_solution_from_col())
static uint8_t s_puzzle_solution_to_row = 0;

uint8_t *game_state_puzzle_solution_to_row(void) { return &s_puzzle_solution_to_row; }
#define puzzle_solution_to_row (*game_state_puzzle_solution_to_row())
static uint8_t s_puzzle_solution_to_col = 0;

uint8_t *game_state_puzzle_solution_to_col(void) { return &s_puzzle_solution_to_col; }
#define puzzle_solution_to_col (*game_state_puzzle_solution_to_col())
static puzzle_feedback_t s_puzzle_feedback = PUZZLE_FEEDBACK_NONE;

puzzle_feedback_t *game_state_puzzle_feedback(void) { return &s_puzzle_feedback; }
#define puzzle_feedback (*game_state_puzzle_feedback())

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
  if (!game_notation_to_coords(puzzle_expected_from, &from_row, &from_col) ||
      !game_notation_to_coords(puzzle_expected_to, &to_row, &to_col)) {
    puzzle_clear_expected();
    return;
  }
  puzzle_solution_from_row = from_row;
  puzzle_solution_from_col = from_col;
  puzzle_solution_to_row = to_row;
  puzzle_solution_to_col = to_col;
}

static void puzzle_show_play_leds(bool reveal_destination) {
  uint8_t row = 0;
  uint8_t col = 0;
  game_led_frame_begin(GAME_EVT_HINT_FADE_MS, false);
  if (puzzle_expected_from[0] != '\0' &&
      game_notation_to_coords(puzzle_expected_from, &row, &col)) {
    game_led_frame_sq(row, col, 255, 210, 0);
  }
  if (reveal_destination && puzzle_expected_to[0] != '\0' &&
      game_notation_to_coords(puzzle_expected_to, &row, &col)) {
    game_led_frame_sq(row, col, 40, 90, 255);
  }
  game_coach_hint_paint();
  game_led_frame_end();
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
    if (!game_hall_square_agrees_with_logic(index, game_get_piece(row, col), phys[index])) {
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
  game_led_clear_board();
  bool match = true;
  for (int i = 0; i < 64; i++) {
    uint8_t want = exp[i] ? (uint8_t)1 : (uint8_t)0;
    uint8_t row = (uint8_t)(i / 8);
    uint8_t col = (uint8_t)(i % 8);
    uint8_t led = chess_pos_to_led_index(row, col);
    if (!puzzle_square_disagrees((uint8_t)i, want, phys)) {
      if (want) {
        game_led_paint_led(led, 0, 90, 0);
      }
      continue;
    }
    match = false;
    uint8_t got = phys[i] ? (uint8_t)1 : (uint8_t)0;
    if (want && !got) {
      game_led_paint_led(led, 210, 210, 210);
    } else {
      game_led_paint_led(led, 255, 0, 0);
    }
  }
  return match;
}

static void puzzle_expected_occupancy_from_logic(uint8_t exp[64]) {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      exp[row * 8 + col] = (game_get_piece(row, col) == PIECE_EMPTY) ? 0 : 1;
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

static void puzzle_enter_setup_shell(void) {
  if (game_is_board_setup_tutorial_active()) {
    game_exit_board_setup_tutorial(false);
  }
  game_opening_cancel();
  game_coach_hint_clear();
  game_reset_game();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_force_resync();
#endif
}

static bool puzzle_setup_guide(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return game_hall_restore_guide_show_target();
#else
  uint8_t exp[64];
  if (!game_fen_expected_occupancy_64(puzzle_fen, exp)) {
    return false;
  }
  return puzzle_paint_target(exp);
#endif
}

bool game_puzzle_apply_staged_setup(void) {
  if (!puzzle_session_loaded || puzzle_fen[0] == '\0') {
    return false;
  }
  uint8_t keep_id = puzzle_setup_id != 0 ? puzzle_setup_id : puzzle_active_id;
  player_t fen_player = PLAYER_WHITE;
  puzzle_enter_setup_shell();
  puzzle_setup_id = keep_id;
  if (!game_load_position_from_fen(puzzle_fen, &fen_player)) {
    return false;
  }
  current_player = fen_player;
  current_game_state = GAME_STATE_IDLE;
  game_active = false;
  puzzle_setup_active = true;
  puzzle_active = false;
  puzzle_solved = false;
  puzzle_awaiting_opponent = false;
  puzzle_sync_hold = false;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_setup_match_latched = puzzle_setup_guide();
  puzzle_feedback =
      puzzle_setup_match_latched ? PUZZLE_FEEDBACK_READY : PUZZLE_FEEDBACK_NONE;
  STAGING_LOGI(TAG, "puzzle: setup session=%u match=%d",
               (unsigned)puzzle_session, puzzle_setup_match_latched ? 1 : 0);
  game_bump_revision_and_notify();
  puzzle_mode_bind();
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
  if (!puzzle_setup_active && !game_puzzle_apply_staged_setup()) {
    return false;
  }
  puzzle_setup_match_latched = puzzle_setup_guide();
  if (!puzzle_setup_match_latched) {
    STAGING_LOGI(TAG, "puzzle: start blocked session=%u (occupancy)",
                 (unsigned)puzzle_session);
    return false;
  }
  if (puzzle_solution_count == 0) {
    char fen_copy[PUZZLE_FEN_MAX];
    strncpy(fen_copy, puzzle_fen, sizeof(fen_copy) - 1);
    fen_copy[sizeof(fen_copy) - 1] = '\0';
    if (!game_start_new_game_from_fen(fen_copy)) {
      STAGING_LOGI(TAG, "puzzle: position-only FEN rejected");
      return false;
    }
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
  (*game_state_game_start_time()) = esp_timer_get_time() / 1000;
  (*game_state_last_move_time()) = (*game_state_game_start_time());
  (*game_state_board_setup_tutorial_active()) = false;
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

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_end_for_play();
#endif
  puzzle_sync_hold = !puzzle_sensors_match_logic();
  if (puzzle_sync_hold) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    (void)game_hall_restore_guide_show_play();
#else
    uint8_t exp[64];
    puzzle_expected_occupancy_from_logic(exp);
    puzzle_paint_target(exp);
#endif
    STAGING_LOGI(TAG, "puzzle: started but sensor hold session=%u",
                 (unsigned)puzzle_session);
  } else {
    puzzle_show_play_leds(false);
    STAGING_LOGI(TAG, "puzzle: play session=%u plies=%u",
                 (unsigned)puzzle_session, (unsigned)puzzle_solution_count);
  }
  game_bump_revision_and_notify();
  puzzle_mode_bind();
  return true;
}

bool game_puzzle_start(uint8_t puzzle_id) {
  if (!game_puzzle_stage_builtin(puzzle_id)) {
    return false;
  }
  return game_puzzle_try_start_staged();
}

void game_puzzle_cancel(void) {
  puzzle_mode_unbind();
  bool was = puzzle_active || puzzle_setup_active || puzzle_session_loaded;
  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  puzzle_forget_session();
  if (was) {
    game_led_clear_board();
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

static void puzzle_on_lift(const position_t *pos, uint8_t square) {
  (void)pos;
  (void)square;
}

static void puzzle_on_drop(const position_t *pos, uint8_t from, uint8_t to,
                           bool has_from) {
  uint8_t fr;
  uint8_t fc;
  uint8_t tr;
  uint8_t tc;
  position_move_t mv;
  (void)has_from;
  if (puzzle_setup_active || puzzle_sync_hold) {
    game_mode_reject_drop();
    return;
  }
  if (!puzzle_active || puzzle_solved) {
    return;
  }
  fr = from / 8u;
  fc = from % 8u;
  tr = to / 8u;
  tc = to % 8u;
  if (game_puzzle_drop_is_expected(fr, fc, tr, tc)) {
    return;
  }
  mv.from = from;
  mv.to = to;
  mv.promo = POS_PROMO_NONE;
  puzzle_feedback = (pos != NULL && position_legal(pos, mv)) ? PUZZLE_FEEDBACK_WRONG
                                                            : PUZZLE_FEEDBACK_ILLEGAL;
  game_mode_reject_drop();
}

static void puzzle_on_move_applied(const position_t *pos, position_move_t mv) {
  (void)pos;
  (void)mv;
  if (!puzzle_active || (*game_state_promotion_state()).pending) {
    return;
  }
  game_puzzle_after_accepted_move();
}

static void puzzle_on_leds(const position_t *pos) {
  (void)pos;
  game_puzzle_service_leds();
}

static const game_mode_t s_puzzle_mode = {
    puzzle_on_lift,
    puzzle_on_drop,
    puzzle_on_move_applied,
    puzzle_on_leds,
};

static void puzzle_mode_bind(void) { game_mode_set(&s_puzzle_mode); }

static void puzzle_mode_unbind(void) { game_mode_clear(&s_puzzle_mode); }

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
    game_led_clear_board();
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

void game_puzzle_refresh_play_leds(void) {
  if (!puzzle_active || puzzle_setup_active) {
    return;
  }
  puzzle_show_play_leds(puzzle_awaiting_opponent);
}

void game_puzzle_service_leds(void) {
  if ((*game_state_error_recovery_state()).waiting_for_move_correction) {
    return;
  }
  if (puzzle_setup_active && puzzle_fen[0] != '\0') {
    bool match = puzzle_setup_guide();
    if (match != puzzle_setup_match_latched) {
      puzzle_setup_match_latched = match;
      puzzle_feedback = match ? PUZZLE_FEEDBACK_READY : PUZZLE_FEEDBACK_NONE;
      STAGING_LOGI(TAG, "puzzle: setup match -> %d", match ? 1 : 0);
      game_bump_revision_and_notify();
    }
    return;
  }
  if (piece_lifted) {
    return;
  }
  if (!puzzle_active || puzzle_solved) {
    return;
  }
  bool match = puzzle_sensors_match_logic();
  if (!match) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    (void)game_hall_restore_guide_show_play();
#else
    uint8_t exp[64];
    puzzle_expected_occupancy_from_logic(exp);
    puzzle_paint_target(exp);
#endif
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
    return puzzle_setup_match_latched;
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

void game_puzzle_service_hall_move(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  static uint32_t s_since;
  static bool s_armed;
  uint8_t occ[64];
  uint8_t fr;
  uint8_t fc;
  uint8_t tr;
  uint8_t tc;
  uint32_t now;
  piece_t mover;
  chess_move_t move;
  bool castle;
  bool ep;
  uint8_t rook_from = 0;
  uint8_t rook_to = 0;
  uint8_t sq;
  if (!puzzle_active || puzzle_solved || puzzle_setup_active ||
      puzzle_expected_from[0] == '\0' ||
      !hall_i2c_squares_readings_usable()) {
    s_armed = false;
    return;
  }
  fr = puzzle_solution_from_row;
  fc = puzzle_solution_from_col;
  tr = puzzle_solution_to_row;
  tc = puzzle_solution_to_col;
  mover = game_get_piece(fr, fc);
  if (mover == PIECE_EMPTY) {
    s_armed = false;
    return;
  }
  castle = (mover == PIECE_WHITE_KING || mover == PIECE_BLACK_KING) &&
           fr == tr && (fc > tc ? fc - tc : tc - fc) == 2;
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
  game_cmd_reply("Puzzle move accepted.", false, NULL);
#else
  (void)0;
#endif
}
