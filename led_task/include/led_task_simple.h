/**
 * @file led_task_simple.h
 * @brief JEDNODUCHY LED SYSTEM - Thread-Safe, Bez Batch, Bez Queue Hell
 * 
 * RESENI PRO ESP32-C6 RMT LIMITACE:
 * - Zadny batch system (konfliktuje s RMT timingem)
 * - Zadny queue hell (8 tasku soucasne)
 * - Zadny triple buffer (memory chaos)
 * - Zadne critical sections (blokuji RMT interrupty)
 * - Pouze prime LED volani s thread-safe mutexem
 * 
 * @author Alfred Krutina
 * @version 2.5 - SIMPLE SYSTEM
 * @date 2025-09-02
 * 
 * @details
 * Jednoduchy LED system poskytuje clean, thread-safe API pro ovladani LED
 * bez slozitych fronticek a batch systemu. Pouziva primo WS2812B driver
 * s mutex ochranou pro thread-safety.
 * 
 * Vyhody:
 * - Okamzite LED aktualizace (zadny batch delay)
 * - Thread-safe s mutexem (zadne race conditions)
 * - Jednoduche API (3 zakladni funkce)
 * - Optimalizovane pro ESP32-C6 RMT
 * - Zadne memory problymy
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
 * @brief Thread-safe nastaveni LED pixelu
 * 
 * OKAMZITY UPDATE - zadny batch system
 * ATOMICKY PRISTUP s mutexem
 * ESP32-C6 RMT optimalizace
 * 
 * @param index LED index (0-72, kde 0-63 sachovnice, 64-72 tlacitka)
 * @param r Cervena komponenta (0-255)
 * @param g Zelena komponenta (0-255)
 * @param b Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @note Tato funkce je THREAD-SAFE a muze byt volana z libovolneho tasku
 */
esp_err_t led_set_pixel_safe(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Thread-safe vymazani vsech LED
 * 
 * Nastavi vsechny LED na cernou barvu (vypnuto).
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @note Tato funkce je THREAD-SAFE
 */
esp_err_t led_clear_all_safe(void);

/**
 * @brief Thread-safe nastaveni vsech LED na stejnou barvu
 * 
 * @param r Cervena komponenta (0-255)
 * @param g Zelena komponenta (0-255)
 * @param b Modra komponenta (0-255)
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 * 
 * @note Tato funkce je THREAD-SAFE
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
