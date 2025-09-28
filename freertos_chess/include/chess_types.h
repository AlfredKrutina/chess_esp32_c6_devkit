/**
 * @file chess_types.h
 * @brief ESP32-C6 Chess System v2.4 - Common Type Definitions
 * 
 * This header contains all common type definitions used throughout
 * the chess system to avoid circular dependencies and provide
 * consistent type definitions across all components.
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
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
// CHESS PIECE DEFINITIONS
// ============================================================================

/**
 * @brief Chess piece types
 */
typedef enum {
    PIECE_EMPTY = 0,
    // White pieces
    PIECE_WHITE_PAWN = 1,
    PIECE_WHITE_KNIGHT = 2,
    PIECE_WHITE_BISHOP = 3,
    PIECE_WHITE_ROOK = 4,
    PIECE_WHITE_QUEEN = 5,
    PIECE_WHITE_KING = 6,
    // Black pieces  
    PIECE_BLACK_PAWN = 7,
    PIECE_BLACK_KNIGHT = 8,
    PIECE_BLACK_BISHOP = 9,
    PIECE_BLACK_ROOK = 10,
    PIECE_BLACK_QUEEN = 11,
    PIECE_BLACK_KING = 12
} piece_t;

// ============================================================================
// GAME STATE DEFINITIONS
// ============================================================================

/**
 * @brief Game states
 */
typedef enum {
    GAME_STATE_IDLE = 0,
    GAME_STATE_INIT = 1,
    GAME_STATE_ACTIVE = 2,
    GAME_STATE_PAUSED = 3,
    GAME_STATE_FINISHED = 4,
    GAME_STATE_ERROR = 5,
    GAME_STATE_WAITING_PIECE_DROP = 6,
    GAME_STATE_CASTLING_IN_PROGRESS = 7,
    GAME_STATE_ERROR_RECOVERY_OPPONENT_LIFT = 8,
    GAME_STATE_OPPONENT_PIECE_INVALID = 9,
    GAME_STATE_OPPONENT_RETURN_ANIMATION = 10,
    GAME_STATE_ERROR_RECOVERY_GENERAL = 11,
    GAME_STATE_ERROR_RECOVERY_CASTLING_CANCEL = 12
} game_state_t;

/**
 * @brief Player definitions
 */
typedef enum {
    PLAYER_WHITE = 0,
    PLAYER_BLACK = 1
} player_t;

/**
 * @brief Move error types
 */
typedef enum {
    MOVE_ERROR_NONE = 0,
    MOVE_ERROR_INVALID_SYNTAX = 1,
    MOVE_ERROR_INVALID_PARAMETER = 2,
    MOVE_ERROR_PIECE_NOT_FOUND = 3,
    MOVE_ERROR_INVALID_MOVE = 4,
    MOVE_ERROR_BLOCKED_PATH = 5,
    MOVE_ERROR_CHECK_VIOLATION = 6,
    MOVE_ERROR_SYSTEM_ERROR = 7,
    // Additional error types needed by game_task.c
    MOVE_ERROR_NO_PIECE = 8,
    MOVE_ERROR_WRONG_COLOR = 9,
    MOVE_ERROR_INVALID_PATTERN = 10,
    MOVE_ERROR_KING_IN_CHECK = 11,
    MOVE_ERROR_CASTLING_BLOCKED = 12,
    MOVE_ERROR_EN_PASSANT_INVALID = 13,
    MOVE_ERROR_DESTINATION_OCCUPIED = 14,
    MOVE_ERROR_OUT_OF_BOUNDS = 15,
    MOVE_ERROR_GAME_NOT_ACTIVE = 16,
    MOVE_ERROR_INVALID_MOVE_STRUCTURE = 17,
    MOVE_ERROR_INVALID_COORDINATES = 18,
    MOVE_ERROR_ILLEGAL_MOVE = 19,
    MOVE_ERROR_INVALID_CASTLING = 20
} move_error_t;

// ============================================================================
// PROMOTION DEFINITIONS
// ============================================================================

/**
 * @brief Promotion choice types
 */
typedef enum {
    PROMOTION_QUEEN = 0,
    PROMOTION_ROOK = 1,
    PROMOTION_BISHOP = 2,
    PROMOTION_KNIGHT = 3
} promotion_choice_t;

// ============================================================================
// CHESS MOVE STRUCTURES
// ============================================================================

/**
 * @brief Chess move structure
 */
typedef struct {
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
    piece_t piece;
    piece_t captured_piece;
    uint32_t timestamp;
} chess_move_t;

/**
 * @brief Move types for enhanced chess logic
 */
typedef enum {
    MOVE_TYPE_NORMAL = 0,
    MOVE_TYPE_CAPTURE = 1,
    MOVE_TYPE_CASTLE_KING = 2,
    MOVE_TYPE_CASTLE_QUEEN = 3,
    MOVE_TYPE_EN_PASSANT = 4,
    MOVE_TYPE_PROMOTION = 5
} move_type_t;

/**
 * @brief Enhanced move structure for complete chess logic
 */
typedef struct {
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
    piece_t piece;
    piece_t captured_piece;
    move_type_t move_type;
    promotion_choice_t promotion_piece;
    uint32_t timestamp;
    bool is_check;
    bool is_checkmate;
    bool is_stalemate;
} chess_move_extended_t;

// ============================================================================
// GAME COMMAND DEFINITIONS
// ============================================================================

/**
 * @brief Game command types for UART communication
 */
typedef enum {
    GAME_CMD_NEW_GAME = 0,
    GAME_CMD_RESET_GAME = 1,
    GAME_CMD_MAKE_MOVE = 2,
    GAME_CMD_UNDO_MOVE = 3,
    GAME_CMD_GET_STATUS = 4,
    GAME_CMD_GET_BOARD = 5,
    GAME_CMD_GET_VALID_MOVES = 6,
    GAME_CMD_PICKUP_PIECE = 7,
    GAME_CMD_DROP_PIECE = 8,
    GAME_CMD_PROMOTION = 9,
    GAME_CMD_MOVE = 10,
    GAME_CMD_SHOW_BOARD = 11,
    GAME_CMD_PICKUP = 12,  // up command - piece lifted
    GAME_CMD_DROP = 13,    // dn command - piece placed
    GAME_CMD_GET_HISTORY = 14,  // get move history
    GAME_CMD_DEBUG_INFO = 15,   // debug game information
    GAME_CMD_DEBUG_BOARD = 16,  // debug board information
    
    // High Priority Commands
    GAME_CMD_EVALUATE = 17,     // position evaluation
    GAME_CMD_SAVE = 18,         // save game
    GAME_CMD_LOAD = 19,         // load game
    
    // Medium Priority Commands
    GAME_CMD_CASTLE = 21,       // castling
    GAME_CMD_PROMOTE = 22,      // pawn promotion
    
    // Component Control Commands
    GAME_CMD_COMPONENT_OFF = 23, // turn off component
    GAME_CMD_COMPONENT_ON = 24,  // turn on component
    
    // Endgame Commands
    GAME_CMD_ENDGAME_WHITE = 25, // endgame white wins
    GAME_CMD_ENDGAME_BLACK = 26, // endgame black wins
    
    // Game Management Commands
    GAME_CMD_SHOW_HISTORY = 29,  // show move history
    GAME_CMD_LIST_GAMES = 27,    // list saved games
    GAME_CMD_DELETE_GAME = 28,   // delete saved game
    
    
    // Animation Test Commands
    GAME_CMD_TEST_MOVE_ANIM = 33,    // Test move animation
    GAME_CMD_TEST_PLAYER_ANIM = 34,  // Test player change animation
    GAME_CMD_TEST_CASTLE_ANIM = 35,  // Test castling animation
    GAME_CMD_TEST_PROMOTE_ANIM = 36, // Test promotion animation
    GAME_CMD_TEST_ENDGAME_ANIM = 37  // Test endgame animation
} game_command_type_t;

/**
 * @brief Chess move command structure for UART
 */
typedef struct {
    uint8_t type;
    char from_notation[4];
    char to_notation[4];
    uint8_t player;
    QueueHandle_t response_queue;
    uint8_t promotion_choice;  // For promotion commands
} chess_move_command_t;

/**
 * @brief Game response types
 */
typedef enum {
    GAME_RESPONSE_SUCCESS = 0,
    GAME_RESPONSE_ERROR = 1,
    GAME_RESPONSE_BOARD = 2,
    GAME_RESPONSE_MOVES = 3,
    GAME_RESPONSE_STATUS = 4,
    GAME_RESPONSE_HISTORY = 5,
    GAME_RESPONSE_MOVE_RESULT = 6,
    GAME_RESPONSE_LED_STATUS = 7
} game_response_type_t;

/**
 * @brief Game response structure for UART communication
 */
typedef struct {
    uint8_t type;
    uint8_t command_type;  // Original command type
    uint8_t error_code;
    char message[256];
    char data[3584];  // Increased buffer size for complete reports - board data ~3000 chars, with safety margin
    uint32_t timestamp;
} game_response_t;

// ============================================================================
// LED SYSTEM DEFINITIONS
// ============================================================================

/**
 * @brief LED command types
 */
typedef enum {
    LED_CMD_SET_PIXEL = 0,
    LED_CMD_SET_ALL = 1,
    LED_CMD_CLEAR = 2,
    LED_CMD_SHOW_BOARD = 3,
    LED_CMD_BUTTON_FEEDBACK = 4,
    LED_CMD_BUTTON_PRESS = 5,
    LED_CMD_BUTTON_RELEASE = 6,
    LED_CMD_ANIMATION = 7,
    LED_CMD_TEST = 8,
    LED_CMD_SET_BRIGHTNESS = 9,
    LED_CMD_TEST_ALL = 10,      // Test all LEDs
    LED_CMD_MATRIX_OFF = 11,    // Disable matrix scanning LED effects
    LED_CMD_MATRIX_ON = 12,     // Enable matrix scanning LED effects
    
    
    // Advanced Chess Animations
    LED_CMD_ANIM_PLAYER_CHANGE = 19, // Player change animation (rays)
    LED_CMD_ANIM_MOVE_PATH = 20,     // Move path animation
    LED_CMD_ANIM_CASTLE = 21,        // Castling animation
    LED_CMD_ANIM_PROMOTE = 22,       // Promotion animation
    LED_CMD_ANIM_ENDGAME = 23,       // Endgame animation (waves)
    LED_CMD_ANIM_CHECK = 24,         // Check animation
    LED_CMD_ANIM_CHECKMATE = 25,     // Checkmate animation
    
    // Component control commands
    LED_CMD_DISABLE = 25,            // Disable LED component
    LED_CMD_ENABLE = 26,             // Enable LED component
    
    // Button LED logic commands
    LED_CMD_BUTTON_PROMOTION_AVAILABLE = 27, // Set promotion available for button
    LED_CMD_BUTTON_PROMOTION_UNAVAILABLE = 28, // Set promotion unavailable for button
    LED_CMD_BUTTON_SET_PRESSED = 29,  // Set button pressed state
    LED_CMD_BUTTON_SET_RELEASED = 30, // Set button released state
    
    // Game state integration commands
    LED_CMD_GAME_STATE_UPDATE = 31,   // Update LEDs based on current game state
    LED_CMD_HIGHLIGHT_PIECES = 32,    // Highlight pieces that can move
    LED_CMD_HIGHLIGHT_MOVES = 33,     // Highlight possible moves for selected piece
    LED_CMD_CLEAR_HIGHLIGHTS = 34,    // Clear all highlights
    LED_CMD_PLAYER_CHANGE = 35,       // Player change animation
    
    // Error handling commands
    LED_CMD_ERROR_INVALID_MOVE = 36,  // Show error for invalid move
    LED_CMD_ERROR_RETURN_PIECE = 37,  // Force user to return piece
    LED_CMD_ERROR_RECOVERY = 38,      // Recovery from error state
    LED_CMD_SHOW_LEGAL_MOVES = 39,    // Show all legal moves for piece type
    
    LED_CMD_STATUS_ACTIVE = 97,
    LED_CMD_STATUS_COMPACT = 98,
    LED_CMD_STATUS_DETAILED = 99
} led_command_type_t;

/**
 * @brief LED command structure
 */
typedef struct {
    led_command_type_t type;
    uint8_t led_index;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint32_t duration_ms;
    void* data;
    QueueHandle_t response_queue;  // For response communication
} led_command_t;

// ============================================================================
// BUTTON SYSTEM DEFINITIONS
// ============================================================================

/**
 * @brief Button event types
 */
typedef enum {
    BUTTON_EVENT_PRESS = 0,
    BUTTON_EVENT_RELEASE = 1,
    BUTTON_EVENT_LONG_PRESS = 2,
    BUTTON_EVENT_DOUBLE_PRESS = 3
} button_event_type_t;

/**
 * @brief Button event structure
 */
typedef struct {
    button_event_type_t type;
    uint8_t button_id;
    uint32_t press_duration_ms;
    uint32_t timestamp;
} button_event_t;

// ============================================================================
// MATRIX SYSTEM DEFINITIONS
// ============================================================================

/**
 * @brief Matrix event types
 */
typedef enum {
    MATRIX_EVENT_PIECE_LIFTED = 0,
    MATRIX_EVENT_PIECE_PLACED = 1,
    MATRIX_EVENT_MOVE_DETECTED = 2,
    MATRIX_EVENT_ERROR = 3
} matrix_event_type_t;

/**
 * @brief Matrix event structure
 */
typedef struct {
    matrix_event_type_t type;
    uint8_t from_square;
    uint8_t to_square;
    piece_t piece_type;
    uint32_t timestamp;
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
} matrix_event_t;

/**
 * @brief Matrix command types
 */
typedef enum {
    MATRIX_CMD_SCAN = 0,
    MATRIX_CMD_RESET = 1,
    MATRIX_CMD_TEST = 2,
    MATRIX_CMD_CALIBRATE = 3,
    MATRIX_CMD_DISABLE = 4,          // Disable matrix scanning
    MATRIX_CMD_ENABLE = 5            // Enable matrix scanning
} matrix_command_type_t;

/**
 * @brief Matrix command structure
 */
typedef struct {
    matrix_command_type_t type;
    uint8_t data[16];
} matrix_command_t;










// ============================================================================
// HARDWARE CONSTANTS
// ============================================================================

// LED system constants
#define CHESS_LED_COUNT_BOARD 64
#define CHESS_LED_COUNT_BUTTONS 9
#define CHESS_LED_COUNT_TOTAL (CHESS_LED_COUNT_BOARD + CHESS_LED_COUNT_BUTTONS)
#define CHESS_LED_COUNT 73  // Total number of LEDs (64 board + 9 buttons)

// Button system constants
#define CHESS_BUTTON_COUNT 9

// Matrix system constants
#define CHESS_MATRIX_SIZE 64

// Game constants
#define MAX_MOVE_HISTORY 200



/**
 * @brief Move suggestion structure for analysis
 */
typedef struct {
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
    piece_t piece;
    bool is_capture;
    bool is_check;
    bool is_castling;
    bool is_en_passant;
    int score;
} move_suggestion_t;

// ============================================================================
// GAME UTILITY FUNCTION DECLARATIONS
// ============================================================================

// Forward declarations for game utility functions
bool game_is_valid_square(int row, int col);
bool game_is_own_piece(piece_t piece, player_t player);
bool game_is_enemy_piece(piece_t piece, player_t player);
bool game_simulate_move_check(chess_move_extended_t* move, player_t player);

// ============================================================================
// SYSTEM CONFIGURATION DEFINITIONS
// ============================================================================

/**
 * @brief System configuration structure
 */
typedef struct {
    bool verbose_mode;           // Verbose logging mode
    bool quiet_mode;            // Quiet mode (minimal output)
    uint8_t log_level;          // Log level (ESP_LOG_*)
    uint32_t command_timeout_ms; // Command timeout in milliseconds
    bool echo_enabled;          // Character echo enabled
    // Extended configuration for LED and Matrix
    uint8_t led_brightness;     // LED brightness 0-100%
    uint8_t matrix_sensitivity; // Matrix sensitivity 0-100%
    bool debug_mode_enabled;    // Debug mode enabled/disabled
} system_config_t;

// Forward declarations for configuration functions
esp_err_t config_manager_init(void);
esp_err_t config_load_from_nvs(system_config_t* config);
esp_err_t config_save_to_nvs(const system_config_t* config);
esp_err_t config_apply_settings(const system_config_t* config);



/**
 * @brief Animation state for LED effects
 */
typedef struct {
    bool is_active;                  // Animation active
    uint8_t animation_type;          // Type of animation
    uint8_t current_frame;           // Current animation frame
    uint8_t total_frames;            // Total frames in animation
    uint32_t frame_duration_ms;      // Duration per frame
    uint32_t last_update_time;       // Last update timestamp
    uint8_t source_square;           // Source square (0-63)
    uint8_t target_square;           // Target square (0-63)
    uint8_t color_r, color_g, color_b; // Animation colors
    bool interrupt_on_placement;     // Stop animation when piece placed
} led_animation_state_t;

#ifdef __cplusplus
}
#endif

#endif // CHESS_TYPES_H
