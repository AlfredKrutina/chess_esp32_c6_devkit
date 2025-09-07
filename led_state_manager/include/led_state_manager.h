/**
 * @file led_state_manager.h
 * @brief Optimized LED State Manager with layer system and smooth updates
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#ifndef LED_STATE_MANAGER_H
#define LED_STATE_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LED layer system for proper compositing
typedef enum {
    LED_LAYER_BACKGROUND = 0,  // Board squares, static elements
    LED_LAYER_PIECES,         // Piece indicators
    LED_LAYER_GUIDANCE,       // Move guidance, valid moves
    LED_LAYER_ANIMATION,      // Active animations
    LED_LAYER_ERROR,          // Error indicators (highest priority)
    LED_LAYER_COUNT
} led_layer_t;

// LED update optimization
typedef struct {
    uint8_t r, g, b;
    bool dirty;               // Needs hardware update
    uint32_t last_update;     // Timestamp of last change
    uint8_t brightness;       // 0-255 brightness level
} led_pixel_t;

// Layer state management
typedef struct {
    led_pixel_t pixels[73];   // 64 board + 9 buttons
    bool layer_enabled;
    uint8_t layer_opacity;    // 0-255
    bool needs_composite;     // Layer changed, needs recomposite
} led_layer_state_t;

// LED manager configuration
typedef struct {
    uint8_t max_brightness;           // Maximum brightness (0-255)
    uint8_t default_brightness;       // Default brightness (0-255)
    bool enable_auto_brightness;      // Auto brightness based on ambient
    bool enable_smooth_transitions;   // Smooth color transitions
    uint32_t transition_duration_ms;  // Transition duration
    bool enable_layer_compositing;    // Enable layer compositing
    uint8_t update_frequency_hz;      // Update frequency
} led_manager_config_t;

// Core LED manager API
esp_err_t led_manager_init(const led_manager_config_t* config);
esp_err_t led_manager_deinit(void);

// Layer management
esp_err_t led_set_pixel_layer(led_layer_t layer, uint8_t led_index, 
                              uint8_t r, uint8_t g, uint8_t b);
esp_err_t led_clear_layer(led_layer_t layer);
esp_err_t led_set_layer_opacity(led_layer_t layer, uint8_t opacity);
esp_err_t led_enable_layer(led_layer_t layer, bool enable);
esp_err_t led_set_layer_brightness(led_layer_t layer, uint8_t brightness);

// Optimized update system
esp_err_t led_composite_and_update(void);
esp_err_t led_force_full_update(void);
uint8_t led_get_dirty_count(void);
uint8_t led_get_dirty_count_by_layer(led_layer_t layer);

// Smooth color transitions
esp_err_t led_fade_pixel(uint8_t led_index, uint8_t target_r, uint8_t target_g, 
                        uint8_t target_b, uint32_t duration_ms);
esp_err_t led_pulse_pixel(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b,
                         uint32_t period_ms, uint8_t pulse_count);
esp_err_t led_rainbow_pixel(uint8_t led_index, uint32_t duration_ms);

// Batch operations
esp_err_t led_set_multiple_pixels(led_layer_t layer, uint8_t* led_indices, 
                                 uint8_t count, uint8_t r, uint8_t g, uint8_t b);
esp_err_t led_clear_multiple_pixels(led_layer_t layer, uint8_t* led_indices, uint8_t count);
esp_err_t led_fade_multiple_pixels(uint8_t* led_indices, uint8_t count,
                                  uint8_t target_r, uint8_t target_g, uint8_t target_b,
                                  uint32_t duration_ms);

// Brightness control
esp_err_t led_set_global_brightness(uint8_t brightness);
esp_err_t led_set_pixel_brightness(uint8_t led_index, uint8_t brightness);
uint8_t led_get_global_brightness(void);
uint8_t led_get_pixel_brightness(uint8_t led_index);

// Color utilities
esp_err_t led_hsv_to_rgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b);
esp_err_t led_rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float* h, float* s, float* v);
esp_err_t led_interpolate_color(uint8_t r1, uint8_t g1, uint8_t b1,
                               uint8_t r2, uint8_t g2, uint8_t b2,
                               float progress, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b);

// Status and debugging
esp_err_t led_get_status(char* buffer, size_t buffer_size);
esp_err_t led_get_layer_status(led_layer_t layer, char* buffer, size_t buffer_size);
bool led_is_pixel_dirty(uint8_t led_index);
uint32_t led_get_last_update_time(uint8_t led_index);

// Configuration
esp_err_t led_set_config(const led_manager_config_t* config);
esp_err_t led_set_update_frequency(uint8_t frequency_hz);
esp_err_t led_set_transition_duration(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif // LED_STATE_MANAGER_H
