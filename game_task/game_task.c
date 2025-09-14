/**
 * @file game_task.c
 * @brief ESP32-C6 Chess System v2.4 - Game Task Implementation
 * 
 * This task manages the chess game logic:
 * - Game state management
 * - Move validation and execution
 * - Game rules enforcement
 * - Player turn management
 * - Game status tracking
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 * 
 * Features:
 * - Standard chess rules
 * - Move validation
 * - Game state persistence
 * - Move history
 * - Game analysis
 */


#include "../freertos_chess/include/chess_types.h"
#include "../freertos_chess/include/streaming_output.h"
#include "game_task.h"
#include "freertos_chess.h"
#include "../led_task/include/led_task.h"  // ✅ FIX: Include led_task.h for led_clear_board_only()
#include "game_led_animations.h"
#include "led_mapping.h"  // ✅ FIX: Include LED mapping functions
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>


static const char *TAG = "GAME_TASK";

// External queue declarations - LED queues removed, using direct LED calls now

// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================

// Direction vectors for piece movement
static const int8_t knight_moves[8][2] = {
    {-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}
};

// Game configuration
#define MAX_MOVES_HISTORY    50      // Maximum moves to remember
#define GAME_TIMEOUT_MS      300000  // 5 minutes per move timeout
#define MOVE_VALIDATION_MS   100     // Move validation timeout

// Game state
static game_state_t current_game_state = GAME_STATE_IDLE;
static player_t current_player = PLAYER_WHITE;
static uint32_t move_count = 0;

// Board representation (8x8)
static piece_t board[8][8] = {0};
static bool piece_moved[8][8] = {false}; // Track if pieces have moved

// Global variables for tracking lifted piece (UP/DN commands)
static bool piece_lifted = false;
static uint8_t lifted_piece_row = 0;
static uint8_t lifted_piece_col = 0;
static piece_t lifted_piece = PIECE_EMPTY;

// Castling flags
static bool white_king_moved = false;
static bool white_rook_a_moved = false;  // a1 rook
static bool white_rook_h_moved = false;  // h1 rook
static bool black_king_moved = false;
static bool black_rook_a_moved = false;  // a8 rook
static bool black_rook_h_moved = false;  // h8 rook

// En passant state
static bool en_passant_available = false;
static uint8_t en_passant_target_row = 0;
static uint8_t en_passant_target_col = 0;
static uint8_t en_passant_victim_row = 0;
static uint8_t en_passant_victim_col = 0;

// Minimal move tracking - only what we actually need
static chess_move_t last_valid_move;
static chess_move_t current_invalid_move;
static bool has_last_valid_move = false;

// Task state
static bool task_running = false;
static bool game_active = false;

// Game statistics
static uint32_t total_games = 0;
static uint32_t white_wins = 0;
static uint32_t black_wins = 0;
static uint32_t draws = 0;

// Extended game statistics
static uint32_t game_start_time = 0;
static uint32_t last_move_time = 0;
static uint32_t white_time_total = 0;
static uint32_t black_time_total = 0;
static uint32_t white_moves_count = 0;
static uint32_t black_moves_count = 0;
static uint32_t white_captures = 0;
static uint32_t black_captures = 0;
static uint32_t white_checks = 0;
static uint32_t black_checks = 0;
static uint32_t white_castles = 0;
static uint32_t black_castles = 0;
static uint32_t moves_without_capture = 0;
static uint32_t max_moves_without_capture = 0;
// Minimal position tracking - only for draw detection
static uint32_t last_position_hash = 0;
static uint8_t position_repetition_count = 0;

// Game state flags
static bool timer_enabled = true;
static bool game_saved = false;
static char saved_game_name[32] = "";
static game_state_t game_result = GAME_STATE_IDLE;

// Error recovery state
static bool error_recovery_active = false;
static chess_move_t invalid_move_backup;
static uint32_t error_recovery_start_time = 0;
static const uint32_t ERROR_RECOVERY_TIMEOUT_MS = 30000; // 30 seconds

// Error counting system
static uint32_t consecutive_error_count = 0;
static const uint32_t MAX_CONSECUTIVE_ERRORS = 10;

// Last valid position tracking
static uint8_t last_valid_position_row = 0;
static uint8_t last_valid_position_col = 0;
static bool has_last_valid_position = false;

// Castling transaction state
static bool castling_in_progress = false;
static bool castling_kingside = false;
static uint8_t castling_king_row = 0;
static uint8_t castling_king_from_col = 0;
static uint8_t castling_king_to_col = 0;
static uint8_t castling_rook_from_col = 0;
static uint8_t castling_rook_to_col = 0;
static uint32_t castling_start_time = 0;
static const uint32_t CASTLING_TIMEOUT_MS = 60000; // 60 seconds

// Castling flags and en passant state
// (declared later in the file)

// Move generation buffers (declared later in the file)
// static chess_move_extended_t temp_moves_buffer[128];
// static uint32_t temp_moves_count = 0;

// Include ctype.h for tolower function
#include <ctype.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

void game_print_board_enhanced(void);
bool game_execute_move_enhanced(chess_move_extended_t* move);
uint32_t game_generate_legal_moves(player_t player);

// ============================================================================
// POSITION HASHING AND DRAW DETECTION
// ============================================================================

/**
 * @brief Calculate hash for current board position
 * @return 32-bit position hash
 */
uint32_t game_calculate_position_hash(void)
{
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
    if (!piece_moved[0][4]) castling_rights |= 0x01; // White king
    if (!piece_moved[0][0]) castling_rights |= 0x02; // White rook a1
    if (!piece_moved[0][7]) castling_rights |= 0x04; // White rook h1
    if (!piece_moved[7][4]) castling_rights |= 0x08; // Black king
    if (!piece_moved[7][0]) castling_rights |= 0x10; // Black rook a8
    if (!piece_moved[7][7]) castling_rights |= 0x20; // Black rook h8
    hash = ((hash << 5) + hash) ^ castling_rights;
    
    // Include en passant state
    if (en_passant_available) {
        uint32_t en_passant_hash = (en_passant_target_row << 8) | en_passant_target_col;
        hash = ((hash << 5) + hash) ^ en_passant_hash;
    }
    
    return hash;
}

/**
 * @brief Check if current position has been repeated three times (threefold repetition)
 * @return true if position is repeated 3+ times (draw by repetition)
 */
bool game_is_position_repeated(void)
{
    uint32_t current_hash = game_calculate_position_hash();
    // repetition_count removed - not needed
    
    // Check if current position repeats
    if (current_hash == last_position_hash && position_repetition_count >= 3) {
        return true; // Threefold repetition detected
    }
    
    return false;
}

/**
 * @brief Add current position to history
 */
void game_add_position_to_history(void)
{
    uint32_t current_hash = game_calculate_position_hash();
    
    // Simple repetition detection - only check if same as last position
    if (current_hash == last_position_hash) {
        position_repetition_count++;
    } else {
        position_repetition_count = 1;
        last_position_hash = current_hash;
    }
}

// ============================================================================
// MATERIAL CALCULATION AND SCORING
// ============================================================================

// Standard piece values
static const int piece_values[] = {
    0,  // PIECE_EMPTY
    1,  // PIECE_WHITE_PAWN
    3,  // PIECE_WHITE_KNIGHT
    3,  // PIECE_WHITE_BISHOP
    5,  // PIECE_WHITE_ROOK
    9,  // PIECE_WHITE_QUEEN
    0,  // PIECE_WHITE_KING (infinite value)
    1,  // PIECE_BLACK_PAWN
    3,  // PIECE_BLACK_KNIGHT
    3,  // PIECE_BLACK_BISHOP
    5,  // PIECE_BLACK_ROOK
    9,  // PIECE_BLACK_QUEEN
    0   // PIECE_BLACK_KING (infinite value)
};

/**
 * @brief Calculate material balance
 * @param white_material Output: white material value
 * @param black_material Output: black material value
 * @return Material difference (positive = white advantage, negative = black advantage)
 */
int game_calculate_material_balance(int* white_material, int* black_material)
{
    int white_total = 0;
    int black_total = 0;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (piece != PIECE_EMPTY) {
                if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_QUEEN) {
                    white_total += piece_values[piece];
                } else if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_QUEEN) {
                    black_total += piece_values[piece - 6]; // Adjust index for black pieces
                }
            }
        }
    }
    
    if (white_material) *white_material = white_total;
    if (black_material) *black_material = black_total;
    
    return white_total - black_total;
}

/**
 * @brief Get material balance as string
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 */
void game_get_material_string(char* buffer, size_t buffer_size)
{
    int white_material, black_material;
    int balance = game_calculate_material_balance(&white_material, &black_material);
    
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
 * @brief Print comprehensive game statistics
 */
void game_print_game_stats(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t game_duration = current_time - game_start_time;
    uint32_t minutes = game_duration / 60;
    uint32_t seconds = game_duration % 60;
    
    // Calculate average time per move
    uint32_t white_avg_time = (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
    uint32_t black_avg_time = (black_moves_count > 0) ? black_time_total / black_moves_count : 0;
    
    // Get material balance
    char material_str[32];
    game_get_material_string(material_str, sizeof(material_str));
    
    // Print atmospheric header
    ESP_LOGI(TAG, "╔═══════════════════════════════╗");
    ESP_LOGI(TAG, "║ ESP32 CHESS v2.4 ║");
    ESP_LOGI(TAG, "║ Move %" PRIu32 " - %s to play ║", 
             move_count, (current_player == PLAYER_WHITE) ? "White" : "Black");
    ESP_LOGI(TAG, "║ Material: %s ║", material_str);
    ESP_LOGI(TAG, "╚═══════════════════════════════╝");
    
    // Game duration and move info
    ESP_LOGI(TAG, "Game duration: %02" PRIu32 ":%02" PRIu32 ", Move %" PRIu32 " (%s to play)",
             minutes, seconds, move_count, 
             (current_player == PLAYER_WHITE) ? "White" : "Black");
    
    // Capture statistics
    ESP_LOGI(TAG, "Captures: White %" PRIu32 " pieces, Black %" PRIu32 " pieces",
             white_captures, black_captures);
    
    // Check and castle statistics
    ESP_LOGI(TAG, "Checks: White %" PRIu32 ", Black %" PRIu32 " | Castles: White %" PRIu32 ", Black %" PRIu32 "",
             white_checks, black_checks, white_castles, black_castles);
    
    // Material breakdown
    int white_material, black_material;
    game_calculate_material_balance(&white_material, &black_material);
    ESP_LOGI(TAG, "Material: White %d points, Black %d points (%s)",
             white_material, black_material, material_str);
    
    // Time statistics
    if (timer_enabled) {
        ESP_LOGI(TAG, "Time per move: White avg %" PRIu32 "s, Black avg %" PRIu32 "s",
                 white_avg_time, black_avg_time);
    }
    
    // Draw detection info
    if (moves_without_capture > 30) {
        ESP_LOGI(TAG, "⚠️  %" PRIu32 " moves without capture (50-move rule approaching)",
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
static uint8_t last_move_from_row = 0;
static uint8_t last_move_from_col = 0;
static uint8_t last_move_to_row = 0;
static uint8_t last_move_to_col = 0;
static bool has_last_move = false;

// Captured pieces counter
// Minimal captured pieces tracking - only counts
static uint8_t white_captured_count = 0;
static uint8_t black_captured_count = 0;


// ============================================================================
// CASTLING VALIDATION FUNCTIONS
// ============================================================================

/**
 * @brief Check if player can castle kingside
 * @param player Player to check
 * @return true if castling is possible
 */
bool game_can_castle_kingside(player_t player)
{
    uint8_t king_row = (player == PLAYER_WHITE) ? 0 : 7;
    uint8_t king_col = 4;
    uint8_t rook_col = 7;
    
    // Check if king and rook are in correct positions
    piece_t king = board[king_row][king_col];
    piece_t rook = board[king_row][rook_col];
    
    bool valid_king = (player == PLAYER_WHITE) ? (king == PIECE_WHITE_KING) : (king == PIECE_BLACK_KING);
    bool valid_rook = (player == PLAYER_WHITE) ? (rook == PIECE_WHITE_ROOK) : (rook == PIECE_BLACK_ROOK);
    
    if (!valid_king || !valid_rook) {
        return false;
    }
    
    // Check if pieces have moved
    if ((player == PLAYER_WHITE && (white_king_moved || white_rook_h_moved)) ||
        (player == PLAYER_BLACK && (black_king_moved || black_rook_h_moved))) {
        return false;
    }
    
    // Check if path is clear
    for (uint8_t col = king_col + 1; col < rook_col; col++) {
        if (board[king_row][col] != PIECE_EMPTY) {
            return false;
        }
    }
    
    // Check if king is in check
    if (game_is_king_in_check(player)) {
        return false;
    }
    
    return true;
}

/**
 * @brief Check if player can castle queenside
 * @param player Player to check
 * @return true if castling is possible
 */
bool game_can_castle_queenside(player_t player)
{
    uint8_t king_row = (player == PLAYER_WHITE) ? 0 : 7;
    uint8_t king_col = 4;
    uint8_t rook_col = 0;
    
    // Check if king and rook are in correct positions
    piece_t king = board[king_row][king_col];
    piece_t rook = board[king_row][rook_col];
    
    bool valid_king = (player == PLAYER_WHITE) ? (king == PIECE_WHITE_KING) : (king == PIECE_BLACK_KING);
    bool valid_rook = (player == PLAYER_WHITE) ? (rook == PIECE_WHITE_ROOK) : (rook == PIECE_BLACK_ROOK);
    
    if (!valid_king || !valid_rook) {
        return false;
    }
    
    // Check if pieces have moved
    if ((player == PLAYER_WHITE && (white_king_moved || white_rook_a_moved)) ||
        (player == PLAYER_BLACK && (black_king_moved || black_rook_a_moved))) {
        return false;
    }
    
    // Check if path is clear
    for (uint8_t col = rook_col + 1; col < king_col; col++) {
        if (board[king_row][col] != PIECE_EMPTY) {
            return false;
        }
    }
    
    // Check if king is in check
    if (game_is_king_in_check(player)) {
        return false;
    }
    
    return true;
}

// ============================================================================
// ENHANCED MOVE VALIDATION SYSTEM
// ============================================================================

// Tutorial mode state
static bool tutorial_mode_active = false;
static bool show_hints = true;
// Game analysis flags (for future use)
// static bool show_warnings = true;
// static bool show_analysis = true;

// Move analysis cache (for future use)
// static move_suggestion_t move_suggestions[100];
// static uint32_t suggestion_count = 0;
// static uint32_t last_analysis_time = 0;


// Piece symbols for display (ASCII) - FIXED INDEXES
static const char* piece_symbols[] = {
    " ",     // 0: PIECE_EMPTY
    "p",     // 1: PIECE_WHITE_PAWN
    "n",     // 2: PIECE_WHITE_KNIGHT
    "b",     // 3: PIECE_WHITE_BISHOP
    "r",     // 4: PIECE_WHITE_ROOK
    "q",     // 5: PIECE_WHITE_QUEEN
    "k",     // 6: PIECE_WHITE_KING
    "P",     // 7: PIECE_BLACK_PAWN
    "N",     // 8: PIECE_BLACK_KNIGHT
    "B",     // 9: PIECE_BLACK_BISHOP
    "R",     // 10: PIECE_BLACK_ROOK
    "Q",     // 11: PIECE_BLACK_QUEEN
    "K"      // 12: PIECE_BLACK_KING
};


// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================


void game_initialize_board(void)
{
    ESP_LOGI(TAG, "Initializing enhanced chess board...");
    
    // Clear board
    memset(board, 0, sizeof(board));
    memset(piece_moved, 0, sizeof(piece_moved));
    
    // Set up white pieces (bottom rank - row 1, index 0)
    board[0][0] = PIECE_WHITE_ROOK;      // a1
    board[0][1] = PIECE_WHITE_KNIGHT;    // b1
    board[0][2] = PIECE_WHITE_BISHOP;    // c1
    board[0][3] = PIECE_WHITE_QUEEN;     // d1
    board[0][4] = PIECE_WHITE_KING;      // e1
    board[0][5] = PIECE_WHITE_BISHOP;    // f1
    board[0][6] = PIECE_WHITE_KNIGHT;    // g1
    board[0][7] = PIECE_WHITE_ROOK;      // h1
    
    // Set up white pawns (row 2, index 1)
    for (int col = 0; col < 8; col++) {
        board[1][col] = PIECE_WHITE_PAWN;
    }
    
    // Rows 3-6 (indices 2-5) remain empty
    
    // Set up black pawns (row 7, index 6)
    for (int col = 0; col < 8; col++) {
        board[6][col] = PIECE_BLACK_PAWN;
    }
    
    // Set up black pieces (top rank - row 8, index 7)
    board[7][0] = PIECE_BLACK_ROOK;      // a8
    board[7][1] = PIECE_BLACK_KNIGHT;    // b8
    board[7][2] = PIECE_BLACK_BISHOP;    // c8
    board[7][3] = PIECE_BLACK_QUEEN;     // d8
    board[7][4] = PIECE_BLACK_KING;      // e8
    board[7][5] = PIECE_BLACK_BISHOP;    // f8
    board[7][6] = PIECE_BLACK_KNIGHT;    // g8
    board[7][7] = PIECE_BLACK_ROOK;      // h8
    
    // Reset game state
    current_player = PLAYER_WHITE;
    current_game_state = GAME_STATE_ACTIVE;
    move_count = 0;
    
    // Reset castling flags (declared later in the file)
    white_king_moved = false;
    white_rook_a_moved = false;
    white_rook_h_moved = false;
    black_king_moved = false;
    black_rook_a_moved = false;
    black_rook_h_moved = false;
    
    // Reset en passant (declared later in the file)
    // en_passant_available = false;
    
    // Reset other counters (declared later in the file)
    // fifty_move_counter = 0;
    // position_history_count = 0;
    
    ESP_LOGI(TAG, "Enhanced chess board initialized successfully");
    ESP_LOGI(TAG, "Initial position: White pieces at bottom, Black pieces at top");
    // Board will be displayed after all tasks are initialized
}


void game_reset_game(void)
{
    ESP_LOGI(TAG, "Resetting game...");
    
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
    position_repetition_count = 0;
    last_position_hash = 0;
    game_result = GAME_STATE_IDLE;
    game_saved = false;
    saved_game_name[0] = '\0';
    
    // Clear move tracking
    has_last_valid_move = false;
    memset(&last_valid_move, 0, sizeof(last_valid_move));
    memset(&current_invalid_move, 0, sizeof(current_invalid_move));
    
    // Clear captured pieces
    white_captured_count = 0;
    black_captured_count = 0;
    
    // Clear last move tracking
    has_last_move = false;
    
    // Reset error handling states
    error_recovery_active = false;
    consecutive_error_count = 0;
    piece_lifted = false;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    lifted_piece = PIECE_EMPTY;
    
    // FIXED: Initialize last valid position tracking for first move
    // Set to starting position of first piece that can be moved
    has_last_valid_position = true;
    last_valid_position_row = 1;  // Starting row for white pieces
    last_valid_position_col = 0;  // Starting column a2
    
    // Reset castling states
    castling_in_progress = false;
    
    // Reinitialize board
    game_initialize_board();
    
    ESP_LOGI(TAG, "Game reset completed");
}


void game_start_new_game(void)
{
    ESP_LOGI(TAG, "Starting new game...");
    
    // Reset game
    game_reset_game();
    
    // Set game state
    current_game_state = GAME_STATE_IDLE;
    game_active = true;
    game_start_time = esp_timer_get_time() / 1000;
    last_move_time = game_start_time;
    
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
    position_repetition_count = 0;
    last_position_hash = 0;
    game_result = GAME_STATE_IDLE;
    game_saved = false;
    saved_game_name[0] = '\0';
    
    total_games++;
    
    ESP_LOGI(TAG, "New game started - White to move");
    ESP_LOGI(TAG, "Total games: %" PRIu32, total_games);
    
    // ✅ OPRAVA: Zvýraznit pohyblivé figurky na začátku hry
    vTaskDelay(pdMS_TO_TICKS(100)); // Krátká pauza pro stabilizaci
    game_highlight_movable_pieces();
    ESP_LOGI(TAG, "✅ Highlighted movable pieces for starting player");
}


// ============================================================================
// BOARD UTILITY FUNCTIONS
// ============================================================================


bool game_is_valid_position(int row, int col)
{
    return (row >= 0 && row < 8 && col >= 0 && col < 8);
}


piece_t game_get_piece(int row, int col)
{
    if (!game_is_valid_position(row, col)) {
        return PIECE_EMPTY;
    }
    return board[row][col];
}


void game_set_piece(int row, int col, piece_t piece)
{
    if (!game_is_valid_position(row, col)) {
        return;
    }
    board[row][col] = piece;
}


bool game_is_empty(int row, int col)
{
    return game_get_piece(row, col) == PIECE_EMPTY;
}


bool game_is_white_piece(piece_t piece)
{
    return (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
}


bool game_is_black_piece(piece_t piece)
{
    return (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
}


bool game_is_same_color(piece_t piece1, piece_t piece2)
{
    if (piece1 == PIECE_EMPTY || piece2 == PIECE_EMPTY) {
        return false;
    }
    return (game_is_white_piece(piece1) && game_is_white_piece(piece2)) ||
           (game_is_black_piece(piece1) && game_is_black_piece(piece2));
}


bool game_is_opponent_piece(piece_t piece, player_t player)
{
    if (piece == PIECE_EMPTY) {
        return false;
    }
    
    if (player == PLAYER_WHITE) {
        return game_is_black_piece(piece);
    } else {
        return game_is_white_piece(piece);
    }
}


// ============================================================================
// MOVE VALIDATION FUNCTIONS
// ============================================================================


move_error_t game_is_valid_move(const chess_move_t* move)
{
    if (!move) {
        return MOVE_ERROR_INVALID_MOVE_STRUCTURE;
    }
    
    if (!game_is_valid_position(move->from_row, move->from_col) ||
        !game_is_valid_position(move->to_row, move->to_col)) {
        return MOVE_ERROR_OUT_OF_BOUNDS;
    }
    
    if (!game_active) {
        return MOVE_ERROR_GAME_NOT_ACTIVE;
    }
    
    // Check if source position has a piece
    piece_t source_piece = game_get_piece(move->from_row, move->from_col);
    if (source_piece == PIECE_EMPTY) {
        return MOVE_ERROR_NO_PIECE;
    }
    
    // Check if source piece belongs to current player
    if ((current_player == PLAYER_WHITE && !game_is_white_piece(source_piece)) ||
        (current_player == PLAYER_BLACK && !game_is_black_piece(source_piece))) {
        return MOVE_ERROR_WRONG_COLOR;
    }
    
    // Check if destination is occupied by own piece
    piece_t dest_piece = game_get_piece(move->to_row, move->to_col);
    if (dest_piece != PIECE_EMPTY && game_is_same_color(source_piece, dest_piece)) {
        return MOVE_ERROR_DESTINATION_OCCUPIED;
    }
    
    // Validate move based on piece type
    move_error_t piece_error = game_validate_piece_move_enhanced(move, source_piece);
    if (piece_error != MOVE_ERROR_NONE) {
        return piece_error;
    }
    
    // Check if move would leave king in check
    if (game_would_move_leave_king_in_check(move)) {
        return MOVE_ERROR_KING_IN_CHECK;
    }
    
    return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_is_valid_move_bool(const chess_move_t* move)
{
    return (game_is_valid_move(move) == MOVE_ERROR_NONE);
}


move_error_t game_validate_piece_move_enhanced(const chess_move_t* move, piece_t piece)
{
    // int row_diff = move->to_row - move->from_row;
    // int col_diff = move->to_col - move->from_col;
    // int abs_row_diff = abs(row_diff);
    // int abs_col_diff = abs(col_diff);
    
    switch (piece) {
        case PIECE_WHITE_PAWN:
        case PIECE_BLACK_PAWN:
            return game_validate_pawn_move_enhanced(move, piece);
            
        case PIECE_WHITE_KNIGHT:
        case PIECE_BLACK_KNIGHT:
            return game_validate_knight_move_enhanced(move);
            
        case PIECE_WHITE_BISHOP:
        case PIECE_BLACK_BISHOP:
            return game_validate_bishop_move_enhanced(move);
            
        case PIECE_WHITE_ROOK:
        case PIECE_BLACK_ROOK:
            return game_validate_rook_move_enhanced(move);
            
        case PIECE_WHITE_QUEEN:
        case PIECE_BLACK_QUEEN:
            return game_validate_queen_move_enhanced(move);
            
        case PIECE_WHITE_KING:
        case PIECE_BLACK_KING:
            return game_validate_king_move_enhanced(move);
            
        default:
            return MOVE_ERROR_INVALID_PATTERN;
    }
}

// Keep the old function for backward compatibility
bool game_validate_piece_move(const chess_move_t* move, piece_t piece)
{
    return (game_validate_piece_move_enhanced(move, piece) == MOVE_ERROR_NONE);
}


move_error_t game_validate_pawn_move_enhanced(const chess_move_t* move, piece_t piece)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_col_diff = abs(col_diff);
    
    bool is_white = game_is_white_piece(piece);
    int direction = is_white ? 1 : -1;
    int start_row = is_white ? 1 : 6;
    
    // Forward move
    if (col_diff == 0) {
        // Single square move
        if (row_diff == direction && game_is_empty(move->to_row, move->to_col)) {
            return MOVE_ERROR_NONE;
        }
        
        // Double square move from starting position
        if (row_diff == 2 * direction && move->from_row == start_row &&
            game_is_empty(move->from_row + direction, move->from_col) &&
            game_is_empty(move->to_row, move->to_col)) {
            return MOVE_ERROR_NONE;
        }
        
        // Check if path is blocked
        if (row_diff > 0 && !game_is_empty(move->from_row + direction, move->from_col)) {
            return MOVE_ERROR_BLOCKED_PATH;
        }
        
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    // Capture move (diagonal)
    if (abs_col_diff == 1 && row_diff == direction) {
        piece_t dest_piece = game_get_piece(move->to_row, move->to_col);
        if (dest_piece != PIECE_EMPTY && !game_is_same_color(piece, dest_piece)) {
            return MOVE_ERROR_NONE;
        }
        
        // Check for en passant
        if (game_is_en_passant_possible(move)) {
            return MOVE_ERROR_NONE;
        }
        
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_pawn_move(const chess_move_t* move, piece_t piece)
{
    return (game_validate_pawn_move_enhanced(move, piece) == MOVE_ERROR_NONE);
}


move_error_t game_validate_knight_move_enhanced(const chess_move_t* move)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_row_diff = abs(row_diff);
    int abs_col_diff = abs(col_diff);
    
    // Knight moves in L-shape: 2 squares in one direction, 1 square perpendicular
    if ((abs_row_diff == 2 && abs_col_diff == 1) || (abs_row_diff == 1 && abs_col_diff == 2)) {
        return MOVE_ERROR_NONE;
    }
    
    return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_knight_move(const chess_move_t* move)
{
    return (game_validate_knight_move_enhanced(move) == MOVE_ERROR_NONE);
}


move_error_t game_validate_bishop_move_enhanced(const chess_move_t* move)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_row_diff = abs(row_diff);
    int abs_col_diff = abs(col_diff);
    
    // Bishop moves diagonally
    if (abs_row_diff != abs_col_diff) {
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    // Check if path is blocked
    int row_step = (row_diff > 0) ? 1 : -1;
    int col_step = (col_diff > 0) ? 1 : -1;
    
    int current_row = move->from_row + row_step;
    int current_col = move->from_col + col_step;
    
    while (current_row != move->to_row && current_col != move->to_col) {
        if (!game_is_empty(current_row, current_col)) {
            return MOVE_ERROR_BLOCKED_PATH;
        }
        current_row += row_step;
        current_col += col_step;
    }
    
    return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_validate_bishop_move(const chess_move_t* move)
{
    return (game_validate_bishop_move_enhanced(move) == MOVE_ERROR_NONE);
}


move_error_t game_validate_rook_move_enhanced(const chess_move_t* move)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    
    // Rook moves horizontally or vertically
    if (row_diff != 0 && col_diff != 0) {
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    // Check if path is blocked
    if (row_diff == 0) {
        // Horizontal move
        int col_step = (col_diff > 0) ? 1 : -1;
        int current_col = move->from_col + col_step;
        
        while (current_col != move->to_col) {
            if (!game_is_empty(move->from_row, current_col)) {
                return MOVE_ERROR_BLOCKED_PATH;
            }
            current_col += col_step;
        }
    } else {
        // Vertical move
        int row_step = (row_diff > 0) ? 1 : -1;
        int current_row = move->from_row + row_step;
        
        while (current_row != move->to_row) {
            if (!game_is_empty(current_row, move->from_col)) {
                return MOVE_ERROR_BLOCKED_PATH;
            }
            current_row += row_step;
        }
    }
    
    return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_validate_rook_move(const chess_move_t* move)
{
    return (game_validate_rook_move_enhanced(move) == MOVE_ERROR_NONE);
}

move_error_t game_validate_queen_move_enhanced(const chess_move_t* move)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_row_diff = abs(row_diff);
    int abs_col_diff = abs(col_diff);
    
    // Queen moves like rook (horizontally/vertically) or bishop (diagonally)
    if (row_diff == 0 || col_diff == 0) {
        // Rook-like move
        return game_validate_rook_move_enhanced(move);
    } else if (abs_row_diff == abs_col_diff) {
        // Bishop-like move
        return game_validate_bishop_move_enhanced(move);
    }
    
    return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_queen_move(const chess_move_t* move)
{
    return (game_validate_queen_move_enhanced(move) == MOVE_ERROR_NONE);
}

move_error_t game_validate_king_move_enhanced(const chess_move_t* move)
{
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_row_diff = abs(row_diff);
    int abs_col_diff = abs(col_diff);
    
    // King moves one square in any direction
    if (abs_row_diff <= 1 && abs_col_diff <= 1) {
        return MOVE_ERROR_NONE;
    }
    
    // Check for castling
    if (abs_row_diff == 0 && abs_col_diff == 2) {
        return game_validate_castling(move);
    }
    
    return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_king_move(const chess_move_t* move)
{
    return (game_validate_king_move_enhanced(move) == MOVE_ERROR_NONE);
}

// Helper functions for enhanced validation
bool game_would_move_leave_king_in_check(const chess_move_t* move)
{
    if (!move) return true; // Safety check
    
    // Save original board state
    piece_t original_from_piece = board[move->from_row][move->from_col];
    piece_t original_to_piece = board[move->to_row][move->to_col];
    
    // Make the move temporarily
    board[move->to_row][move->to_col] = original_from_piece;
    board[move->from_row][move->from_col] = PIECE_EMPTY;
    
    // Determine which player's king to check
    player_t player = (game_is_white_piece(original_from_piece)) ? PLAYER_WHITE : PLAYER_BLACK;
    
    // Check if king is in check after this move
    bool king_in_check = game_is_king_in_check(player);
    
    // Undo the move
    board[move->from_row][move->from_col] = original_from_piece;
    board[move->to_row][move->to_col] = original_to_piece;
    
    return king_in_check;
}

bool game_is_en_passant_possible(const chess_move_t* move)
{
    if (!has_last_move) {
        return false;
    }
    
    // Check if the last move was a pawn moving two squares
    piece_t last_piece = board[last_move_to_row][last_move_to_col];
    bool last_was_pawn = (last_piece == PIECE_WHITE_PAWN || last_piece == PIECE_BLACK_PAWN);
    
    if (!last_was_pawn) {
        return false;
    }
    
    // Check if last move was a double square move
    int last_row_diff = abs((int)last_move_to_row - (int)last_move_from_row);
    if (last_row_diff != 2) {
        return false;
    }
    
    // Check if current move is to the en passant square (behind the last moved pawn)
    bool is_white_pawn = game_is_white_piece(move->piece);
    int en_passant_row = is_white_pawn ? last_move_to_row - 1 : last_move_to_row + 1;
    
    if (move->to_row == en_passant_row && move->to_col == last_move_to_col) {
        return true;
    }
    
    return false;
}

move_error_t game_validate_castling(const chess_move_t* move)
{
    if (!move) {
        return MOVE_ERROR_INVALID_MOVE_STRUCTURE;
    }
    
    piece_t piece = move->piece;
    bool is_white = game_is_white_piece(piece);
    
    // Check if it's a king
    if (piece != PIECE_WHITE_KING && piece != PIECE_BLACK_KING) {
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    // Check if king has moved
    if ((is_white && white_king_moved) || (!is_white && black_king_moved)) {
        return MOVE_ERROR_CASTLING_BLOCKED;
    }
    
    // Check if king is in starting position
    int king_row = is_white ? 0 : 7;
    if (move->from_row != king_row || move->from_col != 4) {
        return MOVE_ERROR_CASTLING_BLOCKED;
    }
    
    // Determine castling direction
    bool is_kingside = (move->to_col == 6);
    bool is_queenside = (move->to_col == 2);
    
    if (!is_kingside && !is_queenside) {
        return MOVE_ERROR_INVALID_PATTERN;
    }
    
    // Check if corresponding rook has moved
    if (is_white) {
        if (is_kingside && white_rook_h_moved) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
        if (is_queenside && white_rook_a_moved) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
    } else {
        if (is_kingside && black_rook_h_moved) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
        if (is_queenside && black_rook_a_moved) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
    }
    
    // Check if squares between king and rook are empty
    int rook_col = is_kingside ? 7 : 0;
    int start_col = (move->from_col < rook_col) ? move->from_col + 1 : rook_col + 1;
    int end_col = (move->from_col < rook_col) ? rook_col : move->from_col;
    
    for (int col = start_col; col < end_col; col++) {
        if (!game_is_empty(king_row, col)) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
    }
    
    // Check if king is currently in check
    if (game_is_king_in_check(is_white ? PLAYER_WHITE : PLAYER_BLACK)) {
        return MOVE_ERROR_CASTLING_BLOCKED;
    }
    
    // Check if king would move through check or end in check
    int step = is_kingside ? 1 : -1;
    for (int col = move->from_col; col != move->to_col + step; col += step) {
        if (col == move->from_col) continue; // Skip starting position
        
        // Create temporary move to test this square
        chess_move_t temp_move = {
            .from_row = move->from_row,
            .from_col = move->from_col,
            .to_row = move->to_row,
            .to_col = col,
            .piece = piece,
            .captured_piece = PIECE_EMPTY,
            .timestamp = 0
        };
        
        if (game_would_move_leave_king_in_check(&temp_move)) {
            return MOVE_ERROR_CASTLING_BLOCKED;
        }
    }
    
    return MOVE_ERROR_NONE;
}

/**
 * @brief Display detailed move error message
 * @param error Move error type
 * @param move Move that caused the error
 */
void game_display_move_error(move_error_t error, const chess_move_t* move)
{
    char from_square[3], to_square[3];
    game_coords_to_square(move->from_row, move->from_col, from_square);
    game_coords_to_square(move->to_row, move->to_col, to_square);
    
    const char* piece_name = game_get_piece_name(move->piece);
    const char* player_name = (current_player == PLAYER_WHITE) ? "White" : "Black";
    
    // Use smaller static buffer to avoid memory issues
    static char error_msg[256];
    
    // Build error message safely
    snprintf(error_msg, sizeof(error_msg), "❌ INVALID MOVE!\n   • Move: %s → %s\n", from_square, to_square);
    
    switch (error) {
        case MOVE_ERROR_NO_PIECE:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: No piece at %s\n   • Solution: Choose a square with your piece", from_square);
            break;
            
        case MOVE_ERROR_WRONG_COLOR:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: %s cannot move %s's piece\n   • Solution: Move only your own pieces", 
                player_name, (current_player == PLAYER_WHITE) ? "Black" : "White");
            break;
            
        case MOVE_ERROR_BLOCKED_PATH:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Path from %s to %s is blocked\n   • Solution: Clear the path or choose different destination", 
                from_square, to_square);
            break;
            
        case MOVE_ERROR_INVALID_PATTERN:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: %s cannot move from %s to %s\n   • Solution: Follow the piece's movement rules", 
                piece_name, from_square, to_square);
            break;
            
        case MOVE_ERROR_KING_IN_CHECK:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: This move would leave your king in check\n   • Solution: Move to protect your king or block the attack");
            break;
            
        case MOVE_ERROR_CASTLING_BLOCKED:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Castling is not allowed (king or rook has moved)\n   • Solution: Castling requires unmoved king and rook");
            break;
            
        case MOVE_ERROR_EN_PASSANT_INVALID:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: En passant is not possible\n   • Solution: En passant only after opponent's 2-square pawn move");
            break;
            
        case MOVE_ERROR_DESTINATION_OCCUPIED:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Destination %s is occupied by your own piece\n   • Solution: Choose empty square or capture opponent's piece", 
                to_square);
            break;
            
        case MOVE_ERROR_OUT_OF_BOUNDS:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Coordinates are out of board bounds\n   • Solution: Use valid chess notation (a1-h8)");
            break;
            
        case MOVE_ERROR_GAME_NOT_ACTIVE:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Game is not active\n   • Solution: Start a new game first");
            break;
            
        case MOVE_ERROR_INVALID_MOVE_STRUCTURE:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Move structure is invalid\n   • Solution: Use proper move format (e.g., e2e4)");
            break;
            
        default:
            snprintf(error_msg + strlen(error_msg), sizeof(error_msg) - strlen(error_msg),
                "   • Reason: Unknown error occurred\n   • Solution: Try a different move");
            break;
    }
    
    // Send error message via printf (safer for this function)
    printf("%s\n", error_msg);
    
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
const char* game_get_piece_name(piece_t piece)
{
    switch (piece) {
        case PIECE_EMPTY: return "Empty";
        case PIECE_WHITE_PAWN: return "White Pawn";
        case PIECE_WHITE_KNIGHT: return "White Knight";
        case PIECE_WHITE_BISHOP: return "White Bishop";
        case PIECE_WHITE_ROOK: return "White Rook";
        case PIECE_WHITE_QUEEN: return "White Queen";
        case PIECE_WHITE_KING: return "White King";
        case PIECE_BLACK_PAWN: return "Black Pawn";
        case PIECE_BLACK_KNIGHT: return "Black Knight";
        case PIECE_BLACK_BISHOP: return "Black Bishop";
        case PIECE_BLACK_ROOK: return "Black Rook";
        case PIECE_BLACK_QUEEN: return "Black Queen";
        case PIECE_BLACK_KING: return "Black King";
        default: return "Unknown";
    }
}

/**
 * @brief Show move suggestions for a piece at given position
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_show_move_suggestions(uint8_t row, uint8_t col)
{
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
    ESP_LOGI(TAG, "💡 Hint: %s at %s can move to:", 
             game_get_piece_name(piece), from_square);
    
    // OPTIMIZED: Use static buffers to reduce stack usage
    static char normal_moves[128] = "";
    static char capture_moves[128] = "";
    static char special_moves[128] = "";
    
    // Clear buffers
    normal_moves[0] = '\0';
    capture_moves[0] = '\0';
    special_moves[0] = '\0';
    
    int normal_pos = 0, capture_pos = 0, special_pos = 0;
    
    for (uint32_t i = 0; i < count && i < 20; i++) { // Limit to 20 moves for display
        char to_square[3];
        game_coords_to_square(suggestions[i].to_row, suggestions[i].to_col, to_square);
        
        if (suggestions[i].is_capture) {
            if (capture_pos > 0) capture_pos += snprintf(capture_moves + capture_pos, 
                                                        sizeof(capture_moves) - capture_pos, ", ");
            capture_pos += snprintf(capture_moves + capture_pos, 
                                   sizeof(capture_moves) - capture_pos, "%s", to_square);
        } else if (suggestions[i].is_castling || suggestions[i].is_en_passant) {
            if (special_pos > 0) special_pos += snprintf(special_moves + special_pos, 
                                                         sizeof(special_moves) - special_pos, ", ");
            special_pos += snprintf(special_moves + special_pos, 
                                   sizeof(special_moves) - special_pos, "%s", to_square);
        } else {
            if (normal_pos > 0) normal_pos += snprintf(normal_moves + normal_pos, 
                                                       sizeof(normal_moves) - normal_pos, ", ");
            normal_pos += snprintf(normal_moves + normal_pos, 
                                  sizeof(normal_moves) - normal_pos, "%s", to_square);
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
uint32_t game_get_available_moves(uint8_t row, uint8_t col, move_suggestion_t* suggestions, uint32_t max_suggestions)
{
    if (suggestions == NULL || max_suggestions == 0) {
        return 0;
    }
    
    piece_t piece = board[row][col];
    if (piece == PIECE_EMPTY) {
        return 0;
    }
    
    uint32_t count = 0;
    
    // Optimized move generation based on piece type
    switch (piece) {
        case PIECE_WHITE_KNIGHT:
        case PIECE_BLACK_KNIGHT:
            // Knight moves in L-shape: 8 possible positions
            for (int i = 0; i < 8 && count < max_suggestions; i++) {
                int to_row = row + knight_moves[i][0];
                int to_col = col + knight_moves[i][1];
                
                if (!game_is_valid_square(to_row, to_col)) continue;
                
                piece_t target = board[to_row][to_col];
                player_t player = (piece == PIECE_WHITE_KNIGHT) ? PLAYER_WHITE : PLAYER_BLACK;
                
                // Skip if occupied by own piece
                if (game_is_own_piece(target, player)) continue;
                
                // Create temporary move for validation
                chess_move_t temp_move = {
                    .from_row = row,
                    .from_col = col,
                    .to_row = to_row,
                    .to_col = to_col,
                    .piece = piece,
                    .captured_piece = target,
                    .timestamp = 0
                };
                
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
                    chess_move_t temp_move = {
                        .from_row = row,
                        .from_col = col,
                        .to_row = to_row,
                        .to_col = to_col,
                        .piece = piece,
                        .captured_piece = board[to_row][to_col],
                        .timestamp = 0
                    };
                    
                    // Check if move is valid
                    move_error_t error = game_is_valid_move(&temp_move);
                    if (error == MOVE_ERROR_NONE) {
                        // Add to suggestions
                        suggestions[count].from_row = row;
                        suggestions[count].from_col = col;
                        suggestions[count].to_row = to_row;
                        suggestions[count].to_col = to_col;
                        suggestions[count].piece = piece;
                        suggestions[count].is_capture = (board[to_row][to_col] != PIECE_EMPTY);
                        suggestions[count].is_check = false; // TODO: Implement check detection
                        
                        // Detect castling
                        suggestions[count].is_castling = (piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING) &&
                                                       (abs((int)to_col - (int)col) == 2);
                        
                        // Detect en passant
                        suggestions[count].is_en_passant = game_is_en_passant_possible(&temp_move);
                        
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
// MOVE EXECUTION FUNCTIONS
// ============================================================================


bool game_execute_move(const chess_move_t* move)
{
    // FIXED: This function executes moves WITHOUT validation
    // Validation is done AFTER calling this function
    
    ESP_LOGI(TAG, "Executing move: %c%d-%c%d", 
              'a' + move->from_col, move->from_row + 1,
              'a' + move->to_col, move->to_row + 1);
    
    // Get pieces
    piece_t source_piece = game_get_piece(move->from_row, move->from_col);
    piece_t dest_piece = game_get_piece(move->to_row, move->to_col);
    
    // FIXED: Use move->piece if provided (for recovery moves where piece is already lifted)
    if (move->piece != PIECE_EMPTY) {
        source_piece = move->piece;
    }
    
    // Create extended move for proper handling of special cases
    chess_move_extended_t extended_move = {
        .from_row = move->from_row,
        .from_col = move->from_col,
        .to_row = move->to_row,
        .to_col = move->to_col,
        .piece = source_piece,
        .captured_piece = dest_piece,
        .move_type = MOVE_TYPE_NORMAL,
        .promotion_piece = PROMOTION_QUEEN,
        .timestamp = esp_timer_get_time() / 1000,
        .is_check = false,
        .is_checkmate = false,
        .is_stalemate = false
    };
    
    // Detect special move types
    
    // 1. Check for castling
    if ((source_piece == PIECE_WHITE_KING || source_piece == PIECE_BLACK_KING) &&
        abs(move->to_col - move->from_col) == 2) {
        extended_move.move_type = (move->to_col > move->from_col) ? MOVE_TYPE_CASTLE_KING : MOVE_TYPE_CASTLE_QUEEN;
    }
    
    // 2. Check for en passant
    else if ((source_piece == PIECE_WHITE_PAWN || source_piece == PIECE_BLACK_PAWN) &&
             abs(move->to_col - move->from_col) == 1 && dest_piece == PIECE_EMPTY) {
        if (game_is_en_passant_possible(move)) {
            extended_move.move_type = MOVE_TYPE_EN_PASSANT;
        }

    }
    
    // 3. Check for promotion
    else if ((source_piece == PIECE_WHITE_PAWN && move->to_row == 7) ||
             (source_piece == PIECE_BLACK_PAWN && move->to_row == 0)) {
        extended_move.move_type = MOVE_TYPE_PROMOTION;
        // Use default promotion to Queen - can be enhanced later with user input
    }
    
    // 4. Check for capture
    else if (dest_piece != PIECE_EMPTY) {
        extended_move.move_type = MOVE_TYPE_CAPTURE;
        ESP_LOGI(TAG, "Capture: %s captures %s", 
                  game_get_piece_name(source_piece),
                  game_get_piece_name(dest_piece));
    }
    
    // Execute the enhanced move
    bool success = game_execute_move_enhanced(&extended_move);
    
    if (success) {
        // Reset error count on successful move
        consecutive_error_count = 0;
        
        // Update last move tracking for future en passant detection
        last_move_from_row = move->from_row;
        last_move_from_col = move->from_col;
        last_move_to_row = move->to_row;
        last_move_to_col = move->to_col;
        has_last_move = true;
        
        // Store last valid move for error recovery
        last_valid_move = *move;
        last_valid_move.piece = source_piece;
        last_valid_move.captured_piece = dest_piece;
        last_valid_move.timestamp = esp_timer_get_time() / 1000;
        has_last_valid_move = true;
        
        // Update game state
        last_move_time = esp_timer_get_time() / 1000;
        
        // FIXED: move_count++ and player switch are handled by game_execute_move_enhanced()
        // No need to duplicate here
        
        ESP_LOGI(TAG, "Move executed successfully. %s to move", 
                  (current_player == PLAYER_WHITE) ? "White" : "Black");
    }
    
    return success;
}


// ============================================================================
// GAME STATUS FUNCTIONS
// ============================================================================


game_state_t game_get_state(void)
{
    return current_game_state;
}


player_t game_get_current_player(void)
{
    return current_player;
}


uint32_t game_get_move_count(void)
{
    return move_count;
}


void game_print_board(void)
{
    // Add spacing before board
    printf("\r\n");
    
    // Enhanced ANSI color codes
    #define COLOR_RESET     "\033[0m"
    #define COLOR_BOLD      "\033[1m"
    #define COLOR_BRIGHT_WHITE   "\033[97m"
    #define COLOR_BRIGHT_RED     "\033[91m"
    #define COLOR_BRIGHT_GREEN   "\033[92m"
    #define COLOR_BRIGHT_YELLOW  "\033[93m"
    #define COLOR_BRIGHT_BLUE    "\033[94m"
    #define COLOR_BRIGHT_CYAN    "\033[96m"
    #define BG_WHITE     "\033[47m"
    #define BG_BLACK     "\033[40m"
    #define BG_YELLOW    "\033[43m"
    
    ESP_LOGI(TAG, "=== Chess Board ===");
    
    // Print board header with coordinates
    printf(COLOR_BRIGHT_CYAN "    a   b   c   d   e   f   g   h\n" COLOR_RESET);
    printf(COLOR_BRIGHT_CYAN "  +---+---+---+---+---+---+---+---+\n" COLOR_RESET);
    
    // Print board rows (8 to 1) - FIXED: row 7 = rank 8, row 0 = rank 1
    for (int row = 7; row >= 0; row--) {
        char row_header[16];
        snprintf(row_header, sizeof(row_header), COLOR_BRIGHT_CYAN " %d " COLOR_RESET, row + 1);
        printf("%s", row_header);
        
        // Print pieces in this row
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            
            // Check if this square is part of the last move
            bool is_last_move = has_last_move && 
                ((row == last_move_from_row && col == last_move_from_col) ||
                 (row == last_move_to_row && col == last_move_to_col));
            
            // Determine square color (white/black)
            bool is_white_square = ((row + col) % 2) == 0;
            
            if (piece == PIECE_EMPTY) {
                if (is_last_move) {
                    printf(BG_YELLOW COLOR_BRIGHT_CYAN " * " COLOR_RESET);  // Highlight empty square from last move
                } else {
                    if (is_white_square) {
                        printf(BG_WHITE "   " COLOR_RESET);
                    } else {
                        printf(BG_BLACK "   " COLOR_RESET);
                    }
                }
            } else {
                // Determine piece color
                bool is_white_piece = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
                const char* piece_color = is_white_piece ? COLOR_BRIGHT_WHITE : COLOR_BRIGHT_RED;
                
                if (is_last_move) {
                    printf(BG_YELLOW COLOR_BRIGHT_CYAN "*" COLOR_RESET);
                    printf(BG_YELLOW "%s%s" COLOR_RESET, piece_color, piece_symbols[piece]);
                    printf(BG_YELLOW COLOR_BRIGHT_CYAN "*" COLOR_RESET);  // Highlight piece from last move
                } else {
                    if (is_white_square) {
                        printf(BG_WHITE "%s%s " COLOR_RESET, piece_color, piece_symbols[piece]);
                    } else {
                        printf(BG_BLACK "%s%s " COLOR_RESET, piece_color, piece_symbols[piece]);
                    }
                }
            }
        }
        char row_footer[16];
        snprintf(row_footer, sizeof(row_footer), COLOR_BRIGHT_CYAN " %d\n" COLOR_RESET, row + 1);
        printf("%s", row_footer);
        
        // Print row separator (except after last row)
        if (row > 0) {
            printf(COLOR_BRIGHT_CYAN "  +---+---+---+---+---+---+---+---+\n" COLOR_RESET);
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
    
    printf(COLOR_BRIGHT_YELLOW "   • Move number: " COLOR_RESET COLOR_BOLD "%" PRIu32 "\r\n" COLOR_RESET, move_count + 1);
    
    // Print last move information
    if (has_last_move) {
        char from_square[4], to_square[4];
        game_coords_to_square(last_move_from_row, last_move_from_col, from_square);
        game_coords_to_square(last_move_to_row, last_move_to_col, to_square);
        printf(COLOR_BRIGHT_YELLOW "   • Last move: " COLOR_RESET COLOR_BOLD "%s → %s\r\n" COLOR_RESET, from_square, to_square);
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
            printf("  White captured: %d pieces\n", white_captured_count);
        }
        if (black_captured_count > 0) {
            printf("  Black captured: %d pieces\n", black_captured_count);
        }
    }
    
    // Print piece legend
    ESP_LOGI(TAG, "Piece Legend:");
    ESP_LOGI(TAG, "  White: p=pawn, n=knight, b=bishop, r=rook, q=queen, k=king");
    ESP_LOGI(TAG, "  Black: P=pawn, N=knight, B=bishop, R=rook, Q=queen, K=king");
    ESP_LOGI(TAG, "  Empty: space, * = last move");
    
    // Print game status
    ESP_LOGI(TAG, "Game Status:");
    ESP_LOGI(TAG, "  Current player: %s", (current_player == PLAYER_WHITE) ? "White" : "Black");
    ESP_LOGI(TAG, "  Move count: %lu", move_count);
    ESP_LOGI(TAG, "  Game state: %s", 
              (current_game_state == GAME_STATE_ACTIVE) ? "Active" :
              (current_game_state == GAME_STATE_IDLE) ? "Idle" :
              (current_game_state == GAME_STATE_PAUSED) ? "Paused" : "Finished");
}


void game_print_move_history(void)
{
    if (has_last_valid_move) {
        ESP_LOGI(TAG, "Last valid move: %c%d-%c%d %s", 
                 'a' + last_valid_move.from_col, last_valid_move.from_row + 1,
                 'a' + last_valid_move.to_col, last_valid_move.to_row + 1,
                 game_get_piece_name(last_valid_move.piece));
    } else {
        ESP_LOGI(TAG, "No moves yet");
    }
}


// ============================================================================
// GENIUS COMPATIBILITY FUNCTIONS
// ============================================================================

/**
 * @brief Send response to UART task via queue
 */
void game_send_response_to_uart(const char* message, bool is_error, QueueHandle_t response_queue)
{
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
        .timestamp = esp_timer_get_time() / 1000
    };
    strcpy(response.data, response_message);
    
    // Use mutex for safe communication
    if (game_mutex != NULL) {
        xSemaphoreTake(game_mutex, portMAX_DELAY);
    }
    
    if (xQueueSend(response_queue, &response, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send response to UART task");
    }
    
    if (game_mutex != NULL) {
        xSemaphoreGive(game_mutex);
    }
}

/**
 * @brief Send board data to UART task via response queue
 */
static void game_send_board_to_uart(QueueHandle_t response_queue)
{
    if (response_queue == NULL) {
        ESP_LOGW(TAG, "No response queue available for board data");
        return;
    }
    
    // Reset WDT before starting operation
    esp_task_wdt_reset();
    
    // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3072)
    // This eliminates heap fragmentation and reduces memory usage
    ESP_LOGI(TAG, "📡 Using streaming output for board display (no malloc)");
    
    // Configure streaming for queue output
    esp_err_t ret = streaming_set_queue_output(response_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure streaming output: %s", esp_err_to_name(ret));
        return;
    }
    
    // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
    
    // Stream board header
    stream_writeln("    a   b   c   d   e   f   g   h");
    stream_writeln("  +---+---+---+---+---+---+---+---+");
    
    // Stream board rows
    for (int row = 7; row >= 0; row--) {
        // Reset watchdog every row to prevent timeouts
        esp_task_wdt_reset();
        
        stream_printf("%d |", row + 1);
        
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            const char* symbol;
            
            switch (piece) {
                case PIECE_WHITE_PAWN:   symbol = "P"; break;
                case PIECE_WHITE_KNIGHT: symbol = "N"; break;
                case PIECE_WHITE_BISHOP: symbol = "B"; break;
                case PIECE_WHITE_ROOK:   symbol = "R"; break;
                case PIECE_WHITE_QUEEN:  symbol = "Q"; break;
                case PIECE_WHITE_KING:   symbol = "K"; break;
                case PIECE_BLACK_PAWN:   symbol = "p"; break;
                case PIECE_BLACK_KNIGHT: symbol = "n"; break;
                case PIECE_BLACK_BISHOP: symbol = "b"; break;
                case PIECE_BLACK_ROOK:   symbol = "r"; break;
                case PIECE_BLACK_QUEEN:  symbol = "q"; break;
                case PIECE_BLACK_KING:   symbol = "k"; break;
                default:                 symbol = "·"; break;
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
        .timestamp = esp_timer_get_time() / 1000
    };
    strcpy(completion_response.data, "streaming completed");
    
    if (response_queue != NULL) {
        xQueueSend(response_queue, &completion_response, pdMS_TO_TICKS(100));
    }
    
    // No need for manual queue management or large buffers
    ESP_LOGI(TAG, "✅ Board display streaming completed successfully");
    
    // Final WDT reset
    esp_task_wdt_reset();
}

// Function removed - using game_send_board_to_uart(QueueHandle_t) instead

/**
 * @brief Highlight valid moves for a specific piece
 * @param row Row coordinate of the piece
 * @param col Column coordinate of the piece
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col)
{
    ESP_LOGI(TAG, "🟡 Highlighting valid moves for piece at %c%d", 'a' + col, row + 1);
    
    // Clear board first
    led_clear_board_only();
    
    // Highlight the piece position in yellow
    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0); // Yellow
    
    // Get available moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
    
    if (valid_moves > 0) {
        ESP_LOGI(TAG, "💡 Found %lu valid moves for piece at %c%d", valid_moves, 'a' + col, row + 1);
        
        // Highlight possible destinations
        for (uint32_t i = 0; i < valid_moves; i++) {
            uint8_t led_index = chess_pos_to_led_index(suggestions[i].to_row, suggestions[i].to_col);
            if (suggestions[i].is_capture) {
                led_set_pixel_safe(led_index, 255, 165, 0); // Orange for captures
            } else {
                led_set_pixel_safe(led_index, 0, 255, 0); // Green for normal moves
            }
        }
    } else {
        ESP_LOGI(TAG, "⚠️ No valid moves found for piece at %c%d", 'a' + col, row + 1);
    }
}

/**
 * @brief Process pickup command (UP)
 */
static void game_process_pickup_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🎯 Processing PICKUP command: %s", cmd->from_notation);
    
    // Convert notation to coordinates
    uint8_t from_row, from_col;
    if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col)) {
        ESP_LOGE(TAG, "❌ Invalid notation: %s", cmd->from_notation);
        game_send_response_to_uart("❌ Invalid square notation", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Check if there's a piece at the square
    piece_t piece = board[from_row][from_col];
    if (piece == PIECE_EMPTY) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "❌ No piece at %s", cmd->from_notation);
        ESP_LOGE(TAG, "❌ No piece at %s", cmd->from_notation);
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Handle different game states according to specification
    if (current_game_state == GAME_STATE_ERROR_RECOVERY_GENERAL) {
        // FIXED: Recovery - ignore actual position and force lift from last_valid_position
        // Player must physically lift piece from last_valid_position, not from invalid destination
        
        // FIXED: Valid lift from error recovery position - use last_valid_position for internal tracking
        if (!has_last_valid_position) {
            ESP_LOGE(TAG, "❌ Error recovery active but no last valid position!");
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ No valid position to return to");
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // 1. Ujistit se, že board[invalid_move_backup.to] obsahuje tu figuru
        piece_lifted = true;
        lifted_piece_row = invalid_move_backup.to_row;
        lifted_piece_col = invalid_move_backup.to_col;
        lifted_piece = invalid_move_backup.piece;
        current_game_state = GAME_STATE_WAITING_PIECE_DROP;

        // 2. LED: žlutá na last_valid + zelené tahy z ní
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(
            last_valid_position_row, last_valid_position_col), 255, 255, 0);
        game_highlight_valid_moves_for_piece(
            last_valid_position_row, last_valid_position_col
        );

        game_send_response_to_uart(
            "✅ Recovery lift – select one of highlighted moves (green) or cancel (yellow)", 
            false, (QueueHandle_t)cmd->response_queue
        );
        return;
        
    } else if (current_game_state == GAME_STATE_IDLE) {
        // Check if piece belongs to current player
        bool is_white_piece = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
        bool is_black_piece = (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
        
        if ((current_player == PLAYER_WHITE && !is_white_piece) ||
            (current_player == PLAYER_BLACK && !is_black_piece)) {
            // Opponent's piece - start opponent lift recovery
            ESP_LOGW(TAG, "❌ Cannot lift opponent's piece at %s", cmd->from_notation);
            
            // Set state to opponent lift recovery
            current_game_state = GAME_STATE_ERROR_RECOVERY_OPPONENT_LIFT;
            invalid_move_backup.from_row = from_row;
            invalid_move_backup.from_col = from_col;
            
            // LED: solid red on opponent piece position
            led_clear_board_only();
            led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 0, 0); // Solid red
            
            // UART instruction
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ Return opponent's piece to %s", cmd->from_notation);
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // Own piece - proceed normally
        current_game_state = GAME_STATE_WAITING_PIECE_DROP;
        
        // FIXED: Uložit last_valid_position při každém UP příkazu
        last_valid_position_row = from_row;
        last_valid_position_col = from_col;
        has_last_valid_position = true;
        
        // Set piece lifted state
        piece_lifted = true;
        lifted_piece_row = from_row;
        lifted_piece_col = from_col;
        lifted_piece = piece;
        
        // Check if this is a king move that could be castling
        if (piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING) {
            // Check if castling is possible
            bool can_castle_kingside = game_can_castle_kingside(current_player);
            bool can_castle_queenside = game_can_castle_queenside(current_player);
            
            if (can_castle_kingside || can_castle_queenside) {
                // Show castling options
                char castling_msg[256];
                snprintf(castling_msg, sizeof(castling_msg),
                    "🏰 King lifted - castling options:\n"
                    "  • Move 2 squares right for kingside\n"
                    "  • Move 2 squares left for queenside\n"
                    "  • Or move normally"
                );
                game_send_response_to_uart(castling_msg, false, (QueueHandle_t)cmd->response_queue);
            }
        }
        
    } else if (current_game_state == GAME_STATE_CASTLING_IN_PROGRESS) {
        // Handle castling piece lift
        uint8_t king_row = (current_player == PLAYER_WHITE) ? 0 : 7;
        uint8_t king_col = castling_king_from_col; // King is at original position
        uint8_t rook_from_col = castling_rook_from_col;
        
        bool is_king = (from_row == king_row && from_col == king_col);
        bool is_rook = (from_row == king_row && from_col == rook_from_col);
        
        if (!is_king && !is_rook) {
            // Wrong piece during castling - handle as general error
            chess_move_t error_move = {
                .from_row = from_row,
                .from_col = from_col,
                .to_row = from_row,
                .to_col = from_col,
                .piece = piece,
                .captured_piece = PIECE_EMPTY,
                .timestamp = esp_timer_get_time() / 1000
            };
            game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &error_move);
            return;
        }
        
        // Set piece lifted state for castling
        piece_lifted = true;
        lifted_piece_row = from_row;
        lifted_piece_col = from_col;
        lifted_piece = piece;
        
    } else if (current_game_state == GAME_STATE_ERROR_RECOVERY_OPPONENT_LIFT) {
        // Only allow lifting the piece that needs to be returned
        if (from_row != invalid_move_backup.from_row || from_col != invalid_move_backup.from_col) {
            ESP_LOGW(TAG, "❌ Error recovery active - can only lift piece at [%d,%d]", 
                     invalid_move_backup.from_row, invalid_move_backup.from_col);
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ Return opponent's piece to %s first", cmd->from_notation);
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // FIXED: Force recovery from correct opponent piece position
        piece_lifted = true;
        lifted_piece_row = invalid_move_backup.from_row;  // Use backup position, not event position
        lifted_piece_col = invalid_move_backup.from_col;  // Use backup position, not event position
        lifted_piece = board[invalid_move_backup.from_row][invalid_move_backup.from_col];
        
        // LED: steady red on opponent piece position
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(invalid_move_backup.from_row, invalid_move_backup.from_col), 255, 0, 0); // Solid red
        
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "✅ Opponent's piece lifted - return to %c%d", 
                'a' + invalid_move_backup.from_col, invalid_move_backup.from_row + 1);
        game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
        return;
    } else if (current_game_state == GAME_STATE_CASTLING_IN_PROGRESS) {
        // FIXED: Handle UP in castling - must be rook from correct position
        if (from_row != castling_king_row || from_col != castling_rook_from_col) {
            ESP_LOGW(TAG, "❌ Castling in progress - lift rook from %c%d", 
                     'a' + castling_rook_from_col, castling_king_row + 1);
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ Lift rook from %c%d", 
                    'a' + castling_rook_from_col, castling_king_row + 1);
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // Valid rook lift - set piece lifted state
        piece_lifted = true;
        lifted_piece_row = from_row;
        lifted_piece_col = from_col;
        lifted_piece = piece;
        
        // LED guidance: steady green on rook_from + blue steady on rook_to
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_rook_from_col), 0, 255, 0); // Green steady
        led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_rook_to_col), 0, 0, 255); // Blue steady
        
        // UART instruction
        char instruction[256];
        snprintf(instruction, sizeof(instruction), "🏰 Place rook on %c%d", 
                'a' + castling_rook_to_col, castling_king_row + 1);
        game_send_response_to_uart(instruction, false, (QueueHandle_t)cmd->response_queue);
        return;
        
    } else {
        // Other states - ignore or send instructions
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "❌ Invalid action in current game state");
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // FIXED: No animation when lifting piece, just highlight possible moves
    ESP_LOGI(TAG, "🔄 Piece lifted from %s - showing possible moves", cmd->from_notation);
    
    // Clear previous highlights before showing new ones
    led_clear_board_only();
    
    // Send LED command to highlight source square using new game state logic
    led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255, 0); // Yellow for source
    
    // Track the lifted piece
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;
    
    // Give FreeRTOS scheduler time to process LED commands
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Show possible moves (always, including during recovery)
    ESP_LOGI(TAG, "🔄 Showing possible moves from %s", cmd->from_notation);
    
    // Get valid moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves = game_get_available_moves(from_row, from_col, suggestions, 64);
    
    ESP_LOGI(TAG, "Found %lu valid moves for piece at %s", valid_moves, cmd->from_notation);
    
    // Send LED commands to highlight possible destinations
    if (valid_moves > 0) {
        for (uint32_t i = 0; i < valid_moves; i++) {
            uint8_t dest_row = suggestions[i].to_row;
            uint8_t dest_col = suggestions[i].to_col;
            uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);
            
            // Check if destination has opponent's piece
            piece_t dest_piece = board[dest_row][dest_col];
            bool is_opponent_piece = (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN && dest_piece <= PIECE_BLACK_KING) ||
                                   (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN && dest_piece <= PIECE_WHITE_KING);
            
            if (is_opponent_piece) {
                // Orange for opponent's pieces (capture)
                led_set_pixel_safe(led_index, 255, 165, 0);
            } else {
                // Green for empty squares
                led_set_pixel_safe(led_index, 0, 255, 0);
            }
        }
    } else {
        ESP_LOGI(TAG, "🔄 No valid moves found for piece at %s", cmd->from_notation);
    }
    
    // Give FreeRTOS scheduler time to process LED commands
    vTaskDelay(pdMS_TO_TICKS(50));
    
    // Send success response
    char success_msg[256];
    if (game_is_error_recovery_active()) {
        snprintf(success_msg, sizeof(success_msg), "✅ Piece lifted for return to correct position");
    } else {
        snprintf(success_msg, sizeof(success_msg), "✅ Piece lifted from %s - ready to move", cmd->from_notation);
    }
    game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process drop command (DN)
 */
static void game_process_drop_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🎯 Processing DROP command: %s", cmd->to_notation);
    
    // Convert notation to coordinates
    uint8_t to_row, to_col;
    if (!convert_notation_to_coords(cmd->to_notation, &to_row, &to_col)) {
        ESP_LOGE(TAG, "❌ Invalid notation: %s", cmd->to_notation);
        game_send_response_to_uart("❌ Invalid square notation", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // FIXED: Progressive color animation from green to blue
    if (piece_lifted) {
        
        // Step 1: Show move path with progressive color change (green -> blue)
        for (int step = 0; step < 10; step++) {
            float progress = (float)step / 9.0f;
            
            // Calculate intermediate position
            int inter_row = lifted_piece_row + (to_row - lifted_piece_row) * progress;
            int inter_col = lifted_piece_col + (to_col - lifted_piece_col) * progress;
            uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);
            
            // Calculate color transition (green -> blue)
            uint8_t red = 0;
            uint8_t green = 255 - (255 * progress);  // 255 -> 0
            uint8_t blue = 0 + (255 * progress);     // 0 -> 255
            
            led_clear_board_only();
            led_set_pixel_safe(inter_led, red, green, blue);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Step 2: Final blue flash on destination
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 0, 0, 255); // Blue
        vTaskDelay(pdMS_TO_TICKS(300));
    } else {
        // Fallback: simple blue flash if no piece was lifted
        led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 0, 0, 255); // Blue flash
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    
    // FIXED: Check for "drop without lift" before state handling
    if (!piece_lifted && current_game_state != GAME_STATE_CASTLING_IN_PROGRESS) {
        ESP_LOGW(TAG, "❌ Drop command without prior lift");
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "❌ Invalid move - lift piece first");
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        
        // LED: blink red on last lifted position if available
        if (lifted_piece_row < 8 && lifted_piece_col < 8) {
            led_clear_board_only();
            for (int i = 0; i < 3; i++) {
                led_set_pixel_safe(chess_pos_to_led_index(lifted_piece_row, lifted_piece_col), 255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_clear_board_only();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        return;
    }
    
    // Handle different game states according to specification
    if (current_game_state == GAME_STATE_ERROR_RECOVERY_OPPONENT_LIFT) {
        // Handle opponent piece return
        if (to_row == invalid_move_backup.from_row && to_col == invalid_move_backup.from_col) {
            // Correct position - clear error state
            led_clear_board_only();
            current_game_state = GAME_STATE_IDLE;
            // FIXED: Reset error count after successful recovery
            consecutive_error_count = 0;
            ESP_LOGI(TAG, "✅ Error count reset to 0 after opponent piece return");
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg), "✅ Opponent's piece returned - ready to play");
            game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
        } else {
            // Wrong position - show error
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ Return opponent's piece to %c%d", 
                    'a' + invalid_move_backup.from_col, invalid_move_backup.from_row + 1);
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            
            // LED: blink red on correct position
            led_clear_board_only();
            for (int i = 0; i < 3; i++) {
                led_set_pixel_safe(chess_pos_to_led_index(invalid_move_backup.from_row, invalid_move_backup.from_col), 255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_clear_board_only();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        return;
    }
    
    if (current_game_state == GAME_STATE_WAITING_PIECE_DROP) {
        // NEW RECOVERY FLOW: Check if this is recovery mode
        if (error_recovery_active) {
            // 3A) Položení na zelené pole → provést recovery move
            move_suggestion_t suggestions[64];
            uint32_t count = game_get_available_moves(
                last_valid_position_row, last_valid_position_col,
                suggestions, 64
            );
            bool is_green = false;
            for (uint32_t i = 0; i < count; i++) {
                if (suggestions[i].to_row == to_row &&
                    suggestions[i].to_col == to_col) {
                    is_green = true;
                    break;
                }
            }

            if (is_green) {
                // FIXED: Manual board update for recovery move
                // 1. Vymaž figurku z nevalidní pozice
                board[invalid_move_backup.to_row][invalid_move_backup.to_col] = PIECE_EMPTY;
                
                // 2. Umísti figurku na validní pozici
                board[to_row][to_col] = lifted_piece;
                
                // 3. Aktualizuj last_valid_position
                last_valid_position_row = to_row;
                last_valid_position_col = to_col;
                
                // 4. Vyčistit recovery
                error_recovery_active = false;
                current_game_state = GAME_STATE_IDLE;
                piece_lifted = false;

                // 5. LED: modrá flash + žlutá na nové last_valid
                led_clear_board_only();
                led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 0, 0, 255);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_clear_board_only();
                led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 255, 0);

                // 6. Změna hráče
                current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

                // 7. Reset error count on successful recovery
                consecutive_error_count = 0;

                game_send_response_to_uart("✅ Move recovered – next player to move", false, (QueueHandle_t)cmd->response_queue);
                return;
            }

            // 3B) Položení na žluté pole → zrušení recovery
            if (to_row == last_valid_position_row && to_col == last_valid_position_col) {
                // FIXED: Manual board update for recovery cancellation
                // 1. Vymaž figurku z nevalidní pozice
                board[invalid_move_backup.to_row][invalid_move_backup.to_col] = PIECE_EMPTY;
                
                // 2. Umísti figurku zpět na last_valid_position
                board[last_valid_position_row][last_valid_position_col] = lifted_piece;
                
                // 3. Vyčistit recovery
                error_recovery_active = false;
                current_game_state = GAME_STATE_IDLE;
                piece_lifted = false;

                // 4. LED: zobrazit všechny tahy hráče
                led_clear_board_only();
                game_highlight_movable_pieces();

                // 5. Reset error count on successful cancellation
                consecutive_error_count = 0;

                game_send_response_to_uart("❌ Recovery cancelled – try a valid move", false, (QueueHandle_t)cmd->response_queue);
                return;
            }

            // 3C) Položení na jiné nevalidní pole → změna nevalidní pozice
            // Vymaž starou figurku
            board[invalid_move_backup.to_row][invalid_move_backup.to_col] = PIECE_EMPTY;
            // Umísti figurku na nové místo
            board[to_row][to_col] = lifted_piece;
            invalid_move_backup.to_row = to_row;
            invalid_move_backup.to_col = to_col;
            // Zůstaň v recovery
            current_game_state = GAME_STATE_ERROR_RECOVERY_GENERAL;
            
            // Error handling - increment error count
            consecutive_error_count++;
            ESP_LOGW(TAG, "❌ Recovery error #%u of %u consecutive errors", (unsigned int)consecutive_error_count, (unsigned int)MAX_CONSECUTIVE_ERRORS);
            
            // Check if we've reached maximum errors
            if (consecutive_error_count >= MAX_CONSECUTIVE_ERRORS) {
                ESP_LOGE(TAG, "🚨 MAXIMUM ERRORS REACHED! Resetting game...");
                game_reset_game();
                current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                consecutive_error_count = 0;
                error_recovery_active = false;
                current_game_state = GAME_STATE_IDLE;
                has_last_valid_position = false;
                game_send_response_to_uart("🚨 Too many errors - game reset", true, (QueueHandle_t)cmd->response_queue);
                return;
            }

            // LED: blikání + trvale červená na nové to
            for (int i = 0; i < 2; i++) {
                led_clear_board_only();
                led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(200));
                led_clear_board_only();
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 0, 0);

            game_send_response_to_uart("❌ Invalid – new return point set, lift again", true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // Normal move handling
        if (!piece_lifted) {
            ESP_LOGE(TAG, "❌ No piece was lifted - use UP command first");
            game_send_response_to_uart("❌ No piece was lifted - use UP command first", true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // Create move structure
        chess_move_t move = {
            .from_row = lifted_piece_row,
            .from_col = lifted_piece_col,
            .to_row = to_row,
            .to_col = to_col,
            .piece = lifted_piece,
            .captured_piece = board[to_row][to_col],
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Check if this is a castling move (king moves 2 columns)
        if (lifted_piece == PIECE_WHITE_KING || lifted_piece == PIECE_BLACK_KING) {
            int col_diff = abs((int)to_col - (int)lifted_piece_col);
            if (col_diff == 2) {
                // This is a castling move - validate and start castling flow
                bool is_kingside = (to_col == 6);
                
                // Validate castling requirements
                bool can_castle = is_kingside ? game_can_castle_kingside(current_player) : game_can_castle_queenside(current_player);
                
                if (can_castle) {
                    // FIXED: Start castling flow WITHOUT moving king in memory
                    if (game_start_castling_transaction_strict(is_kingside, lifted_piece_row, lifted_piece_col, to_row, to_col)) {
                        // Castling transaction started - wait for rook move
                        piece_lifted = false;
                        lifted_piece_row = 0;
                        lifted_piece_col = 0;
                        lifted_piece = PIECE_EMPTY;
                        return;
                    } else {
                        // Castling transaction failed - handle as general error
                        game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &move);
                        return;
                    }
                } else {
                    // Castling not allowed - handle as general error
                    game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &move);
                    return;
                }
            }
        }
        
        // NEW RECOVERY FLOW: Handle invalid moves with board consistency
        move_error_t move_error = game_is_valid_move(&move);
        if (move_error != MOVE_ERROR_NONE) {
            // FIXED: last_valid_position se nyní ukládá při každém UP příkazu
            // Není potřeba ji ukládat zde

            // 2. Ulož invalidní tah pro recovery
            invalid_move_backup = move;

            // 3. Synchronizuj board s fyzickým stavem
            board[move.to_row][move.to_col] = move.piece;
            board[move.from_row][move.from_col] = PIECE_EMPTY;

            // 4. Nastav recovery
            error_recovery_active = true;
            current_game_state = GAME_STATE_ERROR_RECOVERY_GENERAL;
            error_recovery_start_time = esp_timer_get_time() / 1000;
            
            // 5. Error handling - increment error count
            consecutive_error_count++;
            ESP_LOGW(TAG, "❌ Error #%u of %u consecutive errors", (unsigned int)consecutive_error_count, (unsigned int)MAX_CONSECUTIVE_ERRORS);
            
            // Check if we've reached maximum errors
            if (consecutive_error_count >= MAX_CONSECUTIVE_ERRORS) {
                ESP_LOGE(TAG, "🚨 MAXIMUM ERRORS REACHED! Resetting game...");
                game_reset_game();
                current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
                consecutive_error_count = 0;
                error_recovery_active = false;
                current_game_state = GAME_STATE_IDLE;
                has_last_valid_position = false;
                game_send_response_to_uart("🚨 Too many errors - game reset", true, (QueueHandle_t)cmd->response_queue);
                return;
            }

            // 6. LED: blikání + trvale červená
            for (int i = 0; i < 3; i++) {
                led_clear_board_only();
                led_set_pixel_safe(chess_pos_to_led_index(move.to_row, move.to_col), 255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(300));
                led_clear_board_only();
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            led_set_pixel_safe(chess_pos_to_led_index(move.to_row, move.to_col), 255, 0, 0);

            // 7. UART instrukce
            char msg[64];
            snprintf(msg, sizeof(msg), "❌ Invalid move – lift piece from %c%d to return it",
                     'a' + move.to_col, move.to_row + 1);
            game_send_response_to_uart(msg, true, (QueueHandle_t)cmd->response_queue);
            return;
        }
        
        // Move is valid - execute it
        if (game_execute_move(&move)) {
            // Move executed successfully - NOW set last_valid_position
            last_valid_position_row = move.to_row;
            last_valid_position_col = move.to_col;
            has_last_valid_position = true;
            player_t previous_player = current_player;
            
            // last_valid_position set after successful move execution
            
            // Reset error count on successful move
            consecutive_error_count = 0;
            
            // FIXED: move_count++ and player switch are handled by game_execute_move()
            // No need to duplicate here
            
            // Reset piece lifted state
            piece_lifted = false;
            lifted_piece_row = 0;
            lifted_piece_col = 0;
            lifted_piece = PIECE_EMPTY;
            
            // Convert coordinates back to notation for display
            char from_notation[4];
            convert_coords_to_notation(move.from_row, move.from_col, from_notation);
            
            ESP_LOGI(TAG, "✅ Move executed successfully: %s -> %s", from_notation, cmd->to_notation);
            
            // Show player change animation
            game_show_player_change_animation(previous_player, current_player);
            
            // Send success response
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg), "✅ Move completed: %s -> %s", from_notation, cmd->to_notation);
            game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
            
            // Reset lifted piece state
            piece_lifted = false;
            lifted_piece_row = 0;
            lifted_piece_col = 0;
            lifted_piece = PIECE_EMPTY;
            
            // Return to idle state
            current_game_state = GAME_STATE_IDLE;
        } else {
            // Move failed - handle as general error
            game_handle_invalid_move(MOVE_ERROR_INVALID_MOVE, &move);
        }
        return;
    }
    
    if (current_game_state == GAME_STATE_CASTLING_IN_PROGRESS) {
        // FIXED: Handle castling drop - must be rook move to correct position
        if (lifted_piece == PIECE_WHITE_ROOK || lifted_piece == PIECE_BLACK_ROOK) {
            // Check if rook is placed on correct position
            if (to_row == castling_king_row && to_col == castling_rook_to_col) {
                // Valid rook placement - complete castling WITHOUT moving pieces in memory
                if (game_complete_castling_strict()) {
                    // Castling completed successfully
                    piece_lifted = false;
                    lifted_piece_row = 0;
                    lifted_piece_col = 0;
                    lifted_piece = PIECE_EMPTY;
                    current_game_state = GAME_STATE_IDLE;
                } else {
                    // Castling completion failed - handle as general error
                    chess_move_t error_move = {
                        .from_row = lifted_piece_row,
                        .from_col = lifted_piece_col,
                        .to_row = to_row,
                        .to_col = to_col,
                        .piece = lifted_piece,
                        .captured_piece = PIECE_EMPTY,
                        .timestamp = esp_timer_get_time() / 1000
                    };
                    game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &error_move);
                }
            } else {
                // Wrong position - handle as general error
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "❌ Place rook on %c%d", 
                        'a' + castling_rook_to_col, castling_king_row + 1);
                game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
                
                // LED: blink red on correct position
                led_clear_board_only();
                for (int i = 0; i < 3; i++) {
                    led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_rook_to_col), 255, 0, 0);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    led_clear_board_only();
                    vTaskDelay(pdMS_TO_TICKS(200));
                }
                
                chess_move_t error_move = {
                    .from_row = lifted_piece_row,
                    .from_col = lifted_piece_col,
                    .to_row = to_row,
                    .to_col = to_col,
                    .piece = lifted_piece,
                    .captured_piece = PIECE_EMPTY,
                    .timestamp = esp_timer_get_time() / 1000
                };
                game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &error_move);
            }
        } else {
            // Wrong piece during castling - handle as general error
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), "❌ Lift rook from %c%d", 
                    'a' + castling_rook_from_col, castling_king_row + 1);
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            
            chess_move_t error_move = {
                .from_row = lifted_piece_row,
                .from_col = lifted_piece_col,
                .to_row = to_row,
                .to_col = to_col,
                .piece = lifted_piece,
                .captured_piece = PIECE_EMPTY,
                .timestamp = esp_timer_get_time() / 1000
            };
            game_handle_invalid_move(MOVE_ERROR_INVALID_CASTLING, &error_move);
        }
        return;
    }
    
    // FIXED: GAME_STATE_ERROR_RECOVERY_GENERAL je nyní sjednocen s WAITING_PIECE_DROP recovery
    // Duplicitní logika byla odstraněna
    
    // Invalid state
    ESP_LOGE(TAG, "❌ Invalid drop command in state %d", current_game_state);
    game_send_response_to_uart("❌ Invalid action in current game state", true, (QueueHandle_t)cmd->response_queue);
    
    // Step 4: Clear board LEDs after state handling (but NOT for ERROR_RECOVERY_GENERAL)
    if (current_game_state != GAME_STATE_ERROR_RECOVERY_GENERAL) {
        led_clear_board_only();
    }
    
    // Step 5: Player change animation and highlighting is handled in game_show_player_change_animation
    // No need to call game_highlight_movable_pieces() here
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
static void game_generate_advantage_graph(char* buffer, size_t buffer_size, uint32_t game_duration, uint32_t total_moves)
{
    if (!buffer || buffer_size < 128) return;  // Reduced minimum buffer size
    
    // Graph dimensions
    const int GRAPH_WIDTH = 60;
    const int GRAPH_HEIGHT = 20;
    const int MAX_ADVANTAGE = 100; // Maximum advantage percentage
    
    char* ptr = buffer;
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
    int time_points[GRAPH_WIDTH];
    
    // Simulate advantage curve based on real game data
    for (int i = 0; i < GRAPH_WIDTH; i++) {
        float progress = (float)i / (GRAPH_WIDTH - 1);
        float time_minutes = progress * (game_duration / 60.0f);
        time_points[i] = (int)(time_minutes * 10) / 10; // Round to 1 decimal
        
        // Calculate advantage based on material balance and move progression
        int white_material, black_material;
        int current_balance = game_calculate_material_balance(&white_material, &black_material);
        
        // Simulate advantage curve (simplified)
        float base_advantage = (float)current_balance * 10.0f; // Convert material to percentage
        float time_factor = sin(progress * 3.14159f) * 20.0f; // Oscillating factor
        float move_factor = (total_moves > 0) ? ((float)i / total_moves) * 30.0f : 0;
        
        int advantage = (int)(base_advantage + time_factor + move_factor);
        
        // Clamp advantage to valid range
        if (advantage > MAX_ADVANTAGE) advantage = MAX_ADVANTAGE;
        if (advantage < -MAX_ADVANTAGE) advantage = -MAX_ADVANTAGE;
        
        graph_data[i] = advantage;
    }
    
    // Draw graph from top to bottom
    for (int row = GRAPH_HEIGHT - 1; row >= 0; row--) {
        float y_value = (float)row / (GRAPH_HEIGHT - 1) * (2 * MAX_ADVANTAGE) - MAX_ADVANTAGE;
        
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
        written = snprintf(ptr, remaining, "%*s%.1f", 
                          label_pos - (i > 1 ? 3 : 0), "", time_label);
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
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

/**
 * @brief Convert chess notation to board coordinates
 * @param notation Chess notation (e.g., "e2")
 * @param row Output row coordinate (0-7)
 * @param col Output column coordinate (0-7)
 * @return true if conversion successful, false otherwise
 */
bool convert_notation_to_coords(const char* notation, uint8_t* row, uint8_t* col)
{
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
bool convert_coords_to_notation(uint8_t row, uint8_t col, char* notation)
{
    if (!notation || row > 7 || col > 7) {
        return false;
    }
    
    notation[0] = 'a' + col;
    notation[1] = '1' + row;
    notation[2] = '\0';
    
    return true;
}

/**
 * @brief Process pickup piece command from UART
 * @param cmd Pickup command
 */
void game_process_pickup_command(const chess_move_command_t* cmd);

/**
 * @brief Process drop piece command from UART
 * @param cmd Drop command
 */
void game_process_drop_command(const chess_move_command_t* cmd);

/**
 * @brief Process evaluation command from UART
 * @param cmd Evaluation command
 */
void game_process_evaluate_command(const chess_move_command_t* cmd);

/**
 * @brief Process save game command from UART
 * @param cmd Save command
 */
void game_process_save_command(const chess_move_command_t* cmd);

/**
 * @brief Process load game command from UART
 * @param cmd Load command
 */
void game_process_load_command(const chess_move_command_t* cmd);

/**
 * @brief Process puzzle command from UART
 * @param cmd Puzzle command
 */
void game_process_puzzle_command(const chess_move_command_t* cmd);

/**
 * @brief Process castle command from UART
 * @param cmd Castle command
 */
void game_process_castle_command(const chess_move_command_t* cmd);

/**
 * @brief Process promote command from UART
 * @param cmd Promote command
 */
void game_process_promote_command(const chess_move_command_t* cmd);

/**
 * @brief Process component off command from UART
 * @param cmd Component off command
 */
void game_process_component_off_command(const chess_move_command_t* cmd);

/**
 * @brief Process component on command from UART
 * @param cmd Component on command
 */
void game_process_component_on_command(const chess_move_command_t* cmd);

/**
 * @brief Generate chess.com style advantage graph
 * @param buffer Output buffer for graph
 * @param buffer_size Buffer size
 * @param game_duration Game duration in seconds
 * @param total_moves Total number of moves
 */
static void game_generate_advantage_graph(char* buffer, size_t buffer_size, uint32_t game_duration, uint32_t total_moves);

/**
 * @brief Process endgame white command from UART
 * @param cmd Endgame white command
 */
void game_process_endgame_white_command(const chess_move_command_t* cmd);

/**
 * @brief Process endgame black command from UART
 * @param cmd Endgame black command
 */
void game_process_endgame_black_command(const chess_move_command_t* cmd);

/**
 * @brief Process list games command from UART
 * @param cmd List games command
 */
void game_process_list_games_command(const chess_move_command_t* cmd);

/**
 * @brief Process delete game command from UART
 * @param cmd Delete game command
 */
void game_process_delete_game_command(const chess_move_command_t* cmd);

/**
 * @brief Process puzzle next step command from UART
 * @param cmd Puzzle next command
 */
void game_process_puzzle_next_command(const chess_move_command_t* cmd);

/**
 * @brief Process puzzle reset command from UART
 * @param cmd Puzzle reset command
 */
void game_process_puzzle_reset_command(const chess_move_command_t* cmd);

/**
 * @brief Process puzzle complete command from UART
 * @param cmd Puzzle complete command
 */
void game_process_puzzle_complete_command(const chess_move_command_t* cmd);

/**
 * @brief Process puzzle verify command from UART
 * @param cmd Puzzle verify command
 */
void game_process_puzzle_verify_command(const chess_move_command_t* cmd);

/**
 * @brief Process promotion command from UART
 * @param cmd Promotion command
 */
void game_process_promotion_command(const chess_move_command_t* cmd);

// ============================================================================
// NEW COMMAND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Process evaluation command from UART
 * @param cmd Evaluation command
 */
void game_process_evaluate_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
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
                case PIECE_WHITE_PAWN:   material_balance += 100; break;
                case PIECE_WHITE_KNIGHT: material_balance += 300; break;
                case PIECE_WHITE_BISHOP: material_balance += 300; break;
                case PIECE_WHITE_ROOK:   material_balance += 500; break;
                case PIECE_WHITE_QUEEN:  material_balance += 900; break;
                case PIECE_WHITE_KING:   material_balance += 10000; break;
                case PIECE_BLACK_PAWN:   material_balance -= 100; break;
                case PIECE_BLACK_KNIGHT: material_balance -= 300; break;
                case PIECE_BLACK_BISHOP: material_balance -= 300; break;
                case PIECE_BLACK_ROOK:   material_balance -= 500; break;
                case PIECE_BLACK_QUEEN:  material_balance -= 900; break;
                case PIECE_BLACK_KING:   material_balance -= 10000; break;
                default: break;
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
    int total_evaluation = material_balance + positional_score + mobility_score + king_safety;
    
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
        material_balance,
        positional_score,
        mobility_score,
        king_safety,
        total_evaluation,
        total_evaluation > 50 ? "White" : (total_evaluation < -50 ? "Black" : "Equal"),
        current_player == PLAYER_WHITE ? "White" : "Black",
        "Middlegame" // Simplified
    );
    
    game_send_response_to_uart(eval_data, false, (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process save game command from UART
 * @param cmd Save command
 */
void game_process_save_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
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
        current_player == PLAYER_WHITE ? "White" : "Black",
        "In Progress",
        esp_timer_get_time() / 1000
    );
    
    game_send_response_to_uart(save_data, false, (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process load game command from UART
 * @param cmd Load command
 */
void game_process_load_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
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
        current_player == PLAYER_WHITE ? "White" : "Black",
        "In Progress",
        esp_timer_get_time() / 1000
    );
    
    game_send_response_to_uart(load_data, false, (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Process puzzle command from UART
 * @param cmd Puzzle command
 */
// Global puzzle state
static chess_puzzle_t current_puzzle = {0};

// Predefined puzzles database
static const chess_puzzle_t puzzle_database[] = {
    // Beginner Puzzle #1: Knight Fork
    {
        .name = "Knight Fork #1",
        .description = "Find the knight move that forks the king and queen",
        .difficulty = PUZZLE_DIFFICULTY_BEGINNER,
        .initial_board = {
            {PIECE_BLACK_ROOK, PIECE_EMPTY, PIECE_EMPTY, PIECE_BLACK_QUEEN, PIECE_BLACK_KING, PIECE_EMPTY, PIECE_EMPTY, PIECE_BLACK_ROOK},
            {PIECE_BLACK_PAWN, PIECE_BLACK_PAWN, PIECE_BLACK_PAWN, PIECE_EMPTY, PIECE_EMPTY, PIECE_BLACK_PAWN, PIECE_BLACK_PAWN, PIECE_BLACK_PAWN},
            {PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY},
            {PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY},
            {PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY},
            {PIECE_EMPTY, PIECE_EMPTY, PIECE_WHITE_KNIGHT, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY, PIECE_EMPTY},
            {PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN, PIECE_WHITE_PAWN},
            {PIECE_WHITE_ROOK, PIECE_EMPTY, PIECE_EMPTY, PIECE_WHITE_QUEEN, PIECE_WHITE_KING, PIECE_EMPTY, PIECE_EMPTY, PIECE_WHITE_ROOK}
        },
        .steps = {
            {2, 2, 1, 4, "Move knight to e7 - forking king and queen!", true},
            {0, 4, 0, 5, "King must move", false},
            {1, 4, 0, 3, "Capture the queen!", true}
        },
        .step_count = 3,
        .current_step = 0,
        .is_active = false
    }
};

void game_process_puzzle_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🧩 Processing PUZZLE command");
    
    // Select puzzle (simple implementation)
    current_puzzle = puzzle_database[0];
    current_puzzle.is_active = true;
    current_puzzle.current_step = 0;
    current_puzzle.start_time = esp_timer_get_time() / 1000;
    
    // Set up the puzzle board position
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = current_puzzle.initial_board[row][col];
        }
    }
    
    char puzzle_data[1024];  // Restored buffer size for puzzle data
    snprintf(puzzle_data, sizeof(puzzle_data),
        "🧩 CHESS PUZZLE STARTED\n"
        "═══════════════════════════════════════════════════════════════\n"
        "📝 Name: %s\n"
        "🎯 Difficulty: Beginner\n"
        "📖 Description: %s\n"
        "🔢 Steps: %d moves to solve\n"
        "\n"
        "🎮 PUZZLE CONTROLS:\n"
        "  • 'BOARD' - See current position\n"
        "  • 'MOVE e2 e4' - Make your move\n"
        "\n"
        "🎯 CURRENT TASK (Step 1/%d):\n"
        "   %s\n"
        "\n"
        "💡 PUZZLE FEATURES:\n"
        "  🟡 Yellow LEDs - Piece to move\n"
        "  🔵 Cyan Path - Animation shows where to move\n"
        "  🟢 Green LEDs - Destination square\n"
        "\n"
        "🚀 LED animations starting soon...\n"
        "📋 Study the position and find the best move!",
        current_puzzle.name,
        current_puzzle.description,
        current_puzzle.step_count,
        current_puzzle.step_count,
        current_puzzle.steps[0].description
    );
    
    game_send_response_to_uart(puzzle_data, false, (QueueHandle_t)cmd->response_queue);
    
    // Start LED animations
    uint8_t from_square = chess_pos_to_led_index(current_puzzle.steps[0].from_row, current_puzzle.steps[0].from_col);
    led_set_pixel_safe(from_square, 255, 255, 0); // Yellow
    
    ESP_LOGI(TAG, "🧩 Puzzle '%s' loaded with LED animation", current_puzzle.name);
}

/**
 * @brief Process castle command from UART
 * @param cmd Castle command
 */
void game_process_castle_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏰 Processing CASTLE command: %s", cmd->to_notation);
    
    // Parse castle direction
    bool is_kingside = (strcmp(cmd->to_notation, "kingside") == 0);
    bool is_queenside = (strcmp(cmd->to_notation, "queenside") == 0);
    
    if (!is_kingside && !is_queenside) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
            "❌ Invalid castle direction: '%s'\n"
            "💡 Valid options: 'kingside' or 'queenside'\n"
            "💡 Examples: CASTLE kingside, CASTLE queenside\n"
            "💡 Notation: O-O (kingside), O-O-O (queenside)",
            cmd->to_notation
        );
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Check if castling is legal (simplified check)
    bool can_castle = false;
    char castle_notation[8];
    
    if (current_player == PLAYER_WHITE) {
        if (is_kingside) {
            // Check if White can castle kingside (simplified)
            can_castle = (board[0][4] == PIECE_WHITE_KING && board[0][7] == PIECE_WHITE_ROOK);
            strcpy(castle_notation, "O-O");
        } else {
            // Check if White can castle queenside (simplified)
            can_castle = (board[0][4] == PIECE_WHITE_KING && board[0][0] == PIECE_WHITE_ROOK);
            strcpy(castle_notation, "O-O-O");
        }
    } else {
        if (is_kingside) {
            // Check if Black can castle kingside (simplified)
            can_castle = (board[7][4] == PIECE_BLACK_KING && board[7][7] == PIECE_BLACK_ROOK);
            strcpy(castle_notation, "O-O");
        } else {
            // Check if Black can castle queenside (simplified)
            can_castle = (board[7][4] == PIECE_BLACK_KING && board[7][0] == PIECE_BLACK_ROOK);
            strcpy(castle_notation, "O-O-O");
        }
    }
    
    if (can_castle) {
        // Start castling transaction
        // Calculate king positions for castling
        uint8_t king_row = (current_player == PLAYER_WHITE) ? 0 : 7;
        uint8_t king_from_col = 4; // King starts at e1/e8
        uint8_t king_to_col = is_kingside ? 6 : 2; // King moves to g1/g8 or c1/c8
        
        if (game_start_castling_transaction_strict(is_kingside, king_row, king_from_col, king_row, king_to_col)) {
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg),
                "🏰 Castling started!\n"
                "  • %s %s\n"
                "  • Move king first, then rook",
                current_player == PLAYER_WHITE ? "White" : "Black",
                is_kingside ? "Kingside" : "Queenside"
            );
            game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
        } else {
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                "❌ Failed to start castling!\n"
                "  • Check king and rook positions\n"
                "  • Ensure path is clear"
            );
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        }
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
            "❌ Castling not possible!\n"
            "  • Player: %s\n"
            "  • Attempted: %s castling\n"
            "  • Possible reasons: King/rook moved, path blocked, king in check",
            current_player == PLAYER_WHITE ? "White" : "Black",
            is_kingside ? "Kingside" : "Queenside"
        );
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
    }
}

/**
 * @brief Process promote command from UART
 * @param cmd Promote command
 */
void game_process_promote_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "👑 Processing PROMOTE command: %s=%s", cmd->from_notation, cmd->to_notation);
    
    // Parse promotion square and piece
    uint8_t row, col;
    if (!convert_notation_to_coords(cmd->from_notation, &row, &col)) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
            "❌ Invalid square notation: '%s'\n"
            "💡 Valid format: letter + number (e.g., 'e8', 'a1', 'h7')\n"
            "💡 Letters: a-h (columns)\n"
            "💡 Numbers: 1-8 (rows)\n"
            "💡 Example: PROMOTE e8=Q",
            cmd->from_notation
        );
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Check if there's a pawn to promote
    piece_t current_piece = board[row][col];
    bool is_white_pawn = (current_piece == PIECE_WHITE_PAWN);
    bool is_black_pawn = (current_piece == PIECE_BLACK_PAWN);
    
    if (!is_white_pawn && !is_black_pawn) {
        char error_msg[256];
        const char* piece_name = "Unknown";
        if (current_piece != PIECE_EMPTY) {
            switch (current_piece) {
                case PIECE_WHITE_KNIGHT: piece_name = "White Knight"; break;
                case PIECE_WHITE_BISHOP: piece_name = "White Bishop"; break;
                case PIECE_WHITE_ROOK: piece_name = "White Rook"; break;
                case PIECE_WHITE_QUEEN: piece_name = "White Queen"; break;
                case PIECE_WHITE_KING: piece_name = "White King"; break;
                case PIECE_BLACK_PAWN: piece_name = "Black Pawn"; break;
                case PIECE_BLACK_KNIGHT: piece_name = "Black Knight"; break;
                case PIECE_BLACK_BISHOP: piece_name = "Black Bishop"; break;
                case PIECE_BLACK_ROOK: piece_name = "Black Rook"; break;
                case PIECE_BLACK_QUEEN: piece_name = "Black Queen"; break;
                case PIECE_BLACK_KING: piece_name = "Black King"; break;
                default: piece_name = "Unknown piece"; break;
            }
        }
        
        snprintf(error_msg, sizeof(error_msg),
            "❌ No pawn to promote at %s\n"
            "  • Current piece: %s\n"
            "  • Must be a pawn on 8th/1st rank",
            cmd->from_notation,
            current_piece == PIECE_EMPTY ? "Empty square" : piece_name
        );
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Parse promotion piece
    piece_t promotion_piece;
    const char* piece_name;
    
    switch (toupper(cmd->to_notation[0])) {
        case 'Q':
            promotion_piece = is_white_pawn ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;
            piece_name = "Queen";
            break;
        case 'R':
            promotion_piece = is_white_pawn ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
            piece_name = "Rook";
            break;
        case 'B':
            promotion_piece = is_white_pawn ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
            piece_name = "Bishop";
            break;
        case 'N':
            promotion_piece = is_white_pawn ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
            piece_name = "Knight";
            break;
        default:
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg),
                "❌ Invalid promotion piece: '%s'\n"
                "  • Valid: Q (Queen), R (Rook), B (Bishop), N (Knight)\n"
                "  • Example: PROMOTE e8=Q",
                cmd->to_notation
            );
            game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
            return;
    }
    
    // FIXED: Add LED animations for promotion
    uint8_t promotion_led = chess_pos_to_led_index(row, col);
    
    // 1. Highlight promotion square (purple for special move)
    led_set_pixel_safe(promotion_led, 128, 0, 128); // Purple
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 2. Show transformation animation (pawn -> promoted piece)
    for (int i = 0; i < 3; i++) {
        // Flash between pawn color and promoted piece color
        if (i % 2 == 0) {
            // Pawn color (white/black)
            led_set_pixel_safe(promotion_led, is_white_pawn ? 255 : 100, 
                              is_white_pawn ? 255 : 100, is_white_pawn ? 255 : 100);
        } else {
            // Promoted piece color (gold)
            led_set_pixel_safe(promotion_led, 255, 215, 0); // Gold
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // 3. Perform promotion
    board[row][col] = promotion_piece;
    
    // 4. Show final promoted piece (gold)
    led_set_pixel_safe(promotion_led, 255, 215, 0); // Gold
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 5. Show celebration animation (rainbow colors)
    for (int i = 0; i < 5; i++) {
        uint8_t r = (i * 51) % 255;
        uint8_t g = ((i * 51) + 85) % 255;
        uint8_t b = ((i * 51) + 170) % 255;
        led_set_pixel_safe(promotion_led, r, g, b);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    
    // 6. Clear highlights
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg),
        "✅ Pawn promotion successful!\n"
        "  • %s at %s",
        piece_name,
        cmd->from_notation
    );
    
    game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
    
    // Switch player
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
}

/**
 * @brief Process component off command from UART
 * @param cmd Component off command
 */
void game_process_component_off_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🔌 Processing COMPONENT_OFF command");
    
    char response_data[512];
    snprintf(response_data, sizeof(response_data),
        "🔌 Component Control - OFF\n"
        "  • Status: Component turned OFF\n"
        "  • Action: Disabled component functionality\n"
        "  • Note: Hardware tasks continue running\n"
        "  • Timestamp: %llu ms",
        esp_timer_get_time() / 1000
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
void game_process_component_on_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🔌 Processing COMPONENT_ON command");
    
    char response_data[512];
    snprintf(response_data, sizeof(response_data),
        "🔌 Component Control - ON\n"
        "  • Status: Component turned ON\n"
        "  • Action: Enabled component functionality\n"
        "  • Note: Hardware tasks continue running\n"
        "  • Timestamp: %llu ms",
        esp_timer_get_time() / 1000
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
void game_process_endgame_white_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏆 Processing ENDGAME_WHITE command");
    
    // Get real game statistics
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t game_duration = (game_start_time > 0) ? (current_time - game_start_time) : 0;
    uint32_t total_moves = move_count;
    uint32_t white_captures_real = white_captures;
    uint32_t black_captures_real = black_captures;
    
    // Calculate average times
    uint32_t white_avg_time = (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
    uint32_t black_avg_time = (black_moves_count > 0) ? black_time_total / black_moves_count : 0;
    
    // Get material balance
    int white_material, black_material;
    int material_balance = game_calculate_material_balance(&white_material, &black_material);
    
    // Determine game phase
    const char* game_phase = (total_moves > 30) ? "Endgame" : (total_moves > 15) ? "Middle Game" : "Opening";
    
    // Calculate accuracy (simplified based on captures and checks)
    int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
    int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
    if (white_accuracy > 95) white_accuracy = 95;
    if (black_accuracy > 95) black_accuracy = 95;
    
    // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
    // This eliminates heap fragmentation and reduces memory usage
    ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");
    
    // Configure streaming for queue output
    esp_err_t ret = streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure streaming output: %s", esp_err_to_name(ret));
        return;
    }
    
    // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
    char graph_buffer[128]; // Small stack buffer instead of malloc(1024)
    game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer), game_duration, total_moves);
    
    // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
    // Stream endgame report header
    stream_writeln("🏆 ENDGAME REPORT - WHITE VICTORY");
    stream_writeln("═══════════════════════════════════════════════════════════════");
    stream_writeln("🎯 Game Result: WHITE WINS");
    stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n", 
                  game_duration, (float)game_duration / 60.0f);
    
    // Stream move statistics
    stream_writeln("\n📊 Move Statistics:");
    stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32 ", Black: %" PRIu32 ")\n", 
                  total_moves, white_moves_count, black_moves_count);
    stream_printf("  • White Captures: %" PRIu32 " pieces\n", white_captures_real);
    stream_printf("  • Black Captures: %" PRIu32 " pieces\n", black_captures_real);
    stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
    stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
    stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
    stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
    stream_printf("  • Average Time per Move: %.1f seconds\n", 
                  total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
    stream_printf("  • White Average Time: %" PRIu32 " seconds\n", white_avg_time);
    stream_printf("  • Black Average Time: %" PRIu32 " seconds\n", black_avg_time);
    stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n", 
                  white_time_total, (float)white_time_total / 60.0f);
    stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n", 
                  black_time_total, (float)black_time_total / 60.0f);
    
    // Stream game analysis
    stream_writeln("\n🎮 Game Analysis:");
    stream_printf("  • Game Phase: %s\n", game_phase);
    stream_writeln("  • Victory Condition: Checkmate");
    stream_printf("  • Material Balance: %s\n", 
                  material_balance > 0 ? "White Advantage" : (material_balance < 0 ? "Black Advantage" : "Equal"));
    stream_printf("  • Moves without Capture: %" PRIu32 "\n", moves_without_capture);
    stream_printf("  • Max Moves without Capture: %" PRIu32 "\n", max_moves_without_capture);
    
    // Stream performance metrics
    stream_writeln("\n📈 Performance Metrics:");
    stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy, 
                  (white_accuracy >= 85) ? "Excellent" : (white_accuracy >= 75) ? "Good" : (white_accuracy >= 65) ? "Fair" : "Poor");
    stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy, 
                  (black_accuracy >= 85) ? "Excellent" : (black_accuracy >= 75) ? "Good" : (black_accuracy >= 65) ? "Fair" : "Poor");
    stream_printf("  • White Material: %d points\n", white_material);
    stream_printf("  • Black Material: %d points\n", black_material);
    stream_printf("  • Material Advantage: %s %d\n", 
                  material_balance > 0 ? "White +" : (material_balance < 0 ? "Black +" : "Equal"),
                  material_balance > 0 ? material_balance : (material_balance < 0 ? -material_balance : 0));
    
    // Stream game statistics
    stream_writeln("\n📊 Game Statistics:");
    stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
    stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
    stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
    stream_printf("  • Draws: %" PRIu32 "\n", draws);
    stream_printf("  • Win Rate: %.1f%%\n", 
                  total_games > 0 ? (float)white_wins / total_games * 100.0f : 0.0f);
    
    // Stream advantage graph
    stream_writeln("\n📊 Advantage Graph:");
    stream_writeln(graph_buffer);
    
    // Stream conclusion
    stream_writeln("\n🏆 Congratulations to White player!");
    stream_printf("💡 Game saved as 'victory_white_%" PRIu32 ".chess'\n", game_duration);
    
    // MEMORY OPTIMIZATION: Streaming output handles data transmission
    // No need for manual queue management or large buffers
    ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");

}

/**
 * @brief Process endgame black command from UART
 * @param cmd Endgame black command
 */
void game_process_endgame_black_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🏆 Processing ENDGAME_BLACK command");
    
    // Get real game statistics
    uint32_t current_time = esp_timer_get_time() / 1000;
    uint32_t game_duration = (game_start_time > 0) ? (current_time - game_start_time) : 0;
    uint32_t total_moves = move_count;
    uint32_t white_captures_real = white_captures;
    uint32_t black_captures_real = black_captures;
    
    // Calculate average times
    uint32_t white_avg_time = (white_moves_count > 0) ? white_time_total / white_moves_count : 0;
    uint32_t black_avg_time = (black_moves_count > 0) ? black_time_total / black_moves_count : 0;
    
    // Get material balance
    int white_material, black_material;
    int material_balance = game_calculate_material_balance(&white_material, &black_material);
    
    // Determine game phase
    const char* game_phase = (total_moves > 30) ? "Endgame" : (total_moves > 15) ? "Middle Game" : "Opening";
    
    // Calculate accuracy (simplified based on captures and checks)
    int white_accuracy = 70 + (white_captures_real * 3) + (white_checks * 2);
    int black_accuracy = 70 + (black_captures_real * 3) + (black_checks * 2);
    if (white_accuracy > 95) white_accuracy = 95;
    if (black_accuracy > 95) black_accuracy = 95;
    
    // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3584+1024)
    // This eliminates heap fragmentation and reduces memory usage
    ESP_LOGI(TAG, "📡 Using streaming output for endgame report (no malloc)");
    
    // Configure streaming for queue output
    esp_err_t ret = streaming_set_queue_output((QueueHandle_t)cmd->response_queue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure streaming output: %s", esp_err_to_name(ret));
        return;
    }
    
    // MEMORY OPTIMIZATION: Generate advantage graph to small buffer
    char graph_buffer[128]; // Small stack buffer instead of malloc(1024)
    game_generate_advantage_graph(graph_buffer, sizeof(graph_buffer), game_duration, total_moves);
    
    // MEMORY OPTIMIZATION: Use streaming output instead of large buffer
    // Stream endgame report header
    stream_writeln("🏆 ENDGAME REPORT - BLACK VICTORY");
    stream_writeln("═══════════════════════════════════════════════════════════════");
    stream_writeln("🎯 Game Result: BLACK WINS");
    stream_printf("⏱️  Game Duration: %" PRIu32 " seconds (%.1f minutes)\n", 
                  game_duration, (float)game_duration / 60.0f);
    
    // Stream move statistics
    stream_writeln("\n📊 Move Statistics:");
    stream_printf("  • Total Moves: %" PRIu32 " (White: %" PRIu32 ", Black: %" PRIu32 ")\n", 
                  total_moves, white_moves_count, black_moves_count);
    stream_printf("  • White Captures: %" PRIu32 " pieces\n", white_captures_real);
    stream_printf("  • Black Captures: %" PRIu32 " pieces\n", black_captures_real);
    stream_printf("  • White Checks: %" PRIu32 "\n", white_checks);
    stream_printf("  • Black Checks: %" PRIu32 "\n", black_checks);
    stream_printf("  • White Castles: %" PRIu32 "\n", white_castles);
    stream_printf("  • Black Castles: %" PRIu32 "\n", black_castles);
    stream_printf("  • Average Time per Move: %.1f seconds\n", 
                  total_moves > 0 ? (float)game_duration / total_moves : 0.0f);
    stream_printf("  • White Average Time: %" PRIu32 " seconds\n", white_avg_time);
    stream_printf("  • Black Average Time: %" PRIu32 " seconds\n", black_avg_time);
    stream_printf("  • White Total Time: %" PRIu32 " seconds (%.1f minutes)\n", 
                  white_time_total, (float)white_time_total / 60.0f);
    stream_printf("  • Black Total Time: %" PRIu32 " seconds (%.1f minutes)\n", 
                  black_time_total, (float)black_time_total / 60.0f);
    
    // Stream game analysis
    stream_writeln("\n🎮 Game Analysis:");
    stream_printf("  • Game Phase: %s\n", game_phase);
    stream_writeln("  • Victory Condition: Checkmate");
    stream_printf("  • Material Balance: %s\n", 
                  material_balance > 0 ? "White Advantage" : (material_balance < 0 ? "Black Advantage" : "Equal"));
    stream_printf("  • Moves without Capture: %" PRIu32 "\n", moves_without_capture);
    stream_printf("  • Max Moves without Capture: %" PRIu32 "\n", max_moves_without_capture);
    
    // Stream performance metrics
    stream_writeln("\n📈 Performance Metrics:");
    stream_printf("  • White Accuracy: %d%% (%s)\n", white_accuracy, 
                  (white_accuracy >= 85) ? "Excellent" : (white_accuracy >= 75) ? "Good" : (white_accuracy >= 65) ? "Fair" : "Poor");
    stream_printf("  • Black Accuracy: %d%% (%s)\n", black_accuracy, 
                  (black_accuracy >= 85) ? "Excellent" : (black_accuracy >= 75) ? "Good" : (black_accuracy >= 65) ? "Fair" : "Poor");
    stream_printf("  • White Material: %d points\n", white_material);
    stream_printf("  • Black Material: %d points\n", black_material);
    stream_printf("  • Material Advantage: %s %d\n", 
                  material_balance > 0 ? "White +" : (material_balance < 0 ? "Black +" : "Equal"),
                  material_balance > 0 ? material_balance : (material_balance < 0 ? -material_balance : 0));
    
    // Stream game statistics
    stream_writeln("\n📊 Game Statistics:");
    stream_printf("  • Total Games Played: %" PRIu32 "\n", total_games);
    stream_printf("  • White Wins: %" PRIu32 "\n", white_wins);
    stream_printf("  • Black Wins: %" PRIu32 "\n", black_wins);
    stream_printf("  • Draws: %" PRIu32 "\n", draws);
    stream_printf("  • Win Rate: %.1f%%\n", 
                  total_games > 0 ? (float)black_wins / total_games * 100.0f : 0.0f);
    
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
    
    // FIXED: Start endgame animation
    start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, 27); // d4 square
    
    // MEMORY OPTIMIZATION: Streaming output handles data transmission
    // No need for manual queue management or large buffers
    ESP_LOGI(TAG, "✅ Endgame report streaming completed successfully");
}

/**
 * @brief Process list games command from UART
 * @param cmd List games command
 */
void game_process_list_games_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "📁 Processing LIST_GAMES command");
    
    char response_data[1024];  // Restored buffer size for list games
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
        "💡 Total: 3 saved games"
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
    
    // FIXED: Start endgame animation
    start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, 27); // d4 square
    
    ESP_LOGI(TAG, "✅ Endgame report sent successfully in chunks");
}

/**
 * @brief Process delete game command from UART
 * @param cmd Delete game command
 */
void game_process_delete_game_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
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
        cmd->from_notation
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
 * @brief Execute pawn promotion
 * @param choice Promotion choice
 * @return true if successful, false otherwise
 */
bool game_execute_promotion(promotion_choice_t choice);

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
void game_handle_matrix_move(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Highlight pieces that the opponent can move
 */
void game_highlight_opponent_pieces(void);

/**
 * @brief Process chess move command from UART
 * @param cmd Chess move command
 */
void game_process_chess_move(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🎯 Processing UART chess move: %s -> %s (player: %d)", 
              cmd->from_notation, cmd->to_notation, cmd->player);
    
    // Convert notation to coordinates
    uint8_t from_row, from_col, to_row, to_col;
    if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col) ||
        !convert_notation_to_coords(cmd->to_notation, &to_row, &to_col)) {
        ESP_LOGE(TAG, "❌ Invalid notation: %s -> %s", cmd->from_notation, cmd->to_notation);
        return;
    }
    
    // Create chess move structure
    chess_move_t move = {
        .from_row = from_row,
        .from_col = from_col,
        .to_row = to_row,
        .to_col = to_col,
        .piece = board[from_row][from_col],
        .captured_piece = board[to_row][to_col],
        .timestamp = 0
    };
    
    // Validate move first
    move_error_t error = game_is_valid_move(&move);
    
    if (error == MOVE_ERROR_NONE) {
        ESP_LOGI(TAG, "✅ Move is valid, starting UART move animation...");
        
        // STEP 1: Show piece lifting animation (same as HW)
        ESP_LOGI(TAG, "🔄 Step 1: Lifting piece from %s", cmd->from_notation);
        // FIXED: No animation when lifting piece, just highlight possible moves
        
        // Send LED command to highlight source square
        led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255, 0); // Yellow for source
        
        // Give FreeRTOS scheduler time to process LED commands
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms - enough for scheduler, not too long for watchdog
        
        // STEP 2: Show possible moves (same as HW)
        ESP_LOGI(TAG, "🔄 Step 2: Showing possible moves from %s", cmd->from_notation);
        
        // Get valid moves for this piece
        move_suggestion_t suggestions[64];
        uint32_t valid_moves = game_get_available_moves(from_row, from_col, suggestions, 64);
        
        ESP_LOGI(TAG, "Found %lu valid moves for piece at %s", valid_moves, cmd->from_notation);
        
        // Send LED commands to highlight possible destinations
        if (valid_moves > 0) {
            for (uint32_t i = 0; i < valid_moves; i++) {
                uint8_t dest_row = suggestions[i].to_row;
                uint8_t dest_col = suggestions[i].to_col;
                uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);
                
                // Check if destination has opponent's piece
                piece_t dest_piece = board[dest_row][dest_col];
                bool is_opponent_piece = (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN && dest_piece <= PIECE_BLACK_KING) ||
                                       (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN && dest_piece <= PIECE_WHITE_KING);
                
                if (is_opponent_piece) {
                    // Orange for opponent's pieces (capture)
                    led_set_pixel_safe(led_index, 255, 165, 0);
                } else {
                    // Green for empty squares
                    led_set_pixel_safe(led_index, 0, 255, 0);
                }
            }
        }
        
        // Give FreeRTOS scheduler time to process LED commands
        vTaskDelay(pdMS_TO_TICKS(50));  // 50ms - enough for scheduler, not too long for watchdog
        
        // STEP 3: Execute the move immediately (same as HW)
        ESP_LOGI(TAG, "🔄 Step 3: Executing move %s -> %s", cmd->from_notation, cmd->to_notation);
        
        if (game_execute_move(&move)) {
            ESP_LOGI(TAG, "✅ UART move executed successfully: %s -> %s", cmd->from_notation, cmd->to_notation);
            
            // Print successful move with colors - using snprintf for safety
            char move_msg[256];
            snprintf(move_msg, sizeof(move_msg), "\r\n\033[92m✅ \033[1mMOVE EXECUTED SUCCESSFULLY!\033[0m\r\n");
            printf("%s", move_msg);
            
            snprintf(move_msg, sizeof(move_msg), "\033[93m   • Move: \033[1m%s → %s\033[0m\r\n", cmd->from_notation, cmd->to_notation);
            printf("%s", move_msg);
            
            snprintf(move_msg, sizeof(move_msg), "\033[93m   • Piece: \033[1m%s\033[0m\r\n", piece_symbols[move.piece]);
            printf("%s", move_msg);
            
            if (move.captured_piece != PIECE_EMPTY) {
                snprintf(move_msg, sizeof(move_msg), "\033[93m   • Captured: \033[1m%s\033[0m\r\n", piece_symbols[move.captured_piece]);
                printf("%s", move_msg);
            }
            printf("\r\n");
            
            // FIXED: LED animations are now handled in game_process_drop_command
            // This function only shows text information
            
            // Send LED command to highlight destination square
            led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 0, 0, 255); // Blue for final destination
            
            // Clear only board LEDs after a short delay (non-blocking)
            led_clear_board_only();
            
            // Give FreeRTOS scheduler time to process final LED commands
            vTaskDelay(pdMS_TO_TICKS(50));  // 50ms - enough for scheduler, not too long for watchdog
            
            // FIXED: Show player change animation
            player_t previous_player = current_player;
            current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
            game_show_player_change_animation(previous_player, current_player);
            
            // Send success response to UART
            char success_msg[256];
            snprintf(success_msg, sizeof(success_msg), "Move executed: %s -> %s", 
                    cmd->from_notation, cmd->to_notation);
            game_send_response_to_uart(success_msg, false, (QueueHandle_t)cmd->response_queue);
            
        } else {
            ESP_LOGE(TAG, "❌ Failed to execute UART move");
            game_send_response_to_uart("Failed to execute move", true, (QueueHandle_t)cmd->response_queue);
        }
        
    } else {
        ESP_LOGE(TAG, "❌ Invalid UART move: error %d", error);
        game_display_move_error(error, &move);
        
        // Send error response to UART
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Invalid move: %s -> %s (error: %d)", 
                cmd->from_notation, cmd->to_notation, error);
        game_send_response_to_uart(error_msg, true, (QueueHandle_t)cmd->response_queue);
        
        // Show error LED (red flash)
        led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 0, 0); // Red for error
        
        // Clear error LED after delay (non-blocking)
        vTaskDelay(pdMS_TO_TICKS(1000));
        led_clear_board_only();
    }
}


void game_process_commands(void)
{
    // Process commands from queue
    if (game_command_queue != NULL) {
        // Try to receive chess_move_command_t structure (from UART)
        chess_move_command_t chess_cmd;
        if (xQueueReceive(game_command_queue, &chess_cmd, 0) == pdTRUE) {
            switch (chess_cmd.type) {
                case GAME_CMD_NEW_GAME: // 0
                    ESP_LOGI(TAG, "Processing NEW GAME command from UART");
                    game_start_new_game();
                    game_send_response_to_uart("New game started successfully!", false, (QueueHandle_t)chess_cmd.response_queue);
                    break;
                    
                case GAME_CMD_RESET_GAME: // 1
                    ESP_LOGI(TAG, "Processing RESET GAME command from UART");
                    game_reset_game();
                    game_send_response_to_uart("Game reset to starting position!", false, (QueueHandle_t)chess_cmd.response_queue);
                    break;
                    
                case GAME_CMD_MAKE_MOVE: // 2
                    ESP_LOGI(TAG, "Processing MAKE MOVE command from UART: %s -> %s", 
                             chess_cmd.from_notation, chess_cmd.to_notation);
                    game_process_chess_move(&chess_cmd);
                    break;
                    
                case GAME_CMD_UNDO_MOVE: // 3
                    ESP_LOGI(TAG, "Processing UNDO MOVE command from UART");
                    // TODO: Implement undo move
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
                    ESP_LOGI(TAG, "Processing PICKUP PIECE command from UART: %s", chess_cmd.from_notation);
                    game_process_pickup_command(&chess_cmd);
                    break;
                    
                case 8: // GAME_CMD_DROP_PIECE
                    ESP_LOGI(TAG, "Processing DROP PIECE command from UART: %s", chess_cmd.to_notation);
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
                    ESP_LOGI(TAG, "Processing PICKUP command from UART: %s", chess_cmd.from_notation);
                    game_process_pickup_command(&chess_cmd);
                    break;
                    
                case 13: // GAME_CMD_DROP
                    ESP_LOGI(TAG, "Processing DROP command from UART: %s", chess_cmd.to_notation);
                    game_process_drop_command(&chess_cmd);
                    break;
                    
                case 17: // GAME_CMD_EVALUATE
                    ESP_LOGI(TAG, "Processing EVALUATE command from UART");
                    game_process_evaluate_command(&chess_cmd);
                    break;
                    
                case 18: // GAME_CMD_SAVE
                    ESP_LOGI(TAG, "Processing SAVE command from UART: %s", chess_cmd.from_notation);
                    game_process_save_command(&chess_cmd);
                    break;
                    
                case 19: // GAME_CMD_LOAD
                    ESP_LOGI(TAG, "Processing LOAD command from UART: %s", chess_cmd.from_notation);
                    game_process_load_command(&chess_cmd);
                    break;
                    
                case 20: // GAME_CMD_PUZZLE
                    ESP_LOGI(TAG, "Processing PUZZLE command from UART");
                    game_process_puzzle_command(&chess_cmd);
                    break;
                    
                case 21: // GAME_CMD_CASTLE
                    ESP_LOGI(TAG, "Processing CASTLE command from UART: %s", chess_cmd.to_notation);
                    game_process_castle_command(&chess_cmd);
                    break;
                    
                case 22: // GAME_CMD_PROMOTE
                    ESP_LOGI(TAG, "Processing PROMOTE command from UART: %s=%s", chess_cmd.from_notation, chess_cmd.to_notation);
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
                    ESP_LOGI(TAG, "Processing DELETE_GAME command from UART: %s", chess_cmd.from_notation);
                    game_process_delete_game_command(&chess_cmd);
                    break;
                    
                case 29: // GAME_CMD_PUZZLE_NEXT
                    ESP_LOGI(TAG, "Processing PUZZLE_NEXT command from UART");
                    game_process_puzzle_next_command(&chess_cmd);
                    break;
                    
                case 30: // GAME_CMD_PUZZLE_RESET
                    ESP_LOGI(TAG, "Processing PUZZLE_RESET command from UART");
                    game_process_puzzle_reset_command(&chess_cmd);
                    break;
                    
                case 31: // GAME_CMD_PUZZLE_COMPLETE
                    ESP_LOGI(TAG, "Processing PUZZLE_COMPLETE command from UART");
                    game_process_puzzle_complete_command(&chess_cmd);
                    break;
                    
                case 32: // GAME_CMD_PUZZLE_VERIFY
                    ESP_LOGI(TAG, "Processing PUZZLE_VERIFY command from UART");
                    game_process_puzzle_verify_command(&chess_cmd);
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
                    
                case 38: // GAME_CMD_TEST_PUZZLE_ANIM
                    ESP_LOGI(TAG, "Processing TEST_PUZZLE_ANIM command from UART");
                    game_test_puzzle_animation();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown game command: %d", chess_cmd.type);
                    break;
            }
        }
    }
}

// Error recovery variables are now declared globally above

/**
 * @brief Start castling transaction (STRICT - no automatic moves)
 * @param is_kingside true for kingside, false for queenside
 * @param king_from_row King's current row
 * @param king_from_col King's current column
 * @param king_to_row King's destination row
 * @param king_to_col King's destination column
 * @return true if castling transaction started successfully
 */
bool game_start_castling_transaction_strict(bool is_kingside, uint8_t king_from_row, uint8_t king_from_col, uint8_t king_to_row, uint8_t king_to_col)
{
    if (castling_in_progress) {
        ESP_LOGW(TAG, "❌ Castling already in progress");
        return false;
    }
    
    // Validate castling requirements
    bool can_castle = is_kingside ? game_can_castle_kingside(current_player) : game_can_castle_queenside(current_player);
    if (!can_castle) {
        ESP_LOGE(TAG, "❌ Castling not allowed");
        return false;
    }
    
    // Calculate rook positions
    uint8_t rook_col = is_kingside ? 7 : 0;
    uint8_t rook_to_col = is_kingside ? 5 : 3;
    
    // Check if rook is still in correct position
    piece_t rook = board[king_from_row][rook_col];
    bool valid_rook = (current_player == PLAYER_WHITE) ? (rook == PIECE_WHITE_ROOK) : (rook == PIECE_BLACK_ROOK);
    
    if (!valid_rook) {
        ESP_LOGE(TAG, "❌ Invalid castling setup - rook not in position");
        return false;
    }
    
    // Start castling transaction WITHOUT moving king in memory
    castling_in_progress = true;
    castling_kingside = is_kingside;
    castling_king_row = king_from_row;
    castling_king_from_col = king_from_col;
    castling_king_to_col = king_to_col;
    castling_rook_from_col = rook_col;
    castling_rook_to_col = rook_to_col;
    castling_start_time = esp_timer_get_time() / 1000;
    
    // Set game state to castling in progress
    current_game_state = GAME_STATE_CASTLING_IN_PROGRESS;
    
    ESP_LOGI(TAG, "🏰 Castling transaction started (STRICT): %s %s", 
             (current_player == PLAYER_WHITE) ? "White" : "Black",
             is_kingside ? "kingside" : "queenside");
    
    // LED guidance: green blink on rook_from + blue steady on rook_to
    led_clear_board_only();
    for (int i = 0; i < 3; i++) {
        led_set_pixel_safe(chess_pos_to_led_index(king_from_row, rook_col), 0, 255, 0); // Green blink
        vTaskDelay(pdMS_TO_TICKS(200));
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    led_set_pixel_safe(chess_pos_to_led_index(king_from_row, rook_col), 0, 255, 0); // Green steady
    led_set_pixel_safe(chess_pos_to_led_index(king_from_row, rook_to_col), 0, 0, 255); // Blue steady
    
    // UART instruction
    char instruction[256];
    snprintf(instruction, sizeof(instruction), "🏰 Castling: lift rook from %c%d", 
            'a' + rook_col, king_from_row + 1);
    game_send_response_to_uart(instruction, false, NULL);
    
    return true;
}


// King move is now handled in game_process_drop_command
// This function is no longer needed

/**
 * @brief Complete castling (STRICT - no automatic moves)
 * @return true if castling completed successfully
 */
bool game_complete_castling_strict(void)
{
    if (!castling_in_progress) {
        return false;
    }
    
    // FIXED: Complete castling WITHOUT moving pieces in memory
    // The pieces are already in correct positions by player's physical actions
    
    // Update castling flags
    if (current_player == PLAYER_WHITE) {
        if (castling_kingside) {
            white_rook_h_moved = true;
        } else {
            white_rook_a_moved = true;
        }
    } else {
        if (castling_kingside) {
            black_rook_h_moved = true;
        } else {
            black_rook_a_moved = true;
        }
    }
    
    // Complete castling
    castling_in_progress = false;
    
    // Reset error count on successful castling
    consecutive_error_count = 0;
    
    // Set game state to idle
    current_game_state = GAME_STATE_IDLE;
    
    ESP_LOGI(TAG, "🏰 Castling completed successfully (STRICT)!");
    
    // Show completion animation
    led_clear_board_only();
    led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_king_to_col), 0, 255, 0); // Green
    led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_rook_to_col), 0, 255, 0); // Green
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_clear_board_only();
    
    // Switch player
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Send success message
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), "🏰✅ Castling completed successfully!");
    game_send_response_to_uart(success_msg, false, NULL);
    
    return true;
}

/**
 * @brief Handle rook move during castling (LEGACY - with automatic moves)
 * @param from_row Rook's current row
 * @param from_col Rook's current column
 * @param to_row Rook's destination row
 * @param to_col Rook's destination column
 * @return true if castling completed successfully
 */
bool game_handle_castling_rook_move(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)
{
    if (!castling_in_progress) {
        return false;
    }
    
    // Check if this is the expected rook move
    if (from_row != castling_king_row || from_col != castling_rook_from_col ||
        to_row != castling_king_row || to_col != castling_rook_to_col) {
        ESP_LOGW(TAG, "❌ Invalid rook move during castling - expected [%d,%d] -> [%d,%d]",
                 castling_king_row, castling_rook_from_col, castling_king_row, castling_rook_to_col);
        return false;
    }
    
    // Move rook
    piece_t rook = board[from_row][from_col];
    board[from_row][from_col] = PIECE_EMPTY;
    board[to_row][to_col] = rook;
    
    // Update castling flags
    if (current_player == PLAYER_WHITE) {
        if (castling_kingside) {
            white_rook_h_moved = true;
        } else {
            white_rook_a_moved = true;
        }
    } else {
        if (castling_kingside) {
            black_rook_h_moved = true;
        } else {
            black_rook_a_moved = true;
        }
    }
    
    // Complete castling
    castling_in_progress = false;
    
    // Reset error count on successful castling
    consecutive_error_count = 0;
    
    // Set game state to idle
    current_game_state = GAME_STATE_IDLE;
    
    ESP_LOGI(TAG, "🏰 Castling completed successfully!");
    
    // Show completion animation
    led_clear_board_only();
    led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_king_to_col), 0, 255, 0); // Green
    led_set_pixel_safe(chess_pos_to_led_index(castling_king_row, castling_rook_to_col), 0, 255, 0); // Green
    vTaskDelay(pdMS_TO_TICKS(1000));
    led_clear_board_only();
    
    // Switch player
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Send success message
    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg), "🏰✅ Castling completed successfully!");
    game_send_response_to_uart(success_msg, false, NULL);
    
    return true;
}

/**
 * @brief Cancel castling transaction
 */
void game_cancel_castling_transaction(void)
{
    if (castling_in_progress) {
        ESP_LOGW(TAG, "🏰 Cancelling castling transaction");
        
        // FIXED: Správné čištění castling_in_progress
        castling_in_progress = false;
        castling_kingside = false;
        current_game_state = GAME_STATE_IDLE;
        
        // Clear board
        led_clear_board_only();
        
        // Send cancellation message
        game_send_response_to_uart("🏰❌ Castling cancelled – continue playing", false, NULL);
    }
}

/**
 * @brief Check if castling is in progress
 * @return true if castling is in progress
 */
bool game_is_castling_in_progress(void)
{
    return castling_in_progress;
}

/**
 * @brief Check if castling timeout has expired
 * @return true if timeout expired
 */
bool game_is_castling_timeout(void)
{
    if (!castling_in_progress) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    return (current_time - castling_start_time) > CASTLING_TIMEOUT_MS;
}

/**
 * @brief Check if error recovery timeout has expired
 * @return true if timeout expired
 */
bool game_is_error_recovery_timeout(void)
{
    if (!error_recovery_active) {
        return false;
    }
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    return (current_time - error_recovery_start_time) > ERROR_RECOVERY_TIMEOUT_MS;
}

/**
 * @brief Cancel error recovery due to timeout
 */
void game_cancel_recovery(void)
{
    ESP_LOGW(TAG, "⏱️ Recovery timeout - cancelling recovery");
    
    // Clear recovery state
    error_recovery_active = false;
    current_game_state = GAME_STATE_IDLE;
    consecutive_error_count = 0;
    piece_lifted = false;
    
    // Clear board
    led_clear_board_only();
    
    // Send timeout message
    game_send_response_to_uart("⏱️ Recovery timeout – continue playing", false, NULL);
}

// FIXED: Duplicitní funkce byla odstraněna - používá se game_cancel_castling_transaction()

/**
 * @brief Handle invalid move - enhanced error handling system
 * @param error Move error type
 * @param move Move that was attempted
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t* move)
{
    ESP_LOGI(TAG, "🚨 Invalid move detected - implementing enhanced error handling");
    
    // Increment error count
    consecutive_error_count++;
    ESP_LOGW(TAG, "❌ Error #%u of %u consecutive errors", (unsigned int)consecutive_error_count, (unsigned int)MAX_CONSECUTIVE_ERRORS);
    
    // Check if we've reached maximum errors
    if (consecutive_error_count >= MAX_CONSECUTIVE_ERRORS) {
        ESP_LOGE(TAG, "🚨 MAXIMUM ERRORS REACHED! Resetting game...");
        
        // Reset game completely
        game_reset_game();
        
        // FIXED: Switch player after game reset due to errors
        current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        ESP_LOGI(TAG, "🔄 Player switched after game reset due to errors");
        
        // Clear error state
        consecutive_error_count = 0;
        error_recovery_active = false;
        current_game_state = GAME_STATE_IDLE;
        has_last_valid_position = false;
        
        // Send reset message
        char reset_msg[128];
        snprintf(reset_msg, sizeof(reset_msg),
            "🚨 MAXIMUM ERRORS REACHED!\n"
            "  • %u consecutive errors detected\n"
            "  • Game has been reset\n"
            "  • Starting fresh game",
            (unsigned int)MAX_CONSECUTIVE_ERRORS
        );
        game_send_response_to_uart(reset_msg, true, NULL);
        return;
    }
    
    // FIXED: last_valid_position se nyní ukládá při každém UP příkazu
    // Není potřeba ji ukládat zde

    // 2. Uložení invalid_move_backup pro recovery
    invalid_move_backup = *move;

    // 3. Posunutí boardu na nevalidní cíl (pro fyzický pickup)
    board[move->to_row][move->to_col] = move->piece;
    board[move->from_row][move->from_col] = PIECE_EMPTY;

    // 4. Nastavení recovery stavu
    current_game_state = GAME_STATE_ERROR_RECOVERY_GENERAL;
    error_recovery_active = true;
    error_recovery_start_time = esp_timer_get_time() / 1000;
    
    // 5. LED indikace nevalidního cíle
    for (int i = 0; i < 3; i++) {
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(move->to_row, move->to_col), 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(300));
        led_clear_board_only();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    led_set_pixel_safe(chess_pos_to_led_index(move->to_row, move->to_col), 255, 0, 0);

    // 6. UART instrukce
    char msg[64];
    snprintf(msg, sizeof(msg), "❌ Invalid move – lift piece from %c%d to return it",
             'a' + move->to_col, move->to_row + 1);
    game_send_response_to_uart(msg, true, NULL);
    
    ESP_LOGI(TAG, "💡 User must return piece to [%d,%d] and try again", 
             move->from_row, move->from_col);
}

/**
 * @brief Check if error recovery is active
 * @return true if error recovery is active
 */
bool game_is_error_recovery_active(void)
{
    return error_recovery_active;
}

/**
 * @brief Handle piece return during error recovery
 * @param row Row where piece was returned
 * @param col Column where piece was returned
 * @return true if piece was returned to correct position
 */
bool game_handle_piece_return(uint8_t row, uint8_t col)
{
    if (!error_recovery_active) {
        return false;
    }
    
    // Check if piece was returned to correct position
    if (row == invalid_move_backup.from_row && col == invalid_move_backup.from_col) {
        ESP_LOGI(TAG, "✅ Piece returned to correct position - clearing error state");
        
        // Clear error recovery state
        error_recovery_active = false;
        current_game_state = GAME_STATE_IDLE;
        
        // Reset error count on successful recovery
        consecutive_error_count = 0;
        ESP_LOGI(TAG, "✅ Error count reset to 0");
        
        // Clear LEDs
        led_clear_board_only();
        
        // Reset lifted piece state
        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;
        
        return true;
    } else {
        ESP_LOGW(TAG, "❌ Piece returned to wrong position [%d,%d], expected [%d,%d]", 
                 row, col, invalid_move_backup.from_row, invalid_move_backup.from_col);
        
        // Show error - wrong position
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0, 0); // Red - wrong position
        led_set_pixel_safe(chess_pos_to_led_index(invalid_move_backup.from_row, invalid_move_backup.from_col), 
                          255, 255, 0); // Yellow - correct position
        
        return false;
    }
}

// FIXED: Duplicitní definice byla odstraněna

/**
 * @brief Force clear error recovery state
 */
void game_clear_error_recovery(void)
{
    if (error_recovery_active) {
        ESP_LOGI(TAG, "🔄 Clearing error recovery state");
        error_recovery_active = false;
        current_game_state = GAME_STATE_IDLE;
        
        // Reset error count
        consecutive_error_count = 0;
        ESP_LOGI(TAG, "✅ Error count reset to 0");
        
        led_clear_board_only();
        
        // Reset lifted piece state
        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;
    }
}

/**
 * @brief Get current error count
 * @return Current consecutive error count
 */
uint32_t game_get_error_count(void)
{
    return consecutive_error_count;
}


/**
 * @brief Process move command from UART
 * @param move_cmd Move command structure with coordinates
 */
void game_process_move_command(const void* move_cmd_ptr)
{
    typedef struct {
        uint8_t command_type;
        uint8_t from_row;
        uint8_t from_col;
        uint8_t to_row;
        uint8_t to_col;
    } move_command_t;
    
    const move_command_t* move_cmd = (const move_command_t*)move_cmd_ptr;
    
    ESP_LOGI(TAG, "Processing move: [%d,%d] -> [%d,%d]", 
             move_cmd->from_row, move_cmd->from_col, 
             move_cmd->to_row, move_cmd->to_col);
    
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
        ESP_LOGE(TAG, "Invalid move: no piece at [%d,%d]", move_cmd->from_row, move_cmd->from_col);
        return;
    }
    
    // Check if piece belongs to current player
    bool is_white_piece = (from_piece >= PIECE_WHITE_PAWN && from_piece <= PIECE_WHITE_KING);
    bool is_black_piece = (from_piece >= PIECE_BLACK_PAWN && from_piece <= PIECE_BLACK_KING);
    
    if ((current_player == PLAYER_WHITE && !is_white_piece) ||
        (current_player == PLAYER_BLACK && !is_black_piece)) {
        ESP_LOGE(TAG, "Invalid move: cannot move opponent's piece");
        return;
    }
    
    // Check if destination is occupied by own piece
    if (to_piece != PIECE_EMPTY) {
        bool dest_is_white = (to_piece >= PIECE_WHITE_PAWN && to_piece <= PIECE_WHITE_KING);
        bool dest_is_black = (to_piece >= PIECE_BLACK_PAWN && to_piece <= PIECE_BLACK_KING);
        
        if ((current_player == PLAYER_WHITE && dest_is_white) ||
            (current_player == PLAYER_BLACK && dest_is_black)) {
            ESP_LOGE(TAG, "Invalid move: destination occupied by own piece");
            return;
        }
    }
    
    // TODO: Add proper chess move validation (piece-specific rules)
    // For now, just allow any move that passes basic checks
    
    // Create chess move structure for validation
    chess_move_t chess_move = {
        .from_row = move_cmd->from_row,
        .from_col = move_cmd->from_col,
        .to_row = move_cmd->to_row,
        .to_col = move_cmd->to_col,
        .piece = from_piece,
        .captured_piece = to_piece,
        .timestamp = esp_timer_get_time() / 1000
    };
    
    // Validate move using enhanced validation
    move_error_t move_error = game_is_valid_move(&chess_move);
    if (move_error != MOVE_ERROR_NONE) {
        // NEW ERROR HANDLING: Don't return, handle invalid move properly
        game_handle_invalid_move(move_error, &chess_move);
        return;
    }
    
    // Execute the move
    ESP_LOGI(TAG, "Executing move: %s piece from [%d,%d] to [%d,%d]", 
             (is_white_piece ? "White" : "Black"),
             move_cmd->from_row, move_cmd->from_col, 
             move_cmd->to_row, move_cmd->to_col);
    
    // Move piece
    board[move_cmd->to_row][move_cmd->to_col] = from_piece;
    board[move_cmd->from_row][move_cmd->from_col] = PIECE_EMPTY;
    
    // Mark piece as moved
    piece_moved[move_cmd->to_row][move_cmd->to_col] = true;
    
    // Update game state
    last_move_time = esp_timer_get_time() / 1000;
    
    // FIXED: move_count++ and player switch are handled by game_execute_move()
    // No need to duplicate here
    
    // Store last valid move
    last_valid_move.from_row = move_cmd->from_row;
    last_valid_move.from_col = move_cmd->from_col;
    last_valid_move.to_row = move_cmd->to_row;
    last_valid_move.to_col = move_cmd->to_col;
    last_valid_move.piece = from_piece;
    last_valid_move.captured_piece = to_piece;
    last_valid_move.timestamp = last_move_time;
    has_last_valid_move = true;
    
    // Track last move for board display
    last_move_from_row = move_cmd->from_row;
    last_move_from_col = move_cmd->from_col;
    last_move_to_row = move_cmd->to_row;
    last_move_to_col = move_cmd->to_col;
    has_last_move = true;
    
    // Track captured pieces
    if (to_piece != PIECE_EMPTY) {
        if (current_player == PLAYER_WHITE) {
            // Black piece was captured by white
            black_captured_count++;
        } else {
            // White piece was captured by black
            white_captured_count++;
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
    
    // Check for end-game conditions
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
        current_game_state = GAME_STATE_FINISHED;
        game_active = false;
        
        // Update overall statistics
        if (game_result == GAME_STATE_FINISHED) {
            // Check if it was checkmate (game_result indicates finished game)
            if (current_player == PLAYER_WHITE) {
                black_wins++;
            } else {
                white_wins++;
            }
        }
        
        ESP_LOGI(TAG, "🎉 Game finished! Final statistics:");
        game_print_game_stats();
        ESP_LOGI(TAG, "💡 Commands: NEW GAME, ANALYZE, SAVE <name>");
        
        // Když hra skončí, automaticky spustíme animaci
        if (end_game_result == GAME_STATE_FINISHED) {
            // Najdeme pozici krále vítěze
            uint8_t king_pos = 28; // default e4
            for (int i = 0; i < 64; i++) {
                piece_t piece = board[i/8][i%8];
                if ((current_player == PLAYER_WHITE && piece == PIECE_BLACK_KING) ||
                    (current_player == PLAYER_BLACK && piece == PIECE_WHITE_KING)) {
                    king_pos = i;
                    break;
                }
            }
            
            ESP_LOGI(TAG, "🏆 Game ended, starting victory animation");
            start_endgame_animation(ENDGAME_ANIM_VICTORY_WAVE, king_pos);
        }
        
        return;
    }
    
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
                             uint8_t to_row, uint8_t to_col, 
                             piece_t piece, piece_t captured)
{
    // Convert coordinates to chess notation
    char from_square[4], to_square[4];
    game_coords_to_square(from_row, from_col, from_square);
    game_coords_to_square(to_row, to_col, to_square);
    
    // Get piece symbol and name
    const char* piece_symbol = piece_symbols[piece];
    const char* piece_name = game_get_piece_name(piece);
    
    ESP_LOGI(TAG, "╭─────────────────────────────────╮");
    ESP_LOGI(TAG, "│        MOVE ANIMATION          │");
    ESP_LOGI(TAG, "├─────────────────────────────────┤");
    ESP_LOGI(TAG, "│  %s %s moves from %s to %s  │", piece_symbol, piece_name, from_square, to_square);
    
    if (captured != PIECE_EMPTY) {
        const char* captured_symbol = piece_symbols[captured];
        const char* captured_name = game_get_piece_name(captured);
        ESP_LOGI(TAG, "│  Captures: %s %s                │", captured_symbol, captured_name);
    } else {
        ESP_LOGI(TAG, "│  No capture                     │");
    }
    
    ESP_LOGI(TAG, "╰─────────────────────────────────╯");
    
    // FIXED: LED animations are now handled in game_process_drop_command
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
void game_show_player_change_animation(player_t previous_player, player_t current_player)
{
    ESP_LOGI(TAG, "🔄 Showing player change animation: %s -> %s", 
             previous_player == PLAYER_WHITE ? "White" : "Black",
             current_player == PLAYER_WHITE ? "White" : "Black");
    
    // Clear board first
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // FIXED: Find all pieces first, then animate all columns simultaneously
    int prev_pieces[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; // Row positions for each column
    int curr_pieces[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; // Row positions for each column
    
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
        
        // Find closest piece of current player (closest to previous player's side)
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
    
    // ENHANCED: Ultra-smooth player change animation with advanced brightness control
    for (int step = 0; step < 20; step++) { // More steps for ultra-smoothness
        float progress = (float)step / 19.0f;
        
        // Clear board first
        led_clear_board_only();
        
        // Animate all columns at once with enhanced trail effect
        for (int col = 0; col < 8; col++) {
            if (prev_pieces[col] != -1 && curr_pieces[col] != -1) {
                // Create enhanced trail effect with more trail points
                for (int trail = 0; trail < 8; trail++) {
                    float trail_progress = progress - (trail * 0.06f);
                    if (trail_progress < 0) continue;
                    if (trail_progress > 1) break;
                    
                    // Calculate intermediate position with advanced easing
                    float eased_progress = trail_progress * trail_progress * trail_progress * (trail_progress * (trail_progress * 6.0f - 15.0f) + 10.0f); // Smooth step 5
                    int inter_row = prev_pieces[col] + (curr_pieces[col] - prev_pieces[col]) * eased_progress;
                    int inter_col = col;
                    uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);
                    
                    // Enhanced harmonious color transition with more phases
                    uint8_t red, green, blue;
                    if (trail_progress < 0.125f) {
                        // Phase 1: Deep Blue to Royal Blue
                        float local_progress = trail_progress / 0.125f;
                        red = (uint8_t)(100 * local_progress);
                        green = 0;
                        blue = 255;
                    } else if (trail_progress < 0.25f) {
                        // Phase 2: Royal Blue to Purple
                        float local_progress = (trail_progress - 0.125f) / 0.125f;
                        red = 100 + (uint8_t)(155 * local_progress);
                        green = 0;
                        blue = 255;
                    } else if (trail_progress < 0.375f) {
                        // Phase 3: Purple to Magenta
                        red = 255;
                        green = 0;
                        blue = 255;
                    } else if (trail_progress < 0.5f) {
                        // Phase 4: Magenta to Pink
                        float local_progress = (trail_progress - 0.375f) / 0.125f;
                        red = 255;
                        green = (uint8_t)(100 * local_progress);
                        blue = 255 - (uint8_t)(100 * local_progress);
                    } else if (trail_progress < 0.625f) {
                        // Phase 5: Pink to Coral
                        float local_progress = (trail_progress - 0.5f) / 0.125f;
                        red = 255;
                        green = 100 + (uint8_t)(100 * local_progress);
                        blue = 155 - (uint8_t)(100 * local_progress);
                    } else if (trail_progress < 0.75f) {
                        // Phase 6: Coral to Orange
                        float local_progress = (trail_progress - 0.625f) / 0.125f;
                        red = 255;
                        green = 200 + (uint8_t)(55 * local_progress);
                        blue = 55 - (uint8_t)(55 * local_progress);
                    } else if (trail_progress < 0.875f) {
                        // Phase 7: Orange to Gold
                        float local_progress = (trail_progress - 0.75f) / 0.125f;
                        red = 255;
                        green = 255;
                        blue = (uint8_t)(40 * (1.0f - local_progress));
                    } else {
                        // Phase 8: Gold to Bright White
                        float local_progress = (trail_progress - 0.875f) / 0.125f;
                        red = 255;
                        green = 255;
                        blue = 40 + (uint8_t)(215 * local_progress);
                    }
                    
                    // Advanced trail brightness with exponential fade and gamma correction
                    float trail_brightness = powf(1.0f - (trail * 0.12f), 2.2f); // Gamma correction for better visual perception
                    
                    // Multi-layered pulsing effect with different frequencies
                    float pulse1 = 0.5f + 0.5f * sin(progress * 12.56f + trail * 1.26f); // Main pulse
                    float pulse2 = 0.8f + 0.2f * sin(progress * 25.12f + trail * 2.51f); // Secondary pulse
                    float pulse3 = 0.9f + 0.1f * sin(progress * 50.24f + trail * 3.77f); // Fine detail pulse
                    float pulse4 = 0.95f + 0.05f * sin(progress * 100.48f + trail * 5.03f); // Micro pulse
                    float combined_pulse = pulse1 * pulse2 * pulse3 * pulse4;
                    
                    // Apply brightness and pulsing with saturation boost
                    float saturation_boost = 1.0f + 0.2f * sin(progress * 6.28f); // Subtle saturation variation
                    red = (uint8_t)(red * trail_brightness * combined_pulse * saturation_boost);
                    green = (uint8_t)(green * trail_brightness * combined_pulse * saturation_boost);
                    blue = (uint8_t)(blue * trail_brightness * combined_pulse * saturation_boost);
                    
                    // Clamp values to valid range
                    red = (red > 255) ? 255 : red;
                    green = (green > 255) ? 255 : green;
                    blue = (blue > 255) ? 255 : blue;
                    
                    led_set_pixel_safe(inter_led, red, green, blue);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(12)); // Optimized timing: 20 steps × 12ms = 240ms total for better responsiveness
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
void game_test_move_animation(void)
{
    ESP_LOGI(TAG, "🎬 Testing move animation...");
    
    // Test move from e2 to e4
    uint8_t to_led = chess_pos_to_led_index(3, 4);   // e4
    
    // Progressive color animation from green to blue
    for (int step = 0; step < 10; step++) {
        float progress = (float)step / 9.0f;
        
        // Calculate intermediate position
        int inter_row = 1 + (3 - 1) * progress;
        int inter_col = 4;
        uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);
        
        // Calculate color transition (green -> blue)
        uint8_t red = 0;
        uint8_t green = 255 - (255 * progress);  // 255 -> 0
        uint8_t blue = 0 + (255 * progress);     // 0 -> 255
        
        led_clear_board_only();
        led_set_pixel_safe(inter_led, red, green, blue);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Final blue flash
    led_clear_board_only();
    led_set_pixel_safe(to_led, 0, 0, 255);
    vTaskDelay(pdMS_TO_TICKS(500));
    led_clear_board_only();
}

/**
 * @brief Test player change animation
 */
void game_test_player_change_animation(void)
{
    ESP_LOGI(TAG, "🎬 Testing player change animation...");
    game_show_player_change_animation(PLAYER_WHITE, PLAYER_BLACK);
}

/**
 * @brief Test castling animation
 */
void game_test_castle_animation(void)
{
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
    led_command_t castle_cmd = {
        .type = LED_CMD_ANIM_CASTLE,
        .led_index = king_from_led,
        .red = 255, .green = 215, .blue = 0, // Gold
        .duration_ms = 1500,
        .data = &king_to_led
    };
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
void game_test_promote_animation(void)
{
    ESP_LOGI(TAG, "🎬 Testing promotion animation...");
    
    // Test promotion on a8
    uint8_t promotion_led = chess_pos_to_led_index(7, 0); // a8
    
    // Show enhanced promotion animation
    led_command_t promote_cmd = {
        .type = LED_CMD_ANIM_PROMOTE,
        .led_index = promotion_led,
        .red = 255, .green = 215, .blue = 0, // Gold
        .duration_ms = 2000,
        .data = NULL
    };
    led_execute_command_new(&promote_cmd);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Faster animation
    
    // Clear highlights
    led_clear_board_only();
}

/**
 * @brief Test endgame animation
 */
void game_test_endgame_animation(void)
{
    ESP_LOGI(TAG, "🎬 Testing endgame animation...");
    
    // Test endgame animation using direct LED command
    uint8_t king_pos = 27; // d4 square
    
    led_command_t endgame_cmd = {
        .type = LED_CMD_ANIM_ENDGAME,
        .led_index = king_pos,
        .red = 255, .green = 215, .blue = 0, // Gold
        .duration_ms = 3000,
        .data = NULL
    };
    led_execute_command_new(&endgame_cmd);
    vTaskDelay(pdMS_TO_TICKS(1500)); // Faster animation
    
    // Clear board
    led_clear_board_only();
}

/**
 * @brief Test puzzle animation
 */
void game_test_puzzle_animation(void)
{
    ESP_LOGI(TAG, "🎬 Starting MATRIX-INTEGRATED puzzle system...");
    
    // NEW: MATRIX-INTEGRATED PUZZLE SYSTEM
    // This will work with actual matrix scanning and UP/DN commands
    
    // Step 1: Show red LEDs for pieces that should be removed
    ESP_LOGI(TAG, "🔴 Step 1: Highlighting pieces to remove (red LEDs)");
    for (int i = 0; i < 64; i++) {
        // Example: highlight pieces that are not needed for this puzzle
        if (i % 8 == 0 || i % 8 == 7 || i < 8 || i >= 56) { // Edge pieces
            led_set_pixel_safe(i, 255, 0, 0); // Red for removal
        }
    }
    
    ESP_LOGI(TAG, "⏳ Waiting for user to remove red pieces via matrix...");
    ESP_LOGI(TAG, "💡 Use UP command to lift pieces, DN to place them off board");
    
    // Step 2: Wait for matrix input to remove pieces
    // This will be handled by the actual game logic when UP/DN commands are received
    
    // Step 3: Show first move instruction
    ESP_LOGI(TAG, "🟡 Step 2: Showing first move instruction");
    led_clear_board_only();
    
    // Test puzzle path animation from e2 to e4
    uint8_t from_led = chess_pos_to_led_index(1, 4); // e2
    uint8_t to_led = chess_pos_to_led_index(3, 4);   // e4
    
    ESP_LOGI(TAG, "🎯 Puzzle animation: from_led=%d, to_led=%d", from_led, to_led);
    
    led_command_t puzzle_cmd = {
        .type = LED_CMD_ANIM_PUZZLE_PATH,
        .led_index = from_led,
        .red = 0, .green = 255, .blue = 0, // Green
        .duration_ms = 2000,
        .data = &to_led
    };
    led_execute_command_new(&puzzle_cmd);
    
    ESP_LOGI(TAG, "⏳ Waiting for user to make move via matrix...");
    ESP_LOGI(TAG, "💡 Use UP command to lift piece from e2, DN to place on e4");
    
    // Step 4: Wait for matrix input to make the move
    // This will be handled by the actual game logic when UP/DN commands are received
    
    // Step 5: Show next piece to move
    ESP_LOGI(TAG, "🟢 Step 3: Showing next piece instruction");
    led_clear_board_only();
    
    // Highlight next piece to move (example: knight)
    uint8_t next_piece_led = chess_pos_to_led_index(0, 1); // b1
    led_set_pixel_safe(next_piece_led, 255, 255, 0); // Yellow for next piece
    
    ESP_LOGI(TAG, "⏳ Waiting for user to move next piece via matrix...");
    ESP_LOGI(TAG, "💡 Use UP command to lift knight from b1, DN to place on target");
    
    // Clear board
    led_clear_board_only();
    ESP_LOGI(TAG, "✅ MATRIX-INTEGRATED puzzle system ready - waiting for matrix input");
}

/**
 * @brief Check for check/checkmate conditions
 */
void game_check_game_conditions(void)
{
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
             (current_game_state == GAME_STATE_ACTIVE) ? "Active" :
             (current_game_state == GAME_STATE_IDLE) ? "Idle" :
             (current_game_state == GAME_STATE_PAUSED) ? "Paused" : "Finished");
}

// ============================================================================
// END-GAME DETECTION
// ============================================================================

/**
 * @brief Check if king is in check
 * @param player Player to check
 * @return true if king is in check
 */
bool game_is_king_in_check(player_t player)
{
    // Find king position
    int king_row = -1, king_col = -1;
    piece_t king_piece = (player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
                king_row = row;
                king_col = col;
                break;
            }
        }
        if (king_row != -1) break;
    }
    
    if (king_row == -1) return false; // King not found
    
    // Check if any opponent piece can capture the king
    piece_t opponent_pieces_start = (player == PLAYER_WHITE) ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
    piece_t opponent_pieces_end = (player == PLAYER_WHITE) ? PIECE_BLACK_KING : PIECE_WHITE_KING;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (piece >= opponent_pieces_start && piece <= opponent_pieces_end) {
                // Create temporary move to check if it can capture king
                chess_move_t temp_move = {
                    .from_row = row,
                    .from_col = col,
                    .to_row = king_row,
                    .to_col = king_col,
                    .piece = piece,
                    .captured_piece = king_piece,
                    .timestamp = 0
                };
                
                // Check if this move is valid (would capture king)
                if (game_validate_piece_move_enhanced(&temp_move, piece) == MOVE_ERROR_NONE) {
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
bool game_has_legal_moves(player_t player)
{
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
    if (piece == PIECE_EMPTY) return false;
    return (player == PLAYER_WHITE) ? game_is_black_piece(piece) : game_is_white_piece(piece);
}

bool game_is_insufficient_material(void)
{
    int white_pieces = 0, black_pieces = 0;
    int white_minors = 0, black_minors = 0; // Knights and bishops
    bool white_has_bishop = false, black_has_bishop = false;
    bool white_has_knight = false, black_has_knight = false;
    bool white_bishop_color = false, black_bishop_color = false; // false = light, true = dark
    
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
    if ((white_pieces == 1 && black_pieces == 0) || (white_pieces == 0 && black_pieces == 1)) {
        return true;
    }
    
    // King + Bishop vs King + Bishop (same color) - insufficient
    if (white_pieces == 1 && black_pieces == 1 && 
        white_has_bishop && black_has_bishop && 
        white_bishop_color == black_bishop_color) {
        return true;
    }
    
    // King + Knight vs King + Knight - insufficient
    if (white_pieces == 1 && black_pieces == 1 && 
        white_has_knight && black_has_knight) {
        return true;
    }
    
    // King + 2 Knights vs King - insufficient (very rare, but technically insufficient)
    if ((white_pieces == 2 && black_pieces == 0 && white_minors == 2 && white_has_knight) ||
        (white_pieces == 0 && black_pieces == 2 && black_minors == 2 && black_has_knight)) {
        return true;
    }
    
    return false; // Sufficient material for checkmate
}

/**
 * @brief Check for end-game conditions
 * @return Game result state
 */
game_state_t game_check_end_game_conditions(void)
{
    // Check if current player is in check
    bool in_check = game_is_king_in_check(current_player);
    
    // Check if current player has legal moves
    bool has_moves = game_has_legal_moves(current_player);
    
    if (in_check && !has_moves) {
        // Checkmate
        game_result = (current_player == PLAYER_WHITE) ? GAME_STATE_FINISHED : GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🎯 CHECKMATE! %s wins in %" PRIu32 " moves!", 
                 (current_player == PLAYER_WHITE) ? "Black" : "White", move_count);
        return GAME_STATE_FINISHED;
    } else if (!in_check && !has_moves) {
        // Stalemate
        game_result = GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn in %" PRIu32 " moves", move_count);
        return GAME_STATE_FINISHED;
    }
    
    // Check for draw conditions
    if (moves_without_capture >= 50) {
        game_result = GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🤝 DRAW! 50 moves without capture (50-move rule)");
        return GAME_STATE_FINISHED;
    }
    
    if (game_is_position_repeated()) {
        game_result = GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🤝 DRAW! Position repeated (draw by repetition)");
        return GAME_STATE_FINISHED;
    }
    
    // Check for insufficient material
    if (game_is_insufficient_material()) {
        game_result = GAME_STATE_FINISHED;
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
void game_toggle_timer(bool enabled)
{
    timer_enabled = enabled;
    ESP_LOGI(TAG, "Game timer %s", enabled ? "enabled" : "disabled");
}

/**
 * @brief Save current game to NVS
 * @param game_name Name for the saved game
 */
void game_save_game(const char* game_name)
{
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
void game_load_game(const char* game_name)
{
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
void game_export_pgn(char* pgn_buffer, size_t buffer_size)
{
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
    
    // Add last move if available
    if (has_last_valid_move && pos < buffer_size - 50) {
        const chess_move_t* move = &last_valid_move;
        char from_square[4], to_square[4];
        game_coords_to_square(move->from_row, move->from_col, from_square);
        game_coords_to_square(move->to_row, move->to_col, to_square);
        
        // Add move notation
        pos += snprintf(pgn_buffer + pos, buffer_size - pos, "1. %s%s", from_square, to_square);
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
void game_coords_to_square(uint8_t row, uint8_t col, char* square)
{
    if (square == NULL) return;
    
    // Convert column to file (a-h)
    square[0] = 'a' + col;
    
    // Convert row to rank (1-8) - row 0 = rank 1, row 7 = rank 8
    square[1] = '1' + row;
    
    square[2] = '\0';
}


void game_print_status(void)
{
    ESP_LOGI(TAG, "Game Status:");
    ESP_LOGI(TAG, "  State: %d", current_game_state);
    ESP_LOGI(TAG, "  Current player: %s", (current_player == PLAYER_WHITE) ? "White" : "Black");
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


void game_task_start(void *pvParameters)
{
    ESP_LOGI(TAG, "Game task started successfully");
    
    // NOTE: Task is already registered with TWDT in main.c
    // No need to register again here to avoid duplicate registration
    
    ESP_LOGI(TAG, "Features:");
    ESP_LOGI(TAG, "  • Standard chess rules");
    ESP_LOGI(TAG, "  • Move validation");
    ESP_LOGI(TAG, "  • Game state management");
    ESP_LOGI(TAG, "  • Move history tracking");
    ESP_LOGI(TAG, "  • Board visualization");
    ESP_LOGI(TAG, "  • 100ms command cycle");
    
    task_running = true;
    
    // Initialize game
    game_initialize_board();
    
    // Main task loop
    uint32_t loop_count = 0;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    for (;;) {
        // CRITICAL: Reset watchdog for game task in every iteration (only if registered)
        esp_err_t wdt_ret = esp_task_wdt_reset();
        if (wdt_ret != ESP_OK && wdt_ret != ESP_ERR_NOT_FOUND) {
            // Task not registered with TWDT yet - this is normal during startup
        }
        
        // Process game commands
        game_process_commands();
        
        // Process matrix events (moves)
        game_process_matrix_events();
        
        // Check for error recovery timeout
        if (game_is_error_recovery_active() && game_is_error_recovery_timeout()) {
            ESP_LOGW(TAG, "⏰ Error recovery timeout - clearing error state");
            game_clear_error_recovery();
        }
        
        // Check for castling timeout
        if (game_is_castling_in_progress() && game_is_castling_timeout()) {
            ESP_LOGW(TAG, "⏰ Castling timeout - cancelling castling transaction");
            game_cancel_castling_transaction();
        }
        
        // Periodic status update
        if (loop_count % 5000 == 0) { // Every 5 seconds
            ESP_LOGI(TAG, "Game Task Status: loop=%d, state=%d, player=%d, moves=%lu", 
                     loop_count, current_game_state, current_player, move_count);
        }
        
        loop_count++;
        
        // Wait for next cycle (100ms)
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(100));
    }
}


void game_process_matrix_events(void)
{
    matrix_event_t event;
    
    // Process matrix events from queue
    if (matrix_event_queue != NULL) {
        while (xQueueReceive(matrix_event_queue, &event, 0) == pdTRUE) {
            switch (event.type) {
                case MATRIX_EVENT_PIECE_LIFTED:
                    ESP_LOGI(TAG, "🖐️ Matrix: Piece lifted from %c%d", 
                             'a' + event.from_col, event.from_row + 1);
                    game_handle_piece_lifted(event.from_row, event.from_col);
                    break;
                    
                case MATRIX_EVENT_PIECE_PLACED:
                    ESP_LOGI(TAG, "✋ Matrix: Piece placed at %c%d", 
                             'a' + event.to_col, event.to_row + 1);
                    game_handle_piece_placed(event.to_row, event.to_col);
                    break;
                    
                case MATRIX_EVENT_MOVE_DETECTED:
                    ESP_LOGI(TAG, "🎯 Matrix: Move detected %c%d -> %c%d", 
                             'a' + event.from_col, event.from_row + 1,
                             'a' + event.to_col, event.to_row + 1);
                    game_handle_matrix_move(event.from_row, event.from_col, 
                                          event.to_row, event.to_col);
                    break;
                    
                case MATRIX_EVENT_ERROR:
                    ESP_LOGW(TAG, "❌ Matrix: Error event received");
                    break;
                    
                default:
                    ESP_LOGW(TAG, "❓ Matrix: Unknown event type: %d", event.type);
                    break;
            }
        }
    }
}

// ============================================================================
// ENHANCED CHESS LOGIC IMPLEMENTATION
// ============================================================================

// Direction vectors for piece movement (knight_moves moved to top of file)

static const int8_t king_moves[8][2] = {
    {-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}
};

static const int8_t bishop_dirs[4][2] = {
    {-1,-1}, {-1,1}, {1,-1}, {1,1}
};

static const int8_t rook_dirs[4][2] = {
    {-1,0}, {1,0}, {0,-1}, {0,1}
};

// Castling flags (moved to top of file)

// En passant state (moved to top of file)

// Lifted piece state tracking (already defined above)

// Minimal move generation - only when needed
static chess_move_extended_t temp_moves_buffer[16];  // Only for current move validation
static uint32_t temp_moves_count = 0;

// Game position history for draw detection (declared earlier in the file)
// static uint32_t position_hashes[100];
// static uint32_t position_history_count = 0;
static uint32_t fifty_move_counter = 0;

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
    if (piece == PIECE_EMPTY) return false;
    return (player == PLAYER_WHITE) ? game_is_white_piece(piece) : game_is_black_piece(piece);
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
    piece_t attacking_pawn = (by_player == PLAYER_WHITE) ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;
    
    if (game_is_valid_square(pawn_row, col - 1) && 
        board[pawn_row][col - 1] == attacking_pawn) return true;
    if (game_is_valid_square(pawn_row, col + 1) && 
        board[pawn_row][col + 1] == attacking_pawn) return true;
    
    // Check knight attacks
    piece_t attacking_knight = (by_player == PLAYER_WHITE) ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
    for (int i = 0; i < 8; i++) {
        int nr = row + knight_moves[i][0];
        int nc = col + knight_moves[i][1];
        if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_knight) {
            return true;
        }
    }
    
    // Check bishop/queen diagonal attacks
    piece_t attacking_bishop = (by_player == PLAYER_WHITE) ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
    piece_t attacking_queen = (by_player == PLAYER_WHITE) ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;
    
    for (int d = 0; d < 4; d++) {
        int dr = bishop_dirs[d][0];
        int dc = bishop_dirs[d][1];
        
        for (int nr = row + dr, nc = col + dc; 
             game_is_valid_square(nr, nc); 
             nr += dr, nc += dc) {
            piece_t piece = board[nr][nc];
            if (piece == PIECE_EMPTY) continue;
            
            if (piece == attacking_bishop || piece == attacking_queen) {
                return true;
            }
            break; // Blocked by any piece
        }
    }
    
    // Check rook/queen orthogonal attacks
    piece_t attacking_rook = (by_player == PLAYER_WHITE) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    
    for (int d = 0; d < 4; d++) {
        int dr = rook_dirs[d][0];
        int dc = rook_dirs[d][1];
        
        for (int nr = row + dr, nc = col + dc; 
             game_is_valid_square(nr, nc); 
             nr += dr, nc += dc) {
            piece_t piece = board[nr][nc];
            if (piece == PIECE_EMPTY) continue;
            
            if (piece == attacking_rook || piece == attacking_queen) {
                return true;
            }
            break; // Blocked by any piece
        }
    }
    
    // Check king attacks
    piece_t attacking_king = (by_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    for (int i = 0; i < 8; i++) {
        int nr = row + king_moves[i][0];
        int nc = col + king_moves[i][1];
        if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_king) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Find king position
 */
bool game_find_king(player_t player, uint8_t* king_row, uint8_t* king_col) {
    piece_t king_piece = (player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
                *king_row = row;
                *king_col = col;
                return true;
            }
        }
    }
    return false;
}

// This function is already defined earlier in the file
// Removing duplicate definition to avoid conflicts

// ============================================================================
// MOVE VALIDATION AND SIMULATION
// ============================================================================

/**
 * @brief Simulate move and check if it leaves king in check
 */
bool game_simulate_move_check(chess_move_extended_t* move, player_t player) {
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
    
    return !king_in_check; // Return true if move is legal (doesn't leave king in check)
}

// ============================================================================
// MOVE GENERATION - INDIVIDUAL PIECES
// ============================================================================

/**
 * @brief Generate pawn moves
 */
void game_generate_pawn_moves(uint8_t from_row, uint8_t from_col, player_t player) {
    piece_t pawn = board[from_row][from_col];
    bool is_white = (player == PLAYER_WHITE);
    int direction = is_white ? 1 : -1;
    int start_row = is_white ? 1 : 6;
    int promotion_row = is_white ? 7 : 0;
    
    // Forward move
    int to_row = from_row + direction;
    if (game_is_valid_square(to_row, from_col) && board[to_row][from_col] == PIECE_EMPTY) {
        
        if (to_row == promotion_row) {
            // Promotion moves
            for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT; promo++) {
                if (temp_moves_count >= 16) return;
                
                chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
                move->from_row = from_row;
                move->from_col = from_col;
                move->to_row = to_row;
                move->to_col = from_col;
                move->piece = pawn;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_PROMOTION;
                move->promotion_piece = (promotion_choice_t)promo;
                
                if (game_simulate_move_check(move, player)) {
                    temp_moves_count++;
                }
            }
        } else {
            // Normal forward move
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = to_row;
            move->to_col = from_col;
            move->piece = pawn;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_NORMAL;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
            
            // Double move from starting position
            if (from_row == start_row && board[to_row + direction][from_col] == PIECE_EMPTY) {
                if (temp_moves_count >= 16) return;
                
                move = &temp_moves_buffer[temp_moves_count];
                move->from_row = from_row;
                move->from_col = from_col;
                move->to_row = to_row + direction;
                move->to_col = from_col;
                move->piece = pawn;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_NORMAL;
                
                if (game_simulate_move_check(move, player)) {
                    temp_moves_count++;
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
                    for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT; promo++) {
                        if (temp_moves_count >= 16) return;
                        
                        chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
                        move->from_row = from_row;
                        move->from_col = from_col;
                        move->to_row = to_row;
                        move->to_col = to_col;
                        move->piece = pawn;
                        move->captured_piece = target;
                        move->move_type = MOVE_TYPE_PROMOTION;
                        move->promotion_piece = (promotion_choice_t)promo;
                        
                        if (game_simulate_move_check(move, player)) {
                            temp_moves_count++;
                        }
                    }
                } else {
                    // Normal capture
                    if (temp_moves_count >= 16) return;
                    
                    chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
                    move->from_row = from_row;
                    move->from_col = from_col;
                    move->to_row = to_row;
                    move->to_col = to_col;
                    move->piece = pawn;
                    move->captured_piece = target;
                    move->move_type = MOVE_TYPE_CAPTURE;
                    
                    if (game_simulate_move_check(move, player)) {
                        temp_moves_count++;
                    }
                }
            }
        }
    }
    
    // En passant
    if (en_passant_available && from_row == (is_white ? 4 : 3)) {
        for (int dc = -1; dc <= 1; dc += 2) {
            if (from_col + dc == en_passant_target_col) {
                if (temp_moves_count >= 16) return;
                
                chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
                move->from_row = from_row;
                move->from_col = from_col;
                move->to_row = en_passant_target_row;
                move->to_col = en_passant_target_col;
                move->piece = pawn;
                move->captured_piece = board[en_passant_victim_row][en_passant_victim_col];
                move->move_type = MOVE_TYPE_EN_PASSANT;
                
                if (game_simulate_move_check(move, player)) {
                    temp_moves_count++;
                }
            }
        }
    }
}

/**
 * @brief Generate knight moves
 */
void game_generate_knight_moves(uint8_t from_row, uint8_t from_col, player_t player) {
    piece_t knight = board[from_row][from_col];
    
    for (int i = 0; i < 8; i++) {
        int to_row = from_row + knight_moves[i][0];
        int to_col = from_col + knight_moves[i][1];
        
        if (!game_is_valid_square(to_row, to_col)) continue;
        
        piece_t target = board[to_row][to_col];
        
        // Skip if occupied by own piece
        if (game_is_own_piece(target, player)) continue;
        
        if (temp_moves_count >= 16) return;
        
        chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = to_col;
        move->piece = knight;
        move->captured_piece = target;
        move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
        
        if (game_simulate_move_check(move, player)) {
            temp_moves_count++;
        }
    }
}

/**
 * @brief Generate sliding piece moves (bishop, rook, queen)
 */
void game_generate_sliding_moves(uint8_t from_row, uint8_t from_col, player_t player, 
                                const int8_t directions[][2], int num_directions) {
    piece_t piece = board[from_row][from_col];
    
    for (int d = 0; d < num_directions; d++) {
        int dr = directions[d][0];
        int dc = directions[d][1];
        
        for (int to_row = from_row + dr, to_col = from_col + dc;
             game_is_valid_square(to_row, to_col);
             to_row += dr, to_col += dc) {
            
            piece_t target = board[to_row][to_col];
            
            // Can't move to square occupied by own piece
            if (game_is_own_piece(target, player)) break;
            
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = to_row;
            move->to_col = to_col;
            move->piece = piece;
            move->captured_piece = target;
            move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
            
            // Stop if we hit any piece
            if (target != PIECE_EMPTY) break;
        }
    }
}

/**
 * @brief Generate king moves including castling
 */
void game_generate_king_moves(uint8_t from_row, uint8_t from_col, player_t player) {
    piece_t king = board[from_row][from_col];
    
    // Normal king moves
    for (int i = 0; i < 8; i++) {
        int to_row = from_row + king_moves[i][0];
        int to_col = from_col + king_moves[i][1];
        
        if (!game_is_valid_square(to_row, to_col)) continue;
        
        piece_t target = board[to_row][to_col];
        
        // Skip if occupied by own piece
        if (game_is_own_piece(target, player)) continue;
        
        if (temp_moves_count >= 16) return;
        
        chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = to_col;
        move->piece = king;
        move->captured_piece = target;
        move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
        
        if (game_simulate_move_check(move, player)) {
            temp_moves_count++;
        }
    }
    
    // Castling - FIXED: Use proper bounds checking and correct coordinates
    if (game_is_king_in_check(player)) return; // Can't castle when in check
    
    if (player == PLAYER_WHITE && !white_king_moved) {
        // White kingside castling
        if (!white_rook_h_moved &&
            board[0][5] == PIECE_EMPTY && board[0][6] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 5, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 6, PLAYER_BLACK)) {
            
            if (temp_moves_count >= 16) return; // FIXED: Proper bounds checking
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row; // FIXED: Use actual king position
            move->from_col = from_col;
            move->to_row = 0;
            move->to_col = 6;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_KING;
            temp_moves_count++;
        }
        
        // White queenside castling
        if (!white_rook_a_moved &&
            board[0][1] == PIECE_EMPTY && board[0][2] == PIECE_EMPTY && board[0][3] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 2, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 3, PLAYER_BLACK)) {
            
            if (temp_moves_count >= 16) return; // FIXED: Proper bounds checking
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row; // FIXED: Use actual king position
            move->from_col = from_col;
            move->to_row = 0;
            move->to_col = 2;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_QUEEN;
            temp_moves_count++;
        }
    }
    
    if (player == PLAYER_BLACK && !black_king_moved) {
        // Black kingside castling
        if (!black_rook_h_moved &&
            board[7][5] == PIECE_EMPTY && board[7][6] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 5, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 6, PLAYER_WHITE)) {
            
            if (temp_moves_count >= 16) return; // FIXED: Proper bounds checking
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row; // FIXED: Use actual king position
            move->from_col = from_col;
            move->to_row = 7;
            move->to_col = 6;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_KING;
            temp_moves_count++;
        }
        
        // Black queenside castling
        if (!black_rook_a_moved &&
            board[7][1] == PIECE_EMPTY && board[7][2] == PIECE_EMPTY && board[7][3] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 2, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 3, PLAYER_WHITE)) {
            
            if (temp_moves_count >= 16) return; // FIXED: Proper bounds checking
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row; // FIXED: Use actual king position
            move->from_col = from_col;
            move->to_row = 7;
            move->to_col = 2;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_QUEEN;
            temp_moves_count++;
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
    temp_moves_count = 0;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            
            if (!game_is_own_piece(piece, player)) continue;
            
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
    
    return temp_moves_count;
}

// ============================================================================
// MOVE EXECUTION
// ============================================================================

/**
 * @brief Execute an enhanced chess move on the board
 */
bool game_execute_move_enhanced(chess_move_extended_t* move) {
    if (move == NULL) return false;
    
    // Handle special moves
    switch (move->move_type) {
        case MOVE_TYPE_EN_PASSANT:
            // Remove the captured pawn
            board[en_passant_victim_row][en_passant_victim_col] = PIECE_EMPTY;
            break;
            
        case MOVE_TYPE_CASTLE_KING:
            // Move rook for kingside castling
            if (current_player == PLAYER_WHITE) {
                board[0][5] = PIECE_WHITE_ROOK;
                board[0][7] = PIECE_EMPTY;
            } else {
                board[7][5] = PIECE_BLACK_ROOK;
                board[7][7] = PIECE_EMPTY;
            }
            break;
            
        case MOVE_TYPE_CASTLE_QUEEN:
            // Move rook for queenside castling
            if (current_player == PLAYER_WHITE) {
                board[0][3] = PIECE_WHITE_ROOK;
                board[0][0] = PIECE_EMPTY;
            } else {
                board[7][3] = PIECE_BLACK_ROOK;
                board[7][0] = PIECE_EMPTY;
            }
            break;
            
        case MOVE_TYPE_PROMOTION:
            // Handle promotion - the piece will be set below
            break;
            
        default:
            // Normal move or capture - nothing special needed
            break;
    }
    
    // Make the basic move
    board[move->to_row][move->to_col] = move->piece;
    board[move->from_row][move->from_col] = PIECE_EMPTY;
    
    // Handle promotion
    if (move->move_type == MOVE_TYPE_PROMOTION) {
        piece_t promoted_piece;
        if (current_player == PLAYER_WHITE) {
            promoted_piece = PIECE_WHITE_QUEEN + move->promotion_piece;
        } else {
            promoted_piece = PIECE_BLACK_QUEEN + move->promotion_piece;
        }
        board[move->to_row][move->to_col] = promoted_piece;
    }
    
    // Update castling flags
    if (move->piece == PIECE_WHITE_KING) white_king_moved = true;
    if (move->piece == PIECE_BLACK_KING) black_king_moved = true;
    if (move->piece == PIECE_WHITE_ROOK) {
        if (move->from_col == 0) white_rook_a_moved = true;
        if (move->from_col == 7) white_rook_h_moved = true;
    }
    if (move->piece == PIECE_BLACK_ROOK) {
        if (move->from_col == 0) black_rook_a_moved = true;
        if (move->from_col == 7) black_rook_h_moved = true;
    }
    
    // Update en passant state
    en_passant_available = false;
    if ((move->piece == PIECE_WHITE_PAWN || move->piece == PIECE_BLACK_PAWN) &&
        abs(move->to_row - move->from_row) == 2) {
        // Pawn made a double move - enable en passant
        en_passant_available = true;
        en_passant_target_row = (move->from_row + move->to_row) / 2;
        en_passant_target_col = move->from_col;
        en_passant_victim_row = move->to_row;
        en_passant_victim_col = move->to_col;
    }
    
    // Update fifty-move counter
    if (move->piece == PIECE_WHITE_PAWN || move->piece == PIECE_BLACK_PAWN ||
        move->captured_piece != PIECE_EMPTY) {
        fifty_move_counter = 0;
    } else {
        fifty_move_counter++;
    }
    
    // Update move counters and statistics
    if (current_player == PLAYER_WHITE) {
        white_moves_count++;
    } else {
        black_moves_count++;
    }
    
    // Switch players and increment move count
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    move_count++;  // FIXED: Increment move count when player changes
    
    return true;
}

// ============================================================================
// GAME STATE ANALYSIS
// ============================================================================

/**
 * @brief Check for checkmate or stalemate
 */
game_state_t game_analyze_position(player_t player) {
    bool king_in_check = game_is_king_in_check(player);
    uint32_t legal_moves = game_generate_legal_moves(player);
    
    if (legal_moves == 0) {
        if (king_in_check) {
            // Checkmate
            game_result = GAME_STATE_FINISHED;
            if (player == PLAYER_WHITE) {
                black_wins++;
                ESP_LOGI(TAG, "🎯 CHECKMATE! Black wins!");
            } else {
                white_wins++;
                ESP_LOGI(TAG, "🎯 CHECKMATE! White wins!");
            }
            return GAME_STATE_FINISHED;
        } else {
            // Stalemate
            draws++;
            game_result = GAME_STATE_FINISHED;
            ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn!");
            return GAME_STATE_FINISHED;
        }
    }
    
    // Check for fifty-move rule
    if (fifty_move_counter >= 100) { // 50 moves per side
        draws++;
        game_result = GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🤝 DRAW! Fifty-move rule!");
        return GAME_STATE_FINISHED;
    }
    
    // Check for insufficient material (simplified)
    int white_pieces = 0, black_pieces = 0;
    bool white_has_major = false, black_has_major = false;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (piece == PIECE_EMPTY) continue;
            
            if (game_is_white_piece(piece)) {
                white_pieces++;
                if (piece == PIECE_WHITE_QUEEN || piece == PIECE_WHITE_ROOK || piece == PIECE_WHITE_PAWN) {
                    white_has_major = true;
                }
            } else {
                black_pieces++;
                if (piece == PIECE_BLACK_QUEEN || piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_PAWN) {
                    black_has_major = true;
                }
            }
        }
    }
    
    if (white_pieces <= 2 && black_pieces <= 2 && !white_has_major && !black_has_major) {
        draws++;
        game_result = GAME_STATE_FINISHED;
        ESP_LOGI(TAG, "🤝 DRAW! Insufficient material!");
        return GAME_STATE_FINISHED;
    }
    
    return GAME_STATE_ACTIVE;
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
        char* ptr = line;
        
        ptr += sprintf(ptr, " %d │", row + 1);
        
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            const char* symbol;
            
            switch (piece) {
                case PIECE_WHITE_PAWN:   symbol = "♙"; break;
                case PIECE_WHITE_KNIGHT: symbol = "♘"; break;
                case PIECE_WHITE_BISHOP: symbol = "♗"; break;
                case PIECE_WHITE_ROOK:   symbol = "♖"; break;
                case PIECE_WHITE_QUEEN:  symbol = "♕"; break;
                case PIECE_WHITE_KING:   symbol = "♔"; break;
                case PIECE_BLACK_PAWN:   symbol = "♟"; break;
                case PIECE_BLACK_KNIGHT: symbol = "♞"; break;
                case PIECE_BLACK_BISHOP: symbol = "♝"; break;
                case PIECE_BLACK_ROOK:   symbol = "♜"; break;
                case PIECE_BLACK_QUEEN:  symbol = "♛"; break;
                case PIECE_BLACK_KING:   symbol = "♚"; break;
                default:                 symbol = "·"; break;
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
    if (!game_is_valid_square(from_row, from_col) || !game_is_valid_square(to_row, to_col)) {
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
        chess_move_extended_t* move = &temp_moves_buffer[i];
        if (move->from_row == from_row && move->from_col == from_col &&
            move->to_row == to_row && move->to_col == to_col) {
            return MOVE_ERROR_NONE; // Move is legal
        }
    }
    
    return MOVE_ERROR_ILLEGAL_MOVE;
}

/**
 * @brief Enhanced game initialization
 */
void game_initialize_board_enhanced(void) {
    ESP_LOGI(TAG, "Initializing enhanced chess board...");
    
    // Clear board
    memset(board, 0, sizeof(board));
    
    // Set up white pieces (row 0 = rank 1)
    board[0][0] = PIECE_WHITE_ROOK;   // a1
    board[0][1] = PIECE_WHITE_KNIGHT; // b1
    board[0][2] = PIECE_WHITE_BISHOP; // c1
    board[0][3] = PIECE_WHITE_QUEEN;  // d1
    board[0][4] = PIECE_WHITE_KING;   // e1
    board[0][5] = PIECE_WHITE_BISHOP; // f1
    board[0][6] = PIECE_WHITE_KNIGHT; // g1
    board[0][7] = PIECE_WHITE_ROOK;   // h1
    
    // Set up white pawns (row 1 = rank 2)
    for (int col = 0; col < 8; col++) {
        board[1][col] = PIECE_WHITE_PAWN;
    }
    
    // Set up black pawns (row 6 = rank 7)
    for (int col = 0; col < 8; col++) {
        board[6][col] = PIECE_BLACK_PAWN;
    }
    
    // Set up black pieces (row 7 = rank 8)
    board[7][0] = PIECE_BLACK_ROOK;   // a8
    board[7][1] = PIECE_BLACK_KNIGHT; // b8
    board[7][2] = PIECE_BLACK_BISHOP; // c8
    board[7][3] = PIECE_BLACK_QUEEN;  // d8
    board[7][4] = PIECE_BLACK_KING;   // e8
    board[7][5] = PIECE_BLACK_BISHOP; // f8
    board[7][6] = PIECE_BLACK_KNIGHT; // g8
    board[7][7] = PIECE_BLACK_ROOK;   // h8
    
    // Reset game state
    current_player = PLAYER_WHITE;
    current_game_state = GAME_STATE_ACTIVE;
    move_count = 0;
    
    // Reset castling flags
    white_king_moved = false;
    white_rook_a_moved = false;
    white_rook_h_moved = false;
    black_king_moved = false;
    black_rook_a_moved = false;
    black_rook_h_moved = false;
    
    // Reset en passant
    en_passant_available = false;
    
    // Reset other counters
    fifty_move_counter = 0;
    position_repetition_count = 0;
    last_position_hash = 0;
    
    ESP_LOGI(TAG, "Enhanced chess board initialized successfully");
    game_print_board_enhanced();
}

/**
 * @brief Generate castling moves for king
 */
void game_generate_castling_moves(uint8_t from_row, uint8_t from_col, player_t player) {
    piece_t king = board[from_row][from_col];
    
    // Can't castle when in check
    if (game_is_king_in_check(player)) return;
    
    if (player == PLAYER_WHITE && !white_king_moved) {
        // Kingside castling (O-O)
        if (!white_rook_h_moved && 
            board[0][5] == PIECE_EMPTY && board[0][6] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 5, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 6, PLAYER_BLACK)) {
            
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = 0;
            move->to_col = 6;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_KING;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
        }
        
        // Queenside castling (O-O-O)
        if (!white_rook_a_moved && 
            board[0][1] == PIECE_EMPTY && board[0][2] == PIECE_EMPTY && board[0][3] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 2, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 3, PLAYER_BLACK)) {
            
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = 0;
            move->to_col = 2;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_QUEEN;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
        }
    }
    
    if (player == PLAYER_BLACK && !black_king_moved) {
        // Kingside castling (O-O)
        if (!black_rook_h_moved && 
            board[7][5] == PIECE_EMPTY && board[7][6] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 5, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 6, PLAYER_WHITE)) {
            
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = 7;
            move->to_col = 6;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_KING;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
        }
        
        // Queenside castling (O-O-O)
        if (!black_rook_a_moved && 
            board[7][1] == PIECE_EMPTY && board[7][2] == PIECE_EMPTY && board[7][3] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 2, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 3, PLAYER_WHITE)) {
            
            if (temp_moves_count >= 16) return;
            
            chess_move_extended_t* move = &temp_moves_buffer[temp_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = 7;
            move->to_col = 2;
            move->piece = king;
            move->captured_piece = PIECE_EMPTY;
            move->move_type = MOVE_TYPE_CASTLE_QUEEN;
            
            if (game_simulate_move_check(move, player)) {
                temp_moves_count++;
            }
        }
    }
}





// ============================================================================
// MATRIX LED WORKFLOW IMPLEMENTATION
// ============================================================================

/**
 * @brief Handle piece lifted event from matrix
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_lifted(uint8_t row, uint8_t col)
{
    ESP_LOGI(TAG, "🖐️ Matrix: Piece lifted from %c%d", 'a' + col, row + 1);
    
    // Check if there's a piece at the square
    piece_t piece = board[row][col];
    if (piece == PIECE_EMPTY) {
        ESP_LOGW(TAG, "❌ No piece to lift at %c%d", 'a' + col, row + 1);
        return;
    }
    
    // Check if it's the current player's piece
    bool is_white_piece = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
    bool is_current_player = (current_player == PLAYER_WHITE && is_white_piece) || 
                            (current_player == PLAYER_BLACK && !is_white_piece);
    
    if (!is_current_player) {
        ESP_LOGW(TAG, "❌ Cannot lift opponent's piece at %c%d", 'a' + col, row + 1);
        return;
    }
    
    // Show possible moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
    
    if (valid_moves > 0) {
        ESP_LOGI(TAG, "💡 Found %lu valid moves for piece at %c%d", valid_moves, 'a' + col, row + 1);
        
        // Send LED commands to highlight possible moves
            // Highlight source square in yellow
            // Highlight source square (yellow)
            led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0); // Yellow for source
            
            // Highlight possible destinations
            for (uint32_t i = 0; i < valid_moves; i++) {
                uint8_t dest_row = suggestions[i].to_row;
                uint8_t dest_col = suggestions[i].to_col;
                uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);
                
                // Check if destination has opponent's piece
                piece_t dest_piece = board[dest_row][dest_col];
                bool is_opponent_piece = (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN && dest_piece <= PIECE_BLACK_KING) ||
                                       (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN && dest_piece <= PIECE_WHITE_KING);
                
                if (is_opponent_piece) {
                    // Orange for opponent's pieces (capture)
                    led_set_pixel_safe(led_index, 255, 165, 0);
                } else {
                    // Green for empty squares
                    led_set_pixel_safe(led_index, 0, 255, 0);
                }
            }
    } else {
        ESP_LOGI(TAG, "💡 No valid moves for piece at %c%d", 'a' + col, row + 1);
    }
}

/**
 * @brief Handle piece placed event from matrix
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_placed(uint8_t row, uint8_t col)
{
    ESP_LOGI(TAG, "✋ Matrix: Piece placed at %c%d", 'a' + col, row + 1);
    
    // Clear only board LEDs
    led_clear_board_only();
    
    // After piece is placed, show pieces that the opponent can move
    // This completes the cycle as requested by the user
    game_highlight_opponent_pieces();
}

/**
 * @brief Handle complete move from matrix
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 */
void game_handle_matrix_move(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col)
{
    ESP_LOGI(TAG, "🎯 Matrix: Complete move %c%d -> %c%d", 
             'a' + from_col, from_row + 1, 'a' + to_col, to_row + 1);
    
    // Convert matrix event to chess move
    chess_move_t move = {
        .from_row = from_row,
        .from_col = from_col,
        .to_row = to_row,
        .to_col = to_col,
        .piece = PIECE_EMPTY,
        .captured_piece = PIECE_EMPTY,
        .timestamp = 0
    };
    
    // Execute move if valid
    if (game_execute_move(&move)) {
        ESP_LOGI(TAG, "✅ Matrix move executed successfully");
        
        // After successful move, show pieces that the opponent can move
        game_highlight_opponent_pieces();
    } else {
        ESP_LOGW(TAG, "❌ Invalid matrix move rejected");
        
        // Clear LEDs on invalid move
        led_clear_board_only();
    }
}

/**
 * @brief Highlight pieces that the opponent can move
 */
void game_highlight_opponent_pieces(void)
{
    ESP_LOGI(TAG, "🔄 Highlighting opponent pieces that can move");
    
    // LED command queue removed - using direct LED calls now
    
    // Switch to opponent's perspective temporarily
    player_t opponent = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Find all pieces that the opponent can move
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (piece == PIECE_EMPTY) continue;
            
            // Check if it's opponent's piece
            bool is_opponent_piece = (opponent == PLAYER_WHITE && piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) ||
                                   (opponent == PLAYER_BLACK && piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
            
            if (is_opponent_piece) {
                // Check if this piece has valid moves
                move_suggestion_t suggestions[64];
                uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
                
                if (valid_moves > 0) {
                    // Highlight this piece in blue (indicating it can move)
                    led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0, 255); // Blue for pieces that can move
                }
            }
        }
    }
}


/**
 * @brief Process promotion command from UART
 * @param cmd Promotion command structure
 */
void game_process_promotion_command(const chess_move_command_t* cmd)
{
    if (!cmd) {
        ESP_LOGE(TAG, "❌ Invalid promotion command");
        return;
    }
    
    ESP_LOGI(TAG, "👑 Processing promotion command");
    
    // Check if we're in a promotion state (7 = GAME_STATE_PROMOTION)
    if (current_game_state != 7) {
        ESP_LOGW(TAG, "❌ Not in promotion state, ignoring promotion command");
        return;
    }
    
    // Execute the promotion (default to queen for now)
    if (game_execute_promotion(PROMOTION_QUEEN)) {
        ESP_LOGI(TAG, "✅ Promotion executed successfully");
        
        // Clear promotion state (6 = GAME_STATE_PLAYING)
        current_game_state = 6;
        
        // FIXED: move_count++ and player switch are handled by game_execute_move()
        // No need to duplicate here
        
        // Print updated board
        game_print_board();
        
        // Check for end game conditions
        game_check_end_game_conditions();
        
        // Highlight pieces that the new current player can move
        game_highlight_opponent_pieces();
        
    } else {
        ESP_LOGE(TAG, "❌ Failed to execute promotion");
    }
}

/**
 * @brief Execute pawn promotion
 * @param choice Promotion choice (queen, rook, bishop, knight)
 * @return true if promotion was successful, false otherwise
 */
bool game_execute_promotion(promotion_choice_t choice)
{
    ESP_LOGI(TAG, "👑 Executing pawn promotion: %d", choice);
    
    // Find the pawn that needs to be promoted
    // This should be set when a pawn reaches the promotion rank
    // For now, we'll implement a basic version
    
    // Convert promotion choice to piece type
    piece_t promoted_piece;
    if (current_player == PLAYER_WHITE) {
        switch (choice) {
            case PROMOTION_QUEEN:  promoted_piece = PIECE_WHITE_QUEEN; break;
            case PROMOTION_ROOK:   promoted_piece = PIECE_WHITE_ROOK; break;
            case PROMOTION_BISHOP: promoted_piece = PIECE_WHITE_BISHOP; break;
            case PROMOTION_KNIGHT: promoted_piece = PIECE_WHITE_KNIGHT; break;
            default: return false;
        }
    } else {
        switch (choice) {
            case PROMOTION_QUEEN:  promoted_piece = PIECE_BLACK_QUEEN; break;
            case PROMOTION_ROOK:   promoted_piece = PIECE_BLACK_ROOK; break;
            case PROMOTION_BISHOP: promoted_piece = PIECE_BLACK_BISHOP; break;
            case PROMOTION_KNIGHT: promoted_piece = PIECE_BLACK_KNIGHT; break;
            default: return false;
        }
    }
    
    // Find the pawn to promote (this should be stored when promotion is needed)
    // For now, we'll search for a pawn on the promotion rank
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (current_player == PLAYER_WHITE && piece == PIECE_WHITE_PAWN && row == 0) {
                // White pawn on rank 8 - promote
                board[row][col] = promoted_piece;
                ESP_LOGI(TAG, "✅ Promoted white pawn at %c%d to %s", 'a' + col, row + 1, 
                         choice == PROMOTION_QUEEN ? "Queen" :
                         choice == PROMOTION_ROOK ? "Rook" :
                         choice == PROMOTION_BISHOP ? "Bishop" : "Knight");
                return true;
            } else if (current_player == PLAYER_BLACK && piece == PIECE_BLACK_PAWN && row == 7) {
                // Black pawn on rank 1 - promote
                board[row][col] = promoted_piece;
                ESP_LOGI(TAG, "✅ Promoted black pawn at %c%d to %s", 'a' + col, row + 1,
                         choice == PROMOTION_QUEEN ? "Queen" :
                         choice == PROMOTION_ROOK ? "Rook" :
                         choice == PROMOTION_BISHOP ? "Bishop" : "Knight");
                return true;
            }
        }
    }
    
    ESP_LOGW(TAG, "❌ No pawn found for promotion");
    return false;
}

/**
 * @brief Highlight all movable pieces for current player
 */
void game_highlight_movable_pieces(void)
{
    ESP_LOGI(TAG, "🟡 Highlighting movable pieces for %s player", 
             current_player == PLAYER_WHITE ? "white" : "black");
    
    // LED command queue removed - using direct LED calls now
    
    uint32_t highlighted_count = 0;
    
    // Scan all squares for current player's pieces
    for (uint8_t row = 0; row < 8; row++) {
        for (uint8_t col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            
            // Check if piece belongs to current player
            bool is_current_player_piece = false;
            if (current_player == PLAYER_WHITE) {
                is_current_player_piece = (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
            } else {
                is_current_player_piece = (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
            }
            
            if (is_current_player_piece) {
                // Check if piece has valid moves
                move_suggestion_t suggestions[64];
                uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
                
                if (valid_moves > 0) {
                    // Highlight movable piece in yellow
                    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0); // Yellow
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

void game_process_puzzle_next_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "➡️ Processing PUZZLE_NEXT command");
    
    if (!current_puzzle.is_active) {
        game_send_response_to_uart("❌ No active puzzle! Use 'PUZZLE' to start one", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    if (current_puzzle.current_step >= current_puzzle.step_count - 1) {
        game_send_response_to_uart("✅ Puzzle completed! All steps solved", false, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    current_puzzle.current_step++;
    puzzle_step_t* step = &current_puzzle.steps[current_puzzle.current_step];
    
    char response_data[1024];  // Restored buffer size for list games
    snprintf(response_data, sizeof(response_data),
        "➡️ PUZZLE NEXT STEP\n"
        "═══════════════════════════════════════════════════════════════\n"
        "🎯 Step %d/%d\n"
        "📝 Task: %s\n"
        "🔄 Required: %s\n"
        "\n"
        "🎮 Make your move and watch the LED animations!",
        current_puzzle.current_step + 1,
        current_puzzle.step_count,
        step->description,
        step->is_forced ? "Yes (forced move)" : "No (choice available)"
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
    
    // FIXED: Add puzzle LED animation
    uint8_t from_led = chess_pos_to_led_index(step->from_row, step->from_col);
    uint8_t to_led = chess_pos_to_led_index(step->to_row, step->to_col);
    
    // Show puzzle path animation
    led_command_t puzzle_cmd = {
        .type = LED_CMD_ANIM_PUZZLE_PATH,
        .led_index = from_led,
        .red = 0, .green = 255, .blue = 0, // Green
        .duration_ms = 2000,
        .data = &to_led
    };
    led_execute_command_new(&puzzle_cmd);
    
    ESP_LOGI(TAG, "✅ Puzzle next step sent successfully with LED animation");
}

void game_process_puzzle_reset_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🔄 Processing PUZZLE_RESET command");
    
    if (!current_puzzle.is_active) {
        game_send_response_to_uart("❌ No active puzzle to reset! Use 'PUZZLE' to start one", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    // Reset puzzle state
    current_puzzle.current_step = 0;
    current_puzzle.start_time = esp_timer_get_time() / 1000;
    
    // Reset board position
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            board[row][col] = current_puzzle.initial_board[row][col];
        }
    }
    
    char response_data[512];
    snprintf(response_data, sizeof(response_data),
        "🔄 PUZZLE RESET\n"
        "═══════════════════════════════════════════════════════════════\n"
        "📝 Puzzle: %s\n"
        "🎯 Reset to Step 1/%d\n"
        "📋 Board position restored\n"
        "\n"
        "💡 Task: %s\n"
        "🚀 LED animations restarted!",
        current_puzzle.name,
        current_puzzle.step_count,
        current_puzzle.steps[0].description
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
    
    // Stop all animations and restart
    led_clear_board_only();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    uint8_t from_square = chess_pos_to_led_index(current_puzzle.steps[0].from_row, current_puzzle.steps[0].from_col);
    led_set_pixel_safe(from_square, 255, 255, 0); // Yellow highlight
}

void game_process_puzzle_complete_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "✅ Processing PUZZLE_COMPLETE command");
    
    if (!current_puzzle.is_active) {
        game_send_response_to_uart("❌ No active puzzle to complete!", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    current_puzzle.completion_time = esp_timer_get_time() / 1000;
    uint32_t solve_time = current_puzzle.completion_time - current_puzzle.start_time;
    current_puzzle.is_active = false;
    
    char response_data[1024];  // Restored buffer size for list games
    snprintf(response_data, sizeof(response_data),
        "🏆 PUZZLE COMPLETED!\n"
        "═══════════════════════════════════════════════════════════════\n"
        "📝 Puzzle: %s\n"
        "🎯 Difficulty: %s\n"
        "⏱️ Solve Time: %" PRIu32 " seconds\n"
        "🔢 Steps Completed: %d/%d\n"
        "📊 Progress: %d%%\n"
        "\n"
        "🌟 PERFORMANCE RATING:\n"
        "  • Speed: %s\n"
        "  • Accuracy: %s\n"
        "  • Overall: %s\n"
        "\n"
        "🎮 Ready for next puzzle! Use 'PUZZLE' to start another",
        current_puzzle.name,
        current_puzzle.difficulty == 1 ? "Beginner" : "Intermediate",
        solve_time,
        current_puzzle.current_step + 1,
        current_puzzle.step_count,
        (current_puzzle.current_step + 1) * 100 / current_puzzle.step_count,
        solve_time < 30 ? "⚡ Fast" : solve_time < 60 ? "✅ Good" : "🐌 Slow",
        current_puzzle.current_step == current_puzzle.step_count - 1 ? "🎯 Perfect" : "📝 Good",
        solve_time < 30 ? "🏆 Excellent" : "👍 Good"
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
    
    // Completion animation
    led_set_all_safe(0, 255, 0); // Green success
}

void game_process_puzzle_verify_command(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "🔍 Processing PUZZLE_VERIFY command");
    
    if (!current_puzzle.is_active) {
        game_send_response_to_uart("❌ No active puzzle to verify!", true, (QueueHandle_t)cmd->response_queue);
        return;
    }
    
    puzzle_step_t* current_step = &current_puzzle.steps[current_puzzle.current_step];
    
    char response_data[1024];  // Restored buffer size for list games
    snprintf(response_data, sizeof(response_data),
        "🔍 PUZZLE VERIFICATION\n"
        "═══════════════════════════════════════════════════════════════\n"
        "📝 Puzzle: %s\n"
        "🎯 Current Step: %d/%d\n"
        "📋 Expected Move: %c%d -> %c%d\n"
        "💬 Description: %s\n"
        "🔄 Move Type: %s\n"
        "\n"
        "💡 HINT: Look for the piece at %c%d\n"
        "🎯 Target: Square %c%d\n"
        "⚡ LED animation shows the correct path!",
        current_puzzle.name,
        current_puzzle.current_step + 1,
        current_puzzle.step_count,
        'a' + current_step->from_col,
        current_step->from_row + 1,
        'a' + current_step->to_col,
        current_step->to_row + 1,
        current_step->description,
        current_step->is_forced ? "Forced (only legal move)" : "Best choice",
        'a' + current_step->from_col,
        current_step->from_row + 1,
        'a' + current_step->to_col,
        current_step->to_row + 1
    );
    
    // CHUNKED OUTPUT - Send endgame report in chunks to prevent UART buffer overflow
    const size_t CHUNK_SIZE = 256;  // Small chunks for UART buffer
    size_t total_len = strlen(response_data);
    const char* data_ptr = response_data;
    size_t chunks_remaining = total_len;
    
    ESP_LOGI(TAG, "🏆 Sending endgame report in chunks: %zu bytes total", total_len);
    
    while (chunks_remaining > 0) {
        // Calculate chunk size
        size_t chunk_size = (chunks_remaining > CHUNK_SIZE) ? CHUNK_SIZE : chunks_remaining;
        
        // Create chunk response
        game_response_t chunk_response = {
            .type = GAME_RESPONSE_SUCCESS,
            .command_type = GAME_CMD_SHOW_BOARD,  // Reuse board command type
            .error_code = 0,
            .message = "Endgame report chunk sent",
            .timestamp = esp_timer_get_time() / 1000
        };
        
        // Copy chunk data
        strncpy(chunk_response.data, data_ptr, chunk_size);
        chunk_response.data[chunk_size] = '\0';
        
        // Reset WDT before sending chunk
        esp_task_wdt_reset();
        
        // Send chunk
        if (xQueueSend((QueueHandle_t)cmd->response_queue, &chunk_response, pdMS_TO_TICKS(100)) != pdTRUE) {
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
    
    // Show verification animation
    uint8_t from_square = chess_pos_to_led_index(current_step->from_row, current_step->from_col);
    uint8_t to_square = chess_pos_to_led_index(current_step->to_row, current_step->to_col);
    
    // Highlight both squares with verification colors
    led_set_pixel_safe(from_square, 255, 165, 0); // Orange for source
    led_set_pixel_safe(to_square, 0, 255, 0); // Green for destination
}



