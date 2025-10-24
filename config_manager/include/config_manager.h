/**
 * @file config_manager.h
 * @brief ESP32-C6 Chess System v2.4 - Configuration Management Header
 * 
 * This header defines the configuration management interface:
 * - System configuration structure
 * - Configuration management functions
 * - Error codes and constants
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "esp_err.h"
#include "esp_log.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

// system_config_t is defined in chess_types.h

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

/**
 * @brief Initialize configuration manager
 * @return ESP_OK on success, error code on failure
 */
esp_err_t config_manager_init(void);

// config_load_from_nvs declared in chess_types.h

// config_save_to_nvs declared in chess_types.h

// config_apply_settings declared in chess_types.h

/**
 * @brief Reset configuration to defaults
 * @return ESP_OK on success, error code on failure
 */
esp_err_t config_reset_to_defaults(void);

// config_get_defaults declared in chess_types.h

// ============================================================================
// CONSTANTS
// ============================================================================

// Default configuration values
#define CONFIG_DEFAULT_VERBOSE_MODE      false
#define CONFIG_DEFAULT_QUIET_MODE        false
#define CONFIG_DEFAULT_LOG_LEVEL         ESP_LOG_ERROR
#define CONFIG_DEFAULT_COMMAND_TIMEOUT   5000
#define CONFIG_DEFAULT_ECHO_ENABLED      true

// Configuration keys for NVS
#define CONFIG_NVS_NAMESPACE             "chess_config"
#define CONFIG_NVS_KEY_VERBOSE           "verbose"
#define CONFIG_NVS_KEY_QUIET             "quiet"
#define CONFIG_NVS_KEY_LOG_LEVEL         "log_level"
#define CONFIG_NVS_KEY_TIMEOUT           "timeout"
#define CONFIG_NVS_KEY_ECHO              "echo"

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
