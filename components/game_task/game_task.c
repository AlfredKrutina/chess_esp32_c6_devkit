
/**
 * @file game_task.c
 * @brief Game Task - chess logic and game-state management
 *
 * @details
 * =============================================================================
 * WHAT THIS FILE DOES
 * =============================================================================
 *
 * This task is the "brain" of the chessboard. It manages all game logic:
 * 1. Chess rules (standard FIDE rules)
 * 2. Move validation (legal/illegal moves)
 * 3. Special-move detection (castling, en passant, promotion)
 * 4. End-of-game detection (checkmate, stalemate, draw)
 * 5. Clock management (timer system integration)
 * 6. Move history and position analysis
 * 7. Error recovery (red/blue LEDs)
 * 8. Communication with the physical board (squares_task)
 *
 *
 * =============================================================================
 * HOW IT WORKS
 * =============================================================================
 *
 * STARTUP:
 * - Initialize chessboard (starting position)
 * - Register with WDT (watchdog timer)
 * - Initialize timer system
 * - Start main loop (100 ms cycle)
 *
 * MAIN LOOP (100 ms cycle):
 * while (1) {
 *     1. Reset WDT
 *     2. Process commands from queue (game_command_queue)
 *     3. Process matrix events (UP/DN signals from physical board)
 *     4. Update timer display
 *     5. Run periodic checks
 *     6. Wait 100 ms (vTaskDelayUntil)
 * }
 *
 * MOVE PROCESSING:
 * 1. Matrix: UP signal (piece lifted) -> store position
 * 2. Matrix: DN signal (piece placed) -> validate move
 * 3. If valid: execute move, update state, LED animation
 * 4. If invalid: error recovery (red/blue LEDs)
 * 5. Check end of game (mate/stalemate)
 *
 * =============================================================================
 * COMMUNICATION (FIFOS & MUTEXES)
 * =============================================================================
 *
 * QUEUES - Receiving commands:
 * - game_command_queue -> Commands from game_submit() (web, BLE, squares, main)
 * - squares_event_queue -> UP/DN events from physical board
 *
 * QUEUES - Sending responses:
 * - response_queue -> Replies to commands (status, board...)
 * - game_event_queue -> LED scenes for led_task (game_task does not write the strip)
 *
 * MUTEXES - Protecting shared resources:
 * - game_mutex -> Protects game state (board, current_player, move_count...)
 *   IMPORTANT: Always take it when accessing game state!
 *
 * ACCESS:
 * @code
 * xSemaphoreTake(game_mutex, portMAX_DELAY);  // Lock
 * // Work with game state
 * xSemaphoreGive(game_mutex);                  // Unlock
 * @endcode
 *
 * =============================================================================
 * CHESS RULES IMPLEMENTED
 * =============================================================================
 *
 * BASIC MOVES:
 * - Pawn: 1 or 2 squares forward from start rank, captures diagonally
 * - Rook: Horizontally/vertically
 * - Knight: L-shape (2+1 squares)
 * - Bishop: Diagonally
 * - Queen: Combination of rook + bishop
 * - King: 1 square in any direction
 *
 * SPECIAL MOVES:
 * - Castling (kingside and queenside) - with rule checks
 * - En passant - detection and execution
 * - Pawn promotion - automatic to queen (switchable later)
 *
 * END OF GAME:
 * - Checkmate - king in check, no legal moves
 * - Stalemate - not in check, but no legal moves
 * - 50-move rule - 50 moves without capture or pawn move
 * - Threefold repetition - same position 3 times
 * - Insufficient material - not enough material to mate
 *
 * =============================================================================
 * TABLE OF CONTENTS
 * =============================================================================
 *
 * Section 1:  Global Variables & State ............... line 100
 * Section 2:  Board Initialization .................... line 1070
 * Section 3:  Move Validation ......................... line 2000
 * Section 4:  Special Moves (Castle, En Passant) ...... line 3500
 * Section 5:  Move Execution .......................... line 4500
 * Section 6: Endgame Detection ...................... line 5500
 * Section 7: Command Processing ...................... line 6500
 * Section 8: Main Game Task Loop ..................... line 7964
 * Section 9: JSON API Functions ...................... line 11477
 *
 * TIP: Ctrl+G to jump to a line
 *
 * =============================================================================
 * DEPENDENCIES
 * =============================================================================
 *
 * - squares_task: Receive UP/DN event from physical board
 * - led_task: LED control (highlight, animation)
 * - timer_system: Timer for the game
 * - uart_task: diagnostics console only (no chess commands)
 * - web_server_task: Commands from the web
 * - unified_animation_manager: Move animation
 *
 * =============================================================================
 * CRITICAL RULES
 * =============================================================================
 *
 * @warning DO NOT:
 *
 * 1. NEVER access game state without game_mutex!
 *    ❌ current_player = PLAYER_WHITE;  // RACE - can corrupt state
 *    ✅ xSemaphoreTake(game_mutex, ...); current_player = PLAYER_WHITE; xSemaphoreGive(...);
 *
 * 2. NEVER hold a mutex too long!
 *    ❌ xSemaphoreTake(...); long_operation(); xSemaphoreGive(...);
 *    ✅ Perform only necessary operations under the mutex
 *
 * 3. NEVER call a blocking operation with a long timeout!
 *    ❌ xQueueReceive(queue, ..., portMAX_DELAY);  // May block WDT
 *    ✅ xQueueReceive(queue, ..., pdMS_TO_TICKS(100));  // Max 100ms
 *
 * 4. ALWAYS reset the WDT in the main loop!
 *    ✅ game_task_wdt_reset_safe();  // In each 100ms cycle
 *
 * 5. ALWAYS check return values!
 *    ❌ xQueueSend(...);  // Ignores the error
 *    ✅ if (xQueueSend(...) != pdTRUE) { handle error }
 *
 * @author Alfred Krutina
 * @version 1.8.0
 *
 * @note Task priority 4; main loop approx. 100 ms; WDT reset in loop.
 * @see squares_task.c Figure movement detection
 * @see led_task.c LED animation
 * @see uart_task.c Terminal
 * @see web_server_task.c Web interface
 */

#include "game_gesture.h"
#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_mode.h"
#include "game_board_core.h"
#include "game_squares_guard.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_restore_guide.h"
#include "game_hall_position.h"
#endif
#include "game_snapshot.h"
#include "position.h"
#include "game_physical_undo.h"
#include "game_policy.h"
#include "game_required_step.h"
#include "../button_task/include/button_task.h"
#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "freertos_chess.h"
#include "game_events.h"
#include "task_wdt.h"
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
 * @brief Safe WDT reset with WARNING instead of ERROR logging for
 * ESP_ERR_NOT_FOUND
 *
 * This function safely resets the Task Watchdog Timer. If the task is not yet
 * registered (which is normal during startup), WARNING is logged instead of ERROR.
 *
 * @return ESP_OK if successful, ESP_ERR_NOT_FOUND if the task is not registered
 * (WARNING only)
 *
 * @details
 * The function is used to safely reset the watchdog timer during game operation.
 * Prevents startup errors when the task is not yet registered.
 */
esp_err_t game_task_wdt_reset_safe(void) {
  return chess_task_wdt_reset_safe(TAG);
}

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

// Direction vectors for piece movement ((game_state_knight_moves()) in game_move_gen.c)

// Game configuration
// #define GAME_TIMEOUT_MS 300000 // 5 minutes per move timeout   not used in
// this version
#define MOVE_VALIDATION_MS 100 // Move validation timeout

// Game state
/** Monotonic version of state for clients (ETag / If-None-Match, delta). */
static uint32_t s_game_state_revision = 0;

uint32_t *game_state_game_state_revision(void) { return &s_game_state_revision; }
#define game_state_revision (*game_state_game_state_revision())
// New logic: Block auto-new game until at least one move is made
static bool s_auto_new_game_blocked_until_move = false;

bool *game_state_auto_new_game_blocked_until_move(void) { return &s_auto_new_game_blocked_until_move; }
#define auto_new_game_blocked_until_move (*game_state_auto_new_game_blocked_until_move())


// Active castling state (simplified implementation)

// Rook animation state


// Global variables for tracking lifted piece (UP/DN commands)

static const game_mode_t *s_active_mode;
static const game_mode_t *s_suspended_mode;
static bool s_drop_reject;

void game_mode_set(const game_mode_t *mode) {
  s_active_mode = mode;
  s_suspended_mode = NULL;
  s_drop_reject = false;
}

const game_mode_t *game_mode_current(void) { return s_active_mode; }

void game_mode_clear(const game_mode_t *mode) {
  if (mode == NULL) {
    return;
  }
  if (s_active_mode == mode) {
    s_active_mode = NULL;
  }
  if (s_suspended_mode == mode) {
    s_suspended_mode = NULL;
  }
}

void game_mode_suspend_for(const game_mode_t *mode) {
  if (mode == NULL || s_active_mode == mode) {
    return;
  }
  s_suspended_mode = s_active_mode;
  s_active_mode = mode;
}

void game_mode_resume(const game_mode_t *mode) {
  if (s_active_mode != mode) {
    return;
  }
  s_active_mode = s_suspended_mode;
  s_suspended_mode = NULL;
}

void game_mode_reject_drop(void) { s_drop_reject = true; }

bool game_mode_take_drop_reject(void) {
  bool rejected = s_drop_reject;
  s_drop_reject = false;
  return rejected;
}

void game_mode_call_lift(const position_t *pos, uint8_t square) {
  if (s_active_mode != NULL && s_active_mode->on_lift != NULL) {
    s_active_mode->on_lift(pos, square);
  }
}

void game_mode_call_drop(const position_t *pos, uint8_t from, uint8_t to,
                         bool has_from) {
  s_drop_reject = false;
  if (s_active_mode != NULL && s_active_mode->on_drop != NULL) {
    s_active_mode->on_drop(pos, from, to, has_from);
  }
}

void game_mode_call_move_applied(const position_t *pos, position_move_t mv) {
  if (s_active_mode != NULL && s_active_mode->on_move_applied != NULL) {
    s_active_mode->on_move_applied(pos, mv);
  }
}

void game_mode_call_leds(const position_t *pos) {
  if (s_active_mode != NULL && s_active_mode->leds != NULL) {
    s_active_mode->leds(pos);
  }
}

// 3-step capture flow state.

static bool guided_capture_hints_enabled = true;
/** 1 = lift source only … 5 = full help incl. guided capture on LED */
static uint8_t led_guidance_level = 5;

bool game_led_guidance_show_destinations(void) {
  return game_policy_move_hints_legal_blue() && led_guidance_level >= 2;
}

bool game_led_guidance_show_movable_yellow(void) {
  return game_policy_move_hints_movable_yellow() && led_guidance_level >= 3;
}

bool game_led_guidance_show_check_anim(void) {
  return led_guidance_level >= 4;
}


// ============================================================================
// AUTO NEW GAME DETECTION
// ============================================================================

/**
 * @brief Auto new game detection state
 *
 * Detects when all pieces are in the starting position for 2 seconds
 * and automatically start a new game. Prevents infinite reset.
 */
static bool s_auto_new_game_in_starting_position = false;

bool *game_state_auto_new_game_in_starting_position(void) { return &s_auto_new_game_in_starting_position; }
#define auto_new_game_in_starting_position (*game_state_auto_new_game_in_starting_position())
static uint32_t s_auto_new_game_timer_start = 0;

uint32_t *game_state_auto_new_game_timer_start(void) { return &s_auto_new_game_timer_start; }
#define auto_new_game_timer_start (*game_state_auto_new_game_timer_start())
static bool s_auto_new_game_triggered = false; // Flag aby se neopakoval reset

bool *game_state_auto_new_game_triggered(void) { return &s_auto_new_game_triggered; }
#define auto_new_game_triggered (*game_state_auto_new_game_triggered())

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

// En passant state

// Move history

// Task state
static bool task_running = false;

//  Error handling - remembering the last valid field
// Enhanced error recovery state for smart error handling


// ============================================================================
// STARTING POSITION CHECK SETTINGS
// ============================================================================

/**
 * @brief Start position watch - on/off
 *
 * If it is turned on, the system monitors whether the figures are placed correctly
 * before the start of the game. Disabled by default (false).
 */
static bool s_starting_position_check_enabled = false;

bool *game_state_starting_position_check_enabled(void) { return &s_starting_position_check_enabled; }
#define starting_position_check_enabled (*game_state_starting_position_check_enabled())

/**
 * @brief Sets the initial position watch
 * @param enabled true to enable, false to disable
 */
void game_set_starting_position_check(bool enabled) {
  starting_position_check_enabled = enabled;
  ESP_LOGI(TAG, "Starting position check: %s",
           enabled ? "ENABLED" : "DISABLED");
}

/**
 * @brief Returns the initial position guard status
 * @return true if watchdog is on, false otherwise
 */
bool game_get_starting_position_check(void) {
  return starting_position_check_enabled;
}

// ============================================================================
// PROMOTION STATE - PHYSICAL BUTTON SUPPORT
// ============================================================================

/**
 * @brief Pesce graduation status
 *
 * Monitors whether graduation is possible and where the graduation dog is located.
 * It is used for controlling the LED indication of the buttons and processing the button event.
 */

// Mutex to protect (*game_state_promotion_state()) from race conditions.
// Prevents concurrent access from web, UART, button, and matrix tasks

/** Set in game_process_chess_move before game_execute_move; is consumed on
 * start of game_execute_move (immediate promotion from API instead of promotion pending). */

// ============================================================================
// KING RESIGNATION SYSTEM
// ============================================================================



// Forward declarations pro resignation (implementations in game_resignation.c)
bool game_cmd_is_squares_origin(const game_cmd_t *cmd);

// Game statistics

// Flag for endgame report request (instead of calling from a timer callback)




// Extended game statistics

// Captured pieces tracking

// Material advantage tracking (for in-game advantage chart)

/**
 * @brief Convert piece_t to character representation
 */

/**
 * @brief Calculate the current material yield on the board
 * @return Material's advantage (positive = White leads, negative = Black leads)
 */

/**
 * @brief Save the current material output to history
 */

/**
 * @brief Add captured piece to tracking
 */

static uint32_t position_history[100];

// Game state flags
static bool timer_enabled = true;

// Castling flags and en passant state
// (declared later in the file)

// Include ctype.h for tolower function
#include <ctype.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================


// Error recovery functions
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
      piece_t piece = game_get_piece(row, col);
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
;

/**
 * @brief Calculate material balance
 * @param white_material Output: white material value
 * @param black_material Output: black material value
 * @return Material difference (positive = white advantage, negative = black
 * advantage)
 */

/**
 * @brief Get material balance as string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */


/**
 * @brief Print comprehensive game statistics
 */

// Last move tracking for board display

// Captured pieces counter (removed - already declared above with char arrays)

// ============================================================================
// ENHANCED MOVE VALIDATION SYSTEM
// ============================================================================

// Tutorial mode state
static bool tutorial_mode_active = false;
static bool show_hints = true;
/** "baseline" web: empty logic, matrix guard suppressed. */
// Game analysis flags (for future use)
// static bool show_warnings = true;
// static bool show_analysis = true;

// Move analysis cache (for future use)
// static move_suggestion_t move_suggestions[100];
// static uint32_t suggestion_count = 0;
// static uint32_t last_analysis_time = 0;

// Piece symbols for display (ASCII) - FIXED INDEXES
static const char * s_piece_symbols[] = {
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

const char **game_state_piece_symbols(void) { return s_piece_symbols; }
#define piece_symbols (game_state_piece_symbols())


// ============================================================================
// ERROR FORMATTING HELPER
// ============================================================================

/**
 * @brief Helper function for consistent formatting of the error detail line
 *
 * @param label Field name (eg "Reason", "Solution", "Hint")
 * @param fmt Printf-style format string
 * @param ... Variable arguments for the format string
 *
 * @details
 * This function ensures consistent formatting of all error messages.
 * Dumps a variadic function with printf-style arguments.
 */
static void print_error_detail(const char *label, const char *fmt, ...) {
  if (!game_policy_uart_error_detail_enabled()) {
    return;
  }
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
 * @brief Display a detailed error message with reasoning for an invalid move
 *
 * @param error Type of move error (MOVE_ERROR_*)
 * @param move Pointer to the move that caused the error
 *
 * @details
 * The function displays a color-formatted error message including:
 * - The reason why the move is invalid
 * - Hints why the move failed
 * - Suggested solution
 * - Status of target field
 *
 * Special reasoning for fish:
 * - Distinguishes between an empty field and your own figure
 * - Explains why the pawn cannot go diagonally to an empty square
 * - Explains why the dog cannot move forward
 *
 */
void game_display_move_error(move_error_t error, const chess_move_t *move) {
  char from_square[3], to_square[3];
  game_coords_to_square(move->from_row, move->from_col, from_square);
  game_coords_to_square(move->to_row, move->to_col, to_square);

  const char *piece_name = game_get_piece_name(move->piece);
  const char *player_name =
      (current_player == PLAYER_WHITE) ? "White" : "Black";
  piece_t source_piece = game_get_piece(move->from_row, move->from_col);
  piece_t dest_piece = game_get_piece(move->to_row, move->to_col);

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
    // IMPROVED REASONING for pawns
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
        if (game_get_piece(r, c) == king_piece) {
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
 * @brief Helper function for adding a square to a CSV sheet
 *
 * @param buffer Target buffer
 * @param size Maximum buffer size
 * @param pos Current write position
 * @param square Square notation to add (eg "e4")
 * @return New position after adding
 *
 * @details
 * The function automatically adds a check mark if the buffer already contains data.
 * It is used for building the CSV sheet of the move.
 */
static int append_square(char *buffer, size_t size, int pos,
                         const char *square) {
  // Add a separator if something is already in the buffer
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
static move_suggestion_t s_move_suggestions[64];
static game_evt_hint_pixel_t s_lift_hint_pixels[GAME_EVT_HINT_MAX];

move_suggestion_t *game_move_suggestions_work(void) {
  return s_move_suggestions;
}

void game_event_publish_lift_hints(uint8_t lift_sq) {
  uint8_t row = (uint8_t)(lift_sq / 8);
  uint8_t col = (uint8_t)(lift_sq % 8);
  uint32_t valid_moves = 0;
  uint8_t n = 0;

  if (lift_sq >= 64) {
    return;
  }
  if (game_led_guidance_show_destinations()) {
    valid_moves =
        game_get_available_moves(row, col, s_move_suggestions, 64);
  }
  for (uint32_t i = 0; i < valid_moves && n < GAME_EVT_HINT_MAX; i++) {
    uint8_t dest_sq = (uint8_t)(s_move_suggestions[i].to_row * 8u +
                                s_move_suggestions[i].to_col);
    game_evt_hint_pixel_t *h = &s_lift_hint_pixels[n];
    h->square = dest_sq;
      if (s_move_suggestions[i].is_castling) {
      if (!game_policy_move_hints_castling_blue()) {
        continue;
      }
      h->r = 0;
      h->g = 0;
      h->b = 255;
    } else {
      piece_t dest_piece = game_get_piece(s_move_suggestions[i].to_row,
                                          s_move_suggestions[i].to_col);
      bool opponent =
          (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
           dest_piece <= PIECE_BLACK_KING) ||
          (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
           dest_piece <= PIECE_WHITE_KING);
      if (s_move_suggestions[i].is_check) {
        h->r = 255;
        h->g = 255;
        h->b = 255;
      } else if (opponent) {
        h->r = 255;
        h->g = 165;
        h->b = 0;
      } else {
        h->r = 0;
        h->g = 255;
        h->b = 0;
      }
    }
    n++;
    if (en_passant_available &&
        s_move_suggestions[i].to_row == en_passant_target_row &&
        s_move_suggestions[i].to_col == en_passant_target_col &&
        n < GAME_EVT_HINT_MAX) {
      game_evt_hint_pixel_t *ep = &s_lift_hint_pixels[n++];
      ep->square =
          (uint8_t)(en_passant_victim_row * 8u + en_passant_victim_col);
      /* Reed remove cue. Must not replace the empty landing square. */
      ep->r = 255;
      ep->g = 80;
      ep->b = 0;
    }
  }
  game_event_post_pixels(s_lift_hint_pixels, n, NULL, 0, 0, false, lift_sq);
}

void game_show_move_suggestions(uint8_t row, uint8_t col) {
  piece_t piece = game_get_piece(row, col);
  if (piece == PIECE_EMPTY) {
    ESP_LOGI(TAG, "💡 Hint: No piece at this position");
    return;
  }

  // Get available moves for this piece
  uint32_t count =
      game_get_available_moves(row, col, s_move_suggestions, 50);

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
    game_coords_to_square(s_move_suggestions[i].to_row,
                          s_move_suggestions[i].to_col, to_square);

    if (s_move_suggestions[i].is_capture) {
      capture_pos = append_square(capture_moves, sizeof(capture_moves),
                                  capture_pos, to_square);
    } else if (s_move_suggestions[i].is_castling ||
               s_move_suggestions[i].is_en_passant) {
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

  if (game_led_guidance_show_destinations()) {
    game_event_publish_lift_hints((uint8_t)(row * 8u + col));
  }
}

static bool suggestion_gives_check(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, uint8_t to_col,
                                   piece_t piece) {
  position_move_t mv;
  position_status_t st;
  bool white;

  position_t *scratch = game_logic_scratch_position();
  if (game_logic_position() == NULL || scratch == NULL) {
    return false;
  }
  *scratch = *game_logic_position();
  mv.from = (uint8_t)(from_row * 8u + from_col);
  mv.to = (uint8_t)(to_row * 8u + to_col);
  mv.promo = POS_PROMO_NONE;
  if (!position_apply(scratch, mv)) {
    return false;
  }
  white = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
  st = position_status(scratch);
  (void)white;
  return st.in_check;
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

  piece_t piece = game_get_piece(row, col);

  // If the piece is empty and the resignation timer is active for that position,
  // use saved king position from (*game_state_resignation_state())
  if (piece == PIECE_EMPTY) {
    if ((*game_state_resignation_state()).active && (*game_state_resignation_state()).king_row == row &&
        (*game_state_resignation_state()).king_col == col) {
      // King is raised during resignation timer - use king type z
      // (*game_state_resignation_state()).player
      piece = ((*game_state_resignation_state()).player == PLAYER_WHITE) ? PIECE_WHITE_KING
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

  // If the rook is in progress and the rook is being raised from rook_from, add move to
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
      // This is a rook for rook - add move to rook_to as a valid move
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
        suggestions[count].is_check = suggestion_gives_check(
            row, col, castling_state.rook_to_row, castling_state.rook_to_col,
            piece);
        suggestions[count].is_castling = false;
        suggestions[count].is_en_passant = false;
        suggestions[count].score = 0;
        count++;
        ESP_LOGI(
            TAG,
            "🏰 Castling rook: Added valid move from %c%d to %c%d (count=%" PRIu32 ")",
            'a' + col, row + 1, 'a' + castling_state.rook_to_col,
            castling_state.rook_to_row + 1, count);
        return count; // Return only this move - other moves are blocked
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
      int to_row = row + (game_state_knight_moves())[i][0];
      int to_col = col + (game_state_knight_moves())[i][1];

      if (!game_is_valid_square(to_row, to_col))
        continue;

      piece_t target = game_get_piece(to_row, to_col);
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
        suggestions[count].is_check = suggestion_gives_check(
            row, col, (uint8_t)to_row, (uint8_t)to_col, piece);
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
                                  .captured_piece = game_get_piece(to_row, to_col),
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
              (game_get_piece(to_row, to_col) != PIECE_EMPTY);
          suggestions[count].is_check = suggestion_gives_check(
              row, col, (uint8_t)to_row, (uint8_t)to_col, piece);

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

bool game_squares_allow_second_sequential_lift(void) {
  if (game_is_squares_guard_active()) {
    return false;
  }
  if (game_physical_undo_is_active()) {
    return true;
  }
  if (guided_capture_state.active) {
    return true;
  }
  if ((*game_state_resignation_state()).active || (*game_state_promotion_state()).pending ||
      castling_state.in_progress || game_is_castle_animation_active() ||
      (*game_state_error_recovery_state()).waiting_for_move_correction ||
      capture_in_progress) {
    return false;
  }

  piece_t pending_piece = PIECE_EMPTY;
  if (piece_lifted && lifted_piece != PIECE_EMPTY) {
    pending_piece = lifted_piece;
  } else {
    /* Race: matrix queued PICKUP before game_task set piece_lifted. */
    uint8_t pending_sq = squares_get_pending_lift_square();
    if (pending_sq != 255) {
      pending_piece = game_get_piece(pending_sq / 8, pending_sq % 8);
    }
  }

  if (pending_piece == PIECE_EMPTY) {
    return false;
  }
  /* Phase 2b: a second lift is the other piece of a capture or a swap.
   * The hand keeps both. Guard still owns a scan that lifts more than two. */
  return true;
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
      piece_t piece = game_get_piece(row, col);

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
  if ((*game_state_white_captured_count()) > 0 || (*game_state_black_captured_count()) > 0) {
    ESP_LOGI(TAG, "Captured pieces:");
    if ((*game_state_white_captured_count()) > 0) {
      printf("  White captured: ");
      for (uint32_t i = 0; i < (*game_state_white_captured_index()); i++) {
        printf("%s ", piece_symbols[(int)(game_state_white_captured_pieces())[i]]);
      }
      printf("(%lu total)\n", (unsigned long)(*game_state_white_captured_count()));
    }
    if ((*game_state_black_captured_count()) > 0) {
      printf("  Black captured: ");
      for (uint32_t i = 0; i < (*game_state_black_captured_index()); i++) {
        printf("%s ", piece_symbols[(int)(game_state_black_captured_pieces())[i]]);
      }
      printf("(%lu total)\n", (unsigned long)(*game_state_black_captured_count()));
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

uint32_t game_move_history_count(void) { return history_index; }

bool game_format_move_history_line(uint32_t index, char *buf, size_t cap) {
  if (buf == NULL || cap == 0 || index >= history_index) {
    return false;
  }
  const chess_move_t *move = &move_history[index];
  int n = snprintf(buf, cap, "%lu. %c%d-%c%d %s", (unsigned long)(index + 1),
                   'a' + move->from_col, move->from_row + 1, 'a' + move->to_col,
                   move->to_row + 1, game_get_piece_name(move->piece));
  return n > 0 && (size_t)n < cap;
}

void game_print_move_history(void) {
  uint32_t n = history_index;
  ESP_LOGI(TAG, "Move history (%lu moves):", (unsigned long)n);

  char line[80];
  for (uint32_t i = 0; i < n; i++) {
    if (!game_format_move_history_line(i, line, sizeof(line))) {
      break;
    }
    ESP_LOGI(TAG, "  %s", line);
  }
}

// ============================================================================
// GENIUS COMPATIBILITY FUNCTIONS
// ============================================================================

/**
 * @brief Send response to UART task via queue
 */
void game_cmd_reply(const char *message, bool is_error,
                    QueueHandle_t response_queue) {
  if (response_queue == NULL) {
    if (is_error) {
      ESP_LOGE(TAG, "GAME_ERROR: %s", message ? message : "Unknown error");
    } else {
      ESP_LOGI(TAG, "GAME_SUCCESS: %s", message ? message : "Success");
    }
    return;
  }

  game_response_t response = {
      .type = is_error ? GAME_RESPONSE_ERROR : GAME_RESPONSE_SUCCESS,
      .command_type = 0,
      .error_code = is_error ? 1 : 0,
      .timestamp = esp_timer_get_time() / 1000};
  if (message != NULL) {
    strncpy(response.data, message, sizeof(response.data) - 1);
    response.data[sizeof(response.data) - 1] = '\0';
  } else {
    response.data[0] = '\0';
  }

  /* Without game_mutex: send to queue just copies the prepared string; the mutex would
   * could deadlock (game_task × HTTP GET holding mutex for JSON) or duplicate
   * nest-recursive lock. Reading the status for the UART is always via the passed `message`. */
  if (xQueueSend(response_queue, &response, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGW(TAG,
             "Failed to send response to UART task (queue full or timeout)");
  } else {
    ESP_LOGI(TAG, "Response sent to UART task: %s",
             is_error ? "ERROR" : "SUCCESS");
  }
}

bool game_cmd_is_squares_origin(const game_cmd_t *cmd) {
  if (cmd == NULL) {
    return false;
  }
  return (cmd->response_queue == NULL &&
          (cmd->kind == GCMD_BOARD_LIFT || cmd->kind == GCMD_BOARD_DROP));
}

bool game_task_squares_guard_mode_conflict_active(void) {
  return ((*game_state_board_setup_tutorial_active()) || (*game_state_puzzle_setup_active()) || (*game_state_puzzle_active()) ||
          game_is_opening_trainer_active() ||
          game_is_opening_trainer_setup_active() || (*game_state_promotion_state()).pending ||
          castling_state.in_progress || castle_animation_active ||
          (*game_state_resignation_state()).active || guided_capture_state.active ||
          game_physical_undo_is_active());
}

void game_task_squares_guard_freeze_move_flow(void) {
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  capture_in_progress = false;
}



/**
 * @brief Short red pulse on an illegal square. The rest of the board stays.
 */
void game_show_invalid_move_error_with_blink(uint8_t error_row,
                                             uint8_t error_col) {
  bool locked = (*game_state_error_recovery_state()).waiting_for_move_correction;
  bool blink = game_policy_error_recovery_led_red_blink();
  bool persist = game_policy_error_recovery_led_red_persist();
  /* A lock without a red square is forbidden, even if blink/persist are off. */
  if (!locked &&
      (!game_policy_error_recovery_enabled() || (!blink && !persist))) {
    return;
  }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_stop_leds();
#endif
  if (locked && !blink && !persist) {
    persist = true;
  }
  game_event_post_move_rejected((uint8_t)(error_row * 8u + error_col), persist);
  ESP_LOGI(TAG, "Error pulse at %c%d", 'a' + error_col, error_row + 1);
}

void game_stop_error_blink(void) {
  /* Pulse only. waiting_for_move_correction stays until a real fix or reset. */
  game_event_post_move_rejected_clear();
  ESP_LOGI(TAG, "Error pulse stopped");
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
    if (game_get_piece(r, c) == winning_king) {
      king_pos = i;
      break;
    }
  }

  ESP_LOGI(TAG, "🏆 Triggering Victory Animation for %s (King at %d)",
           (winner == PLAYER_WHITE) ? "White" : "Black", king_pos);

  game_event_post_game_end(king_pos, (uint8_t)(*game_state_current_result_type()));
}

/**
 * @brief Reset error recovery state completely
 */
void game_reset_error_recovery_state(void) {
  (*game_state_error_recovery_state()).has_invalid_piece = false;
  (*game_state_error_recovery_state()).waiting_for_move_correction = false;
  (*game_state_error_recovery_state()).error_count = 0;
  (*game_state_error_recovery_state()).invalid_row = 0;
  (*game_state_error_recovery_state()).invalid_col = 0;
  (*game_state_error_recovery_state()).original_valid_row = 0;
  (*game_state_error_recovery_state()).original_valid_col = 0;
  (*game_state_error_recovery_state()).piece_type = PIECE_EMPTY;

  ESP_LOGI(TAG, "✅ Error recovery state reset");
}



// ============================================================================

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
bool game_notation_to_coords(const char *notation, uint8_t *row,
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
bool game_coords_to_notation(uint8_t row, uint8_t col, char *notation) {
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
bool game_timer_is_enabled(void) { return timer_enabled; }

/**
 * @brief Named save is not a product path.
 *
 * The live writer is game_snapshot_persist_after_valid_move(). Calling it
 * again from here would be a second writer of the same NVS snapshot, and a
 * named slot does not exist. This function does not set game_saved.
 * @param game_name Requested name. Logged only.
 */
void game_save_game(const char *game_name) {
  ESP_LOGW(TAG,
           "game_save_game('%s') refused: named save is not implemented. "
           "Persistence is the automatic NVS snapshot after a valid move.",
           (game_name != NULL && game_name[0] != '\0') ? game_name : "(none)");
}

/**
 * @brief Named load is not a product path.
 *
 * Boot restore is game_snapshot_restore_on_boot(). Loading a named game
 * mid-session would fight that snapshot.
 * @param game_name Requested name. Logged only.
 */
void game_load_game(const char *game_name) {
  ESP_LOGW(TAG,
           "game_load_game('%s') refused: named load is not implemented. "
           "The NVS snapshot is restored only on boot.",
           (game_name != NULL && game_name[0] != '\0') ? game_name : "(none)");
}

/**
 * @brief Export game to PGN format
 * @param pgn_buffer Output buffer for PGN string
 * @param buffer_size Size of output buffer
 */

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
  ESP_LOGI(TAG, "  Total games: %lu", (*game_state_total_games()));
  ESP_LOGI(TAG, "  White wins: %lu", (*game_state_white_wins()));
  ESP_LOGI(TAG, "  Black wins: %lu", (*game_state_black_wins()));
  ESP_LOGI(TAG, "  Draws: %lu", (*game_state_draws()));
}

// ============================================================================
// MAIN TASK FUNCTION
// ============================================================================

void game_task_start(void *pvParameters) {
  ESP_LOGI(TAG, "Game task started successfully");
  game_policy_log_boot();

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
  ESP_LOGI(TAG, "  • Command queue ready for web, BLE, squares, and main");
  ESP_LOGI(TAG, "  • LED animations ready");
  ESP_LOGI(TAG, "  • 1 second game loop cycle");

  // #2: Initialize promotion mutex for concurrency protection
  // Use a recursive mutex because game_execute_move() can be called
  // reentrantly (eg from a capture flow that calls game_execute_move() which
  // can detect promotion)
  (*game_state_promotion_mutex()) = xSemaphoreCreateRecursiveMutex();
  if ((*game_state_promotion_mutex()) == NULL) {
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
#undef starting_position_check_enabled
    (*game_state_starting_position_check_enabled()) =
        config.starting_position_check_enabled;
#define starting_position_check_enabled (*game_state_starting_position_check_enabled())
    ESP_LOGI(TAG, "Configuration loaded: starting_position_check=%s",
             starting_position_check_enabled ? "ON" : "OFF");
  } else {
    ESP_LOGW(TAG, "Failed to load configuration, using defaults");
    starting_position_check_enabled = false; // Default: disabled
  }

  /*
   * Start the logic game: the board starts, then NVS or boot tracker.
   * - Valid key in NVS: disable forced new game from boot tracker, Nazi snapshot,
   * game_squares_guard_check_resync_after_restore() on matrix mismatch.
   * - Otherwise: boot_new_game_triggered from game_boot_tracker_should_force_new_game();
   * when true, call game_start_new_game() (main will then not send any NEW_GAME).
   * - Set game_load_snapshot_from_nvs() and game_active on successful snapshot loading.
   */
  game_initialize_board();
  game_snapshot_restore_on_boot();

  // Main task loop
  uint32_t loop_count = 0;

  for (;;) {
    // =========================================================================
    // 🛑 BOOT ANIMATION PROTECTION - BLOCKS ALL GAME OPERATIONS DURING BOOT
    // =========================================================================
    // If the boot animation (controlled from main.c) is running, the game task MUST NOT do ANYTHING!
    // Auto new game detection, LED operation, anything could interrupt
    // animaci.
    if (game_led_boot_active()) {
      game_task_wdt_reset_safe();
      /* Hall v2 SW1 (GPIO23). The rest of the game loop stays paused. */
      game_gesture_poll();
      game_led_flush(); vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    game_snapshot_service_after_boot();
    // =========================================================================

    // CRITICAL: Reset watchdog for game task in every iteration (only if
    // registered)
    esp_err_t wdt_ret = game_task_wdt_reset_safe();
    if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
      // Task not registered with TWDT yet - this is normal during startup
    }

    // Process game commands
    game_process_commands();

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    if ((*game_state_promotion_state()).pending) {
      game_promotion_sync_hall_if_pending();
    }
    game_ep_capture_service();
    if (current_game_state == GAME_STATE_IDLE ||
        current_game_state == GAME_STATE_ACTIVE ||
        current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP ||
        current_game_state == GAME_STATE_FINISHED ||
        current_game_state == GAME_STATE_WAITING_FOR_RETURN) {
      static uint32_t s_last_hall_restore_sync_ms = 0;
      uint32_t hall_now_ms = (uint32_t)(esp_timer_get_time() / 1000);
      if (game_hall_restore_guide_take_sync_request()) {
        game_hall_restore_guide_sync();
      } else if (hall_now_ms - s_last_hall_restore_sync_ms >= 400U) {
        s_last_hall_restore_sync_ms = hall_now_ms;
        game_hall_restore_guide_sync();
      }
    }
    game_hall_position_service();
    game_puzzle_service_hall_move();
    game_opening_service_hall_move();
#endif
    game_required_step_service();

    // BUG FIX 1: Automatic transition from WAITING_FOR_BOARD_SETUP to ACTIVE
    if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
      // If monitoring of the initial position is switched off, we go directly to ACTIVE
      bool should_check_position = starting_position_check_enabled;
      bool position_ok = should_check_position ? game_is_board_in_starting_position() : true;
      bool sensors_ready = game_snapshot_sensors_ready();
      
      if (sensors_ready && position_ok) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        game_force_clear_squares_guard();
#endif
        // The board is ready (or guarding is off) - activate the game
        current_game_state = GAME_STATE_ACTIVE;
        game_active = true;
        (*game_state_game_start_time()) = esp_timer_get_time() / 1000;
        (*game_state_last_move_time()) = (*game_state_game_start_time());
        ha_game_report(HA_GAME_NEW, HA_REASON_NONE);
        
        // Stop any running animations
        unified_animation_stop_all();
        game_event_post_led_ctrl(GAME_EVT_CTRL_STOP_ENDGAME, 0, 0, 0, 0);
        stop_endgame_animation();
        
        // Clear the LED
        game_led_clear_board();
        
        // Update the promotion LED indication
        game_check_promotion_needed();
        
        // Hall starts the clock when the opening guide hands over.
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        game_hall_opening_clock_on_play();
#else
        if (game_is_timer_active()) {
          game_start_timer_move(true);
        }
#endif
        
        // Highlight moving figures
        game_policy_highlight_movable_if_enabled();
        
        // Notify web
        game_bump_revision_and_notify();
        
        if (should_check_position) {
          ESP_LOGI(TAG, "✅ Board setup complete - game now ACTIVE, White to move");
        } else {
          ESP_LOGI(TAG, "✅ Game ACTIVE (starting position check disabled), White to move");
        }
      } else if (sensors_ready) {
        // Periodically update the LED indication of missing figures
        static uint32_t last_led_update = 0;
        uint32_t now = esp_timer_get_time() / 1000;
        if (now - last_led_update > 500 && !game_demo_show_active()) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
          game_squares_guard_check_resync_after_restore();
          if (game_is_squares_guard_active()) {
            game_squares_guard_render_leds();
          } else {
            game_show_missing_pieces_led();
          }
#else
          game_show_missing_pieces_led();
#endif
          last_led_update = now;
        }
      }
    }

    // Puzzle setup / drift, and the short red/green board flash.
    game_board_flash_poll();
    {
      static uint32_t puzzle_led_ms = 0;
      uint32_t puzzle_now_ms = (uint32_t)(esp_timer_get_time() / 1000);
      if (puzzle_now_ms - puzzle_led_ms >= 400U) {
        puzzle_led_ms = puzzle_now_ms;
        if (game_mode_current() != NULL) {
          game_mode_call_leds(game_logic_position());
        } else {
          game_puzzle_service_leds();
        }
      }
    }

    // Update timer display and check for timeout
    game_update_timer_display();

    // Process endgame report request (with enough stack 10KB)
    if ((*game_state_endgame_report_requested())) {
      (*game_state_endgame_report_requested()) = false; // Reset flag
      game_print_endgame_report_uart((*game_state_current_result_type()));
    }

    // Auto new game detection (when pieces are in initial position 2s)
    // Logic update: Must not be blocked (require 1 move)
    // BUG FIX 1: Auto-new game must not hit during WAITING_FOR_BOARD_SETUP
    // BUG FIX 2: Auto-new game only when starting position watch is on
    bool in_start_pos =
        starting_position_check_enabled &&
        !(*game_state_board_setup_tutorial_active()) &&
        !game_puzzle_blocks_new_game() &&
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
        game_cmd_t cmd = {0};
        cmd.kind = GCMD_NEW_GAME;
        if (!game_submit(&cmd)) {
          ESP_LOGW(TAG, "Auto new game submit failed");
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
  }
}




