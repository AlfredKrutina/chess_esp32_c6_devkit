/**
 * @file promotion_button_task.c
 * @brief Promotion button task: ovladani tlacitek pro promoci pescu
 *
 * Tento modul implementuje:
 *  - Inicializaci promotion button tasku a FreeRTOS komponent
 *  - Zpracovani tlacitek pro volbu promoci (dama, vez, strelec, kun)
 *  - Simulacni rezim bez hardware (pro development)
 *  - Integration s game taskem pro promoci
 *  - 4 tlacitka pro ruzne typy promoci
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date 2025-08-16
 * 
 * @details
 * Tento task zpracovava tlacitka pro promoci pescu. Kdyz pesec
 * dojde na konec sachovnice, hrac muze vybrat na co ho promenit
 * pomoci tlacitek. Task detekuje stisknuti tlacitek a posila
 * informaci do game tasku.
 */

#include "promotion_button_task.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos_chess.h"  // OPRAVENO: Přidán include pro konstanty

static const char *TAG = "PROMOTION_BUTTON_TASK";  // Tag pro ESP-IDF logovani

// Promotion button task status
static bool promotion_button_initialized = false;   // Stav inicializace promotion button tasku
static uint32_t button_event_count = 0;            // Pocet zpracovanych tlacitkovych udalosti

esp_err_t promotion_button_task_init(void)
{
    if (promotion_button_initialized) {  // Kontrola ze task uz neni inicializovan
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing promotion button task (SIMULATION MODE)...");
    
    // Create promotion button task
    BaseType_t task_created = xTaskCreate(
        promotion_button_task,                    // Funkce promotion button tasku
        "promotion_button_task",                  // Nazev tasku
        PROMOTION_BUTTON_TASK_STACK_SIZE,         // OPRAVENO: Místo hardcodované 2048 - velikost stacku
        NULL,                                     // Task parameters (unused)
        PROMOTION_BUTTON_TASK_PRIORITY,           // OPRAVENO: Místo hardcodované 3 - priorita tasku
        NULL                                      // Task handle (unused)
    );
    
    if (task_created != pdPASS) {  // Kontrola ze task byl vytvoren uspesne
        ESP_LOGE(TAG, "Failed to create promotion button task");
        return ESP_ERR_NO_MEM;  // Chyba: nedostatek pameti
    }
    
    promotion_button_initialized = true;  // Označení ze task je inicializovan
    ESP_LOGI(TAG, "Promotion button task initialized successfully (SIMULATION MODE)");
    
    return ESP_OK;  // Uspech
}

void promotion_button_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Promotion button task started (SIMULATION MODE)");
    
    promotion_choice_t choice;                    // Buffer pro volbu promoci
    TickType_t last_wake_time = xTaskGetTickCount(); // Cas posledniho probuzeni pro periodicke spousteni
    
    while (1) {
        // CRITICAL: Reset watchdog for promotion button task in every iteration
        esp_task_wdt_reset();
        
        // Process promotion button events (simulation mode - no real queue)
        // TODO: Implement queue communication in the future
        
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));  // Cekani 100ms mezi cykly (10 Hz)
    }
}

esp_err_t process_promotion_choice(promotion_choice_t choice)
{
    if (!promotion_button_initialized) {  // Kontrola inicializace promotion button tasku
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Processing promotion choice: %d", choice);
    
    switch (choice) {
        case PROMOTION_QUEEN:  // Promoce na damu
            ESP_LOGI(TAG, "Promotion choice: QUEEN");
            break;
        case PROMOTION_ROOK:   // Promoce na vez
            ESP_LOGI(TAG, "Promotion choice: ROOK");
            break;
        case PROMOTION_BISHOP: // Promoce na strelce
            ESP_LOGI(TAG, "Promotion choice: BISHOP");
            break;
        case PROMOTION_KNIGHT: // Promoce na kune
            ESP_LOGI(TAG, "Promotion choice: KNIGHT");
            break;
        default:  // Neznamy typ promoce
            ESP_LOGW(TAG, "Unknown promotion choice: %d", choice);
            return ESP_ERR_INVALID_ARG;
    }
    
    // TODO: Send promotion choice to game task
    // For now, just log the choice
    
    return ESP_OK;  // Uspech
}

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

bool promotion_button_is_initialized(void)
{
    return promotion_button_initialized;
}

uint32_t promotion_button_get_event_count(void)
{
    return button_event_count;
}
