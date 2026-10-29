/**
 * @file web_handlers_system.c
 * @brief System HTTP handlers (settings, MQTT, demo, factory reset, light).
 */

#include "web_server_task.h"
#include "cmd_wire.h"
#include "web_server_internal.h"
#include "board_api_auth.h"
#include "../config_manager/include/config_manager.h"
#include "../game_task/include/game_task.h"
#include "../freertos_chess/include/game_cmd.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"
#include "../squares_task/include/hall_i2c_squares.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void toggle_demo_mode(bool enabled);
extern void set_demo_speed_ms(uint32_t speed_ms);
extern bool is_demo_mode_enabled(void);

static const char *TAG = "WEB_SYSTEM";

/**
 * @brief Handler for GET /api/web/lock-status
 *
 * Returns JSON with the lock status of the web interface.
 *
 * @param req HTTP request
 * @return ESP_OK on success, error code on failure
 */
esp_err_t web_get_web_lock_status_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/web/lock-status");

  char response[128];
  int len = snprintf(response, sizeof(response), "{\"locked\":%s}",
                     web_is_locked() ? "true" : "false");

  if (len < 0 || len >= (int)sizeof(response)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

// ============================================================================
// MQTT API HANDLERS
// ============================================================================

/**
 * @brief Handler for GET /api/mqtt/status
 *
 * Returns a JSON with the current state of the MQTT configuration and connection.
 *
 * @param req HTTP request
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * Returns JSON:
 * {"host":"...","port":1883,"username":"...","connected":true/false,"mode":"game/ha"}
 */
esp_err_t web_get_mqtt_status_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/mqtt/status");

  // Get MQTT config
  char host[128] = {0};
  char username[64] = {0};
  char password[64] = {0};
  uint16_t port = 1883;

  esp_err_t ret = mqtt_get_config(host, sizeof(host), &port, username,
                                  sizeof(username), password, sizeof(password));
  if (ret != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to get MQTT config", -1);
    return ESP_FAIL;
  }

  // Get WiFi STA status (required for MQTT)
  bool sta_connected = wifi_is_sta_connected();

  // Get MQTT connection status
  bool mqtt_connected = ha_light_is_mqtt_connected();

  // Get HA mode
  ha_mode_t mode = ha_light_get_mode();
  const char *mode_str = (mode == HA_MODE_GAME) ? "game" : "ha";

  // Build JSON response
  char response[512];
  int len = snprintf(response, sizeof(response),
                     "{"
                     "\"host\":\"%s\","
                     "\"port\":%d,"
                     "\"username\":\"%s\","
                     "\"password\":\"%s\","
                     "\"wifi_connected\":%s,"
                     "\"mqtt_connected\":%s,"
                     "\"mode\":\"%s\""
                     "}",
                     host, port, username[0] ? username : "",
                     password[0] ? "***" : "", sta_connected ? "true" : "false",
                     mqtt_connected ? "true" : "false", mode_str);

  if (len < 0 || len >= (int)sizeof(response)) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "Failed to create response", -1);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response, strlen(response));
  return ESP_OK;
}

/**
 * @brief Handler for POST /api/mqtt/config
 *
 * Saves MQTT configuration (host, port, username, password) in NVS.
 *
 * @param req HTTP request
 * @return ESP_OK on success, error code on failure
 *
 * @details
 * Expects JSON: {"host":"...","port":1883,"username":"...","password":"..."}
 * Returns JSON: {"success": true/false, "message": "..."}
 */
esp_err_t web_post_mqtt_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/mqtt/config");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "MQTT config blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Retrieve JSON from request body
  char content[512] = {0};
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data received\"}",
                    -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parsovat JSON - hledat "host", "port", "username", "password"
  char host[128] = {0};
  uint16_t port = 1883; // Default
  char username[64] = {0};
  char password[64] = {0};

  // Find "guest"
  const char *host_start = strstr(content, "\"host\"");
  if (host_start != NULL) {
    host_start = strchr(host_start, ':');
    if (host_start != NULL) {
      host_start++; // Skip ':'
      while (*host_start == ' ' || *host_start == '\"')
        host_start++;
      const char *host_end = strchr(host_start, '\"');
      if (host_end != NULL && host_end > host_start) {
        size_t len = host_end - host_start;
        if (len < sizeof(host)) {
          strncpy(host, host_start, len);
          host[len] = '\0';
        }
      }
    }
  }

  // Find "port"
  const char *port_start = strstr(content, "\"port\"");
  if (port_start != NULL) {
    port_start = strchr(port_start, ':');
    if (port_start != NULL) {
      port_start++; // Skip ':'
      while (*port_start == ' ')
        port_start++;
      uint32_t port_val = strtoul(port_start, NULL, 10);
      if (port_val > 0 && port_val <= 65535) {
        port = (uint16_t)port_val;
      }
    }
  }

  // Find "username" (optional)
  const char *username_start = strstr(content, "\"username\"");
  if (username_start != NULL) {
    username_start = strchr(username_start, ':');
    if (username_start != NULL) {
      username_start++; // Skip ':'
      while (*username_start == ' ' || *username_start == '\"')
        username_start++;
      const char *username_end = strchr(username_start, '\"');
      if (username_end != NULL && username_end > username_start) {
        size_t len = username_end - username_start;
        if (len < sizeof(username)) {
          strncpy(username, username_start, len);
          username[len] = '\0';
        }
      }
    }
  }

  // Find "password" (optional)
  const char *password_start = strstr(content, "\"password\"");
  if (password_start != NULL) {
    password_start = strchr(password_start, ':');
    if (password_start != NULL) {
      password_start++; // Skip ':'
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

  // Validovat host
  if (strlen(host) == 0 || strlen(host) > 127) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Invalid host (must be "
                    "1-127 characters)\"}",
                    -1);
    return ESP_FAIL;
  }

  // Save to NVS
  const char *username_ptr = (strlen(username) > 0) ? username : NULL;
  const char *password_ptr = (strlen(password) > 0) ? password : NULL;

  esp_err_t err =
      mqtt_save_config_to_nvs(host, port, username_ptr, password_ptr);
  if (err != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "{\"success\":false,\"message\":\"Failed to save: %s\"}",
             esp_err_to_name(err));
    httpd_resp_send(req, error_msg, -1);
    return ESP_FAIL;
  }

  // Reinit MQTT client if WiFi STA is connected
  const char *success_message;
  if (wifi_is_sta_connected()) {
    esp_err_t reinit_ret = ha_light_reinit_mqtt();
    if (reinit_ret == ESP_OK) {
      success_message = "MQTT configuration saved and client reinicialized "
                        "with new settings.";
    } else {
      success_message = "MQTT configuration saved. Client reinit failed (will "
                        "reconnect on next WiFi connection).";
    }
  } else {
    success_message = "MQTT configuration saved. Client will reconnect with "
                      "new settings on next WiFi connection.";
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  char response[256];
  snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"%s\"}",
           success_message);
  httpd_resp_send(req, response, strlen(response));

  return ESP_OK;
}

// ============================================================================
// SETTINGS API HANDLERS
// ============================================================================

esp_err_t web_post_settings_hall_rest_handler(httpd_req_t *req) {
  char body[128];
  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "hall_rest")) {
    return ESP_OK;
  }
  hall_i2c_squares_request_rest_measure();
  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "hall_rest");
  return ESP_OK;
}

esp_err_t web_post_settings_brightness_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/brightness");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[160];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';
  if (cmd_wire_http_guard(req, content, "brightness")) {
    return ESP_OK;
  }

  // Parse "percent" (current) or "brightness" (older clients).
  int brightness_val = -1;
  char *ptr = strstr(content, "\"percent\":");
  if (ptr == NULL || sscanf(ptr, "\"percent\":%d", &brightness_val) != 1) {
    brightness_val = -1;
    ptr = strstr(content, "\"brightness\":");
    if (ptr) {
      (void)sscanf(ptr, "\"brightness\":%d", &brightness_val);
    }
  }
  if (brightness_val >= 0) {
      // Validovat
      if (brightness_val < 0)
        brightness_val = 0;
      if (brightness_val > 100)
        brightness_val = 100;

      // 1. Nastavit LED driver hned
      led_set_brightness_global((uint8_t)brightness_val);

      // 2. Ulozit do configu (load-modify-save)
      system_config_t config;
      // Default init v pripade chyby loadu
      config.brightness_level = 100;

      if (config_load_from_nvs(&config) == ESP_OK) {
        config.brightness_level = (uint8_t)brightness_val;
        config_save_to_nvs(&config);
      } else {
        // Config does not exist, create new ones with defaults and new brightness
        config.brightness_level = (uint8_t)brightness_val;
        // The last defaults (if config_load failed, the config struct is
        // nedefinovany, ale my ukladame jen brightness do NVS v teto verzi
        // config_manageru? Ne, config_manager uklada celou struct?
        // config_save_to_nvs saves key-value pairs. So it's safe.
        config_save_to_nvs(&config);
      }

      cached_brightness = (uint8_t)brightness_val;
      cached_brightness_valid = true;
      ESP_LOGI(TAG, "Brightness updated to %d%% via Web UI", brightness_val);
  }

  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", content, true,
                     "brightness");
  return ESP_OK;
}

esp_err_t web_post_settings_auto_lamp_timeout_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/auto_lamp_timeout");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[80];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  int seconds_val = -1;
  char *ptr = strstr(content, "\"seconds\":");
  if (ptr && sscanf(ptr, "\"seconds\":%d", &seconds_val) == 1) {
    if (seconds_val < 5) seconds_val = 5;
    if (seconds_val > 7200) seconds_val = 7200;
    if (ha_light_set_activity_timeout_sec((uint32_t)seconds_val) == ESP_OK) {
      httpd_resp_set_type(req, "application/json");
      httpd_resp_send(req, "{\"success\":true,\"seconds\":%d}", seconds_val);
      return ESP_OK;
    }
  }
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":false,\"message\":\"Invalid seconds (5..7200)\"}", -1);
  return ESP_OK;
}

esp_err_t web_post_settings_guided_hints_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/guided_hints");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[96];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  system_config_t config;
  if (config_load_from_nvs(&config) != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Config load failed\"}",
                    -1);
    return ESP_OK;
  }
  config.guided_capture_hints_enabled = enabled;
  config.led_guidance_level = enabled ? (uint8_t)5 : (uint8_t)4;
  config_save_to_nvs(&config);
  game_set_led_guidance_level(config.led_guidance_level);

  char resp[120];
  snprintf(resp, sizeof(resp),
           "{\"success\":true,\"enabled\":%s,\"led_guidance_level\":%u}",
           enabled ? "true" : "false",
           (unsigned)config.led_guidance_level);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

esp_err_t web_post_settings_led_guidance_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/led_guidance");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[128];
  int r = httpd_req_recv(req, content, sizeof(content) - 1);
  if (r <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[r] = '\0';

  int level = -1;
  char *p = strstr(content, "\"level\"");
  if (p) {
    p = strchr(p, ':');
    if (p) {
      level = (int)strtol(p + 1, NULL, 10);
    }
  }
  if (level < 1 || level > 5) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"level 1..5\"}", -1);
    return ESP_OK;
  }

  system_config_t config;
  if (config_load_from_nvs(&config) != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Config load failed\"}",
                    -1);
    return ESP_OK;
  }
  config.led_guidance_level = (uint8_t)level;
  config.guided_capture_hints_enabled = (level >= 5);
  config_save_to_nvs(&config);
  game_set_led_guidance_level((uint8_t)level);

  char resp[96];
  snprintf(resp, sizeof(resp),
           "{\"success\":true,\"led_guidance_level\":%u,\"guided_capture_hints_enabled\":%s}",
           (unsigned)level, (level >= 5) ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

/**
 * @brief Handler for GET /api/settings/start_pos_check
 *
 * Return the status of the guard to the initial position (enabled true/false).
 */
esp_err_t web_get_settings_start_pos_check_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/settings/start_pos_check");

  bool enabled = game_get_starting_position_check();

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"enabled\":%s}", enabled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

/**
 * @brief Handler for POST /api/settings/start_pos_check
 *
 * Set the status of the guard to the initial position and save it in the NVS.
 * Points: {"enabled": true/false}
 */
esp_err_t web_post_settings_start_pos_check_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/start_pos_check");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[64];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  // Nastavit v game_task
  game_set_starting_position_check(enabled);

  // Ulozit do NVS
  system_config_t config;
  if (config_load_from_nvs(&config) == ESP_OK) {
    config.starting_position_check_enabled = enabled;
    config_save_to_nvs(&config);
    ESP_LOGI(TAG, "Starting position check saved to NVS: %s", enabled ? "ON" : "OFF");
  }

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"success\":true,\"enabled\":%s}", enabled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

// ============================================================================
// LIGHT API HANDLERS
// ============================================================================

esp_err_t web_post_light_command_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/light/command");

  char content[128];
  if (!web_recv_json_body(req, content, sizeof(content))) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Incomplete light command\"}",
                    -1);
    return ESP_OK;
  }

  bool state = true;
  int r = 255, g = 255, b = 255;
  if (strstr(content, "\"state\":false") || strstr(content, "\"state\": false\"")) {
    state = false;
  }
  char *p;
  if ((p = strstr(content, "\"r\":")) && sscanf(p, "\"r\":%d", &r) == 1) { }
  if ((p = strstr(content, "\"g\":")) && sscanf(p, "\"g\":%d", &g) == 1) { }
  if ((p = strstr(content, "\"b\":")) && sscanf(p, "\"b\":%d", &b) == 1) { }

  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;

  if (!ha_light_request_web_lamp(state, (uint8_t)r, (uint8_t)g, (uint8_t)b)) {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Light task busy or not "
                    "ready\"}",
                    -1);
    return ESP_OK;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

esp_err_t web_post_light_game_mode_handler(httpd_req_t *req) {
  char body[128];
  ESP_LOGI(TAG, "POST /api/light/game_mode");
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "light_game_mode")) {
    return ESP_OK;
  }
  ha_light_report_activity("web_game_mode");
  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "light_game_mode");
  return ESP_OK;
}

// ============================================================================
// DEMO MODE API HANDLERS
// ============================================================================

esp_err_t web_post_demo_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/demo/config");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  char content[100];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';

  // Parse "enabled"
  bool enabled = (strstr(content, "\"enabled\":true") != NULL) ||
                 (strstr(content, "\"enabled\": true") != NULL);

  // Parse "speed_ms"
  char *speed_ptr = strstr(content, "\"speed_ms\":");
  if (speed_ptr) {
    int speed_val;
    if (sscanf(speed_ptr, "\"speed_ms\":%d", &speed_val) == 1) {
      set_demo_speed_ms((uint32_t)speed_val);
    }
  }

  toggle_demo_mode(enabled);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

esp_err_t web_post_demo_start_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/demo/start");

  if (web_is_locked()) {
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"Web locked\"}", -1);
    return ESP_OK;
  }

  toggle_demo_mode(true);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true,\"message\":\"Demo started\"}", -1);
  return ESP_OK;
}

esp_err_t web_get_demo_status_handler(httpd_req_t *req) {
  ESP_LOGD(TAG, "GET /api/demo/status");

  bool enabled = is_demo_mode_enabled();

  char resp_str[64];
  snprintf(resp_str, sizeof(resp_str), "{\"enabled\":%s}",
           enabled ? "true" : "false");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp_str, strlen(resp_str));
  return ESP_OK;
}

// ============================================================================
// FACTORY RESET HELPERS (shared with BLE dispatch)
// ============================================================================

/** Must sit exactly with web (POST /api/system/factory_reset) and iOS. */
#define CZECHMATE_FACTORY_RESET_CONFIRM "erase_all_nvs"

bool json_body_has_factory_confirm(const char *json) {
  if (json == NULL) {
    return false;
  }
  if (strstr(json, "\"confirm\"") == NULL) {
    return false;
  }
  return strstr(json, CZECHMATE_FACTORY_RESET_CONFIRM) != NULL;
}

static void factory_reset_worker(void *ignored) {
  (void)ignored;
  vTaskDelay(pdMS_TO_TICKS(500));
  ESP_LOGW(TAG,
           "[STAGING] factory_reset: raw erase default NVS partition + restart");

  const esp_partition_t *nvs_part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
  esp_err_t er = ESP_FAIL;
  if (nvs_part != NULL) {
    er = esp_partition_erase_range(nvs_part, 0, nvs_part->size);
    ESP_LOGW(TAG, "esp_partition_erase_range(label=%s size=%u): %s",
             nvs_part->label, (unsigned)nvs_part->size, esp_err_to_name(er));
  } else {
    er = nvs_flash_erase();
    ESP_LOGW(TAG, "nvs_flash_erase (no nvs partition label): %s",
             esp_err_to_name(er));
  }

  esp_restart();
}

esp_err_t factory_reset_schedule(void) {
  BaseType_t ok =
      xTaskCreate(factory_reset_worker, "fac_rst", 4096, NULL, 10, NULL);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "factory_reset: xTaskCreate failed");
    return ESP_FAIL;
  }
  return ESP_OK;
}

// ============================================================================
// SETTINGS UI HANDLERS
// ============================================================================

/**
 * GET /api/settings/ui — JSON preferences from NVS (or default).
 */
esp_err_t web_get_settings_ui_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/settings/ui");
  char buf[CONFIG_UI_PREFS_MAX_BYTES + 1];
  size_t len = 0;
  esp_err_t err = config_load_ui_prefs_json(buf, sizeof(buf), &len);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_status(req, "200 OK");
  if (err != ESP_OK || len == 0) {
    httpd_resp_send(req, "{\"version\":1,\"prefs\":{}}", -1);
    return ESP_OK;
  }
  httpd_resp_send(req, buf, len);
  return ESP_OK;
}

/**
 * POST /api/settings/ui — save web UI JSON to NVS.
 */
esp_err_t web_post_settings_ui_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/ui");

  if (web_is_locked()) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(
        req, "{\"success\":false,\"error\":\"Web interface locked\"}", -1);
    return ESP_OK;
  }

  char body[CONFIG_UI_PREFS_MAX_BYTES + 1];
  int r = httpd_req_recv(req, body, (int)sizeof(body) - 1);
  if (r <= 0) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"Empty body\"}", -1);
    return ESP_OK;
  }
  body[r] = '\0';

  esp_err_t s = config_save_ui_prefs_json(body, (size_t)r);
  httpd_resp_set_type(req, "application/json");
  if (s == ESP_ERR_INVALID_ARG) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(
        req,
        "{\"success\":false,\"error\":\"Invalid JSON or too large (max "
        "3072 bytes)\"}",
        -1);
    return ESP_OK;
  }
  if (s != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_send(req, "{\"success\":false,\"error\":\"NVS write failed\"}",
                    -1);
    return ESP_FAIL;
  }
  httpd_resp_set_status(req, "200 OK");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

// ============================================================================
// SYSTEM API (factory reset — Phase 3C.4)
// ============================================================================

/**
 * @brief POST /api/system/factory_reset
 *
 * Deletes the entire NVS partition (raw erase) and restarts the MCU. Does not solve web lock — rescue
 * path The body must contain: {"confirm":"erase_all_nvs"}
 */
esp_err_t web_post_factory_reset_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/system/factory_reset");

  if (board_api_auth_admin_http_denied(req)) {
    return ESP_OK;
  }

  char content[160] = {0};
  int rlen = httpd_req_recv(req, content, sizeof(content) - 1);
  if (rlen <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"No JSON body\"}", -1);
    return ESP_OK;
  }
  content[rlen] = '\0';

  if (!json_body_has_factory_confirm(content)) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req,
        "{\"success\":false,\"message\":\"JSON must include confirm "
        "erase_all_nvs\"}",
        -1);
    return ESP_OK;
  }

  esp_err_t qerr = factory_reset_schedule();
  if (qerr != ESP_OK) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(
        req, "{\"success\":false,\"message\":\"Could not queue reset task\"}",
        -1);
    return ESP_OK;
  }

  httpd_resp_set_status(req, "200 OK");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req,
                  "{\"success\":true,\"message\":\"NVS erase scheduled\","
                  "\"rebooting\":true}",
                  -1);
  return ESP_OK;
}

esp_err_t web_post_timer_config_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/config");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer config blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  // Precist JSON data
  char content[256];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "No data received", -1);
    return ESP_FAIL;
  }
  content[ret] = '\0';
  if (cmd_wire_http_guard(req, content, "timer_config")) {
    return ESP_OK;
  }

  // Parse the JSON and send the command to the game task
  game_cmd_t cmd = {0};
  cmd.kind = GCMD_TIMER_SET;

  // Vylepsene JSON parsovani pouzitim sscanf pro spolehlivejsi parsovani
  // Nejprve najit pole "type"
  char *type_str = strstr(content, "\"type\":");
  if (type_str == NULL) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Missing 'type' field", -1);
    return ESP_FAIL;
  }

  // Parsovat hodnotu type
  int type_value;
  if (sscanf(type_str, "\"type\":%d", &type_value) != 1) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Invalid type value", -1);
    return ESP_FAIL;
  }

  if (type_value < 0 || type_value > 14) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "Type out of range (0-14)", -1);
    return ESP_FAIL;
  }

  cmd.u.timer.time_control_type = (uint8_t)type_value;

  // Parse custom time values ​​if type is CUSTOM
  if (type_value == 14) { // TIME_CONTROL_CUSTOM
    char *minutes_str = strstr(content, "\"custom_minutes\":");
    char *increment_str = strstr(content, "\"custom_increment\":");

    if (minutes_str) {
      int minutes;
      if (sscanf(minutes_str, "\"custom_minutes\":%d", &minutes) == 1) {
        if (minutes >= 1 && minutes <= 180) {
          cmd.u.timer.custom_minutes = (uint32_t)minutes;
        } else {
          httpd_resp_set_status(req, "400 Bad Request");
          httpd_resp_send(req, "Minutes must be 1-180", -1);
          return ESP_FAIL;
        }
      }
    }

    if (increment_str) {
      int increment;
      if (sscanf(increment_str, "\"custom_increment\":%d", &increment) == 1) {
        if (increment >= 0 && increment <= 60) {
          cmd.u.timer.custom_increment = (uint32_t)increment;
        } else {
          httpd_resp_set_status(req, "400 Bad Request");
          httpd_resp_send(req, "Increment must be 0-60", -1);
          return ESP_FAIL;
        }
      }
    }

    // Verify that custom values ​​have been provided
    if (minutes_str == NULL || increment_str == NULL) {
      httpd_resp_set_status(req, "400 Bad Request");
      httpd_resp_send(req, "Custom time control requires minutes and increment",
                      -1);
      return ESP_FAIL;
    }
  }

  // Send the command to the game task
  if (!game_submit(&cmd)) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", content, false,
                       NULL);
    return ESP_OK;
  }

  cmd_wire_http_send(req, "200 OK",
                     "{\"success\":true,\"message\":\"Time control set\"}",
                     content, true, "timer_config");
  return ESP_OK;
}

esp_err_t web_post_timer_pause_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/pause");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer pause blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  char body[160];
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "timer_pause")) {
    return ESP_OK;
  }

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_TIMER_PAUSE;

  if (!game_submit(&cmd)) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }

  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "timer_pause");
  return ESP_OK;
}

esp_err_t web_post_timer_resume_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/resume");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer resume blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  char body[160];
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "timer_resume")) {
    return ESP_OK;
  }

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_TIMER_RESUME;

  if (!game_submit(&cmd)) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }

  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "timer_resume");
  return ESP_OK;
}

esp_err_t web_post_timer_reset_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/timer/reset");

  // Kontrola web lock
  if (web_is_locked()) {
    ESP_LOGW(TAG, "Timer reset blocked: web interface is locked");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, "403 Forbidden");
    httpd_resp_send(req,
                    "{\"success\":false,\"message\":\"Web interface is locked. "
                    "Use UART to unlock.\"}",
                    -1);
    return ESP_OK;
  }

  char body[160];
  body[0] = '\0';
  if (req->content_len > 0) {
    int n = httpd_req_recv(req, body, (int)sizeof(body) - 1);
    if (n > 0) {
      body[n] = '\0';
    }
  }
  if (cmd_wire_http_guard(req, body, "timer_reset")) {
    return ESP_OK;
  }

  game_cmd_t cmd = {0};
  cmd.kind = GCMD_TIMER_RESET;

  if (!game_submit(&cmd)) {
    cmd_wire_http_send(req, "503 Service Unavailable",
                       "{\"ok\":false,\"error\":\"busy\"}", body, false, NULL);
    return ESP_OK;
  }

  cmd_wire_http_send(req, "200 OK", "{\"success\":true}", body, true,
                     "timer_reset");
  return ESP_OK;
}

esp_err_t web_server_ble_system_command_dispatch(const char *cmd, char *buf) {
  if (strcmp(cmd, "demo_config") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] demo_config blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    bool enabled = false;
    cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(en)) {
      enabled = cJSON_IsTrue(en);
    }
    cJSON *sp = cJSON_GetObjectItemCaseSensitive(root, "speed_ms");
    if (cJSON_IsNumber(sp)) {
      double v = cJSON_GetNumberValue(sp);
      if (v >= 0 && v <= 600000) {
        set_demo_speed_ms((uint32_t)v);
      }
    }
    cJSON_Delete(root);
    toggle_demo_mode(enabled);
    ESP_LOGI(TAG, "[BLE] demo_config enabled=%d", (int)enabled);
    return ESP_OK;
  }

  if (strcmp(cmd, "demo_start") == 0) {
    if (web_is_locked()) {
      ESP_LOGW(TAG, "[BLE] demo_start blocked (web locked)");
      return ESP_ERR_INVALID_STATE;
    }
    toggle_demo_mode(true);
    ESP_LOGI(TAG, "[BLE] demo_start");
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_auto_lamp_timeout") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *sec = cJSON_GetObjectItemCaseSensitive(root, "seconds");
    int seconds_val = -1;
    if (cJSON_IsNumber(sec)) {
      seconds_val = (int)cJSON_GetNumberValue(sec);
    }
    cJSON_Delete(root);
    if (seconds_val < 5) {
      seconds_val = 5;
    }
    if (seconds_val > 7200) {
      seconds_val = 7200;
    }
    if (ha_light_set_activity_timeout_sec((uint32_t)seconds_val) != ESP_OK) {
      return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "[BLE] settings_auto_lamp_timeout %d", seconds_val);
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_guided_hints") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    bool enabled = false;
    cJSON *en = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(en)) {
      enabled = cJSON_IsTrue(en);
    }
    cJSON_Delete(root);
    system_config_t syscfg;
    if (config_load_from_nvs(&syscfg) != ESP_OK) {
      return ESP_FAIL;
    }
    syscfg.guided_capture_hints_enabled = enabled;
    syscfg.led_guidance_level = enabled ? (uint8_t)5 : (uint8_t)4;
    config_save_to_nvs(&syscfg);
    game_set_led_guidance_level(syscfg.led_guidance_level);
    ESP_LOGI(TAG, "[BLE] settings_guided_hints enabled=%d", (int)enabled);
    return ESP_OK;
  }

  if (strcmp(cmd, "settings_led_guidance") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *lv = cJSON_GetObjectItemCaseSensitive(root, "level");
    int level = -1;
    if (cJSON_IsNumber(lv)) {
      level = (int)cJSON_GetNumberValue(lv);
    }
    cJSON_Delete(root);
    if (level < 1 || level > 5) {
      return ESP_ERR_INVALID_ARG;
    }
    system_config_t syscfg;
    if (config_load_from_nvs(&syscfg) != ESP_OK) {
      return ESP_FAIL;
    }
    syscfg.led_guidance_level = (uint8_t)level;
    syscfg.guided_capture_hints_enabled = (level >= 5);
    config_save_to_nvs(&syscfg);
    game_set_led_guidance_level((uint8_t)level);
    ESP_LOGI(TAG, "[BLE] settings_led_guidance level=%d", level);
    return ESP_OK;
  }

  if (strcmp(cmd, "mqtt_config") == 0) {
    if (web_is_locked()) {
      return ESP_ERR_INVALID_STATE;
    }
    cJSON *root = cJSON_Parse(buf);
    if (root == NULL) {
      return ESP_FAIL;
    }
    cJSON *hj = cJSON_GetObjectItemCaseSensitive(root, "host");
    if (!cJSON_IsString(hj) || hj->valuestring == NULL ||
        hj->valuestring[0] == '\0' || strlen(hj->valuestring) > 127) {
      cJSON_Delete(root);
      return ESP_ERR_INVALID_ARG;
    }
    uint16_t port = 1883;
    cJSON *pj = cJSON_GetObjectItemCaseSensitive(root, "port");
    if (cJSON_IsNumber(pj)) {
      double pv = cJSON_GetNumberValue(pj);
      if (pv > 0 && pv <= 65535) {
        port = (uint16_t)pv;
      }
    }
    const char *user_str = NULL;
    cJSON *uj = cJSON_GetObjectItemCaseSensitive(root, "username");
    if (cJSON_IsString(uj) && uj->valuestring != NULL && uj->valuestring[0]) {
      user_str = uj->valuestring;
    }
    const char *pass_str = NULL;
    cJSON *pwj = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(pwj) && pwj->valuestring != NULL && pwj->valuestring[0]) {
      pass_str = pwj->valuestring;
    }
    esp_err_t merr = mqtt_save_config_to_nvs(hj->valuestring, port, user_str,
                                             pass_str);
    cJSON_Delete(root);
    if (merr != ESP_OK) {
      ESP_LOGW(TAG, "[BLE] mqtt_config save failed: %s",
               esp_err_to_name(merr));
      return merr;
    }
    if (wifi_is_sta_connected()) {
      (void)ha_light_reinit_mqtt();
    }
    ESP_LOGI(TAG, "[BLE] mqtt_config OK");
    return ESP_OK;
  }

  return ESP_ERR_NOT_FOUND;
}
