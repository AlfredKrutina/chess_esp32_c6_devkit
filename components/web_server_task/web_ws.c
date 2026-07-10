/**
 * @file web_ws.c
 * @brief WebSocket handler and snapshot broadcast (/ws).
 */

#include "web_server_task.h"
#include "web_server_internal.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "WEB_WS";

#if CONFIG_HTTPD_WS_SUPPORT
static esp_timer_handle_t ws_broadcast_timer = NULL;
#endif

#if CONFIG_HTTPD_WS_SUPPORT
#define WS_MAX_CLIENT_FDS 32

esp_err_t http_ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
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
  if (ws_pkt.len) {
    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    free(buf);
    if (ret != ESP_OK) {
      ESP_LOGD(TAG, "ws recv payload: %s", esp_err_to_name(ret));
      return ret;
    }
  }
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
  for (size_t i = 0; i < n; i++) {
    if (httpd_ws_get_fd_info(web_server_get_httpd_handle(), fds[i]) !=
        HTTPD_WS_CLIENT_WEBSOCKET) {
      continue;
    }
    ws_count++;
    (void)web_server_task_wdt_reset_safe();
    esp_err_t err = httpd_ws_send_data(web_server_get_httpd_handle(), fds[i], &ws_pkt);
    if (err != ESP_OK) {
      ESP_LOGD(TAG, "WS send fd %d: %s", fds[i], esp_err_to_name(err));
    }
  }
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
// WEBSOCKET (snapshot push — stejný JSON jako GET /api/game/snapshot)
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
           "WebSocket: watchdog broadcast 3 s + okamžitý push při změně stavu");
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
