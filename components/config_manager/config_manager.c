/**
 * @file config_manager.c
 * @brief ESP32-C6 Chess System v1.8.0 - Sprava konfigurace
 *
 * Tento modul zpracovava persistenci systemove konfigurace v NVS:
 * - Nacteni/ulozeni konfigurace z/do NVS
 * - Aplikace konfiguracnich nastaveni
 * - Sprava vychozi konfigurace
 * - Validace konfigurace
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * Tento modul umoznuje ulozeni a nacteni konfigurace systemu
 * do/ze NVS flash pameti. Udrzuje vychozi hodnoty a validuje
 * konfiguraci pred aplikaci.
 */

#include "config_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "CONFIG_MANAGER";

// Default configuration values
static const system_config_t default_config = {.verbose_mode = false,
                                               /* První flash bez NVS klíče „quiet“: tišší konzole; NVS hodnotu přepíše. */
                                               .quiet_mode = true,
                                               .guided_capture_hints_enabled =
                                                   true,
                                               .led_guidance_level = 5,
                                               .log_level = ESP_LOG_ERROR,
                                               .command_timeout_ms = 5000,
                                               .brightness_level =
                                                   CONFIG_DEFAULT_BRIGHTNESS,
                                               .starting_position_check_enabled =
                                                   false};

esp_err_t config_manager_init(void) {
  ESP_LOGI(TAG, "Initializing configuration manager...");

  // NOTE: NVS is already initialized in main.c init_console()
  // No need to initialize again to avoid conflicts

  ESP_LOGI(TAG, "Configuration manager initialized successfully");
  return ESP_OK;
}

esp_err_t config_load_from_nvs(system_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Opening NVS handle for chess_config...");

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("chess_config", NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to open NVS handle, using defaults");
    memcpy(config, &default_config, sizeof(system_config_t));
    return ESP_OK;
  }

  ESP_LOGI(TAG, "NVS handle opened successfully, loading configuration...");

  // Load configuration values

  // Load verbose_mode
  uint8_t verbose_temp;
  ret = nvs_get_u8(nvs_handle, "verbose", &verbose_temp);
  if (ret != ESP_OK) {
    config->verbose_mode = default_config.verbose_mode;
  } else {
    config->verbose_mode = (bool)verbose_temp;
  }

  // Load quiet_mode
  uint8_t quiet_temp;
  ret = nvs_get_u8(nvs_handle, "quiet", &quiet_temp);
  if (ret != ESP_OK) {
    config->quiet_mode = default_config.quiet_mode;
  } else {
    config->quiet_mode = (bool)quiet_temp;
  }

  // Load log_level
  uint8_t guided_hint_temp;
  ret = nvs_get_u8(nvs_handle, "guided_hint", &guided_hint_temp);
  if (ret != ESP_OK) {
    config->guided_capture_hints_enabled =
        default_config.guided_capture_hints_enabled;
  } else {
    config->guided_capture_hints_enabled = (bool)guided_hint_temp;
  }

  uint8_t led_lvl = 5;
  ret = nvs_get_u8(nvs_handle, "led_guide_lvl", &led_lvl);
  if (ret != ESP_OK) {
    config->led_guidance_level =
        config->guided_capture_hints_enabled ? (uint8_t)5 : (uint8_t)4;
  } else {
    if (led_lvl < 1) {
      led_lvl = 1;
    }
    if (led_lvl > 5) {
      led_lvl = 5;
    }
    config->led_guidance_level = led_lvl;
    config->guided_capture_hints_enabled = (led_lvl >= 5);
  }

  // Load log_level
  ret = nvs_get_u8(nvs_handle, "log_level", &config->log_level);
  if (ret != ESP_OK) {
    config->log_level = default_config.log_level;
  }

  // Load command_timeout_ms
  ret = nvs_get_u32(nvs_handle, "timeout", &config->command_timeout_ms);
  if (ret != ESP_OK) {
    config->command_timeout_ms = default_config.command_timeout_ms;
  }

  // Load brightness
  ret = nvs_get_u8(nvs_handle, "brightness", &config->brightness_level);
  if (ret != ESP_OK) {
    config->brightness_level = default_config.brightness_level;
  }

  // Load starting position check enabled
  uint8_t start_pos_check_temp;
  ret = nvs_get_u8(nvs_handle, "start_pos_chk", &start_pos_check_temp);
  if (ret != ESP_OK) {
    config->starting_position_check_enabled =
        default_config.starting_position_check_enabled;
  } else {
    config->starting_position_check_enabled = (bool)start_pos_check_temp;
  }

  nvs_close(nvs_handle);

  ESP_LOGI(TAG, "Configuration loaded from NVS successfully");
  ESP_LOGI(TAG, "  Verbose: %s", config->verbose_mode ? "ON" : "OFF");
  ESP_LOGI(TAG, "  Quiet: %s", config->quiet_mode ? "ON" : "OFF");
  ESP_LOGI(TAG, "  Guided hints: %s",
           config->guided_capture_hints_enabled ? "ON" : "OFF");
  ESP_LOGI(TAG, "  LED guidance level: %u", (unsigned)config->led_guidance_level);
  ESP_LOGI(TAG, "  Log Level: %d", config->log_level);
  ESP_LOGI(TAG, "  Timeout: %lu ms", config->command_timeout_ms);
  ESP_LOGI(TAG, "  Brightness: %d%%", config->brightness_level);
  ESP_LOGI(TAG, "  Starting position check: %s",
           config->starting_position_check_enabled ? "ON" : "OFF");

  return ESP_OK;
}

esp_err_t config_save_to_nvs(const system_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("chess_config", NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS handle for writing");
    return ret;
  }

  // Save configuration values
  ret = nvs_set_u8(nvs_handle, "verbose", (uint8_t)config->verbose_mode);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save verbose mode");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_u8(nvs_handle, "quiet", (uint8_t)config->quiet_mode);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save quiet mode");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_u8(nvs_handle, "guided_hint",
                   (uint8_t)config->guided_capture_hints_enabled);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save guided hint setting");
    nvs_close(nvs_handle);
    return ret;
  }

  {
    uint8_t lvl = config->led_guidance_level;
    if (lvl < 1) {
      lvl = 1;
    }
    if (lvl > 5) {
      lvl = 5;
    }
    ret = nvs_set_u8(nvs_handle, "led_guide_lvl", lvl);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to save LED guidance level");
      nvs_close(nvs_handle);
      return ret;
    }
  }

  ret = nvs_set_u8(nvs_handle, "log_level", config->log_level);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save log level");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_u32(nvs_handle, "timeout", config->command_timeout_ms);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save timeout");
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_set_u8(nvs_handle, "brightness", config->brightness_level);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save brightness");
    nvs_close(nvs_handle);
    return ret;
  }

  // Save starting position check enabled
  ret = nvs_set_u8(nvs_handle, "start_pos_chk",
                   (uint8_t)config->starting_position_check_enabled);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to save starting position check setting");
    nvs_close(nvs_handle);
    return ret;
  }

  // Commit changes
  ret = nvs_commit(nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit NVS changes");
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);

  ESP_LOGI(TAG, "Configuration saved to NVS successfully");
  return ESP_OK;
}

esp_err_t config_apply_settings(const system_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGI(TAG, "Applying configuration settings...");

  // Apply log level
  if (config->quiet_mode) {
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set("BLE_NIMBLE", ESP_LOG_NONE);
    ESP_LOGI(TAG, "Log level set to NONE (quiet mode)");
  } else if (config->verbose_mode) {
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("UART_TASK", ESP_LOG_DEBUG);
    esp_log_level_set("BLE_NIMBLE", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Log level set to INFO (verbose mode)");
  } else {
    esp_log_level_set("*", config->log_level);
    esp_log_level_set("BLE_NIMBLE", config->log_level);
    ESP_LOGI(TAG, "Log level set to %d", config->log_level);
  }

  ESP_LOGI(TAG, "Configuration settings applied successfully");
  return ESP_OK;
}

esp_err_t config_reset_to_defaults(void) {
  ESP_LOGI(TAG, "Resetting configuration to defaults...");

  system_config_t config = default_config;
  esp_err_t ret = config_save_to_nvs(&config);
  if (ret == ESP_OK) {
    ret = config_apply_settings(&config);
  }

  return ret;
}

esp_err_t config_get_defaults(system_config_t *config) {
  if (config == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  memcpy(config, &default_config, sizeof(system_config_t));
  return ESP_OK;
}

esp_err_t config_save_blob_to_nvs(const char *key, const void *data, size_t len) {
  if (key == NULL || data == NULL || len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to open NVS for blob save (%s)", key);
    return ret;
  }

  ret = nvs_set_blob(nvs_handle, key, data, len);
  if (ret == ESP_OK) {
    ret = nvs_commit(nvs_handle);
  }
  nvs_close(nvs_handle);
  return ret;
}

esp_err_t config_load_blob_from_nvs(const char *key, void *data, size_t *len) {
  if (key == NULL || data == NULL || len == NULL || *len == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = nvs_get_blob(nvs_handle, key, data, len);
  nvs_close(nvs_handle);
  return ret;
}

esp_err_t config_erase_key_from_nvs(const char *key) {
  if (key == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(CONFIG_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
    return ret;
  }

  ret = nvs_erase_key(nvs_handle, key);
  if (ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) {
    ret = nvs_commit(nvs_handle);
  }
  nvs_close(nvs_handle);
  return ret;
}

esp_err_t config_save_ui_prefs_json(const char *json, size_t len) {
  if (json == NULL || len == 0 || len > CONFIG_UI_PREFS_MAX_BYTES) {
    return ESP_ERR_INVALID_ARG;
  }
  size_t i = 0;
  while (i < len && (json[i] == ' ' || json[i] == '\n' || json[i] == '\r' ||
                     json[i] == '\t')) {
    i++;
  }
  if (i >= len || json[i] != '{') {
    return ESP_ERR_INVALID_ARG;
  }
  size_t end = len;
  while (end > i && (json[end - 1] == ' ' || json[end - 1] == '\n' ||
                     json[end - 1] == '\r' || json[end - 1] == '\t')) {
    end--;
  }
  if (end <= i || json[end - 1] != '}') {
    return ESP_ERR_INVALID_ARG;
  }
  return config_save_blob_to_nvs(CONFIG_NVS_KEY_UI_PREFS, json + i, end - i);
}

esp_err_t config_load_ui_prefs_json(char *out_buf, size_t out_buf_size,
                                    size_t *out_len) {
  if (out_buf == NULL || out_buf_size < 2 || out_len == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  size_t len = out_buf_size - 1;
  esp_err_t ret = config_load_blob_from_nvs(CONFIG_NVS_KEY_UI_PREFS, out_buf, &len);
  if (ret != ESP_OK) {
    *out_len = 0;
    out_buf[0] = '\0';
    return ret;
  }
  out_buf[len] = '\0';
  *out_len = len;
  return ESP_OK;
}

int config_ui_prefs_get_chess_hint_limit(void) {
  char buf[CONFIG_UI_PREFS_MAX_BYTES + 1];
  size_t len = 0;
  esp_err_t err = config_load_ui_prefs_json(buf, sizeof(buf), &len);
  if (err != ESP_OK || len == 0) {
    return 0;
  }
  buf[len] = '\0';
  const char *p = strstr(buf, "\"chessHintLimit\"");
  if (p == NULL) {
    return 0;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return 0;
  }
  p++;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
    p++;
  }
  int v = 0;
  if (sscanf(p, "%d", &v) != 1) {
    return 0;
  }
  if (v < 0) {
    v = 0;
  }
  if (v > 99) {
    v = 99;
  }
  return v;
}
