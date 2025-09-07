/**
 * @file led_task.c
 * @brief ESP32-C6 Chess System v2.4 - LED Task Implementation
 * 
 * This task handles all LED operations:
 * - WS2812B LED control (73 LEDs: 64 board + 9 buttons: 8 promotion + 1 reset)
 * - LED animations and patterns
 * - Button LED feedback
 * - Time-multiplexed updates
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 * 
 * Hardware:
 * - WS2812B LED strip on GPIO7 (LED_DATA_PIN)
 * - 64 LEDs for chess board (8x8)
 * - 9 LEDs for button feedback (8 promotion + 1 reset)
 * - Simulation mode (no actual hardware required)
 */


#include "led_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "../unified_animation_manager/include/unified_animation_manager.h"  // Non-blocking endgame animations

// Define min macro if not available
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include "freertos_chess.h"


static const char *TAG = "LED_TASK";

// ============================================================================
// LED SYSTEM OPTIMIZATION CONSTANTS - NOVÉ PRO KOMPLETNÍ OPRAVU
// ============================================================================

// WS2812B optimal timing constants
#define LED_CRITICAL_SECTION_TIMEOUT_MS  50     // ✅ Critical section timeout
#define LED_TASK_MUTEX_TIMEOUT_MS        100                 // ✅ Mutex timeout in milliseconds
#define LED_TASK_MUTEX_TIMEOUT_TICKS     (LED_TASK_MUTEX_TIMEOUT_MS / portTICK_PERIOD_MS)  // ✅ Convert to ticks
#define LED_FRAME_BUFFER_SIZE            2      // ✅ Double buffering
#define LED_MAX_RETRY_COUNT              3      // ✅ Max retry attempts
#define LED_ERROR_RECOVERY_THRESHOLD     10     // ✅ Error recovery threshold
#define LED_HEALTH_CHECK_INTERVAL_MS     5000   // ✅ Health check interval
#define LED_BATCH_COMMIT_INTERVAL_MS     50     // ✅ Batch commit interval for optimal performance
#define LED_WATCHDOG_RESET_INTERVAL      10     // ✅ Reset watchdog every N LEDs during batch update

// ============================================================================
// LED DURATION MANAGEMENT SYSTEM - NOVÝ PRO ŘEŠENÍ DURATION PROBLÉMU
// ============================================================================

// Duration tracking structure
typedef struct {
    uint8_t led_index;
    uint32_t original_color;     // Barva před duration
    uint32_t duration_color;     // Barva během duration  
    uint32_t start_time;         // Začátek v ms
    uint32_t duration_ms;        // Doba trvání
    bool is_active;              // Aktivní duration?
    bool restore_original;       // Obnovit původní barvu?
} led_duration_state_t;

// ============================================================================
// LED FRAME SYNCHRONIZATION SYSTEM - NOVÝ PRO STABILITU
// ============================================================================

// Frame buffer structure for double buffering
typedef struct {
    uint32_t frame_id;
    uint32_t timestamp;
    bool is_complete_frame;
    uint8_t led_count;
    uint32_t pixels[CHESS_LED_COUNT_TOTAL];
} led_frame_buffer_t;

// ============================================================================
// LED HEALTH MONITORING SYSTEM - NOVÝ PRO DIAGNOSTIKU
// ============================================================================

typedef struct {
    uint32_t commands_processed;
    uint32_t commands_failed;
    uint32_t mutex_timeouts;
    uint32_t frame_drops;
    uint32_t hardware_errors;
    uint32_t last_update_time;
    float success_rate;
    uint32_t average_frame_time_ms;
    uint32_t min_frame_time_ms;
    uint32_t max_frame_time_ms;
} led_health_stats_t;

// Hardware configuration
#define LED_DATA_PIN GPIO_NUM_7

// Forward declarations
static void led_send_status_to_uart_immediate(QueueHandle_t response_queue);
void led_execute_command_new(const led_command_t* cmd);
static esp_err_t led_hardware_init(void);
// static void led_hardware_update(void);  // REMOVED: Obsolete with immediate refresh
static void led_hardware_cleanup(void);

// ✅ BATCH UPDATE FUNCTIONS
static void led_commit_pending_changes(void);         // ✅ Commit all pending changes atomically
void led_force_immediate_update(void);        // ✅ Force immediate update for critical operations
static void led_privileged_batch_commit(void);       // ✅ Optimized batch commit with privileged mutex sync

// LED layer management functions
void led_clear_board_only(void);           // Clear only board LEDs (0-63)
void led_clear_buttons_only(void);         // Clear only button LEDs (64-72)
void led_preserve_buttons(void);           // Preserve button states during operations

// ✅ NOVÝ: Duration management functions
static void led_set_pixel_with_duration(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);
static void led_duration_timer_callback(TimerHandle_t xTimer);
static void led_init_duration_system(void);

// Helper function to get ANSI color code from RGB
static const char* get_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);

// Button LED logic functions
static void led_update_button_led_state(uint8_t button_id);
static void led_set_button_pressed(uint8_t button_id, bool pressed);
static void led_process_button_blink_timers(void);

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// LED state storage (73 LEDs total)
static uint32_t led_states[CHESS_LED_COUNT_TOTAL] = {0}; // RGB values for each LED - initialized to 0

// Button LED state tracking
static bool button_pressed[CHESS_BUTTON_COUNT] = {false}; // Button press state
static bool button_available[CHESS_BUTTON_COUNT] = {false, false, false, false, false, false, false, false, true}; // ✅ FIX: Only reset button (index 8) available by default
static uint32_t button_release_time[CHESS_BUTTON_COUNT] = {0}; // Release time for blink
static bool button_blinking[CHESS_BUTTON_COUNT] = {false}; // Blink state

// Animation state
static bool animation_active = false;
static uint32_t animation_start_time = 0;
static uint32_t animation_duration = 0;
static uint8_t animation_pattern = 0;

// Task state
static bool task_running = false;
static bool matrix_scanning_enabled = true;  // Component control
static bool led_component_enabled = true;    // LED component enabled by default
static bool simulation_mode = false; // Changed to false for real hardware operation

// ✅ LED STRIP HARDWARE STATE - Using official driver
static led_strip_handle_t led_strip = NULL;
static bool led_initialized = false;

// Hardware update throttling - OBSOLETE (keeping for compatibility)
// static uint32_t last_hardware_update = 0;  // REMOVED: No longer needed with immediate refresh
// static const uint32_t HARDWARE_UPDATE_INTERVAL_MS = LED_HARDWARE_UPDATE_MS;  // REMOVED: No longer needed

// LED synchronization - BATCH UPDATE SYSTEM
static SemaphoreHandle_t led_unified_mutex = NULL;  // ✅ Queue synchronization only

// ✅ BATCH UPDATE SYSTEM - Collect changes, then commit atomically
static bool led_changes_pending = false;           // ✅ Flag indicating pending changes
static uint32_t led_pending_changes[CHESS_LED_COUNT_TOTAL]; // ✅ Pending color changes
static bool led_changed_flags[CHESS_LED_COUNT_TOTAL];       // ✅ Track which LEDs actually changed

// ✅ NOVÝ: Duration management system
static led_duration_state_t led_durations[CHESS_LED_COUNT_TOTAL] = {0};
static bool led_duration_system_enabled = true;
static TimerHandle_t led_duration_timer = NULL;

// LED patterns
static const uint32_t chess_board_pattern[64] = {
    // White squares (even indices)
    0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF,
    0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000,
    0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF
};

// Button LED colors (kept for reference, not used in new logic)
// static const uint32_t button_colors[9] = {
//     0xFF00FF, // Queen A - Magenta
//     0xFF0000, // Rook A - Red
//     0x00FFFF, // Bishop A - Cyan
//     0xFF8000, // Knight A - Orange
//     0xFF00FF, // Queen B - Magenta
//     0xFF0000, // Rook B - Red
//     0x00FFFF, // Bishop B - Cyan
//     0xFF8000, // Knight B - Orange
//     0xFF0000  // Reset - Red
// };

// Test pattern colors
static const uint32_t test_pattern_colors[] = {
    0xFF0000, // Red
    0x00FF00, // Green
    0x0000FF, // Blue
    0xFFFF00, // Yellow
    0xFF00FF, // Magenta
    0x00FFFF, // Cyan
    0xFFFFFF, // White
    0xFF8000  // Orange
};

// ============================================================================
// SMART LED REPORTING SYSTEM
// ============================================================================

// LED state tracking for smart reporting
static uint32_t previous_led_states[CHESS_LED_COUNT_TOTAL]; // Previous LED states
static bool led_states_initialized = false;
static uint32_t last_status_report_time = 0;
static uint32_t led_changes_count = 0;
static bool quiet_period_active = false;
static uint32_t quiet_period_start = 0;
static uint32_t quiet_period_duration = 5000; // 5 seconds quiet after board operations

// LED grouping for compact display
static const char* color_names[] = {
    "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Yellow", "White"
};

// Button names for LED display
static const char* button_names[] = {
    "Q", "R", "B", "N", "Q", "R", "B", "N", "R"  // Queen, Rook, Bishop, Knight (4 for each player) + Reset
};


// ============================================================================
// LED CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Map button ID to correct LED index
 * @param button_id Button ID (0-8)
 * @return LED index (64-72)
 */
static uint8_t led_get_button_led_index(uint8_t button_id)
{
    if (button_id <= 7) {
        return 64 + button_id;  // Promotion buttons: 64-71 (8 buttons total)
    } else if (button_id == 8) {
        return 72;              // Reset button: 72
    } else {
        return 64 + button_id;  // Fallback (should not happen)
    }
}

void led_set_pixel_internal(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_index >= CHESS_LED_COUNT_TOTAL) {
        ESP_LOGE(TAG, "Invalid LED index: %d (max: %d)", led_index, CHESS_LED_COUNT_TOTAL - 1);
        return;
    }
    
    // Check if LED component is enabled
    if (!led_component_enabled) {
        ESP_LOGD(TAG, "LED component disabled - ignoring LED command for LED %d", led_index);
        return;
    }
    
    // If matrix scanning is disabled and this is a board LED (0-63), limit animations
    if (!matrix_scanning_enabled && led_index < 64) {
        // Only allow static colors, no animations
        if (red > 0 || green > 0 || blue > 0) {
            ESP_LOGD(TAG, "Matrix scanning disabled - limiting LED animation for board LED %d", led_index);
        }
    }
    
    // ✅ OPRAVA: Unified mutex protection
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take LED unified mutex - skipping LED operation");
            return;
        }
    }
    
    // ✅ BATCH UPDATE SYSTEM - Just collect change, don't commit immediately  
    uint32_t color = (red << 16) | (green << 8) | blue;
    
    // Update internal state
    led_states[led_index] = color;
    
    // Mark change as pending for next batch commit
    if (led_initialized && led_strip != NULL && !simulation_mode) {
        led_pending_changes[led_index] = color;
        led_changed_flags[led_index] = true;  // ✅ Mark this LED as changed
        led_changes_pending = true;
    }
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "LED[%d] = RGB(%d,%d,%d) = 0x%06X", led_index, red, green, blue, led_states[led_index]);
    }
    
    // ✅ OPRAVA: Release unified mutex
    if (led_unified_mutex != NULL) {
        xSemaphoreGive(led_unified_mutex);
    }
}


void led_set_all_internal(uint8_t red, uint8_t green, uint8_t blue)
{
    uint32_t color = (red << 16) | (green << 8) | blue;
    
    // ✅ BATCH UPDATE - use batch system instead of immediate hardware access
    if (led_initialized && led_strip && !simulation_mode) {
        // Mark all LEDs as changed for batch commit
        for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
            led_pending_changes[i] = color;
            led_changed_flags[i] = true;
        }
        led_changes_pending = true;
        
        // Force immediate commit for this operation
        led_commit_pending_changes();
    }
    
    // Update internal states
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        led_states[i] = color;
    }
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "All LEDs = RGB(%d,%d,%d) = 0x%06X", red, green, blue, color);
    }
}


void led_clear_all_internal(void)
{
    ESP_LOGI(TAG, "🔄 Clearing all LED states...");
    
    // ✅ SIMPLE CLEAR OPERATION - let driver handle timing
    if (led_initialized && led_strip != NULL && !simulation_mode) {
        esp_err_t ret = led_strip_clear(led_strip);
        if (ret == ESP_OK) {
            ret = led_strip_refresh(led_strip);  // ✅ Let driver handle timing
        }
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "❌ LED strip clear failed: %s", esp_err_to_name(ret));
        }
    }
    
    // Update internal states
    memset(led_states, 0, sizeof(led_states));
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "✅ All LEDs cleared");
    }
}


void led_show_chess_board(void)
{
    ESP_LOGI(TAG, "🔄 Setting chess board pattern...");
    
    // Apply chess board pattern to board LEDs (0-63)
    for (int i = 0; i < 64; i++) {
        led_states[i] = chess_board_pattern[i];
    }
    ESP_LOGI(TAG, "✅ Board LEDs pattern set");
    
    // Initialize button LED states and set to default colors
    // 8 promotion buttons (0-7) + 1 reset button (8) = 9 total
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        button_pressed[i] = false;
        button_available[i] = (i == 8); // Only reset button (8) available by default
        button_release_time[i] = 0;
        button_blinking[i] = false;
        
        // Set button LED color based on availability
        uint8_t led_index = led_get_button_led_index(i);
        if (button_available[i]) {
            led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
        } else {
            led_set_pixel_internal(led_index, 0, 0, 255); // Blue - unavailable
        }
    }
    ESP_LOGI(TAG, "✅ Button LEDs initialized - reset button (LED 72) green, promotion buttons (LED 64-71) blue");
    
    // Force immediate update to ensure button LEDs are visible
    led_force_immediate_update();
    
    // ✅ OPRAVA: Šachovnice už je nastavena výše, jen potvrdit
    ESP_LOGI(TAG, "✅ Chess board pattern displayed - button LEDs preserved");
    
    // ✅ CRITICAL: Force another update to ensure button LEDs are visible immediately
    led_force_immediate_update();
    
    // Debug button LEDs after initialization
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        uint8_t led_index = led_get_button_led_index(i);
        ESP_LOGI(TAG, "Button %d (LED %d) after final init: 0x%06X", i, led_index, led_states[led_index]);
    }
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "Chess board pattern displayed");
        ESP_LOGI(TAG, "  - 64 board LEDs: alternating black/white pattern");
        ESP_LOGI(TAG, "  - 9 button LEDs: 8 promotion (64-71) + 1 reset (72)");
    }
    
    // Start quiet period to reduce verbose LED status reports
    led_start_quiet_period(5000); // 5 seconds quiet after board setup
    
    ESP_LOGI(TAG, "🎉 Chess board pattern complete!");
}


void led_set_button_feedback(uint8_t button_id, bool available)
{
    if (button_id >= CHESS_BUTTON_COUNT) {
        ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id, CHESS_BUTTON_COUNT - 1);
        return;
    }
    
    // Update button availability state
    button_available[button_id] = available;
    
    // Update LED state based on new availability
    led_update_button_led_state(button_id);
    
        if (simulation_mode) {
        ESP_LOGI(TAG, "Button %d LED: %s", button_id, available ? "Available (green)" : "Not available (blue)");
    }
}


void led_set_button_press(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) {
        ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id, CHESS_BUTTON_COUNT - 1);
        return;
    }
    
    // Set button as pressed
    button_pressed[button_id] = true;
    button_blinking[button_id] = false; // Stop any blinking
    
    // Update LED to red (pressed state)
    led_update_button_led_state(button_id);
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "Button %d LED: Press feedback (red)", button_id);
    }
}


void led_set_button_release(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) {
        ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id, CHESS_BUTTON_COUNT - 1);
        return;
    }
    
    // Set button as released
    button_pressed[button_id] = false;
    button_release_time[button_id] = esp_timer_get_time() / 1000;
    button_blinking[button_id] = true; // Start blinking
    
    // Update LED state
    led_update_button_led_state(button_id);
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "Button %d LED: Release feedback (return to availability state)", button_id);
    }
}


uint32_t led_get_button_color(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) {
        ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id, CHESS_BUTTON_COUNT - 1);
        return 0;
    }
    
    // ✅ OPRAVA: Správné mapování button ID na LED indexy
    uint8_t led_index = led_get_button_led_index(button_id);
    return led_states[led_index];
}

/**
 * @brief Get LED state for a specific LED index
 * @param led_index LED index (0-72)
 * @return RGB color value (0xRRGGBB)
 */
uint32_t led_get_led_state(uint8_t led_index)
{
    if (led_index >= CHESS_LED_COUNT_TOTAL) {
        ESP_LOGE(TAG, "Invalid LED index: %d (max: %d)", led_index, CHESS_LED_COUNT_TOTAL - 1);
        return 0;
    }
    
    // ✅ OPRAVA: Unified mutex protection
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            uint32_t state = led_states[led_index];
            xSemaphoreGive(led_unified_mutex);
            return state;
        } else {
            ESP_LOGW(TAG, "Failed to acquire LED unified mutex for get state");
            return 0;
        }
    } else {
        // Fallback if mutex not available
        return led_states[led_index];
    }
}

/**
 * @brief Get all LED states
 * @param states Array to store LED states
 * @param max_count Maximum number of states to copy
 */
void led_get_all_states(uint32_t* states, size_t max_count)
{
    if (states == NULL) {
        ESP_LOGE(TAG, "Invalid states array");
        return;
    }
    
    size_t count = (max_count < CHESS_LED_COUNT_TOTAL) ? max_count : CHESS_LED_COUNT_TOTAL;
    
    // ✅ OPRAVA: Unified mutex protection
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            memcpy(states, led_states, count * sizeof(uint32_t));
            xSemaphoreGive(led_unified_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire LED unified mutex for get all states");
            memset(states, 0, count * sizeof(uint32_t));
        }
    } else {
        // Fallback if mutex not available
        memcpy(states, led_states, count * sizeof(uint32_t));
    }
}


void led_start_animation(uint32_t duration_ms)
{
    animation_active = true;
    animation_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
    animation_duration = duration_ms;
    animation_pattern = 0;
    
    if (simulation_mode) {
        ESP_LOGI(TAG, "Animation started: duration=%dms", duration_ms);
    }
}


void led_test_pattern(void)
{
    ESP_LOGI(TAG, "=== LED Test Pattern ===");
    
    // Test all LEDs with different colors
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        uint32_t color = test_pattern_colors[i % 8];
        uint8_t red = (color >> 16) & 0xFF;
        uint8_t green = (color >> 8) & 0xFF;
        uint8_t blue = color & 0xFF;
        
        led_set_pixel_internal(i, red, green, blue);
        vTaskDelay(pdMS_TO_TICKS(50)); // 50ms delay between LEDs for better performance
    }
    
    ESP_LOGI(TAG, "=== LED Test Pattern Complete ===");
}

void led_test_all_pattern(void)
{
    ESP_LOGI(TAG, "=== LED Test All Pattern ===");
    
    // Test all LEDs with cycling colors
    uint32_t test_colors[] = {
        0xFF0000, // Red
        0x00FF00, // Green
        0x0000FF, // Blue
        0xFFFF00, // Yellow
        0xFF00FF, // Magenta
        0x00FFFF, // Cyan
        0xFFFFFF, // White
        0x000000  // Off
    };
    
    for (int color_idx = 0; color_idx < 8; color_idx++) {
        uint32_t color = test_colors[color_idx];
        uint8_t red = (color >> 16) & 0xFF;
        uint8_t green = (color >> 8) & 0xFF;
        uint8_t blue = color & 0xFF;
        
        ESP_LOGI(TAG, "Testing color: R=%d, G=%d, B=%d", red, green, blue);
        
        // Set all LEDs to current test color
        for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
            led_set_pixel_internal(i, red, green, blue);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500)); // 500ms delay between color changes for better performance
    }
    
    // Clear all LEDs
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        led_set_pixel_internal(i, 0, 0, 0);
    }
    
    ESP_LOGI(TAG, "=== LED Test All Pattern Complete ===");
}


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


// ============================================================================
// OBSOLETE FUNCTION - REMOVED
// ============================================================================

// REMOVED: led_execute_command() - Replaced by led_execute_command_new()
// This function was obsolete and duplicated functionality



void led_process_commands(void)
{
    // LED command processing removed - using direct LED calls now
}

// Forward declarations for animation functions
void led_puzzle_start_animation(const led_command_t* cmd);
void led_puzzle_highlight_piece(const led_command_t* cmd);
void led_puzzle_path_animation(const led_command_t* cmd);
void led_puzzle_destination_highlight(const led_command_t* cmd);
void led_puzzle_complete_animation(const led_command_t* cmd);
void led_puzzle_stop_all_animations(void);
void led_anim_player_change(const led_command_t* cmd);
void led_anim_move_path(const led_command_t* cmd);
void led_anim_castle(const led_command_t* cmd);
void led_anim_promote(const led_command_t* cmd);
void led_anim_endgame(const led_command_t* cmd);
void led_anim_check(const led_command_t* cmd);
void led_anim_checkmate(const led_command_t* cmd);

/**
 * @brief Execute LED command with full command structure
 */
void led_execute_command_new(const led_command_t* cmd)
{
    ESP_LOGI(TAG, "🔄 led_execute_command_new: type=%d", cmd->type);
    switch (cmd->type) {
        case LED_CMD_SET_PIXEL:
            // ✅ OPRAVA: Podporovat duration management
            if (cmd->duration_ms > 0) {
                led_set_pixel_with_duration(cmd->led_index, cmd->red, cmd->green, cmd->blue, cmd->duration_ms);
            } else {
                led_set_pixel_internal(cmd->led_index, cmd->red, cmd->green, cmd->blue);
            }
            break;
            
        case LED_CMD_SET_ALL:
            led_set_all_internal(cmd->red, cmd->green, cmd->blue);
            break;
            
        case LED_CMD_CLEAR:
            led_clear_all_internal();
            break;
            
        case LED_CMD_SHOW_BOARD:
            led_show_chess_board();
            break;
            
        case LED_CMD_BUTTON_FEEDBACK:
            if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
                uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
                led_set_button_feedback(button_id, (cmd->red > 0 || cmd->green > 0 || cmd->blue > 0));
            }
            break;
            
        case LED_CMD_BUTTON_PRESS:
            if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
                uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
                led_set_button_press(button_id);
            }
            break;
            
        case LED_CMD_BUTTON_RELEASE:
            if (cmd->led_index >= CHESS_LED_COUNT_BOARD) {
                uint8_t button_id = cmd->led_index - CHESS_LED_COUNT_BOARD;
                led_set_button_release(button_id);
            }
            break;
            
        case LED_CMD_ANIMATION:
            led_start_animation(cmd->duration_ms);
            break;
            
        case LED_CMD_TEST:
            led_test_pattern();
            break;
            
        case LED_CMD_STATUS_DETAILED:
            ESP_LOGI(TAG, "🔍 Processing LED_CMD_STATUS_DETAILED command");
            // Send status immediately without delays to avoid blocking main loop
            led_send_status_to_uart_immediate(cmd->response_queue);
            break;
            
        case LED_CMD_STATUS_COMPACT:
            led_print_compact_status();
            break;
            
        case LED_CMD_STATUS_ACTIVE:
            led_print_compact_status();
            break;
            
        // Puzzle Animation Cases
        case LED_CMD_PUZZLE_START:
            ESP_LOGI(TAG, "🧩 Starting puzzle animation sequence");
            led_puzzle_start_animation(cmd);
            break;
            
        case LED_CMD_PUZZLE_HIGHLIGHT:
            ESP_LOGI(TAG, "🟡 Highlighting puzzle piece at LED %d", cmd->led_index);
            led_puzzle_highlight_piece(cmd);
            break;
            
        case LED_CMD_PUZZLE_PATH:
            ESP_LOGI(TAG, "🔵 Starting puzzle path animation %d -> target", cmd->led_index);
            led_puzzle_path_animation(cmd);
            break;
            
        case LED_CMD_PUZZLE_DESTINATION:
            ESP_LOGI(TAG, "🟢 Highlighting puzzle destination at LED %d", cmd->led_index);
            led_puzzle_destination_highlight(cmd);
            break;
            
        case LED_CMD_PUZZLE_COMPLETE:
            ESP_LOGI(TAG, "🏆 Starting puzzle completion animation");
            led_puzzle_complete_animation(cmd);
            break;
            
        case LED_CMD_PUZZLE_STOP:
            ESP_LOGI(TAG, "⏹️ Stopping all puzzle animations");
            led_puzzle_stop_all_animations();
            break;
            
        // Advanced Chess Animation Cases
        case LED_CMD_ANIM_PLAYER_CHANGE:
            ESP_LOGI(TAG, "🌟 Player change animation");
            led_anim_player_change(cmd);
            break;
            
        case LED_CMD_ANIM_MOVE_PATH:
            ESP_LOGI(TAG, "➡️ Move path animation");
            led_anim_move_path(cmd);
            break;
            
        case LED_CMD_ANIM_CASTLE:
            ESP_LOGI(TAG, "🏰 Castling animation");
            led_anim_castle(cmd);
            break;
            
        case LED_CMD_ANIM_PROMOTE:
            ESP_LOGI(TAG, "👑 Promotion animation");
            led_anim_promote(cmd);
            break;
            
        case LED_CMD_ANIM_ENDGAME:
            ESP_LOGI(TAG, "🏆 Endgame animation");
            led_anim_endgame(cmd);
            break;
            
        case LED_CMD_ANIM_CHECK:
            ESP_LOGI(TAG, "⚠️ Check animation");
            led_anim_check(cmd);
            break;
            
        case LED_CMD_ANIM_CHECKMATE:
            ESP_LOGI(TAG, "☠️ Checkmate animation");
            led_anim_checkmate(cmd);
            break;
            
        case LED_CMD_ANIM_PUZZLE_PATH:
            ESP_LOGI(TAG, "🧩 Puzzle path animation");
            led_puzzle_path_animation(cmd);
            break;
            
        case LED_CMD_BUTTON_PROMOTION_AVAILABLE:
            ESP_LOGI(TAG, "🟢 Button %d promotion available", cmd->led_index - CHESS_LED_COUNT_BOARD);
            led_set_button_promotion_available(cmd->led_index - CHESS_LED_COUNT_BOARD, true);
            break;
            
        case LED_CMD_BUTTON_PROMOTION_UNAVAILABLE:
            ESP_LOGI(TAG, "🔴 Button %d promotion unavailable", cmd->led_index - CHESS_LED_COUNT_BOARD);
            led_set_button_promotion_available(cmd->led_index - CHESS_LED_COUNT_BOARD, false);
            break;
            
        case LED_CMD_BUTTON_SET_PRESSED:
            ESP_LOGI(TAG, "🔴 Button %d pressed", cmd->led_index - CHESS_LED_COUNT_BOARD);
            led_set_button_pressed(cmd->led_index - CHESS_LED_COUNT_BOARD, true);
            break;
            
        case LED_CMD_BUTTON_SET_RELEASED:
            ESP_LOGI(TAG, "🔵 Button %d released", cmd->led_index - CHESS_LED_COUNT_BOARD);
            led_set_button_pressed(cmd->led_index - CHESS_LED_COUNT_BOARD, false);
            break;
            
        // Game state integration commands
        case LED_CMD_GAME_STATE_UPDATE:
            ESP_LOGI(TAG, "🎮 Game state update requested");
            led_update_game_state();
            break;
            
        case LED_CMD_HIGHLIGHT_PIECES:
            ESP_LOGI(TAG, "🎯 Highlight pieces that can move");
            led_highlight_pieces_that_can_move();
            break;
            
        case LED_CMD_HIGHLIGHT_MOVES:
            ESP_LOGI(TAG, "🎯 Highlight possible moves for square %d", cmd->led_index);
            led_highlight_possible_moves(cmd->led_index);
            break;
            
        case LED_CMD_CLEAR_HIGHLIGHTS:
            ESP_LOGI(TAG, "🧹 Clear all highlights");
            led_clear_all_highlights();
            break;
            
        case LED_CMD_PLAYER_CHANGE:
            ESP_LOGI(TAG, "🔄 Player change animation");
            led_player_change_animation();
            break;
            
        // Error handling commands
        case LED_CMD_ERROR_INVALID_MOVE:
            ESP_LOGI(TAG, "🚨 Error: Invalid move at LED %d", cmd->led_index);
            led_error_invalid_move(cmd);
            break;
            
        case LED_CMD_ERROR_RETURN_PIECE:
            ESP_LOGI(TAG, "🔄 Error: Return piece to LED %d", cmd->led_index);
            led_error_return_piece(cmd);
            break;
            
        case LED_CMD_ERROR_RECOVERY:
            ESP_LOGI(TAG, "✅ Error recovery completed");
            led_error_recovery(cmd);
            break;
            
        case LED_CMD_SHOW_LEGAL_MOVES:
            ESP_LOGI(TAG, "💡 Show legal moves for piece at LED %d", cmd->led_index);
            led_show_legal_moves(cmd);
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown LED command type: %d", cmd->type);
            break;
    }
}

// ============================================================================
// OBSOLETE PUBLIC FUNCTION - KEPT FOR COMPATIBILITY
// ============================================================================

void led_update_hardware(void)
{
    // ✅ OBSOLETE: This function is no longer needed
    // All LED updates now happen immediately via led_set_pixel_internal()
    // Kept only for compatibility with existing code that might call it
    ESP_LOGD(TAG, "led_update_hardware() called but obsolete - all updates now immediate");
}


// ============================================================================
// ANIMATION FUNCTIONS
// ============================================================================


void led_update_animation(void)
{
    if (!animation_active) {
        return;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t elapsed = current_time - animation_start_time;
    
    if (elapsed >= animation_duration) {
        // Animation complete
        animation_active = false;
        if (simulation_mode) {
            ESP_LOGI(TAG, "Animation completed after %dms", elapsed);
        }
        return;
    }
    
    // Calculate animation progress (0.0 to 1.0)
    float progress = (float)elapsed / (float)animation_duration;
    
    // Apply animation pattern
    switch (animation_pattern) {
        case 0: // Rainbow wave
            for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
                float hue = progress + (float)i / CHESS_LED_COUNT_TOTAL;
                hue = fmod(hue, 1.0f);
                
                // Convert HSV to RGB (simplified)
                uint8_t red, green, blue;
                if (hue < 0.33f) {
                    red = 255;
                    green = (uint8_t)(hue * 3.0f * 255);
                    blue = 0;
                } else if (hue < 0.66f) {
                    red = (uint8_t)((0.66f - hue) * 3.0f * 255);
                    green = 255;
                    blue = 0;
                } else {
                    red = 0;
                    green = (uint8_t)((1.0f - hue) * 3.0f * 255);
                    blue = 255;
                }
                
                led_set_pixel_internal(i, red, green, blue);
            }
            break;
            
        case 1: // Breathing effect
            {
                float intensity = (sinf(progress * 2 * M_PI) + 1.0f) / 2.0f;
                uint8_t level = (uint8_t)(intensity * 255);
                
                for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
                    uint32_t base_color = led_states[i];
                    uint8_t red = ((base_color >> 16) & 0xFF) * level / 255;
                    uint8_t green = ((base_color >> 8) & 0xFF) * level / 255;
                    uint8_t blue = (base_color & 0xFF) * level / 255;
                    
                    led_set_pixel_internal(i, red, green, blue);
                }
            }
            break;
            
        default:
            // Default animation: fade in/out
            {
                float intensity = (sinf(progress * 2 * M_PI) + 1.0f) / 2.0f;
                uint8_t level = (uint8_t)(intensity * 255);
                
                led_set_all_internal(level, level, level);
            }
            break;
    }
}


// ============================================================================
// SMART LED REPORTING FUNCTIONS
// ============================================================================

/**
 * @brief Print compact LED status showing only lit LEDs
 */
void led_print_compact_status(void)
{
    if (quiet_period_active) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time - quiet_period_start < quiet_period_duration) {
            ESP_LOGI(TAG, "LED: Quiet period active (%" PRIu32 " ms remaining)", 
                     quiet_period_duration - (current_time - quiet_period_start));
            return;
        }
        quiet_period_active = false;
    }
    
    // Count active LEDs by type
    uint32_t board_leds_count[8] = {0}; // Color counts for board LEDs
    uint32_t button_leds_active = 0;
    uint32_t total_active = 0;
    
    // Analyze board LEDs (0-63)
    for (int i = 0; i < 64; i++) {
        if (led_states[i] != 0) {
            total_active++;
            uint32_t color = led_states[i];
            uint8_t red = (color >> 16) & 0xFF;
            uint8_t green = (color >> 8) & 0xFF;
            uint8_t blue = color & 0xFF;
            
            // Determine color category
            if (red == 0 && green == 0 && blue == 0) {
                board_leds_count[0]++; // Black
            } else if (red == 0 && green == 0 && blue > 0) {
                board_leds_count[1]++; // Blue
            } else if (red == 0 && green > 0 && blue == 0) {
                board_leds_count[2]++; // Green
            } else if (red == 0 && green > 0 && blue > 0) {
                board_leds_count[3]++; // Cyan
            } else if (red > 0 && green == 0 && blue == 0) {
                board_leds_count[4]++; // Red
            } else if (red > 0 && green == 0 && blue > 0) {
                board_leds_count[5]++; // Magenta
            } else if (red > 0 && green > 0 && blue == 0) {
                board_leds_count[6]++; // Yellow
            } else if (red > 0 && green > 0 && blue > 0) {
                board_leds_count[7]++; // White
            }
        }
    }
    
    // Analyze button LEDs (64-72)
    char button_status[128] = "";
    int button_pos = 0;
    for (int i = 64; i < 73; i++) {
        if (led_states[i] != 0) {
            button_leds_active++;
            total_active++;
            
            // Get button color name
            uint32_t color = led_states[i];
            uint8_t red = (color >> 16) & 0xFF;
            uint8_t green = (color >> 8) & 0xFF;
            uint8_t blue = color & 0xFF;
            
            const char* color_name = "Unknown";
            if (red == 0xFF && green == 0x00 && blue == 0xFF) color_name = "Mag";
            else if (red == 0xFF && green == 0x00 && blue == 0x00) color_name = "Red";
            else if (red == 0x00 && green == 0xFF && blue == 0xFF) color_name = "Cya";
            else if (red == 0xFF && green == 0x80 && blue == 0x00) color_name = "Ora";
            else if (red == 0x00 && green == 0xFF && blue == 0x00) color_name = "Gre";
            else if (red == 0x00 && green == 0x00 && blue == 0xFF) color_name = "Blu";
            else if (red == 0xFF && green == 0xFF && blue == 0x00) color_name = "Yel";
            else if (red == 0xFF && green == 0xFF && blue == 0xFF) color_name = "Whi";
            
            // Add to button status string
            if (button_pos > 0) button_pos += snprintf(button_status + button_pos, sizeof(button_status) - button_pos, " ");
            button_pos += snprintf(button_status + button_pos, sizeof(button_status) - button_pos, 
                                 "%s=%s", button_names[i-64], color_name);
        }
    }
    
    // Print compact status
    if (total_active == 0) {
        ESP_LOGI(TAG, "LED: All LEDs off");
        return;
    }
    
    // Board LEDs summary
    char board_summary[256] = "";
    int board_pos = 0;
    for (int i = 0; i < 8; i++) {
        if (board_leds_count[i] > 0) {
            if (board_pos > 0) board_pos += snprintf(board_summary + board_pos, sizeof(board_summary) - board_pos, ", ");
            board_pos += snprintf(board_summary + board_pos, sizeof(board_summary) - board_pos, 
                                "%" PRIu32 "x %s", board_leds_count[i], color_names[i]);
        }
    }
    
    // Final compact output
    if (button_leds_active > 0) {
        ESP_LOGI(TAG, "LED: Board: %s | Buttons: %s | Total: %" PRIu32 " active", 
                 board_summary, button_status, total_active);
    } else {
        ESP_LOGI(TAG, "LED: Board: %s | Total: %" PRIu32 " active", board_summary, total_active);
    }
    
    // Update tracking
    last_status_report_time = esp_timer_get_time() / 1000;
    memcpy(previous_led_states, led_states, sizeof(led_states));
    led_states_initialized = true;
}

/**
 * @brief Print detailed LED status showing all LED states
 */
void led_print_detailed_status(void)
{
    ESP_LOGI(TAG, "=== Detailed LED Status ===");
    ESP_LOGI(TAG, "Total LEDs: %d (64 board + 9 buttons)", CHESS_LED_COUNT_TOTAL);
    
    // Board LEDs
    ESP_LOGI(TAG, "Board LEDs (0-63):");
    for (int row = 7; row >= 0; row--) {
        char row_status[256] = "";
        int pos = 0;
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            uint32_t color = led_states[led_index];
            if (color != 0) {
                uint8_t red = (color >> 16) & 0xFF;
                uint8_t green = (color >> 8) & 0xFF;
                uint8_t blue = color & 0xFF;
                pos += snprintf(row_status + pos, sizeof(row_status) - pos, 
                               "%d=R(%d,%d,%d) ", led_index, red, green, blue);
            }
        }
        if (pos > 0) {
            ESP_LOGI(TAG, "  Row %d: %s", 8-row, row_status);
        }
    }
    
    // Button LEDs
    ESP_LOGI(TAG, "Button LEDs (64-72):");
    for (int i = 64; i < 73; i++) {
        if (led_states[i] != 0) {
            uint32_t color = led_states[i];
            uint8_t red = (color >> 16) & 0xFF;
            uint8_t green = (color >> 8) & 0xFF;
            uint8_t blue = color & 0xFF;
            ESP_LOGI(TAG, "  %d (%s): R(%d,%d,%d)", i, button_names[i-64], red, green, blue);
        }
    }
    
    ESP_LOGI(TAG, "=== End LED Status ===");
}



/**
 * @brief Send LED status to UART task immediately without delays (for real-time response)
 */
static void led_send_status_to_uart_immediate(QueueHandle_t response_queue)
{
    ESP_LOGI(TAG, "🔍 led_send_status_to_uart_immediate called (STREAMING OPTIMIZED)");
    if (response_queue == NULL) {
        ESP_LOGW(TAG, "No response queue available for LED status");
        return;
    }
    
    // Reset WDT before starting operation (only if registered)
    esp_err_t wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // MEMORY OPTIMIZATION: Use streaming output instead of malloc(1536)
    // This eliminates heap fragmentation and reduces memory usage
    ESP_LOGI(TAG, "📡 Using streaming output for immediate LED status (no malloc)");
    
    // Configure streaming for queue output
    esp_err_t ret = streaming_set_queue_output(response_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure streaming output: %s", esp_err_to_name(ret));
        return;
    }
    
    // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
    // Stream LED status header
    stream_writeln("🔍 LED Board Status (Real-time)");
    stream_writeln("═══════════════════════════════════════════════════════════════");
    stream_writeln("📊 Board LEDs (64) - Chessboard Layout:");
    
    
    // Stream board LEDs in chessboard layout (8x8)
    for (int row = 7; row >= 0; row--) {  // Start from row 8 (top)
        // Reset watchdog every row to prevent timeouts (only if registered)
        wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Row header
        stream_printf("  %d │ ", row + 1);
        
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(7 - row, col);  // Convert to LED index
            uint32_t color = led_states[led_index];
                uint8_t red = (color >> 16) & 0xFF;
                uint8_t green = (color >> 8) & 0xFF;
                uint8_t blue = color & 0xFF;
                
            // Convert to chess notation
            char square[4];
            snprintf(square, sizeof(square), "%c%d", 'a' + col, row + 1);
            
            // Get ANSI color for square name and RGB values
            const char* ansi_color = get_ansi_color_from_rgb(red, green, blue);
            
            // Display square with colored name and simplified RGB values
            stream_printf("%s%s\033[0m:%s(%d,%d,%d)\033[0m ", 
                         ansi_color, square, ansi_color, red, green, blue);
        }
        
        // End of row
        stream_writeln("");
    }
    
    // Stream column headers
    stream_printf("    └─");
    for (int col = 0; col < 8; col++) {
        stream_printf("─────");
    }
    stream_writeln("");
    
    // Stream column labels
    stream_printf("     ");
    for (int col = 0; col < 8; col++) {
        stream_printf("  %c  ", 'a' + col);
    }
    stream_writeln("");
    
    // Stream button LEDs (64-72)
    stream_writeln("\n📊 Button LEDs (9):");
    
    const char* button_names[] = {
        "NEW_GAME", "SAVE", "LOAD", "UNDO", "REDO", 
        "HINT", "ANALYZE", "SETTINGS", "HELP"
    };
    
    for (int i = 64; i < 73; i++) {
            uint32_t color = led_states[i];
            uint8_t red = (color >> 16) & 0xFF;
            uint8_t green = (color >> 8) & 0xFF;
            uint8_t blue = color & 0xFF;
        
        // Get ANSI color for button name and RGB values
        const char* ansi_color = get_ansi_color_from_rgb(red, green, blue);
        
        stream_printf("  %d (%s%s\033[0m): %sR(%d,%d,%d)\033[0m\n", 
                     i, ansi_color, button_names[i-64], ansi_color, red, green, blue);
    }
    
    // Send LED data to UART task as single response (no chunking for immediate response)
    // MEMORY OPTIMIZATION: Streaming output handles data transmission
    // No need for manual queue management or large buffers
    ESP_LOGI(TAG, "✅ LED status streaming completed immediately");
    
    // Final WDT reset (only if registered)
    wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // Cleanup hardware on task exit
    led_hardware_cleanup();
}

// ============================================================================
// GAME STATE INTEGRATION FUNCTIONS
// ============================================================================

/**
 * @brief Update LEDs based on current game state
 */
void led_update_game_state(void)
{
    ESP_LOGI(TAG, "🎮 Updating LEDs based on current game state");
    
    // Clear all board highlights first
    led_clear_all_highlights();
    
    // Highlight pieces that can move for current player
    led_highlight_pieces_that_can_move();
    
    ESP_LOGI(TAG, "✅ Game state LED update completed");
}

/**
 * @brief Highlight pieces that can move for current player
 */
void led_highlight_pieces_that_can_move(void)
{
    ESP_LOGI(TAG, "🎯 Highlighting pieces that can move");
    
    // Clear all highlights first
    led_clear_all_highlights();
    
    // ✅ OPRAVA: Použít skutečnou game logiku místo simulace
    // Voláme game_highlight_movable_pieces() z game_task.c
    extern void game_highlight_movable_pieces(void);
    game_highlight_movable_pieces();
    
    ESP_LOGI(TAG, "✅ Called game_highlight_movable_pieces() for real highlighting");
}

/**
 * @brief Highlight possible moves for selected piece
 * @param from_square Square index (0-63) of the selected piece
 */
void led_highlight_possible_moves(uint8_t from_square)
{
    if (from_square >= 64) {
        ESP_LOGE(TAG, "Invalid square index: %d", from_square);
        return;
    }
    
    ESP_LOGI(TAG, "🎯 Highlighting possible moves from square %d", from_square);
    
    // Clear previous highlights
    led_clear_all_highlights();
    
    // Highlight source square in yellow
    led_set_pixel_internal(from_square, 255, 255, 0);
    
    // Simulate highlighting some possible moves
    // In real implementation, this would query game_task for valid moves
    uint8_t example_moves[] = {from_square + 8, from_square + 16}; // Forward moves for pawns
    
    for (int i = 0; i < 2; i++) {
        uint8_t dest_square = example_moves[i];
        if (dest_square < 64) {
            // Highlight possible destinations in green
            led_set_pixel_internal(dest_square, 0, 255, 0);
        }
    }
    
    ESP_LOGI(TAG, "✅ Highlighted possible moves from square %d", from_square);
}

/**
 * @brief Clear all board highlights and return to chess board pattern
 */
void led_clear_all_highlights(void)
{
    ESP_LOGI(TAG, "🧹 Clearing all board highlights");
    
    // Clear all board LEDs (0-63) - they should be off by default
    for (int i = 0; i < 64; i++) {
        led_states[i] = 0x000000; // All off
    }
    
    ESP_LOGI(TAG, "✅ All board highlights cleared");
}

/**
 * @brief Player change animation
 */
void led_player_change_animation(void)
{
    ESP_LOGI(TAG, "🔄 Starting player change animation");
    
    // Simple animation: flash all board LEDs white, then restore pattern
    for (int flash = 0; flash < 3; flash++) {
        // Flash white
        for (int i = 0; i < 64; i++) {
            led_set_pixel_internal(i, 255, 255, 255);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        
        // Flash black
        for (int i = 0; i < 64; i++) {
            led_set_pixel_internal(i, 0, 0, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // Clear all LEDs after animation
    led_clear_all_highlights();
    
    // Highlight pieces for new player
    led_highlight_pieces_that_can_move();
    
    ESP_LOGI(TAG, "✅ Player change animation completed");
}

// ============================================================================
// HARDWARE INTEGRATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize RMT hardware for WS2812B LED control
 * @return ESP_OK on success, error code on failure
 */
static esp_err_t led_hardware_init(void)
{
    ESP_LOGI(TAG, "🔧 Initializing WS2812B hardware with official led_strip driver...");
    
    // ✅ LED STRIP CONFIGURATION
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_DATA_PIN,
        .max_leds = CHESS_LED_COUNT_TOTAL,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    
    // ✅ RMT BACKEND CONFIGURATION - Optimized for WS2812B timing
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz resolution for precise WS2812B timing
        .mem_block_symbols = 128,            // More buffer for stability
        .flags = {
            .with_dma = false,               // ESP32-C6 doesn't support DMA for RMT
        },
    };

    // ✅ CREATE LED STRIP OBJECT
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ LED strip creation failed: %s", esp_err_to_name(ret));
        led_strip = NULL;  // Ensure handle is NULL on failure
        return ret;
    }
    
    // ✅ Verify LED strip handle is valid
    if (led_strip == NULL) {
        ESP_LOGE(TAG, "❌ LED strip handle is NULL after creation");
        return ESP_ERR_INVALID_STATE;
    }
    
    // ✅ CLEAR ALL LEDS
    ret = led_strip_clear(led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ LED strip clear failed: %s", esp_err_to_name(ret));
        led_strip_del(led_strip);
        return ret;
    }

    led_initialized = true;
    
    // ✅ Initialize batch update system
    led_changes_pending = false;
    memset(led_changed_flags, false, sizeof(led_changed_flags));
    memset(led_pending_changes, 0, sizeof(led_pending_changes));
    
    ESP_LOGI(TAG, "✅ WS2812B hardware initialized successfully with official driver");
    ESP_LOGI(TAG, "  • GPIO: %d", LED_DATA_PIN);
    ESP_LOGI(TAG, "  • LEDs: %d total", CHESS_LED_COUNT_TOTAL);
    ESP_LOGI(TAG, "  • Driver: espressif/led_strip");
    ESP_LOGI(TAG, "  • Batch update system: initialized");

    // ✅ SIMPLE STARTUP TEST - NO critical sections, just verify driver works
    ESP_LOGI(TAG, "🟢 STARTING SIMPLE STARTUP TEST...");
    
    // ✅ NO CRITICAL SECTION - let driver handle timing
    ret = led_strip_clear(led_strip);
    if (ret == ESP_OK) {
        ret = led_strip_refresh(led_strip);
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ LED strip cleared and initialized successfully");
    } else {
        ESP_LOGE(TAG, "❌ LED strip initialization failed: %s", esp_err_to_name(ret));
    }
    
    ESP_LOGI(TAG, "✅ Simple startup test completed, proceeding with normal operation");

    
    return ESP_OK;
}



// ============================================================================
// OBSOLETE FUNCTION - REMOVED
// ============================================================================

/**
 * @brief OBSOLETE: Hardware update function no longer needed
 * All LED updates now happen immediately via led_set_pixel_internal()
 * This function kept only for compatibility - does nothing useful
 */
// static void led_hardware_update(void) - REMOVED

/**
 * @brief Cleanup hardware resources
 */
static void led_hardware_cleanup(void)
{
    if (led_initialized && led_strip) {
        ESP_LOGI(TAG, "🧹 Cleaning up WS2812B hardware...");
        led_strip_clear(led_strip);
        led_strip_del(led_strip);
        led_strip = NULL;
        led_initialized = false;
        ESP_LOGI(TAG, "✅ Hardware cleanup completed");
    }
}

/**
 * @brief Print only LED changes since last status
 */
void led_print_changes_only(void)
{
    if (!led_states_initialized) {
        ESP_LOGI(TAG, "LED: No previous state to compare");
        return;
    }
    
    bool changes_found = false;
    char changes_summary[512] = "";
    int pos = 0;
    
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (led_states[i] != previous_led_states[i]) {
            changes_found = true;
            
            uint32_t old_color = previous_led_states[i];
            uint32_t new_color = led_states[i];
            
            uint8_t old_red = (old_color >> 16) & 0xFF;
            uint8_t old_green = (old_color >> 8) & 0xFF;
            uint8_t old_blue = old_color & 0xFF;
            
            uint8_t new_red = (new_color >> 16) & 0xFF;
            uint8_t new_green = (new_color >> 8) & 0xFF;
            uint8_t new_blue = (new_color & 0xFF);
            
            if (pos < sizeof(changes_summary) - 100) {
                if (i < 64) {
                    // Board LED
                    pos += snprintf(changes_summary + pos, sizeof(changes_summary) - pos,
                                   "%d:B(%d,%d,%d)->(%d,%d,%d) ", 
                                   i, old_red, old_green, old_blue, new_red, new_green, new_blue);
                } else {
                    // Button LED
                    pos += snprintf(changes_summary + pos, sizeof(changes_summary) - pos,
                                   "%d:%s(%d,%d,%d)->(%d,%d,%d) ", 
                                   i, button_names[i-64], old_red, old_green, old_blue, 
                                   new_red, new_green, new_blue);
                }
            }
        }
    }
    
    if (changes_found) {
        ESP_LOGI(TAG, "LED Changes: %s", changes_summary);
        led_changes_count++;
    } else {
        ESP_LOGI(TAG, "LED: No changes detected");
    }
}

/**
 * @brief Start quiet period (suppress LED status reports)
 */
void led_start_quiet_period(uint32_t duration_ms)
{
    quiet_period_active = true;
    quiet_period_start = esp_timer_get_time() / 1000;
    quiet_period_duration = duration_ms;
    ESP_LOGI(TAG, "LED: Quiet period started (%" PRIu32 " ms)", duration_ms);
}

/**
 * @brief Check if LED states have changed significantly
 */
bool led_has_significant_changes(void)
{
    if (!led_states_initialized) return true;
    
    uint32_t changed_count = 0;
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (led_states[i] != previous_led_states[i]) {
            changed_count++;
            if (changed_count > 5) return true; // More than 5 LEDs changed
        }
    }
    
    return changed_count > 0;
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void led_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "🚀 LED task starting...");
    
    // Initialize hardware
    esp_err_t hw_ret = led_hardware_init();
    if (hw_ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ Hardware initialization failed: %s", esp_err_to_name(hw_ret));
        ESP_LOGE(TAG, "❌ LED strip handle is NULL - switching to simulation mode");
        // Continue in simulation mode
        simulation_mode = true;
        led_initialized = false;
        ESP_LOGW(TAG, "⚠️ Continuing in simulation mode - LED commands will be logged only");
    } else {
        ESP_LOGI(TAG, "✅ Hardware initialized successfully");
        simulation_mode = false;
        led_initialized = true;
    }
    
    // NOTE: Task is already registered with TWDT in main.c
    // No need to register again here to avoid duplicate registration
    
    // BOOTING ANIMATION: Progressive LED illumination
    ESP_LOGI(TAG, "🌟 Starting booting animation...");
    // led_booting_animation(); // TODO: Fix function declaration
    ESP_LOGI(TAG, "LED task started successfully (%s)", 
             simulation_mode ? "SIMULATION MODE" : "HARDWARE MODE");
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • WS2812B %s: 73 LEDs (64 board + 9 buttons: 8 promotion + 1 reset)", 
             simulation_mode ? "simulation" : "hardware");
    ESP_LOGI(TAG, "  • Chess board pattern: alternating black/white squares");
    ESP_LOGI(TAG, "  • Button LED feedback: availability-based colors");
    ESP_LOGI(TAG, "  • Animation support: rainbow wave, breathing, fade");
    ESP_LOGI(TAG, "  • Command queue processing: LED commands from other tasks");
    ESP_LOGI(TAG, "  • Time-multiplexed updates: 5ms cycle");
    
    ESP_LOGI(TAG, "🔄 Initializing LED states...");
    task_running = true;
    
    // ✅ OPRAVA: Initialize LED unified mutex for synchronization
    led_unified_mutex = xSemaphoreCreateMutex();
    if (led_unified_mutex == NULL) {
        ESP_LOGE(TAG, "❌ CRITICAL: Failed to create LED unified mutex!");
        return;
    }
    ESP_LOGI(TAG, "✅ LED unified mutex created");
    
    // ✅ Initialize duration management system
    led_init_duration_system();
    
    // ✅ Initialize unified animation manager for non-blocking endgame animations
    animation_config_t anim_config = {
        .max_concurrent_animations = 8,
        .update_frequency_hz = 30,  // 30 FPS for smooth animations
        .enable_smooth_interpolation = true,
        .enable_trail_effects = true,
        .default_duration_ms = 1000
    };
    esp_err_t anim_ret = animation_manager_init(&anim_config);
    if (anim_ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ Animation manager init failed: %s", esp_err_to_name(anim_ret));
    } else {
        ESP_LOGI(TAG, "✅ Animation manager initialized");
    }
    
    // Initialize LED states
    ESP_LOGI(TAG, "🔄 Clearing all LEDs...");
    led_clear_all_internal();
    ESP_LOGI(TAG, "🔄 Showing chess board pattern...");
    led_show_chess_board();
    ESP_LOGI(TAG, "✅ LED initialization complete, entering main loop...");
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // Reset watchdog - only if task is registered with WDT
        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Watchdog logging every 1000 iterations (5 seconds)
        if (loop_count % 1000 == 0) {
            ESP_LOGI(TAG, "LED Task Watchdog: loop=%d, heap=%zu", loop_count, esp_get_free_heap_size());
        }
        
        // Process LED commands from queue
        led_process_commands();
        
        // Update animations
        led_update_animation();
        
        // Update unified animation manager (non-blocking endgame animations)
        animation_manager_update();
        
        // Process button blink timers
        led_process_button_blink_timers();
        
        // ✅ COMMIT PENDING LED CHANGES - Use privileged batch commit for maximum stability
        led_privileged_batch_commit();
        
        // Update hardware (simulation mode)

        
        // Periodic status update - reduced frequency for cleaner UART
        if (loop_count % 10000 == 0) { // Every 10000 loops (50 seconds)
            ESP_LOGI(TAG, "LED Task Status: loop=%d, animation=%s", 
                     loop_count, animation_active ? "active" : "inactive");
        }
        
        loop_count++;
        
        // ✅ Optimalizovaný cyklus - 33ms pro 30 FPS animace
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(33));
    }
}

// ============================================================================
// PUZZLE ANIMATION IMPLEMENTATIONS
// ============================================================================

// Global puzzle animation state
static TimerHandle_t puzzle_timer = NULL;
static uint8_t puzzle_animation_frame = 0;
static uint8_t puzzle_animation_total_frames = 0;
static uint8_t puzzle_animation_source = 0;
static uint8_t puzzle_animation_target = 0;
static bool puzzle_animation_active = false;

// Global endgame animation state
static bool endgame_animation_active = false;
static player_t endgame_winner = PLAYER_WHITE;
static uint8_t endgame_animation_style = 0;
static uint32_t endgame_animation_frame = 0;

/**
 * @brief Timer callback for puzzle animations
 */
void puzzle_animation_timer_callback(TimerHandle_t xTimer) {
    if (!puzzle_animation_active) return;
    
    puzzle_animation_frame++;
    
    if (puzzle_animation_frame >= puzzle_animation_total_frames) {
        // Animation complete
        puzzle_animation_active = false;
        xTimerStop(puzzle_timer, 0);
        ESP_LOGI(TAG, "🎯 Puzzle animation completed");
        return;
    }
    
    // Calculate animation progress
    float progress = (float)puzzle_animation_frame / (float)puzzle_animation_total_frames;
    
    // Create path animation effect
    uint8_t source_row = puzzle_animation_source / 8;
    uint8_t source_col = puzzle_animation_source % 8;
    uint8_t target_row = puzzle_animation_target / 8;
    uint8_t target_col = puzzle_animation_target % 8;
    
    // Clear board first
    led_clear_board_only();
    
    // IMPROVED: Multi-point trail animation with brightness effects
    for (int trail = 0; trail < 3; trail++) {
        float trail_progress = progress - (trail * 0.2f);
        if (trail_progress < 0) continue;
        if (trail_progress > 1) break;
    
    // Linear interpolation
        float current_row = source_row + (target_row - source_row) * trail_progress;
        float current_col = source_col + (target_col - source_col) * trail_progress;
    
    uint8_t current_square = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
    
    if (current_square < 64) {
            // Progressive color change: Cyan -> Blue -> Purple
            uint8_t red, green, blue;
            if (trail_progress < 0.5f) {
                // Cyan to Blue
                float local_progress = trail_progress / 0.5f;
                red = 0;
                green = 255 - (uint8_t)(255 * local_progress);
                blue = 255;
            } else {
                // Blue to Purple
                float local_progress = (trail_progress - 0.5f) / 0.5f;
                red = (uint8_t)(255 * local_progress);
                green = 0;
                blue = 255;
            }
            
            // Trail brightness effect - each point gets dimmer
            float trail_brightness = 1.0f - (trail * 0.3f);
            red = (uint8_t)(red * trail_brightness);
            green = (uint8_t)(green * trail_brightness);
            blue = (uint8_t)(blue * trail_brightness);
            
            // Add pulsing effect
            float pulse = 0.7f + 0.3f * sin(progress * 6.28f + trail * 2.09f);
            red = (uint8_t)(red * pulse);
            green = (uint8_t)(green * pulse);
            blue = (uint8_t)(blue * pulse);
            
            led_set_pixel_internal(current_square, red, green, blue);
        }
    }
}

void led_puzzle_start_animation(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Simple welcome animation - single flash
    for (int i = 0; i < 64; i++) {
        led_set_pixel_internal(i, 0, 255, 255); // Cyan
    }

    vTaskDelay(pdMS_TO_TICKS(300));
    
    led_clear_all_highlights();
    ESP_LOGI(TAG, "🧩 Puzzle welcome animation completed");
}

void led_puzzle_highlight_piece(const led_command_t* cmd) {
    if (!cmd || cmd->led_index >= 64) return;
    
    // Simple highlight - just set yellow
    led_set_pixel_internal(cmd->led_index, 255, 255, 0);

    
    ESP_LOGI(TAG, "🟡 Highlighted puzzle piece at LED %d", cmd->led_index);
}

void led_puzzle_path_animation(const led_command_t* cmd) {
    if (!cmd || cmd->led_index >= 64) return;
    
    ESP_LOGI(TAG, "🧩 Starting enhanced puzzle path animation");
    
    uint8_t from_led = cmd->led_index;
    uint8_t to_led = (cmd->data ? *((uint8_t*)cmd->data) : 63); // default to h8
    
    // Convert LED indices to row/col
    uint8_t from_row = from_led / 8;
    uint8_t from_col = from_led % 8;
    uint8_t to_row = to_led / 8;
    uint8_t to_col = to_led % 8;
    
    // STEP 1: Highlight starting position with pulsing effect
    ESP_LOGI(TAG, "🎯 Step 1: Highlighting starting position");
    for (int pulse = 0; pulse < 5; pulse++) {
        led_clear_board_only();
        float brightness = 0.5f + 0.5f * sin(pulse * 1.26f);
        led_set_pixel_safe(from_led, (uint8_t)(255 * brightness), (uint8_t)(255 * brightness), 0); // Pulsing yellow
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // STEP 2: Show path with step-by-step guidance
    ESP_LOGI(TAG, "🎯 Step 2: Showing path step by step");
    int steps = abs(to_row - from_row) + abs(to_col - from_col);
    if (steps == 0) steps = 1;
    
    for (int step = 0; step <= steps; step++) {
        led_clear_board_only();
        
        // Calculate current position
        float progress = (float)step / steps;
        float current_row = from_row + (to_row - from_row) * progress;
        float current_col = from_col + (to_col - from_col) * progress;
        
        uint8_t current_led = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
        
        // Show all previous steps in dimmer color
        for (int prev_step = 0; prev_step < step; prev_step++) {
            float prev_progress = (float)prev_step / steps;
            float prev_row = from_row + (to_row - from_row) * prev_progress;
            float prev_col = from_col + (to_col - from_col) * prev_progress;
            uint8_t prev_led = chess_pos_to_led_index((uint8_t)prev_row, (uint8_t)prev_col);
            led_set_pixel_safe(prev_led, 0, 128, 0); // Dim green for previous steps
        }
        
        // Highlight current step
        if (step < steps) {
            // Current step - bright cyan
            led_set_pixel_safe(current_led, 0, 255, 255);
        } else {
            // Final destination - bright yellow
            led_set_pixel_safe(current_led, 255, 255, 0);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200)); // Faster for better performance
    }
    
    // STEP 3: Final confirmation with celebration
    ESP_LOGI(TAG, "🎯 Step 3: Final confirmation");
    for (int celebration = 0; celebration < 3; celebration++) {
        led_clear_board_only();
        
        // Show complete path
        for (int step = 0; step <= steps; step++) {
            float progress = (float)step / steps;
            float current_row = from_row + (to_row - from_row) * progress;
            float current_col = from_col + (to_col - from_col) * progress;
            uint8_t current_led = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
            
            // Celebration colors
            uint8_t r, g, b;
            if (celebration == 0) { r = 255; g = 0; b = 0; }      // Red
            else if (celebration == 1) { r = 0; g = 255; b = 0; } // Green
            else { r = 0; g = 0; b = 255; }                       // Blue
            
            led_set_pixel_safe(current_led, r, g, b);
        }
        
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    // STEP 4: Clear and show final destination
    led_clear_board_only();
    led_set_pixel_safe(to_led, 255, 255, 0); // Final yellow highlight
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_clear_board_only();
    
    ESP_LOGI(TAG, "🧩 Enhanced puzzle path animation completed");
}

void led_puzzle_destination_highlight(const led_command_t* cmd) {
    if (!cmd || cmd->led_index >= 64) return;
    
    // Simple highlight - just set green
    led_set_pixel_internal(cmd->led_index, 0, 255, 0);

    
    ESP_LOGI(TAG, "🟢 Highlighted destination at LED %d", cmd->led_index);
}

void led_puzzle_complete_animation(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Simple success animation - flash green
    for (int flash = 0; flash < 2; flash++) {
        led_set_all_internal(0, 255, 0); // Green

        vTaskDelay(pdMS_TO_TICKS(300));
    
        led_clear_all_highlights();

        vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    ESP_LOGI(TAG, "🏆 Puzzle completion animation finished");
}

void led_puzzle_stop_all_animations(void) {
    animation_active = false;
    
    if (puzzle_timer != NULL) {
        xTimerStop(puzzle_timer, 0);
    }
    
    // Clear all LEDs
    led_clear_all_internal();

    
    ESP_LOGI(TAG, "⏹️ All puzzle animations stopped");
}

// ============================================================================
// ADVANCED CHESS ANIMATION IMPLEMENTATIONS
// ============================================================================

void led_anim_player_change(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Rays emanating from center
    uint8_t center = 27; // d4 square
    
    // White or black color based on data (default to white)
    bool is_white = cmd->data ? (*((uint8_t*)cmd->data) != 0) : true;
    uint8_t r = is_white ? 255 : 100;
    uint8_t g = is_white ? 255 : 100;
    uint8_t b = is_white ? 255 : 100;
    
    for (int step = 0; step < 4; step++) {
        led_clear_all_internal();
        
        // Draw rays in 8 directions
        for (int dir = 0; dir < 8; dir++) {
            int dx[] = {-1, -1, -1, 0, 0, 1, 1, 1};
            int dy[] = {-1, 0, 1, -1, 1, -1, 0, 1};
            
            int center_row = center / 8;
            int center_col = center % 8;
            
            for (int len = 0; len <= step; len++) {
                int row = center_row + dx[dir] * len;
                int col = center_col + dy[dir] * len;
                
                if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                    uint8_t square = chess_pos_to_led_index(row, col);
                    led_set_pixel_internal(square, r, g, b);
                }
            }
        }
        

        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    // Fade out
    for (int brightness = 255; brightness >= 0; brightness -= 20) {
        for (int i = 0; i < 64; i++) {
            led_set_pixel_internal(i, (r * brightness) / 255, 
                                   (g * brightness) / 255, 
                                   (b * brightness) / 255);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
    
    led_clear_all_internal();

    
    ESP_LOGI(TAG, "🌟 Player change animation completed");
}

void led_anim_move_path(const led_command_t* cmd) {
    if (!cmd || cmd->led_index >= 64) return;
    
    uint8_t from_led = cmd->led_index;
    uint8_t to_led = (cmd->data ? *((uint8_t*)cmd->data) : 63);  // default to h8
    
    ESP_LOGI(TAG, "🎬 Move path animation: %d -> %d", from_led, to_led);
    
    // Calculate path with smooth interpolation
    uint8_t from_row = from_led / 8;
    uint8_t from_col = from_led % 8;
    uint8_t to_row = to_led / 8;
    uint8_t to_col = to_led % 8;
    
    // ULTRA-FAST: Move animation optimized for 30 FPS (33ms total)
    for (int frame = 0; frame < 15; frame++) { // More frames for smoothness
        led_clear_board_only();
        
        float progress = (float)frame / 14.0f;
        
        // Create smooth trail effect
        for (int trail = 0; trail < 3; trail++) {
            float trail_progress = progress - (trail * 0.2f);
            if (trail_progress < 0) continue;
            if (trail_progress > 1) break;
            
            // Calculate current position with smooth interpolation
            float current_row = from_row + (to_row - from_row) * trail_progress;
            float current_col = from_col + (to_col - from_col) * trail_progress;
            
            uint8_t current_led = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
            
            // Smooth color transition: Green -> Blue
            uint8_t red = 0;
            uint8_t green = 255 - (uint8_t)(255 * trail_progress);
            uint8_t blue = (uint8_t)(255 * trail_progress);
            
            // Trail brightness with smooth fade
            float trail_brightness = 1.0f - (trail * 0.25f);
            red = (uint8_t)(red * trail_brightness);
            green = (uint8_t)(green * trail_brightness);
            blue = (uint8_t)(blue * trail_brightness);
            
            // Add smooth pulsing effect
            float pulse = 0.8f + 0.2f * sin(progress * 6.28f + trail * 1.57f);
            red = (uint8_t)(red * pulse);
            green = (uint8_t)(green * pulse);
            blue = (uint8_t)(blue * pulse);
            
            led_set_pixel_safe(current_led, red, green, blue);
        }
        
        vTaskDelay(pdMS_TO_TICKS(2)); // 30 FPS: 15 frames × 2ms = 30ms total
    }
    
    // Quick final highlight (no burst effect)
    led_clear_board_only();
    led_set_pixel_safe(to_led, 0, 0, 255); // Blue destination
    vTaskDelay(pdMS_TO_TICKS(50)); // Quick highlight
    
    led_clear_board_only();
}

void led_anim_castle(const led_command_t* cmd) {
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏰 Starting castling animation");
    
    // Get castling data from command
    uint8_t king_from = cmd->led_index;
    uint8_t king_to = (cmd->data ? *((uint8_t*)cmd->data) : king_from + 2);
    
    // Calculate rook positions based on castling direction
    uint8_t rook_from, rook_to;
    if (king_to > king_from) {
        // Kingside castling - rook moves from h to f
        rook_from = king_from + 3;  // h1 or h8
        rook_to = king_from + 1;    // f1 or f8
    } else {
        // Queenside castling - rook moves from a to d
        rook_from = king_from - 4;  // a1 or a8
        rook_to = king_from - 1;    // d1 or d8
    }
    
    // FIXED: Ensure valid LED indices
    if (rook_from >= 64) rook_from = 63;
    if (rook_to >= 64) rook_to = 63;
    
    // Validate LED indices
    if (king_from >= 64 || king_to >= 64 || rook_from >= 64 || rook_to >= 64) {
        ESP_LOGE(TAG, "❌ Invalid LED indices: king_from=%d, king_to=%d, rook_from=%d, rook_to=%d", 
                 king_from, king_to, rook_from, rook_to);
        return;
    }
    
    // ENHANCED: Multi-point trail animation with brightness effects
    for (int frame = 0; frame < 15; frame++) {
        led_clear_board_only();
        
        float progress = (float)frame / 14.0f;
        
        // Create trail effect for both pieces
        for (int trail = 0; trail < 4; trail++) {
            float trail_progress = progress - (trail * 0.15f);
            if (trail_progress < 0) continue;
            if (trail_progress > 1) break;
            
            // King animation with smooth easing
            float eased_progress = trail_progress * trail_progress * (3.0f - 2.0f * trail_progress);
            uint8_t king_current = king_from + (king_to - king_from) * eased_progress;
            
            // Rook animation with smooth easing
            uint8_t rook_current = rook_from + (rook_to - rook_from) * eased_progress;
            
            // King color: Gold with pulsing effect
            float king_pulse = 0.8f + 0.2f * sin(progress * 6.28f + trail * 1.57f);
            uint8_t king_red = (uint8_t)(255 * king_pulse);
            uint8_t king_green = (uint8_t)(215 * king_pulse);
            uint8_t king_blue = (uint8_t)(0 * king_pulse);
            
            // Rook color: Silver with pulsing effect
            float rook_pulse = 0.7f + 0.3f * sin(progress * 6.28f + trail * 2.09f);
            uint8_t rook_red = (uint8_t)(192 * rook_pulse);
            uint8_t rook_green = (uint8_t)(192 * rook_pulse);
            uint8_t rook_blue = (uint8_t)(192 * rook_pulse);
            
            // Trail brightness effect
            float trail_brightness = 1.0f - (trail * 0.2f);
            king_red = (uint8_t)(king_red * trail_brightness);
            king_green = (uint8_t)(king_green * trail_brightness);
            king_blue = (uint8_t)(king_blue * trail_brightness);
            
            rook_red = (uint8_t)(rook_red * trail_brightness);
            rook_green = (uint8_t)(rook_green * trail_brightness);
            rook_blue = (uint8_t)(rook_blue * trail_brightness);
            
            led_set_pixel_safe(king_current, king_red, king_green, king_blue);
            led_set_pixel_safe(rook_current, rook_red, rook_green, rook_blue);
        }
        
        vTaskDelay(pdMS_TO_TICKS(60)); // Smooth animation
    }
    
    // Final highlight on destination squares
    for (int burst = 0; burst < 3; burst++) {
        led_clear_board_only();
        
        float brightness = 0.5f + 0.5f * sin(burst * 2.09f);
        led_set_pixel_safe(king_to, (uint8_t)(255 * brightness), (uint8_t)(215 * brightness), 0);
        led_set_pixel_safe(rook_to, (uint8_t)(192 * brightness), (uint8_t)(192 * brightness), (uint8_t)(192 * brightness));
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    led_clear_board_only();
    ESP_LOGI(TAG, "🏰 Castling animation completed");
}

void led_anim_promote(const led_command_t* cmd) {
    if (!cmd) return;
    
    ESP_LOGI(TAG, "👑 Starting NON-BLOCKING promotion animation");
    
    uint8_t promotion_led = cmd->led_index;
    
    // ✅ OPRAVA: Non-blocking promotion animation using unified_animation_manager
    uint32_t anim_id = unified_animation_create(ANIM_TYPE_PROMOTION, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = animation_start_promotion(anim_id, promotion_led);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Non-blocking promotion animation started (ID: %lu)", anim_id);
        } else {
            ESP_LOGE(TAG, "❌ Failed to start promotion animation: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to create promotion animation");
    }
}

/**
 * @brief Booting animation - progressive LED illumination
 */
void led_booting_animation(void)
{
    ESP_LOGI(TAG, "🌟 Starting booting animation...");
    
    // Clear all LEDs first
    led_clear_all_safe();
    
    // Progressive illumination of board LEDs (0-63)
    for (int brightness = 0; brightness <= 100; brightness += 5) {
        for (int led = 0; led < 64; led++) {
            // Calculate color based on position
            uint8_t row = led / 8;
            uint8_t col = led % 8;
            
            // Chess board pattern colors
            uint8_t r, g, b;
            if ((row + col) % 2 == 0) {
                // Light squares - warm white
                r = (uint8_t)(255 * brightness / 100);
                g = (uint8_t)(240 * brightness / 100);
                b = (uint8_t)(200 * brightness / 100);
            } else {
                // Dark squares - cool white
                r = (uint8_t)(200 * brightness / 100);
                g = (uint8_t)(220 * brightness / 100);
                b = (uint8_t)(255 * brightness / 100);
            }
            
            led_set_pixel_safe(led, r, g, b);
        }
        
        // Show current brightness
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(50));
        
        // Reset WDT during animation
        esp_task_wdt_reset();
    }
    
    // Brief pause at full brightness
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Fade out
    for (int brightness = 100; brightness >= 0; brightness -= 10) {
        for (int led = 0; led < 64; led++) {
            uint8_t row = led / 8;
            uint8_t col = led % 8;
            
            uint8_t r, g, b;
            if ((row + col) % 2 == 0) {
                r = (uint8_t)(255 * brightness / 100);
                g = (uint8_t)(240 * brightness / 100);
                b = (uint8_t)(200 * brightness / 100);
            } else {
                r = (uint8_t)(200 * brightness / 100);
                g = (uint8_t)(220 * brightness / 100);
                b = (uint8_t)(255 * brightness / 100);
            }
            
            led_set_pixel_safe(led, r, g, b);
        }
        
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(30));
        esp_task_wdt_reset();
    }
    
    // Clear all LEDs
    led_clear_all_safe();
    ESP_LOGI(TAG, "🌟 Booting animation completed");
}

void led_anim_endgame(const led_command_t* cmd) {
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏆 Starting NON-BLOCKING endgame animation");
    
    // Get animation style from data (0=wave, 1=circles, 2=cascade, 3=fireworks)
    uint8_t style = (cmd->data ? *((uint8_t*)cmd->data) : 0);
    
    // Set global endgame state
    endgame_animation_active = true;
    endgame_winner = PLAYER_WHITE; // Default winner, should be set by game logic
    endgame_animation_style = style;
    endgame_animation_frame = 0;
    
    // Reset WDT at start
    esp_task_wdt_reset();
    
    // Actually create and start the animation using unified_animation_manager
    animation_type_t anim_type;
    switch (style) {
        case 0: anim_type = ANIM_TYPE_ENDGAME_WAVE; break;
        case 1: anim_type = ANIM_TYPE_ENDGAME_CIRCLES; break;
        case 2: anim_type = ANIM_TYPE_ENDGAME_CASCADE; break;
        case 3: anim_type = ANIM_TYPE_ENDGAME_FIREWORKS; break;
        default: anim_type = ANIM_TYPE_ENDGAME_WAVE; break;
    }
    
    uint32_t anim_id = unified_animation_create(anim_type, ANIM_PRIORITY_HIGH);
    if (anim_id != 0) {
        esp_err_t ret = ESP_OK;
        switch (style) {
            case 0: ret = animation_start_endgame_wave(anim_id, 27, 0); break;
            case 1: ret = animation_start_endgame_circles(anim_id, 27, 0); break;
            case 2: ret = animation_start_endgame_cascade(anim_id, 27, 0); break;
            case 3: ret = animation_start_endgame_fireworks(anim_id, 27, 0); break;
        }
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ Endgame animation started successfully (ID: %lu, style: %d)", anim_id, style);
        } else {
            ESP_LOGE(TAG, "❌ Failed to start endgame animation: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "❌ Failed to create endgame animation - no free slots");
    }
}

/**
 * @brief Stop endless endgame animation
 */
void led_stop_endgame_animation(void)
{
    ESP_LOGI(TAG, "🛑 Stopping endless endgame animation...");
    endgame_animation_active = false;
    led_clear_board_only();
    ESP_LOGI(TAG, "✅ Endless endgame animation stopped");
}

/**
 * @brief Setup animation after endgame - reset pieces to starting positions
 */
void led_setup_animation_after_endgame(void)
{
    ESP_LOGI(TAG, "🔄 Starting MATRIX-MONITORED setup animation after endgame...");
    
    // Step 1: Clear board
    led_clear_board_only();
    
    // Step 2: Show starting position setup
    ESP_LOGI(TAG, "🎯 Step 1: Setting up starting position");
    
    // White pieces (bottom)
    uint8_t white_pieces[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (int i = 0; i < 16; i++) {
        led_set_pixel_safe(white_pieces[i], 255, 255, 255); // White
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Black pieces (top)
    uint8_t black_pieces[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
    for (int i = 0; i < 16; i++) {
        led_set_pixel_safe(black_pieces[i], 0, 0, 0); // Black (dark)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Step 3: Wait for matrix monitoring to confirm pieces are in place
    ESP_LOGI(TAG, "⏳ Step 2: Waiting for matrix to confirm pieces are in place...");
    ESP_LOGI(TAG, "💡 Please place pieces on rows 1, 2, 7, 8 as shown by LEDs");
    
    // Keep showing the setup until matrix confirms all pieces are in place
    // This will be handled by the actual game logic when matrix scanning detects pieces
    
    // Step 4: Show legal moves for white
    ESP_LOGI(TAG, "🎯 Step 3: Highlighting legal moves for white");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Clear board and show legal moves
    led_clear_board_only();
    
    // Show some legal moves (example: pawn moves)
    uint8_t legal_moves[] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
    for (int i = 0; i < 16; i++) {
        led_set_pixel_safe(legal_moves[i], 0, 255, 0); // Green for legal moves
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    
    // Step 5: Final setup complete
    ESP_LOGI(TAG, "✅ MATRIX-MONITORED setup animation completed - ready for new game");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Clear board
    led_clear_board_only();
}

void led_anim_check(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Flashing red warning
    for (int flash = 0; flash < 6; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 0, 0); // Red
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    led_clear_board_only();
}

void led_anim_checkmate(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Flashing red and white warning
    for (int flash = 0; flash < 8; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(150));
        
        for (int i = 0; i < 64; i++) {
            if (flash % 2 == 0) {
                led_set_pixel_safe(i, 255, 0, 0); // Red
            } else {
                led_set_pixel_safe(i, 255, 255, 255); // White
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    led_clear_board_only();
}

// ============================================================================
// LED UTILITY FUNCTIONS
// ============================================================================

void led_set_pixel_safe(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (led_index >= 64) return;
    
    led_set_pixel_internal(led_index, red, green, blue);
}

void led_clear_all_safe(void)
{
    for (int i = 0; i < 64; i++) {
        led_set_pixel_internal(i, 0, 0, 0);
    }
}

void led_set_all_safe(uint8_t red, uint8_t green, uint8_t blue)
{
    for (int i = 0; i < 64; i++) {
        led_set_pixel_internal(i, red, green, blue);
    }
}

void led_clear_board_only(void)
{
    // Clear only board LEDs (0-63), preserve buttons (64+)
    for (int i = 0; i < 64; i++) {
        led_set_pixel_internal(i, 0, 0, 0);
    }
}

void led_clear_buttons_only(void)
{
    // Clear only button LEDs (64+), preserve board (0-63)
    for (int i = 64; i < 80; i++) {
        led_set_pixel_internal(i, 0, 0, 0);
    }
}

void led_preserve_buttons(void)
{
    // This function preserves button states during board operations
    // Implementation depends on your button LED management system
}

void led_show_legal_moves(const led_command_t* cmd)
{
    if (!cmd) return;
    
    // Show legal moves in green
    for (int i = 0; i < 64; i++) {
        if (cmd->data && ((uint8_t*)cmd->data)[i]) {
            led_set_pixel_safe(i, 0, 255, 0); // Green
        }
    }
}

void led_error_invalid_move(const led_command_t* cmd)
{
    if (!cmd) return;
    
    // Flash red for invalid move
    for (int flash = 0; flash < 3; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 0, 0); // Red
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    led_clear_board_only();
}

void led_error_return_piece(const led_command_t* cmd)
{
    if (!cmd) return;
    
    // Flash yellow for return piece
    for (int flash = 0; flash < 4; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(150));
        
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 255, 255, 0); // Yellow
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    led_clear_board_only();
}

void led_error_recovery(const led_command_t* cmd)
{
    if (!cmd) return;
    
    // Flash blue for recovery
    for (int flash = 0; flash < 5; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(100));
        
        for (int i = 0; i < 64; i++) {
            led_set_pixel_safe(i, 0, 0, 255); // Blue
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    led_clear_board_only();
}

void led_set_button_promotion_available(uint8_t button_id, bool available)
{
    if (button_id >= 4) return;
    
    uint8_t led_index = 64 + button_id;
    if (available) {
        led_set_pixel_internal(led_index, 0, 255, 0); // Green
    } else {
        led_set_pixel_internal(led_index, 0, 0, 255); // Blue
    }
}

void led_update_button_availability_from_game(void) {
    // Get current player and promotion status from game
    player_t current_player = PLAYER_WHITE; // Default, should be updated from game state
    bool white_promotion_possible = false; // Should be updated from game state
    bool black_promotion_possible = false; // Should be updated from game state
    
    // Update button LEDs based on current state
    for (int i = 0; i < 4; i++) {
        uint8_t led_index = 64 + i;
        
        if (current_player == PLAYER_WHITE && white_promotion_possible) {
            led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
        } else if (current_player == PLAYER_BLACK && black_promotion_possible) {
            led_set_pixel_internal(led_index, 0, 255, 0); // Green - available
        } else {
            led_set_pixel_internal(led_index, 0, 0, 255); // Blue - unavailable
        }
    }
    
    // ✅ CRITICAL: Force immediate update to ensure changes are visible
    led_force_immediate_update();
    
    ESP_LOGI(TAG, "✅ Button availability updated - White promotion: %s, Black promotion: %s", 
             white_promotion_possible ? "YES (green)" : "NO (blue)",
             black_promotion_possible ? "YES (green)" : "NO (blue)");
}

/**
 * @brief Get ANSI color code from RGB values
 * @param r Red component (0-255)
 * @param g Green component (0-255) 
 * @param b Blue component (0-255)
 * @return ANSI color escape sequence
 */
static const char* get_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    // Simple color mapping based on dominant color
    if (r > 200 && g < 100 && b < 100) return "\033[31m"; // Red
    if (g > 200 && r < 100 && b < 100) return "\033[32m"; // Green  
    if (b > 200 && r < 100 && g < 100) return "\033[34m"; // Blue
    if (r > 200 && g > 200 && b < 100) return "\033[33m"; // Yellow
    if (r > 200 && g < 100 && b > 200) return "\033[35m"; // Magenta
    if (r < 100 && g > 200 && b > 200) return "\033[36m"; // Cyan
    if (r > 200 && g > 200 && b > 200) return "\033[37m"; // White
    if (r < 50 && g < 50 && b < 50) return "\033[30m";    // Black
    return "\033[0m"; // Default (reset)
}

/**
 * @brief Update button LED state based on current conditions
 * @param button_id Button ID (0-8)
 */
static void led_update_button_led_state(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    // ✅ OPRAVA: Správné mapování button ID na LED indexy
    uint8_t led_index = led_get_button_led_index(button_id);
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Check if button is in blink state (0.5s after release)
    if (button_blinking[button_id] && 
        (current_time - button_release_time[button_id]) < 500) {
        // Blink red (toggle every 100ms)
        if (((current_time - button_release_time[button_id]) / 100) % 2 == 0) {
            led_set_pixel_internal(led_index, 255, 0, 0); // Red
        } else {
            led_set_pixel_internal(led_index, 0, 0, 0); // Off
        }
        return;
    }
    
    // Clear blink state if time expired
    if (button_blinking[button_id] && 
        (current_time - button_release_time[button_id]) >= 500) {
        button_blinking[button_id] = false;
    }
    
    // Set LED color based on state
    if (button_pressed[button_id]) {
        // Button is pressed - red
        led_set_pixel_internal(led_index, 255, 0, 0);
    } else if (button_available[button_id]) {
        // Button is available - green
        led_set_pixel_internal(led_index, 0, 255, 0);
    } else {
        // Button is not available - blue
        led_set_pixel_internal(led_index, 0, 0, 255);
    }
}

/**
 * @brief Set button pressed state
 * @param button_id Button ID (0-8)
 * @param pressed True if button is pressed
 */
static void led_set_button_pressed(uint8_t button_id, bool pressed)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    if (pressed) {
        button_pressed[button_id] = true;
        button_blinking[button_id] = false; // Stop any blinking
    } else {
        button_pressed[button_id] = false;
        button_release_time[button_id] = current_time;
        button_blinking[button_id] = true; // Start blinking
    }
    
    led_update_button_led_state(button_id);
    
    ESP_LOGI(TAG, "Button %d %s", button_id, pressed ? "PRESSED (red)" : "RELEASED");
}

/**
 * @brief Process button blink timers (call from main loop)
 */
static void led_process_button_blink_timers(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        if (button_blinking[i] && 
            (current_time - button_release_time[i]) >= 500) {
            // Blink period ended
            button_blinking[i] = false;
            led_update_button_led_state(i);
        }
    }
}

/**
 * @brief Commit all pending LED changes atomically with watchdog safety
 * This function ensures all LED updates are sent to hardware in one critical section
 */
static void led_commit_pending_changes(void)
{
    if (!led_changes_pending || !led_initialized || led_strip == NULL || simulation_mode) {
        return;
    }
    
    // ✅ Reset watchdog BEFORE critical section (only if registered)
    esp_err_t wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ Count changes for watchdog timing
    uint32_t changed_count = 0;
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (led_changed_flags[i]) {
            changed_count++;
        }
    }
    
    ESP_LOGD(TAG, "Committing %lu LED changes...", changed_count);
    
    // ✅ NO CRITICAL SECTION - let led_strip driver handle timing internally
    // Update ONLY changed LEDs - much more efficient
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (!led_changed_flags[i]) continue;
        
        // ✅ Watchdog reset every N LEDs for large updates (only if registered)
        if ((i % LED_WATCHDOG_RESET_INTERVAL) == 0) {
            wdt_ret = esp_task_wdt_reset();
            if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
                // Task not registered with TWDT yet - this is normal during startup
            }
        }
        
        uint32_t color = led_pending_changes[i];
        uint8_t red = (color >> 16) & 0xFF;
        uint8_t green = (color >> 8) & 0xFF;
        uint8_t blue = color & 0xFF;
        
        esp_err_t ret = led_strip_set_pixel(led_strip, i, red, green, blue);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set LED %d: %s", i, esp_err_to_name(ret));
            continue;
        }
        led_changed_flags[i] = false;
    }
    
    // ✅ Final watchdog reset before refresh (only if registered)
    wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ SINGLE REFRESH - driver handles timing
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret == ESP_OK) {
        led_changes_pending = false;
        ESP_LOGD(TAG, "LED batch update successful (%lu LEDs)", changed_count);
    } else {
        ESP_LOGE(TAG, "LED strip refresh failed: %s", esp_err_to_name(ret));
    }
    
    // ✅ Final reset after refresh (only if registered)
    wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
}

/**
 * @brief Force immediate LED update for critical operations
 * Use this only for startup tests or critical visual feedback
 */
void led_force_immediate_update(void)
{
    if (!led_initialized || led_strip == NULL || simulation_mode) {
        return;
    }
    
    // ✅ FORCE COMMIT ANY PENDING CHANGES
    led_commit_pending_changes();
}

/**
 * @brief Optimized batch commit with privileged mutex synchronization
 * This function provides maximum stability for LED updates
 */
static void led_privileged_batch_commit(void)
{
    if (!led_changes_pending || !led_initialized || led_strip == NULL || simulation_mode) {
        return;
    }
    
    // ✅ Privileged mutex synchronization with timeout
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) != pdTRUE) {
            ESP_LOGW(TAG, "Failed to take LED mutex for privileged batch commit");
            return;
        }
    }
    
    // ✅ Reset watchdog BEFORE critical section
    esp_err_t wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ Count changes for performance monitoring
    uint32_t changed_count = 0;
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (led_changed_flags[i]) {
            changed_count++;
        }
    }
    
    ESP_LOGD(TAG, "Privileged batch commit: %lu LED changes", changed_count);
    
    // ✅ Optimized batch update with watchdog safety
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (!led_changed_flags[i]) continue;
        
        // ✅ Watchdog reset every N LEDs for large updates
        if ((i % LED_WATCHDOG_RESET_INTERVAL) == 0) {
            wdt_ret = esp_task_wdt_reset();
            if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
                // Task not registered with TWDT yet - this is normal during startup
            }
        }
        
        uint32_t color = led_pending_changes[i];
        uint8_t red = (color >> 16) & 0xFF;
        uint8_t green = (color >> 8) & 0xFF;
        uint8_t blue = color & 0xFF;
        
        esp_err_t ret = led_strip_set_pixel(led_strip, i, red, green, blue);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set LED %d: %s", i, esp_err_to_name(ret));
            continue;
        }
        led_changed_flags[i] = false;
    }
    
    // ✅ Final watchdog reset before refresh
    wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ Single refresh with WS2812B timing handled by driver
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret == ESP_OK) {
        led_changes_pending = false;
        ESP_LOGD(TAG, "Privileged batch update successful (%lu LEDs)", changed_count);
    } else {
        ESP_LOGE(TAG, "LED strip refresh failed: %s", esp_err_to_name(ret));
    }
    
    // ✅ Final watchdog reset after refresh
    wdt_ret = esp_task_wdt_reset();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ Release privileged mutex
    if (led_unified_mutex != NULL) {
        xSemaphoreGive(led_unified_mutex);
    }
}

/**
 * @brief Set LED pixel with duration support
 * @param led_index LED index (0-72)
 * @param r Red component (0-255)
 * @param g Green component (0-255) 
 * @param b Blue component (0-255)
 * @param duration_ms Duration in milliseconds
 */
static void led_set_pixel_with_duration(uint8_t led_index, uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms)
{
    if (led_index >= CHESS_LED_COUNT_TOTAL) {
        ESP_LOGE(TAG, "Invalid LED index: %d", led_index);
        return;
    }
    
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            uint32_t new_color = (r << 16) | (g << 8) | b;
            uint32_t current_time = esp_timer_get_time() / 1000;
            
            // ✅ Uložit původní barvu pokud není aktivní duration
            if (!led_durations[led_index].is_active) {
                led_durations[led_index].original_color = led_states[led_index];
            }
            
            // ✅ Nastavit duration tracking
            led_durations[led_index].led_index = led_index;
            led_durations[led_index].duration_color = new_color;
            led_durations[led_index].start_time = current_time;
            led_durations[led_index].duration_ms = duration_ms;
            led_durations[led_index].is_active = true;
            led_durations[led_index].restore_original = true;
            
            // ✅ Aplikovat novou barvu
            led_states[led_index] = new_color;
            
            ESP_LOGD(TAG, "LED[%d] set with duration: RGB(%d,%d,%d) for %lums", 
                     led_index, r, g, b, duration_ms);
            
            xSemaphoreGive(led_unified_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to take LED mutex for duration operation");
        }
    }
}

/**
 * @brief Duration timer callback - zkontroluje expired durations
 * @param xTimer Timer handle
 */
static void led_duration_timer_callback(TimerHandle_t xTimer)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    bool state_changed = false;
    
    if (led_unified_mutex != NULL) {
        if (xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS) == pdTRUE) {
            for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
                if (led_durations[i].is_active) {
                    uint32_t elapsed = current_time - led_durations[i].start_time;
                    
                    // ✅ Duration vypršela?
                    if (elapsed >= led_durations[i].duration_ms) {
                        // Obnovit původní barvu
                        if (led_durations[i].restore_original) {
                            led_states[i] = led_durations[i].original_color;
                        }
                        
                        // Deaktivovat duration
                        led_durations[i].is_active = false;
                        state_changed = true;
                        
                        ESP_LOGD(TAG, "Duration expired for LED %d, restored original color", i);
                    }
                }
            }
            
            xSemaphoreGive(led_unified_mutex);
        }
    }
    
    // ✅ Trigger hardware update pokud se něco změnilo
    if (state_changed) {
        // ✅ Force commit changes to hardware after duration restoration
        led_force_immediate_update();
    }
}

/**
 * @brief Initialize duration management system
 */
static void led_init_duration_system(void)
{
    // ✅ Vymazat všechny duration states
    memset(led_durations, 0, sizeof(led_durations));
    
    // ✅ BEZPEČNÝ duration timer - 50ms interval for better visual smoothness
    led_duration_timer = xTimerCreate("led_duration", 
                                      pdMS_TO_TICKS(50), // 50ms interval - lepší vizuální plynulost
                                      pdTRUE,             // Auto-reload
                                      NULL, 
                                      led_duration_timer_callback);
    
    if (led_duration_timer != NULL) {
        xTimerStart(led_duration_timer, 0);
        ESP_LOGI(TAG, "✅ LED duration management system initialized");
        } else {
        ESP_LOGE(TAG, "❌ Failed to create LED duration timer");
        led_duration_system_enabled = false;
    }
}
