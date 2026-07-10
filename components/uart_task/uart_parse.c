/**
 * @file uart_parse.c
 * @brief Command lookup, move notation parsing, and dispatch.
 */

#include "uart_parse.h"
#include "uart_commands_table.h"
#include "uart_task_internal.h"

#include "uart_task.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <ctype.h>
#include <string.h>

static const char *TAG = "UART_PARSE";

bool parse_move_notation(const char *input, char *from, char *to) {
  if (!input || !from || !to)
    return false;

  // Remove leading/trailing whitespace
  while (*input == ' ' || *input == '\t')
    input++;

  // Support formats: "e2 e4", "e2-e4", "e2e4", "E2 E4", "E2-E4", "E2E4"
  if (strlen(input) < 4)
    return false;

  // Format: "e2 e4" or "E2 E4" (space separated)
  if (strchr(input, ' ')) {
    char *space = strchr(input, ' ');
    size_t from_len = space - input;
    if (from_len != 2)
      return false;

    strncpy(from, input, 2);
    from[2] = '\0';
    from[0] = tolower(from[0]); // Convert to lowercase

    // Skip whitespace
    while (*space == ' ' || *space == '\t')
      space++;
    if (strlen(space) != 2)
      return false;

    strncpy(to, space, 2);
    to[2] = '\0';
    to[0] = tolower(to[0]); // Convert to lowercase

    return true;
  }

  // Format: "e2-e4" or "E2-E4" (dash separated)
  if (strchr(input, '-')) {
    char *dash = strchr(input, '-');
    size_t from_len = dash - input;
    if (from_len != 2)
      return false;

    strncpy(from, input, 2);
    from[2] = '\0';
    from[0] = tolower(from[0]); // Convert to lowercase

    if (strlen(dash + 1) != 2)
      return false;

    strncpy(to, dash + 1, 2);
    to[2] = '\0';
    to[0] = tolower(to[0]); // Convert to lowercase

    return true;
  }

  // Format: "e2e4" or "E2E4" (compact)
  if (strlen(input) == 4) {
    strncpy(from, input, 2);
    from[2] = '\0';
    from[0] = tolower(from[0]); // Convert to lowercase
    strncpy(to, input + 2, 2);
    to[2] = '\0';
    to[0] = tolower(to[0]); // Convert to lowercase

    return true;
  }

  return false;
}

bool validate_chess_squares(const char *from, const char *to) {
  if (!from || !to)
    return false;

  // Check format: [a-h][1-8]
  if (strlen(from) != 2 || strlen(to) != 2)
    return false;

  // Validate file (column) - case-insensitive
  char from_file = tolower(from[0]);
  char to_file = tolower(to[0]);
  if (from_file < 'a' || from_file > 'h' || to_file < 'a' || to_file > 'h') {
    return false;
  }

  // Validate rank (row)
  if (from[1] < '1' || from[1] > '8' || to[1] < '1' || to[1] > '8') {
    return false;
  }

  // Check that from and to are different
  if (strcmp(from, to) == 0)
    return false;

  return true;
}

bool send_to_game_task(const chess_move_command_t *move_cmd) {
  if (!move_cmd)
    return false;

  // Validate queue availability
  if (game_command_queue == NULL) {
    uart_send_error("Internal error: game command queue unavailable");
    return false;
  }

  // Send command to game task via queue with timeout
  if (xQueueSend(game_command_queue, move_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "Move command sent: %s -> %s (player: %d)",
             move_cmd->from_notation, move_cmd->to_notation, move_cmd->player);
    return true;
  } else {
    uart_send_error("Failed to send move command to game engine (queue full)");
    return false;
  }
}

/**
 * @brief Send command to game task and wait for response
 * @param move_cmd Command to send
 * @param response_buffer Buffer to store response message
 * @param buffer_size Size of response buffer
 * @param timeout_ms Timeout in milliseconds
 * @return true if command sent and response received successfully
 */
bool send_to_game_task_with_response(const chess_move_command_t *move_cmd,
                                     char *response_buffer, size_t buffer_size,
                                     uint32_t timeout_ms) {
  if (!move_cmd || !response_buffer)
    return false;

  // Validate queue availability
  if (game_command_queue == NULL) {
    uart_send_error("Internal error: game command queue unavailable");
    return false;
  }

  if (uart_response_queue == NULL) {
    uart_send_error("Internal error: response queue unavailable");
    return false;
  }

  // Send command to game task via queue with timeout
  if (xQueueSend(game_command_queue, move_cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "Move command sent: %s -> %s (player: %d)",
             move_cmd->from_notation, move_cmd->to_notation, move_cmd->player);

    // Wait for response from game task
    game_response_t response;
    if (xQueueReceive(uart_response_queue, &response,
                      pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
      ESP_LOGI(TAG, "Response received: %s", response.data);
      strncpy(response_buffer, response.data, buffer_size - 1);
      response_buffer[buffer_size - 1] = '\0';
      return true;
    } else {
      uart_send_error("Timeout waiting for game task response");
      return false;
    }
  } else {
    uart_send_error("Failed to send move command to game engine (queue full)");
    return false;
  }
}

/**
 * @brief Validate chess move notation (e.g., "e2e4")
 */
bool is_valid_move_notation(const char *move) {
  if (strlen(move) != 4)
    return false;

  // Check format: [a-h][1-8][a-h][1-8]
  if (move[0] < 'a' || move[0] > 'h')
    return false;
  if (move[1] < '1' || move[1] > '8')
    return false;
  if (move[2] < 'a' || move[2] > 'h')
    return false;
  if (move[3] < '1' || move[3] > '8')
    return false;

  return true;
}

/**
 * @brief Validate chess square notation (e.g., "e2")
 */
bool is_valid_square_notation(const char *square) {
  if (strlen(square) != 2)
    return false;

  // Check format: [a-h][1-8]
  if (square[0] < 'a' || square[0] > 'h')
    return false;
  if (square[1] < '1' || square[1] > '8')
    return false;

  return true;
}

// ============================================================================
// COMMAND IMPLEMENTATIONS (Duplicates removed - using existing implementations)
// ============================================================================

// ============================================================================
// GAME COMMAND IMPLEMENTATIONS (Duplicates removed - using existing
// implementations)
// ============================================================================
// COMMAND PARSING AND EXECUTION
// ============================================================================

const uart_command_t *find_command(const char *command) {
  if (command == NULL)
    return NULL;

  // Convert input to uppercase for case-insensitive comparison
  char cmd_upper[64];
  strncpy(cmd_upper, command, sizeof(cmd_upper) - 1);
  cmd_upper[sizeof(cmd_upper) - 1] = '\0';

  for (int i = 0; cmd_upper[i]; i++) {
    cmd_upper[i] = toupper(cmd_upper[i]);
  }

  for (int i = 0; uart_commands[i].name != NULL; i++) {
    if (strcmp(uart_commands[i].name, cmd_upper) == 0) {
      return &uart_commands[i];
    }

    // Check aliases (also case-insensitive)
    for (int j = 0; j < 5 && uart_commands[i].aliases[j][0] != '\0'; j++) {
      if (strcmp(uart_commands[i].aliases[j], cmd_upper) == 0) {
        return &uart_commands[i];
      }
    }
  }

  return NULL;
}

command_result_t execute_command(const char *command, const char *args) {
  if (command == NULL) {
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Find command (case-insensitive)
  const uart_command_t *cmd = find_command(command);
  if (cmd == NULL) {
    // Check if it's a direct move (like "e2e4")
    if (strlen(command) == 4 && is_valid_move_notation(command)) {
      ESP_LOGI(TAG, "Processing direct move: %s", command);

      // Create move command
      chess_move_command_t move_cmd = {.type = GAME_CMD_MAKE_MOVE,
                                       .player = 0, // White to move (default)
                                       .response_queue = 0};

      // Copy move notation (e.g., "e2e4" -> from="e2", to="e4")
      strncpy(move_cmd.from_notation, command, 2);
      move_cmd.from_notation[2] = '\0';
      strncpy(move_cmd.to_notation, command + 2, 2);
      move_cmd.to_notation[2] = '\0';

      // Send to game task
      if (send_to_game_task(&move_cmd)) {
        uart_send_formatted("Move requested: %s → %s", move_cmd.from_notation,
                            move_cmd.to_notation);
        uart_send_formatted("Move sent to game engine for validation");
        command_count++;
        return CMD_SUCCESS;
      } else {
        uart_send_error("Internal error: failed to send move to game engine");
        return CMD_ERROR_SYSTEM_ERROR;
      }
    }

    uart_send_error("❌ Unknown command");
    uart_send_formatted("Command '%s' not found", command);
    uart_send_line("Type 'HELP' for available commands");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Check if args are required
  if (cmd->requires_args && (args == NULL || strlen(args) == 0)) {
    uart_send_error("❌ Missing arguments");
    uart_send_formatted("Usage: %s", cmd->usage);
    return CMD_ERROR_INVALID_SYNTAX;
  }

  // Execute command
  ESP_LOGI(TAG, "Executing command: %s with args: %s", cmd->name,
           args ? args : "none");

  command_result_t result = cmd->handler(args);

  if (result == CMD_SUCCESS) {
    command_count++;
    last_command_time = esp_timer_get_time() / 1000;
  } else {
    error_count++;
    ESP_LOGE(TAG, "Command '%s' failed with result: %d", cmd->name, result);
  }

  return result;
}

void uart_parse_command(const char *input) {
  if (input == NULL || strlen(input) == 0)
    return;

  // Skip leading whitespace
  while (*input == ' ' || *input == '\t')
    input++;

  // Find command and arguments
  char command[64] = {0};
  char args[640] = {0};

  const char *space = strchr(input, ' ');
  if (space != NULL) {
    // Command has arguments
    size_t cmd_len = space - input;
    strncpy(command, input, cmd_len);
    command[cmd_len] = '\0';

    // Skip whitespace after command
    while (*space == ' ' || *space == '\t')
      space++;
    strncpy(args, space, sizeof(args) - 1);
    args[sizeof(args) - 1] = '\0';
  } else {
    // Command without arguments
    strncpy(command, input, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';
  }

  // Execute command
  execute_command(command, args);
}
