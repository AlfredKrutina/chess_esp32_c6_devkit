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
#include "../game_task/include/game_task.h"  // For game_get_piece() in endgame animation
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

// ✅ NOVÉ: Mutex pro animations pole (race condition ochrana)
static SemaphoreHandle_t animations_mutex = NULL;

// Animation patterns
static const uint32_t rainbow_colors[] = {
    0xFF0000, 0xFF8000, 0xFFFF00, 0x80FF00, 0x00FF00, 0x00FF80,
    0x00FFFF, 0x0080FF, 0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080
};



// Frame buffers for common animations
static uint8_t wave_frame[CHESS_LED_COUNT_TOTAL][3];      // RGB values for wave
static uint8_t pulse_frame[CHESS_LED_COUNT_TOTAL][3];     // RGB values for pulse
static uint8_t fade_frame[CHESS_LED_COUNT_TOTAL][3];      // RGB values for fade

// Forward declarations for execute functions (defined later in file)
static void animation_clear_board_only(void);
void animation_execute_player_change(animation_task_t* anim);
void animation_execute_move_path(animation_task_t* anim);
void animation_execute_castle(animation_task_t* anim);
void animation_execute_endgame(animation_task_t* anim);
void animation_execute_check(animation_task_t* anim);
void animation_execute_checkmate(animation_task_t* anim);

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
    
    // ✅ NOVÉ: Vytvořit mutex pro animations pole
    animations_mutex = xSemaphoreCreateMutex();
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create animations mutex");
    }
    
    ESP_LOGI(TAG, "Animation system initialized successfully");
    ESP_LOGI(TAG, "Animation interruption system initialized");
}


uint8_t animation_create(animation_task_type_t type, uint32_t duration_ms, uint8_t priority, bool loop)
{
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return 0xFF; // Invalid ID
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_create()");
        return 0xFF; // Invalid ID
    }
    
    uint8_t created_id = 0xFF; // Invalid ID by default
    
    if (active_animation_count >= MAX_ANIMATIONS) {
        ESP_LOGW(TAG, "Cannot create animation: maximum animations reached");
        xSemaphoreGive(animations_mutex);
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
    
    // ✅ NOVÉ: Inicializovat data_union na nulu
    memset(&animations[slot].data_union, 0, sizeof(animation_data_union_t));
    
    // DEPRECATED - ponechat pro zpětnou kompatibilitu
    animations[slot].data = NULL;
    
    active_animation_count++;
    created_id = animations[slot].id;
    
    ESP_LOGI(TAG, "Animation created: ID=%d, type=%d, duration=%lu ms, priority=%d", 
              animations[slot].id, type, duration_ms, priority);
    
    xSemaphoreGive(animations_mutex);
    return created_id;
}

// ============================================================================
// HELPER FUNKCE PRO VYTVÁŘENÍ ANIMACÍ S PARAMETRY
// ============================================================================

uint8_t animation_create_player_change(player_t player, uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_PLAYER_CHANGE, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a inicializovat data
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                animations[i].data_union.player_change.current_player = player;
                animations[i].data_union.player_change.player_color = (player == PLAYER_WHITE) ? 1 : 0; // DEPRECATED
                // ✅ Set correct total_frames: 50 frames for player_change animation
                animations[i].total_frames = 50;
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_move_path(uint8_t from_row, uint8_t from_col, 
                                    uint8_t to_row, uint8_t to_col,
                                    uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_MOVE_PATH, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a inicializovat data
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                animations[i].data_union.move_path.from_row = from_row;
                animations[i].data_union.move_path.from_col = from_col;
                animations[i].data_union.move_path.to_row = to_row;
                animations[i].data_union.move_path.to_col = to_col;
                animations[i].data_union.move_path.from_led = chess_pos_to_led_index(from_row, from_col);
                animations[i].data_union.move_path.to_led = chess_pos_to_led_index(to_row, to_col);
                // ✅ Set correct total_frames: 25 trail frames + 8 breath frames = 33 frames
                animations[i].total_frames = 33;
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_castle(uint8_t king_from_led, uint8_t king_to_led,
                                uint8_t rook_from_led, uint8_t rook_to_led,
                                uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_CASTLE, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a inicializovat data
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                animations[i].data_union.castle.king_from_led = king_from_led;
                animations[i].data_union.castle.king_to_led = king_to_led;
                animations[i].data_union.castle.rook_from_led = rook_from_led;
                animations[i].data_union.castle.rook_to_led = rook_to_led;
                // ✅ Set correct total_frames: 15 trail frames + 3 burst frames = 18 frames
                animations[i].total_frames = 18;
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_endgame(uint8_t king_led, piece_t winner_piece,
                                 uint32_t duration_ms, uint8_t priority, bool loop)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_ENDGAME, duration_ms, priority, loop);
    if (anim_id != 0xFF) {
        // Najít animaci a inicializovat data
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                animations[i].data_union.endgame.king_led = king_led;
                animations[i].data_union.endgame.winner_piece = winner_piece;
                animations[i].data_union.endgame.radius = 1;
                animations[i].data_union.endgame.last_radius_update = esp_timer_get_time() / 1000;
                
                // Převést LED index na souřadnice
                led_index_to_chess_pos(king_led, &animations[i].data_union.endgame.king_row, 
                                       &animations[i].data_union.endgame.king_col);
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_check(uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_CHECK, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a nastavit správný počet framů
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                // ✅ Set correct total_frames: 6 flashes × 2 (on/off) = 12 frames
                animations[i].total_frames = 12;
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_checkmate(uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_CHECKMATE, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a nastavit správný počet framů
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                // ✅ Set correct total_frames: 8 flashes × 2 (on/off) = 16 frames
                animations[i].total_frames = 16;
                break;
            }
        }
    }
    return anim_id;
}

uint8_t animation_create_promote(uint8_t promotion_led, uint32_t duration_ms, uint8_t priority)
{
    uint8_t anim_id = animation_create(ANIM_TASK_TYPE_PROMOTE, duration_ms, priority, false);
    if (anim_id != 0xFF) {
        // Najít animaci a inicializovat data
        for (int i = 0; i < MAX_ANIMATIONS; i++) {
            if (animations[i].id == anim_id) {
                animations[i].data_union.promote.promotion_led = promotion_led;
                break;
            }
        }
    }
    return anim_id;
}

void animation_stop_by_type(animation_task_type_t type)
{
    // ✅ NOVÉ: Mutex ochrana - ale animation_stop() už má vlastní mutex
    // Voláme animation_stop() která má mutex, takže není potřeba další mutex zde
    // (animation_stop() si vezme mutex uvnitř)
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].type == type && animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animation_stop(animations[i].id);
        }
    }
}

void animation_stop_all_except(animation_task_type_t except_type)
{
    // ✅ NOVÉ: Mutex ochrana - ale animation_stop() už má vlastní mutex
    // Voláme animation_stop() která má mutex, takže není potřeba další mutex zde
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].type != except_type && animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animation_stop(animations[i].id);
        }
    }
}


void animation_start(uint8_t animation_id)
{
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_start()");
        return;
    }
    
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
            
            xSemaphoreGive(animations_mutex);
            return;
        }
    }
    
    xSemaphoreGive(animations_mutex);
    ESP_LOGW(TAG, "Animation not found: ID=%d", animation_id);
}


void animation_stop(uint8_t animation_id)
{
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_stop()");
        return;
    }
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id) {
            // ✅ CRITICAL: Check state BEFORE changing it
            bool was_running = (animations[i].state == ANIM_TASK_STATE_RUNNING);
            
            // ✅ CRITICAL: For loop animations, set interrupted flag
            if (animations[i].loop && was_running) {
                if (animation_interrupt_mutex) {
                    xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
                    animation_interrupted = true;
                    xSemaphoreGive(animation_interrupt_mutex);
                }
            }
            
            animations[i].state = ANIM_TASK_STATE_FINISHED;
            
            // ✅ OPRAVA: Check was_running (checked BEFORE state change)
            if (was_running) {
                active_animation_count--;
            }
            
            ESP_LOGI(TAG, "Animation stopped: ID=%d, type=%d", animation_id, animations[i].type);
            
            xSemaphoreGive(animations_mutex);
            return;
        }
    }
    
    xSemaphoreGive(animations_mutex);
    ESP_LOGW(TAG, "Animation not found: ID=%d", animation_id);
}


void animation_pause(uint8_t animation_id)
{
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_pause()");
        return;
    }
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id && animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animations[i].state = ANIM_TASK_STATE_PAUSED;
            ESP_LOGI(TAG, "Animation paused: ID=%d", animation_id);
            xSemaphoreGive(animations_mutex);
            return;
        }
    }
    
    xSemaphoreGive(animations_mutex);
    ESP_LOGW(TAG, "Animation not found or not running: ID=%d", animation_id);
}


void animation_resume(uint8_t animation_id)
{
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_resume()");
        return;
    }
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].id == animation_id && animations[i].state == ANIM_TASK_STATE_PAUSED) {
            animations[i].state = ANIM_TASK_STATE_RUNNING;
            ESP_LOGI(TAG, "Animation resumed: ID=%d", animation_id);
            xSemaphoreGive(animations_mutex);
            return;
        }
    }
    
    xSemaphoreGive(animations_mutex);
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
    
    // ✅ CRITICAL: Check if animation should finish based on current_frame for frame-based animations
    // For frame-based animations, check current_frame >= total_frames
    // For time-based animations (duration_ms > 0), check elapsed >= duration_ms
    // For infinite animations (duration_ms = 0, loop = true), skip check
    
    bool should_finish = false;
    
    if (anim->duration_ms == 0 && anim->loop) {
        // Infinite loop animation - only check for interruption, not completion
        should_finish = false;
    } else if (anim->total_frames > 0) {
        // Frame-based animation - check current_frame
        should_finish = (anim->current_frame >= anim->total_frames);
    } else if (anim->duration_ms > 0) {
        // Time-based animation - check elapsed time
        should_finish = (elapsed >= anim->duration_ms);
    }
    
    if (should_finish) {
        if (anim->loop) {
            // ✅ CRITICAL: Check for interruption before restarting loop
            if (animation_interrupt_mutex) {
                xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
                bool should_interrupt = animation_interrupted;
                xSemaphoreGive(animation_interrupt_mutex);
                
                if (should_interrupt) {
                    ESP_LOGI(TAG, "🛑 Loop animation interrupted before restart");
                    anim->state = ANIM_TASK_STATE_FINISHED;
                    active_animation_count--;
                    animation_interrupted = false;  // Reset flag
                    return;
                }
            }
            
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
    
    // ✅ CRITICAL: For loop animations with duration_ms = 0 (infinite), check interruption periodically
    if (anim->loop && anim->duration_ms == 0) {
        if (animation_interrupt_mutex) {
            xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
            bool should_interrupt = animation_interrupted;
            xSemaphoreGive(animation_interrupt_mutex);
            
            if (should_interrupt) {
                ESP_LOGI(TAG, "🛑 Infinite loop animation interrupted");
                anim->state = ANIM_TASK_STATE_FINISHED;
                active_animation_count--;
                animation_interrupted = false;  // Reset flag
                return;
            }
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
        
        // NOVÉ: Herní animace
        case ANIM_TASK_TYPE_PLAYER_CHANGE:
            animation_execute_player_change(anim);
            break;
        
        case ANIM_TASK_TYPE_MOVE_PATH:
            animation_execute_move_path(anim);
            break;
        
        case ANIM_TASK_TYPE_CASTLE:
            animation_execute_castle(anim);
            break;
        
        case ANIM_TASK_TYPE_ENDGAME:
            animation_execute_endgame(anim);
            break;
        
        case ANIM_TASK_TYPE_CHECK:
            animation_execute_check(anim);
            break;
        
        case ANIM_TASK_TYPE_CHECKMATE:
            animation_execute_checkmate(anim);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown animation type: %d", anim->type);
            break;
    }
    
    anim->current_frame++;
}


// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Clear only board LEDs (0-63), preserve button LEDs (64-72)
 * 
 * This is a local helper function to avoid including led_task.h which causes conflicts.
 * Uses led_set_pixel_safe() from led_task_simple.h
 */
static void animation_clear_board_only(void)
{
    for (int i = 0; i < 64; i++) {
        led_set_pixel_safe(i, 0, 0, 0);
    }
}

// ============================================================================
// EXECUTE FUNCTIONS FOR GAME ANIMATIONS
// ============================================================================

void animation_execute_player_change(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    player_t current_player = anim->data_union.player_change.current_player;
    
    // REVERSED: Determine row lighting direction based on PREVIOUS player (passing scepter)
    // This creates the effect of passing the scepter TO the current player
    int start_row, end_row;
    if (current_player == PLAYER_WHITE) {
        // Passing scepter TO white: animate from black side (8) to white side (1)
        start_row = 7; // Start from black side
        end_row = 0;   // End at white side
    } else {
        // Passing scepter TO black: animate from white side (1) to black side (8)
        start_row = 0; // Start from white side
        end_row = 7;   // End at black side
    }
    
    // Professional wave animation with S-curve and Gaussian distribution
    const float wave_width = 2.5f; // Narrower wave width for more focused effect
    const int total_steps = 50;    // Same as original
    
    float wave_position = (float)start_row;
    float target_position = (float)end_row;
    
    // ✅ CRITICAL: Use current_frame instead of loop - NO vTaskDelay!
    float progress = (float)anim->current_frame / (float)(total_steps - 1);
    float eased_progress = 0.5f * (1.0f - cosf(progress * M_PI)); // S-curve using cosine
    
    float current_wave_pos = wave_position + (target_position - wave_position) * eased_progress;
    
    // Add gradual startup effect - gradually increase overall brightness at the beginning
    float startup_factor = 1.0f;
    if (anim->current_frame < 15) { // First 15 frames for gradual startup
        float startup_progress = (float)anim->current_frame / 15.0f; // 0.0 to 1.0
        startup_factor = 0.5f * (1.0f - cosf(startup_progress * M_PI)); // S-curve for smooth startup
    }
    
    // ✅ NOVÉ: NEČISTÍME board - každá animace renderuje pouze své LED
    // Ostatní animace mohou běžet současně (vyrenderují se podle priority)
    // Render wave with Gaussian brightness distribution
    for (int row = 0; row < 8; row++) {
        // Calculate distance from wave center
        float distance = fabsf((float)row - current_wave_pos);
        
        // Gaussian brightness distribution (exp(-distance²/(2σ²)))
        float gaussian_factor = expf(-(distance * distance) / (2.0f * wave_width * wave_width));
        
        // Apply startup factor for gradual brightness increase
        gaussian_factor *= startup_factor;
        
        // Apply brightness to entire row
        for (int col = 0; col < 8; col++) {
            uint8_t led_index = chess_pos_to_led_index(row, col);
            
            // Apply Gaussian brightness gradient with startup effect in dark gray
            // RGB(31, 31, 31) - dark gray color
            uint8_t red = (uint8_t)(31 * gaussian_factor);   // Dark gray red component
            uint8_t green = (uint8_t)(31 * gaussian_factor); // Dark gray green component
            uint8_t blue = (uint8_t)(31 * gaussian_factor);  // Dark gray blue component
            
            // Add brightness threshold - only show if bright enough to be visible
            if (gaussian_factor > 0.15f) { // Only show if > 15% brightness
                led_set_pixel_safe(led_index, red, green, blue);
            }
            // If too dim, leave LED off (don't set pixel)
        }
    }
}

void animation_execute_move_path(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    uint8_t from_row = anim->data_union.move_path.from_row;
    uint8_t from_col = anim->data_union.move_path.from_col;
    uint8_t to_row = anim->data_union.move_path.to_row;
    uint8_t to_col = anim->data_union.move_path.to_col;
    uint8_t to_led = anim->data_union.move_path.to_led;
    
    const int trail_frames = 25;  // Trail animation frames
    const int breath_frames = 8;  // Breath effect frames
    
    // ✅ CRITICAL: NO vTaskDelay! Use current_frame to determine phase
    if (anim->current_frame < trail_frames) {
        // Phase 1: Trail animation (frames 0-24)
        int frame = anim->current_frame;
        
        // ✅ NOVÉ: NEČISTÍME board - renderujeme pouze trail LED
        // Ostatní animace mohou běžet současně
        float progress = (float)frame / 24.0f;
        
        // Create enhanced trail effect with multiple brightness levels (6 trails)
        for (int trail = 0; trail < 6; trail++) {
            float trail_progress = progress - (trail * 0.08f);
            if (trail_progress < 0)
                continue;
            if (trail_progress > 1)
                break;
            
            // Calculate current position with smooth easing
            float eased_progress = trail_progress * trail_progress *
                                 (3.0f - 2.0f * trail_progress); // Smooth step
            float current_row = from_row + (to_row - from_row) * eased_progress;
            float current_col = from_col + (to_col - from_col) * eased_progress;
            
            uint8_t current_led = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
            
            // Modrá barva s brightness gradientem podle trail_progress
            float blue_intensity = 1.0f;
            if (trail_progress < 0.2f) {
                // Začátek: tmavší modrá
                blue_intensity = 0.5f + (trail_progress / 0.2f) * 0.5f; // 0.5 -> 1.0
            } else if (trail_progress > 0.8f) {
                // Konec: jasnější modrá
                blue_intensity = 1.0f;
            }
            
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = (uint8_t)(255 * blue_intensity);
            
            // Enhanced trail brightness with exponential fade
            float trail_brightness = powf(1.0f - (trail * 0.15f), 1.5f);
            
            // Advanced pulsing with multiple harmonics
            float pulse1 = 0.6f + 0.4f * sinf(progress * 12.56f + trail * 1.26f);
            float pulse2 = 0.8f + 0.2f * sinf(progress * 25.12f + trail * 2.51f);
            float pulse3 = 0.9f + 0.1f * sinf(progress * 50.24f + trail * 3.77f);
            float combined_pulse = pulse1 * pulse2 * pulse3;
            
            // Apply brightness and pulsing
            red = (uint8_t)(red * trail_brightness * combined_pulse);
            green = (uint8_t)(green * trail_brightness * combined_pulse);
            blue = (uint8_t)(blue * trail_brightness * combined_pulse);
            
            led_set_pixel_safe(current_led, red, green, blue);
        }
    } else {
        // Phase 2: Final destination breathing effect (frames 25-32)
        int breath_frame = anim->current_frame - trail_frames;
        
        // ✅ NOVÉ: NEČISTÍME board - renderujeme pouze final destination LED
        // Ostatní animace mohou běžet současně
        float breath_intensity = 0.5f + 0.5f * sinf(breath_frame * 0.785f); // Breathing effect
        uint8_t final_red = 0;
        uint8_t final_green = 0;
        uint8_t final_blue = (uint8_t)(255 * breath_intensity);
        
        led_set_pixel_safe(to_led, final_red, final_green, final_blue);
    }
}

void animation_execute_castle(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    uint8_t king_from_led = anim->data_union.castle.king_from_led;
    uint8_t king_to_led = anim->data_union.castle.king_to_led;
    uint8_t rook_from_led = anim->data_union.castle.rook_from_led;
    uint8_t rook_to_led = anim->data_union.castle.rook_to_led;
    
    // Validate LED indices
    if (king_from_led >= 64 || king_to_led >= 64 || rook_from_led >= 64 || rook_to_led >= 64) {
        ESP_LOGE(TAG, "❌ Invalid LED indices in castle animation: king_from=%d, king_to=%d, rook_from=%d, rook_to=%d",
                 king_from_led, king_to_led, rook_from_led, rook_to_led);
        return;
    }
    
    const int trail_frames = 15;  // Trail animation frames
    const int burst_frames = 3;   // Final highlight frames
    
    // ✅ CRITICAL: NO vTaskDelay! Use current_frame to determine phase
    if (anim->current_frame < trail_frames) {
        // Phase 1: Trail animation (frames 0-14)
        int frame = anim->current_frame;
        
        // ✅ NOVÉ: NEČISTÍME board - renderujeme pouze castle trail LED
        // Ostatní animace mohou běžet současně
        float progress = (float)frame / 14.0f;
        
        // Create trail effect for both pieces
        for (int trail = 0; trail < 4; trail++) {
            float trail_progress = progress - (trail * 0.15f);
            if (trail_progress < 0)
                continue;
            if (trail_progress > 1)
                break;
            
            // Smooth easing
            float eased_progress = trail_progress * trail_progress * (3.0f - 2.0f * trail_progress);
            
            // Calculate current positions (linear interpolation between LED indices)
            // Note: This assumes LED indices are linear, which may not be true for serpentine layout
            // But for castling moves (horizontal only), this should work
            uint8_t king_current = king_from_led + (uint8_t)((king_to_led - king_from_led) * eased_progress);
            uint8_t rook_current = rook_from_led + (uint8_t)((rook_to_led - rook_from_led) * eased_progress);
            
            // Ensure valid indices
            if (king_current >= 64) king_current = 63;
            if (rook_current >= 64) rook_current = 63;
            
            // King color: Gold with pulsing effect
            float king_pulse = 0.8f + 0.2f * sinf(progress * 6.28f + trail * 1.57f);
            uint8_t king_red = (uint8_t)(255 * king_pulse);
            uint8_t king_green = (uint8_t)(215 * king_pulse);
            uint8_t king_blue = 0;
            
            // Rook color: Silver with pulsing effect
            float rook_pulse = 0.7f + 0.3f * sinf(progress * 6.28f + trail * 2.09f);
            uint8_t rook_red = (uint8_t)(192 * rook_pulse);
            uint8_t rook_green = (uint8_t)(192 * rook_pulse);
            uint8_t rook_blue = (uint8_t)(192 * rook_pulse);
            
            // Trail brightness effect
            float trail_brightness = 1.0f - (trail * 0.2f);
            king_red = (uint8_t)(king_red * trail_brightness);
            king_green = (uint8_t)(king_green * trail_brightness);
            rook_red = (uint8_t)(rook_red * trail_brightness);
            rook_green = (uint8_t)(rook_green * trail_brightness);
            rook_blue = (uint8_t)(rook_blue * trail_brightness);
            
            led_set_pixel_safe(king_current, king_red, king_green, king_blue);
            led_set_pixel_safe(rook_current, rook_red, rook_green, rook_blue);
        }
    } else {
        // Phase 2: Final highlight on destination squares (frames 15-17)
        int burst_frame = anim->current_frame - trail_frames;
        
        // ✅ NOVÉ: NEČISTÍME board - renderujeme pouze destination LED
        // Ostatní animace mohou běžet současně
        float brightness = 0.5f + 0.5f * sinf(burst_frame * 2.09f);
        led_set_pixel_safe(king_to_led, (uint8_t)(255 * brightness),
                          (uint8_t)(215 * brightness), 0);
        led_set_pixel_safe(rook_to_led, (uint8_t)(192 * brightness),
                          (uint8_t)(192 * brightness),
                          (uint8_t)(192 * brightness));
    }
}

void animation_execute_check(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    // Check animation: 6 flashes = 12 frames total (1 frame = on, 1 frame = off)
    // Frame % 2 determines on/off state (0,2,4... = on, 1,3,5... = off)
    bool is_on = (anim->current_frame % 2 == 0);
    
    animation_clear_board_only();
    
    if (is_on) {
        // Flash red warning
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 0, 0); // Red
        }
    }
    // else: board is already cleared
}

void animation_execute_checkmate(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    // Checkmate animation: 8 flashes = 16 frames total (alternating red/white)
    // Frame % 4 determines pattern: 0,4,8... = red, 2,6,10... = white, odd = off
    int pattern = anim->current_frame % 4;
    
    animation_clear_board_only();
    
    if (pattern == 0) {
        // Flash red
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 0, 0); // Red
        }
    } else if (pattern == 2) {
        // Flash white
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 255, 255); // White
        }
    }
    // pattern 1 or 3: board is already cleared
}

void animation_execute_endgame(animation_task_t* anim)
{
    if (!anim || anim->state != ANIM_TASK_STATE_RUNNING) {
        return;
    }
    
    // Endgame animation is time-based (100ms interval), not frame-based
    const uint32_t WAVE_STEP_MS = 100;
    const uint8_t MAX_RADIUS = 14;
    const float WAVE_THICKNESS = 1.2f;
    const int WAVE_LAYERS = 4;
    
    uint32_t current_time = esp_timer_get_time() / 1000; // Current time in ms
    uint32_t last_radius_update = anim->data_union.endgame.last_radius_update;
    
    // Check if it's time for next wave step (time-based, not frame-based)
    if (current_time - last_radius_update < WAVE_STEP_MS) {
        return; // Not time yet - render current state but don't update radius
    }
    
    // Update radius and time
    anim->data_union.endgame.radius++;
    if (anim->data_union.endgame.radius > MAX_RADIUS) {
        anim->data_union.endgame.radius = 1; // Reset for continuous wave effect
    }
    anim->data_union.endgame.last_radius_update = current_time;
    
    uint8_t radius = anim->data_union.endgame.radius;
    uint8_t king_row = anim->data_union.endgame.king_row;
    uint8_t king_col = anim->data_union.endgame.king_col;
    piece_t winner_piece = anim->data_union.endgame.winner_piece;
    bool winner_is_white = (winner_piece == PIECE_WHITE_KING);
    
    // ✅ NOVÉ: NEČISTÍME board - renderujeme pouze endgame wave LED
    // Ostatní animace mohou běžet současně (ale endgame má nejvyšší prioritu)
    // Draw multiple overlapping wave rings for ultra-smooth effect
    for (int ring = 0; ring < WAVE_LAYERS; ring++) {
        float current_radius = radius - (ring * 0.3f);
        if (current_radius < 0.2f)
            continue;
        
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                // Calculate distance from center
                float dist = sqrtf(dx * dx + dy * dy);
                
                // Check if this pixel is part of the wave ring with smooth gradient
                float ring_distance = fabsf(dist - current_radius);
                if (ring_distance <= WAVE_THICKNESS) {
                    int row = king_row + dy;
                    int col = king_col + dx;
                    
                    // Check bounds
                    if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                        uint8_t square = chess_pos_to_led_index(row, col);
                        
                        // Get piece at this position (if available)
                        piece_t piece = PIECE_EMPTY;
                        // ✅ Note: game_get_piece() may cause dependency, but it's necessary for proper coloring
                        // If this causes circular dependency issues, we can use winner_piece to infer colors
                        piece = game_get_piece(row, col);
                        
                        // Calculate intensity based on distance from ring center (smooth gradient)
                        float intensity = 1.0f - (ring_distance / WAVE_THICKNESS);
                        intensity = fmaxf(0.15f, intensity); // Higher minimum brightness
                        
                        uint8_t red, green, blue;
                        
                        if (piece != PIECE_EMPTY) {
                            // Check if it's an opponent piece - use direct piece comparison for reliability
                            bool is_opponent_piece = false;
                            
                            if (winner_is_white) {
                                // Winner is white, check if piece is black
                                is_opponent_piece = (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
                            } else {
                                // Winner is black, check if piece is white
                                is_opponent_piece = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
                            }
                            
                            if (is_opponent_piece) {
                                // BRIGHT RED for opponent pieces - very visible!
                                red = (uint8_t)(255 * intensity);
                                green = (uint8_t)(30 * intensity);
                                blue = (uint8_t)(30 * intensity);
                            } else {
                                // BRIGHT GREEN for own pieces - high contrast
                                red = (uint8_t)(30 * intensity);
                                green = (uint8_t)(255 * intensity);
                                blue = (uint8_t)(80 * intensity);
                            }
                        } else {
                            // BRIGHT BLUE for empty squares - very visible
                            red = (uint8_t)(30 * intensity);
                            green = (uint8_t)(100 * intensity);
                            blue = (uint8_t)(255 * intensity);
                        }
                        
                        led_set_pixel_safe(square, red, green, blue);
                    }
                }
            }
        }
    }
    
    // Always highlight winner king in BRIGHT GOLD
    led_set_pixel_safe(anim->data_union.endgame.king_led, 255, 215, 0);
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
    // ✅ NOVÉ: Mutex ochrana pro animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_stop_all()");
        return;
    }
    
    ESP_LOGI(TAG, "Stopping all animations (including loop animations)");
    
    // ✅ CRITICAL: Set interrupted flag for all running animations (especially loop ones)
    if (animation_interrupt_mutex) {
        xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
        animation_interrupted = true;
        xSemaphoreGive(animation_interrupt_mutex);
    }
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        if (animations[i].state == ANIM_TASK_STATE_RUNNING || 
            animations[i].state == ANIM_TASK_STATE_PAUSED) {
            animations[i].state = ANIM_TASK_STATE_FINISHED;
        }
    }
    
    active_animation_count = 0;
    
    // Reset interrupted flag after stopping all
    if (animation_interrupt_mutex) {
        xSemaphoreTake(animation_interrupt_mutex, portMAX_DELAY);
        animation_interrupted = false;
        xSemaphoreGive(animation_interrupt_mutex);
    }
    
    xSemaphoreGive(animations_mutex);
}


void animation_pause_all(void)
{
    // ✅ NOVÉ: Mutex ochrana - animation_pause() má vlastní mutex, ale iterace musí být chráněna
    // Ale pozor: animation_pause() si vezme mutex uvnitř, takže může dojít k deadlocku!
    // Lepší: iterovat bez mutexu (animation_pause() si vezme mutex uvnitř)
    ESP_LOGI(TAG, "Pausing all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        // animation_pause() má vlastní mutex ochranu, takže můžeme volat bez dalšího mutexu
        // Ale musíme číst state bez mutexu - může dojít k race condition
        // Řešení: animation_pause() zkontroluje state uvnitř, takže je to OK
        if (animations[i].state == ANIM_TASK_STATE_RUNNING) {
            animation_pause(animations[i].id);
        }
    }
}


void animation_resume_all(void)
{
    // ✅ NOVÉ: Mutex ochrana - animation_resume() má vlastní mutex, ale iterace musí být chráněna
    // Poznámka: animation_resume() si vezme mutex uvnitř, takže iterace bez mutexu je OK
    ESP_LOGI(TAG, "Resuming all animations");
    
    for (int i = 0; i < MAX_ANIMATIONS; i++) {
        // animation_resume() má vlastní mutex ochranu, takže můžeme volat bez dalšího mutexu
        if (animations[i].state == ANIM_TASK_STATE_PAUSED) {
            animation_resume(animations[i].id);
        }
    }
}


void animation_print_status(void)
{
    // ✅ NOVÉ: Mutex ochrana pro čtení animations pole
    if (animations_mutex == NULL) {
        ESP_LOGE(TAG, "Animations mutex not initialized!");
        return;
    }
    
    if (xSemaphoreTake(animations_mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take animations mutex in animation_print_status()");
        return;
    }
    
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
    
    xSemaphoreGive(animations_mutex);
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
        
        // NOVÉ: Herní animace
        case ANIM_TASK_TYPE_PLAYER_CHANGE: return "Player change";
        case ANIM_TASK_TYPE_MOVE_PATH: return "Move path";
        case ANIM_TASK_TYPE_CASTLE: return "Castle";
        case ANIM_TASK_TYPE_PROMOTE: return "Promote";
        case ANIM_TASK_TYPE_ENDGAME: return "Endgame wave";
        case ANIM_TASK_TYPE_CHECK: return "Check";
        case ANIM_TASK_TYPE_CHECKMATE: return "Checkmate";
        
        // DEPRECATED (pro zpětnou kompatibilitu)
        case ANIM_TASK_TYPE_MOVE_HIGHLIGHT: return "Move path highlight (DEPRECATED)";
        case ANIM_TASK_TYPE_CHECK_HIGHLIGHT: return "Check indicator (DEPRECATED)";
        case ANIM_TASK_TYPE_GAME_OVER: return "Game over pattern (DEPRECATED)";
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
        
        // ✅ NOVÉ: Mutex ochrana pro animations pole při iteraci
        if (animations_mutex != NULL) {
            if (xSemaphoreTake(animations_mutex, portMAX_DELAY) == pdTRUE) {
                // ✅ NOVÉ: Seřadit animace podle priority (vyšší první)
                // Vytvořit pole indexů běžících animací
                uint8_t running_indices[MAX_ANIMATIONS];
                uint8_t running_count = 0;
                
                for (int i = 0; i < MAX_ANIMATIONS; i++) {
                    if (animations[i].state == ANIM_TASK_STATE_RUNNING) {
                        running_indices[running_count++] = i;
                    }
                }
                
                // Bubble sort podle priority (descending - vyšší priority první)
                for (int i = 0; i < running_count - 1; i++) {
                    for (int j = 0; j < running_count - i - 1; j++) {
                        if (animations[running_indices[j]].priority < animations[running_indices[j + 1]].priority) {
                            // Swap indices
                            uint8_t temp = running_indices[j];
                            running_indices[j] = running_indices[j + 1];
                            running_indices[j + 1] = temp;
                        }
                    }
                }
                
                // ✅ NOVÉ: Clear board POUZE pokud jsou CHECK/CHECKMATE animace (blikají všemi LED)
                // Zkontrolovat, jestli existuje CHECK nebo CHECKMATE animace
                bool has_check_or_checkmate = false;
                for (int i = 0; i < running_count; i++) {
                    uint8_t idx = running_indices[i];
                    if (animations[idx].type == ANIM_TASK_TYPE_CHECK ||
                        animations[idx].type == ANIM_TASK_TYPE_CHECKMATE) {
                        has_check_or_checkmate = true;
                        break;
                    }
                }
                
                // Pokud jsou CHECK/CHECKMATE animace, clear board na začátku
                // Jinak necháme animace se překrývat podle priority
                if (has_check_or_checkmate) {
                    animation_clear_board_only();
                }
                
                // Execute active animations in priority order (highest first)
                // Animace s vyšší prioritou se vyrenderuje později a přepíše nižší
                // (použijeme "last write wins" logiku)
                for (int i = 0; i < running_count; i++) {
                    uint8_t idx = running_indices[i];
                    // Note: animation_execute_frame() modifies anim->current_frame,
                    // but doesn't change animations[] array structure, so it's safe
                    animation_execute_frame(&animations[idx]);
                }
                
                // ✅ NOVÉ: Pokud nejsou žádné animace, clear board
                if (running_count == 0) {
                    animation_clear_board_only();
                }
                
                xSemaphoreGive(animations_mutex);
            } else {
                ESP_LOGE(TAG, "Failed to take animations mutex in main loop");
            }
        } else {
            // Fallback if mutex not initialized (shouldn't happen)
            for (int i = 0; i < MAX_ANIMATIONS; i++) {
                if (animations[i].state == ANIM_TASK_STATE_RUNNING) {
                    animation_execute_frame(&animations[i]);
                }
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