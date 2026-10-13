/**
 * @file web_ws.c
 * @brief WebSocket handler and snapshot broadcast (/ws).
 */

#include "web_server_task.h"
#include "web_server_internal.h"
#include "board_api_auth.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "WEB_WS";

#if CONFIG_HTTPD_WS_SUPPORT
static esp_timer_handle_t ws_broadcast_timer = NULL;

static bool ws_query_token(httpd_req_t *req, uint8_t out[BOARD_PAIR_TOKEN_LEN]) {
  char query[160];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    return false;
  }
  char hex[80];
  if (httpd_query_key_value(query, "token", hex, sizeof(hex)) != ESP_OK) {
    return false;
  }
  size_t n = strlen(hex);
  if (n != 64) {
    return false;
  }
  for (size_t i = 0; i < 32; i++) {
    int hi = -1;
    int lo = -1;
    char a = hex[i * 2];
    char b = hex[i * 2 + 1];
    if (a >= '0' && a <= '9') hi = a - '0';
    else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
    else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
    if (b >= '0' && b <= '9') lo = b - '0';
    else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
    else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

static void ws_send_text(httpd_req_t *req, const char *msg) {
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = (uint8_t *)msg;
  frame.len = strlen(msg);
  (void)httpd_ws_send_frame(req, &frame);
}

static bool json_hex_field(const char *json, const char *key, uint8_t out[32]) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(json, pattern);
  if (p == NULL) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t' || *p == '"') {
    p++;
  }
  for (size_t i = 0; i < 32; i++) {
    int hi = -1;
    int lo = -1;
    char a = p[i * 2];
    char b = p[i * 2 + 1];
    if (a >= '0' && a <= '9') hi = a - '0';
    else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
    else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
    if (b >= '0' && b <= '9') lo = b - '0';
    else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
    else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

static bool json_string_field(const char *json, const char *key, char *dst,
                              size_t cap) {
  char pattern[24];
  snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char *p = strstr(json, pattern);
  if (p == NULL || cap == 0) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p == ' ' || *p == '\t' || *p == '"') {
    p++;
  }
  size_t n = 0;
  while (p[n] != '\0' && p[n] != '"' && n + 1 < cap) {
    dst[n] = p[n];
    n++;
  }
  dst[n] = '\0';
  return n > 0;
}
#endif

#if CONFIG_HTTPD_WS_SUPPORT
#define WS_MAX_CLIENT_FDS 32

static struct {
  int fd;
  uint8_t token[BOARD_PAIR_TOKEN_LEN];
  bool used;
} s_ws_token[8];

static void ws_remember_token(int fd, const uint8_t token[BOARD_PAIR_TOKEN_LEN]) {
  int free_slot = -1;
  for (int i = 0; i < 8; i++) {
    if (s_ws_token[i].used && s_ws_token[i].fd == fd) {
      memcpy(s_ws_token[i].token, token, BOARD_PAIR_TOKEN_LEN);
      return;
    }
    if (!s_ws_token[i].used && free_slot < 0) {
      free_slot = i;
    }
  }
  if (free_slot < 0) {
    free_slot = 0;
  }
  s_ws_token[free_slot].fd = fd;
  s_ws_token[free_slot].used = true;
  memcpy(s_ws_token[free_slot].token, token, BOARD_PAIR_TOKEN_LEN);
}

static bool ws_recall_token(int fd, uint8_t out[BOARD_PAIR_TOKEN_LEN]) {
  for (int i = 0; i < 8; i++) {
    if (s_ws_token[i].used && s_ws_token[i].fd == fd) {
      memcpy(out, s_ws_token[i].token, BOARD_PAIR_TOKEN_LEN);
      return true;
    }
  }
  return false;
}

static void ws_forget_token(int fd) {
  for (int i = 0; i < 8; i++) {
    if (s_ws_token[i].used && s_ws_token[i].fd == fd) {
      s_ws_token[i].used = false;
    }
  }
}

esp_err_t http_ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    /* Snapshot clients stay anonymous. A command client may pass ?token=
     * on this same socket; the token is remembered for later command frames.
     * Snapshot push does not require it. */
    uint8_t token[BOARD_PAIR_TOKEN_LEN];
    if (ws_query_token(req, token)) {
      int fd = httpd_req_to_sockfd(req);
      ws_remember_token(fd, token);
      /* The open socket is the commander's life. A second token still
       * receives snapshots; it just does not take the seat. */
      int verdict = board_pair_ws_command(token, fd);
      if (verdict == BOARD_PAIR_CMD_BUSY) {
        ESP_LOGW(TAG, "[STAGING] ws snapshot open, command seat busy");
      }
    }
    ESP_LOGD(TAG, "WebSocket handshake OK (/ws)");
    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(ws_pkt));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGD(TAG, "ws recv (len): %s", esp_err_to_name(ret));
    return ret;
  }
  if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
    int fd = httpd_req_to_sockfd(req);
    ws_forget_token(fd);
    board_pair_ws_release_fd(fd);
    return ESP_OK;
  }
  if (ws_pkt.len == 0) {
    return ESP_OK;
  }
  uint8_t *buf = calloc(1, ws_pkt.len + 1);
  if (buf == NULL) {
    return ESP_ERR_NO_MEM;
  }
  ws_pkt.payload = buf;
  ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
  if (ret != ESP_OK) {
    free(buf);
    ESP_LOGD(TAG, "ws recv payload: %s", esp_err_to_name(ret));
    return ret;
  }
  buf[ws_pkt.len] = '\0';
  const char *text = (const char *)buf;
  bool command = strstr(text, "\"from\"") != NULL || strstr(text, "\"cmd\"") != NULL;
  if (!command) {
    free(buf);
    return ESP_OK;
  }
  uint8_t token[BOARD_PAIR_TOKEN_LEN];
  int fd = httpd_req_to_sockfd(req);
  bool have = json_hex_field(text, "token", token) || ws_recall_token(fd, token) ||
              ws_query_token(req, token);
  int verdict = have ? board_pair_ws_command(token, fd) : BOARD_PAIR_CMD_NEED_TOKEN;
  if (verdict == BOARD_PAIR_CMD_NEED_TOKEN) {
    ws_send_text(req,
                 "{\"success\":false,\"error\":\"api_token_required\","
                 "\"message\":\"Board command token required.\"}");
    free(buf);
    return ESP_OK;
  }
  if (verdict == BOARD_PAIR_CMD_BUSY) {
    ws_send_text(req,
                 "{\"success\":false,\"error\":\"board_busy\","
                 "\"message\":\"Board is connected to another device.\"}");
    free(buf);
    return ESP_OK;
  }
  if (web_is_locked()) {
    ws_send_text(req,
                 "{\"success\":false,\"error\":\"web_locked\","
                 "\"message\":\"Web interface is locked.\"}");
    free(buf);
    return ESP_OK;
  }
  char from[8];
  char to[8];
  char promo[4];
  from[0] = to[0] = promo[0] = '\0';
  bool is_move = json_string_field(text, "from", from, sizeof(from)) &&
                 json_string_field(text, "to", to, sizeof(to));
  if (!is_move) {
    ws_send_text(req,
                 "{\"success\":false,\"error\":\"unknown_command\","
                 "\"message\":\"This socket accepts move commands.\"}");
    free(buf);
    return ESP_OK;
  }
  (void)json_string_field(text, "promotion", promo, sizeof(promo));
  char err[96];
  bool async_ok = false;
  esp_err_t qerr =
      web_game_enqueue_move(from, to, promo, err, sizeof(err), &async_ok);
  if (qerr == ESP_OK) {
    ws_send_text(req, async_ok
                          ? "{\"success\":true,\"message\":\"Move queued\"}"
                          : "{\"success\":true,\"message\":\"Move processed\"}");
  } else if (qerr == ESP_ERR_INVALID_ARG) {
    char body[180];
    snprintf(body, sizeof(body),
             "{\"success\":false,\"error\":\"illegal\",\"message\":\"%s\"}",
             err[0] != '\0' ? err : "Illegal move");
    ws_send_text(req, body);
  } else {
    ws_send_text(req, "{\"success\":false,\"error\":\"queue\"}");
  }
  free(buf);
  return ESP_OK;
}

static bool ws_has_clients(void) {
  if (web_server_get_httpd_handle() == NULL) {
    return false;
  }
  size_t n = WS_MAX_CLIENT_FDS;
  int fds[WS_MAX_CLIENT_FDS];
  if (httpd_get_client_list(web_server_get_httpd_handle(), &n, fds) != ESP_OK) {
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    if (httpd_ws_get_fd_info(web_server_get_httpd_handle(), fds[i]) ==
        HTTPD_WS_CLIENT_WEBSOCKET) {
      return true;
    }
  }
  return false;
}

void ws_broadcast_snapshot(void) {
  if (web_server_get_httpd_handle() == NULL || !web_server_is_active()) {
    return;
  }
  if (!ws_has_clients()) {
    return;
  }
  (void)web_server_task_wdt_reset_safe();
  size_t len = 0;
  if (build_snapshot_json(&len) != ESP_OK) {
    ESP_LOGD(TAG, "WS broadcast: build_snapshot_json failed");
    return;
  }
  uint8_t *payload = (uint8_t *)malloc(len);
  if (payload == NULL) {
    ESP_LOGE(TAG, "WS broadcast: malloc %u B failed", (unsigned)len);
    return;
  }
  memcpy(payload, snapshot_buffer, len);
  httpd_ws_frame_t ws_pkt = {.type = HTTPD_WS_TYPE_TEXT,
                             .payload = payload,
                             .len = len};
  size_t n = WS_MAX_CLIENT_FDS;
  int fds[WS_MAX_CLIENT_FDS];
  if (httpd_get_client_list(web_server_get_httpd_handle(), &n, fds) != ESP_OK) {
    free(payload);
    return;
  }
  size_t ws_count = 0;
  int ws_fds[WS_MAX_CLIENT_FDS];
  for (size_t i = 0; i < n; i++) {
    if (httpd_ws_get_fd_info(web_server_get_httpd_handle(), fds[i]) !=
        HTTPD_WS_CLIENT_WEBSOCKET) {
      continue;
    }
    if (ws_count < WS_MAX_CLIENT_FDS) {
      ws_fds[ws_count] = fds[i];
    }
    ws_count++;
    (void)web_server_task_wdt_reset_safe();
    esp_err_t err = httpd_ws_send_data(web_server_get_httpd_handle(), fds[i], &ws_pkt);
    if (err != ESP_OK) {
      ESP_LOGD(TAG, "WS send fd %d: %s", fds[i], esp_err_to_name(err));
    }
  }
  board_pair_ws_prune(ws_fds, (int)(ws_count < WS_MAX_CLIENT_FDS ? ws_count
                                                                  : WS_MAX_CLIENT_FDS));
  ESP_LOGD(TAG, "[STAGING] WS broadcast → %u WS client(s), %u B payload",
           (unsigned)ws_count, (unsigned)len);
  free(payload);
}

static void ws_broadcast_timer_cb(void *arg) {
  (void)arg;
  ESP_LOGD(TAG,
           "[STAGING] WS watchdog tick → defer snapshot (esp_timer stack too small)");
  czechmate_ensure_snapshot_notify_queue();
  if (snapshot_notify_queue == NULL) {
    ESP_LOGW(TAG, "WS watchdog: snapshot_notify_queue missing, skip");
    return;
  }
  uint8_t ping = 1;
  if (xQueueSend(snapshot_notify_queue, &ping, 0) != pdTRUE) {
    ESP_LOGD(TAG, "[STAGING] WS watchdog: notify coalesced (queue full)");
  }
}
#endif /* CONFIG_HTTPD_WS_SUPPORT */

// ============================================================================
// WEBSOCKET (snapshot push — same JSON as GET /api/game/snapshot)
// ============================================================================

void web_server_websocket_init(void) {
#if CONFIG_HTTPD_WS_SUPPORT
  if (ws_broadcast_timer != NULL) {
    return;
  }
  const esp_timer_create_args_t args = {.callback = &ws_broadcast_timer_cb,
                                        .name = "ws_snap"};
  esp_err_t e = esp_timer_create(&args, &ws_broadcast_timer);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "WS timer create failed: %s", esp_err_to_name(e));
    return;
  }
  e = esp_timer_start_periodic(ws_broadcast_timer, 3000 * 1000);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "WS timer start failed: %s", esp_err_to_name(e));
    esp_timer_delete(ws_broadcast_timer);
    ws_broadcast_timer = NULL;
    return;
  }
  czechmate_ensure_snapshot_notify_queue();
  ESP_LOGI(TAG,
           "WebSocket: watchdog broadcast 3 s + immediate push on state change");
#else
  ESP_LOGD(TAG, "WebSocket disabled (CONFIG_HTTPD_WS_SUPPORT)");
#endif
}

void web_server_websocket_send_update(const char *data) {
  (void)data;
  if (!web_server_is_active()) {
    return;
  }
#if CONFIG_HTTPD_WS_SUPPORT
  ws_broadcast_snapshot();
#endif
}

void web_ws_shutdown(void) {
#if CONFIG_HTTPD_WS_SUPPORT
  if (ws_broadcast_timer != NULL) {
    esp_timer_stop(ws_broadcast_timer);
    esp_timer_delete(ws_broadcast_timer);
    ws_broadcast_timer = NULL;
  }
#endif
}
