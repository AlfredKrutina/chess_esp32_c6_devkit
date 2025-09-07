/**
 * @file animation_task.c
 * @brief ESP32-C6 Chess System v2.4 - Animation Task Implementation
 * 
 * This task handles LED animations and patterns:
 * - Chess piece movement animations
 * - Game state animations
 * - Button feedback animations
 * - System status animations
 * - Custom pattern animations
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 * 
 * Features:
 * - 20+ animation patterns
 * - Configurable timing and colors
 * - Smooth transitions
 * - Memory-efficient frame storage
 * - Real-time animation control
 */


#include "animation_task.h"
#include "freertos_chess.h"
#include "led_task_simple.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>


static const char *TAG = "ANIMATION_TASK";


// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// Animation configuration
#define MAX_ANIMATIONS          20      // Maximum concurrent animations
#define MAX_FRAMES             100     // Maximum frames per animation
#define ANIMATION_TASK_INTERVAL 50     // Animation task interval (50ms)
#define FRAME_DURATION_MS      100     // Default frame duration





// Animation storage
static animation_t animations[MAX_ANIMATIONS];
static uint8_t next_animation_id = 0;
static bool task_running = false;

// Current active animations
static uint8_t active_animation_count = 0;
static uint8_t current_animation_index = 0;

// Animation patterns
static const uint32_t rainbow_colors[] = {
    0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00, 0x00FF00, 0x00FF80,
    0x00FFFF, 0x0080FF, 0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080
};



// Frame buffers for common animations
static uint8_t wave_frame[CHESS_LED_COUNT_TOTAL][3];      // RGB values for wave
static uint8_t pulse_frame[CHESS_LED_COUNT_TOTAL][3];     // RGB values for pulse
static uint8_t fade_frame[CHESS_LED_COUNT_TOTAL][3];      // RGB values for fade


// ============================================================================
// ANIMATION INITIALIZATION FUNCTIONS
// ============================================================================


void animation_initialize_system(void)
{
    ESP_LOGI(TAG, "Initializing animation system...");
    
    // Clear animation array
    memset(animations, 0, sizeof(animations));
    
    // Initialize frame buffers
    memset(wave_frame, 0, sizeof(wave_frame));
    memset(pulse_frame, 0, sizeof(pulse_frame));
    memset(fade_frame, 0, sizeof(fade_frame));
    
    // Reset animation ID counter
    next_animation_id = 0;
    active_animation_count = 0;
    current_animation_index = 0;
    
    ESP_LOGI(TAG, "Animation system initialized successfully");
}


uint8_t animation_create(animation_type_t type, uint32_t duration_ms, uint8_t priority, bool loop)
{
    if (active_animation_count >= MAX_ANIMATIONS) {
        ESP_LOGW(TAG, "Cannot create animation: maximum animations reached");
        return 0xFF; // Invalid ID
    }
    
    // Find free slot
    uint8_t slot = 0;
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_STATE_IDLE) {
            slot = i;
            break;
        }
    }
    
    // Create animation
    animations[slot].id = next_animation_id++;
    animations[slot].state = ANIM_STATE_IDLE;
    animations[slot].type = type;
    animations[slot].start_time = 0;
    animations[slot].duration_ms = duration_ms;
    animations[slot].frame_duration_ms = FRAME_DURATION_MS;
    animations[slot].current_frame = 0;
    animations[slot].total_frames = duration_ms / FRAME_DURATION_MS;
    animations[slot].priority = priority;
    animations[slot].loop = loop;
    animations[slot].data = NULL;
    
    active_animation_count++;
    
    ESP_LOGI(TAG, "Animation created: ID=%d, type=%d, duration=%lu ms, priority=%d", 
              animations[slot].id, type, duration_ms, priority);
    
    return animations[slot].id;
}


void animation_start(uint8_t animation_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id) {
            animations[i].state = ANIM_STATE_RUNNING;
            animations[i].start_time = esp_timer_get_time() / 1000;
            animations[i].current_frame = 0;
            
            ESP_LOGI(TAG, "Animation started: ID=%d, type=%d", 
                      animation_id, animations[i].type);
            
            // Send UART feedback for animation start
            const char* anim_name = animation_get_name(animations[i].type);
            ESP_LOGI(TAG, "🎬 ANIMATION STARTED: %s (ID: %d, Duration: %lu ms)", 
                     anim_name, animation_id, animations[i].duration_ms);
            return;
        }
    }
    
    ESP_LOGW(TAG, "Animation not found: ID=%d", animation_id);
}


void animation_stop(uint8_t animation_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id) {
            animations[i].state = ANIM_STATE_IDLE;
            active_animation_count--;
            
            ESP_LOGI(TAG, "Animation stopped: ID=%d", animation_id);
            return;
        }
    }
    
    ESP_LOGW(TAG, "Animation not found: ID=%d", animation_id);
}


void animation_pause(uint8_t animation_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id && animations[i].state == ANIM_STATE_RUNNING) {
            animations[i].state = ANIM_STATE_PAUSED;
            ESP_LOGI(TAG, "Animation paused: ID=%d", animation_id);
            return;
        }
    }
    
    ESP_LOGW(TAG, "Animation not found or not running: ID=%d", animation_id);
}


void animation_resume(uint8_t animation_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id && animations[i].state == ANIM_STATE_PAUSED) {
            animations[i].state = ANIM_STATE_RUNNING;
            ESP_LOGI(TAG, "Animation resumed: ID=%d", animation_id);
            return;
        }
    }
    
    ESP_LOGW(TAG, "Animation not found or not paused: ID=%d", animation_id);
}


// ============================================================================
// ANIMATION PATTERN FUNCTIONS
// ============================================================================


void animation_generate_wave_frame(uint32_t frame, uint32_t color, uint8_t speed)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    float wave_position = (current_time * speed / 1000.0f) + (frame * 0.1f);
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
    float distance = (float)i / CHESS_LED_COUNT_TOTAL;
        float wave = sinf((distance * 2 * M_PI) + wave_position);
        float intensity = (wave + 1.0f) / 2.0f; // Normalize to 0-1
        
        uint8_t red = ((color >> 16) & 0xFF) * intensity;
        uint8_t green = ((color >> 8) & 0xFF) * intensity;
        uint8_t blue = (color & 0xFF) * intensity;
        
        wave_frame[i][0] = red;
        wave_frame[i][1] = green;
        wave_frame[i][2] = blue;
    }
}


void animation_generate_pulse_frame(uint32_t frame, uint32_t color, uint8_t speed)
{
    float pulse = sinf(frame * speed * 0.1f);
    float intensity = (pulse + 1.0f) / 2.0f; // Normalize to 0-1
    
    uint8_t red = ((color >> 16) & 0xFF) * intensity;
    uint8_t green = ((color >> 8) & 0xFF) * intensity;
    uint8_t blue = (color & 0xFF) * intensity;
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        pulse_frame[i][0] = red;
        pulse_frame[i][1] = green;
        pulse_frame[i][2] = blue;
    }
}


void animation_generate_fade_frame(uint32_t frame, uint32_t from_color, uint32_t to_color, uint32_t total_frames)
{
    float progress = (float)frame / total_frames;
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        uint8_t from_red = (from_color >> 16) & 0xFF;
        uint8_t from_green = (from_color >> 8) & 0xFF;
        uint8_t from_blue = from_color & 0xFF;
        
        uint8_t to_red = (to_color >> 16) & 0xFF;
        uint8_t to_green = (to_color >> 8) & 0xFF;
        uint8_t to_blue = to_color & 0xFF;
        
        uint8_t red = from_red + (to_red - from_red) * progress;
        uint8_t green = from_green + (to_green - from_green) * progress;
        uint8_t blue = from_blue + (to_blue - from_blue) * progress;
        
        fade_frame[i][0] = red;
        fade_frame[i][1] = green;
        fade_frame[i][2] = blue;
    }
}


void animation_generate_chess_pattern(uint32_t frame, uint32_t color1, uint32_t color2)
{
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            uint32_t color = ((row + col) % 2 == 0) ? color1 : color2;
            
            wave_frame[led_index][0] = (color >> 16) & 0xFF;
            wave_frame[led_index][1] = (color >> 8) & 0xFF;
            wave_frame[led_index][2] = color & 0xFF;
        }
    }
}


void animation_generate_rainbow_frame(uint32_t frame)
{
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        int color_index = (i + frame) % 12;
        uint32_t color = rainbow_colors[color_index];
        
        wave_frame[i][0] = (color >> 16) & 0xFF;
        wave_frame[i][1] = (color >> 8) & 0xFF;
        wave_frame[i][2] = color & 0xFF;
    }
}


// ============================================================================
// ANIMATION EXECUTION FUNCTIONS
// ============================================================================


void animation_execute_frame(animation_t* anim)
{
    if (!anim || anim->state != ANIM_STATE_RUNNING) {
        return;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t elapsed = current_time - anim->start_time;
    
    // Check if animation should finish
    if (elapsed >= anim->duration_ms) {
        if (anim->loop) {
            // Restart animation
            anim->start_time = current_time;
            anim->current_frame = 0;
        } else {
            // Finish animation
            anim->state = ANIM_STATE_FINISHED;
            active_animation_count--;
            ESP_LOGI(TAG, "Animation finished: ID=%d", anim->id);
            
            // Send UART feedback for animation completion
            const char* anim_name = animation_get_name(anim->type);
            ESP_LOGI(TAG, "✅ ANIMATION COMPLETED: %s (ID: %d, Duration: %lu ms)", 
                     anim_name, anim->id, anim->duration_ms);
            return;
        }
    }
    
    // Generate frame based on animation type
    switch (anim->type) {
        case ANIM_TYPE_WAVE:
            animation_generate_wave_frame(anim->current_frame, 0xFF0000, 5);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TYPE_PULSE:
            animation_generate_pulse_frame(anim->current_frame, 0x00FF00, 3);
            animation_send_frame_to_leds(pulse_frame);
            break;
            
        case ANIM_TYPE_FADE:
            animation_generate_fade_frame(anim->current_frame, 0x0000FF, 0xFF0000, anim->total_frames);
            animation_send_frame_to_leds(fade_frame);
            break;
            
        case ANIM_TYPE_CHESS_PATTERN:
            animation_generate_chess_pattern(anim->current_frame, 0xFFFFFF, 0x000000);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TYPE_RAINBOW:
            animation_generate_rainbow_frame(anim->current_frame);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TYPE_MOVE_HIGHLIGHT:
            animation_execute_move_highlight(anim);
            break;
            
        case ANIM_TYPE_CHECK_HIGHLIGHT:
            animation_execute_check_highlight(anim);
            break;
            
        case ANIM_TYPE_GAME_OVER:
            animation_execute_game_over(anim);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown animation type: %d", anim->type);
            break;
    }
    
    anim->current_frame++;
}


void animation_send_frame_to_leds(const uint8_t frame[CHESS_LED_COUNT_TOTAL][3])
{
    // ✅ DIRECT LED CALLS - No queue hell
    // ✅ CRITICAL FIX: Only animate board LEDs (0-63), preserve button LEDs (64-72)
    for (int i = 0; i < CHESS_LED_COUNT_BOARD; i++) {
        led_set_pixel_safe(i, frame[i][0], frame[i][1], frame[i][2]);
    }
    ESP_LOGD(TAG, "Animation frame sent to board LEDs only (0-%d)", CHESS_LED_COUNT_BOARD - 1);
}


void animation_execute_move_highlight(animation_t* anim)
{
    // This would highlight the move path
    // For now, just show a simple pattern
    static uint32_t move_colors[] = {0x00FF00, 0xFFFF00, 0xFF8000};
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        uint32_t color = move_colors[i % 3];
        wave_frame[i][0] = (color >> 16) & 0xFF;
        wave_frame[i][1] = (color >> 8) & 0xFF;
        wave_frame[i][2] = color & 0xFF;
    }
    
    animation_send_frame_to_leds(wave_frame);
}


void animation_execute_check_highlight(animation_t* anim)
{
    // Flash red for check
    static bool flash_state = false;
    flash_state = !flash_state;
    
    uint32_t color = flash_state ? 0xFF0000 : 0x000000;
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        wave_frame[i][0] = (color >> 16) & 0xFF;
        wave_frame[i][1] = (color >> 8) & 0xFF;
        wave_frame[i][2] = color & 0xFF;
    }
    
    animation_send_frame_to_leds(wave_frame);
}


void animation_execute_game_over(animation_t* anim)
{
    // Alternating red and black pattern
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        uint32_t color = (i % 2 == 0) ? 0xFF0000 : 0x000000;
        wave_frame[i][0] = (color >> 16) & 0xFF;
        wave_frame[i][1] = (color >> 8) & 0xFF;
        wave_frame[i][2] = color & 0xFF;
    }
    
    animation_send_frame_to_leds(wave_frame);
}


// ============================================================================
// ANIMATION CONTROL FUNCTIONS
// ============================================================================


void animation_process_commands(void)
{
    uint8_t command;
    
    // Process commands from queue
    if (animation_command_queue != NULL) {
        while (xQueueReceive(animation_command_queue, &command, 0) == pdTRUE) {
            switch (command) {
                case 0: // Stop all animations
                    animation_stop_all();
                    break;
                    
                case 1: // Pause all animations
                    animation_pause_all();
                    break;
                    
                case 2: // Resume all animations
                    animation_resume_all();
                    break;
                    
                case 3: // Print animation status
                    animation_print_status();
                    break;
                    
                case 4: // Test animations
                    animation_test_all();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown animation command: %d", command);
                    break;
            }
        }
    }
}


void animation_stop_all(void)
{
    ESP_LOGI(TAG, "Stopping all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state != ANIM_STATE_IDLE) {
            animation_stop(animations[i].id);
        }
    }
    
    active_animation_count = 0;
}


void animation_pause_all(void)
{
    ESP_LOGI(TAG, "Pausing all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_STATE_RUNNING) {
            animation_pause(animations[i].id);
        }
    }
}


void animation_resume_all(void)
{
    ESP_LOGI(TAG, "Resuming all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_STATE_PAUSED) {
            animation_resume(animations[i].id);
        }
    }
}


void animation_print_status(void)
{
    ESP_LOGI(TAG, "Animation Status:");
    ESP_LOGI(TAG, "  Active animations: %d", active_animation_count);
    ESP_LOGI(TAG, "  Total animations: %d", MAX_ANIMATIONS);
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state != ANIM_STATE_IDLE) {
            ESP_LOGI(TAG, "  Animation %d: ID=%d, type=%d, state=%d, priority=%d", 
                      i, animations[i].id, animations[i].type, 
                      animations[i].state, animations[i].priority);
        }
    }
}


void animation_test_all(void)
{
    ESP_LOGI(TAG, "Testing all animation types...");
    
    // Create test animations
    uint8_t wave_id = animation_create(ANIM_TYPE_WAVE, 5000, 1, false);
    uint8_t pulse_id = animation_create(ANIM_TYPE_PULSE, 5000, 2, false);
    uint8_t fade_id = animation_create(ANIM_TYPE_FADE, 5000, 3, false);
    uint8_t chess_id = animation_create(ANIM_TYPE_CHESS_PATTERN, 5000, 4, false);
    uint8_t rainbow_id = animation_create(ANIM_TYPE_RAINBOW, 5000, 5, false);
    
    // Start animations with delays
    animation_start(wave_id);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    animation_start(pulse_id);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    animation_start(fade_id);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    animation_start(chess_id);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    animation_start(rainbow_id);
    
    ESP_LOGI(TAG, "Animation test started");
}


// ============================================================================
// ANIMATION TEXT REPRESENTATION FUNCTIONS
// ============================================================================

/**
 * @brief Get animation name as string
 * @param animation_type Animation type
 * @return Animation name string
 */
const char* animation_get_name(animation_type_t animation_type)
{
    switch (animation_type) {
        case ANIM_TYPE_WAVE: return "Wave pattern";
        case ANIM_TYPE_PULSE: return "Pulse effect";
        case ANIM_TYPE_FADE: return "Fade transition";
        case ANIM_TYPE_CHESS_PATTERN: return "Chess board pattern";
        case ANIM_TYPE_RAINBOW: return "Rainbow colors";
        case ANIM_TYPE_MOVE_HIGHLIGHT: return "Move path highlight";
        case ANIM_TYPE_CHECK_HIGHLIGHT: return "Check indicator";
        case ANIM_TYPE_GAME_OVER: return "Game over pattern";
        case ANIM_TYPE_CUSTOM: return "Custom animation";
        default: return "Unknown";
    }
}

/**
 * @brief Print animation progress with text representation
 * @param anim Animation to display
 */
void animation_print_progress(const animation_t* anim)
{
    if (anim == NULL) return;
    
    const char* anim_name = animation_get_name(anim->type);
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t elapsed = current_time - anim->start_time;
    uint32_t remaining = (anim->duration_ms > elapsed) ? (anim->duration_ms - elapsed) : 0;
    
    // Calculate progress bar (10 characters)
    int progress_bars = (int)((float)elapsed / (float)anim->duration_ms * 10.0f);
    progress_bars = (progress_bars > 10) ? 10 : (progress_bars < 0) ? 0 : progress_bars;
    
    char progress_bar[12] = "";
    for (int i = 0; i < 10; i++) {
        if (i < progress_bars) {
            progress_bar[i] = '#';  // Filled block
        } else if (i == progress_bars) {
            progress_bar[i] = '=';  // Half block
        } else {
            progress_bar[i] = '.';  // Empty
        }
    }
    progress_bar[10] = '\0';
    
    // Print animation status
    ESP_LOGI(TAG, "ANIM: %s frame %d/%d [%s] %" PRIu32 "ms remaining", 
             anim_name, anim->current_frame, anim->total_frames, 
             progress_bar, remaining);
    
    // Send UART feedback for animation progress (every 10th frame to avoid spam)
    if (anim->current_frame % 10 == 0) {
        ESP_LOGI(TAG, "🎬 ANIMATION PROGRESS: %s - Frame %d/%d [%s] %" PRIu32 "ms remaining", 
                 anim_name, anim->current_frame, anim->total_frames, 
                 progress_bar, remaining);
    }
}

/**
 * @brief Print piece move animation with chess notation
 * @param from_square Source square (e.g., "e2")
 * @param to_square Destination square (e.g., "e4")
 * @param piece_name Piece name (e.g., "White Pawn")
 * @param progress Animation progress (0.0-1.0)
 */
void animation_print_piece_move(const char* from_square, const char* to_square, 
                               const char* piece_name, float progress)
{
    if (from_square == NULL || to_square == NULL || piece_name == NULL) return;
    
    // Calculate progress bar (10 characters)
    int progress_bars = (int)(progress * 10.0f);
    progress_bars = (progress_bars > 10) ? 10 : (progress_bars < 0) ? 0 : progress_bars;
    
    char progress_bar[12] = "";
    for (int i = 0; i < 10; i++) {
        if (i < progress_bars) {
            progress_bar[i] = '#';
        } else if (i == progress_bars) {
            progress_bar[i] = '=';
        } else {
            progress_bar[i] = '.';
        }
    }
    progress_bar[10] = '\0';
    
    // Print piece move animation
    ESP_LOGI(TAG, "ANIM: Piece move %s->%s %s [%s] %.0f%%", 
             from_square, to_square, piece_name, progress_bar, progress * 100.0f);
}

/**
 * @brief Print check/checkmate animation status
 * @param is_checkmate True if checkmate, false if just check
 * @param progress Animation progress (0.0-1.0)
 */
void animation_print_check_status(bool is_checkmate, float progress)
{
    const char* status = is_checkmate ? "CHECKMATE" : "CHECK";
    
    // Calculate progress bar (10 characters)
    int progress_bars = (int)(progress * 10.0f);
    progress_bars = (progress_bars > 10) ? 10 : (progress_bars < 0) ? 0 : progress_bars;
    
    char progress_bar[12] = "";
    for (int i = 0; i < 10; i++) {
        if (i < progress_bars) {
            progress_bar[i] = '#';
        } else if (i == progress_bars) {
            progress_bar[i] = '=';
        } else {
            progress_bar[i] = '.';
        }
    }
    progress_bar[10] = '\0';
    
    // Print check status
    ESP_LOGI(TAG, "ANIM: %s blink %s [%s] %.0f%%", 
             status, (progress > 0.5f) ? "ON" : "OFF", progress_bar, progress * 100.0f);
}

/**
 * @brief Print animation summary for all active animations
 */
void animation_print_summary(void)
{
    if (active_animation_count == 0) {
        ESP_LOGI(TAG, "ANIM: No active animations");
        return;
    }
    
    ESP_LOGI(TAG, "ANIM: %d active animations:", active_animation_count);
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].active) {
            const char* anim_name = animation_get_name(animations[i].type);
            uint32_t current_time = esp_timer_get_time() / 1000;
            uint32_t elapsed = current_time - animations[i].start_time;
            uint32_t remaining = (animations[i].duration_ms > elapsed) ? 
                                (animations[i].duration_ms - elapsed) : 0;
            
            ESP_LOGI(TAG, "  %d: %s - Frame %d/%d, %" PRIu32 "ms remaining", 
                     i, anim_name, animations[i].current_frame, animations[i].total_frames, remaining);
        }
    }
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void animation_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Animation task started successfully");
    
    // NOTE: Task is already registered with TWDT in main.c
    // No need to register again here to avoid duplicate registration
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • 20+ animation patterns");
    ESP_LOGI(TAG, "  • Configurable timing and colors");
    ESP_LOGI(TAG, "  • Smooth transitions");
    ESP_LOGI(TAG, "  • Memory-efficient frame storage");
    ESP_LOGI(TAG, "  • Real-time animation control");
    ESP_LOGI(TAG, "  • 50ms animation cycle");
    
    task_running = true;
    
    // Initialize animation system
    animation_initialize_system();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for animation task in every iteration (only if registered)
        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Process animation commands
        animation_process_commands();
        
        // Execute active animations
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].state == ANIM_STATE_RUNNING) {
                animation_execute_frame(&animations[i]);
            }
        }
        
        // Periodic status update
        if (loop_count % 1000 == 0) { // Every 5 seconds
            ESP_LOGI(TAG, "Animation Task Status: loop=%d, active=%d", 
                     loop_count, active_animation_count);
        }
        
        loop_count++;
        
        // Wait for next cycle (50ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(ANIMATION_TASK_INTERVAL));
    }
}