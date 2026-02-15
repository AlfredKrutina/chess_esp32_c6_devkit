/**
 * @file game_task.h
 * @brief ESP32-C6 Chess System v2.4 - Game Task Hlavicka
 *
 * Tato hlavicka definuje rozhrani pro game task:
 * - Typy a struktury stavu hry
 * - Prototypy funkci game tasku
 * - Funkce pro ovladani a stav hry
 *
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 *
 * @details
 * Game task je stredem sachoveho systemu. Spravuje cely stav hry,
 * provadi validaci tahu, vynucuje sachova pravidla a komunikuje
 * s ostatnimi tasky pres fronty.
 *
 * Hlavni funkce:
 * - Standardni sachova pravidla (vsechny figur ky)
 * - Specialni tahy (rosada, en passant, promoce)
 * - Detekce sachu, matu a patu
 * - Historie tahu a undo funkcionalita
 * - Statistiky hry a materialni rovnovaha
 * - Integrace s LED pro vizualni zpetnou vazbu
 * - Integrace s casovym systemem
 * - JSON export pro webove rozhrani
 */

#ifndef GAME_TASK_H
#define GAME_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdbool.h>
#include <stdint.h>

// Integrace timer systemu
#include "../../timer_system/include/timer_system.h"

// ============================================================================
// KONSTANTY A DEFINICE
// ============================================================================

// Spolecne typy jsou definovany v chess_types.h
#include "chess_types.h"

// Struktury chess_move_t a move_suggestion_t jsou definovany v chess_types.h

// ============================================================================
// PROTOTYPY UTILITY FUNKCI
// ============================================================================

/**
 * @brief Prevede sachovou notaci na souradnice sachovnice
 *
 * @param notation Sachova notace (napr. "e2")
 * @param[out] row Vystupni radek (0-7)
 * @param[out] col Vystupni sloupec (0-7)
 * @return true pokud je prevod uspesny, false pri chybe
 */
bool convert_notation_to_coords(const char *notation, uint8_t *row,
                                uint8_t *col);

/**
 * @brief Prevede souradnice sachovnice na sachovou notaci
 *
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @param[out] notation Vystupni buffer pro notaci (min. 3 znaky)
 * @return true pokud je prevod uspesny, false pri chybe
 */
bool convert_coords_to_notation(uint8_t row, uint8_t col, char *notation);

/**
 * @brief Zpracuj sachovy tah z UART prikazu
 *
 * @param cmd Prikaz s tahem
 */
void game_process_chess_move(const chess_move_command_t *cmd);

// ============================================================================
// JSON EXPORT FUNKCE PRO WEB SERVER
// ============================================================================

/**
 * @brief Exportuj stav sachovnice do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t game_get_board_json(char *buffer, size_t size);

/**
 * @brief Exportuj stav hry do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
/**
 * @brief Vrati aktualni pocet tahu (thread-safe)
 * @return Aktualni pocet tahu od zacatku hry
 */
uint32_t game_get_move_count(void);

/**
 * @brief Exportuj stav hry do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t game_get_status_json(char *buffer, size_t size);

/**
 * @brief Vynuceny refresh LED podle aktualniho stavu hry
 * Volat po navratu z HA rezimu nebo jineho preruseni.
 */
void game_refresh_leds(void);

/**
 * @brief Exportuj historii tahu do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t game_get_history_json(char *buffer, size_t size);

/**
 * @brief Exportuj sebrane figurky do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t game_get_captured_json(char *buffer, size_t size);

/**
 * @brief Exportuj historii materialni vyhody do JSON (pro graf vyhody)
 *
 * @param[out] buffer Vystupni buffer pro JSON retezec
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri selhani
 */
esp_err_t game_get_advantage_json(char *buffer, size_t size);

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Spusti game task
 *
 * Hlavni funkce game tasku. Inicializuje sachovnici a bezi v nekonecne
 * smycce zpracovani prikazu a udalosti.
 *
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void game_task_start(void *pvParameters);

// ============================================================================
// INICIALIZACNI FUNKCE HRY
// ============================================================================

/**
 * @brief Inicializuj sachovnici do vychozi pozice
 *
 * Nastavi vsechny figurky do standardni startovni pozice.
 */
void game_initialize_board(void);

/**
 * @brief Resetuj hru do pocatecniho stavu
 *
 * Vymaze historii, resetuje statistiky a znovu inicializuje sachovnici.
 */
void game_reset_game(void);

/**
 * @brief Spust novou hru
 *
 * Kompletni reset hry vcetne timer systemu.
 */
void game_start_new_game(void);

/**
 * @brief Vynuluje stav error recovery (vraceni figurky na blikajici pole).
 * Volat napr. pri zapnuti demo modu, aby demo mohlo hrat i po predchozim
 * chybnem tahu uzivatele.
 */
void game_reset_error_recovery_state(void);

// ============================================================================
// UTILITY FUNKCE PRO SACHOVNICI
// ============================================================================

/**
 * @brief Overi zda je pozice platna na sachovnici
 *
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @return true pokud je pozice platna
 */
bool game_is_valid_position(int row, int col);

/**
 * @brief Ziskej figurku na dane pozici
 *
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @return Typ figurky na pozici
 */
piece_t game_get_piece(int row, int col);

/**
 * @brief Overi zda je pozice prazdna
 *
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @return true pokud je pozice prazdna
 */
bool game_is_empty(int row, int col);

/**
 * @brief Overi zda je figurka bila
 *
 * @param piece Figurka k overeni
 * @return true pokud je figurka bila
 */
bool game_is_white_piece(piece_t piece);

/**
 * @brief Overi zda je figurka cerna
 *
 * @param piece Figurka k overeni
 * @return true pokud je figurka cerna
 */
bool game_is_black_piece(piece_t piece);

/**
 * @brief Overi zda jsou dve figurky stejne barvy
 *
 * @param piece1 Prvni figurka
 * @param piece2 Druha figurka
 * @return true pokud jsou figurky stejne barvy
 */
bool game_is_same_color(piece_t piece1, piece_t piece2);

// ============================================================================
// FUNKCE PRO VALIDACI TAHU
// ============================================================================

/**
 * @brief Validuj tah podle typu figurky
 *
 * @param move Tah k validaci
 * @param piece Figurka ktera se pohybuje
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_is_valid_move(const chess_move_t *move);

/**
 * @brief Rozsirena validace tahu pro figurku
 *
 * @param move Tah k validaci
 * @param piece Figurka
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_piece_move_enhanced(const chess_move_t *move,
                                               piece_t piece);

/**
 * @brief Validuj tah pescu
 *
 * @param move Tah k validaci
 * @param piece Pesec
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_pawn_move_enhanced(const chess_move_t *move,
                                              piece_t piece);

/**
 * @brief Validuj tah kone
 *
 * @param move Tah k validaci
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_knight_move_enhanced(const chess_move_t *move);

/**
 * @brief Validuj tah strelce
 *
 * @param move Tah k validaci
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_bishop_move_enhanced(const chess_move_t *move);

/**
 * @brief Validuj tah veze
 *
 * @param move Tah k validaci
 * @return MOVE_ERROR_NONE pri uspechu, jinak kod chyby
 */
move_error_t game_validate_rook_move_enhanced(const chess_move_t *move);

/**
 * @brief Validuj tah damy
 *
 * @param move Tah k validaci
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_queen_move_enhanced(const chess_move_t *move);

/**
 * @brief Validuj tah krale
 *
 * @param move Tah k validaci
 * @return MOVE_ERROR_NONE pri platnem tahu, jinak kod chyby
 */
move_error_t game_validate_king_move_enhanced(const chess_move_t *move);

// Pomocne funkce pro rozsirenou validaci

/**
 * @brief Overi zda by tah nechal krale v sachu
 *
 * @param move Tah k overeni
 * @return true pokud by tah nechal krale v sachu
 */
bool game_would_move_leave_king_in_check(const chess_move_t *move);

/**
 * @brief Overi zda je en passant mozny
 *
 * @param move Tah k overeni
 * @return true pokud je en passant mozny
 */
bool game_is_en_passant_possible(const chess_move_t *move);

/**
 * @brief Validuj rosadu
 *
 * @param move Tah rosady k validaci
 * @return MOVE_ERROR_NONE pri platne rosade, jinak kod chyby
 */
move_error_t game_validate_castling(const chess_move_t *move);

/**
 * @brief Overi nedostatecny material pro mat
 *
 * @return true pokud je nedostatecny material (automaticka remiza)
 */
bool game_is_insufficient_material(void);

// Zobrazeni navodu pro tahy

/**
 * @brief Zobraz navrzene tahy pro figurku
 *
 * @param row Radek figurky (0-7)
 * @param col Sloupec figurky (0-7)
 */
void game_show_move_suggestions(uint8_t row, uint8_t col);

/**
 * @brief Ziskej dostupne tahy pro figurku
 *
 * @param row Radek figurky (0-7)
 * @param col Sloupec figurky (0-7)
 * @param[out] suggestions Pole pro navrzene tahy
 * @param max_suggestions Maximalni pocet navrhu
 * @return Pocet nalezenych tahu
 */
uint32_t game_get_available_moves(uint8_t row, uint8_t col,
                                  move_suggestion_t *suggestions,
                                  uint32_t max_suggestions);

// ============================================================================
// FUNKCE PRO PROVEDENI TAHU
// ============================================================================

/**
 * @brief Proved sachovy tah
 *
 * Provede tah pokud je platny. Aktualizuje sachovnici, historii
 * a posle LED feedback.
 *
 * @param move Tah k provedeni
 * @return true pokud byl tah uspesne proveden
 */
bool game_execute_move(const chess_move_t *move);

// ============================================================================
// FUNKCE PRO STAV HRY
// ============================================================================

/**
 * @brief Ziskej aktualni stav hry
 *
 * @return Aktualni stav hry (GAME_STATE_IDLE, GAME_STATE_PLAYING, atd.)
 */
game_state_t game_get_state(void);

/**
 * @brief Ziskej aktualniho hrace na tahu
 *
 * @return Aktualni hrac (PLAYER_WHITE nebo PLAYER_BLACK)
 */
player_t game_get_current_player(void);

/**
 * @brief Ziskej pocet provedenych tahu
 *
 * @return Pocet tahu od zacatku hry
 */
uint32_t game_get_move_count(void);

/**
 * @brief Vypis aktualni pozici sachovnice
 *
 * Zobrazi sachovnici v ASCII formatu s popisky.
 */
void game_print_board(void);

/**
 * @brief Ziskej jmeno figurky jako retezec
 *
 * @param piece Figurka
 * @return Nazev figurky (napr. "Bily pesec", "Cerna dama")
 */
const char *game_get_piece_name(piece_t piece);

// ============================================================================
// FUNKCE PRO ZPRACOVANI PRIKAZU
// ============================================================================

/**
 * @brief Zpracuj game prikazy z fronty
 *
 * Cte prikazy z game_command_queue a vykonava je.
 */
void game_process_commands(void);

/**
 * @brief Zpracuj neplatny tah
 *
 * Zobrazi chybovou zpravu a LED feedback pro neplatny tah.
 *
 * @param error Typ chyby
 * @param move Neplatny tah
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t *move);

/**
 * @brief Zobraz animaci zmeny hrace
 *
 * @param previous_player Predchozi hrac
 * @param current_player Aktualni hrac
 */
void game_show_player_change_animation(player_t previous_player,
                                       player_t current_player);

// ============================================================================
// TESTOVACI FUNKCE PRO ANIMACE
// ============================================================================

/** @brief Testuj animaci tahu */
void game_test_move_animation(void);
/** @brief Testuj animaci zmeny hrace */
void game_test_player_change_animation(void);
/** @brief Testuj animaci rosady */
void game_test_castle_animation(void);
/** @brief Testuj animaci promoci */
void game_test_promote_animation(void);
/** @brief Testuj animaci konce hry */
void game_test_endgame_animation(void);

// ============================================================================
// PRIME LED FUNKCE (bez front) - POUZIVANO V game_led_direct.c
// ============================================================================

/** @brief Zobraz tah primo pres LED */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                           uint8_t to_col);
/** @brief Zobraz sach primo pres LED */
void game_show_check_direct(uint8_t king_row, uint8_t king_col);
/** @brief Zobraz zmenu hrace primo pres LED */
void game_show_player_change_direct(player_t current_player);
/** @brief Vymaz zvyrazneni primo pres LED */
void game_clear_highlights_direct(void);

// ============================================================================
// JEMNE ANIMACNI FUNKCE - POUZIVANO V game_led_direct.c
// ============================================================================

/** @brief Zobraz zvednuti figurky primo pres LED */
void game_show_piece_lift_direct(uint8_t row, uint8_t col);
/** @brief Zobraz platne tahy primo pres LED */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

// ============================================================================
// ERROR HANDLING LED FUNKCE - POUZIVANO V game_led_direct.c
// ============================================================================

/** @brief Zobraz chybu neplatneho tahu */
void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col);
/** @brief Zobraz chybu tlacitka */
void game_show_button_error(uint8_t button_id);
/** @brief Zobraz navod pro rosadu */
void game_show_castling_guidance(uint8_t king_row, uint8_t king_col,
                                 uint8_t rook_row, uint8_t rook_col,
                                 bool is_kingside);

/**
 * @brief Overi podminky sachu/matu
 *
 * Kontroluje zda je kral v sachu, matu nebo patu.
 */
void game_check_game_conditions(void);

/**
 * @brief Prevede souradnice sachovnice na retezec pole
 *
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @param[out] square Vystupni retezec pole (napr. "e2")
 */
void game_coords_to_square(uint8_t row, uint8_t col, char *square);

/**
 * @brief Vypis stav hry
 *
 * Zobrazi detailni informace o stavu hry (hrac na tahu, pocet tahu, atd.).
 */
void game_print_status(void);

// ============================================================================
// STATISTIKY A DETEKCE KONCE HRY
// ============================================================================

/** @brief Vypis statistiky hry */
void game_print_game_stats(void);
/** @brief Ziskej pocet vyher bileho */
uint32_t game_get_white_wins(void);
/** @brief Ziskej pocet vyher cerneho */
uint32_t game_get_black_wins(void);
/** @brief Ziskej pocet remiz */
uint32_t game_get_draws(void);
/** @brief Ziskej celkovy pocet her */
uint32_t game_get_total_games(void);
/** @brief Ziskej textovy retezec stavu hry */
const char *game_get_game_state_string(void);
/** @brief Vypocitej hash pozice pro detekci opakovani */
uint32_t game_calculate_position_hash(void);
/** @brief Overi zda byla pozice opakovana */
bool game_is_position_repeated(void);
/** @brief Pridej pozici do historie pro detekci opakovani */
void game_add_position_to_history(void);
/** @brief Vypocitej materialovou rovnovahu */
int game_calculate_material_balance(int *white_material, int *black_material);
/** @brief Ziskej textovy retezec material balance */
void game_get_material_string(char *buffer, size_t buffer_size);
/** @brief Overi zda je kral v sachu */
bool game_is_king_in_check(player_t player);
/** @brief Overi zda ma hrac legalni tahy */
bool game_has_legal_moves(player_t player);
/** @brief Overi podminky konce hry */
game_state_t game_check_end_game_conditions(void);

// ============================================================================
// ZPRACOVANI MATRIX UDALOSTI
// ============================================================================

/**
 * @brief Zpracuj matrix udalosti (tahy)
 *
 * Cte udalosti z matrix_event_queue a zpracovava detekci tahu.
 */
void game_process_matrix_events(void);

/**
 * @brief Zvyrazni vsechny pohyblive figurky aktualniho hrace
 *
 * Zobrazi jemnou zlutou animaci na vsech figurkach ktere se mohou hybat.
 */
void game_highlight_movable_pieces(void);

/**
 * @brief Detekuj zda jsou figurky ve vychozi pozici
 *
 * Kontroluje zda jsou figurky rozlozeny v radcich 1, 2, 7, 8 (startovni
 * pozice).
 *
 * @return true pokud jsou figurky ve vychozich pozicich
 */
bool game_detect_new_game_setup(void);

// ============================================================================
// CASTLING ANIMATION SYSTEM
// ============================================================================

/**
 * @brief Overi zda je aktivni animace rosady
 *
 * @return true pokud ceka na tah vezeanimace rosady
 */
bool game_is_castle_animation_active(void);

/**
 * @brief Overi zda se ocekava rosada
 *
 * @return true pokud je kral zvednuti a chysta se rosada
 */
bool game_is_castling_expected(void);

/**
 * @brief Dokonci animaci rosady pri tahu vezeм
 *
 * @param from_row Zdrojovy radek veze (0-7)
 * @param from_col Zdrojovy sloupec veze (0-7)
 * @param to_row Cilovy radek veze (0-7)
 * @param to_col Cilovy sloupec veze (0-7)
 * @return true pokud byla rosada uspesne dokoncena
 */
bool game_complete_castle_animation(uint8_t from_row, uint8_t from_col,
                                    uint8_t to_row, uint8_t to_col);

/**
 * @brief Spust opakovani animace veze pro rosadu
 */
void game_start_repeating_rook_animation(void);

/**
 * @brief Zastav opakovanou animaci veze
 */
void game_stop_repeating_rook_animation(void);

/**
 * @brief Zpracuj DROP prikaz (DN)
 *
 * @param cmd Drop prikaz s pozici
 */
void game_process_drop_command(const chess_move_command_t *cmd);

/**
 * @brief Timer callback pro opakovanou animaci veze
 *
 * @param xTimer Handle timeru
 */
void rook_animation_timer_callback(TimerHandle_t xTimer);

// ============================================================================
// ROZSIRENY SMART ERROR HANDLING
// ============================================================================

/**
 * @brief Zvyrazni neplatnou cilovou oblast cervenou LED
 *
 * @param row Radek neplatneho cile (0-7)
 * @param col Sloupec neplatneho cile (0-7)
 */
void game_highlight_invalid_target_area(uint8_t row, uint8_t col);

/**
 * @brief Zvyrazni platne tahy pro specifickou figurku
 *
 * @param row Radek figurky (0-7)
 * @param col Sloupec figurky (0-7)
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col);

// ============================================================================
// ENHANCED CASTLING SYSTEM FUNKCE
// ============================================================================

/**
 * @brief Zobraz LED navod pro tah veze pri rosade
 */
void game_show_castling_rook_guidance();

/**
 * @brief Zobraz animaci dokonceni rosady
 */
void show_castling_completion_animation();

/**
 * @brief Zobraz blikajici cervenou LED pro chybu neplatneho tahu
 *
 * @param error_row Radek neplatne pozice (0-7)
 * @param error_col Sloupec neplatne pozice (0-7)
 */
void game_show_invalid_move_error_with_blink(uint8_t error_row,
                                             uint8_t error_col);

// ============================================================================
// INTEGRACE TIMER SYSTEMU
// ============================================================================

/**
 * @brief Exportuj stav timeru do JSON retezce
 *
 * @param[out] buffer Vystupni buffer pro JSON
 * @param size Velikost bufferu
 * @return ESP_OK pri uspechu
 */
esp_err_t game_get_timer_json(char *buffer, size_t size);

/**
 * @brief Spust timer pro tah aktualniho hrace
 *
 * @param is_white_turn Je na tahu bily?
 * @return ESP_OK pri uspechu
 */
esp_err_t game_start_timer_move(bool is_white_turn);

/**
 * @brief Ukonci timer pro tah
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_end_timer_move(void);

/**
 * @brief Pozastav herni timer
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_pause_timer(void);

/**
 * @brief Obnov herni timer
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_resume_timer(void);

/**
 * @brief Resetuj herni timer
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_reset_timer(void);

/**
 * @brief Ziskej zbyvajici cas hrace
 *
 * @param is_white_turn Je na tahu bily?
 * @return Zbyvajici cas v milisekundach
 */
uint32_t game_get_remaining_time(bool is_white_turn);

/**
 * @brief Inicializuj timer system v game tasku
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_init_timer_system(void);

/**
 * @brief Zpracuj timer prikazy z fronty
 *
 * @param cmd Game prikaz s timer operaci
 * @return ESP_OK pri uspechu
 */
esp_err_t game_process_timer_command(const chess_move_command_t *cmd);

/**
 * @brief Zpracuj vyprseni casu
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_handle_time_expiration(void);

/**
 * @brief Aktualizuj zobrazeni timeru a overi varovani
 *
 * @return ESP_OK pri uspechu
 */
esp_err_t game_update_timer_display(void);

/**
 * @brief Overi zda je timer aktivni
 *
 * @return true pokud je timer aktivni
 */
bool game_is_timer_active(void);

/**
 * @brief Zjisti zda je pro hrace dostupna promoce
 *
 * @param player Hrac (PLAYER_WHITE nebo PLAYER_BLACK)
 * @return true pokud promoce ceka na dokonceni pro daneho hrace
 */
bool game_is_promotion_available(player_t player);

#ifdef __cplusplus
}
#endif

#endif // GAME_TASK_H
