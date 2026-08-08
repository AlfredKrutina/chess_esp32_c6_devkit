/**
 * @file visual_error_system.h
 * @brief Visual Error System - Visual error indications and instructions for users
 * 
 * This module provides a visual system for handling errors:
 * - LED error indication
 * - Text description of errors
 * - User manual for repair
 * - Priority system for multiple errors at the same time
 * - Integration with other visual systems
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * The visual system for processing errors provides a comprehensive display system
 * user errors. It combines LED indications with a text description
 * and provides clear instructions on how to correct the error.
 * 
 * Advantages:
 * - Bright LED error indication
 * - Translations of errors into the path
 * - Step-by-step instructions for repair
 * - Automatically disappear after repair
 * - Support for multiple bugs at once
 */

#ifndef VISUAL_ERROR_SYSTEM_H
#define VISUAL_ERROR_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPY VIZUALNICH CHYB
// ============================================================================

/**
 * @brief Typy vizualnich chyb
 */
typedef enum {
    // Game bugs
    ERROR_VISUAL_INVALID_MOVE = 0,     ///< Neplatny tah
    ERROR_VISUAL_WRONG_TURN,           ///< Poor turn based game
    ERROR_VISUAL_NO_PIECE,             ///< Figure not found
    ERROR_VISUAL_PIECE_BLOCKING,       ///< Figure blocked
    ERROR_VISUAL_CHECK_VIOLATION,      ///< Sach nereseny
    ERROR_VISUAL_SYSTEM_ERROR,         ///< System error
    ERROR_VISUAL_INVALID_SYNTAX,       ///< Neplatna syntaxe
    
    ERROR_VISUAL_COUNT                 ///< Pocet typu chyb
} visual_error_type_t;

// ============================================================================
// STRUKTURA ERROR GUIDANCE
// ============================================================================

/**
 * @brief A structure for error handling
 */
typedef struct {
    visual_error_type_t error_type;    ///< Typ chyby
    uint8_t error_led_positions[16];   ///< Error position LED
    uint8_t error_led_count;           ///< Pocet error LED
    uint8_t guidance_led_positions[16];///< LED position for guidance
    uint8_t guidance_led_count;        ///< Pocet guidance LED
    char user_message[128];            ///< Message to user
    char recovery_hint[128];           ///< Napoveda pro opravu
    uint8_t led_positions[16];         ///< LED position to highlight (legacy)
    uint8_t led_count;                 ///< Pocet LED k zvyrazneni (legacy)
    uint32_t display_duration_ms;      ///< Jak dlouho zobrazit (ms)
    bool require_user_confirm;         ///< Vyzaduje potvrzeni?
} error_guidance_t;

/**
 * @brief Konfigurace error systemu
 */
typedef struct {
    uint8_t flash_count;               ///< Pocet bliknuti
    uint32_t flash_duration_ms;        ///< Delka bliknuti (ms)
    uint32_t guidance_duration_ms;     ///< Delka navodu (ms)
    bool enable_recovery_hints;        ///< Povolit napovedy
    uint8_t max_concurrent_errors;     ///< Max soucasnych chyb
} error_system_config_t;

/**
 * @brief Active error structure
 */
typedef struct {
    visual_error_type_t type;          ///< Typ chyby
    uint32_t id;                       ///< Unikatni ID instance
    bool active;                       ///< Is it active?
    uint32_t start_time;               ///< Cas zobrazeni
    uint32_t animation_id;             ///< ID LED animace
    uint8_t row, col;                  ///< Error position (if relevant)
    bool user_confirmed;               ///< Confirmed by user?
} active_error_t;

// ============================================================================
// INITIALIZE AND BASE FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the visual error system
 * 
 * @return ESP_OK on success
 */
esp_err_t visual_error_init(void);

/**
 * @brief Initialize error system with configuration
 * 
 * @param config System error configuration
 * @return ESP_OK on success
 */
esp_err_t error_system_init(const error_system_config_t* config);

/**
 * @brief Deinitialize the error system
 * 
 * @return ESP_OK on success
 */
esp_err_t error_system_deinit(void);

/**
 * @brief Display a visual error
 * 
 * @param error_type Error type
 * @param row Error row (if relevant, otherwise 0xFF)
 * @param col Error column (if relevant, otherwise 0xFF)
 * @return the error ID or 0 on failure
 */
uint32_t visual_error_show(visual_error_type_t error_type, 
                           uint8_t row, uint8_t col);

/**
 * @brief Clear visual error
 * 
 * @param error_id The ID of the error to clear
 * @return ESP_OK on success
 */
esp_err_t visual_error_clear(uint32_t error_id);

/**
 * @brief Clear all visual errors
 * 
 * @return ESP_OK on success
 */
esp_err_t visual_error_clear_all(void);

/**
 * @brief Clear all error indicators
 * @return ESP_OK on success
 */
esp_err_t error_clear_all_indicators(void);

/**
 * @brief Acknowledge error (for errors requiring acknowledgment)
 * 
 * @param error_id The ID of the error
 * @return ESP_OK on success
 */
esp_err_t visual_error_confirm(uint32_t error_id);

// ============================================================================
// USER GUIDANCE FUNCTIONS
// ============================================================================

/**
 * @brief Display a user guide to the error
 * 
 * @param error_type Error type
 */
void visual_error_show_guidance(visual_error_type_t error_type);

/**
 * @brief Print detailed error text
 * 
 * @param error_type Error type
 */
void visual_error_print_details(visual_error_type_t error_type);

/**
 * @brief Highlight problematic positions on the chest
 * 
 * @param error_type Error type
 * @param positions Array of positions to highlight
 * @param count Number of positions
 */
void visual_error_highlight_positions(visual_error_type_t error_type,
                                      const uint8_t* positions, 
                                      uint8_t count);

// ============================================================================
// SPECIFICKE CHYBOVE SITUACE
// ============================================================================

/**
 * @brief Display an invalid move error
 * 
 * @param from_row Source rows
 * @param from_col The source column
 * @param to_row Target rows
 * @param to_col Destination column
 * @return the ID of the error
 */
uint32_t visual_error_illegal_move(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, uint8_t to_col);

/**
 * @brief Display the error of the wrong player on the turn
 * 
 * @param row Radek
 * @param col The column
 * @return the ID of the error
 */
uint32_t visual_error_wrong_turn(uint8_t row, uint8_t col);

/**
 * @brief Display blocked figure error
 * 
 * @param row Radek
 * @param col The column
 * @param blocked_positions Blocked positions
 * @param count Number of blocked positions
 * @return the ID of the error
 */
uint32_t visual_error_piece_blocked(uint8_t row, uint8_t col,
                                    const uint8_t* blocked_positions,
                                    uint8_t count);

/**
 * @brief Display an unsolved sach error
 * 
 * @param king_row King row
 * @param king_col The king column
 * @param threat_positions Threat positions
 * @param threat_count Number of threatened positions
 * @return the ID of the error
 */
uint32_t visual_error_check_not_resolved(uint8_t king_row, uint8_t king_col,
                                         const uint8_t* threat_positions,
                                         uint8_t threat_count);

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Verify if error is active
 * 
 * @param error_id The ID of the error
 * @return true if error is active
 */
bool visual_error_is_active(uint32_t error_id);

/**
 * @brief Number of active errors
 * 
 * @return The number of active errors
 */
uint8_t visual_error_get_active_count(void);

/**
 * @brief Vypis status erroru
 */
void visual_error_print_status(void);

/**
 * @brief Set the error system configuration
 * 
 * @param config The new configuration
 * @return ESP_OK on success
 */
esp_err_t error_set_config(const error_system_config_t* config);

/**
 * @brief Set the number of blinks
 * 
 * @param count Number of blinks
 * @return ESP_OK on success
 */
esp_err_t error_set_flash_count(uint8_t count);

/**
 * @brief Set the blink duration
 * 
 * @param duration_ms Duration in ms
 * @return ESP_OK on success
 */
esp_err_t error_set_flash_duration(uint32_t duration_ms);

/**
 * @brief Get the LED positions for the move
 * 
 * @param moveMove(chess_move_t*)
 * @param led_positions Output LED positions
 * @param count Output the number of LEDs
 * @return ESP_OK on success
 */
esp_err_t error_get_led_positions_for_move(const chess_move_t* move, uint8_t* led_positions, uint8_t* count);

/**
 * @brief Get the LED positions for the array
 * 
 * @param row Radek
 * @param col The column
 * @param led_positions Output LED positions
 * @param count Output the number of LEDs
 * @return ESP_OK on success
 */
esp_err_t error_get_led_positions_for_square(uint8_t row, uint8_t col, uint8_t* led_positions, uint8_t* count);

/**
 * @brief Get system error status
 * 
 * @param buffer Buffer for the status
 * @param buffer_size Buffer size
 * @return ESP_OK on success
 */
esp_err_t error_get_status(char* buffer, size_t buffer_size);

/**
 * @brief Show visual error (internal)
 * 
 * @param error_type Error type
 * @param failed_move Failed move(chess_move_t*)
 * @return ESP_OK on success
 */
esp_err_t error_show_visual(visual_error_type_t error_type, const chess_move_t* failed_move);

/**
 * @brief Display path blocking
 * 
 * @param from_row From rows
 * @param from_col The from column
 * @param to_row To rows
 * @param to_col To column
 * @return ESP_OK on success
 */
esp_err_t error_show_blocking_path(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Show escape options
 * 
 * @param king_row King row
 * @param king_col The king column
 * @return ESP_OK on success
 */
esp_err_t error_show_check_escape_options(uint8_t king_row, uint8_t king_col);

/**
 * @brief Guidance on the right move
 * 
 * @param correct_move Correct move (chess_move_t*)
 * @return ESP_OK on success
 */
esp_err_t error_guide_correct_move(const chess_move_t* correct_move);

/**
 * @brief Instructions for choosing a figurine
 * 
 * @param movable_pieces Moving pieces
 * @param count The count
 * @return ESP_OK on success
 */
esp_err_t error_guide_piece_selection(uint8_t* movable_pieces, uint8_t count);

/**
 * @brief Guide to valid targets
 * 
 * @param from_row From rows
 * @param from_col The from column
 * @return ESP_OK on success
 */
esp_err_t error_guide_valid_destinations(uint8_t from_row, uint8_t from_col);

/**
 * @brief Get the latest error message
 * 
 * @return Zprava
 */
const char* error_get_last_message(void);

/**
 * @brief Ziskej napovedu k oprave
 * 
 * @return Napoveda
 */
const char* error_get_recovery_hint(void);

/**
 * @brief Converted move error to visual error
 * 
 * @param move_error Move error
 * @param visual_type Output visual type
 * @return ESP_OK on success
 */
esp_err_t error_convert_move_error_to_visual(move_error_t move_error, visual_error_type_t* visual_type);

/**
 * @brief Update errors (call periodically)
 * 
 * @return ESP_OK on success
 */
esp_err_t visual_error_update(void);

#ifdef __cplusplus
}
#endif

#endif // VISUAL_ERROR_SYSTEM_H
