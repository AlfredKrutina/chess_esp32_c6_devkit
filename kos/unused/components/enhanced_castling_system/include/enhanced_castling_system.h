/**
 * @file enhanced_castling_system.h
 * @brief Enhanced Casting System for ESP32 Chessboard
 * 
 * This module provides a comprehensive, robust system for dew with:
 * - Centralized status management
 * - Advanced LED guidance and error indication
 * - Intelligent error recovery
 * - Timeout by handling
 * - Visual cues for players
 * 
 * @author ESP32 Chess Team
 * @date 2024
 * 
 * @details
 * Enhanced Castling System provides an advanced castling management system.
 * Assists players with the execution of the rozade with the help of LED guidance, detects errors
 * and enables automatic correction.
 */

#ifndef ENHANCED_CASTLING_SYSTEM_H
#define ENHANCED_CASTLING_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"
#include "game_led_animations.h"

#ifdef __cplusplus
extern "C" {
#endif

// Predni deklarace - tyto typy jsou definovany v freertos_chess.h
// player_t a rgb_color_t jsou jiz definovany v freertos_chess.h

// ============================================================================
// ENUMERATION OF DEW PHASES
// ============================================================================

/**
 * @brief Dew phase
 * 
 * Defines all phases of the castling process from start to finish.
 */
typedef enum {
    CASTLING_STATE_IDLE = 0,                    ///< Castling is not running
    CASTLING_STATE_KING_LIFTED,                 ///< Kral raised, waiting for transfer
    CASTLING_STATE_KING_MOVED_WAITING_ROOK,     ///< Kral premisten, waiting for the carriage
    CASTLING_STATE_ROOK_LIFTED,                 ///< Lifted, waiting for relocation
    CASTLING_STATE_COMPLETING,                  ///< Dokoncovani rosady
    CASTLING_STATE_ERROR_RECOVERY,              ///< Error status, axis
    CASTLING_STATE_COMPLETED                    ///< Castling finished
} castling_phase_t;

// ============================================================================
// TYPES OF DEW ERRORS
// ============================================================================

/**
 * @brief Typy chyb pri rosade
 */
typedef enum {
    CASTLING_ERROR_NONE = 0,                 ///< No error
    CASTLING_ERROR_WRONG_KING_POSITION,      ///< Incorrect king position
    CASTLING_ERROR_WRONG_ROOK_POSITION,      ///< Wrong carriage position
    CASTLING_ERROR_TIMEOUT,                  ///< Timeout behem rosady
    CASTLING_ERROR_INVALID_SEQUENCE,         ///< Invalid move sequence
    CASTLING_ERROR_HARDWARE_FAILURE,         ///< Hardware error
    CASTLING_ERROR_GAME_STATE_INVALID,       ///< Invalid game state
    CASTLING_ERROR_MAX_ERRORS_EXCEEDED       ///< Prekrocen maximalni pocet chyb
} castling_error_t;

// ============================================================================
// STRUCTURES FOR DEW POSITIONS
// ============================================================================

/**
 * @brief The structure of the positions in the rosade
 * 
 * Contains the source and target positions of the king and rook in rosade.
 */
typedef struct {
    uint8_t king_from_row, king_from_col; ///< Source's king position
    uint8_t king_to_row, king_to_col;     ///< Target's king position
    uint8_t rook_from_row, rook_from_col; ///< Source position of carriage
    uint8_t rook_to_row, rook_to_col;     ///< Target carriage position
} castling_positions_t;

/**
 * @brief LED status for dew
 */
typedef struct {
    uint32_t king_animation_id;      ///< ID animace krale
    uint32_t rook_animation_id;      ///< ID animace veze
    uint32_t guidance_animation_id;  ///< ID navodove animace
    bool showing_error;              ///< Displayed and error indication?
    bool showing_guidance;           ///< Is guidance shown?
} castling_led_state_t;

/**
 * @brief LED configuration for rosadu
 */
typedef struct {
    // Barvy pro ruzne stavy
    struct {
        rgb_color_t king_highlight;      ///< Zvyrazneni krale (zlata)
        rgb_color_t king_destination;    ///< King's target field (green)
        rgb_color_t rook_highlight;      ///< Zvyrazneni veze (stribrna)
        rgb_color_t rook_destination;    ///< Target field carries (blue)
        rgb_color_t error_indication;    ///< Error field (red)
        rgb_color_t path_guidance;       ///< Route guidance (yellow)
    } colors; ///< Paleta barev pro rosadu
    
    // Animation patterns
    struct {
        uint32_t pulsing_speed;                   ///< Rychlost pulzovani
        uint32_t guidance_speed;                  ///< Rychlost vedoucich animaci
        uint8_t error_flash_count;                ///< Number of error flashes
        uint32_t completion_celebration_duration; ///< Delka oslavne animace
    } timing; ///< Casovani animaci
} castling_led_config_t;

/**
 * @brief Struktura informaci o chybe
 */
typedef struct {
    castling_error_t error_type;         ///< Typ chyby
    char description[128];               ///< Popis chyby
    uint8_t error_led_positions[8];      ///< Position for red LED
    uint8_t correction_led_positions[8]; ///< Position for fix LED
    void (*recovery_action)(void);       ///< Akce pro napravu
} castling_error_info_t;

/**
 * @brief The main structure of the enhanced castling system
 */
typedef struct {
    // Stav
    castling_phase_t phase;  ///< Current dew phase
    bool active;             ///< Is Castling active?
    
    // Player and roster type
    player_t player;         ///< The player performing the cast
    bool is_kingside;        ///< Is it a kingside?
    
    // Position of figures
    castling_positions_t positions; ///< Position of King and Queen
    
    // Casovani a error handling
    uint32_t phase_start_time;  ///< Phase start time
    uint32_t phase_timeout_ms;  ///< Phase timeout in ms
    uint8_t error_count;        ///< Pocet chyb
    uint8_t max_errors;         ///< Maximalni pocet chyb
    
    // LED animation and indication
    castling_led_state_t led_state; ///< Stav LED animaci
    
    // Callback for completion
    void (*completion_callback)(bool success); ///< Callback after completion
} enhanced_castling_system_t;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

/** @brief Global instances of the castling system */
extern enhanced_castling_system_t castling_system;

/** @brief LED configuration for dew */
extern castling_led_config_t castling_led_config;

// ============================================================================
// MAIN API FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the enhanced castling system
 * 
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_init(void);

/**
 * @brief Start the dew sequence
 * 
 * @param player The player performing the roster
 * @param is_kingside True for small rosa, false for large rosa
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_start(player_t player, bool is_kingside);

/**
 * @brief Handle the raised king event
 * 
 * @param row Radek king (0-7)
 * @param col King column (0-7)
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_king_lift(uint8_t row, uint8_t col);

/**
 * @brief Handle the king placement event
 * 
 * @param row Radek king (0-7)
 * @param col King column (0-7)
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_king_drop(uint8_t row, uint8_t col);

/**
 * @brief Handle the lift event
 * 
 * @param row Radek drives (0-7)
 * @param col Carrier column (0-7)
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_rook_lift(uint8_t row, uint8_t col);

/**
 * @brief Handle the wire position event
 * 
 * @param row Radek drives (0-7)
 * @param col Carrier column (0-7)
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_handle_rook_drop(uint8_t row, uint8_t col);

/**
 * @brief Cancel the current dew sequence
 * 
 * @return ESP_OK on success
 */
esp_err_t enhanced_castling_cancel(void);

/**
 * @brief Check if castling is currently active
 * 
 * @return True if dew is in progress
 */
bool enhanced_castling_is_active(void);

/**
 * @brief Get the current dew phase
 * 
 * @return Current dew phase
 */
castling_phase_t enhanced_castling_get_phase(void);

/**
 * @brief Update dew phase with timeout handling
 * 
 * @param new_phase New phase to set
 */
void enhanced_castling_update_phase(castling_phase_t new_phase);

/**
 * @brief Handle dew error
 * 
 * @param error Typ chyby
 * @param row Radek kde doslo k chybe
 * @param col Sloupec kde doslo k chybe
 */
void enhanced_castling_handle_error(castling_error_t error, uint8_t row, uint8_t col);

// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Show LED guide for kings
 */
void castling_show_king_guidance(void);

/**
 * @brief Show LED instructions for transport
 */
void castling_show_rook_guidance(void);

/**
 * @brief Display the error indication
 * 
 * @param error Typ chyby k zobrazeni
 */
void castling_show_error_indication(castling_error_t error);

/**
 * @brief Show the celebration of the completion of the dew
 */
void castling_show_completion_celebration(void);

/**
 * @brief Clear all indications of dew
 */
void castling_clear_all_indications(void);

// ============================================================================
// VALIDATION FUNCTION
// ============================================================================

/**
 * @brief Validate the king's move for rosadu
 * 
 * @param from_row Source rows of the king
 * @param from_col Source column of the king
 * @param to_row The target row of the king
 * @param to_col Target column of the king
 * @return true if the king's move is valid for rosadu
 */
bool castling_validate_king_move(uint8_t from_row, uint8_t from_col, 
                                uint8_t to_row, uint8_t to_col);

/**
 * @brief Validate carriage pull for rosadu
 * 
 * @param from_row The source row of the vehicle
 * @param from_col The source column of the vehicle
 * @param to_row The target row to carry
 * @param to_col Target column to carry
 * @return true if the carriage move is valid for dew
 */
bool castling_validate_rook_move(uint8_t from_row, uint8_t from_col,
                                uint8_t to_row, uint8_t to_col);

/**
 * @brief Validate dew sequence
 * 
 * @return true if the sequence is valid
 */
bool castling_validate_sequence(void);

// ============================================================================
// ERROR RECOVERY FUNCTION
// ============================================================================

/**
 * @brief Recovering from an error will not correct the king's position
 */
void castling_recover_king_wrong_position(void);

/**
 * @brief Recovering from an incorrect carriage position error
 */
void castling_recover_rook_wrong_position(void);

/**
 * @brief Recovering from a timeout error
 */
void castling_recover_timeout_error(void);

/**
 * @brief Display the correct positions for dewdrops
 */
void castling_show_correct_positions(void);

/**
 * @brief View Rosada's tutorial
 */
void castling_show_tutorial(void);

// ============================================================================
// INTERNAL UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Compute the positions for dew
 * 
 * @param player The player performing the roster
 * @param is_kingside True for small rosa, false for large rosa
 */
void castling_calculate_positions(player_t player, bool is_kingside);

/**
 * @brief Check if the timeout has expired
 * 
 * @return true if the timeout has expired
 */
bool castling_is_timeout_expired(void);

/**
 * @brief Resetuj castling system
 */
void castling_reset_system(void);

/**
 * @brief Log the state change
 * 
 * @param action Action description
 */
void castling_log_state_change(const char* action);

#ifdef __cplusplus
}
#endif

#endif // ENHANCED_CASTLING_SYSTEM_H
