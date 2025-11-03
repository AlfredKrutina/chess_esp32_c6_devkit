/**
 * @file matrix_task.h
 * @brief ESP32-C6 Chess System v2.4 - Matrix Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro matrix task:
 * - Skenovani 8x8 reed switch matice
 * - Detekce a validace tahu
 * - Generovani maticovych udalosti
 * - Time-multiplexed GPIO ovladani
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Matrix task je zodpovedny za detekci figurek na sachovnici pomoci
 * 8x8 reed switch matice. Skenuje matici kazdych 20ms a detekuje
 * kdy hrac zvedne nebo polozi figurku. Komunikuje s game taskem pres fronty.
 * 
 * Hardware:
 * - 8x8 reed switch matice
 * - Row piny: GPIO10-11,18-23 (vystupy)
 * - Column piny: GPIO0-3,6,14,16-17 (vstupy s pull-up)
 * - Time-multiplexed s tlacitkama
 */

#ifndef MATRIX_TASK_H
#define MATRIX_TASK_H

#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Spusti matrix task
 * 
 * Hlavni funkce matrix tasku. Bezi v nekonecne smycce a zpracovava
 * skenovani matice kazdych 1ms. Detekuje pohyb figurek a generuje udalosti.
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void matrix_task_start(void *pvParameters);

// ============================================================================
// FUNKCE PRO SKENOVANI MATICE
// ============================================================================

/**
 * @brief Oskenuj jeden radek matice
 * 
 * Nastavi radkovy pin na HIGH, precte vsechny sloupcove piny
 * a aktualizuje stav matice pro tento radek.
 * 
 * @param row Radek k oskenovani (0-7)
 */
void matrix_scan_row(uint8_t row);

/**
 * @brief Oskenuj celou matici
 * 
 * Oskenuje vsech 8 radku matice a detekuje zmeny stavu.
 * Pouziva mutex pro thread-safe pristup ke stavu matice.
 */
void matrix_scan_all(void);

// ============================================================================
// FUNKCE PRO DETEKCI TAHU
// ============================================================================

/**
 * @brief Detekuj tahy z matice
 * 
 * Hleda prechody 1->0 (figurka zvednuta) a 0->1 (figurka polozena)
 * a generuje maticove udalosti.
 */
void matrix_detect_moves(void);

/**
 * @brief Detekuj kompletni tah
 * 
 * Detekuje kompletni tah (zvednutí + položení figurky) a posle
 * MATRIX_EVENT_MOVE_DETECTED udalost.
 * 
 * @param from_square Zdrojove pole (0-63)
 * @param to_square Cilove pole (0-63)
 */
void matrix_detect_complete_move(uint8_t from_square, uint8_t to_square);

// ============================================================================
// UTILITY FUNKCE
// ============================================================================

/**
 * @brief Prevede cislo pole na sachovou notaci
 * 
 * @param square Cislo pole (0-63)
 * @param[out] notation Vystupni buffer pro notaci (min. 3 znaky)
 */
void matrix_square_to_notation(uint8_t square, char* notation);

/**
 * @brief Prevede sachovou notaci na cislo pole
 * 
 * @param notation Sachova notace (napr. "e2")
 * @return Cislo pole (0-63)
 */
uint8_t matrix_notation_to_square(const char* notation);

/**
 * @brief Vypis aktualni stav matice
 * 
 * Vypise 8x8 matici s aktualni stav vsech poli (0 = prazdne, 1 = figurka).
 */
void matrix_print_state(void);

/**
 * @brief Simuluj tah v matici
 * 
 * Pro testovani - simuluje zvednutí figurky z jednoho pole
 * a polozeni na jine pole.
 * 
 * @param from Zdrojova sachova notace (napr. "e2")
 * @param to Cilova sachova notace (napr. "e4")
 */
void matrix_simulate_move(const char* from, const char* to);

/**
 * @brief Ziskej stav matice
 * 
 * @param[out] state_buffer Buffer pro 64-prvkove pole stavu (1 = piece, 0 = empty)
 */
void matrix_get_state(uint8_t* state_buffer);

// ============================================================================
// FUNKCE PRO ZPRACOVANI PRIKAZU
// ============================================================================

/**
 * @brief Zpracuj matrix prikazy z fronty
 * 
 * Cte prikazy z matrix_command_queue a vykonava je
 * (scan, reset, test, calibrate, enable/disable).
 */
void matrix_process_commands(void);

/**
 * @brief Resetuj stav matice
 * 
 * Vymaze vsechny vnitrni stavy matice a resetuje detekci tahu.
 */
void matrix_reset(void);

#ifdef __cplusplus
}
#endif

#endif // MATRIX_TASK_H
