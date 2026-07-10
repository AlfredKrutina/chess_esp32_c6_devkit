/**
 * @file board_api_auth.c
 * @brief NVS API token + Bearer kontrola pro admin HTTP endpointy.
 */
#include "board_api_auth.h"

#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "esp_http_server.h"
#endif
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "web_server_task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "board_api_auth";

#define BOARD_API_NVS_NS "web_config"
#define BOARD_API_NVS_KEY_TOKEN "api_tok"

static uint8_t s_token[32];
static bool s_loaded;

static bool ct_memeq(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) {
    d |= (uint8_t)(a[i] ^ b[i]);
  }
  return d == 0;
}

static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return (int)(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return (int)(c - 'a') + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return (int)(c - 'A') + 10;
  }
  return -1;
}

static bool parse_hex32(const char *hex, uint8_t *out32) {
  if (hex == NULL) {
    return false;
  }
  size_t len = strlen(hex);
  if (len != 64) {
    return false;
  }
  for (size_t i = 0; i < 32; i++) {
    int hi = hex_nibble(hex[i * 2]);
    int lo = hex_nibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out32[i] = (uint8_t)((unsigned)hi << 4 | (unsigned)lo);
  }
  return true;
}

esp_err_t board_api_auth_init(void) {
  nvs_handle_t h;
  esp_err_t e = nvs_open(BOARD_API_NVS_NS, NVS_READWRITE, &h);
  if (e != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open: %s", esp_err_to_name(e));
    return e;
  }

  size_t sz = sizeof(s_token);
  e = nvs_get_blob(h, BOARD_API_NVS_KEY_TOKEN, s_token, &sz);
  if (e == ESP_ERR_NVS_NOT_FOUND || sz != sizeof(s_token)) {
    esp_fill_random(s_token, sizeof(s_token));
    e = nvs_set_blob(h, BOARD_API_NVS_KEY_TOKEN, s_token, sizeof(s_token));
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "nvs_set_blob token: %s", esp_err_to_name(e));
      nvs_close(h);
      return e;
    }
    e = nvs_commit(h);
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "nvs_commit: %s", esp_err_to_name(e));
      nvs_close(h);
      return e;
    }
#if CHESS_DEBUG_MODE
    ESP_LOGW(TAG, "[STAGING] Generated new board API token (use UART API_TOKEN)");
#endif
  } else if (e != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_blob token: %s", esp_err_to_name(e));
    nvs_close(h);
    return e;
  }

  nvs_close(h);
  s_loaded = true;
  return ESP_OK;
}

esp_err_t board_api_auth_get_token_hex(char *out_hex, size_t cap) {
  if (!s_loaded || out_hex == NULL || cap < 65) {
    return ESP_ERR_INVALID_ARG;
  }
  for (size_t i = 0; i < sizeof(s_token); i++) {
    snprintf(out_hex + i * 2, cap - i * 2, "%02X", (unsigned)s_token[i]);
  }
  out_hex[64] = '\0';
  return ESP_OK;
}

esp_err_t board_api_auth_rotate_token(void) {
  nvs_handle_t h;
  esp_err_t e = nvs_open(BOARD_API_NVS_NS, NVS_READWRITE, &h);
  if (e != ESP_OK) {
    return e;
  }
  esp_fill_random(s_token, sizeof(s_token));
  e = nvs_set_blob(h, BOARD_API_NVS_KEY_TOKEN, s_token, sizeof(s_token));
  if (e == ESP_OK) {
    e = nvs_commit(h);
  }
  nvs_close(h);
  if (e == ESP_OK) {
    s_loaded = true;
  }
  return e;
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
static bool bearer_matches_request(httpd_req_t *req) {
  if (!s_loaded || req == NULL) {
    return false;
  }

  char auth[180];
  if (httpd_req_get_hdr_value_str(req, "Authorization", auth, sizeof(auth)) !=
      ESP_OK) {
    return false;
  }

  char *p = auth;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (strncasecmp(p, "Bearer ", 7) != 0) {
    return false;
  }
  p += 7;
  while (*p == ' ' || *p == '\t') {
    p++;
  }

  uint8_t candidate[32];
  if (!parse_hex32(p, candidate)) {
    return false;
  }
  return ct_memeq(candidate, s_token, sizeof(s_token));
}

static void send_auth_error(httpd_req_t *req, const char *err_code,
                            const char *message) {
  httpd_resp_set_status(req, "403 Forbidden");
  httpd_resp_set_type(req, "application/json");
  char buf[280];
  snprintf(buf, sizeof(buf),
           "{\"success\":false,\"error\":\"%s\",\"message\":\"%s\"}", err_code,
           message);
  (void)httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

bool board_api_auth_admin_http_denied(httpd_req_t *req) {
  if (bearer_matches_request(req)) {
    return false;
  }
  if (!web_is_locked()) {
    /* WEB_LOCK OFF: stejný model důvěry jako veřejné GET — OTA/Wi‑Fi admin z LAN bez tokenu. */
    return false;
  }
  send_auth_error(
      req, "web_locked",
      "Web locked. Send Authorization Bearer token or UART WEB_LOCK OFF.");
  return true;
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */
