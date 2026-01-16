/**
 * @file animation_task.h
 * @brief ESP32-C6 Chess System v2.4 - Hlavicka Animation Tasku
 * 
 * Tato hlavicka definuje rozhrani pro animation task:
 * - Typy animaci a struktury
 * - Prototypy funkci animation tasku
 * - Funkce pro ovladani animaci a stav
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 */

#ifndef ANIMATION_TASK_H
#define ANIMATION_TASK_H


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




// Typy animaci (prejmenovano pro zabraneni konfliktu s unified_animation_manager)
typedef enum {
    ANIM_TASK_TYPE_WAVE = 0,         // Vlnovy vzor
    ANIM_TASK_TYPE_PULSE,            // Pulzni efekt
    ANIM_TASK_TYPE_FADE,             // Prechod fade
    ANIM_TASK_TYPE_CHESS_PATTERN,    // Sachovnicovy vzor
    ANIM_TASK_TYPE_RAINBOW,          // Duhove barvy
    ANIM_TASK_TYPE_MOVE_HIGHLIGHT,   // Zvyrazneni cesty tahu
    ANIM_TASK_TYPE_CHECK_HIGHLIGHT,  // Indikator sachu
    ANIM_TASK_TYPE_GAME_OVER,        // Vzor konce hry
    ANIM_TASK_TYPE_CUSTOM            // Vlastni animace
} animation_task_type_t;

// Typy stavu animaci (prejmenovano pro zabraneni konfliktu s unified_animation_manager)
typedef enum {
    ANIM_TASK_STATE_IDLE = 0,       // Animace je neaktivni
    ANIM_TASK_STATE_RUNNING,        // Animace bezi
    ANIM_TASK_STATE_PAUSED,         // Animace je pozastavena
    ANIM_TASK_STATE_FINISHED        // Animace je dokoncena
} animation_task_state_t;

// Animation structure
typedef struct {
    uint8_t id;                 // Animation ID
    animation_task_state_t state;    // Current state
    bool active;                // Whether animation is active
    bool paused;                // Whether animation is paused
    bool loop;                  // Whether to loop animation
    uint8_t priority;           // Animation priority (0-255)
    animation_task_type_t type;      // Animation type
    uint32_t start_time;        // Start timestamp
    uint32_t duration_ms;       // Duration in milliseconds
    uint32_t frame_duration_ms; // Frame duration in ms
    uint32_t current_frame;     // Current frame number
    uint32_t total_frames;      // Total frame count
    uint32_t frame_interval;    // Frame interval in ms
    void* data;                 // Animation-specific data
} animation_task_t;


// ============================================================================
// PROTOTYPY FUNKCI TASKU
// ============================================================================


/**
 * @brief Spusti animation task
 * 
 * @param pvParameters Parametry tasku (nepouzivane)
 */
void animation_task_start(void *pvParameters);


// ============================================================================
// INICIALIZACNI FUNKCE ANIMACI
// ============================================================================


/**
 * @brief Inicializuje system animaci
 */
void animation_initialize_system(void);

/**
 * @brief Vytvori novou animaci
 * 
 * @param type Typ animace
 * @param duration_ms Delka v milisekundach
 * @param priority Priorita animace (0-255)
 * @param loop Zda animaci opakovat
 * @return ID animace nebo 0xFF pri selhani
 */
uint8_t animation_create(animation_task_type_t type, uint32_t duration_ms, uint8_t priority, bool loop);

/**
 * @brief Spusti animaci
 * 
 * @param animation_id ID animace ke spusteni
 */
void animation_start(uint8_t animation_id);

/**
 * @brief Zastavi animaci
 * 
 * @param animation_id ID animace k zastaveni
 */
void animation_stop(uint8_t animation_id);

/**
 * @brief Pozastavi animaci
 * 
 * @param animation_id ID animace k pozastaveni
 */
void animation_pause(uint8_t animation_id);

/**
 * @brief Obnovi animaci
 * 
 * @param animation_id ID animace k obnoveni
 */
void animation_resume(uint8_t animation_id);


// ============================================================================
// FUNKCE PRO VZORY ANIMACI
// ============================================================================


/**
 * @brief Generuje snimek vlnove animace
 * 
 * @param frame Cislo snimku
 * @param color Zakladni barva
 * @param speed Rychlost vlny
 */
void animation_generate_wave_frame(uint32_t frame, uint32_t color, uint8_t speed);

/**
 * @brief Generuje snimek pulzni animace
 * 
 * @param frame Cislo snimku
 * @param color Zakladni barva
 * @param speed Rychlost pulzu
 */
void animation_generate_pulse_frame(uint32_t frame, uint32_t color, uint8_t speed);

/**
 * @brief Generuje snimek fade animace
 * 
 * @param frame Cislo snimku
 * @param from_color Zacatecni barva
 * @param to_color Koncova barva
 * @param total_frames Celkovy pocet snimku
 */
void animation_generate_fade_frame(uint32_t frame, uint32_t from_color, uint32_t to_color, uint32_t total_frames);

/**
 * @brief Generuje snimek sachovnicoveho vzoru
 * 
 * @param frame Cislo snimku
 * @param color1 Prvni barva
 * @param color2 Druha barva
 */
void animation_generate_chess_pattern(uint32_t frame, uint32_t color1, uint32_t color2);

/**
 * @brief Generuje snimek duhove animace
 * 
 * @param frame Cislo snimku
 */
void animation_generate_rainbow_frame(uint32_t frame);


// ============================================================================
// FUNKCE PRO VYKONAVANI ANIMACI
// ============================================================================


/**
 * @brief Vykona snimek animace
 * 
 * @param anim Animace k vykonani
 */
void animation_execute_frame(animation_task_t* anim);

/**
 * @brief Posle data snimku do LED
 * 
 * @param frame Pole dat snimku
 */
void animation_send_frame_to_leds(const uint8_t frame[CHESS_LED_COUNT_TOTAL][3]);

/**
 * @brief Vykona animaci zvyrazneni tahu
 * 
 * @param anim Data animace
 */
void animation_execute_move_highlight(animation_task_t* anim);

/**
 * @brief Vykona animaci zvyrazneni sachu
 * 
 * @param anim Data animace
 */
void animation_execute_check_highlight(animation_task_t* anim);

/**
 * @brief Vykona animaci konce hry
 * 
 * @param anim Data animace
 */
void animation_execute_game_over(animation_task_t* anim);


// ============================================================================
// FUNKCE PRO OVLADANI ANIMACI
// ============================================================================


/**
 * @brief Zpracuje prikazy animaci z fronty
 */
void animation_process_commands(void);

/**
 * @brief Zastavi vsechny animace
 */
void animation_stop_all(void);

/**
 * @brief Pozastavi vsechny animace
 */
void animation_pause_all(void);

/**
 * @brief Obnovi vsechny animace
 */
void animation_resume_all(void);

/**
 * @brief Vypise stav animaci
 */
void animation_print_status(void);

/**
 * @brief Otestuje vsechny typy animaci
 */
void animation_test_all(void);

/**
 * @brief Otestuje system animaci
 */
void animation_test_system(void);

// Funkce pro textove zobrazeni animaci
const char* animation_get_name(animation_task_type_t animation_type);
void animation_print_progress(const animation_task_t* anim);
void animation_print_piece_move(const char* from_square, const char* to_square, 
                               const char* piece_name, float progress);
void animation_print_check_status(bool is_checkmate, float progress);
void animation_print_summary(void);

// ============================================================================
// PRIROZENE ANIMACE POHYBU FIGUREK
// ============================================================================

/**
 * @brief Animuje pohyb figurky s prirozenymi pohybovymi vzory
 * 
 * @param from_row Zdrojovy radek
 * @param from_col Zdrojovy sloupec
 * @param to_row Cilovy radek
 * @param to_col Cilovy sloupec
 * @param piece Figurka ktera se pohybuje
 */
void animate_piece_move_natural(uint8_t from_row, uint8_t from_col, 
                               uint8_t to_row, uint8_t to_col, piece_t piece);

/**
 * @brief Pozada o preruseni animace
 */
void animation_request_interrupt();

/**
 * @brief Overi zda byla detekovana nova animace (placeholder)
 * 
 * @return true pokud byla detekovana nova animace
 */
bool new_move_detected();

#endif // ANIMATION_TASK_H