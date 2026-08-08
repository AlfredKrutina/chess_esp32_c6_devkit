/**
 * @file led_unified_api.h  
 * @brief Unified LED API - Unified source of truth for LED control
 * 
 * This header provides a simplified, uniform API for all LED operations.
 * Replaces the messy mix of led_set_pixel_internal, led_set_pixel_safe,
 * led_execute_command_new, etc. with a clean, consistent interface.
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-06
 * 
 * @details
 * Unified LED API provides a simple, consistent interface for
 * all LED operations in the system. Eliminates duplicate functions and provides
 * clearly named functions for each use case.
 */

#ifndef LED_UNIFIED_API_H
#define LED_UNIFIED_API_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// JEDNOTNE LED API - JEDINY ZDROJ PRAVDY
// ============================================================================

/**
 * @brief RGB barevna struktura
 */
typedef struct {
    uint8_t r, g, b; ///< Red, Green, Blue component (0-255)
} led_color_t;

// Predefined colors for consistency
/** @brief Barva - vypnuto (cerna) */
#define LED_COLOR_OFF      ((led_color_t){0, 0, 0})
/** @brief Barva - bila */
#define LED_COLOR_WHITE    ((led_color_t){255, 255, 255})
/** @brief Color - red */
#define LED_COLOR_RED      ((led_color_t){255, 0, 0})
/** @brief Color - green */
#define LED_COLOR_GREEN    ((led_color_t){0, 255, 0})
/** @brief Color - blue */
#define LED_COLOR_BLUE     ((led_color_t){0, 0, 255})
/** @brief Color - yellow */
#define LED_COLOR_YELLOW   ((led_color_t){255, 255, 0})
/** @brief Barva - azurova (cyan) */
#define LED_COLOR_CYAN     ((led_color_t){0, 255, 255})
/** @brief Barva - fialova (magenta) */
#define LED_COLOR_MAGENTA  ((led_color_t){255, 0, 255})

// ============================================================================
// BASE LED FUNCTIONS - ONLY USE THESE
// ============================================================================

/**
 * @brief Set LED color (replaces all led_set_pixel_* variants)
 * 
 * @param position LED position (0-63 drawer, 64-72 button)
 * @param color RGB color to set
 * @return ESP_OK on success
 */
esp_err_t led_set(uint8_t position, led_color_t color);

/**
 * @brief Clear one LED position
 * 
 * @param position LED position (0-72)
 * @return ESP_OK on success
 */
esp_err_t led_clear(uint8_t position);

/**
 * @brief Clear all LEDs (box + button)
 * 
 * @return ESP_OK on success
 */
esp_err_t led_clear_all(void);

/**
 * @brief Clear only the drawer LEDs (0-63), keep the buttons
 * 
 * @return ESP_OK on success
 */
esp_err_t led_clear_board(void);

/**
 * @brief Show legal moves for a piece (highlighted in green)
 * 
 * @param positions Array of LED positions to highlight
 * @param count Number of positions
 * @return ESP_OK on success
 */
esp_err_t led_show_moves(const uint8_t* positions, uint8_t count);

/**
 * @brief Show figures that can move (highlighted in yellow)
 * 
 * @param positions Array of moving figure positions
 * @param count Number of moving figures
 * @return ESP_OK on success
 */
esp_err_t led_show_movable_pieces(const uint8_t* positions, uint8_t count);

/**
 * @brief Start animation (replace led_execute_command_new)
 * 
 * @param type Animation type (string like "move", "check", "endgame")
 * @param from_pos Source position (if relevant)
 * @param to_pos Target position (if relevant)
 * @return ESP_OK on success
 */
esp_err_t led_animate(const char* type, uint8_t from_pos, uint8_t to_pos);

/**
 * @brief Stop all animations
 * 
 * @return ESP_OK on success
 */
esp_err_t led_stop_animations(void);

// ============================================================================
// SACH'S SPECIFIC LED FUNCTIONS
// ============================================================================

/**
 * @brief Show error indication (red highlight)
 * 
 * @param position The position with the error
 * @return ESP_OK on success
 */
esp_err_t led_show_error(uint8_t position);

/**
 * @brief Show figure selection (yellow highlight)
 * 
 * @param position The position of the selected figure
 * @return ESP_OK on success
 */
esp_err_t led_show_selection(uint8_t position);

/**
 * @brief Show collected indication (orange highlight)
 * 
 * @param position Picked position
 * @return ESP_OK on success
 */
esp_err_t led_show_capture(uint8_t position);

/**
 * @brief Commit all LED changes to hardware (batch update)
 * 
 * @return ESP_OK on success
 */
esp_err_t led_commit(void);

#ifdef __cplusplus
}
#endif

#endif // LED_UNIFIED_API_H
