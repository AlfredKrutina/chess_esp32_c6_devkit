/**
 * @file button_task.h
 * @brief ESP32-C6 Chess System v2.4 - Button Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro button task:
 * - Typy a struktury button udalosti
 * - Prototypy funkci button tasku
 * - Funkce pro ovladani a stav tlacitek
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Button task zpracovava vsechny tlacitka v sachovem systemu:
 * - 8 promotion tlacitek (4 pro bileho + 4 pro cerneho)
 * - 1 reset tlacitko
 * - Debouncing a detekce udalosti
 * - LED feedback pro stav tlacitek
 * - Long press a double press detekce
 */

#ifndef BUTTON_TASK_H
#define BUTTON_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// KONSTANTY A DEFINICE
// ============================================================================




/** @brief Typy button prikazu */
typedef enum {
    BUTTON_CMD_RESET = 0,   ///< Resetuj vsechna tlacitka
    BUTTON_CMD_STATUS,      ///< Vypis stav tlacitek
    BUTTON_CMD_TEST         ///< Testuj vsechna tlacitka
} button_command_t;


// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================


/**
 * @brief Spusti button task
 * 
 * Hlavni funkce button tasku. Bezi v nekonecne smycce a zpracovava
 * tlacitka kazdych 5ms. Provadi debouncing a generuje udalosti.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void button_task_start(void *pvParameters);


// ============================================================================
// FUNKCE PRO SKENOVANI TLACITEK
// ============================================================================


/**
 * @brief Oskenuje vsechna tlacitka pro zmenu stavu
 * 
 * Cteni stavu vsech 9 tlacitek a aktualizace interniho stavu.
 * V simulacnim rezimu simuluje postupne stisknuti tlacitek.
 */
void button_scan_all(void);

/**
 * @brief Simuluj stisknuti tlacitka (pro testovani)
 * 
 * @param button_id ID tlacitka k simulaci (0-8)
 */
void button_simulate_press(uint8_t button_id);

/**
 * @brief Simuluj uvolneni tlacitka (pro testovani)
 * 
 * @param button_id ID tlacitka k simulaci (0-8)
 */
void button_simulate_release(uint8_t button_id);


// ============================================================================
// FUNKCE PRO ZPRACOVANI BUTTON UDALOSTI
// ============================================================================


/**
 * @brief Zpracuj button udalosti a zmeny stavu
 * 
 * Detekuje zmeny stavu tlacitek a generuje udalosti (press, release,
 * long press, double press).
 */
void button_process_events(void);

/**
 * @brief Zpracuj udalost stisknuti tlacitka
 * 
 * @param button_id ID tlacitka ktere bylo stisknuto (0-8)
 */
void button_handle_press(uint8_t button_id);

/**
 * @brief Zpracuj udalost uvolneni tlacitka
 * 
 * @param button_id ID tlacitka ktere bylo uvolneno (0-8)
 */
void button_handle_release(uint8_t button_id);

/**
 * @brief Overi double press na tlacitku
 * 
 * Kontroluje zda bylo tlacitko stisknuto dvakrat po sobe
 * v intervalu 300ms.
 * 
 * @param button_id ID tlacitka k overeni (0-8)
 */
void button_check_double_press(uint8_t button_id);

/**
 * @brief Posli button udalost do fronty
 * 
 * @param button_id ID tlacitka (0-8)
 * @param event_type Typ udalosti
 * @param duration Doba stisknuti v milisekundach
 */
void button_send_event(uint8_t button_id, button_event_type_t event_type, uint32_t duration);


// ============================================================================
// FUNKCE PRO LED FEEDBACK TLACITEK
// ============================================================================


/**
 * @brief Aktualizuj LED feedback pro stav tlacitka
 * 
 * Nastavi barvu LED tlacitka podle jeho stavu (stisknuto/uvolneno).
 * Stisknute tlacitko sviti pulzujici cervenou, uvolnene jemnou zelenou.
 * 
 * @param button_id ID tlacitka (0-8)
 * @param pressed Je tlacitko stisknuto?
 */
void button_update_led_feedback(uint8_t button_id, bool pressed);

/**
 * @brief Nastav barvu LED pro tlacitko
 * 
 * @param led_index Index LED (64-72 pro tlacitka)
 * @param red Cervena komponenta (0-255)
 * @param green Zelena komponenta (0-255)
 * @param blue Modra komponenta (0-255)
 */
void button_set_led_color(uint8_t led_index, uint8_t red, uint8_t green, uint8_t blue);


// ============================================================================
// FUNKCE PRO ZPRACOVANI PRIKAZU
// ============================================================================


/**
 * @brief Zpracuj button prikazy z fronty
 * 
 * Cte prikazy z button_command_queue a vykonava je (reset, status, test).
 */
void button_process_commands(void);

/**
 * @brief Resetuj vsechny stavy tlacitek
 * 
 * Vymaze vsechny vnitrni stavy tlacitek a resetuje simulacni rezim.
 */
void button_reset_all(void);

/**
 * @brief Vypis stav tlacitek
 * 
 * Vypise stav vsech 9 tlacitek a informace o simulacnim rezimu.
 */
void button_print_status(void);

/**
 * @brief Testuj vsechna tlacitka
 * 
 * Postupne simuluje stisknuti a uvolneni vsech 9 tlacitek
 * pro otestovani funkcnosti.
 */
void button_test_all(void);


#endif // BUTTON_TASK_H
