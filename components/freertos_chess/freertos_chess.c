/**
 * @file freertos_chess.c
 * @brief ESP32-C6 Chess System v2.4 - Implementace FreeRTOS Chess komponenty
 *
 * Tato komponenta poskytuje zakladni FreeRTOS infrastrukturu pro sachovy
 * system:
 * - Inicializace hardware a GPIO konfigurace
 * - Vytvareni a sprava front a mutexu
 * - Systemove utility funkce
 * - Hardware abstraction layer
 *
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 *
 * @details
 * Tato komponenta je srdcem FreeRTOS infrastruktury sachoveho systemu.
 * Obsahuje inicializaci vsech hardware komponent, vytvareni front
 * a mutexu, a poskytuje utility funkce pro cely system.
 *
 * Hardware funkce:
 * - WS2812B LED paska (73 LED: 64 sachovnice + 9 tlacitek)
 * - 8x8 Reed Switch matice pro detekci figurek
 * - Tlacitkove LED feedback system
 * - Time-multiplexed GPIO sdileni
 * - USB Serial JTAG konzole
 *
 * GPIO mapovani (ESP32-C6 DevKit):
 * - LED Data: GPIO7 (WS2812B) - Bezpecny pin
 * - Matrix Rows: GPIO10,11,18,19,20,21,22,23 (8 vystupu)
 * - Matrix Columns: GPIO0,1,2,3,6,14,16,17 (8 vstupu s pull-up)
 * - Tlacitkove piny: Sdilene s matrix columns (time-multiplexed)
 * - Status LED: GPIO5 (samostatny od matice - GPIO8 je boot strapping pin)
 * - Reset tlacitko: GPIO15 (samostatny pin)
 * - UART: USB Serial JTAG (vestavene, zadne externi piny)
 *
 * Time-Multiplexing (25ms cyklus):
 * - 0-20ms: Matrix skenovani (8x8 reed switchu)
 * - 20-25ms: Tlacitkove skenovani (9 tlacitek)
 * - LED aktualizace probiha nezavisle mimo multiplexing cyklus
 */

#include "freertos_chess.h"
#include "button_task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "led_task.h"
#include "nvs_flash.h"
#include "shared_buffer_pool.h"
#include "streaming_output.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "FREERTOS_CHESS";

// ============================================================================
// GLOBAL QUEUE AND MUTEX HANDLES
// ============================================================================

// LED control queues - REMOVED: Using direct LED calls instead
// QueueHandle_t led_command_queue = NULL;  //  REMOVED: Queue hell eliminated
// QueueHandle_t led_status_queue = NULL;   //  REMOVED: Queue hell eliminated

// Matrix event queues
QueueHandle_t matrix_event_queue = NULL;
QueueHandle_t matrix_command_queue = NULL;
QueueHandle_t matrix_response_queue = NULL;

// Button event queues
QueueHandle_t button_event_queue = NULL;
QueueHandle_t button_command_queue = NULL;

// UART communication queues
QueueHandle_t uart_command_queue = NULL;
QueueHandle_t uart_response_queue = NULL;

// Game control queues
QueueHandle_t game_command_queue = NULL;
QueueHandle_t game_status_queue = NULL;

// Animation control queues
QueueHandle_t animation_command_queue = NULL;
QueueHandle_t animation_status_queue = NULL;

// Screen saver control queues
QueueHandle_t screen_saver_command_queue = NULL;
QueueHandle_t screen_saver_status_queue = NULL;

// DISABLED: Matter control queues - Matter not needed
// QueueHandle_t matter_command_queue = NULL;
// extern QueueHandle_t matter_status_queue;

// Web server control queues
QueueHandle_t web_command_queue = NULL;
extern QueueHandle_t web_server_command_queue;
extern QueueHandle_t web_server_status_queue;

// Test control queues
QueueHandle_t test_command_queue = NULL;

// System mutexes
SemaphoreHandle_t led_mutex = NULL;
SemaphoreHandle_t matrix_mutex = NULL;
SemaphoreHandle_t button_mutex = NULL;
SemaphoreHandle_t game_mutex = NULL;
SemaphoreHandle_t system_mutex = NULL;

// System timers
TimerHandle_t matrix_scan_timer =
    NULL; // LEGACY - not used with coordinated system
TimerHandle_t button_scan_timer =
    NULL; // LEGACY - not used with coordinated system
TimerHandle_t led_update_timer = NULL; // Timer pro periodické obnovení LED
TimerHandle_t system_health_timer = NULL;

// Koordinovaný time-multiplexing timer (perioda 25ms)
static TimerHandle_t coordinated_multiplex_timer = NULL;

// PRODUCTION STABILITY:
// DO NOT run multiplex scanning inside FreeRTOS Timer Service task ("Tmr Svc").
// We'll run the coordinated scan in a dedicated task with sufficient stack.
static TaskHandle_t coordinated_multiplex_task_handle = NULL;
#define COORDINATED_MUX_TASK_STACK_SIZE (8 * 1024)

// System state
static bool system_initialized = false;
static bool hardware_initialized = false;
static bool freertos_initialized = false;

// GPIO pin arrays
const gpio_num_t matrix_row_pins[8] = {MATRIX_ROW_0, MATRIX_ROW_1, MATRIX_ROW_2,
                                       MATRIX_ROW_3, MATRIX_ROW_4, MATRIX_ROW_5,
                                       MATRIX_ROW_6, MATRIX_ROW_7};

const gpio_num_t matrix_col_pins[8] = {MATRIX_COL_0, MATRIX_COL_1, MATRIX_COL_2,
                                       MATRIX_COL_3, MATRIX_COL_4, MATRIX_COL_5,
                                       MATRIX_COL_6, MATRIX_COL_7};

const gpio_num_t promotion_button_pins_a[4] = {BUTTON_QUEEN, BUTTON_ROOK,
                                               BUTTON_BISHOP, BUTTON_KNIGHT};

// NEPOUZIVANO: Toto pole se nikde v kódu nepoužívá!
// Původně bylo navrženo pro druhou sadu promotion tlačítek, ale systém
// používá pouze promotion_button_pins_a (4 sdílená tlačítka pro oba hráče).
// const gpio_num_t promotion_button_pins_b[4] = {
//     BUTTON_PROMOTION_QUEEN, BUTTON_PROMOTION_ROOK, BUTTON_PROMOTION_BISHOP,
//     BUTTON_PROMOTION_KNIGHT};

// ============================================================================
// GPIO VALIDATION FUNCTIONS
// ============================================================================

// Enhanced GPIO validation function
static esp_err_t validate_gpio_pin(gpio_num_t pin, const char *pin_name) {
  // Check if pin exists
  if (pin < 0 || pin > 30) {
    ESP_LOGE(TAG, "Invalid GPIO pin %d for %s (ESP32-C6 has GPIO 0-30)", pin,
             pin_name);
    return ESP_ERR_INVALID_ARG;
  }

  // Check SPI Flash reserved pins (GPIO 24-26 are reserved, but 27-30 can be
  // used for I/O)
  if (pin >= 24 && pin <= 26) {
    ESP_LOGE(TAG, "GPIO %d (%s) is reserved for SPI Flash on ESP32-C6", pin,
             pin_name);
    return ESP_ERR_INVALID_ARG;
  }

  // Warn about SPI Flash pins that can be used for I/O
  if (pin >= 27 && pin <= 30) {
    ESP_LOGW(TAG,
             "GPIO %d (%s) is SPI Flash pin but can be used for I/O - use with "
             "caution",
             pin, pin_name);
  }

  // Warn about strapping pins
  if (pin == 4 || pin == 5 || pin == 8 || pin == 9 || pin == 15) {
    ESP_LOGW(TAG, "GPIO %d (%s) is a strapping pin - use with caution", pin,
             pin_name);
  }

  // Warn about USB-JTAG pins
  if (pin == 12 || pin == 13) {
    ESP_LOGW(TAG, "GPIO %d (%s) is used for USB-JTAG debugging", pin, pin_name);
  }

  return ESP_OK;
}

// ============================================================================
// HARDWARE INITIALIZATION FUNCTIONS
// ============================================================================

esp_err_t chess_gpio_init(void) {
  ESP_LOGI(TAG, "Initializing GPIO pins...");

  // DEBUG: Ověření definice pinů
  ESP_LOGI(TAG, "DEBUG: STATUS_LED_PIN = GPIO%d, BUTTON_RESET = GPIO%d",
           STATUS_LED_PIN, BUTTON_RESET);
  ESP_LOGI(TAG, "DEBUG: LED_DATA_PIN = GPIO%d", LED_DATA_PIN);

  // GPIO initialization - no WDT reset needed during init

  // Enhanced GPIO validation using the new validation function
  ESP_LOGI(TAG, "Validating GPIO pin assignments...");

  // Validate LED pins
  esp_err_t ret = validate_gpio_pin(LED_DATA_PIN, "LED_DATA_PIN");
  if (ret != ESP_OK)
    return ret;

  ret = validate_gpio_pin(STATUS_LED_PIN, "STATUS_LED_PIN");
  if (ret != ESP_OK)
    return ret;

  // Validate matrix row pins
  for (int i = 0; i < 8; i++) {
    // GPIO validation - no WDT reset needed during init
    ret = validate_gpio_pin(matrix_row_pins[i], "MATRIX_ROW");
    if (ret != ESP_OK)
      return ret;
  }

  // Validate matrix column pins
  for (int i = 0; i < 8; i++) {
    // GPIO validation - no WDT reset needed during init
    ret = validate_gpio_pin(matrix_col_pins[i], "MATRIX_COL");
    if (ret != ESP_OK)
      return ret;
  }

  // Validate reset button pin
  ret = validate_gpio_pin(BUTTON_RESET, "BUTTON_RESET");
  if (ret != ESP_OK)
    return ret;

  ESP_LOGI(TAG, "✓ GPIO safety checks passed");

  // GPIO configuration - no WDT reset needed during init

  // Configure matrix row pins as outputs
  for (int i = 0; i < 8; i++) {
    // GPIO config - no WDT reset needed during init

    // Bitová maska s explicitním přetypováním
    uint32_t pin_number = (uint32_t)matrix_row_pins[i];
    uint64_t pin_mask = (1ULL << pin_number);

    ESP_LOGI(TAG, "Configuring MATRIX_ROW_%d (GPIO%d, mask=0x%llx)...", i,
             pin_number, pin_mask);

    gpio_config_t io_conf = {.pin_bit_mask = pin_mask,
                             .mode = GPIO_MODE_OUTPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    ret = gpio_config(&io_conf);
    ESP_LOGI(TAG, "gpio_config returned %s", esp_err_to_name(ret));

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure matrix row pin %d (GPIO%d): %s", i,
               pin_number, esp_err_to_name(ret));
      return ret; // Return error instead of continuing
    } else {
      gpio_set_level(matrix_row_pins[i],
                     1); // Set all rows HIGH initially (inactive state)
      ESP_LOGI(TAG, "gpio_set_level done for GPIO%d", pin_number);
    }
  }

  // Configure matrix column pins as inputs with pull-up
  ESP_LOGI(TAG, "DEBUG: Starting matrix column configuration loop");
  for (int i = 0; i < 8; i++) {
    // WDT reset odstraněn během inicializace

    // Bitová maska s explicitním přetypováním
    uint32_t pin_number = (uint32_t)matrix_col_pins[i];
    uint64_t pin_mask = (1ULL << pin_number);

    ESP_LOGI(TAG, "Configuring MATRIX_COL_%d (GPIO%d, mask=0x%llx)...", i,
             pin_number, pin_mask);

    // DEBUG: Special logging for GPIO17 (column 7)
    if (pin_number == 17) {
      ESP_LOGI(TAG, "🔍 DEBUG: Configuring GPIO17 (MATRIX_COL_7, index=%d)", i);
    }

    // CRITICAL: Skip strapping pins to avoid system reset
    if (pin_number == 9) {
      ESP_LOGW(TAG, "Skipping GPIO%d (strapping pin) to avoid system reset",
               pin_number);
      ESP_LOGI(TAG, "Matrix column pin %d skipped (strapping pin)", i);
      ESP_LOGI(TAG, "DEBUG: About to continue to next iteration");
      continue; // Přeskočení strapping pinu
    }

    ESP_LOGI(TAG, "DEBUG: Proceeding with GPIO%d configuration", pin_number);

    // Explicitly reset GPIO17 before configuration
    // GPIO17 is UART0 RX pin on ESP32-C6 and may have special properties
    // that need to be cleared before using it as GPIO input
    if (pin_number == 17) {
      ESP_LOGI(TAG, "🔍 DEBUG: Resetting GPIO17 before configuration (UART0 RX pin)");
      gpio_reset_pin(GPIO_NUM_17);
      // Small delay to ensure reset is complete
      vTaskDelay(pdMS_TO_TICKS(10));
      
      // CRITICAL: Explicitly switch GPIO17 from UART function to GPIO function
      // GPIO17 is UART0 RX pin on ESP32-C6 and may be stuck in UART mode
      // even if UART driver is not active. We need to force it to GPIO mode.
      // Strategy: Call gpio_set_direction() BEFORE gpio_config() to ensure
      // IOMUX is switched to GPIO function before configuration
      gpio_set_direction(GPIO_NUM_17, GPIO_MODE_INPUT);
      ESP_LOGI(TAG, "🔧 DEBUG: GPIO17 direction set to INPUT (forcing IOMUX switch from UART to GPIO)");
      
      // Small delay to ensure IOMUX switch is complete
      vTaskDelay(pdMS_TO_TICKS(5));
    }

    // WDT reset odstraněn během inicializace

    // Configure all matrix column pins as INPUT with pull-up (standard configuration)
    gpio_config_t io_conf = {.pin_bit_mask = pin_mask,
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_ENABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    ret = gpio_config(&io_conf);
    ESP_LOGI(TAG, "gpio_config returned %s", esp_err_to_name(ret));

    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to configure matrix column pin %d (GPIO%d): %s", i,
               pin_number, esp_err_to_name(ret));
      return ret; // Return error instead of continuing
    } else {
      ESP_LOGI(TAG, "Matrix column pin %d configured successfully", i);
      
      // Verify GPIO17 configuration (UART0 RX pin - needs explicit GPIO mode)
      if (pin_number == 17) {
        // Explicitly set GPIO17 to GPIO INPUT mode (not UART mode)
        // GPIO17 is UART0 RX pin on ESP32-C6 and may default to UART mode
        // even if UART is not active. Force it to GPIO INPUT mode.
        gpio_set_direction(GPIO_NUM_17, GPIO_MODE_INPUT);
        gpio_set_pull_mode(GPIO_NUM_17, GPIO_PULLUP_ONLY);
        
        // Verify configuration
        int test_level = gpio_get_level(GPIO_NUM_17);
        ESP_LOGI(TAG, "🔧 GPIO17: Configured as INPUT with pull-up, initial level=%d", test_level);
      }
    }
  }
  ESP_LOGI(TAG, "DEBUG: Matrix column configuration loop completed");

  ESP_LOGI(TAG, "DEBUG: Matrix column configuration loop completed");

  // Configure status LED pin
  // WDT reset removed during initialization  // Reset před status LED config
  ESP_LOGI(TAG, "DEBUG: About to configure STATUS_LED");

  // Správná bitová maska s explicitním castem
  uint32_t status_led_pin = (uint32_t)STATUS_LED_PIN;
  uint64_t status_led_mask = (1ULL << status_led_pin);

  ESP_LOGI(TAG, "Configuring STATUS_LED (GPIO%d, mask=0x%llx)...",
           status_led_pin, status_led_mask);

  // Reset před gpio_config()
  // WDT reset removed during initialization

  gpio_config_t status_led_conf = {.pin_bit_mask = status_led_mask,
                                   .mode = GPIO_MODE_OUTPUT,
                                   .pull_up_en = GPIO_PULLUP_DISABLE,
                                   .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                   .intr_type = GPIO_INTR_DISABLE};
  ret = gpio_config(&status_led_conf);
  ESP_LOGI(TAG, "gpio_config returned %s", esp_err_to_name(ret));

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure status LED pin (GPIO%d): %s",
             status_led_pin, esp_err_to_name(ret));
    return ret; // Return error instead of continuing
  } else {
    gpio_set_level(STATUS_LED_PIN, 0); // LED off initially
    ESP_LOGI(TAG, "Status LED configured successfully");
  }

  // Configure reset button pin
  // WDT reset removed during initialization  // Reset před reset button config

  // Správná bitová maska s explicitním castem
  uint32_t reset_button_pin = (uint32_t)BUTTON_RESET;
  uint64_t reset_button_mask = (1ULL << reset_button_pin);

  ESP_LOGI(TAG, "Configuring RESET_BUTTON (GPIO%d, mask=0x%llx)...",
           reset_button_pin, reset_button_mask);

  // Reset před gpio_config()
  // WDT reset removed during initialization

  gpio_config_t reset_button_conf = {.pin_bit_mask = reset_button_mask,
                                     .mode = GPIO_MODE_INPUT,
                                     .pull_up_en = GPIO_PULLUP_ENABLE,
                                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                     .intr_type = GPIO_INTR_DISABLE};
  ret = gpio_config(&reset_button_conf);
  ESP_LOGI(TAG, "gpio_config returned %s", esp_err_to_name(ret));

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure reset button pin (GPIO%d): %s",
             reset_button_pin, esp_err_to_name(ret));
    return ret; // Return error instead of continuing
  } else {
    ESP_LOGI(TAG, "Reset button configured successfully");
  }

  // PŘIDAT: Finální reset
  // WDT reset removed during initialization

  ESP_LOGI(TAG, "✓ GPIO pins initialized successfully");

  // Dokončení GPIO konfigurace
  // WDT reset removed during initialization

  // Fallback: If any GPIO configuration failed, log warning but continue
  // ESP_LOGW(TAG, " Some GPIO pins may not be configured correctly - running
  // in simulation mode"); ESP_LOGW(TAG, " Hardware functionality will be
  // limited - check GPIO pin assignments");

  return ESP_OK;
}

esp_err_t chess_led_init(void) {
  ESP_LOGI(TAG, "🔧 Initializing WS2812B LED system...");

  // Reset watchdog timeru před inicializací LED
  esp_err_t wdt_ret = esp_task_wdt_reset();
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
    // Task not registered with TWDT yet - this is normal during startup
  }

  // Inicializace WS2812B hardwaru
  // This function will be called by led_task.c during hardware initialization
  // We just ensure the system is ready for LED operations

  ESP_LOGI(TAG, "✅ LED system initialization prepared");
  ESP_LOGI(TAG, "  - WS2812B data pin: GPIO%d", LED_DATA_PIN);
  ESP_LOGI(TAG, "  - Total LEDs: %d (64 board + 9 buttons)",
           CHESS_LED_COUNT_TOTAL);
  ESP_LOGI(TAG, "  - Hardware initialization will be done by led_task.c");

  return ESP_OK;
}

esp_err_t chess_matrix_init(void) {
  ESP_LOGI(TAG, "Initializing matrix system...");

  // WDT reset pro matrix inicializaci
  // WDT reset removed during initialization

  // Matrix is already configured in GPIO init
  // In simulation mode, we'll simulate reed switch detection
  ESP_LOGI(TAG, "✓ Matrix system initialized");
  ESP_LOGI(TAG, "  - 8x8 reed switch matrix");
  ESP_LOGI(TAG, "  - Row pins: GPIO%d,%d,%d,%d,%d,%d,%d,%d", MATRIX_ROW_0,
           MATRIX_ROW_1, MATRIX_ROW_2, MATRIX_ROW_3, MATRIX_ROW_4, MATRIX_ROW_5,
           MATRIX_ROW_6, MATRIX_ROW_7);
  ESP_LOGI(TAG, "  - Column pins: GPIO%d,%d,%d,%d,%d,%d,%d,%d", MATRIX_COL_0,
           MATRIX_COL_1, MATRIX_COL_2, MATRIX_COL_3, MATRIX_COL_4, MATRIX_COL_5,
           MATRIX_COL_6, MATRIX_COL_7);
  // ESP_LOGI(
  //     TAG,
  //     "  - Simulation mode: matrix events will be generated
  //     programmatically");

  return ESP_OK;
}

esp_err_t chess_button_init(void) {
  ESP_LOGI(TAG, "Initializing button system...");

  // WDT reset pro button inicializaci
  // WDT reset removed during initialization

  // Buttons are already configured in GPIO init
  // In simulation mode, we'll simulate button presses
  ESP_LOGI(TAG, "✓ Button system initialized");
  ESP_LOGI(TAG, "  - 9 buttons total");
  ESP_LOGI(TAG, "  - Promotion buttons A: Queen, Rook, Bishop, Knight");
  ESP_LOGI(TAG, "  - Promotion buttons B: Queen, Rook, Bishop, Knight");
  ESP_LOGI(TAG, "  - Reset button: GPIO%d", BUTTON_RESET);
  // ESP_LOGI(
  //     TAG,
  //     "  - Simulation mode: button events will be generated
  //     programmatically");

  return ESP_OK;
}

esp_err_t chess_hardware_init(void) {
  ESP_LOGI(TAG, "=== Hardware Initialization ===");

  esp_err_t ret;

  // Initialize GPIO
  ESP_LOGI(TAG, "🔄 Initializing GPIO...");
  // WDT reset removed during initialization
  ret = chess_gpio_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "GPIO initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ GPIO initialized successfully");
  vTaskDelay(pdMS_TO_TICKS(1)); // Allow other tasks to run

  // Initialize LED system
  ESP_LOGI(TAG, "🔄 Initializing LED system...");
  // WDT reset removed during initialization
  ret = chess_led_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "LED initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ LED system initialized successfully");
  vTaskDelay(pdMS_TO_TICKS(1)); // Allow other tasks to run

  // Initialize matrix system
  ESP_LOGI(TAG, "🔄 Initializing matrix system...");
  // WDT reset removed during initialization
  ret = chess_matrix_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Matrix initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ Matrix system initialized successfully");
  vTaskDelay(pdMS_TO_TICKS(1)); // Allow other tasks to run

  // Initialize button system
  ESP_LOGI(TAG, "🔄 Initializing button system...");
  // WDT reset removed during initialization
  ret = chess_button_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Button initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ Button system initialized successfully");

  hardware_initialized = true;
  ESP_LOGI(TAG, "✓ All hardware systems initialized successfully");
  return ESP_OK;
}

// ============================================================================
// FREERTOS INITIALIZATION FUNCTIONS
// ============================================================================

esp_err_t chess_create_queues(void) {
  ESP_LOGI(TAG, "=== CREATING FREERTOS QUEUES ===");
  ESP_LOGI(TAG, "Free heap before queues: %zu bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "Min free heap: %zu bytes", esp_get_minimum_free_heap_size());
  ESP_LOGI(TAG, "========================================");

  // WDT reset pro vytváření front
  // WDT reset removed during initialization

  // CRITICAL: Check heap availability before creating queues
  size_t free_heap = esp_get_free_heap_size();
  if (free_heap < 50000) { // Minimum 50KB free heap required
    ESP_LOGE(
        TAG,
        "Insufficient free heap for queue creation: %zu bytes (minimum 50000)",
        free_heap);
    return ESP_ERR_NO_MEM;
  }

  // LED queues - REMOVED: Using direct LED calls instead
  ESP_LOGI(TAG, "🔄 LED queues removed - using direct LED calls");
  ESP_LOGI(TAG, "✅ LED system simplified. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Matrix event queues
  ESP_LOGI(TAG, "🔄 Creating Matrix queues...");
  ESP_LOGI(TAG, "  - Matrix Event Queue: %d items × %zu bytes",
           MATRIX_QUEUE_SIZE, sizeof(matrix_event_t));
  SAFE_CREATE_QUEUE(matrix_event_queue, MATRIX_QUEUE_SIZE,
                    sizeof(matrix_event_t), "Matrix Event Queue");
  ESP_LOGI(TAG, "  - Matrix Command Queue: %d items × %zu bytes",
           MATRIX_QUEUE_SIZE, sizeof(uint8_t));
  SAFE_CREATE_QUEUE(matrix_command_queue, MATRIX_QUEUE_SIZE, sizeof(uint8_t),
                    "Matrix Command Queue");
  ESP_LOGI(TAG, "  - Matrix Response Queue: %d items × %zu bytes",
           MATRIX_QUEUE_SIZE, sizeof(game_response_t));
  SAFE_CREATE_QUEUE(matrix_response_queue, MATRIX_QUEUE_SIZE,
                    sizeof(game_response_t), "Matrix Response Queue");
  ESP_LOGI(TAG, "✅ Matrix queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Button event queues
  ESP_LOGI(TAG, "🔄 Creating Button queues...");
  ESP_LOGI(TAG, "  - Button Event Queue: %d items × %zu bytes",
           BUTTON_QUEUE_SIZE, sizeof(button_event_t));
  SAFE_CREATE_QUEUE(button_event_queue, BUTTON_QUEUE_SIZE,
                    sizeof(button_event_t), "Button Event Queue");
  ESP_LOGI(TAG, "  - Button Command Queue: %d items × %zu bytes",
           BUTTON_QUEUE_SIZE, sizeof(uint8_t));
  SAFE_CREATE_QUEUE(button_command_queue, BUTTON_QUEUE_SIZE, sizeof(uint8_t),
                    "Button Command Queue");
  ESP_LOGI(TAG, "✅ Button queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // UART communication queues
  ESP_LOGI(TAG, "🔄 Creating UART queues...");
  ESP_LOGI(TAG, "  - UART Command Queue: %d items × %zu bytes", UART_QUEUE_SIZE,
           sizeof(char[64]));
  SAFE_CREATE_QUEUE(uart_command_queue, UART_QUEUE_SIZE, sizeof(char[64]),
                    "UART Command Queue");
  ESP_LOGI(TAG, "  - UART Response Queue: %d items × %zu bytes",
           UART_QUEUE_SIZE, sizeof(game_response_t));
  SAFE_CREATE_QUEUE(uart_response_queue, UART_QUEUE_SIZE,
                    sizeof(game_response_t), "UART Response Queue");

  // CRITICAL: Create UART output queue for centralized output
  extern QueueHandle_t uart_output_queue;
  ESP_LOGI(TAG, "  - UART Output Queue: %d items × %zu bytes", 20, 128);
  SAFE_CREATE_QUEUE(
      uart_output_queue, 20, 128,
      "UART Output Queue"); // Reduced item size from 512 to 128 to save ~7.5KB

  ESP_LOGI(TAG, "✅ UART queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Game control queues
  ESP_LOGI(TAG, "🔄 Creating Game queues...");
  ESP_LOGI(TAG, "  - Game Command Queue: %d items × %zu bytes", GAME_QUEUE_SIZE,
           sizeof(chess_move_command_t));
  SAFE_CREATE_QUEUE(game_command_queue, GAME_QUEUE_SIZE,
                    sizeof(chess_move_command_t), "Game Command Queue");
  ESP_LOGI(TAG, "  - Game Status Queue: %d items × %zu bytes", GAME_QUEUE_SIZE,
           sizeof(uint8_t));
  SAFE_CREATE_QUEUE(game_status_queue, GAME_QUEUE_SIZE, sizeof(uint8_t),
                    "Game Status Queue");
  ESP_LOGI(TAG, "✅ Game queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Animation control queues
  ESP_LOGI(TAG, "🔄 Creating Animation queues...");
  ESP_LOGI(TAG, "  - Animation Command Queue: %d items × %zu bytes",
           ANIMATION_QUEUE_SIZE, sizeof(led_command_t));
  SAFE_CREATE_QUEUE(animation_command_queue, ANIMATION_QUEUE_SIZE,
                    sizeof(led_command_t), "Animation Command Queue");
  ESP_LOGI(TAG, "  - Animation Status Queue: %d items × %zu bytes",
           ANIMATION_QUEUE_SIZE, sizeof(esp_err_t));
  SAFE_CREATE_QUEUE(animation_status_queue, ANIMATION_QUEUE_SIZE,
                    sizeof(esp_err_t), "Animation Status Queue");
  ESP_LOGI(TAG, "✅ Animation queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Screen saver control queues
  ESP_LOGI(TAG, "🔄 Creating Screen Saver queues...");
  ESP_LOGI(TAG, "  - Screen Saver Command Queue: %d items × %zu bytes",
           SCREEN_SAVER_QUEUE_SIZE, sizeof(uint8_t));
  SAFE_CREATE_QUEUE(screen_saver_command_queue, SCREEN_SAVER_QUEUE_SIZE,
                    sizeof(uint8_t), "Screen Saver Command Queue");
  ESP_LOGI(TAG, "  - Screen Saver Status Queue: %d items × %zu bytes",
           SCREEN_SAVER_QUEUE_SIZE, sizeof(esp_err_t));
  SAFE_CREATE_QUEUE(screen_saver_status_queue, SCREEN_SAVER_QUEUE_SIZE,
                    sizeof(esp_err_t), "Screen Saver Status Queue");
  ESP_LOGI(TAG, "✅ Screen Saver queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // DISABLED: Matter control queues - Matter not needed
  /*
  ESP_LOGI(TAG, "🔄 Creating Matter queues...");
  ESP_LOGI(TAG, "  - Matter Command Queue: %d items × %zu bytes",
  MATTER_QUEUE_SIZE, sizeof(uint8_t)); SAFE_CREATE_QUEUE(matter_command_queue,
  MATTER_QUEUE_SIZE, sizeof(uint8_t), "Matter Command Queue"); ESP_LOGI(TAG, "
  - Matter Status Queue: %d items × %zu bytes", MATTER_QUEUE_SIZE,
  sizeof(esp_err_t)); SAFE_CREATE_QUEUE(matter_status_queue, MATTER_QUEUE_SIZE,
  sizeof(esp_err_t), "Matter Status Queue"); ESP_LOGI(TAG, "✅ Matter queues
  created. Free heap: %zu bytes", esp_get_free_heap_size());
  */

  // Web server control queues
  ESP_LOGI(TAG, "🔄 Creating Web Server queues...");
  ESP_LOGI(TAG, "  - Web Command Queue: %d items × %zu bytes",
           WEB_SERVER_QUEUE_SIZE, sizeof(uint8_t));
  SAFE_CREATE_QUEUE(web_command_queue, WEB_SERVER_QUEUE_SIZE, sizeof(uint8_t),
                    "Web Command Queue");
  ESP_LOGI(TAG, "  - Web Server Command Queue: %d items × %zu bytes",
           WEB_SERVER_QUEUE_SIZE, sizeof(uint8_t));
  SAFE_CREATE_QUEUE(web_server_command_queue, WEB_SERVER_QUEUE_SIZE,
                    sizeof(uint8_t), "Web Server Command Queue");
  ESP_LOGI(TAG, "  - Web Server Status Queue: %d items × %zu bytes",
           WEB_SERVER_QUEUE_SIZE, sizeof(esp_err_t));
  SAFE_CREATE_QUEUE(web_server_status_queue, WEB_SERVER_QUEUE_SIZE,
                    sizeof(esp_err_t), "Web Server Status Queue");
  ESP_LOGI(TAG, "✅ Web Server queues created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  // Test control queues
  ESP_LOGI(TAG, "🔄 Creating Test queue...");
  ESP_LOGI(TAG, "  - Test Command Queue: %d items × %zu bytes", LED_QUEUE_SIZE,
           sizeof(uint8_t));
  SAFE_CREATE_QUEUE(test_command_queue, LED_QUEUE_SIZE, sizeof(uint8_t),
                    "Test Command Queue");
  ESP_LOGI(TAG, "✅ Test queue created. Free heap: %zu bytes",
           esp_get_free_heap_size());

  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "🎉 ALL FREERTOS QUEUES CREATED SUCCESSFULLY!");
  ESP_LOGI(TAG, "Final free heap: %zu bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "========================================");

  // CRITICAL: Validate all queues were created successfully
  if (!matrix_event_queue || !matrix_command_queue || !matrix_response_queue ||
      !button_event_queue || !button_command_queue || !uart_command_queue ||
      !uart_response_queue || !game_command_queue || !game_status_queue ||
      !animation_command_queue || !animation_status_queue ||
      !screen_saver_command_queue || !screen_saver_status_queue ||
      /* !matter_command_queue || !matter_status_queue || */ // DISABLED -
                                                             // Matter not
                                                             // needed
      !web_command_queue || !web_server_command_queue ||
      !web_server_status_queue || !test_command_queue) {
    ESP_LOGE(TAG, "One or more queues failed to create - system initialization "
                  "will fail");
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "✅ All queue handles validated successfully");
  return ESP_OK;
}

esp_err_t chess_create_mutexes(void) {
  ESP_LOGI(TAG, "Creating FreeRTOS mutexes...");

  SAFE_CREATE_MUTEX(led_mutex, "LED Mutex");
  SAFE_CREATE_MUTEX(matrix_mutex, "Matrix Mutex");
  SAFE_CREATE_MUTEX(button_mutex, "Button Mutex");
  SAFE_CREATE_MUTEX(game_mutex, "Game Mutex");
  SAFE_CREATE_MUTEX(system_mutex, "System Mutex");

  ESP_LOGI(TAG, "✓ All FreeRTOS mutexes created successfully");
  return ESP_OK;
}

// ============================================================================
// TIMER CALLBACK FUNCTIONS
// ============================================================================

// ============================================================================
// COORDINATED TIME-MULTIPLEXING SYSTEM
// ============================================================================

/**
 * @brief Hlavni coordinated multiplexing timer callback
 *
 * Tato funkce ridi 25ms multiplexing cyklus:
 * - 0-20ms:  Matrix scan window (matrix ma kontrolu nad GPIO)
 * - 20-25ms: Button scan window (button ma kontrolu nad GPIO)
 *
 * @param xTimer Timer handle
 *
 * @details
 * Tento timer je KRITICKA cast time-multiplexing systemu.
 * Matrix a button tasky sdilejí stejne GPIO piny (MATRIX_COL_0-7),
 * proto MUSI byt jejich pristup k pinum synchronizovan.
 *
 * Casovani:
 * 1. Matrix scan vola matrix_scan_all() primo v callbacku (rychle)
 * 2. Po matrix scan: matrix_release_pins() uvolni row piny
 * 3. Button scan vola button_scan_all() (rychle)
 * 4. Po button scan: matrix_acquire_pins() (pripravi pro dalsi cyklus)
 */
static void __attribute__((unused))
coordinated_multiplex_timer_callback(TimerHandle_t xTimer) {
  // IMPORTANT:
  // Timer callbacks run in FreeRTOS Timer Service task ("Tmr Svc") which has
  // limited stack. Running matrix scanning / move detection / logging here can
  // crash the system (exactly what your logs show).
  //
  // The coordinated multiplexing is now executed by `coordinated_multiplex_task`.
  (void)xTimer;
}

/**
 * @brief Coordinated time-multiplexing task (25ms cycle)
 *
 * Runs OUTSIDE of "Tmr Svc" with sufficient stack, so scanning can safely call
 * into game/HA/logging paths.
 */
static void coordinated_multiplex_task(void *pvParameters) {
  (void)pvParameters;

  ESP_LOGI(TAG,
           "✅ Coordinated multiplexing TASK started (25ms cycle, stack=%dKB)",
           COORDINATED_MUX_TASK_STACK_SIZE / 1024);

  // Register with TWDT (optional; safe wrapper pattern is used elsewhere)
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGW(TAG, "Multiplex task WDT registration failed: %s",
             esp_err_to_name(wdt_ret));
  }

  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    // Reset WDT (if registered)
    esp_task_wdt_reset();

    // PHASE 1: MATRIX scan
    extern bool matrix_scanning_enabled;
    if (matrix_scanning_enabled) {
      extern void matrix_scan_all(void);
      matrix_scan_all();
    }

    // PHASE 2: release matrix pins for button scan
    extern void matrix_release_pins(void);
    matrix_release_pins();

    // PHASE 3: BUTTON scan
    extern void button_scan_all(void);
    button_scan_all();

    // PHASE 4: re-acquire matrix pins
    extern void matrix_acquire_pins(void);
    matrix_acquire_pins();

    // Full multiplexing cycle
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TOTAL_CYCLE_TIME_MS));
  }
}

// ============================================================================
// LEGACY TIMER CALLBACKS (kept for compatibility, not used)
// ============================================================================

/**
 * @brief Button scan timer callback - LEGACY (not used with coordinated system)
 * @param xTimer Timer handle
 */
void button_scan_timer_callback(TimerHandle_t xTimer) {
  // NOTE: This callback is NOT used when coordinated multiplexing is active
  // Kept for backward compatibility only
  extern void button_scan_all(void);
  button_scan_all();
}

/**
 * @brief Matrix scan timer callback - LEGACY (not used with coordinated system)
 * @param xTimer Timer handle
 */
void matrix_scan_timer_callback(TimerHandle_t xTimer) {
  // NOTE: This callback is NOT used when coordinated multiplexing is active
  // Kept for backward compatibility only
  extern bool matrix_scanning_enabled;
  if (matrix_scanning_enabled) {
    extern void matrix_scan_all(void);
    matrix_scan_all();
  }
}

/**
 * @brief LED update timer callback - REMOVED: Using direct LED calls
 * @param xTimer Timer handle
 */
void led_update_timer_callback(TimerHandle_t xTimer) {
  // NOTE: Timer callbacks run in timer service task context
  // which is not registered with TWDT, so we don't call WDT reset

  // Periodické obnovení LED pro prevenci bílého blikání
  // This ensures LED strip gets regular updates even when no commands are sent
  led_force_immediate_update();
}

esp_err_t chess_create_timers(void) {
  ESP_LOGI(TAG, "Creating FreeRTOS timers...");

  // ============================================================================
  // COORDINATED TIME-MULTIPLEXING TIMER (NEW SYSTEM)
  // ============================================================================

  // PRODUCTION STABILITY:
  // Do not create/start coordinated multiplexing as a FreeRTOS timer.
  // The Timer Service task stack is too small for the call chain
  // (matrix_detect_moves -> ha_light_report_activity -> esp_log -> vfprintf).
  // We use `coordinated_multiplex_task` instead.
  coordinated_multiplex_timer = NULL;
  ESP_LOGI(TAG, "✓ Coordinated multiplexing will run as a TASK (not timer)");

  // ============================================================================
  // LEGACY TIMERS (kept for backward compatibility, not started)
  // ============================================================================

  // Matrix scan timer (20ms period) - LEGACY, not used
  matrix_scan_timer =
      xTimerCreate("MatrixScan", pdMS_TO_TICKS(MATRIX_SCAN_TIME_MS), pdTRUE,
                   NULL, matrix_scan_timer_callback);
  if (matrix_scan_timer == NULL) {
    ESP_LOGW(TAG, "Failed to create legacy matrix scan timer (not critical)");
  }

  // Button scan timer (5ms period) - LEGACY, not used
  button_scan_timer =
      xTimerCreate("ButtonScan", pdMS_TO_TICKS(BUTTON_SCAN_TIME_MS), pdTRUE,
                   NULL, button_scan_timer_callback);
  if (button_scan_timer == NULL) {
    ESP_LOGW(TAG, "Failed to create legacy button scan timer (not critical)");
  }

  // Timer pro periodické obnovení LED
  led_update_timer = xTimerCreate("LEDUpdate", pdMS_TO_TICKS(25), pdTRUE, NULL,
                                  led_update_timer_callback);
  if (led_update_timer == NULL) {
    ESP_LOGE(TAG, "Failed to create LED update timer");
    return ESP_ERR_NO_MEM;
  }
  ESP_LOGI(TAG, "✓ LED update timer created (25ms period)");

  // System health timer - DISABLED (was causing crashes with NULL callback)
  // system_health_timer = xTimerCreate("SystemHealth", pdMS_TO_TICKS(30000),
  // pdTRUE, NULL, NULL); if (system_health_timer == NULL) {
  //     ESP_LOGE(TAG, "Failed to create system health timer");
  //     return ESP_ERR_NO_MEM;
  // }

  ESP_LOGI(TAG, "✓ All FreeRTOS timers created successfully");
  return ESP_OK;
}

esp_err_t chess_start_timers(void) {
  ESP_LOGI(TAG, "Starting FreeRTOS timers...");

  // ============================================================================
  // START COORDINATED MULTIPLEXING TIMER (NEW SYSTEM)
  // ============================================================================

  // Start coordinated multiplexing TASK (safe context)
  if (coordinated_multiplex_task_handle == NULL) {
    BaseType_t ok =
        xTaskCreate(coordinated_multiplex_task, "coord_mux_task",
                    COORDINATED_MUX_TASK_STACK_SIZE, NULL, MATRIX_TASK_PRIORITY,
                    &coordinated_multiplex_task_handle);
    if (ok != pdPASS || coordinated_multiplex_task_handle == NULL) {
      ESP_LOGE(TAG, "Failed to create coordinated multiplexing task");
      return ESP_ERR_NO_MEM;
    }
  }

  ESP_LOGI(TAG, "✓ Coordinated multiplexing task active (25ms cycle)");
  ESP_LOGI(TAG, "  ✅ Matrix and button scanning are now coordinated!");
  ESP_LOGI(TAG, "  ✅ No GPIO conflicts - clean time-multiplexing active");

  // ============================================================================
  // LEGACY TIMERS (DO NOT START - would cause conflicts)
  // ============================================================================

  // DO NOT start legacy matrix_scan_timer - would conflict with coordinated
  // timer
  ESP_LOGI(
      TAG,
      "  ⏸️  Legacy matrix scan timer NOT started (coordinated system active)");

  // DO NOT start legacy button_scan_timer - would conflict with coordinated
  // timer
  ESP_LOGI(
      TAG,
      "  ⏸️  Legacy button scan timer NOT started (coordinated system active)");

  // DISABLED: LED update timer causes WDT errors (timer service task not
  // registered with TWDT)
  /*
  if (led_update_timer != NULL) {
      if (xTimerStart(led_update_timer, 0) != pdPASS) {
          ESP_LOGE(TAG, "Failed to start LED update timer");
          return ESP_ERR_INVALID_STATE;
      }
      ESP_LOGI(TAG, "✓ LED update timer started (25ms period)");
  }
  */
  ESP_LOGI(TAG, "  ⏸️  LED update timer DISABLED (causes WDT errors)");

  // System health timer - DISABLED (was causing crashes)
  // if (system_health_timer != NULL) {
  //     if (xTimerStart(system_health_timer, 0) != pdPASS) {
  //         ESP_LOGE(TAG, "Failed to start system health timer");
  //         return ESP_ERR_INVALID_STATE;
  //     }
  //     ESP_LOGI(TAG, "✓ System health timer started");
  // }

  ESP_LOGI(TAG, "✓ All FreeRTOS timers started successfully");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "🎯 TIME-MULTIPLEXING SYSTEM ACTIVE!");
  ESP_LOGI(TAG, "========================================");
  return ESP_OK;
}

esp_err_t chess_freertos_init(void) {
  ESP_LOGI(TAG, "=== FreeRTOS Initialization ===");
  ESP_LOGI(TAG, "Free heap before FreeRTOS init: %zu bytes",
           esp_get_free_heap_size());
  ESP_LOGI(TAG, "Min free heap: %zu bytes", esp_get_minimum_free_heap_size());
  ESP_LOGI(TAG, "========================================");
  // Don't reset watchdog here - it might not be initialized yet

  esp_err_t ret;

  // Create queues
  ESP_LOGI(TAG, "🔄 Creating FreeRTOS queues...");
  ret = chess_create_queues();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Queue creation failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ FreeRTOS queues created successfully");

  // Create mutexes
  ESP_LOGI(TAG, "🔄 Creating FreeRTOS mutexes...");
  ret = chess_create_mutexes();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Mutex creation failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ FreeRTOS mutexes created successfully");

  // Create timers
  ESP_LOGI(TAG, "🔄 Creating FreeRTOS timers...");
  ret = chess_create_timers();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Timer creation failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ FreeRTOS timers created successfully");

  freertos_initialized = true;
  // Don't reset watchdog here - it might not be initialized yet
  ESP_LOGI(TAG, "🎉 FreeRTOS infrastructure initialized successfully");
  return ESP_OK;
}

// ============================================================================
// SYSTEM UTILITY FUNCTIONS
// ============================================================================

esp_err_t chess_nvs_init(void) {
  ESP_LOGI(TAG, "Initializing NVS flash...");

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS partition was truncated and erased");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(TAG, "✓ NVS flash initialized successfully");
  return ESP_OK;
}

esp_err_t chess_system_init(void) {
  ESP_LOGI(TAG, "=== System Initialization ===");
  // Don't reset watchdog here - it might not be initialized yet
  ESP_LOGI(TAG, "🔄 Starting NVS initialization...");

  esp_err_t ret;

  // Initialize NVS
  ret = chess_nvs_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "NVS initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ NVS initialization completed successfully");

  // Initialize hardware
  ESP_LOGI(TAG, "🔄 Starting hardware initialization...");
  // WDT reset removed during initialization // Reset watchdog before hardware
  // initialization
  ret = chess_hardware_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Hardware initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ Hardware initialization completed successfully");

  // Initialize memory optimization systems
  ESP_LOGI(TAG, "🔄 Starting memory optimization systems initialization...");
  ret = chess_memory_systems_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Memory systems initialization failed");
    return ret;
  }
  ESP_LOGI(TAG, "✅ Memory optimization systems initialized successfully");

  // Initialize FreeRTOS infrastructure
  ESP_LOGI(TAG, "🔄 Starting FreeRTOS infrastructure initialization...");
  ret = chess_freertos_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "FreeRTOS initialization failed");
    return ret;
  }
  ESP_LOGI(TAG,
           "✅ FreeRTOS infrastructure initialization completed successfully");

  system_initialized = true;
  // Don't reset watchdog here - it might not be initialized yet
  ESP_LOGI(TAG, "🎉 System initialization completed successfully");
  return ESP_OK;
}

esp_err_t chess_check_memory_health(void) {
  size_t free_heap = esp_get_free_heap_size();
  size_t min_free_heap = esp_get_minimum_free_heap_size();

  if (free_heap < 10000) { // Less than 10KB free
    ESP_LOGW(TAG, "Low memory warning: %zu bytes free", free_heap);
    return ESP_ERR_NO_MEM;
  }

  if (free_heap < 5000) { // Less than 5KB free
    ESP_LOGE(TAG, "Critical memory warning: %zu bytes free", free_heap);
    return ESP_ERR_NO_MEM;
  }

  ESP_LOGI(TAG, "Memory health: %zu bytes free, %zu bytes minimum", free_heap,
           min_free_heap);
  return ESP_OK;
}

esp_err_t chess_monitor_tasks(void) {
  UBaseType_t task_count = uxTaskGetNumberOfTasks();

  ESP_LOGI(TAG, "Task monitoring: %d active tasks", task_count);

  if (task_count < 5) { // Should have at least 5 tasks
    ESP_LOGW(TAG, "Low task count warning: %d tasks", task_count);
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

void chess_print_system_info(void) {
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "ESP32-C6 Chess System v2.4 Information");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Version: %s", CHESS_VERSION_STRING);
  ESP_LOGI(TAG, "Build Date: %s", CHESS_BUILD_DATE);
  ESP_LOGI(TAG, "Author: %s", CHESS_SYSTEM_AUTHOR);
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Hardware Configuration:");
  ESP_LOGI(TAG, "  • LED Data Pin: GPIO%d", LED_DATA_PIN);
  ESP_LOGI(TAG, "  • Status LED: GPIO%d", STATUS_LED_PIN);
  ESP_LOGI(TAG, "  • Reset Button: GPIO%d", BUTTON_RESET);
  ESP_LOGI(TAG, "  • Matrix: 8x8 reed switches");
  ESP_LOGI(TAG, "  • Buttons: 9 total (promotion + reset)");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "System Status:");
  ESP_LOGI(TAG, "  • Hardware: %s",
           hardware_initialized ? "✓ Initialized" : "✗ Not initialized");
  ESP_LOGI(TAG, "  • FreeRTOS: %s",
           freertos_initialized ? "✓ Initialized" : "✗ Not initialized");
  ESP_LOGI(TAG, "  • System: %s",
           system_initialized ? "✓ Initialized" : "✗ Not initialized");
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Memory Information:");
  ESP_LOGI(TAG, "  • Free Heap: %zu bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "  • Minimum Free: %zu bytes",
           esp_get_minimum_free_heap_size());
  ESP_LOGI(TAG, "  • Total Free: %zu bytes", esp_get_free_heap_size());
  ESP_LOGI(TAG, "========================================");
  ESP_LOGI(TAG, "Task Information:");
  ESP_LOGI(TAG, "  • Active Tasks: %d", uxTaskGetNumberOfTasks());
  ESP_LOGI(TAG, "========================================");
}

// ============================================================================
// HARDWARE ABSTRACTION FUNCTIONS
// ============================================================================

esp_err_t chess_uart_send_string(const char *str) {
  if (str == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // CRITICAL: Use centralized UART output queue
  extern void uart_queue_message(int type, bool add_newline, const char *format,
                                 ...);
  uart_queue_message(0, false, "%s", str); // UART_MSG_NORMAL = 0

  return ESP_OK;
}

esp_err_t chess_uart_printf(const char *format, ...) {
  if (format == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // CRITICAL: Use centralized UART output queue with formatted message
  extern void uart_queue_message(int type, bool add_newline, const char *format,
                                 ...);

  va_list args;
  va_start(args, format);

  char buffer[256];
  int len = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (len > 0 && len < sizeof(buffer)) {
    uart_queue_message(0, false, "%s", buffer); // UART_MSG_NORMAL = 0
    return ESP_OK;
  }

  return ESP_ERR_INVALID_SIZE;
}

esp_err_t chess_led_set_pixel(uint8_t led_index, uint8_t red, uint8_t green,
                              uint8_t blue) {
  if (led_index >= CHESS_LED_COUNT_TOTAL) {
    ESP_LOGE(TAG, "Invalid LED index: %d (max: %d)", led_index,
             CHESS_LED_COUNT_TOTAL - 1);
    return ESP_ERR_INVALID_ARG;
  }

  // In simulation mode, log the LED command
  ESP_LOGI(TAG, "LED Set Pixel: index=%d, RGB=(%d,%d,%d)", led_index, red,
           green, blue);

  // Přímé volání LED funkce
  led_set_pixel_safe(led_index, red, green, blue);
  return ESP_OK;
}

esp_err_t chess_led_set_all(uint8_t red, uint8_t green, uint8_t blue) {
  // In simulation mode, log the LED command
  ESP_LOGI(TAG, "LED Set All: RGB=(%d,%d,%d)", red, green, blue);

  // Přímé volání LED funkce
  led_set_all_safe(red, green, blue);
  return ESP_OK;
}

esp_err_t chess_led_clear(void) {
  // In simulation mode, log the LED command
  ESP_LOGI(TAG, "LED Clear All");

  // Přímé volání LED funkce
  led_clear_all_safe();
  return ESP_OK;
}

esp_err_t chess_led_show_board(void) {
  // In simulation mode, log the LED command
  ESP_LOGI(TAG, "LED Show Chess Board Pattern");

  // Přímé volání LED funkce
  // Show chess board pattern (alternating black/white squares)
  for (int i = 0; i < 64; i++) {
    int row = i / 8;
    int col = i % 8;
    if ((row + col) % 2 == 0) {
      led_set_pixel_safe(i, 255, 255, 255); // White squares
    } else {
      led_set_pixel_safe(i, 0, 0, 0); // Black squares
    }
  }
  return ESP_OK;
}

esp_err_t chess_led_button_feedback(uint8_t button_id, bool available) {
  if (button_id >= CHESS_BUTTON_COUNT) {
    ESP_LOGE(TAG, "Invalid button ID: %d (max: %d)", button_id,
             CHESS_BUTTON_COUNT - 1);
    return ESP_ERR_INVALID_ARG;
  }

  // In simulation mode, log the button feedback
  ESP_LOGI(TAG, "Button LED Feedback: button=%d, available=%s", button_id,
           available ? "true" : "false");

  // Přímé volání LED funkce
  uint8_t led_index =
      button_id + CHESS_LED_COUNT_BOARD; // Button LEDs start at index 64
  if (available) {
    led_set_pixel_safe(led_index, 0, 255, 0); // Green for available
  } else {
    led_set_pixel_safe(led_index, 255, 0, 0); // Red for not available
  }
  return ESP_OK;
}

esp_err_t chess_matrix_scan(void) {
  // In simulation mode, simulate matrix scanning
  ESP_LOGI(TAG, "Matrix Scan: Simulating 8x8 reed switch matrix");

  // Simulate some matrix events
  static uint32_t scan_count = 0;
  scan_count++;

  if (scan_count % 10 == 0) { // Every 10th scan
    // Simulate a piece move
    matrix_event_t event = {.type = MATRIX_EVENT_MOVE_DETECTED,
                            .from_square = (scan_count / 10) % 64,
                            .to_square = ((scan_count / 10) + 1) % 64,
                            .piece_type = (scan_count / 10) % 6 + 1,
                            .timestamp = esp_timer_get_time() / 1000};

    if (matrix_event_queue != NULL) {
      if (xQueueSend(matrix_event_queue, &event, pdMS_TO_TICKS(100)) !=
          pdTRUE) {
        ESP_LOGW(TAG, "Failed to send matrix event to queue");
      }
    }
  }

  return ESP_OK;
}

esp_err_t chess_matrix_reset(void) {
  ESP_LOGI(TAG, "Matrix Reset: Clearing all matrix states");
  return ESP_OK;
}

esp_err_t chess_matrix_get_status(uint8_t *status_array) {
  if (status_array == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // In simulation mode, return simulated matrix status
  for (int i = 0; i < 64; i++) {
    status_array[i] = (i % 8 == 0) ? 1 : 0; // Simulate pieces on A-file
  }

  ESP_LOGI(TAG, "Matrix Status: Retrieved simulated 8x8 matrix state");
  return ESP_OK;
}

esp_err_t chess_button_scan(void) {
  // In simulation mode, simulate button scanning
  ESP_LOGI(TAG, "Button Scan: Simulating 9 button states");

  // Simulate some button events
  static uint32_t scan_count = 0;
  scan_count++;

  if (scan_count % 20 == 0) { // Every 20th scan
    // Simulate a button press
    button_event_t event = {.type = BUTTON_EVENT_PRESS,
                            .button_id = (scan_count / 20) % CHESS_BUTTON_COUNT,
                            .press_duration_ms = 100,
                            .timestamp = esp_timer_get_time() / 1000};

    if (button_event_queue != NULL) {
      if (xQueueSend(button_event_queue, &event, pdMS_TO_TICKS(100)) !=
          pdTRUE) {
        ESP_LOGW(TAG, "Failed to send button event to queue");
      }
    }
  }

  return ESP_OK;
}

esp_err_t chess_button_get_status(uint8_t *button_status) {
  if (button_status == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  // In simulation mode, return simulated button status
  for (int i = 0; i < CHESS_BUTTON_COUNT; i++) {
    button_status[i] = (i % 3 == 0) ? 1 : 0; // Simulate some buttons pressed
  }

  ESP_LOGI(TAG, "Button Status: Retrieved simulated button states");
  return ESP_OK;
}

esp_err_t chess_game_init(void) {
  ESP_LOGI(TAG, "Game Init: Starting new chess game");
  return ESP_OK;
}

esp_err_t chess_game_reset(void) {
  ESP_LOGI(TAG, "Game Reset: Resetting chess game to initial state");
  return ESP_OK;
}

esp_err_t chess_game_get_status(void) {
  ESP_LOGI(TAG, "Game Status: Retrieving current game state");
  return ESP_OK;
}

// ============================================================================
// MEMORY OPTIMIZATION SYSTEMS INITIALIZATION
// ============================================================================

esp_err_t chess_memory_systems_init(void) {
  ESP_LOGI(TAG, "🔄 Initializing memory optimization systems...");

  esp_err_t ret;

  // Initialize shared buffer pool
  ESP_LOGI(TAG, "🔄 Initializing shared buffer pool...");
  ret = buffer_pool_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Shared buffer pool initialization failed: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ Shared buffer pool initialized successfully");

  // Initialize streaming output system
  ESP_LOGI(TAG, "🔄 Initializing streaming output system...");
  ret = streaming_output_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Streaming output initialization failed: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ Streaming output system initialized successfully");

  // Configure streaming output for UART
  ret = streaming_set_uart_output(0); // UART port 0 (USB Serial JTAG)
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "❌ Failed to configure UART streaming: %s",
             esp_err_to_name(ret));
    return ret;
  }
  ESP_LOGI(TAG, "✅ UART streaming configured successfully");

  ESP_LOGI(TAG, "✅ All memory optimization systems initialized successfully");
  return ESP_OK;
}