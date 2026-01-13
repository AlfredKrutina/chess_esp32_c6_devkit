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
    
    // NOVÉ typy - herní animace
    ANIM_TASK_TYPE_PLAYER_CHANGE,    // Animace změny hráče
    ANIM_TASK_TYPE_MOVE_PATH,        // Animace cesty tahu
    ANIM_TASK_TYPE_CASTLE,           // Animace rošády
    ANIM_TASK_TYPE_PROMOTE,          // Animace promoce (volitelně)
    ANIM_TASK_TYPE_ENDGAME,          // Endgame wave animace (nekonečná)
    ANIM_TASK_TYPE_CHECK,            // Animace šachu
    ANIM_TASK_TYPE_CHECKMATE,        // Animace matu
    
    // Ponechat existující (pro zpětnou kompatibilitu)
    ANIM_TASK_TYPE_MOVE_HIGHLIGHT,   // Zvyrazneni cesty tahu (DEPRECATED - použít MOVE_PATH)
    ANIM_TASK_TYPE_CHECK_HIGHLIGHT,  // Indikator sachu (DEPRECATED - použít CHECK)
    ANIM_TASK_TYPE_GAME_OVER,        // Vzor konce hry (DEPRECATED - použít ENDGAME)
    ANIM_TASK_TYPE_CUSTOM            // Vlastni animace
} animation_task_type_t;

// Typy stavu animaci (prejmenovano pro zabraneni konfliktu s unified_animation_manager)
typedef enum {
    ANIM_TASK_STATE_IDLE = 0,       // Animace je neaktivni
    ANIM_TASK_STATE_RUNNING,        // Animace bezi
    ANIM_TASK_STATE_PAUSED,         // Animace je pozastavena
    ANIM_TASK_STATE_FINISHED        // Animace je dokoncena
} animation_task_state_t;

// ============================================================================
// DATOVÉ STRUKTURY PRO ANIMAČNÍ DATA
// ============================================================================

// Note: player_t and piece_t are defined in chess_types.h (included via freertos_chess.h)

// Struktury pro specifická animační data
typedef struct {
    uint8_t from_led;          // Zdrojová LED pozice
    uint8_t to_led;            // Cílová LED pozice
    uint8_t from_row, from_col; // Zdrojové souřadnice
    uint8_t to_row, to_col;     // Cílové souřadnice
} move_path_data_t;

typedef struct {
    uint8_t player_color;      // 1=white, 0=black (DEPRECATED - použít current_player)
    player_t current_player;   // Aktuální hráč
} player_change_data_t;

typedef struct {
    uint8_t king_from_led;     // Král start pozice
    uint8_t king_to_led;       // Král cíl pozice
    uint8_t rook_from_led;     // Věž start pozice
    uint8_t rook_to_led;       // Věž cíl pozice
} castle_data_t;

typedef struct {
    uint8_t king_led;          // Pozice krále vítěze
    uint8_t king_row, king_col; // Souřadnice krále
    piece_t winner_piece;      // Vítězný král (pro barvu)
    uint8_t radius;            // Aktuální poloměr vlny
    uint32_t last_radius_update; // Čas poslední aktualizace radiusu (v ms)
} endgame_data_t;

typedef struct {
    uint8_t promotion_led;     // Pozice promoce
} promote_data_t;

// Union pro různá animační data
typedef union {
    move_path_data_t move_path;
    player_change_data_t player_change;
    castle_data_t castle;
    endgame_data_t endgame;
    promote_data_t promote;
    void* custom;              // Pro custom animace
} animation_data_union_t;

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
    
    // NOVÉ: Union pro data místo void* data
    animation_data_union_t data_union;
    
    // Ponechat pro zpětnou kompatibilitu
    void* data;                 // DEPRECATED - použít data_union
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
 * @brief Helper funkce pro vytvoření player_change animace
 * 
 * @param player Aktuální hráč (PLAYER_WHITE nebo PLAYER_BLACK)
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_player_change(player_t player, uint32_t duration_ms, uint8_t priority);

/**
 * @brief Helper funkce pro vytvoření move_path animace
 * 
 * @param from_row Zdrojový řádek (0-7)
 * @param from_col Zdrojový sloupec (0-7)
 * @param to_row Cílový řádek (0-7)
 * @param to_col Cílový sloupec (0-7)
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_move_path(uint8_t from_row, uint8_t from_col, 
                                    uint8_t to_row, uint8_t to_col,
                                    uint32_t duration_ms, uint8_t priority);

/**
 * @brief Helper funkce pro vytvoření castle animace
 * 
 * @param king_from_led LED index zdrojové pozice krále
 * @param king_to_led LED index cílové pozice krále
 * @param rook_from_led LED index zdrojové pozice věže
 * @param rook_to_led LED index cílové pozice věže
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_castle(uint8_t king_from_led, uint8_t king_to_led,
                                uint8_t rook_from_led, uint8_t rook_to_led,
                                uint32_t duration_ms, uint8_t priority);

/**
 * @brief Helper funkce pro vytvoření endgame animace
 * 
 * @param king_led LED index pozice krále vítěze
 * @param winner_piece Vítězný král (PIECE_WHITE_KING nebo PIECE_BLACK_KING)
 * @param duration_ms Délka animace v milisekundách (0 = nekonečná)
 * @param priority Priorita animace (0-255)
 * @param loop Zda animaci opakovat (true pro nekonečnou)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_endgame(uint8_t king_led, piece_t winner_piece,
                                 uint32_t duration_ms, uint8_t priority, bool loop);

/**
 * @brief Helper funkce pro vytvoření check animace
 * 
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_check(uint32_t duration_ms, uint8_t priority);

/**
 * @brief Helper funkce pro vytvoření checkmate animace
 * 
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_checkmate(uint32_t duration_ms, uint8_t priority);

/**
 * @brief Helper funkce pro vytvoření promote animace
 * 
 * @param promotion_led LED index pozice promoce
 * @param duration_ms Délka animace v milisekundách
 * @param priority Priorita animace (0-255)
 * @return ID animace nebo 0xFF při selhání
 */
uint8_t animation_create_promote(uint8_t promotion_led, uint32_t duration_ms, uint8_t priority);

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
 * @brief Zastaví všechny animace daného typu
 * 
 * @param type Typ animace k zastavení
 */
void animation_stop_by_type(animation_task_type_t type);

/**
 * @brief Zastaví všechny animace kromě zadaného typu
 * 
 * @param except_type Typ animace, která má zůstat běžet
 */
void animation_stop_all_except(animation_task_type_t except_type);

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