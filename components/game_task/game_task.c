
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
esp_err_t game_task_wdt_reset_safe(void) {
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
uint32_t game_state_revision = 0;
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
bool auto_new_game_in_starting_position = false;
uint32_t auto_new_game_timer_start = 0;
bool auto_new_game_triggered = false; // Flag aby se neopakoval reset

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
bool starting_position_check_enabled = false;

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
void resignation_tick(void);
static void resignation_finalize_timeout(void);
bool game_cmd_is_matrix_origin(const chess_move_command_t *cmd);

// Game statistics
uint32_t total_games = 0;
uint32_t white_wins = 0;
uint32_t black_wins = 0;
uint32_t draws = 0;

// Flag pro endgame report request (místo volání z timer callbacku)
bool endgame_report_requested = false;

endgame_reason_t current_endgame_reason = ENDGAME_REASON_CHECKMATE;

last_move_type_t last_move_type = LAST_MOVE_NORMAL;

// Extended game statistics
uint32_t game_start_time = 0;
uint32_t last_move_time = 0;
uint32_t white_time_total = 0;
uint32_t black_time_total = 0;
uint32_t white_moves_count = 0;
uint32_t black_moves_count = 0;
uint32_t white_captures = 0;
uint32_t black_captures = 0;
uint32_t white_checks = 0;
uint32_t black_checks = 0;
uint32_t white_castles = 0;
uint32_t black_castles = 0;

// Captured pieces tracking
piece_t white_captured_pieces[GAME_TASK_MAX_CAPTURED_PIECES];
piece_t black_captured_pieces[GAME_TASK_MAX_CAPTURED_PIECES];
uint32_t white_captured_count = 0;
uint32_t black_captured_count = 0;
uint32_t white_captured_index = 0;
uint32_t black_captured_index = 0;

// Material advantage tracking (pro graf výhody v průběhu hry)
int8_t material_advantage_history[GAME_TASK_MAX_ADVANTAGE_HISTORY];
uint32_t advantage_history_count = 0;

/**
 * @brief Convert piece_t to character representation
 */
char piece_to_char(piece_t piece) {
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
  if (advantage_history_count >= GAME_TASK_MAX_ADVANTAGE_HISTORY) {
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
    if (black_captured_index < GAME_TASK_MAX_CAPTURED_PIECES) {
      black_captured_pieces[black_captured_index++] = piece;
      black_captured_count++;
      black_captures++;
    }
  } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
    // Black piece captured by white
    if (white_captured_index < GAME_TASK_MAX_CAPTURED_PIECES) {
      white_captured_pieces[white_captured_index++] = piece;
      white_captured_count++;
      white_captures++;
    }
  }
}

uint32_t moves_without_capture = 0;
uint32_t max_moves_without_capture = 0;
static uint32_t position_history[100];
uint32_t position_history_count = 0;

// Game state flags
static bool timer_enabled = true;
bool game_saved = false;
char saved_game_name[32] = "";
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
void game_trigger_victory_animation(player_t winner) {
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
void resignation_tick(void) {
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
