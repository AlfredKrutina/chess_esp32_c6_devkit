/**
 * @file led_state_manager.h
 * @brief LED State Manager - Advanced LED state and layer management
 * 
 * This module provides comprehensive LED status management:
 * - Layer system for LED effects
 * - Priority LED manager
 * - Persistent LED status
 * - Optimized updates
 * - Support for composite effects
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * LED State Manager provides an advanced system for managing the LED state.
 * Allows you to combine several effects at once with the help of a layer system,
 * where each layer has its own priority and blending mode.
 * 
 * Advantages:
 * - Layer compositing (box + effects + GUI)
 * - Priority system with alpha blending
 * - Optimized dirty-pixel updates
 * - Thread-safe access to state
 * - Persistence between updates
 */

#ifndef LED_STATE_MANAGER_H
#define LED_STATE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// LED VRSTVY
// ============================================================================

/**
 * @brief LED vrstvy (layers)
 * 
 * Lower number = lower layer (background), higher number = higher layer (foreground)
 */
typedef enum {
    LED_LAYER_BACKGROUND = 0,     ///< Pozadi (board base color)
    LED_LAYER_PIECES = 1,         ///< Figurines
    LED_LAYER_MOVES = 2,          ///< Legal moves
    LED_LAYER_SELECTION = 3,      ///< Select figures
    LED_LAYER_ANIMATION = 4,      ///< Animace (tah, capture, atd.)
    LED_LAYER_STATUS = 5,         ///< Status (check, checkmate)
    LED_LAYER_ERROR = 6,          ///< Error indication
    LED_LAYER_GUI = 7,            ///< GUI overlay (buttons, atd.)
    LED_LAYER_COUNT               ///< Pocet vrstev
} led_layer_t;

/**
 * @brief Blending modes pro LED vrstvy
 */
typedef enum {
    BLEND_REPLACE = 0,    ///< Nahrad spodni vrstvy (alpha = 1.0)
    BLEND_ALPHA,          ///< Alpha blending (mix s alpha)
    BLEND_ADDITIVE,       ///< Aditivni (scitani barev)
    BLEND_MULTIPLY,       ///< Nasobeni
    BLEND_OVERLAY,        ///< Overlay efekt
    BLEND_MODE_COUNT
} blend_mode_t;

// ============================================================================
// KONFIGURACNI STRUKTURY
// ============================================================================

/**
 * @brief Konfigurace LED manageru
 */
typedef struct {
    uint8_t max_brightness;             ///< Maximalni jas (0-255)
    uint8_t default_brightness;         ///< Default brightness (0-255)
    bool enable_smooth_transitions;     ///< Povolit plynule prechody
    bool enable_layer_compositing;      ///< Povolit vrstvove slozeni
    uint8_t update_frequency_hz;        ///< Frekvence aktualizaci (Hz)
    uint32_t transition_duration_ms;    ///< Delka prechodu (ms)
} led_manager_config_t;

// ============================================================================
// LED STRUKTURY
// ============================================================================

/**
 * @brief RGB pixel s rozsir enymi atributy
 */
typedef struct {
    uint8_t r, g, b;      ///< RGB barva
    uint8_t alpha;        ///< Alpha kanal (0-255)
    uint8_t brightness;   ///< Jas pixelu (0-255)
    bool dirty;           ///< Pixel needs an update?
    uint32_t last_update; ///< Cas posledniho update (ms)
} led_pixel_t;

/**
 * @brief LED vrstva
 */
typedef struct {
    led_pixel_t pixels[73];  ///< 73 pixels (64 boxes + 9 buttons)
    blend_mode_t blend_mode; ///< Blending mode
    bool enabled;            ///< Je vrstva povolena? (legacy)
    bool layer_enabled;      ///< Je vrstva povolena?
    uint8_t master_alpha;    ///< Master alpha pro celou vrstvu (legacy)
    uint8_t layer_opacity;   ///< Opacity vrstvy (0-255)
    bool dirty;              ///< Is the layer dirty (needs update)?
    bool needs_composite;    ///< Does it need precomposition?
} led_layer_state_t;

/**
 * @brief Complete LED status (all layers)
 */
typedef struct {
    led_layer_state_t layers[LED_LAYER_COUNT]; ///< All layers
    led_pixel_t composite[64];                 ///< Finalni slozeny obraz
    bool dirty_pixels[64];                     ///< Dirty pixel mapa
    bool needs_update;                         ///< Does it need an update?
    uint32_t last_update_time;                 ///< Cas posledniho update
    SemaphoreHandle_t mutex;                   ///< Mutex pro thread-safety
} led_state_t;

// ============================================================================
// INITIALIZE AND BASE FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the LED state manager
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_init(void);

/**
 * @brief Initialize LED manager with configuration
 * 
 * @param config Manager configuration
 * @return ESP_OK on success
 */
esp_err_t led_manager_init(const led_manager_config_t* config);

/**
 * @brief Deinitialize the LED state manager
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_deinit(void);

/**
 * @brief Deinitialize the LED manager
 * 
 * @return ESP_OK on success
 */
esp_err_t led_manager_deinit(void);

/**
 * @brief Set the LED manager configuration
 * 
 * @param config The new configuration
 * @return ESP_OK on success
 */
esp_err_t led_set_config(const led_manager_config_t* config);

/**
 * @brief Set the update frequency
 * 
 * @param frequency_hz Frequency in Hz (1-60)
 * @return ESP_OK on success
 */
esp_err_t led_set_update_frequency(uint8_t frequency_hz);

/**
 * @brief Set the length of the transition
 * 
 * @param duration_ms Duration in ms
 * @return ESP_OK on success
 */
esp_err_t led_set_transition_duration(uint32_t duration_ms);

/**
 * @brief HSV to RGB conversion
 * 
 * @param h Hue (0.0-1.0)
 * @param s Saturation (0.0-1.0)
 * @param v Value (0.0-1.0)
 * @param r Output Red (0-255)
 * @param g Exit Green (0-255)
 * @param b Exit Blue (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_hsv_to_rgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b);

// ============================================================================
// FUNCTIONS FOR MANAGING LAYERS
// ============================================================================

/**
 * @brief Set the pixel in the layer
 * 
 * @param layer The layer
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @param alpha Alpha (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_state_set_pixel(led_layer_t layer, uint8_t row, uint8_t col,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t alpha);

/**
 * @brief Set pixel in the given layer according to LED index
 * 
 * @param layer The layer
 * @param led_index LED index (0-72)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_set_pixel_layer(led_layer_t layer, uint8_t led_index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Get the pixel from the layer
 * 
 * @param layer The layer
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param[out] pixel The output pixel
 * @return ESP_OK on success
 */
esp_err_t led_state_get_pixel(led_layer_t layer, uint8_t row, uint8_t col,
                               led_pixel_t* pixel);

/**
 * @brief Clear layer (set all pixels to banner)
 * 
 * @param layer The layer
 * @return ESP_OK on success
 */
esp_err_t led_state_clear_layer(led_layer_t layer);

/**
 * @brief Clear layer (alias for led_state_clear_layer)
 * 
 * @param layer The layer
 * @return ESP_OK on success
 */
esp_err_t led_clear_layer(led_layer_t layer);

/**
 * @brief Clear all layers
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_clear_all_layers(void);

/**
 * @brief Enable/disable layer
 * 
 * @param layer The layer
 * @param enabled Enable?
 * @return ESP_OK on success
 */
esp_err_t led_state_set_layer_enabled(led_layer_t layer, bool enabled);

/**
 * @brief Set the layer's blending mode
 * 
 * @param layer The layer
 * @param mode Blending mode
 * @return ESP_OK on success
 */
esp_err_t led_state_set_blend_mode(led_layer_t layer, blend_mode_t mode);

/**
 * @brief Set master layer alpha
 * 
 * @param layer The layer
 * @param alpha Master alpha (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_state_set_layer_alpha(led_layer_t layer, uint8_t alpha);

// ============================================================================
// KOMPOZITNI RENDEROVANI
// ============================================================================

/**
 * @brief Compose all layers into the final image
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_compose(void);

/**
 * @brief Aplikuj blending mezi dvema pixely
 * 
 * @param bottom Spodni pixel
 * @param top Vrchni pixel
 * @param mode Blending mode
 * @param[out] result Vysledny pixel
 */
void led_state_blend_pixel(const led_pixel_t* bottom, 
                           const led_pixel_t* top,
                           blend_mode_t mode, 
                           led_pixel_t* result);

// ============================================================================
// OPTIMALIZOVANE AKTUALIZACE
// ============================================================================

/**
 * @brief Only update dirty pixels to hardware
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_update_dirty(void);

/**
 * @brief Update the entire image to the hardware
 * 
 * @return ESP_OK on success
 */
esp_err_t led_state_update_all(void);

/**
 * @brief Force immediate full update of all LEDs
 * @return ESP_OK on success
 */
esp_err_t led_force_full_update(void);

/**
 * @brief Mark pixel as dirty
 * 
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 */
void led_state_mark_dirty(uint8_t row, uint8_t col);

/**
 * @brief Mark all pixels as dirty
 */
void led_state_mark_all_dirty(void);

/**
 * @brief Clear dirty flags
 */
void led_state_clear_dirty_flags(void);

// ============================================================================
// ADVANCED FUNCTIONS
// ============================================================================

/**
 * @brief Set the entire layer at once
 * 
 * @param layer The layer
 * @param pixels An array of 64 pixels
 * @return ESP_OK on success
 */
esp_err_t led_state_set_layer_bulk(led_layer_t layer, const led_pixel_t* pixels);

/**
 * @brief Fade layer in/out
 * 
 * @param layer The layer
 * @param target_alpha The target's alpha
 * @param duration_ms Length of fade in ms
 * @return ESP_OK on success
 */
esp_err_t led_state_fade_layer(led_layer_t layer, uint8_t target_alpha, 
                               uint32_t duration_ms);

/**
 * @brief Apply gamma correction to the layer
 * 
 * @param layer The layer
 * @param gamma Gamma value (1.0 = no correction, 2.2 = typical)
 * @return ESP_OK on success
 */
esp_err_t led_state_apply_gamma(led_layer_t layer, float gamma);

/**
 * @brief Apply brightness to the layer
 * 
 * @param layer The layer
 * @param brightness Brightness (0-255)
 * @return ESP_OK on success
 */
esp_err_t led_state_apply_brightness(led_layer_t layer, uint8_t brightness);

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Vypis stav LED manageru
 */
void led_state_print_status(void);

/**
 * @brief Vypis stav vrstvy
 * 
 * @param layer Vrstva
 */
void led_state_print_layer(led_layer_t layer);

/**
 * @brief Get global LED status (for read-only access)
 * 
 * @return A pointer to the global state
 */
const led_state_t* led_state_get_global(void);

#ifdef __cplusplus
}
#endif

#endif // LED_STATE_MANAGER_H
