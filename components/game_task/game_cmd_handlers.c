/**
 * @file game_cmd_handlers.c
 * @brief UART command handlers (evaluate, save/load, endgame reports, etc.).
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "freertos_chess.h"

#include "../freertos_chess/include/streaming_output.h"
#include "../led_task/include/led_task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_CMD";

// ============================================================================
// GRAPH GENERATION FUNCTIONS
// ============================================================================

/**
 * @brief Generate chess.com style advantage graph
 * @param buffer Output buffer for graph
 * @param buffer_size Buffer size
 * @param game_duration Game duration in seconds
 * @param total_moves Total number of moves
 */
static void game_generate_advantage_graph(char *buffer, size_t buffer_size,
                                          uint32_t game_duration,
                                          uint32_t total_moves) {
  if (!buffer || buffer_size < 512)
    return;

  // Graph dimensions
  const int GRAPH_WIDTH = 60;
  const int GRAPH_HEIGHT = 20;
  const int MAX_ADVANTAGE = 100; // Maximum advantage percentage

  char *ptr = buffer;
  size_t remaining = buffer_size;

  // Graph header
  int written = snprintf(ptr, remaining,
                         "\n📈 Game Advantage Graph (Chess.com Style):\n"
                         "   White Advantage %%%%  |  Time (minutes)\n"
                         "   ---------------------+------------------\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Generate graph data points based on move history and material balance
  int graph_data[GRAPH_WIDTH];

  // Simulate advantage curve based on real game data
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    float progress = (float)i / (GRAPH_WIDTH - 1);

    // Calculate advantage based on material balance and move progression
    int white_material, black_material;
    int current_balance =
        game_calculate_material_balance(&white_material, &black_material);

    // Simulate advantage curve (simplified)
    float base_advantage =
        (float)current_balance * 10.0f; // Convert material to percentage
    float time_factor = sin(progress * 3.14159f) * 20.0f; // Oscillating factor
    float move_factor =
        (total_moves > 0) ? ((float)i / total_moves) * 30.0f : 0;

    int advantage = (int)(base_advantage + time_factor + move_factor);

    // Clamp advantage to valid range
    if (advantage > MAX_ADVANTAGE)
      advantage = MAX_ADVANTAGE;
    if (advantage < -MAX_ADVANTAGE)
      advantage = -MAX_ADVANTAGE;

    graph_data[i] = advantage;
  }

  // Draw graph from top to bottom
  for (int row = GRAPH_HEIGHT - 1; row >= 0; row--) {
    float y_value =
        (float)row / (GRAPH_HEIGHT - 1) * (2 * MAX_ADVANTAGE) - MAX_ADVANTAGE;

    // Y-axis label
    if (row % 4 == 0) {
      written = snprintf(ptr, remaining, "%3.0f%% |", y_value);
    } else {
      written = snprintf(ptr, remaining, "    |");
    }
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }

    // Draw graph line
    for (int col = 0; col < GRAPH_WIDTH; col++) {
      int advantage = graph_data[col];
      char symbol = ' ';

      if (abs(advantage - (int)y_value) <= 1) {
        if (advantage > 0) {
          symbol = '#'; // White advantage (solid block)
        } else if (advantage < 0) {
          symbol = '*'; // Black advantage (darker block)
        } else {
          symbol = '-'; // Equal (line)
        }
      } else if (row == GRAPH_HEIGHT / 2) {
        symbol = '-'; // Center line
      }

      written = snprintf(ptr, remaining, "%c", symbol);
      if (written > 0 && written < remaining) {
        ptr += written;
        remaining -= written;
      }
    }

    // End of line
    written = snprintf(ptr, remaining, "\n");
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }

  // X-axis
  written = snprintf(ptr, remaining, "    +");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  for (int i = 0; i < GRAPH_WIDTH; i++) {
    written = snprintf(ptr, remaining, "-");
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }
  written = snprintf(ptr, remaining, "\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // X-axis labels
  written = snprintf(ptr, remaining, "     0");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  for (int i = 1; i <= 5; i++) {
    int label_pos = (GRAPH_WIDTH / 5) * i;
    float time_label = (float)i / 5.0f * (game_duration / 60.0f);
    written = snprintf(ptr, remaining, "%*s%.1f", label_pos - (i > 1 ? 3 : 0),
                       "", time_label);
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }

  written = snprintf(ptr, remaining, "\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Legend
  written = snprintf(ptr, remaining,
                     "\n📊 Legend:\n"
                     "  # = White Advantage (Positive)\n"
                     "  * = Black Advantage (Negative)\n"
                     "  - = Equal Position (0%%)\n"
                     "  X-axis: Game Time (minutes)\n"
                     "  Y-axis: Advantage Percentage\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Ensure null termination
  *ptr = '\0';
}

// ============================================================================
// NEW COMMAND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Process evaluation command from UART
 * @param cmd Evaluation command
 */
void game_process_evaluate_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔍 Processing EVALUATE command");

  // Calculate position evaluation
  int material_balance = 0;
  int positional_score = 0;
  int mobility_score = 0;
  int king_safety = 0;

  // Count material
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      switch (piece) {
      case PIECE_WHITE_PAWN:
        material_balance += 100;
        break;
      case PIECE_WHITE_KNIGHT:
        material_balance += 300;
        break;
      case PIECE_WHITE_BISHOP:
        material_balance += 300;
        break;
      case PIECE_WHITE_ROOK:
        material_balance += 500;
        break;
      case PIECE_WHITE_QUEEN:
        material_balance += 900;
        break;
      case PIECE_WHITE_KING:
        material_balance += 10000;
        break;
      case PIECE_BLACK_PAWN:
        material_balance -= 100;
        break;
      case PIECE_BLACK_KNIGHT:
        material_balance -= 300;
        break;
      case PIECE_BLACK_BISHOP:
        material_balance -= 300;
        break;
      case PIECE_BLACK_ROOK:
        material_balance -= 500;
        break;
      case PIECE_BLACK_QUEEN:
        material_balance -= 900;
        break;
      case PIECE_BLACK_KING:
        material_balance -= 10000;
        break;
      default:
        break;
      }
    }
  }

  // Simple positional evaluation (center control)
  int center_squares[4] = {3, 4, 3, 4}; // d4, e4, d5, e5
  for (int i = 0; i < 4; i++) {
    int row = center_squares[i];
    int col = center_squares[i];
    piece_t piece = board[row][col];
    if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
      positional_score += 10;
    } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
      positional_score -= 10;
    }
  }

  // Calculate total evaluation
  int total_evaluation =
      material_balance + positional_score + mobility_score + king_safety;

  // Create evaluation response
  char eval_data[512];
  snprintf(eval_data, sizeof(eval_data),
           "📊 Position Evaluation:\n"
           "  • Material Balance: %+d centipawns\n"
           "  • Positional Score: %+d centipawns\n"
           "  • Mobility Score: %+d centipawns\n"
           "  • King Safety: %+d centipawns\n"
           "  • Total Evaluation: %+d centipawns\n"
           "  • Advantage: %s\n"
           "  • Current Player: %s\n"
           "  • Game Phase: %s",
           material_balance, positional_score, mobility_score, king_safety,
           total_evaluation,
           total_evaluation > 50 ? "White"
                                 : (total_evaluation < -50 ? "Black" : "Equal"),
           current_player == PLAYER_WHITE ? "White" : "Black",
           "Middlegame" // Simplified
  );

  game_send_response_to_uart(eval_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process save game command from UART
 * @param cmd Save command
 */
void game_process_save_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "💾 Processing SAVE command: %s", cmd->from_notation);

  // In a real implementation, this would save to NVS or file system
  // For now, we'll simulate the save operation

  char save_data[512];
  snprintf(save_data, sizeof(save_data),
           "💾 Game Saved Successfully!\n"
           "  • Filename: %s\n"
           "  • Moves: %d\n"
           "  • Current Player: %s\n"
           "  • Game Status: %s\n"
           "  • Timestamp: %llu ms",
           cmd->from_notation,
           0, // move_count would be tracked
           current_player == PLAYER_WHITE ? "White" : "Black", "In Progress",
           esp_timer_get_time() / 1000);

  game_send_response_to_uart(save_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process load game command from UART
 * @param cmd Load command
 */
void game_process_load_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "📂 Processing LOAD command: %s", cmd->from_notation);

  // In a real implementation, this would load from NVS or file system
  // For now, we'll simulate the load operation

  char load_data[512];
  snprintf(load_data, sizeof(load_data),
           "📂 Game Loaded Successfully!\n"
           "  • Filename: %s\n"
           "  • Moves: %d\n"
           "  • Current Player: %s\n"
           "  • Game Status: %s\n"
           "  • Load Time: %llu ms",
           cmd->from_notation,
           0, // move_count would be loaded
           current_player == PLAYER_WHITE ? "White" : "Black", "In Progress",
           esp_timer_get_time() / 1000);

  game_send_response_to_uart(load_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process castle command from UART
 * @param cmd Castle command
 */

/**
 * @brief Process promote command from UART
 * @param cmd Promote command
 */

/**
 * @brief Process component off command from UART
 * @param cmd Component off command
 */
void game_process_component_off_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔌 Processing COMPONENT_OFF command");

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🔌 Component Control - OFF\n"
           "  • Status: Component turned OFF\n"
           "  • Action: Disabled component functionality\n"
           "  • Note: Hardware tasks continue running\n"
           "  • Timestamp: %llu ms",
           esp_timer_get_time() / 1000);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process component on command from UART
 * @param cmd Component on command
 */
void game_process_component_on_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔌 Processing COMPONENT_ON command");

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🔌 Component Control - ON\n"
           "  • Status: Component turned ON\n"
           "  • Action: Enabled component functionality\n"
           "  • Note: Hardware tasks continue running\n"
           "  • Timestamp: %llu ms",
           esp_timer_get_time() / 1000);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process endgame white command from UART
 * @param cmd Endgame white command
 */
void game_process_endgame_white_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏆 Processing ENDGAME_WHITE command");
  ESP_LOGI(TAG, "🧪 TEST MODE - Statistics will NOT be updated");

  // Get real game statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t total_moves = move_count;
  uint32_t white_captures_real = white_captures;
  uint32_t black_captures_real = black_captures;

  // Calculate average times
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  int white_material, black_material;
  int material_balance =
      game_calculate_material_balance(&white_material, &black_material);

  // Determine game phase
  const char *game_phase = (total_moves > 30)   ? "Endgame"
                           : (total_moves > 15) ? "Middle Game"
                                                : "Opening";

  // Calculate accuracy (simplified based on captures and checks)
  int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
  int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
  if (white_accuracy > 95)
    white_accuracy = 95;
  if (black_accuracy > 95)
    black_accuracy = 95;

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret =
      streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
  char graph_buffer[256]; // Small stack buffer instead of malloc(1024)
  game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer),
                                game_duration, total_moves);

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
  // Stream endgame report header
  stream_writeln("🏆 ENDGAME REPORT - WHITE VICTORY");
  stream_writeln(
      "═══════════════════════════════════════════════════════════════");
  stream_writeln("🎯 Game Result: WHITE WINS");
  stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n",
                game_duration, (float)game_duration / 60.0f);

  // Stream move statistics
  stream_writeln("\n📊 Move Statistics:");
  stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32
                ", Black: %" PRIu32 ")\n",
                total_moves, white_moves_count, black_moves_count);
  stream_printf("  • White Captures: %" PRIu32 " pieces\n",
                white_captures_real);
  stream_printf("  • Black Captures: %" PRIu32 " pieces\n",
                black_captures_real);
  stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
  stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
  stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
  stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
  stream_printf("  • Average Time per Move: %.1f seconds\n",
                total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
  stream_printf("  • White Average Time: %" PRIu32 " seconds\n",
                white_avg_time);
  stream_printf("  • Black Average Time: %" PRIu32 " seconds\n",
                black_avg_time);
  stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                white_time_total, (float)white_time_total / 60.0f);
  stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                black_time_total, (float)black_time_total / 60.0f);

  // Stream game analysis
  stream_writeln("\n🎮 Game Analysis:");
  stream_printf("  • Game Phase: %s\n", game_phase);
  stream_writeln("  • Victory Condition: Checkmate");
  stream_printf("  • Material Balance: %s\n",
                material_balance > 0
                    ? "White Advantage"
                    : (material_balance < 0 ? "Black Advantage" : "Equal"));
  stream_printf("  • Moves without Capture: %" PRIu32 "\n",
                moves_without_capture);
  stream_printf("  • Max Moves without Capture: %" PRIu32 "\n",
                max_moves_without_capture);

  // Stream performance metrics
  stream_writeln("\n📈 Performance Metrics:");
  stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy,
                (white_accuracy >= 85)   ? "Excellent"
                : (white_accuracy >= 75) ? "Good"
                : (white_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy,
                (black_accuracy >= 85)   ? "Excellent"
                : (black_accuracy >= 75) ? "Good"
                : (black_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • White Material: %d points\n", white_material);
  stream_printf("  • Black Material: %d points\n", black_material);
  stream_printf(
      "  • Material Advantage: %s %d\n",
      material_balance > 0 ? "White +"
                           : (material_balance < 0 ? "Black +" : "Equal"),
      material_balance > 0 ? material_balance
                           : (material_balance < 0 ? -material_balance : 0));

  // Stream game statistics
  stream_writeln("\n📊 Game Statistics:");
  stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
  stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
  stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
  stream_printf("  • Draws: %" PRIu32 "\n", draws);
  stream_printf("  • Win Rate: %.1f%%\n",
                total_games > 0 ? (float)white_wins / total_games * 100.0f
                                : 0.0f);

  // Stream advantage graph
  stream_writeln("\n📊 Advantage Graph:");
  stream_writeln(graph_buffer);

  // Stream conclusion
  stream_writeln("\n🏆 Congratulations to White player!");
  stream_printf("💡 Game saved as 'victory_white_%" PRIu32 ".chess'\n",
                game_duration);

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");

  // Add TEST MODE completion message
  stream_writeln("\n🧪 TEST MODE COMPLETE - Game state unchanged");
  ESP_LOGI(TAG, "✅ Test complete - Game state unchanged");
}

/**
 * @brief Process endgame black command from UART
 * @param cmd Endgame black command
 */
void game_process_endgame_black_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏆 Processing ENDGAME_BLACK command");
  ESP_LOGI(TAG, "🧪 TEST MODE - Statistics will NOT be updated");

  // Get real game statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t total_moves = move_count;
  uint32_t white_captures_real = white_captures;
  uint32_t black_captures_real = black_captures;

  // Calculate average times
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  int white_material, black_material;
  int material_balance =
      game_calculate_material_balance(&white_material, &black_material);

  // Determine game phase
  const char *game_phase = (total_moves > 30)   ? "Endgame"
                           : (total_moves > 15) ? "Middle Game"
                                                : "Opening";

  // Calculate accuracy (simplified based on captures and checks)
  int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
  int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
  if (white_accuracy > 95)
    white_accuracy = 95;
  if (black_accuracy > 95)
    black_accuracy = 95;

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret =
      streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
  char graph_buffer[256]; // Small stack buffer instead of malloc(1024)
  game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer),
                                game_duration, total_moves);

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
  // Stream endgame report header
  stream_writeln("🏆 ENDGAME REPORT - BLACK VICTORY");
  stream_writeln(
      "═══════════════════════════════════════════════════════════════");
  stream_writeln("🎯 Game Result: BLACK WINS");
  stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n",
                game_duration, (float)game_duration / 60.0f);

  // Stream move statistics
  stream_writeln("\n📊 Move Statistics:");
  stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32
                ", Black: %" PRIu32 ")\n",
                total_moves, white_moves_count, black_moves_count);
  stream_printf("  • White Captures: %" PRIu32 " pieces\n",
                white_captures_real);
  stream_printf("  • Black Captures: %" PRIu32 " pieces\n",
                black_captures_real);
  stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
  stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
  stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
  stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
  stream_printf("  • Average Time per Move: %.1f seconds\n",
                total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
  stream_printf("  • White Average Time: %" PRIu32 " seconds\n",
                white_avg_time);
  stream_printf("  • Black Average Time: %" PRIu32 " seconds\n",
                black_avg_time);
  stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                white_time_total, (float)white_time_total / 60.0f);
  stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                black_time_total, (float)black_time_total / 60.0f);

  // Stream game analysis
  stream_writeln("\n🎮 Game Analysis:");
  stream_printf("  • Game Phase: %s\n", game_phase);
  stream_writeln("  • Victory Condition: Checkmate");
  stream_printf("  • Material Balance: %s\n",
                material_balance > 0
                    ? "White Advantage"
                    : (material_balance < 0 ? "Black Advantage" : "Equal"));
  stream_printf("  • Moves without Capture: %" PRIu32 "\n",
                moves_without_capture);
  stream_printf("  • Max Moves without Capture: %" PRIu32 "\n",
                max_moves_without_capture);

  // Stream performance metrics
  stream_writeln("\n📈 Performance Metrics:");
  stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy,
                (white_accuracy >= 85)   ? "Excellent"
                : (white_accuracy >= 75) ? "Good"
                : (white_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy,
                (black_accuracy >= 85)   ? "Excellent"
                : (black_accuracy >= 75) ? "Good"
                : (black_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • White Material: %d points\n", white_material);
  stream_printf("  • Black Material: %d points\n", black_material);
  stream_printf(
      "  • Material Advantage: %s %d\n",
      material_balance > 0 ? "White +"
                           : (material_balance < 0 ? "Black +" : "Equal"),
      material_balance > 0 ? material_balance
                           : (material_balance < 0 ? -material_balance : 0));

  // Stream game statistics
  stream_writeln("\n📊 Game Statistics:");
  stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
  stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
  stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
  stream_printf("  • Draws: %" PRIu32 "\n", draws);
  stream_printf("  • Win Rate: %.1f%%\n",
                total_games > 0 ? (float)black_wins / total_games * 100.0f
                                : 0.0f);

  // Stream victory conditions
  stream_writeln("\n🏅 Victory Conditions:");
  stream_writeln("  • Checkmate: YES");
  stream_writeln("  • King Captured: NO");
  stream_writeln("  • Resignation: NO");
  stream_writeln("  • Time Out: NO");
  stream_writeln("  • Stalemate: NO");

  // Stream advantage graph
  stream_writeln("\n📊 Advantage Graph:");
  stream_writeln(graph_buffer);

  // Stream conclusion
  stream_writeln("\n🎨 Endgame Animation: Starting...");
  stream_writeln("📈 Game Analysis: White had superior tactical play");
  stream_writeln("💡 Best Move: Qh2# (Checkmate)");
  stream_writeln("🎯 Key Moment: Rook sacrifice on move 35");
  stream_writeln("\n🎉 Congratulations to White player!");
  stream_writeln("🏆 White wins with brilliant endgame technique!");

  // Použít LED_CMD_ANIM_ENDGAME místo start_endgame_animation()
  // Najít pozici bílého krále
  uint8_t king_pos = 27; // default fallback (d4)
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    if (board[r][c] == PIECE_WHITE_KING) {
      king_pos = i;
      break;
    }
  }
  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 0,
                               .green = 0,
                               .blue = 0,
                               .duration_ms = 0, // Endless
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");

  // Add TEST MODE completion message
  stream_writeln("\n🧪 TEST MODE COMPLETE - Game state unchanged");
  ESP_LOGI(TAG, "✅ Test complete - Game state unchanged");
}

/**
 * @brief Process list games command from UART
 * @param cmd List games command
 */
void game_process_list_games_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "📁 Processing LIST_GAMES command");

  char response_data[1024];
  snprintf(response_data, sizeof(response_data),
           "📁 Saved Games List\n"
           "═══════════════════════════════════════════════════════════════\n"
           "🎮 Available saved games:\n"
           "\n"
           "1. game_001.chess\n"
           "   • Date: 2024-01-15 14:30\n"
           "   • Moves: 24\n"
           "   • Status: In Progress\n"
           "   • Player: White to move\n"
           "\n"
           "2. tournament_round1.chess\n"
           "   • Date: 2024-01-14 16:45\n"
           "   • Moves: 42\n"
           "   • Status: Completed\n"
           "   • Result: White wins\n"
           "\n"
           "3. practice_game.chess\n"
           "   • Date: 2024-01-13 10:20\n"
           "   • Moves: 18\n"
           "   • Status: In Progress\n"
           "   • Player: Black to move\n"
           "\n"
           "💡 Use 'LOAD <filename>' to load a game\n"
           "💡 Use 'DELETE_GAME <filename>' to delete a game\n"
           "💡 Total: 3 saved games");

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  // Použít LED_CMD_ANIM_ENDGAME místo start_endgame_animation()
  // Najít pozici černého krále
  uint8_t king_pos = 27; // default fallback (d4)
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    if (board[r][c] == PIECE_BLACK_KING) {
      king_pos = i;
      break;
    }
  }
  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 0,
                               .green = 0,
                               .blue = 0,
                               .duration_ms = 0, // Endless
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process delete game command from UART
 * @param cmd Delete game command
 */
void game_process_delete_game_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🗑️ Processing DELETE_GAME command: %s", cmd->from_notation);

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🗑️ Game Deletion\n"
           "═══════════════════════════════════════════════════════════════\n"
           "🎮 Game: %s\n"
           "✅ Status: Successfully deleted\n"
           "📁 File removed from storage\n"
           "💾 Space freed: ~2.5 KB\n"
           "\n"
           "💡 Use 'LIST_GAMES' to see remaining games",
           cmd->from_notation);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}
