/**
 * @file button_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - Button Task Header
 * 
 * This header defines the interface for the button task:
 * - Button event types and structures
 * - Prototypes button task function
 * - Button control and status functions
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 * 
 * @details
 * Button task processes all buttons in the sach system:
 * - 8 promotion buttons (4 for white + 4 for black)
 * - 1 reset button
 * - Debouncing and event detection
 * - LED feedback for button status
 * - Long press and double press detection
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
// KONSTANTY A DEFINICE
// ============================================================================




/** @brief Button command types */
typedef enum {
    BUTTON_CMD_RESET = 0,   ///< Reset all buttons
    BUTTON_CMD_STATUS,      ///< Print button status
    BUTTON_CMD_TEST         ///< Test all buttons
} button_command_t;


// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================


/**
 * @brief Start the button task
 * 
 * The main function of the button pocket. It runs in an infinite loop and processes
 * button every 5ms. Performs debouncing and generates events.
 * 
 * @param pvParameters Task parameters (not used)
 */
void button_task_start(void *pvParameters);


// ============================================================================
// BUTTON SCAN FUNCTION
// ============================================================================


/**
 * @brief Scans all state change buttons
 * 
 * Reading the state of all 9 buttons and updating the internal state.
 * In the simulation mode, it simulates the successive pressing of the buttons.
 */
void button_scan_all(void);

/**
 * @brief Simulate button presses (for testing)
 * 
 * @param button_id ID of the button to simulate (0-8)
 */
void button_simulate_press(uint8_t button_id);

/**
 * @brief Simulate button release (for testing)
 * 
 * @param button_id ID of the button to simulate (0-8)
 */
void button_simulate_release(uint8_t button_id);


// ============================================================================
// FUNCTION FOR HANDLING BUTTON EVENT
// ============================================================================


/**
 * @brief Handle button events and state changes
 * 
 * Detects button state changes and generates events (press, release,
 * long press, double press).
 */
void button_process_events(void);

/**
 * @brief Handle the button press event
 * 
 * @param button_id ID of the button that was pressed (0-8)
 */
void button_handle_press(uint8_t button_id);

/**
 * @brief Handle the button release event
 * 
 * @param button_id ID of the button that was released (0-8)
 */
void button_handle_release(uint8_t button_id);

/**
 * @brief Verify double press on button
 * 
 * Checks if the button has been pressed twice in a row
 * in an interval of 300ms.
 * 
 * @param button_id Button ID to validate (0-8)
 */
void button_check_double_press(uint8_t button_id);

/**
 * @brief Send the button event to the queue
 * 
 * @param button_id Button ID (0-8)
 * @param event_type Event type
 * @param duration Press time in milliseconds
 */
void button_send_event(uint8_t button_id, button_event_type_t event_type, uint32_t duration);


// ============================================================================
// FUNCTION FOR LED FEEDBACK BUTTONS
// ============================================================================


/**
 * @brief Update LED feedback for button status
 * 
 * Set the LED color of the button according to its state (pressed/released).
 * Pressed button glows pulsating red, soft green when released.
 * 
 * @param button_id Button ID (0-8)
 * @param pressed Is the button pressed?
 */
void button_update_led_feedback(uint8_t button_id, bool pressed);

/**
 * @brief Set the LED color for the button
 * 
 * @param led_index LED index (64-72 for buttons)
 * @param red Red component (0-255)
 * @param green Green component (0-255)
 * @param blue Blue component (0-255)
 */
void button_set_led_color(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process button commands from the queue
 * 
 * Reads commands from button_command_queue and executes them (reset, status, test).
 */
void button_process_commands(void);

/**
 * @brief Reset all button states
 * 
 * Clears all internal button states and resets the simulation mode.
 */
void button_reset_all(void);

/**
 * @brief List the status of the buttons
 * 
 * Lists the status of all 9 buttons and information about the simulation mode.
 */
void button_print_status(void);

/**
 * @brief Test all buttons
 * 
 * It successively simulates pressing and releasing all 9 buttons
 * for functionality testing.
 */
void button_test_all(void);


#endif // BUTTON_TASK_H
