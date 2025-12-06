/**
 * @file screen_saver_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace Screen Saver tasku
 * 
 * Tento task spravuje funkcionalitu screen saveru:
 * - Automaticka aktivace screen saveru
 * - Energeticky usporne LED vzory
 * - Detekce aktivity uzivatele
 * - Deaktivace screen saveru
 * - Vlastni vzory screen saveru
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Screen Saver task je zodpovedny za usporu energie pri neaktivite.
 * Detekuje kdy uzivatel nehraje a aktivuje energeticky usporne
 * LED vzory. Po detekci aktivity se screen saver vypne.
 * 
 * Funkce:
 * - Konfigurovatelne timeout periody
 * - Vice vzoru screen saveru
 * - Integrace detekce pohybu
 * - Sprava energie
 * - Plynule prechody
 */


#include "screen_saver_task.h"
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


static const char *TAG = "SCREEN_SAVER_TASK";


// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// Screen saver configuration
#define SCREEN_SAVER_TIMEOUT_MS    30000   // 30 seconds default timeout
#define SCREEN_SAVER_UPDATE_MS     200     // 200ms update interval
#define SCREEN_SAVER_FADE_MS       1000    // 1 second fade transition
#define MAX_SCREEN_SAVER_PATTERNS  10      // Maximum patterns

// Screen saver states






// Task state
static bool task_running = false;
static screen_saver_t screen_saver;

// Pattern data
static uint8_t pattern_frame[CHESS_LED_COUNT_TOTAL][3];  // RGB values for current frame
static uint32_t pattern_colors[8] = {
    0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,  // Red, Green, Blue, Yellow
    0xFF00FF, 0x00FFFF, 0xFF8000, 0x8000FF   // Magenta, Cyan, Orange, Purple
};

// Activity tracking
static uint32_t last_matrix_activity = 0;
static uint32_t last_button_activity = 0;
static uint32_t last_led_activity = 0;
static uint32_t last_uart_activity = 0;


// ============================================================================
// SCREEN SAVER INITIALIZATION FUNCTIONS
// ============================================================================


void screen_saver_initialize(void)
{
    ESP_LOGI(TAG, "Initializing screen saver system...");
    
    // Initialize screen saver structure
    screen_saver.state = SCREEN_SAVER_STATE_INACTIVE;
    screen_saver.current_pattern = PATTERN_MINIMAL;
    screen_saver.last_activity_time = esp_timer_get_time() / 1000;
    screen_saver.timeout_ms = SCREEN_SAVER_TIMEOUT_MS;
    screen_saver.pattern_start_time = 0;
    screen_saver.frame_count = 0;
    screen_saver.enabled = true;
    screen_saver.brightness = 50;  // 50% brightness for energy saving
    screen_saver.pattern_speed = 3;
    
    // Clear pattern frame
    memset(pattern_frame, 0, sizeof(pattern_frame));
    
    // Reset activity timestamps
    last_matrix_activity = esp_timer_get_time() / 1000;
    last_button_activity = esp_timer_get_time() / 1000;
    last_led_activity = esp_timer_get_time() / 1000;
    last_uart_activity = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Screen saver system initialized successfully");
    ESP_LOGI(TAG, "Timeout: %lu ms, Brightness: %d%%, Pattern: %d", 
              screen_saver.timeout_ms, screen_saver.brightness, screen_saver.current_pattern);
}


void screen_saver_set_timeout(uint32_t timeout_ms)
{
    if (timeout_ms >= 5000 && timeout_ms <= 300000) {  // 5s to 5min
        screen_saver.timeout_ms = timeout_ms;
        ESP_LOGI(TAG, "Screen saver timeout set to %lu ms", timeout_ms);
    } else {
        ESP_LOGW(TAG, "Invalid timeout value: %lu ms (must be 5000-300000)", timeout_ms);
    }
}


void screen_saver_set_brightness(uint8_t brightness)
{
    if (brightness <= 100) {
        screen_saver.brightness = brightness;
        ESP_LOGI(TAG, "Screen saver brightness set to %d%%", brightness);
    } else {
        ESP_LOGW(TAG, "Invalid brightness value: %d%% (must be 0-100)", brightness);
    }
}


void screen_saver_set_pattern(screen_saver_pattern_t pattern)
{
    if (pattern < MAX_SCREEN_SAVER_PATTERNS) {
        screen_saver.current_pattern = pattern;
        screen_saver.pattern_start_time = esp_timer_get_time() / 1000;
        screen_saver.frame_count = 0;
        ESP_LOGI(TAG, "Screen saver pattern set to %d", pattern);
    } else {
        ESP_LOGW(TAG, "Invalid pattern value: %d", pattern);
    }
}


// ============================================================================
// ACTIVITY DETECTION FUNCTIONS
// ============================================================================


void screen_saver_update_activity(activity_source_t source)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    switch (source) {
        case ACTIVITY_MATRIX:
            last_matrix_activity = current_time;
            break;
            
        case ACTIVITY_BUTTON:
            last_button_activity = current_time;
            break;
            
        case ACTIVITY_LED:
            last_led_activity = current_time;
            break;
            
        case ACTIVITY_UART:
            last_uart_activity = current_time;
            break;
            
        default:
            break;
    }
    
    // Update last activity time
    screen_saver.last_activity_time = current_time;
    
    // Deactivate screen saver if it was active
    if (screen_saver.state == SCREEN_SAVER_STATE_ACTIVE) {
        screen_saver_deactivate();
    }
}


bool screen_saver_check_timeout(void)
{
    if (!screen_saver.enabled) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t time_since_activity = current_time - screen_saver.last_activity_time;
    
    return (time_since_activity >= screen_saver.timeout_ms);
}


// ============================================================================
// SCREEN SAVER STATE MANAGEMENT
// ============================================================================


void screen_saver_activate(void)
{
    if (screen_saver.state == SCREEN_SAVER_STATE_ACTIVE) {
        return; // Already active
    }
    
    ESP_LOGI(TAG, "Activating screen saver (pattern: %d)", screen_saver.current_pattern);
    
    screen_saver.state = SCREEN_SAVER_STATE_TRANSITIONING;
    screen_saver.pattern_start_time = esp_timer_get_time() / 1000;
    screen_saver.frame_count = 0;
    
    // Fade out current display
    screen_saver_fade_out();
    
    // Start screen saver pattern
    screen_saver.state = SCREEN_SAVER_STATE_ACTIVE;
    
    ESP_LOGI(TAG, "Screen saver activated successfully");
}


void screen_saver_deactivate(void)
{
    if (screen_saver.state == SCREEN_SAVER_STATE_INACTIVE) {
        return; // Already inactive
    }
    
    ESP_LOGI(TAG, "Deactivating screen saver");
    
    screen_saver.state = SCREEN_SAVER_STATE_TRANSITIONING;
    
    // Fade in to normal display
    screen_saver_fade_in();
    
    // Return to normal state
    screen_saver.state = SCREEN_SAVER_STATE_INACTIVE;
    
    ESP_LOGI(TAG, "Screen saver deactivated successfully");
}


void screen_saver_fade_out(void)
{
    ESP_LOGI(TAG, "Fading out display for screen saver");
    
    // Gradually reduce brightness
    for (int brightness = 100; brightness >= screen_saver.brightness; brightness -= 5) {
        screen_saver_set_global_brightness(brightness);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


void screen_saver_fade_in(void)
{
    ESP_LOGI(TAG, "Fading in display from screen saver");
    
    // Gradually increase brightness
    for (int brightness = screen_saver.brightness; brightness <= 100; brightness += 5) {
        screen_saver_set_global_brightness(brightness);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Restore full brightness
    screen_saver_set_global_brightness(100);
}


void screen_saver_set_global_brightness(uint8_t brightness)
{
    // Send brightness command to LED task
    // Přímé volání LED funkce
    led_set_all_safe(brightness, brightness, brightness);
}


// ============================================================================
// SCREEN SAVER PATTERN FUNCTIONS
// ============================================================================


void screen_saver_generate_pattern(void)
{
    if (screen_saver.state != SCREEN_SAVER_STATE_ACTIVE) {
        return;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t pattern_time = current_time - screen_saver.pattern_start_time;
    
    switch (screen_saver.current_pattern) {
        case PATTERN_FIREWORKS:
            screen_saver_generate_fireworks(pattern_time);
            break;
            
        case PATTERN_STARS:
            screen_saver_generate_stars(pattern_time);
            break;
            
        case PATTERN_OCEAN:
            screen_saver_generate_ocean(pattern_time);
            break;
            
        case PATTERN_FOREST:
            screen_saver_generate_forest(pattern_time);
            break;
            
        case PATTERN_CITY:
            screen_saver_generate_city(pattern_time);
            break;
            
        case PATTERN_SPACE:
            screen_saver_generate_space(pattern_time);
            break;
            
        case PATTERN_GEOMETRIC:
            screen_saver_generate_geometric(pattern_time);
            break;
            
        case PATTERN_MINIMAL:
            screen_saver_generate_minimal(pattern_time);
            break;
            
        default:
            screen_saver_generate_minimal(pattern_time);
            break;
    }
    
    // Apply brightness reduction
    screen_saver_apply_brightness();
    
    // Send pattern to LEDs
    screen_saver_send_pattern_to_leds();
    
    screen_saver.frame_count++;
}


void screen_saver_generate_fireworks(uint32_t time)
{
    // Clear frame
    memset(pattern_frame, 0, sizeof(pattern_frame));
    
    // Generate random fireworks
    for (int i = 0; i < 5; i++) {
        int center_x = rand() % 8;
        int center_y = rand() % 8;
        uint32_t color = pattern_colors[rand() % 8];
        
        // Create explosion effect
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = -2; dy <= 2; dy++) {
                int x = center_x + dx;
                int y = center_y + dy;
                
                if (x >= 0 && x < 8 && y >= 0 && y < 8) {
                    int led_index = y * 8 + x;
                    float distance = sqrtf(dx * dx + dy * dy);
                    float intensity = 1.0f - (distance / 3.0f);
                    
                    if (intensity > 0) {
                        pattern_frame[led_index][0] = ((color >> 16) & 0xFF) * intensity;
                        pattern_frame[led_index][1] = ((color >> 8) & 0xFF) * intensity;
                        pattern_frame[led_index][2] = (color & 0xFF) * intensity;
                    }
                }
            }
        }
    }
}


void screen_saver_generate_stars(uint32_t time)
{
    // Clear frame
    memset(pattern_frame, 0, sizeof(pattern_frame));
    
    // Generate twinkling stars
    for (int i = 0; i < 20; i++) {
        int led_index = rand() % CHESS_LED_COUNT_TOTAL;
        uint32_t color = pattern_colors[rand() % 8];
        float twinkle = (sinf(time * 0.5f + i) + 1.0f) / 2.0f;
        
        pattern_frame[led_index][0] = ((color >> 16) & 0xFF) * twinkle;
        pattern_frame[led_index][1] = ((color >> 8) & 0xFF) * twinkle;
        pattern_frame[led_index][2] = (color & 0xFF) * twinkle;
    }
}


void screen_saver_generate_ocean(uint32_t time)
{
    // Generate ocean wave effect
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            float wave = sinf((col * 0.5f) + (time * 0.3f)) * 0.5f + 0.5f;
            
            uint8_t blue = 100 + (wave * 155);
            uint8_t green = 50 + (wave * 100);
            
            pattern_frame[led_index][0] = 0;      // No red
            pattern_frame[led_index][1] = green;
            pattern_frame[led_index][2] = blue;
        }
    }
}


void screen_saver_generate_forest(uint32_t time)
{
    // Generate forest animation
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            
            if (row < 4) {  // Sky
                uint8_t blue = 100 + (sinf(time * 0.2f + col * 0.1f) * 50);
                pattern_frame[led_index][0] = 0;
                pattern_frame[led_index][1] = 0;
                pattern_frame[led_index][2] = blue;
            } else {  // Trees
                uint8_t green = 50 + (sinf(time * 0.1f + row * 0.2f) * 100);
                pattern_frame[led_index][0] = 0;
                pattern_frame[led_index][1] = green;
                pattern_frame[led_index][2] = 0;
            }
        }
    }
}


void screen_saver_generate_city(uint32_t time)
{
    // Generate city lights
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            
            if (row > 4) {  // Buildings
                if (rand() % 10 < 3) {  // 30% chance of light
                    uint32_t color = pattern_colors[rand() % 8];
                    pattern_frame[led_index][0] = (color >> 16) & 0xFF;
                    pattern_frame[led_index][1] = (color >> 8) & 0xFF;
                    pattern_frame[led_index][2] = color & 0xFF;
                } else {
                    pattern_frame[led_index][0] = 0;
                    pattern_frame[led_index][1] = 0;
                    pattern_frame[led_index][2] = 0;
                }
            } else {  // Sky
                pattern_frame[led_index][0] = 0;
                pattern_frame[led_index][1] = 0;
                pattern_frame[led_index][2] = 50;
            }
        }
    }
}


void screen_saver_generate_space(uint32_t time)
{
    // Generate space theme
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        if (rand() % 100 < 5) {  // 5% chance of star
            uint32_t color = pattern_colors[rand() % 8];
            pattern_frame[i][0] = (color >> 16) & 0xFF;
            pattern_frame[i][1] = (color >> 8) & 0xFF;
            pattern_frame[i][2] = color & 0xFF;
        } else {
            pattern_frame[i][0] = 0;
            pattern_frame[i][1] = 0;
            pattern_frame[i][2] = 0;
        }
    }
}


void screen_saver_generate_geometric(uint32_t time)
{
    // Generate geometric patterns
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int led_index = chess_pos_to_led_index(row, col);
            int pattern = ((row + col + (time / 2)) % 4);
            
            uint32_t color = pattern_colors[pattern];
            pattern_frame[led_index][0] = (color >> 16) & 0xFF;
            pattern_frame[led_index][1] = (color >> 8) & 0xFF;
            pattern_frame[led_index][2] = color & 0xFF;
        }
    }
}


void screen_saver_generate_minimal(uint32_t time)
{
    // Minimal energy pattern - just a few dim lights
    memset(pattern_frame, 0, sizeof(pattern_frame));
    
    for (int i = 0; i < 5; i++) {
        int led_index = rand() % CHESS_LED_COUNT_TOTAL;
        pattern_frame[led_index][0] = 10;  // Very dim red
        pattern_frame[led_index][1] = 0;
        pattern_frame[led_index][2] = 0;
    }
}


void screen_saver_apply_brightness(void)
{
    // Apply brightness reduction to all LEDs
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        pattern_frame[i][0] = (pattern_frame[i][0] * screen_saver.brightness) / 100;
        pattern_frame[i][1] = (pattern_frame[i][1] * screen_saver.brightness) / 100;
        pattern_frame[i][2] = (pattern_frame[i][2] * screen_saver.brightness) / 100;
    }
}


void screen_saver_send_pattern_to_leds(void)
{
    // Send pattern to LED task
    // ✅ DIRECT LED CALLS - No queue hell
    for (int i = 0; i < CHESS_LED_COUNT_TOTAL; i++) {
        led_set_pixel_safe(i, pattern_frame[i][0], pattern_frame[i][1], pattern_frame[i][2]);
    }
}


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


void screen_saver_process_commands(void)
{
    uint8_t command;
    
    // Process commands from queue
    if (screen_saver_command_queue != NULL) {
        while (xQueueReceive(screen_saver_command_queue, &command, 0) == pdTRUE) {
            switch (command) {
                case 0: // Enable screen saver
                    screen_saver.enabled = true;
                    ESP_LOGI(TAG, "Screen saver enabled");
                    break;
                    
                case 1: // Disable screen saver
                    screen_saver.enabled = false;
                    if (screen_saver.state == SCREEN_SAVER_STATE_ACTIVE) {
                        screen_saver_deactivate();
                    }
                    ESP_LOGI(TAG, "Screen saver disabled");
                    break;
                    
                case 2: // Force activate
                    screen_saver_activate();
                    break;
                    
                case 3: // Force deactivate
                    screen_saver_deactivate();
                    break;
                    
                case 4: // Print status
                    screen_saver_print_status();
                    break;
                    
                case 5: // Test patterns
                    screen_saver_test_patterns();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown screen saver command: %d", command);
                    break;
            }
        }
    }
}


void screen_saver_print_status(void)
{
    ESP_LOGI(TAG, "Screen Saver Status:");
    ESP_LOGI(TAG, "  State: %d", screen_saver.state);
    ESP_LOGI(TAG, "  Enabled: %s", screen_saver.enabled ? "Yes" : "No");
    ESP_LOGI(TAG, "  Current pattern: %d", screen_saver.current_pattern);
    ESP_LOGI(TAG, "  Timeout: %lu ms", screen_saver.timeout_ms);
    ESP_LOGI(TAG, "  Brightness: %d%%", screen_saver.brightness);
    ESP_LOGI(TAG, "  Frame count: %lu", screen_saver.frame_count);
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t time_since_activity = current_time - screen_saver.last_activity_time;
    ESP_LOGI(TAG, "  Time since last activity: %lu ms", time_since_activity);
    ESP_LOGI(TAG, "  Time until activation: %lu ms", 
              (time_since_activity >= screen_saver.timeout_ms) ? 0 : 
              (screen_saver.timeout_ms - time_since_activity));
}


void screen_saver_test_patterns(void)
{
    ESP_LOGI(TAG, "Testing all screen saver patterns...");
    
    for (int pattern = 0; pattern < MAX_SCREEN_SAVER_PATTERNS; pattern++) {
        ESP_LOGI(TAG, "Testing pattern %d", pattern);
        
        screen_saver_set_pattern(pattern);
        screen_saver_activate();
        
        // Run pattern for 3 seconds
        for (int i = 0; i < 15; i++) {
            screen_saver_generate_pattern();
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        
        screen_saver_deactivate();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    ESP_LOGI(TAG, "Pattern test completed");
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void screen_saver_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Screen saver task started successfully");
    
    // NOTE: Task is already registered with TWDT in main.c
    // No need to register again here to avoid duplicate registration
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Automatic activation after timeout");
    ESP_LOGI(TAG, "  • Multiple energy-saving patterns");
    ESP_LOGI(TAG, "  • User activity detection");
    ESP_LOGI(TAG, "  • Smooth fade transitions");
    ESP_LOGI(TAG, "  • Configurable brightness and timeout");
    ESP_LOGI(TAG, "  • 200ms update cycle");
    
    task_running = true;
    
    // Initialize screen saver system
    screen_saver_initialize();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // Reset watchdog timeru pro screen saver task v každé iteraci (pouze pokud je registrován)
        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Process screen saver commands
        screen_saver_process_commands();
        
        // Check for timeout and activate if needed
        if (screen_saver_check_timeout() && screen_saver.state == SCREEN_SAVER_STATE_INACTIVE) {
            screen_saver_activate();
        }
        
        // Update screen saver pattern if active
        if (screen_saver.state == SCREEN_SAVER_STATE_ACTIVE) {
            screen_saver_generate_pattern();
        }
        
        // Periodic status update
        if (loop_count % 2500 == 0) { // Every 5 seconds
            ESP_LOGI(TAG, "Screen Saver Task Status: loop=%d, state=%d, pattern=%d", 
                     loop_count, screen_saver.state, screen_saver.current_pattern);
        }
        
        loop_count++;
        
        // Wait for next cycle (200ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SCREEN_SAVER_UPDATE_MS));
    }
}
