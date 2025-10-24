/**
 * @file freertos_chess.h
 * @brief ESP32-C6 Chess System v2.4 - Main System Header
 * 
 * This header contains the main system definitions, constants,
 * and global variables for the FreeRTOS chess system.
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef FREERTOS_CHESS_H
#define FREERTOS_CHESS_H

#include "chess_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SYSTEM VERSION INFORMATION
// ============================================================================

#define CHESS_SYSTEM_NAME "ESP32-C6 Chess System"
#define CHESS_SYSTEM_VERSION "2.4"
#define CHESS_SYSTEM_AUTHOR "Alfred Krutina"
#define CHESS_VERSION_STRING "ESP32-C6 Chess System v2.4"
#define CHESS_BUILD_DATE __DATE__

// ============================================================================
// CORRECTED GPIO PIN DEFINITIONS (ESP32-C6 Compatible)
// ============================================================================

// LED system pins
#define LED_DATA_PIN GPIO_NUM_7        // WS2812B data line
#define STATUS_LED_PIN GPIO_NUM_8      // Status indicator (safe pin - changed from GPIO5 to avoid strapping pin)

// Matrix row pins (outputs) - 8 pins needed
// FIXED: Use safe GPIO pins according to ESP32-C6 DevKit official pinout
#define MATRIX_ROW_0 GPIO_NUM_10
#define MATRIX_ROW_1 GPIO_NUM_11  
#define MATRIX_ROW_2 GPIO_NUM_18
#define MATRIX_ROW_3 GPIO_NUM_19
#define MATRIX_ROW_4 GPIO_NUM_20
#define MATRIX_ROW_5 GPIO_NUM_21
#define MATRIX_ROW_6 GPIO_NUM_22
#define MATRIX_ROW_7 GPIO_NUM_23

// Matrix column pins (inputs with pull-up) - 8 pins needed
// FIXED: Use safe GPIO pins according to ESP32-C6 DevKit official pinout
#define MATRIX_COL_0 GPIO_NUM_0        // Safe pin
#define MATRIX_COL_1 GPIO_NUM_1        // Safe pin
#define MATRIX_COL_2 GPIO_NUM_2        // Safe pin
#define MATRIX_COL_3 GPIO_NUM_3        // Safe pin
#define MATRIX_COL_4 GPIO_NUM_6        // Safe pin
#define MATRIX_COL_5 GPIO_NUM_14       // Safe pin (changed from GPIO9 to avoid strapping pin)
#define MATRIX_COL_6 GPIO_NUM_16       // Safe pin (changed from GPIO13)
#define MATRIX_COL_7 GPIO_NUM_17       // Safe pin (changed from GPIO14)

// Reset button (separate pin) - Keep GPIO4 but handle carefully
#define BUTTON_RESET GPIO_NUM_27       // Safe pin (changed from GPIO4 to avoid strapping pin)

// Button definitions (time-multiplexed with matrix columns)
#define BUTTON_QUEEN     MATRIX_COL_0  // A1 square + Button Queen
#define BUTTON_ROOK      MATRIX_COL_1  // B1 square + Button Rook
#define BUTTON_BISHOP    MATRIX_COL_2  // C1 square + Button Bishop
#define BUTTON_KNIGHT    MATRIX_COL_3  // D1 square + Button Knight
#define BUTTON_PROMOTION_QUEEN  MATRIX_COL_4 // E1 square + Promotion Queen
#define BUTTON_PROMOTION_ROOK   MATRIX_COL_5 // F1 square + Promotion Rook
#define BUTTON_PROMOTION_BISHOP MATRIX_COL_6 // G1 square + Promotion Bishop
#define BUTTON_PROMOTION_KNIGHT MATRIX_COL_7 // H1 square + Promotion Knight

// ============================================================================
// SYSTEM TIMING CONSTANTS
// ============================================================================
    
// Time-multiplexing configuration (25ms total cycle - LED update removed)
#define MATRIX_SCAN_TIME_MS 20      // Matrix scanning time (0-20ms)
#define BUTTON_SCAN_TIME_MS 5       // Button scanning time (20-25ms)
// #define LED_UPDATE_TIME_MS 5        // ❌ REMOVED: No longer needed
#define TOTAL_CYCLE_TIME_MS 25      // Total multiplexing cycle (reduced from 30ms)
#define SYSTEM_HEALTH_TIME_MS 1000  // System health check interval

// ============================================================================
// LED TIMING OPTIMIZATION CONSTANTS - NOVÉ PRO OPRAVU BLIKÁNÍ
// ============================================================================

// WS2812B optimal timing constants
#define LED_COMMAND_TIMEOUT_MS      500   // ✅ Bezpečný timeout pro příkazy (místo 10-100ms)
#define LED_MUTEX_TIMEOUT_MS        200   // ✅ Mutex timeout
#define LED_HARDWARE_UPDATE_MS      300   // ✅ BEZPEČNÝ interval - 300ms (3.3Hz) pro lidské oko
#define LED_FRAME_SPACING_MS        200   // ✅ BEZPEČNÁ mezera - 200ms mezi frames
#define LED_RESET_TIME_US           500   // ✅ BEZPEČNÝ reset - 500μs (10x více než minimum)

// LED synchronization constants
#define LED_SAFE_TIMEOUT            pdMS_TO_TICKS(LED_COMMAND_TIMEOUT_MS)
#define LED_MUTEX_SAFE_TIMEOUT      pdMS_TO_TICKS(LED_MUTEX_TIMEOUT_MS)

// ============================================================================
// QUEUE SIZES
// ============================================================================

// MEMORY OPTIMIZED QUEUE SIZES - Phase 1
// Total queue memory: 8KB → 5KB = 3KB savings
#define LED_QUEUE_SIZE 50           // ✅ OPRAVA: Zvětšeno z 15 na 50 pro stabilitu
#define MATRIX_QUEUE_SIZE 8      // Reduced from 15 to 8 (sufficient for scanning)
#define BUTTON_QUEUE_SIZE 5      // Reduced from 8 to 5 (button events are infrequent)
#define UART_QUEUE_SIZE 10       // Reduced from 20 to 10 to save ~38 KB memory
#define GAME_QUEUE_SIZE 20       // Reduced from 30 to 20 (sufficient for game commands)
#define ANIMATION_QUEUE_SIZE 5   // Reduced from 8 to 5 (simple animations)
#define SCREEN_SAVER_QUEUE_SIZE 3 // Unchanged (already minimal)
#define WEB_SERVER_QUEUE_SIZE 10 // Reduced from 15 to 10 (streaming reduces needs)
// #define MATTER_QUEUE_SIZE 10     // DISABLED - Matter not needed

// ============================================================================
// TASK STACK SIZES AND PRIORITIES
// ============================================================================

// Task stack sizes (in bytes) - MEMORY OPTIMIZED PHASE 1
// Total stack usage: 57KB → 37KB = 20KB savings (UART task increased to prevent stack overflow)
#define LED_TASK_STACK_SIZE (8 * 1024)          // 8KB (CRITICAL: LED task needs more stack for critical sections + large arrays)
#define MATRIX_TASK_STACK_SIZE (3 * 1024)       // 3KB (unchanged - already optimal)
#define BUTTON_TASK_STACK_SIZE (3 * 1024)       // 3KB (unchanged - already optimal)
#define UART_TASK_STACK_SIZE (6 * 1024)         // 6KB (UART task needs more stack for command processing and response handling)
#define GAME_TASK_STACK_SIZE (10 * 1024)        // ✅ OPRAVA: 10KB pro bezpečný error handling (zvětšeno z 6KB)
#define ANIMATION_TASK_STACK_SIZE (2 * 1024)    // 2KB (reduced from 3KB - simple animations)
#define SCREEN_SAVER_TASK_STACK_SIZE (2 * 1024) // 2KB (reduced from 3KB - simple patterns)
#define TEST_TASK_STACK_SIZE (4 * 1024)         // 4KB (increased from 2KB to prevent stack overflow)
// #define MATTER_TASK_STACK_SIZE (8 * 1024)       // DISABLED - Matter not needed
#define WEB_SERVER_TASK_STACK_SIZE (20 * 1024)  // 20KB (increased for WiFi/HTTP server stability + HTML handling)
#define RESET_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)
#define PROMOTION_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)

// Task priorities
#define LED_TASK_PRIORITY 7          // ✅ OPRAVA: Nejvyšší priorita pro LED timing
#define MATRIX_TASK_PRIORITY 6       // ✅ Hardware input
#define BUTTON_TASK_PRIORITY 5       // ✅ User input
#define UART_TASK_PRIORITY 3         // ✅ Communication
#define GAME_TASK_PRIORITY 4         // ✅ OPRAVA: Sníženo z 5 na 4
#define ANIMATION_TASK_PRIORITY 3    // ✅ Visual effects
#define SCREEN_SAVER_TASK_PRIORITY 2 // ✅ Background
#define TEST_TASK_PRIORITY 1         // ✅ Debug only
// #define MATTER_TASK_PRIORITY 4       // DISABLED - Matter not needed
#define WEB_SERVER_TASK_PRIORITY 3   // ✅ Communication
#define RESET_BUTTON_TASK_PRIORITY 3 // ✅ User input
#define PROMOTION_BUTTON_TASK_PRIORITY 3 // ✅ User input

// ============================================================================
// GLOBAL QUEUE HANDLES
// ============================================================================

// LED control queues - REMOVED: Using direct LED calls instead
// extern QueueHandle_t led_command_queue;  // ❌ REMOVED: Queue hell eliminated
// extern QueueHandle_t led_status_queue;   // ❌ REMOVED: Queue hell eliminated
extern QueueHandle_t matrix_event_queue;
extern QueueHandle_t matrix_command_queue;
extern QueueHandle_t button_event_queue;
extern QueueHandle_t button_command_queue;
extern QueueHandle_t uart_command_queue;
extern QueueHandle_t uart_response_queue;
extern QueueHandle_t game_command_queue;
extern QueueHandle_t game_status_queue;
extern QueueHandle_t animation_command_queue;
extern QueueHandle_t animation_status_queue;
extern QueueHandle_t screen_saver_command_queue;
extern QueueHandle_t screen_saver_status_queue;
extern QueueHandle_t matter_command_queue;
extern QueueHandle_t matter_status_queue;
extern QueueHandle_t web_command_queue;
extern QueueHandle_t web_server_command_queue;
extern QueueHandle_t web_server_status_queue;
extern QueueHandle_t test_command_queue;

// ============================================================================
// GLOBAL MUTEX HANDLES
// ============================================================================

extern SemaphoreHandle_t led_mutex;
extern SemaphoreHandle_t matrix_mutex;
extern SemaphoreHandle_t button_mutex;
extern SemaphoreHandle_t game_mutex;
extern SemaphoreHandle_t system_mutex;
extern SemaphoreHandle_t uart_mutex;

// ============================================================================
// GLOBAL TIMER HANDLES
// ============================================================================

extern TimerHandle_t matrix_scan_timer;
extern TimerHandle_t button_scan_timer;
// extern TimerHandle_t led_update_timer;  // ❌ REMOVED: No longer needed
extern TimerHandle_t system_health_timer;

// ============================================================================
// GPIO PIN ARRAYS
// ============================================================================

extern const gpio_num_t matrix_row_pins[8];
extern const gpio_num_t matrix_col_pins[8];
extern const gpio_num_t promotion_button_pins_a[4];
extern const gpio_num_t promotion_button_pins_b[4];

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the chess system
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_system_init(void);

/**
 * @brief Initialize memory optimization systems
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_memory_systems_init(void);

/**
 * @brief Initialize hardware components
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_hardware_init(void);

/**
 * @brief Create all FreeRTOS queues
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_queues(void);

/**
 * @brief Create all FreeRTOS mutexes
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_mutexes(void);

/**
 * @brief Create all FreeRTOS timers
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_timers(void);

/**
 * @brief Start all FreeRTOS timers
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_start_timers(void);

/**
 * @brief Button scan timer callback
 * @param xTimer Timer handle
 */
void button_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Matrix scan timer callback
 * @param xTimer Timer handle
 */
void matrix_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief LED update timer callback - REMOVED: Using direct LED calls
 * @param xTimer Timer handle
 */
// void led_update_timer_callback(TimerHandle_t xTimer);  // ❌ REMOVED: No longer needed

/**
 * @brief Initialize GPIO pins
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_gpio_init(void);

// ============================================================================
// HARDWARE ABSTRACTION FUNCTIONS
// ============================================================================

/**
 * @brief Send string via UART
 * @param str String to send
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_uart_send_string(const char* str);

/**
 * @brief Send formatted string via UART
 * @param format Format string
 * @param ... Format arguments
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_uart_printf(const char* format, ...);

/**
 * @brief Set LED pixel color
 * @param led_index LED index (0-72)
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_led_set_pixel(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set all LEDs to same color
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_led_set_all(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Get matrix status
 * @param status Output buffer for 64-element status array
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_matrix_get_status(uint8_t* status);

// ============================================================================
// SYSTEM UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Print system information
 */
void chess_print_system_info(void);

/**
 * @brief Monitor system tasks
 * @return ESP_OK if all tasks are running properly
 */
esp_err_t chess_monitor_tasks(void);

// ============================================================================
// UTILITY MACROS
// ============================================================================

/**
 * @brief Safe queue creation with error checking
 */
#define SAFE_CREATE_QUEUE(handle, size, item_size, name) \
    do { \
        handle = xQueueCreate(size, item_size); \
        if (handle == NULL) { \
            ESP_LOGE(TAG, "Failed to create queue: %s", name); \
            return ESP_ERR_NO_MEM; \
        } \
        ESP_LOGI(TAG, "✓ Queue created: %s", name); \
    } while (0)

/**
 * @brief Safe mutex creation with error checking
 */
#define SAFE_CREATE_MUTEX(handle, name) \
    do { \
        handle = xSemaphoreCreateMutex(); \
        if (handle == NULL) { \
            ESP_LOGE(TAG, "Failed to create mutex: %s", name); \
            return ESP_ERR_NO_MEM; \
        } \
        ESP_LOGI(TAG, "✓ Mutex created: %s", name); \
    } while (0)

// ============================================================================
// EXTERNAL TASK HANDLES (for uart_task.c access)
// ============================================================================

extern TaskHandle_t led_task_handle;
extern TaskHandle_t matrix_task_handle;
extern TaskHandle_t button_task_handle;
extern TaskHandle_t uart_task_handle;
extern TaskHandle_t game_task_handle;
extern TaskHandle_t animation_task_handle;
extern TaskHandle_t screen_saver_task_handle;
extern TaskHandle_t test_task_handle;
extern TaskHandle_t matter_task_handle;
extern TaskHandle_t web_server_task_handle;
extern TaskHandle_t reset_button_task_handle;
extern TaskHandle_t promotion_button_task_handle;

#ifdef __cplusplus
}
#endif

#endif // FREERTOS_CHESS_H