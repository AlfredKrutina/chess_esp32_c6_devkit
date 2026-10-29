/**
 * @file cmd_wire.c
 * @brief One write session and one cached command id for BLE and HTTP.
 */

#include "cmd_wire.h"

#include "ble_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

static bool s_active;
static uint32_t s_epoch;
static char s_transport[8];

static bool s_cache_valid;
static uint32_t s_cache_id;
static char s_cache_cmd[40];

static uint32_t s_req_id;
static bool s_req_has_id;
static bool s_req_cacheable;
static char s_req_cmd[40];

static void lock(void) { portENTER_CRITICAL(&s_lock); }
static void unlock(void) { portEXIT_CRITICAL(&s_lock); }

static void cache_clear_locked(void) {
  s_cache_valid = false;
  s_cache_id = 0;
  s_cache_cmd[0] = '\0';
}

bool cmd_wire_json_u32(const char *json, const char *key, uint32_t *out) {
  char pat[40];
  const char *p;
  char *end = NULL;
  unsigned long v;

  if (json == NULL || key == NULL || out == NULL) {
    return false;
  }
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  p = strstr(json, pat);
  if (p == NULL) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p != '\0' && isspace((unsigned char)*p)) {
    p++;
  }
  if (*p == '"') {
    return false;
  }
  v = strtoul(p, &end, 10);
  if (end == p) {
    return false;
  }
  *out = (uint32_t)v;
  return true;
}

bool cmd_wire_correlation_id(const char *json, uint32_t *out) {
  uint32_t req = 0;
  uint32_t id = 0;
  if (out == NULL) {
    return false;
  }
  if (cmd_wire_json_u32(json, "req", &req) && req != 0) {
    *out = req;
    return true;
  }
  if (cmd_wire_json_u32(json, "id", &id) && id != 0) {
    *out = id;
    return true;
  }
  return false;
}

bool cmd_wire_json_string(const char *json, const char *key, char *out,
                          size_t out_cap) {
  char pat[40];
  const char *p;
  size_t i = 0;

  if (json == NULL || key == NULL || out == NULL || out_cap == 0) {
    return false;
  }
  out[0] = '\0';
  snprintf(pat, sizeof(pat), "\"%s\"", key);
  p = strstr(json, pat);
  if (p == NULL) {
    return false;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return false;
  }
  p++;
  while (*p != '\0' && isspace((unsigned char)*p)) {
    p++;
  }
  if (*p != '"') {
    return false;
  }
  p++;
  while (*p != '\0' && *p != '"' && i + 1 < out_cap) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0 && *p == '"';
}

bool cmd_wire_is_session_exempt(const char *cmd) {
  if (cmd == NULL) {
    return true;
  }
  return strcmp(cmd, "ping") == 0 || strcmp(cmd, "wifi_survey") == 0 ||
         strcmp(cmd, "ota_ble_status") == 0 || strcmp(cmd, "pair_token") == 0 ||
         strcmp(cmd, "session_claim") == 0 ||
         strcmp(cmd, "session_release") == 0;
}

static bool claim_locked(uint32_t epoch, const char *transport) {
  if (transport == NULL || epoch == 0) {
    return false;
  }
  if (strcmp(transport, "ble") != 0 && strcmp(transport, "wifi") != 0) {
    return false;
  }
  if (!s_active || epoch > s_epoch) {
    s_active = true;
    s_epoch = epoch;
    strncpy(s_transport, transport, sizeof(s_transport) - 1);
    s_transport[sizeof(s_transport) - 1] = '\0';
    cache_clear_locked();
    return true;
  }
  return epoch == s_epoch && strcmp(s_transport, transport) == 0;
}

static bool release_locked(bool has_epoch, uint32_t epoch) {
  if (!s_active) {
    return true;
  }
  if (!has_epoch || epoch != s_epoch) {
    return false;
  }
  s_active = false;
  s_transport[0] = '\0';
  cache_clear_locked();
  return true;
}

static bool epoch_ok_locked(const char *cmd, bool has_epoch, uint32_t epoch) {
  if (cmd_wire_is_session_exempt(cmd) || !s_active) {
    return true;
  }
  return has_epoch && epoch == s_epoch;
}

static bool cache_hit_locked(uint32_t id, const char *cmd) {
  return s_cache_valid && id != 0 && id == s_cache_id && cmd != NULL &&
         strcmp(s_cache_cmd, cmd) == 0;
}

static void cache_store_locked(uint32_t id, const char *cmd) {
  if (id == 0 || cmd == NULL || cmd[0] == '\0') {
    return;
  }
  s_cache_id = id;
  strncpy(s_cache_cmd, cmd, sizeof(s_cache_cmd) - 1);
  s_cache_cmd[sizeof(s_cache_cmd) - 1] = '\0';
  s_cache_valid = true;
}

cmd_wire_gate_t cmd_wire_gate(const char *cmd, const char *json,
                              esp_err_t *out_err) {
  uint32_t id = 0;
  uint32_t epoch = 0;
  bool has_id;
  bool has_epoch;
  char transport[8];
  cmd_wire_gate_t result = CMD_WIRE_PROCEED;
  esp_err_t err = ESP_OK;

  has_id = cmd_wire_correlation_id(json, &id);
  has_epoch = cmd_wire_json_u32(json, "epoch", &epoch);
  transport[0] = '\0';
  (void)cmd_wire_json_string(json, "transport", transport, sizeof(transport));

  lock();
  s_req_id = has_id ? id : 0;
  s_req_has_id = has_id;
  s_req_cacheable = cmd != NULL && !cmd_wire_is_session_exempt(cmd);
  s_req_cmd[0] = '\0';
  if (cmd != NULL) {
    strncpy(s_req_cmd, cmd, sizeof(s_req_cmd) - 1);
    s_req_cmd[sizeof(s_req_cmd) - 1] = '\0';
  }

  if (cmd != NULL && strcmp(cmd, "session_claim") == 0) {
    err = claim_locked(epoch, transport) ? ESP_OK : ESP_ERR_INVALID_VERSION;
    result = (err == ESP_OK) ? CMD_WIRE_DONE_OK : CMD_WIRE_DONE_ERR;
  } else if (cmd != NULL && strcmp(cmd, "session_release") == 0) {
    err = release_locked(has_epoch, epoch) ? ESP_OK : ESP_ERR_INVALID_VERSION;
    result = (err == ESP_OK) ? CMD_WIRE_DONE_OK : CMD_WIRE_DONE_ERR;
  } else if (!epoch_ok_locked(cmd, has_epoch, epoch)) {
    err = ESP_ERR_INVALID_VERSION;
    result = CMD_WIRE_DONE_ERR;
  } else if (s_req_cacheable && has_id && cache_hit_locked(id, cmd)) {
    err = ESP_OK;
    result = CMD_WIRE_DONE_OK;
  }
  unlock();

  if (out_err != NULL) {
    *out_err = err;
  }
  return result;
}

void cmd_wire_note_dispatch_result(esp_err_t err) {
  lock();
  if (err == ESP_OK && s_req_cacheable && s_req_has_id) {
    cache_store_locked(s_req_id, s_req_cmd);
  }
  unlock();
}

void cmd_wire_on_ble_disconnect(void) {
  lock();
  if (s_active && strcmp(s_transport, "ble") == 0) {
    s_active = false;
    s_transport[0] = '\0';
    cache_clear_locked();
  }
  unlock();
}

bool cmd_wire_prefix_id(char *dst, size_t cap, const char *json, uint32_t id,
                        bool has_id) {
  int n;

  if (dst == NULL || cap == 0) {
    return false;
  }
  if (!has_id || json == NULL || json[0] != '{') {
    if (json == NULL) {
      dst[0] = '\0';
      return false;
    }
    n = snprintf(dst, cap, "%s", json);
    return n > 0 && (size_t)n < cap;
  }
  n = snprintf(dst, cap, "{\"id\":%" PRIu32 ",%s", id, json + 1);
  return n > 0 && (size_t)n < cap;
}

void cmd_wire_notify_with_id(const char *json, uint32_t id, bool has_id) {
  char stacked[512];
  size_t need;
  char *heap = NULL;
  const char *out = json;

  if (json == NULL) {
    return;
  }
  if (has_id && json[0] == '{') {
    need = strlen(json) + 24;
    if (need <= sizeof(stacked) &&
        cmd_wire_prefix_id(stacked, sizeof(stacked), json, id, true)) {
      out = stacked;
    } else {
      heap = (char *)malloc(need);
      if (heap != NULL && cmd_wire_prefix_id(heap, need, json, id, true)) {
        out = heap;
      }
    }
  }
  ble_task_notify_cmd_ack_json(out);
  free(heap);
}

void cmd_wire_copy_request_id(uint32_t *id, bool *has_id) {
  lock();
  if (id != NULL) {
    *id = s_req_id;
  }
  if (has_id != NULL) {
    *has_id = s_req_has_id;
  }
  unlock();
}

void cmd_wire_notify_with_current(const char *json) {
  uint32_t id = 0;
  bool has_id = false;
  cmd_wire_copy_request_id(&id, &has_id);
  cmd_wire_notify_with_id(json, id, has_id);
}

static void http_remember(const char *request, const char *cmd) {
  uint32_t id = 0;
  if (cmd == NULL || !cmd_wire_correlation_id(request, &id)) {
    return;
  }
  lock();
  cache_store_locked(id, cmd);
  unlock();
}

bool cmd_wire_http_guard(httpd_req_t *req, const char *body, const char *cmd) {
  uint32_t epoch = 0;
  uint32_t id = 0;
  bool has_epoch;
  bool has_id;
  bool stale = false;
  bool replay = false;

  if (req == NULL) {
    return false;
  }
  has_epoch = cmd_wire_json_u32(body, "epoch", &epoch);
  has_id = cmd_wire_correlation_id(body, &id);

  lock();
  if (!epoch_ok_locked(cmd, has_epoch, epoch)) {
    stale = true;
  } else if (!cmd_wire_is_session_exempt(cmd) && has_id &&
             cache_hit_locked(id, cmd)) {
    replay = true;
  }
  unlock();

  if (stale) {
    cmd_wire_http_send(req, "409 Conflict",
                       "{\"ok\":false,\"error\":\"stale\"}", body, false, NULL);
    return true;
  }
  if (replay) {
    cmd_wire_http_send(req, "200 OK", "{\"ok\":true,\"code\":\"ok\"}", body,
                       false, NULL);
    return true;
  }
  return false;
}

void cmd_wire_http_send(httpd_req_t *req, const char *status, const char *json,
                        const char *request, bool remember, const char *cmd) {
  char stamped[768];
  const char *out = json;
  uint32_t id = 0;
  bool has_id;

  if (req == NULL || json == NULL) {
    return;
  }
  has_id = request != NULL && cmd_wire_correlation_id(request, &id);
  if (has_id && cmd_wire_prefix_id(stamped, sizeof(stamped), json, id, true)) {
    out = stamped;
    if (remember) {
      http_remember(request, cmd);
    }
  }
  if (status != NULL) {
    httpd_resp_set_status(req, status);
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
}

static bool http_take_body(httpd_req_t *req, char *body, size_t cap) {
  int n;
  if (req == NULL || body == NULL || cap < 2) {
    return false;
  }
  body[0] = '\0';
  if (req->content_len == 0) {
    return true;
  }
  if (req->content_len >= cap) {
    return false;
  }
  n = httpd_req_recv(req, body, cap - 1);
  if (n <= 0) {
    return false;
  }
  body[n] = '\0';
  return true;
}

static esp_err_t http_session_handler(httpd_req_t *req, bool claim) {
  char body[128];
  uint32_t epoch = 0;
  char transport[8];
  bool has_epoch;
  bool ok;

  if (!http_take_body(req, body, sizeof(body))) {
    cmd_wire_http_send(req, "400 Bad Request", "{\"ok\":false,\"error\":\"json\"}",
                       NULL, false, NULL);
    return ESP_OK;
  }
  has_epoch = cmd_wire_json_u32(body, "epoch", &epoch);
  transport[0] = '\0';
  (void)cmd_wire_json_string(body, "transport", transport, sizeof(transport));

  lock();
  if (claim) {
    ok = claim_locked(epoch, transport);
  } else {
    ok = release_locked(has_epoch, epoch);
  }
  unlock();

  if (!ok) {
    cmd_wire_http_send(req, "409 Conflict", "{\"ok\":false,\"error\":\"stale\"}",
                       body, false, NULL);
    return ESP_OK;
  }
  cmd_wire_http_send(req, "200 OK", "{\"ok\":true,\"code\":\"ok\"}", body, false,
                     NULL);
  return ESP_OK;
}

esp_err_t cmd_wire_http_claim_handler(httpd_req_t *req) {
  return http_session_handler(req, true);
}

esp_err_t cmd_wire_http_release_handler(httpd_req_t *req) {
  return http_session_handler(req, false);
}
