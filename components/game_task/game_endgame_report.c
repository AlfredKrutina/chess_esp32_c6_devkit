/**
 * @file game_endgame_report.c
 * @brief Endgame statistics update, UART report formatting, win/draw getters.
 */

#include "game_task_internal.h"
#include "game_task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_ENDGAME";

// ============================================================================
// GAME STATISTICS DISPLAY
// ============================================================================

/**
 * @brief Update endgame statistics (centralized function)
 * @param result_type Type of game result
 */
void game_update_endgame_statistics(game_result_type_t result_type) {
  switch (result_type) {
  case RESULT_WHITE_WINS:
    white_wins++;
    ESP_LOGI(TAG, "📊 Statistics: White wins! (Total: %" PRIu32 ")",
             white_wins);
    break;
  case RESULT_BLACK_WINS:
    black_wins++;
    ESP_LOGI(TAG, "📊 Statistics: Black wins! (Total: %" PRIu32 ")",
             black_wins);
    break;
  case RESULT_DRAW_STALEMATE:
  case RESULT_DRAW_50_MOVE:
  case RESULT_DRAW_REPETITION:
  case RESULT_DRAW_INSUFFICIENT:
    draws++;
    ESP_LOGI(TAG, "📊 Statistics: Draw! (Total: %" PRIu32 ")", draws);
    break;
  }
  total_games++;
  ESP_LOGI(TAG, "📊 Total games: %" PRIu32, total_games);
}

/**
 * @brief Print simplified endgame report to UART
 * @param result_type Type of game result
 */

// ============================================================================
// ENDGAME REPORT FORMATTING HELPERS
// ============================================================================

/**
 * @brief Print formatted report header with box borders
 *
 * @param title Header title to display
 *
 * @details
 * Tiskne hlavicku reportu s dvojitym ohranicenim (═══).
 * Pouziva se na zacatku reportu nebo sekci.
 */
static void print_report_header(const char *title) {
  printf("\r\n");
  printf("═══════════════════════════════════════════════════════════════\r\n");
  printf("%s\r\n", title);
  printf("═══════════════════════════════════════════════════════════════\r\n");
}

/**
 * @brief Print separator line
 *
 * @details
 * Tiskne oddelovaci caru (───) mezi sekce reportu.
 */
static void print_separator(void) {
  printf("───────────────────────────────────────────────────────────────\r\n");
}

/**
 * @brief Print single report line with label and formatted value
 *
 * @param label descriptive label (e.g., "Winner", "Total Moves")
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 *
 * @details
 * Tiskne jednotliv radek reportu ve formatu "  • Label: Value".
 * Podporuje printf-style formatovani hodnot.
 *
 * Priklad:
 *   print_report_line("Winner", "%s", winner);
 *   print_report_line("Total Moves", "%u", move_count);
 */
static void print_report_line(const char *label, const char *fmt, ...) {
  printf("  • %s: ", label);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  printf("\r\n");
}

void game_print_endgame_report_uart(game_result_type_t result_type) {
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t minutes = game_duration / 60;
  uint32_t seconds = game_duration % 60;

  // Print endgame announcement
  print_report_header("🏆 ENDGAME REPORT");

  // Určit výsledek a typ ukončení podle current_endgame_reason
  const char *winner = "";
  const char *loser = "";
  const char *end_reason = "";

  // Nejprve určíme vítěze z result_type
  if (result_type == RESULT_WHITE_WINS) {
    winner = "White";
    loser = "Black";
  } else if (result_type == RESULT_BLACK_WINS) {
    winner = "Black";
    loser = "White";
  } else {
    winner = "Draw";
    loser = "Draw";
  }

  // Pak určíme důvod konce hry a vytiskneme výsledek
  switch (current_endgame_reason) {
  // === CHECKMATE VARIATIONS ===
  case ENDGAME_REASON_CHECKMATE:
    printf("🎯 Game Result: %s WINS BY CHECKMATE\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate";
    break;
  case ENDGAME_REASON_CHECKMATE_EN_PASSANT:
    printf("⚔️ Game Result: %s WINS BY EN PASSANT CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (En Passant)";
    break;
  case ENDGAME_REASON_CHECKMATE_CASTLING:
    printf("🏰 Game Result: %s WINS BY CASTLING CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Castling)";
    break;
  case ENDGAME_REASON_CHECKMATE_PROMOTION:
    printf("👑 Game Result: %s WINS BY PROMOTION CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Promotion)";
    break;
  case ENDGAME_REASON_CHECKMATE_DISCOVERED:
    printf("💥 Game Result: %s WINS BY DISCOVERED CHECK CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Discovered Check)";
    break;

  // === PLAYER ACTIONS ===
  case ENDGAME_REASON_RESIGNATION:
    printf("🏳️ Game Result: %s WINS BY RESIGNATION\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Resignation";
    break;
  case ENDGAME_REASON_TIMEOUT:
    printf("⏰ Game Result: %s WINS BY TIMEOUT\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Timeout";
    break;

  // === DRAW CONDITIONS ===
  case ENDGAME_REASON_STALEMATE:
    printf("🤝 Game Result: DRAW - Stalemate\r\n");
    end_reason = "Stalemate";
    break;
  case ENDGAME_REASON_50_MOVE:
    printf("🤝 Game Result: DRAW - 50-move rule\r\n");
    end_reason = "50-move rule";
    break;
  case ENDGAME_REASON_REPETITION:
    printf("🤝 Game Result: DRAW - Three-fold repetition\r\n");
    end_reason = "Threefold repetition";
    break;
  case ENDGAME_REASON_INSUFFICIENT:
    printf("🤝 Game Result: DRAW - Insufficient material\r\n");
    end_reason = "Insufficient material";
    break;
  }

  print_separator();
  printf("📋 Game Summary:\r\n");
  print_report_line("Winner", "%s", winner);
  if (result_type != RESULT_DRAW_STALEMATE &&
      result_type != RESULT_DRAW_50_MOVE &&
      result_type != RESULT_DRAW_REPETITION &&
      result_type != RESULT_DRAW_INSUFFICIENT) {
    print_report_line("Loser", "%s", loser);
  }
  print_report_line("End Reason", "%s", end_reason);
  print_report_line("Total Moves", "%" PRIu32, move_count);
  print_report_line("Game Duration", "%" PRIu32 ":%02" PRIu32 " (mm:ss)",
                    minutes, seconds);

  // Vypočítat material advantage
  int white_material = 0, black_material = 0;
  for (uint32_t i = 0; i < white_captured_count; i++) {
    piece_t p = white_captured_pieces[i];
    if (p == PIECE_BLACK_PAWN)
      white_material += 1;
    else if (p == PIECE_BLACK_KNIGHT || p == PIECE_BLACK_BISHOP)
      white_material += 3;
    else if (p == PIECE_BLACK_ROOK)
      white_material += 5;
    else if (p == PIECE_BLACK_QUEEN)
      white_material += 9;
  }
  for (uint32_t i = 0; i < black_captured_count; i++) {
    piece_t p = black_captured_pieces[i];
    if (p == PIECE_WHITE_PAWN)
      black_material += 1;
    else if (p == PIECE_WHITE_KNIGHT || p == PIECE_WHITE_BISHOP)
      black_material += 3;
    else if (p == PIECE_WHITE_ROOK)
      black_material += 5;
    else if (p == PIECE_WHITE_QUEEN)
      black_material += 9;
  }
  int material_diff = white_material - black_material;

  print_separator();
  printf("📊 Game Statistics:\r\n");
  print_report_line("White Moves", "%" PRIu32, (move_count + 1) / 2);
  print_report_line("Black Moves", "%" PRIu32, move_count / 2);
  if (game_duration > 0 && move_count > 0) {
    print_report_line("Avg Time per Move", "%" PRIu32 "s",
                      game_duration / move_count);
  }

  print_separator();
  printf("⚔️  Combat Statistics:\r\n");
  print_report_line("White Captures", "%" PRIu32 " pieces (%d points)",
                    white_captures, white_material);
  print_report_line("Black Captures", "%" PRIu32 " pieces (%d points)",
                    black_captures, black_material);
  if (material_diff > 0) {
    print_report_line("Material Advantage", "White +%d", material_diff);
  } else if (material_diff < 0) {
    print_report_line("Material Advantage", "Black +%d", -material_diff);
  } else {
    print_report_line("Material Advantage", "Equal (0)");
  }
  print_report_line("White Checks", "%" PRIu32, white_checks);
  print_report_line("Black Checks", "%" PRIu32, black_checks);

  // Captured pieces vizuálně
  if (white_captured_count > 0) {
    printf("  • White Captured: ");
    for (uint32_t i = 0; i < white_captured_count && i < GAME_TASK_MAX_CAPTURED_PIECES;
         i++) {
      char piece_char = piece_to_char(white_captured_pieces[i]);
      printf("%c ", piece_char);
    }
    printf("\r\n");
  }
  if (black_captured_count > 0) {
    printf("  • Black Captured: ");
    for (uint32_t i = 0; i < black_captured_count && i < GAME_TASK_MAX_CAPTURED_PIECES;
         i++) {
      char piece_char = piece_to_char(black_captured_pieces[i]);
      printf("%c ", piece_char);
    }
    printf("\r\n");
  }

  // Castling info
  print_report_line("White Castling", "%s", white_castles > 0 ? "Yes" : "No");
  print_report_line("Black Castling", "%s", black_castles > 0 ? "Yes" : "No");

  print_separator();
  printf("📈 Overall Statistics:\r\n");
  print_report_line("Total Games Played", "%" PRIu32, total_games);
  print_report_line("White Wins", "%" PRIu32 " (%.0f%%)", white_wins,
                    total_games > 0 ? (white_wins * 100.0f / total_games)
                                    : 0.0f);
  print_report_line("Black Wins", "%" PRIu32 " (%.0f%%)", black_wins,
                    total_games > 0 ? (black_wins * 100.0f / total_games)
                                    : 0.0f);
  print_report_line("Draws", "%" PRIu32 " (%.0f%%)", draws,
                    total_games > 0 ? (draws * 100.0f / total_games) : 0.0f);

  printf("═══════════════════════════════════════════════════════════════\r\n");
  printf("\r\n");

  // Flush stdout pro okamžité zobrazení
  fflush(stdout);
}

/**
 * @brief Get white wins count
 */
uint32_t game_get_white_wins(void) { return white_wins; }

/**
 * @brief Get black wins count
 */
uint32_t game_get_black_wins(void) { return black_wins; }

/**
 * @brief Get draws count
 */
uint32_t game_get_draws(void) { return draws; }

/**
 * @brief Get total games count
 */
uint32_t game_get_total_games(void) { return total_games; }

/**
 * @brief Get game state as string
 */
const char *game_get_game_state_string(void) {
  switch (current_game_state) {
  case GAME_STATE_ACTIVE:
    return "Active";
  case GAME_STATE_PAUSED:
    return "Paused";
  case GAME_STATE_FINISHED:
    return "Finished";
  case GAME_STATE_PROMOTION:
    return "Promotion";
  case GAME_STATE_IDLE:
    return "Idle";
  case GAME_STATE_WAITING_FOR_BOARD_SETUP:
    return "WaitingForBoardSetup";
  default:
    return "Unknown";
  }
}
