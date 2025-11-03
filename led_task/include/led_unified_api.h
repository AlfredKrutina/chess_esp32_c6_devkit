/**
 * @file led_unified_api.h  
 * @brief Unified LED API - Jednotny zdroj pravdy pro LED ovladani
 * 
 * Tato hlavicka poskytuje zjednodusene, jednotne API pro vsechny LED operace.
 * Nahradi chaotickou smes led_set_pixel_internal, led_set_pixel_safe,
 * led_execute_command_new, atd. cistym, konzistentnim rozhranim.
 * 
 * @author Alfred Krutina
 * @version 2.5
 * @date 2025-09-06
 * 
 * @details
 * Unified LED API poskytuje jednoduche, konzistentni rozhrani pro
 * vsechny LED operace v systemu. Eliminuje duplicitni funkce a poskytuje
 * jasne pojmenovane funkce pro kazdy pripad pouziti.
 */

#ifndef LED_UNIFIED_API_H
#define LED_UNIFIED_API_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// JEDNOTNE LED API - JEDINY ZDROJ PRAVDY
// ============================================================================

/**
 * @brief RGB barevna struktura
 */
typedef struct {
    uint8_t r, g, b; ///< Cervena, zelena, modra komponenta (0-255)
} led_color_t;

// Preddefinovane barvy pro konzistenci
/** @brief Barva - vypnuto (cerna) */
#define LED_COLOR_OFF      ((led_color_t){0, 0, 0})
/** @brief Barva - bila */
#define LED_COLOR_WHITE    ((led_color_t){255, 255, 255})
/** @brief Barva - cervena */
#define LED_COLOR_RED      ((led_color_t){255, 0, 0})
/** @brief Barva - zelena */
#define LED_COLOR_GREEN    ((led_color_t){0, 255, 0})
/** @brief Barva - modra */
#define LED_COLOR_BLUE     ((led_color_t){0, 0, 255})
/** @brief Barva - zluta */
#define LED_COLOR_YELLOW   ((led_color_t){255, 255, 0})
/** @brief Barva - azurova (cyan) */
#define LED_COLOR_CYAN     ((led_color_t){0, 255, 255})
/** @brief Barva - fialova (magenta) */
#define LED_COLOR_MAGENTA  ((led_color_t){255, 0, 255})

// ============================================================================
// ZAKLADNI LED FUNKCE - POUZIVEJTE POUZE TYTO
// ============================================================================

/**
 * @brief Nastav LED barvu (nahradi vsechny led_set_pixel_* varianty)
 * 
 * @param position Pozice LED (0-63 sachovnice, 64-72 tlacitka)
 * @param color RGB barva k nastaveni
 * @return ESP_OK pri uspechu
 */
esp_err_t led_set(uint8_t position, led_color_t color);

/**
 * @brief Vymaz jednu LED pozici
 * 
 * @param position Pozice LED (0-72)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear(uint8_t position);

/**
 * @brief Vymaz vsechny LED (sachovnice + tlacitka)
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear_all(void);

/**
 * @brief Vymaz pouze sachovnicove LED (0-63), zachovej tlacitka
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_clear_board(void);

/**
 * @brief Zobraz legalni tahy pro figurku (zelena zvyrazneni)
 * 
 * @param positions Pole LED pozic k zvyrazneni
 * @param count Pocet pozic
 * @return ESP_OK pri uspechu
 */
esp_err_t led_show_moves(const uint8_t* positions, uint8_t count);

/**
 * @brief Zobraz figurky ktere se mohou hybat (zlute zvyrazneni)
 * 
 * @param positions Pole pozic pohyblivych figurek
 * @param count Pocet pohyblivych figurek
 * @return ESP_OK pri uspechu
 */
esp_err_t led_show_movable_pieces(const uint8_t* positions, uint8_t count);

/**
 * @brief Spust animaci (nahradi led_execute_command_new)
 * 
 * @param type Typ animace (retezec jako "move", "check", "endgame")
 * @param from_pos Zdrojova pozice (pokud je relevantni)
 * @param to_pos Cilova pozice (pokud je relevantni)
 * @return ESP_OK pri uspechu
 */
esp_err_t led_animate(const char* type, uint8_t from_pos, uint8_t to_pos);

/**
 * @brief Zastav vsechny animace
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_stop_animations(void);

// ============================================================================
// SACHOVE SPECIFICKE LED FUNKCE
// ============================================================================

/**
 * @brief Zobraz chybovou indikaci (cervene zvyrazneni)
 * 
 * @param position Pozice s chybou
 * @return ESP_OK pri uspechu
 */
esp_err_t led_show_error(uint8_t position);

/**
 * @brief Zobraz vyber figurky (zlute zvyrazneni)
 * 
 * @param position Pozice vybrane figurky
 * @return ESP_OK pri uspechu
 */
esp_err_t led_show_selection(uint8_t position);

/**
 * @brief Zobraz indikaci sebrani (oranzove zvyrazneni)
 * 
 * @param position Pozice sebrani
 * @return ESP_OK pri uspechu
 */
esp_err_t led_show_capture(uint8_t position);

/**
 * @brief Commitni vsechny LED zmeny do hardwaru (batch aktualizace)
 * 
 * @return ESP_OK pri uspechu
 */
esp_err_t led_commit(void);

#ifdef __cplusplus
}
#endif

#endif // LED_UNIFIED_API_H
