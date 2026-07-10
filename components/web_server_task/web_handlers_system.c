/**
 * @file web_handlers_system.c
 * @brief System HTTP handlers (settings, MQTT, demo, factory reset, light).
 */

#include "web_server_task.h"
#include "web_server_internal.h"
#include "board_api_auth.h"
#include "../config_manager/include/config_manager.h"
#include "../game_task/include/game_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../led_task/include/led_task.h"

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
 * @brief Handler pro GET /api/web/lock-status
 *
 * Vraci JSON s lock statusem web rozhrani.
 *
 * @param req HTTP request
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t http_get_web_lock_status_handler(httpd_req_t *req) {
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
 * @brief Handler pro GET /api/mqtt/status
 *
 * Vrátí JSON s aktuálním stavem MQTT konfigurace a připojení.
 *
 * @param req HTTP request
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Vrací JSON:
 * {"host":"...","port":1883,"username":"...","connected":true/false,"mode":"game/ha"}
 */
esp_err_t http_get_mqtt_status_handler(httpd_req_t *req) {
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
 * @brief Handler pro POST /api/mqtt/config
 *
 * Uloží MQTT konfiguraci (host, port, username, password) do NVS.
 *
 * @param req HTTP request
 * @return ESP_OK při úspěchu, chybový kód při chybě
 *
 * @details
 * Očekává JSON: {"host":"...","port":1883,"username":"...","password":"..."}
 * Vrací JSON: {"success": true/false, "message": "..."}
 */
esp_err_t http_post_mqtt_config_handler(httpd_req_t *req) {
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

  // Načíst JSON z request body
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

  // Najít "host"
  const char *host_start = strstr(content, "\"host\"");
  if (host_start != NULL) {
    host_start = strchr(host_start, ':');
    if (host_start != NULL) {
      host_start++; // Přeskočit ':'
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

  // Najít "port"
  const char *port_start = strstr(content, "\"port\"");
  if (port_start != NULL) {
    port_start = strchr(port_start, ':');
    if (port_start != NULL) {
      port_start++; // Přeskočit ':'
      while (*port_start == ' ')
        port_start++;
      uint32_t port_val = strtoul(port_start, NULL, 10);
      if (port_val > 0 && port_val <= 65535) {
        port = (uint16_t)port_val;
      }
    }
  }

  // Najít "username" (volitelné)
  const char *username_start = strstr(content, "\"username\"");
  if (username_start != NULL) {
    username_start = strchr(username_start, ':');
    if (username_start != NULL) {
      username_start++; // Přeskočit ':'
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

  // Najít "password" (volitelné)
  const char *password_start = strstr(content, "\"password\"");
  if (password_start != NULL) {
    password_start = strchr(password_start, ':');
    if (password_start != NULL) {
      password_start++; // Přeskočit ':'
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

  // Uložit do NVS
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

esp_err_t http_post_settings_brightness_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/settings/brightness");

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

  // Parse "brightness"
  int brightness_val = -1;
  char *ptr = strstr(content, "\"brightness\":");
  if (ptr) {
    if (sscanf(ptr, "\"brightness\":%d", &brightness_val) == 1) {
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
      config.brightness_level = 50;

      if (config_load_from_nvs(&config) == ESP_OK) {
        config.brightness_level = (uint8_t)brightness_val;
        config_save_to_nvs(&config);
      } else {
        // Config neexistuje, vytvorit novy s defaulty a novym jasem
        config.brightness_level = (uint8_t)brightness_val;
        // Ostatni defaulty (pokud config_load selhal, config struct je
        // nedefinovany, ale my ukladame jen brightness do NVS v teto verzi
        // config_manageru? Ne, config_manager uklada celou struct?
        // config_save_to_nvs uklada key-value pairs. Takze je to bezpecne.
        config_save_to_nvs(&config);
      }

      cached_brightness = (uint8_t)brightness_val;
      cached_brightness_valid = true;
      ESP_LOGI(TAG, "Brightness updated to %d%% via Web UI", brightness_val);
    }
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

esp_err_t http_post_settings_auto_lamp_timeout_handler(httpd_req_t *req) {
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

esp_err_t http_post_settings_guided_hints_handler(httpd_req_t *req) {
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

esp_err_t http_post_settings_led_guidance_handler(httpd_req_t *req) {
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
 * @brief Handler pro GET /api/settings/start_pos_check
 *
 * Vrati stav hlidani pocatecni pozice (enabled true/false).
 */
esp_err_t http_get_settings_start_pos_check_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "GET /api/settings/start_pos_check");

  bool enabled = game_get_starting_position_check();

  char resp[64];
  snprintf(resp, sizeof(resp), "{\"enabled\":%s}", enabled ? "true" : "false");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, resp, strlen(resp));
  return ESP_OK;
}

/**
 * @brief Handler pro POST /api/settings/start_pos_check
 *
 * Nastavi stav hlidani pocatecni pozice a ulozi do NVS.
 * Body: {"enabled": true/false}
 */
esp_err_t http_post_settings_start_pos_check_handler(httpd_req_t *req) {
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

esp_err_t http_post_light_command_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/light/command");

  char content[128];
  int ret = httpd_req_recv(req, content, sizeof(content) - 1);
  if (ret <= 0) {
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_send(req, "{\"success\":false,\"message\":\"No data\"}", -1);
    return ESP_OK;
  }
  content[ret] = '\0';

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

esp_err_t http_post_light_game_mode_handler(httpd_req_t *req) {
  ESP_LOGI(TAG, "POST /api/light/game_mode");
  ha_light_report_activity("web_game_mode");
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, "{\"success\":true}", -1);
  return ESP_OK;
}

// ============================================================================
// DEMO MODE API HANDLERS
// ============================================================================

esp_err_t http_post_demo_config_handler(httpd_req_t *req) {
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

esp_err_t http_post_demo_start_handler(httpd_req_t *req) {
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

esp_err_t http_get_demo_status_handler(httpd_req_t *req) {
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

/** Musí přesně sedět s webem (POST /api/system/factory_reset) a iOS. */
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
 * GET /api/settings/ui — JSON preference z NVS (nebo vychozi).
 */
esp_err_t http_get_settings_ui_handler(httpd_req_t *req) {
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
 * POST /api/settings/ui — ulozeni web UI JSON do NVS.
 */
esp_err_t http_post_settings_ui_handler(httpd_req_t *req) {
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
 * Smaže celý NVS oddíl (raw erase) a restartuje MCU. Neřeší web lock — záchranná
 * cesta. Tělo musí obsahovat: {"confirm":"erase_all_nvs"}
 */
esp_err_t http_post_factory_reset_handler(httpd_req_t *req) {
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
