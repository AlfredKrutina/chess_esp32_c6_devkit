/**
 * @file enhanced_castling_system.h
 * @brief Enhanced Castling System for ESP32 Chess Board
 * 
 * This module provides a comprehensive, robust castling system with:
 * - Centralized state management
 * - Advanced LED guidance and error indication
 * - Intelligent error recovery
 * - Timeout handling
 * - Visual player guidance
 * 
 * @author ESP32 Chess Team
 * @date 2024
 */

#ifndef ENHANCED_CASTLING_SYSTEM_H
#define ENHANCED_CASTLING_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h" // Includes player_t
#include "game_led_animations.h" // Includes rgb_color_t

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations - these types are defined in freertos_chess.h
// player_t and rgb_color_t are already defined in freertos_chess.h

/**
 * @brief Castling phase enumeration
 */
typedef enum {
    CASTLING_STATE_IDLE = 0,                    // Rošáda neprobíhá
    CASTLING_STATE_KING_LIFTED,                 // Král zvednut, čeká na přemístění
    CASTLING_STATE_KING_MOVED_WAITING_ROOK,     // Král přemístěn, čeká na věž
    CASTLING_STATE_ROOK_LIFTED,                 // Věž zvednuta, čeká na přemístění
    CASTLING_STATE_COMPLETING,                  // Dokončování rošády
    CASTLING_STATE_ERROR_RECOVERY,              // Chybový stav, náprava
    CASTLING_STATE_COMPLETED                    // Rošáda dokončena
} castling_phase_t;

/**
 * @brief Castling error types
 */
typedef enum {
    CASTLING_ERROR_NONE = 0,
    CASTLING_ERROR_WRONG_KING_POSITION,         // Nesprávná pozice krále
    CASTLING_ERROR_WRONG_ROOK_POSITION,         // Nesprávná pozice věže
    CASTLING_ERROR_TIMEOUT,                     // Timeout během rošády
    CASTLING_ERROR_INVALID_SEQUENCE,            // Neplatná sekvence tahů
    CASTLING_ERROR_HARDWARE_FAILURE,            // Hardware chyba
    CASTLING_ERROR_GAME_STATE_INVALID,          // Neplatný stav hry
    CASTLING_ERROR_MAX_ERRORS_EXCEEDED          // Překročen maximální počet chyb
} castling_error_t;

/**
 * @brief Castling positions structure
 */
typedef struct {
    uint8_t king_from_row, king_from_col;
    uint8_t king_to_row, king_to_col;
    uint8_t rook_from_row, rook_from_col;
    uint8_t rook_to_row, rook_to_col;
} castling_positions_t;

/**
 * @brief LED state for castling
 */
typedef struct {
    uint32_t king_animation_id;
    uint32_t rook_animation_id;
    uint32_t guidance_animation_id;
    bool showing_error;
    bool showing_guidance;
} castling_led_state_t;

/**
 * @brief LED configuration for castling
 */
typedef struct {
    // Barvy pro různé stavy
    struct {
        rgb_color_t king_highlight;      // Zvýraznění krále (zlatá)
        rgb_color_t king_destination;    // Cílové pole krále (zelená)
        rgb_color_t rook_highlight;      // Zvýraznění věže (stříbrná)  
        rgb_color_t rook_destination;    // Cílové pole věže (modrá)
        rgb_color_t error_indication;    // Chybové pole (červená)
        rgb_color_t path_guidance;       // Vedení cesty (žlutá)
    } colors;
    
    // Animace patterns
    struct {
        uint32_t pulsing_speed;          // Rychlost pulzování
        uint32_t guidance_speed;         // Rychlost vedoucích animací
        uint8_t error_flash_count;       // Počet chybových bliknutí
        uint32_t completion_celebration_duration; // Délka oslavné animace
    } timing;
} castling_led_config_t;

/**
 * @brief Error information structure
 */
typedef struct {
    castling_error_t error_type;
    char description[128];
    uint8_t error_led_positions[8];      // Pozice pro červené LED
    uint8_t correction_led_positions[8]; // Pozice pro nápravné LED
    void (*recovery_action)(void);       // Akce pro nápravu
} castling_error_info_t;

/**
 * @brief Main enhanced castling system structure
 */
typedef struct {
    // Stav
    castling_phase_t phase;
    bool active;
    
    // Hráč a typ rošády
    player_t player;
    bool is_kingside;
    
    // Pozice figurek
    castling_positions_t positions;
    
    // Časování a error handling
    uint32_t phase_start_time;
    uint32_t phase_timeout_ms;
    uint8_t error_count;
    uint8_t max_errors;
    
    // LED animace a indikace
    castling_led_state_t led_state;
    
    // Callback pro dokončení
    void (*completion_callback)(bool success);
} enhanced_castling_system_t;

// Global castling system instance
extern enhanced_castling_system_t castling_system;

// LED configuration
extern castling_led_config_t castling_led_config;

/**
 * @brief Initialize enhanced castling system
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_init(void);

/**
 * @brief Start castling sequence
 * @param player Player performing castling
 * @param is_kingside True for kingside, false for queenside
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_start(player_t player, bool is_kingside);

/**
 * @brief Handle king lift event
 * @param row King row position
 * @param col King column position
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_king_lift(uint8_t row, uint8_t col);

/**
 * @brief Handle king drop event
 * @param row King row position
 * @param col King column position
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_king_drop(uint8_t row, uint8_t col);

/**
 * @brief Handle rook lift event
 * @param row Rook row position
 * @param col Rook column position
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_rook_lift(uint8_t row, uint8_t col);

/**
 * @brief Handle rook drop event
 * @param row Rook row position
 * @param col Rook column position
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_rook_drop(uint8_t row, uint8_t col);

/**
 * @brief Cancel current castling sequence
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_cancel(void);

/**
 * @brief Check if castling is currently active
 * @return True if castling is active
 */
bool enhanced_castling_is_active(void);

/**
 * @brief Get current castling phase
 * @return Current castling phase
 */
castling_phase_t enhanced_castling_get_phase(void);

/**
 * @brief Update castling phase with timeout handling
 * @param new_phase New phase to set
 */
void enhanced_castling_update_phase(castling_phase_t new_phase);

/**
 * @brief Handle castling error
 * @param error Error type
 * @param row Row position where error occurred
 * @param col Column position where error occurred
 */
void enhanced_castling_handle_error(castling_error_t error, uint8_t row, uint8_t col);

/**
 * @brief LED guidance functions
 */
void castling_show_king_guidance(void);
void castling_show_rook_guidance(void);
void castling_show_error_indication(castling_error_t error);
void castling_show_completion_celebration(void);
void castling_clear_all_indications(void);

/**
 * @brief Validation functions
 */
bool castling_validate_king_move(uint8_t from_row, uint8_t from_col, 
                                uint8_t to_row, uint8_t to_col);
bool castling_validate_rook_move(uint8_t from_row, uint8_t from_col,
                                uint8_t to_row, uint8_t to_col);
bool castling_validate_sequence(void);

/**
 * @brief Error recovery functions
 */
void castling_recover_king_wrong_position(void);
void castling_recover_rook_wrong_position(void);
void castling_recover_timeout_error(void);
void castling_show_correct_positions(void);
void castling_show_tutorial(void);

/**
 * @brief Internal utility functions
 */
void castling_calculate_positions(player_t player, bool is_kingside);
bool castling_is_timeout_expired(void);
void castling_reset_system(void);
void castling_log_state_change(const char* action);

#ifdef __cplusplus
}
#endif

#endif // ENHANCED_CASTLING_SYSTEM_H
