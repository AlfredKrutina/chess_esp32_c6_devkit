/**
 * @file config_manager.c
 * @brief ESP32-C6 Chess System v2.4 - Configuration Management
 * 
 * This module handles system configuration persistence in NVS:
 * - Load/save configuration from/to NVS
 * - Apply configuration settings
 * - Default configuration management
 * - Configuration validation
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#include "config_manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "CONFIG_MANAGER";

// Default configuration values
static const system_config_t default_config = {
    .verbose_mode = false,
    .quiet_mode = false,
    .log_level = ESP_LOG_ERROR,
    .command_timeout_ms = 5000,
    .echo_enabled = true,
    .led_brightness = 100,
    .matrix_sensitivity = 50,
    .debug_mode_enabled = false
};

esp_err_t config_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing configuration manager...");
    
    // NOTE: NVS is already initialized in main.c init_console()
    // No need to initialize again to avoid conflicts
    
    ESP_LOGI(TAG, "Configuration manager initialized successfully");
    return ESP_OK;
}

esp_err_t config_load_from_nvs(system_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Opening NVS handle for chess_config...");
    esp_task_wdt_reset(); // Reset watchdog before NVS operations
    
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
    esp_task_wdt_reset(); // Reset watchdog before NVS read
    uint8_t verbose_temp;
    ret = nvs_get_u8(nvs_handle, "verbose", &verbose_temp);
    if (ret != ESP_OK) {
        config->verbose_mode = default_config.verbose_mode;
    } else {
        config->verbose_mode = (bool)verbose_temp;
    }
    
    // Load quiet_mode
    esp_task_wdt_reset(); // Reset watchdog before NVS read
    uint8_t quiet_temp;
    ret = nvs_get_u8(nvs_handle, "quiet", &quiet_temp);
    if (ret != ESP_OK) {
        config->quiet_mode = default_config.quiet_mode;
    } else {
        config->quiet_mode = (bool)quiet_temp;
    }
    
    // Load log_level
    esp_task_wdt_reset(); // Reset watchdog before NVS read
    ret = nvs_get_u8(nvs_handle, "log_level", &config->log_level);
    if (ret != ESP_OK) {
        config->log_level = default_config.log_level;
    }
    

    
    // Load command_timeout_ms
    esp_task_wdt_reset(); // Reset watchdog before NVS read
    ret = nvs_get_u32(nvs_handle, "timeout", &config->command_timeout_ms);
    if (ret != ESP_OK) {
        config->command_timeout_ms = default_config.command_timeout_ms;
    }
    
    // Load echo_enabled
    esp_task_wdt_reset(); // Reset watchdog before NVS read
    uint8_t echo_temp;
    ret = nvs_get_u8(nvs_handle, "echo", &echo_temp);
    if (ret != ESP_OK) {
        config->echo_enabled = default_config.echo_enabled;
    } else {
        config->echo_enabled = (bool)echo_temp;
    }
    
    esp_task_wdt_reset(); // Reset watchdog before NVS close
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Configuration loaded from NVS successfully");
    ESP_LOGI(TAG, "  Verbose: %s", config->verbose_mode ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Quiet: %s", config->quiet_mode ? "ON" : "OFF");
    ESP_LOGI(TAG, "  Log Level: %d", config->log_level);
    ESP_LOGI(TAG, "  Timeout: %lu ms", config->command_timeout_ms);
    ESP_LOGI(TAG, "  Echo enabled: %s", config->echo_enabled ? "ON" : "OFF");
    
    return ESP_OK;
}

esp_err_t config_save_to_nvs(const system_config_t* config)
{
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
    
    ret = nvs_set_u8(nvs_handle, "echo", (uint8_t)config->echo_enabled);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save echo setting");
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

esp_err_t config_apply_settings(const system_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Applying configuration settings...");
    
    // Apply log level
    if (config->quiet_mode) {
        esp_log_level_set("*", ESP_LOG_NONE);
        ESP_LOGI(TAG, "Log level set to NONE (quiet mode)");
    } else if (config->verbose_mode) {
        esp_log_level_set("*", ESP_LOG_INFO);
        esp_log_level_set("UART_TASK", ESP_LOG_DEBUG);
        ESP_LOGI(TAG, "Log level set to INFO (verbose mode)");
    } else {
        esp_log_level_set("*", config->log_level);
        ESP_LOGI(TAG, "Log level set to %d", config->log_level);
    }
    
    ESP_LOGI(TAG, "Configuration settings applied successfully");
    return ESP_OK;
}

esp_err_t config_reset_to_defaults(void)
{
    ESP_LOGI(TAG, "Resetting configuration to defaults...");
    
    system_config_t config = default_config;
    esp_err_t ret = config_save_to_nvs(&config);
    if (ret == ESP_OK) {
        ret = config_apply_settings(&config);
    }
    
    return ret;
}

esp_err_t config_get_defaults(system_config_t* config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(config, &default_config, sizeof(system_config_t));
    return ESP_OK;
}
