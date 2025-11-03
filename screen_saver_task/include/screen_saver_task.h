/**
 * @file screen_saver_task.h
 * @brief ESP32-C6 Chess System v2.4 - Screen Saver Task Hlavicka
 * 
 * Tato hlavicka definuje rozhrani pro screen saver task:
 * - Typy a struktury screen saveru
 * - Prototypy funkci screen saver tasku
 * - Funkce pro ovladani a stav screen saveru
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Screen Saver Task spravuje usporu energie pri neaktivite.
 * Detekuje kdy uzivatel nehraje a aktivuje energeticky usporne
 * LED vzory. Po detekci aktivity se screen saver automaticky vypne.
 */

#ifndef SCREEN_SAVER_TASK_H
#define SCREEN_SAVER_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "freertos_chess.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// KONSTANTY A DEFINICE
// ============================================================================


/**
 * @brief Typy zdroju aktivity
 */
typedef enum {
    ACTIVITY_MATRIX = 0,  ///< Aktivita matice (pohyb figurek)
    ACTIVITY_BUTTON,      ///< Aktivita tlacitek
    ACTIVITY_LED,         ///< Aktivita LED
    ACTIVITY_UART         ///< Aktivita UART (prikazy)
} activity_source_t;

/**
 * @brief Typy stavu screen saveru
 */
typedef enum {
    SCREEN_SAVER_STATE_ACTIVE = 0,      ///< Screen saver je aktivni
    SCREEN_SAVER_STATE_INACTIVE,        ///< Screen saver je neaktivni
    SCREEN_SAVER_STATE_TRANSITIONING    ///< Prechod mezi stavy
} screen_saver_state_t;

/**
 * @brief Typy vzoru screen saveru
 */
typedef enum {
    PATTERN_FIREWORKS = 0,  ///< Efekt ohnostroje
    PATTERN_STARS,          ///< Blikajici hvezdy
    PATTERN_OCEAN,          ///< Oceanove vlny
    PATTERN_FOREST,         ///< Lesni animace
    PATTERN_CITY,           ///< Mestska svetla
    PATTERN_SPACE,          ///< Vesmirne tema
    PATTERN_GEOMETRIC,      ///< Geometricke vzory
    PATTERN_MINIMAL         ///< Minimalni energeticky vzor
} screen_saver_pattern_t;

/**
 * @brief Struktura screen saveru
 */
typedef struct {
    screen_saver_state_t state;        ///< Aktualni stav
    screen_saver_pattern_t current_pattern; ///< Aktualni vzor
    uint32_t last_activity_time;       ///< Cas posledni aktivity
    uint32_t timeout_ms;               ///< Timeout v ms
    uint32_t pattern_start_time;       ///< Cas startu vzoru
    uint32_t frame_count;              ///< Pocet snimku
    bool enabled;                      ///< Je screen saver povolen?
    uint8_t brightness;                ///< Jas (0-100%)
    uint8_t pattern_speed;             ///< Rychlost vzoru
} screen_saver_t;

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Spusti screen saver task
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void screen_saver_task_start(void *pvParameters);

// ============================================================================
// INICIALIZACNI FUNKCE
// ============================================================================

/**
 * @brief Inicializuj screen saver system
 */
void screen_saver_initialize(void);

/**
 * @brief Nastav timeout screen saveru
 * 
 * @param timeout_ms Timeout v milisekundach (5000-300000)
 */
void screen_saver_set_timeout(uint32_t timeout_ms);

/**
 * @brief Nastav jas screen saveru
 * 
 * @param brightness Jas v procentech (0-100)
 */
void screen_saver_set_brightness(uint8_t brightness);

/**
 * @brief Nastav vzor screen saveru
 * 
 * @param pattern Typ vzoru
 */
void screen_saver_set_pattern(screen_saver_pattern_t pattern);

// ============================================================================
// FUNKCE PRO DETEKCI AKTIVITY
// ============================================================================

/**
 * @brief Aktualizuj casovou znamku aktivity
 * 
 * @param source Zdroj aktivity
 */
void screen_saver_update_activity(activity_source_t source);

/**
 * @brief Overi zda vyprsel timeout
 * 
 * @return true pokud vyprsel timeout a screen saver by mel byt aktivovan
 */
bool screen_saver_check_timeout(void);

// ============================================================================
// SPRAVA STAVU SCREEN SAVERU
// ============================================================================

/**
 * @brief Aktivuj screen saver
 */
void screen_saver_activate(void);

/**
 * @brief Deaktivuj screen saver
 */
void screen_saver_deactivate(void);

/**
 * @brief Fade out displeje pro screen saver
 */
void screen_saver_fade_out(void);

/**
 * @brief Fade in displeje ze screen saveru
 */
void screen_saver_fade_in(void);

/**
 * @brief Nastav globalni jas
 * 
 * @param brightness Jas v procentech (0-100)
 */
void screen_saver_set_global_brightness(uint8_t brightness);

// ============================================================================
// FUNKCE PRO GENEROVANI VZORU
// ============================================================================

/**
 * @brief Generuj screen saver vzor
 */
void screen_saver_generate_pattern(void);

/**
 * @brief Generuj ohnostroj vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_fireworks(uint32_t time);

/**
 * @brief Generuj hvezdny vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_stars(uint32_t time);

/**
 * @brief Generuj oceanovy vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_ocean(uint32_t time);

/**
 * @brief Generuj lesni vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_forest(uint32_t time);

/**
 * @brief Generuj mestsky vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_city(uint32_t time);

/**
 * @brief Generuj vesmirny vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_space(uint32_t time);

/**
 * @brief Generuj geometricky vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_geometric(uint32_t time);

/**
 * @brief Generuj minimalni vzor
 * 
 * @param time Aktualni cas
 */
void screen_saver_generate_minimal(uint32_t time);

/**
 * @brief Aplikuj snizeni jasu na vzor
 */
void screen_saver_apply_brightness(void);

/**
 * @brief Posli vzor do LED
 */
void screen_saver_send_pattern_to_leds(void);

// ============================================================================
// FUNKCE PRO ZPRACOVANI PRIKAZU
// ============================================================================

/**
 * @brief Zpracuj screen saver prikazy z fronty
 */
void screen_saver_process_commands(void);

/**
 * @brief Vypis status screen saveru
 */
void screen_saver_print_status(void);

/**
 * @brief Testuj vsechny screen saver vzory
 */
void screen_saver_test_patterns(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_SAVER_TASK_H
