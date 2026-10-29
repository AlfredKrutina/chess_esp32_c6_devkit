/**
 * @file game_endgame_report.c
 * @brief Endgame statistics update, UART report formatting, win/draw getters.
 */

#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "../ha_light_task/include/ha_light_task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_ENDGAME";
/* Win/draw counters, captured material, and advantage history. */
static uint32_t s_total_games = 0;

uint32_t *game_state_total_games(void) { return &s_total_games; }
#define total_games (*game_state_total_games())
static uint32_t s_white_wins = 0;

uint32_t *game_state_white_wins(void) { return &s_white_wins; }
#define white_wins (*game_state_white_wins())
static uint32_t s_black_wins = 0;

uint32_t *game_state_black_wins(void) { return &s_black_wins; }
#define black_wins (*game_state_black_wins())
static uint32_t s_draws = 0;

uint32_t *game_state_draws(void) { return &s_draws; }
#define draws (*game_state_draws())
static bool s_endgame_report_requested = false;

bool *game_state_endgame_report_requested(void) { return &s_endgame_report_requested; }
#define endgame_report_requested (*game_state_endgame_report_requested())
static endgame_reason_t s_current_endgame_reason = ENDGAME_REASON_CHECKMATE;

endgame_reason_t *game_state_current_endgame_reason(void) { return &s_current_endgame_reason; }
#define current_endgame_reason (*game_state_current_endgame_reason())
static last_move_type_t s_last_move_type = LAST_MOVE_NORMAL;

last_move_type_t *game_state_last_move_type(void) { return &s_last_move_type; }
#define last_move_type (*game_state_last_move_type())
static uint32_t s_game_start_time = 0;

uint32_t *game_state_game_start_time(void) { return &s_game_start_time; }
#define game_start_time (*game_state_game_start_time())
static uint32_t s_last_move_time = 0;

uint32_t *game_state_last_move_time(void) { return &s_last_move_time; }
#define last_move_time (*game_state_last_move_time())
static uint32_t s_white_time_total = 0;

uint32_t *game_state_white_time_total(void) { return &s_white_time_total; }
#define white_time_total (*game_state_white_time_total())
static uint32_t s_black_time_total = 0;

uint32_t *game_state_black_time_total(void) { return &s_black_time_total; }
#define black_time_total (*game_state_black_time_total())
static uint32_t s_white_moves_count = 0;

uint32_t *game_state_white_moves_count(void) { return &s_white_moves_count; }
#define white_moves_count (*game_state_white_moves_count())
static uint32_t s_black_moves_count = 0;

uint32_t *game_state_black_moves_count(void) { return &s_black_moves_count; }
#define black_moves_count (*game_state_black_moves_count())
static uint32_t s_white_captures = 0;

uint32_t *game_state_white_captures(void) { return &s_white_captures; }
#define white_captures (*game_state_white_captures())
static uint32_t s_black_captures = 0;

uint32_t *game_state_black_captures(void) { return &s_black_captures; }
#define black_captures (*game_state_black_captures())
static uint32_t s_white_checks = 0;

uint32_t *game_state_white_checks(void) { return &s_white_checks; }
#define white_checks (*game_state_white_checks())
static uint32_t s_black_checks = 0;

uint32_t *game_state_black_checks(void) { return &s_black_checks; }
#define black_checks (*game_state_black_checks())
static uint32_t s_white_castles = 0;

uint32_t *game_state_white_castles(void) { return &s_white_castles; }
#define white_castles (*game_state_white_castles())
static uint32_t s_black_castles = 0;

uint32_t *game_state_black_castles(void) { return &s_black_castles; }
#define black_castles (*game_state_black_castles())
static piece_t s_white_captured_pieces[GAME_TASK_MAX_CAPTURED_PIECES];

piece_t *game_state_white_captured_pieces(void) { return s_white_captured_pieces; }
#define white_captured_pieces (game_state_white_captured_pieces())
static piece_t s_black_captured_pieces[GAME_TASK_MAX_CAPTURED_PIECES];

piece_t *game_state_black_captured_pieces(void) { return s_black_captured_pieces; }
#define black_captured_pieces (game_state_black_captured_pieces())
static uint32_t s_white_captured_count = 0;

uint32_t *game_state_white_captured_count(void) { return &s_white_captured_count; }
#define white_captured_count (*game_state_white_captured_count())
static uint32_t s_black_captured_count = 0;

uint32_t *game_state_black_captured_count(void) { return &s_black_captured_count; }
#define black_captured_count (*game_state_black_captured_count())
static uint32_t s_white_captured_index = 0;

uint32_t *game_state_white_captured_index(void) { return &s_white_captured_index; }
#define white_captured_index (*game_state_white_captured_index())
static uint32_t s_black_captured_index = 0;

uint32_t *game_state_black_captured_index(void) { return &s_black_captured_index; }
#define black_captured_index (*game_state_black_captured_index())
static int8_t s_material_advantage_history[GAME_TASK_MAX_ADVANTAGE_HISTORY];

int8_t *game_state_material_advantage_history(void) { return s_material_advantage_history; }
#define material_advantage_history (game_state_material_advantage_history())
static uint32_t s_advantage_history_count = 0;

uint32_t *game_state_advantage_history_count(void) { return &s_advantage_history_count; }
#define advantage_history_count (*game_state_advantage_history_count())
static game_state_t s_game_result = GAME_STATE_IDLE;

game_state_t *game_state_game_result(void) { return &s_game_result; }
#define game_result (*game_state_game_result())
static game_result_type_t s_current_result_type =
    RESULT_WHITE_WINS; // Current end game type

game_result_type_t *game_state_current_result_type(void) { return &s_current_result_type; }
#define current_result_type (*game_state_current_result_type())


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
  case RESULT_DRAW_AGREEMENT:
    draws++;
    ESP_LOGI(TAG, "📊 Statistics: Draw! (Total: %" PRIu32 ")", draws);
    break;
  }
  total_games++;
  ESP_LOGI(TAG, "📊 Total games: %" PRIu32, total_games);

  uint8_t fact = HA_GAME_DRAW;
  uint8_t reason = HA_REASON_NONE;
  if (result_type == RESULT_WHITE_WINS) {
    fact = HA_GAME_WHITE_WIN;
  } else if (result_type == RESULT_BLACK_WINS) {
    fact = HA_GAME_BLACK_WIN;
  }
  switch (current_endgame_reason) {
  case ENDGAME_REASON_CHECKMATE:
  case ENDGAME_REASON_CHECKMATE_EN_PASSANT:
  case ENDGAME_REASON_CHECKMATE_CASTLING:
  case ENDGAME_REASON_CHECKMATE_PROMOTION:
  case ENDGAME_REASON_CHECKMATE_DISCOVERED:
    reason = HA_REASON_CHECKMATE;
    break;
  case ENDGAME_REASON_STALEMATE:
    reason = HA_REASON_STALEMATE;
    break;
  case ENDGAME_REASON_50_MOVE:
    reason = HA_REASON_FIFTY;
    break;
  case ENDGAME_REASON_REPETITION:
    reason = HA_REASON_REPETITION;
    break;
  case ENDGAME_REASON_INSUFFICIENT:
    reason = HA_REASON_INSUFFICIENT;
    break;
  case ENDGAME_REASON_TIMEOUT:
    reason = HA_REASON_TIMEOUT;
    break;
  case ENDGAME_REASON_RESIGNATION:
    reason = HA_REASON_RESIGNATION;
    break;
  case ENDGAME_REASON_AGREEMENT:
    reason = HA_REASON_AGREEMENT;
    break;
  default:
    break;
  }
  if (reason == HA_REASON_NONE) {
    if (result_type == RESULT_DRAW_STALEMATE) {
      reason = HA_REASON_STALEMATE;
    } else if (result_type == RESULT_DRAW_50_MOVE) {
      reason = HA_REASON_FIFTY;
    } else if (result_type == RESULT_DRAW_REPETITION) {
      reason = HA_REASON_REPETITION;
    } else if (result_type == RESULT_DRAW_INSUFFICIENT) {
      reason = HA_REASON_INSUFFICIENT;
    } else if (result_type == RESULT_DRAW_AGREEMENT) {
      reason = HA_REASON_AGREEMENT;
    } else {
      reason = HA_REASON_CHECKMATE;
    }
  }
  ha_game_report(fact, reason);
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
 * Used at the start of a report or section.
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
 * Prints a single line of the report in the format " • Label: Value".
 * Supports printf-style formatting of values.
 *
 * Example:
 * print_report_line("Winner", "%s", winner);
 * print_report_line("Total Moves", "%u", move_count);
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

  // Determine outcome and end type by current_endgame_reason
  const char *winner = "";
  const char *loser = "";
  const char *end_reason = "";

  // First we determine the winner from the result_type
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

  // Then we determine the reason for the end of the game and print the result
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
  case ENDGAME_REASON_AGREEMENT:
    printf("🤝 Game Result: DRAW - Agreement\r\n");
    end_reason = "Agreement";
    break;
  }

  print_separator();
  printf("📋 Game Summary:\r\n");
  print_report_line("Winner", "%s", winner);
  if (result_type != RESULT_DRAW_STALEMATE &&
      result_type != RESULT_DRAW_50_MOVE &&
      result_type != RESULT_DRAW_REPETITION &&
      result_type != RESULT_DRAW_INSUFFICIENT &&
      result_type != RESULT_DRAW_AGREEMENT) {
    print_report_line("Loser", "%s", loser);
  }
  print_report_line("End Reason", "%s", end_reason);
  print_report_line("Total Moves", "%" PRIu32, move_count);
  print_report_line("Game Duration", "%" PRIu32 ":%02" PRIu32 " (mm:ss)",
                    minutes, seconds);

  // Calculate material advantage
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

  // Captured pieces visually
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

  // Flush stdout for immediate display
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

char piece_to_char(piece_t piece) {
  switch (piece) {
  case PIECE_EMPTY:
    return ' ';
  case PIECE_WHITE_PAWN:
    return 'P';
  case PIECE_WHITE_KNIGHT:
    return 'N';
  case PIECE_WHITE_BISHOP:
    return 'B';
  case PIECE_WHITE_ROOK:
    return 'R';
  case PIECE_WHITE_QUEEN:
    return 'Q';
  case PIECE_WHITE_KING:
    return 'K';
  case PIECE_BLACK_PAWN:
    return 'p';
  case PIECE_BLACK_KNIGHT:
    return 'n';
  case PIECE_BLACK_BISHOP:
    return 'b';
  case PIECE_BLACK_ROOK:
    return 'r';
  case PIECE_BLACK_QUEEN:
    return 'q';
  case PIECE_BLACK_KING:
    return 'k';
  default:
    return ' ';
  }
}

static int8_t game_calculate_material_advantage(void) {
  int white_material = 0;
  int black_material = 0;

  // Figure values
  const int pawn_value = 1;
  const int knight_value = 3;
  const int bishop_value = 3;
  const int rook_value = 5;
  const int queen_value = 9;

  // Spocitat material na boardu
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = game_get_piece(row, col);
      switch (piece) {
      case PIECE_WHITE_PAWN:
        white_material += pawn_value;
        break;
      case PIECE_WHITE_KNIGHT:
        white_material += knight_value;
        break;
      case PIECE_WHITE_BISHOP:
        white_material += bishop_value;
        break;
      case PIECE_WHITE_ROOK:
        white_material += rook_value;
        break;
      case PIECE_WHITE_QUEEN:
        white_material += queen_value;
        break;
      case PIECE_BLACK_PAWN:
        black_material += pawn_value;
        break;
      case PIECE_BLACK_KNIGHT:
        black_material += knight_value;
        break;
      case PIECE_BLACK_BISHOP:
        black_material += bishop_value;
        break;
      case PIECE_BLACK_ROOK:
        black_material += rook_value;
        break;
      case PIECE_BLACK_QUEEN:
        black_material += queen_value;
        break;
      default:
        break;
      }
    }
  }

  int advantage = white_material - black_material;
  // Omezit na int8_t rozsah
  if (advantage > 127)
    advantage = 127;
  if (advantage < -127)
    advantage = -127;

  return (int8_t)advantage;
}

void game_record_material_advantage(void) {
  if (advantage_history_count >= GAME_TASK_MAX_ADVANTAGE_HISTORY) {
    return; // Historie plna
  }

  int8_t advantage = game_calculate_material_advantage();
  material_advantage_history[advantage_history_count++] = advantage;
}

void game_add_captured_piece(piece_t piece) {
  // Determine which array to use based on piece color
  if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
    // White piece captured by black
    if (black_captured_index < GAME_TASK_MAX_CAPTURED_PIECES) {
      black_captured_pieces[black_captured_index++] = piece;
      black_captured_count++;
      black_captures++;
    }
  } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
    // Black piece captured by white
    if (white_captured_index < GAME_TASK_MAX_CAPTURED_PIECES) {
      white_captured_pieces[white_captured_index++] = piece;
      white_captured_count++;
      white_captures++;
    }
  }
}

static const int piece_values[] = {
    0, // PIECE_EMPTY
    1, // PIECE_WHITE_PAWN
    3, // PIECE_WHITE_KNIGHT
    3, // PIECE_WHITE_BISHOP
    5, // PIECE_WHITE_ROOK
    9, // PIECE_WHITE_QUEEN
    0, // PIECE_WHITE_KING (infinite value)
    1, // PIECE_BLACK_PAWN
    3, // PIECE_BLACK_KNIGHT
    3, // PIECE_BLACK_BISHOP
    5, // PIECE_BLACK_ROOK
    9, // PIECE_BLACK_QUEEN
    0  // PIECE_BLACK_KING (infinite value)
};

int game_calculate_material_balance(int *white_material, int *black_material) {
  int white_total = 0;
  int black_total = 0;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = game_get_piece(row, col);
      if (piece != PIECE_EMPTY) {
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_QUEEN) {
          white_total += piece_values[piece];
        } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_QUEEN) {
          black_total +=
              piece_values[piece - 6]; // Adjust index for black pieces
        }
      }
    }
  }

  if (white_material)
    *white_material = white_total;
  if (black_material)
    *black_material = black_total;

  return white_total - black_total;
}

void game_get_material_string(char *buffer, size_t buffer_size) {
  int white_material, black_material;
  int balance =
      game_calculate_material_balance(&white_material, &black_material);

  if (balance > 0) {
    snprintf(buffer, buffer_size, "White +%d", balance);
  } else if (balance < 0) {
    snprintf(buffer, buffer_size, "Black +%d", -balance);
  } else {
    snprintf(buffer, buffer_size, "Even (+0)");
  }
}

void game_print_game_stats(void) {
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration = current_time - game_start_time;
  uint32_t minutes = game_duration / 60;
  uint32_t seconds = game_duration % 60;

  // Calculate average time per move
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  char material_str[32];
  game_get_material_string(material_str, sizeof(material_str));

  // Print atmospheric header
  ESP_LOGI(TAG, "╔═══════════════════════════════╗");
  ESP_LOGI(TAG, "║ ESP32 CHESS v2.4 ║");
  ESP_LOGI(TAG, "║ Move %" PRIu32 " - %s to play ║", move_count,
           (current_player == PLAYER_WHITE) ? "White" : "Black");
  ESP_LOGI(TAG, "║ Material: %s ║", material_str);
  ESP_LOGI(TAG, "╚═══════════════════════════════╝");

  // Game duration and move info
  ESP_LOGI(TAG,
           "Game duration: %02" PRIu32 ":%02" PRIu32 ", Move %" PRIu32
           " (%s to play)",
           minutes, seconds, move_count,
           (current_player == PLAYER_WHITE) ? "White" : "Black");

  // Capture statistics
  ESP_LOGI(TAG, "Captures: White %" PRIu32 " pieces, Black %" PRIu32 " pieces",
           white_captures, black_captures);

  // Check and castle statistics
  ESP_LOGI(TAG,
           "Checks: White %" PRIu32 ", Black %" PRIu32
           " | Castles: White %" PRIu32 ", Black %" PRIu32 "",
           white_checks, black_checks, white_castles, black_castles);

  // Material breakdown
  int white_material, black_material;
  game_calculate_material_balance(&white_material, &black_material);
  ESP_LOGI(TAG, "Material: White %d points, Black %d points (%s)",
           white_material, black_material, material_str);

  // Time statistics
  if (game_timer_is_enabled()) {
    ESP_LOGI(TAG,
             "Time per move: White avg %" PRIu32 "s, Black avg %" PRIu32 "s",
             white_avg_time, black_avg_time);
  }

  // Draw detection info
  if (moves_without_capture > 30) {
    ESP_LOGI(TAG,
             "⚠️  %" PRIu32 " moves without capture (50-move rule approaching)",
             moves_without_capture);
  }

  if (game_is_position_repeated()) {
    ESP_LOGI(TAG, "⚠️  Position repeated (potential draw by repetition)");
  }

  // Game save status
  if (game_saved) {
    ESP_LOGI(TAG, "💾 Game saved as: %s", saved_game_name);
  }

  ESP_LOGI(TAG, "═══════════════════════════════");
}

void game_export_pgn(char *pgn_buffer, size_t buffer_size) {
  if (pgn_buffer == NULL || buffer_size == 0) {
    ESP_LOGE(TAG, "Invalid PGN buffer");
    return;
  }

  // Start PGN with metadata
  int pos = snprintf(pgn_buffer, buffer_size,
                     "[Event \"ESP32 Chess Game\"]\n"
                     "[Site \"ESP32-C6\"]\n"
                     "[Date \"%s\"]\n"
                     "[Round \"1\"]\n"
                     "[White \"Player 1\"]\n"
                     "[Black \"Player 2\"]\n"
                     "[Result \"*\"]\n\n",
                     "2025-01-01"); // TODO: Get actual date

  // Add moves
  for (uint32_t i = 0; i < history_index && pos < buffer_size - 50; i++) {
    const chess_move_t *move = &move_history[i];
    char from_square[4], to_square[4];
    game_coords_to_square(move->from_row, move->from_col, from_square);
    game_coords_to_square(move->to_row, move->to_col, to_square);

    if (i % 2 == 0) {
      // White move
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, "%" PRIu32 ". %s%s",
                      (i / 2) + 1, from_square, to_square);
    } else {
      // Black move
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " %s%s ",
                      from_square, to_square);
    }

    // Add newline every 10 moves
    if ((i + 1) % 20 == 0) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, "\n");
    }
  }

  // Add result
  if (game_result == GAME_STATE_FINISHED) {
    if (strstr(saved_game_name, "CHECKMATE")) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1-0");
    } else if (strstr(saved_game_name, "STALEMATE")) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1/2-1/2");
    } else {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1/2-1/2");
    }
  } else {
    pos += snprintf(pgn_buffer + pos, buffer_size - pos, " *");
  }

  ESP_LOGI(TAG, "📄 PGN export completed (%d characters)", pos);
}

uint8_t game_endgame_reason(void) { return (uint8_t)current_endgame_reason; }

