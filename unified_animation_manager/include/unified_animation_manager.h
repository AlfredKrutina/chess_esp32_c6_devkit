/**
 * @file unified_animation_manager.h
 * @brief Unified Animation Manager - Single source of truth for all LED animations
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#ifndef UNIFIED_ANIMATION_MANAGER_H
#define UNIFIED_ANIMATION_MANAGER_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Animation priority levels
typedef enum {
    ANIM_PRIORITY_CRITICAL = 0,    // Error indicators, system messages
    ANIM_PRIORITY_HIGH = 1,        // Game moves, captures
    ANIM_PRIORITY_MEDIUM = 2,      // Piece guidance, valid moves
    ANIM_PRIORITY_LOW = 3,         // Ambient effects, idle animations
    ANIM_PRIORITY_BACKGROUND = 4   // Screen saver, subtle effects
} animation_priority_t;

// Animation types with smooth transitions
typedef enum {
    ANIM_TYPE_MOVE_PATH = 0,       // Smooth piece movement with trail
    ANIM_TYPE_PIECE_GUIDANCE,      // Gentle pulsing for movable pieces
    ANIM_TYPE_VALID_MOVES,         // Subtle highlight for valid destinations
    ANIM_TYPE_ERROR_FLASH,         // Clear error indication
    ANIM_TYPE_PUZZLE_SETUP,        // Puzzle piece removal guidance
    ANIM_TYPE_CAPTURE_EFFECT,      // Satisfying capture animation
    ANIM_TYPE_CHECK_WARNING,       // Urgent but not distracting
    ANIM_TYPE_GAME_END,            // Celebratory end game animation
    ANIM_TYPE_PLAYER_CHANGE,       // Player change animation
    ANIM_TYPE_CASTLE,              // Castling animation
    ANIM_TYPE_PROMOTION,           // Promotion animation
    ANIM_TYPE_PUZZLE_PATH,         // Puzzle solving guidance
    ANIM_TYPE_CONFIRMATION,        // Action confirmation
    ANIM_TYPE_ENDGAME_WAVE,        // Endgame wave animation
    ANIM_TYPE_ENDGAME_CIRCLES,     // Endgame circles animation
    ANIM_TYPE_ENDGAME_CASCADE,     // Endgame cascade animation
    ANIM_TYPE_ENDGAME_FIREWORKS,   // Endgame fireworks animation
    ANIM_TYPE_ENDGAME_DRAW_SPIRAL, // Draw animation - spiral pattern
    ANIM_TYPE_ENDGAME_DRAW_PULSE,  // Draw animation - pulsing pattern
    ANIM_TYPE_COUNT
} animation_type_t;

// Smooth interpolation system
typedef struct animation_state_s {
    uint8_t from_led;
    uint8_t to_led;
    float progress;                 // 0.0 to 1.0
    uint32_t duration_ms;
    uint32_t start_time;
    uint8_t trail_length;          // Number of LEDs in trail
    struct {
        uint8_t r, g, b;
    } color_start, color_end;
    bool (*update_func)(struct animation_state_s* anim);
    bool active;
    animation_priority_t priority;
    animation_type_t type;
    uint32_t id;
} animation_state_t;

// Animation configuration
typedef struct {
    uint8_t max_concurrent_animations;
    uint8_t update_frequency_hz;   // Target FPS
    bool enable_smooth_interpolation;
    bool enable_trail_effects;
    uint32_t default_duration_ms;
} animation_config_t;

// ============================================================================
// INITIALIZATION AND MANAGEMENT
// ============================================================================

/**
 * @brief Initialize the unified animation manager
 * @param config Animation system configuration
 * @return ESP_OK on success, error code on failure
 */
esp_err_t animation_manager_init(const animation_config_t* config);

/**
 * @brief Deinitialize the animation manager and cleanup resources
 * @return ESP_OK on success, error code on failure
 */
esp_err_t animation_manager_deinit(void);

// Animation creation and control
uint32_t unified_animation_create(animation_type_t type, animation_priority_t priority);
esp_err_t animation_start_move(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms);
esp_err_t animation_start_guidance(uint32_t anim_id, uint8_t* led_array, uint8_t count);
esp_err_t animation_start_error(uint32_t anim_id, uint8_t led_index, uint32_t flash_count);
esp_err_t animation_start_puzzle_removal(uint32_t anim_id, uint8_t* pieces_to_remove, uint8_t count);
esp_err_t animation_start_capture(uint32_t anim_id, uint8_t led_index, uint32_t duration_ms);
esp_err_t animation_start_confirmation(uint32_t anim_id, uint8_t led_index);
esp_err_t animation_start_endgame_wave(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);
esp_err_t animation_start_endgame_circles(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);
esp_err_t animation_start_endgame_cascade(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);
esp_err_t animation_start_endgame_fireworks(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);
esp_err_t animation_start_endgame_draw_spiral(uint32_t anim_id, uint8_t center_led);
esp_err_t animation_start_endgame_draw_pulse(uint32_t anim_id, uint8_t center_led);
esp_err_t animation_start_promotion(uint32_t anim_id, uint8_t promotion_led);
esp_err_t unified_animation_stop(uint32_t anim_id);
esp_err_t animation_stop_all_except_priority(animation_priority_t min_priority);
esp_err_t unified_animation_stop_all(void);

// Non-blocking update system
void animation_manager_update(void);
bool animation_is_running(uint32_t anim_id);
uint8_t animation_get_active_count(void);
uint8_t animation_get_active_count_by_priority(animation_priority_t priority);

// Configuration
esp_err_t animation_set_config(const animation_config_t* config);
esp_err_t animation_set_default_duration(uint32_t duration_ms);
esp_err_t animation_set_update_frequency(uint8_t frequency_hz);

// Utility functions
const char* animation_get_type_name(animation_type_t type);
const char* animation_get_priority_name(animation_priority_t priority);
esp_err_t animation_get_status(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // UNIFIED_ANIMATION_MANAGER_H
