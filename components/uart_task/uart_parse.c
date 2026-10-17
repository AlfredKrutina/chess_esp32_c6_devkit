/**
 * @file uart_parse.c
 * @brief Command lookup and dispatch. The command table is the whole console.
 */

#include "uart_parse.h"
#include "uart_commands_table.h"
#include "uart_task_internal.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <ctype.h>
#include <string.h>

static const char *TAG = "UART_PARSE";

const uart_command_t *find_command(const char *command) {
  if (command == NULL)
    return NULL;

  char cmd_upper[64];
  strncpy(cmd_upper, command, sizeof(cmd_upper) - 1);
  cmd_upper[sizeof(cmd_upper) - 1] = '\0';

  for (int i = 0; cmd_upper[i]; i++) {
    cmd_upper[i] = (char)toupper((unsigned char)cmd_upper[i]);
  }

  for (int i = 0; uart_commands[i].name != NULL; i++) {
    if (strcmp(uart_commands[i].name, cmd_upper) == 0) {
      return &uart_commands[i];
    }
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

  const uart_command_t *cmd = find_command(command);
  if (cmd == NULL) {
    uart_send_error("Unknown command");
    uart_send_formatted("Command '%s' not found", command);
    uart_send_line("Type HELP for available commands");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  if (cmd->requires_args && (args == NULL || strlen(args) == 0)) {
    uart_send_error("Missing arguments");
    uart_send_formatted("Usage: %s", cmd->usage);
    return CMD_ERROR_INVALID_SYNTAX;
  }

  ESP_LOGI(TAG, "Executing command: %s", cmd->name);
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

  while (*input == ' ' || *input == '\t')
    input++;

  char command[64] = {0};
  char args[640] = {0};

  const char *space = strchr(input, ' ');
  if (space != NULL) {
    size_t cmd_len = (size_t)(space - input);
    if (cmd_len >= sizeof(command))
      cmd_len = sizeof(command) - 1;
    memcpy(command, input, cmd_len);
    command[cmd_len] = '\0';
    while (*space == ' ' || *space == '\t')
      space++;
    strncpy(args, space, sizeof(args) - 1);
  } else {
    strncpy(command, input, sizeof(command) - 1);
  }

  execute_command(command, args);
}
