/**
 * @file   reset_button_task.h
 * @brief  Reset Button Task hlavicka pro ESP32-C6 sachovy projekt
 *
 * Tato hlavicka definuje rozhrani pro reset button task:
 * - Inicializace reset button tasku a FreeRTOS komponent
 * - Zpracovani reset tlacitka pro restart hry
 * - Simulacni rezim bez hardware (pro development)
 * - Integrace s game taskem pro reset
 * - Jedno tlacitko pro reset cele hry
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date   2025-08-16
 * 
 * @details
 * Tento task zpracovava reset tlacitko pro restart hry. Kdyz hrac
 * stiskne reset tlacitko (GPIO27), hra se restartuje do vychoziho stavu.
 * Task detekuje stisknuti a posila prikaz game tasku.
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
 * @brief Inicializuje reset button task
 * 
 * Vytvori FreeRTOS task pro zpracovani reset tlacitka.
 * 
 * @return ESP_OK pri uspechu, ESP_ERR_NO_MEM pri nedostatku pameti
 */
esp_err_t reset_button_task_init(void);

/**
 * @brief Hlavni funkce reset button tasku
 * 
 * Bezi v nekonecne smycce a zpracovava reset tlacitko.
 * V simulacnim rezimu pouze loguje udalosti.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void reset_button_task(void *pvParameters);

/**
 * @brief Zpracuj pozadavek na reset
 * 
 * Zpracuje pozadavek hrace na reset hry (stisknuti reset tlacitka).
 * 
 * @param reset_request Je pozadavek na reset aktivni?
 * @return ESP_OK pri uspechu, ESP_ERR_INVALID_STATE pokud task neni inicializovan
 */
esp_err_t process_reset_request(bool reset_request);

/**
 * @brief Simuluj stisknuti/uvolneni reset tlacitka
 * 
 * Pro testovani - simuluje stisknuti nebo uvolneni reset tlacitka.
 * 
 * @param pressed Je tlacitko stisknuto? (true = stisknuto, false = uvolneno)
 * @return ESP_OK pri uspechu
 */
esp_err_t simulate_reset_button_press(bool pressed);

/**
 * @brief Overi zda je reset button task inicializovan
 * 
 * @return true pokud je task inicializovan
 */
bool reset_button_is_initialized(void);

/**
 * @brief Ziskej pocet zpracovanych button udalosti
 * 
 * @return Pocet zpracovanych udalosti od startu
 */
uint32_t reset_button_get_event_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RESET_BUTTON_TASK_H */
