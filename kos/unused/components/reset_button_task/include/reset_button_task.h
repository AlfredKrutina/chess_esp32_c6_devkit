/**
 * @file reset_button_task.h
 * @brief Reset Button Task header for ESP32-C6 sach project
 *
 * This header defines the interface for the reset button task:
 * - Initialization of reset button task and FreeRTOS components
 * - Processing of the reset button to restart the game
 * - Simulation mode without hardware (for development)
 * - Integration with game task for reset
 * - One button to reset the entire game
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-16
 * 
 * @details
 * This task processes the reset button to restart the game. When the player
 * press the reset button (GPIO15), the game restarts to the default state.
 * Task detects the press and sends a command to the game task.
 */
#ifndef RESET_BUTTON_TASK_H
#define RESET_BUTTON_TASK_H

// ============================================================================
// KONSTANTY A TYPY
// ============================================================================



#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Initializes the reset button task
 * 
 * Create a FreeRTOS task to process the reset button.
 * 
 * @return ESP_OK on success, ESP_ERR_NO_MEM on lack of memory
 */
esp_err_t reset_button_task_init(void);

/**
 * @brief The main function of the reset button task
 * 
 * Runs in an infinite loop and processes the reset button.
 * Only logs events in simulation mode.
 * 
 * @param pvParameters Task parameters (not used)
 */
void reset_button_task(void *pvParameters);

/**
 * @brief Process the reset request
 * 
 * Processes the game's request to reset the game (press the reset button).
 * 
 * @param reset_request Is the reset request active?
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if the task is not initialized
 */
esp_err_t process_reset_request(bool reset_request);

/**
 * @brief Simulate pressing/releasing the reset button
 * 
 * For testing - simulates pressing or releasing the reset button.
 * 
 * @param pressed Is the button pressed? (true = pressed, false = released)
 * @return ESP_OK on success
 */
esp_err_t simulate_reset_button_press(bool pressed);

/**
 * @brief Verify that the reset button task is initialized
 * 
 * @return true if the task is initialized
 */
bool reset_button_is_initialized(void);

/**
 * @brief Get the number of button events processed
 * 
 * @return Number of processed events since start
 */
uint32_t reset_button_get_event_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RESET_BUTTON_TASK_H */
