/**
 * @file chess_types.h
 * @brief ESP32-C6 Chess System v2.4 - Spolecne definice typu
 * 
 * Tato hlavicka obsahuje vsechny spolecne definice typu pouzivane
 * v sachovem systemu. Zabranuje cirkularnim zavislost a poskytuje
 * konzistentni definice typu pres vsechny komponenty.
 * 
 * @author Alfred Krutina
 * @version 2.4
 * @date 2025-08-24
 * 
 * @details
 * Tento soubor definuje vsechny zakladni datove typy pouzivane v sachovem systemu:
 * - Typy sachovych figurek (piece_t)
 * - Stavy hry (game_state_t)
 * - Hraci (player_t)
 * - Typy tahu a chyb (move_error_t)
 * - Struktury pro tahy (chess_move_t, chess_move_extended_t)
 * - Prikazy a odpovedi (game_command_type_t, game_response_t)
 * - LED prikazy a udalosti
 * - Button a matrix udalosti
 * - Konfigurace systemu
 */

#ifndef CHESS_TYPES_H
#define CHESS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DEFINICE SACHOVYCH FIGUREK
// ============================================================================

/**
 * @brief Typy sachovych figurek
 * 
 * Enumerace definujici vsechny typy sachovych figurek pro obe barvy.
 * Hodnota 0 znamena prazdne pole.
 */
typedef enum {
    PIECE_EMPTY = 0,          ///< Prazdne pole (zadna figurka)
    // Bile figurky
    PIECE_WHITE_PAWN = 1,     ///< Bily pesec
    PIECE_WHITE_KNIGHT = 2,   ///< Bily kun
    PIECE_WHITE_BISHOP = 3,   ///< Bily strelec
    PIECE_WHITE_ROOK = 4,     ///< Bila vez
    PIECE_WHITE_QUEEN = 5,    ///< Bila dama
    PIECE_WHITE_KING = 6,     ///< Bily kral
    // Cerne figurky
    PIECE_BLACK_PAWN = 7,     ///< Cerny pesec
    PIECE_BLACK_KNIGHT = 8,   ///< Cerny kun
    PIECE_BLACK_BISHOP = 9,   ///< Cerny strelec
    PIECE_BLACK_ROOK = 10,    ///< Cerna vez
    PIECE_BLACK_QUEEN = 11,   ///< Cerna dama
    PIECE_BLACK_KING = 12     ///< Cerny kral
} piece_t;

// ============================================================================
// DEFINICE STAVU HRY
// ============================================================================

/**
 * @brief Stavy sachove hry
 * 
 * Enumerace definujici vsechny mozne stavy sachove hry od inicializace
 * az po konec hry a error handling.
 */
typedef enum {
    GAME_STATE_IDLE = 0,              ///< Necinny stav (pred inicializaci)
    GAME_STATE_INIT = 1,              ///< Inicializace hry
    GAME_STATE_ACTIVE = 2,            ///< Aktivni hra
    GAME_STATE_PAUSED = 3,            ///< Pozastavena hra
    GAME_STATE_FINISHED = 4,          ///< Ukoncena hra
    GAME_STATE_ERROR = 5,             ///< Chybovy stav
    GAME_STATE_PLAYING = 6,           ///< Probihajici hra
    GAME_STATE_PROMOTION = 7,         ///< Ceka se na vyber promoci pescu
    GAME_STATE_ERROR_RECOVERY = 8,    ///< Obnova po chybe (novy stav)
    GAME_STATE_WAITING_FOR_RETURN = 9 ///< Ceka se na vraceni figurky na misto
} game_state_t;

/**
 * @brief Definice hracu
 * 
 * Enumerace definujici obe barvy hracu v sach.
 */
typedef enum {
    PLAYER_WHITE = 0,  ///< Bily hrac
    PLAYER_BLACK = 1   ///< Cerny hrac
} player_t;

/**
 * @brief Typy vysledku hry pro statistiky
 * 
 * Enumerace definujici vsechny mozne vysledky hry vcetne remizy.
 */
typedef enum {
    RESULT_WHITE_WINS = 0,          ///< Bily vitezi
    RESULT_BLACK_WINS = 1,          ///< Cerny vitezi
    RESULT_DRAW_STALEMATE = 2,      ///< Remiza - pat (stalemate)
    RESULT_DRAW_50_MOVE = 3,        ///< Remiza - pravidlo 50 tahu
    RESULT_DRAW_REPETITION = 4,     ///< Remiza - opakovani pozice
    RESULT_DRAW_INSUFFICIENT = 5    ///< Remiza - nedostatecny material
} game_result_type_t;

/**
 * @brief Typy chyb pri tahu
 * 
 * Enumerace definujici vsechny mozne typy chyb pri provadeni tahu.
 * Pouziva se pro detailni error handling a zobrazeni napovedy hraci.
 */
typedef enum {
    MOVE_ERROR_NONE = 0,                    ///< Zadna chyba
    MOVE_ERROR_INVALID_SYNTAX = 1,          ///< Neplatna syntaxe tahu
    MOVE_ERROR_INVALID_PARAMETER = 2,       ///< Neplatny parameter
    MOVE_ERROR_PIECE_NOT_FOUND = 3,         ///< Figurka nenalezena
    MOVE_ERROR_INVALID_MOVE = 4,            ///< Neplatny tah
    MOVE_ERROR_BLOCKED_PATH = 5,            ///< Blokovana cesta
    MOVE_ERROR_CHECK_VIOLATION = 6,         ///< Tah by nechal krale v sachu
    MOVE_ERROR_SYSTEM_ERROR = 7,            ///< Systemova chyba
    // Dalsi typy chyb potrebne v game_task.c
    MOVE_ERROR_NO_PIECE = 8,                ///< Na zdrojovem poli neni figurka
    MOVE_ERROR_WRONG_COLOR = 9,             ///< Spatna barva figurky (neni na tahu)
    MOVE_ERROR_INVALID_PATTERN = 10,        ///< Neplatny vzor pohybu
    MOVE_ERROR_KING_IN_CHECK = 11,          ///< Kral je v sachu
    MOVE_ERROR_CASTLING_BLOCKED = 12,       ///< Rosada je blokovana
    MOVE_ERROR_EN_PASSANT_INVALID = 13,     ///< Neplatny en passant
    MOVE_ERROR_DESTINATION_OCCUPIED = 14,   ///< Cilove pole je obsazeno vlastni figurkou
    MOVE_ERROR_OUT_OF_BOUNDS = 15,          ///< Souradnice mimo sachovnici
    MOVE_ERROR_GAME_NOT_ACTIVE = 16,        ///< Hra neni aktivni
    MOVE_ERROR_INVALID_MOVE_STRUCTURE = 17, ///< Neplatna struktura tahu
    MOVE_ERROR_INVALID_COORDINATES = 18,    ///< Neplatne souradnice
    MOVE_ERROR_ILLEGAL_MOVE = 19            ///< Nelegalni tah
} move_error_t;

// ============================================================================
// DEFINICE PROMOCI
// ============================================================================

/**
 * @brief Typy volby promoci pescu
 * 
 * Kdyz pesec dosahne konec sachovnice, hrac si muze vybrat
 * na kterou figurku chce pesc promovat.
 */
typedef enum {
    PROMOTION_QUEEN = 0,   ///< Promovat na damu
    PROMOTION_ROOK = 1,    ///< Promovat na vez
    PROMOTION_BISHOP = 2,  ///< Promovat na strelce
    PROMOTION_KNIGHT = 3   ///< Promovat na kone
} promotion_choice_t;

// ============================================================================
// STRUKTURY PRO SACHOVE TAHY
// ============================================================================

/**
 * @brief Zakladni struktura sachoveho tahu
 * 
 * Obsahuje minimalni informace o tahu - zdrojove a cilove souradnice,
 * figurku a pripadnou sebranou figurku.
 */
typedef struct {
    uint8_t from_row;        ///< Zdrojovy radek (0-7)
    uint8_t from_col;        ///< Zdrojovy sloupec (0-7)
    uint8_t to_row;          ///< Cilovy radek (0-7)
    uint8_t to_col;          ///< Cilovy sloupec (0-7)
    piece_t piece;           ///< Figurka ktera se pohybuje
    piece_t captured_piece;  ///< Sebrana figurka (PIECE_EMPTY pokud neni zadna)
    uint32_t timestamp;      ///< Casova znamka tahu (v milisekundach)
} chess_move_t;

/**
 * @brief Typy tahu pro rozsirenou sachovou logiku
 * 
 * Definuje specialni typy tahu v sach (rosada, en passant, promoce).
 */
typedef enum {
    MOVE_TYPE_NORMAL = 0,      ///< Normalni tah
    MOVE_TYPE_CAPTURE = 1,     ///< Sebrani figurky
    MOVE_TYPE_CASTLE_KING = 2, ///< Mala rosada (kingside)
    MOVE_TYPE_CASTLE_QUEEN = 3,///< Velka rosada (queenside)
    MOVE_TYPE_EN_PASSANT = 4,  ///< En passant (sebrani pescu mimochodem)
    MOVE_TYPE_PROMOTION = 5    ///< Promoce pescu
} move_type_t;

/**
 * @brief Rozsirena struktura tahu pro kompletni sachovou logiku
 * 
 * Obsahuje vsechny informace o tahu vcetne specialnich vlastnosti
 * jako je promoce, sach, mat a pat.
 */
typedef struct {
    uint8_t from_row;                 ///< Zdrojovy radek (0-7)
    uint8_t from_col;                 ///< Zdrojovy sloupec (0-7)
    uint8_t to_row;                   ///< Cilovy radek (0-7)
    uint8_t to_col;                   ///< Cilovy sloupec (0-7)
    piece_t piece;                    ///< Figurka ktera se pohybuje
    piece_t captured_piece;           ///< Sebrana figurka
    move_type_t move_type;            ///< Typ tahu (normal, capture, castle, ...)
    promotion_choice_t promotion_piece; ///< Figurka pro promoci (pokud je to promoce)
    uint32_t timestamp;               ///< Casova znamka tahu
    bool is_check;                    ///< Je to sach?
    bool is_checkmate;                ///< Je to mat?
    bool is_stalemate;                ///< Je to pat?
} chess_move_extended_t;

// ============================================================================
// DEFINICE GAME PRIKAZU
// ============================================================================

/**
 * @brief Typy game prikazu pro UART komunikaci
 * 
 * Kompletni seznam vsech prikazu ktere lze poslat game tasku pres frontu.
 * Pouziva se pro ovladani hry, debug, testovani a spravu systemu.
 */
typedef enum {
    GAME_CMD_NEW_GAME = 0,            ///< Nova hra (inicializace sachovnice)
    GAME_CMD_RESET_GAME = 1,          ///< Reset hry (vrati se na zacatek)
    GAME_CMD_MAKE_MOVE = 2,           ///< Proved tah (move e2e4)
    GAME_CMD_UNDO_MOVE = 3,           ///< Vrat zpet posledni tah (undo)
    GAME_CMD_GET_STATUS = 4,          ///< Ziskej stav hry (status)
    GAME_CMD_GET_BOARD = 5,           ///< Ziskej aktualni pozici (board)
    GAME_CMD_GET_VALID_MOVES = 6,     ///< Ziskej platne tahy pro figurku
    GAME_CMD_PICKUP_PIECE = 7,        ///< Zved figurku (pickup)
    GAME_CMD_DROP_PIECE = 8,          ///< Poloz figurku (drop)
    GAME_CMD_PROMOTION = 9,           ///< Promoc pescu (promotion)
    GAME_CMD_MOVE = 10,               ///< Tah (move)
    GAME_CMD_SHOW_BOARD = 11,         ///< Zobraz sachovnici (board)
    GAME_CMD_PICKUP = 12,             ///< UP prikaz - figurka zvednuta
    GAME_CMD_DROP = 13,               ///< DN prikaz - figurka polozena
    GAME_CMD_GET_HISTORY = 14,        ///< Ziskej historii tahu
    GAME_CMD_DEBUG_INFO = 15,         ///< Debug informace o hre
    GAME_CMD_DEBUG_BOARD = 16,        ///< Debug informace o sachovnici
    
    // Vysokoprioritni prikazy
    GAME_CMD_EVALUATE = 17,           ///< Evaluace pozice (vyhodnoceni)
    GAME_CMD_SAVE = 18,               ///< Uloz hru do pameti
    GAME_CMD_LOAD = 19,               ///< Nahraj hru z pameti
    GAME_CMD_PUZZLE = 20,             ///< Sachova uloha (puzzle)
    
    // Stredneprioritni prikazy
    GAME_CMD_CASTLE = 21,             ///< Rosada (castle)
    GAME_CMD_PROMOTE = 22,            ///< Promoce pescu (promote)
    
    // Prikazy pro ovladani komponent
    GAME_CMD_COMPONENT_OFF = 23,      ///< Vypni komponentu (off)
    GAME_CMD_COMPONENT_ON = 24,       ///< Zapni komponentu (on)
    
    // Endgame prikazy
    GAME_CMD_ENDGAME_WHITE = 25,      ///< Konec hry - bily vitezi
    GAME_CMD_ENDGAME_BLACK = 26,      ///< Konec hry - cerny vitezi
    
    // Prikazy pro spravu her
    GAME_CMD_LIST_GAMES = 27,         ///< Vypis ulozenych her
    GAME_CMD_DELETE_GAME = 28,        ///< Smaz ulozenou hru
    
    // Puzzle prikazy
    GAME_CMD_PUZZLE_NEXT = 29,        ///< Dalsi krok puzzle
    GAME_CMD_PUZZLE_RESET = 30,       ///< Reset aktualniho puzzle
    GAME_CMD_PUZZLE_COMPLETE = 31,    ///< Dokonceni aktualniho puzzle
    GAME_CMD_PUZZLE_VERIFY = 32,      ///< Overeni puzzle tahu
    
    // Prikazy pro testovani animaci
    GAME_CMD_TEST_MOVE_ANIM = 33,     ///< Test animace tahu
    GAME_CMD_TEST_PLAYER_ANIM = 34,   ///< Test animace zmeny hrace
    GAME_CMD_TEST_CASTLE_ANIM = 35,   ///< Test animace rosady
    GAME_CMD_TEST_PROMOTE_ANIM = 36,  ///< Test animace promoci
    GAME_CMD_TEST_ENDGAME_ANIM = 37,  ///< Test animace konce hry
    GAME_CMD_TEST_PUZZLE_ANIM = 38,   ///< Test animace puzzle
    
    // Prikazy pro casovy system
    GAME_CMD_SET_TIME_CONTROL = 39,   ///< Nastav casovou kontrolu
    GAME_CMD_PAUSE_TIMER = 40,        ///< Pozastav timer
    GAME_CMD_RESUME_TIMER = 41,       ///< Obnov timer
    GAME_CMD_RESET_TIMER = 42,        ///< Resetuj timer
    GAME_CMD_GET_TIMER_STATE = 43,    ///< Ziskej stav timeru
    GAME_CMD_TIMER_TIMEOUT = 44       ///< Vyprseni casoveho limitu
} game_command_type_t;

/**
 * @brief Struktura prikazu pro sachovy tah pres UART
 * 
 * Tato struktura se pouziva pro poslani prikazu game tasku pres frontu.
 * Obsahuje vsechny potrebne informace pro provedeni tahu a zpracovani odpovedi.
 */
typedef struct {
    uint8_t type;                ///< Typ prikazu (game_command_type_t)
    char from_notation[8];       ///< Zdrojova notace (napr. "e2") - zvetseno z 4 na 8 pro bezpecnost
    char to_notation[8];         ///< Cilova notace (napr. "e4") - zvetseno z 4 na 8 pro bezpecnost
    uint8_t player;              ///< Hrac provadejici prikaz (PLAYER_WHITE nebo PLAYER_BLACK)
    QueueHandle_t response_queue;///< Fronta pro poslani odpovedi
    uint8_t promotion_choice;    ///< Volba promoci (pro promotion prikazy)
    
    // Pole pro timer system
    union {
        struct {
            uint8_t time_control_type;  ///< Typ casove kontroly (pro GAME_CMD_SET_TIME_CONTROL)
            uint32_t custom_minutes;    ///< Vlastni minuty (pro vlastni casovou kontrolu)
            uint32_t custom_increment;  ///< Vlastni increment (pro vlastni casovou kontrolu)
        } timer_config;                 ///< Konfigurace timeru
        struct {
            bool is_white_turn;         ///< Je na tahu bily? (pro timer operace)
        } timer_state;                  ///< Stav timeru
    } timer_data;                       ///< Union pro timer data
} chess_move_command_t;

/**
 * @brief Typy game odpovedi
 * 
 * Definuje typy odpovedi ktere game task posilana zpet pres UART.
 */
typedef enum {
    GAME_RESPONSE_SUCCESS = 0,      ///< Uspesne provedeno
    GAME_RESPONSE_ERROR = 1,        ///< Chyba pri provadeni
    GAME_RESPONSE_BOARD = 2,        ///< Odpoved obsahuje sachovnici
    GAME_RESPONSE_MOVES = 3,        ///< Odpoved obsahuje seznam tahu
    GAME_RESPONSE_STATUS = 4,       ///< Odpoved obsahuje stav hry
    GAME_RESPONSE_HISTORY = 5,      ///< Odpoved obsahuje historii tahu
    GAME_RESPONSE_MOVE_RESULT = 6,  ///< Odpoved obsahuje vysledek tahu
    GAME_RESPONSE_LED_STATUS = 7    ///< Odpoved obsahuje LED status
} game_response_type_t;

/**
 * @brief Struktura game odpovedi pro UART komunikaci
 * 
 * Tato struktura obsahuje odpoved game tasku na prikaz.
 * Je poslana zpet pres UART pro zobrazeni uzivateli.
 */
typedef struct {
    uint8_t type;           ///< Typ odpovedi (game_response_type_t)
    uint8_t command_type;   ///< Puvodni typ prikazu (game_command_type_t)
    uint8_t error_code;     ///< Kod chyby (move_error_t, 0 pokud neni chyba)
    char message[256];      ///< Zprava pro uzivatele (human-readable)
    char data[3584];        ///< Data odpovedi (sachovnice ~3000 znaku, s bezpecnostni rezervou)
    uint32_t timestamp;     ///< Casova znamka odpovedi (v milisekundach)
} game_response_t;

// ============================================================================
// DEFINICE LED SYSTEMU
// ============================================================================

/**
 * @brief Typy LED prikazu
 * 
 * Kompletni seznam vsech prikazu pro ovladani LED systemu.
 * Obsahuje zakladni prikazy, animace, puzzle animace, error handling
 * a pokrocile sachove animace.
 */
typedef enum {
    LED_CMD_SET_PIXEL = 0,      ///< Nastav barvu jedne LED
    LED_CMD_SET_ALL = 1,        ///< Nastav vsechny LED na stejnou barvu
    LED_CMD_CLEAR = 2,          ///< Vymas vsechny LED
    LED_CMD_SHOW_BOARD = 3,     ///< Zobraz sachovnici
    LED_CMD_BUTTON_FEEDBACK = 4,///< Button feedback (dostupnost tlacitka)
    LED_CMD_BUTTON_PRESS = 5,   ///< Tlacitko stisknuto
    LED_CMD_BUTTON_RELEASE = 6, ///< Tlacitko uvolneno
    LED_CMD_ANIMATION = 7,      ///< Spust animaci
    LED_CMD_TEST = 8,           ///< Testovaci vzor
    LED_CMD_SET_BRIGHTNESS = 9, ///< Nastav jas LED
    LED_CMD_TEST_ALL = 10,      ///< Testuj vsechny LED (postupne sviceni)
    LED_CMD_MATRIX_OFF = 11,    ///< Vypni LED efekty pri skenovani matice
    LED_CMD_MATRIX_ON = 12,     ///< Zapni LED efekty pri skenovani matice
    
    // Puzzle animacni prikazy
    LED_CMD_PUZZLE_START = 13,       ///< Spust puzzle animacni sekvenci
    LED_CMD_PUZZLE_HIGHLIGHT = 14,   ///< Zvyrazni zdrojovou figurku
    LED_CMD_PUZZLE_PATH = 15,        ///< Zobraz cestu ze zdroje do cile
    LED_CMD_PUZZLE_DESTINATION = 16, ///< Zvyrazni cil
    LED_CMD_PUZZLE_COMPLETE = 17,    ///< Dokonci puzzle krok animaci
    LED_CMD_PUZZLE_STOP = 18,        ///< Zastav vsechny puzzle animace
    
    // Pokrocile sachove animace
    LED_CMD_ANIM_PLAYER_CHANGE = 19, ///< Animace zmeny hrace (paprsky)
    LED_CMD_ANIM_MOVE_PATH = 20,     ///< Animace cesty tahu
    LED_CMD_ANIM_CASTLE = 21,        ///< Animace rosady
    LED_CMD_ANIM_PROMOTE = 22,       ///< Animace promoci
    LED_CMD_ANIM_ENDGAME = 23,       ///< Animace konce hry (vlny)
    LED_CMD_ANIM_CHECK = 24,         ///< Animace sachu
    LED_CMD_ANIM_CHECKMATE = 25,     ///< Animace matu
    LED_CMD_ANIM_PUZZLE_PATH = 26,   ///< Animace puzzle cesty
    
    // Prikazy pro ovladani komponent
    LED_CMD_DISABLE = 25,            ///< Vypni LED komponentu
    LED_CMD_ENABLE = 26,             ///< Zapni LED komponentu
    
    // Prikazy pro logiku button LED
    LED_CMD_BUTTON_PROMOTION_AVAILABLE = 27,   ///< Nastav promocni tlacitko jako dostupne
    LED_CMD_BUTTON_PROMOTION_UNAVAILABLE = 28, ///< Nastav promocni tlacitko jako nedostupne
    LED_CMD_BUTTON_SET_PRESSED = 29,           ///< Nastav tlacitko jako stisknute
    LED_CMD_BUTTON_SET_RELEASED = 30,          ///< Nastav tlacitko jako uvolnene
    
    // Prikazy pro integraci se stavem hry
    LED_CMD_GAME_STATE_UPDATE = 31,   ///< Aktualizuj LED podle aktualniho stavu hry
    LED_CMD_HIGHLIGHT_PIECES = 32,    ///< Zvyrazni figurky ktere se mohou hybat
    LED_CMD_HIGHLIGHT_MOVES = 33,     ///< Zvyrazni mozne tahy pro vybranou figurku
    LED_CMD_CLEAR_HIGHLIGHTS = 34,    ///< Vymazej vsechna zvyrazneni
    LED_CMD_PLAYER_CHANGE = 35,       ///< Animace zmeny hrace
    
    // Error handling prikazy
    LED_CMD_ERROR_INVALID_MOVE = 36,  ///< Zobraz chybu neplatneho tahu
    LED_CMD_ERROR_RETURN_PIECE = 37,  ///< Vyzvi uzivatele k vraceni figurky
    LED_CMD_ERROR_RECOVERY = 38,      ///< Obnova z chyboveho stavu
    LED_CMD_SHOW_LEGAL_MOVES = 39,    ///< Zobraz vsechny legalni tahy pro typ figurky
    
    // Prikazy Enhanced Castling systemu
    LED_CMD_CASTLING_GUIDANCE = 40,      ///< Zobraz navod pro rosadu (pozice krale/veze)
    LED_CMD_CASTLING_ERROR = 41,         ///< Zobraz chybovou indikaci rosady
    LED_CMD_CASTLING_CELEBRATION = 42,   ///< Zobraz oslavu dokonceni rosady
    LED_CMD_CASTLING_TUTORIAL = 43,      ///< Zobraz tutorial rosady
    LED_CMD_CASTLING_CLEAR = 44,         ///< Vymaz vsechny indikace rosady
    
    LED_CMD_STATUS_ACTIVE = 97,   ///< Status prikaz - aktivni LED
    LED_CMD_STATUS_COMPACT = 98,  ///< Status prikaz - kompaktni vystup
    LED_CMD_STATUS_DETAILED = 99  ///< Status prikaz - detailni vystup
} led_command_type_t;

/**
 * @brief Struktura LED prikazu
 * 
 * Tato struktura obsahuje vsechny informace potrebne pro provedeni
 * LED prikazu (barva, index, cas trvani, atd.).
 */
typedef struct {
    led_command_type_t type;    ///< Typ LED prikazu
    uint8_t led_index;          ///< Index LED (0-72)
    uint8_t red;                ///< Cervena komponenta (0-255)
    uint8_t green;              ///< Zelena komponenta (0-255)
    uint8_t blue;               ///< Modra komponenta (0-255)
    uint32_t duration_ms;       ///< Doba trvani efektu v milisekundach
    void* data;                 ///< Dodatecna data specifick pro prikaz
    QueueHandle_t response_queue; ///< Fronta pro poslani odpovedi
} led_command_t;

// ============================================================================
// DEFINICE BUTTON SYSTEMU
// ============================================================================

/**
 * @brief Typy button udalosti
 * 
 * Definuje vsechny typy udalosti ktere mohou nastat pri pouziti tlacitek.
 */
typedef enum {
    BUTTON_EVENT_PRESS = 0,      ///< Tlacitko stisknuto
    BUTTON_EVENT_RELEASE = 1,    ///< Tlacitko uvolneno
    BUTTON_EVENT_LONG_PRESS = 2, ///< Dlouhe stisknuti (vice nez 1 sekunda)
    BUTTON_EVENT_DOUBLE_PRESS = 3///< Dvojite stisknuti (do 300ms)
} button_event_type_t;

/**
 * @brief Struktura button udalosti
 * 
 * Obsahuje informace o button udalosti (typ, ID tlacitka, cas).
 */
typedef struct {
    button_event_type_t type;   ///< Typ udalosti (press, release, long press, double press)
    uint8_t button_id;          ///< ID tlacitka (0-8)
    uint32_t press_duration_ms; ///< Doba stisknuti v milisekundach
    uint32_t timestamp;         ///< Casova znamka udalosti
} button_event_t;

// ============================================================================
// DEFINICE MATRIX SYSTEMU
// ============================================================================

/**
 * @brief Typy matrix udalosti
 * 
 * Definuje vsechny typy udalosti ktere muze generovat matrix task
 * pri detekovani pohybu figurek.
 */
typedef enum {
    MATRIX_EVENT_PIECE_LIFTED = 0, ///< Figurka zvednuta (reed switch rozepnut)
    MATRIX_EVENT_PIECE_PLACED = 1, ///< Figurka polozena (reed switch sepnut)
    MATRIX_EVENT_MOVE_DETECTED = 2,///< Kompletni tah detekovan (lift + place)
    MATRIX_EVENT_ERROR = 3         ///< Chyba pri detekovani
} matrix_event_type_t;

/**
 * @brief Struktura matrix udalosti
 * 
 * Obsahuje informace o matrix udalosti (typ, pozice, figurka).
 */
typedef struct {
    matrix_event_type_t type; ///< Typ udalosti (lifted, placed, move detected)
    uint8_t from_square;      ///< Zdrojove pole (0-63)
    uint8_t to_square;        ///< Cilove pole (0-63)
    piece_t piece_type;       ///< Typ figurky
    uint32_t timestamp;       ///< Casova znamka udalosti
    uint8_t from_row;         ///< Zdrojovy radek (0-7)
    uint8_t from_col;         ///< Zdrojovy sloupec (0-7)
    uint8_t to_row;           ///< Cilovy radek (0-7)
    uint8_t to_col;           ///< Cilovy sloupec (0-7)
} matrix_event_t;

/**
 * @brief Typy matrix prikazu
 * 
 * Definuje prikazy ktere lze poslat matrix tasku.
 */
typedef enum {
    MATRIX_CMD_SCAN = 0,      ///< Proved skenovani matice
    MATRIX_CMD_RESET = 1,     ///< Resetuj stav matice
    MATRIX_CMD_TEST = 2,      ///< Testuj matrix funkci
    MATRIX_CMD_CALIBRATE = 3, ///< Kalibruj matrix (nastaveni citlivosti)
    MATRIX_CMD_DISABLE = 4,   ///< Vypni skenovani matice
    MATRIX_CMD_ENABLE = 5     ///< Zapni skenovani matice
} matrix_command_type_t;

/**
 * @brief Struktura matrix prikazu
 * 
 * Obsahuje informace o prikazu pro matrix task.
 */
typedef struct {
    matrix_command_type_t type; ///< Typ prikazu
    uint8_t data[16];           ///< Dodatecna data pro prikaz
} matrix_command_t;










// ============================================================================
// HARDWAROVE KONSTANTY
// ============================================================================

// LED systemove konstanty
/** @brief Pocet LED pro sachovnici (8x8 = 64) */
#define CHESS_LED_COUNT_BOARD 64
/** @brief Pocet LED pro tlacitka (8 promotion + 1 reset = 9) */
#define CHESS_LED_COUNT_BUTTONS 9
/** @brief Celkovy pocet LED (64 sachovnice + 9 tlacitek = 73) */
#define CHESS_LED_COUNT_TOTAL (CHESS_LED_COUNT_BOARD + CHESS_LED_COUNT_BUTTONS)
/** @brief Celkovy pocet LED v systemu (alias pro CHESS_LED_COUNT_TOTAL) */
#define CHESS_LED_COUNT 73  // Total number of LEDs (64 board + 9 buttons)

// Button systemove konstanty
/** @brief Pocet tlacitek v systemu (8 promotion + 1 reset = 9) */
#define CHESS_BUTTON_COUNT 9

// Matrix systemove konstanty
/** @brief Velikost matrix (8x8 = 64 poli) */
#define CHESS_MATRIX_SIZE 64

// Game konstanty
/** @brief Maximalni pocet tahu v historii (200 tahu) */
#define MAX_MOVE_HISTORY 200



/**
 * @brief Struktura navrhu tahu pro analyzu
 * 
 * Tato struktura obsahuje informace o navrženem tahu vcetne skore
 * a specialnich vlastnosti (capture, check, rosada, en passant).
 */
typedef struct {
    uint8_t from_row;      ///< Zdrojovy radek (0-7)
    uint8_t from_col;      ///< Zdrojovy sloupec (0-7)
    uint8_t to_row;        ///< Cilovy radek (0-7)
    uint8_t to_col;        ///< Cilovy sloupec (0-7)
    piece_t piece;         ///< Figurka ktera se pohybuje
    bool is_capture;       ///< Je to sebrani?
    bool is_check;         ///< Vyvola to sach?
    bool is_castling;      ///< Je to rosada?
    bool is_en_passant;    ///< Je to en passant?
    int score;             ///< Skore tahu (pro AI evaluaci, vyssi = lepsi)
} move_suggestion_t;

// ============================================================================
// DEKLARACE GAME UTILITY FUNKCI
// ============================================================================

// Predni deklarace utility funkci pro game task
/**
 * @brief Overi zda je pozice platna na sachovnici
 * @param row Radek (0-7)
 * @param col Sloupec (0-7)
 * @return true pokud je pozice platna
 */
bool game_is_valid_square(int row, int col);

/**
 * @brief Overi zda je figurka vlastni (patri aktualnimu hraci)
 * @param piece Figurka k overeni
 * @param player Hrac
 * @return true pokud je figurka vlastni
 */
bool game_is_own_piece(piece_t piece, player_t player);

/**
 * @brief Overi zda je figurka nepratelska
 * @param piece Figurka k overeni
 * @param player Hrac
 * @return true pokud je figurka nepratelska
 */
bool game_is_enemy_piece(piece_t piece, player_t player);

/**
 * @brief Simuluje tah a overi zda by nechal krale v sachu
 * @param move Tah k simulaci
 * @param player Hrac
 * @return true pokud by tah nechal krale v sachu
 */
bool game_simulate_move_check(chess_move_extended_t* move, player_t player);

// ============================================================================
// DEFINICE SYSTEMOVE KONFIGURACE
// ============================================================================

/**
 * @brief Struktura systemove konfigurace
 * 
 * Obsahuje vsechna nastaveni systemu ktere lze ulozit do NVS flash.
 */
typedef struct {
    bool verbose_mode;           ///< Podrobny logovaci rezim (detailni vypisy)
    bool quiet_mode;             ///< Tichy rezim (minimalni vystup)
    uint8_t log_level;           ///< Uroven logovania (ESP_LOG_ERROR, ESP_LOG_INFO, atd.)
    uint32_t command_timeout_ms; ///< Timeout prikazu v milisekundach
    bool echo_enabled;           ///< Povoleno echo znaku pri psani prikazu
} system_config_t;

// Predni deklarace konfiguracnich funkci
/**
 * @brief Inicializuje config manager
 * @return ESP_OK pri uspechu
 */
esp_err_t config_manager_init(void);

/**
 * @brief Nacte konfiguraci z NVS flash
 * @param[out] config Ukazatel na strukturu pro nacteni konfigurace
 * @return ESP_OK pri uspechu
 */
esp_err_t config_load_from_nvs(system_config_t* config);

/**
 * @brief Ulozi konfiguraci do NVS flash
 * @param config Konfigurace k ulozeni
 * @return ESP_OK pri uspechu
 */
esp_err_t config_save_to_nvs(const system_config_t* config);

/**
 * @brief Aplikuje nastaveni konfigurace na system
 * @param config Konfigurace k aplikaci
 * @return ESP_OK pri uspechu
 */
esp_err_t config_apply_settings(const system_config_t* config);

/**
 * @brief Urovne obtiznosti puzzle
 * 
 * Definuje urovne obtiznosti pro sachove puzzle (ulohy).
 */
typedef enum {
    PUZZLE_DIFFICULTY_BEGINNER = 1,     ///< Zacatecnik (2-3 tahy, zakladni taktiky)
    PUZZLE_DIFFICULTY_INTERMEDIATE = 2, ///< Pokrocily (3-5 tahu, slozite taktiky)
    PUZZLE_DIFFICULTY_ADVANCED = 3,     ///< Expert (5+ tahu, pokrocile kombinace)
    PUZZLE_DIFFICULTY_MASTER = 4        ///< Mistr (slozite endgames a studie)
} puzzle_difficulty_t;

/**
 * @brief Struktura kroku puzzle
 * 
 * Obsahuje informace o jednom kroku v puzzle (zdrojova a cilova pozice,
 * popis a zda je tah vynuceny).
 */
typedef struct {
    uint8_t from_row;      ///< Zdrojovy radek (0-7)
    uint8_t from_col;      ///< Zdrojovy sloupec (0-7)
    uint8_t to_row;        ///< Cilovy radek (0-7)
    uint8_t to_col;        ///< Cilovy sloupec (0-7)
    char description[64];  ///< Citelny popis kroku pro uzivatele
    bool is_forced;        ///< Je tento tah vynuceny? (jedina moznost)
} puzzle_step_t;

/**
 * @brief Kompletni struktura puzzle
 * 
 * Obsahuje vsechny informace o sachovem puzzle vcetne nazvu,
 * popisu, obtiznosti, pocatecni pozice a kroku reseni.
 */
typedef struct {
    char name[32];                   ///< Nazev puzzle
    char description[128];           ///< Popis puzzle pro uzivatele
    puzzle_difficulty_t difficulty;  ///< Uroven obtiznosti
    piece_t initial_board[8][8];     ///< Pocatecni pozice na sachovnici
    puzzle_step_t steps[16];         ///< Kroky reseni (max 16 tahu)
    uint8_t step_count;              ///< Pocet kroku
    uint8_t current_step;            ///< Index aktualniho kroku
    bool is_active;                  ///< Je puzzle aktivni?
    uint32_t start_time;             ///< Casova znamka startu puzzle
    uint32_t completion_time;        ///< Casova znamka dokonceni
} chess_puzzle_t;

/**
 * @brief Stav animace pro LED efekty
 * 
 * Obsahuje informace o probiha jici LED animaci (typ, snimky, barvy, atd.).
 */
typedef struct {
    bool is_active;                  ///< Je animace aktivni?
    uint8_t animation_type;          ///< Typ animace
    uint8_t current_frame;           ///< Aktualni snimek animace
    uint8_t total_frames;            ///< Celkovy pocet snimku
    uint32_t frame_duration_ms;      ///< Doba trvani jednoho snimku v ms
    uint32_t last_update_time;       ///< Casova znamka posledni aktualizace
    uint8_t source_square;           ///< Zdrojove pole (0-63)
    uint8_t target_square;           ///< Cilove pole (0-63)
    uint8_t color_r, color_g, color_b; ///< Barvy animace (RGB)
    bool interrupt_on_placement;     ///< Zastav animaci pri polozeni figurky?
} led_animation_state_t;

#ifdef __cplusplus
}
#endif

#endif // CHESS_TYPES_H
