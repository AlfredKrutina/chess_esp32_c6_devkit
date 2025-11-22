/**
 * @file unified_animation_manager.h
 * @brief Unified Animation Manager - Jednotna sprava vsech LED animaci
 * 
 * Tento modul poskytuje jednoduche rozhrani pro vsechny LED animace:
 * - Centraliz ovana sprava animaci
 * - Prioritni system pro konfliktni animace
 * - Jednoduche API pro rychle pouziti
 * - Podpora pro vice soucasne bezicich animaci
 * - Plynule prechody mezi animacemi
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Unified Animation Manager poskytuje centralizovany system pro spravu
 * vsech LED animaci v systemu. Poskytuje prioritni system, podpora pro
 * vice soucasne bezicich animaci a plynule prechody.
 * 
 * Vyhody:
 * - Jednoduche API: animation_start(ANIM_MOVE, from, to, duration)
 * - Automaticke conflict resolution s prioritami
 * - Podpora pro stackovani animaci (napr. check + move)
 * - Plynule fade-in/fade-out prechody
 * - Thread-safe pristup
 */

#ifndef UNIFIED_ANIMATION_MANAGER_H
#define UNIFIED_ANIMATION_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos_chess.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PRIORITY ANIMACI
// ============================================================================

/**
 * @brief Priority levels pro animace
 * 
 * Vyssi cislo = vyssi priorita, animace prerusuji nizsi priority.
 */
typedef enum {
    ANIM_PRIORITY_BACKGROUND = 0,    ///< Pozadi / screen saver (nejnizsi)
    ANIM_PRIORITY_LOW = 10,          ///< Nizka priorita
    ANIM_PRIORITY_MEDIUM = 20,       ///< Stredni priorita
    ANIM_PRIORITY_HIGH = 30,         ///< Vysoka priorita
    ANIM_PRIORITY_CRITICAL = 50,     ///< Kriticke animace (nejvetsi priorita)
    // Aliases pro kompatibilitu
    ANIM_PRIORITY_AMBIENT = 10,      ///< Alias pro LOW
    ANIM_PRIORITY_GAME = 20,         ///< Alias pro MEDIUM
    ANIM_PRIORITY_INTERACTION = 30,  ///< Alias pro HIGH
    ANIM_PRIORITY_ALERT = 40         ///< Varovani / chyby
} animation_priority_t;

// ============================================================================
// TYPY ANIMACI
// ============================================================================

/**
 * @brief Typy animaci v unified systemu
 */
typedef enum {
    // Zakladni herni animace (PRIORITY_GAME)
    ANIM_TYPE_MOVE_PATH = 0,     ///< Animace cesty tahu
    ANIM_TYPE_PIECE_GUIDANCE,    ///< Navod pro figurku
    ANIM_TYPE_VALID_MOVES,       ///< Zobrazeni platnych tahu
    ANIM_TYPE_ERROR_FLASH,       ///< Chybove bliknuti
    ANIM_TYPE_CAPTURE_EFFECT,    ///< Efekt sebrani
    ANIM_TYPE_CHECK_WARNING,     ///< Varovani sachu
    ANIM_TYPE_GAME_END,          ///< Konec hry
    ANIM_TYPE_PLAYER_CHANGE,     ///< Zmena hrace
    ANIM_TYPE_CASTLE,            ///< Rosada
    ANIM_TYPE_PROMOTION,         ///< Promoce
    ANIM_TYPE_CONFIRMATION,      ///< Potvrzeni
    
    // Endgame animace
    ANIM_TYPE_ENDGAME_WAVE,      ///< Vlna vitezstvi
    ANIM_TYPE_ENDGAME_CIRCLES,   ///< Expandujici kruhy
    ANIM_TYPE_ENDGAME_CASCADE,   ///< Kaskadove padani
    ANIM_TYPE_ENDGAME_FIREWORKS, ///< Ohnostroj
    ANIM_TYPE_ENDGAME_DRAW_SPIRAL, ///< Spirala pro remi
    ANIM_TYPE_ENDGAME_DRAW_PULSE,  ///< Pulzovani pro remi
    
    ANIM_TYPE_COUNT          ///< Pocet typu animaci
} animation_type_t;

// ============================================================================
// KONFIGURACNI STRUKTURY
// ============================================================================

/**
 * @brief Konfigurace animation manageru
 */
typedef struct {
    uint8_t max_concurrent_animations;    ///< Maximalni pocet soucasne bezicich animaci
    uint8_t update_frequency_hz;          ///< Frekvence aktualizaci v Hz
    bool enable_smooth_interpolation;     ///< Povolit plynule interpolace
    bool enable_trail_effects;            ///< Povolit sledujici efekty
    uint32_t default_duration_ms;         ///< Vychozi delka animace v ms
} animation_config_t;

// ============================================================================
// STRUKTURA ANIMACE
// ============================================================================

// Forward declaration pro kruhovou referenci v update_func
typedef struct animation_state_struct animation_state_t;

/**
 * @brief Struktura animace
 */
struct animation_state_struct {
    uint32_t id;                       ///< Unikatni ID animace
    animation_type_t type;             ///< Typ animace
    animation_priority_t priority;     ///< Priorita
    bool active;                       ///< Je aktivni?
    bool looping;                      ///< Opakuje se?
    uint32_t start_time;               ///< Cas spusteni
    uint32_t duration_ms;              ///< Delka v ms (0 = nekonecna)
    uint32_t current_frame;            ///< Aktualni snimek
    float progress;                    ///< Pokrok animace (0.0-1.0)
    
    // LED pozice
    uint8_t from_led;                  ///< Zdrojova LED
    uint8_t to_led;                    ///< Cilova LED
    uint8_t center_led;                ///< Stredova LED (pro endgame)
    uint8_t trail_length;              ///< Delka sledujiciho efektu
    
    // Pozice a parametry (legacy)
    uint8_t from_row, from_col;        ///< Zdrojova pozice
    uint8_t to_row, to_col;            ///< Cilova pozice
    uint8_t affected_positions[64];    ///< Ovlivnene pozice
    uint8_t affected_count;            ///< Pocet ovlivnenych pozic
    
    // Barvy (inline struktury)
    struct { uint8_t r, g, b; } color_start;     ///< Pocatecni barva RGB
    struct { uint8_t r, g, b; } color_end;       ///< Koncova barva RGB
    struct { uint8_t r, g, b; } color_primary;   ///< Primarni barva RGB (legacy)
    struct { uint8_t r, g, b; } color_secondary; ///< Sekundarni barva RGB (legacy)
    
    // Animacni parametry
    uint8_t speed;                     ///< Rychlost (0-255)
    uint8_t intensity;                 ///< Intenzita (0-255)
    uint8_t winner_color;              ///< Barva viteze (0=white, 1=black)
    
    // Update funkce  
    bool (*update_func)(animation_state_t* anim); ///< Update funkce
    
    // Callbacks
    void (*on_complete)(uint32_t id);  ///< Callback pri dokonceni
    void (*on_frame)(uint32_t id, uint32_t frame); ///< Callback pro snimek
};
// typedef uz byl v forward declaration

// ============================================================================
// INICIALIZACE A ZAKLADNI OVLADANI
// ============================================================================

/**
 * @brief Inicializuj unified animation manager
 * 
 * @param config Konfigurace manageru
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_manager_init(const animation_config_t* config);

/**
 * @brief Deinicializuj unified animation manager
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_manager_deinit(void);

/**
 * @brief Vytvor novou animaci (alokuj slot)
 * 
 * @param type Typ animace
 * @param priority Priorita animace
 * @return ID animace nebo 0 pri chybe
 */
uint32_t unified_animation_create(animation_type_t type, animation_priority_t priority);

/**
 * @brief Zastav animaci
 * 
 * @param anim_id ID animace
 * @return ESP_OK pri uspechu
 */
esp_err_t unified_animation_stop(uint32_t anim_id);

/**
 * @brief Zastav vsechny animace
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t unified_animation_stop_all(void);

/**
 * @brief Spust animaci
 * 
 * @param type Typ animace
 * @param from_row Zdrojovy radek (pokud relevantni)
 * @param from_col Zdrojovy sloupec (pokud relevantni)
 * @param to_row Cilovy radek (pokud relevantni)
 * @param to_col Cilovy sloupec (pokud relevantni)
 * @param duration_ms Delka animace v ms (0 = default)
 * @return ID animace nebo 0 pri selhani
 */
uint32_t animation_start(animation_type_t type, 
                         uint8_t from_row, uint8_t from_col,
                         uint8_t to_row, uint8_t to_col,
                         uint32_t duration_ms);

/**
 * @brief Spust jednoduchou animaci na jednom poli
 * 
 * @param type Typ animace
 * @param row Radek
 * @param col Sloupec
 * @param duration_ms Delka animace
 * @return ID animace
 */
uint32_t animation_start_simple(animation_type_t type, 
                                uint8_t row, uint8_t col,
                                uint32_t duration_ms);

/**
 * @brief Zastav animaci
 * 
 * @param animation_id ID animace k zastaveni
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_stop(uint32_t animation_id);

/**
 * @brief Zastav vsechny animace daneho typu
 * 
 * @param type Typ animaci k zastaveni
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_stop_all_of_type(animation_type_t type);

/**
 * @brief Zastav vsechny animace nizsi nebo rovno priority
 * 
 * @param max_priority Maximalni priorita k zastaveni
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_stop_all_up_to_priority(animation_priority_t max_priority);

/**
 * @brief Zastav vsechny animace
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_stop_all(void);

// ============================================================================
// POKROCILE OVLADANI ANIMACI
// ============================================================================

/**
 * @brief Nastav parametry animace
 * 
 * @param animation_id ID animace
 * @param speed Rychlost (0-255)
 * @param intensity Intenzita (0-255)
 * @param looping Ma se opakovat?
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_set_params(uint32_t animation_id, 
                                uint8_t speed, uint8_t intensity, 
                                bool looping);

/**
 * @brief Nastav barvy animace
 * 
 * @param animation_id ID animace
 * @param r_primary Cervena primarni (0-255)
 * @param g_primary Zelena primarni (0-255)
 * @param b_primary Modra primarni (0-255)
 * @param r_secondary Cervena sekundarni (0-255)
 * @param g_secondary Zelena sekundarni (0-255)
 * @param b_secondary Modra sekundarni (0-255)
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_set_colors(uint32_t animation_id, 
                                uint8_t r_primary, uint8_t g_primary, uint8_t b_primary,
                                uint8_t r_secondary, uint8_t g_secondary, uint8_t b_secondary);

/**
 * @brief Fade out animace
 * 
 * @param animation_id ID animace
 * @param fade_duration_ms Delka fade out v ms
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_fade_out(uint32_t animation_id, uint32_t fade_duration_ms);

/**
 * @brief Nastav callback pro dokonceni
 * 
 * @param animation_id ID animace
 * @param callback Callback funkce
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_set_completion_callback(uint32_t animation_id, 
                                             void (*callback)(uint32_t));

// ============================================================================
// STATUS A DIAGNOSTIKA
// ============================================================================

/**
 * @brief Overi zda je animace aktivni
 * 
 * @param animation_id ID animace
 * @return true pokud je animace aktivni
 */
bool animation_is_active(uint32_t animation_id);

/**
 * @brief Pocet aktivnich animaci
 * 
 * @return Pocet bezicich animaci
 */
uint8_t animation_get_active_count(void);

/**
 * @brief Pocet animaci s danou prioritou
 * 
 * @param priority Priorita
 * @return Pocet animaci
 */
uint8_t animation_get_count_by_priority(animation_priority_t priority);

/**
 * @brief Vypis status animaci
 */
void animation_print_status(void);

/**
 * @brief Aktualizuj vsechny aktivni animace (volat periodicky)
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_update_all(void);

/**
 * @brief Aktualizuj animation manager (alias pro animation_update_all)
 */
void animation_manager_update(void);

// ============================================================================
// ZAKLADNI ANIMACNI FUNKCE
// ============================================================================

/**
 * @brief Spust move animaci
 * 
 * @param anim_id ID animace
 * @param from_led Zdrojova LED
 * @param to_led Cilova LED
 * @param duration_ms Delka animace
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_move(uint32_t anim_id, uint8_t from_led, uint8_t to_led, uint32_t duration_ms);

/**
 * @brief Spust guidance animaci
 * 
 * @param anim_id ID animace
 * @param led_array Pole LED k zvyrazneni
 * @param count Pocet LED
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_guidance(uint32_t anim_id, uint8_t* led_array, uint8_t count);

/**
 * @brief Spust error animaci
 * 
 * @param anim_id ID animace
 * @param led_index LED pozice
 * @param flash_count Pocet bliknuti
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_error(uint32_t anim_id, uint8_t led_index, uint32_t flash_count);

/**
 * @brief Spust promotion animaci
 * 
 * @param anim_id ID animace
 * @param promotion_led LED pozice promoce
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_promotion(uint32_t anim_id, uint8_t promotion_led);

// ============================================================================
// ENDGAME ANIMACNI FUNKCE
// ============================================================================

/**
 * @brief Spust endgame wave animaci
 * 
 * @param anim_id ID animace (ziskane z unified_animation_create)
 * @param center_led Stredova LED pozice (kral)
 * @param winner_color Barva viteze (0=white, 1=black)
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_wave(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Spust endgame circles animaci
 * 
 * @param anim_id ID animace
 * @param center_led Stredova LED pozice
 * @param winner_color Barva viteze
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_circles(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Spust endgame cascade animaci
 * 
 * @param anim_id ID animace
 * @param center_led Stredova LED pozice
 * @param winner_color Barva viteze
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_cascade(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Spust endgame fireworks animaci
 * 
 * @param anim_id ID animace
 * @param center_led Stredova LED pozice
 * @param winner_color Barva viteze
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_fireworks(uint32_t anim_id, uint8_t center_led, uint8_t winner_color);

/**
 * @brief Spust endgame draw spiral animaci (pro remi)
 * 
 * @param anim_id ID animace
 * @param center_led Stredova LED pozice
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_draw_spiral(uint32_t anim_id, uint8_t center_led);

/**
 * @brief Spust endgame draw pulse animaci (pro remi)
 * 
 * @param anim_id ID animace
 * @param center_led Stredova LED pozice
 * @return ESP_OK pri uspechu
 */
esp_err_t animation_start_endgame_draw_pulse(uint32_t anim_id, uint8_t center_led);

#ifdef __cplusplus
}
#endif

#endif // UNIFIED_ANIMATION_MANAGER_H
