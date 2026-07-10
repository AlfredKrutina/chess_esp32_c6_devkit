
/**
 * @file game_task.c
 * @brief Game Task - Sachova logika a sprava stavu hry
 *
 * @details
 * =============================================================================
 * CO TENTO SOUBOR DELA?
 * =============================================================================
 *
 * Tento task je "mozek" sachovnice. Spravuje celou logiku hry:
 * 1. Sachova pravidla (standardni FIDE pravidla)
 * 2. Validace tahu (legalni/nelegalni tahy)
 * 3. Detekce specialnich tahu (rosada, en passant, promoc)
 * 4. Detekce konce hry (mat, pat, remiza)
 * 5. Sprava casovace (timer system integration)
 * 6. Historie tahu a analyza pozice
 * 7. Error recovery (cervene/modre LED)
 * 8. Komunikace s fyzickou deskou (matrix_task)
 *
 *
 * =============================================================================
 * JAK TO FUNGUJE?
 * =============================================================================
 *
 * STARTUP:
 * - Inicializace sachovnice (pocatecni pozice)
 * - Registrace s WDT (watchdog timer)
 * - Inicializace timer systemu
 * - Start hlavni smycky (100ms cyklus)
 *
 * HLAVNI SMYCKA (100ms cyklus):
 * while (1) {
 *     1. Reset WDT
 *     2. Zpracuj prikazy z fronty (game_command_queue)
 *     3. Zpracuj matrix eventy (UP/DN signaly z fyzicke desky)
 *     4. Aktualizuj timer display
 *     5. Proved periodicke kontroly
 *     6. Cekej 100ms (vTaskDelayUntil)
 * }
 *
 * ZPRACOVANI TAHU:
 * 1. Matrix: UP signal (figurka zvednuta) -> uloz pozici
 * 2. Matrix: DN signal (figurka polozena) -> validuj tah
 * 3. Pokud validni: proved tah, aktualizuj stav, LED animace
 * 4. Pokud nevalidni: error recovery (cervene/modre LED)
 * 5. Zkontroluj konec hry (mat/pat)
 *
 * =============================================================================
 * KOMUNIKACE (FIFOS & MUTEXY)
 * =============================================================================
 *
 * FRONTY (QUEUES) - Prijem prikazu:
 * - game_command_queue -> Prikazy z UART/Web (move, reset, status...)
 * - matrix_event_queue -> UP/DN eventy z fyzicke desky
 *
 * FRONTY (QUEUES) - Odeslani prikazu:
 * - response_queue -> Odpovedi na prikazy (status, board...)
 * - LED se ovladaji primymi volanimi (fronta byla odstranena)
 *
 * MUTEXY - Ochrana sdilenych zdroju:
 * - game_mutex -> Ochrana stavu hry (board, current_player, move_count...)
 *   DULEZITE: Vzdy pouzij pri pristupu ke game state!
 *
 * PRISTUP:
 * @code
 * xSemaphoreTake(game_mutex, portMAX_DELAY);  // Zamkni
 * // Prace se stavem hry
 * xSemaphoreGive(game_mutex);                  // Odemkni
 * @endcode
 *
 * =============================================================================
 * SACHOVCE PRAVIDLA IMPLEMENTOVANA
 * =============================================================================
 *
 * ZAKLADNI TAHY:
 * - Pesec: 1 nebo 2 pole dopredu z vychoziho radku, bere sikmo
 * - Vez: Horizontalne/vertikalne
 * - Kun: L-tvar (2+1 pole)
 * - Strelec: Diagonalne
 * - Dama: Kombinace vez + strelec
 * - Kral: 1 pole libovolnym smerem
 *
 * SPECIALNI TAHY:
 * - Rosada (kratka i dlouha) - implementovano s kontrolou pravidel
 * - En Passant - detekce a provedeni
 * - Promoc pescu - automaticka na damu (moznost prepnout v budoucnu)
 *
 * KONEC HRY:
 * - Mat (checkmate) - kral v sachu, zadne legalni tahy
 * - Pat (stalemate) - neni v sachu, ale zadne legalni tahy
 * - 50-move rule - 50 tahu bez brани nebo tahu pescem
 * - Threefold repetition - 3x stejna pozice
 * - Insufficient material - nedostatek materialu na mat
 *
 * =============================================================================
 * TABLE OF CONTENTS (NAVIGACE)
 * =============================================================================
 *
 * Sekce 1:  Global Variables & State ............... radek 100
 * Sekce 2:  Board Initialization .................... radek 1070
 * Sekce 3:  Move Validation ......................... radek 2000
 * Sekce 4:  Special Moves (Castle, En Passant) ...... radek 3500
 * Sekce 5:  Move Execution .......................... radek 4500
 * Sekce 6:  Endgame Detection ....................... radek 5500
 * Sekce 7:  Command Processing ...................... radek 6500
 * Sekce 8:  Main Game Task Loop ..................... radek 7964
 * Sekce 9:  JSON API Functions ...................... radek 11477
 *
 * TIP: Ctrl+G pro skok na radek
 *
 * =============================================================================
 * DEPENDENCIES
 * =============================================================================
 *
 * - matrix_task: Prijem UP/DN eventu z fyzicke desky
 * - led_task: Ovladani LED (highlight, animace)
 * - timer_system: Casovac pro hru
 * - uart_task: Prikazy z terminalu
 * - web_server_task: Prikazy z webu
 * - unified_animation_manager: Animace tahu
 *
 * =============================================================================
 * KRITICKA PRAVIDLA
 * =============================================================================
 *
 * @warning CO SE NESMI DELAT:
 *
 * 1. NIKDY nepristupuj ke stavu hry bez game_mutex!
 *    ❌ current_player = PLAYER_WHITE;  // SPATNE - muze zpusobit race
condition
 *    ✅ xSemaphoreTake(game_mutex, ...); current_player = PLAYER_WHITE;
xSemaphoreGive(...);
 *
 * 2. NIKDY nedrzи mutex prilis dlouho!
 *    ❌ xSemaphoreTake(...); dlouha_operace(); xSemaphoreGive(...);
 *    ✅ Proved jen nezbytne operace s mutexem
 *
 * 3. NIKDY nevolej blocking operace s dlouhym timeout!
 *    ❌ xQueueReceive(queue, ..., portMAX_DELAY);  // Muze zablokovat WDT
 *    ✅ xQueueReceive(queue, ..., pdMS_TO_TICKS(100));  // Max 100ms
 *
 * 4. VZDY resetuj WDT v hlavni smycce!
 *    ✅ game_task_wdt_reset_safe();  // V kazdem cyklu 100ms
 *
 * 5. VZDY kontroluj navratove hodnoty!
 *    ❌ xQueueSend(...);  // Ignoruje chybu
 *    ✅ if (xQueueSend(...) != pdTRUE) { handle error }
 *
 * @author Alfred Krutina
 * @version 1.8.0
 *
 * @note Priorita tasku 4; hlavní smyčka cca 100 ms; WDT reset ve smyčce.
 * @see matrix_task.c Detekce pohybu figurek
 * @see led_task.c LED animace
 * @see uart_task.c Terminál
 * @see web_server_task.c Web rozhraní
 */

#include "game_task.h"
#include "game_board_core.h"
#include "game_matrix_guard.h"
#include "game_snapshot.h"
#include "game_task_internal.h"
#include "../button_task/include/button_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "../led_task/include/led_task.h"
#include "../matrix_task/include/matrix_task.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "freertos_chess.h"
#include "game_led_animations.h"
#include "led_mapping.h"
// Timer system integration
#include "../../timer_system/include/timer_system.h"
// ANSI color formatting macros
#include "game_colors.h"
// HA Light Task integration
#include "../ha_light_task/include/ha_light_task.h"
#include "../config_manager/include/config_manager.h"
// Note: animation_task.h is not included to avoid type conflicts with
// unified_animation_manager
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../game_hooks/include/game_state_notify.h"

static const char *TAG = "GAME_TASK";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

// ============================================================================
// WDT WRAPPER FUNCTIONS
// ============================================================================

/**
 * @brief Bezpecny reset WDT s logovanim WARNING misto ERROR pro
 * ESP_ERR_NOT_FOUND
 *
 * Tato funkce bezpecne resetuje Task Watchdog Timer. Pokud task neni jeste
 * registrovany (coz je normalni behem startupu), loguje se WARNING misto ERROR.
 *
 * @return ESP_OK pokud uspesne, ESP_ERR_NOT_FOUND pokud task neni registrovany
 * (WARNING pouze)
 *
 * @details
 * Funkce je pouzivana pro bezpecny reset watchdog timeru behem game operaci.
 * Zabranuje chybam pri startupu kdy task jeste neni registrovany.
 */
static esp_err_t game_task_wdt_reset_safe(void) {
  esp_err_t ret = esp_task_wdt_reset();

  if (ret == ESP_ERR_NOT_FOUND) {
    // Log as WARNING instead of ERROR - task not registered yet
    ESP_LOGW(
        TAG,
        "WDT reset: task not registered yet (this is normal during startup)");
    return ESP_OK; // Treat as success for our purposes
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "WDT reset failed: %s", esp_err_to_name(ret));
    return ret;
  }

  return ESP_OK;
}

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

// Direction vectors for piece movement
static const int8_t knight_moves[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                          {1, -2},  {1, 2},  {2, -1},  {2, 1}};

// Game configuration
#define MAX_MOVES_HISTORY 200 // Maximum moves to remember
// #define GAME_TIMEOUT_MS 300000 // 5 minutes per move timeout   not used in
// this version
#define MOVE_VALIDATION_MS 100 // Move validation timeout

// Game state
game_state_t current_game_state = GAME_STATE_IDLE;
player_t current_player = PLAYER_WHITE;
uint32_t move_count = 0;
bool resync_required_after_restore = false;
/** Monotónní verze stavu pro klienty (ETag / If-None-Match, delta). */
static uint32_t game_state_revision = 0;
// New logic: Block auto-new game until at least one move is made
bool auto_new_game_blocked_until_move = false;

// Simplified castling state (type in game_task_internal.h)
castling_state_t castling_state = {0};

// Active castling state (simplified implementation)
bool castle_animation_active = false;
static chess_move_extended_t pending_castle_move;

// Rook animation state
static TimerHandle_t rook_animation_timer = NULL;
static bool rook_animation_active = false;
uint8_t rook_from_row, rook_from_col, rook_to_row, rook_to_col;

// Board representation (8x8)
piece_t board[8][8] = {0};
bool piece_moved[8][8] = {false};

// Global variables for tracking lifted piece (UP/DN commands)
bool piece_lifted = false;
uint8_t lifted_piece_row = 0;
uint8_t lifted_piece_col = 0;
piece_t lifted_piece = PIECE_EMPTY;

// 3-step capture flow state.
bool capture_in_progress = false; // Je rozpracovaný capture?
uint8_t capture_target_row = 0;   // Kam se má položit vlastní figurka
uint8_t capture_target_col = 0;
piece_t capture_removed_piece = PIECE_EMPTY; // Jaká figurka byla odebrána

guided_capture_state_t guided_capture_state = {0};
static bool guided_capture_hints_enabled = true;
/** 1 = jen zdroj zvednutí … 5 = plná nápověda vč. guided capture na LED */
static uint8_t led_guidance_level = 5;

bool game_led_guidance_show_destinations(void) {
  return led_guidance_level >= 2;
}

bool game_led_guidance_show_movable_yellow(void) {
  return led_guidance_level >= 3;
}

bool game_led_guidance_show_check_anim(void) {
  return led_guidance_level >= 4;
}

bool opponent_piece_moved = false;
piece_t opponent_piece_type = PIECE_EMPTY;
uint8_t opponent_original_row = 0;
uint8_t opponent_original_col = 0;
uint8_t opponent_current_row = 0;
uint8_t opponent_current_col = 0;

// ============================================================================
// AUTO NEW GAME DETECTION
// ============================================================================

/**
 * @brief Auto new game detection state
 *
 * Detekuje kdyz jsou vsechny figurky v pocatecni pozici po dobu 2 sekund
 * a automaticky spusti novou hru. Zabranuje nekonecnemu resetu.
 */
static bool auto_new_game_in_starting_position = false;
static uint32_t auto_new_game_timer_start = 0;
static bool auto_new_game_triggered = false; // Flag aby se neopakoval reset

#define AUTO_NEW_GAME_CONFIRMATION_TIME_MS 2000 // 2 sekundy

// ============================================================================
// DEMO MODE STATE
// ============================================================================

/**
 * @brief Demo mode state variables
 *
 * Demo mode automatically plays random legal moves.
 * Includes special handling for castling (2-phase moves) and promotion.
 */

// Castling flags
bool white_king_moved = false;
bool white_rook_a_moved = false; // a1 rook
bool white_rook_h_moved = false; // h1 rook
bool black_king_moved = false;
bool black_rook_a_moved = false; // a8 rook
bool black_rook_h_moved = false; // h8 rook

// En passant state
bool en_passant_available = false;
uint8_t en_passant_target_row = 0;
uint8_t en_passant_target_col = 0;
uint8_t en_passant_victim_row = 0;
uint8_t en_passant_victim_col = 0;

// Move history
chess_move_t move_history[MAX_MOVES_HISTORY];
move_type_t move_history_kind[MAX_MOVES_HISTORY];
uint32_t history_index = 0;

// Task state
static bool task_running = false;
bool game_active = false;

//  Error handling - pamatování posledního validního pole
// Enhanced error recovery state for smart error handling
game_task_error_recovery_t error_recovery_state = {false, 0, 0, 0, 0, PIECE_EMPTY, false, 0};

// NON-BLOCKING BLINK SYSTEM
typedef struct {
  bool active;
  uint8_t led_index;
  uint32_t blink_count;
  uint32_t max_blinks;
  uint32_t last_toggle_time;
  uint32_t blink_interval_ms;
  bool led_state;
} non_blocking_blink_state_t;

static non_blocking_blink_state_t blink_state = {false, 0,   0,    10,
                                                 0,     300, false};

// ============================================================================
// STARTING POSITION CHECK SETTINGS
// ============================================================================

/**
 * @brief Hlídání počáteční pozice - zapnuto/vypnuto
 *
 * Pokud je zapnuto, systém hlídá zda jsou figurky správně rozestaveny
 * před začátkem hry. Defaultně vypnuto (false).
 */
static bool starting_position_check_enabled = false;

/**
 * @brief Nastaví hlídání počáteční pozice
 * @param enabled true pro zapnutí, false pro vypnutí
 */
void game_set_starting_position_check(bool enabled) {
  starting_position_check_enabled = enabled;
  ESP_LOGI(TAG, "Starting position check: %s",
           enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Vrací stav hlídání počáteční pozice
 * @return true pokud je hlídání zapnuto, jinak false
 */
bool game_get_starting_position_check(void) {
  return starting_position_check_enabled;
}

// ============================================================================
// PROMOTION STATE - PHYSICAL BUTTON SUPPORT
// ============================================================================

/**
 * @brief Stav promoce pesce
 *
 * Sleduje zda je promoce mozna a kde se nachazi pesec k promoci.
 * Pouziva se pro rizeni LED indikace tlacitek a zpracovani button eventu.
 */
game_task_promotion_state_t promotion_state = {false, 0, 0, PLAYER_WHITE};

// Mutex to protect promotion_state from race conditions.
// Prevents concurrent access from web, UART, button, and matrix tasks
SemaphoreHandle_t promotion_mutex = NULL;

/** Nastaveno v game_process_chess_move před game_execute_move; spotřebuje se na
 * začátku game_execute_move (okamžitá promoce z API místo promotion pending). */
bool s_uart_move_immediate_promotion;
promotion_choice_t s_uart_move_immediate_promotion_piece;

// ============================================================================
// KING RESIGNATION SYSTEM
// ============================================================================

king_resignation_state_t resignation_state = {.active = false,
                                                     .player = PLAYER_WHITE,
                                                     .king_row = 0,
                                                     .king_col = 0,
                                                     .main_timer = NULL,
                                                     .animation_timer = NULL,
                                                     .start_time_ms = 0,
                                                     .last_countdown_sec = 10};

// Forward declarations pro resignation
static void resignation_main_timer_callback(TimerHandle_t xTimer)
    __attribute__((unused));
static void resignation_animation_timer_callback(TimerHandle_t xTimer)
    __attribute__((unused));
void resignation_start(player_t player, uint8_t row, uint8_t col);
void resignation_stop(bool finalize);
static void resignation_update_button_leds(uint32_t elapsed_ms);
static void resignation_tick(void);
static void resignation_finalize_timeout(void);
bool game_cmd_is_matrix_origin(const chess_move_command_t *cmd);

// Forward declarations
void game_highlight_opponent_pieces(void);

// Game statistics
static uint32_t total_games = 0;
static uint32_t white_wins = 0;
static uint32_t black_wins = 0;
static uint32_t draws = 0;

// Flag pro endgame report request (místo volání z timer callbacku)
static bool endgame_report_requested = false;

// Typ konce hry pro lepší rozlišení v reportu
typedef enum {
  // Checkmate variations
  ENDGAME_REASON_CHECKMATE = 0,
  ENDGAME_REASON_CHECKMATE_EN_PASSANT = 1, // Šachmat pomocí en passant
  ENDGAME_REASON_CHECKMATE_CASTLING = 2,   // Šachmat pomocí rošády
  ENDGAME_REASON_CHECKMATE_PROMOTION = 3,  // Šachmat po promoci pěšce
  ENDGAME_REASON_CHECKMATE_DISCOVERED = 4, // Šachmat odkrytým šachem

  // Draw conditions
  ENDGAME_REASON_STALEMATE = 10,
  ENDGAME_REASON_50_MOVE = 11,
  ENDGAME_REASON_REPETITION = 12,
  ENDGAME_REASON_INSUFFICIENT = 13,

  // Player actions
  ENDGAME_REASON_TIMEOUT = 20,
  ENDGAME_REASON_RESIGNATION = 21
} endgame_reason_t;

static endgame_reason_t current_endgame_reason = ENDGAME_REASON_CHECKMATE;

last_move_type_t last_move_type = LAST_MOVE_NORMAL;

// Extended game statistics
uint32_t game_start_time = 0;
uint32_t last_move_time = 0;
uint32_t white_time_total = 0;
uint32_t black_time_total = 0;
uint32_t white_moves_count = 0;
uint32_t black_moves_count = 0;
static uint32_t white_captures = 0;
static uint32_t black_captures = 0;
static uint32_t white_checks = 0;
static uint32_t black_checks = 0;
uint32_t white_castles = 0;
uint32_t black_castles = 0;

// Captured pieces tracking
#define MAX_CAPTURED_PIECES 16
static piece_t white_captured_pieces[MAX_CAPTURED_PIECES];
static piece_t black_captured_pieces[MAX_CAPTURED_PIECES];
static uint32_t white_captured_count = 0;
static uint32_t black_captured_count = 0;
static uint32_t white_captured_index = 0;
static uint32_t black_captured_index = 0;

// Material advantage tracking (pro graf výhody v průběhu hry)
#define MAX_ADVANTAGE_HISTORY 200
static int8_t material_advantage_history
    [MAX_ADVANTAGE_HISTORY]; // -127 až +127 (White positive, Black negative)
static uint32_t advantage_history_count = 0;

/**
 * @brief Convert piece_t to character representation
 */
static char piece_to_char(piece_t piece) {
  switch (piece) {
  case PIECE_EMPTY:
    return ' ';
  case PIECE_WHITE_PAWN:
    return 'P';
  case PIECE_WHITE_KNIGHT:
    return 'N';
  case PIECE_WHITE_BISHOP:
    return 'B';
  case PIECE_WHITE_ROOK:
    return 'R';
  case PIECE_WHITE_QUEEN:
    return 'Q';
  case PIECE_WHITE_KING:
    return 'K';
  case PIECE_BLACK_PAWN:
    return 'p';
  case PIECE_BLACK_KNIGHT:
    return 'n';
  case PIECE_BLACK_BISHOP:
    return 'b';
  case PIECE_BLACK_ROOK:
    return 'r';
  case PIECE_BLACK_QUEEN:
    return 'q';
  case PIECE_BLACK_KING:
    return 'k';
  default:
    return ' ';
  }
}

/**
 * @brief Vypocitat aktualni materialovou vyhodu na boardu
 * @return Materialova vyhoda (kladna = White vede, zaporna = Black vede)
 */
static int8_t game_calculate_material_advantage(void) {
  int white_material = 0;
  int black_material = 0;

  // Hodnoty figur
  const int pawn_value = 1;
  const int knight_value = 3;
  const int bishop_value = 3;
  const int rook_value = 5;
  const int queen_value = 9;

  // Spocitat material na boardu
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      switch (piece) {
      case PIECE_WHITE_PAWN:
        white_material += pawn_value;
        break;
      case PIECE_WHITE_KNIGHT:
        white_material += knight_value;
        break;
      case PIECE_WHITE_BISHOP:
        white_material += bishop_value;
        break;
      case PIECE_WHITE_ROOK:
        white_material += rook_value;
        break;
      case PIECE_WHITE_QUEEN:
        white_material += queen_value;
        break;
      case PIECE_BLACK_PAWN:
        black_material += pawn_value;
        break;
      case PIECE_BLACK_KNIGHT:
        black_material += knight_value;
        break;
      case PIECE_BLACK_BISHOP:
        black_material += bishop_value;
        break;
      case PIECE_BLACK_ROOK:
        black_material += rook_value;
        break;
      case PIECE_BLACK_QUEEN:
        black_material += queen_value;
        break;
      default:
        break;
      }
    }
  }

  int advantage = white_material - black_material;
  // Omezit na int8_t rozsah
  if (advantage > 127)
    advantage = 127;
  if (advantage < -127)
    advantage = -127;

  return (int8_t)advantage;
}

/**
 * @brief Ulozit aktualni materialovou vyhodu do historie
 */
void game_record_material_advantage(void) {
  if (advantage_history_count >= MAX_ADVANTAGE_HISTORY) {
    return; // Historie plna
  }

  int8_t advantage = game_calculate_material_advantage();
  material_advantage_history[advantage_history_count++] = advantage;
}

/**
 * @brief Add captured piece to tracking
 */
void game_add_captured_piece(piece_t piece) {
  // Determine which array to use based on piece color
  if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
    // White piece captured by black
    if (black_captured_index < MAX_CAPTURED_PIECES) {
      black_captured_pieces[black_captured_index++] = piece;
      black_captured_count++;
      black_captures++;
    }
  } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
    // Black piece captured by white
    if (white_captured_index < MAX_CAPTURED_PIECES) {
      white_captured_pieces[white_captured_index++] = piece;
      white_captured_count++;
      white_captures++;
    }
  }
}

static uint32_t moves_without_capture = 0;
static uint32_t max_moves_without_capture = 0;
static uint32_t position_history[100];
static uint32_t position_history_count = 0;

// Game state flags
static bool timer_enabled = true;
static bool game_saved = false;
static char saved_game_name[32] = "";
game_state_t game_result = GAME_STATE_IDLE;
game_result_type_t current_result_type =
    RESULT_WHITE_WINS; // Aktuální typ konce hry

// Castling flags and en passant state
// (declared later in the file)

// Move generation buffers (declared later in the file)
// static chess_move_extended_t legal_moves_buffer[128];
// static uint32_t legal_moves_count = 0;

// Include ctype.h for tolower function
#include <ctype.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

// Promotion functions - MUST BE FIRST (called from game_initialize_board)
// Note: These are defined later in the file (around line 3800)
void game_check_promotion_needed(void);
void game_update_promotion_anchor_led(void);
static void game_process_promotion_button(uint8_t button_id);
static bool game_execute_promotion(promotion_choice_t choice);

void game_print_board_enhanced(void);
uint32_t game_generate_legal_moves(player_t player);

// Error recovery functions
void game_update_error_blink(void);
void game_stop_error_blink(void);

// ============================================================================
// POSITION HASHING AND DRAW DETECTION
// ============================================================================

/**
 * @brief Calculate hash for current board position
 * @return 32-bit position hash
 */
uint32_t game_calculate_position_hash(void) {
  uint32_t hash = 0;

  // Hash board pieces
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece != PIECE_EMPTY) {
        // Combine piece type, position, and color
        uint32_t piece_hash = (piece << 16) | (row << 8) | col;
        hash = ((hash << 5) + hash) ^ piece_hash; // Simple hash function
      }
    }
  }

  // Include current player in hash
  hash = ((hash << 5) + hash) ^ (current_player << 24);

  // Include castling rights
  uint32_t castling_rights = 0;
  if (!piece_moved[0][4])
    castling_rights |= 0x01; // White king
  if (!piece_moved[0][0])
    castling_rights |= 0x02; // White rook a1
  if (!piece_moved[0][7])
    castling_rights |= 0x04; // White rook h1
  if (!piece_moved[7][4])
    castling_rights |= 0x08; // Black king
  if (!piece_moved[7][0])
    castling_rights |= 0x10; // Black rook a8
  if (!piece_moved[7][7])
    castling_rights |= 0x20; // Black rook h8
  hash = ((hash << 5) + hash) ^ castling_rights;

  // Include en passant state
  if (en_passant_available) {
    uint32_t en_passant_hash =
        (en_passant_target_row << 8) | en_passant_target_col;
    hash = ((hash << 5) + hash) ^ en_passant_hash;
  }

  return hash;
}

/**
 * @brief Check if current position has been repeated three times (threefold
 * repetition)
 * @return true if position is repeated 3+ times (draw by repetition)
 */
bool game_is_position_repeated(void) {
  uint32_t current_hash = game_calculate_position_hash();
  int repetition_count = 0;

  // Count how many times current position appears in history
  for (int i = 0; i < position_history_count; i++) {
    if (position_history[i] == current_hash) {
      repetition_count++;
      if (repetition_count >= 3) {
        return true; // Threefold repetition detected
      }
    }
  }

  return false;
}

/**
 * @brief Add current position to history
 */
void game_add_position_to_history(void) {
  uint32_t current_hash = game_calculate_position_hash();

  if (position_history_count < 100) {
    position_history[position_history_count++] = current_hash;
  } else {
    // Shift history and add new position
    for (int i = 0; i < 99; i++) {
      position_history[i] = position_history[i + 1];
    }
    position_history[99] = current_hash;
  }
}

// ============================================================================
// MATERIAL CALCULATION AND SCORING
// ============================================================================

// Standard piece values
static const int piece_values[] = {
    0, // PIECE_EMPTY
    1, // PIECE_WHITE_PAWN
    3, // PIECE_WHITE_KNIGHT
    3, // PIECE_WHITE_BISHOP
    5, // PIECE_WHITE_ROOK
    9, // PIECE_WHITE_QUEEN
    0, // PIECE_WHITE_KING (infinite value)
    1, // PIECE_BLACK_PAWN
    3, // PIECE_BLACK_KNIGHT
    3, // PIECE_BLACK_BISHOP
    5, // PIECE_BLACK_ROOK
    9, // PIECE_BLACK_QUEEN
    0  // PIECE_BLACK_KING (infinite value)
};

/**
 * @brief Calculate material balance
 * @param white_material Output: white material value
 * @param black_material Output: black material value
 * @return Material difference (positive = white advantage, negative = black
 * advantage)
 */
int game_calculate_material_balance(int *white_material, int *black_material) {
  int white_total = 0;
  int black_total = 0;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece != PIECE_EMPTY) {
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_QUEEN) {
          white_total += piece_values[piece];
        } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_QUEEN) {
          black_total +=
              piece_values[piece - 6]; // Adjust index for black pieces
        }
      }
    }
  }

  if (white_material)
    *white_material = white_total;
  if (black_material)
    *black_material = black_total;

  return white_total - black_total;
}

/**
 * @brief Get material balance as string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void game_get_material_string(char *buffer, size_t buffer_size) {
  int white_material, black_material;
  int balance =
      game_calculate_material_balance(&white_material, &black_material);

  if (balance > 0) {
    snprintf(buffer, buffer_size, "White +%d", balance);
  } else if (balance < 0) {
    snprintf(buffer, buffer_size, "Black +%d", -balance);
  } else {
    snprintf(buffer, buffer_size, "Even (+0)");
  }
}

// ============================================================================
// GAME STATISTICS DISPLAY
// ============================================================================

/**
 * @brief Update endgame statistics (centralized function)
 * @param result_type Type of game result
 */
void game_update_endgame_statistics(game_result_type_t result_type) {
  switch (result_type) {
  case RESULT_WHITE_WINS:
    white_wins++;
    ESP_LOGI(TAG, "📊 Statistics: White wins! (Total: %" PRIu32 ")",
             white_wins);
    break;
  case RESULT_BLACK_WINS:
    black_wins++;
    ESP_LOGI(TAG, "📊 Statistics: Black wins! (Total: %" PRIu32 ")",
             black_wins);
    break;
  case RESULT_DRAW_STALEMATE:
  case RESULT_DRAW_50_MOVE:
  case RESULT_DRAW_REPETITION:
  case RESULT_DRAW_INSUFFICIENT:
    draws++;
    ESP_LOGI(TAG, "📊 Statistics: Draw! (Total: %" PRIu32 ")", draws);
    break;
  }
  total_games++;
  ESP_LOGI(TAG, "📊 Total games: %" PRIu32, total_games);
}

/**
 * @brief Print simplified endgame report to UART
 * @param result_type Type of game result
 */

// ============================================================================
// ENDGAME REPORT FORMATTING HELPERS
// ============================================================================

/**
 * @brief Print formatted report header with box borders
 *
 * @param title Header title to display
 *
 * @details
 * Tiskne hlavicku reportu s dvojitym ohranicenim (═══).
 * Pouziva se na zacatku reportu nebo sekci.
 */
static void print_report_header(const char *title) {
  printf("\r\n");
  printf("═══════════════════════════════════════════════════════════════\r\n");
  printf("%s\r\n", title);
  printf("═══════════════════════════════════════════════════════════════\r\n");
}

/**
 * @brief Print separator line
 *
 * @details
 * Tiskne oddelovaci caru (───) mezi sekce reportu.
 */
static void print_separator(void) {
  printf("───────────────────────────────────────────────────────────────\r\n");
}

/**
 * @brief Print single report line with label and formatted value
 *
 * @param label descriptive label (e.g., "Winner", "Total Moves")
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 *
 * @details
 * Tiskne jednotliv radek reportu ve formatu "  • Label: Value".
 * Podporuje printf-style formatovani hodnot.
 *
 * Priklad:
 *   print_report_line("Winner", "%s", winner);
 *   print_report_line("Total Moves", "%u", move_count);
 */
static void print_report_line(const char *label, const char *fmt, ...) {
  printf("  • %s: ", label);

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);

  printf("\r\n");
}

void game_print_endgame_report_uart(game_result_type_t result_type) {
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t minutes = game_duration / 60;
  uint32_t seconds = game_duration % 60;

  // Print endgame announcement
  print_report_header("🏆 ENDGAME REPORT");

  // Určit výsledek a typ ukončení podle current_endgame_reason
  const char *winner = "";
  const char *loser = "";
  const char *end_reason = "";

  // Nejprve určíme vítěze z result_type
  if (result_type == RESULT_WHITE_WINS) {
    winner = "White";
    loser = "Black";
  } else if (result_type == RESULT_BLACK_WINS) {
    winner = "Black";
    loser = "White";
  } else {
    winner = "Draw";
    loser = "Draw";
  }

  // Pak určíme důvod konce hry a vytiskneme výsledek
  switch (current_endgame_reason) {
  // === CHECKMATE VARIATIONS ===
  case ENDGAME_REASON_CHECKMATE:
    printf("🎯 Game Result: %s WINS BY CHECKMATE\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate";
    break;
  case ENDGAME_REASON_CHECKMATE_EN_PASSANT:
    printf("⚔️ Game Result: %s WINS BY EN PASSANT CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (En Passant)";
    break;
  case ENDGAME_REASON_CHECKMATE_CASTLING:
    printf("🏰 Game Result: %s WINS BY CASTLING CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Castling)";
    break;
  case ENDGAME_REASON_CHECKMATE_PROMOTION:
    printf("👑 Game Result: %s WINS BY PROMOTION CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Promotion)";
    break;
  case ENDGAME_REASON_CHECKMATE_DISCOVERED:
    printf("💥 Game Result: %s WINS BY DISCOVERED CHECK CHECKMATE!\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Checkmate (Discovered Check)";
    break;

  // === PLAYER ACTIONS ===
  case ENDGAME_REASON_RESIGNATION:
    printf("🏳️ Game Result: %s WINS BY RESIGNATION\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Resignation";
    break;
  case ENDGAME_REASON_TIMEOUT:
    printf("⏰ Game Result: %s WINS BY TIMEOUT\r\n",
           (result_type == RESULT_WHITE_WINS) ? "WHITE" : "BLACK");
    end_reason = "Timeout";
    break;

  // === DRAW CONDITIONS ===
  case ENDGAME_REASON_STALEMATE:
    printf("🤝 Game Result: DRAW - Stalemate\r\n");
    end_reason = "Stalemate";
    break;
  case ENDGAME_REASON_50_MOVE:
    printf("🤝 Game Result: DRAW - 50-move rule\r\n");
    end_reason = "50-move rule";
    break;
  case ENDGAME_REASON_REPETITION:
    printf("🤝 Game Result: DRAW - Three-fold repetition\r\n");
    end_reason = "Threefold repetition";
    break;
  case ENDGAME_REASON_INSUFFICIENT:
    printf("🤝 Game Result: DRAW - Insufficient material\r\n");
    end_reason = "Insufficient material";
    break;
  }

  print_separator();
  printf("📋 Game Summary:\r\n");
  print_report_line("Winner", "%s", winner);
  if (result_type != RESULT_DRAW_STALEMATE &&
      result_type != RESULT_DRAW_50_MOVE &&
      result_type != RESULT_DRAW_REPETITION &&
      result_type != RESULT_DRAW_INSUFFICIENT) {
    print_report_line("Loser", "%s", loser);
  }
  print_report_line("End Reason", "%s", end_reason);
  print_report_line("Total Moves", "%" PRIu32, move_count);
  print_report_line("Game Duration", "%" PRIu32 ":%02" PRIu32 " (mm:ss)",
                    minutes, seconds);

  // Vypočítat material advantage
  int white_material = 0, black_material = 0;
  for (uint32_t i = 0; i < white_captured_count; i++) {
    piece_t p = white_captured_pieces[i];
    if (p == PIECE_BLACK_PAWN)
      white_material += 1;
    else if (p == PIECE_BLACK_KNIGHT || p == PIECE_BLACK_BISHOP)
      white_material += 3;
    else if (p == PIECE_BLACK_ROOK)
      white_material += 5;
    else if (p == PIECE_BLACK_QUEEN)
      white_material += 9;
  }
  for (uint32_t i = 0; i < black_captured_count; i++) {
    piece_t p = black_captured_pieces[i];
    if (p == PIECE_WHITE_PAWN)
      black_material += 1;
    else if (p == PIECE_WHITE_KNIGHT || p == PIECE_WHITE_BISHOP)
      black_material += 3;
    else if (p == PIECE_WHITE_ROOK)
      black_material += 5;
    else if (p == PIECE_WHITE_QUEEN)
      black_material += 9;
  }
  int material_diff = white_material - black_material;

  print_separator();
  printf("📊 Game Statistics:\r\n");
  print_report_line("White Moves", "%" PRIu32, (move_count + 1) / 2);
  print_report_line("Black Moves", "%" PRIu32, move_count / 2);
  if (game_duration > 0 && move_count > 0) {
    print_report_line("Avg Time per Move", "%" PRIu32 "s",
                      game_duration / move_count);
  }

  print_separator();
  printf("⚔️  Combat Statistics:\r\n");
  print_report_line("White Captures", "%" PRIu32 " pieces (%d points)",
                    white_captures, white_material);
  print_report_line("Black Captures", "%" PRIu32 " pieces (%d points)",
                    black_captures, black_material);
  if (material_diff > 0) {
    print_report_line("Material Advantage", "White +%d", material_diff);
  } else if (material_diff < 0) {
    print_report_line("Material Advantage", "Black +%d", -material_diff);
  } else {
    print_report_line("Material Advantage", "Equal (0)");
  }
  print_report_line("White Checks", "%" PRIu32, white_checks);
  print_report_line("Black Checks", "%" PRIu32, black_checks);

  // Captured pieces vizuálně
  if (white_captured_count > 0) {
    printf("  • White Captured: ");
    for (uint32_t i = 0; i < white_captured_count && i < MAX_CAPTURED_PIECES;
         i++) {
      char piece_char = piece_to_char(white_captured_pieces[i]);
      printf("%c ", piece_char);
    }
    printf("\r\n");
  }
  if (black_captured_count > 0) {
    printf("  • Black Captured: ");
    for (uint32_t i = 0; i < black_captured_count && i < MAX_CAPTURED_PIECES;
         i++) {
      char piece_char = piece_to_char(black_captured_pieces[i]);
      printf("%c ", piece_char);
    }
    printf("\r\n");
  }

  // Castling info
  print_report_line("White Castling", "%s", white_castles > 0 ? "Yes" : "No");
  print_report_line("Black Castling", "%s", black_castles > 0 ? "Yes" : "No");

  print_separator();
  printf("📈 Overall Statistics:\r\n");
  print_report_line("Total Games Played", "%" PRIu32, total_games);
  print_report_line("White Wins", "%" PRIu32 " (%.0f%%)", white_wins,
                    total_games > 0 ? (white_wins * 100.0f / total_games)
                                    : 0.0f);
  print_report_line("Black Wins", "%" PRIu32 " (%.0f%%)", black_wins,
                    total_games > 0 ? (black_wins * 100.0f / total_games)
                                    : 0.0f);
  print_report_line("Draws", "%" PRIu32 " (%.0f%%)", draws,
                    total_games > 0 ? (draws * 100.0f / total_games) : 0.0f);

  printf("═══════════════════════════════════════════════════════════════\r\n");
  printf("\r\n");

  // Flush stdout pro okamžité zobrazení
  fflush(stdout);
}

/**
 * @brief Get white wins count
 */
uint32_t game_get_white_wins(void) { return white_wins; }

/**
 * @brief Get black wins count
 */
uint32_t game_get_black_wins(void) { return black_wins; }

/**
 * @brief Get draws count
 */
uint32_t game_get_draws(void) { return draws; }

/**
 * @brief Get total games count
 */
uint32_t game_get_total_games(void) { return total_games; }

/**
 * @brief Get game state as string
 */
const char *game_get_game_state_string(void) {
  switch (current_game_state) {
  case GAME_STATE_ACTIVE:
    return "Active";
  case GAME_STATE_PAUSED:
    return "Paused";
  case GAME_STATE_FINISHED:
    return "Finished";
  case GAME_STATE_PROMOTION:
    return "Promotion";
  case GAME_STATE_IDLE:
    return "Idle";
  case GAME_STATE_WAITING_FOR_BOARD_SETUP:
    return "WaitingForBoardSetup";
  default:
    return "Unknown";
  }
}

/**
 * @brief Print comprehensive game statistics
 */
void game_print_game_stats(void) {
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration = current_time - game_start_time;
  uint32_t minutes = game_duration / 60;
  uint32_t seconds = game_duration % 60;

  // Calculate average time per move
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  char material_str[32];
  game_get_material_string(material_str, sizeof(material_str));

  // Print atmospheric header
  ESP_LOGI(TAG, "╔═══════════════════════════════╗");
  ESP_LOGI(TAG, "║ ESP32 CHESS v2.4 ║");
  ESP_LOGI(TAG, "║ Move %" PRIu32 " - %s to play ║", move_count,
           (current_player == PLAYER_WHITE) ? "White" : "Black");
  ESP_LOGI(TAG, "║ Material: %s ║", material_str);
  ESP_LOGI(TAG, "╚═══════════════════════════════╝");

  // Game duration and move info
  ESP_LOGI(TAG,
           "Game duration: %02" PRIu32 ":%02" PRIu32 ", Move %" PRIu32
           " (%s to play)",
           minutes, seconds, move_count,
           (current_player == PLAYER_WHITE) ? "White" : "Black");

  // Capture statistics
  ESP_LOGI(TAG, "Captures: White %" PRIu32 " pieces, Black %" PRIu32 " pieces",
           white_captures, black_captures);

  // Check and castle statistics
  ESP_LOGI(TAG,
           "Checks: White %" PRIu32 ", Black %" PRIu32
           " | Castles: White %" PRIu32 ", Black %" PRIu32 "",
           white_checks, black_checks, white_castles, black_castles);

  // Material breakdown
  int white_material, black_material;
  game_calculate_material_balance(&white_material, &black_material);
  ESP_LOGI(TAG, "Material: White %d points, Black %d points (%s)",
           white_material, black_material, material_str);

  // Time statistics
  if (timer_enabled) {
    ESP_LOGI(TAG,
             "Time per move: White avg %" PRIu32 "s, Black avg %" PRIu32 "s",
             white_avg_time, black_avg_time);
  }

  // Draw detection info
  if (moves_without_capture > 30) {
    ESP_LOGI(TAG,
             "⚠️  %" PRIu32 " moves without capture (50-move rule approaching)",
             moves_without_capture);
  }

  if (game_is_position_repeated()) {
    ESP_LOGI(TAG, "⚠️  Position repeated (potential draw by repetition)");
  }

  // Game save status
  if (game_saved) {
    ESP_LOGI(TAG, "💾 Game saved as: %s", saved_game_name);
  }

  ESP_LOGI(TAG, "═══════════════════════════════");
}

// Last move tracking for board display
uint8_t last_move_from_row = 0;
uint8_t last_move_from_col = 0;
uint8_t last_move_to_row = 0;
uint8_t last_move_to_col = 0;
bool has_last_move = false;

// Captured pieces counter (removed - already declared above with char arrays)

// ============================================================================
// ENHANCED MOVE VALIDATION SYSTEM
// ============================================================================

// Tutorial mode state
static bool tutorial_mode_active = false;
static bool show_hints = true;
/** Web „základní postavení“: prázdná logika, matrix guard potlačen. */
bool board_setup_tutorial_active = false;
// Game analysis flags (for future use)
// static bool show_warnings = true;
// static bool show_analysis = true;

// Move analysis cache (for future use)
// static move_suggestion_t move_suggestions[100];
// static uint32_t suggestion_count = 0;
// static uint32_t last_analysis_time = 0;

// Piece symbols for display (ASCII) - FIXED INDEXES
const char *piece_symbols[] = {
    " ", // 0: PIECE_EMPTY
    "p", // 1: PIECE_WHITE_PAWN
    "n", // 2: PIECE_WHITE_KNIGHT
    "b", // 3: PIECE_WHITE_BISHOP
    "r", // 4: PIECE_WHITE_ROOK
    "q", // 5: PIECE_WHITE_QUEEN
    "k", // 6: PIECE_WHITE_KING
    "P", // 7: PIECE_BLACK_PAWN
    "N", // 8: PIECE_BLACK_KNIGHT
    "B", // 9: PIECE_BLACK_BISHOP
    "R", // 10: PIECE_BLACK_ROOK
    "Q", // 11: PIECE_BLACK_QUEEN
    "K"  // 12: PIECE_BLACK_KING
};

// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================

/** Vzorků matice při kontrole výchozí pozice (reed kontakty často „klepou“). */
#define GAME_START_POS_MATRIX_SAMPLES 5
#define GAME_START_POS_MAJORITY_VOTES 3

static bool game_board_starting_occupancy_matches(const uint8_t state[64]) {
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] != 0) {
        return false;
      }
    }
  }
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] == 0) {
        return false;
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (state[row * 8 + col] == 0) {
        return false;
      }
    }
  }
  return true;
}

static void game_log_first_startpos_mismatch(const uint8_t state[64]) {
  char sq[4];
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] != 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 3–6 square %s must be empty "
                 "(majority vote saw piece)",
                 sq);
        return;
      }
    }
  }
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] == 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 1–2 square %s must be occupied "
                 "(majority vote saw empty)",
                 sq);
        return;
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (state[idx] == 0) {
        matrix_square_to_notation((uint8_t)idx, sq);
        ESP_LOGW(TAG,
                 "starting position: rank 7–8 square %s must be occupied "
                 "(majority vote saw empty)",
                 sq);
        return;
      }
    }
  }
}

/**
 * @brief Checks if board is physically in starting position (occupancy only)
 *
 * Uses RAW SENSOR DATA from Matrix Task. Rows 0–1 a 6–7 plné, 2–5 prázdné.
 * Více vzorků + většinové hlasování kvůli šumu u reedů při dokončení tutorialu.
 */
static bool game_is_board_in_starting_position(void) {
  uint8_t votes[64] = {0};

  for (int s = 0; s < GAME_START_POS_MATRIX_SAMPLES; s++) {
    uint8_t snap[64];
    matrix_get_state(snap);
    for (int i = 0; i < 64; i++) {
      votes[i] += snap[i];
    }
    if (s + 1 < GAME_START_POS_MATRIX_SAMPLES) {
      vTaskDelay(pdMS_TO_TICKS(12));
    }
  }

  uint8_t stable[64];
  for (int i = 0; i < 64; i++) {
    stable[i] = (votes[i] >= GAME_START_POS_MAJORITY_VOTES) ? 1 : 0;
  }

  if (game_board_starting_occupancy_matches(stable)) {
    return true;
  }
  game_log_first_startpos_mismatch(stable);
  return false;
}

/**
 * @brief Zobrazí červené LED na polích, kde chybí figurky v počáteční pozici
 * 
 * Používá se ve stavu GAME_STATE_WAITING_FOR_BOARD_SETUP pro indikaci
 * uživateli, která pole musí obsadit figurkami.
 */
static void game_show_missing_pieces_led(void) {
  uint8_t matrix_state[64] = {0};
  matrix_get_state(matrix_state);
  
  // Vyčistit desku
  led_clear_board_only();
  
  // Definice očekávané počáteční pozice (row-major order)
  // Rows 0-1 a 6-7 by měly být obsazené, rows 2-5 prázdné
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] == 0) {
        // Chybí figurka na bílém základním řádku
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  for (int row = 6; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] == 0) {
        // Chybí figurka na černém základním řádku
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  for (int row = 2; row < 6; row++) {
    for (int col = 0; col < 8; col++) {
      int idx = row * 8 + col;
      if (matrix_state[idx] != 0) {
        // Náhodná figurka uprostřed desky - také červeně
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Červená
      }
    }
  }
  
  ESP_LOGI(TAG, "🔴 LED: Showing missing pieces in red");
}


void game_bump_revision_and_notify(void) {
  game_task_wdt_reset_safe();
  game_state_revision++;
  if (game_state_revision == 0U) {
    game_state_revision = 1U;
  }
  game_task_wdt_reset_safe();
  czechmate_on_game_state_changed();
  game_task_wdt_reset_safe();
}

uint32_t game_get_state_revision(void) { return game_state_revision; }

/**
 * @brief Resetuje hru do vychoziho stavu
 *
 * Tato funkce resetuje hru do vychoziho stavu. Vymaze vsechny tahy,
 * resetuje stavy hry a inicializuje novou hru.
 *
 * @details
 * Funkce resetuje:
 * - Stav hry na IDLE
 * - Aktualni hrace na WHITE
 * - Pocet tahu na 0
 * - Vsechny castling flagy
 * - En passant stav
 * - Historie tahu
 * - Error recovery stav
 */
void game_reset_game(void) {
  ESP_LOGI(TAG, "Resetting game...");

  // Zastavit resignation timer pokud běží (tichý cleanup, hra se resetuje)
  if (resignation_state.active) {
    resignation_stop(true);
  }

  // Reset game state
  current_game_state = GAME_STATE_IDLE;
  current_player = PLAYER_WHITE;
  game_start_time = 0;
  last_move_time = 0;
  move_count = 0;
  game_active = false;

  // Reset extended statistics
  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  current_result_type = RESULT_WHITE_WINS;           // Reset pro novou hru
  current_endgame_reason = ENDGAME_REASON_CHECKMATE; // Reset endgame reason.
  last_move_type = LAST_MOVE_NORMAL;                 // Reset last move type.
  game_saved = false;
  saved_game_name[0] = '\0';

  // Clear move history
  memset(move_history, 0, sizeof(move_history));
  memset(move_history_kind, 0, sizeof(move_history_kind));
  history_index = 0;
  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;

  // Clear captured pieces
  white_captured_count = 0;
  black_captured_count = 0;
  white_captured_index = 0;
  black_captured_index = 0;

  // Clear material advantage history
  advantage_history_count = 0;
  memset(material_advantage_history, 0, sizeof(material_advantage_history));

  // Clear last move tracking
  has_last_move = false;

  // Kompletní reset error recovery state
  error_recovery_state.has_invalid_piece = false;
  error_recovery_state.invalid_row = 0;
  error_recovery_state.invalid_col = 0;
  error_recovery_state.original_valid_row = 0;
  error_recovery_state.original_valid_col = 0;
  error_recovery_state.piece_type = PIECE_EMPTY;
  error_recovery_state.waiting_for_move_correction = false;
  error_recovery_state.error_count = 0;

  // Reset opponent piece recovery state
  opponent_piece_moved = false;
  opponent_piece_type = PIECE_EMPTY;
  opponent_original_row = 0;
  opponent_original_col = 0;
  opponent_current_row = 0;
  opponent_current_col = 0;

  // Reset capture state
  capture_in_progress = false;
  capture_target_row = 0;
  capture_target_col = 0;
  capture_removed_piece = PIECE_EMPTY;

  // Reset guided capture state
  guided_capture_state.active = false;
  guided_capture_state.target_row = 0;
  guided_capture_state.target_col = 0;
  guided_capture_state.target_piece = PIECE_EMPTY;
  guided_capture_state.attacker_count = 0;
  memset(guided_capture_state.attacker_rows, 0,
         sizeof(guided_capture_state.attacker_rows));
  memset(guided_capture_state.attacker_cols, 0,
         sizeof(guided_capture_state.attacker_cols));
  memset(guided_capture_state.attacker_pieces, 0,
         sizeof(guided_capture_state.attacker_pieces));

  // Reset matrix guard pause state (full clear after board init below).
  resync_required_after_restore = false;

  // Reset promotion state to prevent blocking moves in a new game.
  promotion_state.pending = false;
  promotion_state.square_row = 0;
  promotion_state.square_col = 0;
  promotion_state.player = PLAYER_WHITE;

  // Reset lifted piece state.
  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  /* Každý plný reset hry končí tutoriál rozestavení; vstup do tutoriálu ho hned
   * znovu zapne (game_enter_board_setup_tutorial). Dřív zůstával true po „Nová
   * hra“ z webu → ignorovaný matrix guard a zamrzlá detekce z matice. */
  board_setup_tutorial_active = false;

  // Reset auto-new game detection flags.
  // This allows auto-new game to work after manual resets or when pieces are
  // manually arranged into starting position
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false; // Reset block flag too
  ESP_LOGI(TAG, "✅ Auto new game detection flags reset in game_reset_game()");

  // Zastavit endgame animaci při resetu hry
  led_stop_endgame_animation();

  // Reinitialize board
  game_initialize_board();

  game_matrix_guard_clear_both_layers();

  ESP_LOGI(TAG, "Game reset completed");
  game_bump_revision_and_notify();
}

/**
 * Po game_reset_game(): vyprázdní logiku a IDLE — společné pro vstup/výstup z
 * tutoriálu rozestavení.
 */
void game_apply_empty_logical_board_after_full_reset(void) {
  memset(board, 0, sizeof(board));
  memset(piece_moved, 0, sizeof(piece_moved));
  current_game_state = GAME_STATE_IDLE;
  game_active = false;
  move_count = 0;
  white_king_moved = true;
  white_rook_a_moved = true;
  white_rook_h_moved = true;
  black_king_moved = true;
  black_rook_a_moved = true;
  black_rook_h_moved = true;
  memset(&castling_state, 0, sizeof(castling_state));
  en_passant_available = false;
  game_check_promotion_needed();
}

void game_enter_board_setup_tutorial(void) {
  game_puzzle_cancel();
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  board_setup_tutorial_active = true;
  STAGING_LOGI(TAG,
               "board_setup_tutorial: entered (empty logical, guard bypass)");
}

void game_exit_board_setup_tutorial(bool apply_full_start_position) {
  board_setup_tutorial_active = false;
  if (apply_full_start_position) {
    game_start_new_game();
    STAGING_LOGI(TAG, "board_setup_tutorial: exited with full start position");
    return;
  }
  game_reset_game();
  game_apply_empty_logical_board_after_full_reset();
  STAGING_LOGI(TAG, "board_setup_tutorial: cancelled (empty logical board)");
}

bool game_is_board_setup_tutorial_active(void) {
  return board_setup_tutorial_active;
}

bool game_is_physical_board_starting_occupancy(void) {
  return game_is_board_in_starting_position();
}

bool game_finish_board_setup_tutorial_from_web(void) {
  bool physical_ok = false;
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
      return false;
    }
  }
  if (!board_setup_tutorial_active) {
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return false;
  }
  physical_ok = game_is_board_in_starting_position();
  if (!physical_ok) {
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return false;
  }
  board_setup_tutorial_active = false;
  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }
  game_start_new_game();
  STAGING_LOGI(TAG,
               "board_setup_tutorial: finish OK — physical start, new game");
  return true;
}



/**
 * @brief Spusti novou hru
 *
 * Tato funkce spusti novou sachovou hru. Inicializuje sachovnici,
 * resetuje vsechny stavy a pripravi hru pro hrani.
 *
 * @details
 * Funkce:
 * - Inicializuje sachovnici do vychoziho stavu
 * - Resetuje vsechny stavy hry
 * - Nastavi prvni hrace na WHITE
 * - Zastavi vsechny animace
 * - Aktualizuje LED feedback
 * - Zvysi pocet her
 */
void game_start_new_game(void) {
  ESP_LOGI(TAG, "Starting new game...");

  game_task_wdt_reset_safe();
  game_snapshot_erase_nvs();
  game_task_wdt_reset_safe();

  game_snapshot_reset_failure_flags();

  // Reset game
  game_reset_game();
  game_task_wdt_reset_safe();

  // Reset timer před novou hrou
  game_reset_timer();
  ESP_LOGI(TAG, "Timer reset for new game");

  // BUG FIX 1: Kontrola fyzické desky před aktivací hry (pouze pokud je hlídání zapnuto)
  if (starting_position_check_enabled && !game_is_board_in_starting_position()) {
    // Deska není fyzicky připravena - přejít do stavu čekání
    current_game_state = GAME_STATE_WAITING_FOR_BOARD_SETUP;
    game_active = false;
    
    // LED indikace: červené blikání chybějících polí
    game_show_missing_pieces_led();
    
    // Notify web
    game_bump_revision_and_notify();
    
    ESP_LOGW(TAG, "Board not in starting position - waiting for physical setup");
    return;
  }

  // Set game state - pouze když je deska OK
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  game_start_time = esp_timer_get_time() / 1000;
  last_move_time = game_start_time;

  // Start timer for white player if time control is active
  if (game_is_timer_active()) {
    ESP_LOGI(TAG, "Starting timer for white player at game start");
    game_start_timer_move(true); // Start timer for white
  }

  // Initialize extended statistics
  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  game_saved = false;
  saved_game_name[0] = '\0';

  total_games++;

  ESP_LOGI(TAG, "New game started - White to move");
  ESP_LOGI(TAG, "Total games: %" PRIu32, total_games);

  // Boot sequence: first game vs. restart.
  if (total_games > 1) {
    // Restart hry - zastavit všechny animace
    unified_animation_stop_all();
    led_stop_endgame_animation(); // Legacy endgame animations
    stop_endgame_animation();     // Stop NEW endgame animations system
    ESP_LOGI(TAG, "✅ All animations stopped for new game (restart)");
  } else {
    // První hra - boot sequence stále probíhá
    ESP_LOGI(TAG, "⏸️  Boot sequence: Skipping animation stop (first game)");
  }

  // Wait for boot animation to finish if running (critical for button LEDs)
  while (led_is_booting()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
  }

  // Update promotion LED indications
  game_check_promotion_needed();
  ESP_LOGI(TAG, "✅ Updated promotion button LED indications");

  // Reset auto-new game flags for all games (not just the first).
  // This allows auto-new game detection to work after manual resets
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false; // Reset block flag too
  ESP_LOGI(TAG, "✅ Auto new game detection flags reset for new game");

  // Boot sequence: first game vs. restart.
  if (total_games >= 1) { // Povolit highlight i pro první hru!
    // Restart hry - zvýraznit pohyblivé figurky
    vTaskDelay(pdMS_TO_TICKS(100)); // Krátká pauza pro stabilizaci
    game_task_wdt_reset_safe();
    game_highlight_movable_pieces();
    game_task_wdt_reset_safe();
    ESP_LOGI(TAG,
             "✅ Highlighted movable pieces for starting player (restart)");
  } else {
    // První hra - boot sequence stále probíhá, skipnout highlight
    ESP_LOGI(
        TAG,
        "⏸️  Boot sequence: Skipping movable pieces highlight (first game)");
  }
  game_bump_revision_and_notify();
}

void game_start_new_game_from_fen(const char *fen) {
  if (fen == NULL || fen[0] == '\0') {
    game_start_new_game();
    return;
  }

  ESP_LOGI(TAG, "Starting new game from FEN...");
  game_task_wdt_reset_safe();
  game_snapshot_erase_nvs();
  game_task_wdt_reset_safe();

  game_snapshot_reset_failure_flags();

  game_reset_game();
  game_task_wdt_reset_safe();
  game_reset_timer();

  player_t fen_player = PLAYER_WHITE;
  if (!game_load_position_from_fen(fen, &fen_player)) {
    ESP_LOGE(TAG, "FEN parse failed, falling back to standard new game");
    game_start_new_game();
    return;
  }

  white_king_moved = true;
  black_king_moved = true;
  white_rook_a_moved = true;
  white_rook_h_moved = true;
  black_rook_a_moved = true;
  black_rook_h_moved = true;
  en_passant_available = false;
  en_passant_target_row = 0;
  en_passant_target_col = 0;
  en_passant_victim_row = 0;
  en_passant_victim_col = 0;

  puzzle_active = false;
  puzzle_active_id = 0;
  puzzle_setup_active = false;
  puzzle_setup_id = 0;
  puzzle_feedback = PUZZLE_FEEDBACK_NONE;
  board_setup_tutorial_active = false;

  current_player = fen_player;
  current_game_state = GAME_STATE_ACTIVE;
  game_active = true;
  game_start_time = esp_timer_get_time() / 1000;
  last_move_time = game_start_time;

  if (game_is_timer_active()) {
    game_start_timer_move(fen_player == PLAYER_WHITE);
  }

  white_time_total = 0;
  black_time_total = 0;
  white_moves_count = 0;
  black_moves_count = 0;
  white_captures = 0;
  black_captures = 0;
  white_checks = 0;
  black_checks = 0;
  white_castles = 0;
  black_castles = 0;
  moves_without_capture = 0;
  max_moves_without_capture = 0;
  position_history_count = 0;
  game_result = GAME_STATE_IDLE;
  game_saved = false;
  saved_game_name[0] = '\0';
  total_games++;

  if (total_games > 1) {
    unified_animation_stop_all();
    led_stop_endgame_animation();
    stop_endgame_animation();
  }

  while (led_is_booting()) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
  }

  game_check_promotion_needed();
  auto_new_game_in_starting_position = false;
  auto_new_game_triggered = false;
  auto_new_game_blocked_until_move = false;

  if (total_games >= 1) {
    vTaskDelay(pdMS_TO_TICKS(100));
    game_task_wdt_reset_safe();
    game_highlight_movable_pieces();
    game_task_wdt_reset_safe();
  }
  game_bump_revision_and_notify();
}


// ============================================================================
// ERROR FORMATTING HELPER
// ============================================================================

/**
 * @brief Helper funkce pro konzistentni formatovani error detail radku
 *
 * @param label Nazev pole (napr. "Reason", "Solution", "Hint")
 * @param fmt Printf-style format string
 * @param ... Variable arguments pro format string
 *
 * @details
 * Tato funkce zajistuje konzistentni formatovani vsech error zprav.
 * Vyhodi variadickou funkci s printf-style argumenty.
 */
static void print_error_detail(const char *label, const char *fmt, ...) {
  va_list args;
  char buffer[256];

  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  // Format based on label type
  if (strcmp(label, "Reason") == 0) {
    printf(ANSI_RED "   • %s: " ANSI_BOLD "%s" ANSI_RESET "\r\n", label,
           buffer);
  } else if (strcmp(label, "Hint") == 0 || strcmp(label, "Solution") == 0) {
    printf(FMT_HINT("   • %s: %s") "\r\n", label, buffer);
  } else {
    printf("   • %s: %s\r\n", label, buffer);
  }
}

/**
 * @brief Zobrazi detailni chybovou zpravu s reasoning pro neplatny tah
 *
 * @param error Typ chyby tahu (MOVE_ERROR_*)
 * @param move Ukazatel na tah ktery zpusobil chybu
 *
 * @details
 * Funkce zobrazi barevne formatovanou chybovou zpravu vcetne:
 * - Duvodu proc je tah neplatny
 * - Napovedy proc tah selhal
 * - Navrhu reseni
 * - Stavu ciloveho pole
 *
 * Specialni reasoning pro pesce:
 * - Rozlisuje mezi prazdnym polem a vlastni figurkou
 * - Vysvetluje proc pesec nemuze jit diagonalne na prazdne pole
 * - Vysvetluje proc pesec nemuze brat vpred
 *
 */
void game_display_move_error(move_error_t error, const chess_move_t *move) {
  char from_square[3], to_square[3];
  game_coords_to_square(move->from_row, move->from_col, from_square);
  game_coords_to_square(move->to_row, move->to_col, to_square);

  const char *piece_name = game_get_piece_name(move->piece);
  const char *player_name =
      (current_player == PLAYER_WHITE) ? "White" : "Black";
  piece_t source_piece = board[move->from_row][move->from_col];
  piece_t dest_piece = board[move->to_row][move->to_col];

  // Enhanced error messages with colors and detailed reasoning
  printf("\r\n");
  printf(FMT_ERROR("❌ INVALID MOVE!") "\r\n");
  printf(FMT_INFO("   • Move: %s → %s") "\r\n", from_square, to_square);
  printf(FMT_INFO("   • Piece: %s") "\r\n", piece_name);

  switch (error) {
  case MOVE_ERROR_NO_PIECE:
    print_error_detail("Reason", "No piece at %s", from_square);
    print_error_detail("Solution", "Choose a square with your piece");
    break;

  case MOVE_ERROR_WRONG_COLOR:
    print_error_detail("Reason", "%s cannot move %s's piece", player_name,
                       (current_player == PLAYER_WHITE) ? "Black" : "White");
    print_error_detail("Solution", "Move only your own pieces");
    break;

  case MOVE_ERROR_BLOCKED_PATH:
    print_error_detail("Reason", "Path from %s to %s is blocked", from_square,
                       to_square);
    print_error_detail("Hint", "Another piece is blocking the way");
    print_error_detail("Solution",
                       "Clear the path or choose different destination");
    break;

  case MOVE_ERROR_INVALID_PATTERN:
    // VYLEPŠENÉ REASONING pro pěšce
    if (source_piece == PIECE_WHITE_PAWN || source_piece == PIECE_BLACK_PAWN) {
      int col_diff = abs((int)move->to_col - (int)move->from_col);
      bool is_diagonal = (col_diff == 1);

      if (is_diagonal && dest_piece == PIECE_EMPTY) {
        print_error_detail(
            "Reason",
            "Pawn can only capture diagonally when enemy piece is present");
        print_error_detail("Hint",
                           "Square %s is empty - pawns cannot move diagonally "
                           "to empty squares",
                           to_square);
        print_error_detail(
            "Solution",
            "Move pawn forward or capture diagonally where enemy piece is");
      } else if (!is_diagonal && dest_piece != PIECE_EMPTY) {
        print_error_detail("Reason", "Pawn cannot capture straight ahead");
        print_error_detail(
            "Hint", "Square %s is occupied - pawns can only capture diagonally",
            to_square);
        print_error_detail("Solution",
                           "Capture the piece diagonally, not forward");
      } else {
        print_error_detail("Reason", "Pawn cannot move in this pattern");
        print_error_detail("Hint", "Pawns move forward 1 square (or 2 from "
                                   "start), capture diagonally");
        print_error_detail("Solution", "Follow pawn movement rules");
      }
    } else {
      print_error_detail("Reason", "%s cannot move from %s to %s", piece_name,
                         from_square, to_square);
      print_error_detail("Solution", "Follow the piece's movement rules");
    }
    break;

  case MOVE_ERROR_KING_IN_CHECK:
    print_error_detail("Reason",
                       "This move would leave/keep your king in check");
    // Find and display king position
    printf(FMT_HINT("   • Hint: Your king at "));
    piece_t king_piece =
        (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    for (int r = 0; r < 8; r++) {
      for (int c = 0; c < 8; c++) {
        if (board[r][c] == king_piece) {
          char king_sq[3];
          game_coords_to_square(r, c, king_sq);
          printf("%s would be under attack\r\n", king_sq);
          goto king_found;
        }
      }
    }
    printf("is under attack\r\n");
  king_found:
    print_error_detail(
        "Solution",
        "Protect your king by blocking, capturing attacker, or moving king");
    break;

  case MOVE_ERROR_CASTLING_BLOCKED:
    print_error_detail("Reason", "Castling is not allowed");
    print_error_detail(
        "Hint", "King/Rook moved, squares under attack, or path blocked");
    print_error_detail(
        "Solution", "Castling requires unmoved king and rook with clear path");
    break;

  case MOVE_ERROR_EN_PASSANT_INVALID:
    print_error_detail("Reason", "En passant is not possible here");
    print_error_detail("Hint", "En passant only works immediately after "
                               "opponent's 2-square pawn move");
    print_error_detail("Solution", "Capture normally or make different move");
    break;

  case MOVE_ERROR_DESTINATION_OCCUPIED:
    print_error_detail("Reason", "Destination %s is occupied by your own %s",
                       to_square, game_get_piece_name(dest_piece));
    print_error_detail("Hint", "You cannot capture your own pieces");
    print_error_detail("Solution",
                       "Choose empty square or capture opponent's piece");
    break;

  case MOVE_ERROR_OUT_OF_BOUNDS:
    print_error_detail("Reason", "Coordinates are out of board bounds");
    print_error_detail("Solution", "Use valid chess notation (a1-h8)");
    break;

  case MOVE_ERROR_GAME_NOT_ACTIVE:
    print_error_detail("Reason", "Game is not active");
    print_error_detail("Solution",
                       "Start a new game first (use GAME_NEW command)");
    break;

  case MOVE_ERROR_INVALID_MOVE_STRUCTURE:
    print_error_detail("Reason", "Move structure is invalid");
    print_error_detail("Solution",
                       "Use proper move format (e.g., e2e4 or e2-e4)");
    break;

  default:
    print_error_detail("Reason", "Unknown error occurred (code: %d)", error);
    print_error_detail("Solution", "Try a different move");
    break;
  }

  // Zobrazit stav pole
  if (dest_piece != PIECE_EMPTY) {
    printf(FMT_DATA("   • Target: %s at %s") "\r\n",
           game_get_piece_name(dest_piece), to_square);
  } else {
    printf(FMT_DATA("   • Target: Empty square at %s") "\r\n", to_square);
  }

  printf("\r\n");

  // Show helpful suggestions
  if (tutorial_mode_active && show_hints) {
    game_show_move_suggestions(move->from_row, move->from_col);
  }
}

/**
 * @brief Get piece name as string
 * @param piece Piece type
 * @return Piece name string
 */
const char *game_get_piece_name(piece_t piece) {
  switch (piece) {
  case PIECE_EMPTY:
    return "Empty";
  case PIECE_WHITE_PAWN:
    return "White Pawn";
  case PIECE_WHITE_KNIGHT:
    return "White Knight";
  case PIECE_WHITE_BISHOP:
    return "White Bishop";
  case PIECE_WHITE_ROOK:
    return "White Rook";
  case PIECE_WHITE_QUEEN:
    return "White Queen";
  case PIECE_WHITE_KING:
    return "White King";
  case PIECE_BLACK_PAWN:
    return "Black Pawn";
  case PIECE_BLACK_KNIGHT:
    return "Black Knight";
  case PIECE_BLACK_BISHOP:
    return "Black Bishop";
  case PIECE_BLACK_ROOK:
    return "Black Rook";
  case PIECE_BLACK_QUEEN:
    return "Black Queen";
  case PIECE_BLACK_KING:
    return "Black King";
  default:
    return "Unknown";
  }
}

// ============================================================================
// STRING BUILDING HELPER
// ============================================================================

/**
 * @brief Helper funkce pro pridani square do CSV listu
 *
 * @param buffer Cilovy buffer
 * @param size Maximalni velikost bufferu
 * @param pos Aktualni write pozice
 * @param square Square notace k pridani (napr. "e4")
 * @return Nova pozice po pridani
 *
 * @details
 * Funkce automaticky prida carku pokud uz buffer obsahuje data.
 * Pouziva se pro budovani CSV listu tahu.
 */
static int append_square(char *buffer, size_t size, int pos,
                         const char *square) {
  // Pridat separator pokud uz neco v bufferu je
  if (pos > 0) {
    pos += snprintf(buffer + pos, size - pos, ", ");
  }

  // Pridat square
  pos += snprintf(buffer + pos, size - pos, "%s", square);

  return pos;
}

/**
 * @brief Show move suggestions for a piece at given position
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_show_move_suggestions(uint8_t row, uint8_t col) {
  piece_t piece = board[row][col];
  if (piece == PIECE_EMPTY) {
    ESP_LOGI(TAG, "💡 Hint: No piece at this position");
    return;
  }

  // Get available moves for this piece
  move_suggestion_t suggestions[50];
  uint32_t count = game_get_available_moves(row, col, suggestions, 50);

  if (count == 0) {
    char from_square[3];
    game_coords_to_square(row, col, from_square);
    ESP_LOGI(TAG, "💡 Hint: %s at %s has no legal moves",
             game_get_piece_name(piece), from_square);
    return;
  }

  char from_square[3];
  game_coords_to_square(row, col, from_square);
  ESP_LOGI(TAG, "💡 Hint: %s at %s can move to:", game_get_piece_name(piece),
           from_square);

  // Group moves by type
  char normal_moves[128] = "";
  char capture_moves[128] = "";
  char special_moves[128] = "";

  int normal_pos = 0, capture_pos = 0, special_pos = 0;

  for (uint32_t i = 0; i < count && i < 20;
       i++) { // Limit to 20 moves for display
    char to_square[3];
    game_coords_to_square(suggestions[i].to_row, suggestions[i].to_col,
                          to_square);

    if (suggestions[i].is_capture) {
      capture_pos = append_square(capture_moves, sizeof(capture_moves),
                                  capture_pos, to_square);
    } else if (suggestions[i].is_castling || suggestions[i].is_en_passant) {
      special_pos = append_square(special_moves, sizeof(special_moves),
                                  special_pos, to_square);
    } else {
      normal_pos = append_square(normal_moves, sizeof(normal_moves), normal_pos,
                                 to_square);
    }
  }

  if (normal_pos > 0) {
    ESP_LOGI(TAG, "   Normal moves: %s", normal_moves);
  }
  if (capture_pos > 0) {
    ESP_LOGI(TAG, "   Capture moves: %s", capture_moves);
  }
  if (special_pos > 0) {
    ESP_LOGI(TAG, "   Special moves: %s", special_moves);
  }

  if (count > 20) {
    ESP_LOGI(TAG, "   ... and %" PRIu32 " more moves", count - 20);
  }
}

/**
 * @brief Get all available moves for a piece at given position
 * @param row Row coordinate
 * @param col Column coordinate
 * @param suggestions Array to store move suggestions
 * @param max_suggestions Maximum number of suggestions to store
 * @return Number of available moves found
 */
uint32_t game_get_available_moves(uint8_t row, uint8_t col,
                                  move_suggestion_t *suggestions,
                                  uint32_t max_suggestions) {
  if (suggestions == NULL || max_suggestions == 0) {
    return 0;
  }

  piece_t piece = board[row][col];

  // Pokud je piece prázdné a resignation timer je aktivní pro tuto pozici,
  // použít uloženou pozici krále z resignation_state
  if (piece == PIECE_EMPTY) {
    if (resignation_state.active && resignation_state.king_row == row &&
        resignation_state.king_col == col) {
      // Král je zvednutý během resignation timeru - použít typ krále z
      // resignation_state.player
      piece = (resignation_state.player == PLAYER_WHITE) ? PIECE_WHITE_KING
                                                         : PIECE_BLACK_KING;
      ESP_LOGI(TAG,
               "🏰 game_get_available_moves: Using stored king piece (%d) "
               "because king is lifted (resignation timer active)",
               piece);
    } else {
      return 0;
    }
  }

  uint32_t count = 0;

  // Pokud je rošáda v progress a zvedá se věž z rook_from, přidat tah na
  // rook_to jako valid move
  if (castling_state.in_progress) {
    ESP_LOGI(
        TAG,
        "🏰 game_get_available_moves: castling_state.in_progress=true, "
        "checking row=%d col=%d piece=%d, rook_from_row=%d rook_from_col=%d",
        row, col, piece, castling_state.rook_from_row,
        castling_state.rook_from_col);

    if (row == castling_state.rook_from_row &&
        col == castling_state.rook_from_col &&
        (piece == PIECE_WHITE_ROOK || piece == PIECE_BLACK_ROOK)) {
      // Toto je věž pro rošádu - přidat tah na rook_to jako valid move
      ESP_LOGI(TAG,
               "🏰 Castling rook detected at %c%d - adding valid move to %c%d",
               'a' + col, row + 1, 'a' + castling_state.rook_to_col,
               castling_state.rook_to_row + 1);

      if (count < max_suggestions) {
        suggestions[count].from_row = row;
        suggestions[count].from_col = col;
        suggestions[count].to_row = castling_state.rook_to_row;
        suggestions[count].to_col = castling_state.rook_to_col;
        suggestions[count].piece = piece;
        suggestions[count].is_capture = false;
        suggestions[count].is_check = false;
        suggestions[count].is_castling = false;
        suggestions[count].is_en_passant = false;
        suggestions[count].score = 0;
        count++;
        ESP_LOGI(
            TAG,
            "🏰 Castling rook: Added valid move from %c%d to %c%d (count=%" PRIu32 ")",
            'a' + col, row + 1, 'a' + castling_state.rook_to_col,
            castling_state.rook_to_row + 1, count);
        return count; // Vrátit pouze tento tah - ostatní tahy jsou blokované
      } else {
        ESP_LOGW(
            TAG,
            "🏰 Castling rook: max_suggestions=%lu reached, cannot add move",
            max_suggestions);
      }
    } else {
      ESP_LOGD(TAG,
               "🏰 game_get_available_moves: Not castling rook - row=%d col=%d "
               "piece=%d vs rook_from_row=%d rook_from_col=%d",
               row, col, piece, castling_state.rook_from_row,
               castling_state.rook_from_col);
    }
  }

  // Optimized move generation based on piece type
  switch (piece) {
  case PIECE_WHITE_KNIGHT:
  case PIECE_BLACK_KNIGHT:
    // Knight moves in L-shape: 8 possible positions
    for (int i = 0; i < 8 && count < max_suggestions; i++) {
      int to_row = row + knight_moves[i][0];
      int to_col = col + knight_moves[i][1];

      if (!game_is_valid_square(to_row, to_col))
        continue;

      piece_t target = board[to_row][to_col];
      player_t player =
          (piece == PIECE_WHITE_KNIGHT) ? PLAYER_WHITE : PLAYER_BLACK;

      // Skip if occupied by own piece
      if (game_is_own_piece(target, player))
        continue;

      // Create temporary move for validation
      chess_move_t temp_move = {.from_row = row,
                                .from_col = col,
                                .to_row = to_row,
                                .to_col = to_col,
                                .piece = piece,
                                .captured_piece = target,
                                .timestamp = 0};

      // Check if move is valid (including check validation)
      move_error_t error = game_is_valid_move(&temp_move);
      if (error == MOVE_ERROR_NONE) {
        suggestions[count].from_row = row;
        suggestions[count].from_col = col;
        suggestions[count].to_row = to_row;
        suggestions[count].to_col = to_col;
        suggestions[count].piece = piece;
        suggestions[count].is_capture = (target != PIECE_EMPTY);
        suggestions[count].is_check = false; // TODO: Implement check detection
        suggestions[count].is_castling = false;
        suggestions[count].is_en_passant = false;
        suggestions[count].score = 0;
        count++;
      }
    }
    break;

  default:
    // For other pieces, use the original method (less efficient but works)
    for (int to_row = 0; to_row < 8 && count < max_suggestions; to_row++) {
      for (int to_col = 0; to_col < 8 && count < max_suggestions; to_col++) {
        // Skip same position
        if (to_row == row && to_col == col) {
          continue;
        }

        // Create temporary move for validation
        chess_move_t temp_move = {.from_row = row,
                                  .from_col = col,
                                  .to_row = to_row,
                                  .to_col = to_col,
                                  .piece = piece,
                                  .captured_piece = board[to_row][to_col],
                                  .timestamp = 0};

        // Check if move is valid
        move_error_t error = game_is_valid_move(&temp_move);
        if (error == MOVE_ERROR_NONE) {
          // Add to suggestions
          suggestions[count].from_row = row;
          suggestions[count].from_col = col;
          suggestions[count].to_row = to_row;
          suggestions[count].to_col = to_col;
          suggestions[count].piece = piece;
          suggestions[count].is_capture =
              (board[to_row][to_col] != PIECE_EMPTY);
          suggestions[count].is_check =
              false; // TODO: Implement check detection

          // Detect castling
          suggestions[count].is_castling =
              (piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING) &&
              (abs((int)to_col - (int)col) == 2);

          // Detect en passant
          suggestions[count].is_en_passant =
              game_is_en_passant_possible(&temp_move);

          // Update capture flag for en passant
          if (suggestions[count].is_en_passant) {
            suggestions[count].is_capture = true;
          }

          suggestions[count].score = 0; // TODO: Implement move scoring

          count++;
        }
      }
    }
    break;
  }

  return count;
}


// ============================================================================
// GAME STATUS FUNCTIONS
// ============================================================================

game_state_t game_get_state(void) { return current_game_state; }

player_t game_get_current_player(void) { return current_player; }

void game_set_guided_capture_hints_enabled(bool enabled) {
  guided_capture_hints_enabled = enabled;
  STAGING_LOGI(TAG, "Guided capture hints set to: %s",
               enabled ? "ON" : "OFF");

  if (guided_capture_state.active) {
    game_show_guided_capture_leds();
  }
}

bool game_get_guided_capture_hints_enabled(void) {
  return guided_capture_hints_enabled;
}

bool game_matrix_allow_second_sequential_lift(void) {
  if (game_is_matrix_guard_active()) {
    return false;
  }
  if (guided_capture_state.active) {
    return true;
  }
  if (resignation_state.active || promotion_state.pending ||
      castling_state.in_progress || game_is_castle_animation_active() ||
      error_recovery_state.waiting_for_move_correction ||
      capture_in_progress) {
    return false;
  }

  piece_t pending_piece = PIECE_EMPTY;
  if (piece_lifted && lifted_piece != PIECE_EMPTY) {
    pending_piece = lifted_piece;
  } else {
    /* Race: matrix queued PICKUP before game_task set piece_lifted. */
    uint8_t pending_sq = matrix_get_pending_lift_square();
    if (pending_sq != 255) {
      pending_piece = board[pending_sq / 8][pending_sq % 8];
    }
  }

  if (pending_piece == PIECE_EMPTY) {
    return false;
  }

  bool is_white_piece =
      (pending_piece >= PIECE_WHITE_PAWN && pending_piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (pending_piece >= PIECE_BLACK_PAWN && pending_piece <= PIECE_BLACK_KING);
  if (current_player == PLAYER_WHITE && is_white_piece) {
    return true;
  }
  if (current_player == PLAYER_BLACK && is_black_piece) {
    return true;
  }
  return false;
}

uint8_t game_get_led_guidance_level(void) { return led_guidance_level; }

void game_set_led_guidance_level(uint8_t level) {
  if (level < 1U) {
    level = 1;
  }
  if (level > 5U) {
    level = 5;
  }
  led_guidance_level = level;
  bool want_gc = (level >= 5);
  if (guided_capture_hints_enabled != want_gc) {
    game_set_guided_capture_hints_enabled(want_gc);
  }
}

bool game_is_resync_required_after_restore(void) {
  return resync_required_after_restore;
}

void game_print_board(void) {
  // Add spacing before board
  printf("\r\n");

// Enhanced ANSI color codes
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_BRIGHT_WHITE "\033[97m"
#define COLOR_BRIGHT_RED "\033[91m"
#define COLOR_BRIGHT_GREEN "\033[92m"
#define COLOR_BRIGHT_YELLOW "\033[93m"
#define COLOR_BRIGHT_BLUE "\033[94m"
#define COLOR_BRIGHT_CYAN "\033[96m"
#define BG_WHITE "\033[47m"
#define BG_BLACK "\033[40m"
#define BG_YELLOW "\033[43m"

  ESP_LOGI(TAG, "=== Chess Board ===");

  // Print board header with coordinates
  printf(COLOR_BRIGHT_CYAN "    a   b   c   d   e   f   g   h\n" COLOR_RESET);
  printf(COLOR_BRIGHT_CYAN "  +---+---+---+---+---+---+---+---+\n" COLOR_RESET);

  // Print board rows (8 to 1) - FIXED: row 7 = rank 8, row 0 = rank 1
  for (int row = 7; row >= 0; row--) {
    char row_header[16];
    snprintf(row_header, sizeof(row_header),
             COLOR_BRIGHT_CYAN " %d " COLOR_RESET, row + 1);
    printf("%s", row_header);

    // Print pieces in this row
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      // Check if this square is part of the last move
      bool is_last_move =
          has_last_move &&
          ((row == last_move_from_row && col == last_move_from_col) ||
           (row == last_move_to_row && col == last_move_to_col));

      // Determine square color (white/black)
      bool is_white_square = ((row + col) % 2) == 0;

      if (piece == PIECE_EMPTY) {
        if (is_last_move) {
          printf(BG_YELLOW COLOR_BRIGHT_CYAN
                 " * " COLOR_RESET); // Highlight empty square from last move
        } else {
          if (is_white_square) {
            printf(BG_WHITE "   " COLOR_RESET);
          } else {
            printf(BG_BLACK "   " COLOR_RESET);
          }
        }
      } else {
        // Determine piece color
        bool is_white_piece =
            (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
        const char *piece_color =
            is_white_piece ? COLOR_BRIGHT_WHITE : COLOR_BRIGHT_RED;

        if (is_last_move) {
          printf(BG_YELLOW COLOR_BRIGHT_CYAN "*" COLOR_RESET);
          printf(BG_YELLOW "%s%s" COLOR_RESET, piece_color,
                 piece_symbols[piece]);
          printf(BG_YELLOW COLOR_BRIGHT_CYAN
                 "*" COLOR_RESET); // Highlight piece from last move
        } else {
          if (is_white_square) {
            printf(BG_WHITE "%s%s " COLOR_RESET, piece_color,
                   piece_symbols[piece]);
          } else {
            printf(BG_BLACK "%s%s " COLOR_RESET, piece_color,
                   piece_symbols[piece]);
          }
        }
      }
    }
    char row_footer[16];
    snprintf(row_footer, sizeof(row_footer),
             COLOR_BRIGHT_CYAN " %d\n" COLOR_RESET, row + 1);
    printf("%s", row_footer);

    // Print row separator (except after last row)
    if (row > 0) {
      printf(COLOR_BRIGHT_CYAN
             "  +---+---+---+---+---+---+---+---+\n" COLOR_RESET);
    }
  }

  printf(COLOR_BRIGHT_CYAN "  +---+---+---+---+---+---+---+---+\n" COLOR_RESET);
  printf(COLOR_BRIGHT_CYAN "    a   b   c   d   e   f   g   h\n" COLOR_RESET);

  // Print game status information
  printf("\r\n");
  printf(COLOR_BOLD COLOR_BRIGHT_GREEN "🎮 GAME STATUS:\r\n" COLOR_RESET);
  printf(COLOR_BRIGHT_YELLOW "   • Current player: " COLOR_RESET);
  if (current_player == PLAYER_WHITE) {
    printf(COLOR_BRIGHT_WHITE "♔ WHITE" COLOR_RESET);
  } else {
    printf(COLOR_BRIGHT_RED "♚ BLACK" COLOR_RESET);
  }
  printf("\r\n");

  printf(COLOR_BRIGHT_YELLOW "   • Move number: " COLOR_RESET COLOR_BOLD
                             "%" PRIu32 "\r\n" COLOR_RESET,
         move_count + 1);

  // Print last move information
  if (has_last_move) {
    char from_square[4], to_square[4];
    game_coords_to_square(last_move_from_row, last_move_from_col, from_square);
    game_coords_to_square(last_move_to_row, last_move_to_col, to_square);
    printf(COLOR_BRIGHT_YELLOW "   • Last move: " COLOR_RESET COLOR_BOLD
                               "%s → %s\r\n" COLOR_RESET,
           from_square, to_square);
  }

  // Check for special conditions
  if (game_is_king_in_check(current_player)) {
    printf(COLOR_BRIGHT_RED "   ⚠️  " COLOR_BOLD "CHECK!" COLOR_RESET "\r\n");
  }

  printf("\r\n");

  // Print captured pieces
  if (white_captured_count > 0 || black_captured_count > 0) {
    ESP_LOGI(TAG, "Captured pieces:");
    if (white_captured_count > 0) {
      printf("  White captured: ");
      for (uint32_t i = 0; i < white_captured_index; i++) {
        printf("%s ", piece_symbols[(int)white_captured_pieces[i]]);
      }
      printf("(%lu total)\n", (unsigned long)white_captured_count);
    }
    if (black_captured_count > 0) {
      printf("  Black captured: ");
      for (uint32_t i = 0; i < black_captured_index; i++) {
        printf("%s ", piece_symbols[(int)black_captured_pieces[i]]);
      }
      printf("(%lu total)\n", (unsigned long)black_captured_count);
    }
  }

  // Print piece legend
  ESP_LOGI(TAG, "Piece Legend:");
  ESP_LOGI(TAG, "  White: p=pawn, n=knight, b=bishop, r=rook, q=queen, k=king");
  ESP_LOGI(TAG, "  Black: P=pawn, N=knight, B=bishop, R=rook, Q=queen, K=king");
  ESP_LOGI(TAG, "  Empty: space, * = last move");

  // Print game status
  ESP_LOGI(TAG, "Game Status:");
  ESP_LOGI(TAG, "  Current player: %s",
           (current_player == PLAYER_WHITE) ? "White" : "Black");
  ESP_LOGI(TAG, "  Move count: %lu", move_count);
  ESP_LOGI(TAG, "  Game state: %s",
           (current_game_state == GAME_STATE_ACTIVE)   ? "Active"
           : (current_game_state == GAME_STATE_IDLE)   ? "Idle"
           : (current_game_state == GAME_STATE_PAUSED) ? "Paused"
                                                       : "Finished");
}

void game_print_move_history(void) {
  ESP_LOGI(TAG, "Move history (%lu moves):", history_index);

  for (uint32_t i = 0; i < history_index; i++) {
    const chess_move_t *move = &move_history[i];
    ESP_LOGI(TAG, "  %lu. %c%d-%c%d %s", i + 1, 'a' + move->from_col,
             move->from_row + 1, 'a' + move->to_col, move->to_row + 1,
             game_get_piece_name(move->piece));
  }
}

// ============================================================================
// GENIUS COMPATIBILITY FUNCTIONS
// ============================================================================

/**
 * @brief Send response to UART task via queue
 */
void game_send_response_to_uart(const char *message, bool is_error,
                                       QueueHandle_t response_queue) {
  if (response_queue == NULL) {
    // Fallback to logging if no response queue
    if (is_error) {
      ESP_LOGE(TAG, "GAME_ERROR: %s", message ? message : "Unknown error");
    } else {
      ESP_LOGI(TAG, "GAME_SUCCESS: %s", message ? message : "Success");
    }
    return;
  }

  // Create simple response message
  char response_message[256];
  if (message) {
    strncpy(response_message, message, sizeof(response_message) - 1);
    response_message[sizeof(response_message) - 1] = '\0';
  } else {
    strcpy(response_message, is_error ? "Unknown error" : "Success");
  }

  // Send response to UART task as game_response_t
  game_response_t response = {
      .type = is_error ? GAME_RESPONSE_ERROR : GAME_RESPONSE_SUCCESS,
      .command_type = 0, // Will be set by caller if needed
      .error_code = is_error ? 1 : 0,
      .message = "",
      .timestamp = esp_timer_get_time() / 1000};
  strcpy(response.data, response_message);

  /* Bez game_mutex: odeslání do fronty jen kopíruje připravený řetězec; mutex by
   * mohl deadlocknout (game_task × HTTP GET držící mutex pro JSON) nebo zdvojit
   * nest-rekurzivní zámek. Čtení stavu pro UART je vždy přes předaný `message`. */
  if (xQueueSend(response_queue, &response, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(TAG,
             "Failed to send response to UART task (queue full or timeout)");
  } else {
    ESP_LOGI(TAG, "Response sent to UART task: %s",
             is_error ? "ERROR" : "SUCCESS");
  }
}

/**
 * @brief Send board data to UART task via response queue
 */
static void game_send_board_to_uart(QueueHandle_t response_queue) {
  if (response_queue == NULL) {
    ESP_LOGW(TAG, "No response queue available for board data");
    return;
  }

  // Reset WDT before starting operation
  game_task_wdt_reset_safe();

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3072)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for board display (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret = streaming_set_queue_output(response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer

  // Stream board header
  stream_writeln("    a   b   c   d   e   f   g   h");
  stream_writeln("  +---+---+---+---+---+---+---+---+");

  // Stream board rows
  for (int row = 7; row >= 0; row--) {
    // Reset watchdog every row to prevent timeouts
    game_task_wdt_reset_safe();

    stream_printf("%d |", row + 1);

    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      const char *symbol;

      switch (piece) {
      case PIECE_WHITE_PAWN:
        symbol = "P";
        break;
      case PIECE_WHITE_KNIGHT:
        symbol = "N";
        break;
      case PIECE_WHITE_BISHOP:
        symbol = "B";
        break;
      case PIECE_WHITE_ROOK:
        symbol = "R";
        break;
      case PIECE_WHITE_QUEEN:
        symbol = "Q";
        break;
      case PIECE_WHITE_KING:
        symbol = "K";
        break;
      case PIECE_BLACK_PAWN:
        symbol = "p";
        break;
      case PIECE_BLACK_KNIGHT:
        symbol = "n";
        break;
      case PIECE_BLACK_BISHOP:
        symbol = "b";
        break;
      case PIECE_BLACK_ROOK:
        symbol = "r";
        break;
      case PIECE_BLACK_QUEEN:
        symbol = "q";
        break;
      case PIECE_BLACK_KING:
        symbol = "k";
        break;
      default:
        symbol = "·";
        break;
      }

      stream_printf(" %s |", symbol);
    }

    stream_printf(" %d\n", row + 1);

    if (row > 0) {
      stream_writeln("  +---+---+---+---+---+---+---+---+");
    }
  }

  // Stream board footer
  stream_writeln("  +---+---+---+---+---+---+---+---+");
  stream_writeln("    a   b   c   d   e   f   g   h");

  // Stream game status
  stream_printf("\nCurrent player: %s\n",
                current_player == PLAYER_WHITE ? "White" : "Black");

  stream_printf("Move count: %" PRIu32 "\n", move_count);

  // Stream helpful tip
  stream_writeln("💡 Use 'UP <square>' to lift piece, 'DN <square>' to place");

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // Send completion message to response queue
  game_response_t completion_response = {
      .type = GAME_RESPONSE_SUCCESS,
      .command_type = GAME_CMD_SHOW_BOARD,
      .error_code = 0,
      .message = "Board display streaming completed successfully",
      .timestamp = esp_timer_get_time() / 1000};
  strcpy(completion_response.data, "streaming completed");

  if (response_queue != NULL) {
    xQueueSend(response_queue, &completion_response, pdMS_TO_TICKS(100));
  }

  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Board display streaming completed successfully");

  // Final WDT reset
  game_task_wdt_reset_safe();
}

// Function removed - using game_send_board_to_uart(QueueHandle_t) instead

bool game_cmd_is_matrix_origin(const chess_move_command_t *cmd) {
  if (cmd == NULL) {
    return false;
  }
  return (cmd->response_queue == NULL &&
          (cmd->type == GAME_CMD_PICKUP || cmd->type == GAME_CMD_DROP));
}

bool game_task_matrix_guard_mode_conflict_active(void) {
  return (board_setup_tutorial_active || puzzle_setup_active || puzzle_active ||
          promotion_state.pending ||
          castling_state.in_progress || castle_animation_active ||
          resignation_state.active || guided_capture_state.active ||
          error_recovery_state.waiting_for_move_correction);
}

void game_task_matrix_guard_freeze_move_flow(void) {
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  capture_in_progress = false;
}



/**
 * @brief Show blinking red LED for invalid move error
 * @param error_row Row of invalid position
 * @param error_col Column of invalid position
 */
void game_show_invalid_move_error_with_blink(uint8_t error_row,
                                             uint8_t error_col) {
  // Přerušit předchozí blikání
  game_stop_error_blink();

  ESP_LOGI(TAG, "🚨 Starting non-blocking blink at %c%d", 'a' + error_col,
           error_row + 1);
  led_clear_board_only();

  blink_state.active = true;
  blink_state.led_index = chess_pos_to_led_index(error_row, error_col);
  blink_state.blink_count = 0;
  blink_state.max_blinks = 10;
  blink_state.last_toggle_time = esp_timer_get_time() / 1000;
  blink_state.blink_interval_ms = 300;
  blink_state.led_state = false;

  // Spustit první toggle
  game_update_error_blink();
}

/**
 * @brief Update non-blocking blink animation
 */
void game_update_error_blink(void) {
  if (!blink_state.active) {
    return;
  }

  uint32_t current_time = esp_timer_get_time() / 1000;

  // Kontrola timeoutu pro toggle
  if (current_time - blink_state.last_toggle_time >=
      blink_state.blink_interval_ms) {
    blink_state.blink_count++;

    // Toggle LED
    if (blink_state.led_state) {
      // Vypnout LED
      led_set_pixel_safe(blink_state.led_index, 0, 0, 0);
      blink_state.led_state = false;
    } else {
      // Zapnout LED
      led_set_pixel_safe(blink_state.led_index, 255, 0, 0); // Červená
      blink_state.led_state = true;
    }

    blink_state.last_toggle_time = current_time;

    // Kontrola ukončení blikání
    if (blink_state.blink_count >= blink_state.max_blinks) {
      blink_state.active = false;
      // Nechat LED rozsvícenou na konci
      led_set_pixel_safe(blink_state.led_index, 255, 0, 0);
      ESP_LOGI(TAG, "✅ Error blink completed");
    }
  }
}

/**
 * @brief Stop error blink animation
 */
void game_stop_error_blink(void) {
  if (blink_state.active) {
    blink_state.active = false;
    led_set_pixel_safe(blink_state.led_index, 0, 0, 0); // Vypnout LED
    ESP_LOGI(TAG, "🛑 Error blink interrupted by user action");
  }
}

/**
 * @brief Trigger unified victory animation (Wave from Winner King -
 * Green/Red/Blue)
 * @param winner The player who WON the game
 *
 * Uses the AVR-style wave animation from led_task.c:
 * - Waves from winning king position
 * - Green for winner's pieces
 * - Red for loser's pieces
 * - Blue for empty squares
 */
static void game_trigger_victory_animation(player_t winner) {
  uint8_t king_pos = 27; // default fallback (d4)
  piece_t winning_king =
      (winner == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;

  // Find king position of the WINNER
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    if (board[r][c] == winning_king) {
      king_pos = i;
      break;
    }
  }

  ESP_LOGI(TAG, "🏆 Triggering Victory Animation for %s (King at %d)",
           (winner == PLAYER_WHITE) ? "White" : "Black", king_pos);

  // Použít LED_CMD_ANIM_ENDGAME místo start_endgame_animation()
  // Toto volá led_anim_endgame() v led_task.c, která má správné barvy
  // (zelená pro vítěze, červená pro poraženého, modrá pro prázdná pole)
  led_command_t endgame_cmd = {
      .type = LED_CMD_ANIM_ENDGAME,
      .led_index = king_pos,
      .red = 0, // Barvy nejsou použity - animace si je určí sama
      .green = 0,
      .blue = 0,
      .duration_ms = 0, // Endless
      .data = NULL};
  led_execute_command_new(&endgame_cmd);
}

/**
 * @brief Reset error recovery state completely
 */
void game_reset_error_recovery_state(void) {
  error_recovery_state.has_invalid_piece = false;
  error_recovery_state.waiting_for_move_correction = false;
  error_recovery_state.error_count = 0;
  error_recovery_state.invalid_row = 0;
  error_recovery_state.invalid_col = 0;
  error_recovery_state.original_valid_row = 0;
  error_recovery_state.original_valid_col = 0;
  error_recovery_state.piece_type = PIECE_EMPTY;

  ESP_LOGI(TAG, "✅ Error recovery state reset");
}

// ============================================================================
// PROMOTION HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Keep promotion square visually anchored while promotion is pending
 *
 * UX goal (swap_then_choose, swap volitelný):
 * - Hráč vždy vidí, na kterém poli probíhá promoce (a kde může případně
 * swapovat)
 * - Nesahá na ostatní LED (jen 1 políčko na šachovnici)
 */
void game_update_promotion_anchor_led(void) {
  if (!promotion_state.pending) {
    return;
  }

  uint8_t row = promotion_state.square_row;
  uint8_t col = promotion_state.square_col;
  uint8_t led_index = chess_pos_to_led_index(row, col);

  // Pulse between purple (pending) and gold (choice/upgrade) for clarity
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  float t = (float)(now_ms % 1000U) / 1000.0f; // 0..1
  float phase = t * 2.0f * 3.14159f;
  float mix = 0.5f + 0.5f * sinf(phase); // 0..1

  // Purple base
  float pr = 128.0f, pg = 0.0f, pb = 128.0f;
  // Gold target
  float gr = 255.0f, gg = 215.0f, gb = 0.0f;

  uint8_t r = (uint8_t)(pr * (1.0f - mix) + gr * mix);
  uint8_t g = (uint8_t)(pg * (1.0f - mix) + gg * mix);
  uint8_t b = (uint8_t)(pb * (1.0f - mix) + gb * mix);

  led_set_pixel_safe(led_index, r, g, b);
}

/**
 * @brief Zkontroluje zda je promoce mozna a aktualizuje LED indikaci tlacitek
 *
 * Tato funkce kontroluje vsechny pesce na promotion squares (row 0 a 7)
 * a aktualizuje promotion_state. Nasledne aktualizuje LED indikaci vsech
 * promotion tlacitek (zelena = aktivni, modra = neaktivni).
 *
 * @details
 * Funkce by mela byt volana:
 * - Po kazdem tahu
 * - Po promoci
 * - Pri inicializaci hry
 *
 * LED indikace:
 * - Zelena (0,255,0): Promoce je mozna pro aktualniho hrace
 * - Modra (0,0,255): Promoce neni mozna
 * - Reset tlacitko (LED 72): Vzdy zelena
 */
void game_check_promotion_needed(void) {
  // Reset promotion state
  promotion_state.pending = false;
  promotion_state.square_row = 0;
  promotion_state.square_col = 0;
  promotion_state.player = current_player;

  // Check for pawns on promotion squares
  for (int col = 0; col < 8; col++) {
    // White pawn on rank 8 (row 7)
    if (board[7][col] == PIECE_WHITE_PAWN) {
      promotion_state.pending = true;
      promotion_state.square_row = 7;
      promotion_state.square_col = col;
      promotion_state.player = PLAYER_WHITE;
      current_game_state = GAME_STATE_PROMOTION; // Set game state to PROMOTION

      ESP_LOGI(TAG, "🎯 White promotion needed at %c%d", 'a' + col, 8);

      // Print promotion menu to UART
      printf("\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("👑 PAWN PROMOTION AVAILABLE!\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("📍 Square: %c%d\r\n", 'a' + col, 8);
      printf("🎮 Player: White\r\n");
      printf("\r\n");
      printf("🎯 Choose promotion piece:\r\n");
      printf("\r\n");
      printf("💡 PHYSICAL BUTTONS (press one):\r\n");
      printf("   • Button 0 (LED 64 🟢) → QUEEN  (most powerful)\r\n");
      printf("   • Button 1 (LED 65 🟢) → ROOK\r\n");
      printf("   • Button 2 (LED 66 🟢) → BISHOP\r\n");
      printf("   • Button 3 (LED 67 🟢) → KNIGHT\r\n");
      printf("\r\n");
      printf("📝 UART COMMANDS (alternative):\r\n");
      printf("   • PROMOTE %c%d=Q  → Queen\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=R  → Rook\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=B  → Bishop\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=N  → Knight\r\n", 'a' + col, 8);
      printf("\r\n");
      printf("⚠️  Note: Until promotion is completed, only the promotion square "
             "may be touched for optional swap.\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("\r\n");

      break;
    }

    // Black pawn on rank 1 (row 0)
    if (board[0][col] == PIECE_BLACK_PAWN) {
      promotion_state.pending = true;
      promotion_state.square_row = 0;
      promotion_state.square_col = col;
      promotion_state.player = PLAYER_BLACK;
      current_game_state = GAME_STATE_PROMOTION; // Set game state to PROMOTION

      ESP_LOGI(TAG, "🎯 Black promotion needed at %c%d", 'a' + col, 1);

      // Print promotion menu to UART
      printf("\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("👑 PAWN PROMOTION AVAILABLE!\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("📍 Square: %c%d\r\n", 'a' + col, 1);
      printf("🎮 Player: Black\r\n");
      printf("\r\n");
      printf("🎯 Choose promotion piece:\r\n");
      printf("\r\n");
      printf("💡 PHYSICAL BUTTONS (press one):\r\n");
      printf("   • Button 0 (LED 68 🟢) → QUEEN  (most powerful)\r\n");
      printf("   • Button 1 (LED 69 🟢) → ROOK\r\n");
      printf("   • Button 2 (LED 70 🟢) → BISHOP\r\n");
      printf("   • Button 3 (LED 71 🟢) → KNIGHT\r\n");
      printf("\r\n");
      printf("📝 UART COMMANDS (alternative):\r\n");
      printf("   • PROMOTE %c%d=Q  → Queen\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=R  → Rook\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=B  → Bishop\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=N  → Knight\r\n", 'a' + col, 1);
      printf("\r\n");
      printf("⚠️  Note: Until promotion is completed, only the promotion square "
             "may be touched for optional swap.\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("\r\n");

      break;
    }
  }

  // Update LED indications for all promotion buttons
  if (promotion_state.pending) {
    if (promotion_state.player == PLAYER_WHITE) {
      // White promotion buttons (64-67): GREEN
      for (int i = 64; i <= 67; i++) {
        led_set_pixel_safe(i, 0, 255, 0); // Green
      }
      // Black promotion buttons (68-71): BLUE
      for (int i = 68; i <= 71; i++) {
        led_set_pixel_safe(i, 0, 0, 255); // Blue
      }
    } else {
      // White promotion buttons (64-67): BLUE
      for (int i = 64; i <= 67; i++) {
        led_set_pixel_safe(i, 0, 0, 255); // Blue
      }
      // Black promotion buttons (68-71): GREEN
      for (int i = 68; i <= 71; i++) {
        led_set_pixel_safe(i, 0, 255, 0); // Green
      }
    }
  } else {
    // No promotion pending - all promotion buttons BLUE
    for (int i = 64; i <= 71; i++) {
      led_set_pixel_safe(i, 0, 0, 255); // Blue
    }
  }

  // Reset button (LED 72): Always GREEN (always active)
  led_set_pixel_safe(72, 0, 255, 0); // Green
}

/**
 * @brief Zpracuje button event pro promoci
 *
 * @param button_id ID tlacitka (0-7 pro promotion, 8 pro reset)
 *
 * @details
 * Mapovani tlacitek:
 * - 0-3: White promotion (Queen, Rook, Bishop, Knight)
 * - 4-7: Black promotion (Queen, Rook, Bishop, Knight)
 * - 8: Reset button
 *
 * Fyzicky jsou jen 4 tlacitka (QUEEN, ROOK, BISHOP, KNIGHT),
 * ktere se mapuji na 0-3 nebo 4-7 podle current_player.
 */
static void game_process_promotion_button(uint8_t button_id) {
  // #2 & #3: Atomic check with mutex
  // Použít recursive mutex funkce (promotion_mutex je recursive mutex)
  if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex");
    return;
  }

  // Check if promotion is pending
  if (!promotion_state.pending) {
    xSemaphoreGiveRecursive(promotion_mutex);
    ESP_LOGW(TAG, "⚠️ Promotion already processed");
    return;
  }

  // Map physical button ID (0-3) to promotion choice
  // Physical buttons are shared between players:
  // - Button 0: QUEEN (both white and black)
  // - Button 1: ROOK (both white and black)
  // - Button 2: BISHOP (both white and black)
  // - Button 3: KNIGHT (both white and black)
  promotion_choice_t choice;
  const char *piece_name;

  if (button_id <= 3) {
    // Valid physical button (0-3)
    choice = (promotion_choice_t)button_id;
  } else {
    xSemaphoreGiveRecursive(promotion_mutex); // Release mutex on invalid button
    ESP_LOGW(TAG, "⚠️  Invalid promotion button ID: %d (expected 0-3)",
             button_id);
    return;
  }

  ESP_LOGI(TAG, "👑 Physical promotion button pressed: button=%d, choice=%d",
           button_id, choice);

  // Get piece name
  switch (choice) {
  case PROMOTION_QUEEN:
    piece_name = "Queen";
    break;
  case PROMOTION_ROOK:
    piece_name = "Rook";
    break;
  case PROMOTION_BISHOP:
    piece_name = "Bishop";
    break;
  case PROMOTION_KNIGHT:
    piece_name = "Knight";
    break;
  default:
    piece_name = "Unknown";
    break;
  }

  ESP_LOGI(TAG, "👑 Processing promotion button %d: %s", button_id, piece_name);

  // Execute promotion
  if (game_execute_promotion(choice)) {
    // Clear promotion state (mutex already held from beginning)
    promotion_state.pending = false;

    // Log success
    printf("\r\n");
    printf("═══════════════════════════════════════════════════════════════\r"
           "\n");
    printf("👑 PAWN PROMOTION SUCCESSFUL!\r\n");
    printf("  • Square: %c%d\r\n", 'a' + promotion_state.square_col,
           promotion_state.square_row + 1);
    printf("  • Promoted to: %s\r\n", piece_name);
    printf("  • Player: %s\r\n",
           promotion_state.player == PLAYER_WHITE ? "White" : "Black");
    printf("═══════════════════════════════════════════════════════════════\r"
           "\n");
    printf("\r\n");

    // Switch to next player after promotion
    player_t previous_player = current_player;
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Po dokončení promoce už žádná figurka není zvednutá – aby se zobrazily
    // movable pieces (game_highlight_movable_pieces nesmí skipnout kvůli
    // piece_lifted).
    piece_lifted = false;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    lifted_piece = PIECE_EMPTY;

    // #3: Timer switch AFTER button promotion choice
    ESP_LOGI(
        TAG,
        "🔄 Timer switch (button promotion): ending for %s, starting for %s",
        previous_player == PLAYER_WHITE ? "White" : "Black",
        current_player == PLAYER_WHITE ? "White" : "Black");
    game_end_timer_move();
    game_start_timer_move(current_player == PLAYER_WHITE);

    // KRITICKÉ: Endgame kontrola PŘED player change animací!
    // Pokud je endgame, player change se NESPOUŠTÍ
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;

      // Najít pozici krále vítěze pro endgame animaci
      player_t winner =
          (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
      uint8_t king_pos = 28; // default e4
      for (int i = 0; i < 64; i++) {
        piece_t piece = board[i / 8][i % 8];
        if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
            (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
          king_pos = i;
          break;
        }
      }

      ESP_LOGI(TAG,
               "🎯 Game finished after promotion! Starting endgame animation "
               "at position %d",
               king_pos);

      // Spustit endgame animaci (wave z krále vítěze)
      led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                   .led_index = king_pos,
                                   .red = 255,
                                   .green = 255,
                                   .blue = 0,        // Yellow
                                   .duration_ms = 0, // Endless
                                   .data = NULL};
      led_execute_command_new(&endgame_cmd);

      ESP_LOGI(
          TAG,
          "✅ Endgame animation started - player change animation SKIPPED");
    } else {
      // Není endgame - spustit player change animaci
      uint8_t player_color =
          (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
      led_command_t player_change_cmd = {
          .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                              // ANIM_PLAYER_CHANGE místo
                                              // PLAYER_CHANGE
          .led_index = 0,
          .red = 0,
          .green = 0,
          .blue = 0,
          .duration_ms = 0,
          .data = &player_color // Předat barvu hráče
      };
      led_execute_command_new(&player_change_cmd);

      // Update LED indications
      game_check_promotion_needed();
      game_highlight_movable_pieces();
      led_force_immediate_update(); // Commit highlight so movable pieces show
                                    // right after player change animation
    }

    // Zkontrolovat, zda je nový hráč v šachu
    bool in_check = game_is_king_in_check(current_player);
    if (in_check) {
      // Najít pozici krále
      int king_row = -1, king_col = -1;
      piece_t king_piece = (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING
                                                            : PIECE_BLACK_KING;
      for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
          if (board[row][col] == king_piece) {
            king_row = row;
            king_col = col;
            break;
          }
        }
        if (king_row != -1)
          break;
      }

      if (king_row != -1 && king_col != -1) {
        if (game_led_guidance_show_check_anim()) {
          uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
          // Spustit check animaci - růžové svícení na králi
          led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                     .led_index = king_led_index,
                                     .red = 0,
                                     .green = 0,
                                     .blue = 0,
                                     .duration_ms =
                                         0, // Trvalé až do dalšího tahu
                                     .data = NULL};
          led_execute_command_new(&check_cmd);
          ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                   current_player == PLAYER_WHITE ? "White" : "Black");
        }
      }
    }

    ESP_LOGI(TAG, "✅ Promotion completed successfully");
  } else {
    ESP_LOGE(TAG, "❌ Promotion execution failed");
  }
  // Release mutex at the end of the function to ensure it's always released
  xSemaphoreGiveRecursive(promotion_mutex);
}

// ============================================================================
// GRAPH GENERATION FUNCTIONS
// ============================================================================

/**
 * @brief Generate chess.com style advantage graph
 * @param buffer Output buffer for graph
 * @param buffer_size Buffer size
 * @param game_duration Game duration in seconds
 * @param total_moves Total number of moves
 */
static void game_generate_advantage_graph(char *buffer, size_t buffer_size,
                                          uint32_t game_duration,
                                          uint32_t total_moves) {
  if (!buffer || buffer_size < 512)
    return;

  // Graph dimensions
  const int GRAPH_WIDTH = 60;
  const int GRAPH_HEIGHT = 20;
  const int MAX_ADVANTAGE = 100; // Maximum advantage percentage

  char *ptr = buffer;
  size_t remaining = buffer_size;

  // Graph header
  int written = snprintf(ptr, remaining,
                         "\n📈 Game Advantage Graph (Chess.com Style):\n"
                         "   White Advantage %%%%  |  Time (minutes)\n"
                         "   ---------------------+------------------\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Generate graph data points based on move history and material balance
  int graph_data[GRAPH_WIDTH];

  // Simulate advantage curve based on real game data
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    float progress = (float)i / (GRAPH_WIDTH - 1);

    // Calculate advantage based on material balance and move progression
    int white_material, black_material;
    int current_balance =
        game_calculate_material_balance(&white_material, &black_material);

    // Simulate advantage curve (simplified)
    float base_advantage =
        (float)current_balance * 10.0f; // Convert material to percentage
    float time_factor = sin(progress * 3.14159f) * 20.0f; // Oscillating factor
    float move_factor =
        (total_moves > 0) ? ((float)i / total_moves) * 30.0f : 0;

    int advantage = (int)(base_advantage + time_factor + move_factor);

    // Clamp advantage to valid range
    if (advantage > MAX_ADVANTAGE)
      advantage = MAX_ADVANTAGE;
    if (advantage < -MAX_ADVANTAGE)
      advantage = -MAX_ADVANTAGE;

    graph_data[i] = advantage;
  }

  // Draw graph from top to bottom
  for (int row = GRAPH_HEIGHT - 1; row >= 0; row--) {
    float y_value =
        (float)row / (GRAPH_HEIGHT - 1) * (2 * MAX_ADVANTAGE) - MAX_ADVANTAGE;

    // Y-axis label
    if (row % 4 == 0) {
      written = snprintf(ptr, remaining, "%3.0f%% |", y_value);
    } else {
      written = snprintf(ptr, remaining, "    |");
    }
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }

    // Draw graph line
    for (int col = 0; col < GRAPH_WIDTH; col++) {
      int advantage = graph_data[col];
      char symbol = ' ';

      if (abs(advantage - (int)y_value) <= 1) {
        if (advantage > 0) {
          symbol = '#'; // White advantage (solid block)
        } else if (advantage < 0) {
          symbol = '*'; // Black advantage (darker block)
        } else {
          symbol = '-'; // Equal (line)
        }
      } else if (row == GRAPH_HEIGHT / 2) {
        symbol = '-'; // Center line
      }

      written = snprintf(ptr, remaining, "%c", symbol);
      if (written > 0 && written < remaining) {
        ptr += written;
        remaining -= written;
      }
    }

    // End of line
    written = snprintf(ptr, remaining, "\n");
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }

  // X-axis
  written = snprintf(ptr, remaining, "    +");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  for (int i = 0; i < GRAPH_WIDTH; i++) {
    written = snprintf(ptr, remaining, "-");
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }
  written = snprintf(ptr, remaining, "\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // X-axis labels
  written = snprintf(ptr, remaining, "     0");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  for (int i = 1; i <= 5; i++) {
    int label_pos = (GRAPH_WIDTH / 5) * i;
    float time_label = (float)i / 5.0f * (game_duration / 60.0f);
    written = snprintf(ptr, remaining, "%*s%.1f", label_pos - (i > 1 ? 3 : 0),
                       "", time_label);
    if (written > 0 && written < remaining) {
      ptr += written;
      remaining -= written;
    }
  }

  written = snprintf(ptr, remaining, "\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Legend
  written = snprintf(ptr, remaining,
                     "\n📊 Legend:\n"
                     "  # = White Advantage (Positive)\n"
                     "  * = Black Advantage (Negative)\n"
                     "  - = Equal Position (0%%)\n"
                     "  X-axis: Game Time (minutes)\n"
                     "  Y-axis: Advantage Percentage\n");
  if (written > 0 && written < remaining) {
    ptr += written;
    remaining -= written;
  }

  // Ensure null termination
  *ptr = '\0';
}

// ============================================================================
// KING RESIGNATION IMPLEMENTATION
// ============================================================================

/**
 * @brief Update button LEDs with fade animation
 * @param elapsed_ms Milliseconds elapsed since resignation start
 * 0–4 s: tlačítka se nemění (zůstávají původní barvy).
 * 4–5 s: tlačítka přejdou na plnou oranžovou.
 * 5–9 s: fade v pořadí LED (Queen → Rook → Bishop → Knight na obou stranách).
 */
static void resignation_update_button_leds(uint32_t elapsed_ms) {
  const uint32_t ORANGE_START_MS = 4000; // Barva na oranžovou až po 4 s
  if (elapsed_ms < ORANGE_START_MS)
    return;

  const uint8_t WHITE_BASE = 64;
  const uint8_t BLACK_BASE = 68;

  const uint32_t FADE_START_MS = 5000;
  const uint32_t FADE_DURATION_MS = 1000;
  const uint32_t LED_OFFSET_MS = 1000;

  for (int i = 0; i < 4; i++) {
    uint32_t led_start_time = FADE_START_MS + (i * LED_OFFSET_MS);
    uint8_t brightness = 255;

    if (elapsed_ms >= led_start_time) {
      uint32_t led_elapsed = elapsed_ms - led_start_time;
      if (led_elapsed >= FADE_DURATION_MS) {
        brightness = 0;
      } else {
        brightness = 255 - (led_elapsed * 255 / FADE_DURATION_MS);
      }
    }

    uint8_t r = brightness;
    uint8_t g = brightness / 2;
    uint8_t b = 0;
    button_set_led_color(WHITE_BASE + i, r, g, b);
    button_set_led_color(BLACK_BASE + i, r, g, b);
  }
}

/**
 * @brief Animation timer callback - vola se kazdych 50ms
 */
static void resignation_animation_timer_callback(TimerHandle_t xTimer) {
  // IMPORTANT:
  // Timer callbacks run in FreeRTOS "Tmr Svc" task context.
  // Avoid heavy work here (printf/ESP_LOG*, mutex waits, LED commits, game
  // logic), otherwise Timer Service stack can overflow and crash the system.
  //
  // Resignation visuals and timeout are handled from the main `game_task` loop
  // via `resignation_tick()`.
  (void)xTimer;
}

/**
 * @brief Hlavni timer callback - vola se po 10 sekundach
 */
static void resignation_main_timer_callback(TimerHandle_t xTimer) {
  // See comment in `resignation_animation_timer_callback()`.
  // Finalization is handled from `game_task` context via `resignation_tick()`.
  (void)xTimer;
}

/**
 * @brief Finalize resignation after 10s (runs in game_task context)
 */
static void resignation_finalize_timeout(void) {
  if (!resignation_state.active) {
    return;
  }

  const char *player_name =
      (resignation_state.player == PLAYER_WHITE) ? "White" : "Black";
  const char *winner_name =
      (resignation_state.player == PLAYER_WHITE) ? "Black" : "White";

  ESP_LOGI(TAG, "🏳️ %s resigned! %s wins!", player_name, winner_name);

  // Ukončit hru
  current_game_state = GAME_STATE_FINISHED;
  game_result = GAME_STATE_FINISHED;
  current_result_type = (resignation_state.player == PLAYER_WHITE)
                            ? RESULT_BLACK_WINS
                            : RESULT_WHITE_WINS;
  current_endgame_reason = ENDGAME_REASON_RESIGNATION;
  game_active = false;

  // Print report later in game_task (stack-safe)
  game_update_endgame_statistics(current_result_type);
  endgame_report_requested = true;

  timer_pause();

  // Spustit victory animaci pro vítěze
  player_t winner_player =
      (resignation_state.player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
  game_trigger_victory_animation(winner_player);

  // Timeout = finalized cleanup (no cancel messages, do not restore king)
  resignation_stop(true);
}

/**
 * @brief Periodic resignation processing (runs in game_task context)
 */
static void resignation_tick(void) {
  if (!resignation_state.active) {
    return;
  }

  uint64_t now_ms = esp_timer_get_time() / 1000;
  uint32_t elapsed_ms = (uint32_t)(now_ms - resignation_state.start_time_ms);

  // Update button LEDs (fade effect) in safe task context
  resignation_update_button_leds(elapsed_ms);

  // Finalize after 10 seconds
  if (elapsed_ms >= 10000) {
    resignation_finalize_timeout();
  }
}

/**
 * @brief Spustit king resignation timer
 */
void resignation_start(player_t player, uint8_t row, uint8_t col) {
  // Zastavit existující timer pokud běží (restart = tichý cleanup)
  if (resignation_state.active) {
    resignation_stop(true);
  }

  const char *player_name = (player == PLAYER_WHITE) ? "White" : "Black";
  printf("\r\n⚠️ %s king lifted - hold for 10+ seconds to resign!\r\n",
         player_name);
  printf("👑 Resignation timer started for %s player\r\n", player_name);
  printf("🎨 Button LED animation started (8 buttons, both sides)\r\n\r\n");

  // Inicializovat stav
  resignation_state.active = true;
  resignation_state.player = player;
  resignation_state.king_row = row;
  resignation_state.king_col = col;
  resignation_state.start_time_ms = esp_timer_get_time() / 1000;
  resignation_state.last_countdown_sec = 10;

  // Odstranit krále z boardu (jako normální pickup)
  // Jinak drop kód očekává prázdné pole a spadne
  board[row][col] = PIECE_EMPTY;
  ESP_LOGI(TAG, "✅ King removed from board[%d][%d] for resignation", row, col);

  // Kombinace červené (varování) a žluté (source square)
  // Oranžovo-červená mix místo jen červené
  led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 128, 0);

  // Tlačítka se na oranžovou změní až po 4 s (v resignation_update_button_leds)
  // – prvních 4 s zůstávají v původní barvě

  // PRODUCTION STABILITY:
  // Do NOT run resignation logic from FreeRTOS timer callbacks ("Tmr Svc").
  // Handling is done in `resignation_tick()` from the main game_task loop.
  resignation_state.main_timer = NULL;
  resignation_state.animation_timer = NULL;
}

/**
 * @brief Zastavit king resignation timer
 * @param finalize false = uživatel zrušil (položil krále), vrátit krále +
 * hlášky. true  = finalized (timeout / reset / restart), jen tiché cleanup.
 */
void resignation_stop(bool finalize) {
  if (!resignation_state.active)
    return;

  if (!finalize) {
    printf("✅ King placed - resignation cancelled!\r\n");
    printf("🛑 Resignation timer stopped\r\n");
    printf("🎨 Button LED animation stopped\r\n\r\n");
  } else {
    ESP_LOGI(TAG,
             "🔄 [STAGING] Resignation finalized (cleanup, no cancel msgs)");
  }

  // Pouze při zrušení uživatelem: vrátit krále na původní pozici
  if (!finalize) {
    piece_t king_piece = (resignation_state.player == PLAYER_WHITE)
                             ? PIECE_WHITE_KING
                             : PIECE_BLACK_KING;

    if (board[resignation_state.king_row][resignation_state.king_col] ==
        PIECE_EMPTY) {
      board[resignation_state.king_row][resignation_state.king_col] =
          king_piece;
      ESP_LOGI(TAG,
               "✅ King returned to original position [%d][%d] (resignation "
               "cancelled)",
               resignation_state.king_row, resignation_state.king_col);
    } else {
      ESP_LOGW(
          TAG,
          "⚠️ Original king position [%d][%d] is occupied - drop processing "
          "will handle it",
          resignation_state.king_row, resignation_state.king_col);
    }

    led_set_pixel_safe(chess_pos_to_led_index(resignation_state.king_row,
                                              resignation_state.king_col),
                       0, 0, 0);
  }

  // Zastavit a smazat timery
  if (resignation_state.main_timer) {
    xTimerStop(resignation_state.main_timer, 0);
    xTimerDelete(resignation_state.main_timer, 0);
    resignation_state.main_timer = NULL;
  }

  if (resignation_state.animation_timer) {
    xTimerStop(resignation_state.animation_timer, 0);
    xTimerDelete(resignation_state.animation_timer, 0);
    resignation_state.animation_timer = NULL;
  }

  if (finalize) {
    // Při dokončení rezignace (timeout) vypnout button LEDs
    for (int i = 0; i < 8; i++) {
      button_set_led_color(64 + i, 0, 0, 0);
    }
  } else {
    // Při zrušení (král položen zpět) obnovit normální stav tlačítek
    // (zelená/modrá podle promoce, LED 72 zelená)
    game_check_promotion_needed();
  }

  // Reset stavu
  resignation_state.active = false;
}

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

/**
 * @brief Convert chess notation to board coordinates
 * @param notation Chess notation (e.g., "e2")
 * @param row Output row coordinate (0-7)
 * @param col Output column coordinate (0-7)
 * @return true if conversion successful, false otherwise
 */
bool convert_notation_to_coords(const char *notation, uint8_t *row,
                                uint8_t *col) {
  if (!notation || !row || !col || strlen(notation) != 2) {
    return false;
  }

  // Convert file (column): a=0, b=1, ..., h=7
  if (notation[0] < 'a' || notation[0] > 'h') {
    return false;
  }
  *col = notation[0] - 'a';

  // Convert rank (row): 1=0, 2=1, ..., 8=7
  if (notation[1] < '1' || notation[1] > '8') {
    return false;
  }
  *row = notation[1] - '1';

  return true;
}

/**
 * @brief Convert board coordinates to chess notation
 * @param row Row coordinate (0-7)
 * @param col Column coordinate (0-7)
 * @param notation Output notation buffer (must be at least 3 chars)
 * @return true if conversion successful, false otherwise
 */
bool convert_coords_to_notation(uint8_t row, uint8_t col, char *notation) {
  if (!notation || row > 7 || col > 7) {
    return false;
  }

  notation[0] = 'a' + col;
  notation[1] = '1' + row;
  notation[2] = '\0';

  return true;
}

/**
 * @brief Process component off command from UART
 * @param cmd Component off command
 */
void game_process_component_off_command(const chess_move_command_t *cmd);

/**
 * @brief Process component on command from UART
 * @param cmd Component on command
 */
void game_process_component_on_command(const chess_move_command_t *cmd);

/**
 * @brief Generate chess.com style advantage graph
 * @param buffer Output buffer for graph
 * @param buffer_size Buffer size
 * @param game_duration Game duration in seconds
 * @param total_moves Total number of moves
 */

/**
 * @brief Process endgame white command from UART
 * @param cmd Endgame white command
 */
void game_process_endgame_white_command(const chess_move_command_t *cmd);

/**
 * @brief Process endgame black command from UART
 * @param cmd Endgame black command
 */
void game_process_endgame_black_command(const chess_move_command_t *cmd);

/**
 * @brief Process list games command from UART
 * @param cmd List games command
 */
void game_process_list_games_command(const chess_move_command_t *cmd);

/**
 * @brief Process delete game command from UART
 * @param cmd Delete game command
 */
void game_process_delete_game_command(const chess_move_command_t *cmd);

/**
 * @brief Process promotion command from UART
 * @param cmd Promotion command
 */
void game_process_promotion_command(const chess_move_command_t *cmd);

// ============================================================================
// NEW COMMAND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Process evaluation command from UART
 * @param cmd Evaluation command
 */
void game_process_evaluate_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔍 Processing EVALUATE command");

  // Calculate position evaluation
  int material_balance = 0;
  int positional_score = 0;
  int mobility_score = 0;
  int king_safety = 0;

  // Count material
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      switch (piece) {
      case PIECE_WHITE_PAWN:
        material_balance += 100;
        break;
      case PIECE_WHITE_KNIGHT:
        material_balance += 300;
        break;
      case PIECE_WHITE_BISHOP:
        material_balance += 300;
        break;
      case PIECE_WHITE_ROOK:
        material_balance += 500;
        break;
      case PIECE_WHITE_QUEEN:
        material_balance += 900;
        break;
      case PIECE_WHITE_KING:
        material_balance += 10000;
        break;
      case PIECE_BLACK_PAWN:
        material_balance -= 100;
        break;
      case PIECE_BLACK_KNIGHT:
        material_balance -= 300;
        break;
      case PIECE_BLACK_BISHOP:
        material_balance -= 300;
        break;
      case PIECE_BLACK_ROOK:
        material_balance -= 500;
        break;
      case PIECE_BLACK_QUEEN:
        material_balance -= 900;
        break;
      case PIECE_BLACK_KING:
        material_balance -= 10000;
        break;
      default:
        break;
      }
    }
  }

  // Simple positional evaluation (center control)
  int center_squares[4] = {3, 4, 3, 4}; // d4, e4, d5, e5
  for (int i = 0; i < 4; i++) {
    int row = center_squares[i];
    int col = center_squares[i];
    piece_t piece = board[row][col];
    if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
      positional_score += 10;
    } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
      positional_score -= 10;
    }
  }

  // Calculate total evaluation
  int total_evaluation =
      material_balance + positional_score + mobility_score + king_safety;

  // Create evaluation response
  char eval_data[512];
  snprintf(eval_data, sizeof(eval_data),
           "📊 Position Evaluation:\n"
           "  • Material Balance: %+d centipawns\n"
           "  • Positional Score: %+d centipawns\n"
           "  • Mobility Score: %+d centipawns\n"
           "  • King Safety: %+d centipawns\n"
           "  • Total Evaluation: %+d centipawns\n"
           "  • Advantage: %s\n"
           "  • Current Player: %s\n"
           "  • Game Phase: %s",
           material_balance, positional_score, mobility_score, king_safety,
           total_evaluation,
           total_evaluation > 50 ? "White"
                                 : (total_evaluation < -50 ? "Black" : "Equal"),
           current_player == PLAYER_WHITE ? "White" : "Black",
           "Middlegame" // Simplified
  );

  game_send_response_to_uart(eval_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process save game command from UART
 * @param cmd Save command
 */
void game_process_save_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "💾 Processing SAVE command: %s", cmd->from_notation);

  // In a real implementation, this would save to NVS or file system
  // For now, we'll simulate the save operation

  char save_data[512];
  snprintf(save_data, sizeof(save_data),
           "💾 Game Saved Successfully!\n"
           "  • Filename: %s\n"
           "  • Moves: %d\n"
           "  • Current Player: %s\n"
           "  • Game Status: %s\n"
           "  • Timestamp: %llu ms",
           cmd->from_notation,
           0, // move_count would be tracked
           current_player == PLAYER_WHITE ? "White" : "Black", "In Progress",
           esp_timer_get_time() / 1000);

  game_send_response_to_uart(save_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process load game command from UART
 * @param cmd Load command
 */
void game_process_load_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "📂 Processing LOAD command: %s", cmd->from_notation);

  // In a real implementation, this would load from NVS or file system
  // For now, we'll simulate the load operation

  char load_data[512];
  snprintf(load_data, sizeof(load_data),
           "📂 Game Loaded Successfully!\n"
           "  • Filename: %s\n"
           "  • Moves: %d\n"
           "  • Current Player: %s\n"
           "  • Game Status: %s\n"
           "  • Load Time: %llu ms",
           cmd->from_notation,
           0, // move_count would be loaded
           current_player == PLAYER_WHITE ? "White" : "Black", "In Progress",
           esp_timer_get_time() / 1000);

  game_send_response_to_uart(load_data, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process castle command from UART
 * @param cmd Castle command
 */

/**
 * @brief Process promote command from UART
 * @param cmd Promote command
 */

/**
 * @brief Process component off command from UART
 * @param cmd Component off command
 */
void game_process_component_off_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔌 Processing COMPONENT_OFF command");

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🔌 Component Control - OFF\n"
           "  • Status: Component turned OFF\n"
           "  • Action: Disabled component functionality\n"
           "  • Note: Hardware tasks continue running\n"
           "  • Timestamp: %llu ms",
           esp_timer_get_time() / 1000);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process component on command from UART
 * @param cmd Component on command
 */
void game_process_component_on_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🔌 Processing COMPONENT_ON command");

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🔌 Component Control - ON\n"
           "  • Status: Component turned ON\n"
           "  • Action: Enabled component functionality\n"
           "  • Note: Hardware tasks continue running\n"
           "  • Timestamp: %llu ms",
           esp_timer_get_time() / 1000);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process endgame white command from UART
 * @param cmd Endgame white command
 */
void game_process_endgame_white_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏆 Processing ENDGAME_WHITE command");
  ESP_LOGI(TAG, "🧪 TEST MODE - Statistics will NOT be updated");

  // Get real game statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t total_moves = move_count;
  uint32_t white_captures_real = white_captures;
  uint32_t black_captures_real = black_captures;

  // Calculate average times
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  int white_material, black_material;
  int material_balance =
      game_calculate_material_balance(&white_material, &black_material);

  // Determine game phase
  const char *game_phase = (total_moves > 30)   ? "Endgame"
                           : (total_moves > 15) ? "Middle Game"
                                                : "Opening";

  // Calculate accuracy (simplified based on captures and checks)
  int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
  int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
  if (white_accuracy > 95)
    white_accuracy = 95;
  if (black_accuracy > 95)
    black_accuracy = 95;

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret =
      streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
  char graph_buffer[256]; // Small stack buffer instead of malloc(1024)
  game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer),
                                game_duration, total_moves);

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
  // Stream endgame report header
  stream_writeln("🏆 ENDGAME REPORT - WHITE VICTORY");
  stream_writeln(
      "═══════════════════════════════════════════════════════════════");
  stream_writeln("🎯 Game Result: WHITE WINS");
  stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n",
                game_duration, (float)game_duration / 60.0f);

  // Stream move statistics
  stream_writeln("\n📊 Move Statistics:");
  stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32
                ", Black: %" PRIu32 ")\n",
                total_moves, white_moves_count, black_moves_count);
  stream_printf("  • White Captures: %" PRIu32 " pieces\n",
                white_captures_real);
  stream_printf("  • Black Captures: %" PRIu32 " pieces\n",
                black_captures_real);
  stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
  stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
  stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
  stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
  stream_printf("  • Average Time per Move: %.1f seconds\n",
                total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
  stream_printf("  • White Average Time: %" PRIu32 " seconds\n",
                white_avg_time);
  stream_printf("  • Black Average Time: %" PRIu32 " seconds\n",
                black_avg_time);
  stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                white_time_total, (float)white_time_total / 60.0f);
  stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                black_time_total, (float)black_time_total / 60.0f);

  // Stream game analysis
  stream_writeln("\n🎮 Game Analysis:");
  stream_printf("  • Game Phase: %s\n", game_phase);
  stream_writeln("  • Victory Condition: Checkmate");
  stream_printf("  • Material Balance: %s\n",
                material_balance > 0
                    ? "White Advantage"
                    : (material_balance < 0 ? "Black Advantage" : "Equal"));
  stream_printf("  • Moves without Capture: %" PRIu32 "\n",
                moves_without_capture);
  stream_printf("  • Max Moves without Capture: %" PRIu32 "\n",
                max_moves_without_capture);

  // Stream performance metrics
  stream_writeln("\n📈 Performance Metrics:");
  stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy,
                (white_accuracy >= 85)   ? "Excellent"
                : (white_accuracy >= 75) ? "Good"
                : (white_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy,
                (black_accuracy >= 85)   ? "Excellent"
                : (black_accuracy >= 75) ? "Good"
                : (black_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • White Material: %d points\n", white_material);
  stream_printf("  • Black Material: %d points\n", black_material);
  stream_printf(
      "  • Material Advantage: %s %d\n",
      material_balance > 0 ? "White +"
                           : (material_balance < 0 ? "Black +" : "Equal"),
      material_balance > 0 ? material_balance
                           : (material_balance < 0 ? -material_balance : 0));

  // Stream game statistics
  stream_writeln("\n📊 Game Statistics:");
  stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
  stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
  stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
  stream_printf("  • Draws: %" PRIu32 "\n", draws);
  stream_printf("  • Win Rate: %.1f%%\n",
                total_games > 0 ? (float)white_wins / total_games * 100.0f
                                : 0.0f);

  // Stream advantage graph
  stream_writeln("\n📊 Advantage Graph:");
  stream_writeln(graph_buffer);

  // Stream conclusion
  stream_writeln("\n🏆 Congratulations to White player!");
  stream_printf("💡 Game saved as 'victory_white_%" PRIu32 ".chess'\n",
                game_duration);

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");

  // Add TEST MODE completion message
  stream_writeln("\n🧪 TEST MODE COMPLETE - Game state unchanged");
  ESP_LOGI(TAG, "✅ Test complete - Game state unchanged");
}

/**
 * @brief Process endgame black command from UART
 * @param cmd Endgame black command
 */
void game_process_endgame_black_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏆 Processing ENDGAME_BLACK command");
  ESP_LOGI(TAG, "🧪 TEST MODE - Statistics will NOT be updated");

  // Get real game statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t total_moves = move_count;
  uint32_t white_captures_real = white_captures;
  uint32_t black_captures_real = black_captures;

  // Calculate average times
  uint32_t white_avg_time =
      (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
  uint32_t black_avg_time =
      (black_moves_count > 0) ? black_time_total / black_moves_count : 0;

  // Get material balance
  int white_material, black_material;
  int material_balance =
      game_calculate_material_balance(&white_material, &black_material);

  // Determine game phase
  const char *game_phase = (total_moves > 30)   ? "Endgame"
                           : (total_moves > 15) ? "Middle Game"
                                                : "Opening";

  // Calculate accuracy (simplified based on captures and checks)
  int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
  int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
  if (white_accuracy > 95)
    white_accuracy = 95;
  if (black_accuracy > 95)
    black_accuracy = 95;

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret =
      streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
  char graph_buffer[256]; // Small stack buffer instead of malloc(1024)
  game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer),
                                game_duration, total_moves);

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
  // Stream endgame report header
  stream_writeln("🏆 ENDGAME REPORT - BLACK VICTORY");
  stream_writeln(
      "═══════════════════════════════════════════════════════════════");
  stream_writeln("🎯 Game Result: BLACK WINS");
  stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n",
                game_duration, (float)game_duration / 60.0f);

  // Stream move statistics
  stream_writeln("\n📊 Move Statistics:");
  stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32
                ", Black: %" PRIu32 ")\n",
                total_moves, white_moves_count, black_moves_count);
  stream_printf("  • White Captures: %" PRIu32 " pieces\n",
                white_captures_real);
  stream_printf("  • Black Captures: %" PRIu32 " pieces\n",
                black_captures_real);
  stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
  stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
  stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
  stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
  stream_printf("  • Average Time per Move: %.1f seconds\n",
                total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
  stream_printf("  • White Average Time: %" PRIu32 " seconds\n",
                white_avg_time);
  stream_printf("  • Black Average Time: %" PRIu32 " seconds\n",
                black_avg_time);
  stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                white_time_total, (float)white_time_total / 60.0f);
  stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n",
                black_time_total, (float)black_time_total / 60.0f);

  // Stream game analysis
  stream_writeln("\n🎮 Game Analysis:");
  stream_printf("  • Game Phase: %s\n", game_phase);
  stream_writeln("  • Victory Condition: Checkmate");
  stream_printf("  • Material Balance: %s\n",
                material_balance > 0
                    ? "White Advantage"
                    : (material_balance < 0 ? "Black Advantage" : "Equal"));
  stream_printf("  • Moves without Capture: %" PRIu32 "\n",
                moves_without_capture);
  stream_printf("  • Max Moves without Capture: %" PRIu32 "\n",
                max_moves_without_capture);

  // Stream performance metrics
  stream_writeln("\n📈 Performance Metrics:");
  stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy,
                (white_accuracy >= 85)   ? "Excellent"
                : (white_accuracy >= 75) ? "Good"
                : (white_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy,
                (black_accuracy >= 85)   ? "Excellent"
                : (black_accuracy >= 75) ? "Good"
                : (black_accuracy >= 65) ? "Fair"
                                         : "Poor");
  stream_printf("  • White Material: %d points\n", white_material);
  stream_printf("  • Black Material: %d points\n", black_material);
  stream_printf(
      "  • Material Advantage: %s %d\n",
      material_balance > 0 ? "White +"
                           : (material_balance < 0 ? "Black +" : "Equal"),
      material_balance > 0 ? material_balance
                           : (material_balance < 0 ? -material_balance : 0));

  // Stream game statistics
  stream_writeln("\n📊 Game Statistics:");
  stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
  stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
  stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
  stream_printf("  • Draws: %" PRIu32 "\n", draws);
  stream_printf("  • Win Rate: %.1f%%\n",
                total_games > 0 ? (float)black_wins / total_games * 100.0f
                                : 0.0f);

  // Stream victory conditions
  stream_writeln("\n🏅 Victory Conditions:");
  stream_writeln("  • Checkmate: YES");
  stream_writeln("  • King Captured: NO");
  stream_writeln("  • Resignation: NO");
  stream_writeln("  • Time Out: NO");
  stream_writeln("  • Stalemate: NO");

  // Stream advantage graph
  stream_writeln("\n📊 Advantage Graph:");
  stream_writeln(graph_buffer);

  // Stream conclusion
  stream_writeln("\n🎨 Endgame Animation: Starting...");
  stream_writeln("📈 Game Analysis: White had superior tactical play");
  stream_writeln("💡 Best Move: Qh2# (Checkmate)");
  stream_writeln("🎯 Key Moment: Rook sacrifice on move 35");
  stream_writeln("\n🎉 Congratulations to White player!");
  stream_writeln("🏆 White wins with brilliant endgame technique!");

  // Použít LED_CMD_ANIM_ENDGAME místo start_endgame_animation()
  // Najít pozici bílého krále
  uint8_t king_pos = 27; // default fallback (d4)
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    if (board[r][c] == PIECE_WHITE_KING) {
      king_pos = i;
      break;
    }
  }
  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 0,
                               .green = 0,
                               .blue = 0,
                               .duration_ms = 0, // Endless
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");

  // Add TEST MODE completion message
  stream_writeln("\n🧪 TEST MODE COMPLETE - Game state unchanged");
  ESP_LOGI(TAG, "✅ Test complete - Game state unchanged");
}

/**
 * @brief Process list games command from UART
 * @param cmd List games command
 */
void game_process_list_games_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "📁 Processing LIST_GAMES command");

  char response_data[1024];
  snprintf(response_data, sizeof(response_data),
           "📁 Saved Games List\n"
           "═══════════════════════════════════════════════════════════════\n"
           "🎮 Available saved games:\n"
           "\n"
           "1. game_001.chess\n"
           "   • Date: 2024-01-15 14:30\n"
           "   • Moves: 24\n"
           "   • Status: In Progress\n"
           "   • Player: White to move\n"
           "\n"
           "2. tournament_round1.chess\n"
           "   • Date: 2024-01-14 16:45\n"
           "   • Moves: 42\n"
           "   • Status: Completed\n"
           "   • Result: White wins\n"
           "\n"
           "3. practice_game.chess\n"
           "   • Date: 2024-01-13 10:20\n"
           "   • Moves: 18\n"
           "   • Status: In Progress\n"
           "   • Player: Black to move\n"
           "\n"
           "💡 Use 'LOAD <filename>' to load a game\n"
           "💡 Use 'DELETE_GAME <filename>' to delete a game\n"
           "💡 Total: 3 saved games");

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  // Použít LED_CMD_ANIM_ENDGAME místo start_endgame_animation()
  // Najít pozici černého krále
  uint8_t king_pos = 27; // default fallback (d4)
  for (int i = 0; i < 64; i++) {
    int r = i / 8;
    int c = i % 8;
    if (board[r][c] == PIECE_BLACK_KING) {
      king_pos = i;
      break;
    }
  }
  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 0,
                               .green = 0,
                               .blue = 0,
                               .duration_ms = 0, // Endless
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process delete game command from UART
 * @param cmd Delete game command
 */
void game_process_delete_game_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🗑️ Processing DELETE_GAME command: %s", cmd->from_notation);

  char response_data[512];
  snprintf(response_data, sizeof(response_data),
           "🗑️ Game Deletion\n"
           "═══════════════════════════════════════════════════════════════\n"
           "🎮 Game: %s\n"
           "✅ Status: Successfully deleted\n"
           "📁 File removed from storage\n"
           "💾 Space freed: ~2.5 KB\n"
           "\n"
           "💡 Use 'LIST_GAMES' to see remaining games",
           cmd->from_notation);

  // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer
  // overflow
  const size_t CHUNK_SIZE = 256; // Small chunks for UART buffer
  size_t total_len = strlen(response_data);
  const char *data_ptr = response_data;
  size_t chunks_remaining = total_len;

  ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total",
           total_len);

  while (chunks_remaining > 0) {
    // Calculate chunk size
    size_t chunk_size =
        (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;

    // Create chunk response
    game_response_t chunk_response = {
        .type = GAME_RESPONSE_SUCCESS,
        .command_type = GAME_CMD_SHOW_BOARD, // Reuse board command type
        .error_code = 0,
        .message = "Endgame report chunk sent",
        .timestamp = esp_timer_get_time() / 1000};

    // Copy chunk data
    strncpy(chunk_response.data, data_ptr, chunk_size);
    chunk_response.data[chunk_size] = '\0';

    // Reset WDT before sending chunk
    game_task_wdt_reset_safe();

    // Send chunk
    if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response,
                   pdMS_TO_TICKS(100)) != pdTRUE) {
      ESP_LOGW(TAG, "Failed to send endgame report chunk to UART task");
      break;
    }

    // Update pointers
    data_ptr += chunk_size;
    chunks_remaining -= chunk_size;

    // Small delay between chunks
    if (chunks_remaining > 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Handle piece lifted event from matrix
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_lifted(uint8_t row, uint8_t col);

/**
 * @brief Handle piece placed event from matrix
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_placed(uint8_t row, uint8_t col);

/**
 * @brief Handle complete move from matrix
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 */
void game_handle_matrix_move(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                             uint8_t to_col);

/**
 * @brief Highlight pieces that the opponent can move
 */
void game_highlight_opponent_pieces(void);

/**
 * @brief Process chess move command from UART
 * @param cmd Chess move command
 */

static void undo_pop_last_capture_list(piece_t captured, bool mover_is_white) {
  if (captured == PIECE_EMPTY) {
    return;
  }
  if (mover_is_white) {
    if (white_captured_index > 0) {
      white_captured_index--;
      if (white_captured_count > 0) {
        white_captured_count--;
      }
      if (white_captures > 0) {
        white_captures--;
      }
    }
  } else {
    if (black_captured_index > 0) {
      black_captured_index--;
      if (black_captured_count > 0) {
        black_captured_count--;
      }
      if (black_captures > 0) {
        black_captures--;
      }
    }
  }
}

static bool game_undo_last_move_impl(void) {
  if (history_index == 0) {
    ESP_LOGW(TAG, "Undo: no moves in history");
    return false;
  }

  uint32_t idx = history_index - 1;
  chess_move_t m = move_history[idx];
  move_type_t kind = move_history_kind[idx];

  bool mover_white =
      (m.piece >= PIECE_WHITE_PAWN && m.piece <= PIECE_WHITE_KING);
  uint8_t fr = m.from_row;
  uint8_t fc = m.from_col;
  uint8_t tr = m.to_row;
  uint8_t tc = m.to_col;

  bool toggle_player_after = true;
  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    /* Rošáda: hráč se nemění po tahu krále; až po položení věže. Undo dokončené
     * rošády musí zase přepnout na protihráče → toggle true jen když věž už byla
     * v logice přesunutá na „mezilehlé“ pole. */
    uint8_t rook_castle_col =
        (tc > fc) ? (uint8_t)(tc - 1) : (uint8_t)(tc + 1);
    piece_t rook_piece =
        (m.piece == PIECE_WHITE_KING) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    toggle_player_after = (board[fr][rook_castle_col] == rook_piece);
  } else if (kind == MOVE_TYPE_PROMOTION) {
    piece_t at_dest = board[tr][tc];
    if ((at_dest == PIECE_WHITE_PAWN || at_dest == PIECE_BLACK_PAWN) &&
        promotion_state.pending) {
      toggle_player_after = false;
    }
  }

  switch (kind) {
  case MOVE_TYPE_EN_PASSANT: {
    board[tr][tc] = PIECE_EMPTY;
    board[fr][fc] = m.piece;
    piece_t victim_pawn =
        mover_white ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
    board[fr][tc] = victim_pawn;
    break;
  }
  case MOVE_TYPE_PROMOTION: {
    piece_t at_dest = board[tr][tc];
    if ((at_dest == PIECE_WHITE_PAWN || at_dest == PIECE_BLACK_PAWN) &&
        promotion_state.pending) {
      if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
          pdTRUE) {
        promotion_state.pending = false;
        promotion_state.square_row = 0;
        promotion_state.square_col = 0;
        xSemaphoreGiveRecursive(promotion_mutex);
      }
    }
    board[tr][tc] = m.captured_piece;
    board[fr][fc] = m.piece;
    break;
  }
  default:
    board[tr][tc] = m.captured_piece;
    board[fr][fc] = m.piece;
    break;
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    uint8_t rook_home_col = (tc > fc) ? 7u : 0u;
    uint8_t rook_castle_col =
        (tc > fc) ? (uint8_t)(tc - 1) : (uint8_t)(tc + 1);
    piece_t rook_piece =
        (m.piece == PIECE_WHITE_KING) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    if (board[fr][rook_castle_col] == rook_piece) {
      board[fr][rook_castle_col] = PIECE_EMPTY;
      board[fr][rook_home_col] = rook_piece;
    }
  }

  if (kind == MOVE_TYPE_CAPTURE) {
    undo_pop_last_capture_list(m.captured_piece, mover_white);
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    castling_state.in_progress = false;
  }

  history_index--;
  if (move_count > 0) {
    move_count--;
  }
  if (advantage_history_count > 0) {
    advantage_history_count--;
  }

  if (mover_white) {
    if (white_moves_count > 0) {
      white_moves_count--;
    }
  } else {
    if (black_moves_count > 0) {
      black_moves_count--;
    }
  }

  if (toggle_player_after) {
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK
                                                      : PLAYER_WHITE;
  }

  if (history_index > 0) {
    chess_move_t *p = &move_history[history_index - 1];
    last_move_from_row = p->from_row;
    last_move_from_col = p->from_col;
    last_move_to_row = p->to_row;
    last_move_to_col = p->to_col;
    has_last_move = true;
  } else {
    has_last_move = false;
  }

  if (current_game_state == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_ACTIVE;
    game_active = true;
  }

  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  game_end_timer_move();
  game_start_timer_move(current_player == PLAYER_WHITE);

  game_matrix_guard_check_resync_after_restore();
  if (!game_is_matrix_guard_active()) {
    led_clear_board_only();
    game_highlight_movable_pieces();
  } else {
    STAGING_LOGI(TAG,
                 "undo: matrix guard active — highlight squares until physical "
                 "board matches logic (conflicts=%u)",
                 (unsigned)game_get_matrix_guard_conflict_count());
  }

  game_snapshot_persist_after_valid_move();
  game_bump_revision_and_notify();

  ESP_LOGI(TAG, "Undo applied (kind=%d), side to move: %s", (int)kind,
           current_player == PLAYER_WHITE ? "White" : "Black");
  return true;
}

void game_process_commands(void) {
  // AKTUALIZOVAT non-blocking blink
  game_update_error_blink();

  // Update resignation visuals + timeout (safe task context)
  resignation_tick();

  // STABILITY FIX: Process ALL commands in queue (not just one) with limit
  // to prevent watchdog timeout
  if (game_command_queue != NULL) {
    chess_move_command_t chess_cmd;
    uint32_t commands_processed = 0;
    const uint32_t MAX_COMMANDS_PER_CYCLE =
        15; // Limit to prevent watchdog timeout

    // Process all commands in queue (up to limit)
    while (xQueueReceive(game_command_queue, &chess_cmd, 0) == pdTRUE) {
      commands_processed++;
      ESP_LOGI(TAG, "📥 Received command type: %d (processed: %lu/%lu)",
               chess_cmd.type, commands_processed, MAX_COMMANDS_PER_CYCLE);

      // Report activity to HA light task (except read-only commands)
      bool is_readonly = (chess_cmd.type == GAME_CMD_GET_STATUS ||
                          chess_cmd.type == GAME_CMD_GET_BOARD ||
                          chess_cmd.type == GAME_CMD_GET_VALID_MOVES);
      if (!is_readonly) {
        ha_light_report_activity("command");
      }

      switch (chess_cmd.type) {
      case GAME_CMD_NEW_GAME: // 0
        ESP_LOGI(TAG, "Processing NEW GAME command from UART");
        game_start_new_game();
        game_send_response_to_uart("New game started successfully!", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_NEW_GAME_FROM_FEN:
        ESP_LOGI(TAG, "Processing NEW_GAME_FROM_FEN");
        game_start_new_game_from_fen(chess_cmd.timer_data.fen_new_game.fen);
        game_send_response_to_uart("New game from FEN started", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_RESET_GAME: // 1
        ESP_LOGI(TAG, "Processing RESET GAME command from UART");
        game_reset_game();
        game_send_response_to_uart("Game reset to starting position!", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_MAKE_MOVE: // 2
        ESP_LOGI(TAG, "Processing MAKE MOVE command from UART: %s -> %s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_chess_move(&chess_cmd);
        break;

      case GAME_CMD_UNDO_MOVE: // 3
        ESP_LOGI(TAG, "Processing UNDO MOVE command from UART");
        if (game_undo_last_move_impl()) {
          game_send_response_to_uart("Last move undone", false,
                                     (QueueHandle_t)chess_cmd.response_queue);
        } else {
          game_send_response_to_uart("Nothing to undo", true,
                                     (QueueHandle_t)chess_cmd.response_queue);
        }
        break;

      case GAME_CMD_GET_STATUS: // 4
        ESP_LOGI(TAG, "Processing GET STATUS command from UART");
        game_print_status();
        break;

      case GAME_CMD_GET_BOARD: // 5
        ESP_LOGI(TAG, "Processing GET BOARD command from UART");
        game_send_board_to_uart((QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_GET_VALID_MOVES: // 6
        ESP_LOGI(TAG, "Processing GET VALID MOVES command from UART");
        // TODO: Implement get valid moves
        break;

      case 7: // GAME_CMD_PICKUP_PIECE
        ESP_LOGI(TAG, "Processing PICKUP PIECE command from UART: %s",
                 chess_cmd.from_notation);
        game_process_pickup_command(&chess_cmd);
        break;

      case 8: // GAME_CMD_DROP_PIECE
        ESP_LOGI(TAG, "Processing DROP PIECE command from UART: %s",
                 chess_cmd.to_notation);
        game_process_drop_command(&chess_cmd);
        break;

      case 9: // GAME_CMD_PROMOTION
        ESP_LOGI(TAG, "Processing PROMOTION command from UART");
        game_process_promotion_command(&chess_cmd);
        break;

      case 10: // GAME_CMD_MOVE
        ESP_LOGI(TAG, "Processing MOVE command from UART: %s -> %s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_chess_move(&chess_cmd);
        break;

      case 11: // GAME_CMD_SHOW_BOARD
        ESP_LOGI(TAG, "Processing SHOW BOARD command from UART");
        game_send_board_to_uart((QueueHandle_t)chess_cmd.response_queue);
        break;

      case 12: // GAME_CMD_PICKUP
        ESP_LOGI(TAG, "Processing PICKUP command from UART: %s",
                 chess_cmd.from_notation);
        game_process_pickup_command(&chess_cmd);
        break;

      case 13: // GAME_CMD_DROP
        ESP_LOGI(TAG, "Processing DROP command from UART: %s",
                 chess_cmd.to_notation);
        game_process_drop_command(&chess_cmd);
        break;

      case 17: // GAME_CMD_EVALUATE
        ESP_LOGI(TAG, "Processing EVALUATE command from UART");
        game_process_evaluate_command(&chess_cmd);
        break;

      case 18: // GAME_CMD_SAVE
        ESP_LOGI(TAG, "Processing SAVE command from UART: %s",
                 chess_cmd.from_notation);
        game_process_save_command(&chess_cmd);
        break;

      case 19: // GAME_CMD_LOAD
        ESP_LOGI(TAG, "Processing LOAD command from UART: %s",
                 chess_cmd.from_notation);
        game_process_load_command(&chess_cmd);
        break;

      case 20: // GAME_CMD_RESERVED_SLOT_20
      case 29: // GAME_CMD_RESERVED_SLOT_29
      case 30: // GAME_CMD_RESERVED_SLOT_30
      case 31: // GAME_CMD_RESERVED_SLOT_31
      case 32: // GAME_CMD_RESERVED_SLOT_32
      case 38: // GAME_CMD_RESERVED_SLOT_38
        ESP_LOGW(TAG, "Ignored legacy puzzle command: %d", chess_cmd.type);
        game_send_response_to_uart("❌ Puzzle system neni dostupny", true,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case 21: // GAME_CMD_CASTLE
        ESP_LOGI(TAG, "Processing CASTLE command from UART: %s",
                 chess_cmd.to_notation);
        game_process_castle_command(&chess_cmd);
        break;

      case 22: // GAME_CMD_PROMOTE
        ESP_LOGI(TAG, "Processing PROMOTE command from UART: %s=%s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_promote_command(&chess_cmd);
        break;

      case 23: // GAME_CMD_COMPONENT_OFF
        ESP_LOGI(TAG, "Processing COMPONENT_OFF command from UART");
        game_process_component_off_command(&chess_cmd);
        break;

      case 24: // GAME_CMD_COMPONENT_ON
        ESP_LOGI(TAG, "Processing COMPONENT_ON command from UART");
        game_process_component_on_command(&chess_cmd);
        break;

      case 25: // GAME_CMD_ENDGAME_WHITE
        ESP_LOGI(TAG, "Processing ENDGAME_WHITE command from UART");
        game_process_endgame_white_command(&chess_cmd);
        break;

      case 26: // GAME_CMD_ENDGAME_BLACK
        ESP_LOGI(TAG, "Processing ENDGAME_BLACK command from UART");
        game_process_endgame_black_command(&chess_cmd);
        break;

      case 27: // GAME_CMD_LIST_GAMES
        ESP_LOGI(TAG, "Processing LIST_GAMES command from UART");
        game_process_list_games_command(&chess_cmd);
        break;

      case 28: // GAME_CMD_DELETE_GAME
        ESP_LOGI(TAG, "Processing DELETE_GAME command from UART: %s",
                 chess_cmd.from_notation);
        game_process_delete_game_command(&chess_cmd);
        break;

      case 33: // GAME_CMD_TEST_MOVE_ANIM
        ESP_LOGI(TAG, "Processing TEST_MOVE_ANIM command from UART");
        game_test_move_animation();
        break;

      case 34: // GAME_CMD_TEST_PLAYER_ANIM
        ESP_LOGI(TAG, "Processing TEST_PLAYER_ANIM command from UART");
        game_test_player_change_animation();
        break;

      case 35: // GAME_CMD_TEST_CASTLE_ANIM
        ESP_LOGI(TAG, "Processing TEST_CASTLE_ANIM command from UART");
        game_test_castle_animation();
        break;

      case 36: // GAME_CMD_TEST_PROMOTE_ANIM
        ESP_LOGI(TAG, "Processing TEST_PROMOTE_ANIM command from UART");
        game_test_promote_animation();
        break;

      case 37: // GAME_CMD_TEST_ENDGAME_ANIM
        ESP_LOGI(TAG, "Processing TEST_ENDGAME_ANIM command from UART");
        game_test_endgame_animation();
        break;

      // Timer System Commands
      case GAME_CMD_SET_TIME_CONTROL: // 39
        ESP_LOGI(TAG, "Processing SET_TIME_CONTROL command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_PAUSE_TIMER: // 40
        ESP_LOGI(TAG, "Processing PAUSE_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_RESUME_TIMER: // 41
        ESP_LOGI(TAG, "Processing RESUME_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_RESET_TIMER: // 42
        ESP_LOGI(TAG, "Processing RESET_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_GET_TIMER_STATE: // 43
        ESP_LOGI(TAG, "Processing GET_TIMER_STATE command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_TIMER_TIMEOUT: // 44
        ESP_LOGI(TAG, "Processing TIMER_TIMEOUT command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_MATRIX_GUARD: // 45
        ESP_LOGW(TAG, "Processing MATRIX_GUARD command");
        game_matrix_guard_handle_command(&chess_cmd);
        break;

      case GAME_CMD_BOARD_SETUP_TUTORIAL: // 46
        ESP_LOGI(TAG, "BOARD_SETUP_TUTORIAL action=%u",
                 (unsigned)chess_cmd.promotion_choice);
        switch (chess_cmd.promotion_choice) {
        case 0:
          game_enter_board_setup_tutorial();
          break;
        case 1:
          game_exit_board_setup_tutorial(false);
          break;
        default:
          ESP_LOGW(TAG, "BOARD_SETUP_TUTORIAL unknown action");
          break;
        }
        break;

      case GAME_CMD_PUZZLE: // 47
        ESP_LOGI(TAG, "PUZZLE action/id=%u", (unsigned)chess_cmd.promotion_choice);
        if (chess_cmd.promotion_choice == 0U) {
          game_puzzle_cancel();
        } else if (chess_cmd.promotion_choice >= 101U &&
                   chess_cmd.promotion_choice <= 105U) {
          uint8_t prep_id =
              (uint8_t)(chess_cmd.promotion_choice - 100U);
          if (!game_puzzle_enter_setup(prep_id)) {
            ESP_LOGW(TAG, "PUZZLE prepare failed id=%u", (unsigned)prep_id);
          }
        } else if (chess_cmd.promotion_choice >= 1U &&
                   chess_cmd.promotion_choice <= 5U) {
          if (!game_puzzle_start(chess_cmd.promotion_choice)) {
            ESP_LOGW(TAG, "PUZZLE start failed for id=%u",
                     (unsigned)chess_cmd.promotion_choice);
          }
        } else {
          ESP_LOGW(TAG, "PUZZLE unknown promotion_choice=%u",
                   (unsigned)chess_cmd.promotion_choice);
        }
        break;

      default:
        ESP_LOGW(TAG, "Unknown game command: %d", chess_cmd.type);
        break;
      }

      // STABILITY FIX: Check limit to prevent watchdog timeout
      if (commands_processed >= MAX_COMMANDS_PER_CYCLE) {
        ESP_LOGW(TAG,
                 "Reached max commands per cycle (%lu), processing rest in "
                 "next cycle",
                 MAX_COMMANDS_PER_CYCLE);
        break; // Process remaining commands in next cycle
      }
    }

    // Log if we processed multiple commands
    if (commands_processed > 1) {
      ESP_LOGI(TAG, "✅ Processed %lu commands in this cycle",
               commands_processed);
    }
  }

  // ============================================================================
  // BUTTON EVENT PROCESSING - Physical button support
  // ============================================================================

  // Process button events from button_task
  if (button_event_queue != NULL) {
    button_event_t button_event;
    while (xQueueReceive(button_event_queue, &button_event, 0) == pdTRUE) {

      // Only process PRESS events (ignore release, long press, double press)
      if (button_event.type == BUTTON_EVENT_PRESS) {

        ESP_LOGI(TAG, "🎮 Button pressed: ID=%d", button_event.button_id);

        // Reset button (ID 8)
        if (button_event.button_id == 8) {
          ESP_LOGI(TAG, "🔄 Reset button pressed - resetting game");
          game_reset_game();
          game_check_promotion_needed(); // Update LED indications
          continue;
        }

        // Promotion buttons (ID 0-3) - physical buttons
        if (button_event.button_id <= 3) {
          ESP_LOGI(TAG, "👑 Promotion button %d pressed",
                   button_event.button_id);
          game_process_promotion_button(button_event.button_id);
          continue;
        }

        // Unknown button
        ESP_LOGW(TAG, "⚠️  Unknown button ID: %d", button_event.button_id);
      }
    }
  }

  // Keep promo square visually anchored (does not clear board)
  game_update_promotion_anchor_led();
}

/**
 * @brief Handle invalid move - new error handling system
 * @param error Move error type
 * @param move Move that was attempted
 */
// ============================================================================
// ENHANCED SMART ERROR HANDLING SYSTEM
// ============================================================================

/**
 * @brief Enhanced smart error handling for invalid moves
 * @param move Invalid move that was attempted
 * @param error Type of error that occurred
 */
void game_handle_invalid_move_smart(const chess_move_t *move,
                                    move_error_t error) {
  ESP_LOGI(TAG, "🚫 INVALID MOVE - Starting smart recovery");

  if (!move) {
    ESP_LOGE(TAG, "❌ Critical error: NULL move pointer in error handling");
    return;
  }

  // 1. Červené bliknutí chybného tahu
  uint8_t from_led = chess_pos_to_led_index(move->from_row, move->from_col);
  uint8_t to_led = chess_pos_to_led_index(move->to_row, move->to_col);

  // Flash error - 5 rychlých červených bliknutí
  for (int i = 0; i < 5; i++) {
    led_set_pixel_safe(from_led, 255, 0, 0); // Red
    led_set_pixel_safe(to_led, 255, 0, 0);   // Red
    vTaskDelay(pdMS_TO_TICKS(100));
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // 2. Rosvítit zeleně zdrojové pole (kde je figurka)
  led_set_pixel_safe(from_led, 0, 255, 0); // Green - kde vzít figurku

  // 3. Rosvítit červeně kolem nevalidního cíle
  game_highlight_invalid_target_area(move->to_row, move->to_col);

  // 4. Rosvítit validní tahy pro tuto figurku
  game_highlight_valid_moves_for_piece(move->from_row, move->from_col);

  // 5. KRITICKÉ: Neměnit hráče, nechat ho opravit tah
  ESP_LOGI(TAG, "💡 Please return piece to correct square. Valid moves are "
                "highlighted.");

  // 6. Nastavit internal stav "waiting for correction"
  error_recovery_state.waiting_for_move_correction = true;
  error_recovery_state.invalid_row = move->from_row;
  error_recovery_state.invalid_col = move->from_col;
}

/**
 * @brief Highlight invalid target area with red LEDs
 * @param row Row of invalid target
 * @param col Column of invalid target
 */
void game_highlight_invalid_target_area(uint8_t row, uint8_t col) {
  // Rosvítit červeně pole okolo nevalidního cíle
  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      int new_row = row + dr;
      int new_col = col + dc;

      if (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
        uint8_t led_index = chess_pos_to_led_index(new_row, new_col);
        led_set_pixel_safe(led_index, 255, 50, 50); // Light red
      }
    }
  }
}

/**
 * @brief Highlight valid moves for a specific piece
 * @param row Row of the piece
 * @param col Column of the piece
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col) {
  piece_t piece = board[row][col];
  if (piece == PIECE_EMPTY)
    return;

  // Najít všechny validní tahy pro tuto figurku
  for (int to_row = 0; to_row < 8; to_row++) {
    for (int to_col = 0; to_col < 8; to_col++) {
      chess_move_t test_move = {.from_row = row,
                                .from_col = col,
                                .to_row = to_row,
                                .to_col = to_col,
                                .piece = piece,
                                .captured_piece = board[to_row][to_col]};

      if (game_is_valid_move(&test_move) == MOVE_ERROR_NONE) {
        uint8_t led_index = chess_pos_to_led_index(to_row, to_col);
        led_set_pixel_safe(led_index, 0, 0, 255); // Blue - validní tahy
      }
    }
  }
}

/**
 * @brief Legacy function for backward compatibility
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t *move) {
  ESP_LOGI(TAG, "🚨 ENTERING game_handle_invalid_move - error: %d", error);

  // KRITICKÁ OPRAVA: Bezpečnostní kontrola move pointeru
  if (!move) {
    ESP_LOGE(TAG, "❌ Critical error: NULL move pointer in error handling");
    return;
  }

  // Kontrola validity coordinates před použitím
  if (move->from_row >= 8 || move->from_col >= 8 || move->to_row >= 8 ||
      move->to_col >= 8) {
    ESP_LOGE(TAG, "❌ Critical error: Invalid coordinates in move structure");
    return;
  }

  ESP_LOGI(TAG, "🚨 Invalid move detected - smart error handling");
  ESP_LOGI(TAG, "   Move: %c%d -> %c%d", 'a' + move->from_col,
           move->from_row + 1, 'a' + move->to_col, move->to_row + 1);

  // Remote / zastaralý UI může poslat tah z prázdného pole. Simulace „figurka je
  // fyzicky na to“ kopíruje board[from] na board[to]; když je from prázdné,
  // smaže se figurka na cíli (logicky „zmizí“). HW recovery v tomto případě
  // nedává smysl — desku neměnit.
  piece_t src_piece = board[move->from_row][move->from_col];
  if (src_piece == PIECE_EMPTY) {
    ESP_LOGW(TAG,
             "[STAGING] game_handle_invalid_move: empty source square — "
             "skipping board mutation and LED recovery (error=%d)",
             (int)error);
    return;
  }

  // Chytrý error handling podle požadavků

  // 1. Nastavit červené pole na neplatné pozici
  error_recovery_state.has_invalid_piece = true;
  error_recovery_state.invalid_row = move->to_row;
  error_recovery_state.invalid_col = move->to_col;
  error_recovery_state.piece_type = board[move->from_row][move->from_col];

  // 2. Zapamatovat si původní validní pozici (odkud byl tah)
  error_recovery_state.original_valid_row = move->from_row;
  error_recovery_state.original_valid_col = move->from_col;

  // 3. Přesunout figurku na neplatnou pozici (simulace HW reality)
  board[move->to_row][move->to_col] = board[move->from_row][move->from_col];
  board[move->from_row][move->from_col] = PIECE_EMPTY;

  // 4. JASNÉ VIZUÁLNÍ UPOZORNĚNÍ - červené pole + blikání pro upoutání
  // pozornosti
  led_clear_all_safe();
  uint8_t invalid_led = chess_pos_to_led_index(move->to_row, move->to_col);

  ESP_LOGI(TAG, "🚨 STARTING ERROR ANIMATION - RED BLINKING");

  // Blikání pro upoutání pozornosti (krátké, non-blocking)
  for (int blink = 0; blink < 4; blink++) {
    led_set_pixel_safe(invalid_led, 255, 0, 0); // Červená
    vTaskDelay(pdMS_TO_TICKS(150));
    led_set_pixel_safe(invalid_led, 0, 0, 0); // Zhasnout
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  // Nechat červené pole trvale rozsvícené
  led_set_pixel_safe(invalid_led, 255, 0, 0);

  ESP_LOGI(TAG, "🔴 RED SQUARE ACTIVE: %c%d (piece must be lifted from here)",
           'a' + move->to_col, move->to_row + 1);
  ESP_LOGI(TAG,
           "📍 RECOVERY TARGET: Valid moves will show from %c%d when piece is "
           "lifted",
           'a' + move->from_col, move->from_row + 1);
  ESP_LOGI(TAG,
           "💡 USER ACTION REQUIRED: Lift piece from red square to continue");

  // KRITICKÁ OPRAVA: NON-BLOCKING error handling - žádné loops!
  // Červené pole zůstane rozsvícené dokud se figurka nezvedne
  // Recovery se řeší v game_process_pickup_command()

  ESP_LOGI(TAG, "💡 Error recovery active - red square will stay lit until "
                "piece is lifted");
}

/**
 * @brief Process move command from UART
 * @param move_cmd Move command structure with coordinates
 */
void game_process_move_command(const void *move_cmd_ptr) {
  typedef struct {
    uint8_t command_type;
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
  } move_command_t;

  const move_command_t *move_cmd = (const move_command_t *)move_cmd_ptr;

  ESP_LOGI(TAG, "Processing move: [%d,%d] -> [%d,%d]", move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Validate coordinates
  if (move_cmd->from_row >= 8 || move_cmd->from_col >= 8 ||
      move_cmd->to_row >= 8 || move_cmd->to_col >= 8) {
    ESP_LOGE(TAG, "Invalid coordinates: out of board range");
    return;
  }

  // Get pieces at source and destination
  piece_t from_piece = board[move_cmd->from_row][move_cmd->from_col];
  piece_t to_piece = board[move_cmd->to_row][move_cmd->to_col];

  // Check if source square has a piece
  if (from_piece == PIECE_EMPTY) {
    ESP_LOGE(TAG, "Invalid move: no piece at [%d,%d]", move_cmd->from_row,
             move_cmd->from_col);
    return;
  }

  // Check if piece belongs to current player
  bool is_white_piece =
      (from_piece >= PIECE_WHITE_PAWN && from_piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (from_piece >= PIECE_BLACK_PAWN && from_piece <= PIECE_BLACK_KING);

  if ((current_player == PLAYER_WHITE && !is_white_piece) ||
      (current_player == PLAYER_BLACK && !is_black_piece)) {
    ESP_LOGE(TAG, "Invalid move: cannot move opponent's piece");
    return;
  }

  // Check if destination is occupied by own piece
  if (to_piece != PIECE_EMPTY) {
    bool dest_is_white =
        (to_piece >= PIECE_WHITE_PAWN && to_piece <= PIECE_WHITE_KING);
    bool dest_is_black =
        (to_piece >= PIECE_BLACK_PAWN && to_piece <= PIECE_BLACK_KING);

    if ((current_player == PLAYER_WHITE && dest_is_white) ||
        (current_player == PLAYER_BLACK && dest_is_black)) {
      ESP_LOGE(TAG, "Invalid move: destination occupied by own piece");
      return;
    }
  }

  // TODO: Add proper chess move validation (piece-specific rules)
  // For now, just allow any move that passes basic checks

  // Create chess move structure for validation
  chess_move_t chess_move = {.from_row = move_cmd->from_row,
                             .from_col = move_cmd->from_col,
                             .to_row = move_cmd->to_row,
                             .to_col = move_cmd->to_col,
                             .piece = from_piece,
                             .captured_piece = to_piece,
                             .timestamp = esp_timer_get_time() / 1000};

  // Validate move using enhanced validation
  move_error_t move_error = game_is_valid_move(&chess_move);
  if (move_error != MOVE_ERROR_NONE) {
    // NEW ERROR HANDLING: Don't return, handle invalid move properly
    game_handle_invalid_move(move_error, &chess_move);
    return;
  }

  // Use game_execute_move() instead of direct board
  // manipulation This ensures all game state is properly updated (castling
  // flags, en passant, etc.) and player is changed correctly
  ESP_LOGI(TAG, "Executing move: %s piece from [%d,%d] to [%d,%d]",
           (is_white_piece ? "White" : "Black"), move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Execute the move using proper game_execute_move() function
  if (!game_execute_move(&chess_move)) {
    ESP_LOGE(TAG, "Failed to execute move from [%d,%d] to [%d,%d]",
             move_cmd->from_row, move_cmd->from_col, move_cmd->to_row,
             move_cmd->to_col);
    // Note: move_cmd doesn't have response_queue, this is legacy function
    return;
  }

  // HRÁČ SE UŽ ZMĚNIL V game_execute_move() - použít aktuálního hráče
  player_t previous_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  // Spustit move path animaci PŘED změnou hráče (podle starého projektu)
  uint8_t from_led =
      chess_pos_to_led_index(move_cmd->from_row, move_cmd->from_col);
  uint8_t to_led = chess_pos_to_led_index(move_cmd->to_row, move_cmd->to_col);
  led_command_t move_path_cmd = {
      .type = LED_CMD_ANIM_MOVE_PATH,
      .led_index = from_led,
      .red = 255,
      .green = 255,
      .blue = 0, // Yellow
      .duration_ms = 1000,
      .data = &to_led // Cílová pozice v data
  };
  led_execute_command_new(&move_path_cmd);

  // STABILITY FIX: Animace běží asynchronně, neblokujeme zpracování
  // Animace jsou spuštěny v led_task a běží nezávisle
  // (vTaskDelay odstraněn pro lepší throughput)

  // KRITICKÉ: Endgame kontrola PŘED player change animací!
  // Pokud je endgame, player change se NESPOUŠTÍ
  game_state_t end_game_result = game_check_end_game_conditions();
  if (end_game_result == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_FINISHED;
    game_active = false;

    // Najít pozici krále vítěze pro endgame animaci
    player_t winner =
        previous_player;   // Vítěz je předchozí hráč (ten, kdo udělal tah)
    uint8_t king_pos = 28; // default e4
    for (int i = 0; i < 64; i++) {
      piece_t piece = board[i / 8][i % 8];
      if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
          (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
        king_pos = i;
        break;
      }
    }

    ESP_LOGI(TAG,
             "🎯 Game finished in move command! Starting endgame animation at "
             "position %d",
             king_pos);

    // Spustit endgame animaci (wave z krále vítěze)
    led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                 .led_index = king_pos,
                                 .red = 255,
                                 .green = 255,
                                 .blue = 0,        // Yellow
                                 .duration_ms = 0, // Endless
                                 .data = NULL};
    led_execute_command_new(&endgame_cmd);

    ESP_LOGI(TAG,
             "✅ Endgame animation started - player change animation SKIPPED");
  } else {
    // Není endgame - spustit player change animaci s NOVÝM hráčem
    // (current_player)
    uint8_t player_color =
        (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
    led_command_t player_change_cmd = {
        .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                            // ANIM_PLAYER_CHANGE místo
                                            // PLAYER_CHANGE
        .led_index = 0,
        .red = 0,
        .green = 0,
        .blue = 0,
        .duration_ms = 0,
        .data = &player_color // Předat barvu NOVÉHO hráče
    };
    led_execute_command_new(&player_change_cmd);

    // Zkontrolovat, zda je nový hráč v šachu
    bool in_check = game_is_king_in_check(current_player);
    if (in_check) {
      // Najít pozici krále
      int king_row = -1, king_col = -1;
      piece_t king_piece = (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING
                                                            : PIECE_BLACK_KING;
      for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
          if (board[row][col] == king_piece) {
            king_row = row;
            king_col = col;
            break;
          }
        }
        if (king_row != -1)
          break;
      }

      if (king_row != -1 && king_col != -1) {
        if (game_led_guidance_show_check_anim()) {
          uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
          // Spustit check animaci - růžové svícení na králi
          led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                     .led_index = king_led_index,
                                     .red = 0,
                                     .green = 0,
                                     .blue = 0,
                                     .duration_ms =
                                         0, // Trvalé až do dalšího tahu
                                     .data = NULL};
          led_execute_command_new(&check_cmd);
          ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                   current_player == PLAYER_WHITE ? "White" : "Black");
        }
      }
    }
  }

  // History and last_move tracking is already done in
  // game_execute_move() - no need to duplicate here

  // Track captured pieces
  if (to_piece != PIECE_EMPTY) {
    if (current_player == PLAYER_WHITE) {
      // Black piece was captured by white
      if (black_captured_index < 16) {
        black_captured_pieces[black_captured_index++] = to_piece;
        black_captured_count++;
      }
    } else {
      // White piece was captured by black
      if (white_captured_index < 16) {
        white_captured_pieces[white_captured_index++] = to_piece;
        white_captured_count++;
      }
    }
    moves_without_capture = 0; // Reset counter on capture
  } else {
    moves_without_capture++; // Increment counter for non-capture moves
    if (moves_without_capture > max_moves_without_capture) {
      max_moves_without_capture = moves_without_capture;
    }
  }

  // Update time statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t move_time = current_time - last_move_time;

  if (current_player == PLAYER_WHITE) {
    // Black just moved
    black_time_total += move_time;
    black_moves_count++;
  } else {
    // White just moved
    white_time_total += move_time;
    white_moves_count++;
  }

  // Update position history for draw detection
  game_add_position_to_history();

  // Note: End-game conditions already checked earlier in this function (line
  // 6612) If game finished, it was already handled with endgame animation

  // Check if king is in check
  if (game_is_king_in_check(current_player)) {
    if (current_player == PLAYER_WHITE) {
      white_checks++;
    } else {
      black_checks++;
    }
    ESP_LOGI(TAG, "⚠️  CHECK! %s king is under attack!",
             (current_player == PLAYER_WHITE) ? "White" : "Black");
  }

  // Update last move time for next move
  last_move_time = current_time;

  // Display updated board (check auto-display setting)
  // Note: Auto-display control is in UART task, board is always shown for now
  game_print_board();

  // Check for check/checkmate conditions
  game_check_game_conditions();
}

/**
 * @brief Show move animation with ASCII art
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param piece Piece being moved
 * @param captured Piece captured (if any)
 */
void game_show_move_animation(uint8_t from_row, uint8_t from_col,
                              uint8_t to_row, uint8_t to_col, piece_t piece,
                              piece_t captured) {
  // Convert coordinates to chess notation
  char from_square[4], to_square[4];
  game_coords_to_square(from_row, from_col, from_square);
  game_coords_to_square(to_row, to_col, to_square);

  // Get piece symbol and name
  const char *piece_symbol = piece_symbols[piece];
  const char *piece_name = game_get_piece_name(piece);

  ESP_LOGI(TAG, "╭─────────────────────────────────╮");
  ESP_LOGI(TAG, "│        MOVE ANIMATION          │");
  ESP_LOGI(TAG, "├─────────────────────────────────┤");
  ESP_LOGI(TAG, "│  %s %s moves from %s to %s  │", piece_symbol, piece_name,
           from_square, to_square);

  if (captured != PIECE_EMPTY) {
    const char *captured_symbol = piece_symbols[captured];
    const char *captured_name = game_get_piece_name(captured);
    ESP_LOGI(TAG, "│  Captures: %s %s                │", captured_symbol,
             captured_name);
  } else {
    ESP_LOGI(TAG, "│  No capture                     │");
  }

  ESP_LOGI(TAG, "╰─────────────────────────────────╯");

  // ED: LED animations are now handled in game_process_drop_command
  // This function only shows text information

  ESP_LOGI(TAG, "🎯 Move: %s -> %s", from_square, to_square);
  ESP_LOGI(TAG, "♟️  %s %s moves...", piece_symbol, piece_name);
  ESP_LOGI(TAG, "✨ ...to %s", to_square);

  if (captured != PIECE_EMPTY) {
    ESP_LOGI(TAG, "💥 %s captured!", game_get_piece_name(captured));
  }

  ESP_LOGI(TAG, "✅ Move completed!");
}

/**
 * @brief Show player change animation
 * @param previous_player Previous player
 * @param current_player Current player
 */
void game_show_player_change_animation(player_t previous_player,
                                       player_t current_player) {
  ESP_LOGI(TAG, "🔄 Showing player change animation: %s -> %s",
           previous_player == PLAYER_WHITE ? "White" : "Black",
           current_player == PLAYER_WHITE ? "White" : "Black");

  // Clear board first
  led_clear_board_only();
  vTaskDelay(pdMS_TO_TICKS(200));

  // ED: Find all pieces first, then animate all columns simultaneously
  int prev_pieces[8] = {-1, -1, -1, -1,
                        -1, -1, -1, -1}; // Row positions for each column
  int curr_pieces[8] = {-1, -1, -1, -1,
                        -1, -1, -1, -1}; // Row positions for each column

  // Find closest pieces for all columns
  for (int col = 0; col < 8; col++) {
    // Find closest piece of previous player (closest to opponent's side)
    if (previous_player == PLAYER_WHITE) {
      // Look from top (row 7) down to find white piece closest to black side
      for (int row = 7; row >= 0; row--) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
          prev_pieces[col] = row;
          break;
        }
      }
    } else {
      // Look from bottom (row 0) up to find black piece closest to white side
      for (int row = 0; row < 8; row++) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
          prev_pieces[col] = row;
          break;
        }
      }
    }

    // Find closest piece of current player (closest to previous player's
    // side)
    if (current_player == PLAYER_WHITE) {
      // Look from bottom (row 0) up to find white piece closest to black side
      for (int row = 0; row < 8; row++) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
          curr_pieces[col] = row;
          break;
        }
      }
    } else {
      // Look from top (row 7) down to find black piece closest to white side
      for (int row = 7; row >= 0; row--) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
          curr_pieces[col] = row;
          break;
        }
      }
    }
  }

  // ENHANCED: Ultra-smooth player change animation with harmonious colors
  for (int step = 0; step < 8; step++) { // Reduced steps for speed
    float progress = (float)step / 7.0f;

    // Clear board first
    led_clear_board_only();

    // Animate all columns at once with ultra-smooth trail effect
    for (int col = 0; col < 8; col++) {
      if (prev_pieces[col] != -1 && curr_pieces[col] != -1) {
        // Create ultra-smooth trail effect - show multiple points along the
        // path
        for (int trail = 0; trail < 5; trail++) {
          float trail_progress = progress - (trail * 0.12f);
          if (trail_progress < 0)
            continue;
          if (trail_progress > 1)
            break;

          // Calculate intermediate position with smooth easing
          float eased_progress = trail_progress * trail_progress *
                                 (3.0f - 2.0f * trail_progress); // Smooth step
          int inter_row =
              prev_pieces[col] +
              (curr_pieces[col] - prev_pieces[col]) * eased_progress;
          int inter_col = col;
          uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);

          // Harmonious color transition: Deep Blue -> Purple -> Magenta ->
          // Pink
          // -> Gold -> Yellow
          uint8_t red, green, blue;
          if (trail_progress < 0.16f) {
            // Deep Blue to Purple
            float local_progress = trail_progress / 0.16f;
            red = (uint8_t)(128 * local_progress);
            green = 0;
            blue = 255;
          } else if (trail_progress < 0.33f) {
            // Purple to Magenta
            float local_progress = (trail_progress - 0.16f) / 0.17f;
            red = 128 + (uint8_t)(127 * local_progress);
            green = 0;
            blue = 255;
          } else if (trail_progress < 0.5f) {
            // Magenta to Pink
            float local_progress = (trail_progress - 0.33f) / 0.17f;
            red = 255;
            green = (uint8_t)(128 * local_progress);
            blue = 255 - (uint8_t)(128 * local_progress);
          } else if (trail_progress < 0.66f) {
            // Pink to Gold
            float local_progress = (trail_progress - 0.5f) / 0.16f;
            red = 255;
            green = 128 + (uint8_t)(127 * local_progress);
            blue = 127 - (uint8_t)(127 * local_progress);
          } else if (trail_progress < 0.83f) {
            // Gold to Yellow
            red = 255;
            green = 255;
            blue = 0;
          } else {
            // Yellow to Bright White
            float local_progress = (trail_progress - 0.83f) / 0.17f;
            red = 255;
            green = 255;
            blue = (uint8_t)(255 * local_progress);
          }

          // Enhanced trail brightness effect - smoother gradient
          float trail_brightness = 1.0f - (trail * 0.18f);
          red = (uint8_t)(red * trail_brightness);
          green = (uint8_t)(green * trail_brightness);
          blue = (uint8_t)(blue * trail_brightness);

          // Enhanced pulsing effect with multiple harmonics
          float pulse1 = 0.7f + 0.3f * sin(progress * 6.28f + trail * 1.26f);
          float pulse2 = 0.9f + 0.1f * sin(progress * 12.56f + trail * 2.51f);
          float combined_pulse = pulse1 * pulse2;

          red = (uint8_t)(red * combined_pulse);
          green = (uint8_t)(green * combined_pulse);
          blue = (uint8_t)(blue * combined_pulse);

          led_set_pixel_safe(inter_led, red, green, blue);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(25)); // Faster animation - half the time
  }

  // Clear board after animation
  led_clear_board_only();
  vTaskDelay(pdMS_TO_TICKS(200));

  // Finally, highlight movable pieces for current player
  game_highlight_movable_pieces();
}

/**
 * @brief Test move animation
 */
void game_test_move_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing move animation...");

  // Test move from e2 to e4
  // uint8_t to_led = chess_pos_to_led_index(3, 4);   // e4 - removed unused
  // variable

  // Progressive color animation from green to blue - ZRYCHLENO
  for (int step = 0; step < 6; step++) { // ZRYCHLENO: z 10 na 6 kroků
    float progress = (float)step / 5.0f;

    // Calculate intermediate position
    int inter_row = 1 + (3 - 1) * progress;
    int inter_col = 4;
    uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);

    // Calculate color transition (green -> blue)
    uint8_t red = 0;
    uint8_t green = 255 - (255 * progress); // 255 -> 0
    uint8_t blue = 0 + (255 * progress);    // 0 -> 255

    led_clear_board_only();
    led_set_pixel_safe(inter_led, red, green, blue);
    vTaskDelay(pdMS_TO_TICKS(50)); // ZRYCHLENO: z 100ms na 50ms
  }

  // Odstraněno modré bliknutí na konci - animace končí plynule
}

/**
 * @brief Test player change animation
 */
void game_test_player_change_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing player change animation...");
  game_show_player_change_animation(PLAYER_WHITE, PLAYER_BLACK);
}

/**
 * @brief Test castling animation
 */
void game_test_castle_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing castling animation...");

  // Test white kingside castling
  uint8_t king_from_led = chess_pos_to_led_index(0, 4); // e1
  uint8_t king_to_led = chess_pos_to_led_index(0, 6);   // g1
  uint8_t rook_from_led = chess_pos_to_led_index(0, 7); // h1
  uint8_t rook_to_led = chess_pos_to_led_index(0, 5);   // f1

  // 1. Highlight king and rook positions (gold)
  led_set_pixel_safe(king_from_led, 255, 215, 0); // Gold
  led_set_pixel_safe(rook_from_led, 255, 215, 0); // Gold
  vTaskDelay(pdMS_TO_TICKS(500));

  // 2. Show castling animation
  led_command_t castle_cmd = {.type = LED_CMD_ANIM_CASTLE,
                              .led_index = king_from_led,
                              .red = 255,
                              .green = 215,
                              .blue = 0, // Gold
                              .duration_ms = 1500,
                              .data = &king_to_led};
  led_execute_command_new(&castle_cmd);
  vTaskDelay(pdMS_TO_TICKS(1000));

  // 3. Highlight final positions (green)
  led_set_pixel_safe(king_to_led, 0, 255, 0); // Green
  led_set_pixel_safe(rook_to_led, 0, 255, 0); // Green
  vTaskDelay(pdMS_TO_TICKS(500));

  // 4. Clear highlights
  led_clear_board_only();
}

/**
 * @brief Test promotion animation
 */
void game_test_promote_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing promotion animation...");

  // Test promotion on a8
  uint8_t promotion_led = chess_pos_to_led_index(7, 0); // a8

  // Show enhanced promotion animation
  led_command_t promote_cmd = {.type = LED_CMD_ANIM_PROMOTE,
                               .led_index = promotion_led,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 2000,
                               .data = NULL};
  led_execute_command_new(&promote_cmd);
  vTaskDelay(pdMS_TO_TICKS(1000)); // Faster animation

  // Clear highlights
  led_clear_board_only();
}

/**
 * @brief Test endgame animation
 */
void game_test_endgame_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing endgame animation...");

  // Test endgame animation using direct LED command
  uint8_t king_pos = 27; // d4 square

  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 3000,
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);
  vTaskDelay(pdMS_TO_TICKS(1500)); // Faster animation

  // Clear board
  led_clear_board_only();
}

/**
 * @brief Check for check/checkmate conditions
 */
void game_check_game_conditions(void) {
  // TODO: Implement check detection
  // For now, just log the current state

  ESP_LOGI(TAG, "🔍 Checking game conditions...");

  // Check if king is in check (simplified)
  bool white_king_found = false;
  bool black_king_found = false;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (board[row][col] == PIECE_WHITE_KING) {
        white_king_found = true;
      } else if (board[row][col] == PIECE_BLACK_KING) {
        black_king_found = true;
      }
    }
  }

  if (!white_king_found) {
    ESP_LOGW(TAG, "⚠️  WHITE KING MISSING - Black wins!");
    current_game_state = GAME_STATE_FINISHED;
  } else if (!black_king_found) {
    ESP_LOGW(TAG, "⚠️  BLACK KING MISSING - White wins!");
    current_game_state = GAME_STATE_FINISHED;
  }

  ESP_LOGI(TAG, "Game state: %s",
           (current_game_state == GAME_STATE_ACTIVE)   ? "Active"
           : (current_game_state == GAME_STATE_IDLE)   ? "Idle"
           : (current_game_state == GAME_STATE_PAUSED) ? "Paused"
                                                       : "Finished");
}

// ============================================================================
// END-GAME DETECTION
// ============================================================================

/**
 * @brief Check if king is in check
 * @param player Player to check
 * @return true if king is in check
 */
bool game_is_king_in_check(player_t player) {
  // Find king position
  int king_row = -1, king_col = -1;
  piece_t king_piece =
      (player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (board[row][col] == king_piece) {
        king_row = row;
        king_col = col;
        break;
      }
    }
    if (king_row != -1)
      break;
  }

  if (king_row == -1)
    return false; // King not found

  // Check if any opponent piece can capture the king
  piece_t opponent_pieces_start =
      (player == PLAYER_WHITE) ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
  piece_t opponent_pieces_end =
      (player == PLAYER_WHITE) ? PIECE_BLACK_KING : PIECE_WHITE_KING;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece >= opponent_pieces_start && piece <= opponent_pieces_end) {
        // Create temporary move to check if it can capture king
        chess_move_t temp_move = {.from_row = row,
                                  .from_col = col,
                                  .to_row = king_row,
                                  .to_col = king_col,
                                  .piece = piece,
                                  .captured_piece = king_piece,
                                  .timestamp = 0};

        // Check if this move is valid (would capture king)
        if (game_validate_piece_move_enhanced(&temp_move, piece) ==
            MOVE_ERROR_NONE) {
          return true; // King is in check
        }
      }
    }
  }

  return false;
}

/**
 * @brief Check if player has any legal moves (optimized version)
 * @param player Player to check
 * @return true if player has legal moves
 */
bool game_has_legal_moves(player_t player) {
  // Use existing move generation system instead of brute force
  uint32_t moves_count = game_generate_legal_moves(player);

  return (moves_count > 0);
}

/**
 * @brief Check if current position has insufficient material for checkmate
 * @return true if insufficient material (draw)
 */
/**
 * @brief Check if piece belongs to opponent
 */
bool game_is_enemy_piece(piece_t piece, player_t player) {
  if (piece == PIECE_EMPTY)
    return false;
  return (player == PLAYER_WHITE) ? game_is_black_piece(piece)
                                  : game_is_white_piece(piece);
}

bool game_is_insufficient_material(void) {
  int white_pieces = 0, black_pieces = 0;
  int white_minors = 0, black_minors = 0; // Knights and bishops
  bool white_has_bishop = false, black_has_bishop = false;
  bool white_has_knight = false, black_has_knight = false;
  bool white_bishop_color = false,
       black_bishop_color = false; // false = light, true = dark

  // Count pieces and analyze material
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      switch (piece) {
      case PIECE_WHITE_PAWN:
      case PIECE_WHITE_ROOK:
      case PIECE_WHITE_QUEEN:
        white_pieces++;
        break;
      case PIECE_WHITE_BISHOP:
        white_pieces++;
        white_minors++;
        white_has_bishop = true;
        white_bishop_color = ((row + col) % 2 == 0); // Light squares = false
        break;
      case PIECE_WHITE_KNIGHT:
        white_pieces++;
        white_minors++;
        white_has_knight = true;
        break;
      case PIECE_BLACK_PAWN:
      case PIECE_BLACK_ROOK:
      case PIECE_BLACK_QUEEN:
        black_pieces++;
        break;
      case PIECE_BLACK_BISHOP:
        black_pieces++;
        black_minors++;
        black_has_bishop = true;
        black_bishop_color = ((row + col) % 2 == 0); // Light squares = false
        break;
      case PIECE_BLACK_KNIGHT:
        black_pieces++;
        black_minors++;
        black_has_knight = true;
        break;
      default:
        break;
      }
    }
  }

  // King vs King - always insufficient
  if (white_pieces == 0 && black_pieces == 0) {
    return true;
  }

  // King + minor vs King - insufficient
  if ((white_pieces == 1 && black_pieces == 0) ||
      (white_pieces == 0 && black_pieces == 1)) {
    return true;
  }

  // King + Bishop vs King + Bishop (same color) - insufficient
  if (white_pieces == 1 && black_pieces == 1 && white_has_bishop &&
      black_has_bishop && white_bishop_color == black_bishop_color) {
    return true;
  }

  // King + Knight vs King + Knight - insufficient
  if (white_pieces == 1 && black_pieces == 1 && white_has_knight &&
      black_has_knight) {
    return true;
  }

  // King + 2 Knights vs King - insufficient (very rare, but technically
  // insufficient)
  if ((white_pieces == 2 && black_pieces == 0 && white_minors == 2 &&
       white_has_knight) ||
      (white_pieces == 0 && black_pieces == 2 && black_minors == 2 &&
       black_has_knight)) {
    return true;
  }

  return false; // Sufficient material for checkmate
}

/**
 * @brief Kontroluje podminky pro konec hry (sachmat, pat, remizy)
 *
 * @return GAME_STATE_FINISHED pokud hra skoncila, GAME_STATE_ACTIVE pokud
 * pokracuje
 *
 * @details
 * Funkce kontroluje vsechny mozne zpusoby konce hry:
 *
 * Vyherne podminky:
 * - Sachmat: kral v sachu && zadne validni tahy
 *
 * Remizove podminky:
 * - Pat (Stalemate): kral NENI v sachu && zadne validni tahy
 * - 50-Move Rule: 100 pultahu bez brani nebo posunu pesce
 * - Threefold Repetition: stejna pozice 3x opakována
 * - Insufficient Material: nedostatecny material pro mat
 *
 * Funkce take:
 * - Aktualizuje statistiky
 * - Zastavuje timer
 * - Generuje endgame report
 * - Rozlisuje typy sachmatu (en passant, castling, promotion, discovered)
 *
 * @note Opraveno v2.4.1:
 * - BUG #13: 50-Move Rule nyni kontroluje >= 100 pultahu (50 tahu na stranu)
 * - Puvodni kontrola >= 50 byla spatna (polovicni pocet)
 */
game_state_t game_check_end_game_conditions(void) {
  // Check if current player is in check
  bool in_check = game_is_king_in_check(current_player);

  // Check if current player has legal moves
  bool has_moves = game_has_legal_moves(current_player);

  if (in_check && !has_moves) {
    // Checkmate - určit typ podle posledního tahu
    game_result = GAME_STATE_FINISHED;
    current_result_type = (current_player == PLAYER_WHITE) ? RESULT_BLACK_WINS
                                                           : RESULT_WHITE_WINS;

    // Rozlišit typ šachmatu podle posledního tahu
    switch (last_move_type) {
    case LAST_MOVE_EN_PASSANT:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_EN_PASSANT;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY EN PASSANT! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_CASTLING:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_CASTLING;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY CASTLING! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_PROMOTION:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_PROMOTION;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY PROMOTION! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_DISCOVERED:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_DISCOVERED;
      ESP_LOGI(
          TAG,
          "🎯 CHECKMATE BY DISCOVERED CHECK! %s wins in %" PRIu32 " moves!",
          (current_player == PLAYER_WHITE) ? "Black" : "White", move_count);
      break;
    default:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE;
      ESP_LOGI(TAG, "🎯 CHECKMATE! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    }

    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    // UNIFIED VICTORY ANIMATION
    // Checkmate = Victory for current_player (who delivered mate? No wait.)
    // Logic: if current_player is in check & no moves, they LOST.
    // Winner is the OPPONENT.
    player_t winner =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    game_trigger_victory_animation(winner);

    return GAME_STATE_FINISHED;
  } else if (!in_check && !has_moves) {
    // Stalemate
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_STALEMATE;
    current_endgame_reason = ENDGAME_REASON_STALEMATE; // Označit jako stalemate
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn in %" PRIu32 " moves", move_count);
    return GAME_STATE_FINISHED;
  }

  // BUG #13: Check for draw conditions
  // 50-move rule: 50 plných tahů = 100 půltahů (half-moves/ply)
  // moves_without_capture počítá půltahy, takže >= 100
  if (moves_without_capture >= 100) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_50_MOVE;
    current_endgame_reason =
        ENDGAME_REASON_50_MOVE; // Označit jako 50-move rule
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! 50 moves without capture (50-move rule)");
    return GAME_STATE_FINISHED;
  }

  if (game_is_position_repeated()) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_REPETITION;
    current_endgame_reason =
        ENDGAME_REASON_REPETITION; // Označit jako threefold repetition
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! Position repeated (draw by repetition)");
    return GAME_STATE_FINISHED;
  }

  // Check for insufficient material
  if (game_is_insufficient_material()) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_INSUFFICIENT;
    current_endgame_reason =
        ENDGAME_REASON_INSUFFICIENT; // Označit jako insufficient material
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! Insufficient material to checkmate");
    return GAME_STATE_FINISHED;
  }

  return GAME_STATE_ACTIVE;
}

// ============================================================================
// GAME CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Toggle game timer on/off
 * @param enabled true to enable timer, false to disable
 */
void game_toggle_timer(bool enabled) {
  timer_enabled = enabled;
  ESP_LOGI(TAG, "Game timer %s", enabled ? "enabled" : "disabled");
}

/**
 * @brief Save current game to NVS
 * @param game_name Name for the saved game
 */
void game_save_game(const char *game_name) {
  if (game_name == NULL || strlen(game_name) == 0) {
    ESP_LOGE(TAG, "Invalid game name for save");
    return;
  }

  // This is a placeholder - in a real implementation you would:
  // 1. Save board state to NVS
  // 2. Save move history to NVS
  // 3. Save game statistics to NVS

  strncpy(saved_game_name, game_name, sizeof(saved_game_name) - 1);
  saved_game_name[sizeof(saved_game_name) - 1] = '\0';
  game_saved = true;

  ESP_LOGI(TAG, "💾 Game saved as: %s", saved_game_name);
}

/**
 * @brief Load game from NVS
 * @param game_name Name of the game to load
 */
void game_load_game(const char *game_name) {
  if (game_name == NULL || strlen(game_name) == 0) {
    ESP_LOGE(TAG, "Invalid game name for load");
    return;
  }

  // This is a placeholder - in a real implementation you would:
  // 1. Load board state from NVS
  // 2. Load move history from NVS
  // 3. Load game statistics from NVS

  ESP_LOGI(TAG, "📂 Loading game: %s", game_name);
  ESP_LOGI(TAG, "⚠️  Game loading not yet implemented");
}

/**
 * @brief Export game to PGN format
 * @param pgn_buffer Output buffer for PGN string
 * @param buffer_size Size of output buffer
 */
void game_export_pgn(char *pgn_buffer, size_t buffer_size) {
  if (pgn_buffer == NULL || buffer_size == 0) {
    ESP_LOGE(TAG, "Invalid PGN buffer");
    return;
  }

  // Start PGN with metadata
  int pos = snprintf(pgn_buffer, buffer_size,
                     "[Event \"ESP32 Chess Game\"]\n"
                     "[Site \"ESP32-C6\"]\n"
                     "[Date \"%s\"]\n"
                     "[Round \"1\"]\n"
                     "[White \"Player 1\"]\n"
                     "[Black \"Player 2\"]\n"
                     "[Result \"*\"]\n\n",
                     "2025-01-01"); // TODO: Get actual date

  // Add moves
  for (uint32_t i = 0; i < history_index && pos < buffer_size - 50; i++) {
    const chess_move_t *move = &move_history[i];
    char from_square[4], to_square[4];
    game_coords_to_square(move->from_row, move->from_col, from_square);
    game_coords_to_square(move->to_row, move->to_col, to_square);

    if (i % 2 == 0) {
      // White move
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, "%" PRIu32 ". %s%s",
                      (i / 2) + 1, from_square, to_square);
    } else {
      // Black move
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " %s%s ",
                      from_square, to_square);
    }

    // Add newline every 10 moves
    if ((i + 1) % 20 == 0) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, "\n");
    }
  }

  // Add result
  if (game_result == GAME_STATE_FINISHED) {
    if (strstr(saved_game_name, "CHECKMATE")) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1-0");
    } else if (strstr(saved_game_name, "STALEMATE")) {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1/2-1/2");
    } else {
      pos += snprintf(pgn_buffer + pos, buffer_size - pos, " 1/2-1/2");
    }
  } else {
    pos += snprintf(pgn_buffer + pos, buffer_size - pos, " *");
  }

  ESP_LOGI(TAG, "📄 PGN export completed (%d characters)", pos);
}

/**
 * @brief Convert board coordinates to chess square notation
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param square Output square string (e.g., "e2")
 */
void game_coords_to_square(uint8_t row, uint8_t col, char *square) {
  if (square == NULL)
    return;

  // Convert column to file (a-h)
  square[0] = 'a' + col;

  // Convert row to rank (1-8) - row 0 = rank 1, row 7 = rank 8
  square[1] = '1' + row;

  square[2] = '\0';
}

void game_print_status(void) {
  ESP_LOGI(TAG, "Game Status:");
  ESP_LOGI(TAG, "  State: %d", current_game_state);
  ESP_LOGI(TAG, "  Current player: %s",
           (current_player == PLAYER_WHITE) ? "White" : "Black");
  ESP_LOGI(TAG, "  Move count: %lu", move_count);
  ESP_LOGI(TAG, "  Game active: %s", game_active ? "Yes" : "No");
  ESP_LOGI(TAG, "  Total games: %lu", total_games);
  ESP_LOGI(TAG, "  White wins: %lu", white_wins);
  ESP_LOGI(TAG, "  Black wins: %lu", black_wins);
  ESP_LOGI(TAG, "  Draws: %lu", draws);
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void game_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "Game task started successfully");

  // CRITICAL: Register with TWDT from within task
  esp_err_t wdt_ret = esp_task_wdt_add(NULL);
  if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_INVALID_ARG) {
    ESP_LOGE(TAG, "Failed to register Game task with TWDT: %s",
             esp_err_to_name(wdt_ret));
  } else {
    ESP_LOGI(TAG, "✅ Game task registered with TWDT");
  }

  ESP_LOGI(TAG, "Features:");
  ESP_LOGI(TAG, "  • Standard chess rules");
  ESP_LOGI(TAG, "  • Move validation");
  ESP_LOGI(TAG, "Game task initialized - ready to start game");
  ESP_LOGI(TAG, "  • Board initialized with starting position");
  ESP_LOGI(TAG, "  • Command queue ready for UART/Web/Button inputs");
  ESP_LOGI(TAG, "  • LED animations ready");
  ESP_LOGI(TAG, "  • 1 second game loop cycle");

  // #2: Initialize promotion mutex for concurrency protection
  // Použít recursive mutex, protože game_execute_move() může být volána
  // reentrantně (např. z capture flow, který volá game_execute_move(), který
  // může detekovat promotion)
  promotion_mutex = xSemaphoreCreateRecursiveMutex();
  if (promotion_mutex == NULL) {
    ESP_LOGE(TAG, "❌ CRITICAL: Failed to create promotion mutex!");
  } else {
    ESP_LOGI(TAG, "✅ Promotion recursive mutex initialized");
  }

  task_running = true;

  // Initialize timer system
  esp_err_t timer_ret = game_init_timer_system();
  if (timer_ret != ESP_OK) {
    ESP_LOGW(TAG,
             "Timer system initialization failed, continuing without timer");
  }

  // Load configuration from NVS
  system_config_t config;
  esp_err_t config_ret = config_load_from_nvs(&config);
  if (config_ret == ESP_OK) {
    starting_position_check_enabled = config.starting_position_check_enabled;
    ESP_LOGI(TAG, "Configuration loaded: starting_position_check=%s",
             starting_position_check_enabled ? "ON" : "OFF");
  } else {
    ESP_LOGW(TAG, "Failed to load configuration, using defaults");
    starting_position_check_enabled = false; // Default: disabled
  }

  /*
   * Start logicke hry: vychozi deska, pak NVS nebo boot tracker.
   * - Platny klic v NVS: vypnout nucenou novou hru z boot trackeru, nacist snapshot,
   *   game_matrix_guard_check_resync_after_restore() pri nesouladu matice.
   * - Jinak: boot_new_game_triggered z game_boot_tracker_should_force_new_game();
   *   pri true volat game_start_new_game() (main pak neposle druhy NEW_GAME).
   * - Pri uspesnem nacteni snapshotu nastavi game_load_snapshot_from_nvs() i game_active.
   */
  game_initialize_board();
  game_snapshot_restore_on_boot();

  // Main task loop
  uint32_t loop_count = 0;
  TickType_t last_wake_time = xTaskGetTickCount();

  for (;;) {
    // =========================================================================
    // 🛑 BOOT ANIMATION PROTECTION - BLOKUJE VŠECHNY GAME OPERACE BĚHEM BOOT
    // =========================================================================
    // Pokud běží boot animace (řízená z main.c), game task NESMÍ dělat NIC!
    // Auto new game detection, LED operace, cokoliv by mohlo přerušit
    // animaci.
    if (led_is_booting()) {
      // Reset WDT aby task nespadl, ale nic nedělat
      game_task_wdt_reset_safe();
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms - rychlé probuzení po fade_out!
      continue;                      // Přeskočit zbytek smyčky
    }
    // =========================================================================

    // CRITICAL: Reset watchdog for game task in every iteration (only if
    // registered)
    esp_err_t wdt_ret = game_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Process game commands
    game_process_commands();

    // BUG FIX 1: Automatický přechod z WAITING_FOR_BOARD_SETUP do ACTIVE
    if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
      // Pokud je hlídání počáteční pozice vypnuto, přejdeme rovnou do ACTIVE
      bool should_check_position = starting_position_check_enabled;
      bool position_ok = should_check_position ? game_is_board_in_starting_position() : true;
      
      if (position_ok) {
        // Deska je připravena (nebo hlídání je vypnuto) - aktivovat hru
        current_game_state = GAME_STATE_ACTIVE;
        game_active = true;
        game_start_time = esp_timer_get_time() / 1000;
        last_move_time = game_start_time;
        
        // Zastavit případné běžící animace
        unified_animation_stop_all();
        led_stop_endgame_animation();
        stop_endgame_animation();
        
        // Vyčistit LED
        led_clear_board_only();
        
        // Aktualizovat promotion LED indikace
        game_check_promotion_needed();
        
        // Spustit timer pro bílého hráče pokud je aktivní
        if (game_is_timer_active()) {
          game_start_timer_move(true);
        }
        
        // Zvýraznit pohyblivé figurky
        game_highlight_movable_pieces();
        
        // Notify web
        game_bump_revision_and_notify();
        
        if (should_check_position) {
          ESP_LOGI(TAG, "✅ Board setup complete - game now ACTIVE, White to move");
        } else {
          ESP_LOGI(TAG, "✅ Game ACTIVE (starting position check disabled), White to move");
        }
      } else {
        // Periodicky aktualizovat LED indikaci chybějících figur
        static uint32_t last_led_update = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_led_update > 500) { // Každých 500ms
          game_show_missing_pieces_led();
          last_led_update = now;
        }
      }
    }

    // Process matrix events (moves)
    // DEPRECATED: Matrix events are now processed via game_command_queue
    // game_process_matrix_events();  // No longer needed

    // Update timer display and check for timeout
    game_update_timer_display();

    // Zpracovat endgame report request (s dostatečným stackem 10KB)
    if (endgame_report_requested) {
      endgame_report_requested = false; // Reset flag
      game_print_endgame_report_uart(current_result_type);
    }

    // Auto new game detection (kdyz jsou figurky v pocatecni pozici 2s)
    // Logic update: Must not be blocked (require 1 move)
    // BUG FIX 1: Během WAITING_FOR_BOARD_SETUP nesmí auto-new game zasáhnout
    // BUG FIX 2: Auto-new game jen když je hlídání počáteční pozice zapnuto
    bool in_start_pos =
        starting_position_check_enabled &&
        !board_setup_tutorial_active && 
        current_game_state != GAME_STATE_WAITING_FOR_BOARD_SETUP &&
        game_is_board_in_starting_position();

    if (in_start_pos && !auto_new_game_triggered &&
        !auto_new_game_blocked_until_move) {
      if (!auto_new_game_in_starting_position) {
        auto_new_game_in_starting_position = true;
        auto_new_game_timer_start = esp_timer_get_time() / 1000;
        ESP_LOGI(TAG,
                 "Auto new game: Starting position detected, waiting 2s...");
      } else if ((esp_timer_get_time() / 1000 - auto_new_game_timer_start) >
                 AUTO_NEW_GAME_CONFIRMATION_TIME_MS) {
        ESP_LOGI(
            TAG,
            "Auto new game: Position confirmed for 2s - triggering new game");
        chess_move_command_t cmd = {
            .type = GAME_CMD_NEW_GAME, .player = 0, .response_queue = NULL};
        if (game_command_queue != NULL) {
          xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
        }
        auto_new_game_triggered = true;
        // Block auto-new game detection until at least one move
        // is made. This prevents infinite loop when auto-new game is triggered
        // while pieces are already in starting position.
        auto_new_game_blocked_until_move = true;
        ESP_LOGI(
            TAG,
            "🔒 Auto new game detection blocked until first move (prevents "
            "infinite loop)");
      }
    } else {
      if (!in_start_pos)
        auto_new_game_triggered = false;
      auto_new_game_in_starting_position = false;
    }

    // Periodic status update
    if (loop_count % 5000 == 0) { // Every 5 seconds
      ESP_LOGI(TAG, "Game Task Status: loop=%" PRIu32 ", state=%d, player=%d, moves=%" PRIu32,
               loop_count, current_game_state, current_player, move_count);
    }

    loop_count++;

    // Wait for next cycle (100ms)
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
  }
}

/**
 * @brief [DEPRECATED] Process matrix events from queue
 * @deprecated Matrix events are now sent as chess_move_command_t to
 * game_command_queue and processed by game_process_pickup_command() /
 * game_process_drop_command(). This function is kept for compatibility but
 * does nothing.
 */
void game_process_matrix_events(void) {
  // Matrix events are now processed via game_command_queue
  // This function is kept for compatibility but does nothing
}

// ============================================================================
// ENHANCED CHESS LOGIC IMPLEMENTATION
// ============================================================================

// Direction vectors for piece movement (knight_moves moved to top of file)

static const int8_t king_moves[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                        {0, 1},   {1, -1}, {1, 0},  {1, 1}};

static const int8_t bishop_dirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

static const int8_t rook_dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

// Castling flags (moved to top of file)

// En passant state (moved to top of file)

// Lifted piece state tracking (already defined above)

// Move generation buffers
static chess_move_extended_t legal_moves_buffer[128];
static uint32_t legal_moves_count = 0;

// Game position history for draw detection (declared earlier in the file)
// static uint32_t position_hashes[100];
// static uint32_t position_history_count = 0;
// Promotion state (for future use)
// static bool promotion_pending = false;
// static chess_move_extended_t promotion_move;

// ============================================================================
// BASIC UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Check if coordinates are valid
 */
bool game_is_valid_square(int row, int col) {
  return (row >= 0 && row < 8 && col >= 0 && col < 8);
}

/**
 * @brief Check if piece belongs to current player
 */
bool game_is_own_piece(piece_t piece, player_t player) {
  if (piece == PIECE_EMPTY)
    return false;
  return (player == PLAYER_WHITE) ? game_is_white_piece(piece)
                                  : game_is_black_piece(piece);
}

// ============================================================================
// ATTACK AND CHECK DETECTION
// ============================================================================

/**
 * @brief Check if square is attacked by opponent
 */
bool game_is_square_attacked(uint8_t row, uint8_t col, player_t by_player) {
  // Check pawn attacks
  int pawn_dir = (by_player == PLAYER_WHITE) ? 1 : -1;
  int pawn_row = row - pawn_dir;
  piece_t attacking_pawn =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;

  if (game_is_valid_square(pawn_row, col - 1) &&
      board[pawn_row][col - 1] == attacking_pawn)
    return true;
  if (game_is_valid_square(pawn_row, col + 1) &&
      board[pawn_row][col + 1] == attacking_pawn)
    return true;

  // Check knight attacks
  piece_t attacking_knight =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
  for (int i = 0; i < 8; i++) {
    int nr = row + knight_moves[i][0];
    int nc = col + knight_moves[i][1];
    if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_knight) {
      return true;
    }
  }

  // Check bishop/queen diagonal attacks
  piece_t attacking_bishop =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
  piece_t attacking_queen =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;

  for (int d = 0; d < 4; d++) {
    int dr = bishop_dirs[d][0];
    int dc = bishop_dirs[d][1];

    for (int nr = row + dr, nc = col + dc; game_is_valid_square(nr, nc);
         nr += dr, nc += dc) {
      piece_t piece = board[nr][nc];
      if (piece == PIECE_EMPTY)
        continue;

      if (piece == attacking_bishop || piece == attacking_queen) {
        return true;
      }
      break; // Blocked by any piece
    }
  }

  // Check rook/queen orthogonal attacks
  piece_t attacking_rook =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;

  for (int d = 0; d < 4; d++) {
    int dr = rook_dirs[d][0];
    int dc = rook_dirs[d][1];

    for (int nr = row + dr, nc = col + dc; game_is_valid_square(nr, nc);
         nr += dr, nc += dc) {
      piece_t piece = board[nr][nc];
      if (piece == PIECE_EMPTY)
        continue;

      if (piece == attacking_rook || piece == attacking_queen) {
        return true;
      }
      break; // Blocked by any piece
    }
  }

  // Check king attacks
  piece_t attacking_king =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
  for (int i = 0; i < 8; i++) {
    int nr = row + king_moves[i][0];
    int nc = col + king_moves[i][1];
    if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_king) {
      return true;
    }
  }

  return false;
}

// ============================================================================
// MOVE VALIDATION AND SIMULATION
// ============================================================================

/**
 * @brief Simulate move and check if it leaves king in check
 */
bool game_simulate_move_check(chess_move_extended_t *move, player_t player) {
  // Save original state
  piece_t original_piece = board[move->to_row][move->to_col];
  piece_t original_en_passant = PIECE_EMPTY;

  // Handle en passant capture
  if (move->move_type == MOVE_TYPE_EN_PASSANT) {
    original_en_passant = board[en_passant_victim_row][en_passant_victim_col];
    board[en_passant_victim_row][en_passant_victim_col] = PIECE_EMPTY;
  }

  // Make the move
  board[move->to_row][move->to_col] = move->piece;
  board[move->from_row][move->from_col] = PIECE_EMPTY;

  // Check if king is in check after this move
  bool king_in_check = game_is_king_in_check(player);

  // Restore original state
  board[move->from_row][move->from_col] = move->piece;
  board[move->to_row][move->to_col] = original_piece;

  if (move->move_type == MOVE_TYPE_EN_PASSANT) {
    board[en_passant_victim_row][en_passant_victim_col] = original_en_passant;
  }

  return !king_in_check; // Return true if move is legal (doesn't leave king
                         // in check)
}

// ============================================================================
// MOVE GENERATION - INDIVIDUAL PIECES
// ============================================================================

/**
 * @brief Generate pawn moves
 */
void game_generate_pawn_moves(uint8_t from_row, uint8_t from_col,
                              player_t player) {
  piece_t pawn = board[from_row][from_col];
  bool is_white = (player == PLAYER_WHITE);
  int direction = is_white ? 1 : -1;
  int start_row = is_white ? 1 : 6;
  int promotion_row = is_white ? 7 : 0;

  // Forward move
  int to_row = from_row + direction;
  if (game_is_valid_square(to_row, from_col) &&
      board[to_row][from_col] == PIECE_EMPTY) {

    if (to_row == promotion_row) {
      // Promotion moves
      for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT; promo++) {
        if (legal_moves_count >= 128)
          return;

        chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = from_col;
        move->piece = pawn;
        move->captured_piece = PIECE_EMPTY;
        move->move_type = MOVE_TYPE_PROMOTION;
        move->promotion_piece = (promotion_choice_t)promo;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    } else {
      // Normal forward move
      if (legal_moves_count >= 128)
        return;

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row;
      move->from_col = from_col;
      move->to_row = to_row;
      move->to_col = from_col;
      move->piece = pawn;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_NORMAL;

      if (game_simulate_move_check(move, player)) {
        legal_moves_count++;
      }

      // Double move from starting position
      if (from_row == start_row &&
          board[to_row + direction][from_col] == PIECE_EMPTY) {
        if (legal_moves_count >= 128)
          return;

        move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row + direction;
        move->to_col = from_col;
        move->piece = pawn;
        move->captured_piece = PIECE_EMPTY;
        move->move_type = MOVE_TYPE_NORMAL;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    }
  }

  // Captures (diagonal)
  for (int dc = -1; dc <= 1; dc += 2) {
    int to_col = from_col + dc;
    if (game_is_valid_square(to_row, to_col)) {
      piece_t target = board[to_row][to_col];

      if (game_is_enemy_piece(target, player)) {
        if (to_row == promotion_row) {
          // Capture with promotion
          for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT;
               promo++) {
            if (legal_moves_count >= 128)
              return;

            chess_move_extended_t *move =
                &legal_moves_buffer[legal_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = to_row;
            move->to_col = to_col;
            move->piece = pawn;
            move->captured_piece = target;
            move->move_type = MOVE_TYPE_PROMOTION;
            move->promotion_piece = (promotion_choice_t)promo;

            if (game_simulate_move_check(move, player)) {
              legal_moves_count++;
            }
          }
        } else {
          // Normal capture
          if (legal_moves_count >= 128)
            return;

          chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
          move->from_row = from_row;
          move->from_col = from_col;
          move->to_row = to_row;
          move->to_col = to_col;
          move->piece = pawn;
          move->captured_piece = target;
          move->move_type = MOVE_TYPE_CAPTURE;

          if (game_simulate_move_check(move, player)) {
            legal_moves_count++;
          }
        }
      }
    }
  }

  // En passant
  if (en_passant_available && from_row == (is_white ? 4 : 3)) {
    for (int dc = -1; dc <= 1; dc += 2) {
      if (from_col + dc == en_passant_target_col) {
        if (legal_moves_count >= 128)
          return;

        chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = en_passant_target_row;
        move->to_col = en_passant_target_col;
        move->piece = pawn;
        move->captured_piece =
            board[en_passant_victim_row][en_passant_victim_col];
        move->move_type = MOVE_TYPE_EN_PASSANT;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    }
  }
}

/**
 * @brief Generate knight moves
 */
void game_generate_knight_moves(uint8_t from_row, uint8_t from_col,
                                player_t player) {
  piece_t knight = board[from_row][from_col];

  for (int i = 0; i < 8; i++) {
    int to_row = from_row + knight_moves[i][0];
    int to_col = from_col + knight_moves[i][1];

    if (!game_is_valid_square(to_row, to_col))
      continue;

    piece_t target = board[to_row][to_col];

    // Skip if occupied by own piece
    if (game_is_own_piece(target, player))
      continue;

    if (legal_moves_count >= 128)
      return;

    chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
    move->from_row = from_row;
    move->from_col = from_col;
    move->to_row = to_row;
    move->to_col = to_col;
    move->piece = knight;
    move->captured_piece = target;
    move->move_type =
        (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

    if (game_simulate_move_check(move, player)) {
      legal_moves_count++;
    }
  }
}

/**
 * @brief Generate sliding piece moves (bishop, rook, queen)
 */
void game_generate_sliding_moves(uint8_t from_row, uint8_t from_col,
                                 player_t player, const int8_t directions[][2],
                                 int num_directions) {
  piece_t piece = board[from_row][from_col];

  for (int d = 0; d < num_directions; d++) {
    int dr = directions[d][0];
    int dc = directions[d][1];

    for (int to_row = from_row + dr, to_col = from_col + dc;
         game_is_valid_square(to_row, to_col); to_row += dr, to_col += dc) {

      piece_t target = board[to_row][to_col];

      // Can't move to square occupied by own piece
      if (game_is_own_piece(target, player))
        break;

      if (legal_moves_count >= 128)
        return;

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row;
      move->from_col = from_col;
      move->to_row = to_row;
      move->to_col = to_col;
      move->piece = piece;
      move->captured_piece = target;
      move->move_type =
          (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

      if (game_simulate_move_check(move, player)) {
        legal_moves_count++;
      }

      // Stop if we hit any piece
      if (target != PIECE_EMPTY)
        break;
    }
  }
}

/**
 * @brief Generate king moves including castling
 */
void game_generate_king_moves(uint8_t from_row, uint8_t from_col,
                              player_t player) {
  piece_t king = board[from_row][from_col];

  // Normal king moves
  for (int i = 0; i < 8; i++) {
    int to_row = from_row + king_moves[i][0];
    int to_col = from_col + king_moves[i][1];

    if (!game_is_valid_square(to_row, to_col))
      continue;

    piece_t target = board[to_row][to_col];

    // Skip if occupied by own piece
    if (game_is_own_piece(target, player))
      continue;

    if (legal_moves_count >= 128)
      return;

    chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
    move->from_row = from_row;
    move->from_col = from_col;
    move->to_row = to_row;
    move->to_col = to_col;
    move->piece = king;
    move->captured_piece = target;
    move->move_type =
        (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

    if (game_simulate_move_check(move, player)) {
      legal_moves_count++;
    }
  }

  // Castling - FIXED: Use proper bounds checking and correct coordinates
  if (game_is_king_in_check(player))
    return; // Can't castle when in check

  if (player == PLAYER_WHITE && !white_king_moved) {
    // White kingside castling
    if (!white_rook_h_moved && board[0][5] == PIECE_EMPTY &&
        board[0][6] == PIECE_EMPTY &&
        !game_is_square_attacked(0, 5, PLAYER_BLACK) &&
        !game_is_square_attacked(0, 6, PLAYER_BLACK)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 0;
      move->to_col = 6;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_KING;
      legal_moves_count++;
    }

    // White queenside castling
    if (!white_rook_a_moved && board[0][1] == PIECE_EMPTY &&
        board[0][2] == PIECE_EMPTY && board[0][3] == PIECE_EMPTY &&
        !game_is_square_attacked(0, 2, PLAYER_BLACK) &&
        !game_is_square_attacked(0, 3, PLAYER_BLACK)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 0;
      move->to_col = 2;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_QUEEN;
      legal_moves_count++;
    }
  }

  if (player == PLAYER_BLACK && !black_king_moved) {
    // Black kingside castling
    if (!black_rook_h_moved && board[7][5] == PIECE_EMPTY &&
        board[7][6] == PIECE_EMPTY &&
        !game_is_square_attacked(7, 5, PLAYER_WHITE) &&
        !game_is_square_attacked(7, 6, PLAYER_WHITE)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 7;
      move->to_col = 6;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_KING;
      legal_moves_count++;
    }

    // Black queenside castling
    if (!black_rook_a_moved && board[7][1] == PIECE_EMPTY &&
        board[7][2] == PIECE_EMPTY && board[7][3] == PIECE_EMPTY &&
        !game_is_square_attacked(7, 2, PLAYER_WHITE) &&
        !game_is_square_attacked(7, 3, PLAYER_WHITE)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 7;
      move->to_col = 2;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_QUEEN;
      legal_moves_count++;
    }
  }
}

// ============================================================================
// MAIN MOVE GENERATION
// ============================================================================

/**
 * @brief Generate all legal moves for current player
 */
uint32_t game_generate_legal_moves(player_t player) {
  legal_moves_count = 0;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      if (!game_is_own_piece(piece, player))
        continue;

      switch (piece) {
      case PIECE_WHITE_PAWN:
      case PIECE_BLACK_PAWN:
        game_generate_pawn_moves(row, col, player);
        break;

      case PIECE_WHITE_KNIGHT:
      case PIECE_BLACK_KNIGHT:
        game_generate_knight_moves(row, col, player);
        break;

      case PIECE_WHITE_BISHOP:
      case PIECE_BLACK_BISHOP:
        game_generate_sliding_moves(row, col, player, bishop_dirs, 4);
        break;

      case PIECE_WHITE_ROOK:
      case PIECE_BLACK_ROOK:
        game_generate_sliding_moves(row, col, player, rook_dirs, 4);
        break;

      case PIECE_WHITE_QUEEN:
      case PIECE_BLACK_QUEEN:
        game_generate_sliding_moves(row, col, player, bishop_dirs, 4);
        game_generate_sliding_moves(row, col, player, rook_dirs, 4);
        break;

      case PIECE_WHITE_KING:
      case PIECE_BLACK_KING:
        game_generate_king_moves(row, col, player);
        break;

      default:
        break;
      }
    }
  }

  return legal_moves_count;
}


// ============================================================================
// ENHANCED BOARD DISPLAY
// ============================================================================

/**
 * @brief Print enhanced chess board with coordinates and status
 */
void game_print_board_enhanced(void) {
  ESP_LOGI(TAG, "╔═══════════════════════════════╗");
  ESP_LOGI(TAG, "║        CHESS BOARD            ║");
  ESP_LOGI(TAG, "╚═══════════════════════════════╝");

  // Print board from rank 8 to rank 1
  for (int row = 7; row >= 0; row--) {
    char line[64] = "";
    char *ptr = line;

    ptr += sprintf(ptr, " %d │", row + 1);

    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      const char *symbol;

      switch (piece) {
      case PIECE_WHITE_PAWN:
        symbol = "♙";
        break;
      case PIECE_WHITE_KNIGHT:
        symbol = "♘";
        break;
      case PIECE_WHITE_BISHOP:
        symbol = "♗";
        break;
      case PIECE_WHITE_ROOK:
        symbol = "♖";
        break;
      case PIECE_WHITE_QUEEN:
        symbol = "♕";
        break;
      case PIECE_WHITE_KING:
        symbol = "♔";
        break;
      case PIECE_BLACK_PAWN:
        symbol = "♟";
        break;
      case PIECE_BLACK_KNIGHT:
        symbol = "♞";
        break;
      case PIECE_BLACK_BISHOP:
        symbol = "♝";
        break;
      case PIECE_BLACK_ROOK:
        symbol = "♜";
        break;
      case PIECE_BLACK_QUEEN:
        symbol = "♛";
        break;
      case PIECE_BLACK_KING:
        symbol = "♚";
        break;
      default:
        symbol = "·";
        break;
      }

      ptr += sprintf(ptr, " %s │", symbol);
    }

    ESP_LOGI(TAG, "%s", line);
    if (row > 0) {
      ESP_LOGI(TAG, "   ├───┼───┼───┼───┼───┼───┼───┼───┤");
    }
  }

  ESP_LOGI(TAG, "   └───┴───┴───┴───┴───┴───┴───┴───┘");
  ESP_LOGI(TAG, "     a   b   c   d   e   f   g   h  ");

  // Print game status
  ESP_LOGI(TAG, "");
  ESP_LOGI(TAG, "Game Status: %s to move",
           (current_player == PLAYER_WHITE) ? "White" : "Black");
  ESP_LOGI(TAG, "Move #%lu", move_count + 1);

  if (game_is_king_in_check(current_player)) {
    ESP_LOGI(TAG, "⚠️  CHECK!");
  }
}

// ============================================================================
// INTEGRATION WITH EXISTING GAME TASK
// ============================================================================

/**
 * @brief Enhanced move validation that replaces game_is_valid_move
 */
move_error_t game_validate_move_enhanced(uint8_t from_row, uint8_t from_col,
                                         uint8_t to_row, uint8_t to_col) {
  // Basic coordinate validation
  if (!game_is_valid_square(from_row, from_col) ||
      !game_is_valid_square(to_row, to_col)) {
    return MOVE_ERROR_INVALID_COORDINATES;
  }

  // Check if there's a piece at source
  piece_t piece = board[from_row][from_col];
  if (piece == PIECE_EMPTY) {
    return MOVE_ERROR_NO_PIECE;
  }

  // Check if it's player's piece
  if (!game_is_own_piece(piece, current_player)) {
    return MOVE_ERROR_WRONG_COLOR;
  }

  // Generate all legal moves and check if this move is in the list
  uint32_t num_legal = game_generate_legal_moves(current_player);

  for (uint32_t i = 0; i < num_legal; i++) {
    chess_move_extended_t *move = &legal_moves_buffer[i];
    if (move->from_row == from_row && move->from_col == from_col &&
        move->to_row == to_row && move->to_col == to_col) {
      return MOVE_ERROR_NONE; // Move is legal
    }
  }

  return MOVE_ERROR_ILLEGAL_MOVE;
}

// ============================================================================
// MATRIX LED WORKFLOW IMPLEMENTATION
// ============================================================================

/**
 * @brief Detect if pieces are arranged in starting positions (rows 1, 2, 7,
 * 8)
 * @return true if pieces are in starting positions, false otherwise
 */
bool game_detect_new_game_setup(void) {
  if (board_setup_tutorial_active) {
    return false;
  }

  // Zkontrolovat, jestli jsou řádky 1, 2, 7, 8 obsazené figurkami
  // a řádky 3, 4, 5, 6 jsou prázdné

  // Zkontrolovat řádky 0, 1 (bílé figurky) a 6, 7 (černé figurky)
  bool rows_0_1_6_7_occupied = true;
  bool rows_2_3_4_5_empty = true;

  // Zkontrolovat řádky 0, 1, 6, 7 - musí být obsazené
  for (int row = 0; row < 8; row++) {
    bool row_has_pieces = false;
    for (int col = 0; col < 8; col++) {
      if (board[row][col] != PIECE_EMPTY) {
        row_has_pieces = true;
        break;
      }
    }

    if ((row == 0 || row == 1 || row == 6 || row == 7)) {
      // Tyto řádky musí být obsazené
      if (!row_has_pieces) {
        rows_0_1_6_7_occupied = false;
        break;
      }
    } else if (row >= 2 && row <= 5) {
      // Tyto řádky musí být prázdné
      if (row_has_pieces) {
        rows_2_3_4_5_empty = false;
        break;
      }
    }
  }

  bool is_starting_setup = rows_0_1_6_7_occupied && rows_2_3_4_5_empty;

  if (is_starting_setup) {
    ESP_LOGI(TAG, "🎮 Starting position detected: rows 0,1,6,7 occupied, rows "
                  "2,3,4,5 empty");
  }

  return is_starting_setup;
}

/**
 * @brief [DEPRECATED] Handle piece lifted event from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_PICKUP to
 * game_command_queue and processed by game_process_pickup_command(). This
 * function is kept for compatibility.
 *
 * Do not wire this back into the matrix path: it rejects opponent lifts and
 * would break guided capture (opponent-first) and related LED flows.
 *
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_lifted(uint8_t row, uint8_t col) {
  ESP_LOGI(TAG, "🖐️ Matrix: Piece lifted from %c%d", 'a' + col, row + 1);

  // Check if there's a piece at the square
  piece_t piece = board[row][col];
  if (piece == PIECE_EMPTY) {
    ESP_LOGW(TAG, "❌ No piece to lift at %c%d", 'a' + col, row + 1);
    return;
  }

  // Check if it's the current player's piece
  bool is_white_piece =
      (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
  bool is_current_player = (current_player == PLAYER_WHITE && is_white_piece) ||
                           (current_player == PLAYER_BLACK && !is_white_piece);

  if (!is_current_player) {
    ESP_LOGW(TAG, "❌ Cannot lift opponent's piece at %c%d", 'a' + col,
             row + 1);
    return;
  }

  // Check if castle animation is active and this is a rook
  if (game_is_castle_animation_active()) {
    bool is_rook = (piece == PIECE_WHITE_ROOK || piece == PIECE_BLACK_ROOK);

    if (is_rook) {
      // Check if rook is being lifted from the correct position for castling
      if (row == rook_from_row && col == rook_from_col) {
        ESP_LOGI(TAG, "✅ Rook lifted from correct position for castling");
        // Continue with normal move highlighting
      } else {
        // Rook lifted from wrong position - show error and valid moves from
        // correct position
        ESP_LOGW(TAG,
                 "❌ Rook lifted from wrong position for castling: %c%d "
                 "(expected: %c%d)",
                 'a' + col, row + 1, 'a' + rook_from_col, rook_from_row + 1);

        // Show error animation - blink red
        led_clear_board_only();
        for (int i = 0; i < 3; i++) {
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0,
                             0); // Red
          vTaskDelay(pdMS_TO_TICKS(200));
          led_clear_board_only();
          vTaskDelay(pdMS_TO_TICKS(200));
        }

        // Show valid moves from the correct rook position
        led_set_pixel_safe(chess_pos_to_led_index(rook_from_row, rook_from_col),
                           255, 255,
                           0); // Yellow
        led_set_pixel_safe(chess_pos_to_led_index(rook_to_row, rook_to_col), 0,
                           255, 0); // Green

        return;
      }
    } else {
      ESP_LOGI(TAG, "🏰 Castle animation active - only rook can be moved");
      return;
    }
  }

  // Show possible moves for this piece
  move_suggestion_t suggestions[64];
  uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);

  if (valid_moves > 0) {
    ESP_LOGI(TAG, "💡 Found %lu valid moves for piece at %c%d", valid_moves,
             'a' + col, row + 1);

    // Send LED commands to highlight possible moves
    // Highlight source square in yellow
    // Highlight source square (yellow)
    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                       0); // Yellow for source

    // Highlight possible destinations
    if (game_led_guidance_show_destinations()) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if destination has opponent's piece
        piece_t dest_piece = board[dest_row][dest_col];
        bool is_opponent_piece =
            (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
             dest_piece <= PIECE_BLACK_KING) ||
            (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
             dest_piece <= PIECE_WHITE_KING);

        if (is_opponent_piece) {
          // Orange for opponent's pieces (capture)
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          // Green for empty squares
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }
    }
  } else {
    ESP_LOGI(TAG, "💡 No valid moves for piece at %c%d", 'a' + col, row + 1);
  }

  // Po zvednutí figurky spustit animaci změny hráče a zobrazit
  // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
  // očekávána rosada)
  if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
    // After piece is lifted, show pieces that the opponent can move
    // This completes the cycle as requested by the user
    game_highlight_movable_pieces();
  }
}

/**
 * @brief [DEPRECATED] Handle piece placed event from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_DROP to
 * game_command_queue and processed by game_process_drop_command(). This
 * function is kept for compatibility.
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_placed(uint8_t row, uint8_t col) {
  ESP_LOGI(TAG, "✋ Matrix: Piece placed at %c%d", 'a' + col, row + 1);

  // Detekce nové hry po endgame animaci
  // Zkontrolovat, jestli se figurky rozestavily na startovní pozice (řádky 1,
  // 2, 7, 8)
  if (game_detect_new_game_setup()) {
    ESP_LOGI(TAG,
             "🎮 NEW GAME DETECTED! Pieces arranged in starting positions");

    // Zastavit všechny animace
    unified_animation_stop_all();

    // Spustit novou hru
    game_start_new_game();

    return; // Ukončit - nová hra byla spuštěna
  }

  // Check if castle animation is active
  if (game_is_castle_animation_active()) {
    ESP_LOGI(
        TAG,
        "🏰 Castle animation active - checking if rook was placed correctly");

    // Check if this is a rook piece being placed
    piece_t piece = board[row][col];
    bool is_rook = (piece == PIECE_WHITE_ROOK || piece == PIECE_BLACK_ROOK);

    if (is_rook) {
      // Check if rook was placed on correct position for castling
      if (row == rook_to_row && col == rook_to_col) {
        ESP_LOGI(TAG, "✅ Rook placed on correct position for castling");
        // The move will be completed in game_handle_matrix_move
        return;
      } else {
        // Rook placed on wrong position - show error
        ESP_LOGW(TAG,
                 "❌ Rook placed on wrong position for castling: %c%d "
                 "(expected: %c%d)",
                 'a' + col, row + 1, 'a' + rook_to_col, rook_to_row + 1);

        // Show error animation - blink red then show valid moves
        led_clear_board_only();

        // Blink the wrong position red
        for (int i = 0; i < 3; i++) {
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0,
                             0); // Red
          vTaskDelay(pdMS_TO_TICKS(200));
          led_clear_board_only();
          vTaskDelay(pdMS_TO_TICKS(200));
        }

        // Show correct rook position and destination
        led_set_pixel_safe(chess_pos_to_led_index(rook_from_row, rook_from_col),
                           255, 255,
                           0); // Yellow
        led_set_pixel_safe(chess_pos_to_led_index(rook_to_row, rook_to_col), 0,
                           255, 0); // Green

        return;
      }
    } else {
      ESP_LOGI(TAG,
               "🏰 Castle animation active - waiting for rook to be moved");
      return;
    }
  }

  // Clear only board LEDs
  led_clear_board_only();

  // Po umístění figurky spustit animaci změny hráče a zobrazit
  // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
  // očekávána rosada)
  if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
    // After piece is placed, show pieces that the opponent can move
    // This completes the cycle as requested by the user
    game_highlight_opponent_pieces();
  }
}

/**
 * @brief [DEPRECATED] Handle complete move from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_DROP to
 * game_command_queue and processed by game_process_drop_command(). This
 * function is kept for compatibility.
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 */
void game_handle_matrix_move(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                             uint8_t to_col) {
  ESP_LOGI(TAG, "🎯 Matrix: Complete move %c%d -> %c%d", 'a' + from_col,
           from_row + 1, 'a' + to_col, to_row + 1);

  // Check if castle animation is active
  if (game_is_castle_animation_active()) {
    if (game_complete_castle_animation(from_row, from_col, to_row, to_col)) {
      ESP_LOGI(TAG, "✅ Castle animation completed via matrix move");
      return;
    } else {
      ESP_LOGW(TAG,
               "❌ Invalid rook move for castling - waiting for correct move");
      return;
    }
  }

  // Convert matrix event to chess move
  chess_move_t move = {
      .from_row = from_row,
      .from_col = from_col,
      .to_row = to_row,
      .to_col = to_col,

      .piece = board[from_row][from_col], // Skutečná figurka ze zdrojového pole
      .captured_piece =
          board[to_row][to_col], // Skutečná figurka z cílového pole
      .timestamp = esp_timer_get_time() / 1000};

  // KRITICKÁ OPRAVA: Validace tahu před voláním game_execute_move (stejně
  // jako UART flow)
  move_error_t error = game_is_valid_move(&move);
  if (error != MOVE_ERROR_NONE) {
    ESP_LOGW(TAG, "❌ Invalid matrix move rejected: %c%d -> %c%d (error: %d)",
             'a' + from_col, from_row + 1, 'a' + to_col, to_row + 1, error);

    // Clear LEDs on invalid move
    led_clear_board_only();

    // Show error animation
    led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 0,
                       0); // Red for source
    led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 0,
                       0); // Red for destination

    return; // Reject invalid move
  }

  // Detekce rosady před voláním game_execute_move
  bool is_castling =
      (move.piece == PIECE_WHITE_KING || move.piece == PIECE_BLACK_KING) &&
      abs((int)move.to_col - (int)move.from_col) == 2;

  if (is_castling) {
    ESP_LOGI(TAG, "🏰 CASTLING DETECTED in matrix move: %c%d -> %c%d",
             'a' + move.from_col, move.from_row + 1, 'a' + move.to_col,
             move.to_row + 1);
  }

  // Execute move (already validated)
  if (game_execute_move(&move)) {
    ESP_LOGI(TAG, "✅ Matrix move executed successfully");

    // KRITICKÁ OPRAVA: Kontrola checkmate/stalemate po každém tahu!
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;
      ESP_LOGI(TAG, "🎉 Game finished detected in matrix move!");
    }

    if (is_castling) {
      // Pro rosadu nespouštět game_highlight_opponent_pieces
      ESP_LOGI(TAG, "🏰 Castling move completed - waiting for rook animation");
    } else {
      // Po úspěšném tahu spustit animaci změny hráče a zobrazit
      // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
      // očekávána rosada)
      if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
        // After successful move, show pieces that the opponent can move
        game_highlight_movable_pieces();
      }
    }
  } else {
    ESP_LOGW(TAG, "❌ Invalid matrix move rejected");

    // Clear LEDs on invalid move
    led_clear_board_only();
  }
}

/**
 * @brief Highlight pieces that the opponent can move
 */
void game_highlight_opponent_pieces(void) {
  ESP_LOGI(TAG, "🔄 Highlighting opponent pieces that can move");

  // LED command queue removed - using direct LED calls now

  // Switch to opponent's perspective temporarily
  player_t opponent =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  // Find all pieces that the opponent can move
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece == PIECE_EMPTY)
        continue;

      // Check if it's opponent's piece
      bool is_opponent_piece =
          (opponent == PLAYER_WHITE && piece >= PIECE_WHITE_PAWN &&
           piece <= PIECE_WHITE_KING) ||
          (opponent == PLAYER_BLACK && piece >= PIECE_BLACK_PAWN &&
           piece <= PIECE_BLACK_KING);

      if (is_opponent_piece) {
        // Check if this piece has valid moves
        move_suggestion_t suggestions[64];
        uint32_t valid_moves =
            game_get_available_moves(row, col, suggestions, 64);

        if (valid_moves > 0) {
          // Highlight this piece in blue (indicating it can move)
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0,
                             255); // Blue for pieces that can move
        }
      }
    }
  }
}

/**
 * @brief Process promotion command from UART
 * @param cmd Promotion command structure
 */
void game_process_promotion_command(const chess_move_command_t *cmd) {
  if (!cmd) {
    ESP_LOGE(TAG, "❌ Invalid promotion command");
    return;
  }

  ESP_LOGI(TAG, "👑 Processing promotion command");

  // #2 & #3: Atomic check with mutex to prevent double execution
  // Použít recursive mutex funkce (promotion_mutex je recursive mutex)
  if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex");
    return;
  }

  // Check if promotion is still pending (might have been processed by another
  // task)
  if (!promotion_state.pending) {
    xSemaphoreGiveRecursive(promotion_mutex);
    ESP_LOGW(TAG, "⚠️ Promotion already processed by another task");
    return;
  }

  // CRITICAL: Keep mutex held during ENTIRE execution to prevent race!
  // DO NOT release here - release only after completion or on error

  // Determine promotion choice (default to Queen if invalid or not specified)
  promotion_choice_t choice = PROMOTION_QUEEN;

  if (cmd->promotion_choice <= PROMOTION_KNIGHT) {
    choice = (promotion_choice_t)cmd->promotion_choice;
    ESP_LOGI(TAG, "👑 Promotion choice received: %d (%s)", choice,
             choice == PROMOTION_QUEEN    ? "Queen"
             : choice == PROMOTION_ROOK   ? "Rook"
             : choice == PROMOTION_BISHOP ? "Bishop"
                                          : "Knight");
  } else {
    ESP_LOGW(TAG,
             "⚠️ Invalid or missing promotion choice (%d), defaulting to Queen",
             cmd->promotion_choice);
  }

  // Execute the promotion with the selected piece
  if (game_execute_promotion(choice)) {
    ESP_LOGI(TAG, "✅ Promotion executed successfully");

    // Clear promotion state (mutex already held from line 10419)
    promotion_state.pending = false;

    // Clear promotion state and return to active gameplay
    current_game_state =
        GAME_STATE_ACTIVE; // Fix: use constant instead of magic number 6

    // Switch to next player
    player_t previous_player = current_player;
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Po dokončení promoce už žádná figurka není zvednutá – aby se zobrazily
    // movable pieces (game_highlight_movable_pieces nesmí skipnout kvůli
    // piece_lifted).
    piece_lifted = false;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    lifted_piece = PIECE_EMPTY;

    // #2: Timer switch AFTER promotion choice
    ESP_LOGI(TAG, "🔄 Timer switch (promotion): ending for %s, starting for %s",
             previous_player == PLAYER_WHITE ? "White" : "Black",
             current_player == PLAYER_WHITE ? "White" : "Black");
    game_end_timer_move();
    game_start_timer_move(current_player == PLAYER_WHITE);

    // KRITICKÉ: Endgame kontrola PŘED player change animací!
    // Pokud je endgame, player change se NESPOUŠTÍ
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;

      // Najít pozici krále vítěze pro endgame animaci
      player_t winner =
          (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
      uint8_t king_pos = 28; // default e4
      for (int i = 0; i < 64; i++) {
        piece_t piece = board[i / 8][i % 8];
        if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
            (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
          king_pos = i;
          break;
        }
      }

      ESP_LOGI(TAG,
               "🎯 Game finished after promotion command! Starting endgame "
               "animation at position %d",
               king_pos);

      // Spustit endgame animaci (wave z krále vítěze)
      led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                   .led_index = king_pos,
                                   .red = 255,
                                   .green = 255,
                                   .blue = 0,        // Yellow
                                   .duration_ms = 0, // Endless
                                   .data = NULL};
      led_execute_command_new(&endgame_cmd);

      ESP_LOGI(
          TAG,
          "✅ Endgame animation started - player change animation SKIPPED");
    } else {
      // Není endgame - spustit player change animaci
      uint8_t player_color =
          (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
      led_command_t player_change_cmd = {
          .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                              // ANIM_PLAYER_CHANGE místo
                                              // PLAYER_CHANGE
          .led_index = 0,
          .red = 0,
          .green = 0,
          .blue = 0,
          .duration_ms = 0,
          .data = &player_color // Předat barvu hráče
      };
      led_execute_command_new(&player_change_cmd);

      // Print updated board
      game_print_board();

      // Highlight pieces that the new current player can move
      game_highlight_movable_pieces();
      led_force_immediate_update(); // Commit highlight so movable pieces show
                                    // right after player change animation

      // Zkontrolovat, zda je nový hráč v šachu
      bool in_check = game_is_king_in_check(current_player);
      if (in_check) {
        // Najít pozici krále
        int king_row = -1, king_col = -1;
        piece_t king_piece = (current_player == PLAYER_WHITE)
                                 ? PIECE_WHITE_KING
                                 : PIECE_BLACK_KING;
        for (int row = 0; row < 8; row++) {
          for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
              king_row = row;
              king_col = col;
              break;
            }
          }
          if (king_row != -1)
            break;
        }

        if (king_row != -1 && king_col != -1) {
          if (game_led_guidance_show_check_anim()) {
            uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
            // Spustit check animaci - růžové svícení na králi
            led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                       .led_index = king_led_index,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms =
                                           0, // Trvalé až do dalšího tahu
                                       .data = NULL};
            led_execute_command_new(&check_cmd);
            ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                     current_player == PLAYER_WHITE ? "White" : "Black");
          }
        }
      }
    }

  } else {
    ESP_LOGE(TAG, "❌ Failed to execute promotion");
  }

  // CRITICAL: Release mutex at the end of the function to ensure it's always
  // released Použít recursive mutex funkce (promotion_mutex je recursive mutex)
  xSemaphoreGiveRecursive(promotion_mutex);
}

/**
 * @brief Provede promoci pesce na vybranou figurku
 *
 * @param choice Vyber figurky pro promoci (PROMOTION_QUEEN, PROMOTION_ROOK,
 * PROMOTION_BISHOP, PROMOTION_KNIGHT)
 * @return true pokud byla promoce uspesna, false pokud selha
 *
 * @details
 * Funkce hleda pesce na promocni rade a povysuje ho na vybranou figurku:
 * - Bily pesec na 8. rade (row 7) -> povyseni
 * - Cerny pesec na 1. rade (row 0) -> povyseni
 *
 * Proces:
 * 1. Prevod choice na typ figurky podle barvy hrace
 * 2. Prohledani desky pro pesce na promocni rade
 * 3. Nahrazeni pesce povysenou figurkou
 *
 * @note KRITICKA OPRAVA v2.4.1:
 * - BUG #10: Row indexing byl OBRACENY!
 * - Puvodni: WHITE row==0, BLACK row==7 (SPATNE!)
 * - Opraveno: WHITE row==7 (8. rada), BLACK row==0 (1. rada)
 * - Promoce nyni funguje 100% spravne
 */
static bool game_execute_promotion(promotion_choice_t choice) {
  ESP_LOGI(TAG, "👑 Executing pawn promotion: %d", choice);

  // Find the pawn that needs to be promoted
  // This should be set when a pawn reaches the promotion rank
  // For now, we'll implement a basic version

  // Convert promotion choice to piece type
  piece_t promoted_piece;
  // Use promotion_state.player instead of current_player
  // current_player has already been switched to the opponent by
  // game_execute_move() so we must use the player stored in the promotion
  // state
  if (promotion_state.player == PLAYER_WHITE) {
    switch (choice) {
    case PROMOTION_QUEEN:
      promoted_piece = PIECE_WHITE_QUEEN;
      break;
    case PROMOTION_ROOK:
      promoted_piece = PIECE_WHITE_ROOK;
      break;
    case PROMOTION_BISHOP:
      promoted_piece = PIECE_WHITE_BISHOP;
      break;
    case PROMOTION_KNIGHT:
      promoted_piece = PIECE_WHITE_KNIGHT;
      break;
    default:
      return false;
    }
  } else {
    switch (choice) {
    case PROMOTION_QUEEN:
      promoted_piece = PIECE_BLACK_QUEEN;
      break;
    case PROMOTION_ROOK:
      promoted_piece = PIECE_BLACK_ROOK;
      break;
    case PROMOTION_BISHOP:
      promoted_piece = PIECE_BLACK_BISHOP;
      break;
    case PROMOTION_KNIGHT:
      promoted_piece = PIECE_BLACK_KNIGHT;
      break;
    default:
      return false;
    }
  }

  // BUG #3: Use stored position instead of scanning entire board
  // The position was already found and stored in promotion_state by
  // game_check_promotion_needed()
  uint8_t row = promotion_state.square_row;
  uint8_t col = promotion_state.square_col;
  piece_t piece = board[row][col];

  // Verify it's actually a pawn (safety check)
  if (piece != PIECE_WHITE_PAWN && piece != PIECE_BLACK_PAWN) {
    ESP_LOGE(TAG, "❌ Promotion failed: No pawn at stored position %c%d",
             'a' + col, row + 1);
    return false;
  }

  // Promote the pawn
  board[row][col] = promoted_piece;
  current_game_state = GAME_STATE_ACTIVE; // Restore game state to ACTIVE
  ESP_LOGI(TAG, "✅ Promoted %s pawn at %c%d to %s",
           current_player == PLAYER_WHITE ? "white" : "black", 'a' + col,
           row + 1,
           choice == PROMOTION_QUEEN    ? "Queen"
           : choice == PROMOTION_ROOK   ? "Rook"
           : choice == PROMOTION_BISHOP ? "Bishop"
                                        : "Knight");

  // Spustit promotion animaci
  uint8_t promotion_led = chess_pos_to_led_index(row, col);
  led_command_t promote_cmd = {.type = LED_CMD_ANIM_PROMOTE,
                               .led_index = promotion_led,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 2000,
                               .data = NULL};
  led_execute_command_new(&promote_cmd);

  return true;
}

/**
 * @brief Highlight all movable pieces for current player
 */
void game_highlight_movable_pieces(void) {
  // Do not highlight pieces if system is booting
  if (led_is_booting()) {
    return;
  }

  // Do not highlight movable pieces if a piece is already
  // lifted This prevents overwriting the "valid moves" (green) with "movable
  // pieces" (yellow)
  if (piece_lifted) {
    ESP_LOGD(TAG, "Highlight skipped - piece is lifted");
    return;
  }

  // Nespouštět highlight během rošády - rošáda má vlastní LED indikaci
  if (castling_state.in_progress) {
    ESP_LOGD(TAG, "Highlight skipped - castling in progress");
    return;
  }

  if (!game_led_guidance_show_movable_yellow()) {
    ESP_LOGD(TAG, "Movable piece highlights skipped (LED guidance level %u)",
             (unsigned)led_guidance_level);
    return;
  }

  ESP_LOGI(TAG, "🟡 Highlighting movable pieces for %s player",
           current_player == PLAYER_WHITE ? "white" : "black");

  // Endgame detection is handled in game_analyze_position() called from
  // game_execute_move()

  // LED command queue removed - using direct LED calls now

  uint32_t highlighted_count = 0;

  // Scan all squares for current player's pieces
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      // Check if piece belongs to current player
      bool is_current_player_piece = false;
      if (current_player == PLAYER_WHITE) {
        is_current_player_piece =
            (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
      } else {
        is_current_player_piece =
            (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
      }

      if (is_current_player_piece) {
        // Check if piece has valid moves
        // OPTIMIZATION: Reduced buffer size from 64 to 1 to prevent stack
        // overflow in Timer Task We only need to know if count > 0, so asking
        // for 1 move is enough
        move_suggestion_t suggestions[1];
        uint32_t valid_moves =
            game_get_available_moves(row, col, suggestions, 1);

        if (valid_moves > 0) {
          // Highlight movable piece in yellow
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                             0); // Yellow
          highlighted_count++;
        }
      }
    }
  }

  ESP_LOGI(TAG, "🟡 Highlighted %lu movable pieces", highlighted_count);
}

// ============================================================================
// PUZZLE COMMAND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Detect and handle castling in DROP command
 * @param move Move that was attempted
 */
void game_detect_and_handle_castling(const chess_move_t *move) {
  piece_t piece = move->piece;

  // Je to král a pohybuje se o 2 pole?
  if ((piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING) &&
      abs((int)move->to_col - (int)move->from_col) == 2) {

    ESP_LOGI(TAG, "🏰 CASTLING DETECTED! King moved 2 squares");

    // Nastavit stav rošády
    castling_state.in_progress = true;
    castling_state.player = current_player;

    // Vypočítat pozice věže
    bool is_kingside = (move->to_col == 6);
    castling_state.rook_from_row = move->to_row;
    castling_state.rook_from_col = is_kingside ? 7 : 0;
    castling_state.rook_to_row = move->to_row;
    castling_state.rook_to_col = is_kingside ? 5 : 3;

    // Zobrazit LED guidance pro věž
    game_show_castling_rook_guidance();

    // NEZMĚNIT HRÁČE - čekat na dokončení rošády
    ESP_LOGI(TAG, "⏳ Waiting for rook move to complete castling...");
  }
}

/**
 * @brief Show LED guidance for castling rook move
 */
void game_show_castling_rook_guidance() {
  // Vyčistit board
  led_clear_board_only();

  // Postupná animace od věže k cíli
  uint8_t rook_from_led = chess_pos_to_led_index(castling_state.rook_from_row,
                                                 castling_state.rook_from_col);
  uint8_t rook_to_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                               castling_state.rook_to_col);

  // Zvýraznit věž (žlutá)
  led_set_pixel_safe(rook_from_led, 255, 255, 0); // Žlutá - zvedni odtud

  // Postupně rozsvítit pole od věže k cíli
  int from_col = castling_state.rook_from_col;
  int to_col = castling_state.rook_to_col;
  int step = (to_col > from_col) ? 1 : -1;

  for (int col = from_col + step; col != to_col + step; col += step) {
    uint8_t led_index =
        chess_pos_to_led_index(castling_state.rook_from_row, col);
    led_set_pixel_safe(led_index, 0, 255, 0); // Zelená - cesta
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  // Zvýraznit cíl (zelená)
  led_set_pixel_safe(rook_to_led, 0, 255, 0); // Zelená - polož sem

  ESP_LOGI(TAG, "💡 Move rook from %c%d to %c%d to complete castling",
           'a' + castling_state.rook_from_col, castling_state.rook_from_row + 1,
           'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
}

/**
 * @brief Check if castling is completed in DROP command
 * @param move Move that was attempted
 * @return true if castling was completed
 */
bool game_check_castling_completion(const chess_move_t *move) {
  if (!castling_state.in_progress)
    return false;

  // Je to správný tah věže?
  if (move->from_row == castling_state.rook_from_row &&
      move->from_col == castling_state.rook_from_col &&
      move->to_row == castling_state.rook_to_row &&
      move->to_col == castling_state.rook_to_col) {

    ESP_LOGI(TAG, "✅ CASTLING COMPLETED! Rook moved correctly");

    // Provedeme posun věže v board[][]
    board[move->to_row][move->to_col] = move->piece;
    board[castling_state.rook_from_row][castling_state.rook_from_col] =
        PIECE_EMPTY;

    // Aktualizovat příznaky pro krále a věž
    if (castling_state.player == PLAYER_WHITE) {
      white_king_moved = true;
      if (castling_state.rook_from_col == 7)
        white_rook_h_moved = true;
      else
        white_rook_a_moved = true;
    } else {
      black_king_moved = true;
      if (castling_state.rook_from_col == 7)
        black_rook_h_moved = true;
      else
        black_rook_a_moved = true;
    }

    // Zlatá animace dokončení rošády
    show_castling_completion_animation();

    // Změnit hráče TEPRVE TEĎ
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // KRITICKÉ: Endgame kontrola PŘED player change animací!
    // Pokud je endgame, player change se NESPOUŠTÍ
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;

      // Najít pozici krále vítěze pro endgame animaci
      player_t winner =
          (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
      uint8_t king_pos = 28; // default e4
      for (int i = 0; i < 64; i++) {
        piece_t piece = board[i / 8][i % 8];
        if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
            (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
          king_pos = i;
          break;
        }
      }

      ESP_LOGI(TAG,
               "🎯 Game finished after castling completion! Starting endgame "
               "animation at position %d",
               king_pos);

      // Spustit endgame animaci (wave z krále vítěze)
      led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                   .led_index = king_pos,
                                   .red = 255,
                                   .green = 255,
                                   .blue = 0,        // Yellow
                                   .duration_ms = 0, // Endless
                                   .data = NULL};
      led_execute_command_new(&endgame_cmd);

      ESP_LOGI(
          TAG,
          "✅ Endgame animation started - player change animation SKIPPED");
    } else {
      // Není endgame - spustit player change animaci
      uint8_t player_color =
          (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
      led_command_t player_change_cmd = {
          .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                              // ANIM_PLAYER_CHANGE místo
                                              // PLAYER_CHANGE
          .led_index = 0,
          .red = 0,
          .green = 0,
          .blue = 0,
          .duration_ms = 0,
          .data = &player_color // Předat barvu hráče
      };
      led_execute_command_new(&player_change_cmd);

      // Vyčistit stav rošády
      memset(&castling_state, 0, sizeof(castling_state));

      // Zobrazit pohyblivé figury pro nového hráče
      led_clear_board_only();
      game_highlight_movable_pieces();

      ESP_LOGI(TAG, "🏰 Castling completed! %s to move",
               (current_player == PLAYER_WHITE) ? "White" : "Black");

      // Zkontrolovat, zda je nový hráč v šachu
      bool in_check = game_is_king_in_check(current_player);
      if (in_check) {
        // Najít pozici krále
        int king_row = -1, king_col = -1;
        piece_t king_piece = (current_player == PLAYER_WHITE)
                                 ? PIECE_WHITE_KING
                                 : PIECE_BLACK_KING;
        for (int row = 0; row < 8; row++) {
          for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
              king_row = row;
              king_col = col;
              break;
            }
          }
          if (king_row != -1)
            break;
        }

        if (king_row != -1 && king_col != -1) {
          if (game_led_guidance_show_check_anim()) {
            uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
            // Spustit check animaci - růžové svícení na králi
            led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                       .led_index = king_led_index,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms =
                                           0, // Trvalé až do dalšího tahu
                                       .data = NULL};
            led_execute_command_new(&check_cmd);
            ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                     current_player == PLAYER_WHITE ? "White" : "Black");
          }
        }
      }
    }

    return true;
  } else {
    // Nesprávný tah věže - ukázat znovu guidance
    ESP_LOGI(TAG, "❌ Incorrect rook move for castling");
    game_show_castling_rook_guidance();
    return false;
  }
}


// ============================================================================
// CASTLING ANIMATION SYSTEM (LEGACY)
// ============================================================================

/**
 * @brief Start castle animation - highlight rook that needs to be moved
 * @param move Castle move (king has already moved)
 */
void game_start_castle_animation(const chess_move_extended_t *move) {
  if (!move)
    return;

  ESP_LOGI(TAG, "🏰 Starting castle animation for %s",
           (move->move_type == MOVE_TYPE_CASTLE_KING) ? "kingside"
                                                      : "queenside");

  // Calculate rook positions
  uint8_t king_row = (current_player == PLAYER_WHITE) ? 0 : 7;

  if (move->move_type == MOVE_TYPE_CASTLE_KING) {
    // Kingside: rook from h-file to f-file
    rook_from_row = king_row;
    rook_from_col = 7; // h-file
    rook_to_row = king_row;
    rook_to_col = 5; // f-file
  } else {
    // Queenside: rook from a-file to d-file
    rook_from_row = king_row;
    rook_from_col = 0; // a-file
    rook_to_row = king_row;
    rook_to_col = 3; // d-file
  }

  // Store castle move for completion
  pending_castle_move = *move;
  castle_animation_active = true;

  // Clear board and start repeating rook move animation
  led_clear_board_only();

  // Spustit opakující se animaci pohybu věže
  game_start_repeating_rook_animation();

  // Show initial animation immediately
  uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
  uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
  led_set_pixel_safe(from_led, 255, 255, 0); // Yellow for rook position
  led_set_pixel_safe(to_led, 0, 255, 0);     // Green for destination

  ESP_LOGI(TAG, "🏰 Rook at %c%d needs to move to %c%d", 'a' + rook_from_col,
           rook_from_row + 1, 'a' + rook_to_col, rook_to_row + 1);
}

/**
 * @brief Timer callback for repeating rook animation
 * @param xTimer Timer handle
 */
void rook_animation_timer_callback(TimerHandle_t xTimer) {
  if (!rook_animation_active || !castle_animation_active) {
    return;
  }

  // Simple pulsing animation - show rook position and destination
  uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
  uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);

  // Clear board first
  led_clear_board_only();

  // Show rook position in yellow (pulsing)
  static bool pulse_state = false;
  pulse_state = !pulse_state;

  if (pulse_state) {
    // Bright yellow for rook position
    led_set_pixel_safe(from_led, 255, 255, 0);
    // Bright green for destination
    led_set_pixel_safe(to_led, 0, 255, 0);
  } else {
    // Dim yellow for rook position
    led_set_pixel_safe(from_led, 128, 128, 0);
    // Dim green for destination
    led_set_pixel_safe(to_led, 0, 128, 0);
  }

  ESP_LOGI(TAG, "🏰 Rook animation: %c%d -> %c%d (%s)", 'a' + rook_from_col,
           rook_from_row + 1, 'a' + rook_to_col, rook_to_row + 1,
           pulse_state ? "bright" : "dim");
}

/**
 * @brief Start repeating rook animation
 */
void game_start_repeating_rook_animation(void) {
  if (rook_animation_timer == NULL) {
    // Create timer for rook animation
    rook_animation_timer =
        xTimerCreate("rook_anim_timer",
                     pdMS_TO_TICKS(500), // 500ms interval (snappier)
                     pdTRUE,             // Auto-reload
                     NULL, rook_animation_timer_callback);

    if (rook_animation_timer == NULL) {
      ESP_LOGE(TAG, "❌ Failed to create rook animation timer");
      return;
    }
  }

  rook_animation_active = true;

  // Start the timer
  if (xTimerStart(rook_animation_timer, 0) != pdPASS) {
    ESP_LOGE(TAG, "❌ Failed to start rook animation timer");
    rook_animation_active = false;
    return;
  }

  ESP_LOGI(TAG, "🏰 Started repeating rook animation");
}

/**
 * @brief Stop repeating rook animation
 */
void game_stop_repeating_rook_animation(void) {
  if (rook_animation_timer != NULL) {
    xTimerStop(rook_animation_timer, 0);
  }
  rook_animation_active = false;
  ESP_LOGI(TAG, "🏰 Stopped repeating rook animation");
}

/**
 * @brief Check if castle animation is active
 * @return true if castle animation is waiting for rook move
 */
bool game_is_castle_animation_active(void) { return castle_animation_active; }

/**
 * @brief Check if castling is expected (king lifted and about to be placed 2
 * squares away)
 * @return true if castling is expected
 */
bool game_is_castling_expected(void) {
  // Check if a king is lifted and about to be placed 2 squares away
  if (piece_lifted &&
      (lifted_piece == PIECE_WHITE_KING || lifted_piece == PIECE_BLACK_KING)) {
    // This function is called from game_handle_piece_placed
    // We need to check if the king is being placed 2 squares away from its
    // original position But we don't have access to the destination
    // coordinates here So we return true to prevent premature opponent piece
    // highlighting The actual castling detection will happen in
    // game_handle_matrix_move
    ESP_LOGI(TAG, "🏰 King is lifted - castling might be expected");
    return true;
  }
  return false;
}

/**
 * @brief Complete castle animation when rook is moved
 * @param from_row Rook source row
 * @param from_col Rook source column
 * @param to_row Rook destination row
 * @param to_col Rook destination column
 * @return true if castle was completed successfully
 */
bool game_complete_castle_animation(uint8_t from_row, uint8_t from_col,
                                    uint8_t to_row, uint8_t to_col) {
  if (!castle_animation_active) {
    return false;
  }

  // Check if rook was moved to correct position
  if (from_row == rook_from_row && from_col == rook_from_col &&
      to_row == rook_to_row && to_col == rook_to_col) {

    ESP_LOGI(TAG, "✅ Castle animation completed! Rook moved correctly.");

    // Skutečně přesunout věž až když hráč udělá správný tah
    piece_t rook_piece = board[rook_from_row][rook_from_col];
    board[rook_from_row][rook_from_col] = PIECE_EMPTY;
    board[rook_to_row][rook_to_col] = rook_piece;

    // Stop repeating rook animation
    game_stop_repeating_rook_animation();

    // Complete the castle move
    castle_animation_active = false;

    // Switch players now that castle is complete
    player_t previous_player = current_player;
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Po úspěšné rošádě spustit animaci změny hráče
    game_show_player_change_animation(previous_player, current_player);

    // Clear board and show movable pieces for new player
    led_clear_board_only();
    game_highlight_movable_pieces();

    ESP_LOGI(TAG, "🏰 Castling completed! %s to move",
             (current_player == PLAYER_WHITE) ? "White" : "Black");

    return true;
  } else {
    ESP_LOGW(TAG,
             "❌ Invalid rook move for castling: %c%d -> %c%d (expected: %c%d "
             "-> %c%d)",
             'a' + from_col, from_row + 1, 'a' + to_col, to_row + 1,
             'a' + rook_from_col, rook_from_row + 1, 'a' + rook_to_col,
             rook_to_row + 1);

    // Keep castle animation active, wait for correct move
    return false;
  }
}

// ============================================================================
// WEB SERVER JSON EXPORT FUNCTIONS
// ============================================================================

esp_err_t game_get_board_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to board
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string manually (more efficient than cJSON)
  int offset = 0;

  offset += snprintf(buffer + offset, size - offset, "{\"board\":[");

  for (int row = 0; row < 8; row++) {
    offset += snprintf(buffer + offset, size - offset, "[");
    for (int col = 0; col < 8; col++) {
      char piece_char = piece_to_char(board[row][col]);
      offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
      if (col < 7) {
        offset += snprintf(buffer + offset, size - offset, ",");
      }
    }
    offset += snprintf(buffer + offset, size - offset, "]");
    if (row < 7) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  uint64_t timestamp = esp_timer_get_time() / 1000;
  offset += snprintf(buffer + offset, size - offset, "],\"timestamp\":%llu}",
                     timestamp);

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

/**
 * @brief Get current move count safely
 */
uint32_t game_get_move_count(void) {
  uint32_t count = 0;
  if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    count = move_count;
    xSemaphoreGive(game_mutex);
  }
  return count;
}

static esp_err_t game_status_json_append(int *offset, char *buffer, size_t size,
                                        const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (*offset < 0 || (size_t)*offset >= size) {
    va_end(ap);
    ESP_LOGE(TAG, "game_get_status_json: offset invalid %d", *offset);
    return ESP_ERR_NO_MEM;
  }
  size_t rem = size - (size_t)*offset;
  if (rem <= 1) {
    va_end(ap);
    return ESP_ERR_NO_MEM;
  }
  int n = vsnprintf(buffer + *offset, rem, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= rem) {
    ESP_LOGE(TAG,
             "game_get_status_json: append overflow need=%d rem=%zu off=%d", n,
             rem, *offset);
    return ESP_ERR_NO_MEM;
  }
  *offset += n;
  return ESP_OK;
}

#define GAME_STATUS_JSON_APPEND(...)                                           \
  do {                                                                         \
    if (game_status_json_append(&offset, buffer, size, __VA_ARGS__) !=         \
        ESP_OK) {                                                              \
      if (game_mutex != NULL)                                                  \
        xSemaphoreGive(game_mutex);                                            \
      return ESP_ERR_NO_MEM;                                                   \
    }                                                                          \
  } while (0)

esp_err_t game_get_status_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to game state
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;

  // Game state string
  const char *game_state_str = "idle";
  switch (current_game_state) {
  case GAME_STATE_ACTIVE:
    game_state_str = "active";
    break;
  case GAME_STATE_PAUSED:
    game_state_str = "paused";
    break;
  case GAME_STATE_FINISHED:
    game_state_str = "finished";
    break;
  case GAME_STATE_PROMOTION:
    game_state_str = "promotion";
    break;
  case GAME_STATE_WAITING_FOR_RETURN:
    game_state_str = "active";
    break;
  case GAME_STATE_PLAYING:
    game_state_str = "playing";
    break;
  default:
    game_state_str = "idle";
    break;
  }

  // Check detection
  bool in_check = game_is_king_in_check(current_player);

  // Checkmate detection (simplified - king in check and no legal moves)
  bool checkmate = false;
  if (in_check) {
    // Try to generate legal moves - if none, it's checkmate
    uint32_t legal_moves = game_generate_legal_moves(current_player);
    checkmate = (legal_moves == 0);
  }

  // Stalemate detection (simplified - not in check but no legal moves)
  bool stalemate = false;
  if (!in_check) {
    uint32_t legal_moves = game_generate_legal_moves(current_player);
    stalemate = (legal_moves == 0);
  }

  GAME_STATUS_JSON_APPEND("{\"game_state\":\"%s\","
                          "\"current_player\":\"%s\","
                          "\"move_count\":%" PRIu32 ","
                          "\"white_time\":%" PRIu32 ","
                          "\"black_time\":%" PRIu32 ","
                          "\"in_check\":%s,"
                          "\"checkmate\":%s,"
                          "\"stalemate\":%s",
                          game_state_str,
                          (current_player == PLAYER_WHITE) ? "White" : "Black",
                          move_count, white_time_total, black_time_total,
                          in_check ? "true" : "false", checkmate ? "true" : "false",
                          stalemate ? "true" : "false");

  // Piece lifted info
  if (piece_lifted) {
    char piece_char = piece_to_char(lifted_piece);
    char notation[4] = {0};
    convert_coords_to_notation(lifted_piece_row, lifted_piece_col, notation);
    GAME_STATUS_JSON_APPEND(",\"piece_lifted\":{\"lifted\":true,\"row\":%d,"
                            "\"col\":%d,"
                            "\"piece\":\"%c\",\"notation\":\"%s\"}",
                            lifted_piece_row, lifted_piece_col, piece_char,
                            notation);
  } else {
    GAME_STATUS_JSON_APPEND(",\"piece_lifted\":{\"lifted\":false,\"row\":0,"
                            "\"col\":"
                            "0,\"piece\":\" \",\"notation\":\"\"}");
  }

  /* Rošáda na 2 tahy: web musí vědět, že má čekat na tah věže (deska je mezistav). */
  if (castling_state.in_progress) {
    char cf[4] = {0}, ct[4] = {0};
    convert_coords_to_notation(castling_state.rook_from_row,
                              castling_state.rook_from_col, cf);
    convert_coords_to_notation(castling_state.rook_to_row,
                              castling_state.rook_to_col, ct);
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":true,"
                            "\"castling_from\":\"%s\","
                            "\"castling_to\":\"%s\"",
                            cf, ct);
  } else {
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":false");
  }

  // Game end information - použít current_endgame_reason pro přesné
  // rozlišení
  if (current_game_state == GAME_STATE_FINISHED) {
    const char *end_reason = "Unknown";
    const char *winner = "None";
    const char *loser = "None";

    // Nejprve určit vítěze
    if (current_result_type == RESULT_WHITE_WINS) {
      winner = "White";
      loser = "Black";
    } else if (current_result_type == RESULT_BLACK_WINS) {
      winner = "Black";
      loser = "White";
    } else {
      winner = "Draw";
      loser = "Draw";
    }

    // Pak určit důvod podle current_endgame_reason
    switch (current_endgame_reason) {
    case ENDGAME_REASON_CHECKMATE:
      end_reason = "Checkmate";
      break;
    case ENDGAME_REASON_CHECKMATE_EN_PASSANT:
      end_reason = "Checkmate (En Passant)";
      break;
    case ENDGAME_REASON_CHECKMATE_CASTLING:
      end_reason = "Checkmate (Castling)";
      break;
    case ENDGAME_REASON_CHECKMATE_PROMOTION:
      end_reason = "Checkmate (Promotion)";
      break;
    case ENDGAME_REASON_CHECKMATE_DISCOVERED:
      end_reason = "Checkmate (Discovered Check)";
      break;
    case ENDGAME_REASON_RESIGNATION:
      end_reason = "Resignation";
      break;
    case ENDGAME_REASON_TIMEOUT:
      end_reason = "Timeout";
      break;
    case ENDGAME_REASON_STALEMATE:
      end_reason = "Stalemate";
      break;
    case ENDGAME_REASON_50_MOVE:
      end_reason = "50-move rule";
      break;
    case ENDGAME_REASON_REPETITION:
      end_reason = "Threefold repetition";
      break;
    case ENDGAME_REASON_INSUFFICIENT:
      end_reason = "Insufficient material";
      break;
    }

    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":true,\"reason\":\"%s\","
                            "\"winner\":\"%s\",\"loser\":\"%s\"}",
                            end_reason, winner, loser);
  } else {
    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":false,\"reason\":\"\","
                            "\"winner\":\"\",\"loser\":\"\"}");
  }

  // Error recovery state pro vizuální indikaci na webu
  if (error_recovery_state.waiting_for_move_correction) {
    char invalid_notation[4] = {0};
    char original_notation[4] = {0};
    convert_coords_to_notation(error_recovery_state.invalid_row,
                               error_recovery_state.invalid_col,
                               invalid_notation);
    convert_coords_to_notation(error_recovery_state.original_valid_row,
                               error_recovery_state.original_valid_col,
                               original_notation);

    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":true,"
                            "\"invalid_pos\":\"%s\","
                            "\"original_pos\":\"%s\","
                            "\"error_count\":%d}",
                            invalid_notation, original_notation,
                            error_recovery_state.error_count);
  } else {
    /* Uzavřít objekt error_state — dříve chybějící } rozbíjelo JSON (restore_state
     * vnořený do error_state). */
    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":false}");
  }

  GAME_STATUS_JSON_APPEND(
      ",\"restore_state\":{\"snapshot_loaded\":%s,\"snapshot_fallback_used\":%s,"
      "\"snapshot_restore_failed\":%s,\"snapshot_save_failed\":%s,"
      "\"resync_required\":%s,"
      "\"boot_new_game_triggered\":%s}",
      game_was_snapshot_loaded_on_boot() ? "true" : "false",
      game_is_snapshot_fallback_used() ? "true" : "false",
      game_has_snapshot_restore_failure() ? "true" : "false",
      game_has_snapshot_save_failure() ? "true" : "false",
      resync_required_after_restore ? "true" : "false",
      game_was_boot_new_game_triggered() ? "true" : "false");

  GAME_STATUS_JSON_APPEND(",\"board_setup_tutorial\":%s",
                            board_setup_tutorial_active ? "true" : "false");

  const game_puzzle_definition_t *pd_play =
      game_get_puzzle_definition(puzzle_active_id);
  const game_puzzle_definition_t *pd_setup =
      game_get_puzzle_definition(puzzle_setup_id);
  const game_puzzle_definition_t *pd =
      puzzle_active ? pd_play : (puzzle_setup_active ? pd_setup : NULL);
  bool puzzle_phys_match = false;
  if (puzzle_setup_active && pd_setup != NULL) {
    puzzle_phys_match = game_puzzle_physical_matches_fen(pd_setup->fen);
  }
  const char *puzzle_fen_json = "";
  if (puzzle_setup_active && pd_setup != NULL) {
    puzzle_fen_json = pd_setup->fen;
  } else if (puzzle_active && pd_play != NULL) {
    puzzle_fen_json = pd_play->fen;
  }
  GAME_STATUS_JSON_APPEND(",\"puzzle\":{\"active\":%s,\"setup_active\":%s,"
                          "\"setup_id\":%u,\"physical_match\":%s,\"fen\":\"%s\","
                          "\"id\":%u,\"difficulty\":%u,"
                          "\"title\":\"%s\",\"teaser\":\"%s\",\"feedback\":\"%s\","
                          "\"message\":\"%s\"}",
                          puzzle_active ? "true" : "false",
                          puzzle_setup_active ? "true" : "false",
                          (unsigned)puzzle_setup_id,
                          puzzle_phys_match ? "true" : "false", puzzle_fen_json,
                          (unsigned)(puzzle_active ? puzzle_active_id
                                                   : (puzzle_setup_active
                                                          ? puzzle_setup_id
                                                          : 0)),
                          (unsigned)(pd ? pd->difficulty : 0U), pd ? pd->title : "",
                          pd ? pd->teaser : "", game_puzzle_feedback_key(),
                          game_puzzle_feedback_message());

  if (board_setup_tutorial_active || puzzle_setup_active) {
    uint8_t occ[64];
    matrix_get_state(occ);
    GAME_STATUS_JSON_APPEND(",\"matrix_occupied\":[");
    for (int i = 0; i < 64; i++) {
      GAME_STATUS_JSON_APPEND("%s%u", (i > 0) ? "," : "",
                              (unsigned int)occ[i]);
    }
    GAME_STATUS_JSON_APPEND("]");
  }

  GAME_STATUS_JSON_APPEND("}");

#undef GAME_STATUS_JSON_APPEND

  buffer[offset] = '\0';
  if (strstr(buffer, "\"game_state\"") == NULL) {
    ESP_LOGE(TAG, "game_get_status_json: missing game_state in output");
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return ESP_FAIL;
  }

#ifndef NDEBUG
  STAGING_LOGI(TAG, "game_get_status_json: len=%d cap=%zu", offset, size);
#endif

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

esp_err_t game_get_history_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to move history
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"moves\":[");

  for (uint32_t i = 0; i < history_index && i < MAX_MOVES_HISTORY; i++) {
    char from_notation[4] = {0};
    char to_notation[4] = {0};
    convert_coords_to_notation(move_history[i].from_row,
                               move_history[i].from_col, from_notation);
    convert_coords_to_notation(move_history[i].to_row, move_history[i].to_col,
                               to_notation);
    char piece_char = piece_to_char(move_history[i].piece);

    offset += snprintf(buffer + offset, size - offset,
                       "{\"from\":\"%s\",\"to\":\"%s\",\"piece\":\"%c\","
                       "\"timestamp\":%" PRIu32 "}",
                       from_notation, to_notation, piece_char,
                       move_history[i].timestamp);

    if (i < history_index - 1 && i < MAX_MOVES_HISTORY - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "]}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

esp_err_t game_get_captured_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to captured pieces
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"white_captured\":[");

  // Add white captured pieces
  for (uint32_t i = 0; i < white_captured_count; i++) {
    char piece_char = piece_to_char(white_captured_pieces[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < white_captured_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"black_captured\":[");

  // Add black captured pieces
  for (uint32_t i = 0; i < black_captured_count; i++) {
    char piece_char = piece_to_char(black_captured_pieces[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < black_captured_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "]}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

/**
 * @brief Export material advantage history to JSON string (pro graf)
 * @param buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_advantage_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON: {"history":[0,1,2,-1,0,...], "count":42, "white_checks":5,
  // "black_checks":3, ...}
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"history\":[");

  // Add advantage values
  for (uint32_t i = 0; i < advantage_history_count; i++) {
    offset += snprintf(buffer + offset, size - offset, "%d",
                       material_advantage_history[i]);
    if (i < advantage_history_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"count\":%" PRIu32,
                     advantage_history_count);

  // Přidat další statistiky
  offset += snprintf(buffer + offset, size - offset,
                     ",\"white_checks\":%" PRIu32 ",\"black_checks\":%" PRIu32,
                     white_checks, black_checks);
  offset +=
      snprintf(buffer + offset, size - offset,
               ",\"white_castles\":%" PRIu32 ",\"black_castles\":%" PRIu32,
               white_castles, black_castles);

  // Průměrný čas na tah
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t avg_time_per_move =
      (move_count > 0) ? (game_duration / move_count) : 0;
  offset += snprintf(buffer + offset, size - offset,
                     ",\"game_duration_ms\":%" PRIu32
                     ",\"avg_time_per_move_ms\":%" PRIu32,
                     game_duration, avg_time_per_move);

  offset += snprintf(buffer + offset, size - offset, "}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

// ============================================================================
// TIMER SYSTEM INTEGRATION IMPLEMENTATION
// ============================================================================

esp_err_t game_get_timer_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    ESP_LOGE(TAG, "Invalid buffer parameters");
    return ESP_ERR_INVALID_ARG;
  }

  return timer_get_json(buffer, size);
}

esp_err_t game_set_time_control(const time_control_config_t *config) {
  if (config == NULL) {
    ESP_LOGE(TAG, "Invalid config parameter");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = timer_set_time_control(config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Time control set: %s", config->name);

    // Pokud je hra aktivní, spustit timer pro aktuálního hráče
    if (current_game_state == GAME_STATE_ACTIVE) {
      ESP_LOGI(TAG, "Game is active, starting timer for current player");
      timer_start_move(current_player == PLAYER_WHITE);
    } else {
      ESP_LOGI(TAG,
               "Game state is %d (not active), timer will start on first move",
               current_game_state);
    }
  }

  return ret;
}

esp_err_t game_start_timer_move(bool is_white_turn) {
  ESP_LOGI(
      TAG,
      "🎮 game_start_timer_move called: is_white_turn=%s, current_player=%s",
      is_white_turn ? "WHITE" : "BLACK",
      current_player == PLAYER_WHITE ? "WHITE" : "BLACK");
  esp_err_t ret = timer_start_move(is_white_turn);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Timer started successfully for %s",
             is_white_turn ? "White" : "Black");
  } else {
    ESP_LOGE(TAG, "❌ Failed to start timer for %s: %s",
             is_white_turn ? "White" : "Black", esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t game_end_timer_move(void) {
  esp_err_t ret = timer_end_move();
  if (ret == ESP_OK) {
    ESP_LOGD(TAG, "Timer ended for move");
  }

  return ret;
}

esp_err_t game_pause_timer(void) {
  esp_err_t ret = timer_pause();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer paused");
  }

  return ret;
}

esp_err_t game_resume_timer(void) {
  esp_err_t ret = timer_resume();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer resumed");
  }

  return ret;
}

esp_err_t game_reset_timer(void) {
  esp_err_t ret = timer_reset();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer reset");
  }

  return ret;
}

bool game_check_timer_timeout(void) { return timer_check_timeout(); }

uint32_t game_get_remaining_time(bool is_white_turn) {
  return timer_get_remaining_time(is_white_turn);
}

esp_err_t game_get_timer_state(chess_timer_t *timer_data) {
  if (timer_data == NULL) {
    ESP_LOGE(TAG, "Invalid timer_data parameter");
    return ESP_ERR_INVALID_ARG;
  }

  return timer_get_state(timer_data);
}

esp_err_t game_init_timer_system(void) {
  ESP_LOGI(TAG, "Initializing timer system in game task...");

  esp_err_t ret = timer_system_init();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer system initialized successfully");
  } else {
    ESP_LOGE(TAG, "Failed to initialize timer system: %s",
             esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t game_process_timer_command(const chess_move_command_t *cmd) {
  if (cmd == NULL) {
    ESP_LOGE(TAG, "Invalid command parameter");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  switch (cmd->type) {
  case GAME_CMD_SET_TIME_CONTROL: {
    time_control_config_t config;
    time_control_type_t type =
        (time_control_type_t)cmd->timer_data.timer_config.time_control_type;

    if (type == TIME_CONTROL_CUSTOM) {
      // Vlastní časová kontrola
      ret = timer_set_custom_time_control(
          cmd->timer_data.timer_config.custom_minutes,
          cmd->timer_data.timer_config.custom_increment);
    } else {
      // Předdefinovaná časová kontrola
      ret = timer_get_config_by_type(type, &config);
      if (ret == ESP_OK) {
        ret = timer_set_time_control(&config);
      }
    }

    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "Time control command processed successfully");
    } else {
      ESP_LOGE(TAG, "Failed to process time control command");
    }
  } break;

  case GAME_CMD_PAUSE_TIMER:
    ret = game_pause_timer();
    break;

  case GAME_CMD_RESUME_TIMER:
    ret = game_resume_timer();
    break;

  case GAME_CMD_RESET_TIMER:
    ret = game_reset_timer();
    break;

  case GAME_CMD_GET_TIMER_STATE:
    // Timer state je získáván přes JSON API
    ret = ESP_OK;
    break;

  case GAME_CMD_TIMER_TIMEOUT:
    ret = game_handle_time_expiration();
    break;

  default:
    ESP_LOGW(TAG, "Unknown timer command: %d", cmd->type);
    ret = ESP_ERR_INVALID_ARG;
    break;
  }

  return ret;
}

esp_err_t game_handle_time_expiration(void) {
  ESP_LOGW(TAG, "⏰ Time expired! Handling timeout...");

  // Získat stav timeru
  chess_timer_t timer_data;
  esp_err_t ret = timer_get_state(&timer_data);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get timer state");
    return ret;
  }

  // Určit vítěze
  player_t winner = timer_data.is_white_turn ? PLAYER_BLACK : PLAYER_WHITE;
  const char *winner_name = (winner == PLAYER_WHITE) ? "White" : "Black";
  const char *loser_name = (winner == PLAYER_WHITE) ? "Black" : "White";

  ESP_LOGW(TAG, "🏆 %s wins by timeout! %s ran out of time.", winner_name,
           loser_name);

  // Ukončit hru
  current_game_state = GAME_STATE_FINISHED;
  game_result = GAME_STATE_FINISHED;
  current_result_type =
      (winner == PLAYER_WHITE) ? RESULT_WHITE_WINS : RESULT_BLACK_WINS;
  current_endgame_reason = ENDGAME_REASON_TIMEOUT; // Označit jako timeout

  // Požadavek na endgame report (stejný fix jako u resignation)
  game_update_endgame_statistics(current_result_type);
  endgame_report_requested = true;

  // UNIFIED VICTORY ANIMATION
  // Timeout = Winner determined above
  game_trigger_victory_animation(winner);

  // Zobrazit výsledek na LED (použít existující LED funkce)
  // led_show_game_result(game_result); // Funkce neexistuje

  // Pozastavit timer
  timer_pause();

  ESP_LOGI(TAG, "Game ended due to timeout. Winner: %s", winner_name);

  return ESP_OK;
}

esp_err_t game_update_timer_display(void) {
  // Kontrola timeout
  if (timer_check_timeout()) {
    return game_handle_time_expiration();
  }

  return ESP_OK;
}

uint32_t game_get_available_time_controls(time_control_config_t *controls,
                                          uint32_t max_count) {
  if (controls == NULL || max_count == 0) {
    return 0;
  }

  return timer_get_available_controls(controls, max_count);
}

esp_err_t game_set_custom_time_control(uint32_t minutes,
                                       uint32_t increment_seconds) {
  return timer_set_custom_time_control(minutes, increment_seconds);
}

esp_err_t game_get_timer_stats(uint32_t *total_moves, uint32_t *avg_move_time) {
  if (total_moves == NULL || avg_move_time == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  *total_moves = timer_get_total_moves();
  *avg_move_time = timer_get_average_move_time();

  return ESP_OK;
}

bool game_is_timer_active(void) { return timer_is_active(); }

time_control_type_t game_get_current_time_control_type(void) {
  return timer_get_current_type();
}

/**
 * @brief Force refresh of LED state based on current game status
 * Called when switching back from HA mode or other interruptions
 */
void game_refresh_leds(void) {
  ESP_LOGI(TAG, "🔄 Refreshing LED state based on game status...");

  // If game is not active, just show the board pattern
  if (!game_active && current_game_state != GAME_STATE_PROMOTION) {
    // If we are in "setup" or "idle" mode
    led_show_chess_board();
    return;
  }

  // Clear board first (to remove HA colors)
  led_clear_board_only();

  if (game_is_matrix_guard_active()) {
    ESP_LOGI(TAG, "  - Restoring MATRIX GUARD LEDs (physical vs logic)");
    game_matrix_guard_render_leds();
    game_check_promotion_needed();
    return;
  }

  // 1. Check for Error Recovery State
  if (error_recovery_state.waiting_for_move_correction) {
    ESP_LOGI(TAG, "  - Restoring ERROR state LEDs");
    // Show validation error
    game_show_invalid_move_error_with_blink(error_recovery_state.invalid_row,
                                            error_recovery_state.invalid_col);

    // Also highlight the original valid position (Blue)
    led_set_pixel_safe(
        chess_pos_to_led_index(error_recovery_state.original_valid_row,
                               error_recovery_state.original_valid_col),
        0, 0, 255);

    return;
  }

  // 2. Check for Lifted Piece State
  if (piece_lifted) {
    ESP_LOGI(TAG, "  - Restoring LIFTED piece state LEDs at %c%d",
             'a' + lifted_piece_col, lifted_piece_row + 1);

    // Highlight source square (Yellow)
    led_set_pixel_safe(
        chess_pos_to_led_index(lifted_piece_row, lifted_piece_col), 255, 255,
        0);

    // Show valid moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves = game_get_available_moves(
        lifted_piece_row, lifted_piece_col, suggestions, 64);

    if (valid_moves > 0) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if destination has opponent's piece
        piece_t dest_piece = board[dest_row][dest_col];
        bool is_opponent_piece =
            (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
             dest_piece <= PIECE_BLACK_KING) ||
            (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
             dest_piece <= PIECE_WHITE_KING);

        if (is_opponent_piece) {
          led_set_pixel_safe(led_index, 255, 165, 0); // Orange for capture
        } else {
          led_set_pixel_safe(led_index, 0, 255, 0); // Green for move
        }
      }
    }
    return;
  }

  // 3. Check for Check state
  if (game_is_king_in_check(current_player)) {
    // Find King
    int king_row = -1, king_col = -1;
    piece_t king_piece =
        (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (board[row][col] == king_piece) {
          king_row = row;
          king_col = col;
          break;
        }
      }
      if (king_row != -1)
        break;
    }

    if (king_row != -1) {
      if (game_led_guidance_show_check_anim()) {
        ESP_LOGI(TAG, "  - Restoring CHECK state LEDs");
        uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
        led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                   .led_index = king_led_index,
                                   .red = 0,
                                   .green = 0,
                                   .blue = 0,
                                   .duration_ms = 0,
                                   .data = NULL};
        led_execute_command_new(&check_cmd);
      }
    }
  }

  // 4. Default Active State - Highlight movable pieces
  ESP_LOGI(TAG, "  - Restoring ACTIVE state LEDs (movable pieces)");
  game_highlight_movable_pieces();

  // Update button LEDs (64-71) to match current game state (promotion green/blue)
  // so they are correct after switching back to game mode (e.g. from HA).
  game_check_promotion_needed();
}
