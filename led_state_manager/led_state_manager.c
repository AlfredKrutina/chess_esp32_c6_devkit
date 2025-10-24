/**
 * @file led_state_manager.c
 * @brief LED State Manager Implementation
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#include "led_state_manager.h"
#include "led_task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "string.h"
#include "math.h"

static const char* TAG = "LED_MGR";

// LED manager state
static bool manager_initialized = false;
static led_manager_config_t current_config;
static led_layer_state_t layers[LED_LAYER_COUNT];
static uint8_t global_brightness = 255;
static uint32_t last_update_time = 0;
static uint8_t dirty_count = 0;

// Forward declarations
static void led_composite_pixel(uint8_t led_index);
static void led_apply_brightness(uint8_t led_index, uint8_t* r, uint8_t* g, uint8_t* b);
static void led_mark_dirty(uint8_t led_index);
static void led_clear_dirty(uint8_t led_index);
static bool led_is_layer_enabled(led_layer_t layer);
static uint8_t led_get_layer_brightness(led_layer_t layer);

esp_err_t led_manager_init(const led_manager_config_t* config) {
    if (manager_initialized) {
        ESP_LOGW(TAG, "LED manager already initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!config) {
        ESP_LOGE(TAG, "Invalid configuration provided");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Initialize configuration
    memcpy(&current_config, config, sizeof(led_manager_config_t));
    
    // Initialize all layers
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        memset(&layers[layer], 0, sizeof(led_layer_state_t));
        layers[layer].layer_enabled = true;
        layers[layer].layer_opacity = 255;
        layers[layer].needs_composite = false;
        
        // Initialize all pixels in this layer
        for (int i = 0; i < 73; i++) {
            layers[layer].pixels[i].r = 0;
            layers[layer].pixels[i].g = 0;
            layers[layer].pixels[i].b = 0;
            layers[layer].pixels[i].dirty = false;
            layers[layer].pixels[i].last_update = 0;
            layers[layer].pixels[i].brightness = current_config.default_brightness;
        }
    }
    
    global_brightness = current_config.default_brightness;
    last_update_time = esp_timer_get_time() / 1000;
    dirty_count = 0;
    manager_initialized = true;
    
    ESP_LOGI(TAG, "LED State Manager initialized");
    ESP_LOGI(TAG, "  Max brightness: %d", current_config.max_brightness);
    ESP_LOGI(TAG, "  Default brightness: %d", current_config.default_brightness);
    ESP_LOGI(TAG, "  Smooth transitions: %s", current_config.enable_smooth_transitions ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Layer compositing: %s", current_config.enable_layer_compositing ? "enabled" : "disabled");
    ESP_LOGI(TAG, "  Update frequency: %d Hz", current_config.update_frequency_hz);
    
    return ESP_OK;
}

esp_err_t led_manager_deinit(void) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Clear all layers
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        led_clear_layer((led_layer_t)layer);
    }
    
    // Force final update
    led_force_full_update();
    
    manager_initialized = false;
    ESP_LOGI(TAG, "LED manager deinitialized");
    
    return ESP_OK;
}

esp_err_t led_set_pixel_layer(led_layer_t layer, uint8_t led_index, 
                              uint8_t r, uint8_t g, uint8_t b) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (layer >= LED_LAYER_COUNT || led_index >= 73) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!led_is_layer_enabled(layer)) {
        return ESP_OK; // Layer disabled, ignore
    }
    
    led_pixel_t* pixel = &layers[layer].pixels[led_index];
    
    // Check if pixel actually changed
    if (pixel->r == r && pixel->g == g && pixel->b == b) {
        return ESP_OK; // No change needed
    }
    
    // Update pixel
    pixel->r = r;
    pixel->g = g;
    pixel->b = b;
    pixel->last_update = esp_timer_get_time() / 1000;
    pixel->dirty = true;
    
    // Mark layer as needing composite
    layers[layer].needs_composite = true;
    
    // Mark as dirty for update
    led_mark_dirty(led_index);
    
    ESP_LOGD(TAG, "Set pixel %d on layer %d: RGB(%d,%d,%d)", 
             led_index, layer, r, g, b);
    
    return ESP_OK;
}

esp_err_t led_clear_layer(led_layer_t layer) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (layer >= LED_LAYER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clear all pixels in layer
    for (int i = 0; i < 73; i++) {
        led_pixel_t* pixel = &layers[layer].pixels[i];
        if (pixel->r != 0 || pixel->g != 0 || pixel->b != 0) {
            pixel->r = 0;
            pixel->g = 0;
            pixel->b = 0;
            pixel->dirty = true;
            led_mark_dirty(i);
        }
    }
    
    layers[layer].needs_composite = true;
    ESP_LOGD(TAG, "Cleared layer %d", layer);
    
    return ESP_OK;
}

esp_err_t led_set_layer_opacity(led_layer_t layer, uint8_t opacity) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (layer >= LED_LAYER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    layers[layer].layer_opacity = opacity;
    layers[layer].needs_composite = true;
    
    // Mark all pixels in this layer as dirty
    for (int i = 0; i < 73; i++) {
        led_mark_dirty(i);
    }
    
    ESP_LOGD(TAG, "Set layer %d opacity to %d", layer, opacity);
    
    return ESP_OK;
}

esp_err_t led_enable_layer(led_layer_t layer, bool enable) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (layer >= LED_LAYER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    layers[layer].layer_enabled = enable;
    layers[layer].needs_composite = true;
    
    // Mark all pixels as dirty when enabling/disabling layer
    for (int i = 0; i < 73; i++) {
        led_mark_dirty(i);
    }
    
    ESP_LOGD(TAG, "Layer %d %s", layer, enable ? "enabled" : "disabled");
    
    return ESP_OK;
}

esp_err_t led_set_layer_brightness(led_layer_t layer, uint8_t brightness) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (layer >= LED_LAYER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update brightness for all pixels in this layer
    for (int i = 0; i < 73; i++) {
        layers[layer].pixels[i].brightness = brightness;
        layers[layer].pixels[i].dirty = true;
        led_mark_dirty(i);
    }
    
    layers[layer].needs_composite = true;
    ESP_LOGD(TAG, "Set layer %d brightness to %d", layer, brightness);
    
    return ESP_OK;
}

esp_err_t led_composite_and_update(void) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Check if we need to update
    if (dirty_count == 0) {
        return ESP_OK; // Nothing to update
    }
    
    // Update frequency limiting
    if (current_time - last_update_time < (1000 / current_config.update_frequency_hz)) {
        return ESP_OK;
    }
    
    // Composite all layers for each dirty pixel
    for (int i = 0; i < 73; i++) {
        if (layers[0].pixels[i].dirty || // Background layer
            layers[1].pixels[i].dirty || // Pieces layer
            layers[2].pixels[i].dirty || // Guidance layer
            layers[3].pixels[i].dirty || // Animation layer
            layers[4].pixels[i].dirty) { // Error layer
            
            led_composite_pixel(i);
        }
    }
    
    last_update_time = current_time;
    dirty_count = 0;
    
    return ESP_OK;
}

esp_err_t led_force_full_update(void) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Mark all pixels as dirty
    for (int i = 0; i < 73; i++) {
        led_mark_dirty(i);
    }
    
    // Force composite and update
    return led_composite_and_update();
}

uint8_t led_get_dirty_count(void) {
    return dirty_count;
}

uint8_t led_get_dirty_count_by_layer(led_layer_t layer) {
    if (layer >= LED_LAYER_COUNT) {
        return 0;
    }
    
    uint8_t count = 0;
    for (int i = 0; i < 73; i++) {
        if (layers[layer].pixels[i].dirty) {
            count++;
        }
    }
    
    return count;
}

esp_err_t led_fade_pixel(uint8_t led_index, uint8_t target_r, uint8_t target_g, 
                        uint8_t target_b, uint32_t duration_ms) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_index >= 73) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // This is a simplified fade - in a full implementation, you'd use a timer
    // For now, just set the pixel directly
    return led_set_pixel_layer(LED_LAYER_ANIMATION, led_index, target_r, target_g, target_b);
}

esp_err_t led_pulse_pixel(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b,
                         uint32_t period_ms, uint8_t pulse_count) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_index >= 73) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simplified pulsing - in full implementation, use timer
    uint32_t current_time = esp_timer_get_time() / 1000;
    float pulse = (sinf((current_time % period_ms) * 2.0f * M_PI / period_ms) + 1.0f) / 2.0f;
    
    uint8_t pulse_r = (uint8_t)(r * pulse);
    uint8_t pulse_g = (uint8_t)(g * pulse);
    uint8_t pulse_b = (uint8_t)(b * pulse);
    
    return led_set_pixel_layer(LED_LAYER_ANIMATION, led_index, pulse_r, pulse_g, pulse_b);
}

esp_err_t led_rainbow_pixel(uint8_t led_index, uint32_t duration_ms) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_index >= 73) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Calculate rainbow color based on time
    uint32_t current_time = esp_timer_get_time() / 1000;
    float hue = (current_time % duration_ms) * 360.0f / duration_ms;
    
    uint8_t r, g, b;
    led_hsv_to_rgb(hue, 1.0f, 1.0f, &r, &g, &b);
    
    return led_set_pixel_layer(LED_LAYER_ANIMATION, led_index, r, g, b);
}

esp_err_t led_set_multiple_pixels(led_layer_t layer, uint8_t* led_indices, 
                                 uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!led_indices || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = ESP_OK;
    for (uint8_t i = 0; i < count; i++) {
        esp_err_t pixel_result = led_set_pixel_layer(layer, led_indices[i], r, g, b);
        if (pixel_result != ESP_OK) {
            result = pixel_result;
        }
    }
    
    return result;
}

esp_err_t led_clear_multiple_pixels(led_layer_t layer, uint8_t* led_indices, uint8_t count) {
    return led_set_multiple_pixels(layer, led_indices, count, 0, 0, 0);
}

esp_err_t led_fade_multiple_pixels(uint8_t* led_indices, uint8_t count,
                                  uint8_t target_r, uint8_t target_g, uint8_t target_b,
                                  uint32_t duration_ms) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!led_indices || count == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_err_t result = ESP_OK;
    for (uint8_t i = 0; i < count; i++) {
        esp_err_t pixel_result = led_fade_pixel(led_indices[i], target_r, target_g, target_b, duration_ms);
        if (pixel_result != ESP_OK) {
            result = pixel_result;
        }
    }
    
    return result;
}

esp_err_t led_set_global_brightness(uint8_t brightness) {
    if (brightness > current_config.max_brightness) {
        brightness = current_config.max_brightness;
    }
    
    global_brightness = brightness;
    
    // Mark all pixels as dirty to apply new brightness
    for (int i = 0; i < 73; i++) {
        led_mark_dirty(i);
    }
    
    ESP_LOGD(TAG, "Global brightness set to %d", brightness);
    
    return ESP_OK;
}

esp_err_t led_set_pixel_brightness(uint8_t led_index, uint8_t brightness) {
    if (!manager_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (led_index >= 73) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Update brightness for all layers
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        layers[layer].pixels[led_index].brightness = brightness;
        layers[layer].pixels[led_index].dirty = true;
    }
    
    led_mark_dirty(led_index);
    
    return ESP_OK;
}

uint8_t led_get_global_brightness(void) {
    return global_brightness;
}

uint8_t led_get_pixel_brightness(uint8_t led_index) {
    if (led_index >= 73) {
        return 0;
    }
    
    // Return brightness from first enabled layer
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        if (led_is_layer_enabled(layer)) {
            return layers[layer].pixels[led_index].brightness;
        }
    }
    
    return 0;
}

// Color utility functions
esp_err_t led_hsv_to_rgb(float h, float s, float v, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Normalize inputs
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    s = fmaxf(0.0f, fminf(1.0f, s));
    v = fmaxf(0.0f, fminf(1.0f, v));
    
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    float r_f, g_f, b_f;
    if (h < 60) { r_f = c; g_f = x; b_f = 0; }
    else if (h < 120) { r_f = x; g_f = c; b_f = 0; }
    else if (h < 180) { r_f = 0; g_f = c; b_f = x; }
    else if (h < 240) { r_f = 0; g_f = x; b_f = c; }
    else if (h < 300) { r_f = x; g_f = 0; b_f = c; }
    else { r_f = c; g_f = 0; b_f = x; }
    
    *r = (uint8_t)((r_f + m) * 255);
    *g = (uint8_t)((g_f + m) * 255);
    *b = (uint8_t)((b_f + m) * 255);
    
    return ESP_OK;
}

esp_err_t led_rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float* h, float* s, float* v) {
    if (!h || !s || !v) {
        return ESP_ERR_INVALID_ARG;
    }
    
    float r_f = r / 255.0f;
    float g_f = g / 255.0f;
    float b_f = b / 255.0f;
    
    float max_val = fmaxf(r_f, fmaxf(g_f, b_f));
    float min_val = fminf(r_f, fminf(g_f, b_f));
    float delta = max_val - min_val;
    
    *v = max_val;
    
    if (max_val == 0) {
        *s = 0;
        *h = 0;
    } else {
        *s = delta / max_val;
        
        if (delta == 0) {
            *h = 0;
        } else if (max_val == r_f) {
            *h = 60.0f * fmodf((g_f - b_f) / delta, 6.0f);
        } else if (max_val == g_f) {
            *h = 60.0f * ((b_f - r_f) / delta + 2.0f);
        } else {
            *h = 60.0f * ((r_f - g_f) / delta + 4.0f);
        }
        
        if (*h < 0) *h += 360.0f;
    }
    
    return ESP_OK;
}

esp_err_t led_interpolate_color(uint8_t r1, uint8_t g1, uint8_t b1,
                               uint8_t r2, uint8_t g2, uint8_t b2,
                               float progress, uint8_t* out_r, uint8_t* out_g, uint8_t* out_b) {
    if (!out_r || !out_g || !out_b) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Clamp progress to 0.0-1.0
    progress = fmaxf(0.0f, fminf(1.0f, progress));
    
    *out_r = (uint8_t)(r1 + (r2 - r1) * progress);
    *out_g = (uint8_t)(g1 + (g2 - g1) * progress);
    *out_b = (uint8_t)(b1 + (b2 - b1) * progress);
    
    return ESP_OK;
}

// Status and debugging functions
esp_err_t led_get_status(char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    int written = snprintf(buffer, buffer_size,
        "LED State Manager Status:\n"
        "  Initialized: %s\n"
        "  Global brightness: %d\n"
        "  Dirty pixels: %d\n"
        "  Update frequency: %d Hz\n"
        "  Smooth transitions: %s\n"
        "  Layer compositing: %s\n",
        manager_initialized ? "Yes" : "No",
        global_brightness,
        dirty_count,
        current_config.update_frequency_hz,
        current_config.enable_smooth_transitions ? "Yes" : "No",
        current_config.enable_layer_compositing ? "Yes" : "No");
    
    if (written >= buffer_size) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t led_get_layer_status(led_layer_t layer, char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (layer >= LED_LAYER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* layer_names[] = {"Background", "Pieces", "Guidance", "Animation", "Error"};
    
    int written = snprintf(buffer, buffer_size,
        "Layer %d (%s) Status:\n"
        "  Enabled: %s\n"
        "  Opacity: %d\n"
        "  Needs composite: %s\n"
        "  Dirty pixels: %d\n",
        layer, layer_names[layer],
        led_is_layer_enabled(layer) ? "Yes" : "No",
        layers[layer].layer_opacity,
        layers[layer].needs_composite ? "Yes" : "No",
        led_get_dirty_count_by_layer(layer));
    
    if (written >= buffer_size) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

bool led_is_pixel_dirty(uint8_t led_index) {
    if (led_index >= 73) {
        return false;
    }
    
    // Check if any layer has this pixel dirty
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        if (layers[layer].pixels[led_index].dirty) {
            return true;
        }
    }
    
    return false;
}

uint32_t led_get_last_update_time(uint8_t led_index) {
    if (led_index >= 73) {
        return 0;
    }
    
    uint32_t latest_time = 0;
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        if (layers[layer].pixels[led_index].last_update > latest_time) {
            latest_time = layers[layer].pixels[led_index].last_update;
        }
    }
    
    return latest_time;
}

// Configuration functions
esp_err_t led_set_config(const led_manager_config_t* config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(&current_config, config, sizeof(led_manager_config_t));
    ESP_LOGI(TAG, "LED configuration updated");
    
    return ESP_OK;
}

esp_err_t led_set_update_frequency(uint8_t frequency_hz) {
    if (frequency_hz < 1 || frequency_hz > 60) {
        return ESP_ERR_INVALID_ARG;
    }
    
    current_config.update_frequency_hz = frequency_hz;
    ESP_LOGI(TAG, "Update frequency set to %d Hz", frequency_hz);
    
    return ESP_OK;
}

esp_err_t led_set_transition_duration(uint32_t duration_ms) {
    current_config.transition_duration_ms = duration_ms;
    ESP_LOGI(TAG, "Transition duration set to %dms", duration_ms);
    
    return ESP_OK;
}

// Internal helper functions
static void led_composite_pixel(uint8_t led_index) {
    if (led_index >= 73) {
        return;
    }
    
    uint8_t final_r = 0, final_g = 0, final_b = 0;
    bool has_color = false;
    
    // Composite layers from background to error (highest priority)
    for (int layer = LED_LAYER_BACKGROUND; layer < LED_LAYER_COUNT; layer++) {
        if (!led_is_layer_enabled(layer)) {
            continue;
        }
        
        led_pixel_t* pixel = &layers[layer].pixels[led_index];
        
        // Skip transparent pixels
        if (pixel->r == 0 && pixel->g == 0 && pixel->b == 0) {
            continue;
        }
        
        // Apply layer opacity
        uint8_t layer_r = (pixel->r * layers[layer].layer_opacity) / 255;
        uint8_t layer_g = (pixel->g * layers[layer].layer_opacity) / 255;
        uint8_t layer_b = (pixel->b * layers[layer].layer_opacity) / 255;
        
        // Apply pixel brightness
        uint8_t pixel_brightness = led_get_layer_brightness(layer);
        layer_r = (layer_r * pixel_brightness) / 255;
        layer_g = (layer_g * pixel_brightness) / 255;
        layer_b = (layer_b * pixel_brightness) / 255;
        
        // Blend with existing color (simple alpha blending)
        if (has_color) {
            final_r = (final_r + layer_r) / 2;
            final_g = (final_g + layer_g) / 2;
            final_b = (final_b + layer_b) / 2;
        } else {
            final_r = layer_r;
            final_g = layer_g;
            final_b = layer_b;
            has_color = true;
        }
    }
    
    // Apply global brightness
    led_apply_brightness(led_index, &final_r, &final_g, &final_b);
    
    // Update hardware LED
    led_set_pixel_safe(led_index, final_r, final_g, final_b);
    
    // Clear dirty flags for this pixel
    for (int layer = 0; layer < LED_LAYER_COUNT; layer++) {
        layers[layer].pixels[led_index].dirty = false;
    }
    
    led_clear_dirty(led_index);
}

static void led_apply_brightness(uint8_t led_index, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) {
        return;
    }
    
    *r = (*r * global_brightness) / 255;
    *g = (*g * global_brightness) / 255;
    *b = (*b * global_brightness) / 255;
}

static void led_mark_dirty(uint8_t led_index) {
    if (led_index < 73) {
        dirty_count++;
    }
}

static void led_clear_dirty(uint8_t led_index) {
    if (led_index < 73 && dirty_count > 0) {
        dirty_count--;
    }
}

static bool led_is_layer_enabled(led_layer_t layer) {
    if (layer >= LED_LAYER_COUNT) {
        return false;
    }
    
    return layers[layer].layer_enabled;
}

static uint8_t led_get_layer_brightness(led_layer_t layer) {
    if (layer >= LED_LAYER_COUNT) {
        return 255;
    }
    
    return layers[layer].pixels[0].brightness; // All pixels in layer have same brightness
}
