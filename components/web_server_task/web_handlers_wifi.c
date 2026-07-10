/**
 * @file web_handlers_wifi.c
 * @brief WiFi-related HTTP handlers (api/wifi endpoints).
 */

#include "web_server_task.h"
#include "web_server_internal.h"
#include "board_api_auth.h"

#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "WEB_WIFI";

// ============================================================================
// WIFI API HANDLERS
// ============================================================================

/**
 * @brief Handler pro POST /api/wifi/config
 *
 * Ulozi WiFi konfiguraci (SSID a heslo) do NVS.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Ocekava JSON: {"ssid": "...", "password": "..."}
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
esp_err_t http_post_wifi_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/config");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Nacist JSON z request body
  char content[256] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Jednoduchy JSON parser (hleda "ssid" a "password")
  char ssid[33] = {0};
  char password[65] = {0};

  // Najit "ssid"
  const char *ssid_start = strstr(content, "\"ssid\"");
  if (ssid_start != NULL) {
    ssid_start = strchr(ssid_start, ':');
    if (ssid_start != NULL) {
      ssid_start++; // Preskocit ':'
      while (*ssid_start == ' ' || *ssid_start == '\"')
        ssid_start++;
      const char *ssid_end = strchr(ssid_start, '\"');
      if (ssid_end != NULL && ssid_end > ssid_start) {
        size_t len = ssid_end - ssid_start;
        if (len < sizeof(ssid)) {
          strncpy(ssid, ssid_start, len);
          ssid[len] = '\0';
        }
      }
    }
  }

  // Najit "password"
  const char *password_start = strstr(content, "\"password\"");
  if (password_start != NULL) {
    password_start = strchr(password_start, ':');
    if (password_start != NULL) {
      password_start++; // Preskocit ':'
      while (*password_start == ' ' || *password_start == '\"')
        password_start++;
      const char *password_end = strchr(password_start, '\"');
      if (password_end != NULL && password_end > password_start) {
        size_t len = password_end - password_start;
        if (len < sizeof(password)) {
          strncpy(password, password_start, len);
          password[len] = '\0';
        }
      }
    }
  }

  // Validovat SSID
  size_t ssid_len = strlen(ssid);
  size_t password_len = strlen(password);

  if (ssid_len == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"SSID is required\"}",
                    -1);
    return ESP_FAIL;
  }
  if (ssid_len > 32) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"SSID must be 1-32 characters "
             "(current: %zu)\"}",
             ssid_len);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Validovat password
  if (password_len == 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Password is required\"}", -1);
    return ESP_FAIL;
  }
  if (password_len > 64) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Password must be 1-64 "
             "characters (current: %zu)\"}",
             password_len);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Ulozit do NVS
  esp_err_t err = wifi_save_config_to_nvs(ssid, password);
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to save: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"WiFi config saved\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/connect
 *
 * Pripoji ESP32 k WiFi site s ulozenou konfiguraci.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
esp_err_t http_post_wifi_connect_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/connect");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Zkontrolovat, zda existuje konfigurace
  char ssid[33] = {0};
  char password[65] = {0};
  esp_err_t load_ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));
  if (load_ret != ESP_OK) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"No WiFi configuration "
                    "found. Please save SSID and password first.\"}",
                    -1);
    return ESP_FAIL;
  }

  esp_err_t err = wifi_connect_sta();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[256];
    const char *user_message = NULL;

    // Prevest ESP error kody na uzivatelsky pristupne zpravy
    if (err == ESP_ERR_INVALID_STATE) {
      user_message = "Connection already in progress. Please wait...";
    } else if (err == ESP_ERR_NOT_FOUND) {
      user_message = "Network not found. Please check SSID and ensure the "
                     "network is in range.";
    } else if (err == ESP_ERR_INVALID_RESPONSE) {
      user_message =
          "Authentication failed. Please check password and try again.";
    } else if (err == ESP_ERR_TIMEOUT) {
      user_message =
          "Connection timeout. The network may be too far or not responding.";
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
      user_message =
          "WiFi configuration not found. Please save SSID and password first.";
    } else {
      user_message = "Connection failed. Please check SSID, password, and "
                     "network availability.";
    }

    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"%s\"}", user_message);
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Connected to WiFi\"}",
                  -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/disconnect
 *
 * Odpoji ESP32 od WiFi site.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
esp_err_t http_post_wifi_disconnect_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/disconnect");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  esp_err_t err = wifi_disconnect_sta();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Disconnect failed: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(
      req, "{\"success\":true,\"message\":\"Disconnected from WiFi\"}", -1);

  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/wifi/clear
 *
 * Vymaze ulozenou WiFi konfiguraci z NVS.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON: {"success": true/false, "message": "..."}
 */
esp_err_t http_post_wifi_clear_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/wifi/clear");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  // Odpojit STA pokud je pripojeny
  if (wifi_is_sta_connected()) {
    wifi_disconnect_sta();
  }

  esp_err_t err = wifi_clear_config_from_nvs();
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to clear: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(
      req, "{\"success\":true,\"message\":\"WiFi configuration cleared\"}", -1);

  return ESP_OK;
}

/**
 * @brief Handler pro GET /api/wifi/status
 *
 * Vrati aktualni stav WiFi (AP i STA).
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 *
 * @details
 * Vraci JSON s informacemi o AP a STA statusu.
 */
esp_err_t http_get_wifi_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/wifi/status");

  char local_json[WIFI_STATUS_JSON_MAX];
  esp_err_t ret = wifi_get_sta_status_json(local_json, sizeof(local_json));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get WiFi status", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, local_json, strlen(local_json));

  return ESP_OK;
}

