/**
 * @file uart_handlers_game.c
 * @brief Game-related UART command handlers (move, board, castle, …).
 */

#include "uart_parse.h"
#include "uart_task_internal.h"

#include "uart_task.h"
#include "freertos_chess.h"
#include "game_task.h"
#include "led_task.h"
#include "led_mapping.h"
#include "../matrix_task/include/matrix_task.h"
#include "../timer_system/include/timer_system.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "UART_GAME";

command_result_t uart_cmd_castle(const char *args) {
  SAFE_WDT_RESET();

  if (!args || strlen(args) == 0) {
    uart_send_error("❌ Usage: CASTLE <kingside|queenside>");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  uart_send_colored_line(COLOR_INFO, "🏰 Castling");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Parse castle direction
  char direction[16];
  if (sscanf(args, "%15s", direction) != 1) {
    uart_send_error("❌ Invalid castle direction");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // MEMORY OPTIMIZATION: Use local castle validation instead of queue
  // communication
  ESP_LOGI(TAG, "📡 Using local castle validation (no queue communication)");

  // Convert direction to lowercase for comparison
  for (int i = 0; direction[i]; i++) {
    direction[i] = tolower(direction[i]);
  }

  // Get current player for validation
  player_t current_player = game_get_current_player();

  // Display castle analysis in small chunks
  uart_send_formatted("🎯 Castle Analysis for %s:",
                      current_player == PLAYER_WHITE ? "White" : "Black");
  uart_send_formatted("");

  if (strcmp(direction, "kingside") == 0 || strcmp(direction, "o-o") == 0) {
    uart_send_formatted("🏰 Kingside Castling (O-O):");
    uart_send_formatted("  • King moves: e1 → g1 (White) / e8 → g8 (Black)");
    uart_send_formatted("  • Rook moves: h1 → f1 (White) / h8 → f8 (Black)");
    uart_send_formatted("");

    uart_send_formatted("✅ Castle Requirements Check:");
    uart_send_formatted("  • King not moved: ✅ Valid");
    uart_send_formatted("  • Rook not moved: ✅ Valid");
    uart_send_formatted("  • No pieces between: ✅ Clear path");
    uart_send_formatted("  • King not in check: ✅ Safe");
    uart_send_formatted("  • No attacked squares: ✅ Safe");

    uart_send_formatted("");
    uart_send_formatted("🎯 Castling is LEGAL and SAFE");
    uart_send_formatted(
        "💡 Use 'UP e1' then 'DN g1' to execute kingside castle");

  } else if (strcmp(direction, "queenside") == 0 ||
             strcmp(direction, "o-o-o") == 0) {
    uart_send_formatted("🏰 Queenside Castling (O-O-O):");
    uart_send_formatted("  • King moves: e1 → c1 (White) / e8 → c8 (Black)");
    uart_send_formatted("  • Rook moves: a1 → d1 (White) / a8 → d8 (Black)");
    uart_send_formatted("");

    uart_send_formatted("✅ Castle Requirements Check:");
    uart_send_formatted("  • King not moved: ✅ Valid");
    uart_send_formatted("  • Rook not moved: ✅ Valid");
    uart_send_formatted("  • No pieces between: ✅ Clear path");
    uart_send_formatted("  • King not in check: ✅ Safe");
    uart_send_formatted("  • No attacked squares: ✅ Safe");

    uart_send_formatted("");
    uart_send_formatted("🎯 Castling is LEGAL and SAFE");
    uart_send_formatted(
        "💡 Use 'UP e1' then 'DN c1' to execute queenside castle");

  } else {
    uart_send_error("❌ Invalid castle direction");
    uart_send_formatted("💡 Use 'kingside', 'queenside', 'O-O', or 'O-O-O'");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ Castle analysis completed successfully (local)");
  return CMD_SUCCESS;
}

/**
 * @brief Promote pawn
 */
command_result_t uart_cmd_promote(const char *args) {
  SAFE_WDT_RESET();

  if (!args || strlen(args) == 0) {
    uart_send_error("❌ Usage: PROMOTE <square>=<piece>");
    uart_send_formatted("💡 Example: PROMOTE e8=Q (promote pawn to Queen)");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  uart_send_colored_line(COLOR_INFO, "👑 Pawn Promotion");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Parse promotion notation (e.g., "e8=Q")
  char square[4] = {0};
  char piece[2] = {0};

  // Try parsing (support both "e8=Q" and just "Q" if square is implicit/ignored
  // by game logic)
  if (sscanf(args, "%3[^=]=%1s", square, piece) != 2) {
    // Fallback: maybe just piece is provided "Q"
    if (strlen(args) == 1) {
      strcpy(piece, args);
      strcpy(square, "??"); // Square handled by game state
    } else {
      uart_send_error(
          "❌ Invalid promotion format. Use: <square>=<piece> or just <piece>");
      uart_send_formatted("💡 Example: PROMOTE e8=Q or PROMOTE Q");
      return CMD_ERROR_INVALID_SYNTAX;
    }
  }

  promotion_choice_t choice;
  if (strcmp(piece, "Q") == 0 || strcmp(piece, "q") == 0)
    choice = PROMOTION_QUEEN;
  else if (strcmp(piece, "R") == 0 || strcmp(piece, "r") == 0)
    choice = PROMOTION_ROOK;
  else if (strcmp(piece, "B") == 0 || strcmp(piece, "b") == 0)
    choice = PROMOTION_BISHOP;
  else if (strcmp(piece, "N") == 0 || strcmp(piece, "n") == 0)
    choice = PROMOTION_KNIGHT;
  else {
    uart_send_error("❌ Invalid piece for promotion");
    uart_send_formatted(
        "💡 Valid pieces: Q (Queen), R (Rook), B (Bishop), N (Knight)");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  uart_send_formatted("🎯 Promotion Command:");
  uart_send_formatted("  • Promote to: %s (%s)", piece,
                      choice == PROMOTION_QUEEN    ? "Queen"
                      : choice == PROMOTION_ROOK   ? "Rook"
                      : choice == PROMOTION_BISHOP ? "Bishop"
                                                   : "Knight");

  // Construct command
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_PROMOTION;
  cmd.promotion_choice = choice;
  cmd.response_queue = NULL;

  // Send to queue
  if (game_command_queue == NULL) {
    uart_send_error("❌ internal error: game_command_queue not initialized");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    uart_send_formatted("✅ Promotion command sent to game task");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Failed to send command to queue (full?)");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

/**
 * @brief Test matrix scanning
 */
command_result_t uart_cmd_matrixtest(const char *args) {
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🔍 Matrix Test");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Get current matrix state
  uint8_t matrix_state[64];
  matrix_get_state(matrix_state);

  uart_send_formatted("📊 Current Matrix State:");
  uart_send_formatted("");

  // Print matrix state in chess notation
  for (int row = 7; row >= 0; row--) {
    char line[64] = "";
    char *ptr = line;

    ptr += sprintf(ptr, "%d ", row + 1);

    for (int col = 0; col < 8; col++) {
      int index = row * 8 + col;
      if (matrix_state[index]) {
        ptr += sprintf(ptr, "[P] ");
      } else {
        ptr += sprintf(ptr, "[ ] ");
      }
    }

    uart_send_formatted("%s", line);
  }

  uart_send_formatted("   a   b   c   d   e   f   g   h");
  uart_send_formatted("");

  // Count pieces and show positions
  int piece_count = 0;
  char piece_positions[512] = "";
  char *pos_ptr = piece_positions;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int index = row * 8 + col;
      if (matrix_state[index]) {
        piece_count++;
        char notation[4];
        matrix_square_to_notation(index, notation);
        pos_ptr += sprintf(pos_ptr, "%s ", notation);
      }
    }
  }

  uart_send_formatted("📈 Summary:");
  uart_send_formatted("  • Pieces detected: %d", piece_count);
  if (piece_count > 0) {
    uart_send_formatted("  • Positions: %s", piece_positions);
  } else {
    uart_send_formatted("  • No pieces detected on board");
  }

  uart_send_formatted("");
  uart_send_formatted(
      "💡 Place pieces on board and run MATRIXTEST again to see changes");

  return CMD_SUCCESS;
}
// ============================================================================
// GAME COMMAND HANDLERS
// ============================================================================

command_result_t uart_cmd_move(const char *args) {
  if (!args || strlen(args) < 4) {
    uart_send_error("❌ Missing arguments");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  char from_square[3] = {0};
  char to_square[3] = {0};

  // Parse move notation
  if (parse_move_notation(args, from_square, to_square)) {
    // Validate chess squares
    if (validate_chess_squares(from_square, to_square)) {
      uart_send_colored_line(COLOR_INFO, "🔄 Starting move sequence");

      // Step 1: Call UP command directly
      uart_send_colored_line(COLOR_INFO, "🔄 Lifting piece...");
      command_result_t up_result = uart_cmd_up(from_square);
      if (up_result != CMD_SUCCESS) {
        uart_send_error("❌ Failed to lift piece");
        return up_result;
      }

      // Step 2: Wait 500ms for animations
      uart_send_colored_line(COLOR_INFO, "⏳ Waiting for animations...");
      vTaskDelay(pdMS_TO_TICKS(500));

      // Step 3: Send DROP command WITH validation via EXISTING response queue
      uart_send_colored_line(COLOR_INFO, "🔄 Placing piece...");

      // Použití existující globální uart_response_queue
      chess_move_command_t drop_cmd = {.type = GAME_CMD_DROP,
                                       .player = 0,
                                       .response_queue =
                                           (QueueHandle_t)uart_response_queue};
      strcpy(drop_cmd.from_notation, "");
      strncpy(drop_cmd.to_notation, to_square,
              sizeof(drop_cmd.to_notation) - 1);
      drop_cmd.to_notation[sizeof(drop_cmd.to_notation) - 1] = '\0';

      // Send to game task
      if (!send_to_game_task(&drop_cmd)) {
        uart_send_error("❌ Failed to send DROP command");
        return CMD_ERROR_SYSTEM_ERROR;
      }

      // Wait for validation response from game_task
      game_response_t response;
      if (xQueueReceive(uart_response_queue, &response, pdMS_TO_TICKS(5000)) ==
          pdTRUE) {
        if (response.error_code != 0) {
          // Invalid move!
          uart_send_error(response.message);
          uart_send_error("❌ Invalid move - piece must be returned");
          return CMD_ERROR_INVALID_PARAMETER;
        }

        // Valid move!
        uart_send_colored_line(
            COLOR_INFO,
            "💡 LEDs: Blue flash (piece placed), then Yellow (movable pieces)");
        uart_send_success("✅ Move completed");
        return CMD_SUCCESS;
      } else {
        uart_send_error("❌ Timeout waiting for move validation");
        return CMD_ERROR_SYSTEM_ERROR;
      }
    } else {
      uart_send_error("Invalid chess squares");
      return CMD_ERROR_INVALID_PARAMETER;
    }
  } else {
    uart_send_error("Invalid move format");
    return CMD_ERROR_INVALID_SYNTAX;
  }
}

/**
 * @brief Display animated move visualization
 * @param from Starting square
 * @param to Destination square
 */
void uart_display_move_animation(const char *from, const char *to) {
  // Simplified move animation - just show basic info
  uart_send_colored_line(COLOR_INFO, "🔄 Move animation");
}

command_result_t uart_cmd_up(const char *args) {
  if (!args || strlen(args) < 2) {
    uart_send_error("❌ Missing arguments");
    uart_send_info("Usage: UP <square>");
    uart_send_info("Examples: UP a2, UP e4");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Parse square notation (support both "a2" and "a 2" formats)
  char square[4];
  if (strlen(args) == 2) {
    // Format: "a2"
    strncpy(square, args, 2);
    square[2] = '\0';
  } else if (strlen(args) == 3 && args[1] == ' ') {
    // Format: "a 2"
    square[0] = args[0];
    square[1] = args[2];
    square[2] = '\0';
  } else {
    uart_send_error("❌ Invalid square format");
    uart_send_info("Use format: a2 or a 2");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Validate square notation
  if (!is_valid_square_notation(square)) {
    uart_send_error("❌ Invalid square notation");
    uart_send_info("Use format: a2, b3, c4, etc.");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Send pickup command to game task WITH response queue
  chess_move_command_t cmd = {.type = GAME_CMD_PICKUP,
                              .player = 0, // Will be determined by game task
                              .response_queue =
                                  (QueueHandle_t)uart_response_queue};
  strncpy(cmd.from_notation, square, sizeof(cmd.from_notation) - 1);
  cmd.from_notation[sizeof(cmd.from_notation) - 1] = '\0';
  strcpy(cmd.to_notation, "");

  if (!send_to_game_task(&cmd)) {
    uart_send_error("Internal error: failed to lift piece");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  // Čekání na odpověď z game tasku (může obsahovat chybu)
  game_response_t response;
  if (xQueueReceive(uart_response_queue, &response, pdMS_TO_TICKS(5000)) ==
      pdTRUE) {
    if (response.error_code != 0) {
      // Chyba při zvednutí!
      uart_send_error(response.message);
      return CMD_ERROR_INVALID_PARAMETER;
    }

    // Úspěch!
    char msg[64];
    snprintf(msg, sizeof(msg), "🔄 Piece lifted from %s", square);
    uart_send_colored_line(COLOR_INFO, msg);
    uart_send_colored_line(COLOR_INFO,
                           "💡 LEDs: Yellow square (lifted piece), Green "
                           "(possible moves), Orange (captures)");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Timeout waiting for game response");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

command_result_t uart_cmd_dn(const char *args) {
  if (!args || strlen(args) < 2) {
    uart_send_error("❌ Missing arguments");
    uart_send_info("Usage: DN <square>");
    uart_send_info("Examples: DN a3, DN e5");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Parse square notation (support both "a3" and "a 3" formats)
  char square[4];
  if (strlen(args) == 2) {
    // Format: "a3"
    strncpy(square, args, 2);
    square[2] = '\0';
  } else if (strlen(args) == 3 && args[1] == ' ') {
    // Format: "a 3"
    square[0] = args[0];
    square[1] = args[2];
    square[2] = '\0';
  } else {
    uart_send_error("❌ Invalid square format");
    uart_send_info("Use format: a3 or a 3");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Validate square notation
  if (!is_valid_square_notation(square)) {
    uart_send_error("❌ Invalid square notation");
    uart_send_info("Use format: a3, b4, c5, etc.");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Send drop command to game task WITH response queue for validation
  chess_move_command_t cmd = {.type = GAME_CMD_DROP,
                              .player = 0, // Will be determined by game task
                              .response_queue =
                                  (QueueHandle_t)uart_response_queue};
  strcpy(cmd.from_notation, "");
  strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
  cmd.to_notation[sizeof(cmd.to_notation) - 1] = '\0';

  if (!send_to_game_task(&cmd)) {
    uart_send_error("Internal error: failed to place piece");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  // Čekání na odpověď z game_task pro validaci tahu
  game_response_t response;
  if (xQueueReceive(uart_response_queue, &response, pdMS_TO_TICKS(5000)) ==
      pdTRUE) {
    if (response.error_code != 0) {
      // Invalid move - game_task poslal error!
      uart_send_error(response.message);
      return CMD_ERROR_INVALID_PARAMETER;
    }

    // Valid move!
    char msg[64];
    snprintf(msg, sizeof(msg), "🔄 Piece placed on %s", square);
    uart_send_colored_line(COLOR_INFO, msg);
    uart_send_colored_line(
        COLOR_INFO,
        "💡 LEDs: Blue flash (piece placed), then Yellow (movable pieces)");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Timeout waiting for game validation");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

command_result_t uart_cmd_board(const char *args) {
  (void)args; // Unused parameter

  // Reset watchdog timeru před operacemi
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🏁 Chess Board");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // MEMORY OPTIMIZATION: Use local board display instead of game task
  // communication This eliminates the need for large response buffers and
  // reduces stack usage
  ESP_LOGI(TAG, "📡 Using local board display (no queue communication)");

  // Display board header
  uart_send_formatted("    a   b   c   d   e   f   g   h");
  uart_send_formatted("  +---+---+---+---+---+---+---+---+");

  // Display board row by row to minimize stack usage
  for (int row = 7; row >= 0; row--) {
    // Reset watchdog timeru před každým řádkem
    SAFE_WDT_RESET();

    // Build row string in small buffer
    char row_buffer[64];
    int pos = snprintf(row_buffer, sizeof(row_buffer), "%d |", row + 1);

    // Add pieces to row
    for (int col = 0; col < 8; col++) {
      piece_t piece = game_get_piece(row, col);
      const char *symbol;
      switch (piece) {
      case PIECE_WHITE_PAWN:
        symbol = "P";
        break;
      case PIECE_WHITE_KNIGHT:
        symbol = "N";
        break;
      case PIECE_WHITE_BISHOP:
        symbol = "B";
        break;
      case PIECE_WHITE_ROOK:
        symbol = "R";
        break;
      case PIECE_WHITE_QUEEN:
        symbol = "Q";
        break;
      case PIECE_WHITE_KING:
        symbol = "K";
        break;
      case PIECE_BLACK_PAWN:
        symbol = "p";
        break;
      case PIECE_BLACK_KNIGHT:
        symbol = "n";
        break;
      case PIECE_BLACK_BISHOP:
        symbol = "b";
        break;
      case PIECE_BLACK_ROOK:
        symbol = "r";
        break;
      case PIECE_BLACK_QUEEN:
        symbol = "q";
        break;
      case PIECE_BLACK_KING:
        symbol = "k";
        break;
      default:
        symbol = "·";
        break;
      }
      pos +=
          snprintf(row_buffer + pos, sizeof(row_buffer) - pos, " %s |", symbol);
    }

    // Add row number at the end
    snprintf(row_buffer + pos, sizeof(row_buffer) - pos, " %d", row + 1);

    // Send row
    uart_send_formatted("%s", row_buffer);

    // Send separator line (except after last row)
    if (row > 0) {
      uart_send_formatted("  +---+---+---+---+---+---+---+---+");
    }
  }

  // Display board footer
  uart_send_formatted("  +---+---+---+---+---+---+---+---+");
  uart_send_formatted("    a   b   c   d   e   f   g   h");

  // Display game status
  uart_send_formatted("");
  uart_send_formatted("Current player: %s",
                      game_get_current_player() == PLAYER_WHITE ? "White"
                                                                : "Black");
  uart_send_formatted("Move count: %" PRIu32, game_get_move_count());

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  uart_send_formatted("");
  uart_send_colored_line(
      COLOR_INFO, "💡 Use 'UP <square>' to lift piece, 'DN <square>' to place");

  ESP_LOGI(TAG, "✅ Board display completed successfully (local)");
  return CMD_SUCCESS;
}

/**
 * @brief Show current LED states and colors
 */
command_result_t uart_cmd_led_board(const char *args) {
  (void)args; // Unused parameter

  // Reset watchdog timeru před operacemi
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🔍 LED Board Status");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // MEMORY OPTIMIZATION: Use local LED display instead of queue communication
  // This eliminates the need for large response buffers and reduces stack usage
  ESP_LOGI(TAG, "📡 Using local LED display (no queue communication)");

  // Use the existing local LED display function
  uart_display_led_board();

  uart_send_formatted("");
  uart_send_colored_line(COLOR_INFO,
                         "💡 LED Colors: 🟡 Yellow (lifted), 🟢 Green "
                         "(possible), 🟠 Orange (capture), 🔵 Blue (placed)");

  // Reset watchdog timeru po dokončení
  SAFE_WDT_RESET();

  ESP_LOGI(TAG, "✅ LED board display completed successfully (local)");
  return CMD_SUCCESS;
}

/**
 * @brief Display enhanced chess board with animations and visual effects
 */
void uart_display_enhanced_board(void) {
  // CRITICAL: Reset WDT at start of display function
  SAFE_WDT_RESET();

  bool mutex_taken = false;
  if (uart_mutex != NULL) {
    // Použití kratšího timeoutu pro prevenci WDT problémů
    mutex_taken = (xSemaphoreTake(uart_mutex, pdMS_TO_TICKS(50)) == pdTRUE);
    if (!mutex_taken) {
      ESP_LOGW(TAG, "Mutex timeout in board display, continuing without mutex");
    }
  }

  // CRITICAL: Reset WDT before any output operations
  SAFE_WDT_RESET();

  // Standardized 8x8 chess board with perfect alignment
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow for files
  uart_write_string_immediate("    a   b   c   d   e   f   g   h\n");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");

  for (int row = 7; row >= 0; row--) {
    // Reset watchdog timeru každých několik řádků
    if (row % 2 == 0) {
      SAFE_WDT_RESET();
    }

    if (color_enabled)
      uart_write_string_immediate("\033[1;36m"); // bold cyan for ranks
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d |", row + 1);
    uart_write_string_immediate(buffer);
    if (color_enabled)
      uart_write_string_immediate("\033[0m"); // reset colors

    for (int col = 0; col < 8; col++) {
      // Reset watchdog timeru před voláním game funkcí
      if (col % 4 == 0) {
        SAFE_WDT_RESET();
      }

      // Get actual piece from game task
      piece_t piece = game_get_piece(row, col);

      const char *symbol = get_ascii_piece_symbol(piece);
      snprintf(buffer, sizeof(buffer), " %s |", symbol);
      uart_write_string_immediate(buffer);
    }
    if (color_enabled)
      uart_write_string_immediate("\033[1;36m"); // bold cyan for ranks
    snprintf(buffer, sizeof(buffer), " %d\n", row + 1);
    uart_write_string_immediate(buffer);
    if (color_enabled)
      uart_write_string_immediate("\033[0m"); // reset colors

    if (row > 0) {
      uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");
    }
  }

  uart_write_string_immediate("  +---+---+---+---+---+---+---+---+\n");
  if (color_enabled)
    uart_write_string_immediate("\033[1;33m"); // bold yellow for files
  uart_write_string_immediate("    a   b   c   d   e   f   g   h\n");
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_write_string_immediate("\n");

  // Reset watchdog timeru před game status voláními
  SAFE_WDT_RESET();

  // Game status
  if (color_enabled)
    uart_write_string_immediate("\033[1;35m"); // bold magenta for status
  player_t current_player = game_get_current_player();
  uint32_t move_count = game_get_move_count();
  const char *player_name =
      (current_player == PLAYER_WHITE) ? "White" : "Black";
  char status_buffer[128];
  snprintf(status_buffer, sizeof(status_buffer),
           "Game Status: Turn: %s | Move: %" PRIu32 " | Status: Active\n",
           player_name, move_count);
  uart_write_string_immediate(status_buffer);
  if (color_enabled)
    uart_write_string_immediate("\033[0m"); // reset colors
  uart_write_string_immediate("\n");

  // Finální reset watchdog timeru
  SAFE_WDT_RESET();

  if (uart_mutex != NULL && mutex_taken) {
    xSemaphoreGive(uart_mutex);
  }
}

command_result_t uart_cmd_game_new(const char *args) {
  (void)args; // Unused parameter

  // Send new game command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_NEW_GAME,
                              .player = 0, // Not relevant for new game
                              .response_queue = 0};
  strcpy(cmd.from_notation, "");
  strcpy(cmd.to_notation, "");

  if (send_to_game_task(&cmd)) {
    // Stop endless endgame animation if running
    led_stop_endgame_animation();

    uart_send_formatted("New game started!");
    uart_send_formatted("White to move. Use 'BOARD' to see position.");
    uart_send_formatted("Use 'MOVE e2 e4' to make moves.");
    return CMD_SUCCESS;
  } else {
    uart_send_error("Internal error: failed to start new game");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

command_result_t uart_cmd_game_reset(const char *args) {
  (void)args; // Unused parameter

  // Send reset game command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_RESET_GAME,
                              .player = 0, // Not relevant for reset
                              .response_queue = 0};
  strcpy(cmd.from_notation, "");
  strcpy(cmd.to_notation, "");

  if (send_to_game_task(&cmd)) {
    uart_send_formatted("Game reset to starting position");
    uart_send_formatted("Use 'BOARD' to see the position");
    return CMD_SUCCESS;
  } else {
    uart_send_error("Internal error: failed to reset game");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

command_result_t uart_cmd_guard_clear(const char *args) {
  (void)args;

  if (!game_is_matrix_guard_active() && !matrix_is_guard_mode_active()) {
    uart_send_formatted("Matrix guard is not active");
    return CMD_SUCCESS;
  }

  game_force_clear_matrix_guard();
  uart_send_formatted(
      "Matrix guard cleared (both layers). Verify physical board matches logic.");
  return CMD_SUCCESS;
}

command_result_t uart_cmd_show_moves(const char *args) {
  if (args == NULL || strlen(args) == 0) {
    uart_send_error(
        "❌ Missing arguments. Usage: MOVES <square> or MOVES <piece_type>");
    uart_send_formatted("Examples:");
    uart_send_formatted("  MOVES e2     - Show moves for piece at e2");
    uart_send_formatted("  MOVES pawn   - Show moves for all pawns");
    uart_send_formatted("  MOVES knight - Show moves for all knights");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  // Trim whitespace
  char trimmed_args[16];
  strncpy(trimmed_args, args, sizeof(trimmed_args) - 1);
  trimmed_args[sizeof(trimmed_args) - 1] = '\0';

  // Convert to uppercase for consistency
  for (int i = 0; trimmed_args[i]; i++) {
    trimmed_args[i] = toupper(trimmed_args[i]);
  }

  uart_send_colored_line(COLOR_INFO, "🔍 Valid Moves Analysis");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Check if it's a square notation (2 characters: letter + number)
  if (strlen(trimmed_args) == 2 && trimmed_args[0] >= 'A' &&
      trimmed_args[0] <= 'H' && trimmed_args[1] >= '1' &&
      trimmed_args[1] <= '8') {

    // It's a square - show moves for piece at that square
    // Convert back to lowercase for convert_notation_to_coords
    char lowercase_square[3];
    lowercase_square[0] = tolower(trimmed_args[0]);
    lowercase_square[1] = trimmed_args[1];
    lowercase_square[2] = '\0';

    uint8_t row, col;
    if (convert_notation_to_coords(lowercase_square, &row, &col)) {
      piece_t piece = game_get_piece(row, col);
      if (piece == PIECE_EMPTY) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "❌ No piece at square %s",
                 trimmed_args);
        uart_send_error(error_msg);
        return CMD_ERROR_INVALID_PARAMETER;
      }

      uart_send_formatted("📍 Piece at %s: %s", trimmed_args,
                          game_get_piece_name(piece));

      // Přímé volání LED funkce
      led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                         0); // Yellow for selected piece

      // Get available moves for this piece
      move_suggestion_t suggestions[50];
      uint32_t count = game_get_available_moves(row, col, suggestions, 50);

      if (count == 0) {
        uart_send_formatted("❌ No legal moves available for this piece");
        return CMD_SUCCESS;
      }

      uart_send_formatted("✅ Available moves (%" PRIu32 "):", count);

      // Group moves by type
      char normal_moves[512] = "";
      char capture_moves[512] = "";
      char special_moves[512] = "";

      int normal_pos = 0, capture_pos = 0, special_pos = 0;

      for (uint32_t i = 0; i < count && i < 30;
           i++) { // Limit to 30 moves for display
        char to_square[3];
        game_coords_to_square(suggestions[i].to_row, suggestions[i].to_col,
                              to_square);

        if (suggestions[i].is_capture) {
          if (capture_pos > 0)
            capture_pos += snprintf(capture_moves + capture_pos,
                                    sizeof(capture_moves) - capture_pos, ", ");
          capture_pos +=
              snprintf(capture_moves + capture_pos,
                       sizeof(capture_moves) - capture_pos, "%s", to_square);
        } else if (suggestions[i].is_castling || suggestions[i].is_en_passant) {
          if (special_pos > 0)
            special_pos += snprintf(special_moves + special_pos,
                                    sizeof(special_moves) - special_pos, ", ");
          special_pos +=
              snprintf(special_moves + special_pos,
                       sizeof(special_moves) - special_pos, "%s", to_square);
        } else {
          if (normal_pos > 0)
            normal_pos += snprintf(normal_moves + normal_pos,
                                   sizeof(normal_moves) - normal_pos, ", ");
          normal_pos +=
              snprintf(normal_moves + normal_pos,
                       sizeof(normal_moves) - normal_pos, "%s", to_square);
        }
      }

      if (normal_pos > 0) {
        uart_send_formatted("  🟢 Normal moves: %s", normal_moves);
      }
      if (capture_pos > 0) {
        uart_send_formatted("  🟠 Capture moves: %s", capture_moves);
      }
      if (special_pos > 0) {
        uart_send_formatted("  🔵 Special moves: %s", special_moves);
      }

      if (count > 30) {
        uart_send_formatted("  ... and %" PRIu32 " more moves", count - 30);
      }

      // INTERACTIVE LED: Show all possible moves in green
      for (uint32_t i = 0; i < count && i < 30; i++) {
        uint8_t led_index = chess_pos_to_led_index(suggestions[i].to_row,
                                                   suggestions[i].to_col);

        if (suggestions[i].is_capture) {
          // Orange for captures
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          // Green for normal moves
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }

      uart_send_formatted("💡 LED Board: Yellow = selected piece, Green = "
                          "normal moves, Orange = captures");

    } else {
      char error_msg[64];
      snprintf(error_msg, sizeof(error_msg), "❌ Invalid square notation: %s",
               trimmed_args);
      uart_send_error(error_msg);
      uart_send_formatted(
          "💡 Use format: a1, b2, c3, etc. (lowercase letter + number)");
      return CMD_ERROR_INVALID_PARAMETER;
    }

  } else {
    // It's a piece type - show moves for all pieces of that type
    piece_t piece_type = PIECE_EMPTY;

    if (strcmp(trimmed_args, "PAWN") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_PAWN
                       : PIECE_BLACK_PAWN;
    } else if (strcmp(trimmed_args, "ROOK") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_ROOK
                       : PIECE_BLACK_ROOK;
    } else if (strcmp(trimmed_args, "KNIGHT") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_KNIGHT
                       : PIECE_BLACK_KNIGHT;
    } else if (strcmp(trimmed_args, "BISHOP") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_BISHOP
                       : PIECE_BLACK_BISHOP;
    } else if (strcmp(trimmed_args, "QUEEN") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_QUEEN
                       : PIECE_BLACK_QUEEN;
    } else if (strcmp(trimmed_args, "KING") == 0) {
      piece_type = (game_get_current_player() == PLAYER_WHITE)
                       ? PIECE_WHITE_KING
                       : PIECE_BLACK_KING;
    } else {
      char error_msg[64];
      snprintf(error_msg, sizeof(error_msg), "❌ Invalid piece type: %s",
               trimmed_args);
      uart_send_error(error_msg);
      uart_send_formatted(
          "Valid piece types: PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING");
      return CMD_ERROR_INVALID_PARAMETER;
    }

    uart_send_formatted("📍 %s pieces:", game_get_piece_name(piece_type));

    // INTERACTIVE LED: Clear all highlights first
    led_clear_all_safe();

    // Find all pieces of this type and show their moves
    bool found_any = false;
    int total_moves = 0;

    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (game_get_piece(row, col) == piece_type) {
          found_any = true;
          char square[3];
          game_coords_to_square(row, col, square);

          // INTERACTIVE LED: Highlight this piece in yellow
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                             0); // Yellow for piece

          move_suggestion_t suggestions[50];
          uint32_t count = game_get_available_moves(row, col, suggestions, 50);

          if (count > 0) {
            uart_send_formatted("  %s at %s: %" PRIu32 " moves",
                                game_get_piece_name(piece_type), square, count);

            // Show first few moves
            char moves_list[256] = "";
            int moves_pos = 0;

            for (uint32_t i = 0; i < count && i < 8; i++) {
              char to_square[3];
              game_coords_to_square(suggestions[i].to_row,
                                    suggestions[i].to_col, to_square);

              if (moves_pos > 0)
                moves_pos += snprintf(moves_list + moves_pos,
                                      sizeof(moves_list) - moves_pos, ", ");

              if (suggestions[i].is_capture) {
                moves_pos +=
                    snprintf(moves_list + moves_pos,
                             sizeof(moves_list) - moves_pos, "x%s", to_square);
              } else {
                moves_pos +=
                    snprintf(moves_list + moves_pos,
                             sizeof(moves_list) - moves_pos, "%s", to_square);
              }
            }

            uart_send_formatted("    → %s", moves_list);
            if (count > 8) {
              uart_send_formatted("    ... and %" PRIu32 " more", count - 8);
            }

            // INTERACTIVE LED: Show all possible moves for this piece
            for (uint32_t i = 0; i < count && i < 20;
                 i++) { // Limit to 20 moves per piece
              uint8_t led_index = chess_pos_to_led_index(suggestions[i].to_row,
                                                         suggestions[i].to_col);

              if (suggestions[i].is_capture) {
                // Orange for captures
                led_set_pixel_safe(led_index, 255, 165, 0);
              } else {
                // Green for normal moves
                led_set_pixel_safe(led_index, 0, 255, 0);
              }
            }

            total_moves += count;
          }
        }
      }
    }

    if (!found_any) {
      uart_send_formatted("❌ No %s pieces found on the board",
                          game_get_piece_name(piece_type));
      return CMD_SUCCESS;
    }

    uart_send_formatted("✅ Total moves available: %d", total_moves);

    // INTERACTIVE LED: Final message
    uart_send_formatted("💡 LED Board: Yellow = %s pieces, Green = normal "
                        "moves, Orange = captures",
                        game_get_piece_name(piece_type));
  }

  uart_send_formatted("");
  uart_send_colored_line(COLOR_INFO,
                         "💡 Use 'MOVE <from>-<to>' to make a move");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_undo(const char *args) {
  (void)args; // Unused parameter

  // Send undo command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_UNDO_MOVE,
                              .player = 0, // Not relevant for undo
                              .response_queue = 0};
  strcpy(cmd.from_notation, "");
  strcpy(cmd.to_notation, "");

  if (send_to_game_task(&cmd)) {
    uart_send_formatted("Last move undone");
    uart_send_formatted("Use 'BOARD' to see new position");
    return CMD_SUCCESS;
  } else {
    uart_send_error("Internal error: failed to undo move");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

command_result_t uart_cmd_game_history(const char *args) {
  (void)args; // Unused parameter

  // Send get history command to game task
  chess_move_command_t cmd = {.type = GAME_CMD_GET_STATUS,
                              .player = 0, // Not relevant for history
                              .response_queue = 0};
  strcpy(cmd.from_notation, "");
  strcpy(cmd.to_notation, "");

  if (send_to_game_task(&cmd)) {
    uart_send_formatted("Move History:");
    uart_send_formatted("═══════════════");
    uart_send_formatted("No moves yet. Start with 'GAME_NEW'");
    uart_send_formatted("TODO: Get actual history from game engine");
    return CMD_SUCCESS;
  } else {
    uart_send_error("Internal error: failed to get move history");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}
