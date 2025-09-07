/**
 * @file   reset_button_task.h
 * @brief  Reset button task header for ESP32-C6 chess project
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date   2025-08-16
 */
#ifndef RESET_BUTTON_TASK_H
#define RESET_BUTTON_TASK_H

// ============================================================================
// CONSTANTS AND TYPES
// ============================================================================



#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>

// Function prototypes

/**
 * @brief Initialize reset button task
 * @return ESP_OK on success
 */
esp_err_t reset_button_task_init(void);

/**
 * @brief Reset button task main function
 * @param pvParameters Task parameters (unused)
 */
void reset_button_task(void *pvParameters);

/**
 * @brief Process reset request
 * @param reset_request Whether reset is requested
 * @return ESP_OK on success
 */
esp_err_t process_reset_request(bool reset_request);

/**
 * @brief Simulate reset button press/release
 * @param pressed Whether button is pressed
 * @return ESP_OK on success
 */
esp_err_t simulate_reset_button_press(bool pressed);

/**
 * @brief Check if reset button task is initialized
 * @return true if initialized
 */
bool reset_button_is_initialized(void);

/**
 * @brief Get reset button event count
 * @return Number of button events processed
 */
uint32_t reset_button_get_event_count(void);

#endif /* RESET_BUTTON_TASK_H */
