/**
 * @file web_handlers_game.c
 * @brief Game-related HTTP handlers and snapshot JSON builders.
 */

#include "web_server_task.h"
#include "web_server_internal.h"
#include "../game_task/include/game_task.h"
#include "../matrix_task/include/matrix_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "../config_manager/include/config_manager.h"

#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "esp_http_server.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

extern QueueHandle_t game_command_queue;

static const char *TAG = "WEB_GAME";


char json_buffer[JSON_BUFFER_SIZE];
char snapshot_buffer[SNAPSHOT_BUFFER_SIZE];
SemaphoreHandle_t snapshot_build_mutex;

/** Po konfliktu při finish setup tutoriálu krátce nevolat znovu validaci (BLE + HTTP). */
static TickType_t s_setup_tutorial_finish_conflict_until_tick;

void setup_tutorial_reset_finish_cooldown(void) {
  s_setup_tutorial_finish_conflict_until_tick = 0;
}

bool setup_tutorial_finish_in_cooldown(void) {
  if (s_setup_tutorial_finish_conflict_until_tick == 0) {
    return false;
  }
  return xTaskGetTickCount() < s_setup_tutorial_finish_conflict_until_tick;
}

void setup_tutorial_note_finish_conflict(void) {
  s_setup_tutorial_finish_conflict_until_tick =
      xTaskGetTickCount() + pdMS_TO_TICKS(900);
}
/** Doplnění GET /api/status o web lock, WiFi, jas, matrix guard, lampu (sdílené se snapshot). */
void inject_web_status_fields(char *buf, size_t buf_size) {
  char *last_brace = strrchr(buf, '}');
  if (!last_brace) {
    ESP_LOGW(TAG, "[STAGING] inject_web_status_fields: no closing brace in buf");
    return;
  }
  size_t remaining = buf_size - (size_t)(last_brace - buf);
  /* První blok ~370 B (+ chess_hint_limit) + druhý ~120 B — při těsném bufferu raději neposlat useknutý JSON. */
  if (remaining < 430) {
    ESP_LOGE(TAG,
             "[STAGING] inject_web_status_fields: remaining=%zu B too small for "
             "web fields (cap=%zu)",
             remaining, buf_size);
    return;
  }
  uint8_t b = cached_brightness_valid ? cached_brightness : 50;
  int hint_lim = config_ui_prefs_get_chess_hint_limit();
  int wr =
      snprintf(last_brace, remaining,
               ",\"web_locked\":%s,\"internet_connected\":%s,\"brightness\":%d,"
               "\"guided_capture_hints_enabled\":%s,\"led_guidance_level\":%u,"
               "\"matrix_guard_active\":%s,"
               "\"matrix_guard_conflicts\":%u,"
               "\"matrix_guard_lifted_low\":%lu,\"matrix_guard_lifted_high\":%lu,"
               "\"matrix_guard_dropped_low\":%lu,\"matrix_guard_dropped_high\":%lu,"
               "\"chess_hint_limit\":%d}",
               web_is_locked() ? "true" : "false",
               wifi_is_sta_connected() ? "true" : "false", (int)b,
               game_get_guided_capture_hints_enabled() ? "true" : "false",
               (unsigned)game_get_led_guidance_level(),
               game_is_matrix_guard_active() ? "true" : "false",
               (unsigned int)game_get_matrix_guard_conflict_count(),
               (unsigned long)game_get_matrix_guard_lifted_mask_low(),
               (unsigned long)game_get_matrix_guard_lifted_mask_high(),
               (unsigned long)game_get_matrix_guard_dropped_mask_low(),
               (unsigned long)game_get_matrix_guard_dropped_mask_high(),
               hint_lim);
  if (wr < 0 || (size_t)wr >= remaining) {
    ESP_LOGE(TAG,
             "[STAGING] inject_web_status_fields: first snprintf truncated "
             "wr=%d rem=%zu",
             wr, remaining);
    return;
  }
  if (remaining >= 150) {
    last_brace = strrchr(buf, '}');
    if (last_brace) {
      remaining = buf_size - (size_t)(last_brace - buf);
      if (remaining < 130) {
        ESP_LOGW(TAG,
                 "[STAGING] inject_web_status_fields: skip lamp fields "
                 "remaining=%zu",
                 remaining);
        return;
      }
      ha_mode_t light_mode = ha_light_get_mode();
      uint8_t lr = 255, lg = 255, lb = 255, lbright = 255;
      bool lstate = true;
      ha_light_get_state(&lr, &lg, &lb, &lbright, &lstate);
      uint32_t auto_sec = ha_light_get_activity_timeout_sec();
      wr = snprintf(last_brace, remaining,
                    ",\"light_mode\":\"%s\",\"light_state\":%s,\"light_r\":%u,"
                    "\"light_g\":%u,\"light_b\":%u,\"auto_lamp_timeout_sec\":%lu}",
                    (light_mode == HA_MODE_HA) ? "lamp" : "game",
                    lstate ? "true" : "false", (unsigned)lr, (unsigned)lg,
                    (unsigned)lb, (unsigned long)auto_sec);
      if (wr < 0 || (size_t)wr >= remaining) {
        ESP_LOGE(TAG,
                 "[STAGING] inject_web_status_fields: lamp snprintf truncated "
                 "wr=%d rem=%zu",
                 wr, remaining);
      }
    }
  }
}

void snapshot_build_mutex_take(void) {
  if (snapshot_build_mutex == NULL) {
    snapshot_build_mutex = xSemaphoreCreateMutex();
  }
  /* portMAX_DELAY zde zablokoval web_server_task na mutexu (HTTP httpd drží build)
   * → bez esp_task_wdt_reset() TWDT timeout (~10 s). Čekáme po krocích + reset. */
  while (xSemaphoreTake(snapshot_build_mutex, pdMS_TO_TICKS(400)) != pdTRUE) {
    (void)web_server_task_wdt_reset_safe();
  }
}

void snapshot_build_mutex_give(void) {
  if (snapshot_build_mutex != NULL) {
    xSemaphoreGive(snapshot_build_mutex);
  }
}

/** Pod `snapshot_build_mutex` — žádný paralelní build; šetří ~1 KiB stacku na NimBLE host task. */
static char s_clock_json_build_scratch[TIMER_HTTP_JSON_MAX];

/** Stejný JSON jako GET /api/game/snapshot — do bufferu `out` (HTTP i BLE). */
static esp_err_t build_snapshot_json_to_buffer(char *out, size_t cap,
                                               size_t *out_len) {
  snapshot_build_mutex_take();
  (void)web_server_task_wdt_reset_safe();
  esp_err_t ret = game_get_board_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  size_t L = strlen(json_buffer);
  if (L < 4 || json_buffer[0] != '{' || json_buffer[L - 1] != '}') {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  json_buffer[L - 1] = '\0';
  uint32_t srev = game_get_state_revision();
  size_t pos = (size_t)snprintf(out, cap, "{\"state_version\":%" PRIu32 ",%s",
                                srev, json_buffer + 1);
  if (pos >= cap) {
    json_buffer[L - 1] = '}';
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }

  ret = game_get_status_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
#ifndef NDEBUG
  ESP_LOGI(TAG,
           "[STAGING] build_snapshot: status raw len=%zu / cap=%zu",
           strlen(json_buffer), sizeof(json_buffer));
#endif
  inject_web_status_fields(json_buffer, sizeof(json_buffer));
#ifndef NDEBUG
  ESP_LOGI(TAG,
           "[STAGING] build_snapshot: status after inject len=%zu / cap=%zu",
           strlen(json_buffer), sizeof(json_buffer));
#endif
  int n = snprintf(out + pos, cap - pos, ",\"status\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_history_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"history\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_captured_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"captured\":%s", json_buffer);
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_timer_json(s_clock_json_build_scratch,
                            sizeof(s_clock_json_build_scratch));
  if (ret == ESP_OK) {
    n = snprintf(out + pos, cap - pos, ",\"clock\":%s}",
                 s_clock_json_build_scratch);
  } else {
#ifndef NDEBUG
    ESP_LOGW(TAG, "build_snapshot: game_get_timer_json failed: %s",
             esp_err_to_name(ret));
#endif
    n = snprintf(out + pos, cap - pos, "}");
  }
  if (n < 0 || (size_t)n >= cap - pos) {
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  *out_len = pos;
  snapshot_build_mutex_give();
  return ESP_OK;
}

esp_err_t build_snapshot_json(size_t *out_len) {
  return build_snapshot_json_to_buffer(snapshot_buffer, sizeof(snapshot_buffer),
                                       out_len);
}

esp_err_t web_server_build_game_snapshot_json(char *out, size_t cap,
                                              size_t *out_len) {
  if (out == NULL || out_len == NULL || cap == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  return build_snapshot_json_to_buffer(out, cap, out_len);
}

esp_err_t web_server_build_game_snapshot_json_shared(char **out_json,
                                                     size_t *out_len) {
  if (out_json == NULL || out_len == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t e =
      build_snapshot_json_to_buffer(snapshot_buffer, sizeof(snapshot_buffer),
                                    out_len);
  if (e == ESP_OK) {
    *out_json = snapshot_buffer;
  }
  return e;
}

/** Společná logika POST /api/game/hint_highlight a BLE příkazu hint_highlight. */
esp_err_t web_server_apply_hint_highlight_json_body(const char *buf) {
  if (buf == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  const char *p_to = strstr(buf, "\"to\"");
  if (!p_to) {
    return ESP_ERR_INVALID_ARG;
  }
  p_to += 4;
  while (*p_to && *p_to != '"')
    p_to++;
  if (*p_to == '"')
    p_to++;
  if (!*p_to || !p_to[1]) {
    return ESP_ERR_INVALID_ARG;
  }
  char to_sq[3] = {0};
  to_sq[0] = (char)((unsigned char)p_to[0] <= 'Z' ? p_to[0] + 32 : p_to[0]);
  to_sq[1] = p_to[1];
  if (to_sq[0] < 'a' || to_sq[0] > 'h' || to_sq[1] < '1' || to_sq[1] > '8') {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t to_led = chess_notation_to_led_index(to_sq);
  if (to_led >= 64) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t from_led = 64;
  const char *p_from = strstr(buf, "\"from\"");
  if (p_from) {
    p_from += 6;
    while (*p_from && *p_from != '"')
      p_from++;
    if (*p_from == '"')
      p_from++;
    if (*p_from && p_from[1]) {
      char from_sq[3] = {0};
      from_sq[0] =
          (char)((unsigned char)p_from[0] <= 'Z' ? p_from[0] + 32 : p_from[0]);
      from_sq[1] = p_from[1];
      if (from_sq[0] >= 'a' && from_sq[0] <= 'h' && from_sq[1] >= '1' &&
          from_sq[1] <= '8') {
        uint8_t fl = chess_notation_to_led_index(from_sq);
        if (fl < 64)
          from_led = fl;
      }
    }
  }

  led_command_t cmd = {0};
  cmd.type = LED_CMD_HIGHLIGHT_HINT;
  cmd.led_index = from_led;
  cmd.data = (void *)&to_led;
  led_execute_command_new(&cmd);
  ESP_LOGI(TAG, "Hint highlight: %s (LED %u -> %u)", to_sq, (unsigned)from_led,
           (unsigned)to_led);
  return ESP_OK;
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
esp_err_t http_get_board_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/board");

  // Ziskat stav sachovnice z game tasku
  esp_err_t ret = game_get_board_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get board state", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

esp_err_t http_get_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/status");

  // Ziskat stav hry z game tasku
  esp_err_t ret = game_get_status_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get game status", -1);
    return ESP_FAIL;
  }

  inject_web_status_fields(json_buffer, sizeof(json_buffer));

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

esp_err_t http_get_history_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/history");

  // Ziskat historii tahu z game tasku
  esp_err_t ret = game_get_history_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get move history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

esp_err_t http_get_captured_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/captured");

  // Ziskat sebrane figurky z game tasku
  esp_err_t ret = game_get_captured_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get captured pieces", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}

esp_err_t http_get_game_snapshot_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/game/snapshot");
  uint32_t rev = game_get_state_revision();
  char etag[24];
  snprintf(etag, sizeof(etag), "%" PRIu32, rev);

  char inm[64];
  if (httpd_req_get_hdr_value_str(req, "If-None-Match", inm, sizeof(inm)) ==
      ESP_OK) {
    if (strcmp(inm, etag) == 0) {
      httpd_resp_set_status(req, "304 Not Modified");
      httpd_resp_send(req, NULL, 0);
      return ESP_OK;
    }
  }

  size_t pos = 0;
  esp_err_t ret = build_snapshot_json(&pos);
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to build game snapshot", -1);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "application/json");
  if (httpd_resp_set_hdr(req, "ETag", etag) != ESP_OK) {
    ESP_LOGD(TAG, "ETag header not set");
  }
  httpd_resp_send(req, snapshot_buffer, pos);
  return ESP_OK;
}

esp_err_t http_get_advantage_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/advantage");

  // Ziskat historii material advantage z game tasku
  esp_err_t ret = game_get_advantage_json(json_buffer, sizeof(json_buffer));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get advantage history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, json_buffer, strlen(json_buffer));

  return ESP_OK;
}
// ============================================================================
// VIRTUAL GAME ACTIONS (REMOTE CONTROL)
// ============================================================================

/**
 * @brief Handler pro POST /api/move
 *
 * Provede tah na sachovnici.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"from": "e2", "to": "e4", "promotion": "q"}
 */
esp_err_t http_post_game_move_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/move");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Move blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Precist JSON data
  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data received", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser
  char from[8] = {0};
  char to[8] = {0};
  char promotion[4] = {0};

  // Najit "from"
  const char *from_start = strstr(content, "\"from\"");
  if (from_start != NULL) {
    from_start = strchr(from_start, ':');
    if (from_start != NULL) {
      from_start++;
      while (*from_start == ' ' || *from_start == '\"')
        from_start++;
      const char *from_end = strchr(from_start, '\"');
      if (from_end != NULL && from_end > from_start) {
        size_t len = from_end - from_start;
        if (len < sizeof(from))
          strncpy(from, from_start, len);
      }
    }
  }

  // Najit "to"
  const char *to_start = strstr(content, "\"to\"");
  if (to_start != NULL) {
    to_start = strchr(to_start, ':');
    if (to_start != NULL) {
      to_start++;
      while (*to_start == ' ' || *to_start == '\"')
        to_start++;
      const char *to_end = strchr(to_start, '\"');
      if (to_end != NULL && to_end > to_start) {
        size_t len = to_end - to_start;
        if (len < sizeof(to))
          strncpy(to, to_start, len);
      }
    }
  }

  // Najit "promotion"
  const char *promo_start = strstr(content, "\"promotion\"");
  if (promo_start != NULL) {
    promo_start = strchr(promo_start, ':');
    if (promo_start != NULL) {
      promo_start++;
      while (*promo_start == ' ' || *promo_start == '\"')
        promo_start++;
      const char *promo_end = strchr(promo_start, '\"');
      if (promo_end != NULL && promo_end > promo_start) {
        size_t len = promo_end - promo_start;
        if (len < sizeof(promotion))
          strncpy(promotion, promo_start, len);
      }
    }
  }

  if (strlen(from) == 0 || strlen(to) == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Missing 'from' or 'to'\"}", -1);
    return ESP_FAIL;
  }

  /* Promoce se týká jen tahu PĚŠCE na poslední řadu. Dříve zde HTTP vracelo 400
   * pro každý tah na řádky 1/8 bez "promotion" — to blokovalo legální tahy
   * dámy, věže, střelce a jezdce na poslední řadu. Game task validuje tah a u
   * pěšce spustí promotion flow (pending + volba z UI/tlačítek). */
  bool dest_back_rank =
      (strlen(to) >= 2 && (to[1] == '1' || to[1] == '8'));
  if (!dest_back_rank && strlen(promotion) > 0) {
    ESP_LOGW(TAG,
             "Promotion parameter for %s->%s (cíl není řádek 1/8), ignoring",
             from, to);
  }

  // Pripravit command pro game task
  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_MOVE;
  strncpy(cmd.from_notation, from, sizeof(cmd.from_notation) - 1);
  strncpy(cmd.to_notation, to, sizeof(cmd.to_notation) - 1);

  cmd.promotion_choice = PROMOTION_QUEEN;
  if (strlen(promotion) > 0) {
    switch (promotion[0]) {
    case 'q':
    case 'Q':
      cmd.promotion_choice = PROMOTION_QUEEN;
      break;
    case 'r':
    case 'R':
      cmd.promotion_choice = PROMOTION_ROOK;
      break;
    case 'b':
    case 'B':
      cmd.promotion_choice = PROMOTION_BISHOP;
      break;
    case 'n':
    case 'N':
      cmd.promotion_choice = PROMOTION_KNIGHT;
      break;
    default:
      cmd.promotion_choice = PROMOTION_QUEEN;
      break;
    }
  }
  cmd.promotion_from_remote = (strlen(promotion) > 0) ? 1U : 0U;

  // Odeslat do fronty
  if (game_command_queue == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Game queue not available", -1);
    return ESP_FAIL;
  }

  // Synchronous Verification Setup
  // Create a temporary queue to receive the validation result
  // This allows us to return 400 Bad Request if the move is invalid
  QueueHandle_t response_queue = xQueueCreate(1, sizeof(game_response_t));
  if (response_queue == NULL) {
    ESP_LOGE(TAG, "Failed to create response queue");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response queue", -1);
    return ESP_FAIL;
  }
  cmd.response_queue = response_queue;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    vQueueDelete(response_queue); // Cleanup
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to queue move", -1);
    return ESP_FAIL;
  }

  // Wait for response (validation result)
  game_response_t response;
  // 500ms timeout should be enough for simple validation
  if (xQueueReceive(response_queue, &response, pdMS_TO_TICKS(1000)) == pdTRUE) {
    // Check if move was successful
    if (response.type == GAME_RESPONSE_ERROR) {
      ESP_LOGW(TAG, "❌ Move rejected by game task: %s", response.data);
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_set_type(req, "application/json");
      char error_json[256];
      snprintf(error_json, sizeof(error_json),
               "{\"success\":false,\"message\":\"%s\"}", response.data);
      httpd_resp_send(req, error_json, -1);
    } else {
      ESP_LOGI(TAG, "✅ Move accepted by game task");
      httpd_resp_set_status(req, "200 OK");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"success\":true,\"message\":\"Move processed\"}",
                      -1);
    }
  } else {
    // Timeout - treat as success (async fallback) or error?
    // Let's treat as partial success but log warning
    ESP_LOGW(TAG, "⚠️ Move validation timed out");
    httpd_resp_set_status(req, "202 Accepted"); // Accepted but not verified yet
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":true,\"message\":\"Move queued (timeout)\"}", -1);
  }

  // Cleanup queue
  vQueueDelete(response_queue);
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/virtual_action
 *
 * Umoznuje virtualni zvedani a pokladani figurek pres web.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"action": "pickup"|"drop", "square": "e2"}
 */
esp_err_t http_post_game_virtual_action_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/virtual_action");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Virtual action blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Nacist JSON z request body
  char content[128] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser
  char action[16] = {0};
  char square[8] = {0};
  char choice[4] = {0};

  // Najit "action"
  const char *action_start = strstr(content, "\"action\"");
  if (action_start != NULL) {
    action_start = strchr(action_start, ':');
    if (action_start != NULL) {
      action_start++; // Preskocit ':'
      while (*action_start == ' ' || *action_start == '\"')
        action_start++;
      const char *action_end = strchr(action_start, '\"');
      if (action_end != NULL && action_end > action_start) {
        size_t len = action_end - action_start;
        if (len < sizeof(action)) {
          strncpy(action, action_start, len);
        }
      }
    }
  }

  // Najit "square"
  const char *square_start = strstr(content, "\"square\"");
  if (square_start != NULL) {
    square_start = strchr(square_start, ':');
    if (square_start != NULL) {
      square_start++; // Preskocit ':'
      while (*square_start == ' ' || *square_start == '\"')
        square_start++;
      const char *square_end = strchr(square_start, '\"');
      if (square_end != NULL && square_end > square_start) {
        size_t len = square_end - square_start;
        if (len < sizeof(square)) {
          strncpy(square, square_start, len);
        }
      }
    }
  }

  // Najit "choice" (pro promotion)
  const char *choice_start = strstr(content, "\"choice\"");
  if (choice_start != NULL) {
    choice_start = strchr(choice_start, ':');
    if (choice_start != NULL) {
      choice_start++; // Preskocit ':'
      while (*choice_start == ' ' || *choice_start == '\"')
        choice_start++;
      const char *choice_end = strchr(choice_start, '\"');
      if (choice_end != NULL && choice_end > choice_start) {
        size_t len = choice_end - choice_start;
        if (len < sizeof(choice)) {
          strncpy(choice, choice_start, len);
        }
      }
    }
  }

  // Validace
  if (strlen(action) == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Missing action\"}",
                    -1);
    return ESP_FAIL;
  }

  // Pripravit command pro game task
  chess_move_command_t cmd = {0};

  if (strcmp(action, "pickup") == 0) {
    cmd.type = GAME_CMD_PICKUP; // UP command
  } else if (strcmp(action, "drop") == 0) {
    cmd.type = GAME_CMD_DROP; // DOWN command
  } else if (strcmp(action, "promote") == 0) {
    cmd.type = GAME_CMD_PROMOTION;
  } else {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Invalid action\"}",
                    -1);
    return ESP_FAIL;
  }

  if (strcmp(action, "pickup") == 0) {
    // PICKUP: source square goes in from_notation
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for pickup\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.from_notation, square, sizeof(cmd.from_notation) - 1);
  } else if (strcmp(action, "drop") == 0) {
    // DROP: destination square goes in to_notation
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for drop\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
  } else if (strcmp(action, "promote") == 0) {
    // PROMOTE: choice goes into promotion_choice
    if (strlen(choice) == 0)
      strcpy(choice, "Q"); // Default to Queen

    if (strcasecmp(choice, "Q") == 0)
      cmd.promotion_choice = PROMOTION_QUEEN;
    else if (strcasecmp(choice, "R") == 0)
      cmd.promotion_choice = PROMOTION_ROOK;
    else if (strcasecmp(choice, "B") == 0)
      cmd.promotion_choice = PROMOTION_BISHOP;
    else if (strcasecmp(choice, "N") == 0)
      cmd.promotion_choice = PROMOTION_KNIGHT;
    else
      cmd.promotion_choice = PROMOTION_QUEEN; // Fallback

    // Optional square
    if (strlen(square) > 0)
      strncpy(cmd.to_notation, square, sizeof(cmd.to_notation) - 1);
  }

  // Odeslat do fronty
  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to send command", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Action processed\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief POST /api/game/new: fronta GAME_CMD_NEW_GAME (explicitni nova hra z webu).
 *
 * @param req ukazatel na httpd_req_t
 * @return ESP_OK nebo chybovy kod
 *
 * @details
 * Bez JSON payload. Odlisne se od automatickeho NEW_GAME pri startu z main
 * (initialize_chess_game), kde se pri obnove NVS prikaz neposila.
 */
esp_err_t http_post_game_new_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/new");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "New game blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  chess_move_command_t cmd = {0};
  cmd.player = 0;
  cmd.response_queue = NULL;

  char body[256] = {0};
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r > 0) {
    body[r] = '\0';
    cJSON *root = cJSON_Parse(body);
    if (root != NULL) {
      cJSON *fj = cJSON_GetObjectItemCaseSensitive(root, "fen");
      if (cJSON_IsString(fj) && fj->valuestring != NULL &&
          fj->valuestring[0] != '\0') {
        cmd.type = GAME_CMD_NEW_GAME_FROM_FEN;
        strncpy(cmd.timer_data.fen_new_game.fen, fj->valuestring,
                sizeof(cmd.timer_data.fen_new_game.fen) - 1);
        cmd.timer_data.fen_new_game.fen[sizeof(cmd.timer_data.fen_new_game.fen) -
                                        1] = '\0';
        cJSON_Delete(root);
      } else {
        cJSON_Delete(root);
        cmd.type = GAME_CMD_NEW_GAME;
      }
    } else {
      cmd.type = GAME_CMD_NEW_GAME;
    }
  } else {
    cmd.type = GAME_CMD_NEW_GAME;
  }

  if (game_command_queue == NULL) {
    ESP_LOGE(TAG, "Game command queue not available");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Queue not available\"}", -1);
    return ESP_FAIL;
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to send NEW_GAME command to queue");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Failed to send command\"}", -1);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "✅ game/new command sent (type=%d)", (int)cmd.type);

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"New game started\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/hint_highlight
 *
 * Body: { "to": "e4" } povinne; { "from": "e2" } volitelne.
 * Kdyz "from" chybi nebo je neplatny, posle from_led=64 takze na desce sviti jen "to".
 */
esp_err_t http_post_game_hint_highlight_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/hint_highlight");

  if (web_is_locked()) {
    ESP_LOGW(TAG, "Hint highlight blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  char buf[128];
  int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"No body\"}", -1);
    return ESP_OK;
  }
  buf[ret] = '\0';

  esp_err_t herr = web_server_apply_hint_highlight_json_body(buf);
  if (herr == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Invalid hint body\"}",
                    -1);
    return ESP_OK;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/game/hint_clear
 * Vymaze vizualizaci hintu na LED (vola se pri clearBotSuggestion).
 */
esp_err_t http_post_game_hint_clear_handler(httpd_req_t *req) {
  (void)req;
  ESP_LOGI(TAG, "POST /api/game/hint_clear");
  led_command_t cmd = {0};
  cmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
  led_execute_command_new(&cmd);
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

esp_err_t http_post_game_guard_clear_handler(httpd_req_t *req) {
  (void)req;
  ESP_LOGI(TAG, "POST /api/game/guard_clear");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  const bool was_active =
      game_is_matrix_guard_active() || matrix_is_guard_mode_active();
  if (was_active) {
    game_force_clear_matrix_guard();
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  if (was_active) {
    httpd_resp_send(
        req,
        "{\"success\":true,\"cleared\":true,\"message\":\"Matrix guard "
        "cleared. Verify physical board matches the game.\"}",
        -1);
  } else {
    httpd_resp_send(req, "{\"success\":true,\"cleared\":false}", -1);
  }
  return ESP_OK;
}

/**
 * POST /api/game/setup_tutorial
 * Body: {"action":"start"|"cancel"|"finish"}
 */
esp_err_t http_post_game_setup_tutorial_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/setup_tutorial");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char buf[192] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';

  bool want_start = (strstr(buf, "\"start\"") != NULL);
  bool want_cancel = (strstr(buf, "\"cancel\"") != NULL);
  bool want_finish = (strstr(buf, "\"finish\"") != NULL);
  int n = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) + (want_finish ? 1 : 0);
  if (n != 1) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req,
                    "{\"success\":false,\"error\":\"Specify exactly one action: "
                    "start, cancel, or finish\"}",
                    -1);
    return ESP_OK;
  }

  if (want_start || want_cancel) {
    setup_tutorial_reset_finish_cooldown();
  }

  if (want_finish) {
    if (!game_is_board_setup_tutorial_active()) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(
          req, "{\"success\":false,\"error\":\"Tutorial is not active\"}", -1);
      return ESP_OK;
    }
    if (setup_tutorial_finish_in_cooldown()) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "409 Conflict");
      httpd_resp_send(req,
                      "{\"success\":false,\"error\":\"Physical board does "
                      "not match starting occupancy (rows 0-1,6-7 full; "
                      "2-5 empty)\"}",
                      -1);
      return ESP_OK;
    }
    if (!game_finish_board_setup_tutorial_from_web()) {
      httpd_resp_set_type(req, "application/json");
      if (game_is_board_setup_tutorial_active()) {
        setup_tutorial_note_finish_conflict();
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_send(req,
                        "{\"success\":false,\"error\":\"Physical board does "
                        "not match starting occupancy (rows 0-1,6-7 full; "
                        "2-5 empty)\"}",
                        -1);
      } else {
        setup_tutorial_reset_finish_cooldown();
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(
            req, "{\"success\":false,\"error\":\"Could not finish tutorial\"}",
            -1);
      }
      return ESP_OK;
    }
    setup_tutorial_reset_finish_cooldown();
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":true,\"message\":\"Starting position "
                    "confirmed; new game started\"}",
                    -1);
    return ESP_OK;
  }

  if (game_command_queue == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Game command queue unavailable\"}",
        -1);
    return ESP_FAIL;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_BOARD_SETUP_TUTORIAL;
  cmd.promotion_choice = want_start ? 0U : 1U;
  cmd.response_queue = NULL;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "setup_tutorial: queue send failed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Queue full\"}", -1);
    return ESP_FAIL;
  }

  if (want_cancel) {
    led_command_t lcmd = {0};
    lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&lcmd);
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

/**
 * POST /api/game/puzzle
 * Body: {"action":"start","id":1..5} | {"action":"prepare","id":1..5} |
 * {"action":"cancel"}
 */
esp_err_t http_post_game_puzzle_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/puzzle");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char buf[192] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';

  bool want_start = (strstr(buf, "\"start\"") != NULL);
  bool want_cancel = (strstr(buf, "\"cancel\"") != NULL);
  bool want_prepare = (strstr(buf, "\"prepare\"") != NULL);
  int n = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) + (want_prepare ? 1 : 0);
  if (n != 1) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req,
                    "{\"success\":false,\"error\":\"Specify exactly one action: "
                    "start, prepare, or cancel\"}",
                    -1);
    return ESP_OK;
  }

  if (game_command_queue == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Game command queue unavailable\"}",
        -1);
    return ESP_FAIL;
  }

  chess_move_command_t cmd = {0};
  cmd.type = GAME_CMD_PUZZLE;
  cmd.promotion_choice = 0U;
  cmd.response_queue = NULL;

  if (want_start || want_prepare) {
    int puzzle_id = -1;
    char *id_ptr = strstr(buf, "\"id\"");
    if (id_ptr != NULL) {
      char *colon = strchr(id_ptr, ':');
      if (colon != NULL) {
        puzzle_id = atoi(colon + 1);
      }
    }
    if (puzzle_id < 1 || puzzle_id > 5) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req,
                      "{\"success\":false,\"error\":\"Puzzle id must be 1..5\"}",
                      -1);
      return ESP_OK;
    }
    cmd.promotion_choice =
        want_prepare ? (uint8_t)(100 + puzzle_id) : (uint8_t)puzzle_id;
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "puzzle: queue send failed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Queue full\"}", -1);
    return ESP_FAIL;
  }

  if (want_cancel) {
    led_command_t lcmd = {0};
    lcmd.type = LED_CMD_CLEAR_HIGHLIGHTS;
    led_execute_command_new(&lcmd);
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

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

static const char *opening_dispatch_error_key(esp_err_t err) {
  if (err == ESP_ERR_INVALID_STATE) {
    return "mode_conflict";
  }
  if (err == ESP_ERR_NOT_ALLOWED) {
    return "resync_incomplete";
  }
  if (err == ESP_FAIL) {
    return "queue_full";
  }
  return "invalid_line";
}

/** POST /api/game/opening */
esp_err_t http_post_game_opening_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/opening");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"Web interface locked\"}",
                    -1);
    return ESP_OK;
  }

  char buf[2048] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';

  cJSON *root = cJSON_Parse(buf);
  if (root == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"Invalid JSON\"}", -1);
    return ESP_OK;
  }

  cJSON *action_j = cJSON_GetObjectItemCaseSensitive(root, "action");
  if (!cJSON_IsString(action_j) || action_j->valuestring == NULL) {
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"Missing action\"}", -1);
    return ESP_OK;
  }

  uint8_t action_code = opening_action_code(action_j->valuestring);
  if (action_code == 255) {
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"invalid_action\"}", -1);
    return ESP_OK;
  }

  esp_err_t dispatch_err = web_server_opening_dispatch_json(root);
  cJSON_Delete(root);

  if (dispatch_err == ESP_OK) {
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", -1);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "application/json");
  if (dispatch_err == ESP_ERR_INVALID_STATE) {
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"mode_conflict\"}", -1);
    return ESP_OK;
  }
  if (dispatch_err == ESP_ERR_NOT_ALLOWED) {
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"resync_incomplete\"}", -1);
    return ESP_OK;
  }
  if (dispatch_err == ESP_FAIL) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"queue_full\"}", -1);
    return ESP_FAIL;
  }
  httpd_resp_set_status(req, "400 Bad Request");
  char err_body[96];
  snprintf(err_body, sizeof(err_body),
           "{\"ok\":false,\"error\":\"%s\"}", opening_dispatch_error_key(dispatch_err));
  httpd_resp_send(req, err_body, -1);
  return ESP_OK;
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */
