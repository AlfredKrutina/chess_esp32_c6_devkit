/**
 * @file game_led_animations.h
 * @brief ESP32-C6 Chess System - Advanced LED Animations Header
 * 
 * Header pro pokročilé LED animace s kompletním API
 * 
 * Author: Alfred Krutina
 * Version: 2.5 - ADVANCED ANIMATIONS
 * Date: 2025-09-04
 */

#ifndef GAME_LED_ANIMATIONS_H
#define GAME_LED_ANIMATIONS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// KONSTANTY A DEFINICE
// ============================================================================

#define MAX_WAVES 5          // Maximální počet vln současně
#define MAX_FIREWORKS 6      // Maximální počet ohňostrojů současně

// ============================================================================
// VÝČTOVÉ TYPY
// ============================================================================

/**
 * @brief Typy endgame animací
 */
typedef enum {
    ENDGAME_ANIM_VICTORY_WAVE = 1,     // Vlna vítězství od krále
    ENDGAME_ANIM_VICTORY_CIRCLES = 2,  // Expandující kruhy
    ENDGAME_ANIM_VICTORY_CASCADE = 3,  // Kaskádové padání
    ENDGAME_ANIM_VICTORY_FIREWORKS = 4,// Ohňostroj
    ENDGAME_ANIM_VICTORY_CROWN = 5,    // Korunka pro vítěze
    ENDGAME_ANIM_MAX                   // Maximální hodnota
} endgame_animation_type_t;

/**
 * @brief Typy jemných animací
 */
typedef enum {
    SUBTLE_ANIM_GENTLE_WAVE = 0,    // Jemná vlna - mírné změny sytosti
    SUBTLE_ANIM_WARM_GLOW = 1,      // Teplé záření - žlutý/oranžový nádech
    SUBTLE_ANIM_COOL_PULSE = 2,     // Chladné pulzování - modrý/fialový nádech
    SUBTLE_ANIM_WHITE_WINS = 3,     // Bílý vítězí - bílá animace
    SUBTLE_ANIM_BLACK_WINS = 4,     // Černý vítězí - černá animace
    SUBTLE_ANIM_DRAW = 5            // Remíza - neutrální animace
} subtle_anim_type_t;

// ============================================================================
// STRUKTURY
// ============================================================================

/**
 * @brief RGB barva
 */
typedef struct {
    uint8_t r;
    uint8_t g; 
    uint8_t b;
} rgb_color_t;

/**
 * @brief Stav jedné vlny
 */
typedef struct {
    float radius;
    bool active;
} wave_t;

/**
 * @brief Stav vlnové animace
 */
typedef struct {
    uint8_t center_pos;      // Pozice středu (vítězný král)
    float max_radius;        // Maximální rádius vlny
    float current_radius;    // Aktuální rádius
    float wave_speed;        // Rychlost vlny
    int active_waves;        // Počet aktivních vln
    wave_t waves[MAX_WAVES]; // Jednotlivé vlny
    uint32_t frame;          // Čítač snímků
} wave_animation_state_t;

/**
 * @brief Stav ohňostroje
 */
typedef struct {
    uint8_t center_x, center_y;  // Střed ohňostroje
    float radius;                // Aktuální rádius
    float max_radius;            // Maximální rádius
    uint8_t color_idx;           // Index barvy
    bool active;                 // Aktivní stav
    int delay;                   // Zpoždění před startem
} firework_t;

/**
 * @brief Stav jemné animace
 */
typedef struct {
    bool active;                 // Aktivní stav
    subtle_anim_type_t type;     // Typ animace
    uint32_t frame;              // Čítač snímků
    rgb_color_t base_color;      // Základní barva
} subtle_animation_state_t;

// ============================================================================
// HLAVNÍ API FUNKCE
// ============================================================================

/**
 * @brief Inicializace animačního systému
 * @return ESP_OK při úspěchu, error kód při selhání
 */
esp_err_t game_led_animations_init(void);

/**
 * @brief Spuštění endgame animace
 * @param animation_type Typ animace (1-5)
 * @param king_position Pozice vítězného krále (0-63)
 * @return ESP_OK při úspěchu, error kód při selhání
 */
esp_err_t start_endgame_animation(endgame_animation_type_t animation_type, uint8_t king_position);

/**
 * @brief Zastavení endgame animace
 * @return ESP_OK při úspěchu
 */
esp_err_t stop_endgame_animation(void);

/**
 * @brief Kontrola zda běží endgame animace
 * @return true pokud běží animace
 */
bool is_endgame_animation_running(void);

/**
 * @brief Získání názvu animace
 * @param animation_type Typ animace
 * @return Název animace jako string
 */
const char* get_endgame_animation_name(endgame_animation_type_t animation_type);

// ============================================================================
// JEMNÉ ANIMACE
// ============================================================================

/**
 * @brief Spuštění jemné animace pro figurku
 * @param piece_position Pozice figurky (0-63)
 * @param anim_type Typ jemné animace
 * @return ESP_OK při úspěchu, error kód při selhání
 */
esp_err_t start_subtle_piece_animation(uint8_t piece_position, subtle_anim_type_t anim_type);

/**
 * @brief Spuštění jemné animace pro tlačítko
 * @param button_id ID tlačítka (0-8)
 * @param anim_type Typ jemné animace
 * @return ESP_OK při úspěchu, error kód při selhání
 */
esp_err_t start_subtle_button_animation(uint8_t button_id, subtle_anim_type_t anim_type);

/**
 * @brief Zastavení všech jemných animací
 * @return ESP_OK při úspěchu
 */
esp_err_t stop_all_subtle_animations(void);

// ============================================================================
// HELPER FUNKCE PRO INTEGRACE
// ============================================================================

/**
 * @brief Aktivace jemných animací pro pohyblivé figurky
 * @param movable_positions Pole pozic pohyblivých figur
 * @param count Počet pozic
 * @return ESP_OK při úspěchu
 */
esp_err_t activate_subtle_animations_for_movable_pieces(uint8_t* movable_positions, uint8_t count);

/**
 * @brief Aktivace jemných animací pro dostupná tlačítka
 * @param available_buttons Pole dostupných tlačítek
 * @param count Počet tlačítek
 * @return ESP_OK při úspěchu
 */
esp_err_t activate_subtle_animations_for_buttons(uint8_t* available_buttons, uint8_t count);

// ============================================================================
// KOMPATIBILNÍ ALIASY PRO EXISTUJÍCÍ KÓD
// ============================================================================

/**
 * @brief Alias pro kompatibilitu s existujícím kódem
 */
#define init_endgame_animation_system() game_led_animations_init()

#ifdef __cplusplus
}
#endif

#endif // GAME_LED_ANIMATIONS_H