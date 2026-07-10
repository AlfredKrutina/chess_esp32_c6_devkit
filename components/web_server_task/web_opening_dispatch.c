/**
 * @file web_opening_dispatch.c
 * @brief Shared opening trainer JSON dispatch for HTTP and BLE.
 */

#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "web_server_internal.h"
#include "../game_task/include/game_task.h"
#include "../led_task/include/led_task.h"

#include "esp_log.h"

#include <string.h>

extern QueueHandle_t game_command_queue;

static const char *TAG = "WEB_OPENING";

static uint8_t opening_action_code(const char *action) {
  if (action == NULL) {
    return 255;
  }
  if (strcmp(action, "cancel") == 0) {
    return 0;
  }
  if (strcmp(action, "start") == 0) {
    return 1;
  }
  if (strcmp(action, "hint") == 0) {
    return 2;
  }
  if (strcmp(action, "checkpoint_ack") == 0) {
    return 3;
  }
  return 255;
}

static esp_err_t opening_load_start_config(cJSON *root) {
  cJSON *line_id_j = cJSON_GetObjectItemCaseSensitive(root, "line_id");
  cJSON *fen_j = cJSON_GetObjectItemCaseSensitive(root, "start_fen");
  cJSON *uci_j = cJSON_GetObjectItemCaseSensitive(root, "line_uci");
  cJSON *player_j = cJSON_GetObjectItemCaseSensitive(root, "player_ply_indices");
  cJSON *checkpoint_j =
      cJSON_GetObjectItemCaseSensitive(root, "checkpoint_ply_indices");
  cJSON *mode_j = cJSON_GetObjectItemCaseSensitive(root, "mode");
  cJSON *side_j = cJSON_GetObjectItemCaseSensitive(root, "side");
  cJSON *opponent_j = cJSON_GetObjectItemCaseSensitive(root, "opponent_mode");

  if (!cJSON_IsString(line_id_j) || !cJSON_IsString(fen_j) ||
      !cJSON_IsArray(uci_j) || !cJSON_IsArray(player_j)) {
    return ESP_ERR_INVALID_ARG;
  }

  int uci_count = cJSON_GetArraySize(uci_j);
  int player_count = cJSON_GetArraySize(player_j);
  if (uci_count <= 0 || uci_count > 16 || player_count <= 0 ||
      player_count > 8) {
    return ESP_ERR_INVALID_ARG;
  }

  char line_uci[16][6];
  uint8_t player_idx[8];
  uint8_t checkpoint_idx[4];
  uint8_t checkpoint_count = 0;

  for (int i = 0; i < uci_count; i++) {
    cJSON *item = cJSON_GetArrayItem(uci_j, i);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
      return ESP_ERR_INVALID_ARG;
    }
    strncpy(line_uci[i], item->valuestring, sizeof(line_uci[i]) - 1);
  }
  for (int i = 0; i < player_count; i++) {
    cJSON *item = cJSON_GetArrayItem(player_j, i);
    if (!cJSON_IsNumber(item)) {
      return ESP_ERR_INVALID_ARG;
    }
    player_idx[i] = (uint8_t)item->valueint;
  }
  if (cJSON_IsArray(checkpoint_j)) {
    int cp_count = cJSON_GetArraySize(checkpoint_j);
    if (cp_count > 4) {
      cp_count = 4;
    }
    for (int i = 0; i < cp_count; i++) {
      cJSON *item = cJSON_GetArrayItem(checkpoint_j, i);
      if (cJSON_IsNumber(item)) {
        checkpoint_idx[checkpoint_count++] = (uint8_t)item->valueint;
      }
    }
  }

  uint8_t mode = 0;
  if (cJSON_IsString(mode_j) && mode_j->valuestring != NULL) {
    if (strcmp(mode_j->valuestring, "drill") == 0) {
      mode = 1;
    } else if (strcmp(mode_j->valuestring, "timed") == 0) {
      mode = 2;
    } else if (strcmp(mode_j->valuestring, "mirror") == 0) {
      mode = 3;
    }
  }
  bool side_white = true;
  if (cJSON_IsString(side_j) && side_j->valuestring != NULL) {
    side_white = (strcmp(side_j->valuestring, "white") == 0);
  }
  uint8_t opponent_mode = 0;
  if (cJSON_IsString(opponent_j) && opponent_j->valuestring != NULL) {
    if (strcmp(opponent_j->valuestring, "physical") == 0) {
      opponent_mode = 1;
    }
  }

  if (!game_opening_load_config(line_id_j->valuestring, fen_j->valuestring,
                                line_uci, (uint8_t)uci_count, player_idx,
                                (uint8_t)player_count, checkpoint_idx,
                                checkpoint_count, mode, side_white,
                                opponent_mode)) {
    return ESP_ERR_INVALID_ARG;
  }
  return ESP_OK;
}

static esp_err_t opening_queue_action(uint8_t action_code) {
  if (game_command_queue == NULL) {
    ESP_LOGE(TAG, "game_command_queue NULL");
    return ESP_FAIL;
  }
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_OPENING_TRAINER;
  cmd.promotion_choice = action_code;
  cmd.response_queue = NULL;
  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "queue send failed action=%u", (unsigned)action_code);
    return ESP_FAIL;
  }
  if (action_code == 0) {
    led_command_t lcmd = {0};
    lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&lcmd);
  }
  return ESP_OK;
}

/**
 * Dispatch opening trainer command from JSON body (HTTP or BLE).
 * Expects `action` field; start also requires line payload fields.
 */
esp_err_t web_server_opening_dispatch_body(const char *json) {
  if (json == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  cJSON *root = cJSON_Parse(json);
  if (root == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = web_server_opening_dispatch_json(root);
  cJSON_Delete(root);
  return err;
}

esp_err_t web_server_opening_dispatch_json(cJSON *root) {
  if (root == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (web_is_locked()) {
    return ESP_ERR_INVALID_STATE;
  }

  cJSON *action_j = cJSON_GetObjectItemCaseSensitive(root, "action");
  if (!cJSON_IsString(action_j) || action_j->valuestring == NULL) {
    return ESP_ERR_NOT_FOUND;
  }

  uint8_t action_code = opening_action_code(action_j->valuestring);
  if (action_code == 255) {
    return ESP_ERR_INVALID_ARG;
  }

  if (action_code == 1) {
    esp_err_t load_err = opening_load_start_config(root);
    if (load_err != ESP_OK) {
      return load_err;
    }
    if (!game_opening_physical_matches_start()) {
      if (!game_opening_enter_setup_phase()) {
        return ESP_ERR_INVALID_ARG;
      }
      ESP_LOGI(TAG, "opening physical mismatch — setup_phase, start not queued");
      return ESP_OK;
    }
  }

  if (action_code == 2) {
    if (!game_opening_hint()) {
      return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
  }

  if (action_code == 3) {
    if (!game_opening_checkpoint_ack()) {
      return ESP_ERR_NOT_ALLOWED;
    }
    return ESP_OK;
  }

  return opening_queue_action(action_code);
}
