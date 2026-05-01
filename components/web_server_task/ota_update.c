/**
 * @file ota_update.c
 * HTTPS OTA přes vestavěný CA bundle (GitHub Pages / Releases bez ručního PEM).
 */
#include "ota_update.h"

#include "web_server_task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_partition.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "ota_update";

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

static bool url_ok_https(const char *url) {
  if (url == NULL || strlen(url) < 12) {
    return false;
  }
  return strncmp(url, "https://", 8) == 0;
}

static void ota_worker_task(void *arg) {
  char *url = (char *)arg;
  if (url == NULL) {
    xSemaphoreGive(s_ota_sem);
    vTaskDelete(NULL);
    return;
  }

  s_last_err[0] = '\0';
  s_state = OTA_UI_DOWNLOADING;
  s_percent = 0;

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

  esp_err_t err = esp_https_ota(&ota_cfg);
  free(url);

  if (err == ESP_OK) {
    s_percent = 100;
    s_state = OTA_UI_DONE;
#if CHESS_DEBUG_MODE
    ESP_LOGI(TAG, "[STAGING] OTA OK, restart");
#endif
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
  }

  s_state = OTA_UI_ERROR;
  snprintf(s_last_err, sizeof(s_last_err), "%s", esp_err_to_name(err));
  ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
  xSemaphoreGive(s_ota_sem);
  vTaskDelete(NULL);
}

static esp_err_t schedule_ota(const char *url) {
  if (!ota_partition_layout_ok()) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (!wifi_is_sta_connected()) {
    return ESP_ERR_INVALID_STATE;
  }
  if (!url_ok_https(url)) {
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

  BaseType_t ok =
      xTaskCreate(ota_worker_task, "ota_https", 12288, copy, 5, NULL);
  if (ok != pdPASS) {
    free(copy);
    xSemaphoreGive(s_ota_sem);
    return ESP_FAIL;
  }
  return ESP_OK;
}

static esp_err_t http_get_firmware(httpd_req_t *req) {
  const esp_app_desc_t *app = esp_app_get_description();
  char buf[192];
  int n = snprintf(buf, sizeof(buf),
                   "{\"version\":\"%s\",\"project_name\":\"%s\","
                   "\"idf\":\"%s\"}",
                   app->version, app->project_name,
                   esp_get_idf_version());
  if (n <= 0 || n >= (int)sizeof(buf)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", -1);
    return ESP_OK;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, n);
  return ESP_OK;
}

static esp_err_t http_get_ota_status(httpd_req_t *req) {
  char buf[256];
  ota_ui_state_t st = s_state;
  int pct = s_percent;
  const char *err = s_last_err;
  int n = snprintf(
      buf, sizeof(buf),
      "{\"state\":\"%s\",\"percent\":%d,\"message\":\"%s\"}",
      ota_state_json_str(st), pct, err);
  if (n <= 0 || n >= (int)sizeof(buf)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{}", -1);
    return ESP_OK;
  }
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, n);
  return ESP_OK;
}

static esp_err_t http_post_ota(httpd_req_t *req) {
  char body[520];
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
    httpd_resp_send(req,
                    "{\"ok\":false,\"error\":\"busy_or_no_wifi_sta\"}", -1);
    return ESP_OK;
  }
  if (q == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"ok\":false,\"error\":\"need_https_url\"}", -1);
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
  if (s_ota_sem == NULL) {
    s_ota_sem = xSemaphoreCreateBinary();
    if (s_ota_sem == NULL) {
      return ESP_ERR_NO_MEM;
    }
    xSemaphoreGive(s_ota_sem);
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
