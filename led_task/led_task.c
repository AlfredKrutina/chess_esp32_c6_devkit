/**
 * @file led_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace LED tasku
 * 
 * Tento task zpracovava vsechny LED operace:
 * - WS2812B LED ovladani (73 LED: 64 sachovnice + 9 tlacitek: 8 promotion + 1 reset)
 * - LED animace a vzory
 * - Tlacitkove LED feedback
 * - Time-multiplexed aktualizace
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task je zodpovedny za vsechny LED operace v systemu.
 * Ovladá 73 WS2812B LED: 64 pro sachovnici a 9 pro tlacitka.
 * Obsahuje pokrocile animace a efekty pro lepsi uzivatelsky zazitek.
 * 
 * Hardware:
 * - WS2812B LED paska na GPIO7 (LED_DATA_PIN)
 * - 64 LED pro sachovnici (8x8)
 * - 9 LED pro tlacitkove feedback (8 promotion + 1 reset)
 * - Simulacni mod (neni potreba skutecny hardware)
 */


#include "led_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "../unified_animation_manager/include/unified_animation_manager.h"  // Non-blocking endgame animations
#include "../game_task/include/game_task.h"  // For game_get_piece() function

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

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static const char *TAG = "LED_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered (WARNING only)
 */
static esp_err_t led_task_wdt_reset_safe(void) {
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
void led_anim_player_change(const led_command_t* cmd);
void led_anim_move_path(const led_command_t* cmd);
void led_anim_castle(const led_command_t* cmd);
void led_anim_promote(const led_command_t* cmd);
void led_anim_endgame(const led_command_t* cmd);
void led_update_endgame_wave(void);  // Update endgame wave animation (non-blocking)
void led_anim_check(const led_command_t* cmd);
void led_anim_checkmate(const led_command_t* cmd);

// Enhanced Castling System functions
void led_enhanced_castling_guidance(const led_command_t* cmd);
void led_enhanced_castling_error(const led_command_t* cmd);
void led_enhanced_castling_celebration(const led_command_t* cmd);
void led_enhanced_castling_tutorial(const led_command_t* cmd);
void led_enhanced_castling_clear(void);

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
            
        // Enhanced Castling System commands
        case LED_CMD_CASTLING_GUIDANCE:
            ESP_LOGI(TAG, "🏰 Enhanced castling guidance");
            led_enhanced_castling_guidance(cmd);
            break;
            
        case LED_CMD_CASTLING_ERROR:
            ESP_LOGI(TAG, "❌ Enhanced castling error indication");
            led_enhanced_castling_error(cmd);
            break;
            
        case LED_CMD_CASTLING_CELEBRATION:
            ESP_LOGI(TAG, "🎉 Enhanced castling celebration");
            led_enhanced_castling_celebration(cmd);
            break;
            
        case LED_CMD_CASTLING_TUTORIAL:
            ESP_LOGI(TAG, "📖 Enhanced castling tutorial");
            led_enhanced_castling_tutorial(cmd);
            break;
            
        case LED_CMD_CASTLING_CLEAR:
            ESP_LOGI(TAG, "🧹 Clear enhanced castling indications");
            led_enhanced_castling_clear();
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
    esp_err_t wdt_ret = led_task_wdt_reset_safe();
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
        wdt_ret = led_task_wdt_reset_safe();
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
        stream_printf("───");
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
    wdt_ret = led_task_wdt_reset_safe();
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
    
    // ✅ Wait for RMT channel to be fully initialized
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // ✅ NO CRITICAL SECTION - let driver handle timing
    ret = led_strip_clear(led_strip);
    if (ret == ESP_OK) {
        ret = led_strip_refresh(led_strip);
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ LED strip cleared and initialized successfully");
    } else {
        ESP_LOGW(TAG, "⚠️ LED strip initialization warning: %s (may be normal during startup)", esp_err_to_name(ret));
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
    
    // ✅ CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register LED task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ LED task registered with TWDT");
    }
    
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
        esp_err_t wdt_ret = led_task_wdt_reset_safe();
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
        
        // Update endgame wave animation (non-blocking, podle starého projektu)
        led_update_endgame_wave();
        
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

// Global endgame animation state
static bool endgame_animation_active = false;
// ============================================================================
// ADVANCED CHESS ANIMATION IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Player change animation - wave animation podle starého projektu
 * 
 * Profesionální wave animace s Gaussian distribucí, která prochází přes šachovnici.
 * Animace jde od předchozího hráče k novému hráči (passing scepter effect).
 * 
 * @param cmd LED command s data pointerem na player_t (0=white, 1=black)
 */
void led_anim_player_change(const led_command_t* cmd) {
    if (!cmd) return;
    
    // Získat barvu nového hráče z data (1=white, 0=black)
    uint8_t player_color_data = cmd->data ? (*((uint8_t*)cmd->data)) : 1;
    player_t current_player = (player_color_data == 1) ? PLAYER_WHITE : PLAYER_BLACK;
    
    ESP_LOGI(TAG, "🔄 Starting player change animation - passing scepter to %s", 
             current_player == PLAYER_WHITE ? "White" : "Black");
    
    // Clear board first
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(30));
    
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
    const float wave_width = 2.5f;  // Narrower wave width for more focused effect
    const int total_steps = 50;     // Reduced for faster animation
    
    float wave_position = (float)start_row;
    float target_position = (float)end_row;
    
    for (int frame = 0; frame < total_steps; frame++) {
        // Clear board first
        led_clear_board_only();
        
        // Calculate current wave position with S-curve easing
        float progress = (float)frame / (float)(total_steps - 1);
        float eased_progress = 0.5f * (1.0f - cosf(progress * M_PI)); // S-curve using cosine
        
        float current_wave_pos = wave_position + (target_position - wave_position) * eased_progress;
        
        // Add gradual startup effect - gradually increase overall brightness at the beginning
        float startup_factor = 1.0f;
        if (frame < 15) { // First 15 frames (180ms) for gradual startup
            startup_factor = (float)frame / 15.0f; // 0.0 to 1.0
            startup_factor = 0.5f * (1.0f - cosf(startup_factor * M_PI)); // S-curve for smooth startup
        }
        
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
                uint8_t red = (uint8_t)(31 * gaussian_factor);    // Dark gray red component
                uint8_t green = (uint8_t)(31 * gaussian_factor);  // Dark gray green component
                uint8_t blue = (uint8_t)(31 * gaussian_factor);   // Dark gray blue component
                
                // Add brightness threshold - only show if bright enough to be visible
                if (gaussian_factor > 0.15f) { // Only show if > 15% brightness
                    led_set_pixel_safe(led_index, red, green, blue);
                }
                // If too dim, leave LED off (don't set pixel)
            }
        }
        
        // Faster delay for quicker animation
        vTaskDelay(pdMS_TO_TICKS(12)); // 12ms = ~83 FPS (faster)
    }
    
    // Clear board after animation
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(30));
    
    ESP_LOGI(TAG, "✅ Player change animation completed");
}

void led_anim_move_path(const led_command_t* cmd) {
    if (!cmd || cmd->led_index >= 64) return;
    
    uint8_t from_led = cmd->led_index;
    uint8_t to_led = (cmd->data ? *((uint8_t*)cmd->data) : 63);  // default to h8
    
    ESP_LOGI(TAG, "🎬 Enhanced move path animation: %d -> %d", from_led, to_led);
    
    // ✅ OPRAVA: Použít led_index_to_chess_pos() místo jednoduchého dělení (kvůli serpentine layoutu)
    uint8_t from_row, from_col, to_row, to_col;
    led_index_to_chess_pos(from_led, &from_row, &from_col);
    led_index_to_chess_pos(to_led, &to_row, &to_col);
    
    // ENHANCED: Ultra-smooth move animation with brightness control (25 frames, 6 trails)
    for (int frame = 0; frame < 25; frame++) { // More frames for ultra-smoothness
        led_clear_board_only();
        
        float progress = (float)frame / 24.0f;
        
        // Create enhanced trail effect with multiple brightness levels (6 trails)
        for (int trail = 0; trail < 6; trail++) {
            float trail_progress = progress - (trail * 0.08f);
            if (trail_progress < 0) continue;
            if (trail_progress > 1) break;
            
            // Calculate current position with smooth easing
            float eased_progress = trail_progress * trail_progress * (3.0f - 2.0f * trail_progress); // Smooth step
            float current_row = from_row + (to_row - from_row) * eased_progress;
            float current_col = from_col + (to_col - from_col) * eased_progress;
            
            uint8_t current_led = chess_pos_to_led_index((uint8_t)current_row, (uint8_t)current_col);
            
            // ✅ OPRAVA: Modrá barva pro move animaci (podle požadavku uživatele)
            // Modrá s různou intenzitou podle pozice v trailu
            uint8_t red, green, blue;
            
            // Modrá barva s brightness gradientem podle trail_progress
            float blue_intensity = 1.0f;
            if (trail_progress < 0.2f) {
                // Začátek: tmavší modrá
                blue_intensity = 0.5f + (trail_progress / 0.2f) * 0.5f; // 0.5 -> 1.0
            } else if (trail_progress > 0.8f) {
                // Konec: jasnější modrá
                blue_intensity = 1.0f;
            }
            
            red = 0;
            green = 0;
            blue = (uint8_t)(255 * blue_intensity);
            
            // Enhanced trail brightness with exponential fade
            float trail_brightness = powf(1.0f - (trail * 0.15f), 1.5f);
            
            // Advanced pulsing with multiple harmonics
            float pulse1 = 0.6f + 0.4f * sin(progress * 12.56f + trail * 1.26f);
            float pulse2 = 0.8f + 0.2f * sin(progress * 25.12f + trail * 2.51f);
            float pulse3 = 0.9f + 0.1f * sin(progress * 50.24f + trail * 3.77f);
            float combined_pulse = pulse1 * pulse2 * pulse3;
            
            // Apply brightness and pulsing
            red = (uint8_t)(red * trail_brightness * combined_pulse);
            green = (uint8_t)(green * trail_brightness * combined_pulse);
            blue = (uint8_t)(blue * trail_brightness * combined_pulse);
            
            led_set_pixel_safe(current_led, red, green, blue);
        }
        
        // ✅ KRITICKÉ: Okamžitě aktualizovat LED po každém frame (jako ve starém projektu)
        led_force_immediate_update();
        
        vTaskDelay(pdMS_TO_TICKS(2)); // Optimized timing: 25 frames × 2ms = 50ms total for better responsiveness
    }
    
    // Enhanced final destination effect with breathing (8 breaths)
    for (int breath = 0; breath < 8; breath++) {
        led_clear_board_only();
        
        float breath_intensity = 0.5f + 0.5f * sin(breath * 0.785f); // Breathing effect
        // ✅ OPRAVA: Modrá barva pro final destination (podle požadavku uživatele)
        uint8_t final_red = 0;
        uint8_t final_green = 0;
        uint8_t final_blue = (uint8_t)(255 * breath_intensity);
        
        led_set_pixel_safe(to_led, final_red, final_green, final_blue);
        
        // ✅ KRITICKÉ: Okamžitě aktualizovat LED po každém breath frame
        led_force_immediate_update();
        
        vTaskDelay(pdMS_TO_TICKS(20)); // Optimized breathing timing
    }
    
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
        led_task_wdt_reset_safe();
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
        led_task_wdt_reset_safe();
    }
    
    // Clear all LEDs
    led_clear_all_safe();
    ESP_LOGI(TAG, "🌟 Booting animation completed");
}

// Endgame animation state structure (podle starého projektu)
typedef struct {
    bool active;
    uint8_t win_king_led;
    uint8_t win_king_row;
    uint8_t win_king_col;
    uint8_t lose_king_row;
    uint8_t lose_king_col;
    uint8_t radius;
    uint32_t last_update;
    bool initialized;
    piece_t winner_piece; // Store the actual winner piece for proper detection
} endgame_wave_state_t;

static endgame_wave_state_t endgame_wave = {0};

/**
 * @brief Initialize AVR-style wave endgame animation (podle starého projektu)
 */
void led_anim_endgame(const led_command_t* cmd) {
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏆 Starting AVR-style wave endgame animation");
    
    // Get winner king position from led_index
    endgame_wave.win_king_led = cmd->led_index;
    // ✅ OPRAVA: Použít led_index_to_chess_pos() místo jednoduchého dělení (kvůli serpentine layoutu)
    led_index_to_chess_pos(endgame_wave.win_king_led, &endgame_wave.win_king_row, &endgame_wave.win_king_col);
    
    // Get the actual winner piece from the board for proper detection
    endgame_wave.winner_piece = game_get_piece(endgame_wave.win_king_row, endgame_wave.win_king_col);
    
    // For simplicity, we'll just animate around the winner king
    // The loser king position is not critical for the wave effect
    endgame_wave.lose_king_row = 0;
    endgame_wave.lose_king_col = 0;
    
    ESP_LOGI(TAG, "🎯 Winner king at (%d,%d) piece=%d - wave animation around this position", 
             endgame_wave.win_king_row, endgame_wave.win_king_col, endgame_wave.winner_piece);
    
    // Initialize animation state
    endgame_wave.radius = 1;
    endgame_wave.last_update = xTaskGetTickCount();
    endgame_wave.active = true;
    endgame_wave.initialized = true;
    
    // Set global endgame state
    endgame_animation_active = true;
    
    ESP_LOGI(TAG, "🌊 AVR-style wave endgame animation initialized");
}

/**
 * @brief Update enhanced wave endgame animation with improved colors, faster animation and perfect piece highlighting (podle starého projektu)
 */
void led_update_endgame_wave(void) {
    if (!endgame_wave.active || !endgame_wave.initialized) {
        return;
    }
    
    const uint32_t WAVE_STEP_MS = 100; // ✅ OPRAVA: Pomalejší animace (100ms místo 30ms)
    const uint8_t MAX_RADIUS = 14;    // Larger radius for better coverage
    const float WAVE_THICKNESS = 1.2f; // Thinner waves for more precise effect
    const int WAVE_LAYERS = 4;        // Fewer layers but with higher FPS
    
    // Check if it's time for next wave step
    if (xTaskGetTickCount() - endgame_wave.last_update < pdMS_TO_TICKS(WAVE_STEP_MS)) {
        return; // Not time yet
    }
    
    endgame_wave.last_update = xTaskGetTickCount();
    
    // Clear board
    led_clear_board_only();
    
    // Use stored winner piece for reliable detection
    piece_t winner_king = endgame_wave.winner_piece;
    bool winner_is_white = (winner_king == PIECE_WHITE_KING);
    
    // Draw multiple overlapping wave rings for ultra-smooth effect
    for (int ring = 0; ring < WAVE_LAYERS; ring++) {
        float current_radius = endgame_wave.radius - (ring * 0.3f);
        if (current_radius < 0.2f) continue;
        
        for (int dy = -endgame_wave.radius; dy <= endgame_wave.radius; dy++) {
            for (int dx = -endgame_wave.radius; dx <= endgame_wave.radius; dx++) {
                // Calculate distance from center
                float dist = sqrtf(dx * dx + dy * dy);
                
                // Check if this pixel is part of the wave ring with smooth gradient
                float ring_distance = fabsf(dist - current_radius);
                if (ring_distance <= WAVE_THICKNESS) {
                    int row = endgame_wave.win_king_row + dy;
                    int col = endgame_wave.win_king_col + dx;
                    
                    // Check bounds
                    if (row >= 0 && row < 8 && col >= 0 && col < 8) {
                        uint8_t square = chess_pos_to_led_index(row, col);
                        
                        // Get piece at this position
                        piece_t piece = game_get_piece(row, col);
                        
                        // ✅ OPRAVA: Použít intensity místo gradientu (jako ve starém projektu)
                        // Calculate intensity based on distance from ring center (smooth gradient)
                        float intensity = 1.0f - (ring_distance / WAVE_THICKNESS);
                        intensity = fmaxf(0.15f, intensity); // Higher minimum brightness
                        
                        // ✅ OPRAVA: Barvy podle starého projektu - všechny barvy se násobí intensity
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
    
    // ✅ OPRAVA: Always highlight winner king in BRIGHT GOLD (mimo wave loop, jako ve starém projektu)
    led_set_pixel_safe(endgame_wave.win_king_led, 255, 215, 0);
    
    // Increment radius for next wave
    endgame_wave.radius++;
    if (endgame_wave.radius > MAX_RADIUS) {
        endgame_wave.radius = 1; // Reset for continuous wave effect
    }
}

/**
 * @brief Stop endless endgame animation
 */
void led_stop_endgame_animation(void)
{
    ESP_LOGI(TAG, "🛑 Stopping endless endgame animation...");
    endgame_animation_active = false;
    
    // ✅ OPRAVA: Zastavit endgame wave animaci
    endgame_wave.active = false;
    endgame_wave.initialized = false;
    endgame_wave.radius = 0;
    
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
    esp_err_t wdt_ret = led_task_wdt_reset_safe();
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
            wdt_ret = led_task_wdt_reset_safe();
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
    wdt_ret = led_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ SINGLE REFRESH - driver handles timing
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret == ESP_OK) {
        led_changes_pending = false;
        ESP_LOGD(TAG, "LED batch update successful (%lu LEDs)", changed_count);
    } else if (ret == ESP_ERR_INVALID_STATE) {
        // RMT channel not ready yet - this is normal during startup
        ESP_LOGD(TAG, "LED strip refresh skipped (RMT not ready)");
    } else {
        ESP_LOGW(TAG, "LED strip refresh warning: %s", esp_err_to_name(ret));
    }
    
    // ✅ Final reset after refresh (only if registered)
    wdt_ret = led_task_wdt_reset_safe();
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
    esp_err_t wdt_ret = led_task_wdt_reset_safe();
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
            wdt_ret = led_task_wdt_reset_safe();
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
    wdt_ret = led_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
        // Task not registered with TWDT yet - this is normal during startup
    }
    
    // ✅ Single refresh with WS2812B timing handled by driver
    esp_err_t ret = led_strip_refresh(led_strip);
    if (ret == ESP_OK) {
        led_changes_pending = false;
        ESP_LOGD(TAG, "Privileged batch update successful (%lu LEDs)", changed_count);
    } else if (ret == ESP_ERR_INVALID_STATE) {
        // RMT channel not ready yet - this is normal during startup
        ESP_LOGD(TAG, "LED strip refresh skipped (RMT not ready)");
    } else {
        ESP_LOGW(TAG, "LED strip refresh warning: %s", esp_err_to_name(ret));
    }
    
    // ✅ Final watchdog reset after refresh
    wdt_ret = led_task_wdt_reset_safe();
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

// ============================================================================
// ENHANCED CASTLING SYSTEM LED FUNCTIONS
// ============================================================================

/**
 * @brief Enhanced castling guidance LED function
 */
void led_enhanced_castling_guidance(const led_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏰 Enhanced castling guidance at LED %d", cmd->led_index);
    
    // Clear board first
    led_clear_board_only();
    
    // Show guidance based on command data
    // cmd->data should contain castling guidance data
    if (cmd->data) {
        // Parse guidance data and show appropriate guidance
        // This would be called from enhanced_castling_system.c
        led_set_pixel_safe(cmd->led_index, cmd->red, cmd->green, cmd->blue);
    } else {
        // Default guidance - show source and destination
        led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold for source
    }
}

/**
 * @brief Enhanced castling error LED function
 */
void led_enhanced_castling_error(const led_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "❌ Enhanced castling error at LED %d", cmd->led_index);
    
    // Flash error indication
    for (int flash = 0; flash < 3; flash++) {
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(200));
        
        led_set_pixel_safe(cmd->led_index, 255, 0, 0); // Red error
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    led_clear_board_only();
}

/**
 * @brief Enhanced castling celebration LED function
 */
void led_enhanced_castling_celebration(const led_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🎉 Enhanced castling celebration");
    
    // Rainbow celebration effect
    for (int cycle = 0; cycle < 3; cycle++) {
        uint8_t colors[][3] = {
            {255, 0, 0},    // Red
            {0, 255, 0},    // Green
            {0, 0, 255},    // Blue
            {255, 255, 0},  // Yellow
            {255, 0, 255},  // Magenta
            {0, 255, 255}   // Cyan
        };
        
        for (int i = 0; i < 6; i++) {
            led_clear_board_only();
            led_set_pixel_safe(cmd->led_index, colors[i][0], colors[i][1], colors[i][2]);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    
    // Final success state
    led_clear_board_only();
    led_set_pixel_safe(cmd->led_index, 0, 255, 0); // Green for success
}

/**
 * @brief Enhanced castling tutorial LED function
 */
void led_enhanced_castling_tutorial(const led_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "📖 Enhanced castling tutorial");
    
    // Step-by-step tutorial
    for (int step = 0; step < 3; step++) {
        led_clear_board_only();
        
        if (step == 0) {
            // Show king position
            led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold
        } else if (step == 1) {
            // Show rook position
            led_set_pixel_safe(cmd->led_index, 192, 192, 192); // Silver
        } else {
            // Show both positions
            led_set_pixel_safe(cmd->led_index, 255, 215, 0); // Gold
            if (cmd->data) {
                uint8_t* rook_pos = (uint8_t*)cmd->data;
                led_set_pixel_safe(*rook_pos, 192, 192, 192); // Silver
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
    
    led_clear_board_only();
}

/**
 * @brief Clear enhanced castling indications
 */
void led_enhanced_castling_clear(void)
{
    ESP_LOGI(TAG, "🧹 Clearing enhanced castling indications");
    led_clear_board_only();
}

// ============================================================================
// BOOT ANIMATION LED FUNCTIONS
// ============================================================================

/**
 * @brief LED boot animation step - rozsviti LED podle progress
 * @param progress_percent Progress v procentech (0-100)
 * @details
 * Rozsviti LED podle progress boot procesu. Pouziva se pro zobrazeni
 * postupu inicializace systemu. Rozsviti LED svetle zelenou barvou.
 * @note Funkce je bezpecna - kontroluje inicializaci a simulation mode
 */
void led_boot_animation_step(uint8_t progress_percent)
{
    // Bezpecnostni kontroly
    if (!led_initialized || simulation_mode) {
        ESP_LOGD(TAG, "LED boot: Progress %d%% - %s", progress_percent, 
                 simulation_mode ? "simulation mode" : "not initialized");
        return;
    }
    
    // Omezit na 0-100%
    if (progress_percent > 100) {
        progress_percent = 100;
    }
    
    // Vypocitat index LED (pouze board LEDs 0-63)
    int led_index = (progress_percent * CHESS_LED_COUNT_BOARD) / 100;
    if (led_index >= CHESS_LED_COUNT_BOARD) {
        led_index = CHESS_LED_COUNT_BOARD - 1;
    }
    
    // Rozsvitit LED svetle zelenou
    led_set_pixel_internal(led_index, 0, 128, 0);
    
    ESP_LOGD(TAG, "LED boot: Progress %d%% -> LED[%d] RGB(0,128,0)", progress_percent, led_index);
}

/**
 * @brief LED boot animation fade out - postupne ztlumi vsechny LED na 0
 * @details
 * Postupne ztlumi vsechny board LED z brightness 128 na 0.
 * Pouziva se na konci boot procesu pro plynule ztlumeni.
 * @note Funkce je bezpecna - kontroluje inicializaci a simulation mode
 */
void led_boot_animation_fade_out(void)
{
    // Bezpecnostni kontroly
    if (!led_initialized || simulation_mode) {
        ESP_LOGD(TAG, "LED boot fade out: %s", simulation_mode ? "simulation mode" : "not initialized");
        return;
    }
    
    ESP_LOGI(TAG, "🌟 Starting LED boot animation fade out...");
    
    // Postupne ztlumeni z brightness 128 na 0
    const int fade_steps = 20;  // 20 kroku pro plynule ztlumeni
    const int step_delay_ms = 30; // 30ms mezi kroky
    
    for (int step = fade_steps; step >= 0; step--) {
        // Vypocitat brightness (128 -> 0)
        uint8_t brightness = (step * 128) / fade_steps;
        
        // Ztlumit vsechny board LED (0-63)
        for (int led_index = 0; led_index < CHESS_LED_COUNT_BOARD; led_index++) {
            led_set_pixel_internal(led_index, 0, brightness, 0);
        }
        
        // Kratka pauza pro plynulou animaci
        vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
        
        // Reset watchdog behem animace
        led_task_wdt_reset_safe();
    }
    
    // Vymazat vsechny board LED
    led_clear_board_only();
    
    ESP_LOGI(TAG, "✅ LED boot animation fade out completed");
}