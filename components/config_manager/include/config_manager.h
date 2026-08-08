/**
 * @file config_manager.h
 * @brief ESP32-C6 Chess System v1.8.0 - Config Manager Header
 *
 * This header defines the configuration management interface:
 * - System configuration structure
 * - Functions for configuration management
 * - Error codes and constants
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * Config Manager manages system configuration stored in NVS flash.
 * Enables loading, saving and applying system settings (verbose mode,
 * quiet mode, log level, timeout, echo).
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
// KONFIGURACNI STRUKTURY
// ============================================================================

// system_config_t je definovan v chess_types.h

// ============================================================================
// DEKLARACE FUNKCI
// ============================================================================

/**
 * @brief Initializes the config manager
 *
 * Initializes the NVS flash to store the configuration.
 * NVS flash is already initialized in main.c init_console(),
 * so this function only verifies availability.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t config_manager_init(void);

// config_load_from_nvs deklarovan v chess_types.h

// config_save_to_nvs deklarovan v chess_types.h

// config_apply_settings deklarovan v chess_types.h

/**
 * @brief Reset the configuration to the default value
 *
 * Restores default system settings and saves them to NVS flash.
 * Pote applies the default settings to the system.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t config_reset_to_defaults(void);

// config_get_defaults deklarovan v chess_types.h

// ============================================================================
// KONSTANTY
// ============================================================================

// Default configuration values
/** @brief Default verbose mode (off) */
#define CONFIG_DEFAULT_VERBOSE_MODE false
/** @brief Default quiet mode (off) */
#define CONFIG_DEFAULT_QUIET_MODE false
/** @brief Default log level (errors only) */
#define CONFIG_DEFAULT_LOG_LEVEL ESP_LOG_ERROR
/** @brief Default command timeout (5 seconds) */
#define CONFIG_DEFAULT_COMMAND_TIMEOUT 5000
/** @brief Defaults to echo (on) */
#define CONFIG_DEFAULT_ECHO_ENABLED true
/** @brief Default brightness (50%) */
#define CONFIG_DEFAULT_BRIGHTNESS 50

// Konfiguracni klice pro NVS
/** @brief NVS namespace for sach configuration */
#define CONFIG_NVS_NAMESPACE "chess_config"
/** @brief NVS klic pro verbose mode */
#define CONFIG_NVS_KEY_VERBOSE "verbose"
/** @brief NVS klic pro quiet mode */
#define CONFIG_NVS_KEY_QUIET "quiet"
/** @brief NVS klic pro log level */
#define CONFIG_NVS_KEY_LOG_LEVEL "log_level"
/** @brief NVS klic pro timeout */
#define CONFIG_NVS_KEY_TIMEOUT "timeout"
/** @brief NVS klic pro echo */
#define CONFIG_NVS_KEY_ECHO "echo"
/** @brief NVS klic pro jas */
#define CONFIG_NVS_KEY_BRIGHTNESS "brightness"
/** @brief NVS klic pro guided capture LED napovedu */
#define CONFIG_NVS_KEY_GUIDED_HINT "guided_hint"
/** @brief NVS klic pro plny snapshot hry */
#define CONFIG_NVS_KEY_GAME_SNAPSHOT_FULL "g_snap_full"
/** @brief NVS klic pro fallback snapshot hry */
#define CONFIG_NVS_KEY_GAME_SNAPSHOT_MIN "g_snap_min"
/** @brief NVS klic pro boot tracker */
#define CONFIG_NVS_KEY_BOOT_TRACKER "g_boot_trk"
/** @brief NVS call to keep track of initial position */
#define CONFIG_NVS_KEY_START_POS_CHECK "start_pos_chk"
/** @brief Web UI preference (UTF-8 JSON: {"version":1,"prefs":{...}}) */
#define CONFIG_NVS_KEY_UI_PREFS "ui_prefs_v1"
/** @brief Max velikost JSON pro ui_prefs (NVS blob) */
#define CONFIG_UI_PREFS_MAX_BYTES 3072

/**
 * @brief Ulozi binarni blob do NVS pod danym klicem
 */
esp_err_t config_save_blob_to_nvs(const char *key, const void *data, size_t len);

/**
 * @brief Ulozi web UI JSON do NVS (jednoducha validace tvaru).
 */
esp_err_t config_save_ui_prefs_json(const char *json, size_t len);

/**
 * @brief Load web UI JSON from NVS into buffer (null terminator if misto).
 */
esp_err_t config_load_ui_prefs_json(char *out_buf, size_t out_buf_size,
                                    size_t *out_len);

/**
 * @brief Returns chessHintLimit from stored UI prefs JSON (0-99, 0 = unlimited / key missing).
 */
int config_ui_prefs_get_chess_hint_limit(void);

/**
 * @brief Nacte binarni blob z NVS pod danym klicem
 */
esp_err_t config_load_blob_from_nvs(const char *key, void *data, size_t *len);

/**
 * @brief Smaze hodnotu pod danym klicem z NVS
 */
esp_err_t config_erase_key_from_nvs(const char *key);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
