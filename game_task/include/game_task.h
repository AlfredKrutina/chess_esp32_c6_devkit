/**
 * @file game_task.h
 * @brief ESP32-C6 Chess System v2.4 - Game Task Header
 * 
 * This header defines the interface for the game task:
 * - Game state types and structures
 * - Game task function prototypes
 * - Game control and status functions
 * 
 * Author: Alfred Krutina
 * Version: 2.4
 * Date: 2025-08-24
 */

#ifndef GAME_TASK_H
#define GAME_TASK_H


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>


// ============================================================================
// CONSTANTS AND DEFINITIONS
// ============================================================================


// Common types are defined in chess_types.h
#include "chess_types.h"


// Chess move structure and move suggestion structure are defined in chess_types.h


// ============================================================================
// UTILITY FUNCTION PROTOTYPES
// ============================================================================

/**
 * @brief Convert chess notation to board coordinates
 * @param notation Chess notation (e.g., "e2")
 * @param row Output row coordinate (0-7)
 * @param col Output column coordinate (0-7)
 * @return true if conversion successful, false otherwise
 */
bool convert_notation_to_coords(const char* notation, uint8_t* row, uint8_t* col);

/**
 * @brief Convert board coordinates to chess notation
 * @param row Row coordinate (0-7)
 * @param col Column coordinate (0-7)
 * @param notation Output notation buffer (must be at least 3 chars)
 * @return true if conversion successful, false otherwise
 */
bool convert_coords_to_notation(uint8_t row, uint8_t col, char* notation);

/**
 * @brief Process chess move command from UART
 * @param cmd Chess move command
 */
void game_process_chess_move(const chess_move_command_t* cmd);

// ============================================================================
// TASK FUNCTION PROTOTYPES
// ============================================================================


/**
 * @brief Start the game task
 * @param pvParameters Task parameters (unused)
 */
void game_task_start(void *pvParameters);


// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================


/**
 * @brief Initialize the chess board to starting position
 */
void game_initialize_board(void);

/**
 * @brief Reset the game to initial state
 */
void game_reset_game(void);

/**
 * @brief Start a new game
 */
void game_start_new_game(void);


// ============================================================================
// BOARD UTILITY FUNCTIONS
// ============================================================================


/**
 * @brief Check if position is valid on the board
 * @param row Row index (0-7)
 * @param col Column index (0-7)
 * @return true if position is valid
 */
bool game_is_valid_position(int row, int col);

/**
 * @brief Get piece at specified position
 * @param row Row index (0-7)
 * @param col Column index (0-7)
 * @return Piece at position
 */
piece_t game_get_piece(int row, int col);

/**
 * @brief Set piece at specified position
 * @param row Row index (0-7)
 * @param col Column index (0-7)
 * @param piece Piece to set
 */
void game_set_piece(int row, int col, piece_t piece);

/**
 * @brief Check if position is empty
 * @param row Row index (0-7)
 * @param col Column index (0-7)
 * @return true if position is empty
 */
bool game_is_empty(int row, int col);

/**
 * @brief Check if piece is white
 * @param piece Piece to check
 * @return true if piece is white
 */
bool game_is_white_piece(piece_t piece);

/**
 * @brief Check if piece is black
 * @param piece Piece to check
 * @return true if piece is black
 */
bool game_is_black_piece(piece_t piece);

/**
 * @brief Check if two pieces are the same color
 * @param piece1 First piece
 * @param piece2 Second piece
 * @return true if pieces are same color
 */
bool game_is_same_color(piece_t piece1, piece_t piece2);

/**
 * @brief Check if piece belongs to opponent
 * @param piece Piece to check
 * @param player Current player
 * @return true if piece belongs to opponent
 */
bool game_is_opponent_piece(piece_t piece, player_t player);


// ============================================================================
// MOVE VALIDATION FUNCTIONS
// ============================================================================


/**
 * @brief Check if move is valid
 * @param move Move to validate
 * @return true if move is valid
 */
bool game_is_valid_move_bool(const chess_move_t* move);

/**
 * @brief Validate move based on piece type
 * @param move Move to validate
 * @param piece Piece being moved
 * @return true if move is valid for piece type
 */
bool game_validate_piece_move(const chess_move_t* move, piece_t piece);

/**
 * @brief Validate pawn move
 * @param move Move to validate
 * @param piece Pawn piece
 * @return true if pawn move is valid
 */
bool game_validate_pawn_move(const chess_move_t* move, piece_t piece);

/**
 * @brief Validate knight move
 * @param move Move to validate
 * @return true if knight move is valid
 */
bool game_validate_knight_move(const chess_move_t* move);

/**
 * @brief Validate bishop move
 * @param move Move to validate
 * @return true if bishop move is valid
 */
bool game_validate_bishop_move(const chess_move_t* move);

/**
 * @brief Validate rook move
 * @param move Move to validate
 * @return true if rook move is valid
 */
bool game_validate_rook_move(const chess_move_t* move);

/**
 * @brief Validate queen move
 * @param move Move to validate
 * @return true if queen move is valid
 */
bool game_validate_queen_move(const chess_move_t* move);

/**
 * @brief Validate king move
 * @param move Move to validate
 * @return true if king move is valid
 */
bool game_validate_king_move(const chess_move_t* move);

// Enhanced move validation functions
move_error_t game_is_valid_move(const chess_move_t* move);
move_error_t game_validate_piece_move_enhanced(const chess_move_t* move, piece_t piece);
move_error_t game_validate_pawn_move_enhanced(const chess_move_t* move, piece_t piece);
move_error_t game_validate_knight_move_enhanced(const chess_move_t* move);
move_error_t game_validate_bishop_move_enhanced(const chess_move_t* move);
move_error_t game_validate_rook_move_enhanced(const chess_move_t* move);
move_error_t game_validate_queen_move_enhanced(const chess_move_t* move);
move_error_t game_validate_king_move_enhanced(const chess_move_t* move);

// Helper functions for enhanced validation
bool game_would_move_leave_king_in_check(const chess_move_t* move);
bool game_is_en_passant_possible(const chess_move_t* move);
move_error_t game_validate_castling(const chess_move_t* move);
bool game_is_insufficient_material(void);

// Move error display and suggestions
void game_display_move_error(move_error_t error, const chess_move_t* move);
void game_show_move_suggestions(uint8_t row, uint8_t col);
uint32_t game_get_available_moves(uint8_t row, uint8_t col, move_suggestion_t* suggestions, uint32_t max_suggestions);

// Enhanced error handling functions
void game_handle_invalid_move(move_error_t error, const chess_move_t* move);
bool game_is_error_recovery_active(void);
bool game_handle_piece_return(uint8_t row, uint8_t col);
bool game_is_error_recovery_timeout(void);
void game_clear_error_recovery(void);

// Strict castling functions (no automatic moves)
bool game_start_castling_transaction_strict(bool is_kingside, uint8_t king_from_row, uint8_t king_from_col, uint8_t king_to_row, uint8_t king_to_col);
bool game_complete_castling_strict(void);
uint32_t game_get_error_count(void);

// Castling transaction functions
bool game_handle_castling_king_move(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);
bool game_handle_castling_rook_move(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);
void game_cancel_castling_transaction(void);
bool game_is_castling_in_progress(void);
bool game_is_castling_timeout(void);


// ============================================================================
// MOVE EXECUTION FUNCTIONS
// ============================================================================


/**
 * @brief Execute a chess move
 * @param move Move to execute
 * @return true if move was executed successfully
 */
bool game_execute_move(const chess_move_t* move);


// ============================================================================
// GAME STATUS FUNCTIONS
// ============================================================================


/**
 * @brief Get current game state
 * @return Current game state
 */
game_state_t game_get_state(void);

/**
 * @brief Get current player
 * @return Current player
 */
player_t game_get_current_player(void);

/**
 * @brief Get move count
 * @return Number of moves made
 */
uint32_t game_get_move_count(void);

/**
 * @brief Print current board position
 */
void game_print_board(void);

/**
 * @brief Print move history
 */
void game_print_move_history(void);

/**
 * @brief Get piece name as string
 * @param piece Piece to get name for
 * @return Piece name string
 */
const char* game_get_piece_name(piece_t piece);


// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================


/**
 * @brief Process game commands from queue
 */
void game_process_commands(void);

/**
 * @brief Process move command from UART
 * @param move_cmd Move command structure with coordinates
 */
void game_process_move_command(const void* move_cmd_ptr);
void game_handle_invalid_move(move_error_t error, const chess_move_t* move);

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
                             piece_t piece, piece_t captured);

/**
 * @brief Show player change animation
 * @param previous_player Previous player
 * @param current_player Current player
 */
void game_show_player_change_animation(player_t previous_player, player_t current_player);

// Animation test functions
void game_test_move_animation(void);
void game_test_player_change_animation(void);
void game_test_castle_animation(void);
void game_test_promote_animation(void);
void game_test_endgame_animation(void);
void game_test_puzzle_animation(void);

// Direct LED functions (no queue)
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);
void game_show_check_direct(uint8_t king_row, uint8_t king_col);
void game_show_player_change_direct(player_t current_player);
void game_clear_highlights_direct(void);

// Jemné animační funkce (subtle animations)
void game_show_piece_lift_direct(uint8_t row, uint8_t col);
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

// Error handling LED functions
void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);
void game_show_button_error(uint8_t button_id);
void game_show_castling_guidance(uint8_t king_row, uint8_t king_col, uint8_t rook_row, uint8_t rook_col, bool is_kingside);

/**
 * @brief Check for check/checkmate conditions
 */
void game_check_game_conditions(void);

/**
 * @brief Convert board coordinates to chess square notation
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param square Output square string (e.g., "e2")
 */
void game_coords_to_square(uint8_t row, uint8_t col, char* square);

/**
 * @brief Print game status
 */
void game_print_status(void);

// New game statistics and end-game detection functions
void game_print_game_stats(void);
uint32_t game_calculate_position_hash(void);
bool game_is_position_repeated(void);
void game_add_position_to_history(void);
int game_calculate_material_balance(int* white_material, int* black_material);
void game_get_material_string(char* buffer, size_t buffer_size);
bool game_is_king_in_check(player_t player);
bool game_has_legal_moves(player_t player);
game_state_t game_check_end_game_conditions(void);

// Game control functions
void game_toggle_timer(bool enabled);
void game_save_game(const char* game_name);
void game_load_game(const char* game_name);
void game_export_pgn(char* pgn_buffer, size_t buffer_size);


// ============================================================================
// MATRIX EVENT PROCESSING
// ============================================================================


/**
 * @brief Process matrix events (moves)
 */
void game_process_matrix_events(void);

/**
 * @brief Highlight all movable pieces for current player
 */
void game_highlight_movable_pieces(void);


#endif // GAME_TASK_H