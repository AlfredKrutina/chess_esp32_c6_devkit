/**
 * @file   promotion_button_task.h
 * @brief  Promotion button task header for ESP32-C6 chess project
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date   2025-08-16
 */
#ifndef PROMOTION_BUTTON_TASK_H
#define PROMOTION_BUTTON_TASK_H

#include "freertos_chess.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CONSTANTS AND TYPES
// ============================================================================


#define PROMOTION_BUTTON_TASK_PRIORITY   3

// Function prototypes

/**
 * @brief Initialize promotion button task
 * @return ESP_OK on success
 */
esp_err_t promotion_button_task_init(void);

/**
 * @brief Promotion button task main function
 * @param pvParameters Task parameters (unused)
 */
void promotion_button_task(void *pvParameters);

/**
 * @brief Process promotion choice
 * @param choice Promotion choice
 * @return ESP_OK on success
 */
esp_err_t process_promotion_choice(promotion_choice_t choice);

/**
 * @brief Simulate promotion button press
 * @param button_index Button index (0-3)
 * @return ESP_OK on success
 */
esp_err_t simulate_promotion_button_press(uint8_t button_index);

/**
 * @brief Check if promotion button task is initialized
 * @return true if initialized
 */
bool promotion_button_is_initialized(void);

/**
 * @brief Get promotion button event count
 * @return Number of button events processed
 */
uint32_t promotion_button_get_event_count(void);

#endif /* PROMOTION_BUTTON_TASK_H */
