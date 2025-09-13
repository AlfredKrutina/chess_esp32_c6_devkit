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


#include "game_task.h"
#include "freertos_chess.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>


static const char *TAG = "GAME_TASK";


// ============================================================================
// LOCAL VARIABLES AND CONSTANTS
// ============================================================================


// Game configuration
#define MAX_MOVES_HISTORY    200     // Maximum moves to remember
#define GAME_TIMEOUT_MS      300000  // 5 minutes per move timeout
#define MOVE_VALIDATION_MS   100     // Move validation timeout

// Game state
static game_state_t current_game_state = GAME_STATE_IDLE;
static player_t current_player = PLAYER_WHITE;
static uint32_t move_count = 0;

// Board representation (8x8)
static piece_t board[8][8] = {0};
static bool piece_moved[8][8] = {false}; // Track if pieces have moved

// Move history
static chess_move_t move_history[MAX_MOVES_HISTORY];
static uint32_t history_index = 0;

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
static uint32_t position_hash = 0;
static uint32_t position_history[100];
static uint32_t position_history_count = 0;

// Game state flags
static bool timer_enabled = true;
static bool game_saved = false;
static char saved_game_name[32] = "";
static game_state_t game_result = GAME_STATE_IDLE;

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
    
    return hash;
}

/**
 * @brief Check if current position has been repeated
 * @return true if position is repeated (potential draw)
 */
bool game_is_position_repeated(void)
{
    uint32_t current_hash = game_calculate_position_hash();
    
    // Check last 50 positions for repetition
    for (int i = position_history_count - 1; i >= 0 && i >= position_history_count - 50; i--) {
        if (position_history[i] == current_hash) {
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Add current position to history
 */
void game_add_position_to_history(void)
{
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
static uint32_t white_captured_count = 0;
static uint32_t black_captured_count = 0;
static piece_t white_captured_pieces[16];  // Max 16 pieces can be captured
static piece_t black_captured_pieces[16];
static uint32_t white_captured_index = 0;
static uint32_t black_captured_index = 0;


// ============================================================================
// ENHANCED MOVE VALIDATION SYSTEM
// ============================================================================

// Tutorial mode state
static bool tutorial_mode_active = false;
static bool show_hints = true;
static bool show_warnings = true;
static bool show_analysis = true;

// Move analysis cache
static move_suggestion_t move_suggestions[100];
static uint32_t suggestion_count = 0;
static uint32_t last_analysis_time = 0;


// Piece symbols for display (ASCII)
static const char* piece_symbols[] = {
    " ", "p", "n", "b", "r", "q", "k",    // White pieces: p=pawn, n=knight, b=bishop, r=rook, q=queen, k=king
    " ", "P", "N", "B", "R", "Q", "K"     // Black pieces: P=pawn, N=knight, B=bishop, R=rook, Q=queen, K=king
};


// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================


void game_initialize_board(void)
{
    ESP_LOGI(TAG, "Initializing chess board...");
    
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
    
    ESP_LOGI(TAG, "Chess board initialized successfully");
    ESP_LOGI(TAG, "Initial position: White pieces at bottom, Black pieces at top");
    game_print_board();
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
    position_history_count = 0;
    game_result = GAME_STATE_IDLE;
    game_saved = false;
    saved_game_name[0] = '\0';
    
    // Clear move history
    memset(move_history, 0, sizeof(move_history));
    history_index = 0;
    
    // Clear captured pieces
    white_captured_count = 0;
    black_captured_count = 0;
    white_captured_index = 0;
    black_captured_index = 0;
    
    // Clear last move tracking
    has_last_move = false;
    
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
    current_game_state = GAME_STATE_ACTIVE;
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
    position_history_count = 0;
    game_result = GAME_STATE_IDLE;
    game_saved = false;
    saved_game_name[0] = '\0';
    
    total_games++;
    
    ESP_LOGI(TAG, "New game started - White to move");
    ESP_LOGI(TAG, "Total games: %" PRIu32, total_games);
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
    int row_diff = move->to_row - move->from_row;
    int col_diff = move->to_col - move->from_col;
    int abs_row_diff = abs(row_diff);
    int abs_col_diff = abs(col_diff);
    
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
    // This is a placeholder - in a real implementation, you would:
    // 1. Make the move temporarily
    // 2. Check if king is in check
    // 3. Undo the move
    // For now, return false (move is safe)
    return false;
}

bool game_is_en_passant_possible(const chess_move_t* move)
{
    // This is a placeholder - in a real implementation, you would:
    // 1. Check if the last move was a pawn moving two squares
    // 2. Check if current move is to the en passant square
    // For now, return false
    return false;
}

move_error_t game_validate_castling(const chess_move_t* move)
{
    // This is a placeholder - in a real implementation, you would:
    // 1. Check if king and rook have moved
    // 2. Check if squares between are empty
    // 3. Check if king is not in check
    // 4. Check if king doesn't move through check
    // For now, return blocked
    return MOVE_ERROR_CASTLING_BLOCKED;
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
    
    switch (error) {
        case MOVE_ERROR_NO_PIECE:
            ESP_LOGE(TAG, "Invalid move: no piece at %s", from_square);
            break;
            
        case MOVE_ERROR_WRONG_COLOR:
            ESP_LOGE(TAG, "Invalid move: %s cannot move %s's piece", player_name, 
                     (current_player == PLAYER_WHITE) ? "Black" : "White");
            break;
            
        case MOVE_ERROR_BLOCKED_PATH:
            ESP_LOGE(TAG, "Invalid move: path from %s to %s is blocked", from_square, to_square);
            break;
            
        case MOVE_ERROR_INVALID_PATTERN:
            ESP_LOGE(TAG, "Invalid move: %s cannot move from %s to %s", piece_name, from_square, to_square);
            break;
            
        case MOVE_ERROR_KING_IN_CHECK:
            ESP_LOGE(TAG, "Invalid move: this move would leave your king in check");
            break;
            
        case MOVE_ERROR_CASTLING_BLOCKED:
            ESP_LOGE(TAG, "Invalid move: castling is not allowed (king or rook has moved)");
            break;
            
        case MOVE_ERROR_EN_PASSANT_INVALID:
            ESP_LOGE(TAG, "Invalid move: en passant is not possible");
            break;
            
        case MOVE_ERROR_DESTINATION_OCCUPIED:
            ESP_LOGE(TAG, "Invalid move: destination %s is occupied by your own piece", to_square);
            break;
            
        case MOVE_ERROR_OUT_OF_BOUNDS:
            ESP_LOGE(TAG, "Invalid move: coordinates are out of board bounds");
            break;
            
        case MOVE_ERROR_GAME_NOT_ACTIVE:
            ESP_LOGE(TAG, "Invalid move: game is not active");
            break;
            
        case MOVE_ERROR_INVALID_MOVE_STRUCTURE:
            ESP_LOGE(TAG, "Invalid move: move structure is invalid");
            break;
            
        default:
            ESP_LOGE(TAG, "Invalid move: unknown error occurred");
            break;
    }
    
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
        ESP_LOGI(TAG, "💡 Hint: %s at %c%c has no legal moves", 
                 game_get_piece_name(piece), 'a' + col, '8' - row);
        return;
    }
    
    ESP_LOGI(TAG, "💡 Hint: %s at %c%c can move to:", 
             game_get_piece_name(piece), 'a' + col, '8' - row);
    
    // Group moves by type
    char normal_moves[256] = "";
    char capture_moves[256] = "";
    char special_moves[256] = "";
    
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
    
    // Check all possible destination squares
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
                suggestions[count].is_castling = false; // TODO: Implement castling detection
                suggestions[count].is_en_passant = false; // TODO: Implement en passant detection
                suggestions[count].score = 0; // TODO: Implement move scoring
                
                count++;
            }
        }
    }
    
    return count;
}


// ============================================================================
// MOVE EXECUTION FUNCTIONS
// ============================================================================


bool game_execute_move(const chess_move_t* move)
{
    if (!game_is_valid_move(move)) {
        ESP_LOGW(TAG, "Invalid move attempted");
        return false;
    }
    
    ESP_LOGI(TAG, "Executing move: %c%d-%c%d", 
              'a' + move->from_col, 8 - move->from_row,
              'a' + move->to_col, 8 - move->to_row);
    
    // Get pieces
    piece_t source_piece = game_get_piece(move->from_row, move->from_col);
    piece_t dest_piece = game_get_piece(move->to_row, move->to_col);
    
    // Check for capture
    if (dest_piece != PIECE_EMPTY) {
        ESP_LOGI(TAG, "Capture: %s captures %s", 
                  game_get_piece_name(source_piece),
                  game_get_piece_name(dest_piece));
    }
    
    // Execute move
    game_set_piece(move->to_row, move->to_col, source_piece);
    game_set_piece(move->from_row, move->from_col, PIECE_EMPTY);
    
    // Mark piece as moved
    piece_moved[move->to_row][move->to_col] = true;
    
    // Add to move history
    if (history_index < MAX_MOVES_HISTORY) {
        move_history[history_index] = *move;
        move_history[history_index].piece = source_piece;
        move_history[history_index].captured_piece = dest_piece;
        move_history[history_index].timestamp = esp_timer_get_time() / 1000;
        history_index++;
    }
    
    // Update game state
    move_count++;
    last_move_time = esp_timer_get_time() / 1000;
    
    // Switch players
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    ESP_LOGI(TAG, "Move executed successfully. %s to move", 
              (current_player == PLAYER_WHITE) ? "White" : "Black");
    
    return true;
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
    ESP_LOGI(TAG, "=== Chess Board ===");
    
    // Print column headers
    printf("    a   b   c   d   e   f   g   h\n");
    printf("  +---+---+---+---+---+---+---+---+\n");
    
    // Print board rows (8 to 1)
    for (int row = 7; row >= 0; row--) {
        printf(" %d |", 8 - row);
        
        // Print pieces in this row
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            
            // Check if this square is part of the last move
            bool is_last_move = has_last_move && 
                ((row == last_move_from_row && col == last_move_from_col) ||
                 (row == last_move_to_row && col == last_move_to_col));
            
            if (piece == PIECE_EMPTY) {
                if (is_last_move) {
                    printf(" * |");  // Highlight empty square from last move
            } else {
                    printf("   |");
                }
            } else {
                if (is_last_move) {
                    printf("*%s*|", piece_symbols[piece]);  // Highlight piece from last move
                } else {
                    printf(" %s |", piece_symbols[piece]);
                }
            }
        }
        printf(" %d\n", 8 - row);
        
        // Print row separator (except after last row)
        if (row > 0) {
            printf("  +---+---+---+---+---+---+---+---+\n");
        }
    }
    
    // Print bottom border and column headers
    printf("  +---+---+---+---+---+---+---+---+\n");
    printf("    a   b   c   d   e   f   g   h\n");
    
    // Print last move information
    if (has_last_move) {
        char from_square[4], to_square[4];
        game_coords_to_square(last_move_from_row, last_move_from_col, from_square);
        game_coords_to_square(last_move_to_row, last_move_to_col, to_square);
        ESP_LOGI(TAG, "Last move: *%s* -> *%s*", from_square, to_square);
    }
    
    // Print captured pieces
    if (white_captured_count > 0 || black_captured_count > 0) {
        ESP_LOGI(TAG, "Captured pieces:");
        if (white_captured_count > 0) {
            printf("  White captured: ");
            for (uint32_t i = 0; i < white_captured_index; i++) {
                printf("%s ", piece_symbols[white_captured_pieces[i]]);
            }
            printf("(%" PRIu32 " total)\n", white_captured_count);
        }
        if (black_captured_count > 0) {
            printf("  Black captured: ");
            for (uint32_t i = 0; i < black_captured_index; i++) {
                printf("%s ", piece_symbols[black_captured_pieces[i]]);
            }
            printf("(%" PRIu32 " total)\n", black_captured_count);
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
    ESP_LOGI(TAG, "Move history (%lu moves):", history_index);
    
    for (uint32_t i = 0; i < history_index; i++) {
        const chess_move_t* move = &move_history[i];
        ESP_LOGI(TAG, "  %lu. %c%d-%c%d %s", 
                  i + 1,
                  'a' + move->from_col, 8 - move->from_row,
                  'a' + move->to_col, 8 - move->to_row,
                  game_get_piece_name(move->piece));
    }
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
 * @brief Process chess move command from UART
 * @param cmd Chess move command
 */
void game_process_chess_move(const chess_move_command_t* cmd)
{
    if (!cmd) return;
    
    ESP_LOGI(TAG, "Processing chess move: %s -> %s (player: %d)", 
              cmd->from_notation, cmd->to_notation, cmd->player);
    
    // Convert notation to coordinates
    uint8_t from_row, from_col, to_row, to_col;
    if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col) ||
        !convert_notation_to_coords(cmd->to_notation, &to_row, &to_col)) {
        ESP_LOGE(TAG, "Invalid notation: %s -> %s", cmd->from_notation, cmd->to_notation);
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
    
    // Validate move
    move_error_t error = game_is_valid_move(&move);
    
    if (error == MOVE_ERROR_NONE) {
        // Execute move
        if (game_execute_move(&move)) {
            ESP_LOGI(TAG, "Move executed successfully: %s -> %s", cmd->from_notation, cmd->to_notation);
            // TODO: Send response back to UART
        } else {
            ESP_LOGE(TAG, "Failed to execute move");
        }
    } else {
        ESP_LOGE(TAG, "Invalid move: error %d", error);
        // TODO: Send error response back to UART
    }
}


void game_process_commands(void)
{
    // Process commands from queue
    if (game_command_queue != NULL) {
        // Try to receive move command (larger structure)
        typedef struct {
            uint8_t command_type;
            uint8_t from_row;
            uint8_t from_col;
            uint8_t to_row;
            uint8_t to_col;
        } move_command_t;
        
        move_command_t move_cmd;
        if (xQueueReceive(game_command_queue, &move_cmd, 0) == pdTRUE) {
            switch (move_cmd.command_type) {
                case 0: // Reset game
                    game_reset_game();
                    break;
                    
                case 1: // Start new game
                    game_start_new_game();
                    break;
                    
                case 2: // Print board
                    game_print_board();
                    break;
                    
                case 3: // Print move history
                    game_print_move_history();
                    break;
                    
                case 4: // Print game status
                    game_print_status();
                    break;
                    
                case 5: // MOVE command
                    game_process_move_command(&move_cmd);
                    break;
                    
                case 10: // STATS command
                    game_print_game_stats();
                    break;
                    
                case 11: // SCORE command
                    {
                        int white_material, black_material;
                        int balance = game_calculate_material_balance(&white_material, &black_material);
                        char material_str[32];
                        game_get_material_string(material_str, sizeof(material_str));
                        
                        ESP_LOGI(TAG, "=== Material Score ===");
                        ESP_LOGI(TAG, "White material: %d points", white_material);
                        ESP_LOGI(TAG, "Black material: %d points", black_material);
                        ESP_LOGI(TAG, "Balance: %s", material_str);
                        ESP_LOGI(TAG, "Piece values: P=1, N=3, B=3, R=5, Q=9, K=∞");
                    }
                    break;
                    
                case 12: // TIMER ON command
                    game_toggle_timer(true);
                    break;
                    
                case 13: // TIMER OFF command
                    game_toggle_timer(false);
                    break;
                    
                case 14: // SAVE command
                    game_save_game("auto_save");
                    break;
                    
                case 15: // LOAD command
                    game_load_game("auto_save");
                    break;
                    
                case 16: // EXPORT command
                    {
                        char pgn_buffer[2048];
                        game_export_pgn(pgn_buffer, sizeof(pgn_buffer));
                        ESP_LOGI(TAG, "=== PGN Export ===");
                        ESP_LOGI(TAG, "%s", pgn_buffer);
                    }
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown game command: %d", move_cmd.command_type);
                    break;
            }
        }
        
        // Also try to receive simple commands (backward compatibility)
        uint8_t simple_command;
        if (xQueueReceive(game_command_queue, &simple_command, 0) == pdTRUE) {
            switch (simple_command) {
                case 0: // Reset game
                    game_reset_game();
                    break;
                    
                case 1: // Start new game
                    game_start_new_game();
                    break;
                    
                case 2: // Print board
                    game_print_board();
                    break;
                    
                case 3: // Print move history
                    game_print_move_history();
                    break;
                    
                case 4: // Print game status
                    game_print_status();
                    break;
                    
                default:
                    ESP_LOGW(TAG, "Unknown simple game command: %d", simple_command);
                    break;
            }
        }
    }
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
        // Display detailed error message
        game_display_move_error(move_error, &chess_move);
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
    move_count++;
    last_move_time = esp_timer_get_time() / 1000;
    
    // Switch player
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
    // Add to move history
    if (history_index < MAX_MOVES_HISTORY) {
        move_history[history_index].from_row = move_cmd->from_row;
        move_history[history_index].from_col = move_cmd->from_col;
        move_history[history_index].to_row = move_cmd->to_row;
        move_history[history_index].to_col = move_cmd->to_col;
        move_history[history_index].piece = from_piece;
        move_history[history_index].captured_piece = to_piece;
        move_history[history_index].timestamp = last_move_time;
        history_index++;
    }
    
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
    
    // Animation sequence with delays
    vTaskDelay(pdMS_TO_TICKS(500));  // Pause for readability
    
    ESP_LOGI(TAG, "🎯 Move: %s -> %s", from_square, to_square);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    ESP_LOGI(TAG, "♟️  %s %s moves...", piece_symbol, piece_name);
    vTaskDelay(pdMS_TO_TICKS(300));
    
    ESP_LOGI(TAG, "✨ ...to %s", to_square);
    vTaskDelay(pdMS_TO_TICKS(200));
    
    if (captured != PIECE_EMPTY) {
        ESP_LOGI(TAG, "💥 %s captured!", game_get_piece_name(captured));
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    
    ESP_LOGI(TAG, "✅ Move completed!");
    vTaskDelay(pdMS_TO_TICKS(200));
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
 * @brief Check if player has any legal moves
 * @param player Player to check
 * @return true if player has legal moves
 */
bool game_has_legal_moves(player_t player)
{
    // Check all pieces of the player
    piece_t player_pieces_start = (player == PLAYER_WHITE) ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;
    piece_t player_pieces_end = (player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
    
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            piece_t piece = board[row][col];
            if (piece >= player_pieces_start && piece <= player_pieces_end) {
                // Check all possible destinations for this piece
                for (int to_row = 0; to_row < 8; to_row++) {
                    for (int to_col = 0; to_col < 8; to_col++) {
                        if (to_row == row && to_col == col) continue;
                        
                        chess_move_t temp_move = {
                            .from_row = row,
                            .from_col = col,
                            .to_row = to_row,
                            .to_col = to_col,
                            .piece = piece,
                            .captured_piece = board[to_row][to_col],
                            .timestamp = 0
                        };
                        
                        // Check if move is valid and doesn't leave king in check
                        if (game_is_valid_move(&temp_move) == MOVE_ERROR_NONE) {
                            return true; // Found a legal move
                        }
                    }
                }
            }
        }
    }
    
    return false; // No legal moves found
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
    int white_material, black_material;
    game_calculate_material_balance(&white_material, &black_material);
    if (white_material <= 1 && black_material <= 1) {
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
    
    // Add moves
    for (uint32_t i = 0; i < history_index && pos < buffer_size - 50; i++) {
        const chess_move_t* move = &move_history[i];
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
void game_coords_to_square(uint8_t row, uint8_t col, char* square)
{
    if (square == NULL) return;
    
    // Convert column to file (a-h)
    square[0] = 'a' + col;
    
    // Convert row to rank (1-8) - chess notation is bottom-up
    square[1] = '8' - row;
    
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
    
    // CRITICAL: Register this task with Task Watchdog Timer
    esp_err_t ret = esp_task_wdt_add(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Game task with TWDT: %s", esp_err_to_name(ret));
        // Continue anyway, but log the error
    } else {
        ESP_LOGI(TAG, "✓ Game task registered with Task Watchdog Timer");
    }
    
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
        // CRITICAL: Reset watchdog for game task in every iteration
        esp_task_wdt_reset();
        
        // Process game commands
        game_process_commands();
        
        // Process matrix events (moves)
        game_process_matrix_events();
        
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
            if (event.type == MATRIX_EVENT_MOVE_DETECTED) {
                // Convert matrix event to chess move
                chess_move_t move = {
                    .from_row = event.from_row,
                    .from_col = event.from_col,
                    .to_row = event.to_row,
                    .to_col = event.to_col,
                    .piece = PIECE_EMPTY,
                    .captured_piece = PIECE_EMPTY,
                    .timestamp = 0
                };
                
                // Execute move if valid
                if (game_execute_move(&move)) {
                    ESP_LOGI(TAG, "Matrix move executed successfully");
                } else {
                    ESP_LOGW(TAG, "Invalid matrix move rejected");
                }
            }
        }
    }
}

// ============================================================================
// ENHANCED CHESS LOGIC IMPLEMENTATION
// ============================================================================

// Direction vectors for piece movement
static const int8_t knight_moves[8][2] = {
    {-2,-1}, {-2,1}, {-1,-2}, {-1,2}, {1,-2}, {1,2}, {2,-1}, {2,1}
};

static const int8_t king_moves[8][2] = {
    {-1,-1}, {-1,0}, {-1,1}, {0,-1}, {0,1}, {1,-1}, {1,0}, {1,1}
};

static const int8_t bishop_dirs[4][2] = {
    {-1,-1}, {-1,1}, {1,-1}, {1,1}
};

static const int8_t rook_dirs[4][2] = {
    {-1,0}, {1,0}, {0,-1}, {0,1}
};

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

// Move generation buffers
static chess_move_extended_t legal_moves_buffer[128];
static uint32_t legal_moves_count = 0;

// Game position history for draw detection
static uint32_t position_hashes[100];
static uint32_t position_history_count = 0;
static uint32_t fifty_move_counter = 0;

// Promotion state
static bool promotion_pending = false;
static chess_move_extended_t promotion_move;

// ============================================================================
// BASIC UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Check if coordinates are valid
 */
static inline bool game_is_valid_square(int row, int col) {
    return (row >= 0 && row < 8 && col >= 0 && col < 8);
}

/**
 * @brief Check if piece is white
 */
static inline bool game_is_white_piece(piece_t piece) {
    return (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
}

/**
 * @brief Check if piece is black
 */
static inline bool game_is_black_piece(piece_t piece) {
    return (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
}

/**
 * @brief Check if piece belongs to current player
 */
static inline bool game_is_own_piece(piece_t piece, player_t player) {
    if (piece == PIECE_EMPTY) return false;
    return (player == PLAYER_WHITE) ? game_is_white_piece(piece) : game_is_black_piece(piece);
}

/**
 * @brief Check if piece belongs to opponent
 */
static inline bool game_is_enemy_piece(piece_t piece, player_t player) {
    if (piece == PIECE_EMPTY) return false;
    return (player == PLAYER_WHITE) ? game_is_black_piece(piece) : game_is_white_piece(piece);
}

/**
 * @brief Get piece name for display
 */
const char* game_get_piece_name(piece_t piece) {
    switch (piece) {
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
        default: return "Empty";
    }
}

/**
 * @brief Convert coordinates to chess notation (e.g., 0,0 -> "a1")
 */
void game_coords_to_square(uint8_t row, uint8_t col, char* notation) {
    if (notation == NULL) return;
    notation[0] = 'a' + col;
    notation[1] = '1' + row;
    notation[2] = '\0';
}

/**
 * @brief Convert chess notation to coordinates (e.g., "e4" -> 3,4)
 */
bool game_square_to_coords(const char* notation, uint8_t* row, uint8_t* col) {
    if (notation == NULL || row == NULL || col == NULL) return false;
    if (strlen(notation) != 2) return false;
    
    char file = tolower(notation[0]);
    char rank = notation[1];
    
    if (file < 'a' || file > 'h' || rank < '1' || rank > '8') {
        return false;
    }
    
    *col = file - 'a';
    *row = rank - '1';
    return true;
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

/**
 * @brief Check if player's king is in check
 */
bool game_is_king_in_check(player_t player) {
    uint8_t king_row, king_col;
    if (!game_find_king(player, &king_row, &king_col)) {
        return false; // King not found (shouldn't happen)
    }
    
    player_t opponent = (player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    return game_is_square_attacked(king_row, king_col, opponent);
}

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
                if (legal_moves_count >= 128) return;
                
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
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
            if (legal_moves_count >= 128) return;
            
            chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
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
            if (from_row == start_row && board[to_row + direction][from_col] == PIECE_EMPTY) {
                if (legal_moves_count >= 128) return;
                
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
                    for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT; promo++) {
                        if (legal_moves_count >= 128) return;
                        
                        chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
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
                    if (legal_moves_count >= 128) return;
                    
                    chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
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
                if (legal_moves_count >= 128) return;
                
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
                move->from_row = from_row;
                move->from_col = from_col;
                move->to_row = en_passant_target_row;
                move->to_col = en_passant_target_col;
                move->piece = pawn;
                move->captured_piece = board[en_passant_victim_row][en_passant_victim_col];
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
void game_generate_knight_moves(uint8_t from_row, uint8_t from_col, player_t player) {
    piece_t knight = board[from_row][from_col];
    
    for (int i = 0; i < 8; i++) {
        int to_row = from_row + knight_moves[i][0];
        int to_col = from_col + knight_moves[i][1];
        
        if (!game_is_valid_square(to_row, to_col)) continue;
        
        piece_t target = board[to_row][to_col];
        
        // Skip if occupied by own piece
        if (game_is_own_piece(target, player)) continue;
        
        if (legal_moves_count >= 128) return;
        
        chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = to_col;
        move->piece = knight;
        move->captured_piece = target;
        move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
        
        if (game_simulate_move_check(move, player)) {
            legal_moves_count++;
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
            
            if (legal_moves_count >= 128) return;
            
            chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = to_row;
            move->to_col = to_col;
            move->piece = piece;
            move->captured_piece = target;
            move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
            
            if (game_simulate_move_check(move, player)) {
                legal_moves_count++;
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
        
        if (legal_moves_count >= 128) return;
        
        chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = to_col;
        move->piece = king;
        move->captured_piece = target;
        move->move_type = (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;
        
        if (game_simulate_move_check(move, player)) {
            legal_moves_count++;
        }
    }
    
    // Castling
    if (game_is_king_in_check(player)) return; // Can't castle when in check
    
    if (player == PLAYER_WHITE && !white_king_moved) {
        // White kingside castling
        if (!white_rook_h_moved &&
            board[0][5] == PIECE_EMPTY && board[0][6] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 5, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 6, PLAYER_BLACK)) {
            
            if (legal_moves_count < 128) {
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
                move->from_row = 0;
                move->from_col = 4;
                move->to_row = 0;
                move->to_col = 6;
                move->piece = king;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_CASTLE_KING;
                legal_moves_count++;
            }
        }
        
        // White queenside castling
        if (!white_rook_a_moved &&
            board[0][1] == PIECE_EMPTY && board[0][2] == PIECE_EMPTY && board[0][3] == PIECE_EMPTY &&
            !game_is_square_attacked(0, 2, PLAYER_BLACK) &&
            !game_is_square_attacked(0, 3, PLAYER_BLACK)) {
            
            if (legal_moves_count < 128) {
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
                move->from_row = 0;
                move->from_col = 4;
                move->to_row = 0;
                move->to_col = 2;
                move->piece = king;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_CASTLE_QUEEN;
                legal_moves_count++;
            }
        }
    }
    
    if (player == PLAYER_BLACK && !black_king_moved) {
        // Black kingside castling
        if (!black_rook_h_moved &&
            board[7][5] == PIECE_EMPTY && board[7][6] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 5, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 6, PLAYER_WHITE)) {
            
            if (legal_moves_count < 128) {
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
                move->from_row = 7;
                move->from_col = 4;
                move->to_row = 7;
                move->to_col = 6;
                move->piece = king;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_CASTLE_KING;
                legal_moves_count++;
            }
        }
        
        // Black queenside castling
        if (!black_rook_a_moved &&
            board[7][1] == PIECE_EMPTY && board[7][2] == PIECE_EMPTY && board[7][3] == PIECE_EMPTY &&
            !game_is_square_attacked(7, 2, PLAYER_WHITE) &&
            !game_is_square_attacked(7, 3, PLAYER_WHITE)) {
            
            if (legal_moves_count < 128) {
                chess_move_extended_t* move = &legal_moves_buffer[legal_moves_count];
                move->from_row = 7;
                move->from_col = 4;
                move->to_row = 7;
                move->to_col = 2;
                move->piece = king;
                move->captured_piece = PIECE_EMPTY;
                move->move_type = MOVE_TYPE_CASTLE_QUEEN;
                legal_moves_count++;
            }
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
    
    return legal_moves_count;
}

// ============================================================================
// MOVE EXECUTION
// ============================================================================

/**
 * @brief Execute a chess move on the board
 */
bool game_execute_move(chess_move_extended_t* move) {
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
    move_count++;
    if (current_player == PLAYER_WHITE) {
        white_moves_count++;
    } else {
        black_moves_count++;
    }
    
    // Switch players
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    
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
        chess_move_extended_t* move = &legal_moves_buffer[i];
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
    position_history_count = 0;
    
    ESP_LOGI(TAG, "Enhanced chess board initialized successfully");
    game_print_board_enhanced();
}
