/**
 * @file ota_update.c
 * OTA transports: HTTPS from the Internet (STA + CA bundle), HTTP from the LAN
 * (e.g. phone on AP 192.168.4.x hosts .bin — STA not needed), BLE `OB` stream.
 *
 * State, journal and boot confirmation live in the `ota_guard` component; this file
 * only moves bytes into the inactive slot and reports phases to the guard:
 * receiving → verifying (esp_ota_end + project check) → rebooting (boot slot switched).
 * Success is reported only by the next boot after the new image confirms itself.
 */
#include "ota_update.h"

#include "board_api_auth.h"
#include "ota_guard.h"
#include "web_server_task.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "led_task.h"

static const char *TAG = "ota_update";

/** Delay between boot-slot switch and esp_restart: lets the last BLE write
 *  response / HTTP status poll go out so the client sees `rebooting`. */
#define OTA_RESTART_DELAY_MS 1500

static int s_ota_led_last_pct = -1;
static TickType_t s_ota_led_last_tick;

static void ota_ui_led_reset_throttle(void) {
  s_ota_led_last_pct = -1;
  s_ota_led_last_tick = 0;
}

static void ota_ui_led_pulse(int percent) {
  TickType_t now = xTaskGetTickCount();
  bool pct_changed = (percent != s_ota_led_last_pct);
  if (!pct_changed && (now - s_ota_led_last_tick) < pdMS_TO_TICKS(200)) {
    return;
  }
  s_ota_led_last_pct = percent;
  s_ota_led_last_tick = now;
  int p = percent;
  if (p < 0) {
    p = 0;
  }
  if (p > 100) {
    p = 100;
  }
  led_ota_progress_ui((uint8_t)p);
}

static SemaphoreHandle_t s_ota_sem;
static char s_last_err[128];

typedef enum {
  BLE_OTA_IDLE = 0,
  BLE_OTA_RX,
  BLE_OTA_SUSPENDED, /**< Link lost; we hold the esp_ota handle, return to RX at the next chunk */
  BLE_OTA_FINALIZED, /**< Image verified, boot slot switched, restart scheduled */
} ble_ota_rx_state_t;

static ble_ota_rx_state_t s_ble_ota_rx = BLE_OTA_IDLE;
static esp_ota_handle_t s_ble_ota_handle;
static const esp_partition_t *s_ble_ota_part;
static uint32_t s_ble_ota_total;
static uint32_t s_ble_ota_rcvd;
static uint16_t s_ble_ota_next_idx;
static uint16_t s_ble_ota_chunk_total_expect;
static bool s_ble_ota_have_chunk_meta;

static esp_timer_handle_t s_ble_ota_suspend_timer;

/** After 24 h without a connection, the session will be canceled (free the semaphore + esp_ota_abort). */
#define BLE_OTA_SUSPEND_TIMEOUT_US (24ULL * 3600ULL * 1000000ULL)

#define BLE_OTA_MAGIC0 ((uint8_t)'O')
#define BLE_OTA_MAGIC1 ((uint8_t)'B')

/** Max POST body `/api/system/ota` (JSON + long HTTPS URL). */
#define OTA_HTTP_POST_BODY_CAP 4096

static void ota_set_last_err(const char *reason) {
  snprintf(s_last_err, sizeof(s_last_err), "%s", reason != NULL ? reason : "");
}

static void ble_ota_session_reset(void) {
  s_ble_ota_rx = BLE_OTA_IDLE;
  s_ble_ota_part = NULL;
  s_ble_ota_have_chunk_meta = false;
  s_ble_ota_next_idx = 0;
  s_ble_ota_rcvd = 0;
  s_ble_ota_total = 0;
}

static void ble_ota_suspend_timer_cancel(void) {
  if (s_ble_ota_suspend_timer != NULL) {
    esp_timer_stop(s_ble_ota_suspend_timer);
  }
}

static void ble_ota_release_session(const char *reason, bool as_error) {
  ble_ota_suspend_timer_cancel();
  if (s_ble_ota_handle != 0) {
    esp_ota_abort(s_ble_ota_handle);
    s_ble_ota_handle = 0;
  }
  ble_ota_session_reset();
  ota_set_last_err(reason);
  if (as_error) {
    ota_guard_error(reason);
    ESP_LOGE(TAG, "BLE stream OTA failed: %s", reason);
  } else {
    ota_guard_aborted(reason, false);
    ESP_LOGW(TAG, "BLE stream OTA aborted: %s", reason);
  }
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
}

static void ble_ota_suspend_timer_cb(void *arg) {
  (void)arg;
  if (s_ble_ota_rx == BLE_OTA_SUSPENDED) {
    ESP_LOGW(TAG, "BLE OTA: suspend timeout (24h) — abort");
    ble_ota_release_session("resume_timeout", false);
  }
}

static void ble_ota_suspend_timer_arm(void) {
  if (s_ble_ota_suspend_timer == NULL) {
    const esp_timer_create_args_t targs = {.callback = ble_ota_suspend_timer_cb,
                                           .name = "ble_ota_suspend"};
    if (esp_timer_create(&targs, &s_ble_ota_suspend_timer) != ESP_OK) {
      ESP_LOGE(TAG, "BLE OTA: suspend timer create failed");
      return;
    }
  }
  esp_timer_stop(s_ble_ota_suspend_timer);
  esp_timer_start_once(s_ble_ota_suspend_timer, BLE_OTA_SUSPEND_TIMEOUT_US);
}

static esp_err_t ota_ensure_sem_initialized(void) {
  if (s_ota_sem != NULL) {
    return ESP_OK;
  }
  s_ota_sem = xSemaphoreCreateBinary();
  if (s_ota_sem == NULL) {
    return ESP_ERR_NO_MEM;
  }
  xSemaphoreGive(s_ota_sem);
  return ESP_OK;
}

static esp_err_t ble_ota_fail_unlock(const char *reason_tag) {
  ble_ota_release_session(reason_tag, true);
  return ESP_FAIL;
}

/**
 * esp_ota_end (full image hash/checksum) → project check → boot slot switch.
 * Restart is deferred so the ATT write response for the last chunk is delivered.
 */
static esp_err_t ble_ota_finalize_and_restart(void) {
  ble_ota_suspend_timer_cancel();
  ota_guard_verifying();
  esp_err_t err = esp_ota_end(s_ble_ota_handle);
  s_ble_ota_handle = 0;
  if (err != ESP_OK) {
    return ble_ota_fail_unlock(ota_guard_reason_from_err(err));
  }
  char why[48];
  if (ota_guard_validate_image(s_ble_ota_part, why, sizeof(why)) != ESP_OK) {
    return ble_ota_fail_unlock(why);
  }
  err = esp_ota_set_boot_partition(s_ble_ota_part);
  if (err != ESP_OK) {
    return ble_ota_fail_unlock(ota_guard_reason_from_err(err));
  }
  ota_guard_rebooting(s_ble_ota_part);
  s_ble_ota_rx = BLE_OTA_FINALIZED;
  s_ble_ota_have_chunk_meta = false;
  ota_ui_led_reset_throttle();
  ota_ui_led_pulse(100);
  ESP_LOGI(TAG, "BLE stream OTA: image verified, boot -> %s, restart in %u ms",
           s_ble_ota_part->label, (unsigned)OTA_RESTART_DELAY_MS);
  ota_guard_schedule_restart(OTA_RESTART_DELAY_MS);
  return ESP_OK;
}

/** Standard HTTPS OTA needs both ota_0 and ota_1 (factory-only table on 4 MB flash → disabled). */
static bool ota_partition_layout_ok(void) {
  const esp_partition_t *p0 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *p1 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  return p0 != NULL && p1 != NULL;
}

/** New image runs but is not confirmed yet: esp_ota_begin would refuse (rollback target). */
static bool ota_confirm_pending(void) {
  ota_guard_status_t st;
  ota_guard_get_status(&st);
  return st.rollback_active && !st.confirmed;
}

static bool url_is_https(const char *url) {
  return url != NULL && strlen(url) >= 12 &&
         strncmp(url, "https://", 8) == 0;
}

static bool url_is_http(const char *url) {
  return url != NULL && strlen(url) >= 10 &&
         strncmp(url, "http://", 7) == 0;
}

static void ota_worker_fail(const char *reason) {
  ota_set_last_err(reason);
  ota_guard_error(reason);
  ESP_LOGE(TAG, "OTA failed: %s", reason);
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
}

static void ota_https_worker_task(void *arg) {
  char *url = (char *)arg;
  esp_https_ota_handle_t https_ota_handle = NULL;
  char why[48];

  if (url == NULL) {
    xSemaphoreGive(s_ota_sem);
    vTaskDelete(NULL);
    return;
  }

  const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
  s_last_err[0] = '\0';
  ota_guard_begin("https", target);
  ota_ui_led_reset_throttle();
  led_ota_progress_ui(0);

  esp_http_client_config_t http_cfg = {
      .url = url,
      .timeout_ms = 120000,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .keep_alive_enable = true,
  };

  esp_https_ota_config_t ota_cfg = {
      .http_config = &http_cfg,
  };

#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] OTA start url=%s", url);
#endif

  esp_err_t err = esp_https_ota_begin(&ota_cfg, &https_ota_handle);
  free(url);
  url = NULL;
  if (err != ESP_OK) {
    ota_worker_fail(ota_guard_reason_from_err(err));
    vTaskDelete(NULL);
    return;
  }

  esp_app_desc_t new_desc;
  err = esp_https_ota_get_img_desc(https_ota_handle, &new_desc);
  if (err != ESP_OK) {
    esp_https_ota_abort(https_ota_handle);
    ota_worker_fail("image_header_unreadable");
    vTaskDelete(NULL);
    return;
  }
  if (ota_guard_validate_desc(new_desc.project_name, new_desc.version, why,
                              sizeof(why)) != ESP_OK) {
    esp_https_ota_abort(https_ota_handle);
    ota_worker_fail(why);
    vTaskDelete(NULL);
    return;
  }
  int img_total = esp_https_ota_get_image_size(https_ota_handle);
  if (target != NULL && img_total > 0 && (uint32_t)img_total > target->size) {
    esp_https_ota_abort(https_ota_handle);
    ota_worker_fail("image_too_large");
    vTaskDelete(NULL);
    return;
  }

  while (1) {
    err = esp_https_ota_perform(https_ota_handle);
    if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
      break;
    }
    int img_sz = esp_https_ota_get_image_size(https_ota_handle);
    int rd = esp_https_ota_get_image_len_read(https_ota_handle);
    int p = 0;
    if (img_sz > 0) {
      p = (int)(((int64_t)rd * 100LL) / (int64_t)img_sz);
      if (p > 99) {
        p = 99;
      }
    }
    ota_guard_progress(p);
    ota_ui_led_pulse(p);
  }

  if (err == ESP_OK && !esp_https_ota_is_complete_data_received(https_ota_handle)) {
    esp_https_ota_abort(https_ota_handle);
    ota_worker_fail("incomplete_download");
    vTaskDelete(NULL);
    return;
  }
  if (err != ESP_OK) {
    esp_https_ota_abort(https_ota_handle);
    ota_worker_fail(ota_guard_reason_from_err(err));
    vTaskDelete(NULL);
    return;
  }

  /* esp_https_ota_finish = esp_ota_end (image verify) + esp_ota_set_boot_partition. */
  ota_guard_verifying();
  err = esp_https_ota_finish(https_ota_handle);
  https_ota_handle = NULL;
  if (err != ESP_OK) {
    ota_worker_fail(ota_guard_reason_from_err(err));
    vTaskDelete(NULL);
    return;
  }

  ota_guard_rebooting(esp_ota_get_boot_partition());
  ota_ui_led_reset_throttle();
  ota_ui_led_pulse(100);
  ESP_LOGI(TAG, "HTTPS OTA: image verified, restart in %u ms",
           (unsigned)OTA_RESTART_DELAY_MS);
  ota_guard_schedule_restart(OTA_RESTART_DELAY_MS);
  vTaskDelete(NULL);
}

/**
 * Image download over pure HTTP (no TLS) — typically phone as server on AP subnet.
 */
static void ota_http_worker_task(void *arg) {
  char *url = (char *)arg;
  esp_http_client_handle_t client = NULL;
  esp_ota_handle_t ota_handle = 0;
  const esp_partition_t *update_partition = NULL;
  bool ota_begin_done = false;
  bool http_open = false;
  char why[48];
  why[0] = '\0';

  if (url == NULL) {
    xSemaphoreGive(s_ota_sem);
    vTaskDelete(NULL);
    return;
  }

  update_partition = esp_ota_get_next_update_partition(NULL);
  s_last_err[0] = '\0';
  ota_guard_begin("http", update_partition);
  ota_ui_led_reset_throttle();
  led_ota_progress_ui(0);

#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] HTTP OTA start url=%s", url);
#endif

  esp_http_client_config_t cfg = {
      .url = url,
      .timeout_ms = 300000,
      .keep_alive_enable = true,
  };

  esp_err_t err = ESP_FAIL;
  client = esp_http_client_init(&cfg);
  if (client == NULL) {
    err = ESP_ERR_NO_MEM;
    goto http_fail;
  }

  err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    snprintf(why, sizeof(why), "http_connect_failed");
    goto http_fail;
  }
  http_open = true;

  int content_length = esp_http_client_fetch_headers(client);
  int http_status = esp_http_client_get_status_code(client);
  if (http_status != 200) {
    snprintf(why, sizeof(why), "http_%d", http_status);
    err = ESP_FAIL;
    goto http_fail;
  }

  if (update_partition == NULL) {
    err = ESP_ERR_NOT_FOUND;
    goto http_fail;
  }
  if (content_length > 0 && (uint32_t)content_length > update_partition->size) {
    err = ESP_ERR_INVALID_SIZE;
    goto http_fail;
  }

  err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    goto http_fail;
  }
  ota_begin_done = true;

  char buf[4096];
  int received = 0;
  while (1) {
    int data_read = esp_http_client_read(client, buf, sizeof(buf));
    if (data_read < 0) {
      snprintf(why, sizeof(why), "http_read_failed");
      err = ESP_FAIL;
      goto http_fail;
    }
    if (data_read == 0) {
      break;
    }
    err = esp_ota_write(ota_handle, buf, (size_t)data_read);
    if (err != ESP_OK) {
      goto http_fail;
    }
    received += data_read;
    int p = 0;
    if (content_length > 0) {
      p = (int)(((int64_t)received * 100LL) / (int64_t)content_length);
    } else if (update_partition->size > 0) {
      p = (int)(((int64_t)received * 100LL) / (int64_t)update_partition->size);
    }
    if (p > 99) {
      p = 99;
    }
    ota_guard_progress(p);
    ota_ui_led_pulse(p);
  }

  if (content_length > 0 && received != content_length) {
    snprintf(why, sizeof(why), "incomplete_download");
    err = ESP_ERR_INVALID_SIZE;
    goto http_fail;
  }

  ota_guard_verifying();
  err = esp_ota_end(ota_handle);
  ota_begin_done = false;
  if (err != ESP_OK) {
    goto http_fail;
  }
  if (ota_guard_validate_image(update_partition, why, sizeof(why)) != ESP_OK) {
    err = ESP_ERR_INVALID_VERSION;
    goto http_fail;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    goto http_fail;
  }

  if (http_open) {
    esp_http_client_close(client);
    http_open = false;
  }
  esp_http_client_cleanup(client);
  client = NULL;
  free(url);
  url = NULL;

  ota_guard_rebooting(update_partition);
  ota_ui_led_reset_throttle();
  ota_ui_led_pulse(100);
  ESP_LOGI(TAG, "HTTP OTA: image verified, boot -> %s, restart in %u ms",
           update_partition->label, (unsigned)OTA_RESTART_DELAY_MS);
  ota_guard_schedule_restart(OTA_RESTART_DELAY_MS);
  vTaskDelete(NULL);
  return;

http_fail:
  if (client != NULL) {
    if (http_open) {
      esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
  }
  if (ota_begin_done) {
    esp_ota_abort(ota_handle);
  }
  free(url);
  ota_worker_fail(why[0] != '\0' ? why : ota_guard_reason_from_err(err));
  vTaskDelete(NULL);
}

static esp_err_t schedule_ota(const char *url) {
  if (ota_ensure_sem_initialized() != ESP_OK) {
    return ESP_FAIL;
  }
  if (!ota_partition_layout_ok()) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (ota_confirm_pending()) {
    return ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
  }
  if (url_is_https(url)) {
    if (!wifi_is_sta_connected()) {
      /* Separate from "buses" (semaphore) — HTTP client maps to 428. */
      return ESP_ERR_NOT_ALLOWED;
    }
  } else if (!url_is_http(url)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (xSemaphoreTake(s_ota_sem, 0) != pdTRUE) {
    return ESP_ERR_INVALID_STATE;
  }

  size_t len = strlen(url);
  char *copy = (char *)malloc(len + 1);
  if (copy == NULL) {
    xSemaphoreGive(s_ota_sem);
    return ESP_ERR_NO_MEM;
  }
  memcpy(copy, url, len + 1);

  bool https = url_is_https(url);
  BaseType_t ok = xTaskCreate(https ? ota_https_worker_task : ota_http_worker_task,
                              https ? "ota_https" : "ota_http",
                              https ? 12288 : 12288, copy, 5, NULL);
  if (ok != pdPASS) {
    free(copy);
    xSemaphoreGive(s_ota_sem);
    return ESP_FAIL;
  }
  return ESP_OK;
}

/**
 * Rollback after a failed start of a new image: another slot has an invalid status,
 * running previous firmware. The client (Flutter) can display a warning to the user.
 */
static void firmware_json_add_rollback_fields(cJSON *root) {
  if (!ota_partition_layout_ok()) {
    return;
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *inv = esp_ota_get_last_invalid_partition();
  bool failed = (inv != NULL && running != NULL && inv != running);
  cJSON_AddBoolToObject(root, "ota_last_boot_failed", failed ? 1 : 0);
  if (!failed) {
    return;
  }
  cJSON_AddStringToObject(root, "ota_failed_slot", inv->label);
  esp_app_desc_t d;
  memset(&d, 0, sizeof(d));
  if (esp_ota_get_partition_description(inv, &d) == ESP_OK &&
      d.version[0] != '\0') {
    cJSON_AddStringToObject(root, "ota_failed_firmware_version", d.version);
  }
}

static void firmware_json_add_guard_fields(cJSON *root) {
  ota_guard_status_t st;
  ota_guard_get_status(&st);
  cJSON_AddStringToObject(root, "ota_state", ota_guard_state_str(st.state));
  cJSON_AddStringToObject(root, "ota_reason", st.reason);
  cJSON_AddBoolToObject(root, "ota_confirmed", st.confirmed ? 1 : 0);
  cJSON_AddBoolToObject(root, "ota_awaiting_confirm", st.awaiting_confirm ? 1 : 0);
  cJSON_AddBoolToObject(root, "ota_rollback_active", st.rollback_active ? 1 : 0);
  cJSON_AddStringToObject(root, "running_slot", st.running_slot);
}

static esp_err_t http_get_firmware(httpd_req_t *req) {
  const esp_app_desc_t *app = esp_app_get_description();
  if (app == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON_AddStringToObject(root, "version", app->version);
  cJSON_AddStringToObject(root, "project_name", app->project_name);
  cJSON_AddStringToObject(root, "idf", esp_get_idf_version());
  cJSON_AddItemToObject(
      root, "ota_supported",
      cJSON_CreateBool(ota_partition_layout_ok() ? 1 : 0));
  firmware_json_add_rollback_fields(root);
  firmware_json_add_guard_fields(root);

  char *printed = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (printed == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "application/json");
  (void)httpd_resp_send(req, printed, HTTPD_RESP_USE_STRLEN);
  free(printed);
  return ESP_OK;
}

/**
 * `{"state":…,"percent":…,…,"message":"<last error>"}` — state vocabulary:
 * idle | receiving | verifying | rebooting | success | aborted | rolled_back | error.
 */
static esp_err_t http_get_ota_status(httpd_req_t *req) {
  char fields[512];
  int n = ota_guard_format_json_fields(fields, sizeof(fields));
  if (n < 0 || (size_t)n >= sizeof(fields)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }
  char msg[sizeof(s_last_err)];
  size_t i = 0;
  for (; s_last_err[i] != '\0' && i + 1 < sizeof(msg); i++) {
    char c = s_last_err[i];
    msg[i] = (c == '"' || c == '\\' || (unsigned char)c < 0x20) ? '_' : c;
  }
  msg[i] = '\0';

  char out[sizeof(fields) + sizeof(msg) + 24];
  snprintf(out, sizeof(out), "{%s,\"message\":\"%s\"}", fields, msg);
  httpd_resp_set_type(req, "application/json");
  (void)httpd_resp_send(req, out, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static esp_err_t http_post_ota(httpd_req_t *req) {
  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  if (req->content_len > 0 &&
      req->content_len > (OTA_HTTP_POST_BODY_CAP - 1)) {
    httpd_resp_set_status(req, "413 Payload Too Large");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"body_too_large\"}", -1);
    return ESP_OK;
  }

  char body[OTA_HTTP_POST_BODY_CAP];
  size_t off = 0;
  while (off < sizeof(body) - 1) {
    int rlen =
        httpd_req_recv(req, body + off, (size_t)(sizeof(body) - 1 - off));
    if (rlen < 0) {
      httpd_resp_set_status(req, "500 Internal Server Error");
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"ok\":false,\"error\":\"recv_failed\"}", -1);
      return ESP_OK;
    }
    if (rlen == 0) {
      break;
    }
    off += (size_t)rlen;
  }
  body[off] = '\0';

  if (off == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"empty_body\"}", -1);
    return ESP_OK;
  }

  cJSON *root = cJSON_Parse(body);
  if (root == NULL) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"bad_json\"}", -1);
    return ESP_OK;
  }
  cJSON *ju = cJSON_GetObjectItemCaseSensitive(root, "url");
  const char *url =
      (cJSON_IsString(ju) && ju->valuestring != NULL) ? ju->valuestring : NULL;
  esp_err_t q = schedule_ota(url);
  cJSON_Delete(root);

  httpd_resp_set_type(req, "application/json");
  if (q == ESP_OK) {
    httpd_resp_set_status(req, "202 Accepted");
    httpd_resp_send(req, "{\"ok\":true,\"accepted\":true}", -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_INVALID_STATE) {
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"busy\"}", -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_OTA_ROLLBACK_INVALID_STATE) {
    httpd_resp_set_status(req, "409 Conflict");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"confirm_pending\"}", -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_NOT_ALLOWED) {
    httpd_resp_set_status(req, "428 Precondition Required");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"https_requires_sta\"}", -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"need_http_or_https_url\"}",
                    -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_NOT_SUPPORTED) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(
        req,
        "{\"ok\":false,\"error\":\"ota_requires_ota0_ota1_partitions\"}", -1);
    return ESP_OK;
  }
  httpd_resp_set_status(req, "500 Internal Server Error");
  httpd_resp_send(req, "{\"ok\":false,\"error\":\"ota_queue_failed\"}", -1);
  return ESP_OK;
}

esp_err_t ota_update_register_http_handlers(httpd_handle_t hd) {
  if (ota_ensure_sem_initialized() != ESP_OK) {
    return ESP_ERR_NO_MEM;
  }

  if (!ota_partition_layout_ok()) {
    ESP_LOGW(TAG,
             "HTTPS OTA unavailable: partition table missing ota_0+ota_1 (e.g. 4 MB factory layout)");
  }

  httpd_uri_t get_fw = {.uri = "/api/system/firmware",
                        .method = HTTP_GET,
                        .handler = http_get_firmware,
                        .user_ctx = NULL};
  httpd_uri_t get_st = {.uri = "/api/system/ota/status",
                        .method = HTTP_GET,
                        .handler = http_get_ota_status,
                        .user_ctx = NULL};
  httpd_uri_t post_ota = {.uri = "/api/system/ota",
                          .method = HTTP_POST,
                          .handler = http_post_ota,
                          .user_ctx = NULL};

  esp_err_t e = httpd_register_uri_handler(hd, &get_fw);
  if (e != ESP_OK) {
    return e;
  }
  e = httpd_register_uri_handler(hd, &get_st);
  if (e != ESP_OK) {
    return e;
  }
  return httpd_register_uri_handler(hd, &post_ota);
}

esp_err_t ota_update_try_start_url(const char *url) { return schedule_ota(url); }

esp_err_t ota_update_ble_begin_from_json(const char *json_buf) {
  if (json_buf == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (ota_ensure_sem_initialized() != ESP_OK) {
    return ESP_FAIL;
  }
  if (!ota_partition_layout_ok()) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (s_ble_ota_rx == BLE_OTA_RX || s_ble_ota_rx == BLE_OTA_FINALIZED) {
    return ESP_ERR_INVALID_STATE;
  }
  if (ota_confirm_pending()) {
    return ESP_ERR_OTA_ROLLBACK_INVALID_STATE;
  }

  cJSON *root = cJSON_Parse(json_buf);
  if (root == NULL) {
    return ESP_FAIL;
  }
  cJSON *jc = cJSON_GetObjectItemCaseSensitive(root, "cmd");
  const char *cmd =
      (cJSON_IsString(jc) && jc->valuestring != NULL) ? jc->valuestring : "";
  if (strcmp(cmd, "ota_ble_begin") != 0) {
    cJSON_Delete(root);
    return ESP_ERR_NOT_FOUND;
  }
  cJSON *jsz = cJSON_GetObjectItemCaseSensitive(root, "size");
  if (!cJSON_IsNumber(jsz)) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  }
  double dv = cJSON_GetNumberValue(jsz);
  if (dv < (double)(32 * 1024) || dv > (double)UINT32_MAX) {
    cJSON_Delete(root);
    return ESP_ERR_INVALID_ARG;
  }
  uint32_t total = (uint32_t)dv;
  cJSON_Delete(root);

  const esp_partition_t *p = esp_ota_get_next_update_partition(NULL);
  if (p == NULL) {
    return ESP_ERR_NOT_FOUND;
  }
  if (total > p->size) {
    ESP_LOGW(TAG, "BLE OTA: image %u B > slot %s %u B", (unsigned)total,
             p->label, (unsigned)p->size);
    return ESP_ERR_INVALID_SIZE;
  }

  /* Explicit new begin while a disconnected session is parked: the client chose
   * to restart from zero instead of resuming — drop the old session first. */
  if (s_ble_ota_rx == BLE_OTA_SUSPENDED) {
    ESP_LOGW(TAG, "BLE OTA: new begin replaces suspended session");
    ble_ota_release_session("replaced_by_new_begin", false);
  }

  if (xSemaphoreTake(s_ota_sem, pdMS_TO_TICKS(3000)) != pdTRUE) {
    return ESP_ERR_INVALID_STATE;
  }

  s_last_err[0] = '\0';
  s_ble_ota_have_chunk_meta = false;
  s_ble_ota_chunk_total_expect = 0;
  s_ble_ota_next_idx = 0;
  s_ble_ota_rcvd = 0;
  s_ble_ota_total = total;
  s_ble_ota_part = p;

  esp_err_t err = esp_ota_begin(p, total, &s_ble_ota_handle);
  if (err != ESP_OK) {
    s_ble_ota_part = NULL;
    s_ble_ota_handle = 0;
    xSemaphoreGive(s_ota_sem);
    return err;
  }

  s_ble_ota_rx = BLE_OTA_RX;
  ota_guard_begin("ble", p);
  ota_ui_led_reset_throttle();
  led_ota_progress_ui(0);
#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] BLE stream OTA begin size=%u label=%s", (unsigned)total,
           p->label);
#endif
  return ESP_OK;
}

esp_err_t ota_update_ble_abort(void) {
  if (s_ble_ota_rx != BLE_OTA_RX && s_ble_ota_rx != BLE_OTA_SUSPENDED) {
    return ESP_ERR_INVALID_STATE;
  }
  ble_ota_release_session("client_abort", false);
  s_last_err[0] = '\0';
  return ESP_OK;
}

void ota_update_ble_on_disconnect(void) {
  if (s_ble_ota_rx != BLE_OTA_RX) {
    return;
  }
  s_ble_ota_rx = BLE_OTA_SUSPENDED;
  ble_ota_suspend_timer_arm();
  ota_guard_aborted("disconnected", true);
  ESP_LOGW(TAG,
           "BLE OTA: disconnected at %u/%u B — image NOT activated, session "
           "parked 24h for resume",
           (unsigned)s_ble_ota_rcvd, (unsigned)s_ble_ota_total);
}

bool ota_update_ble_is_rx_active(void) {
  return (s_ble_ota_rx == BLE_OTA_RX && s_ble_ota_handle != 0);
}

esp_err_t ota_update_ble_build_status_ack_json(char *buf, size_t cap) {
  if (buf == NULL || cap < 200) {
    return ESP_ERR_INVALID_ARG;
  }
  char fields[512];
  int fn = ota_guard_format_json_fields(fields, sizeof(fields));
  if (fn < 0 || (size_t)fn >= sizeof(fields)) {
    return ESP_ERR_INVALID_SIZE;
  }
  bool session = (s_ble_ota_rx == BLE_OTA_RX || s_ble_ota_rx == BLE_OTA_SUSPENDED) &&
                 s_ble_ota_handle != 0;
  bool paused = session && (s_ble_ota_rx == BLE_OTA_SUSPENDED);
  bool active_rx = session && (s_ble_ota_rx == BLE_OTA_RX);
  int n = snprintf(buf, cap,
                   "{\"channel\":\"cmd_ack\",\"ok\":true,\"code\":\"ok\","
                   "\"cmd\":\"ota_ble_status\",\"session\":%s,"
                   "\"paused\":%s,\"active_rx\":%s,"
                   "\"bytes\":%u,\"total\":%u,\"next_chunk\":%u,"
                   "\"esp\":0,%s}",
                   session ? "true" : "false", paused ? "true" : "false",
                   active_rx ? "true" : "false",
                   (unsigned)(session ? s_ble_ota_rcvd : 0),
                   (unsigned)(session ? s_ble_ota_total : 0),
                   (unsigned)(session ? s_ble_ota_next_idx : 0), fields);
  if (n < 0 || (size_t)n >= cap) {
    return ESP_ERR_INVALID_SIZE;
  }
  return ESP_OK;
}

esp_err_t ota_update_ble_feed_chunk(const uint8_t *data, size_t len) {
  if (data == NULL || len < 7) {
    return ESP_ERR_INVALID_ARG;
  }
  if (data[0] != BLE_OTA_MAGIC0 || data[1] != BLE_OTA_MAGIC1) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_ble_ota_rx == BLE_OTA_SUSPENDED) {
    s_ble_ota_rx = BLE_OTA_RX;
    ble_ota_suspend_timer_cancel();
    ota_guard_resumed();
  }
  if (s_ble_ota_rx != BLE_OTA_RX || s_ble_ota_handle == 0) {
    return ESP_ERR_INVALID_STATE;
  }

  uint16_t chunk_idx = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
  uint16_t chunk_total = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
  size_t payload_len = len - 6;

  if (chunk_total == 0 || chunk_idx >= chunk_total) {
    (void)ble_ota_fail_unlock("bad_chunk_header");
    return ESP_ERR_INVALID_ARG;
  }

  if (chunk_idx == 0) {
    s_ble_ota_chunk_total_expect = chunk_total;
    s_ble_ota_have_chunk_meta = true;
  } else if (!s_ble_ota_have_chunk_meta) {
    return ESP_ERR_INVALID_STATE;
  } else if (chunk_total != s_ble_ota_chunk_total_expect) {
    (void)ble_ota_fail_unlock("chunk_total_mismatch");
    return ESP_ERR_INVALID_STATE;
  }

  if (chunk_idx != s_ble_ota_next_idx) {
    (void)ble_ota_fail_unlock("chunk_order");
    return ESP_ERR_INVALID_STATE;
  }

  if (s_ble_ota_rcvd + payload_len > s_ble_ota_total) {
    (void)ble_ota_fail_unlock("overflow");
    return ESP_ERR_INVALID_SIZE;
  }

  esp_err_t werr = esp_ota_write(s_ble_ota_handle, data + 6, payload_len);
  if (werr != ESP_OK) {
    return ble_ota_fail_unlock(ota_guard_reason_from_err(werr));
  }

  s_ble_ota_rcvd += (uint32_t)payload_len;
  s_ble_ota_next_idx++;

  int pcent = (int)((uint64_t)s_ble_ota_rcvd * 100ULL / (uint64_t)s_ble_ota_total);
  if (pcent > 99) {
    pcent = 99;
  }
  ota_guard_progress(pcent);
  ota_ui_led_pulse(pcent);

  if (s_ble_ota_rcvd == s_ble_ota_total) {
    if (chunk_idx != chunk_total - 1) {
      return ble_ota_fail_unlock("last_chunk_idx");
    }
    return ble_ota_finalize_and_restart();
  }

  return ESP_OK;
}

esp_err_t ota_update_ble_try_dispatch(const char *json_buf) {
  if (json_buf == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  cJSON *root = cJSON_Parse(json_buf);
  if (root == NULL) {
    return ESP_FAIL;
  }
  cJSON *jc = cJSON_GetObjectItemCaseSensitive(root, "cmd");
  const char *cmd =
      (cJSON_IsString(jc) && jc->valuestring != NULL) ? jc->valuestring : "";
  if (strcmp(cmd, "ota_start") != 0) {
    cJSON_Delete(root);
    return ESP_ERR_NOT_FOUND;
  }
  cJSON *ju = cJSON_GetObjectItemCaseSensitive(root, "url");
  const char *url =
      (cJSON_IsString(ju) && ju->valuestring != NULL) ? ju->valuestring : NULL;
  esp_err_t err = schedule_ota(url);
  cJSON_Delete(root);
  return err;
}
