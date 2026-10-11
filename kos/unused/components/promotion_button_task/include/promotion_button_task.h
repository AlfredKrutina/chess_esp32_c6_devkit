/**
 * @file promotion_button_task.h
 * @brief Promotion Button Task header for ESP32-C6 sach project
 *
 * This header defines the interface for the promotion button task:
 * - Initialization of the promotion button task and FreeRTOS components
 * - Processing of buttons for choosing graduation (queen, queen, archer, king)
 * - Simulation mode without hardware (for development)
 * - Integration with game task for graduation
 * - 4 buttons for different types of graduation
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-16
 * 
 * @details
 * This task is processed by the sand graduation button. When the dog
 * reaches the end of the box, the player can choose what to change it to
 * using the keys (queen, queen, shooter, king).
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

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONSTANTY A TYPY
// ============================================================================

/** @brief Promotion button task priority */
#define PROMOTION_BUTTON_TASK_PRIORITY   3

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Initializes the promotion button task
 * 
 * Create a FreeRTOS task for processing promotion buttons.
 * 
 * @return ESP_OK on success, ESP_ERR_NO_MEM on lack of memory
 */
esp_err_t promotion_button_task_init(void);

/**
 * @brief The main function of the promotion button task
 * 
 * Runs in an infinite loop and processes promotion buttons.
 * Only logs events in simulation mode.
 * 
 * @param pvParameters Task parameters (not used)
 */
void promotion_button_task(void *pvParameters);

/**
 * @brief Process graduation choice
 * 
 * Processes the choice of player for the graduation of the pesca (queen, queen, shooter, king).
 * 
 * @param choice Choice of promotion (PROMOTION_QUEEN, PROMOTION_ROOK, etc.)
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if the task is not initialized
 */
esp_err_t process_promotion_choice(promotion_choice_t choice);

/**
 * @brief Simulate pressing the promotion button
 * 
 * For testing - simulates pressing one of the 4 promotion buttons.
 * 
 * @param button_index Index of the button (0-3: queen, queen, bishop, king)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on invalid index
 */
esp_err_t simulate_promotion_button_press(uint8_t button_index);

/**
 * @brief Verify if the promotion button task is initialized
 * 
 * @return true if the task is initialized
 */
bool promotion_button_is_initialized(void);

/**
 * @brief Get the number of button events processed
 * 
 * @return Number of processed events since start
 */
uint32_t promotion_button_get_event_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PROMOTION_BUTTON_TASK_H */
