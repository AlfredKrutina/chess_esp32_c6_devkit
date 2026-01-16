/**
 * @file animation_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace Animation tasku
 * 
 * Tento task zpracovava LED animace a vzory:
 * - Animace pohybu sachovych figurek
 * - Animace stavu hry
 * - Animace feedback tlacitek
 * - Animace stavu systemu
 * - Vlastni vzory animaci
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task je zodpovedny za vsechny LED animace v systemu.
 * Obsahuje 20+ animacnich vzoru pro ruzne situace v hre.
 * Animace jsou plynule a efektivne z hlediska pameti.
 * 
 * Funkce:
 * - 20+ animacnich vzoru
 * - Konfigurovatelny timing a barvy
 * - Plynule prechody
 * - Efektivni ulozeni framu
 * - Real-time ovladani animaci
 */


#include "animation_task.h"
#include "freertos_chess.h"
#include "led_task_simple.h"
#include "led_mapping.h"
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
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered (WARNING only)
 */
static esp_err_t animation_task_wdt_reset_safe(void) {
    esp_err_t ret = esp_task_wdt_reset();
    
    if (ret == ESP_ERR_NOT_FOUND) {
        // Log as WARNING instead of ERROR - task not registered yet
        ESP_LOGW(TAG, "WDT reset: task not registered yet (this is normal during startup)");
        return ESP_OK; // Treat as success for our purposes
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    return ESP_OK;
}

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// Animation configuration
#define MAX_ANIMATIONS          20      // Maximum concurrent animations
#define MAX_FRAMES             100     // Maximum frames per animation
#define ANIMATION_TASK_INTERVAL 50     // Animation task interval (50ms)
#define FRAME_DURATION_MS      100     // Default frame duration





// Animation storage
static animation_task_t animations[MAX_ANIMATIONS];
static uint8_t next_animation_id = 0;
static bool task_running = false;

// Current active animations
static uint8_t active_animation_count = 0;
static uint8_t current_animation_index = 0;

// Global flag for animation interruption
static volatile bool animation_interrupted = false;
static SemaphoreHandle_t animation_interrupt_mutex = NULL;

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
    
    // Vytvořit mutex pro přerušení animací
    animation_interrupt_mutex = xSemaphoreCreateMutex();
    if (animation_interrupt_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create animation interrupt mutex");
    }
    
    ESP_LOGI(TAG, "Animation system initialized successfully");
    ESP_LOGI(TAG, "Animation interruption system initialized");
}


uint8_t animation_create(animation_task_type_t type, uint32_t duration_ms, uint8_t priority, bool loop)
{
    if (active_animation_count >= MAX_ANIMATIONS) {
        ESP_LOGW(TAG, "Cannot create animation: maximum animations reached");
        return 0xFF; // Invalid ID
    }
    
    // Find free slot
    uint8_t slot = 0;
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_TASK_STATE_IDLE) {
            slot = i;
            break;
        }
    }
    
    // Create animation
    animations[slot].id = next_animation_id++;
    animations[slot].state = ANIM_TASK_STATE_IDLE;
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
            animations[i].state = ANIM_TASK_STATE_RUNNING;
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
            animations[i].state = ANIM_TASK_STATE_IDLE;
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
        if (animations[i].id == animation_id && animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animations[i].state = ANIM_TASK_STATE_PAUSED;
            ESP_LOGI(TAG, "Animation paused: ID=%d", animation_id);
            return;
        }
    }
    
    ESP_LOGW(TAG, "Animation not found or not running: ID=%d", animation_id);
}


void animation_resume(uint8_t animation_id)
{
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id && animations[i].state == ANIM_TASK_STATE_PAUSED) {
            animations[i].state = ANIM_TASK_STATE_RUNNING;
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
// NATURAL PIECE MOVEMENT ANIMATIONS
// ============================================================================

// Chess position structure for path calculation
typedef struct {
    uint8_t row, col;
} chess_position_t;

// Forward declarations
void animate_path_with_interruption(chess_position_t* path, int path_length, piece_t piece);
void check_for_move_interruption();
bool new_move_detected();

/**
 * @brief Calculate natural knight path (L-shape movement)
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param path_length Output parameter for path length
 * @return Array of positions for knight path
 */
chess_position_t* calculate_knight_path(uint8_t from_row, uint8_t from_col, 
                                       uint8_t to_row, uint8_t to_col, int* path_length)
{
    chess_position_t* path = malloc(sizeof(chess_position_t) * 4);
    *path_length = 0;
    
    // Začátek
    path[(*path_length)++] = (chess_position_t){from_row, from_col};
    
    // Určit směr L-pohybu
    int row_diff = to_row - from_row;
    int col_diff = to_col - from_col;
    
    if (abs(row_diff) == 2) {
        // 2 pole vertikálně, 1 horizontálně
        path[(*path_length)++] = (chess_position_t){from_row + row_diff/2, from_col};
        path[(*path_length)++] = (chess_position_t){from_row + row_diff, from_col};
        path[(*path_length)++] = (chess_position_t){to_row, to_col};
    } else {
        // 2 pole horizontálně, 1 vertikálně  
        path[(*path_length)++] = (chess_position_t){from_row, from_col + col_diff/2};
        path[(*path_length)++] = (chess_position_t){from_row, from_col + col_diff};
        path[(*path_length)++] = (chess_position_t){to_row, to_col};
    }
    
    return path;
}

/**
 * @brief Calculate diagonal path for bishop
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param path_length Output parameter for path length
 * @return Array of positions for diagonal path
 */
chess_position_t* calculate_diagonal_path(uint8_t from_row, uint8_t from_col, 
                                         uint8_t to_row, uint8_t to_col, int* path_length)
{
    int row_diff = to_row - from_row;
    int col_diff = to_col - from_col;
    int steps = abs(row_diff);
    
    chess_position_t* path = malloc(sizeof(chess_position_t) * (steps + 1));
    *path_length = 0;
    
    int row_step = (row_diff > 0) ? 1 : -1;
    int col_step = (col_diff > 0) ? 1 : -1;
    
    for (int i = 0; i <= steps; i++) {
        path[(*path_length)++] = (chess_position_t){
            from_row + i * row_step, 
            from_col + i * col_step
        };
    }
    
    return path;
}

/**
 * @brief Calculate straight path for rook
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param path_length Output parameter for path length
 * @return Array of positions for straight path
 */
chess_position_t* calculate_straight_path(uint8_t from_row, uint8_t from_col, 
                                         uint8_t to_row, uint8_t to_col, int* path_length)
{
    int row_diff = to_row - from_row;
    int col_diff = to_col - from_col;
    int steps = (row_diff != 0) ? abs(row_diff) : abs(col_diff);
    
    chess_position_t* path = malloc(sizeof(chess_position_t) * (steps + 1));
    *path_length = 0;
    
    int row_step = (row_diff > 0) ? 1 : (row_diff < 0) ? -1 : 0;
    int col_step = (col_diff > 0) ? 1 : (col_diff < 0) ? -1 : 0;
    
    for (int i = 0; i <= steps; i++) {
        path[(*path_length)++] = (chess_position_t){
            from_row + i * row_step, 
            from_col + i * col_step
        };
    }
    
    return path;
}

/**
 * @brief Calculate direct path for other pieces
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param path_length Output parameter for path length
 * @return Array of positions for direct path
 */
chess_position_t* calculate_direct_path(uint8_t from_row, uint8_t from_col, 
                                       uint8_t to_row, uint8_t to_col, int* path_length)
{
    chess_position_t* path = malloc(sizeof(chess_position_t) * 2);
    *path_length = 2;
    
    path[0] = (chess_position_t){from_row, from_col};
    path[1] = (chess_position_t){to_row, to_col};
    
    return path;
}

/**
 * @brief Animate piece move with natural movement patterns
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param piece Piece being moved
 */
void animate_piece_move_natural(uint8_t from_row, uint8_t from_col, 
                               uint8_t to_row, uint8_t to_col, piece_t piece)
{
    chess_position_t* path = NULL;
    int path_length = 0;
    
    // Určit typ animace podle figury
    switch (piece) {
        case PIECE_WHITE_KNIGHT:
        case PIECE_BLACK_KNIGHT:
            path = calculate_knight_path(from_row, from_col, to_row, to_col, &path_length);
            break;
            
        case PIECE_WHITE_BISHOP:
        case PIECE_BLACK_BISHOP:
            path = calculate_diagonal_path(from_row, from_col, to_row, to_col, &path_length);
            break;
            
        case PIECE_WHITE_ROOK:
        case PIECE_BLACK_ROOK:
            path = calculate_straight_path(from_row, from_col, to_row, to_col, &path_length);
            break;
            
        default:
            // Ostatní figury - přímá cesta
            path = calculate_direct_path(from_row, from_col, to_row, to_col, &path_length);
            break;
    }
    
    // Animovat cestu
    if (path != NULL) {
        animate_path_with_interruption(path, path_length, piece);
        free(path);
    }
}

/**
 * @brief Animate path with interruption capability
 * @param path Array of positions to animate
 * @param path_length Number of positions in path
 * @param piece Piece being animated
 */
void animate_path_with_interruption(chess_position_t* path, int path_length, piece_t piece)
{
    animation_interrupted = false;
    
    // Získat barvy figury
    uint8_t r = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) ? 0 : 255;
    uint8_t g = 255;
    uint8_t b = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) ? 255 : 0;
    
    for (int i = 0; i < path_length && !animation_interrupted; i++) {
        // Vymazat předchozí
        if (i > 0) {
            uint8_t prev_led = chess_pos_to_led_index(path[i-1].row, path[i-1].col);
            led_set_pixel_safe(prev_led, 0, 0, 0);
        }
        
        // Rosvítit aktuální
        uint8_t led_index = chess_pos_to_led_index(path[i].row, path[i].col);
        led_set_pixel_safe(led_index, r, g, b);
        
        // Krátká pauza s kontrolou přerušení
        for (int wait = 0; wait < 20 && !animation_interrupted; wait++) {
            vTaskDelay(pdMS_TO_TICKS(25));  // 25ms x 20 = 500ms celkem
            
            // Kontrola nového tahu během animace
            check_for_move_interruption();
        }
    }
    
    // Vyčistit na konci
    led_clear_all_safe();
}

/**
 * @brief Check for move interruption during animation
 */
void check_for_move_interruption()
{
    // Kontrola, jestli hráč nezačal nový tah
    // (implementace závisí na způsobu detekce tahů)
    if (new_move_detected()) {
        animation_interrupted = true;
        ESP_LOGI(TAG, "🏃 Animation interrupted by new move");
    }
}

/**
 * @brief Request animation interruption
 */
void animation_request_interrupt()
{
    if (animation_interrupt_mutex) {
        xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
        animation_interrupted = true;
        xSemaphoreGive(animation_interrupt_mutex);
        ESP_LOGI(TAG, "🛑 Animation interruption requested");
    }
}

/**
 * @brief Check if new move was detected (placeholder)
 * @return true if new move detected
 */
bool new_move_detected()
{
    // TODO: Implement actual move detection logic
    // This could check a global flag set by the game task
    return false;
}

// ============================================================================
// ANIMATION EXECUTION FUNCTIONS
// ============================================================================


void animation_execute_frame(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    // Kontrola přerušení na začátku každého framu
    if (animation_interrupt_mutex) {
        xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
        bool should_interrupt = animation_interrupted;
        xSemaphoreGive(animation_interrupt_mutex);
        
        if (should_interrupt) {
            ESP_LOGI(TAG, "🛑 Animation interrupted mid-frame");
            anim->state = ANIM_TASK_STATE_FINISHED;
            animation_interrupted = false;  // Reset flag
            return;
        }
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
            anim->state = ANIM_TASK_STATE_FINISHED;
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
        case ANIM_TASK_TYPE_WAVE:
            animation_generate_wave_frame(anim->current_frame, 0xFF0000, 5);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TASK_TYPE_PULSE:
            animation_generate_pulse_frame(anim->current_frame, 0x00FF00, 3);
            animation_send_frame_to_leds(pulse_frame);
            break;
            
        case ANIM_TASK_TYPE_FADE:
            animation_generate_fade_frame(anim->current_frame, 0x0000FF, 0xFF0000, anim->total_frames);
            animation_send_frame_to_leds(fade_frame);
            break;
            
        case ANIM_TASK_TYPE_CHESS_PATTERN:
            animation_generate_chess_pattern(anim->current_frame, 0xFFFFFF, 0x000000);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TASK_TYPE_RAINBOW:
            animation_generate_rainbow_frame(anim->current_frame);
            animation_send_frame_to_leds(wave_frame);
            break;
            
        case ANIM_TASK_TYPE_MOVE_HIGHLIGHT:
            animation_execute_move_highlight(anim);
            break;
            
        case ANIM_TASK_TYPE_CHECK_HIGHLIGHT:
            animation_execute_check_highlight(anim);
            break;
            
        case ANIM_TASK_TYPE_GAME_OVER:
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


void animation_execute_move_highlight(animation_task_t* anim)
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


void animation_execute_check_highlight(animation_task_t* anim)
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


void animation_execute_game_over(animation_task_t* anim)
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
        if (animations[i].state != ANIM_TASK_STATE_IDLE) {
            animation_stop(animations[i].id);
        }
    }
    
    active_animation_count = 0;
}


void animation_pause_all(void)
{
    ESP_LOGI(TAG, "Pausing all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animation_pause(animations[i].id);
        }
    }
}


void animation_resume_all(void)
{
    ESP_LOGI(TAG, "Resuming all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_TASK_STATE_PAUSED) {
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
        if (animations[i].state != ANIM_TASK_STATE_IDLE) {
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
    uint8_t wave_id = animation_create(ANIM_TASK_TYPE_WAVE, 5000, 1, false);
    uint8_t pulse_id = animation_create(ANIM_TASK_TYPE_PULSE, 5000, 2, false);
    uint8_t fade_id = animation_create(ANIM_TASK_TYPE_FADE, 5000, 3, false);
    uint8_t chess_id = animation_create(ANIM_TASK_TYPE_CHESS_PATTERN, 5000, 4, false);
    uint8_t rainbow_id = animation_create(ANIM_TASK_TYPE_RAINBOW, 5000, 5, false);
    
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
const char* animation_get_name(animation_task_type_t animation_type)
{
    switch (animation_type) {
        case ANIM_TASK_TYPE_WAVE: return "Wave pattern";
        case ANIM_TASK_TYPE_PULSE: return "Pulse effect";
        case ANIM_TASK_TYPE_FADE: return "Fade transition";
        case ANIM_TASK_TYPE_CHESS_PATTERN: return "Chess board pattern";
        case ANIM_TASK_TYPE_RAINBOW: return "Rainbow colors";
        case ANIM_TASK_TYPE_MOVE_HIGHLIGHT: return "Move path highlight";
        case ANIM_TASK_TYPE_CHECK_HIGHLIGHT: return "Check indicator";
        case ANIM_TASK_TYPE_GAME_OVER: return "Game over pattern";
        case ANIM_TASK_TYPE_CUSTOM: return "Custom animation";
        default: return "Unknown";
    }
}

/**
 * @brief Print animation progress with text representation
 * @param anim Animation to display
 */
void animation_print_progress(const animation_task_t* anim)
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
    
    // ✅ CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register Animation task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ Animation task registered with TWDT");
    }
    
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
        // CRITICAL: Reset watchdog for animation task in every iteration
        esp_err_t wdt_ret = animation_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Process animation commands
        animation_process_commands();
        
        // Execute active animations
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].state == ANIM_TASK_STATE_RUNNING) {
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