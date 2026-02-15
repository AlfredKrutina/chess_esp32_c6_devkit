/**
 * @file led_task_simple.h
 * @brief Jednoduchy LED system - Bezpecne rozhrani z vice vlaken
 * 
 * Tato hlavicka definuje jednoduchy LED system pro ESP32-C6:
 * - Bezpecne rozhrani z vice vlaken pro ovladani LED
 * - Prime volani WS2812B driveru s mutex ochranou
 * - Optimalizovane pro ESP32-C6 RMT
 * - Jednoduche rozhrani bez slozitych fronticek
 * 
 * @author Alfred Krutina
 * @version 2.5
 * @date 2025-09-02
 * 
 * @details
 * Jednoduchy LED system poskytuje ciste, bezpecne rozhrani z vice vlaken pro ovladani LED
 * bez slozitych fronticek a davkoveho systemu. Pouziva primo WS2812B driver
 * s mutex ochranou pro bezpecnost z vice vlaken.
 * 
 * Vlastnosti:
 * - Okamzite LED aktualizace bez davkoveho zpozdeni
 * - Bezpecne z vice vlaken s mutexem pro prevenci soubeznych pristupu
 * - Jednoduche rozhrani se 3 zakladnimi funkcemi
 * - Optimalizovane pro ESP32-C6 RMT
 * - Minimalni pametove naroky
 */

#ifndef LED_TASK_SIMPLE_H
#define LED_TASK_SIMPLE_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// JEDNODUCHE LED SYSTEM API
// ============================================================================

/**
 * @brief Bezpecne nastaveni LED pixelu z vice vlaken
 * 
 * @param index LED index (0-72, kde 0-63 sachovnice, 64-72 tlacitka)
 * @param r Cervena komponenta (0-255)
 * @param g Zelena komponenta (0-255)
 * @param b Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @details
 * Nastavi barvu jednoho LED pixelu. Funkce provadi okamzite aktualizace
 * bez davkoveho systemu. Pouziva mutex pro bezpecnost z vice vlaken a je optimalizovana
 * pro ESP32-C6 RMT.
 * 
 * @note Tato funkce je bezpecna z vice vlaken a muze byt volana z libovolneho tasku
 */
esp_err_t led_set_pixel_safe(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Bezpecne vymazani vsech LED z vice vlaken
 * 
 * Nastavi vsechny LED na cernou barvu (vypnuto).
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @note Tato funkce je bezpecna z vice vlaken
 */
esp_err_t led_clear_all_safe(void);

/**
 * @brief Bezpecne nastaveni vsech LED na stejnou barvu z vice vlaken
 * 
 * @param r Cervena komponenta (0-255)
 * @param g Zelena komponenta (0-255)
 * @param b Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @note Tato funkce je bezpecna z vice vlaken
 */
esp_err_t led_set_all_safe(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Inicializuj LED system
 * 
 * Inicializuje WS2812B LED driver, vytvori mutex a nastavi
 * vsechny LED na vypnuto.
 * 
 * @return ESP_OK pri uspechu, ESP_FAIL pri selhani
 */
esp_err_t led_system_init(void);

/**
 * @brief Overi zda je LED system inicializovan
 * 
 * @return true pokud je system inicializovan a pripraven k pouziti
 */
bool led_system_is_initialized(void);

/**
 * @brief Ziskej barvu LED
 * 
 * @param index LED index (0-72)
 * @return LED barva jako uint32_t RGB (0x00RRGGBB)
 */
uint32_t led_get_color(uint8_t index);

/**
 * @brief Spust jednoduchy LED task
 * 
 * Toto je hlavni funkce LED tasku. Nepouzivejte primo.
 * 
 * @param pvParameters Parametry tasku
 */
void led_task_start(void *pvParameters);

// ============================================================================
// KOMPATIBILNI API (pro existujici kod)
// ============================================================================

/**
 * @brief Kompatibilni funkce pro existujici kod
 * 
 * Alias pro led_set_pixel_safe().
 * 
 * @param index LED index (0-72)
 * @param r Cervena (0-255)
 * @param g Zelena (0-255)
 * @param b Modra (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_pixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Kompatibilni funkce pro existujici kod
 * 
 * Alias pro led_clear_all_safe().
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear_all(void);

/**
 * @brief Kompatibilni funkce pro existujici kod
 * 
 * Alias pro led_set_all_safe().
 * 
 * @param r Cervena (0-255)
 * @param g Zelena (0-255)
 * @param b Modra (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set_all(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif // LED_TASK_SIMPLE_H
