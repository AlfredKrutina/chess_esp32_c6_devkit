/**
 * @file chess_types.h
 * @brief ESP32-C6 Chess System v1.8.0 - Common type definition
 *
 * This header contains all common definitions of the used type
 * in the Sacha system. Prevents circular dependencies and provides
 * consistent type definitions across all components.
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * This file defines all basic data types used in storage
 * system:
 * - Chess piece types (piece_t)
 * - Game states (game_state_t)
 * - Players (player_t)
 * - Move and error types (move_error_t)
 * - Structures for moves (chess_move_t, chess_move_extended_t)
 * - Commands and responses (game_command_type_t, game_response_t)
 * - LED commands and events
 * - Button and event matrix
 * - System configuration
 */

#ifndef CHESS_TYPES_H
#define CHESS_TYPES_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DEFINITION OF SACH'S PIECES
// ============================================================================

/**
 * @brief Chess figure types
 *
 * Enumeration defining all types of chess pieces for both suits.
 * The value 0 means an empty field.
 */
typedef enum {
  PIECE_EMPTY = 0, ///< Empty field (back figure)
  // Bile figurines
  PIECE_WHITE_PAWN = 1,   ///< Bily pesec
  PIECE_WHITE_KNIGHT = 2, ///< Bily kun
  PIECE_WHITE_BISHOP = 3, ///< Bily strelec
  PIECE_WHITE_ROOK = 4,   ///< Bila vez
  PIECE_WHITE_QUEEN = 5,  ///< Bila dama
  PIECE_WHITE_KING = 6,   ///< They were kings
  // Black figurines
  PIECE_BLACK_PAWN = 7,   ///< Cerny pesec
  PIECE_BLACK_KNIGHT = 8, ///< Cerny kun
  PIECE_BLACK_BISHOP = 9, ///< Cerny strelec
  PIECE_BLACK_ROOK = 10,  ///< Cerna vez
  PIECE_BLACK_QUEEN = 11, ///< Cerna dama
  PIECE_BLACK_KING = 12   ///< Black King
} piece_t;

// ============================================================================
// GAME STATE DEFINITION
// ============================================================================

/**
 * @brief Sacha game states
 *
 * An enumeration defining all possible chess game states since initialization
 * and until the end of the game and error handling.
 */
typedef enum {
  GAME_STATE_IDLE = 0,              ///< Idle state (before initialization)
  GAME_STATE_INIT = 1,              ///< Initialize the game
  GAME_STATE_ACTIVE = 2,            ///< Active game
  GAME_STATE_PAUSED = 3,            ///< Pozastavena hra
  GAME_STATE_FINISHED = 4,          ///< Ukoncena hra
  GAME_STATE_ERROR = 5,             ///< Error condition
  GAME_STATE_PLAYING = 6,           ///< Probihajici hra
  GAME_STATE_PROMOTION = 7,         ///< Waiting for selection of graduation sand
  GAME_STATE_ERROR_RECOVERY = 8,    ///< Error recovery (new state)
  GAME_STATE_WAITING_FOR_RETURN = 9, ///< Waiting for the figurine to be returned to its place
  GAME_STATE_WAITING_FOR_BOARD_SETUP = 10 ///< Waiting for the physical placement of the figures
} game_state_t;

/**
 * @brief Definice hracu
 *
 * Enumerace definujici obe barvy hracu v sach.
 */
typedef enum {
  PLAYER_WHITE = 0, ///< They were toys
  PLAYER_BLACK = 1  ///< Black player
} player_t;

/**
 * @brief Game result types for stats
 *
 * An enumeration defining all possible outcomes of the game, including a draw.
 */
typedef enum {
  RESULT_WHITE_WINS = 0,       ///< Bily vitezi
  RESULT_BLACK_WINS = 1,       ///< Cerny vitezi
  RESULT_DRAW_STALEMATE = 2,   ///< Remiza - pat (stalemate)
  RESULT_DRAW_50_MOVE = 3,     ///< Tie - 50 turn rule
  RESULT_DRAW_REPETITION = 4,  ///< Draw - repeat position
  RESULT_DRAW_INSUFFICIENT = 5 ///< Remiza - nedostatecny material
} game_result_type_t;

/**
 * @brief Pull error types
 *
 * Enumeration defining all possible types of errors when performing a move.
 * Used for detailed error handling and displaying player predictions.
 */
typedef enum {
  MOVE_ERROR_NONE = 0,              ///< No error
  MOVE_ERROR_INVALID_SYNTAX = 1,    ///< Invalid stroke syntax
  MOVE_ERROR_INVALID_PARAMETER = 2, ///< Neplatny parameter
  MOVE_ERROR_PIECE_NOT_FOUND = 3,   ///< Figure not found
  MOVE_ERROR_INVALID_MOVE = 4,      ///< Neplatny tah
  MOVE_ERROR_BLOCKED_PATH = 5,      ///< Blokovana cesta
  MOVE_ERROR_CHECK_VIOLATION = 6,   ///< Tah by nechal krale v sachu
  MOVE_ERROR_SYSTEM_ERROR = 7,      ///< System error
  // Other error types needed in game_task.c
  MOVE_ERROR_NO_PIECE = 8,            ///< There is no figure on the source field
  MOVE_ERROR_WRONG_COLOR = 9,         ///< Wrong piece color (not on turn)
  MOVE_ERROR_INVALID_PATTERN = 10,    ///< Neplatny vzor pohybu
  MOVE_ERROR_KING_IN_CHECK = 11,      ///< The king is in the sack
  MOVE_ERROR_CASTLING_BLOCKED = 12,   ///< Castling is blocked
  MOVE_ERROR_EN_PASSANT_INVALID = 13, ///< Neplatny en passant
  MOVE_ERROR_DESTINATION_OCCUPIED =
      14,                          ///< Target field is occupied by own piece
  MOVE_ERROR_OUT_OF_BOUNDS = 15,   ///< Coordinates outside the box
  MOVE_ERROR_GAME_NOT_ACTIVE = 16, ///< The game is not active
  MOVE_ERROR_INVALID_MOVE_STRUCTURE = 17, ///< Invalid stroke structure
  MOVE_ERROR_INVALID_COORDINATES = 18,    ///< Neplatne souradnice
  MOVE_ERROR_ILLEGAL_MOVE = 19            ///< Nelegalni tah
} move_error_t;

// ============================================================================
// DEFINITION OF GRADUATION
// ============================================================================

/**
 * @brief Pescu graduation choice types
 *
 * When the sandbox reaches the end of the box, the player can choose
 * which figurine the dog wants to graduate to.
 */
typedef enum {
  PROMOTION_QUEEN = 0,  ///< Promovat na damu
  PROMOTION_ROOK = 1,   ///< Promovat na vez
  PROMOTION_BISHOP = 2, ///< Promovat na strelce
  PROMOTION_KNIGHT = 3  ///< Promovat na kone
} promotion_choice_t;

// ============================================================================
// STRUCTURES FOR SACH MOVES
// ============================================================================

/**
 * @brief The basic structure of a sach move
 *
 * Contains minimal information about the move - source and target coordinates,
 * figure and any collected figure.
 */
typedef struct {
  uint8_t from_row;       ///< Zdrojovy radek (0-7)
  uint8_t from_col;       ///< Zdrojovy sloupec (0-7)
  uint8_t to_row;         ///< Cilovy radek (0-7)
  uint8_t to_col;         ///< Cilovy sloupec (0-7)
  piece_t piece;          ///< A figure that moves
  piece_t captured_piece; ///< Collected figure (PIECE_EMPTY if there is none)
  uint32_t timestamp;     ///< Stroke timestamp (in milliseconds)
} chess_move_t;

/**
 * @brief Move types for extended sach logic
 *
 * Defines special types of moves in chess (castling, en passant, promotion).
 */
typedef enum {
  MOVE_TYPE_NORMAL = 0,       ///< Normalni tah
  MOVE_TYPE_CAPTURE = 1,      ///< Figures collected
  MOVE_TYPE_CASTLE_KING = 2,  ///< Kingside castling
  MOVE_TYPE_CASTLE_QUEEN = 3, ///< Queenside
  MOVE_TYPE_EN_PASSANT = 4,   ///< En passant
  MOVE_TYPE_PROMOTION = 5     ///< Graduation pesc
} move_type_t;

/**
 * @brief Expanded move structure for complete chess logic
 *
 * Contains all information about the move including special features
 * such as graduation, chess, checkmate and pat.
 */
typedef struct {
  uint8_t from_row;       ///< Zdrojovy radek (0-7)
  uint8_t from_col;       ///< Zdrojovy sloupec (0-7)
  uint8_t to_row;         ///< Cilovy radek (0-7)
  uint8_t to_col;         ///< Cilovy sloupec (0-7)
  piece_t piece;          ///< A figure that moves
  piece_t captured_piece; ///< Figure collected
  move_type_t move_type;  ///< Move type (normal, capture, castle, ...)
  promotion_choice_t
      promotion_piece; ///< Graduation figure (if graduation)
  uint32_t timestamp;  ///< Timestamp of stroke
  bool is_check;       ///< Je to sach?
  bool is_checkmate;   ///< Je to mat?
  bool is_stalemate;   ///< Je to pat?
} chess_move_extended_t;

// ============================================================================
// GAME COMMAND DEFINITION
// ============================================================================

/**
 * @brief Game command types for UART communication
 *
 * A complete list of all commands that can be sent to the game task via the queue.
 * Used for game control, debugging, testing and system administration.
 */
typedef enum {
  GAME_CMD_NEW_GAME = 0,        ///< New game; board and NVS key reset (see main initialize_chess_game)
  GAME_CMD_RESET_GAME = 1,      ///< Reset hry (vrati se na zacatek)
  GAME_CMD_MAKE_MOVE = 2,       ///< Perform move (move e2e4)
  GAME_CMD_UNDO_MOVE = 3,       ///< Vrat zpet posledni tah (undo)
  GAME_CMD_GET_STATUS = 4,      ///< Get game state (status)
  GAME_CMD_GET_BOARD = 5,       ///< Get current position (board)
  GAME_CMD_GET_VALID_MOVES = 6, ///< Get valid moves for a figure
  GAME_CMD_PICKUP_PIECE = 7,    ///< Pickup
  GAME_CMD_DROP_PIECE = 8,      ///< Drop
  GAME_CMD_PROMOTION = 9,       ///< Promoc pescu (promotion)
  GAME_CMD_MOVE = 10,           ///< Tah (move)
  GAME_CMD_SHOW_BOARD = 11,     ///< Display the board
  GAME_CMD_PICKUP = 12,         ///< UP command - figure raised
  GAME_CMD_DROP = 13,           ///< DN display - figure placed
  GAME_CMD_GET_HISTORY = 14,    ///< Get move history
  GAME_CMD_DEBUG_INFO = 15,     ///< Debug informace o hre
  GAME_CMD_DEBUG_BOARD = 16,    ///< Debug inbox information

  // High priority commands
  GAME_CMD_EVALUATE = 17,         ///< Position evaluation (evaluated)
  GAME_CMD_SAVE = 18,             ///< Save the game to memory
  GAME_CMD_LOAD = 19,             ///< Load the game from memory
  GAME_CMD_RESERVED_SLOT_20 = 20, ///< Rezervovano (legacy slot)

  // Medium priority commands
  GAME_CMD_CASTLE = 21,  ///< Castling (castle)
  GAME_CMD_PROMOTE = 22, ///< Promotion of pesca (promote)

  // Commands for controlling components
  GAME_CMD_COMPONENT_OFF = 23, ///< Vypni komponentu (off)
  GAME_CMD_COMPONENT_ON = 24,  ///< Zapni komponentu (on)

  // Endgame commands
  GAME_CMD_ENDGAME_WHITE = 25, ///< Konec hry - bily vitezi
  GAME_CMD_ENDGAME_BLACK = 26, ///< Konec hry - cerny vitezi

  // Game management commands
  GAME_CMD_LIST_GAMES = 27,  ///< List of saved games
  GAME_CMD_DELETE_GAME = 28, ///< Delete saved game

  // Reserved commands (legacies)
  GAME_CMD_RESERVED_SLOT_29 = 29, ///< Rezervovano (legacy slot)
  GAME_CMD_RESERVED_SLOT_30 = 30, ///< Rezervovano (legacy slot)
  GAME_CMD_RESERVED_SLOT_31 = 31, ///< Rezervovano (legacy slot)
  GAME_CMD_RESERVED_SLOT_32 = 32, ///< Rezervovano (legacy slot)

  // Commands for testing animation
  GAME_CMD_TEST_MOVE_ANIM = 33,    ///< Pull animation test
  GAME_CMD_TEST_PLAYER_ANIM = 34,  ///< Game change animation test
  GAME_CMD_TEST_CASTLE_ANIM = 35,  ///< Test animace rosady
  GAME_CMD_TEST_PROMOTE_ANIM = 36, ///< Graduation animation test
  GAME_CMD_TEST_ENDGAME_ANIM = 37, ///< Test animace konce hry
  GAME_CMD_RESERVED_SLOT_38 = 38,  ///< Rezervovano (legacy slot)

  // Commands for the time system
  GAME_CMD_SET_TIME_CONTROL = 39, ///< Nastav casovou kontrolu
  GAME_CMD_PAUSE_TIMER = 40,      ///< Pozastav timer
  GAME_CMD_RESUME_TIMER = 41,     ///< Obnov timer
  GAME_CMD_RESET_TIMER = 42,      ///< Resetuj timer
  GAME_CMD_GET_TIMER_STATE = 43,  ///< Ziskej stav timeru
  GAME_CMD_TIMER_TIMEOUT = 44,    ///< Vyprseni casoveho limitu
  GAME_CMD_MATRIX_GUARD = 45,     ///< Guard rezim pro multi-lift anomalii
  GAME_CMD_BOARD_SETUP_TUTORIAL =
      46, ///< Web tutorial setup; promotion_choice: 0=start,1=cancel,2=finish
  GAME_CMD_PUZZLE =
      47, ///< Web puzzle: 0=cancel,1..5=start,101..105=prepare id 1..5
  GAME_CMD_NEW_GAME_FROM_FEN =
      48, ///< New game from FEN (placement + page); data in timer_data.fen_new_game
  GAME_CMD_OPENING_TRAINER =
      49 ///< Opening trainer: promotion_choice 0=cancel,1=start,2=hint,3=checkpoint_ack
} game_command_type_t;

/**
 * @brief Command structure for sach move via UART
 *
 * This structure is used to send the game task command through the queue.
 * Contains all the necessary information to perform the move and process the response.
 */
typedef struct {
  uint8_t type;          ///< Command type (game_command_type_t)
  char from_notation[8]; ///< Zdrojova notace (napr. "e2") - zvetseno z 4 na 8
                         ///< for security
  char to_notation[8];   ///< Cilova notace (napr. "e4") - zvetseno z 4 na 8 pro
                         ///< security
  uint8_t player; ///< Player performing the command (PLAYER_WHITE or PLAYER_BLACK)
  QueueHandle_t response_queue; ///< Queue for sending responses
  uint8_t promotion_choice;     ///< Choice of promotion (for promotion commands)
  uint8_t promotion_from_remote; ///< 1 = WEB/BLE poslalo pole promotion u GAME_CMD_MOVE

  // Pole pro timer system
  union {
    struct {
      uint8_t time_control_type; ///< Typ casove kontroly (pro
                                 ///< GAME_CMD_SET_TIME_CONTROL)
      uint32_t
          custom_minutes; ///< Vlastni minuty (pro vlastni casovou kontrolu)
      uint32_t custom_increment; ///< Vlastni increment (pro vlastni casovou
                                 ///< kontrolu)
    } timer_config;              ///< Konfigurace timeru
    struct {
      bool is_white_turn; ///< Is the ball on the move? (for timer operations)
    } timer_state;        ///< Stav timeru
    struct {
      uint32_t lifted_mask_low;  ///< Bits 0-31 for raised fields
      uint32_t lifted_mask_high; ///< Bits 32-63 for raised fields
      uint32_t dropped_mask_low; ///< Bits 0-31 for array positions
      uint32_t dropped_mask_high; ///< Bits 32-63 for array positions
      uint8_t action;            ///< 1=enter/update, 0=clear
    } matrix_guard;              ///< Data pro matrix guard rezim
    struct {
      char fen[120]; ///< FEN pro GAME_CMD_NEW_GAME_FROM_FEN (placement + w/b)
    } fen_new_game;
  } timer_data;           ///< Union pro timer data
  bool is_demo_mode;      ///< Flag pro demo mode (skip resignation timer)
} chess_move_command_t;

/**
 * @brief Game response types
 *
 * Defines the types of responses that the game task sends back via UART.
 */
typedef enum {
  GAME_RESPONSE_SUCCESS = 0,     ///< Done successfully
  GAME_RESPONSE_ERROR = 1,       ///< Execution error
  GAME_RESPONSE_BOARD = 2,       ///< The response contains a mailbox
  GAME_RESPONSE_MOVES = 3,       ///< The response contains the move list
  GAME_RESPONSE_STATUS = 4,      ///< The response contains the state of the game
  GAME_RESPONSE_HISTORY = 5,     ///< The response contains the history of the move
  GAME_RESPONSE_MOVE_RESULT = 6, ///< The response contains the result of the move
  GAME_RESPONSE_LED_STATUS = 7   ///< The response contains the LED status
} game_response_type_t;

/**
 * @brief Game response structure for UART communication
 *
 * This structure contains the response of the game task to the command.
 * It is sent back via UART for display by the user.
 */
typedef struct {
  uint8_t type;         ///< Response Type (game_response_type_t)
  uint8_t command_type; ///< Original command type (game_command_type_t)
  uint8_t error_code;   ///< Error code (move_error_t, 0 if no error)
  char message[256];    ///< Message for users (human-readable)
  char data[64];        ///< Response data (reduced from 3584 to 64 bytes for
                        ///< memory optimization)
  uint32_t timestamp;   ///< Response timestamp (in milliseconds)
} game_response_t;

// ============================================================================
// DEFINICE LED SYSTEMU
// ============================================================================

/**
 * @brief LED display types
 *
 * Complete list of all commands for controlling the LED system.
 * Contains basic commands, animations, error handling and advanced
 * sach animations. Some legacy values ​​are reserved.
 */
typedef enum {
  LED_CMD_SET_PIXEL = 0,       ///< Nastav barvu jedne LED
  LED_CMD_SET_ALL = 1,         ///< Set all LEDs to the same color
  LED_CMD_CLEAR = 2,           ///< Clear all LEDs
  LED_CMD_SHOW_BOARD = 3,      ///< Show inbox
  LED_CMD_BUTTON_FEEDBACK = 4, ///< Button feedback (availability of the button)
  LED_CMD_BUTTON_PRESS = 5,    ///< Button pressed
  LED_CMD_BUTTON_RELEASE = 6,  ///< Button released
  LED_CMD_ANIMATION = 7,       ///< Start animation
  LED_CMD_TEST = 8,            ///< Test pattern
  LED_CMD_TEST_ALL = 10,       ///< Test all LEDs (progressive lighting)
  LED_CMD_MATRIX_OFF = 11,     ///< Turn off LED effects when scanning the matrix
  LED_CMD_MATRIX_ON = 12,      ///< Turn on LED effects when scanning the matrix

  // Reserved legacy animation commands
  LED_CMD_RESERVED_SLOT_13 = 13, ///< Rezervovano (legacy slot)
  LED_CMD_RESERVED_SLOT_14 = 14, ///< Rezervovano (legacy slot)
  LED_CMD_RESERVED_SLOT_15 = 15, ///< Rezervovano (legacy slot)
  LED_CMD_RESERVED_SLOT_16 = 16, ///< Rezervovano (legacy slot)
  LED_CMD_RESERVED_SLOT_17 = 17, ///< Rezervovano (legacy slot)
  LED_CMD_RESERVED_SLOT_18 = 18, ///< Rezervovano (legacy slot)

  // Advanced sach animations
  LED_CMD_ANIM_PLAYER_CHANGE = 19, ///< Player change animation (beams)
  LED_CMD_ANIM_MOVE_PATH = 20,     ///< Stroke path animation
  LED_CMD_ANIM_CASTLE = 21,        ///< Animace rosady
  LED_CMD_ANIM_PROMOTE = 22,       ///< Graduation animation
  LED_CMD_ANIM_ENDGAME = 23,       ///< Animace konce hry (vlny)
  LED_CMD_ANIM_CHECK = 24,         ///< Animace sachu
  LED_CMD_ANIM_CHECKMATE = 25,     ///< Animace matu
  LED_CMD_RESERVED_SLOT_26 = 26,   ///< Rezervovano (legacy slot)

  // Commands for controlling components
  LED_CMD_DISABLE = 25, ///< Vypni LED komponentu
  LED_CMD_ENABLE = 26,  ///< Zapni LED komponentu

  // Commands for LED button logic
  LED_CMD_BUTTON_PROMOTION_AVAILABLE =
      27, ///< Set power key as available
  LED_CMD_BUTTON_PROMOTION_UNAVAILABLE =
      28,                          ///< Set power key as unavailable
  LED_CMD_BUTTON_SET_PRESSED = 29, ///< Set button as pressed
  LED_CMD_BUTTON_SET_RELEASED = 30, ///< Set button as released

  // Commands to integrate with game state
  LED_CMD_GAME_STATE_UPDATE = 31, ///< Update the LED according to the current state of the game
  LED_CMD_HIGHLIGHT_PIECES = 32,  ///< Highlight figures that can move
  LED_CMD_HIGHLIGHT_MOVES = 33,   ///< Highlight the possible moves for the selected piece
  LED_CMD_CLEAR_HIGHLIGHTS = 34,  ///< Clear all highlights
  LED_CMD_PLAYER_CHANGE = 35,     ///< Game change animation

  // Error handling commands
  LED_CMD_ERROR_INVALID_MOVE = 36, ///< Show invalid move error
  LED_CMD_ERROR_RETURN_PIECE = 37, ///< Prompt the user to return the figure
  LED_CMD_ERROR_RECOVERY = 38,     ///< Recovery from an error state
  LED_CMD_SHOW_LEGAL_MOVES =
      39, ///< Display all legal moves for a piece type

  // Commands of the Enhanced Casting system
  LED_CMD_CASTLING_GUIDANCE =
      40,                      ///< Show instructions for the rook (king/ rook position)
  LED_CMD_CASTLING_ERROR = 41, ///< Show castling error indication
  LED_CMD_CASTLING_CELEBRATION = 42, ///< Show castling completion celebration
  LED_CMD_CASTLING_TUTORIAL = 43,    ///< Show castling tutorial
  LED_CMD_CASTLING_CLEAR = 44,       ///< Clear all dew indications
  LED_CMD_HIGHLIGHT_HINT = 45,       ///< Zvyrazni napovedu (odkud/kam) - led_index=from, data=(uint8_t*)to_index

  LED_CMD_STATUS_ACTIVE = 97,   ///< Status display - active LED
  LED_CMD_STATUS_COMPACT = 98,  ///< Status display - compact output
  LED_CMD_STATUS_DETAILED = 99, ///< Status display - detailed output
  LED_CMD_SET_BRIGHTNESS = 100  ///< Set global brightness (0-100)
} led_command_type_t;

/**
 * @brief The structure of the LED command
 *
 * This structure contains all the information necessary for execution
 * LED display (color, index, duration, etc.).
 */
typedef struct {
  led_command_type_t type;      ///< LED display type
  uint8_t led_index;            ///< Index LED (0-72)
  uint8_t red;                  ///< Red component (0-255)
  uint8_t green;                ///< Green component (0-255)
  uint8_t blue;                 ///< Blue component (0-255)
  uint32_t duration_ms;         ///< Duration of the effect in milliseconds
  void *data;                   ///< Additional data specific to the command
  QueueHandle_t response_queue; ///< Queue for sending responses
} led_command_t;

// ============================================================================
// DEFINICE BUTTON SYSTEMU
// ============================================================================

/**
 * @brief Button event types
 *
 * Defines all types of events that can occur when using buttons.
 */
typedef enum {
  BUTTON_EVENT_PRESS = 0,       ///< Button pressed
  BUTTON_EVENT_RELEASE = 1,     ///< Button released
  BUTTON_EVENT_LONG_PRESS = 2,  ///< Dlouhe stisknuti (vice nez 1 sekunda)
  BUTTON_EVENT_DOUBLE_PRESS = 3 ///< Dvojite stisknuti (do 300ms)
} button_event_type_t;

/**
 * @brief The button event structure
 *
 * Contains information about the button event (type, button ID, time).
 */
typedef struct {
  button_event_type_t
      type; ///< Event type (press, release, long press, double press)
  uint8_t button_id;          ///< Button ID (0-8)
  uint32_t press_duration_ms; ///< Press time in milliseconds
  uint32_t timestamp;         ///< Event timestamp
} button_event_t;

// ============================================================================
// DEFINICE MATRIX SYSTEMU
// ============================================================================

/**
 * @brief Event matrix types
 *
 * Defines all types of events that can be generated by the matrix task
 * when detecting the movement of figures.
 */
typedef enum {
  MATRIX_EVENT_PIECE_LIFTED = 0,  ///< Figure raised (reed switch open)
  MATRIX_EVENT_PIECE_PLACED = 1,  ///< Figure placed (reed switch on)
  MATRIX_EVENT_MOVE_DETECTED = 2, ///< Complete move detected (lift + place)
  MATRIX_EVENT_ERROR = 3          ///< Detection error
} matrix_event_type_t;

/**
 * @brief Event matrix structure
 *
 * Contains information about the event matrix (type, position, piece).
 */
typedef struct {
  matrix_event_type_t type; ///< Event type (lifted, placed, move detected)
  uint8_t from_square;      ///< Zdrojove pole (0-63)
  uint8_t to_square;        ///< Cilove pole (0-63)
  piece_t piece_type;       ///< Figure type
  uint32_t timestamp;       ///< Event timestamp
  uint8_t from_row;         ///< Zdrojovy radek (0-7)
  uint8_t from_col;         ///< Zdrojovy sloupec (0-7)
  uint8_t to_row;           ///< Cilovy radek (0-7)
  uint8_t to_col;           ///< Cilovy sloupec (0-7)
} matrix_event_t;

/**
 * @brief Matrix command types
 *
 * Defines the commands that can be sent to the matrix task.
 */
typedef enum {
  MATRIX_CMD_SCAN = 0,      ///< Perform matrix scan
  MATRIX_CMD_RESET = 1,     ///< Reset matrix state
  MATRIX_CMD_TEST = 2,      ///< Testuj matrix funkci
  MATRIX_CMD_CALIBRATE = 3, ///< Calibrate matrix (sensitivity settings)
  MATRIX_CMD_DISABLE = 4,   ///< Turn off matrix scanning
  MATRIX_CMD_ENABLE = 5     ///< Enable matrix scanning
} matrix_command_type_t;

/**
 * @brief Structure of matrix command
 *
 * Contains command information for the matrix task.
 */
typedef struct {
  matrix_command_type_t type; ///< Command type
  uint8_t data[16];           ///< Additional data for the command
} matrix_command_t;

// ============================================================================
// HARDWAROVE KONSTANTY
// ============================================================================

// LED systemove konstanty
/** @brief Number of LEDs for the box (8x8 = 64) */
#define CHESS_LED_COUNT_BOARD 64
/** @brief Number of LEDs for buttons (8 promotion + 1 reset = 9) */
#define CHESS_LED_COUNT_BUTTONS 9
/** @brief Total number of LEDs (64 boxes + 9 buttons = 73) */
#define CHESS_LED_COUNT_TOTAL (CHESS_LED_COUNT_BOARD + CHESS_LED_COUNT_BUTTONS)
/** @brief Celkovy pocet LED v systemu (alias pro CHESS_LED_COUNT_TOTAL) */
#define CHESS_LED_COUNT 73 // Total number of LEDs (64 board + 9 buttons)

// Button systemove konstanty
/** @brief Number of buttons in the system (8 promotion + 1 reset = 9) */
#define CHESS_BUTTON_COUNT 9

// Matrix systemove konstanty
/** @brief Velikost matrix (8x8 = 64 poli) */
#define CHESS_MATRIX_SIZE 64

// Game konstanty
/** @brief Maximum number of moves in history (200 moves) */
#define MAX_MOVE_HISTORY 200

/**
 * @brief Structure of draft draft for parsing
 *
 * This structure contains information about the proposed move, including early
 * and special features (capture, check, castling, en passant).
 */
typedef struct {
  uint8_t from_row;   ///< Zdrojovy radek (0-7)
  uint8_t from_col;   ///< Zdrojovy sloupec (0-7)
  uint8_t to_row;     ///< Cilovy radek (0-7)
  uint8_t to_col;     ///< Cilovy sloupec (0-7)
  piece_t piece;      ///< A figure that moves
  bool is_capture;    ///< Je to sebrani?
  bool is_check;      ///< Vyvola to sach?
  bool is_castling;   ///< Is it dew?
  bool is_en_passant; ///< Je to en passant?
  int score;          ///< Move speed (for AI evaluation, higher = better)
} move_suggestion_t;

// ============================================================================
// DEKLARACE GAME UTILITY FUNKCI
// ============================================================================

// Predni deklarace utility funkci pro game task
/**
 * @brief Verify if the position is valid on the inbox
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @return true if position is valid
 */
bool game_is_valid_square(int row, int col);

/**
 * @brief Check if the figure is your own (belongs to the current player)
 * @param piece The piece to verify
 * @param player The player
 * @return true if the figure is owned
 */
bool game_is_own_piece(piece_t piece, player_t player);

/**
 * @brief Check if the figure is unfriendly
 * @param piece The piece to verify
 * @param player The player
 * @return true if the figure is enemy
 */
bool game_is_enemy_piece(piece_t piece, player_t player);

/**
 * @brief Simulates the move and checks if it would leave the king in check
 * @param move Move to simulate
 * @param player The player
 * @return true if the move would leave the king in check
 */
bool game_simulate_move_check(chess_move_extended_t *move, player_t player);

// ============================================================================
// DEFINICE SYSTEMOVE KONFIGURACE
// ============================================================================

/**
 * @brief System configuration structure
 *
 * Contains all system settings that can be saved to NVS flash.
 */
typedef struct {
  bool verbose_mode; ///< Podrobny logovaci rezim (detailni vypisy)
  bool quiet_mode;   ///< Silent mode (minimum output)
  bool guided_capture_hints_enabled; ///< LED help guided capture
  /** In-game LED help level: 1-5 (5 = full). Controls NVS key led_guide_lvl. */
  uint8_t led_guidance_level;
  uint8_t log_level; ///< Uroven logovania (ESP_LOG_ERROR, ESP_LOG_INFO, atd.)
  uint32_t command_timeout_ms; ///< Command timeout in milliseconds
  uint8_t brightness_level;    ///< Global LED brightness (0-100%)
  bool starting_position_check_enabled; ///< Start position monitoring (off by default)
} system_config_t;

// Predni deklarace konfiguracnich funkci
/**
 * @brief Initializes the config manager
 * @return ESP_OK on success
 */
esp_err_t config_manager_init(void);

/**
 * @brief Load configuration from NVS flash
 * @param[out] config Pointer to a structure to load the configuration
 * @return ESP_OK on success
 */
esp_err_t config_load_from_nvs(system_config_t *config);

/**
 * @brief Save configuration to NVS flash
 * @param config Configuration to save
 * @return ESP_OK on success
 */
esp_err_t config_save_to_nvs(const system_config_t *config);

/**
 * @brief Applies configuration settings to system
 * @param config Configuration for the application
 * @return ESP_OK on success
 */
esp_err_t config_apply_settings(const system_config_t *config);

/**
 * @brief Stav animace pro LED efekty
 *
 * Obsahuje informace o probiha jici LED animaci (typ, snimky, barvy, atd.).
 */
typedef struct {
  bool is_active;                    ///< Is the animation active?
  uint8_t animation_type;            ///< Typ animace
  uint8_t current_frame;             ///< Current animation frame
  uint8_t total_frames;              ///< Celkovy pocet snimku
  uint32_t frame_duration_ms;        ///< Doba trvani jednoho snimku v ms
  uint32_t last_update_time;         ///< Timestamp of last update
  uint8_t source_square;             ///< Zdrojove pole (0-63)
  uint8_t target_square;             ///< Cilove pole (0-63)
  uint8_t color_r, color_g, color_b; ///< Barvy animace (RGB)
  bool interrupt_on_placement;       ///< Stop animation when placing figure?
} led_animation_state_t;

#ifdef __cplusplus
}
#endif

#endif // CHESS_TYPES_H
