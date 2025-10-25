/**
 * @file matrix_task.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace Matrix tasku
 * 
 * Tento task zpracovava 8x8 reed switch matici:
 * - Skenovani matice a detekce figurek
 * - Time-multiplexed GPIO ovladani
 * - Detekce a validace tahu
 * - Generovani maticovych udalosti
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento task je zodpovedny za detekci figurek na sachovnici.
 * Skenuje 8x8 matici reed switchu a detekuje kdy hrac zvedne
 * nebo polozi figurku. Komunikuje s game taskem pres fronty.
 * 
 * Hardware:
 * - 8x8 reed switch matice
 * - Row piny: GPIO10,11,18,19,20,21,22,23 (vystupy)
 * - Column piny: GPIO0,1,2,3,6,9,16,17 (vstupy s pull-up)
 * - Simulacni mod (neni potreba skutecny hardware)
 */


#include "matrix_task.h"
#include "freertos_chess.h"
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
#include <ctype.h>


static const char *TAG = "MATRIX_TASK";

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Safe WDT reset that logs WARNING instead of ERROR for ESP_ERR_NOT_FOUND
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if task not registered (WARNING only)
 */
static esp_err_t matrix_task_wdt_reset_safe(void) {
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


// Matrix state
static uint8_t matrix_state[64] = {0}; // Current matrix state
static uint8_t matrix_previous[64] = {0}; // Previous matrix state
static uint8_t matrix_changes[64] = {0}; // Change detection

// Task state
static bool task_running = false;
static bool simulation_mode = false; // Changed to false for real hardware operation
bool matrix_scanning_enabled = true; // Matrix scanning enabled by default (extern for timer callback)

// Scanning state
static uint8_t current_row = 0;
static uint32_t last_scan_time = 0;
static uint32_t scan_count = 0;

// Move detection
static uint8_t last_piece_lifted = 255; // No piece
static uint8_t last_piece_placed = 255; // No piece
static uint32_t move_detection_timeout = 0;

// Matrix patterns for simulation
static const uint8_t simulation_patterns[][64] = {
    // Pattern 0: Empty board
    {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0},
    
    // Pattern 1: Starting position
    {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2},
    
    // Pattern 2: Mid-game position
    {1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2}
};

static uint8_t current_pattern = 1; // Start with pattern 1


// ============================================================================
// MATRIX SCANNING FUNCTIONS
// ============================================================================


// Internal function - scans row WITHOUT mutex (caller must hold mutex)
static void matrix_scan_row_internal(uint8_t row)
{
    if (row >= 8) return;
    
    // Set current row high
    gpio_set_level(matrix_row_pins[row], 1);
    
    // Small delay for signal stabilization
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // Read all column pins for this row
    for (int col = 0; col < 8; col++) {
        int index = row * 8 + col;
        int pin_level = gpio_get_level(matrix_col_pins[col]);
        
        // In simulation mode, use simulated values
        if (simulation_mode) {
            matrix_state[index] = simulation_patterns[current_pattern][index];
        } else {
            // Real hardware: reed switch closed = piece present
            matrix_state[index] = (pin_level == 0) ? 1 : 0;
        }
    }
    
    // Set row back to low
    gpio_set_level(matrix_row_pins[row], 0);
}

// Public function - scans row WITH mutex protection
void matrix_scan_row(uint8_t row)
{
    if (row >= 8) return;
    
    // CRITICAL: Protect matrix state with mutex
    if (matrix_mutex != NULL) {
        if (xSemaphoreTake(matrix_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            matrix_scan_row_internal(row);
            xSemaphoreGive(matrix_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire matrix mutex for scan row");
        }
    } else {
        // Fallback if mutex not available
        matrix_scan_row_internal(row);
    }
}


void matrix_scan_all(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    // NOTE: Don't call esp_task_wdt_reset() here because this function
    // is called from timer callbacks which run in timer service task
    // that is not registered with TWDT
    
    // CRITICAL: Protect matrix state with mutex for change detection
    if (matrix_mutex != NULL) {
        if (xSemaphoreTake(matrix_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Scan each row (use internal function since we already hold mutex)
            for (int row = 0; row < 8; row++) {
                // NOTE: Don't call WDT reset in timer callback context
                matrix_scan_row_internal(row);
            }
            
            // Detect changes
            for (int i = 0; i < 64; i++) {
                if (matrix_state[i] != matrix_previous[i]) {
                    matrix_changes[i] = 1;
                } else {
                    matrix_changes[i] = 0;
                }
            }
            
            // Update previous state
            memcpy(matrix_previous, matrix_state, sizeof(matrix_state));
            
            last_scan_time = current_time;
            scan_count++;
            
            // NOTE: Don't call WDT reset in timer callback context
            
            xSemaphoreGive(matrix_mutex);
        } else {
            ESP_LOGW(TAG, "Failed to acquire matrix mutex for scan all");
            return;
        }
    } else {
        // Fallback if mutex not available
        for (int row = 0; row < 8; row++) {
            matrix_scan_row_internal(row);
        }
        
        for (int i = 0; i < 64; i++) {
            if (matrix_state[i] != matrix_previous[i]) {
                matrix_changes[i] = 1;
            } else {
                matrix_changes[i] = 0;
            }
        }
        
        memcpy(matrix_previous, matrix_state, sizeof(matrix_state));
        last_scan_time = current_time;
        scan_count++;
    }
    
    if (simulation_mode) {
        ESP_LOGD(TAG, "Matrix scan completed: pattern=%d, changes=%d", 
                 current_pattern, 0); // Would count changes
    }
}


// ============================================================================
// MOVE DETECTION FUNCTIONS
// ============================================================================


void matrix_detect_moves(void)
{
    // Look for piece lifted (1->0 transition)
    uint8_t piece_lifted = 255;
    for (int i = 0; i < 64; i++) {
        if (matrix_previous[i] == 1 && matrix_state[i] == 0) {
            piece_lifted = i;
            break;
        }
    }
    
    // Look for piece placed (0->1 transition)
    uint8_t piece_placed = 255;
    for (int i = 0; i < 64; i++) {
        if (matrix_previous[i] == 0 && matrix_state[i] == 1) {
            piece_placed = i;
            break;
        }
    }
    
    // Process move detection
    if (piece_lifted != 255) {
        last_piece_lifted = piece_lifted;
        move_detection_timeout = esp_timer_get_time() / 1000 + 5000; // 5 second timeout
        
        ESP_LOGI(TAG, "Piece lifted from square %d", piece_lifted);
        
        // Send matrix event
        if (matrix_event_queue != NULL) {
            matrix_event_t event = {
                .type = MATRIX_EVENT_PIECE_LIFTED,
                .from_square = piece_lifted,
                .to_square = 255,
                .piece_type = 1,
                .timestamp = esp_timer_get_time() / 1000
            };
            
            if (xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "Piece lifted event sent to queue");
            }
        }
    }
    
    if (piece_placed != 255) {
        last_piece_placed = piece_placed;
        
        ESP_LOGI(TAG, "Piece placed on square %d", piece_placed);
        
        // Send matrix event
        if (matrix_event_queue != NULL) {
            matrix_event_t event = {
                .type = MATRIX_EVENT_PIECE_PLACED,
                .from_square = 255,
                .to_square = piece_placed,
                .piece_type = 1,
                .timestamp = esp_timer_get_time() / 1000
            };
            
            if (xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
                ESP_LOGI(TAG, "Piece placed event sent to queue");
            }
        }
        
        // Check if this completes a move
        if (last_piece_lifted != 255 && last_piece_lifted != piece_placed) {
            matrix_detect_complete_move(last_piece_lifted, piece_placed);
            last_piece_lifted = 255; // Reset
        }
    }
    
    // Check for move timeout
    if (last_piece_lifted != 255) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        if (current_time > move_detection_timeout) {
            ESP_LOGW(TAG, "Move detection timeout - piece lifted from %d", last_piece_lifted);
            last_piece_lifted = 255; // Reset
        }
    }
}


void matrix_detect_complete_move(uint8_t from_square, uint8_t to_square)
{
    ESP_LOGI(TAG, "Complete move detected: %d -> %d", from_square, to_square);
    
    // Convert square indices to chess notation
    char from_notation[4], to_notation[4];
    matrix_square_to_notation(from_square, from_notation);
    matrix_square_to_notation(to_square, to_notation);
    
    ESP_LOGI(TAG, "Move: %s -> %s", from_notation, to_notation);
    
    // Send complete move event
    if (matrix_event_queue != NULL) {
        matrix_event_t event = {
            .type = MATRIX_EVENT_MOVE_DETECTED,
            .from_square = from_square,
            .to_square = to_square,
            .piece_type = 1, // Pawn for now
            .timestamp = esp_timer_get_time() / 1000
        };
        
        if (xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Complete move event sent to queue");
        }
    }
}


// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================


void matrix_square_to_notation(uint8_t square, char* notation)
{
    if (square >= 64 || notation == NULL) return;
    
    int row = square / 8;
    int col = square % 8;
    
    notation[0] = 'a' + col;
    notation[1] = '1' + row;
    notation[2] = '\0';
}


uint8_t matrix_notation_to_square(const char* notation)
{
    if (notation == NULL || strlen(notation) != 2) return 255;
    
    char file = tolower(notation[0]);
    char rank = notation[1];
    
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return 255;
    }
    
    int col = file - 'a';
    int row = rank - '1';
    
    return row * 8 + col;
}


void matrix_print_state(void)
{
    ESP_LOGI(TAG, "Matrix State (8x8):");
    
    for (int row = 7; row >= 0; row--) {
        char line[32] = "";
        char* ptr = line;
        
        ptr += sprintf(ptr, "%d ", row + 1);
        
        for (int col = 0; col < 8; col++) {
            int index = row * 8 + col;
            if (matrix_state[index]) {
                ptr += sprintf(ptr, "[P] ");
            } else {
                ptr += sprintf(ptr, "[ ] ");
            }
        }
        
        ESP_LOGI(TAG, "%s", line);
    }
    
    ESP_LOGI(TAG, "   a   b   c   d   e   f   g   h");
}

void matrix_test_scanning(void)
{
    ESP_LOGI(TAG, "🔍 Testing matrix scanning functionality...");
    
    // Test 1: Reset matrix state
    ESP_LOGI(TAG, "Test 1: Resetting matrix state");
    matrix_reset();
    matrix_print_state();
    
    // Test 2: Simulate piece placement
    ESP_LOGI(TAG, "Test 2: Simulating piece placement");
    // Place pieces directly on squares
    matrix_state[1*8 + 4] = 1; // e2
    matrix_state[3*8 + 4] = 1; // e4
    matrix_state[6*8 + 3] = 1; // d7
    matrix_state[4*8 + 3] = 1; // d5
    matrix_print_state();
    
    // Test 3: Simulate piece movement
    ESP_LOGI(TAG, "Test 3: Simulating piece movement");
    matrix_simulate_move("e2", "e4"); // Move from e2 to e4
    matrix_simulate_move("d7", "d5"); // Move from d7 to d5
    matrix_print_state();
    
    // Test 4: Simulate piece removal
    ESP_LOGI(TAG, "Test 4: Simulating piece removal");
    // Remove pieces directly from squares
    matrix_state[3*8 + 4] = 0; // e4
    matrix_state[4*8 + 3] = 0; // d5
    matrix_print_state();
    
    // Test 5: Test all squares
    ESP_LOGI(TAG, "Test 5: Testing all squares");
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            // Place piece directly on square
            matrix_state[row * 8 + col] = 1;
            vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
        }
    }
    matrix_print_state();
    
    // Reset after test
    matrix_reset();
    ESP_LOGI(TAG, "✅ Matrix test completed successfully");
}

void matrix_simulate_move(const char* from, const char* to)
{
    uint8_t from_square = matrix_notation_to_square(from);
    uint8_t to_square = matrix_notation_to_square(to);
    
    if (from_square == 255 || to_square == 255) {
        ESP_LOGE(TAG, "Invalid chess notation: %s -> %s", from, to);
        return;
    }
    
    ESP_LOGI(TAG, "Simulating move: %s (%d) -> %s (%d)", from, from_square, to, to_square);
    
    // Update matrix state
    matrix_state[from_square] = 0; // Remove piece from source
    matrix_state[to_square] = 1;   // Place piece at destination
    
    // Force change detection
    matrix_changes[from_square] = 1;
    matrix_changes[to_square] = 1;
    
    ESP_LOGI(TAG, "Move simulation completed");
}


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


void matrix_process_commands(void)
{
    uint8_t command;
    
    // Process commands from queue
    if (matrix_command_queue != NULL) {
        while (xQueueReceive(matrix_command_queue, &command, 0) == pdTRUE) {
            switch (command) {
                case 0: // Reset matrix
                    matrix_reset();
                    break;
                    
                case 1: // Print state
                    matrix_print_state();
                    break;
                    
                case 2: // MATRIX_CMD_TEST - Test matrix scanning
                    ESP_LOGI(TAG, "=== Matrix Test Started ===");
                    matrix_test_scanning();
                    ESP_LOGI(TAG, "=== Matrix Test Complete ===");
                    break;
                    
                case 3: // Change simulation pattern
                    current_pattern = (current_pattern + 1) % 3;
                    ESP_LOGI(TAG, "Simulation pattern changed to %d", current_pattern);
                    break;
                    
                case 4: // MATRIX_CMD_DISABLE - Disable matrix scanning
                    matrix_scanning_enabled = false;
                    ESP_LOGI(TAG, "Matrix scanning DISABLED");
                    break;
                    
                case 5: // MATRIX_CMD_ENABLE - Enable matrix scanning
                    matrix_scanning_enabled = true;
                    ESP_LOGI(TAG, "Matrix scanning ENABLED");
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown matrix command: %d", command);
                    break;
            }
        }
    }
}


void matrix_reset(void)
{
    ESP_LOGI(TAG, "Resetting matrix state");
    
    // Clear all states
    memset(matrix_state, 0, sizeof(matrix_state));
    memset(matrix_previous, 0, sizeof(matrix_previous));
    memset(matrix_changes, 0, sizeof(matrix_changes));
    
    // Reset move detection
    last_piece_lifted = 255;
    last_piece_placed = 255;
    move_detection_timeout = 0;
    
    // Reset scanning
    current_row = 0;
    scan_count = 0;
    
    ESP_LOGI(TAG, "Matrix reset completed");
}


// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================


void matrix_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Matrix task started successfully");
    
    // ✅ CRITICAL: Register with TWDT from within task
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Failed to register Matrix task with TWDT: %s", esp_err_to_name(wdt_ret));
    } else {
        ESP_LOGI(TAG, "✅ Matrix task registered with TWDT");
    }
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • 8x8 reed switch matrix scanning");
    ESP_LOGI(TAG, "  • Time-multiplexed GPIO control");
    ESP_LOGI(TAG, "  • Move detection and validation");
    ESP_LOGI(TAG, "  • Matrix event generation");
    ESP_LOGI(TAG, "  • Simulation mode (no HW required)");
    ESP_LOGI(TAG, "  • 20ms scan cycle");
    
    task_running = true;
    
    // Initialize matrix state
    matrix_reset();
    
    // Set initial pattern
    current_pattern = 1;
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for matrix task in every iteration (only if registered)
        esp_err_t wdt_ret = matrix_task_wdt_reset_safe();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Watchdog logging every 500 iterations (5 seconds)
        if (loop_count % 500 == 0) {
            ESP_LOGI(TAG, "Matrix Task Watchdog: loop=%d, heap=%zu", loop_count, esp_get_free_heap_size());
        }
        
        // Process matrix commands
        matrix_process_commands();
        
        // Matrix scanning is now handled by FreeRTOS timer callback
        // No need to call matrix_scan_all() here to avoid race conditions
        
        // Periodic status update - reduced frequency for cleaner UART
        if (loop_count % 50000 == 0) { // Every 50000 loops (1000 seconds)
            ESP_LOGI(TAG, "Matrix Task Status: loop=%d, scans=%lu, pattern=%d", 
                     loop_count, scan_count, current_pattern);
            
            // Print matrix state occasionally
            if (loop_count % 100000 == 0) {
                matrix_print_state();
            }
        }
        
        loop_count++;
        
        // Wait for next cycle (10ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10));
    }
}
