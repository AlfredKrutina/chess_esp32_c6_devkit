/**
 * @file visual_error_system.h
 * @brief Visual Error System with LED feedback and user guidance
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#ifndef VISUAL_ERROR_SYSTEM_H
#define VISUAL_ERROR_SYSTEM_H

#include "freertos_chess.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Visual error types with specific LED patterns
typedef enum {
    ERROR_VISUAL_INVALID_MOVE = 0,     // Red flash on source/destination
    ERROR_VISUAL_PIECE_BLOCKING,       // Orange highlight on blocking pieces
    ERROR_VISUAL_CHECK_VIOLATION,      // Purple flash on king
    ERROR_VISUAL_NO_PIECE,            // Yellow flash on empty square
    ERROR_VISUAL_WRONG_TURN,          // Blue flash on opponent pieces
    ERROR_VISUAL_PUZZLE_WRONG_MOVE,   // Pink flash with guidance
    ERROR_VISUAL_PUZZLE_WRONG_REMOVAL, // Red flash on wrong piece removal
    ERROR_VISUAL_SYSTEM_ERROR,        // White flash - technical issue
    ERROR_VISUAL_INVALID_SYNTAX,      // Cyan flash - command syntax error
    ERROR_VISUAL_COUNT
} visual_error_type_t;

// Error recovery guidance
typedef struct {
    visual_error_type_t error_type;
    uint8_t error_led_positions[8];    // LEDs to highlight for error
    uint8_t guidance_led_positions[8]; // LEDs to highlight for guidance
    uint8_t error_led_count;
    uint8_t guidance_led_count;
    uint32_t display_duration_ms;
    char user_message[128];            // Human-readable error message
    char recovery_hint[128];           // How to fix the error
} error_guidance_t;

// Error system configuration
typedef struct {
    uint8_t flash_count;               // Number of flashes for error indication
    uint32_t flash_duration_ms;        // Duration of each flash
    uint32_t guidance_duration_ms;     // How long to show guidance
    bool enable_recovery_hints;        // Show recovery hints
    bool enable_sound_feedback;        // Enable sound feedback (if available)
    uint8_t max_concurrent_errors;     // Maximum concurrent error displays
} error_system_config_t;

// Core error API
esp_err_t error_system_init(const error_system_config_t* config);
esp_err_t error_system_deinit(void);

// Visual error display
esp_err_t error_show_visual(visual_error_type_t error_type, 
                           const chess_move_t* failed_move);
esp_err_t error_show_blocking_path(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col);
esp_err_t error_show_check_escape_options(uint8_t king_row, uint8_t king_col);
esp_err_t error_show_puzzle_guidance(const char* puzzle_hint);
esp_err_t error_clear_all_indicators(void);

// User guidance functions
esp_err_t error_guide_correct_move(const chess_move_t* suggested_move);
esp_err_t error_guide_piece_selection(uint8_t* movable_pieces, uint8_t count);
esp_err_t error_guide_valid_destinations(uint8_t from_row, uint8_t from_col);
esp_err_t error_guide_puzzle_removal(uint8_t* pieces_to_remove, uint8_t count);

// Error message and hint retrieval
const char* error_get_last_message(void);
const char* error_get_recovery_hint(void);
const char* error_get_error_type_name(visual_error_type_t type);

// Error system status
esp_err_t error_get_status(char* buffer, size_t buffer_size);
bool error_is_displaying(void);
uint8_t error_get_active_count(void);

// Configuration
esp_err_t error_set_config(const error_system_config_t* config);
esp_err_t error_set_flash_count(uint8_t count);
esp_err_t error_set_flash_duration(uint32_t duration_ms);

// Utility functions
esp_err_t error_convert_move_error_to_visual(move_error_t move_error, visual_error_type_t* visual_type);
esp_err_t error_get_led_positions_for_move(const chess_move_t* move, uint8_t* led_positions, uint8_t* count);
esp_err_t error_get_led_positions_for_square(uint8_t row, uint8_t col, uint8_t* led_positions, uint8_t* count);

#ifdef __cplusplus
}
#endif

#endif // VISUAL_ERROR_SYSTEM_H
