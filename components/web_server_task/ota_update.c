/**
 * @file ota_update.c
 * OTA: HTTPS z internetu (STA + CA bundle), nebo HTTP z LAN (např. telefon na AP
 * 192.168.4.x hostí .bin — STA nepotřeba).
 */
#include "ota_update.h"

#include "board_api_auth.h"
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

typedef enum {
  OTA_UI_IDLE = 0,
  OTA_UI_DOWNLOADING,
  OTA_UI_DONE,
  OTA_UI_ERROR,
} ota_ui_state_t;

static SemaphoreHandle_t s_ota_sem;
static volatile ota_ui_state_t s_state = OTA_UI_IDLE;
static volatile int s_percent;
static char s_last_err[128];

typedef enum {
  BLE_OTA_IDLE = 0,
  BLE_OTA_RX,
  BLE_OTA_SUSPENDED, /**< Link lost; esp_ota handle držíme, návrat do RX při dalším chunku */
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

/** Po 24 h bez spojení zruší session (uvolní semafor + esp_ota_abort). */
#define BLE_OTA_SUSPEND_TIMEOUT_US (24ULL * 3600ULL * 1000000ULL)

#define BLE_OTA_MAGIC0 ((uint8_t)'O')
#define BLE_OTA_MAGIC1 ((uint8_t)'B')

static void ble_ota_suspend_timer_cancel(void) {
  if (s_ble_ota_suspend_timer != NULL) {
    esp_timer_stop(s_ble_ota_suspend_timer);
  }
}

static void ble_ota_suspend_timer_cb(void *arg) {
  (void)arg;
  if (s_ble_ota_rx == BLE_OTA_SUSPENDED) {
    ESP_LOGW(TAG, "BLE OTA: suspend timeout (24h) — abort");
    (void)ota_update_ble_abort();
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
  ble_ota_suspend_timer_cancel();
  if (s_ble_ota_handle != 0) {
    esp_ota_abort(s_ble_ota_handle);
    s_ble_ota_handle = 0;
  }
  s_ble_ota_rx = BLE_OTA_IDLE;
  s_ble_ota_part = NULL;
  s_ble_ota_have_chunk_meta = false;
  s_ble_ota_next_idx = 0;
  s_ble_ota_rcvd = 0;
  s_ble_ota_total = 0;
  s_state = OTA_UI_ERROR;
  snprintf(s_last_err, sizeof(s_last_err), "%s", reason_tag);
  ESP_LOGE(TAG, "BLE stream OTA aborted: %s", reason_tag);
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
  return ESP_FAIL;
}

static esp_err_t ble_ota_finalize_and_restart(void) {
  ble_ota_suspend_timer_cancel();
  esp_err_t err = esp_ota_end(s_ble_ota_handle);
  s_ble_ota_handle = 0;
  if (err != ESP_OK) {
    return ble_ota_fail_unlock(esp_err_to_name(err));
  }
  err = esp_ota_set_boot_partition(s_ble_ota_part);
  if (err != ESP_OK) {
    return ble_ota_fail_unlock(esp_err_to_name(err));
  }
  s_ble_ota_part = NULL;
  s_ble_ota_rx = BLE_OTA_IDLE;
  s_ble_ota_have_chunk_meta = false;
  s_percent = 100;
  s_state = OTA_UI_DONE;
  ota_ui_led_reset_throttle();
  ota_ui_led_pulse(100);
#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] BLE stream OTA OK, restart");
#endif
  vTaskDelay(pdMS_TO_TICKS(300));
  esp_restart();
  return ESP_OK;
}

/** Standardní HTTPS OTA potřebuje ota_0 i ota_1 (factory-only tabulka na 4 MB flash → vypnuto). */
static bool ota_partition_layout_ok(void) {
  const esp_partition_t *p0 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *p1 = esp_partition_find_first(
      ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  return p0 != NULL && p1 != NULL;
}

static const char *ota_state_json_str(ota_ui_state_t st) {
  switch (st) {
    case OTA_UI_DOWNLOADING:
      return "downloading";
    case OTA_UI_DONE:
      return "done";
    case OTA_UI_ERROR:
      return "error";
    default:
      return "idle";
  }
}

static bool url_is_https(const char *url) {
  return url != NULL && strlen(url) >= 12 &&
         strncmp(url, "https://", 8) == 0;
}

static bool url_is_http(const char *url) {
  return url != NULL && strlen(url) >= 10 &&
         strncmp(url, "http://", 7) == 0;
}

static void ota_https_worker_task(void *arg) {
  char *url = (char *)arg;
  esp_https_ota_handle_t https_ota_handle = NULL;

  if (url == NULL) {
    xSemaphoreGive(s_ota_sem);
    vTaskDelete(NULL);
    return;
  }

  s_last_err[0] = '\0';
  s_state = OTA_UI_DOWNLOADING;
  s_percent = 0;
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
  if (err != ESP_OK) {
    snprintf(s_last_err, sizeof(s_last_err), "%s", esp_err_to_name(err));
    s_state = OTA_UI_ERROR;
    ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
    led_ota_restore_board_after_update_abort();
    free(url);
    xSemaphoreGive(s_ota_sem);
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
    s_percent = p;
    ota_ui_led_pulse(p);
  }

  if (err == ESP_OK) {
    err = esp_https_ota_finish(https_ota_handle);
    https_ota_handle = NULL;
  } else {
    esp_https_ota_abort(https_ota_handle);
    https_ota_handle = NULL;
  }

  free(url);
  url = NULL;

  if (err == ESP_OK) {
    s_percent = 100;
    s_state = OTA_UI_DONE;
    ota_ui_led_reset_throttle();
    ota_ui_led_pulse(100);
#if CHESS_DEBUG_MODE
    ESP_LOGI(TAG, "[STAGING] OTA OK, restart");
#endif
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
  }

  s_state = OTA_UI_ERROR;
  snprintf(s_last_err, sizeof(s_last_err), "%s", esp_err_to_name(err));
  ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
  vTaskDelete(NULL);
}

/**
 * Stahování obrazu po čistém HTTP (bez TLS) — typicky telefon jako server na AP subnetu.
 */
static void ota_http_worker_task(void *arg) {
  char *url = (char *)arg;
  esp_http_client_handle_t client = NULL;
  esp_ota_handle_t ota_handle = 0;
  const esp_partition_t *update_partition = NULL;
  bool ota_begin_done = false;
  bool http_open = false;

  if (url == NULL) {
    xSemaphoreGive(s_ota_sem);
    vTaskDelete(NULL);
    return;
  }

  s_last_err[0] = '\0';
  s_state = OTA_UI_DOWNLOADING;
  s_percent = 0;
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
    goto http_fail;
  }
  http_open = true;

  int content_length = esp_http_client_fetch_headers(client);
  int http_status = esp_http_client_get_status_code(client);
  if (http_status != 200) {
    snprintf(s_last_err, sizeof(s_last_err), "HTTP %d", http_status);
    err = ESP_FAIL;
    goto http_fail;
  }

  update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL) {
    snprintf(s_last_err, sizeof(s_last_err), "no OTA partition");
    err = ESP_ERR_NOT_FOUND;
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
      snprintf(s_last_err, sizeof(s_last_err), "http_read_failed");
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
    if (content_length > 0) {
      int p = (int)(((int64_t)received * 100LL) / (int64_t)content_length);
      if (p > 99) {
        p = 99;
      }
      s_percent = p;
    } else if (update_partition != NULL && update_partition->size > 0) {
      int p = (int)(((int64_t)received * 100LL) /
                   (int64_t)update_partition->size);
      if (p > 99) {
        p = 99;
      }
      s_percent = p;
    }
    ota_ui_led_pulse(s_percent);
  }

  err = esp_ota_end(ota_handle);
  ota_begin_done = false;
  if (err != ESP_OK) {
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

  s_percent = 100;
  s_state = OTA_UI_DONE;
  ota_ui_led_reset_throttle();
  ota_ui_led_pulse(100);
#if CHESS_DEBUG_MODE
  ESP_LOGI(TAG, "[STAGING] HTTP OTA OK, restart");
#endif
  vTaskDelay(pdMS_TO_TICKS(500));
  esp_restart();

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

  s_state = OTA_UI_ERROR;
  if (err != ESP_OK && s_last_err[0] == '\0') {
    snprintf(s_last_err, sizeof(s_last_err), "%s", esp_err_to_name(err));
  }
  ESP_LOGE(TAG, "HTTP OTA failed: %s", s_last_err);
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
  vTaskDelete(NULL);
}

static esp_err_t schedule_ota(const char *url) {
  if (ota_ensure_sem_initialized() != ESP_OK) {
    return ESP_FAIL;
  }
  if (!ota_partition_layout_ok()) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (url_is_https(url)) {
    if (!wifi_is_sta_connected()) {
      /* Odděleně od „busy“ (semafor) — HTTP klient mapuje na 428. */
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

static esp_err_t http_get_ota_status(httpd_req_t *req) {
  ota_ui_state_t st = s_state;
  int pct = s_percent;
  const char *err = s_last_err;

  cJSON *root = cJSON_CreateObject();
  if (root == NULL) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
  }

  cJSON_AddStringToObject(root, "state", ota_state_json_str(st));
  cJSON_AddNumberToObject(root, "percent", pct);
  cJSON_AddStringToObject(root, "message", (err != NULL) ? err : "");

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

static esp_err_t http_post_ota(httpd_req_t *req) {
  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  char body[1536];
  int rlen = httpd_req_recv(req, body, sizeof(body) - 1);
  if (rlen <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"empty_body\"}", -1);
    return ESP_OK;
  }
  body[rlen] = '\0';

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
             "HTTPS OTA nedostupné: v tabulce oddílů chybí ota_0+ota_1 (např. 4 MB factory layout)");
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
  if (s_ble_ota_rx != BLE_OTA_IDLE) {
    return ESP_ERR_INVALID_STATE;
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
    return ESP_ERR_INVALID_SIZE;
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
    xSemaphoreGive(s_ota_sem);
    return err;
  }

  s_ble_ota_rx = BLE_OTA_RX;
  s_state = OTA_UI_DOWNLOADING;
  s_percent = 0;
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
  ble_ota_suspend_timer_cancel();
  if (s_ble_ota_handle != 0) {
    esp_ota_abort(s_ble_ota_handle);
    s_ble_ota_handle = 0;
  }
  s_ble_ota_rx = BLE_OTA_IDLE;
  s_ble_ota_part = NULL;
  s_ble_ota_have_chunk_meta = false;
  s_ble_ota_next_idx = 0;
  s_ble_ota_rcvd = 0;
  s_ble_ota_total = 0;
  s_state = OTA_UI_IDLE;
  s_percent = 0;
  s_last_err[0] = '\0';
  led_ota_restore_board_after_update_abort();
  xSemaphoreGive(s_ota_sem);
  return ESP_OK;
}

void ota_update_ble_on_disconnect(void) {
  if (s_ble_ota_rx != BLE_OTA_RX) {
    return;
  }
  s_ble_ota_rx = BLE_OTA_SUSPENDED;
  ble_ota_suspend_timer_arm();
  ESP_LOGW(TAG,
           "BLE OTA: disconnected — session paused (same image within 24h)");
}

bool ota_update_ble_is_rx_active(void) {
  return (s_ble_ota_rx == BLE_OTA_RX && s_ble_ota_handle != 0);
}

esp_err_t ota_update_ble_build_status_ack_json(char *buf, size_t cap) {
  if (buf == NULL || cap < 200) {
    return ESP_ERR_INVALID_ARG;
  }
  bool session = (s_ble_ota_rx != BLE_OTA_IDLE && s_ble_ota_handle != 0);
  if (!session) {
    snprintf(buf, cap,
             "{\"channel\":\"cmd_ack\",\"ok\":true,\"code\":\"ok\","
             "\"cmd\":\"ota_ble_status\",\"session\":false,"
             "\"paused\":false,\"active_rx\":false,"
             "\"bytes\":0,\"total\":0,\"next_chunk\":0,\"percent\":0,\"esp\":0}");
    return ESP_OK;
  }
  bool paused = (s_ble_ota_rx == BLE_OTA_SUSPENDED);
  bool active_rx = (s_ble_ota_rx == BLE_OTA_RX);
  int pct = s_percent;
  if (pct < 0) {
    pct = 0;
  }
  if (pct > 100) {
    pct = 100;
  }
  snprintf(buf, cap,
           "{\"channel\":\"cmd_ack\",\"ok\":true,\"code\":\"ok\","
           "\"cmd\":\"ota_ble_status\",\"session\":true,"
           "\"paused\":%s,\"active_rx\":%s,"
           "\"bytes\":%u,\"total\":%u,\"next_chunk\":%u,\"percent\":%d,"
           "\"esp\":0}",
           paused ? "true" : "false", active_rx ? "true" : "false",
           (unsigned)s_ble_ota_rcvd, (unsigned)s_ble_ota_total,
           (unsigned)s_ble_ota_next_idx, pct);
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
    return ble_ota_fail_unlock(esp_err_to_name(werr));
  }

  s_ble_ota_rcvd += (uint32_t)payload_len;
  s_ble_ota_next_idx++;

  int pcent = (int)((uint64_t)s_ble_ota_rcvd * 100ULL / (uint64_t)s_ble_ota_total);
  if (pcent > 99) {
    pcent = 99;
  }
  s_percent = pcent;
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
