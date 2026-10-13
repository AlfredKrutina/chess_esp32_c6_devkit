/**
 * @file board_api_auth.c
 * @brief NVS API token + Bearer kontrola pro admin HTTP endpointy.
 */
#include "board_api_auth.h"

#include "sdkconfig.h"
#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "esp_http_server.h"
#endif
#include "ble_task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "web_server_task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "board_api_auth";

#define BOARD_API_NVS_NS "web_config"
#define BOARD_API_NVS_KEY_TOKEN "api_tok"

static uint8_t s_token[32];
static bool s_loaded;

static esp_err_t board_pair_slots_init(void);

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
  (void)board_pair_slots_init();
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
    /* WEB_LOCK OFF: same trust model as public GET — OTA/Wi‑Fi admin from LAN without token. */
    return false;
  }
  send_auth_error(
      req, "web_locked",
      "Web locked. Send Authorization Bearer token or UART WEB_LOCK OFF.");
  return true;
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */

#define PAIR_NVS_NS "pair_slots"
#define PAIR_WIFI_LEASE_US (120LL * 1000 * 1000)

static board_pair_book_t s_pair_book;
static SemaphoreHandle_t s_pair_mu;
static bool s_pair_ready;
static int64_t s_wifi_lease_until_us;

static bool pair_lock(void) {
  if (s_pair_mu == NULL) {
    return false;
  }
  return xSemaphoreTake(s_pair_mu, pdMS_TO_TICKS(250)) == pdTRUE;
}

static void pair_unlock(void) {
  if (s_pair_mu != NULL) {
    xSemaphoreGive(s_pair_mu);
  }
}

static void slot_store(const board_pair_slot_t *slot, uint8_t blob[44]) {
  memset(blob, 0, 44);
  blob[0] = slot->kind;
  blob[1] = slot->addr_type;
  memcpy(blob + 2, slot->addr, 6);
  memcpy(blob + 8, slot->token, BOARD_PAIR_TOKEN_LEN);
  memcpy(blob + 40, &slot->seq, sizeof(slot->seq));
}

static void slot_load(board_pair_slot_t *slot, const uint8_t blob[44]) {
  memset(slot, 0, sizeof(*slot));
  slot->kind = blob[0];
  slot->addr_type = blob[1];
  memcpy(slot->addr, blob + 2, 6);
  memcpy(slot->token, blob + 8, BOARD_PAIR_TOKEN_LEN);
  memcpy(&slot->seq, blob + 40, sizeof(slot->seq));
  if (slot->kind != BOARD_PAIR_KIND_BLE && slot->kind != BOARD_PAIR_KIND_WIFI) {
    memset(slot, 0, sizeof(*slot));
  }
}

static void persist_book_locked(void) {
  nvs_handle_t h;
  if (nvs_open(PAIR_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
    return;
  }
  for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
    char key[4];
    snprintf(key, sizeof(key), "s%d", i);
    uint8_t blob[44];
    slot_store(&s_pair_book.slots[i], blob);
    (void)nvs_set_blob(h, key, blob, sizeof(blob));
  }
  (void)nvs_set_u32(h, "pseq", s_pair_book.next_seq);
  (void)nvs_commit(h);
  nvs_close(h);
}

static void token_to_hex(const uint8_t token[BOARD_PAIR_TOKEN_LEN], char *out,
                         size_t cap) {
  for (size_t i = 0; i < BOARD_PAIR_TOKEN_LEN && (i * 2 + 2) < cap; i++) {
    snprintf(out + i * 2, cap - i * 2, "%02X", (unsigned)token[i]);
  }
  if (cap > 64) {
    out[64] = '\0';
  }
}

static void expire_wifi_lease_locked(void) {
  if (s_pair_book.wifi_fd >= 0) {
    return;
  }
  if (s_wifi_lease_until_us != 0 &&
      esp_timer_get_time() >= s_wifi_lease_until_us) {
    board_pair_book_release_wifi_lease(&s_pair_book);
    s_wifi_lease_until_us = 0;
  }
}

static void arm_wifi_lease_locked(void) {
  if (s_pair_book.wifi_fd >= 0) {
    s_wifi_lease_until_us = 0;
    return;
  }
  s_wifi_lease_until_us = esp_timer_get_time() + PAIR_WIFI_LEASE_US;
}

static esp_err_t issue_locked(uint8_t kind, uint8_t addr_type,
                              const uint8_t addr[6], char *out_hex, size_t cap) {
  if (out_hex == NULL || cap < 65) {
    return ESP_ERR_INVALID_ARG;
  }
  uint8_t token[BOARD_PAIR_TOKEN_LEN];
  esp_fill_random(token, sizeof(token));
  if (board_pair_book_put(&s_pair_book, kind, addr_type, addr, token) < 0) {
    return ESP_FAIL;
  }
  persist_book_locked();
  token_to_hex(token, out_hex, cap);
  return ESP_OK;
}

static esp_err_t board_pair_slots_init(void) {
  board_pair_book_init(&s_pair_book);
  if (s_pair_mu == NULL) {
    s_pair_mu = xSemaphoreCreateMutex();
    if (s_pair_mu == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }
  nvs_handle_t h;
  esp_err_t e = nvs_open(PAIR_NVS_NS, NVS_READONLY, &h);
  if (e == ESP_OK) {
    for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
      char key[4];
      snprintf(key, sizeof(key), "s%d", i);
      uint8_t blob[44];
      size_t sz = sizeof(blob);
      if (nvs_get_blob(h, key, blob, &sz) == ESP_OK && sz == sizeof(blob)) {
        slot_load(&s_pair_book.slots[i], blob);
        if (s_pair_book.slots[i].seq > s_pair_book.next_seq) {
          s_pair_book.next_seq = s_pair_book.slots[i].seq;
        }
      }
    }
    uint32_t seq = 0;
    if (nvs_get_u32(h, "pseq", &seq) == ESP_OK && seq > s_pair_book.next_seq) {
      s_pair_book.next_seq = seq;
    }
    nvs_close(h);
  }
  s_pair_ready = true;
  return ESP_OK;
}

esp_err_t board_pair_issue_for_peer(uint8_t addr_type, const uint8_t addr[6],
                                    char *out_hex, size_t cap) {
  if (!s_pair_ready || addr == NULL || !pair_lock()) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t e =
      issue_locked(BOARD_PAIR_KIND_BLE, addr_type, addr, out_hex, cap);
  pair_unlock();
  return e;
}

esp_err_t board_pair_issue_wifi(char *out_hex, size_t cap) {
  if (!s_pair_ready || !pair_lock()) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t e = issue_locked(BOARD_PAIR_KIND_WIFI, 0, NULL, out_hex, cap);
  pair_unlock();
  return e;
}

bool board_pair_copy_token_hex(uint8_t addr_type, const uint8_t addr[6],
                               char *out_hex, size_t cap) {
  if (!s_pair_ready || addr == NULL || out_hex == NULL || cap < 65 ||
      !pair_lock()) {
    return false;
  }
  bool found = false;
  for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
    const board_pair_slot_t *slot = &s_pair_book.slots[i];
    if (slot->kind == BOARD_PAIR_KIND_BLE && slot->addr_type == addr_type &&
        memcmp(slot->addr, addr, 6) == 0) {
      token_to_hex(slot->token, out_hex, cap);
      found = true;
      break;
    }
  }
  pair_unlock();
  return found;
}

void board_pair_on_ble_up(uint8_t addr_type, const uint8_t addr[6]) {
  if (!s_pair_ready || addr == NULL || !pair_lock()) {
    return;
  }
  board_pair_book_set_ble(&s_pair_book, true, addr_type, addr);
  pair_unlock();
}

void board_pair_on_ble_down(void) {
  if (!s_pair_ready || !pair_lock()) {
    return;
  }
  board_pair_book_set_ble(&s_pair_book, false, 0, NULL);
  pair_unlock();
}

#if CONFIG_CHESS_ENABLE_WEB_SERVER
static bool read_bearer(httpd_req_t *req, uint8_t out[BOARD_PAIR_TOKEN_LEN]) {
  if (req == NULL || out == NULL) {
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
  return parse_hex32(p, out);
}

bool board_pair_http_command_denied(httpd_req_t *req) {
  uint8_t token[BOARD_PAIR_TOKEN_LEN];
  bool have = read_bearer(req, token);
  if (!s_pair_ready || !pair_lock()) {
    send_auth_error(req, "api_token_required",
                    "Board command token required.");
    return true;
  }
  expire_wifi_lease_locked();
  int verdict = have ? board_pair_book_command(&s_pair_book, token, true)
                     : BOARD_PAIR_CMD_NEED_TOKEN;
  if (verdict == BOARD_PAIR_CMD_ALLOW) {
    arm_wifi_lease_locked();
#if CHESS_DEBUG_MODE
    ESP_LOGI(TAG, "[STAGING] command token accepted (wifi seat %d)",
             s_pair_book.wifi_slot);
#endif
  }
  pair_unlock();
  if (verdict == BOARD_PAIR_CMD_ALLOW) {
    return false;
  }
  if (verdict == BOARD_PAIR_CMD_BUSY) {
#if CHESS_DEBUG_MODE
    ESP_LOGW(TAG, "[STAGING] command refused: board busy");
#endif
    send_auth_error(req, "board_busy",
                    "Board is connected to another device.");
    return true;
  }
#if CHESS_DEBUG_MODE
  ESP_LOGW(TAG, "[STAGING] command refused: token required");
#endif
  send_auth_error(req, "api_token_required", "Board command token required.");
  return true;
}

int board_pair_ws_command(const uint8_t token[32], int fd) {
  if (!s_pair_ready || token == NULL || !pair_lock()) {
    return BOARD_PAIR_CMD_NEED_TOKEN;
  }
  expire_wifi_lease_locked();
  int verdict = board_pair_book_command(&s_pair_book, token, true);
  if (verdict == BOARD_PAIR_CMD_ALLOW) {
    if (fd >= 0) {
      board_pair_book_note_wifi_fd(&s_pair_book, fd);
      s_wifi_lease_until_us = 0;
    } else {
      arm_wifi_lease_locked();
    }
#if CHESS_DEBUG_MODE
    ESP_LOGI(TAG, "[STAGING] ws command token accepted fd=%d", fd);
#endif
  } else if (verdict == BOARD_PAIR_CMD_BUSY) {
#if CHESS_DEBUG_MODE
    ESP_LOGW(TAG, "[STAGING] ws command refused: board busy");
#endif
  }
  pair_unlock();
  return verdict;
}

void board_pair_ws_release_fd(int fd) {
  if (!s_pair_ready || !pair_lock()) {
    return;
  }
  board_pair_book_release_wifi_fd(&s_pair_book, fd);
  pair_unlock();
}

void board_pair_ws_prune(const int *live_fds, int live_count) {
  if (!s_pair_ready || !pair_lock()) {
    return;
  }
  board_pair_book_prune_wifi_fd(&s_pair_book, live_fds, live_count);
  pair_unlock();
}

esp_err_t board_pair_http_handler(httpd_req_t *req) {
  /* Wi-Fi hosts have no GATT. The phone path is the cmd_ack notify
   * {"code":"paired","api_token":"..."} after SMP. This POST is the same
   * token, and only while the 60 s window is still open. The first success
   * claims the window so a second device does not get another token. */
  if (!ble_task_claim_pairing_success()) {
    send_auth_error(req, "not_pairing",
                    "Board is not in pairing mode.");
    return ESP_OK;
  }
  char hex[65];
  esp_err_t e = board_pair_issue_wifi(hex, sizeof(hex));
  ble_task_finish_pairing_window();
  if (e != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    (void)httpd_resp_send(req, "{\"success\":false,\"error\":\"token\"}",
                          HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] Wi-Fi pair token issued, window closed");
#endif
  char body[128];
  snprintf(body, sizeof(body),
           "{\"success\":true,\"api_token\":\"%s\"}", hex);
  httpd_resp_set_type(req, "application/json");
  (void)httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */
