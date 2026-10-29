/**
 * @file web_resign_dispatch.c
 * @brief App resignation for HTTP (POST /api/game/resign) and BLE ({"cmd":"resign"}).
 *
 * Body (both transports): optional {"side":"white"|"black"}. Without "side"
 * the side on move resigns. With "side" the board refuses when the other
 * side is on move, so a stale app snapshot cannot resign the wrong colour.
 * The game itself ends in game_task (GAME_CMD_RESIGN → game_resign_from_app),
 * the same path as the 10 s king-lift resignation.
 */

#include "cJSON.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "web_server_internal.h"
#include "web_server_task.h"
#include "cmd_wire.h"
#include "../game_task/include/game_task.h"

#include "esp_log.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

extern QueueHandle_t game_command_queue;

static const char *TAG = "WEB_RESIGN";

static const char *resign_error_code(game_resign_result_t r) {
  switch (r) {
  case GAME_RESIGN_ERR_NOT_RUNNING:
    return "not_running";
  case GAME_RESIGN_ERR_MODE:
    return "mode_active";
  case GAME_RESIGN_ERR_SIDE:
    return "side_mismatch";
  default:
    return "unknown";
  }
}

static esp_err_t resign_refuse(const char *err_code, const char **out_error) {
  ESP_LOGW(TAG, "resign refused: %s", err_code);
  if (out_error != NULL) {
    *out_error = err_code;
  }
  return ESP_ERR_INVALID_STATE;
}

esp_err_t web_server_resign_dispatch_body(const char *json,
                                          const char **out_error) {
  uint8_t side = GAME_RESIGN_SIDE_TO_MOVE;

  if (web_is_locked()) {
    return resign_refuse("web_locked", out_error);
  }

  if (json != NULL && json[0] != '\0') {
    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
      if (out_error != NULL) {
        *out_error = "invalid_json";
      }
      ESP_LOGW(TAG, "resign: invalid JSON body");
      return ESP_ERR_INVALID_ARG;
    }
    cJSON *side_j = cJSON_GetObjectItemCaseSensitive(root, "side");
    if (cJSON_IsString(side_j) && side_j->valuestring != NULL) {
      if (strcasecmp(side_j->valuestring, "white") == 0) {
        side = GAME_RESIGN_SIDE_WHITE;
      } else if (strcasecmp(side_j->valuestring, "black") == 0) {
        side = GAME_RESIGN_SIDE_BLACK;
      } else {
        cJSON_Delete(root);
        if (out_error != NULL) {
          *out_error = "invalid_side";
        }
        ESP_LOGW(TAG, "resign: invalid side");
        return ESP_ERR_INVALID_ARG;
      }
    }
    cJSON_Delete(root);
  }

  game_resign_result_t check = game_resign_check(side);
  ESP_LOGD(TAG, "[STAGING] resign request: side=%u state=%d on_move=%d check=%d",
           (unsigned)side, (int)game_get_state(),
           (int)game_get_current_player(), (int)check);
  if (check != GAME_RESIGN_OK) {
    return resign_refuse(resign_error_code(check), out_error);
  }

  if (game_command_queue == NULL) {
    if (out_error != NULL) {
      *out_error = "queue_unavailable";
    }
    return ESP_FAIL;
  }

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_RESIGN;
  cmd.u.resign.side = side;
  if (!game_submit(&cmd)) {
    ESP_LOGE(TAG, "resign: failed to send to game queue");
    if (out_error != NULL) {
      *out_error = "busy";
    }
    return ESP_ERR_TIMEOUT;
  }

  ESP_LOGI(TAG, "resign: queued (side=%u)", (unsigned)side);
  return ESP_OK;
}

esp_err_t web_post_game_resign_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/resign");

  char body[160] = {0};
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r > 0) {
    body[r] = '\0';
  } else {
    body[0] = '\0';
  }
  if (cmd_wire_http_guard(req, body, "resign")) {
    return ESP_OK;
  }

  const char *err_code = NULL;
  esp_err_t err = web_server_resign_dispatch_body(body, &err_code);

  if (err == ESP_OK) {
    cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                       "resign");
    return ESP_OK;
  }
  if (err == ESP_ERR_TIMEOUT) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }

  const char *status = "500 Internal Server Error";
  if (err == ESP_ERR_INVALID_ARG) {
    status = "400 Bad Request";
  } else if (err == ESP_ERR_INVALID_STATE) {
    status = (err_code != NULL && strcmp(err_code, "web_locked") == 0)
                 ? "403 Forbidden"
                 : "409 Conflict";
  }
  char out[96];
  snprintf(out, sizeof(out), "{\"success\":false,\"error\":\"%s\"}",
           err_code != NULL ? err_code : "unknown");
  cmd_wire_http_send(req, status, out, body, false, NULL);
  return ESP_OK;
}

esp_err_t web_server_draw_dispatch(const char **out_error) {
  if (web_is_locked()) {
    return resign_refuse("web_locked", out_error);
  }
  game_resign_result_t check = game_resign_check(GAME_RESIGN_SIDE_TO_MOVE);
  if (check != GAME_RESIGN_OK) {
    return resign_refuse(resign_error_code(check), out_error);
  }
  if (game_command_queue == NULL) {
    if (out_error != NULL) {
      *out_error = "queue_unavailable";
    }
    return ESP_FAIL;
  }
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_AGREE_DRAW;
  if (!game_submit(&cmd)) {
    if (out_error != NULL) {
      *out_error = "busy";
    }
    return ESP_ERR_TIMEOUT;
  }
  ESP_LOGI(TAG, "draw: queued");
  return ESP_OK;
}

esp_err_t web_post_game_draw_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/draw");
  const char *err_code = NULL;
  esp_err_t err = web_server_draw_dispatch(&err_code);
  httpd_resp_set_type(req, "application/json");
  if (err == ESP_OK) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_send(req, "{\"success\":true,\"message\":\"Draw agreed\"}", -1);
    return ESP_OK;
  }
  const char *status = "500 Internal Server Error";
  if (err == ESP_ERR_INVALID_ARG) {
    status = "400 Bad Request";
  } else if (err == ESP_ERR_INVALID_STATE) {
    status = (err_code != NULL && strcmp(err_code, "web_locked") == 0)
                 ? "403 Forbidden"
                 : "409 Conflict";
  }
  char out[96];
  snprintf(out, sizeof(out), "{\"success\":false,\"error\":\"%s\"}",
           err_code != NULL ? err_code : "unknown");
  httpd_resp_set_status(req, status);
  httpd_resp_send(req, out, -1);
  return ESP_OK;
}
