/**
 * @file button_task.h
 * @brief ESP32-C6 Chess System v2.4 - Button Task Header
 * 
 * This header defines the interface for the button task:
 * - Button event types and structures
 * - Button task function prototypes
 * - Button control and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// CONSTANTS AND DEFINITIONS
// ============================================================================





// Button command types
typedef enum {
    BUTTON_CMD_RESET = 0,          // Reset all buttons
    BUTTON_CMD_STATUS,              // Print button status
    BUTTON_CMD_TEST                 // Test all buttons
} button_command_t;


// ============================================================================
// TASK FUNCTION PROTOTYPES
// ============================================================================


/**
 * @brief Start the button task
 * @param pvParameters Task parameters (unused)
 */
void button_task_start(void *pvParameters);


// ============================================================================
// BUTTON SCANNING FUNCTIONS
// ============================================================================


/**
 * @brief Scan all buttons for state changes
 */
void button_scan_all(void);

/**
 * @brief Simulate button press (for testing)
 * @param button_id Button ID to simulate
 */
void button_simulate_press(uint8_t button_id);

/**
 * @brief Simulate button release (for testing)
 * @param button_id Button ID to simulate
 */
void button_simulate_release(uint8_t button_id);


// ============================================================================
// BUTTON EVENT PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process button events and state changes
 */
void button_process_events(void);

/**
 * @brief Handle button press event
 * @param button_id Button ID that was pressed
 */
void button_handle_press(uint8_t button_id);

/**
 * @brief Handle button release event
 * @param button_id Button ID that was released
 */
void button_handle_release(uint8_t button_id);

/**
 * @brief Check for double press on button
 * @param button_id Button ID to check
 */
void button_check_double_press(uint8_t button_id);

/**
 * @brief Send button event to queue
 * @param button_id Button ID
 * @param event_type Type of event
 * @param duration Press duration in milliseconds
 */
void button_send_event(uint8_t button_id, button_event_type_t event_type, uint32_t duration);


// ============================================================================
// BUTTON LED FEEDBACK FUNCTIONS
// ============================================================================


/**
 * @brief Update LED feedback for button state
 * @param button_id Button ID
 * @param pressed Whether button is pressed
 */
void button_update_led_feedback(uint8_t button_id, bool pressed);

/**
 * @brief Set LED color for button
 * @param led_index LED index
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 */
void button_set_led_color(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process button commands from queue
 */
void button_process_commands(void);

/**
 * @brief Reset all button states
 */
void button_reset_all(void);

/**
 * @brief Print button status
 */
void button_print_status(void);

/**
 * @brief Test all buttons
 */
void button_test_all(void);


#endif // BUTTON_TASK_H
