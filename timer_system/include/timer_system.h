/**
 * @file timer_system.h
 * @brief ESP32-C6 Chess System v2.4 - Timer System komponenta
 * 
 * Tato komponenta spravuje časový systém pro sachovou hru:
 * - Ruzne typy casovych kontrol (bullet, blitz, rapid, classical)
 * - Presne mereni casu s ESP32 timer API
 * - Thread-safe operace s casem
 * - Integrace s game taskem
 * - Web API pro ovladani casu
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-01-XX
 * 
 * @details
 * Timer system poskytuje kompletni casovou kontrolu pro sachovou hru.
 * Podporuje standardni casove kontroly a vlastni nastaveni.
 * Vsechny operace jsou thread-safe a optimalizovane pro ESP32.
 */

#ifndef TIMER_SYSTEM_H
#define TIMER_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONSTANTY A TYPY
// ============================================================================

/**
 * @brief Typy casovych kontrol
 */
typedef enum {
    TIME_CONTROL_NONE = 0,           ///< Bez casove kontroly
    TIME_CONTROL_BULLET_1_0,         ///< Bullet 1+0 (1 minuta)
    TIME_CONTROL_BULLET_1_1,         ///< Bullet 1+1 (1 min + 1s increment)
    TIME_CONTROL_BULLET_2_1,         ///< Bullet 2+1 (2 min + 1s increment)
    TIME_CONTROL_BLITZ_3_0,          ///< Blitz 3+0 (3 minuty)
    TIME_CONTROL_BLITZ_3_2,          ///< Blitz 3+2 (3 min + 2s increment)
    TIME_CONTROL_BLITZ_5_0,          ///< Blitz 5+0 (5 minut)
    TIME_CONTROL_BLITZ_5_3,          ///< Blitz 5+3 (5 min + 3s increment)
    TIME_CONTROL_RAPID_10_0,         ///< Rapid 10+0 (10 minut)
    TIME_CONTROL_RAPID_10_5,         ///< Rapid 10+5 (10 min + 5s increment)
    TIME_CONTROL_RAPID_15_10,        ///< Rapid 15+10 (15 min + 10s increment)
    TIME_CONTROL_RAPID_30_0,         ///< Rapid 30+0 (30 minut)
    TIME_CONTROL_CLASSICAL_60_0,     ///< Classical 60+0 (1 hodina)
    TIME_CONTROL_CLASSICAL_90_30,     ///< Classical 90+30 (90 min + 30s increment)
    TIME_CONTROL_CUSTOM,             ///< Vlastni nastaveni
    TIME_CONTROL_MAX
} time_control_type_t;

/**
 * @brief Konfigurace casove kontroly
 */
typedef struct {
    time_control_type_t type;        ///< Typ casove kontroly
    uint32_t initial_time_ms;       ///< Pocatecni cas v milisekundach
    uint32_t increment_ms;          ///< Increment po tahu v milisekundach
    char name[32];                  ///< Nazev casove kontroly
    char description[64];           ///< Popis pro uzivatele
    bool is_fast;                   ///< Je-li rychla hra (< 10 min)
} time_control_config_t;

/**
 * @brief Stav casoveho systemu
 */
typedef struct {
    // Casove udaje
    uint32_t white_time_ms;         ///< Zbyvajici cas bileho
    uint32_t black_time_ms;         ///< Zbyvajici cas cerneho
    uint32_t move_start_time;       ///< Cas zacatku tahu (esp_timer_get_time())
    uint32_t last_move_time;        ///< Cas posledniho tahu
    
    // Stav timeru
    bool timer_running;             ///< Je-li timer aktivni
    bool is_white_turn;             ///< Je-li na tahu bily
    bool game_paused;               ///< Je-li hra pozastavena
    bool time_expired;              ///< Vyprsel-li cas
    
    // Konfigurace
    time_control_config_t config;   ///< Aktualni konfigurace
    
    // Statistiky
    uint32_t total_moves;           ///< Celkovy pocet tahu
    uint32_t avg_move_time_ms;      ///< Prumerny cas na tah
    
    // Upozorneni
    bool warning_30s_shown;          ///< Upozorneni na 30s bylo zobrazeno
    bool warning_10s_shown;          ///< Upozorneni na 10s bylo zobrazeno
    bool warning_5s_shown;           ///< Upozorneni na 5s bylo zobrazeno
} chess_timer_t;

// ============================================================================
// VEREJNE API FUNKCE
// ============================================================================

/**
 * @brief Inicializuje timer system
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_system_init(void);

/**
 * @brief Nastavi casovou kontrolu
 * 
 * @param config Konfigurace casove kontroly
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_set_time_control(const time_control_config_t* config);

/**
 * @brief Spusti timer pro tah
 * 
 * @param is_white_turn Je-li na tahu bily
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_start_move(bool is_white_turn);

/**
 * @brief Ukonci tah a prida increment
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_end_move(void);

/**
 * @brief Pozastavi timer
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_pause(void);

/**
 * @brief Obnovi timer
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_resume(void);

/**
 * @brief Resetuje timer na pocatecni hodnoty
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_reset(void);

/**
 * @brief Kontroluje vyprseni casu
 * 
 * @return true pokud cas vyprsel, false jinak
 */
bool timer_check_timeout(void);

/**
 * @brief Ziska aktualni stav timeru
 * 
 * @param timer_data Ukazatel na strukturu pro data timeru
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_get_state(chess_timer_t* timer_data);

/**
 * @brief Ziska zbývající cas pro hrace
 * 
 * @param is_white_turn Je-li na tahu bily
 * @return Zbyvajici cas v milisekundach
 */
uint32_t timer_get_remaining_time(bool is_white_turn);

/**
 * @brief Ziska konfiguraci casove kontroly podle typu
 * 
 * @param type Typ casove kontroly
 * @param config Ukazatel na strukturu pro konfiguraci
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_get_config_by_type(time_control_type_t type, time_control_config_t* config);

/**
 * @brief Vytvori JSON reprezentaci stavu casoveho systemu
 * 
 * @param buffer Buffer pro JSON data
 * @param buffer_size Velikost bufferu
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_get_json(char* buffer, size_t buffer_size);

/**
 * @brief Ziska pocet dostupnych casovych kontrol
 * 
 * @return Pocet dostupnych casovych kontrol
 */
uint32_t timer_get_available_controls_count(void);

/**
 * @brief Ziska seznam dostupnych casovych kontrol
 * 
 * @param controls Ukazatel na pole konfiguraci
 * @param max_count Maximalni pocet konfiguraci
 * @return Pocet ziskanych konfiguraci
 */
uint32_t timer_get_available_controls(time_control_config_t* controls, uint32_t max_count);

/**
 * @brief Ulozi nastaveni timeru do NVS
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_save_settings(void);

/**
 * @brief Nacte nastaveni timeru z NVS
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_load_settings(void);

/**
 * @brief Ziska prumerny cas na tah
 * 
 * @return Prumerny cas na tah v milisekundach
 */
uint32_t timer_get_average_move_time(void);

/**
 * @brief Ziska celkovy pocet tahu
 * 
 * @return Celkovy pocet tahu
 */
uint32_t timer_get_total_moves(void);

/**
 * @brief Kontroluje zda je casova kontrola aktivni
 * 
 * @return true pokud je casova kontrola aktivni, false jinak
 */
bool timer_is_active(void);

/**
 * @brief Ziska typ aktualni casove kontroly
 * 
 * @return Typ aktualni casove kontroly
 */
time_control_type_t timer_get_current_type(void);

/**
 * @brief Nastavi vlastni casovou kontrolu
 * 
 * @param minutes Pocet minut
 * @param increment_seconds Increment v sekundach
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_set_custom_time_control(uint32_t minutes, uint32_t increment_seconds);

/**
 * @brief Ziska casovou kontrolu podle indexu
 * 
 * @param index Index casove kontroly
 * @param config Ukazatel na strukturu pro konfiguraci
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_get_config_by_index(uint32_t index, time_control_config_t* config);

/**
 * @brief Deinicializuje timer system
 * 
 * @return ESP_OK pri uspechu, chybovy kod pri chybe
 */
esp_err_t timer_system_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // TIMER_SYSTEM_H
