/**
 * @file button_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace Button tasku
 * 
 * Tento task zpracovava vstup z tlacitek a feedback:
 * - 9 stavu tlacitek (promotion + reset)
 * - Debouncing tlacitek a detekce udalosti
 * - LED feedback pro stavy tlacitek
 * - Generovani tlacitkovych udalosti
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task zpracovava vsechna tlacitka na sachovnici. Detekuje
 * stisknuti tlacitek, provadi debouncing a posila udalosti do
 * game tasku. Také ovladá LED feedback pro tlacitka.
 * 
 * Hardware:
 * - 9 tlacitek celkem
 * - Promotion tlacitka A: Dama, Vez, Strelec, Kun
 * - Promotion tlacitka B: Dama, Vez, Strelec, Kun
 * - Reset tlacitko: GPIO4 (samostatny pin)
 * - Tlacitkove LED: WS2812B indexy 64-72
 * - Simulacni mod (neni potreba skutecny hardware)
 */


#include "button_task.h"
#include "freertos_chess.h"
#include "led_task_simple.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>


static const char *TAG = "BUTTON_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered (WARNING only)
 */
static esp_err_t button_task_wdt_reset_safe(void) {
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


// Button configuration
#define BUTTON_DEBOUNCE_MS       50      // Debounce time
#define BUTTON_LONG_PRESS_MS     1000    // Long press threshold
#define BUTTON_DOUBLE_PRESS_MS   300     // Double press window

// Button state tracking
static bool button_states[CHESS_BUTTON_COUNT] = {false}; // Current button states
static bool button_previous[CHESS_BUTTON_COUNT] = {false}; // Previous button states
static uint32_t button_press_time[CHESS_BUTTON_COUNT] = {0}; // Press start time
static uint32_t button_release_time[CHESS_BUTTON_COUNT] = {0}; // Release time
static uint8_t button_press_count[CHESS_BUTTON_COUNT] = {0}; // Press count for double press
static bool button_long_press_sent[CHESS_BUTTON_COUNT] = {false}; // Long press event sent

// Task state
static bool task_running = false;
static bool simulation_mode = false; // Changed to false for real hardware operation

// Button names for logging
// ✅ OPRAVA: 8 promotion buttons (4 for each player) + 1 reset button
static const char* button_names[] = {
    "White Promotion Queen", "White Promotion Rook", "White Promotion Bishop", "White Promotion Knight",  // 0-3: White promotion buttons
    "Black Promotion Queen", "Black Promotion Rook", "Black Promotion Bishop", "Black Promotion Knight",  // 4-7: Black promotion buttons
    "Reset"                                                                                                // 8: Reset button
};

// Button LED indices - REMOVED
// LED indications are now controlled by game_task via game_check_promotion_needed()
// LED mapping:
//   64-67: White promotion buttons (Queen, Rook, Bishop, Knight)
//   68-71: Black promotion buttons (Queen, Rook, Bishop, Knight)
//   72:    Reset button

// Button colors (RGB values) - kept for reference, not used in new LED logic
// static const uint32_t button_colors[] = {
//     0xFF00FF, // Queen - Magenta
//     0xFF0000, // Rook - Red
//     0x00FFFF, // Bishop - Cyan
//     0xFF8000, // Knight - Orange
//     0xFF00FF, // Promotion Queen - Magenta
//     0xFF0000, // Promotion Rook - Red
//     0x00FFFF, // Promotion Bishop - Cyan
//     0xFF8000, // Promotion Knight - Orange
//     0xFF0000  // Reset - Red
// };

// Simulation state
static uint32_t last_simulation_time = 0;
static uint8_t current_simulation_button = 0;
static uint32_t button_simulate_release_time = 0;


// ============================================================================
// BUTTON SCANNING FUNCTIONS
// ============================================================================


void button_scan_all(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // In simulation mode, simulate button presses
    if (simulation_mode) {
        // Simulate button presses every 5 seconds
        if (current_time - last_simulation_time > 5000) {
            last_simulation_time = current_time;
            
            // Cycle through buttons
            current_simulation_button = (current_simulation_button + 1) % CHESS_BUTTON_COUNT;
            
            // Simulate button press
            button_simulate_press(current_simulation_button);
            
            // Simulate button release after 200ms using non-blocking approach
            // Store release time instead of blocking
            button_simulate_release_time = current_time + 200;
        }
        
        // Check if it's time to release the simulated button
        if (button_simulate_release_time > 0 && current_time >= button_simulate_release_time) {
            button_simulate_release(current_simulation_button);
            button_simulate_release_time = 0; // Reset release time
        }
        
        return;
    }
    
    // REAL HARDWARE BUTTON SCANNING
    // Scan physical button pins and update button_states
    // 
    // Physical buttons:
    // - 4 promotion buttons (MATRIX_COL_0-3): button_id 0-3
    // - 1 reset button (GPIO27): button_id 8
    // 
    // LED indications (9 LEDs):
    // - LED 64-67: White promotion (button_id 0-3 visual)
    // - LED 68-71: Black promotion (button_id 4-7 visual)  
    // - LED 72: Reset (button_id 8)
    
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        bool current_state = false;
        
        if (i == 8) {
            // Reset button (GPIO27) - active low
            current_state = (gpio_get_level(BUTTON_RESET) == 0);
            
        } else if (i >= 0 && i <= 3) {
            // Promotion buttons (0-3): MATRIX_COL_0-3
            // These pins are time-multiplexed with matrix columns
            // During button scan window (20-25ms), they can be read as buttons
            // Active low (pulled up, buttons pull to ground)
            const gpio_num_t promotion_pins[] = {
                BUTTON_QUEEN,  // MATRIX_COL_0
                BUTTON_ROOK,   // MATRIX_COL_1
                BUTTON_BISHOP, // MATRIX_COL_2
                BUTTON_KNIGHT  // MATRIX_COL_3
            };
            current_state = (gpio_get_level(promotion_pins[i]) == 0);
            
        } else {
            // button_id 4-7: No physical buttons (only LED indications)
            current_state = false;
        }
        
        button_states[i] = current_state;
    }
}


void button_simulate_press(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    ESP_LOGI(TAG, "Simulating button press: %s (ID: %d)", button_names[button_id], button_id);
    
    button_states[button_id] = true;
    button_press_time[button_id] = esp_timer_get_time() / 1000;
    button_long_press_sent[button_id] = false;
    
    // Send button press event
    button_send_event(button_id, BUTTON_EVENT_PRESS, 0);
    
    // Update button LED feedback
    button_update_led_feedback(button_id, true);
}


void button_simulate_release(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    uint32_t press_duration = esp_timer_get_time() / 1000 - button_press_time[button_id];
    
    ESP_LOGI(TAG, "Simulating button release: %s (ID: %d, duration: %lu ms)", 
              button_names[button_id], button_id, press_duration);
    
    button_states[button_id] = false;
    button_release_time[button_id] = esp_timer_get_time() / 1000;
    
    // Check for long press
    if (press_duration >= BUTTON_LONG_PRESS_MS && !button_long_press_sent[button_id]) {
        button_send_event(button_id, BUTTON_EVENT_LONG_PRESS, press_duration);
        button_long_press_sent[button_id] = true;
    }
    
    // Send button release event
    button_send_event(button_id, BUTTON_EVENT_RELEASE, press_duration);
    
    // Update button LED feedback
    button_update_led_feedback(button_id, false);
    
    // Check for double press
    button_check_double_press(button_id);
}


// ============================================================================
// BUTTON EVENT PROCESSING
// ============================================================================


void button_process_events(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // Process button state changes
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        if (button_states[i] != button_previous[i]) {
            if (button_states[i]) {
                // Button pressed
                button_handle_press(i);
            } else {
                // Button released
                button_handle_release(i);
            }
            
            button_previous[i] = button_states[i];
        }
        
        // Check for long press
        if (button_states[i] && !button_long_press_sent[i]) {
            uint32_t press_duration = current_time - button_press_time[i];
            if (press_duration >= BUTTON_LONG_PRESS_MS) {
                button_send_event(i, BUTTON_EVENT_LONG_PRESS, press_duration);
                button_long_press_sent[i] = true;
            }
        }
    }
}


void button_handle_press(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    ESP_LOGI(TAG, "Button pressed: %s (ID: %d)", button_names[button_id], button_id);
    
    button_press_time[button_id] = esp_timer_get_time() / 1000;
    button_long_press_sent[button_id] = false;
    
    // Send button press event
    button_send_event(button_id, BUTTON_EVENT_PRESS, 0);
    
    // Update button LED feedback
    button_update_led_feedback(button_id, true);
}


void button_handle_release(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    uint32_t press_duration = esp_timer_get_time() / 1000 - button_press_time[button_id];
    
    ESP_LOGI(TAG, "Button released: %s (ID: %d, duration: %lu ms)", 
              button_names[button_id], button_id, press_duration);
    
    button_release_time[button_id] = esp_timer_get_time() / 1000;
    
    // Check for long press
    if (press_duration >= BUTTON_LONG_PRESS_MS && !button_long_press_sent[button_id]) {
        button_send_event(button_id, BUTTON_EVENT_LONG_PRESS, press_duration);
        button_long_press_sent[button_id] = true;
    }
    
    // Send button release event
    button_send_event(button_id, BUTTON_EVENT_RELEASE, press_duration);
    
    // Update button LED feedback
    button_update_led_feedback(button_id, false);
    
    // Check for double press
    button_check_double_press(button_id);
}


void button_check_double_press(uint8_t button_id)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t time_since_last_release = current_time - button_release_time[button_id];
    
    if (time_since_last_release <= BUTTON_DOUBLE_PRESS_MS) {
        button_press_count[button_id]++;
        
        if (button_press_count[button_id] == 2) {
            ESP_LOGI(TAG, "Double press detected: %s (ID: %d)", button_names[button_id], button_id);
            
            // Send double press event
            button_send_event(button_id, BUTTON_EVENT_DOUBLE_PRESS, 0);
            
            // Reset press count
            button_press_count[button_id] = 0;
        }
    } else {
        // Reset press count if too much time has passed
        button_press_count[button_id] = 1;
    }
}


void button_send_event(uint8_t button_id, button_event_type_t event_type, uint32_t duration)
{
    if (button_id >= CHESS_BUTTON_COUNT) return;
    
    // Create button event
    button_event_t event = {
        .type = event_type,
        .button_id = button_id,
        .press_duration_ms = duration,
        .timestamp = esp_timer_get_time() / 1000
    };
    
    // Send to button event queue
    if (button_event_queue != NULL) {
        if (xQueueSend(button_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Button event sent: %s (ID: %d, type: %d)", 
                      button_names[button_id], button_id, event_type);
        } else {
            ESP_LOGW(TAG, "Failed to send button event to queue");
        }
    } else {
        ESP_LOGW(TAG, "Button event queue not available");
    }
}


// ============================================================================
// BUTTON LED FEEDBACK FUNCTIONS
// ============================================================================


void button_update_led_feedback(uint8_t button_id, bool pressed)
{
    // ✅ LED feedback is now controlled by game_check_promotion_needed()
    // LED indications are:
    // - Green (0,255,0): Promotion is possible / button is active
    // - Blue (0,0,255): Promotion not possible / button is inactive
    // - Reset button (LED 72): Always green (always active)
    //
    // This function is kept empty to avoid interfering with game_task LED control
    // LED state is updated by game_check_promotion_needed() after each move
    
    (void)button_id;  // Suppress unused parameter warning
    (void)pressed;    // Suppress unused parameter warning
}


void button_set_led_color(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue)
{
    // ✅ DIRECT LED CALL - No queue hell
    led_set_pixel_safe(led_index, red, green, blue);
    ESP_LOGD(TAG, "Button LED %d set to RGB(%d,%d,%d)", led_index, red, green, blue);
}


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


void button_process_commands(void)
{
    uint8_t command;
    
    // Process commands from queue
    if (button_command_queue != NULL) {
        while (xQueueReceive(button_command_queue, &command, 0) == pdTRUE) {
            switch (command) {
                case 0: // Reset all buttons
                    button_reset_all();
                    break;
                    
                case 1: // Print button status
                    button_print_status();
                    break;
                    
                case 2: // Test all buttons
                    button_test_all();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown button command: %d", command);
                    break;
            }
        }
    }
}


void button_reset_all(void)
{
    ESP_LOGI(TAG, "Resetting all button states");
    
    // Clear all button states
    memset(button_states, 0, sizeof(button_states));
    memset(button_previous, 0, sizeof(button_previous));
    memset(button_press_time, 0, sizeof(button_press_time));
    memset(button_release_time, 0, sizeof(button_release_time));
    memset(button_press_count, 0, sizeof(button_press_count));
    memset(button_long_press_sent, 0, sizeof(button_long_press_sent));
    
    // Reset simulation state
    last_simulation_time = 0;
    current_simulation_button = 0;
    
    ESP_LOGI(TAG, "Button reset completed");
}


void button_print_status(void)
{
    ESP_LOGI(TAG, "Button Status:");
    
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        ESP_LOGI(TAG, "  Button %d (%s): %s", i, button_names[i], 
                 button_states[i] ? "PRESSED" : "released");
    }
    
    ESP_LOGI(TAG, "Simulation mode: %s", simulation_mode ? "enabled" : "disabled");
    ESP_LOGI(TAG, "Current simulation button: %d", current_simulation_button);
}


void button_test_all(void)
{
    ESP_LOGI(TAG, "Testing all buttons...");
    
    for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
        ESP_LOGI(TAG, "Testing button %d: %s", i, button_names[i]);
        
        // Simulate button press
        button_simulate_press(i);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Simulate button release
        button_simulate_release(i);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "Button test completed");
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void button_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Button task started successfully");
    
    // ✅ CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register Button task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ Button task registered with TWDT");
    }
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • 9 button handling (promotion + reset)");
    ESP_LOGI(TAG, "  • Button debouncing and event detection");
    ESP_LOGI(TAG, "  • LED feedback for button states");
    ESP_LOGI(TAG, "  • Long press and double press detection");
    ESP_LOGI(TAG, "  • Simulation mode (no HW required)");
    ESP_LOGI(TAG, "  • 5ms scan cycle");
    
    task_running = true;
    
    // Initialize button states
    button_reset_all();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for button task in every iteration
        esp_err_t wdt_ret = button_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Watchdog logging every 1000 iterations (5 seconds)
        if (loop_count % 1000 == 0) {
            ESP_LOGI(TAG, "Button Task Watchdog: loop=%d, heap=%zu", loop_count, esp_get_free_heap_size());
        }
        
        // Process button commands
        button_process_commands();
        
        // Button scanning is now handled by FreeRTOS timer
        // This task only processes commands and events
        button_process_events();
        
        // Periodic status update - reduced frequency for cleaner UART
        if (loop_count % 100000 == 0) { // Every 100000 loops (500 seconds)
            ESP_LOGI(TAG, "Button Task Status: loop=%d, simulation_button=%d", 
                     loop_count, current_simulation_button);
        }
        
        loop_count++;
        
        // Wait for next cycle (5ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(5));
    }
}
