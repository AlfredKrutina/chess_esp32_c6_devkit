/**
 * @file led_mapping.h
 * @brief ESP32-C6 Chess System - LED Mapovani funkce
 * 
 * Tento modul poskytuje funkce pro prevod mezi sachovnicovymi pozicemi a LED indexy.
 * Pouziva serpentine layout pro optimalni rozlozeni LED na sachovnici.
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * LED Mapping modul prevadi sachovnicove pozice (radek, sloupec) na LED indexy
 * a vice versa. Pouziva serpentine layout pro hadovite rozlozeni LED pasku:
 * - Sude radky: a->h (zleva doprava)
 * - Liche radky: h->a (zprava doleva)
 * 
 * Priklad serpentine layout:
 * @code
 * Radek 1: LED  0,  1,  2,  3,  4,  5,  6,  7  (a1->h1)
 * Radek 2: LED 15, 14, 13, 12, 11, 10,  9,  8  (h2->a2)
 * Radek 3: LED 16, 17, 18, 19, 20, 21, 22, 23  (a3->h3)
 * ...
 * @endcode
 */

#ifndef LED_MAPPING_H
#define LED_MAPPING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// FUNKCE PRO MAPOVANI POZIC
// ============================================================================

/**
 * @brief Prevede sachovnicovou pozici na LED index
 * 
 * Tato funkce prevede souradnice sachovnice (radek, sloupec) na LED index
 * v serpentine layoutu.
 * 
 * @param row Radek sachovnice (0-7, kde 0 = rank 1, 7 = rank 8)
 * @param col Sloupec sachovnice (0-7, kde 0 = file a, 7 = file h)
 * @return LED index (0-63) pro pozici na sachovnici
 * 
 * @note Funkce automaticky osetruje neplatne vstupy a loguje chybu
 */
uint8_t chess_pos_to_led_index(uint8_t row, uint8_t col);

/**
 * @brief Prevede LED index na sachovnicovou pozici
 * 
 * Tato funkce prevede LED index na souradnice sachovnice (radek, sloupec)
 * v serpentine layoutu.
 * 
 * @param led_index LED index (0-63)
 * @param[out] row Ukazatel na vystupni radek (0-7)
 * @param[out] col Ukazatel na vystupni sloupec (0-7)
 * 
 * @note Funkce automaticky osetruje neplatne vstupy a loguje chybu
 */
void led_index_to_chess_pos(uint8_t led_index, uint8_t* row, uint8_t* col);

/**
 * @brief Prevede sachovou notaci na LED index
 * 
 * Tato funkce prevede sachovou notaci (napr. "e2", "a8") na LED index
 * pouzitim serpentine layoutu.
 * 
 * @param notation Sachova notace ve formatu "e2" (pismeno a-h, cislice 1-8)
 * @return LED index (0-63) pro danou notaci, nebo 0 pri chybe
 * 
 * @note Funkce automaticky osetruje neplatne vstupy a loguje chybu
 * 
 * @par Priklad pouziti:
 * @code
 * uint8_t led = chess_notation_to_led_index("e4");  // LED pro pole e4
 * @endcode
 */
uint8_t chess_notation_to_led_index(const char* notation);

/**
 * @brief Testuje spravnost LED mapovani
 * 
 * Tato funkce provede automaticky test vsech znamych pozic a overi
 * spravnost prevodu mezi pozicemi a LED indexy.
 * 
 * @note Vysledky testu jsou vypisovany do logu
 */
void test_led_mapping(void);

#ifdef __cplusplus
}
#endif

#endif // LED_MAPPING_H
