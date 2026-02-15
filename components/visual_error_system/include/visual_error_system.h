/**
 * @file visual_error_system.h
 * @brief Visual Error System - Vizualni indikace chyb a navod pro uzivatele
 * 
 * Tento modul poskytuje vizualni system pro zpracovani chyb:
 * - Indikace chyb pomoci LED
 * - Textovy popis chyb
 * - Uzivatelsky navod pro napravu
 * - Prioritni system pro vice chyb soucasne
 * - Integrace s ostatnimi vizualnimi systemy
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Vizualni system pro zpracovani chyb poskytuje komplexni system pro zobrazovani
 * chyb uzivateli. Kombinuje LED indikace s textovym popisem
 * a poskytuje jasny navod jak chybu opravit.
 * 
 * Vyhody:
 * - Jasne LED indikace chyb
 * - Preklady chyb do cestiny
 * - Krok-po-kroku navod pro opravu
 * - Automaticke zmizeni po oprave
 * - Podpora pro vice chyb soucasne
 */

#ifndef VISUAL_ERROR_SYSTEM_H
#define VISUAL_ERROR_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// TYPY VIZUALNICH CHYB
// ============================================================================

/**
 * @brief Typy vizualnich chyb
 */
typedef enum {
    // Hernichyby
    ERROR_VISUAL_INVALID_MOVE = 0,     ///< Neplatny tah
    ERROR_VISUAL_WRONG_TURN,           ///< Spatny hrac na tahu
    ERROR_VISUAL_NO_PIECE,             ///< Figurka nenalezena
    ERROR_VISUAL_PIECE_BLOCKING,       ///< Figurka blokovana
    ERROR_VISUAL_CHECK_VIOLATION,      ///< Sach nereseny
    ERROR_VISUAL_SYSTEM_ERROR,         ///< Systemova chyba
    ERROR_VISUAL_INVALID_SYNTAX,       ///< Neplatna syntaxe
    
    ERROR_VISUAL_COUNT                 ///< Pocet typu chyb
} visual_error_type_t;

// ============================================================================
// STRUKTURA ERROR GUIDANCE
// ============================================================================

/**
 * @brief Struktura pro navod k chybe
 */
typedef struct {
    visual_error_type_t error_type;    ///< Typ chyby
    uint8_t error_led_positions[16];   ///< LED pozice chyby
    uint8_t error_led_count;           ///< Pocet error LED
    uint8_t guidance_led_positions[16];///< LED pozice pro navod
    uint8_t guidance_led_count;        ///< Pocet guidance LED
    char user_message[128];            ///< Zprava pro uzivatele
    char recovery_hint[128];           ///< Napoveda pro opravu
    uint8_t led_positions[16];         ///< LED pozice k zvyrazneni (legacy)
    uint8_t led_count;                 ///< Pocet LED k zvyrazneni (legacy)
    uint32_t display_duration_ms;      ///< Jak dlouho zobrazit (ms)
    bool require_user_confirm;         ///< Vyzaduje potvrzeni?
} error_guidance_t;

/**
 * @brief Konfigurace error systemu
 */
typedef struct {
    uint8_t flash_count;               ///< Pocet bliknuti
    uint32_t flash_duration_ms;        ///< Delka bliknuti (ms)
    uint32_t guidance_duration_ms;     ///< Delka navodu (ms)
    bool enable_recovery_hints;        ///< Povolit napovedy
    uint8_t max_concurrent_errors;     ///< Max soucasnych chyb
} error_system_config_t;

/**
 * @brief Struktura aktivniho erroru
 */
typedef struct {
    visual_error_type_t type;          ///< Typ chyby
    uint32_t id;                       ///< Unikatni ID instance
    bool active;                       ///< Je aktivni?
    uint32_t start_time;               ///< Cas zobrazeni
    uint32_t animation_id;             ///< ID LED animace
    uint8_t row, col;                  ///< Pozice chyby (pokud relevantni)
    bool user_confirmed;               ///< Potvrzeno uzivatelem?
} active_error_t;

// ============================================================================
// INICIALIZACE A ZAKLADNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuj visual error system
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t visual_error_init(void);

/**
 * @brief Inicializuj error system s konfiguraci
 * 
 * @param config Konfigurace error systemu
 * @return ESP_OK pri uspechu
 */
esp_err_t error_system_init(const error_system_config_t* config);

/**
 * @brief Deinicializuj error system
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t error_system_deinit(void);

/**
 * @brief Zobraz vizualni chybu
 * 
 * @param error_type Typ chyby
 * @param row Radek chyby (pokud relevantni, jinak 0xFF)
 * @param col Sloupec chyby (pokud relevantni, jinak 0xFF)
 * @return ID chyby nebo 0 pri selhani
 */
uint32_t visual_error_show(visual_error_type_t error_type, 
                           uint8_t row, uint8_t col);

/**
 * @brief Vymaz vizualni chybu
 * 
 * @param error_id ID chyby k vymazani
 * @return ESP_OK pri uspechu
 */
esp_err_t visual_error_clear(uint32_t error_id);

/**
 * @brief Vymaz vsechny vizualni chyby
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t visual_error_clear_all(void);

/**
 * @brief Vymaz vsechny error indikatory
 * @return ESP_OK pri uspechu
 */
esp_err_t error_clear_all_indicators(void);

/**
 * @brief Potvrd chybu (pro chyby vyzadujici potvrzeni)
 * 
 * @param error_id ID chyby
 * @return ESP_OK pri uspechu
 */
esp_err_t visual_error_confirm(uint32_t error_id);

// ============================================================================
// FUNKCE PRO UZIVATELSKY NAVOD
// ============================================================================

/**
 * @brief Zobraz uzivatelsky navod k chybe
 * 
 * @param error_type Typ chyby
 */
void visual_error_show_guidance(visual_error_type_t error_type);

/**
 * @brief Vypis detailni chybovy text
 * 
 * @param error_type Typ chyby
 */
void visual_error_print_details(visual_error_type_t error_type);

/**
 * @brief Zvyrazni problematicke pozice na sachovnici
 * 
 * @param error_type Typ chyby
 * @param positions Pole pozic k zvyrazneni
 * @param count Pocet pozic
 */
void visual_error_highlight_positions(visual_error_type_t error_type,
                                      const uint8_t* positions, 
                                      uint8_t count);

// ============================================================================
// SPECIFICKE CHYBOVE SITUACE
// ============================================================================

/**
 * @brief Zobraz chybu neplatneho tahu
 * 
 * @param from_row Zdrojovy radek
 * @param from_col Zdrojovy sloupec
 * @param to_row Cilovy radek
 * @param to_col Cilovy sloupec
 * @return ID chyby
 */
uint32_t visual_error_illegal_move(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, uint8_t to_col);

/**
 * @brief Zobraz chybu spatneho hrace na tahu
 * 
 * @param row Radek
 * @param col Sloupec
 * @return ID chyby
 */
uint32_t visual_error_wrong_turn(uint8_t row, uint8_t col);

/**
 * @brief Zobraz chybu blokovane figurky
 * 
 * @param row Radek
 * @param col Sloupec
 * @param blocked_positions Blokovane pozice
 * @param count Pocet blokovanych pozic
 * @return ID chyby
 */
uint32_t visual_error_piece_blocked(uint8_t row, uint8_t col,
                                    const uint8_t* blocked_positions,
                                    uint8_t count);

/**
 * @brief Zobraz chybu nereseneho sachu
 * 
 * @param king_row Radek krale
 * @param king_col Sloupec krale
 * @param threat_positions Pozice ohrozeni
 * @param threat_count Pocet pozic ohrozeni
 * @return ID chyby
 */
uint32_t visual_error_check_not_resolved(uint8_t king_row, uint8_t king_col,
                                         const uint8_t* threat_positions,
                                         uint8_t threat_count);

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Overi zda je error aktivni
 * 
 * @param error_id ID chyby
 * @return true pokud je error aktivni
 */
bool visual_error_is_active(uint32_t error_id);

/**
 * @brief Pocet aktivnich erroru
 * 
 * @return Pocet aktivnich chyb
 */
uint8_t visual_error_get_active_count(void);

/**
 * @brief Vypis status erroru
 */
void visual_error_print_status(void);

/**
 * @brief Nastav konfiguraci error systemu
 * 
 * @param config Nova konfigurace
 * @return ESP_OK pri uspechu
 */
esp_err_t error_set_config(const error_system_config_t* config);

/**
 * @brief Nastav pocet bliknuti
 * 
 * @param count Pocet bliknuti
 * @return ESP_OK pri uspechu
 */
esp_err_t error_set_flash_count(uint8_t count);

/**
 * @brief Nastav delku bliknuti
 * 
 * @param duration_ms Delka v ms
 * @return ESP_OK pri uspechu
 */
esp_err_t error_set_flash_duration(uint32_t duration_ms);

/**
 * @brief Ziskej LED pozice pro tah
 * 
 * @param move Tah (chess_move_t*)
 * @param led_positions Vystup LED pozic
 * @param count Vystup pocet LED
 * @return ESP_OK pri uspechu
 */
esp_err_t error_get_led_positions_for_move(const chess_move_t* move, uint8_t* led_positions, uint8_t* count);

/**
 * @brief Ziskej LED pozice pro pole
 * 
 * @param row Radek
 * @param col Sloupec
 * @param led_positions Vystup LED pozic
 * @param count Vystup pocet LED
 * @return ESP_OK pri uspechu
 */
esp_err_t error_get_led_positions_for_square(uint8_t row, uint8_t col, uint8_t* led_positions, uint8_t* count);

/**
 * @brief Ziskej status error systemu
 * 
 * @param buffer Buffer pro status
 * @param buffer_size Velikost bufferu
 * @return ESP_OK pri uspechu
 */
esp_err_t error_get_status(char* buffer, size_t buffer_size);

/**
 * @brief Zobraz vizualni chybu (interni)
 * 
 * @param error_type Typ chyby
 * @param failed_move Chybny tah (chess_move_t*)
 * @return ESP_OK pri uspechu
 */
esp_err_t error_show_visual(visual_error_type_t error_type, const chess_move_t* failed_move);

/**
 * @brief Zobraz blokovani cesty
 * 
 * @param from_row Od radek
 * @param from_col Od sloupec
 * @param to_row Do radek
 * @param to_col Do sloupec
 * @return ESP_OK pri uspechu
 */
esp_err_t error_show_blocking_path(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Zobraz moznosti unikuti ze sachu
 * 
 * @param king_row Radek krale
 * @param king_col Sloupec krale
 * @return ESP_OK pri uspechu
 */
esp_err_t error_show_check_escape_options(uint8_t king_row, uint8_t king_col);

/**
 * @brief Navod na spravny tah
 * 
 * @param correct_move Spravny tah (chess_move_t*)
 * @return ESP_OK pri uspechu
 */
esp_err_t error_guide_correct_move(const chess_move_t* correct_move);

/**
 * @brief Navod na vyber figurky
 * 
 * @param movable_pieces Tahnouci figurky
 * @param count Pocet
 * @return ESP_OK pri uspechu
 */
esp_err_t error_guide_piece_selection(uint8_t* movable_pieces, uint8_t count);

/**
 * @brief Navod na platne cile
 * 
 * @param from_row Od radek
 * @param from_col Od sloupec
 * @return ESP_OK pri uspechu
 */
esp_err_t error_guide_valid_destinations(uint8_t from_row, uint8_t from_col);

/**
 * @brief Ziskej posledni chybovou zpravu
 * 
 * @return Zprava
 */
const char* error_get_last_message(void);

/**
 * @brief Ziskej napovedu k oprave
 * 
 * @return Napoveda
 */
const char* error_get_recovery_hint(void);

/**
 * @brief Preved tah error na vizualni error
 * 
 * @param move_error Tah error
 * @param visual_type Vystup vizualni typ
 * @return ESP_OK pri uspechu
 */
esp_err_t error_convert_move_error_to_visual(move_error_t move_error, visual_error_type_t* visual_type);

/**
 * @brief Aktualizuj errory (volat periodicky)
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t visual_error_update(void);

#ifdef __cplusplus
}
#endif

#endif // VISUAL_ERROR_SYSTEM_H
