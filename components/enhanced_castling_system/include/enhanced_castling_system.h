/**
 * @file enhanced_castling_system.h
 * @brief Enhanced Castling System pro ESP32 Sachovou Sachovnici
 * 
 * Tento modul poskytuje komplexni, robustni system pro rosadu s:
 * - Centralizovanou spravou stavu
 * - Pokrocilym LED navodem a indikaci chyb
 * - Inteligentnim error recovery
 * - Timeout handlingem
 * - Vizualnim navodem pro hrace
 * 
 * @author ESP32 Chess Team
 * @date 2024
 * 
 * @details
 * Enhanced Castling System poskytuje pokrocily system pro spravu rosady.
 * Pomaha hracum s provedenim rosady pomoci LED navodu, detekuje chyby
 * a umoznuje automatickou napravu.
 */

#ifndef ENHANCED_CASTLING_SYSTEM_H
#define ENHANCED_CASTLING_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"
#include "game_led_animations.h"

#ifdef __cplusplus
extern "C" {
#endif

// Predni deklarace - tyto typy jsou definovany v freertos_chess.h
// player_t a rgb_color_t jsou jiz definovany v freertos_chess.h

// ============================================================================
// ENUMERACE FAZI ROSADY
// ============================================================================

/**
 * @brief Faze rosady
 * 
 * Definuje vsechny faze procesu rosady od zacatku do konce.
 */
typedef enum {
    CASTLING_STATE_IDLE = 0,                    ///< Rosada neprobiha
    CASTLING_STATE_KING_LIFTED,                 ///< Kral zvednuti, ceka na premisteni
    CASTLING_STATE_KING_MOVED_WAITING_ROOK,     ///< Kral premisten, ceka na vez
    CASTLING_STATE_ROOK_LIFTED,                 ///< Vez zvednuta, ceka na premisteni
    CASTLING_STATE_COMPLETING,                  ///< Dokoncovani rosady
    CASTLING_STATE_ERROR_RECOVERY,              ///< Chybovy stav, naprava
    CASTLING_STATE_COMPLETED                    ///< Rosada dokoncena
} castling_phase_t;

// ============================================================================
// TYPY CHYB ROSADY
// ============================================================================

/**
 * @brief Typy chyb pri rosade
 */
typedef enum {
    CASTLING_ERROR_NONE = 0,                 ///< Zadna chyba
    CASTLING_ERROR_WRONG_KING_POSITION,      ///< Nespravna pozice krale
    CASTLING_ERROR_WRONG_ROOK_POSITION,      ///< Nespravna pozice veze
    CASTLING_ERROR_TIMEOUT,                  ///< Timeout behem rosady
    CASTLING_ERROR_INVALID_SEQUENCE,         ///< Neplatna sekvence tahu
    CASTLING_ERROR_HARDWARE_FAILURE,         ///< Hardware chyba
    CASTLING_ERROR_GAME_STATE_INVALID,       ///< Neplatny stav hry
    CASTLING_ERROR_MAX_ERRORS_EXCEEDED       ///< Prekrocen maximalni pocet chyb
} castling_error_t;

// ============================================================================
// STRUKTURY PRO POZICE ROSADY
// ============================================================================

/**
 * @brief Struktura pozic pri rosade
 * 
 * Obsahuje zdrojove a cilove pozice krale a veze pri rosade.
 */
typedef struct {
    uint8_t king_from_row, king_from_col; ///< Zdrojova pozice krale
    uint8_t king_to_row, king_to_col;     ///< Cilova pozice krale
    uint8_t rook_from_row, rook_from_col; ///< Zdrojova pozice veze
    uint8_t rook_to_row, rook_to_col;     ///< Cilova pozice veze
} castling_positions_t;

/**
 * @brief LED stav pro rosadu
 */
typedef struct {
    uint32_t king_animation_id;      ///< ID animace krale
    uint32_t rook_animation_id;      ///< ID animace veze
    uint32_t guidance_animation_id;  ///< ID navodove animace
    bool showing_error;              ///< Zobrazen a je chybova indikace?
    bool showing_guidance;           ///< Zobrazeny je navod?
} castling_led_state_t;

/**
 * @brief LED konfigurace pro rosadu
 */
typedef struct {
    // Barvy pro ruzne stavy
    struct {
        rgb_color_t king_highlight;      ///< Zvyrazneni krale (zlata)
        rgb_color_t king_destination;    ///< Cilove pole krale (zelena)
        rgb_color_t rook_highlight;      ///< Zvyrazneni veze (stribrna)
        rgb_color_t rook_destination;    ///< Cilove pole veze (modra)
        rgb_color_t error_indication;    ///< Chybove pole (cervena)
        rgb_color_t path_guidance;       ///< Vedeni cesty (zluta)
    } colors; ///< Paleta barev pro rosadu
    
    // Animacni vzory
    struct {
        uint32_t pulsing_speed;                   ///< Rychlost pulzovani
        uint32_t guidance_speed;                  ///< Rychlost vedoucich animaci
        uint8_t error_flash_count;                ///< Pocet chybovych bliknuti
        uint32_t completion_celebration_duration; ///< Delka oslavne animace
    } timing; ///< Casovani animaci
} castling_led_config_t;

/**
 * @brief Struktura informaci o chybe
 */
typedef struct {
    castling_error_t error_type;         ///< Typ chyby
    char description[128];               ///< Popis chyby
    uint8_t error_led_positions[8];      ///< Pozice pro cervene LED
    uint8_t correction_led_positions[8]; ///< Pozice pro napravne LED
    void (*recovery_action)(void);       ///< Akce pro napravu
} castling_error_info_t;

/**
 * @brief Hlavni struktura enhanced castling systemu
 */
typedef struct {
    // Stav
    castling_phase_t phase;  ///< Aktualni faze rosady
    bool active;             ///< Je rosada aktivni?
    
    // Hrac a typ rosady
    player_t player;         ///< Hrac provadejici rosadu
    bool is_kingside;        ///< Je to mala rosada (kingside)?
    
    // Pozice figurek
    castling_positions_t positions; ///< Pozice krale a veze
    
    // Casovani a error handling
    uint32_t phase_start_time;  ///< Cas startu faze
    uint32_t phase_timeout_ms;  ///< Timeout faze v ms
    uint8_t error_count;        ///< Pocet chyb
    uint8_t max_errors;         ///< Maximalni pocet chyb
    
    // LED animace a indikace
    castling_led_state_t led_state; ///< Stav LED animaci
    
    // Callback pro dokonceni
    void (*completion_callback)(bool success); ///< Callback po dokonceni
} enhanced_castling_system_t;

// ============================================================================
// GLOBALNI PROMENNE
// ============================================================================

/** @brief Globalni instance castling systemu */
extern enhanced_castling_system_t castling_system;

/** @brief LED konfigurace pro rosadu */
extern castling_led_config_t castling_led_config;

// ============================================================================
// HLAVNI API FUNKCE
// ============================================================================

/**
 * @brief Inicializuj enhanced castling system
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_init(void);

/**
 * @brief Spust sekvenci rosady
 * 
 * @param player Hrac provadejici rosadu
 * @param is_kingside True pro malou rosadu, false pro velkou
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_start(player_t player, bool is_kingside);

/**
 * @brief Zpracuj udalost zvednuti krale
 * 
 * @param row Radek krale (0-7)
 * @param col Sloupec krale (0-7)
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_handle_king_lift(uint8_t row, uint8_t col);

/**
 * @brief Zpracuj udalost polozeni krale
 * 
 * @param row Radek krale (0-7)
 * @param col Sloupec krale (0-7)
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_handle_king_drop(uint8_t row, uint8_t col);

/**
 * @brief Zpracuj udalost zvednuti veze
 * 
 * @param row Radek veze (0-7)
 * @param col Sloupec veze (0-7)
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_handle_rook_lift(uint8_t row, uint8_t col);

/**
 * @brief Zpracuj udalost polozeni veze
 * 
 * @param row Radek veze (0-7)
 * @param col Sloupec veze (0-7)
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_handle_rook_drop(uint8_t row, uint8_t col);

/**
 * @brief Zrus aktualni sekvenci rosady
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t enhanced_castling_cancel(void);

/**
 * @brief Overi zda je rosada aktualne aktivni
 * 
 * @return True pokud rosada probiha
 */
bool enhanced_castling_is_active(void);

/**
 * @brief Ziskej aktualni fazi rosady
 * 
 * @return Aktualni faze rosady
 */
castling_phase_t enhanced_castling_get_phase(void);

/**
 * @brief Aktualizuj fazi rosady s timeout handlingem
 * 
 * @param new_phase Nova faze k nastaveni
 */
void enhanced_castling_update_phase(castling_phase_t new_phase);

/**
 * @brief Zpracuj chybu rosady
 * 
 * @param error Typ chyby
 * @param row Radek kde doslo k chybe
 * @param col Sloupec kde doslo k chybe
 */
void enhanced_castling_handle_error(castling_error_t error, uint8_t row, uint8_t col);

// ============================================================================
// LED NAVODOVE FUNKCE
// ============================================================================

/**
 * @brief Zobraz LED navod pro krale
 */
void castling_show_king_guidance(void);

/**
 * @brief Zobraz LED navod pro vez
 */
void castling_show_rook_guidance(void);

/**
 * @brief Zobraz chybovou indikaci
 * 
 * @param error Typ chyby k zobrazeni
 */
void castling_show_error_indication(castling_error_t error);

/**
 * @brief Zobraz oslavu dokonceni rosady
 */
void castling_show_completion_celebration(void);

/**
 * @brief Vymaz vsechny indikace rosady
 */
void castling_clear_all_indications(void);

// ============================================================================
// VALIDACNI FUNKCE
// ============================================================================

/**
 * @brief Validuj tah krale pro rosadu
 * 
 * @param from_row Zdrojovy radek krale
 * @param from_col Zdrojovy sloupec krale
 * @param to_row Cilovy radek krale
 * @param to_col Cilovy sloupec krale
 * @return true pokud je tah krale platny pro rosadu
 */
bool castling_validate_king_move(uint8_t from_row, uint8_t from_col, 
                                uint8_t to_row, uint8_t to_col);

/**
 * @brief Validuj tah veze pro rosadu
 * 
 * @param from_row Zdrojovy radek veze
 * @param from_col Zdrojovy sloupec veze
 * @param to_row Cilovy radek veze
 * @param to_col Cilovy sloupec veze
 * @return true pokud je tah veze platny pro rosadu
 */
bool castling_validate_rook_move(uint8_t from_row, uint8_t from_col,
                                uint8_t to_row, uint8_t to_col);

/**
 * @brief Validuj sekvenci rosady
 * 
 * @return true pokud je sekvence platna
 */
bool castling_validate_sequence(void);

// ============================================================================
// ERROR RECOVERY FUNKCE
// ============================================================================

/**
 * @brief Obnova z chyby nespravne pozice krale
 */
void castling_recover_king_wrong_position(void);

/**
 * @brief Obnova z chyby nespravne pozice veze
 */
void castling_recover_rook_wrong_position(void);

/**
 * @brief Obnova z chyby timeoutu
 */
void castling_recover_timeout_error(void);

/**
 * @brief Zobraz spravne pozice pro rosadu
 */
void castling_show_correct_positions(void);

/**
 * @brief Zobraz tutorial rosady
 */
void castling_show_tutorial(void);

// ============================================================================
// INTERNI UTILITY FUNKCE
// ============================================================================

/**
 * @brief Vypocitej pozice pro rosadu
 * 
 * @param player Hrac provadejici rosadu
 * @param is_kingside True pro malou rosadu, false pro velkou
 */
void castling_calculate_positions(player_t player, bool is_kingside);

/**
 * @brief Overi zda vyprsel timeout
 * 
 * @return true pokud vyprsel timeout
 */
bool castling_is_timeout_expired(void);

/**
 * @brief Resetuj castling system
 */
void castling_reset_system(void);

/**
 * @brief Zaloguj zmenu stavu
 * 
 * @param action Popis akce
 */
void castling_log_state_change(const char* action);

#ifdef __cplusplus
}
#endif

#endif // ENHANCED_CASTLING_SYSTEM_H
