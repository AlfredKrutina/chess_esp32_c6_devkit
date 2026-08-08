/**
 * @file timer_system.h
 * @brief ESP32-C6 Chess System v1.8.0 - Timer System component
 * 
 * This component manages the time system for the chess game:
 * - Different types of time checks (bullet, blitz, rapid, classical)
 * - Accurate time measurement with ESP32 timer API
 * - Thread-safe operations with case
 * - Integration with game task
 * - Web API for time control
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-01-XX
 * 
 * @details
 * Timer system provides complete time control for chess game.
 * Supports standard time controls and custom settings.
 * All operations are thread-safe and optimized for ESP32.
 */

#ifndef TIMER_SYSTEM_H
#define TIMER_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONSTANTY A TYPY
// ============================================================================

/**
 * @brief Typy casovych kontrol
 */
typedef enum {
    TIME_CONTROL_NONE = 0,           ///< Bez casove kontroly
    TIME_CONTROL_BULLET_1_0,         ///< Bullet 1+0 (1 minuta)
    TIME_CONTROL_BULLET_1_1,         ///< Bullet 1+1 (1 min + 1s increment)
    TIME_CONTROL_BULLET_2_1,         ///< Bullet 2+1 (2 min + 1s increment)
    TIME_CONTROL_BLITZ_3_0,          ///< Blitz 3+0 (3 minuty)
    TIME_CONTROL_BLITZ_3_2,          ///< Blitz 3+2 (3 min + 2s increment)
    TIME_CONTROL_BLITZ_5_0,          ///< Blitz 5+0 (5 minut)
    TIME_CONTROL_BLITZ_5_3,          ///< Blitz 5+3 (5 min + 3s increment)
    TIME_CONTROL_RAPID_10_0,         ///< Rapid 10+0 (10 minut)
    TIME_CONTROL_RAPID_10_5,         ///< Rapid 10+5 (10 min + 5s increment)
    TIME_CONTROL_RAPID_15_10,        ///< Rapid 15+10 (15 min + 10s increment)
    TIME_CONTROL_RAPID_30_0,         ///< Rapid 30+0 (30 minut)
    TIME_CONTROL_CLASSICAL_60_0,     ///< Classical 60+0 (1 hodina)
    TIME_CONTROL_CLASSICAL_90_30,     ///< Classical 90+30 (90 min + 30s increment)
    TIME_CONTROL_CUSTOM,             ///< Custom settings
    TIME_CONTROL_MAX
} time_control_type_t;

/**
 * @brief Konfigurace casove kontroly
 */
typedef struct {
    time_control_type_t type;        ///< Typ casove kontroly
    uint32_t initial_time_ms;       ///< Initial time in milliseconds
    uint32_t increment_ms;          ///< Increment after stroke in milliseconds
    char name[32];                  ///< Nazev casove kontroly
    char description[64];           ///< User description
    bool is_fast;                   ///< Je-li rychla hra (< 10 min)
} time_control_config_t;

/**
 * @brief Stav casoveho systemu
 */
typedef struct {
    // Casove udaje
    uint32_t white_time_ms;         ///< Zbyvajici cas bileho
    uint32_t black_time_ms;         ///< Zbyvajici cas cerneho
    uint32_t move_start_time;       ///< Turn start time (esp_timer_get_time())
    uint32_t last_move_time;        ///< Last move time
    
    // Stav timeru
    bool timer_running;             ///< If the timer is active
    bool is_white_turn;             ///< If there is a bill on the move
    bool game_paused;               ///< Je-li hra pozastavena
    bool time_expired;              ///< Vyprsel-li cas
    
    // Konfigurace
    time_control_config_t config;   ///< Current configuration
    
    // Statistiky
    uint32_t total_moves;           ///< Total number of moves
    uint32_t avg_move_time_ms;      ///< Prumerny cas na tah
    
    // Upozorneni
    bool warning_30s_shown;          ///< Upozorneni na 30s bylo zobrazeno
    bool warning_10s_shown;          ///< Upozorneni na 10s bylo zobrazeno
    bool warning_5s_shown;           ///< Upozorneni na 5s bylo zobrazeno
} chess_timer_t;

// ============================================================================
// PUBLIC API FUNCTIONS
// ============================================================================

/**
 * @brief Initializes the timer system
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_system_init(void);

/**
 * @brief Set the time check
 * 
 * @param config Timing configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_set_time_control(const time_control_config_t* config);

/**
 * @brief Start the timer for the move
 * 
 * @param is_white_turn If there is a white on the turn
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_start_move(bool is_white_turn);

/**
 * @brief End move and add increment
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_end_move(void);

/**
 * @brief Pause the timer
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_pause(void);

/**
 * @brief Reset the timer
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_resume(void);

/**
 * @brief Resets the timer to initial values
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_reset(void);

/**
 * @brief Checks for a timeout
 * 
 * @return true if the time has expired, false otherwise
 */
bool timer_check_timeout(void);

/**
 * @brief Gets the current state of the timer
 * 
 * @param timer_data A pointer to the structure for the timer data
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_get_state(chess_timer_t* timer_data);

/**
 * @brief Gets the remaining time for the players
 * 
 * @param is_white_turn If there is a white on the turn
 * @return The remaining time in milliseconds
 */
uint32_t timer_get_remaining_time(bool is_white_turn);

/**
 * @brief Gets the timing control configuration by type
 * 
 * @param type Time control type
 * @param config A pointer to a structure to configure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_get_config_by_type(time_control_type_t type, time_control_config_t* config);

/**
 * @brief Create a JSON representation of the time system state
 * 
 * @param buffer Buffer for JSON data
 * @param buffer_size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_get_json(char* buffer, size_t buffer_size);

/**
 * @brief Ziska pocet dostupnych casovych kontrol
 * 
 * @return Pocet dostupnych casovych kontrol
 */
uint32_t timer_get_available_controls_count(void);

/**
 * @brief Ziska seznam dostupnych casovych kontrol
 * 
 * @param controls Ukazatel na pole konfiguraci
 * @param max_count Maximalni pocet konfiguraci
 * @return Pocet ziskanych konfiguraci
 */
uint32_t timer_get_available_controls(time_control_config_t* controls, uint32_t max_count);

/**
 * @brief Save timer settings to NVS
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_save_settings(void);

/**
 * @brief Write timer settings from NVS
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_load_settings(void);

/**
 * @brief Gets the average time per turn
 * 
 * @return Average time per stroke in milliseconds
 */
uint32_t timer_get_average_move_time(void);

/**
 * @brief Gain the total number of turns
 * 
 * @return Total turn count
 */
uint32_t timer_get_total_moves(void);

/**
 * @brief Checks if the time check is active
 * 
 * @return true if time checking is active, false otherwise
 */
bool timer_is_active(void);

/**
 * @brief Gets the current time control type
 * 
 * @return The type of the current time control
 */
time_control_type_t timer_get_current_type(void);

/**
 * @brief Set custom time check
 * 
 * @param minutes Number of minutes
 * @param increment_seconds Increment in seconds
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_set_custom_time_control(uint32_t minutes, uint32_t increment_seconds);

/**
 * @brief Gets the time check by index
 * 
 * @param index Index of time control
 * @param config A pointer to a structure to configure
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_get_config_by_index(uint32_t index, time_control_config_t* config);

/**
 * @brief Deinitializes the timer system
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t timer_system_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // TIMER_SYSTEM_H
