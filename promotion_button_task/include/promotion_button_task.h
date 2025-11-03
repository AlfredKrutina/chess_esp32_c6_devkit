/**
 * @file   promotion_button_task.h
 * @brief  Promotion Button Task hlavicka pro ESP32-C6 sachovy projekt
 *
 * Tato hlavicka definuje rozhrani pro promotion button task:
 * - Inicializace promotion button tasku a FreeRTOS komponent
 * - Zpracovani tlacitek pro volbu promoci (dama, vez, strelec, kun)
 * - Simulacni rezim bez hardware (pro development)
 * - Integrace s game taskem pro promoci
 * - 4 tlacitka pro ruzne typy promoci
 *
 * @author Alfred Krutina
 * @version 2.0
 * @date   2025-08-16
 * 
 * @details
 * Tento task zpracovava tlacitka pro promoci pescu. Kdyz pesec
 * dojde na konec sachovnice, hrac muze vybrat na co ho promenit
 * pomoci tlacitek (dama, vez, strelec, kun).
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

/** @brief Priorita promotion button tasku */
#define PROMOTION_BUTTON_TASK_PRIORITY   3

// ============================================================================
// PROTOTYPY FUNKCI
// ============================================================================

/**
 * @brief Inicializuje promotion button task
 * 
 * Vytvori FreeRTOS task pro zpracovani promotion tlacitek.
 * 
 * @return ESP_OK pri uspechu, ESP_ERR_NO_MEM pri nedostatku pameti
 */
esp_err_t promotion_button_task_init(void);

/**
 * @brief Hlavni funkce promotion button tasku
 * 
 * Bezi v nekonecne smycce a zpracovava promotion tlacitka.
 * V simulacnim rezimu pouze loguje udalosti.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void promotion_button_task(void *pvParameters);

/**
 * @brief Zpracuj volbu promoci
 * 
 * Zpracuje volbu hrace pro promoci pescu (dama, vez, strelec, kun).
 * 
 * @param choice Volba promoci (PROMOTION_QUEEN, PROMOTION_ROOK, atd.)
 * @return ESP_OK pri uspechu, ESP_ERR_INVALID_STATE pokud task neni inicializovan
 */
esp_err_t process_promotion_choice(promotion_choice_t choice);

/**
 * @brief Simuluj stisknuti promotion tlacitka
 * 
 * Pro testovani - simuluje stisknuti jednoho ze 4 promotion tlacitek.
 * 
 * @param button_index Index tlacitka (0-3: dama, vez, strelec, kun)
 * @return ESP_OK pri uspechu, ESP_ERR_INVALID_ARG pri neplatnem indexu
 */
esp_err_t simulate_promotion_button_press(uint8_t button_index);

/**
 * @brief Overi zda je promotion button task inicializovan
 * 
 * @return true pokud je task inicializovan
 */
bool promotion_button_is_initialized(void);

/**
 * @brief Ziskej pocet zpracovanych button udalosti
 * 
 * @return Pocet zpracovanych udalosti od startu
 */
uint32_t promotion_button_get_event_count(void);

#ifdef __cplusplus
}
#endif

#endif /* PROMOTION_BUTTON_TASK_H */
