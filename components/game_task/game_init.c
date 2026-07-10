/**
 * @file game_init.c
 * @brief Game reset, new game, board setup tutorial, and state revision.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_board_core.h"
#include "game_snapshot.h"
#include "game_matrix_guard.h"

#include "../matrix_task/include/matrix_task.h"
#include "../led_task/include/led_task.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "../game_hooks/include/game_state_notify.h"
#include "game_led_animations.h"
#include "led_mapping.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "GAME_INIT";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================

/** Vzorků matice při kontrole výchozí pozice (reed kontakty často „klepou“). */
#define GAME_START_POS_MATRIX_SAMPLES 5
#define GAME_START_POS_MAJORITY_VOTES 3

static bool game_board_starting_occupancy_matches(const uint8_t state[64]) {
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] != 0) {
        return false;
      }
    }
  }
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] == 0) {
        return false;
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] == 0) {
        return false;
      }
    }
  }
  return true;
}

static void game_log_first_startpos_mismatch(const uint8_t state[64]) {
  char sq[4];
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] != 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 3–6 square %s must be empty "
                 "(majority vote saw piece)",
                 sq);
        return;
      }
    }
  }
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] == 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 1–2 square %s must be occupied "
                 "(majority vote saw empty)",
                 sq);
        return;
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] == 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 7–8 square %s must be occupied "
                 "(majority vote saw empty)",
                 sq);
        return;
      }
    }
  }
}

/**
 * @brief Checks if board is physically in starting position (occupancy only)
 *
 * Uses RAW SENSOR DATA from Matrix Task. Rows 0–1 a 6–7 plné, 2–5 prázdné.
 * Více vzorků + většinové hlasování kvůli šumu u reedů při dokončení tutorialu.
 */
static bool game_is_board_in_starting_position(void) {
  uint8_t votes[64] = {0};

  for (int s = 0; s < GAME_START_POS_MATRIX_SAMPLES; s++) {
    uint8_t snap[64];
    matrix_get_state(snap);
    for (int i = 0; i < 64; i++) {
      votes[i] += snap[i];
    }
    if (s + 1 < GAME_START_POS_MATRIX_SAMPLES) {
      vTaskDelay(pdMS_TO_TICKS(12));
    }
  }

  uint8_t stable[64];
  for (int i = 0; i < 64; i++) {
    stable[i] = (votes[i] >= GAME_START_POS_MAJORITY_VOTES) ? 1 : 0;
  }

  if (game_board_starting_occupancy_matches(stable)) {
    return true;
  }
  game_log_first_startpos_mismatch(stable);
  return false;
}

/**
 * @brief Zobrazí červené LED na polích, kde chybí figurky v počáteční pozici
 * 
 * Používá se ve stavu GAME_STATE_WAITING_FOR_BOARD_SETUP pro indikaci
 * uživateli, která pole musí obsadit figurkami.
 */
static void game_show_missing_pieces_led(void) {
  uint8_t matrix_state[64] = {0};
  matrix_get_state(matrix_state);
  
  // Vyčistit desku
  led_clear_board_only();
  
  // Definice očekávané počáteční pozice (row-major order)
  // Rows 0-1 a 6-7 by měly být obsazené, rows 2-5 prázdné
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] == 0) {
        // Chybí figurka na bílém základním řádku
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] == 0) {
        // Chybí figurka na černém základním řádku
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] != 0) {
        // Náhodná figurka uprostřed desky - také červeně
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  
  ESP_LOGI(TAG, "🔴 LED: Showing missing pieces in red");
}


void game_bump_revision_and_notify(void) {
  game_task_wdt_reset_safe();
  game_state_revision++;
  if (game_state_revision == 0U) {
    game_state_revision = 1U;
  }
  game_task_wdt_reset_safe();
  czechmate_on_game_state_changed();
  game_task_wdt_reset_safe();
}

uint32_t game_get_state_revision(void) { return game_state_revision; }

/**
 * @brief Resetuje hru do vychoziho stavu
 *
 * Tato funkce resetuje hru do vychoziho stavu. Vymaze vsechny tahy,
 * resetuje stavy hry a inicializuje novou hru.
 *
 * @details
 * Funkce resetuje:
 * - Stav hry na IDLE
 * - Aktualni hrace na WHITE
 * - Pocet tahu na 0
 * - Vsechny castling flagy
 * - En passant stav
 * - Historie tahu
 * - Error recovery stav
 */
void game_reset_game(void) {
  ESP_LOGI(TAG, "Resetting game...");

  // Zastavit resignation timer pokud běží (tichý cleanup, hra se resetuje)
  if (resignation_state.active) {
    resignation_stop(true);
  }

  // Reset game state
  current_game_state = GAME_STATE_IDLE;
  current_player = PLAYER_WHITE;
  game_start_time = 0;
  last_move_time = 0;
  move_count = 0;
  game_active = false;

  // Reset extended statistics
  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  current_result_type = RESULT_WHITE_WINS;           // Reset pro novou hru
  current_endgame_reason = ENDGAME_REASON_CHECKMATE; // Reset endgame reason.
  last_move_type = LAST_MOVE_NORMAL;                 // Reset last move type.
  game_saved = false;
  saved_game_name[0] = '\0';

  // Clear move history
  memset(move_history, 0, sizeof(move_history));
  memset(move_history_kind, 0, sizeof(move_history_kind));
  history_index = 0;
  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;

  // Clear captured pieces
  white_captured_count = 0;
  black_captured_count = 0;
  white_captured_index = 0;
  black_captured_index = 0;

  // Clear material advantage history
  advantage_history_count = 0;
  memset(material_advantage_history, 0, sizeof(material_advantage_history));

  // Clear last move tracking
  has_last_move = false;

  // Kompletní reset error recovery state
  error_recovery_state.has_invalid_piece = false;
  error_recovery_state.invalid_row = 0;
  error_recovery_state.invalid_col = 0;
  error_recovery_state.original_valid_row = 0;
  error_recovery_state.original_valid_col = 0;
  error_recovery_state.piece_type = PIECE_EMPTY;
  error_recovery_state.waiting_for_move_correction = false;
  error_recovery_state.error_count = 0;

  // Reset opponent piece recovery state
  opponent_piece_moved = false;
  opponent_piece_type = PIECE_EMPTY;
  opponent_original_row = 0;
  opponent_original_col = 0;
  opponent_current_row = 0;
  opponent_current_col = 0;

  // Reset capture state
  capture_in_progress = false;
  capture_target_row = 0;
  capture_target_col = 0;
  capture_removed_piece = PIECE_EMPTY;

  // Reset guided capture state
  guided_capture_state.active = false;
  guided_capture_state.target_row = 0;
  guided_capture_state.target_col = 0;
  guided_capture_state.target_piece = PIECE_EMPTY;
  guided_capture_state.attacker_count = 0;
  memset(guided_capture_state.attacker_rows, 0,
         sizeof(guided_capture_state.attacker_rows));
  memset(guided_capture_state.attacker_cols, 0,
         sizeof(guided_capture_state.attacker_cols));
  memset(guided_capture_state.attacker_pieces, 0,
         sizeof(guided_capture_state.attacker_pieces));

  // Reset matrix guard pause state (full clear after board init below).
  resync_required_after_restore = false;

  // Reset promotion state to prevent blocking moves in a new game.
  promotion_state.pending = false;
  promotion_state.square_row = 0;
  promotion_state.square_col = 0;
  promotion_state.player = PLAYER_WHITE;

  // Reset lifted piece state.
  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  /* Každý plný reset hry končí tutoriál rozestavení; vstup do tutoriálu ho hned
   * znovu zapne (game_enter_board_setup_tutorial). Dřív zůstával true po „Nová
   * hra“ z webu → ignorovaný matrix guard a zamrzlá detekce z matice. */
  board_setup_tutorial_active = false;

  // Reset auto-new game detection flags.
  // This allows auto-new game to work after manual resets or when pieces are
  // manually arranged into starting position
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false; // Reset block flag too
  ESP_LOGI(TAG, "✅ Auto new game detection flags reset in game_reset_game()");

  // Zastavit endgame animaci při resetu hry
  led_stop_endgame_animation();

  // Reinitialize board
  game_initialize_board();

  game_matrix_guard_clear_both_layers();

  ESP_LOGI(TAG, "Game reset completed");
  game_bump_revision_and_notify();
}

/**
 * Po game_reset_game(): vyprázdní logiku a IDLE — společné pro vstup/výstup z
 * tutoriálu rozestavení.
 */
void game_apply_empty_logical_board_after_full_reset(void) {
  memset(board, 0, sizeof(board));
  memset(piece_moved, 0, sizeof(piece_moved));
  current_game_state = GAME_STATE_IDLE;
  game_active = false;
  move_count = 0;
  white_king_moved = true;
  white_rook_a_moved = true;
  white_rook_h_moved = true;
  black_king_moved = true;
  black_rook_a_moved = true;
  black_rook_h_moved = true;
  memset(&castling_state, 0, sizeof(castling_state));
  en_passant_available = false;
  game_check_promotion_needed();
}

void game_enter_board_setup_tutorial(void) {
  game_puzzle_cancel();
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  board_setup_tutorial_active = true;
  STAGING_LOGI(TAG,
               "board_setup_tutorial: entered (empty logical, guard bypass)");
}

void game_exit_board_setup_tutorial(bool apply_full_start_position) {
  board_setup_tutorial_active = false;
  if (apply_full_start_position) {
    game_start_new_game();
    STAGING_LOGI(TAG, "board_setup_tutorial: exited with full start position");
    return;
  }
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  STAGING_LOGI(TAG, "board_setup_tutorial: cancelled (empty logical board)");
}

bool game_is_board_setup_tutorial_active(void) {
  return board_setup_tutorial_active;
}

bool game_is_physical_board_starting_occupancy(void) {
  return game_is_board_in_starting_position();
}

bool game_finish_board_setup_tutorial_from_web(void) {
  bool physical_ok = false;
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
      return false;
    }
  }
  if (!board_setup_tutorial_active) {
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return false;
  }
  physical_ok = game_is_board_in_starting_position();
  if (!physical_ok) {
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return false;
  }
  board_setup_tutorial_active = false;
  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }
  game_start_new_game();
  STAGING_LOGI(TAG,
               "board_setup_tutorial: finish OK — physical start, new game");
  return true;
}



/**
 * @brief Spusti novou hru
 *
 * Tato funkce spusti novou sachovou hru. Inicializuje sachovnici,
 * resetuje vsechny stavy a pripravi hru pro hrani.
 *
 * @details
 * Funkce:
 * - Inicializuje sachovnici do vychoziho stavu
 * - Resetuje vsechny stavy hry
 * - Nastavi prvni hrace na WHITE
 * - Zastavi vsechny animace
 * - Aktualizuje LED feedback
 * - Zvysi pocet her
 */
void game_start_new_game(void) {
  ESP_LOGI(TAG, "Starting new game...");

  game_task_wdt_reset_safe();
  game_snapshot_erase_nvs();
  game_task_wdt_reset_safe();

  game_snapshot_reset_failure_flags();

  // Reset game
  game_reset_game();
  game_task_wdt_reset_safe();

  // Reset timer před novou hrou
  game_reset_timer();
  ESP_LOGI(TAG, "Timer reset for new game");

  // BUG FIX 1: Kontrola fyzické desky před aktivací hry (pouze pokud je hlídání zapnuto)
  if (starting_position_check_enabled && !game_is_board_in_starting_position()) {
    // Deska není fyzicky připravena - přejít do stavu čekání
    current_game_state = GAME_STATE_WAITING_FOR_BOARD_SETUP;
    game_active = false;
    
    // LED indikace: červené blikání chybějících polí
    game_show_missing_pieces_led();
    
    // Notify web
    game_bump_revision_and_notify();
    
    ESP_LOGW(TAG, "Board not in starting position - waiting for physical setup");
    return;
  }

  // Set game state - pouze když je deska OK
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  game_start_time = esp_timer_get_time() / 1000;
  last_move_time = game_start_time;

  // Start timer for white player if time control is active
  if (game_is_timer_active()) {
    ESP_LOGI(TAG, "Starting timer for white player at game start");
    game_start_timer_move(true); // Start timer for white
  }

  // Initialize extended statistics
  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  game_saved = false;
  saved_game_name[0] = '\0';

  total_games++;

  ESP_LOGI(TAG, "New game started - White to move");
  ESP_LOGI(TAG, "Total games: %" PRIu32, total_games);

  // Boot sequence: first game vs. restart.
  if (total_games > 1) {
    // Restart hry - zastavit všechny animace
    unified_animation_stop_all();
    led_stop_endgame_animation(); // Legacy endgame animations
    stop_endgame_animation();     // Stop NEW endgame animations system
    ESP_LOGI(TAG, "✅ All animations stopped for new game (restart)");
  } else {
    // První hra - boot sequence stále probíhá
    ESP_LOGI(TAG, "⏸️  Boot sequence: Skipping animation stop (first game)");
  }

  // Wait for boot animation to finish if running (critical for button LEDs)
  while (led_is_booting()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
  }

  // Update promotion LED indications
  game_check_promotion_needed();
  ESP_LOGI(TAG, "✅ Updated promotion button LED indications");

  // Reset auto-new game flags for all games (not just the first).
  // This allows auto-new game detection to work after manual resets
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false; // Reset block flag too
  ESP_LOGI(TAG, "✅ Auto new game detection flags reset for new game");

  // Boot sequence: first game vs. restart.
  if (total_games >= 1) { // Povolit highlight i pro první hru!
    // Restart hry - zvýraznit pohyblivé figurky
    vTaskDelay(pdMS_TO_TICKS(100)); // Krátká pauza pro stabilizaci
    game_task_wdt_reset_safe();
    game_highlight_movable_pieces();
    game_task_wdt_reset_safe();
    ESP_LOGI(TAG,
             "✅ Highlighted movable pieces for starting player (restart)");
  } else {
    // První hra - boot sequence stále probíhá, skipnout highlight
    ESP_LOGI(
        TAG,
        "⏸️  Boot sequence: Skipping movable pieces highlight (first game)");
  }
  game_bump_revision_and_notify();
}

void game_start_new_game_from_fen(const char *fen) {
  if (fen == NULL || fen[0] == '\0') {
    game_start_new_game();
    return;
  }

  ESP_LOGI(TAG, "Starting new game from FEN...");
  game_task_wdt_reset_safe();
  game_snapshot_erase_nvs();
  game_task_wdt_reset_safe();

  game_snapshot_reset_failure_flags();

  game_reset_game();
  game_task_wdt_reset_safe();
  game_reset_timer();

  player_t fen_player = PLAYER_WHITE;
  if (!game_load_position_from_fen(fen, &fen_player)) {
    ESP_LOGE(TAG, "FEN parse failed, falling back to standard new game");
    game_start_new_game();
    return;
  }

  white_king_moved = true;
  black_king_moved = true;
  white_rook_a_moved = true;
  white_rook_h_moved = true;
  black_rook_a_moved = true;
  black_rook_h_moved = true;
  en_passant_available = false;
  en_passant_target_row = 0;
  en_passant_target_col = 0;
  en_passant_victim_row = 0;
  en_passant_victim_col = 0;

  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  board_setup_tutorial_active = false;

  current_player = fen_player;
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  game_start_time = esp_timer_get_time() / 1000;
  last_move_time = game_start_time;

  if (game_is_timer_active()) {
    game_start_timer_move(fen_player == PLAYER_WHITE);
  }

  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  game_saved = false;
  saved_game_name[0] = '\0';
  total_games++;

  if (total_games > 1) {
    unified_animation_stop_all();
    led_stop_endgame_animation();
    stop_endgame_animation();
  }

  while (led_is_booting()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
  }

  game_check_promotion_needed();
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false;

  if (total_games >= 1) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
    game_highlight_movable_pieces();
    game_task_wdt_reset_safe();
  }
  game_bump_revision_and_notify();
}

