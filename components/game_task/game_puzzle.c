/**
 * @file game_puzzle.c
 * @brief Chess puzzle definitions, setup, and lifecycle.
 */

#include "game_task_internal.h"
#include "game_board_core.h"
#include "game_task.h"
#include "../matrix_task/include/matrix_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

#include <string.h>

static const char *TAG = "GAME_PUZZLE";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

static const game_puzzle_definition_t game_puzzles[] = {
    {1, 1, "Mat 1 – Dáma na poslední řadě",
     "Klasický motiv: otevřený f-sloupec, dáma dá mat na f8.",
     "7k/7p/8/8/8/8/5Q2/6K1 w - - 0 1", "f2", "f8"},
    {2, 2, "Mat 1 – Dáma po sloupci",
     "Útok po ose: dáma stoupá z b2 na b8.",
     "6k1/5ppp/8/8/8/8/1Q6/6K1 w - - 0 1", "b2", "b8"},
    {3, 3, "Mat 1 – Věž bere věž",
     "Taktika zadní řady: bílá věž sebere černou na e8 a matuje krále.",
     "4r1k1/5ppp/8/8/8/8/4R3/4K3 w - - 0 1", "e2", "e8"},
    {4, 4, "Mat 1 – Školácký mat",
     "Známá ukázková pozice: střelec na c4, dáma na h5 — mat na f7.",
     "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 0 1",
     "h5", "f7"},
    {5, 5, "Mat 1 – Dáma na d8",
     "Centrální úder: dáma z d4 uzavře mat na poli d8.",
     "6k1/5ppp/8/8/3Q4/8/6PP/6K1 w - - 0 1", "d4", "d8"},
};
bool puzzle_active = false;
/** Příprava fyzické pozice před game_puzzle_start (prázdná logika, matrix v JSON). */
bool puzzle_setup_active = false;
uint8_t puzzle_setup_id = 0;
uint8_t puzzle_active_id = 0;
uint8_t puzzle_solution_from_row = 0;
uint8_t puzzle_solution_from_col = 0;
uint8_t puzzle_solution_to_row = 0;
uint8_t puzzle_solution_to_col = 0;
puzzle_feedback_t puzzle_feedback = PUZZLE_FEEDBACK_NONE;
const game_puzzle_definition_t *game_get_puzzle_definition(uint8_t puzzle_id) {
  for (size_t i = 0; i < (sizeof(game_puzzles) / sizeof(game_puzzles[0])); i++) {
    if (game_puzzles[i].id == puzzle_id) {
      return &game_puzzles[i];
    }
  }
  return NULL;
}

/** Očekávaná obsazenost (0/1) z FEN — jen placement, bez typů figurek. */
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

bool game_puzzle_physical_matches_fen(const char *fen) {
  uint8_t exp[64];
  if (!game_fen_expected_occupancy_64(fen, exp)) {
    return false;
  }
  uint8_t phys[64];
  matrix_get_state(phys);
  for (int i = 0; i < 64; i++) {
    uint8_t want = exp[i] ? (uint8_t)1 : (uint8_t)0;
    uint8_t got = phys[i] ? (uint8_t)1 : (uint8_t)0;
    if (want != got) {
      return false;
    }
  }
  return true;
}

bool game_puzzle_enter_setup(uint8_t puzzle_id) {
  const game_puzzle_definition_t *puzzle = game_get_puzzle_definition(puzzle_id);
  if (puzzle == NULL) {
    return false;
  }
  if (game_is_board_setup_tutorial_active()) {
    game_exit_board_setup_tutorial(false);
  }
  game_puzzle_cancel();
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  puzzle_setup_active = true;
  puzzle_setup_id = puzzle_id;
  led_clear_board_only();
  STAGING_LOGI(TAG, "puzzle: enter setup id=%u", (unsigned)puzzle_id);
  return true;
}

bool game_is_puzzle_setup_active(void) { return puzzle_setup_active; }

bool game_puzzle_start(uint8_t puzzle_id) {
  const game_puzzle_definition_t *puzzle = game_get_puzzle_definition(puzzle_id);
  if (puzzle == NULL) {
    return false;
  }

  game_reset_game();

  player_t fen_player = PLAYER_WHITE;
  if (!game_load_position_from_fen(puzzle->fen, &fen_player)) {
    ESP_LOGE(TAG, "puzzle: failed to load FEN for id=%u", (unsigned)puzzle_id);
    return false;
  }

  uint8_t from_row = 0, from_col = 0, to_row = 0, to_col = 0;
  if (!convert_notation_to_coords(puzzle->solution_from, &from_row, &from_col) ||
      !convert_notation_to_coords(puzzle->solution_to, &to_row, &to_col)) {
    ESP_LOGE(TAG, "puzzle: invalid solution notation for id=%u",
             (unsigned)puzzle_id);
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
  puzzle_active_id = puzzle_id;
  puzzle_solution_from_row = from_row;
  puzzle_solution_from_col = from_col;
  puzzle_solution_to_row = to_row;
  puzzle_solution_to_col = to_col;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  led_clear_board_only();
  game_highlight_movable_pieces();
  STAGING_LOGI(TAG, "puzzle: started id=%u difficulty=%u",
               (unsigned)puzzle->id, (unsigned)puzzle->difficulty);
  return true;
}

void game_puzzle_cancel(void) {
  bool was = puzzle_active || puzzle_setup_active;
  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  if (was) {
    STAGING_LOGI(TAG, "puzzle: cancelled");
  }
}

bool game_is_puzzle_active(void) {
  return puzzle_active;
}

const char *game_puzzle_feedback_key(void) {
  switch (puzzle_feedback) {
  case PUZZLE_FEEDBACK_WRONG:
    return "wrong";
  case PUZZLE_FEEDBACK_SOLVED:
    return "solved";
  case PUZZLE_FEEDBACK_ILLEGAL:
    return "illegal";
  case PUZZLE_FEEDBACK_NONE:
  default:
    return "none";
  }
}

const char *game_puzzle_feedback_message(void) {
  switch (puzzle_feedback) {
  case PUZZLE_FEEDBACK_WRONG:
    return "To neni spravne reseni, vrat figurku na puvodni pole.";
  case PUZZLE_FEEDBACK_SOLVED:
    return "Spravne! Puzzle je vyresene.";
  case PUZZLE_FEEDBACK_ILLEGAL:
    return "Nelegalni tah, zkus jiny.";
  case PUZZLE_FEEDBACK_NONE:
  default:
    return "";
  }
}
