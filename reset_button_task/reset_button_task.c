/**
 * @file   reset_button_task.c
 * @brief  Reset button task: ovladani reset tlacitka pro restart hry
 *
 * Tento modul implementuje:
 *  - Inicializaci reset button tasku a FreeRTOS komponent
 *  - Zpracovani reset tlacitka pro restart hry
 *  - Simulacni rezim bez hardware (pro development)
 *  - Integration s game taskem pro reset
 *  - Jedno tlacitko pro reset cele hry
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date   2025-08-16
 */

#include "reset_button_task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos_chess.h"  // OPRAVENO: Přidán include pro konstanty

static const char *TAG = "RESET_BUTTON_TASK";

// Reset button task status
static bool reset_button_initialized = false;
static uint32_t button_event_count = 0;

esp_err_t reset_button_task_init(void)
{
    if (reset_button_initialized) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing reset button task (SIMULATION MODE)...");
    
    // Create reset button task
    BaseType_t task_created = xTaskCreate(
        reset_button_task,
        "reset_button_task",
        RESET_BUTTON_TASK_STACK_SIZE,  // OPRAVENO: Místo hardcodované 2048
        NULL,
        RESET_BUTTON_TASK_PRIORITY,    // OPRAVENO: Místo hardcodované 3
        NULL
    );
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create reset button task");
        return ESP_ERR_NO_MEM;
    }
    
    reset_button_initialized = true;
    ESP_LOGI(TAG, "Reset button task initialized successfully (SIMULATION MODE)");
    
    return ESP_OK;
}

void reset_button_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Reset button task started (SIMULATION MODE)");
    
    bool reset_request;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        // CRITICAL: Reset watchdog for reset button task in every iteration
        esp_task_wdt_reset();
        
        // Process reset button events (simulation mode - no real queue)
        // TODO: Implement queue communication in the future
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }
}

esp_err_t process_reset_request(bool reset_request)
{
    if (!reset_button_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (reset_request) {
        ESP_LOGI(TAG, "Reset button pressed - requesting game reset");
        // TODO: Send reset command to game task
        // For now, just log the request
    } else {
        ESP_LOGI(TAG, "Reset button released");
    }
    
    return ESP_OK;
}

esp_err_t simulate_reset_button_press(bool pressed)
{
    if (!reset_button_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // Simulation mode - just log the event
    button_event_count++;
    ESP_LOGI(TAG, "Simulated reset button %s", pressed ? "press" : "release");
    
    return ESP_OK;
}

bool reset_button_is_initialized(void)
{
    return reset_button_initialized;
}

uint32_t reset_button_get_event_count(void)
{
    return button_event_count;
}
