/**
 * @file unified_animation_manager.c
 * @brief Implementace Unified Animation Manageru
 * 
 * Tento modul poskytuje jednotne rozhrani pro vsechny LED animace.
 * Umožnuje spravovat vice animaci soucasne a poskytuje pokrocile
 * funkce pro animace.
 * 
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 * 
 * @details
 * Unified Animation Manager je centralni system pro spravu vsech
 * LED animaci v systemu. Umožnuje spustit vice animaci soucasne,
 * spravuje jejich zivotni cyklus a poskytuje pokrocile efekty.
 */

#include "unified_animation_manager.h"
#include "led_task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"
#include "math.h"
#include "freertos_chess.h"
#include "led_mapping.h"

// Define min macro if not available
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char* TAG = "ANIM_MGR";

// Animation manager state
static bool manager_initialized = false;
static animation_config_t current_config;
static animation_state_t animations[16]; // Max 16 concurrent animations
static uint32_t next_animation_id = 1;
static uint32_t last_update_time = 0;

// Forward declarations
static bool animation_update_smooth_interpolation(animation_state_t* anim);
static void animation_update_trail_effect(animation_state_t* anim);
static bool animation_update_pulsing(animation_state_t* anim);
static bool animation_update_flashing(animation_state_t* anim);
static bool animation_update_rainbow(animation_state_t* anim);
static bool animation_update_endgame_wave(animation_state_t* anim);
static bool animation_update_endgame_circles(animation_state_t* anim);
static bool animation_update_endgame_cascade(animation_state_t* anim);
static bool animation_update_endgame_fireworks(animation_state_t* anim);
static bool animation_update_endgame_draw_spiral(animation_state_t* anim);
static bool animation_update_endgame_draw_pulse(animation_state_t* anim);
static bool animation_update_promotion(animation_state_t* anim);
static bool animation_is_valid_id(uint32_t anim_id);
static animation_state_t* animation_find_by_id(uint32_t anim_id);
static void animation_cleanup(animation_state_t* anim);

// Smooth interpolation functions
static float ease_in_out_cubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

static float ease_out_bounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;
    
    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

static void interpolate_color(uint8_t from_r, uint8_t from_g, uint8_t from_b,
                             uint8_t to_r, uint8_t to_g, uint8_t to_b,
                             float progress, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b) {
    float eased_progress = ease_in_out_cubic(progress);
    
    *out_r = (uint8_t)(from_r + (to_r - from_r) * eased_progress);
    *out_g = (uint8_t)(from_g + (to_g - from_g) * eased_progress);
    *out_b = (uint8_t)(from_b + (to_b - from_b) * eased_progress);
}

esp_err_t animation_manager_init(const animation_config_t* config) {
    if (manager_initialized) {
        ESP_LOGW(TAG, "Animation manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize configuration
    memcpy(&current_config, config, sizeof(animation_config_t));
    
    // Initialize animation states
    memset(animations, 0, sizeof(animations));
    for (int i = 0; i < 16; i++) {
        animations[i].id = 0; // Mark as inactive
        animations[i].active = false;
    }
    
    next_animation_id = 1;
    last_update_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    manager_initialized = true;
    
    ESP_LOGI(TAG, "Unified Animation Manager initialized");
    ESP_LOGI(TAG, "  Max concurrent animations: %d", current_config.max_concurrent_animations);
    ESP_LOGI(TAG, "  Update frequency: %d Hz", current_config.update_frequency_hz);
    ESP_LOGI(TAG, "  Smooth interpolation: %s", current_config.enable_smooth_interpolation ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Trail effects: %s", current_config.enable_trail_effects ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t animation_manager_deinit(void) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Stop all animations
    unified_animation_stop_all();
    
    manager_initialized = false;
    ESP_LOGI(TAG, "Animation manager deinitialized");
    
    return ESP_OK;
}

uint32_t unified_animation_create(animation_type_t type, animation_priority_t priority) {
    if (!manager_initialized) {
        ESP_LOGE(TAG, "Animation manager not initialized");
        return 0;
    }
    
    // ✅ OPRAVA: Zastavit všechny předchozí animace - pouze jedna současně
    int stopped_count = 0;
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active) {
            animation_cleanup(&animations[i]);
            stopped_count++;
        }
    }
    if (stopped_count > 0) {
        ESP_LOGI(TAG, "🛑 Stopped %d previous animations to start new one", stopped_count);
    }
    
    // Find free animation slot (should be available now)
    animation_state_t* anim = NULL;
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (!animations[i].active) {
            anim = &animations[i];
            break;
        }
    }
    
    if (!anim) {
        ESP_LOGE(TAG, "❌ Critical error: No animation slots available after cleanup");
        return 0;
    }
    
    // Initialize animation
    memset(anim, 0, sizeof(animation_state_t));
    anim->id = next_animation_id++;
    anim->type = type;
    anim->priority = priority;
    anim->active = true;
    anim->progress = 0.0f;
    anim->duration_ms = current_config.default_duration_ms;
    anim->start_time = esp_timer_get_time() / 1000;
    
    ESP_LOGD(TAG, "Created animation %d (type: %d, priority: %d)", 
             anim->id, type, priority);
    
    return anim->id;
}

esp_err_t animation_start_move(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = from_led;
    anim->to_led = to_led;
    anim->duration_ms = duration_ms;
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_smooth_interpolation;
    
    // Set colors for move animation
    anim->color_start.r = 0; anim->color_start.g = 255; anim->color_start.b = 0; // Green start
    anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 255; // Blue end
    
    if (current_config.enable_trail_effects) {
        anim->trail_length = 3; // 3 LED trail
    }
    
    ESP_LOGD(TAG, "Started move animation %d: LED %d -> %d (%dms)", 
             anim_id, from_led, to_led, duration_ms);
    
    return ESP_OK;
}

esp_err_t animation_start_guidance(uint32_t anim_id, uint8_t* led_array, uint8_t count) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim || !led_array || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = led_array[0]; // Use first LED as reference
    anim->to_led = led_array[0];
    anim->duration_ms = 2000; // 2 second pulsing
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_pulsing;
    
    // Gentle blue pulsing for guidance
    anim->color_start.r = 0; anim->color_start.g = 100; anim->color_start.b = 255;
    anim->color_end.r = 0; anim->color_end.g = 200; anim->color_end.b = 255;
    
    ESP_LOGD(TAG, "Started guidance animation %d for %d LEDs", anim_id, count);
    
    return ESP_OK;
}

esp_err_t animation_start_error(uint32_t anim_id, uint8_t led_index, uint32_t flash_count) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = led_index;
    anim->to_led = led_index;
    anim->duration_ms = flash_count * 200; // 200ms per flash
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_flashing;
    
    // Red flashing for errors
    anim->color_start.r = 255; anim->color_start.g = 0; anim->color_start.b = 0;
    anim->color_end.r = 255; anim->color_end.g = 0; anim->color_end.b = 0;
    
    ESP_LOGD(TAG, "Started error animation %d: LED %d (%d flashes)", 
             anim_id, led_index, flash_count);
    
    return ESP_OK;
}

esp_err_t animation_start_puzzle_removal(uint32_t anim_id, uint8_t* pieces_to_remove, uint8_t count) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim || !pieces_to_remove || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = pieces_to_remove[0];
    anim->to_led = pieces_to_remove[0];
    anim->duration_ms = 5000; // 5 second pulsing
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_pulsing;
    
    // Red pulsing for pieces to remove
    anim->color_start.r = 255; anim->color_start.g = 0; anim->color_start.b = 0;
    anim->color_end.r = 255; anim->color_end.g = 100; anim->color_end.b = 100;
    
    ESP_LOGD(TAG, "Started puzzle removal animation %d for %d pieces", anim_id, count);
    
    return ESP_OK;
}

esp_err_t animation_start_capture(uint32_t anim_id, uint8_t led_index, uint32_t duration_ms) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = led_index;
    anim->to_led = led_index;
    anim->duration_ms = duration_ms;
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_rainbow;
    
    // Rainbow effect for capture
    anim->color_start.r = 255; anim->color_start.g = 0; anim->color_start.b = 0;
    anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 255;
    
    ESP_LOGD(TAG, "Started capture animation %d: LED %d (%dms)", 
             anim_id, led_index, duration_ms);
    
    return ESP_OK;
}

esp_err_t animation_start_confirmation(uint32_t anim_id, uint8_t led_index) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = led_index;
    anim->to_led = led_index;
    anim->duration_ms = 1000; // 1 second confirmation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_pulsing;
    
    // Green pulsing for confirmation
    anim->color_start.r = 0; anim->color_start.g = 255; anim->color_start.b = 0;
    anim->color_end.r = 0; anim->color_end.g = 255; anim->color_end.b = 100;
    
    ESP_LOGD(TAG, "Started confirmation animation %d: LED %d", anim_id, led_index);
    
    return ESP_OK;
}

esp_err_t animation_start_endgame_draw_spiral(uint32_t anim_id, uint8_t center_led) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_draw_spiral;
    
    // Draw colors - neutral/balanced
    anim->color_start.r = 128; anim->color_start.g = 128; anim->color_start.b = 128; // Gray
    anim->color_end.r = 255; anim->color_end.g = 255; anim->color_end.b = 0; // Yellow
    
    return ESP_OK;
}

esp_err_t animation_start_endgame_draw_pulse(uint32_t anim_id, uint8_t center_led) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_draw_pulse;
    
    // Draw colors - neutral/balanced
    anim->color_start.r = 100; anim->color_start.g = 100; anim->color_start.b = 255; // Light blue
    anim->color_end.r = 255; anim->color_end.g = 100; anim->color_end.b = 100; // Light red
    
    return ESP_OK;
}

// ============================================================================
// ENDGAME ANIMATION FUNCTIONS
// ============================================================================

esp_err_t animation_start_endgame_wave(uint32_t anim_id, uint8_t center_led, uint8_t winner_color) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_wave;
    
    // Winner colors based on winner_color parameter
    if (winner_color == 0) { // White winner
        anim->color_start.r = 255; anim->color_start.g = 215; anim->color_start.b = 0; // Gold
        anim->color_end.r = 255; anim->color_end.g = 255; anim->color_end.b = 255; // White
    } else { // Black winner
        anim->color_start.r = 100; anim->color_start.g = 100; anim->color_start.b = 100; // Gray
        anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 0; // Black
    }
    
    ESP_LOGD(TAG, "Started endgame wave animation %d: center LED %d, winner %d", anim_id, center_led, winner_color);
    
    return ESP_OK;
}

esp_err_t animation_start_endgame_circles(uint32_t anim_id, uint8_t center_led, uint8_t winner_color) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_circles;
    
    // Winner colors
    if (winner_color == 0) { // White winner
        anim->color_start.r = 255; anim->color_start.g = 215; anim->color_start.b = 0; // Gold
        anim->color_end.r = 255; anim->color_end.g = 255; anim->color_end.b = 255; // White
    } else { // Black winner
        anim->color_start.r = 100; anim->color_start.g = 100; anim->color_start.b = 100; // Gray
        anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 0; // Black
    }
    
    ESP_LOGD(TAG, "Started endgame circles animation %d: center LED %d, winner %d", anim_id, center_led, winner_color);
    
    return ESP_OK;
}

esp_err_t animation_start_endgame_cascade(uint32_t anim_id, uint8_t center_led, uint8_t winner_color) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_cascade;
    
    // Winner colors
    if (winner_color == 0) { // White winner
        anim->color_start.r = 255; anim->color_start.g = 215; anim->color_start.b = 0; // Gold
        anim->color_end.r = 255; anim->color_end.g = 255; anim->color_end.b = 255; // White
    } else { // Black winner
        anim->color_start.r = 100; anim->color_start.g = 100; anim->color_start.b = 100; // Gray
        anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 0; // Black
    }
    
    ESP_LOGD(TAG, "Started endgame cascade animation %d: center LED %d, winner %d", anim_id, center_led, winner_color);
    
    return ESP_OK;
}

esp_err_t animation_start_endgame_fireworks(uint32_t anim_id, uint8_t center_led, uint8_t winner_color) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->from_led = center_led;
    anim->to_led = center_led;
    anim->duration_ms = 0; // Endless animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_endgame_fireworks;
    
    // Winner colors
    if (winner_color == 0) { // White winner
        anim->color_start.r = 255; anim->color_start.g = 215; anim->color_start.b = 0; // Gold
        anim->color_end.r = 255; anim->color_end.g = 255; anim->color_end.b = 255; // White
    } else { // Black winner
        anim->color_start.r = 100; anim->color_start.g = 100; anim->color_start.b = 100; // Gray
        anim->color_end.r = 0; anim->color_end.g = 0; anim->color_end.b = 0; // Black
    }
    
    ESP_LOGD(TAG, "Started endgame fireworks animation %d: center LED %d, winner %d", anim_id, center_led, winner_color);
    
    return ESP_OK;
}

esp_err_t unified_animation_stop(uint32_t anim_id) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    animation_cleanup(anim);
    ESP_LOGD(TAG, "Stopped animation %d", anim_id);
    
    return ESP_OK;
}

esp_err_t animation_stop_all_except_priority(animation_priority_t min_priority) {
    int stopped_count = 0;
    
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active && animations[i].priority > min_priority) {
            animation_cleanup(&animations[i]);
            stopped_count++;
        }
    }
    
    ESP_LOGD(TAG, "Stopped %d animations (priority > %d)", stopped_count, min_priority);
    return ESP_OK;
}

esp_err_t unified_animation_stop_all(void) {
    int stopped_count = 0;
    
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active) {
            animation_cleanup(&animations[i]);
            stopped_count++;
        }
    }
    
    ESP_LOGD(TAG, "Stopped all %d animations", stopped_count);
    return ESP_OK;
}

void animation_manager_update(void) {
    if (!manager_initialized) {
        return;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t delta_time = current_time - last_update_time;
    
    // Update frequency limiting
    if (delta_time < (1000 / current_config.update_frequency_hz)) {
        return;
    }
    
    last_update_time = current_time;
    
    // Update all active animations
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        animation_state_t* anim = &animations[i];
        if (!anim->active) {
            continue;
        }
        
        // Calculate progress
        uint32_t elapsed = current_time - anim->start_time;
        
        // Handle infinite animations (duration_ms = 0)
        if (anim->duration_ms == 0) {
            anim->progress = 0.0f; // Keep at 0 for infinite animations
        } else {
            anim->progress = (float)elapsed / (float)anim->duration_ms;
            
            // Check if animation is complete
            if (anim->progress >= 1.0f) {
                animation_cleanup(anim);
                continue;
            }
        }
        
        // Update animation
        if (anim->update_func) {
            anim->update_func(anim);
        }
    }
}

bool animation_is_running(uint32_t anim_id) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    return anim ? anim->active : false;
}

uint8_t animation_get_active_count(void) {
    uint8_t count = 0;
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active) {
            count++;
        }
    }
    return count;
}

uint8_t animation_get_active_count_by_priority(animation_priority_t priority) {
    uint8_t count = 0;
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active && animations[i].priority == priority) {
            count++;
        }
    }
    return count;
}

// Animation update functions
static bool animation_update_smooth_interpolation(animation_state_t* anim) {
    if (!current_config.enable_smooth_interpolation) {
        return true;
    }
    
    uint8_t r, g, b;
    interpolate_color(anim->color_start.r, anim->color_start.g, anim->color_start.b,
                     anim->color_end.r, anim->color_end.g, anim->color_end.b,
                     anim->progress, &r, &g, &b);
    
    // Apply to LED
    led_set_pixel_safe(anim->to_led, r, g, b);
    
    // Trail effect
    if (current_config.enable_trail_effects && anim->trail_length > 0) {
        float trail_progress = anim->progress * anim->trail_length;
        for (int i = 0; i < anim->trail_length; i++) {
            if (trail_progress > i) {
                float trail_intensity = 1.0f - (float)i / anim->trail_length;
                uint8_t trail_r = (uint8_t)(r * trail_intensity);
                uint8_t trail_g = (uint8_t)(g * trail_intensity);
                uint8_t trail_b = (uint8_t)(b * trail_intensity);
                
                // Calculate trail position (simplified)
                uint8_t trail_led = anim->from_led + (anim->to_led - anim->from_led) * i / anim->trail_length;
                led_set_pixel_safe(trail_led, trail_r, trail_g, trail_b);
            }
        }
    }
    return true;
}

/**
 * @brief Aktualizuje trail efekt pro animaci
 * 
 * Vytvari sledujici efekt za pohybujicim se objektem pomoci sinus vlny.
 * 
 * @param anim Ukazatel na stav animace
 */
static void animation_update_trail_effect(animation_state_t* anim) {
    // Implement trail effect for move animations
    float wave = sinf(anim->progress * 2.0f * M_PI);
    float intensity = (wave + 1.0f) / 2.0f; // 0.0 to 1.0
    
    uint8_t r = (uint8_t)(anim->color_start.r * intensity);
    uint8_t g = (uint8_t)(anim->color_start.g * intensity);
    uint8_t b = (uint8_t)(anim->color_start.b * intensity);
    
    led_set_pixel_safe(anim->to_led, r, g, b);
}

/**
 * @brief Aktualizuje pulzujici animaci
 * 
 * Vytvari plynuly pulzujici efekt pomoci sinus funkce.
 * Intenzita se meni mezi 30% a 100%.
 * 
 * @param anim Ukazatel na stav animace
 * @return true pokud animace pokracuje
 */
static bool animation_update_pulsing(animation_state_t* anim) {
    float pulse = (sinf(anim->progress * 4.0f * M_PI) + 1.0f) / 2.0f; // 0.0 to 1.0
    float intensity = 0.3f + 0.7f * pulse; // 0.3 to 1.0
    
    uint8_t r = (uint8_t)(anim->color_start.r * intensity);
    uint8_t g = (uint8_t)(anim->color_start.g * intensity);
    uint8_t b = (uint8_t)(anim->color_start.b * intensity);
    
    led_set_pixel_safe(anim->to_led, r, g, b);
    return true;
}

/**
 * @brief Aktualizuje blikajici animaci
 * 
 * Vytvari binarni blikajici efekt (zapnuto/vypnuto).
 * 
 * @param anim Ukazatel na stav animace
 * @return true pokud animace pokracuje
 */
static bool animation_update_flashing(animation_state_t* anim) {
    float flash = (sinf(anim->progress * 8.0f * M_PI) + 1.0f) / 2.0f; // 0.0 to 1.0
    float intensity = flash > 0.5f ? 1.0f : 0.0f; // Binary flashing
    
    uint8_t r = (uint8_t)(anim->color_start.r * intensity);
    uint8_t g = (uint8_t)(anim->color_start.g * intensity);
    uint8_t b = (uint8_t)(anim->color_start.b * intensity);
    
    led_set_pixel_safe(anim->to_led, r, g, b);
    return true;
}

/**
 * @brief Aktualizuje duhovou animaci
 * 
 * Provadi plynuly prechod pres vse

chny barvy duhy pomoci HSV->RGB konverze.
 * Obsahuje jemne pulzovani jasu pro dynamictejsi efekt.
 * 
 * @param anim Ukazatel na stav animace
 * @return true pokud animace pokracuje
 */
static bool animation_update_rainbow(animation_state_t* anim) {
    float hue = anim->progress * 360.0f; // 0 to 360 degrees
    float saturation = 1.0f;
    float value = 0.8f + 0.2f * sinf(anim->progress * 4.0f * M_PI); // Pulsing brightness
    
    // HSV to RGB conversion
    float c = value * saturation;
    float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
    float m = value - c;
    
    float r, g, b;
    if (hue < 60) { r = c; g = x; b = 0; }
    else if (hue < 120) { r = x; g = c; b = 0; }
    else if (hue < 180) { r = 0; g = c; b = x; }
    else if (hue < 240) { r = 0; g = x; b = c; }
    else if (hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    led_set_pixel_safe(anim->to_led, 
                      (uint8_t)((r + m) * 255),
                      (uint8_t)((g + m) * 255),
                      (uint8_t)((b + m) * 255));
    return true;
}

// ============================================================================
// ENDGAME ANIMATION UPDATE FUNCTIONS
// ============================================================================

static bool animation_update_endgame_wave(animation_state_t* anim) {
    // Non-blocking wave animation - only update one frame per call
    static uint32_t frame_counter = 0;
    frame_counter++;
    
    // Debug output every 30 frames (about 1 second at 30 FPS)
    if (frame_counter % 30 == 0) {
        ESP_LOGI("ANIM_WAVE", "🌊 Wave animation frame %lu", frame_counter);
    }
    
    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Calculate current radius (0-7)
    int radius = (frame_counter / 8) % 8;
    
    // Draw wave at current radius
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int dist_to_edge = min(min(row, 7-row), min(col, 7-col));
            
            if (dist_to_edge == radius) {
                uint8_t square = row * 8 + col;
                
                // Calculate brightness with pulsing effect
                float brightness = 0.5f + 0.5f * sinf(frame_counter * 0.1f + radius * 0.5f);
                float pulse = 0.7f + 0.3f * sinf(frame_counter * 0.2f + radius * 0.8f);
                
                uint8_t r = (uint8_t)(anim->color_start.r * brightness * pulse);
                uint8_t g = (uint8_t)(anim->color_start.g * brightness * pulse);
                uint8_t b = (uint8_t)(anim->color_start.b * brightness * pulse);
                
                led_set_pixel_safe(square, r, g, b);
            }
        }
    }
    
    return true; // Continue animation
}

static bool animation_update_endgame_circles(animation_state_t* anim) {
    // Non-blocking circles animation
    static uint32_t frame_counter = 0;
    frame_counter++;
    
    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Calculate current radius (0-7)
    int radius = (frame_counter / 10) % 8;
    
    uint8_t center_row = anim->from_led / 8;
    uint8_t center_col = anim->from_led % 8;
    
    // Draw expanding circles
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int dist = abs(row - center_row) + abs(col - center_col);
            
            if (dist == radius) {
                uint8_t square = row * 8 + col;
                
                // Color transition based on distance
                float color_progress = (float)radius / 7.0f;
                uint8_t r = anim->color_start.r - (uint8_t)((anim->color_start.r - anim->color_end.r) * color_progress);
                uint8_t g = anim->color_start.g - (uint8_t)((anim->color_start.g - anim->color_end.g) * color_progress);
                uint8_t b = anim->color_start.b - (uint8_t)((anim->color_start.b - anim->color_end.b) * color_progress);
                
                // Brightness with pulsing
                float brightness = 0.6f + 0.4f * sinf(frame_counter * 0.1f + radius * 0.4f);
                r = (uint8_t)(r * brightness);
                g = (uint8_t)(g * brightness);
                b = (uint8_t)(b * brightness);
                
                led_set_pixel_safe(square, r, g, b);
            }
        }
    }
    
    return true; // Continue animation
}

static bool animation_update_endgame_cascade(animation_state_t* anim) {
    // Non-blocking cascade animation
    static uint32_t frame_counter = 0;
    frame_counter++;
    
    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Draw falling lights
    for (int i = 0; i < 8; i++) {
        int row = (frame_counter * 2 + i * 3) % 8;
        int col = (frame_counter * 3 + i * 2) % 8;
        
        uint8_t square = row * 8 + col;
        
        // Random colors with winner theme
        uint8_t r = (i * 23 + frame_counter * 17) % 256;
        uint8_t g = (i * 37 + frame_counter * 29) % 256;
        uint8_t b = (i * 41 + frame_counter * 31) % 256;
        
        // Apply winner color theme
        if (anim->color_start.r > 200) { // White winner - bright colors
            r = (r + anim->color_start.r) / 2;
            g = (g + anim->color_start.g) / 2;
            b = (b + anim->color_start.b) / 2;
        } else { // Black winner - darker colors
            r = r / 3;
            g = g / 3;
            b = b / 3;
        }
        
        // Brightness pulsing
        float brightness = 0.4f + 0.6f * sinf(i * 0.5f + frame_counter * 0.1f);
        r = (uint8_t)(r * brightness);
        g = (uint8_t)(g * brightness);
        b = (uint8_t)(b * brightness);
        
        led_set_pixel_safe(square, r, g, b);
    }
    
    return true; // Continue animation
}

static bool animation_update_endgame_fireworks(animation_state_t* anim) {
    // Non-blocking fireworks animation
    static uint32_t frame_counter = 0;
    frame_counter++;
    
    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Create random bursts
    for (int i = 0; i < 6; i++) {
        int row = (frame_counter * 5 + i * 7) % 8;
        int col = (frame_counter * 3 + i * 11) % 8;
        
        uint8_t square = row * 8 + col;
        
        // Random colors with winner theme
        uint8_t r = (i * 19 + frame_counter * 13) % 256;
        uint8_t g = (i * 31 + frame_counter * 23) % 256;
        uint8_t b = (i * 43 + frame_counter * 37) % 256;
        
        // Apply winner color theme
        if (anim->color_start.r > 200) { // White winner - bright colors
            r = (r + anim->color_start.r) / 2;
            g = (g + anim->color_start.g) / 2;
            b = (b + anim->color_start.b) / 2;
        } else { // Black winner - darker colors
            r = r / 4;
            g = g / 4;
            b = b / 4;
        }
        
        // Brightness with random bursts
        float brightness = 0.3f + 0.7f * sinf(i * 1.2f + frame_counter * 0.15f);
        r = (uint8_t)(r * brightness);
        g = (uint8_t)(g * brightness);
        b = (uint8_t)(b * brightness);
        
        led_set_pixel_safe(square, r, g, b);
    }
    
    return true; // Continue animation
}

static bool animation_update_endgame_draw_spiral(animation_state_t* anim) {
    // Non-blocking spiral animation for draw
    static uint32_t frame_counter = 0;
    frame_counter++;

    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }

    // Create spiral pattern from center outward
    int center_row = 4, center_col = 4;
    int max_radius = 4;
    
    for (int radius = 0; radius <= max_radius; radius++) {
        float angle_offset = frame_counter * 0.1f + radius * 0.5f;
        
        for (int i = 0; i < 8; i++) {
            float angle = (i * 3.14159f / 4.0f) + angle_offset;
            int row = center_row + (int)(radius * cosf(angle));
            int col = center_col + (int)(radius * sinf(angle));
            
            if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                uint8_t square = row * 8 + col;
                
                // Spiral colors - alternating between gray and yellow
                float brightness = 0.5f + 0.5f * sinf(frame_counter * 0.08f + radius * 0.8f);
                
                uint8_t r = (uint8_t)(anim->color_start.r * brightness);
                uint8_t g = (uint8_t)(anim->color_start.g * brightness);
                uint8_t b = (uint8_t)(anim->color_start.b * brightness);
                
                // Add yellow accent
                if ((frame_counter + radius * 3) % 16 < 8) {
                    r = (uint8_t)(anim->color_end.r * brightness);
                    g = (uint8_t)(anim->color_end.g * brightness);
                    b = (uint8_t)(anim->color_end.b * brightness);
                }
                
                led_set_pixel_safe(square, r, g, b);
            }
        }
    }
    
    return true; // Continue animation
}

static bool animation_update_endgame_draw_pulse(animation_state_t* anim) {
    // Non-blocking pulse animation for draw
    static uint32_t frame_counter = 0;
    frame_counter++;

    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }

    // Create pulsing pattern - all squares pulse in sync
    float pulse = 0.3f + 0.7f * sinf(frame_counter * 0.12f);
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint8_t square = row * 8 + col;
            
            // Alternating colors across the board
            bool use_color1 = ((row + col + frame_counter / 20) % 2) == 0;
            
            uint8_t r, g, b;
            if (use_color1) {
                r = (uint8_t)(anim->color_start.r * pulse);
                g = (uint8_t)(anim->color_start.g * pulse);
                b = (uint8_t)(anim->color_start.b * pulse);
            } else {
                r = (uint8_t)(anim->color_end.r * pulse);
                g = (uint8_t)(anim->color_end.g * pulse);
                b = (uint8_t)(anim->color_end.b * pulse);
            }
            
            led_set_pixel_safe(square, r, g, b);
        }
    }
    
    return true; // Continue animation
}

// Utility functions
static bool animation_is_valid_id(uint32_t anim_id) {
    return anim_id > 0 && anim_id < next_animation_id;
}

static animation_state_t* animation_find_by_id(uint32_t anim_id) {
    if (!animation_is_valid_id(anim_id)) {
        return NULL;
    }
    
    for (int i = 0; i < current_config.max_concurrent_animations; i++) {
        if (animations[i].active && animations[i].id == anim_id) {
            return &animations[i];
        }
    }
    
    return NULL;
}

static void animation_cleanup(animation_state_t* anim) {
    if (!anim) return;
    
    // Clear LED
    led_set_pixel_safe(anim->to_led, 0, 0, 0);
    
    // Reset animation state
    anim->active = false;
    anim->id = 0;
    anim->progress = 0.0f;
    anim->update_func = NULL;
}

// Configuration functions
esp_err_t animation_set_config(const animation_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&current_config, config, sizeof(animation_config_t));
    ESP_LOGI(TAG, "Animation configuration updated");
    
    return ESP_OK;
}

esp_err_t animation_set_default_duration(uint32_t duration_ms) {
    current_config.default_duration_ms = duration_ms;
    ESP_LOGI(TAG, "Default animation duration set to %dms", duration_ms);
    return ESP_OK;
}

esp_err_t animation_set_update_frequency(uint8_t frequency_hz) {
    if (frequency_hz < 1 || frequency_hz > 60) {
        return ESP_ERR_INVALID_ARG;
    }
    
    current_config.update_frequency_hz = frequency_hz;
    ESP_LOGI(TAG, "Animation update frequency set to %d Hz", frequency_hz);
    return ESP_OK;
}

const char* animation_get_type_name(animation_type_t type) {
    switch (type) {
        case ANIM_TYPE_MOVE_PATH: return "Move Path";
        case ANIM_TYPE_PIECE_GUIDANCE: return "Piece Guidance";
        case ANIM_TYPE_VALID_MOVES: return "Valid Moves";
        case ANIM_TYPE_ERROR_FLASH: return "Error Flash";
        case ANIM_TYPE_PUZZLE_SETUP: return "Puzzle Setup";
        case ANIM_TYPE_CAPTURE_EFFECT: return "Capture Effect";
        case ANIM_TYPE_CHECK_WARNING: return "Check Warning";
        case ANIM_TYPE_GAME_END: return "Game End";
        case ANIM_TYPE_PLAYER_CHANGE: return "Player Change";
        case ANIM_TYPE_CASTLE: return "Castle";
        case ANIM_TYPE_PROMOTION: return "Promotion";
        case ANIM_TYPE_PUZZLE_PATH: return "Puzzle Path";
        case ANIM_TYPE_CONFIRMATION: return "Confirmation";
        default: return "Unknown";
    }
}

const char* animation_get_priority_name(animation_priority_t priority) {
    switch (priority) {
        case ANIM_PRIORITY_CRITICAL: return "Critical";
        case ANIM_PRIORITY_HIGH: return "High";
        case ANIM_PRIORITY_MEDIUM: return "Medium";
        case ANIM_PRIORITY_LOW: return "Low";
        case ANIM_PRIORITY_BACKGROUND: return "Background";
        default: return "Unknown";
    }
}

esp_err_t animation_get_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int written = snprintf(buffer, buffer_size, 
        "Animation Manager Status:\n"
        "  Initialized: %s\n"
        "  Active animations: %d/%d\n"
        "  Update frequency: %d Hz\n"
        "  Smooth interpolation: %s\n"
        "  Trail effects: %s\n",
        manager_initialized ? "Yes" : "No",
        animation_get_active_count(),
        current_config.max_concurrent_animations,
        current_config.update_frequency_hz,
        current_config.enable_smooth_interpolation ? "Yes" : "No",
        current_config.enable_trail_effects ? "Yes" : "No");
    
    if (written >= buffer_size) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

// ============================================================================
// PROMOTION ANIMATION FUNCTIONS
// ============================================================================

esp_err_t animation_start_promotion(uint32_t anim_id, uint8_t promotion_led) {
    animation_state_t* anim = animation_find_by_id(anim_id);
    if (!anim) {
        return ESP_ERR_INVALID_ARG;
    }
    
    anim->active = true;
    anim->duration_ms = 3000; // 3 second promotion animation
    anim->start_time = esp_timer_get_time() / 1000;
    anim->progress = 0.0f;
    anim->update_func = animation_update_promotion;
    
    // Promotion colors - gold transformation
    anim->color_start.r = 255; anim->color_start.g = 255; anim->color_start.b = 255; // White (pawn)
    anim->color_end.r = 255; anim->color_end.g = 215; anim->color_end.b = 0; // Gold (promoted piece)
    
    // Store promotion LED position
    anim->from_led = promotion_led;
    anim->to_led = promotion_led;
    
    return ESP_OK;
}

static bool animation_update_promotion(animation_state_t* anim) {
    // Non-blocking promotion animation
    static uint32_t frame_counter = 0;
    frame_counter++;
    
    uint8_t promotion_led = anim->from_led;
    
    // Clear board
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
    
    // Multi-stage promotion animation
    uint32_t stage_duration = anim->duration_ms / 4; // 4 stages
    uint32_t elapsed = (esp_timer_get_time() / 1000) - anim->start_time;
    uint32_t stage = elapsed / stage_duration;
    
    if (stage >= 4) {
        // Animation finished
        led_clear_board_only();
        return false; // Stop animation
    }
    
    switch (stage) {
        case 0: {
            // Stage 1: Highlight pawn (white)
            led_set_pixel_safe(promotion_led, 255, 255, 255);
            break;
        }
        case 1: {
            // Stage 2: Transformation effect (pulsing)
            float brightness = 0.5f + 0.5f * sinf(frame_counter * 0.2f);
            uint8_t color = (uint8_t)(255 * brightness);
            led_set_pixel_safe(promotion_led, color, color, color);
            break;
        }
        case 2: {
            // Stage 3: Show promoted piece (gold)
            led_set_pixel_safe(promotion_led, 255, 215, 0);
            break;
        }
        case 3: {
            // Stage 4: Celebration effect (rainbow burst)
            uint32_t burst = (frame_counter / 4) % 8;
            
            // Rainbow color progression
            uint8_t r, g, b;
            if (burst < 2) {
                r = 255; g = 0; b = 0; // Red
            } else if (burst < 4) {
                r = 255; g = 165; b = 0; // Orange
            } else if (burst < 6) {
                r = 255; g = 255; b = 0; // Yellow
            } else {
                r = 0; g = 255; b = 0; // Green
            }
            
            // Add surrounding glow effect
            for (int offset = -1; offset <= 1; offset++) {
                for (int offset2 = -1; offset2 <= 1; offset2++) {
                    if (offset == 0 && offset2 == 0) continue;
                    int glow_row = (promotion_led / 8) + offset;
                    int glow_col = (promotion_led % 8) + offset2;
                    if (glow_row >= 0 && glow_row < 8 && glow_col >= 0 && glow_col < 8) {
                        uint8_t glow_led = chess_pos_to_led_index(glow_row, glow_col);
                        led_set_pixel_safe(glow_led, (uint8_t)(r * 0.3f), (uint8_t)(g * 0.3f), (uint8_t)(b * 0.3f));
                    }
                }
            }
            
            led_set_pixel_safe(promotion_led, r, g, b);
            break;
        }
    }
    
    return true; // Continue animation
}
