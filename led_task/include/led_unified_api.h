/**
 * @file led_unified_api.h  
 * @brief Unified LED API - Single Source of Truth for LED Control
 * @author Alfred Krutina
 * @version 2.5
 * @date 2025-09-06
 * 
 * This header provides a simplified, unified API for all LED operations.
 * Replaces the chaotic mix of led_set_pixel_internal, led_set_pixel_safe,
 * led_execute_command_new, etc. with a clean, consistent interface.
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
// UNIFIED LED API - SINGLE SOURCE OF TRUTH
// ============================================================================

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r, g, b;
} led_color_t;

// Predefined colors for consistency
#define LED_COLOR_OFF      ((led_color_t){0, 0, 0})
#define LED_COLOR_WHITE    ((led_color_t){255, 255, 255})
#define LED_COLOR_RED      ((led_color_t){255, 0, 0})
#define LED_COLOR_GREEN    ((led_color_t){0, 255, 0})
#define LED_COLOR_BLUE     ((led_color_t){0, 0, 255})
#define LED_COLOR_YELLOW   ((led_color_t){255, 255, 0})
#define LED_COLOR_CYAN     ((led_color_t){0, 255, 255})
#define LED_COLOR_MAGENTA  ((led_color_t){255, 0, 255})

// ============================================================================
// CORE LED FUNCTIONS - USE THESE ONLY
// ============================================================================

/**
 * @brief Set LED color (replaces all led_set_pixel_* variants)
 * @param position LED position (0-63 for board, 64-72 for buttons)  
 * @param color RGB color to set
 * @return ESP_OK on success
 */
esp_err_t led_set(uint8_t position, led_color_t color);

/**
 * @brief Clear single LED position
 * @param position LED position (0-72)
 * @return ESP_OK on success
 */
esp_err_t led_clear(uint8_t position);

/**
 * @brief Clear all LEDs (board + buttons)
 * @return ESP_OK on success
 */
esp_err_t led_clear_all(void);

/**
 * @brief Clear only board LEDs (0-63), keep buttons
 * @return ESP_OK on success  
 */
esp_err_t led_clear_board(void);

/**
 * @brief Show legal moves for a piece (green highlights)
 * @param positions Array of LED positions to highlight
 * @param count Number of positions
 * @return ESP_OK on success
 */
esp_err_t led_show_moves(const uint8_t* positions, uint8_t count);

/**
 * @brief Show piece that can be moved (yellow highlight)
 * @param positions Array of movable piece positions
 * @param count Number of movable pieces
 * @return ESP_OK on success
 */
esp_err_t led_show_movable_pieces(const uint8_t* positions, uint8_t count);

/**
 * @brief Start animation (replaces led_execute_command_new)
 * @param type Animation type
 * @param from_pos Source position (if applicable)
 * @param to_pos Destination position (if applicable)
 * @return ESP_OK on success
 */
esp_err_t led_animate(const char* type, uint8_t from_pos, uint8_t to_pos);

/**
 * @brief Stop all animations
 * @return ESP_OK on success
 */
esp_err_t led_stop_animations(void);

// ============================================================================
// CHESS-SPECIFIC LED FUNCTIONS
// ============================================================================

/**
 * @brief Show error indication (red highlight)
 * @param position Position with error
 * @return ESP_OK on success
 */
esp_err_t led_show_error(uint8_t position);

/**
 * @brief Show piece selection (yellow highlight)
 * @param position Selected piece position
 * @return ESP_OK on success
 */
esp_err_t led_show_selection(uint8_t position);

/**
 * @brief Show capture indication (orange highlight)
 * @param position Capture position
 * @return ESP_OK on success
 */
esp_err_t led_show_capture(uint8_t position);

/**
 * @brief Commit all LED changes to hardware (batch update)
 * @return ESP_OK on success
 */
esp_err_t led_commit(void);

#ifdef __cplusplus
}
#endif

#endif // LED_UNIFIED_API_H
