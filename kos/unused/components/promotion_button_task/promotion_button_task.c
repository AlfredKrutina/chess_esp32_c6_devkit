/**
 * @file promotion_button_task.c
 * @brief Promotion button task: control of promotion buttons
 *
 * This module implements:
 * - Initialization of promotion button task and FreeRTOS components
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
 * help buttons. Task detects a button press and sends it
 * information in the game task.
 */

#include "promotion_button_task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos_chess.h"  // FIXED: Added include for constants

static const char *TAG = "PROMOTION_BUTTON_TASK";  // Tag pro ESP-IDF logovani

// Promotion button task status
static bool promotion_button_initialized = false;   // Promotion button task initialization status
static uint32_t button_event_count = 0;            // The number of processed button events

/**
 * @brief Initialize the promotion button task
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t promotion_button_task_init(void)
{
    if (promotion_button_initialized) {  // Check that the task is no longer initialized
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing promotion button task (SIMULATION MODE)...");
    
    // Create promotion button task
    BaseType_t task_created = xTaskCreate(
        promotion_button_task,                    // Function promotion button task
        "promotion_button_task",                  // Nazev tasku
        PROMOTION_BUTTON_TASK_STACK_SIZE,         // FIXED: Instead of hardcoded 2048 - stack size
        NULL,                                     // Task parameters (unused)
        PROMOTION_BUTTON_TASK_PRIORITY,           // FIXED: Instead of hardcoded 3 - task priority
        NULL                                      // Task handle (unused)
    );
    
    if (task_created != pdPASS) {  // Check from the task was created successfully
        ESP_LOGE(TAG, "Failed to create promotion button task");
        return ESP_ERR_NO_MEM;  // Error: out of memory
    }
    
    promotion_button_initialized = true;  // The tag from the task is initialized
    ESP_LOGI(TAG, "Promotion button task initialized successfully (SIMULATION MODE)");
    
    return ESP_OK;  // Success
}

/**
 * @brief The main function of the promotion button task
 * 
 * @param pvParameters Task parameters (not used)
 */
void promotion_button_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Promotion button task started (SIMULATION MODE)");
    
    TickType_t last_wake_time = xTaskGetTickCount(); // Cas posledniho probuzeni pro periodicke spousteni
    
    while (1) {
        // CRITICAL: Reset watchdog for promotion button task in every iteration
        esp_task_wdt_reset();
        
        // Process promotion button events (simulation mode - no real queue)
        // TODO: Implement queue communication in the future
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));  // Cekani 100ms mezi cykly (10 Hz)
    }
}

/**
 * @brief Process graduation choice
 * 
 * @param choice Graduation type (QUEEN, ROOK, BISHOP, KNIGHT)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t process_promotion_choice(promotion_choice_t choice)
{
    if (!promotion_button_initialized) {  // Checking the initialization of the promotion button task
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Processing promotion choice: %d", choice);
    
    switch (choice) {
        case PROMOTION_QUEEN:  // Graduation to a lady
            ESP_LOGI(TAG, "Promotion choice: QUEEN");
            break;
        case PROMOTION_ROOK:   // Graduation on
            ESP_LOGI(TAG, "Promotion choice: ROOK");
            break;
        case PROMOTION_BISHOP: // Graduation on shooter
            ESP_LOGI(TAG, "Promotion choice: BISHOP");
            break;
        case PROMOTION_KNIGHT: // Graduation on kune
            ESP_LOGI(TAG, "Promotion choice: KNIGHT");
            break;
        default:  // We do not know the type of graduation
            ESP_LOGW(TAG, "Unknown promotion choice: %d", choice);
            return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Send promotion choice to game task
    // For now, just log the choice
    
    return ESP_OK;  // Success
}

/**
 * @brief Simulate pressing the promotion button (for testing)
 * 
 * @param button_index Button index (0=QUEEN, 1=ROOK, 2=BISHOP, 3=KNIGHT)
 * @return ESP_OK on success, error code on failure
 */
esp_err_t simulate_promotion_button_press(uint8_t button_index)
{
    if (!promotion_button_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (button_index >= 4) {
        ESP_LOGW(TAG, "Invalid button index: %d (must be 0-3)", button_index);
        return ESP_ERR_INVALID_ARG;
    }
    
    promotion_choice_t choice;
    switch (button_index) {
        case 0: choice = PROMOTION_QUEEN; break;
        case 1: choice = PROMOTION_ROOK; break;
        case 2: choice = PROMOTION_BISHOP; break;
        case 3: choice = PROMOTION_KNIGHT; break;
        default: choice = PROMOTION_QUEEN; break;
    }
    
    // Simulation mode - just log the event
    
    ESP_LOGI(TAG, "Simulated promotion button %d press (choice: %d)", button_index, choice);
    
    return ESP_OK;
}

/**
 * @brief Verify if the promotion button task is initialized
 * 
 * @return True if the task is initialized
 */
bool promotion_button_is_initialized(void)
{
    return promotion_button_initialized;
}

/**
 * @brief Get the number of processed button events
 * 
 * @return Event count
 */
uint32_t promotion_button_get_event_count(void)
{
    return button_event_count;
}
