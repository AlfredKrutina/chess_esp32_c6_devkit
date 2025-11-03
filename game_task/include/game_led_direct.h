/**
 * @file game_led_direct.h
 * @brief PRIME LED FUNKCE PRO GAME TASK - Zadny Queue Hell
 * 
 * RESENI: Misto xQueueSend(led_command_queue) pouzivat prime LED volani
 * - Zadny queue hell
 * - Zadne timing problemy
 * - Okamzite LED aktualizace
 * 
 * @author Alfred Krutina
 * @version 2.5 - DIRECT SYSTEM
 * @date 2025-09-02
 * 
 * @details
 * Tento modul poskytuje prime LED funkce pro game task bez pouziti front.
 * Eliminuje queue hell a timing problemy tim, ze vola LED funkce primo.
 * Vsechny funkce jsou thread-safe diky pouziti mutexu v led_task.
 */

#ifndef GAME_LED_DIRECT_H
#define GAME_LED_DIRECT_H

#include <stdint.h>
#include "chess_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PRIME LED FUNKCE - ZADNY QUEUE HELL
// ============================================================================

/**
 * @brief Zobraz tah s primymi LED volanimi
 * 
 * Zobrazi animaci tahu s postupnym svicenim zdrojoveho a ciloveho pole.
 * Pouziva barvy: zluta (zdroj), zelena (cil), cervena (capture).
 * 
 * @param from_row Zdrojovy radek (0-7)
 * @param from_col Zdrojovy sloupec (0-7)
 * @param to_row Cilovy radek (0-7)
 * @param to_col Cilovy sloupec (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Zobraz zvednutí figurky s primymi LED volanimi
 * 
 * Zobrazi jemnou pulzujici zluto-oranzovou animaci na pozici zvednute figurky.
 * 
 * @param row Radek figurky (0-7)
 * @param col Sloupec figurky (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_piece_lift_direct(uint8_t row, uint8_t col);

/**
 * @brief Zobraz platne tahy s primymi LED volanimi
 * 
 * Zobrazi jemnou zelenou vlnu na vsech pozicich kam muze figurka tahn out.
 * 
 * @param valid_positions Pole LED indexu platnych pozic
 * @param count Pocet platnych pozic
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

/**
 * @brief Zobraz chybu s primymi LED volanimi
 * 
 * Zobrazi cervenou LED na pozici kde doslo k chybe.
 * 
 * @param row Radek chyby (0-7)
 * @param col Sloupec chyby (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_error_direct(uint8_t row, uint8_t col);

/**
 * @brief Vymaz zvyrazneni s primymi LED volanimi
 * 
 * Vymaze vsechna LED zvyrazneni na sachovnici (zachova tlacitka).
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_clear_highlights_direct(void);

/**
 * @brief Zobraz stav hry s primymi LED volanimi
 * 
 * Zobrazi aktualni pozici vsech figurek na sachovnici pomoci LED.
 * Bile figurky = bila barva, cerne figurky = modra barva.
 * 
 * @param board Ukazatel na pole stavu sachovnice (64 prvku)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_state_direct(piece_t *board);

/**
 * @brief Zobraz puzzle s primymi LED volanimi
 * 
 * Zobrazi navod pro reseni puzzle (zdrojova a cilova pozice).
 * 
 * @param from_row Zdrojovy radek (0-7)
 * @param from_col Zdrojovy sloupec (0-7)
 * @param to_row Cilovy radek (0-7)
 * @param to_col Cilovy sloupec (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_puzzle_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Zobraz sach s primymi LED volanimi
 * 
 * Zobrazi cervenou blikajici LED na pozici krale v sachu.
 * 
 * @param king_row Radek krale (0-7)
 * @param king_col Sloupec krale (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_check_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Zobraz mat s primymi LED volanimi
 * 
 * Zobrazi intenzivni cervenou blikajici animaci na pozici krale v matu.
 * 
 * @param king_row Radek krale (0-7)
 * @param king_col Sloupec krale (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Zobraz promoci s primymi LED volanimi
 * 
 * Zobrazi zlato-bilou blikajici animaci na pozici promovaneho pescu.
 * 
 * @param row Radek promoci (0-7)
 * @param col Sloupec promoci (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_promotion_direct(uint8_t row, uint8_t col);

/**
 * @brief Zobraz rosadu s primymi LED volanimi
 * 
 * Zobrazi animaci rosady - pohyb krale a veze.
 * 
 * @param king_from_row Zdrojovy radek krale (0-7)
 * @param king_from_col Zdrojovy sloupec krale (0-7)
 * @param king_to_row Cilovy radek krale (0-7)
 * @param king_to_col Cilovy sloupec krale (0-7)
 * @param rook_from_row Zdrojovy radek veze (0-7)
 * @param rook_from_col Zdrojovy sloupec veze (0-7)
 * @param rook_to_row Cilovy radek veze (0-7)
 * @param rook_to_col Cilovy sloupec veze (0-7)
 * 
 * @note PRIME VOLANI - zadny queue
 */
void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col, 
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col);

#ifdef __cplusplus
}
#endif

#endif // GAME_LED_DIRECT_H
