/**
 * @file freertos_chess.h
 * @brief ESP32-C6 Chess System v1.8.0 - Main system header
 *
 * This header contains the main system definitions, constants
 * and global variables for the FreeRTOS storage system.
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2026-05-04
 *
 * @details
 * This header is the central definition point for the entire Sacha system.
 * Contains all key constants, GPIO definitions, queue handles,
 * mutex handles, timer handles and system functions.
 *
 * Main functions:
 * - GPIO pin definition for ESP32-C6
 * - System constants and sizes
 * - FreeRTOS queue and mutex handles
 * - System initialization function
 * - Hardware abstraction layer
 * - Utility functions and macros
 */

#ifndef FREERTOS_CHESS_H
#define FREERTOS_CHESS_H

#include "sdkconfig.h"
#include "chess_types.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SYSTEMOVE INFORMACE O VERZI
// ============================================================================

/** @brief The name of the sach system */
#define CHESS_SYSTEM_NAME "ESP32-C6 Chess System"
/** @brief Chess system version (CMake can override -DCHESS_SYSTEM_VERSION=…) */
#ifndef CHESS_SYSTEM_VERSION
#define CHESS_SYSTEM_VERSION "1.8.0"
#endif
/** @brief Author of the sach system */
#define CHESS_SYSTEM_AUTHOR "Alfred Krutina"
/** @brief Complete version string (CMake can override -DCHESS_VERSION_STRING=…) */
#ifndef CHESS_VERSION_STRING
#define CHESS_VERSION_STRING "ESP32-C6 Chess System v1.8.0"
#endif
/** @brief Datum sestaveni */
#define CHESS_BUILD_DATE __DATE__

// ============================================================================
// GPIO PIN DEFINICE (ESP32-C6 Kompatibilni)
// ============================================================================

/** @brief Pin pro WS2812B LED data (GPIO7) */
#define LED_DATA_PIN GPIO_NUM_7 // WS2812B data line
/** @brief Pin for status LED indicator (GPIO5 - safe pin) */
#define STATUS_LED_PIN                                                         \
  GPIO_NUM_5 // Status indicator (safe pin - GPIO8 is boot strapping pin!)

// Pins for the rows of the matrix (outputs) - need 8 pins
/** @brief Pin for row 0 of the matrix (GPIO10 - output) */
#define MATRIX_ROW_0 GPIO_NUM_10
/** @brief Pin for row 1 of the matrix (GPIO11 - output) */
#define MATRIX_ROW_1 GPIO_NUM_11
/** @brief Pin for row 2 of the matrix (GPIO18 - output) */
#define MATRIX_ROW_2 GPIO_NUM_18
/** @brief Pin for row 3 of the matrix (GPIO19 - output) */
#define MATRIX_ROW_3 GPIO_NUM_19
/** @brief Pin for row 4 of the matrix (GPIO20 - output) */
#define MATRIX_ROW_4 GPIO_NUM_20
/** @brief Pin for row 5 of the matrix (GPIO21 - output) */
#define MATRIX_ROW_5 GPIO_NUM_21
/** @brief Pin for row 6 of the matrix (GPIO22 - output) */
#define MATRIX_ROW_6 GPIO_NUM_22
/** @brief Pin for row 7 of the matrix (GPIO23 - output) */
#define MATRIX_ROW_7 GPIO_NUM_23

// Pins for matrix columns (pull-up inputs) - need 8 pin
/** @brief Pin for column 0 of the matrix (GPIO0 - input with pull-up, safe pin) */
#define MATRIX_COL_0 GPIO_NUM_0 // Safe pin
/** @brief Pin for column 1 of the matrix (GPIO1 - input with pull-up, safe pin) */
#define MATRIX_COL_1 GPIO_NUM_1 // Safe pin
/** @brief Pin for column 2 of the matrix (GPIO2 - input with pull-up, safe pin) */
#define MATRIX_COL_2 GPIO_NUM_2 // Safe pin
/** @brief Pin for column 3 of the matrix (GPIO3 - input with pull-up, safe pin) */
#define MATRIX_COL_3 GPIO_NUM_3 // Safe pin
/** @brief Pin for column 4 of the matrix (GPIO6 - input with pull-up, safe pin) */
#define MATRIX_COL_4 GPIO_NUM_6 // Safe pin
/** @brief Pin for column 5 of the matrix (GPIO4 - input with pull-up, safe pin,
 * changed from GPIO9) */
#define MATRIX_COL_5                                                           \
  GPIO_NUM_4 // Safe pin (changed from GPIO9 to GPIO4 to avoid strapping pin)
/** @brief Pin for column 6 of the matrix (GPIO16 - pull-up input) */
#define MATRIX_COL_6 GPIO_NUM_16 // Column G
/** @brief Pin for column 7 of the matrix (GPIO17 - pull-up input) */
#define MATRIX_COL_7 GPIO_NUM_17 // Column H

/** @brief Pin for reset button (GPIO15 - input with pull-up, strapping pin for
 * ROM messages, safe for button) */
#define BUTTON_RESET                                                           \
  GPIO_NUM_15 // Reset button (GPIO27 is not available on the LaskaKit board)

/**
 * The pin is also the NRST output for stm32_i2c_bootloader — it must not be configured
 * as input reset button nor read in button_task (otherwise false presses during
 * flashing and broken NRST).
 */
static inline bool chess_gpio_pin_is_stm32_nrst_output(int gpio_num) {
#if CONFIG_CHESS_STM32_I2C_BL_ENABLE
  if (gpio_num < 0) {
    return false;
  }
  return (CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0 >= 0 &&
          gpio_num == CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG0) ||
         (CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1 >= 0 &&
          gpio_num == CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG1) ||
         (CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2 >= 0 &&
          gpio_num == CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG2) ||
         (CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3 >= 0 &&
          gpio_num == CONFIG_CHESS_STM32_BL_NRST_GPIO_SEG3);
#else
  (void)gpio_num;
  return false;
#endif
}

// Button definitions (time-multiplexed with matrix columns)
/** @brief Graduation button on checker (shared with MATRIX_COL_0) */
#define BUTTON_QUEEN MATRIX_COL_0 // A1 square + Button Queen
/** @brief Graduation button per carriage (shared with MATRIX_COL_1) */
#define BUTTON_ROOK MATRIX_COL_1 // B1 square + Button Rook
/** @brief Shooter graduation button (shared with MATRIX_COL_2) */
#define BUTTON_BISHOP MATRIX_COL_2 // C1 square + Button Bishop
/** @brief Horse graduation button (shared with MATRIX_COL_3) */
#define BUTTON_KNIGHT MATRIX_COL_3 // D1 square + Button Knight

// ============================================================================
// NEPOUZIVANA MAKRA - ZAKOMENTOVANO
// ============================================================================
// POZOR: Nasledujici makra se NEPOUZIVAJI v kodu!
// They were originally designed for the second set of promotion buttons, but the system
// uses only 4 shared buttons (BUTTON_QUEEN, BUTTON_ROOK, BUTTON_BISHOP,
// BUTTON_KNIGHT) which are common to both players. Field
// promotion_button_pins_b is also not used.
//
// /** @brief Graduation button on checker B (shared with MATRIX_COL_4) */
// #define BUTTON_PROMOTION_QUEEN MATRIX_COL_4 // E1 square + Promotion Queen
// /** @brief Graduation button for row B (shared with MATRIX_COL_5) */
// #define BUTTON_PROMOTION_ROOK MATRIX_COL_5 // F1 square + Promotion Rook
// /** @brief Shooter B graduation button (shared with MATRIX_COL_6) */
// #define BUTTON_PROMOTION_BISHOP MATRIX_COL_6 // G1 square + Promotion Bishop
// /** @brief Graduation button for horse B (shared with MATRIX_COL_7) */
// #define BUTTON_PROMOTION_KNIGHT MATRIX_COL_7 // H1 square + Promotion Knight

// ============================================================================
// SYSTEMOVE CASOVE KONSTANTY
// ============================================================================

// Time-multiplexing configuration (25ms total cycle - LED update removed)
/** @brief Matrix scan time in milliseconds (0-20ms) */
#define MATRIX_SCAN_TIME_MS 20 // Matrix scanning time (0-20ms)
/** @brief Button scan time in milliseconds (20-25ms) */
#define BUTTON_SCAN_TIME_MS 5 // Button scanning time (20-25ms)
// #define LED_UPDATE_TIME_MS 5        //  REMOVED: No longer needed
/** @brief Total multiplexing cycle time in milliseconds (reduced from 30ms) */
#define TOTAL_CYCLE_TIME_MS 25 // Total multiplexing cycle (reduced from 30ms)
/** @brief System health check interval in milliseconds */
#define SYSTEM_HEALTH_TIME_MS 1000 // System health check interval

// ============================================================================
// LED TIMING OPTIMALIZACNI KONSTANTY - NOVE PRO OPRAVU BLIKANI
// ============================================================================

// WS2812B optimalni casove konstanty
/** @brief Safe timeout for LED commands in milliseconds (500ms misto
 * 10-100ms) */
#define LED_COMMAND_TIMEOUT_MS 500 // Safe timeout for commands
/** @brief Timeout for LED mutex in milliseconds (200ms) */
#define LED_MUTEX_TIMEOUT_MS 200 // Timeout pro mutex
/** @brief Safe LED update interval in milliseconds (300ms = 3.3Hz for
 * human eye) */
#define LED_HARDWARE_UPDATE_MS                                                 \
  300 // Safe interval - 300ms (3.3Hz) for the human eye
/** @brief Safe gap between LED frames in milliseconds (200ms) */
#define LED_FRAME_SPACING_MS 200 // Safe gap - 200ms between frames
/** @brief Safe reset time for WS2812B in microseconds (500μs = 10x more than
 * minimum) */
#define LED_RESET_TIME_US 500 // Safe reset - 500us (10x more than the minimum)

// LED synchronizacni konstanty
/** @brief Safe timeout for LED operations in FreeRTOS ticks */
#define LED_SAFE_TIMEOUT pdMS_TO_TICKS(LED_COMMAND_TIMEOUT_MS)
/** @brief Safe mutex timeout in FreeRTOS ticks */
#define LED_MUTEX_SAFE_TIMEOUT pdMS_TO_TICKS(LED_MUTEX_TIMEOUT_MS)

// ============================================================================
// VELIKOSTI FRONT
// ============================================================================

/** @brief Matrix: events when scanning 8x8 (enough depth for burst). */
#define MATRIX_QUEUE_SIZE 8
/** @brief Button: events from ISR (rare). */
#define BUTTON_QUEUE_SIZE 5
/** @brief UART: commands/responses (sizeof(game_response_t) ~ 328 B per item). */
#define UART_QUEUE_SIZE 10
/** @brief Game: fast moves from web/matrix (24 × chess_move_command_t; previously 50). */
#define GAME_QUEUE_SIZE 24
/**
 * @brief Test: uint8 commands to test_task (0-5 in test_process_commands).
 * @note LED queues are not used (direct LED); drive was this value
 * erroneously named LED_QUEUE_SIZE.
 */
#define TEST_COMMAND_QUEUE_SIZE 16
/** @brief UART output queue(item = sizeof(uart_message_t), uart_queue_message.h). */
#define UART_OUTPUT_QUEUE_LENGTH 20
/** @brief Velikost animation fronty (5 prvku, snizeno z 8 na 5, jednoduche
 * animace) */
#define ANIMATION_QUEUE_SIZE 5 // Reduced from 8 to 5 (simple animations)
/** @brief Velikost screen saver fronty (3 prvky, nezmeneno, jiz minimalni) */
#define SCREEN_SAVER_QUEUE_SIZE 3 // Unchanged (already minimal)
/** @brief Web server queue size (10 elements, reduced from 15 to 10, streaming
 *reduces needs) */
#define WEB_SERVER_QUEUE_SIZE                                                  \
  10 // Reduced from 15 to 10 (streaming reduces needs)
// #define MATTER_QUEUE_SIZE 10     // DISABLED - Matter not needed

// ============================================================================
// VELIKOSTI STACKU A PRIORITY TASKU
// ============================================================================

/** @brief LED: WS2812 + animation logic (measure high-water mark before reducing). */
#define LED_TASK_STACK_SIZE (8 * 1024)
/** @brief Matrix: sken + odezvy (game_response_t). */
#define MATRIX_TASK_STACK_SIZE (4 * 1024)
#define BUTTON_TASK_STACK_SIZE (3 * 1024)
/** @brief UART: input line + game_response_t when forwarding response. */
#define UART_TASK_STACK_SIZE (5 * 1024)
#define GAME_TASK_STACK_SIZE (6 * 1024)
/** @brief Velikost stacku Animation tasku (2KB, snizeno z 3KB, jednoduche
 * animace) */
#define ANIMATION_TASK_STACK_SIZE                                              \
  (2 * 1024) // 2KB (reduced from 3KB - simple animations)
/** @brief Velikost stacku Screen Saver tasku (2KB, snizeno z 3KB, jednoduche
 * vzory) */
#define SCREEN_SAVER_TASK_STACK_SIZE                                           \
  (2 * 1024) // 2KB (reduced from 3KB - simple patterns)
/** @brief Velikost stacku Test tasku (4KB, zvyseno z 2KB pro prevenci stack
 * overflow) */
#define TEST_TASK_STACK_SIZE                                                   \
  (4 * 1024) // 4KB (increased from 2KB to prevent stack overflow)
// #define MATTER_TASK_STACK_SIZE (8 * 1024)       // DISABLED - Matter not
// needed
/** @brief Velikost stacku Web Server tasku (20KB, zvyseno pro WiFi/HTTP server
 * stabilitu + HTML handling) */
#define WEB_SERVER_TASK_STACK_SIZE                                             \
  (20 * 1024) // 20KB (increased for WiFi/HTTP server stability + HTML handling)
/** @brief Velikost stacku Reset Button tasku (2KB - nezmeneno) */
#define RESET_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)
/** @brief Velikost stacku Promotion Button tasku (2KB - nezmeneno) */
#define PROMOTION_BUTTON_TASK_STACK_SIZE (2 * 1024) // 2KB (unchanged)
/** @brief Velikost stacku HA Light tasku (8KB) */
#define HA_LIGHT_TASK_STACK_SIZE (8 * 1024) // 8KB

// Priority tasku
/** @brief LED task priority (7 - highest for LED timing) */
#define LED_TASK_PRIORITY 7 // Highest priority for LED timing
/** @brief Matrix task priority (6 - hardware input) */
#define MATRIX_TASK_PRIORITY 6 // Hardware vstup
/** @brief Priority Button task (5 - user input) */
#define BUTTON_TASK_PRIORITY 5 // User login
/** @brief UART task priority (3 - communication) */
#define UART_TASK_PRIORITY 3 // Komunikace
/** @brief Game task priority (4) */
#define GAME_TASK_PRIORITY 4 // Game task priority
/** @brief Animation task priority (3 - visual effects) */
#define ANIMATION_TASK_PRIORITY 3 // Vizualni efekty
/** @brief Screen saver task priority (2 - background) */
#define SCREEN_SAVER_TASK_PRIORITY 2 // Pozadi
/** @brief Priority Test task (1 - only for debug) */
#define TEST_TASK_PRIORITY 1 // For debug only
// #define MATTER_TASK_PRIORITY 4       // DISABLED - Matter not needed
/** @brief Web server task priority (3 - communication) */
#define WEB_SERVER_TASK_PRIORITY 3 // Komunikace
/** @brief Priority Reset Button task (3 - user input) */
#define RESET_BUTTON_TASK_PRIORITY 3 // User login
/** @brief Priority Promotion Button taska (3 - user input) */
#define PROMOTION_BUTTON_TASK_PRIORITY 3 // User login
/** @brief HA light task priority (3 - communication) */
#define HA_LIGHT_TASK_PRIORITY 3 // Komunikace

// ============================================================================
// GLOBAL QUEUE HANDLES
// ============================================================================

// LED control queues - REMOVED: Prime LED calls are used instead
// extern QueueHandle_t led_command_queue;  //  REMOVED: Queue hell eliminated
// extern QueueHandle_t led_status_queue;   //  REMOVED: Queue hell eliminated
/** @brief Queue for event matrix (piece lifted/placed) */
extern QueueHandle_t matrix_event_queue;
/** @brief Queue for matrix commands (scan, reset, test) */
extern QueueHandle_t matrix_command_queue;
/** @brief Queue for the response matrix (responses from the system) */
extern QueueHandle_t matrix_response_queue;
/** @brief Queue for button events (press, release, long press) */
extern QueueHandle_t button_event_queue;
/** @brief Queue for button commands (reset, status, test) */
extern QueueHandle_t button_command_queue;
/** @brief Queue for UART commands (communication with the system) */
extern QueueHandle_t uart_command_queue;
/** @brief Queue for UART responses (system responses) */
extern QueueHandle_t uart_response_queue;
/** @brief Queue for game commands (new game, move, status) */
extern QueueHandle_t game_command_queue;
/** @brief Queue for game status (game state) */
extern QueueHandle_t game_status_queue;
/** @brief Queue for animation commands (start, stop, pause) */
extern QueueHandle_t animation_command_queue;
/** @brief Fronta pro animation status (stav animaci) */
extern QueueHandle_t animation_status_queue;
/** @brief Queue for screen saver commands (activate, deactivate) */
extern QueueHandle_t screen_saver_command_queue;
/** @brief Fronta pro screen saver status (stav screen saveru) */
extern QueueHandle_t screen_saver_status_queue;
/** @brief Queue for matter commands (DISABLED - Matter is not needed) */
extern QueueHandle_t matter_command_queue;
/** @brief Queue for matter status (DISABLED - Matter is not needed) */
extern QueueHandle_t matter_status_queue;
/** @brief Queue for web commands (start, stop, config) */
extern QueueHandle_t web_command_queue;
/** @brief Queue for web server commands (HTTP requests) */
extern QueueHandle_t web_server_command_queue;
/** @brief Fronta pro web server status (server state) */
extern QueueHandle_t web_server_status_queue;
/** @brief Queue for test commands (run, status, reset) */
extern QueueHandle_t test_command_queue;

// ============================================================================
// GLOBAL MUTEX HANDLES
// ============================================================================

/** @brief Mutex for LED operations (protect LED state) */
extern SemaphoreHandle_t led_mutex;
/** @brief Mutex for matrix operations (protect matrix state) */
extern SemaphoreHandle_t matrix_mutex;
/** @brief Mutex for button operations (protect button state) */
extern SemaphoreHandle_t button_mutex;
/** @brief Mutex for game operations (protect game state) */
extern SemaphoreHandle_t game_mutex;
/** @brief Mutex for system operations (global system protection) */
extern SemaphoreHandle_t system_mutex;
/** @brief Mutex for UART operations (protect UART output) */
extern SemaphoreHandle_t uart_mutex;

// ============================================================================
// GLOBAL TIMER HANDLES
// ============================================================================

/** @brief Timer for periodic matrix scanning */
extern TimerHandle_t matrix_scan_timer;
/** @brief Timer for periodic button scanning */
extern TimerHandle_t button_scan_timer;
// extern TimerHandle_t led_update_timer;  //  REMOVED: No longer needed
/** @brief Timer pro periodicke kontroly zdravi systemu */
extern TimerHandle_t system_health_timer;

// ============================================================================
// GPIO PIN POLE
// ============================================================================

/** @brief GPIO pin array for matrix rows (8 output) */
extern const gpio_num_t matrix_row_pins[8];
/** @brief GPIO pin array for matrix columns (8 inputs with pull-up) */
extern const gpio_num_t matrix_col_pins[8];
/** @brief GPIO pin array for promotion button A (4 buttons) */
extern const gpio_num_t promotion_button_pins_a[4];
// NEPOUZIVANO: Toto pole se nikde v kodu nepouziva!
// /** @brief GPIO pin array for promotion button B (4 buttons) */
// extern const gpio_num_t promotion_button_pins_b[4];

// ============================================================================
// SYSTEM INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initializes the sach system
 *
 * This function initializes the entire sach system including hardware, queue,
 * mutex and timer. It calls all necessary initialization functions.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_system_init(void);

/**
 * @brief Initializes memory optimization systems
 *
 * Initializes the buffer pool and streaming output for efficient memory usage.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_memory_systems_init(void);

/**
 * @brief Initializes hardware components
 *
 * Initializes GPIO pins, LED strip and other hardware components.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_hardware_init(void);

/**
 * @brief Create all FreeRTOS queues
 *
 * Create all necessary queues for communication between tasks.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_queues(void);

/**
 * @brief Create all FreeRTOS mutexes
 *
 * Create all necessary mutexes for thread-safe access to datum.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_mutexes(void);

/**
 * @brief Create all FreeRTOS timers
 *
 * Create periodic timers for matrix, button and system scans.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_create_timers(void);

/**
 * @brief Start all FreeRTOS timers
 *
 * Spusti periodicke timery vytvorene funkci chess_create_timers().
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_start_timers(void);

/**
 * @brief Callback function for button scan timer
 *
 * This function is called periodically by the button scan timer.
 *
 * @param xTimer Timer handle
 */
void button_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Callback function for matrix scan timer
 *
 * This function is called periodically by the matrix scan timer.
 *
 * @param xTimer Timer handle
 */
void matrix_scan_timer_callback(TimerHandle_t xTimer);

/**
 * @brief LED update timer callback - REMOVED: Prime LED callbacks are used
 * @param xTimer Timer handle
 */
// void led_update_timer_callback(TimerHandle_t xTimer);  //  REMOVED: No
// longer needed

/**
 * @brief Initializes the GPIO pins
 *
 * Initializes all GPIO pins for matrix, buttons and LEDs.
 * Validates pin safety and sets pull-up/pull-down resistors.
 *
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_gpio_init(void);

// ============================================================================
// HARDWARE ABSTRACTION FUNCTIONS
// ============================================================================

/**
 * @brief Send chain via UART
 *
 * @param str String to send via UART
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_uart_send_string(const char *str);

/**
 * @brief Send formatted string via UART
 *
 * Works like printf() but sends output via UART.
 *
 * @param format Format string (printf style)
 * @param ... Arguments for the format string
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_uart_printf(const char *format, ...);

/**
 * @brief Set the LED pixel color
 *
 * @param led_index LED index (0-72, where 0-63 case, 64-72 button)
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_led_set_pixel(uint8_t led_index, uint8_t red, uint8_t green,
                              uint8_t blue);

/**
 * @brief Set all LEDs to the same color
 *
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_led_set_all(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Gets the state of the matrix
 *
 * @param[out] status Output buffer for the 64-element status array (1 = piece
 * present, 0 = empty)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t chess_matrix_get_status(uint8_t *status);

// ============================================================================
// SYSTEM UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Vypise systemove informace
 *
 * Vypise informace o verzi systemu, konfiguraci GPIO pinu,
 * velikostech front a priority tasku.
 */
void chess_print_system_info(void);

/**
 * @brief Monitors system tasks
 *
 * Checks whether all tasks are running correctly and whether they are not blocked.
 *
 * @return ESP_OK if all tasks run correctly, error code for problem
 */
esp_err_t chess_monitor_tasks(void);

/**
 * @brief Checks if demo mode is currently active
 *
 * @return true if demo mode is active, false otherwise
 */
bool is_demo_mode_enabled(void);

// ============================================================================
// UTILITY MAKRA
// ============================================================================

/**
 * @brief Create queues safely with error checking
 *
 * This macro creates a FreeRTOS queue and automatically checks for success.
 * If the queue is not created, it logs an error and returns ESP_ERR_NO_MEM.
 *
 * @param handle Variable to store the handle of the queue
 * @param size The number of elements in the queue
 * @param item_size Size of one item in the queue (in bytes)
 * @param name The name of the login queue
 */
#define SAFE_CREATE_QUEUE(handle, size, item_size, name)                       \
  do {                                                                         \
    handle = xQueueCreate(size, item_size);                                    \
    if (handle == NULL) {                                                      \
      ESP_LOGE(TAG, "Failed to create queue: %s", name);                       \
      return ESP_ERR_NO_MEM;                                                   \
    }                                                                          \
    ESP_LOGI(TAG, "✓ Queue created: %s", name);                                \
  } while (0)

/**
 * @brief Create a mutex safely with error checking
 *
 * This macro creates a FreeRTOS mutex and automatically checks for success.
 * If the mutex is not created, log an error and return ESP_ERR_NO_MEM.
 *
 * @param handle A variable to store the handle of the mutex
 * @param name The name of the mutex for logging
 */
#define SAFE_CREATE_MUTEX(handle, name)                                        \
  do {                                                                         \
    handle = xSemaphoreCreateMutex();                                          \
    if (handle == NULL) {                                                      \
      ESP_LOGE(TAG, "Failed to create mutex: %s", name);                       \
      return ESP_ERR_NO_MEM;                                                   \
    }                                                                          \
    ESP_LOGI(TAG, "✓ Mutex created: %s", name);                                \
  } while (0)

// ============================================================================
// EXTERNI TASK HANDLES (pro pristup z uart_task.c a dalsich modulu)
// ============================================================================

/** @brief Handle pro LED task */
extern TaskHandle_t led_task_handle;
/** @brief Handle pro Matrix task */
extern TaskHandle_t matrix_task_handle;
/** @brief Handle pro Button task */
extern TaskHandle_t button_task_handle;
/** @brief Handle pro UART task */
extern TaskHandle_t uart_task_handle;
/** @brief Handle pro Game task */
extern TaskHandle_t game_task_handle;
/** @brief Handle pro Animation task */
extern TaskHandle_t animation_task_handle;
/** @brief Handle pro Screen Saver task */
extern TaskHandle_t screen_saver_task_handle;
/** @brief Handle pro Test task */
extern TaskHandle_t test_task_handle;
/** @brief Handle for Matter task (DISABLED - Matter is not needed) */
extern TaskHandle_t matter_task_handle;
/** @brief Handle pro Web Server task */
extern TaskHandle_t web_server_task_handle;
/** @brief Handle pro Reset Button task */
extern TaskHandle_t reset_button_task_handle;
/** @brief Handle pro Promotion Button task */
extern TaskHandle_t promotion_button_task_handle;

#ifdef __cplusplus
}
#endif

#endif // FREERTOS_CHESS_H