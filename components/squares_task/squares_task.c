/**
 * @file squares_task.c
 * @brief Matrix Scanning Task - Detection of physical figures on a chest of drawers
 *
 * @details
 * This file has two compile-time inputs selected by CONFIG_CHESS_MATRIX_INPUT:
 * - GPIO reed matrix (default): row/column multiplex scan.
 * - I2C Hall (CONFIG_CHESS_MATRIX_INPUT_I2C_HALL): reads raw ADC pairs from
 *   4x STM32 slaves, runs the ported POC classification (energy + hysteresis +
 *   debounce) and collapses each pair to occupancy 0/1. Stable piece type and
 *   color are stable POC outputs; game_task uses them for move legality when
 *   this input mode is enabled.
 *
 * =============================================================================
 * WHAT DID THIS FILE DO?
 * =============================================================================
 *
 * This task scans the chest and detects the movement of pieces:
 * 1. Multiplexed 8x8 matrix scan (Language contacts under each field)
 * 2. Debouncing (ignores the "shaking" when lifting/placing the figure)
 * 3. UP/DN event detection (piece raised/placed)
 * 4. Send the move to game_task (when it detects a complete move)
 * 5. Error detection (bad sequence of events)
 *
 * =============================================================================
 * HOW DOES IT WORK?
 * =============================================================================
 *
 * MULTIPLEX SCANNING:
 * - Activate row 0 (row select GPIO)
 * - Honor 8 columns (column read GPIO)
 * - Save state -> squares_state[0][0-7]
 * - Deactivate row 0
 * - Repeat for rows 1-7
 * - Total time: ~10ms (full scan)
 *
 * DEBOUNCING:
 * - Each sensor must be stable for 3 scans in a row
 * - Only then do we detect the change (UP/DN)
 * - Ignores "shudder" during manipulation
 *
 * PIECE TRACKING:
 * - State: IDLE -> PIECE_UP -> PIECE_DN -> IDLE
 * - UP on e2 -> we wait for DN somewhere else
 * - DN on e4 -> move e2-e4 detected!
 * - Send to game_command_queue
 *
 * =============================================================================
 * COMMUNICATION (FIFOS)
 * =============================================================================
 *
 * QUEUE - We send detected moves:
 * - game_command_queue -> Send a move of type GAME_CMD_MOVE_DETECTED
 *
 * NO MUTEX - Task is read-only (only read GPIO)
 *
 * =============================================================================
 * HARDWARE (GPIO MAPPING)
 * =============================================================================
 *
 * ROW SELECT (8 pin, active LOW):
 * - GPIO 10-17: Select the box row
 *
 * COLUMN READ (8 pin, pull-up):
 * - GPIO 18-25: Reading language contacts (Reed switches)
 *
 * Language contacts: I detect the magnet in the figurine
 * - LOW = Figure present
 * - HIGH = Empty field
 *
 * =============================================================================
 * CRITIC OF RULES
 * =============================================================================
 *
 * @warning WHAT NOT TO DO:
 *
 * 1. NEVER have debounce delay!
 * Without debounce it will detect "shakes" as tens of events
 *
 * 2. NEVER shorten the scan interval below 10ms!
 * GPIO multiplex needs time to stabilize
 *
 * 3. NEVER send a move directly to game_execute_move!
 * ❌ game_execute_move(&move);  // Access from another task!
 * ✅ xQueueSend(game_command_queue, &cmd, ...);  // Pass the queue
 *
 * 4. ALWAYS check queue overflow!
 * If game_task is busy, the queue may fill up
 *
 * =============================================================================
 * TABLE OF CONTENTS
 * =============================================================================
 *
 * Section 1: GPIO Init ............................ line 80
 * Section 2: Matrix Scanning ...................... line 200
 * Section 3: Debouncing Logic ..................... line 350
 * Section 4: Event Detection ...................... line 500
 * Section 5: Main Matrix Task ..................... line 750
 *
 * =============================================================================
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-12-22
 *
 * @note
 * - Task priority: 6 (higher than game - realtime detection)
 * - Scan interval: 10ms
 * - Debounce: 3 scans (30ms)
 *
 * @see game_task.c - Accepts detked moves
 */

#include "sdkconfig.h"
#include "squares_task.h"
#include "hall_i2c_squares.h"
#include "led_task.h"
#if CONFIG_CHESS_STM32_I2C_BL_ENABLE
#include "stm32_i2c_bl.h"
#endif
#include "game_task.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_restore_guide.h"
#endif
#include "chess_gameplay_policy.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "chess_types.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
// #include "freertos/FreeRTOS.h" // Unused
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos_chess.h"
#include "../led_task/include/led_task.h"
#include "task_wdt.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// External function to report activity (implemented in main.c)
extern void demo_report_activity(void);

static const char *TAG = "SQUARES_TASK";

// ============================================================================
// LOKALNI PROMENNE A KONSTANTY
// ============================================================================

// Matrix status
static uint8_t squares_state[64] = {0};    // The current state of the matrix
static uint8_t squares_previous[64] = {0}; // Antecedent state of the matrix
static uint8_t squares_changes[64] = {0};  // Detekce zmen

// Stav tasku
static bool task_running = false;
static bool simulation_mode = false; // Zmeneno na false pro realny hardware
bool squares_scanning_enabled = true; // Matrix scanning enabled by default
                                     // settings (external for timer callback)

// Stav skenovani
static uint8_t current_row = 0;
static uint32_t last_scan_time = 0;
static uint32_t scan_count = 0;

// Thrust detection
static uint8_t last_piece_lifted = 255; // Rear figure
static uint8_t last_piece_placed = 255; // Rear figure
static uint32_t move_detection_timeout = 0;
static bool squares_guard_mode_active = false;
static uint8_t squares_guard_expected_state[64] = {0};

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
/** Last stable Hall type/color while square occupancy was 1 (for pickup after lift). */
static hall_piece_type_t squares_hall_last_type[64];
static hall_piece_color_t squares_hall_last_color[64];

static void squares_hall_refresh_last_occupied_snapshot(void) {
  for (int i = 0; i < 64; i++) {
    if (squares_state[i] != 0) {
      squares_hall_last_type[i] = hall_i2c_squares_get_piece_type((uint8_t)i);
      squares_hall_last_color[i] = hall_i2c_squares_get_piece_color((uint8_t)i);
    }
  }
}
#endif

// Matrix patterns for simulation
static const uint8_t simulation_patterns[][64] = {
    // Pattern 0: Empty board
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    // Pattern 1: Starting position
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},

    // Pattern 2: Mid-game position
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}};

static uint8_t current_pattern = 1; // Zacit s vzorem 1

// ============================================================================
// MATRIX SCAN FUNCTION
// ============================================================================

/**
 * @brief Scans one row of a matrix (mutexless internal functions)
 *
 * This function scans one row of an 8x8 reed switch matrix. Before scanning
 * sets all row pins to HIGH (inactive), then activates the current row
 * set to LOW, wait 5 ms for signal stabilization, clean column pins
 * and set the line back to HIGH.
 *
 * @param row Row number (0-7)
 *
 * @details
 * Hardware logic:
 * - Row pins are DRIVE (outputs) - you actively set the state
 * - Column pins are SENSE (input with pull-up) - read them
 * - Active row = LOW (pulls the column pin to LOW if there is a piece/reed switch
 * closed)
 * - Inactive row = HIGH (column pin remains HIGH if there is no figure)
 *
 * Before activating the current row, all row pins are set
 * to HIGH. This ensures that the other row pins do not interfere with reading
 * column pin. Without this, the figures would be in one column on different boards
 * could cause false detection (if row pin remains LOW, column pin
 * would be LOW even when we scan other rows).
 *
 * Stabilization wait: After activating the row pin to LOW, it waits 5 ms for correct
 * establishing the voltage state. Without this waiting, something could go wrong
 * reading the status of the column pin due to electromagnetic interference and capacitance
 * effect.
 *
 * @note Callers must hold the squares_mutex before calling this function
 */
static void squares_scan_row_internal(uint8_t row) {
  if (row >= 8)
    return;

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  (void)row;
  return;
#else

  // IMPROVED: Switch all lines to INPUT before activating the current line
  // This is safer than just setting it to HIGH because it prevents conflicts
  // when switching between lines (as in the reference code)
  for (int i = 0; i < 8; i++) {
    gpio_config_t io_conf = {.pin_bit_mask = (1ULL << matrix_row_pins[i]),
                             .mode = GPIO_MODE_INPUT,
                             .pull_up_en = GPIO_PULLUP_DISABLE,
                             .pull_down_en = GPIO_PULLDOWN_DISABLE,
                             .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf);
  }

  // IMPROVED: Use esp_rom_delay_us instead of vTaskDelay (timer callback
  // context)
  extern void esp_rom_delay_us(uint32_t us);
  esp_rom_delay_us(20); // 20us settling time (reduced from 1ms)

  // Set current line to OUTPUT and LOW (activation)
  // This pulls the column pin LOW if there is a piece (reed
  // switch closed)
  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << matrix_row_pins[row]),
                           .mode = GPIO_MODE_OUTPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_DISABLE};
  gpio_config(&io_conf);
  gpio_set_level(matrix_row_pins[row], 0); // LOW = active

  // GPIO settling time: 200us for signal stabilization (long wires support)
  // For digital signals with long cables (capacitance), we need more time
  // for the signal to fall to logic LOW (grounding).
  esp_rom_delay_us(200);

  // Read all column pins for this row
  // Column pins have a pull-up, so they will be HIGH without the figure
  // If the figure is (reed switch closed), the column pin will be LOW
  
  // Read all column pins for this row
  // Column pins have a pull-up, so they will be HIGH without the figure
  // If the figure is (reed switch closed), the column pin will be LOW
  
  for (int col = 0; col < 8; col++) {
    // Invert column index to fix GPIO physical wiring (H→A instead of
    // A→H) GPIO pins are wired in reverse: pin 0 = column H, pin 7 = column A
    // So we invert: col 0 → index 7 (H), col 1 → index 6 (G), ..., col 7 →
    // index 0 (A)
    int inverted_col = 7 - col;
    int index = row * 8 + inverted_col;
    
    // DEBUG: Check if this is GPIO17 (column 7)
    bool is_gpio17 = (matrix_col_pins[col] == GPIO_NUM_17);
    
    int pin_level = gpio_get_level(matrix_col_pins[col]);

    // In simulation mode, use simulated values
    if (simulation_mode) {
      squares_state[index] = simulation_patterns[current_pattern][index];
    } else {
      // Real hardware: reed switch closed = piece present
      // pin_level == 0 means that the column pin is pulled to LOW (the figure is
      // present)
      // NOTE: GPIO17 is configured with pull-down for debug testing
      // Normal logic still applies, but we'll log values for analysis
      squares_state[index] = (pin_level == 0) ? 1 : 0;
      
      // DEBUG: Minimal logging for GPIO17 (user testing with multimeter)
      // Only log first state change to confirm scanning works
      static int last_gpio17_level = -1;
      static bool gpio17_logged_once = false;
      if (is_gpio17 && !gpio17_logged_once) {
        ESP_LOGI(TAG, "🔍 GPIO17 scan confirmed: row=%d, level=%d (pull-down mode)", row, pin_level);
        gpio17_logged_once = true;
        last_gpio17_level = pin_level;
      } else if (is_gpio17 && last_gpio17_level == -1) {
        last_gpio17_level = pin_level;
      }
    }
  }

  // IMPROVED: Switch line back to INPUT after scan (safer)
  io_conf.pin_bit_mask = (1ULL << matrix_row_pins[row]);
  io_conf.mode = GPIO_MODE_INPUT;
  gpio_config(&io_conf);

  // A small wait before the next line for stabilization
  esp_rom_delay_us(20); // 20us delay (reduced from 2ms)
#endif
}

// Public function - scans row WITH mutex protection
void squares_scan_row(uint8_t row) {
  if (row >= 8)
    return;

  // CRITICAL: Protect matrix state with mutex
  if (squares_mutex != NULL) {
    if (xSemaphoreTake(squares_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      squares_scan_row_internal(row);
      xSemaphoreGive(squares_mutex);
    } else {
      ESP_LOGW(TAG, "Failed to acquire matrix mutex for scan row");
    }
  } else {
    // Fallback if mutex not available
    squares_scan_row_internal(row);
  }
}

/*
 * Kconfig CHESS_MATRIX_INPUT:
 * - GPIO_REED: squares_scan_row_internal(0..7) loop below — row multiplex.
 * - I2C_HALL: hall_i2c_squares_fill_state() (read from STM slave addresses).
 * STM32 flash over I2C when CHESS_STM32_I2C_BL_ENABLE is on: bootloader on its own
 * suspend scan (squares_scanning_enabled in stm32_i2c_bl.c) until flash is done.
 */
static bool s_squares_events_armed;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
static bool s_hall_was_usable;
#endif

/** Moves are accepted only after boot, and after Hall has locked a real baseline once. */
static bool squares_sensors_accept_events(void) {
  if (led_is_booting()) {
    return false;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (simulation_mode) {
    return true;
  }
  if (hall_i2c_squares_readings_usable()) {
    s_hall_was_usable = true;
  }
  return s_hall_was_usable;
#else
  return true;
#endif
}

void squares_scan_all(void) {
  uint32_t current_time = esp_timer_get_time() / 1000;

  // NOTE: Don't call esp_task_wdt_reset() here because this function
  // is called from timer callbacks which run in timer service task
  // that is not registered with TWDT

  // CRITICAL: Protect matrix state with mutex for change detection
  if (squares_mutex != NULL) {
    if (xSemaphoreTake(squares_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (simulation_mode) {
        memcpy(squares_state, simulation_patterns[current_pattern],
               sizeof(squares_state));
      } else {
        hall_i2c_squares_fill_state(squares_state);
      }
      if (!simulation_mode) {
        squares_hall_refresh_last_occupied_snapshot();
      }
#else
      for (int row = 0; row < 8; row++) {
        squares_scan_row_internal(row);
      }
#endif

      /* First stable frame after boot/baseline is the reference, not 64 drops. */
      bool accept_events = squares_sensors_accept_events();
      bool detect_events = accept_events && s_squares_events_armed;
      if (detect_events) {
        for (int i = 0; i < 64; i++) {
          if (squares_state[i] != squares_previous[i]) {
            squares_changes[i] = 1;
          } else {
            squares_changes[i] = 0;
          }
          if (squares_changes[i]) {
            demo_report_activity();
          }
        }
        squares_detect_moves();
      }
      memcpy(squares_previous, squares_state, sizeof(squares_state));
      s_squares_events_armed = accept_events;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (!simulation_mode && accept_events) {
        game_hall_restore_guide_note_stable_squares(squares_state);
      }
#endif

      last_scan_time = current_time;
      scan_count++;

      // NOTE: Don't call WDT reset in timer callback context

      xSemaphoreGive(squares_mutex);
    } else {
      ESP_LOGW(TAG, "Failed to acquire matrix mutex for scan all");
      return;
    }
  } else {
    // Fallback if mutex not available
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    if (simulation_mode) {
      memcpy(squares_state, simulation_patterns[current_pattern],
             sizeof(squares_state));
    } else {
      hall_i2c_squares_fill_state(squares_state);
    }
    if (!simulation_mode) {
      squares_hall_refresh_last_occupied_snapshot();
    }
#else
    for (int row = 0; row < 8; row++) {
      squares_scan_row_internal(row);
    }
#endif

    bool accept_events = squares_sensors_accept_events();
    bool detect_events = accept_events && s_squares_events_armed;
    if (detect_events) {
      for (int i = 0; i < 64; i++) {
        if (squares_state[i] != squares_previous[i]) {
          squares_changes[i] = 1;
        } else {
          squares_changes[i] = 0;
        }
        if (squares_changes[i]) {
          demo_report_activity();
        }
      }
      squares_detect_moves();
    }
    memcpy(squares_previous, squares_state, sizeof(squares_state));
    s_squares_events_armed = accept_events;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    if (!simulation_mode && accept_events) {
      game_hall_restore_guide_note_stable_squares(squares_state);
    }
#endif
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

/**
 * @brief Helper function to send PICKUP command to game_command_queue
 * @param square Square index (0-63)
 */
static void squares_send_pickup_command(uint8_t square) {
  extern QueueHandle_t game_command_queue;

  if (game_command_queue == NULL) {
    ESP_LOGW(TAG,
             "game_command_queue not available - cannot send PICKUP command");
    return;
  }

  char notation[4];
  squares_square_to_notation(square, notation);

  chess_move_command_t cmd = {
      .type = GAME_CMD_PICKUP,
      .player = 0,           // Will be determined by game task
      .response_queue = NULL // Matrix events don't have response queue
  };
  strncpy(cmd.from_notation, notation, sizeof(cmd.from_notation) - 1);
  cmd.from_notation[sizeof(cmd.from_notation) - 1] = '\0';
  strcpy(cmd.to_notation, "");

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "PICKUP command sent to game_command_queue: %s", notation);
    // Report activity to HA light task
    ha_light_report_activity("pickup");
  } else {
    ESP_LOGW(TAG, "Failed to send PICKUP command to game_command_queue");
  }
}

/**
 * @brief Helper function to send DROP command to game_command_queue
 * @param square Square index (0-63)
 */
/*
static void squares_send_drop_command(uint8_t square) {
  extern QueueHandle_t game_command_queue;

  if (game_command_queue == NULL) {
    ESP_LOGW(TAG,
             "game_command_queue not available - cannot send DROP command");
    return;
  }

  char notation[4];
  squares_square_to_notation(square, notation);

  chess_move_command_t cmd = {
      .type = GAME_CMD_DROP,
      .player = 0,           // Will be determined by game task
      .response_queue = NULL // Matrix events don't have response queue
  };
  strncpy(cmd.to_notation, notation, sizeof(cmd.to_notation) - 1);
  cmd.to_notation[sizeof(cmd.to_notation) - 1] = '\0';
  strcpy(cmd.from_notation, "");

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "DROP command sent to game_command_queue: %s", notation);
    // Report activity to HA light task
    ha_light_report_activity("drop");
  } else {
    ESP_LOGW(TAG, "Failed to send DROP command to game_command_queue");
  }
}
*/

/**
 * @brief Helper function to send DROP command with from/to notation to
 * game_command_queue
 * @param from_square Source square index (0-63)
 * @param to_square Destination square index (0-63)
 */
static void squares_send_drop_command_with_from(uint8_t from_square,
                                               uint8_t to_square) {
  extern QueueHandle_t game_command_queue;

  if (game_command_queue == NULL) {
    ESP_LOGW(TAG, "game_command_queue not available - cannot send DROP command "
                  "with from");
    return;
  }

  char from_notation[4], to_notation[4];
  squares_square_to_notation(from_square, from_notation);
  squares_square_to_notation(to_square, to_notation);

  chess_move_command_t cmd = {
      .type = GAME_CMD_DROP,
      .player = 0,           // Will be determined by game task
      .response_queue = NULL // Matrix events don't have response queue
  };
  strncpy(cmd.from_notation, from_notation, sizeof(cmd.from_notation) - 1);
  cmd.from_notation[sizeof(cmd.from_notation) - 1] = '\0';
  strncpy(cmd.to_notation, to_notation, sizeof(cmd.to_notation) - 1);
  cmd.to_notation[sizeof(cmd.to_notation) - 1] = '\0';

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "DROP command with from sent to game_command_queue: %s -> %s",
             from_notation, to_notation);
    // Report activity to HA light task
    ha_light_report_activity("drop");
  } else {
    ESP_LOGW(TAG,
             "Failed to send DROP command with from to game_command_queue");
  }
}

static void squares_send_guard_command(uint32_t lifted_mask_low,
                                      uint32_t lifted_mask_high,
                                      uint32_t dropped_mask_low,
                                      uint32_t dropped_mask_high,
                                      uint8_t action) {
  extern QueueHandle_t game_command_queue;

  if (action != 0 && !chess_policy_squares_guard_enabled()) {
    ESP_LOGD(TAG, "Matrix guard detection disabled — skipping activate");
    return;
  }

  if (game_command_queue == NULL) {
    ESP_LOGW(TAG, "game_command_queue not available - cannot send GUARD command");
    return;
  }

  chess_move_command_t cmd = {
      .type = GAME_CMD_SQUARES_GUARD,
      .player = 0,
      .response_queue = NULL,
  };
  cmd.timer_data.squares_guard.lifted_mask_low = lifted_mask_low;
  cmd.timer_data.squares_guard.lifted_mask_high = lifted_mask_high;
  cmd.timer_data.squares_guard.dropped_mask_low = dropped_mask_low;
  cmd.timer_data.squares_guard.dropped_mask_high = dropped_mask_high;
  cmd.timer_data.squares_guard.action = action;
  cmd.from_notation[0] = '\0';
  cmd.to_notation[0] = '\0';

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGW(TAG,
             "MATRIX GUARD command sent: action=%u lifted=%08" PRIx32
             ":%08" PRIx32 " dropped=%08" PRIx32 ":%08" PRIx32,
             (unsigned int)action, lifted_mask_high, lifted_mask_low,
             dropped_mask_high, dropped_mask_low);
  } else {
    ESP_LOGW(TAG, "Failed to send MATRIX GUARD command");
  }
}

void squares_detect_moves(void) {
  // Check for move timeout - change from RESET to WARNING + EXTEND
  if (last_piece_lifted != 255) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time > move_detection_timeout) {
      ESP_LOGW(TAG,
               "⏰ Move taking longer than 5s - piece from %d still in air",
               last_piece_lifted);
      // DON'T reset last_piece_lifted - allow longer moves
      // Just extend timeout to give player more time
      move_detection_timeout = current_time + 5000; // Give another 5 seconds
    }
  }

  // Collect full delta in this scan to detect ambiguous states.
  uint8_t piece_lifted = 255;
  uint8_t piece_placed = 255;
  uint8_t lift_count = 0;
  uint8_t drop_count = 0;
  uint32_t lifted_mask_low = 0;
  uint32_t lifted_mask_high = 0;
  uint32_t dropped_mask_low = 0;
  uint32_t dropped_mask_high = 0;
  for (int i = 0; i < 64; i++) {
    if (squares_previous[i] == 1 && squares_state[i] == 0) {
      lift_count++;
      if (piece_lifted == 255) {
        piece_lifted = i;
      }
      if (i < 32) {
        lifted_mask_low |= (1UL << i);
      } else {
        lifted_mask_high |= (1UL << (i - 32));
      }
    } else if (squares_previous[i] == 0 && squares_state[i] == 1) {
      drop_count++;
      if (piece_placed == 255) {
        piece_placed = i;
      }
      if (i < 32) {
        dropped_mask_low |= (1UL << i);
      } else {
        dropped_mask_high |= (1UL << (i - 32));
      }
    }
  }

  // Two UP in a row: normally ambiguous (matrix guard), but guided capture
  // (victim first, then attacker) and 3-step defense (own → opponent) require it.
  bool multi_change = (lift_count > 1) || (drop_count > 1);
  bool pending_lift_and_new_up =
      (last_piece_lifted != 255 && lift_count > 0);
  bool ambiguous_state = multi_change;
  if (!ambiguous_state && pending_lift_and_new_up) {
    if (!(lift_count == 1 && game_squares_allow_second_sequential_lift())) {
      ambiguous_state = true;
    }
  }

  if (ambiguous_state) {
    if (!squares_guard_mode_active) {
      if (!chess_policy_squares_guard_enabled()) {
        ESP_LOGW(TAG,
                 "Ambiguous matrix state ignored (matrix guard disabled)");
        return;
      }
      // Snapshot expected board occupancy before anomaly; game_task realigns to
      // board[] when handling GAME_CMD_SQUARES_GUARD.
      memcpy(squares_guard_expected_state, squares_previous,
             sizeof(squares_guard_expected_state));
      squares_guard_mode_active = true;
      last_piece_lifted = 255;
      last_piece_placed = 255;
      move_detection_timeout = 0;
      squares_send_guard_command(lifted_mask_low, lifted_mask_high, dropped_mask_low,
                                dropped_mask_high, 1);
    }
    return;
  }

  if (squares_guard_mode_active) {
    // Keep matrix flow paused until board occupancy is back to expected state.
    bool back_to_expected =
        (memcmp(squares_state, squares_guard_expected_state,
                sizeof(squares_guard_expected_state)) == 0);
    if (lift_count == 0 && drop_count == 0 && back_to_expected) {
      squares_guard_mode_active = false;
      squares_send_guard_command(0, 0, 0, 0, 0);
    }
    return;
  }

  // Process move detection
  if (piece_lifted != 255) {
    last_piece_lifted = piece_lifted;
    move_detection_timeout =
        esp_timer_get_time() / 1000 + 5000; // 5 second timeout

    // Convert square index to row/col
    uint8_t from_row = piece_lifted / 8;
    uint8_t from_col = piece_lifted % 8;

    ESP_LOGI(TAG, "Piece lifted from square %d (%c%d)", piece_lifted,
             'a' + from_col, from_row + 1);

    // UNIFIED FLOW: Send PICKUP command to game_command_queue (same as UART)
    squares_send_pickup_command(piece_lifted);
  }

  if (piece_placed != 255) {
    last_piece_placed = piece_placed;

    // Convert square index to row/col
    uint8_t to_row = piece_placed / 8;
    uint8_t to_col = piece_placed % 8;

    ESP_LOGI(TAG, "Piece placed on square %d (%c%d)", piece_placed,
             'a' + to_col, to_row + 1);

    // Check if this completes a move
    if (last_piece_lifted != 255) {
      if (last_piece_lifted == piece_placed) {
        // Return to same square - send with from=to
        ESP_LOGI(TAG,
                 "Piece returned to same square - sending DROP with from=to");
        squares_send_drop_command_with_from(last_piece_lifted, piece_placed);
      } else {
        // Complete move - send with from/to
        squares_send_drop_command_with_from(last_piece_lifted, piece_placed);
      }
      last_piece_lifted = 255; // Reset
    } else {
      // Piece placed without lift - IGNORE
      ESP_LOGW(TAG, "Piece placed without previous lift (square %d) - ignoring",
               piece_placed);
    }
  }
}

void squares_detect_complete_move(uint8_t from_square, uint8_t to_square) {
  // Convert square indices to chess notation
  char from_notation[4], to_notation[4];
  squares_square_to_notation(from_square, from_notation);
  squares_square_to_notation(to_square, to_notation);

  ESP_LOGI(TAG, "Complete move detected: %d -> %d (%s -> %s)", from_square,
           to_square, from_notation, to_notation);

  // UNIFIED FLOW: Send complete move as DROP with from/to to
  // game_command_queue (same as UART)
  squares_send_drop_command_with_from(from_square, to_square);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void squares_square_to_notation(uint8_t square, char *notation) {
  if (square >= 64 || notation == NULL)
    return;

  int row = square / 8;
  int col = square % 8;

  notation[0] = 'a' + col;
  notation[1] = '1' + row;
  notation[2] = '\0';
}

uint8_t squares_notation_to_square(const char *notation) {
  if (notation == NULL || strlen(notation) != 2)
    return 255;

  char file = tolower(notation[0]);
  char rank = notation[1];

  if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
    return 255;
  }

  int col = file - 'a';
  int row = rank - '1';

  return row * 8 + col;
}

void squares_get_state(uint8_t *state_buffer) {
  if (state_buffer == NULL)
    return;

  // Copy current matrix state to buffer
  memcpy(state_buffer, squares_state, 64);
}

void squares_print_state(void) {
  ESP_LOGI(TAG, "Matrix State (8x8):");

  for (int row = 7; row >= 0; row--) {
    char line[32] = "";
    char *ptr = line;

    ptr += sprintf(ptr, "%d ", row + 1);

    for (int col = 0; col < 8; col++) {
      int index = row * 8 + col;
      if (squares_state[index]) {
        ptr += sprintf(ptr, "[P] ");
      } else {
        ptr += sprintf(ptr, "[ ] ");
      }
    }

    ESP_LOGI(TAG, "%s", line);
  }

  ESP_LOGI(TAG, "   a   b   c   d   e   f   g   h");
}

void squares_test_scanning(void) {
  ESP_LOGI(TAG, "🔍 Testing matrix scanning functionality...");

  // Test 1: Reset matrix state
  ESP_LOGI(TAG, "Test 1: Resetting matrix state");
  squares_reset();
  squares_print_state();

  // Test 2: Simulate piece placement
  ESP_LOGI(TAG, "Test 2: Simulating piece placement");
  // Place pieces directly on squares
  squares_state[1 * 8 + 4] = 1; // e2
  squares_state[3 * 8 + 4] = 1; // e4
  squares_state[6 * 8 + 3] = 1; // d7
  squares_state[4 * 8 + 3] = 1; // d5
  squares_print_state();

  // Test 3: Simulate piece movement
  ESP_LOGI(TAG, "Test 3: Simulating piece movement");
  squares_simulate_move("e2", "e4"); // Move from e2 to e4
  squares_simulate_move("d7", "d5"); // Move from d7 to d5
  squares_print_state();

  // Test 4: Simulate piece removal
  ESP_LOGI(TAG, "Test 4: Simulating piece removal");
  // Remove pieces directly from squares
  squares_state[3 * 8 + 4] = 0; // e4
  squares_state[4 * 8 + 3] = 0; // d5
  squares_print_state();

  // Test 5: Test all squares
  ESP_LOGI(TAG, "Test 5: Testing all squares");
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      // Place piece directly on square
      squares_state[row * 8 + col] = 1;
      vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
    }
  }
  squares_print_state();

  // Reset after test
  squares_reset();
  ESP_LOGI(TAG, "✅ Matrix test completed successfully");
}

void squares_simulate_move(const char *from, const char *to) {
  uint8_t from_square = squares_notation_to_square(from);
  uint8_t to_square = squares_notation_to_square(to);

  if (from_square == 255 || to_square == 255) {
    ESP_LOGE(TAG, "Invalid chess notation: %s -> %s", from, to);
    return;
  }

  ESP_LOGI(TAG, "Simulating move: %s (%d) -> %s (%d)", from, from_square, to,
           to_square);

  // Update matrix state
  squares_state[from_square] = 0; // Remove piece from source
  squares_state[to_square] = 1;   // Place piece at destination

  // Force change detection
  squares_changes[from_square] = 1;
  squares_changes[to_square] = 1;

  ESP_LOGI(TAG, "Move simulation completed");
}

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

void squares_process_commands(void) {
  uint8_t command;

  // Process commands from queue
  if (squares_command_queue != NULL) {
    while (xQueueReceive(squares_command_queue, &command, 0) == pdTRUE) {
      switch (command) {
      case 0: // Reset matrix
        squares_reset();
        break;

      case 1: // Print state
        squares_print_state();
        break;

      case 2: // SQUARES_CMD_TEST - Test matrix scanning
        ESP_LOGI(TAG, "=== Matrix Test Started ===");
        squares_test_scanning();
        ESP_LOGI(TAG, "=== Matrix Test Complete ===");
        break;

      case 3: // Change simulation pattern
        current_pattern = (current_pattern + 1) % 3;
        ESP_LOGI(TAG, "Simulation pattern changed to %d", current_pattern);
        break;

      case 4: // SQUARES_CMD_DISABLE - Disable matrix scanning
        squares_scanning_enabled = false;
        ESP_LOGI(TAG, "Matrix scanning DISABLED");
        break;

      case 5: // SQUARES_CMD_ENABLE - Enable matrix scanning
        squares_scanning_enabled = true;
        ESP_LOGI(TAG, "Matrix scanning ENABLED");
        break;

      default:
        ESP_LOGW(TAG, "Unknown matrix command: %d", command);
        break;
      }
    }
  }
}

void squares_reset(void) {
  ESP_LOGI(TAG, "Resetting matrix state");

  // Clear all states
  memset(squares_state, 0, sizeof(squares_state));
  memset(squares_previous, 0, sizeof(squares_previous));
  memset(squares_changes, 0, sizeof(squares_changes));

  // Reset move detection
  last_piece_lifted = 255;
  last_piece_placed = 255;
  move_detection_timeout = 0;
  squares_guard_mode_active = false;
  memset(squares_guard_expected_state, 0, sizeof(squares_guard_expected_state));

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  memset(squares_hall_last_type, 0, sizeof(squares_hall_last_type));
  memset(squares_hall_last_color, 0, sizeof(squares_hall_last_color));
#endif

  // Reset scanning
  current_row = 0;
  scan_count = 0;

  ESP_LOGI(TAG, "Matrix reset completed");
}

void squares_guard_apply_expected_occupancy(const uint8_t expected[64]) {
  if (expected == NULL) {
    return;
  }

  bool gave = false;
  if (squares_mutex != NULL) {
    if (xSemaphoreTake(squares_mutex, pdMS_TO_TICKS(300)) == pdTRUE) {
      gave = true;
    } else {
      ESP_LOGW(TAG,
               "matrix_guard_apply_expected: mutex timeout — applying without lock");
    }
  }

  memcpy(squares_guard_expected_state, expected, sizeof(squares_guard_expected_state));
  squares_guard_mode_active = true;
  last_piece_lifted = 255;
  last_piece_placed = 255;
  move_detection_timeout = 0;

  if (gave) {
    xSemaphoreGive(squares_mutex);
  }

  ESP_LOGI(TAG, "matrix_guard_apply_expected: recovery target synced to logic");
}

bool squares_is_guard_mode_active(void) { return squares_guard_mode_active; }

uint8_t squares_get_pending_lift_square(void) { return last_piece_lifted; }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
hall_piece_type_t squares_get_hall_piece_type(uint8_t square) {
  return hall_i2c_squares_get_piece_type(square);
}

hall_piece_color_t squares_get_hall_piece_color(uint8_t square) {
  return hall_i2c_squares_get_piece_color(square);
}

const char* squares_get_hall_piece_type_name(hall_piece_type_t type) {
  return hall_i2c_squares_piece_type_name(type);
}

const char* squares_get_hall_piece_color_name(hall_piece_color_t color) {
  return hall_i2c_squares_piece_color_name(color);
}

hall_piece_type_t squares_get_hall_last_occupied_type(uint8_t square) {
  if (square >= 64) {
    return HALL_PIECE_TYPE_NONE;
  }
  return squares_hall_last_type[square];
}

hall_piece_color_t squares_get_hall_last_occupied_color(uint8_t square) {
  if (square >= 64) {
    return HALL_PIECE_COLOR_NONE;
  }
  return squares_hall_last_color[square];
}
#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

void squares_abort_ambiguous_guard_baseline(void) {
  bool gave = false;
  if (squares_mutex != NULL) {
    if (xSemaphoreTake(squares_mutex, pdMS_TO_TICKS(300)) == pdTRUE) {
      gave = true;
    } else {
      ESP_LOGW(TAG,
               "matrix_abort_ambiguous: mutex timeout — partial clear (no memcpy)");
    }
  }

  squares_guard_mode_active = false;
  memset(squares_guard_expected_state, 0, sizeof(squares_guard_expected_state));
  last_piece_lifted = 255;
  last_piece_placed = 255;
  move_detection_timeout = 0;

  if (gave) {
    memcpy(squares_previous, squares_state, sizeof(squares_state));
    xSemaphoreGive(squares_mutex);
  }

  ESP_LOGI(TAG,
           "matrix_abort_ambiguous: local guard off, lift tracking cleared%s",
           gave ? ", baseline synced" : "");
}

// ============================================================================
// TIME-MULTIPLEXING FUNCTIONS
// ============================================================================

/**
 * @brief Release matrix row pins for button scanning
 *
 * Set all row pins to HIGH (inactive state) to button task
 * could clean column pins without interference.
 *
 * @details
 * This function is called before the button scan window (20-25ms).
 * All row pins are set to HIGH (pulled up) so
 * column pins can be safely read for button detection.
 */
void squares_release_pins(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  (void)0;
#else
  for (int row = 0; row < 8; row++) {
    gpio_set_level(matrix_row_pins[row], 1);
  }
#endif
  ESP_LOGD(TAG, "Matrix pins released for button scan");
}

/**
 * @brief Re-enable matrix row pins for matrix scanning
 *
 * Restore normal matrix scanning mode after button scan window.
 *
 * @details
 * This function is called after button scan window. The Matrix can continue
 * in normal scanning.
 */
void squares_acquire_pins(void) {
  // Matrix scanning can resume normally
  // Row pins will be set LOW/HIGH during normal scanning cycle
  ESP_LOGD(TAG, "Matrix pins acquired for matrix scan");

  // No explicit action needed - squares_scan_all() will handle pins
}

/**
 * @brief Verify if the matrix pins are released
 *
 * @return true if all row pins are HIGH (released)
 */
bool squares_pins_released(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  return true;
#else
  for (int row = 0; row < 8; row++) {
    if (gpio_get_level(matrix_row_pins[row]) == 0) {
      return false;
    }
  }
  return true;
#endif
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void squares_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "Matrix task started successfully");

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  esp_err_t hi = hall_i2c_squares_init();
  if (hi != ESP_OK) {
    ESP_LOGE(TAG, "Hall I2C init failed: %s — matrix reads will be empty",
             esp_err_to_name(hi));
  }
#endif

#if CONFIG_CHESS_STM32_I2C_BL_ENABLE
  ESP_LOGI(TAG,
           "STM32 I2C bootloader: after I2C init I start the possible auto-flash "
           "(logy STM32_I2C_BL / STM32_AUTO)");
  stm32_i2c_bl_maybe_auto_flash_on_boot();
#endif

  // CRITICAL: Register with TWDT from within task
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGE(TAG, "Failed to register Matrix task with TWDT: %s",
             esp_err_to_name(wdt_ret));
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
  squares_reset();

  // Set initial pattern
  current_pattern = 1;

  // Main task loop
  uint32_t loop_count = 0;
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    // CRITICAL: Reset watchdog for matrix task in every iteration (only if
    // registered)
    esp_err_t wdt_ret = chess_task_wdt_reset_safe(TAG);
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Watchdog logging every 500 iterations (5 seconds)
    if (loop_count % 500 == 0) {
      ESP_LOGI(TAG, "Matrix Task Watchdog: loop=%" PRIu32 ", heap=%" PRIu32,
               loop_count, esp_get_free_heap_size());
    }

#if HALL_DEBUG_LIGHT_WHERE_PIECE
    hall_i2c_squares_debug_light_pieces();
#else
    static bool corner_probe_done;
    if (!corner_probe_done && !led_is_booting()) {
      corner_probe_done = true;
      hall_i2c_squares_corner_probe();
    }
#endif

    // Process matrix commands
    squares_process_commands();

    // Matrix scanning is now handled by FreeRTOS timer callback
    // No need to call squares_scan_all() here to avoid race conditions

    // Periodic status update - reduced frequency for cleaner UART
    if (loop_count % 50000 == 0) { // Every 50000 loops (1000 seconds)
      ESP_LOGI(TAG, "Matrix Task Status: loop=%" PRIu32 ", scans=%" PRIu32
               ", pattern=%d",
               loop_count, scan_count, current_pattern);

      // Print matrix state occasionally
      if (loop_count % 100000 == 0) {
        squares_print_state();
      }
    }

    loop_count++;

    // Wait for next cycle (10ms)
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(10));
  }
}
