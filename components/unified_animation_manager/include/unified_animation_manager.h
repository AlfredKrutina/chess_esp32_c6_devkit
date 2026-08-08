/**
 * @file unified_animation_manager.h
 * @brief Unified Animation Manager - Unified management of all LED animations
 * 
 * This module provides a simple interface for all LED animations:
 * - Centralized animation management
 * - Priority system for conflict animations
 * - Simple API to use quickly
 * - Support for multiple animations running simultaneously
 * - Smooth transitions between animations
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * Unified Animation Manager provides a centralized system for management
 * all LED animations in the system. It provides a priority system, support for
 * multiple simultaneous running animations and smooth transitions.
 * 
 * Advantages:
 * - Simple API: animation_start(ANIM_MOVE, from, to, duration)
 * - Automatic conflict resolution with priorities
 * - Support for stacking animation (e.g. check + move)
 * - Smooth fade-in/fade-out transitions
 * - Thread-safe approach
 */

#ifndef UNIFIED_ANIMATION_MANAGER_H
#define UNIFIED_ANIMATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PRIORITY ANIMACI
// ============================================================================

/**
 * @brief Priority levels pro animace
 * 
 * Higher number = higher priority; animations interrupt lower priorities.
 */
typedef enum {
    ANIM_PRIORITY_BACKGROUND = 0,    ///< Pozadi / screen saver (nejnizsi)
    ANIM_PRIORITY_LOW = 10,          ///< Low priority
    ANIM_PRIORITY_MEDIUM = 20,       ///< Medium priority
    ANIM_PRIORITY_HIGH = 30,         ///< High priority
    ANIM_PRIORITY_CRITICAL = 50,     ///< Critical animations (highest priority)
    // Aliases pro kompatibilitu
    ANIM_PRIORITY_AMBIENT = 10,      ///< Alias pro LOW
    ANIM_PRIORITY_GAME = 20,         ///< Alias pro MEDIUM
    ANIM_PRIORITY_INTERACTION = 30,  ///< Alias pro HIGH
    ANIM_PRIORITY_ALERT = 40         ///< Varovani / chyby
} animation_priority_t;

// ============================================================================
// TYPY ANIMACI
// ============================================================================

/**
 * @brief Typy animaci v unified systemu
 */
typedef enum {
    // Basic game animations (PRIORITY_GAME)
    ANIM_TYPE_MOVE_PATH = 0,     ///< Stroke path animation
    ANIM_TYPE_PIECE_GUIDANCE,    ///< Instructions for the figurine
    ANIM_TYPE_VALID_MOVES,       ///< Show valid moves
    ANIM_TYPE_ERROR_FLASH,       ///< Chybove bliknuti
    ANIM_TYPE_CAPTURE_EFFECT,    ///< Efekt sebrani
    ANIM_TYPE_CHECK_WARNING,     ///< Varovani sachu
    ANIM_TYPE_GAME_END,          ///< Konec hry
    ANIM_TYPE_PLAYER_CHANGE,     ///< Game change
    ANIM_TYPE_CASTLE,            ///< Castling
    ANIM_TYPE_PROMOTION,         ///< Graduation
    ANIM_TYPE_CONFIRMATION,      ///< Potvrzeni
    
    // Endgame animace
    ANIM_TYPE_ENDGAME_WAVE,      ///< Vlna vitezstvi
    ANIM_TYPE_ENDGAME_CIRCLES,   ///< Expandujici kruhy
    ANIM_TYPE_ENDGAME_CASCADE,   ///< Kaskadove padani
    ANIM_TYPE_ENDGAME_FIREWORKS, ///< Ohnostroj
    ANIM_TYPE_ENDGAME_DRAW_SPIRAL, ///< Spirala pro remi
    ANIM_TYPE_ENDGAME_DRAW_PULSE,  ///< Pulzovani pro remi
    
    ANIM_TYPE_COUNT          ///< Pocet typu animaci
} animation_type_t;

// ============================================================================
// KONFIGURACNI STRUKTURY
// ============================================================================

/**
 * @brief Konfigurace animation manageru
 */
typedef struct {
    uint8_t max_concurrent_animations;    ///< Maximalni pocet soucasne bezicich animaci
    uint8_t update_frequency_hz;          ///< Frekvence aktualizaci v Hz
    bool enable_smooth_interpolation;     ///< Povolit plynule interpolace
    bool enable_trail_effects;            ///< Povolit sledujici efekty
    uint32_t default_duration_ms;         ///< Default animation length in ms
} animation_config_t;

// ============================================================================
// STRUKTURA ANIMACE
// ============================================================================

// Forward declaration pro kruhovou referenci v update_func
typedef struct animation_state_struct animation_state_t;

/**
 * @brief Struktura animace
 */
struct animation_state_struct {
    uint32_t id;                       ///< Unikatni ID animace
    animation_type_t type;             ///< Typ animace
    animation_priority_t priority;     ///< Priority
    bool active;                       ///< Is it active?
    bool looping;                      ///< Opakuje se?
    uint32_t start_time;               ///< Cas spusteni
    uint32_t duration_ms;              ///< Delka v ms (0 = nekonecna)
    uint32_t current_frame;            ///< Current snapshot
    float progress;                    ///< Pokrok animace (0.0-1.0)
    
    // LED position
    uint8_t from_led;                  ///< Zdrojova LED
    uint8_t to_led;                    ///< Cilova LED
    uint8_t center_led;                ///< Stredova LED (pro endgame)
    uint8_t trail_length;              ///< Delka sledujiciho efektu
    
    // Positions and parameters (legacies)
    uint8_t from_row, from_col;        ///< Source position
    uint8_t to_row, to_col;            ///< Target position
    uint8_t affected_positions[64];    ///< Position affected
    uint8_t affected_count;            ///< Number of positions affected
    
    // Barvy (inline struktury)
    struct { uint8_t r, g, b; } color_start;     ///< Initial RGB color
    struct { uint8_t r, g, b; } color_end;       ///< Koncova barva RGB
    struct { uint8_t r, g, b; } color_primary;   ///< Primarni barva RGB (legacy)
    struct { uint8_t r, g, b; } color_secondary; ///< Sekundarni barva RGB (legacy)
    
    // Animation parameters
    uint8_t speed;                     ///< Rychlost (0-255)
    uint8_t intensity;                 ///< Intenzita (0-255)
    uint8_t winner_color;              ///< Barva viteze (0=white, 1=black)
    
    // Update function
    bool (*update_func)(animation_state_t* anim); ///< Update function
    
    // Callbacks
    void (*on_complete)(uint32_t id);  ///< Completion callback
    void (*on_frame)(uint32_t id, uint32_t frame); ///< Callback pro snimek
};
// typedef uz byl v forward declaration

// ============================================================================
// INITIALIZATION AND BASIC CONTROLS
// ============================================================================

/**
 * @brief Initialize the unified animation manager
 * 
 * @param config Manager configuration
 * @return ESP_OK on success
 */
esp_err_t animation_manager_init(const animation_config_t* config);

/**
 * @brief Deinitialize the unified animation manager
 * 
 * @return ESP_OK on success
 */
esp_err_t animation_manager_deinit(void);

/**
 * @brief Create new animation (allocate slot)
 * 
 * @param type The animation type
 * @param priority The priority of the animation
 * @return animation ID or 0 on error
 */
uint32_t unified_animation_create(animation_type_t type, animation_priority_t priority);

/**
 * @brief Stop the animation
 * 
 * @param anim_id the ID of the animation
 * @return ESP_OK on success
 */
esp_err_t unified_animation_stop(uint32_t anim_id);

/**
 * @brief Stop all animations
 * 
 * @return ESP_OK on success
 */
esp_err_t unified_animation_stop_all(void);

/**
 * @brief Start the animation
 * 
 * @param type The animation type
 * @param from_row Source rows (if relevant)
 * @param from_col Source column (if relevant)
 * @param to_row Target rows (if relevant)
 * @param to_col Target column (if relevant)
 * @param duration_ms Duration of the animation in ms (0 = default)
 * @return animation ID or 0 on failure
 */
uint32_t animation_start(animation_type_t type, 
                         uint8_t from_row, uint8_t from_col,
                         uint8_t to_row, uint8_t to_col,
                         uint32_t duration_ms);

/**
 * @brief Run a simple animation on a single field
 * 
 * @param type Typ animace
 * @param row Radek
 * @param col Sloupec
 * @param duration_ms Delka animace
 * @return ID animace
 */
uint32_t animation_start_simple(animation_type_t type, 
                                uint8_t row, uint8_t col,
                                uint32_t duration_ms);

/**
 * @brief Stop the animation
 * 
 * @param animation_id The ID of the animation to stop
 * @return ESP_OK on success
 */
esp_err_t animation_stop(uint32_t animation_id);

/**
 * @brief Stop all animations of the given type
 * 
 * @param type Type of animation to stop
 * @return ESP_OK on success
 */
esp_err_t animation_stop_all_of_type(animation_type_t type);

/**
 * @brief Stop all animations of lower or equal priority
 * 
 * @param max_priority Maximum priority to stop
 * @return ESP_OK on success
 */
esp_err_t animation_stop_all_up_to_priority(animation_priority_t max_priority);

/**
 * @brief Stop all animations
 * 
 * @return ESP_OK on success
 */
esp_err_t animation_stop_all(void);

// ============================================================================
// ADVANCED ANIMATION CONTROL
// ============================================================================

/**
 * @brief Set animation parameters
 * 
 * @param animation_id The ID of the animation
 * @param speed Speed (0-255)
 * @param intensity Intensity (0-255)
 * @param looping Should I repeat?
 * @return ESP_OK on success
 */
esp_err_t animation_set_params(uint32_t animation_id, 
                                uint8_t speed, uint8_t intensity, 
                                bool looping);

/**
 * @brief Set animation colors
 * 
 * @param animation_id The ID of the animation
 * @param r_primary Red primary (0-255)
 * @param g_primary Primary green (0-255)
 * @param b_primary Blue primary (0-255)
 * @param r_secondary Red secondary (0-255)
 * @param g_secondary Green secondary (0-255)
 * @param b_secondary Blue secondary (0-255)
 * @return ESP_OK on success
 */
esp_err_t animation_set_colors(uint32_t animation_id, 
                                uint8_t r_primary, uint8_t g_primary, uint8_t b_primary,
                                uint8_t r_secondary, uint8_t g_secondary, uint8_t b_secondary);

/**
 * @brief Fade out animation
 * 
 * @param animation_id The ID of the animation
 * @param fade_duration_ms Length of fade out in ms
 * @return ESP_OK on success
 */
esp_err_t animation_fade_out(uint32_t animation_id, uint32_t fade_duration_ms);

/**
 * @brief Set callback for completion
 * 
 * @param animation_id The ID of the animation
 * @param callback The callback function
 * @return ESP_OK on success
 */
esp_err_t animation_set_completion_callback(uint32_t animation_id, 
                                             void (*callback)(uint32_t));

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Verify if the animation is active
 * 
 * @param animation_id The ID of the animation
 * @return true if the animation is active
 */
bool animation_is_active(uint32_t animation_id);

/**
 * @brief Number of active animations
 * 
 * @return Number of running animations
 */
uint8_t animation_get_active_count(void);

/**
 * @brief Pocet animaci s danou prioritou
 * 
 * @param priority Priority
 * @return Pocet animaci
 */
uint8_t animation_get_count_by_priority(animation_priority_t priority);

/**
 * @brief Vypis status animaci
 */
void animation_print_status(void);

/**
 * @brief Update all active animations (call periodically)
 * 
 * @return ESP_OK on success
 */
esp_err_t animation_update_all(void);

/**
 * @brief Update animation manager (alias for animation_update_all)
 */
void animation_manager_update(void);

// ============================================================================
// BASIC ANIMATION FUNCTIONS
// ============================================================================

/**
 * @brief Start the move animation
 * 
 * @param anim_id the ID of the animation
 * @param from_led Source LED
 * @param to_led Target LED
 * @param duration_ms Duration of the animation
 * @return ESP_OK on success
 */
esp_err_t animation_start_move(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms);

/**
 * @brief Start the guidance animation
 * 
 * @param anim_id the ID of the animation
 * @param led_array The LED array to highlight
 * @param count Number of LEDs
 * @return ESP_OK on success
 */
esp_err_t animation_start_guidance(uint32_t anim_id, uint8_t* led_array, uint8_t count);

/**
 * @brief Start the error animation
 * 
 * @param anim_id the ID of the animation
 * @param led_index LED position
 * @param flash_count Number of flashes
 * @return ESP_OK on success
 */
esp_err_t animation_start_error(uint32_t anim_id, uint8_t led_index, uint32_t flash_count);

/**
 * @brief Start the promotion animation
 * 
 * @param anim_id the ID of the animation
 * @param promotion_led LED position of the promotion
 * @return ESP_OK on success
 */
esp_err_t animation_start_promotion(uint32_t anim_id, uint8_t promotion_led);

// ============================================================================
// ENDGAME ANIMATION FEATURE
// ============================================================================

/**
 * @brief Start the endgame wave animation
 * 
 * @param anim_id Animation ID (obtained from unified_animation_create)
 * @param center_led Center LED position (king)
 * @param winner_color Color of the knight (0=white, 1=black)
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_wave(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Start the endgame circles animation
 * 
 * @param anim_id the ID of the animation
 * @param center_led Center LED position
 * @param winner_color The color of the knight
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_circles(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Start the endgame cascade animation
 * 
 * @param anim_id the ID of the animation
 * @param center_led Center LED position
 * @param winner_color The color of the knight
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_cascade(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Start the endgame fireworks animation
 * 
 * @param anim_id the ID of the animation
 * @param center_led Center LED position
 * @param winner_color The color of the knight
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_fireworks(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Start endgame draw spiral animation (for draw)
 * 
 * @param anim_id the ID of the animation
 * @param center_led Center LED position
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_draw_spiral(uint32_t anim_id, uint8_t center_led);

/**
 * @brief Start endgame draw pulse animation (for draw)
 * 
 * @param anim_id the ID of the animation
 * @param center_led Center LED position
 * @return ESP_OK on success
 */
esp_err_t animation_start_endgame_draw_pulse(uint32_t anim_id, uint8_t center_led);

#ifdef __cplusplus
}
#endif

#endif // UNIFIED_ANIMATION_MANAGER_H
