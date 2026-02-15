/**
 * @file game_led_animations.h
 * @brief ESP32-C6 Chess System - Advanced LED Animations Header
 * 
 * Header pro pokrocile LED animace s kompletnim API
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

#define MAX_WAVES 5          // Maximalni pocet vln soucasne
#define MAX_FIREWORKS 6      // Maximalni pocet ohnostroju soucasne

// ============================================================================
// VYCETOVE TYPY
// ============================================================================

/**
 * @brief Typy endgame animaci
 */
typedef enum {
    ENDGAME_ANIM_VICTORY_WAVE = 1,     // Vlna vitezstvi od krale
    ENDGAME_ANIM_VICTORY_CIRCLES = 2,  // Expandujici kruhy
    ENDGAME_ANIM_VICTORY_CASCADE = 3,  // Kaskadove padani
    ENDGAME_ANIM_VICTORY_FIREWORKS = 4,// Ohnostroj
    ENDGAME_ANIM_VICTORY_CROWN = 5,    // Korunka pro viteze
    ENDGAME_ANIM_MAX                   // Maximalni hodnota
} endgame_animation_type_t;

/**
 * @brief Typy jemnych animaci
 */
typedef enum {
    SUBTLE_ANIM_GENTLE_WAVE = 0,    // Jemna vlna - mirne zmeny sytosti
    SUBTLE_ANIM_WARM_GLOW = 1,      // Teple zareni - zluty/oranzovy nadech
    SUBTLE_ANIM_COOL_PULSE = 2,     // Chladne pulzovani - modry/fialovy nadech
    SUBTLE_ANIM_WHITE_WINS = 3,     // Bily vitezi - bila animace
    SUBTLE_ANIM_BLACK_WINS = 4,     // Cerny vitezi - cerna animace
    SUBTLE_ANIM_DRAW = 5            // Remiza - neutralni animace
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
 * @brief Stav jedne vlny
 */
typedef struct {
    float radius;
    bool active;
} wave_t;

/**
 * @brief Stav vlnove animace
 */
typedef struct {
    uint8_t center_pos;      // Pozice stredu (vitezny kral)
    float max_radius;        // Maximalni radius vlny
    float current_radius;    // Aktualni radius
    float wave_speed;        // Rychlost vlny
    int active_waves;        // Pocet aktivnich vln
    wave_t waves[MAX_WAVES]; // Jednotlive vlny
    uint32_t frame;          // Citac snimku
} wave_animation_state_t;

/**
 * @brief Stav ohnostroje
 */
typedef struct {
    uint8_t center_x, center_y;  // Stred ohnostroje
    float radius;                // Aktualni radius
    float max_radius;            // Maximalni radius
    uint8_t color_idx;           // Index barvy
    bool active;                 // Aktivni stav
    int delay;                   // Zpozdeni pred startem
} firework_t;

/**
 * @brief Stav jemne animace
 */
typedef struct {
    bool active;                 // Aktivni stav
    subtle_anim_type_t type;     // Typ animace
    uint32_t frame;              // Citac snimku
    rgb_color_t base_color;      // Zakladni barva
} subtle_animation_state_t;

// ============================================================================
// HLAVNÍ API FUNKCE
// ============================================================================

/**
 * @brief Inicializace animacniho systemu
 * @return ESP_OK pri uspechu, error kod pri selhani
 */
esp_err_t game_led_animations_init(void);

/**
 * @brief Spusteni endgame animace
 * @param animation_type Typ animace (1-5)
 * @param king_position Pozice vitezneho krale (0-63)
 * @return ESP_OK pri uspechu, error kod pri selhani
 */
esp_err_t start_endgame_animation(endgame_animation_type_t animation_type, uint8_t king_position);

/**
 * @brief Zastaveni endgame animace
 * @return ESP_OK pri uspechu
 */
esp_err_t stop_endgame_animation(void);

/**
 * @brief Kontrola zda bezi endgame animace
 * @return true pokud bezi animace
 */
bool is_endgame_animation_running(void);

/**
 * @brief Ziskani nazvu animace
 * @param animation_type Typ animace
 * @return Nazev animace jako string
 */
const char* get_endgame_animation_name(endgame_animation_type_t animation_type);

// ============================================================================
// JEMNE ANIMACE
// ============================================================================

/**
 * @brief Spusteni jemne animace pro figurku
 * @param piece_position Pozice figurky (0-63)
 * @param anim_type Typ jemne animace
 * @return ESP_OK pri uspechu, error kod pri selhani
 */
esp_err_t start_subtle_piece_animation(uint8_t piece_position, subtle_anim_type_t anim_type);

/**
 * @brief Spusteni jemne animace pro tlacitko
 * @param button_id ID tlacitka (0-8)
 * @param anim_type Typ jemne animace
 * @return ESP_OK pri uspechu, error kod pri selhani
 */
esp_err_t start_subtle_button_animation(uint8_t button_id, subtle_anim_type_t anim_type);

/**
 * @brief Zastaveni vsech jemnych animaci
 * @return ESP_OK pri uspechu
 */
esp_err_t stop_all_subtle_animations(void);

// ============================================================================
// HELPER FUNKCE PRO INTEGRACI
// ============================================================================

/**
 * @brief Aktivace jemnych animaci pro pohyblive figurky
 * @param movable_positions Pole pozic pohyblivych figur
 * @param count Pocet pozic
 * @return ESP_OK pri uspechu
 */
esp_err_t activate_subtle_animations_for_movable_pieces(uint8_t* movable_positions, uint8_t count);

/**
 * @brief Aktivace jemnych animaci pro dostupna tlacitka
 * @param available_buttons Pole dostupnych tlacitek
 * @param count Pocet tlacitek
 * @return ESP_OK pri uspechu
 */
esp_err_t activate_subtle_animations_for_buttons(uint8_t* available_buttons, uint8_t count);

// ============================================================================
// KOMPATIBILNI ALIASY PRO EXISTUJICI KOD
// ============================================================================

/**
 * @brief Alias pro kompatibilitu s existujicim kodem
 */
#define init_endgame_animation_system() game_led_animations_init()

#ifdef __cplusplus
}
#endif

#endif // GAME_LED_ANIMATIONS_H