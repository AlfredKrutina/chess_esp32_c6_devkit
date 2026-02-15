/**
 * @file config_manager.h
 * @brief ESP32-C6 Chess System v2.4 - Config Manager Hlavicka
 *
 * Tato hlavicka definuje rozhrani pro spravu konfigurace:
 * - Struktura systemove konfigurace
 * - Funkce pro spravu konfigurace
 * - Chybove kody a konstanty
 *
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 *
 * @details
 * Config Manager spravuje systemovou konfiguraci ulozeno v NVS flash.
 * Umoznuje nacitat, ukladat a aplikovat nastaveni systemu (verbose mode,
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
 * @brief Inicializuje config manager
 *
 * Inicializuje NVS flash pro ukladani konfigurace.
 * NVS flash je jiz inicializovan v main.c init_console(),
 * takze tato funkce pouze overi dostupnost.
 *
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t config_manager_init(void);

// config_load_from_nvs deklarovan v chess_types.h

// config_save_to_nvs deklarovan v chess_types.h

// config_apply_settings deklarovan v chess_types.h

/**
 * @brief Resetuj konfiguraci na vychozi hodnoty
 *
 * Obnovi vychozi nastaveni systemu a ulozi je do NVS flash.
 * Pote aplikuje vychozi nastaveni na system.
 *
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t config_reset_to_defaults(void);

// config_get_defaults deklarovan v chess_types.h

// ============================================================================
// KONSTANTY
// ============================================================================

// Vychozi hodnoty konfigurace
/** @brief Vychozi verbose mode (vypnuto) */
#define CONFIG_DEFAULT_VERBOSE_MODE false
/** @brief Vychozi quiet mode (vypnuto) */
#define CONFIG_DEFAULT_QUIET_MODE false
/** @brief Vychozi log level (pouze chyby) */
#define CONFIG_DEFAULT_LOG_LEVEL ESP_LOG_ERROR
/** @brief Vychozi timeout prikazu (5 sekund) */
#define CONFIG_DEFAULT_COMMAND_TIMEOUT 5000
/** @brief Vychozi echo (zapnuto) */
#define CONFIG_DEFAULT_ECHO_ENABLED true
/** @brief Vychozi jas (50%) */
#define CONFIG_DEFAULT_BRIGHTNESS 50

// Konfiguracni klice pro NVS
/** @brief NVS namespace pro sachovou konfiguraci */
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

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
