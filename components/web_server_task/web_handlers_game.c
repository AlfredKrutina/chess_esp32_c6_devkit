/**
 * @file web_handlers_game.c
 * @brief Game-related HTTP handlers and snapshot JSON builders.
 */

#include "web_server_task.h"
#include "web_server_internal.h"
#include "cmd_wire.h"
#include "../game_task/include/game_task.h"
#include "../game_task/include/game_board_core.h"
#include "../freertos_chess/include/game_cmd.h"
#include "../squares_task/include/squares_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "../led_task/include/power_guard.h"
#include "led_mapping.h"
#include "../config_manager/include/config_manager.h"

#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "board_api_auth.h"
#include "esp_http_server.h"
#endif
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "../freertos_chess/include/shared_buffer_pool.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

extern QueueHandle_t game_command_queue;

static const char *TAG = "WEB_GAME";

static StaticQueue_t s_web_move_rsp_q_struct;
static uint8_t s_web_move_rsp_q_storage[sizeof(game_response_t)];
static QueueHandle_t s_web_move_rsp_queue;
static SemaphoreHandle_t s_web_move_rsp_mutex;

static QueueHandle_t web_move_response_queue(void) {
  if (s_web_move_rsp_queue == NULL) {
    s_web_move_rsp_queue =
        xQueueCreateStatic(1, sizeof(game_response_t), s_web_move_rsp_q_storage,
                           &s_web_move_rsp_q_struct);
    s_web_move_rsp_mutex = xSemaphoreCreateMutex();
  }
  return s_web_move_rsp_queue;
}


char *snapshot_buffer;
SemaphoreHandle_t snapshot_build_mutex;

void web_snapshot_buffer_release(void) {
  if (snapshot_buffer != NULL) {
    (void)release_shared_buffer(snapshot_buffer);
    snapshot_buffer = NULL;
  }
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
static void http_resp_pool_busy(httpd_req_t *req) {
  httpd_resp_set_status(req, "503 Service Unavailable");
  httpd_resp_send(req, "Buffer pool busy", HTTPD_RESP_USE_STRLEN);
}
#endif

/** After a conflict during the finish setup tutorial, briefly do not call validation again (BLE + HTTP). */
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
/** Supplementing GET /api/status with web lock, WiFi, brightness, matrix guard, lamp (shared with snapshot). */
void inject_web_status_fields(char *buf, size_t buf_size) {
  char *last_brace = strrchr(buf, '}');
  if (!last_brace) {
    ESP_LOGW(TAG, "[STAGING] inject_web_status_fields: no closing brace in buf");
    return;
  }
  size_t remaining = buf_size - (size_t)(last_brace - buf);
  /* The first block ~370 B (+ chess_hint_limit) + the second ~120 B — with a tight buffer, it is better not to send truncated JSON. */
  if (remaining < 560) {
    ESP_LOGE(TAG,
             "[STAGING] inject_web_status_fields: remaining=%zu B too small for "
             "web fields (cap=%zu)",
             remaining, buf_size);
    return;
  }
  uint8_t b = cached_brightness_valid ? cached_brightness : 100;
  int hint_lim = config_ui_prefs_get_chess_hint_limit();
  int wr =
      snprintf(last_brace, remaining,
               ",\"web_locked\":%s,\"internet_connected\":%s,\"brightness\":%d,"
               "\"guided_capture_hints_enabled\":%s,\"led_guidance_level\":%u,"
               "\"matrix_guard_active\":%s,"
               "\"matrix_guard_conflicts\":%u,"
               "\"matrix_guard_lifted_low\":%lu,\"matrix_guard_lifted_high\":%lu,"
               "\"matrix_guard_dropped_low\":%lu,\"matrix_guard_dropped_high\":%lu,"
               "\"chess_hint_limit\":%d,"
               "\"power_limit\":%s,\"power_scale\":%u,"
               "\"power_brownout_streak\":%u,\"power_brownout_total\":%u}",
               web_is_locked() ? "true" : "false",
               wifi_is_sta_connected() ? "true" : "false", (int)b,
               game_get_guided_capture_hints_enabled() ? "true" : "false",
               (unsigned)game_get_led_guidance_level(),
               game_is_squares_guard_active() ? "true" : "false",
               (unsigned int)game_get_squares_guard_conflict_count(),
               (unsigned long)game_get_squares_guard_lifted_mask_low(),
               (unsigned long)game_get_squares_guard_lifted_mask_high(),
               (unsigned long)game_get_squares_guard_dropped_mask_low(),
               (unsigned long)game_get_squares_guard_dropped_mask_high(),
               hint_lim,
               power_guard_output_half() ? "true" : "false",
               (unsigned)power_guard_scale_percent(),
               (unsigned)power_guard_brownout_streak(),
               (unsigned)power_guard_brownout_total());
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
  /* portMAX_DELAY here blocked web_server_task on mutex (HTTP httpd holds build)
   * → without esp_task_wdt_reset() TWDT timeout (~10s). We are waiting after the steps + reset. */
  while (xSemaphoreTake(snapshot_build_mutex, pdMS_TO_TICKS(400)) != pdTRUE) {
    (void)web_server_task_wdt_reset_safe();
  }
}

void snapshot_build_mutex_give(void) {
  if (snapshot_build_mutex != NULL) {
    xSemaphoreGive(snapshot_build_mutex);
  }
}

/** Under `snapshot_build_mutex` — no parallel build; saves ~1 KiB stack per NimBLE host task. */
static char s_clock_json_build_scratch[TIMER_HTTP_JSON_MAX];

/** Same JSON as GET /api/game/snapshot — to buffer `out` (both HTTP and BLE). */
static esp_err_t build_snapshot_json_to_buffer(char *out, size_t cap,
                                               size_t *out_len) {
  snapshot_build_mutex_take();
  char *json_buf = get_shared_buffer(JSON_BUFFER_SIZE);
  if (json_buf == NULL) {
    snapshot_build_mutex_give();
    return ESP_ERR_NO_MEM;
  }
  (void)web_server_task_wdt_reset_safe();
  esp_err_t ret = game_get_board_json(json_buf, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  size_t L = strlen(json_buf);
  if (L < 4 || json_buf[0] != '{' || json_buf[L - 1] != '}') {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  json_buf[L - 1] = '\0';
  uint32_t srev = game_get_state_revision();
  size_t pos = (size_t)snprintf(out, cap, "{\"state_version\":%" PRIu32 ",%s",
                                srev, json_buf + 1);
  if (pos >= cap) {
    json_buf[L - 1] = '}';
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }

  ret = game_get_status_json(json_buf, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
#ifndef NDEBUG
  ESP_LOGI(TAG, "[STAGING] build_snapshot: status raw len=%zu / cap=%d",
           strlen(json_buf), JSON_BUFFER_SIZE);
#endif
  inject_web_status_fields(json_buf, JSON_BUFFER_SIZE);
#ifndef NDEBUG
  ESP_LOGI(TAG, "[STAGING] build_snapshot: status after inject len=%zu / cap=%d",
           strlen(json_buf), JSON_BUFFER_SIZE);
#endif
  int n = snprintf(out + pos, cap - pos, ",\"status\":%s", json_buf);
  if (n < 0 || (size_t)n >= cap - pos) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_history_json(json_buf, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"history\":%s", json_buf);
  if (n < 0 || (size_t)n >= cap - pos) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  ret = game_get_captured_json(json_buf, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ret;
  }
  (void)web_server_task_wdt_reset_safe();
  n = snprintf(out + pos, cap - pos, ",\"captured\":%s", json_buf);
  if (n < 0 || (size_t)n >= cap - pos) {
    (void)release_shared_buffer(json_buf);
    snapshot_build_mutex_give();
    return ESP_FAIL;
  }
  pos += (size_t)n;

  (void)release_shared_buffer(json_buf);

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
  web_snapshot_buffer_release();
  char *snap = get_shared_buffer(SNAPSHOT_BUFFER_SIZE);
  if (snap == NULL) {
    return ESP_ERR_NO_MEM;
  }
  snapshot_buffer = snap;
  esp_err_t e = build_snapshot_json_to_buffer(
      snap, shared_buffer_capacity(snap), out_len);
  if (e != ESP_OK) {
    web_snapshot_buffer_release();
  }
  return e;
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
  web_snapshot_buffer_release();
  char *snap = get_shared_buffer(SNAPSHOT_BUFFER_SIZE);
  if (snap == NULL) {
    return ESP_ERR_NO_MEM;
  }
  snapshot_buffer = snap;
  esp_err_t e = build_snapshot_json_to_buffer(
      snap, shared_buffer_capacity(snap), out_len);
  if (e == ESP_OK) {
    *out_json = snapshot_buffer;
  } else {
    web_snapshot_buffer_release();
  }
  return e;
}

bool web_game_submit_hint_clear(void) {
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_WEB_HINT;
  cmd.u.hint.clear = 1;
  return game_submit(&cmd);
}

void web_game_on_client_drop(void) {
  if (!game_active) {
    return;
  }
  /* Hint-clear is the idle full frame. Do not paint from this task. */
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_WEB_HINT;
  cmd.u.hint.clear = 1;
  (void)game_submit(&cmd);
}

static bool web_game_submit_hint_pixels(uint8_t from_sq, uint8_t to_sq) {
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_WEB_HINT;
  cmd.u.hint.from_sq = from_sq;
  cmd.u.hint.to_sq = to_sq;
  return game_submit(&cmd);
}

/** Common logic of POST /api/game/hint_highlight and hint_highlight BLE command. */
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

  /* Chess square (rank*8+file). The painter maps that to the LED chain.
   * An LED index here was drawn on a different square, so the move stayed dark. */
  uint8_t to_square = (uint8_t)((to_sq[1] - '1') * 8 + (to_sq[0] - 'a'));
  uint8_t from_square = 64;
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
        from_square =
            (uint8_t)((from_sq[1] - '1') * 8 + (from_sq[0] - 'a'));
      }
    }
  }

  if (!web_game_submit_hint_pixels(from_square, to_square)) {
    return ESP_ERR_TIMEOUT;
  }
  ESP_LOGI(TAG, "Hint highlight: %s (sq %u -> %u)", to_sq, (unsigned)from_square,
           (unsigned)to_square);
  return ESP_OK;
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
esp_err_t web_get_board_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/board");

  char *jb = get_shared_buffer(JSON_BUFFER_SIZE);
  if (jb == NULL) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
  esp_err_t ret = game_get_board_json(jb, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(jb);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get board state", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jb, strlen(jb));
  (void)release_shared_buffer(jb);
  return ESP_OK;
}

esp_err_t web_get_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/status");

  char *jb = get_shared_buffer(JSON_BUFFER_SIZE);
  if (jb == NULL) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
  esp_err_t ret = game_get_status_json(jb, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(jb);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get game status", -1);
    return ESP_FAIL;
  }

  inject_web_status_fields(jb, JSON_BUFFER_SIZE);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jb, strlen(jb));
  (void)release_shared_buffer(jb);
  return ESP_OK;
}

esp_err_t web_get_history_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/history");

  char *jb = get_shared_buffer(JSON_BUFFER_SIZE);
  if (jb == NULL) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
  esp_err_t ret = game_get_history_json(jb, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(jb);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get move history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jb, strlen(jb));
  (void)release_shared_buffer(jb);
  return ESP_OK;
}

esp_err_t web_get_captured_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/captured");

  char *jb = get_shared_buffer(JSON_BUFFER_SIZE);
  if (jb == NULL) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
  esp_err_t ret = game_get_captured_json(jb, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(jb);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get captured pieces", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jb, strlen(jb));
  (void)release_shared_buffer(jb);
  return ESP_OK;
}

esp_err_t web_get_game_snapshot_handler(httpd_req_t *req) {
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
  if (ret == ESP_ERR_NO_MEM) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
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
  web_snapshot_buffer_release();
  return ESP_OK;
}

esp_err_t web_get_advantage_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/advantage");

  char *jb = get_shared_buffer(JSON_BUFFER_SIZE);
  if (jb == NULL) {
    http_resp_pool_busy(req);
    return ESP_FAIL;
  }
  esp_err_t ret = game_get_advantage_json(jb, JSON_BUFFER_SIZE);
  if (ret != ESP_OK) {
    (void)release_shared_buffer(jb);
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get advantage history", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, jb, strlen(jb));
  (void)release_shared_buffer(jb);
  return ESP_OK;
}
// ============================================================================
// VIRTUAL GAME ACTIONS (REMOTE CONTROL)
// ============================================================================

/**
 * @brief Handler for POST /api/move
 *
 * Makes a move on the chest.
 *
 * @param req HTTP request
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * Expected JSON: {"from": "e2", "to": "e4", "promotion": "q"}
 */
esp_err_t web_post_game_move_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/move");

#if CONFIG_CHESS_ENABLE_WEB_SERVER
  if (board_pair_http_command_denied(req)) {
    return ESP_OK;
  }
#endif

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
  char content[192];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data received", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';
  if (cmd_wire_http_guard(req, content, "move")) {
    return ESP_OK;
  }

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

  /* The promotion only applies to the Pawn move on the last row. Previously, HTTP returned 400 here
   * for every move on lines 1/8 without "promotion" — this blocked legal moves
   * queens, rooks, bishops and knights on the last row. Game task validates the move and u
   * Pawn starts promotion flow (pending + selection from UI/buttons). */
  bool dest_back_rank =
      (strlen(to) >= 2 && (to[1] == '1' || to[1] == '8'));
  if (!dest_back_rank && strlen(promotion) > 0) {
    ESP_LOGW(TAG,
             "Promotion parameter for %s->%s (target is not line 1/8), ignoring",
             from, to);
  }

  bool accepted_async = false;
  char move_err[96];
  move_err[0] = '\0';
  esp_err_t qerr =
      web_game_enqueue_move(from, to, promotion, move_err, sizeof(move_err),
                            &accepted_async);
  if (qerr == ESP_ERR_INVALID_STATE) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Game queue not available", -1);
    return ESP_FAIL;
  }
  if (qerr == ESP_ERR_NO_MEM || qerr == ESP_FAIL || qerr == ESP_ERR_TIMEOUT) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", content, false,
                       NULL);
    return ESP_OK;
  }
  if (qerr == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    char error_json[256];
    snprintf(error_json, sizeof(error_json),
             "{\"success\":false,\"message\":\"%s\"}",
             move_err[0] != '\0' ? move_err : "Illegal move");
    httpd_resp_send(req, error_json, -1);
    return ESP_OK;
  }
  if (accepted_async) {
    cmd_wire_http_send(
        req, "202 Accepted",
        "{\"success\":true,\"message\":\"Move queued (timeout)\"}", content,
        true, "move");
  } else {
    cmd_wire_http_send(req, "200 OK",
                       "{\"success\":true,\"message\":\"Move processed\"}",
                       content, true, "move");
  }
  return ESP_OK;
}

esp_err_t web_game_enqueue_move(const char *from, const char *to,
                                const char *promotion, char *err, size_t err_cap,
                                bool *accepted_async) {
  if (accepted_async != NULL) {
    *accepted_async = false;
  }
  if (err != NULL && err_cap > 0) {
    err[0] = '\0';
  }
  if (from == NULL || to == NULL || from[0] == '\0' || to[0] == '\0') {
    return ESP_ERR_INVALID_ARG;
  }
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_MOVE;
  strncpy(cmd.u.move.from, from, sizeof(cmd.u.move.from) - 1);
  strncpy(cmd.u.move.to, to, sizeof(cmd.u.move.to) - 1);
  if (promotion != NULL && promotion[0] != '\0') {
    switch (promotion[0]) {
    case 'r':
    case 'R':
      cmd.u.move.promotion = 'r';
      break;
    case 'b':
    case 'B':
      cmd.u.move.promotion = 'b';
      break;
    case 'n':
    case 'N':
      cmd.u.move.promotion = 'n';
      break;
    default:
      cmd.u.move.promotion = 'q';
      break;
    }
  }
  if (game_command_queue == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  QueueHandle_t response_queue = web_move_response_queue();
  if (response_queue == NULL || s_web_move_rsp_mutex == NULL) {
    return ESP_ERR_NO_MEM;
  }
  if (xSemaphoreTake(s_web_move_rsp_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  game_response_t junk;
  while (xQueueReceive(response_queue, &junk, 0) == pdTRUE) {
  }
  cmd.response_queue = response_queue;
  if (!game_submit(&cmd)) {
    xSemaphoreGive(s_web_move_rsp_mutex);
    return ESP_FAIL;
  }
  game_response_t response;
  if (xQueueReceive(response_queue, &response, pdMS_TO_TICKS(1000)) == pdTRUE) {
    xSemaphoreGive(s_web_move_rsp_mutex);
    if (response.type == GAME_RESPONSE_ERROR) {
      ESP_LOGW(TAG, "Move rejected by game task: %s", response.data);
      if (err != NULL && err_cap > 0) {
        snprintf(err, err_cap, "%s", response.data);
      }
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "Move accepted by game task");
    return ESP_OK;
  }
  xSemaphoreGive(s_web_move_rsp_mutex);
  ESP_LOGW(TAG, "Move validation timed out");
  if (accepted_async != NULL) {
    *accepted_async = true;
  }
  return ESP_OK;
}

/**
 * @brief Handler for POST /api/game/virtual_action
 *
 * Enables virtual lifting and placing of figurines via the web.
 *
 * @param req HTTP request
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * Expected JSON: {"action": "pickup"|"drop", "square": "e2"}
 */
esp_err_t web_post_game_virtual_action_handler(httpd_req_t *req) {
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

  // Extract JSON from request body
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

  game_cmd_t cmd = {0};

  if (strcmp(action, "pickup") == 0) {
    cmd.kind = GCMD_BOARD_LIFT;
  } else if (strcmp(action, "drop") == 0) {
    cmd.kind = GCMD_BOARD_DROP;
  } else if (strcmp(action, "promote") == 0) {
    cmd.kind = GCMD_PROMOTION_CHOICE;
  } else {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Invalid action\"}",
                    -1);
    return ESP_FAIL;
  }

  if (strcmp(action, "pickup") == 0) {
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for pickup\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.u.lift.square, square, sizeof(cmd.u.lift.square) - 1);
  } else if (strcmp(action, "drop") == 0) {
    if (strlen(square) == 0) {
      httpd_resp_send(
          req, "{\"success\":false,\"message\":\"Missing square for drop\"}",
          -1);
      return ESP_FAIL;
    }
    strncpy(cmd.u.drop.to, square, sizeof(cmd.u.drop.to) - 1);
  } else if (strcmp(action, "promote") == 0) {
    if (strlen(choice) == 0) {
      strcpy(choice, "Q");
    }
    if (strcasecmp(choice, "R") == 0) {
      cmd.u.promo.piece = 'r';
    } else if (strcasecmp(choice, "B") == 0) {
      cmd.u.promo.piece = 'b';
    } else if (strcasecmp(choice, "N") == 0) {
      cmd.u.promo.piece = 'n';
    } else {
      cmd.u.promo.piece = 'q';
    }
  }

  if (!game_submit(&cmd)) {
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
 * @brief POST /api/game/new: queue GAME_CMD_NEW_GAME (explicit new game from web).
 *
 * @param req pointer to httpd_req_t
 * @return ESP_OK or error code
 *
 * @details
 * Without JSON payload. It differs from the automatic NEW_GAME when starting from main
 * (initialize_chess_game), where the NVS command is not sent when restoring.
 */
esp_err_t web_post_game_new_handler(httpd_req_t *req) {
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

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_NEW_GAME;

  char body[256] = {0};
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r > 0) {
    body[r] = '\0';
  }
  if (cmd_wire_http_guard(req, body, "new_game")) {
    return ESP_OK;
  }
  if (r > 0) {
    cJSON *root = cJSON_Parse(body);
    if (root != NULL) {
      cJSON *fj = cJSON_GetObjectItemCaseSensitive(root, "fen");
      if (cJSON_IsString(fj) && fj->valuestring != NULL &&
          fj->valuestring[0] != '\0') {
        cmd.kind = GCMD_NEW_GAME_FEN;
        strncpy(cmd.u.fen.fen, fj->valuestring, sizeof(cmd.u.fen.fen) - 1);
        cJSON_Delete(root);
      } else {
        cJSON_Delete(root);
      }
    }
  }

  if (game_command_queue == NULL) {
    ESP_LOGE(TAG, "Game command queue not available");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Queue not available\"}", -1);
    return ESP_FAIL;
  }

  if (cmd.kind == GCMD_NEW_GAME_FEN) {
    game_fen_position_t parsed;
    if (!game_parse_fen(cmd.u.fen.fen, &parsed)) {
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"success\":false,\"error\":\"FEN rejected\"}",
                      -1);
      return ESP_OK;
    }
  }

  if (!game_submit(&cmd)) {
    ESP_LOGE(TAG, "Failed to send NEW_GAME command to queue");
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }

  ESP_LOGI(TAG, "game/new command sent (kind=%d)", (int)cmd.kind);

  cmd_wire_http_send(req, "200 OK",
                     "{\"success\":true,\"message\":\"New game accepted\"}",
                     body, true, "new_game");

  return ESP_OK;
}

/**
 * @brief Handler for POST /api/game/hint_highlight
 *
 * Points: { "to": "e4" } required; { "from": "e2" } optional.
 * If "from" is missing or invalid, send from_led=64 so that only "it" lights up on the board.
 */
esp_err_t web_post_game_hint_highlight_handler(httpd_req_t *req) {
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

  char buf[192];
  if (!web_recv_json_body(req, buf, sizeof(buf))) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Incomplete hint\"}", -1);
    return ESP_OK;
  }

  if (cmd_wire_http_guard(req, buf, "hint_highlight")) {
    return ESP_OK;
  }

  esp_err_t herr = web_server_apply_hint_highlight_json_body(buf);
  if (herr == ESP_ERR_INVALID_ARG) {
    cmd_wire_http_send(req, "400 Bad Request",
                       "{\"success\":false,\"error\":\"Invalid hint body\"}",
                       buf, false, NULL);
    return ESP_OK;
  }
  if (herr == ESP_ERR_NO_MEM || herr == ESP_ERR_TIMEOUT || herr == ESP_FAIL) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", buf, false, NULL);
    return ESP_OK;
  }

  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", buf, true,
                     "hint_highlight");
  return ESP_OK;
}

/**
 * @brief Handler for POST /api/game/hint_clear
 * Deletes the visualization of the hint on the LED (called with clearBotSuggestion).
 */
esp_err_t web_post_game_hint_clear_handler(httpd_req_t *req) {
  char body[160];
  ESP_LOGI(TAG, "POST /api/game/hint_clear");
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "hint_clear")) {
    return ESP_OK;
  }
  if (!web_game_submit_hint_clear()) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }
  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "hint_clear");
  return ESP_OK;
}

esp_err_t web_post_game_guard_clear_handler(httpd_req_t *req) {
  char body[160];
  ESP_LOGI(TAG, "POST /api/game/guard_clear");
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "guard_clear")) {
    return ESP_OK;
  }

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  const bool was_active =
      game_is_squares_guard_active() || squares_is_guard_mode_active();
  if (was_active) {
    game_force_clear_squares_guard();
  }

  if (was_active) {
    cmd_wire_http_send(
        req, "200 OK",
        "{\"success\":true,\"cleared\":true,\"message\":\"Matrix guard "
        "cleared. Verify physical board matches the game.\"}",
        body, true, "guard_clear");
  } else {
    cmd_wire_http_send(req, "200 OK", "{\"success\":true,\"cleared\":false}",
                       body, true, "guard_clear");
  }
  return ESP_OK;
}

/**
 * POST /api/game/setup_tutorial
 * Body: {"action":"start"|"cancel"|"finish"}
 */
esp_err_t web_post_game_setup_tutorial_handler(httpd_req_t *req) {
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
    if (!game_is_physical_board_starting_occupancy()) {
      setup_tutorial_note_finish_conflict();
      httpd_resp_set_type(req, "application/json");
      httpd_resp_set_status(req, "409 Conflict");
      httpd_resp_send(req,
                      "{\"success\":false,\"error\":\"Physical board does "
                      "not match starting occupancy (rows 0-1,6-7 full; "
                      "2-5 empty)\"}",
                      -1);
      return ESP_OK;
    }
    setup_tutorial_reset_finish_cooldown();
  }

  if (game_command_queue == NULL) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Game command queue unavailable\"}",
        -1);
    return ESP_FAIL;
  }

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_MODE_TUTORIAL;
  cmd.u.mode.act = want_start ? (uint8_t)TUTORIAL_ACT_START
                              : (want_finish ? (uint8_t)TUTORIAL_ACT_FINISH
                                             : (uint8_t)TUTORIAL_ACT_CANCEL);

  if (!game_submit(&cmd)) {
    ESP_LOGE(TAG, "setup_tutorial: queue send failed");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Queue full\"}", -1);
    return ESP_FAIL;
  }

  if (want_cancel) {
    (void)web_game_submit_hint_clear();
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

static esp_err_t puzzle_queue_act(puzzle_act_t act) {
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_MODE_PUZZLE;
  cmd.u.mode.act = (uint8_t)act;
  if (!game_submit(&cmd)) {
    ESP_LOGE(TAG, "puzzle: queue send failed act=%u", (unsigned)act);
    return ESP_FAIL;
  }
  if (act == PUZZLE_ACT_CANCEL) {
    (void)web_game_submit_hint_clear();
  }
  return ESP_OK;
}

static void puzzle_copy_response(char *response, size_t response_len,
                                 const char *json) {
  if (response == NULL || response_len == 0 || json == NULL) {
    return;
  }
  strncpy(response, json, response_len - 1);
  response[response_len - 1] = '\0';
}

/**
 * Shared HTTP/BLE puzzle command.
 * action: load | start | prepare | cancel | hint
 * load: fen + solution[] (solver, opponent, solver…). Empty solution only
 * places the position after the matrix matches.
 * Returns ESP_ERR_INVALID_STATE when start is refused because the board
 * does not match (setup is queued instead).
 */
esp_err_t web_server_puzzle_dispatch_body(const char *json, char *response,
                                         size_t response_len) {
  if (json == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  cJSON *root = cJSON_Parse(json);
  if (root == NULL) {
    puzzle_copy_response(response, response_len,
                         "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return ESP_ERR_INVALID_ARG;
  }
  cJSON *action_j = cJSON_GetObjectItemCaseSensitive(root, "action");
  if (!cJSON_IsString(action_j) || action_j->valuestring == NULL) {
    cJSON_Delete(root);
    puzzle_copy_response(response, response_len,
                         "{\"success\":false,\"error\":\"Missing action\"}");
    return ESP_ERR_INVALID_ARG;
  }
  const char *action = action_j->valuestring;

  if (strcmp(action, "cancel") == 0) {
    cJSON_Delete(root);
    if (puzzle_queue_act(PUZZLE_ACT_CANCEL) != ESP_OK) {
      puzzle_copy_response(response, response_len,
                           "{\"success\":false,\"error\":\"Queue full\"}");
      return ESP_FAIL;
    }
    puzzle_copy_response(response, response_len, "{\"success\":true}");
    return ESP_OK;
  }

  if (strcmp(action, "hint") == 0) {
    cJSON_Delete(root);
    if (!game_puzzle_hint()) {
      puzzle_copy_response(
          response, response_len,
          "{\"success\":false,\"error\":\"hint_unavailable\"}");
      return ESP_ERR_INVALID_STATE;
    }
    puzzle_copy_response(response, response_len, "{\"success\":true}");
    return ESP_OK;
  }

  bool is_load = strcmp(action, "load") == 0;
  bool is_start = strcmp(action, "start") == 0;
  bool is_prepare = strcmp(action, "prepare") == 0;
  if (!is_load && !is_start && !is_prepare) {
    cJSON_Delete(root);
    puzzle_copy_response(response, response_len,
                         "{\"success\":false,\"error\":\"invalid_action\"}");
    return ESP_ERR_INVALID_ARG;
  }

  cJSON *id_j = cJSON_GetObjectItemCaseSensitive(root, "id");
  int puzzle_id = cJSON_IsNumber(id_j) ? id_j->valueint : 0;
  cJSON *fen_j = cJSON_GetObjectItemCaseSensitive(root, "fen");
  cJSON *title_j = cJSON_GetObjectItemCaseSensitive(root, "title");
  cJSON *sol_j = cJSON_GetObjectItemCaseSensitive(root, "solution");

  bool staged = false;
  if (is_load || (cJSON_IsString(fen_j) && fen_j->valuestring != NULL)) {
    if (!cJSON_IsString(fen_j) || fen_j->valuestring == NULL) {
      cJSON_Delete(root);
      puzzle_copy_response(response, response_len,
                           "{\"success\":false,\"error\":\"Missing fen\"}");
      return ESP_ERR_INVALID_ARG;
    }
    char line[12][8];
    memset(line, 0, sizeof(line));
    uint8_t count = 0;
    if (cJSON_IsArray(sol_j)) {
      int n = cJSON_GetArraySize(sol_j);
      if (n > 12) {
        cJSON_Delete(root);
        puzzle_copy_response(
            response, response_len,
            "{\"success\":false,\"error\":\"Too many solution plies\"}");
        return ESP_ERR_INVALID_ARG;
      }
      for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(sol_j, i);
        if (!cJSON_IsString(item) || item->valuestring == NULL) {
          cJSON_Delete(root);
          puzzle_copy_response(
              response, response_len,
              "{\"success\":false,\"error\":\"Bad solution move\"}");
          return ESP_ERR_INVALID_ARG;
        }
        strncpy(line[count], item->valuestring, sizeof(line[count]) - 1);
        count++;
      }
    }
    const char *title = (cJSON_IsString(title_j) && title_j->valuestring != NULL)
                            ? title_j->valuestring
                            : "";
    if (!game_puzzle_stage_line(fen_j->valuestring, title, line, count)) {
      cJSON_Delete(root);
      puzzle_copy_response(response, response_len,
                           "{\"success\":false,\"error\":\"Bad fen or solution\"}");
      return ESP_ERR_INVALID_ARG;
    }
    staged = true;
  } else if (puzzle_id >= 1 && puzzle_id <= 5 && (is_start || is_prepare)) {
    if (!game_puzzle_stage_builtin((uint8_t)puzzle_id)) {
      cJSON_Delete(root);
      puzzle_copy_response(response, response_len,
                           "{\"success\":false,\"error\":\"Bad puzzle id\"}");
      return ESP_ERR_INVALID_ARG;
    }
    staged = true;
  }

  if (!staged && is_start && game_puzzle_staged_fen()[0] == '\0') {
    cJSON_Delete(root);
    puzzle_copy_response(response, response_len,
                         "{\"success\":false,\"error\":\"No staged puzzle\"}");
    return ESP_ERR_INVALID_ARG;
  }

  const char *fen = game_puzzle_staged_fen();
  bool match = (fen != NULL && fen[0] != '\0') &&
               game_physical_squares_matches_fen(fen);
  cJSON_Delete(root);

  if (is_prepare || !match) {
    if (puzzle_queue_act(PUZZLE_ACT_APPLY_SETUP) != ESP_OK) {
      puzzle_copy_response(response, response_len,
                           "{\"success\":false,\"error\":\"Queue full\"}");
      return ESP_FAIL;
    }
    if (is_prepare || is_load) {
      char ok_body[96];
      snprintf(ok_body, sizeof(ok_body),
               "{\"success\":true,\"physical_match\":%s,\"play_queued\":false}",
               match ? "true" : "false");
      puzzle_copy_response(response, response_len, ok_body);
      return ESP_OK;
    }
    puzzle_copy_response(
        response, response_len,
        "{\"success\":false,\"error\":\"physical_mismatch\",\"physical_match\":false}");
    return ESP_ERR_INVALID_STATE;
  }

  if (puzzle_queue_act(PUZZLE_ACT_START_STAGED) != ESP_OK) {
    puzzle_copy_response(response, response_len,
                         "{\"success\":false,\"error\":\"Queue full\"}");
    return ESP_FAIL;
  }
  puzzle_copy_response(
      response, response_len,
      "{\"success\":true,\"physical_match\":true,\"play_queued\":true}");
  return ESP_OK;
}

/**
 * POST /api/game/puzzle
 * Body: {"action":"load","fen":"...","solution":["g6g8"],"title":"..."}
 *       {"action":"start"} | {"action":"start","id":1}
 *       {"action":"prepare","id":1} | {"action":"cancel"} | {"action":"hint"}
 */
esp_err_t web_post_game_puzzle_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/game/puzzle");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char buf[1536] = {0};
  int r = httpd_req_recv(req, buf, (int)sizeof(buf) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  buf[r] = '\0';
  if (cmd_wire_http_guard(req, buf, "puzzle")) {
    return ESP_OK;
  }

  char response[192];
  response[0] = '\0';
  esp_err_t err = web_server_puzzle_dispatch_body(buf, response, sizeof(response));
  const char *sent =
      response[0] != '\0' ? response : "{\"success\":false,\"error\":\"puzzle\"}";
  if (err == ESP_OK) {
    cmd_wire_http_send(req, "200 OK", sent, buf, true, "puzzle");
    return ESP_OK;
  }
  if (err == ESP_ERR_INVALID_STATE) {
    cmd_wire_http_send(req, "409 Conflict", sent, buf, false, NULL);
    return ESP_OK;
  }
  if (err == ESP_FAIL || err == ESP_ERR_TIMEOUT) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", buf, false, NULL);
    return ESP_OK;
  }
  cmd_wire_http_send(req, "400 Bad Request", sent, buf, false, NULL);
  return ESP_OK;
}

static const char *opening_dispatch_error_key(esp_err_t err) {
  if (err == ESP_ERR_INVALID_STATE) {
    return "mode_conflict";
  }
  if (err == ESP_ERR_NOT_ALLOWED) {
    return "resync_incomplete";
  }
  if (err == ESP_FAIL || err == ESP_ERR_TIMEOUT) {
    return "busy";
  }
  return "invalid_line";
}

/** POST /api/game/opening */
esp_err_t web_post_game_opening_handler(httpd_req_t *req) {
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
  if (cmd_wire_http_guard(req, buf, "opening")) {
    return ESP_OK;
  }

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

  uint8_t action_code = web_opening_action_code(action_j->valuestring);
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
    cmd_wire_http_send(req, "200 OK", "{\"ok\":true}", buf, true, "opening");
    return ESP_OK;
  }
  if (dispatch_err == ESP_ERR_INVALID_STATE) {
    cmd_wire_http_send(req, "409 Conflict",
                       "{\"ok\":false,\"error\":\"mode_conflict\"}", buf, false,
                       NULL);
    return ESP_OK;
  }
  if (dispatch_err == ESP_ERR_NOT_ALLOWED) {
    cmd_wire_http_send(req, "409 Conflict",
                       "{\"ok\":false,\"error\":\"resync_incomplete\"}", buf,
                       false, NULL);
    return ESP_OK;
  }
  if (dispatch_err == ESP_FAIL || dispatch_err == ESP_ERR_TIMEOUT) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", buf, false, NULL);
    return ESP_OK;
  }
  char err_body[96];
  snprintf(err_body, sizeof(err_body), "{\"ok\":false,\"error\":\"%s\"}",
           opening_dispatch_error_key(dispatch_err));
  cmd_wire_http_send(req, "400 Bad Request", err_body, buf, false, NULL);
  return ESP_OK;
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */

/**
 * BLE game commands (move, new game, timers, virtual, tutorial, puzzle, opening).
 * @return ESP_ERR_NOT_FOUND when cmd is not handled here.
 */
esp_err_t web_server_ble_game_command_dispatch(const char *cmd, char *buf) {
  if (strcmp(cmd, "move") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] move blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    // BLE move command: {"cmd": "move", "from": "e2", "to": "e4", "promotion": "q"}
    char from[8] = {0};
    char to[8] = {0};
    char promotion[8] = {0};

    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "Invalid JSON in move command");
      return ESP_FAIL;
    }

    cJSON *from_obj = cJSON_GetObjectItemCaseSensitive(root, "from");
    cJSON *to_obj = cJSON_GetObjectItemCaseSensitive(root, "to");
    cJSON *promo_obj = cJSON_GetObjectItemCaseSensitive(root, "promotion");

    if (cJSON_IsString(from_obj) && cJSON_IsString(to_obj)) {
      strncpy(from, from_obj->valuestring, sizeof(from) - 1);
      strncpy(to, to_obj->valuestring, sizeof(to) - 1);
      from[sizeof(from) - 1] = '\0';
      to[sizeof(to) - 1] = '\0';
      if (cJSON_IsString(promo_obj)) {
        strncpy(promotion, promo_obj->valuestring, sizeof(promotion) - 1);
      }

      /* Same field (double tap in app) — don't send to game_task (MOVE_ERROR_DESTINATION_OCCUPIED). */
      if (strcmp(from, to) == 0) {
        ESP_LOGW(TAG, "[BLE] move: ignored (from==to)");
        cJSON_Delete(root);
        return ESP_OK;
      }

      {
        game_cmd_t move_cmd = {0};
        move_cmd.kind = GCMD_MOVE;
        strncpy(move_cmd.u.move.from, from, sizeof(move_cmd.u.move.from) - 1);
        strncpy(move_cmd.u.move.to, to, sizeof(move_cmd.u.move.to) - 1);
        if (strlen(promotion) > 0) {
          switch (promotion[0]) {
          case 'r':
          case 'R':
            move_cmd.u.move.promotion = 'r';
            break;
          case 'b':
          case 'B':
            move_cmd.u.move.promotion = 'b';
            break;
          case 'n':
          case 'N':
            move_cmd.u.move.promotion = 'n';
            break;
          default:
            move_cmd.u.move.promotion = 'q';
            break;
          }
        }

        if (!game_submit(&move_cmd)) {
          ESP_LOGE(TAG, "[BLE] move: failed to send to game queue");
          cJSON_Delete(root);
          return ESP_ERR_TIMEOUT;
        }
        ESP_LOGI(TAG, "[BLE] move: %s -> %s (promo: %s)", from, to,
                 strlen(promotion) > 0 ? promotion : "none");
      }
    } else {
      ESP_LOGW(TAG, "[BLE] move: missing from/to fields");
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    cJSON_Delete(root);
    return ESP_OK;
  }

  if (strcmp(cmd, "new_game") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] new_game blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    game_cmd_t new_cmd = {0};
    new_cmd.kind = GCMD_NEW_GAME;
    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {
      cJSON *fj = cJSON_GetObjectItemCaseSensitive(root, "fen");
      if (cJSON_IsString(fj) && fj->valuestring != NULL &&
          fj->valuestring[0] != '\0') {
        new_cmd.kind = GCMD_NEW_GAME_FEN;
        strncpy(new_cmd.u.fen.fen, fj->valuestring, sizeof(new_cmd.u.fen.fen) - 1);
      }
      cJSON_Delete(root);
    }
    if (new_cmd.kind == GCMD_NEW_GAME_FEN) {
      game_fen_position_t parsed;
      if (!game_parse_fen(new_cmd.u.fen.fen, &parsed)) {
        ESP_LOGW(TAG, "[BLE] new_game: FEN rejected");
        return ESP_ERR_INVALID_ARG;
      }
    }
    if (game_submit(&new_cmd)) {
      ESP_LOGI(TAG, "[BLE] new_game: kind=%d", (int)new_cmd.kind);
    } else {
      ESP_LOGE(TAG, "[BLE] new_game: failed to send to game queue");
      return ESP_FAIL;
    }
    return ESP_OK;
  }

  if (strcmp(cmd, "resign") == 0) {
    const char *rerr = NULL;
    esp_err_t rr = web_server_resign_dispatch_body(buf, &rerr);
    if (rr == ESP_OK) {
      ESP_LOGI(TAG, "[BLE] resign: command sent");
    } else {
      ESP_LOGW(TAG, "[BLE] resign refused: %s", rerr != NULL ? rerr : "?");
    }
    return rr;
  }

  if (strcmp(cmd, "draw") == 0) {
    const char *derr = NULL;
    esp_err_t dr = web_server_draw_dispatch(&derr);
    if (dr == ESP_OK) {
      ESP_LOGI(TAG, "[BLE] draw: command sent");
    } else {
      ESP_LOGW(TAG, "[BLE] draw refused: %s", derr != NULL ? derr : "?");
    }
    return dr;
  }

  if (strcmp(cmd, "undo") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] undo blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    if (game_move_history_count() == 0) {
      ESP_LOGW(TAG, "[BLE] undo: nothing to undo");
      return ESP_ERR_INVALID_STATE;
    }
    {
      game_cmd_t undo_cmd = {0};
      undo_cmd.kind = GCMD_UNDO;
      if (!game_submit(&undo_cmd)) {
        ESP_LOGE(TAG, "[BLE] undo: failed to send to game queue");
        return ESP_ERR_TIMEOUT;
      }
      ESP_LOGI(TAG, "[BLE] undo: command sent");
    }
    return ESP_OK;
  }

  if (strcmp(cmd, "promotion") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] promotion blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      ESP_LOGE(TAG, "Invalid JSON in promotion command");
      return ESP_FAIL;
    }
    cJSON *choice_obj = cJSON_GetObjectItemCaseSensitive(root, "choice");
    if (cJSON_IsString(choice_obj)) {
      game_cmd_t promo_cmd = {0};
      promo_cmd.kind = GCMD_PROMOTION_CHOICE;
      switch (choice_obj->valuestring[0]) {
      case 'r':
      case 'R':
        promo_cmd.u.promo.piece = 'r';
        break;
      case 'b':
      case 'B':
        promo_cmd.u.promo.piece = 'b';
        break;
      case 'n':
      case 'N':
        promo_cmd.u.promo.piece = 'n';
        break;
      default:
        promo_cmd.u.promo.piece = 'q';
        break;
      }
      if (!game_submit(&promo_cmd)) {
        ESP_LOGE(TAG, "[BLE] promotion: failed to send to game queue");
        cJSON_Delete(root);
        return ESP_ERR_TIMEOUT;
      }
      ESP_LOGI(TAG, "[BLE] promotion: choice %s", choice_obj->valuestring);
    }
    cJSON_Delete(root);
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_config") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_config blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    game_cmd_t tcmd = {0};
    tcmd.kind = GCMD_TIMER_SET;

    char *type_str = strstr(buf, "\"type\":");
    if (type_str == NULL) {
      ESP_LOGW(TAG, "[BLE] timer_config: missing type");
      return ESP_ERR_INVALID_ARG;
    }
    int type_value;
    if (sscanf(type_str, "\"type\":%d", &type_value) != 1) {
      ESP_LOGW(TAG, "[BLE] timer_config: invalid type");
      return ESP_ERR_INVALID_ARG;
    }
    if (type_value < 0 || type_value > 14) {
      ESP_LOGW(TAG, "[BLE] timer_config: type out of range");
      return ESP_ERR_INVALID_ARG;
    }
    tcmd.u.timer.time_control_type = (uint8_t)type_value;

    if (type_value == 14) {
      char *minutes_str = strstr(buf, "\"custom_minutes\":");
      char *increment_str = strstr(buf, "\"custom_increment\":");
      if (minutes_str) {
        int minutes;
        if (sscanf(minutes_str, "\"custom_minutes\":%d", &minutes) == 1) {
          if (minutes >= 1 && minutes <= 180) {
            tcmd.u.timer.custom_minutes = (uint32_t)minutes;
          } else {
            ESP_LOGW(TAG, "[BLE] timer_config: minutes out of range");
            return ESP_ERR_INVALID_ARG;
          }
        }
      }
      if (increment_str) {
        int increment;
        if (sscanf(increment_str, "\"custom_increment\":%d", &increment) ==
            1) {
          if (increment >= 0 && increment <= 60) {
            tcmd.u.timer.custom_increment = (uint32_t)increment;
          } else {
            ESP_LOGW(TAG, "[BLE] timer_config: increment out of range");
            return ESP_ERR_INVALID_ARG;
          }
        }
      }
      if (minutes_str == NULL || increment_str == NULL) {
        ESP_LOGW(TAG, "[BLE] timer_config: custom needs minutes+increment");
        return ESP_ERR_INVALID_ARG;
      }
    }

    if (!game_submit(&tcmd)) {
      ESP_LOGE(TAG, "[BLE] timer_config: queue send failed");
      return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "[BLE] timer_config: type=%d", type_value);
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_pause") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_pause blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    game_cmd_t tcmd = {0};
    tcmd.kind = GCMD_TIMER_PAUSE;
    if (!game_submit(&tcmd)) {
      return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "[BLE] timer_pause");
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_resume") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_resume blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    game_cmd_t tcmd = {0};
    tcmd.kind = GCMD_TIMER_RESUME;
    if (!game_submit(&tcmd)) {
      return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "[BLE] timer_resume");
    return ESP_OK;
  }

  if (strcmp(cmd, "timer_reset") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] timer_reset blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    game_cmd_t tcmd = {0};
    tcmd.kind = GCMD_TIMER_RESET;
    if (!game_submit(&tcmd)) {
      return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "[BLE] timer_reset");
    return ESP_OK;
  }

  if (strcmp(cmd, "virtual_action") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] virtual_action blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    char action[16] = {0};
    char square[8] = {0};
    char choice[8] = {0};

    const char *action_start = strstr(buf, "\"action\"");
    if (action_start != NULL) {
      action_start = strchr(action_start, ':');
      if (action_start != NULL) {
        action_start++;
        while (*action_start == ' ' || *action_start == '\"') {
          action_start++;
        }
        const char *action_end = strchr(action_start, '\"');
        if (action_end != NULL && action_end > action_start) {
          size_t len = (size_t)(action_end - action_start);
          if (len < sizeof(action)) {
            strncpy(action, action_start, len);
          }
        }
      }
    }

    const char *square_start = strstr(buf, "\"square\"");
    if (square_start != NULL) {
      square_start = strchr(square_start, ':');
      if (square_start != NULL) {
        square_start++;
        while (*square_start == ' ' || *square_start == '\"') {
          square_start++;
        }
        const char *square_end = strchr(square_start, '\"');
        if (square_end != NULL && square_end > square_start) {
          size_t len = (size_t)(square_end - square_start);
          if (len < sizeof(square)) {
            strncpy(square, square_start, len);
          }
        }
      }
    }

    const char *choice_start = strstr(buf, "\"choice\"");
    if (choice_start != NULL) {
      choice_start = strchr(choice_start, ':');
      if (choice_start != NULL) {
        choice_start++;
        while (*choice_start == ' ' || *choice_start == '\"') {
          choice_start++;
        }
        const char *choice_end = strchr(choice_start, '\"');
        if (choice_end != NULL && choice_end > choice_start) {
          size_t len = (size_t)(choice_end - choice_start);
          if (len < sizeof(choice)) {
            strncpy(choice, choice_start, len);
          }
        }
      }
    }

    if (strlen(action) == 0) {
      ESP_LOGW(TAG, "[BLE] virtual_action: missing action");
      return ESP_ERR_INVALID_ARG;
    }

    game_cmd_t vacmd = {0};
    if (strcmp(action, "pickup") == 0) {
      vacmd.kind = GCMD_BOARD_LIFT;
    } else if (strcmp(action, "drop") == 0) {
      vacmd.kind = GCMD_BOARD_DROP;
    } else if (strcmp(action, "promote") == 0) {
      vacmd.kind = GCMD_PROMOTION_CHOICE;
    } else {
      ESP_LOGW(TAG, "[BLE] virtual_action: invalid action");
      return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(action, "pickup") == 0) {
      if (strlen(square) == 0) {
        ESP_LOGW(TAG, "[BLE] virtual_action: pickup needs square");
        return ESP_ERR_INVALID_ARG;
      }
      strncpy(vacmd.u.lift.square, square, sizeof(vacmd.u.lift.square) - 1);
    } else if (strcmp(action, "drop") == 0) {
      if (strlen(square) == 0) {
        ESP_LOGW(TAG, "[BLE] virtual_action: drop needs square");
        return ESP_ERR_INVALID_ARG;
      }
      strncpy(vacmd.u.drop.to, square, sizeof(vacmd.u.drop.to) - 1);
    } else if (strcmp(action, "promote") == 0) {
      if (strlen(choice) == 0) {
        strcpy(choice, "Q");
      }
      if (strcasecmp(choice, "R") == 0) {
        vacmd.u.promo.piece = 'r';
      } else if (strcasecmp(choice, "B") == 0) {
        vacmd.u.promo.piece = 'b';
      } else if (strcasecmp(choice, "N") == 0) {
        vacmd.u.promo.piece = 'n';
      } else {
        vacmd.u.promo.piece = 'q';
      }
    }

    if (!game_submit(&vacmd)) {
      ESP_LOGE(TAG, "[BLE] virtual_action: queue send failed");
      return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(TAG, "[BLE] virtual_action: %s", action);
    return ESP_OK;
  }

  if (strcmp(cmd, "setup_tutorial") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *aj = cJSON_GetObjectItemCaseSensitive(root, "action");
    const char *act = NULL;
    if (cJSON_IsString(aj) && aj->valuestring != NULL) {
      act = aj->valuestring;
    }
    bool want_start = (act != NULL && strcmp(act, "start") == 0);
    bool want_cancel = (act != NULL && strcmp(act, "cancel") == 0);
    bool want_finish = (act != NULL && strcmp(act, "finish") == 0);
    int nact = (want_start ? 1 : 0) + (want_cancel ? 1 : 0) +
               (want_finish ? 1 : 0);
    cJSON_Delete(root);
    if (nact != 1) {
      return ESP_ERR_INVALID_ARG;
    }

    if (want_start || want_cancel) {
      setup_tutorial_reset_finish_cooldown();
    }

    if (want_finish) {
      if (!game_is_board_setup_tutorial_active()) {
        return ESP_ERR_INVALID_ARG;
      }
      if (setup_tutorial_finish_in_cooldown()) {
        return ESP_ERR_NOT_FINISHED;
      }
      if (!game_is_physical_board_starting_occupancy()) {
        setup_tutorial_note_finish_conflict();
        return ESP_ERR_NOT_FINISHED;
      }
      setup_tutorial_reset_finish_cooldown();
    }

    if (game_command_queue == NULL) {
      ESP_LOGE(TAG, "[BLE] setup_tutorial: game_command_queue NULL");
      return ESP_FAIL;
    }
    game_cmd_t gcmd = {0};
    gcmd.kind = GCMD_MODE_TUTORIAL;
    gcmd.u.mode.act = want_start ? (uint8_t)TUTORIAL_ACT_START
                                 : (want_finish ? (uint8_t)TUTORIAL_ACT_FINISH
                                                : (uint8_t)TUTORIAL_ACT_CANCEL);
    if (!game_submit(&gcmd)) {
      ESP_LOGE(TAG, "[BLE] setup_tutorial: queue send failed");
      return ESP_ERR_TIMEOUT;
    }
    if (want_cancel) {
      (void)web_game_submit_hint_clear();
    }
    ESP_LOGI(TAG, "[BLE] setup_tutorial action=%s queued",
             want_start ? "start" : (want_finish ? "finish" : "cancel"));
    return ESP_OK;
  }

  if (strcmp(cmd, "puzzle") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] puzzle blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    esp_err_t perr = web_server_puzzle_dispatch_body(buf, NULL, 0);
    if (perr == ESP_OK) {
      ESP_LOGI(TAG, "[BLE] puzzle dispatched");
      return ESP_OK;
    }
    if (perr == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "[BLE] puzzle physical mismatch or hint unavailable");
      return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGW(TAG, "[BLE] puzzle dispatch failed: %s", esp_err_to_name(perr));
    return perr;
  }

  if (strcmp(cmd, "opening") == 0) {
    esp_err_t oerr = web_server_opening_dispatch_body(buf);
    if (oerr == ESP_OK) {
      ESP_LOGI(TAG, "[BLE] opening dispatched");
      return ESP_OK;
    }
    if (oerr == ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "[BLE] opening blocked (locked or mode conflict)");
      return ESP_ERR_INVALID_STATE;
    }
    if (oerr == ESP_ERR_NOT_ALLOWED) {
      ESP_LOGW(TAG, "[BLE] opening checkpoint resync incomplete");
      return ESP_ERR_NOT_ALLOWED;
    }
    ESP_LOGW(TAG, "[BLE] opening dispatch failed: %s", esp_err_to_name(oerr));
    return oerr;
  }

  return ESP_ERR_NOT_FOUND;
}

