/**
 * @file game_task.h
 * @brief ESP32-C6 Chess System v1.8.0 - Game Task Header
 *
 * This header defines the interface for the game task:
 * - Game state types and structures
 * - Prototypes of the game task function
 * - Functions for control and game state
 *
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-08-24
 *
 * @details
 * Game task is the center of the game system. Manages the entire game state,
 * performs move validation, enforces sach rules and communicates
 * with the last tasks through the queues.
 *
 * Main functions:
 * - Standard Sacha rules (all pieces)
 * - Special moves (castling, en passant, promotion)
 * - Detection of sach, mat and heel
 * - Drag history and undo functionality
 * - Game statistics and material balance
 * - Integration with LEDs for visual feedback
 * - Integration with the time system
 * - JSON export for web interface
 */

#ifndef GAME_TASK_H
#define GAME_TASK_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <stdbool.h>
#include <stdint.h>

// Integrace timer systemu
#include "../../timer_system/include/timer_system.h"

// ============================================================================
// KONSTANTY A DEFINICE
// ============================================================================

// Spolecne typy jsou definovany v chess_types.h
#include "chess_types.h"

// Struktury chess_move_t a move_suggestion_t jsou definovany v chess_types.h

// ============================================================================
// PROTOTYPY UTILITY FUNKCI
// ============================================================================

/**
 * @brief Converts chess notation to chessboard coordinates
 *
 * @param notation Sach's notation (eg "e2")
 * @param[out] row Output row (0-7)
 * @param[out] col Output column (0-7)
 * @return true if the conversion is successful, false on error
 */
bool convert_notation_to_coords(const char *notation, uint8_t *row,
                                uint8_t *col);

/**
 * @brief Converts chessboard coordinates to chess notation
 *
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param[out] notation Output buffer for notation (min. 3 characters)
 * @return true if the conversion is successful, false on error
 */
bool convert_coords_to_notation(uint8_t row, uint8_t col, char *notation);

/**
 * @brief Process sach move from UART command
 *
 * @param cmd Display with stroke
 */
void game_process_chess_move(const chess_move_command_t *cmd);

// ============================================================================
// JSON EXPORT FUNCTION FOR WEB SERVER
// ============================================================================

/**
 * @brief Export the state of the inbox to a JSON string
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_board_json(char *buffer, size_t size);

/**
 * @brief Export game state to JSON string
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
/**
 * @brief Return the current number of turns (thread-safe)
 * @return The current number of turns since the start of the game
 */
uint32_t game_get_move_count(void);

/**
 * @brief Export game state to JSON string
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_status_json(char *buffer, size_t size);

/**
 * @brief Forced LED refresh according to the state of the game (highlight, sach, errors, graduation).
 * @details Call after returning from HA, after fade-out boot when restoring NVS (main), etc.
 */
void game_refresh_leds(void);

/**
 * @brief Export the pull history to a JSON string
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_history_json(char *buffer, size_t size);

/**
 * @brief Export the collected figures to a JSON string
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_captured_json(char *buffer, size_t size);

/**
 * @brief Export the history of material benefits to JSON (for the benefits chart)
 *
 * @param[out] buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_advantage_json(char *buffer, size_t size);

// ============================================================================
// PROTOTYPY TASK FUNKCI
// ============================================================================

/**
 * @brief Start game task
 *
 * The main functions of the game task. Initializes the inbox and runs indefinitely
 * command and event processing loops.
 *
 * @param pvParameters Task parameters (not used)
 */
void game_task_start(void *pvParameters);

// ============================================================================
// GAME INITIALIZATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the inbox to the default position
 *
 * Set all pieces to the standard starting position.
 */
void game_initialize_board(void);

/**
 * @brief Reset the game to its initial state
 *
 * Clears history, resets statistics, and reinitializes the inbox.
 */
void game_reset_game(void);

/**
 * @brief Start a new game
 *
 * Complete reset of the game including the system timer.
 */
void game_start_new_game(void);

/**
 * @brief New game from FEN (only setup + turn side). Without checking the physical default position.
 */
void game_start_new_game_from_fen(const char *fen);

/**
 * @brief Monotone version of game state (ETag / If-None-Match on snapshot).
 */
uint32_t game_get_state_revision(void);

/**
 * @brief Resets the error recovery state (returning pieces to the flashing field).
 * Call, for example, when the demo mode is turned on, so that the demo can play even after the previous one
 * wrong move by the user.
 */
void game_reset_error_recovery_state(void);

// ============================================================================
// UTILITY FUNCTIONS FOR CABINET
// ============================================================================

/**
 * @brief Verify if the position is valid on the inbox
 *
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @return true if position is valid
 */
bool game_is_valid_position(int row, int col);

/**
 * @brief Get a figure for the given position
 *
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @return The type of the figure at the position
 */
piece_t game_get_piece(int row, int col);

/**
 * @brief Check if the position is empty
 *
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @return true if position is empty
 */
bool game_is_empty(int row, int col);

/**
 * @brief Check if the figure is beaten
 *
 * @param piece The piece to verify
 * @return true if the figure was beaten
 */
bool game_is_white_piece(piece_t piece);

/**
 * @brief Check if the figure is black
 *
 * @param piece The piece to verify
 * @return true if the figure is black
 */
bool game_is_black_piece(piece_t piece);

/**
 * @brief Check if two figures are of the same color
 *
 * @param piece1 The first piece
 * @param piece2 The second piece
 * @return true if the pieces are the same color
 */
bool game_is_same_color(piece_t piece1, piece_t piece2);

// ============================================================================
// PUSH VALIDATION FUNCTION
// ============================================================================

/**
 * @brief Validate move based on figure type
 *
 * @param move Move to validate
 * @param piece The figure that moves
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_is_valid_move(const chess_move_t *move);

/**
 * @brief Expanded move validation for figure
 *
 * @param move Move to validate
 * @param piece The figurine
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_piece_move_enhanced(const chess_move_t *move,
                                               piece_t piece);

/**
 * @brief Validate sand move
 *
 * @param move Move to validate
 * @param piece The piece
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_pawn_move_enhanced(const chess_move_t *move,
                                              piece_t piece);

/**
 * @brief Validate the horse's move
 *
 * @param move Move to validate
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_knight_move_enhanced(const chess_move_t *move);

/**
 * @brief Validate shooter move
 *
 * @param move Move to validate
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_bishop_move_enhanced(const chess_move_t *move);

/**
 * @brief Validate wire pull
 *
 * @param move Move to validate
 * @return MOVE_ERROR_NONE on success, otherwise error code
 */
move_error_t game_validate_rook_move_enhanced(const chess_move_t *move);

/**
 * @brief Validate a checker move
 *
 * @param move Move to validate
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_queen_move_enhanced(const chess_move_t *move);

/**
 * @brief Validate the king's move
 *
 * @param move Move to validate
 * @return MOVE_ERROR_NONE for a valid move, otherwise error code
 */
move_error_t game_validate_king_move_enhanced(const chess_move_t *move);

// Helper functions for extended validation

/**
 * @brief Check if the move would leave the king in check
 *
 * @param move The move to verify
 * @return true if the move would leave the king in check
 */
bool game_would_move_leave_king_in_check(const chess_move_t *move);

/**
 * @brief Verify if en passant is possible
 *
 * @param move The move to verify
 * @return true if en passant is possible
 */
bool game_is_en_passant_possible(const chess_move_t *move);

/**
 * @brief Validate dew
 *
 * @param move Dew move to validate
 * @return MOVE_ERROR_NONE if the cast is valid, otherwise an error code
 */
move_error_t game_validate_castling(const chess_move_t *move);

/**
 * @brief Verify insufficient mat material
 *
 * @return true if there is insufficient material (automatic draw)
 */
bool game_is_insufficient_material(void);

// Showing move instructions

/**
 * @brief Display suggested moves for the figure
 *
 * @param row Row of figurines (0-7)
 * @param col Column of the figure (0-7)
 */
void game_show_move_suggestions(uint8_t row, uint8_t col);

/**
 * @brief Get available moves for a figure
 *
 * @param row Row of figurines (0-7)
 * @param col Column of the figure (0-7)
 * @param[out] suggestions Field for suggested moves
 * @param max_suggestions Maximum number of suggestions
 * @return The number of moves found
 */
uint32_t game_get_available_moves(uint8_t row, uint8_t col,
                                  move_suggestion_t *suggestions,
                                  uint32_t max_suggestions);

// ============================================================================
// FUNCTION TO MAKE A MOVE
// ============================================================================

/**
 * @brief Made a sach move
 *
 * Performs the move if valid. Updates inbox, history
 * and post LED feedback.
 *
 * @param move The move to perform
 * @return true if the move was made successfully
 */
bool game_execute_move(const chess_move_t *move);

// ============================================================================
// GAME STATE FUNCTIONS
// ============================================================================

/**
 * @brief Get the current state of the game
 *
 * @return The current state of the game (GAME_STATE_IDLE, GAME_STATE_PLAYING, etc.)
 */
game_state_t game_get_state(void);

/**
 * @brief Get the current player on a turn
 *
 * @return The current player (PLAYER_WHITE or PLAYER_BLACK)
 */
player_t game_get_current_player(void);

/**
 * @brief Turns guided capture LED on/off (game logic remains active)
 */
void game_set_guided_capture_hints_enabled(bool enabled);

/**
 * @brief Returns the state of the guided capture help LED
 */
bool game_get_guided_capture_hints_enabled(void);

/**
 * @brief Matrix: enable second UP in a row (victim in hand + attacker, or 3-step take).
 * @details Without it, matrix_detect_moves() evaluates the status as ambiguous and turns on the guard.
 */
bool game_matrix_allow_second_sequential_lift(void);

/**
 * @brief Level of LED help while playing (1 = minimum … 5 = full).
 * @details Controls move highlighting, LED chess, guided capture, etc.
 */
void game_set_led_guidance_level(uint8_t level);
uint8_t game_get_led_guidance_level(void);

/** Web tutorial assembly from a blank board (LED + instructions). */
void game_enter_board_setup_tutorial(void);
void game_exit_board_setup_tutorial(bool apply_full_start_position);
bool game_is_board_setup_tutorial_active(void);
/** Physical occupancy of rows 0–1 and 6–7 full, 2–5 empty (matrix). */
bool game_is_physical_board_starting_occupancy(void);
/** Ends the tutorial and starts a new game only if the physical position fits. */
bool game_finish_board_setup_tutorial_from_web(void);
bool game_puzzle_start(uint8_t puzzle_id);
void game_puzzle_cancel(void);
bool game_is_puzzle_active(void);
bool game_puzzle_enter_setup(uint8_t puzzle_id);
bool game_is_puzzle_setup_active(void);

/** Compare reed matrix occupancy (0/1) to FEN piece placement. */
bool game_physical_matrix_matches_fen(const char *fen);

/** Opening trainer — multi-ply line with virtual opponent replies. */
void game_opening_cancel(void);
bool game_is_opening_trainer_active(void);
bool game_is_opening_trainer_setup_active(void);
bool game_opening_physical_matches_start(void);
bool game_opening_enter_setup_phase(void);
bool game_opening_load_config(const char *line_id, const char *start_fen,
                              const char line_uci[][6], uint8_t line_uci_count,
                              const uint8_t *player_ply_indices,
                              uint8_t player_ply_count,
                              const uint8_t *checkpoint_ply_indices,
                              uint8_t checkpoint_count, uint8_t mode,
                              uint8_t player_side_white,
                              uint8_t opponent_mode);
bool game_opening_start(void);
bool game_opening_hint(void);
bool game_opening_checkpoint_ack(void);
bool game_opening_awaiting_opponent_physical(void);
bool game_opening_is_physical_opponent_mode(void);
bool game_opening_validate_opponent_pickup(uint8_t from_row, uint8_t from_col);
void game_opening_on_opponent_piece_lifted(void);
void game_opening_advance_after_opponent_physical(void);
bool game_opening_validate_expected_move(uint8_t from_row, uint8_t from_col,
                                         uint8_t to_row, uint8_t to_col);
void game_opening_advance_after_correct(void);
bool game_opening_on_wrong_player_move(void);
void game_opening_record_wrong_uci(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, uint8_t to_col);
bool game_opening_on_illegal_player_move(void);
bool game_opening_apply_uci(const char *uci);
bool game_opening_validate_checkpoint_physical(void);
bool game_opening_status_needs_matrix(void);
const char *game_opening_feedback_key(void);
const char *game_opening_opponent_mode_key(void);
void game_opening_export_status_json(char *buf, size_t buf_size, size_t *offset);

/**
 * @brief Matrix guard: active pause when the matrix does not match the logic board.
 * @details true = user must flatten the physical board before further moves.
 */
bool game_is_matrix_guard_active(void);

/**
 * @brief Count of fields with conflict (array vs. expected state after refresh).
 */
uint8_t game_get_matrix_guard_conflict_count(void);

/** @brief Bitova maska zvednutych poli (spodnich 32 poli), matrix guard. */
uint32_t game_get_matrix_guard_lifted_mask_low(void);
/** @brief Bitova maska zvednutych poli (hornich 32 poli), matrix guard. */
uint32_t game_get_matrix_guard_lifted_mask_high(void);
/** @brief Bitova maska polozenych poli (spodnich 32), matrix guard. */
uint32_t game_get_matrix_guard_dropped_mask_low(void);
/** @brief Bitova maska polozenych poli (hornich 32), matrix guard. */
uint32_t game_get_matrix_guard_dropped_mask_high(void);

/**
 * @brief Emergency cancel matrix guard (game + matrix layer) and restore LED hint.
 * @details Use only when the board is physically level, but the guard is left hanging.
 * Prefer automatic cleaning after comparing figures.
 */
void game_force_clear_matrix_guard(void);

/**
 * @brief true if game_load_snapshot_from_nvs() in game_task_start successfully loaded the game.
 * @see game_was_boot_new_game_triggered()
 */
bool game_was_snapshot_loaded_on_boot(void);

/**
 * @brief true if a minimal snapshot (min) was used instead of a full one (full).
 */
bool game_is_snapshot_fallback_used(void);

/** @brief Snapshot restore failure (CRC / format). */
bool game_has_snapshot_restore_failure(void);

/** @brief Last snapshot save to NVS failed. */
bool game_has_snapshot_save_failure(void);

/**
 * @brief After restoring from NVS: the physical board does not fit the saved position (prompt to resync).
 */
bool game_is_resync_required_after_restore(void);

/**
 * @brief Boot tracker vynutil novou hru (napr. prilis casty restart); main neposila NEW_GAME.
 */
bool game_was_boot_new_game_triggered(void);

/**
 * @brief Get the number of moves made
 *
 * @return The number of turns since the start of the game
 */
uint32_t game_get_move_count(void);

/**
 * @brief List the current position of the inbox
 *
 * Displays the inbox in ASCII format with labels.
 */
void game_print_board(void);

/**
 * @brief Get the figure name as a string
 *
 * @param piece The figurine
 * @return Name of the figure (e.g. "Bily pesec", "Black lady")
 */
const char *game_get_piece_name(piece_t piece);

// ============================================================================
// COMMAND PROCESSING FUNCTIONS
// ============================================================================

/**
 * @brief Process game commands from the queue
 *
 * Reads commands from game_command_queue and executes them.
 */
void game_process_commands(void);

/**
 * @brief Zpracuj neplatny tah
 *
 * Displays an error message and LED feedback for an invalid move.
 *
 * @param error Typ chyby
 * @param move Neplatny tah
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t *move);

/**
 * @brief Display the game change animation
 *
 * @param previous_player Previous player
 * @param current_player The current player
 */
void game_show_player_change_animation(player_t previous_player,
                                       player_t current_player);

// ============================================================================
// TEST FUNCTIONS FOR ANIMATIONS
// ============================================================================

/** @brief Test the move animation */
void game_test_move_animation(void);
/** @brief Test the player change animation */
void game_test_player_change_animation(void);
/** @brief Test dewdrop animation */
void game_test_castle_animation(void);
/** @brief Test graduation animation */
void game_test_promote_animation(void);
/** @brief Testuj animaci konce hry */
void game_test_endgame_animation(void);

// ============================================================================
// PRIME LED FEATURE (no queues) - USED IN game_led_direct.c
// ============================================================================

/** @brief Show move directly via LED */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                           uint8_t to_col);
/** @brief Show check directly via LED */
void game_show_check_direct(uint8_t king_row, uint8_t king_col);
/** @brief Display the game change directly via LED */
void game_show_player_change_direct(player_t current_player);
/** @brief Clear highlight directly via LED */
void game_clear_highlights_direct(void);

// ============================================================================
// SMOOTH ANIMATION FEATURE - USED IN game_led_direct.c
// ============================================================================

/** @brief Display the raised figure directly through the LED */
void game_show_piece_lift_direct(uint8_t row, uint8_t col);
/** @brief Display valid moves directly via LED */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

// ============================================================================
// ERROR HANDLING LED FUNCTION - USED IN game_led_direct.c
// ============================================================================

/** @brief Display an invalid move error */
void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col);
/** @brief Show button error */
void game_show_button_error(uint8_t button_id);
/** @brief Show instructions for dew */
void game_show_castling_guidance(uint8_t king_row, uint8_t king_col,
                                 uint8_t rook_row, uint8_t rook_col,
                                 bool is_kingside);

/**
 * @brief Verify checkmate/checkmate conditions
 *
 * Checks whether the king is in check, checkmate or tail.
 */
void game_check_game_conditions(void);

/**
 * @brief Converts the coordinates of a box to an array string
 *
 * @param row Row (0-7)
 * @param col Column (0-7)
 * @param[out] square Output array string (eg "e2")
 */
void game_coords_to_square(uint8_t row, uint8_t col, char *square);

/**
 * @brief List the state of the game
 *
 * Displays detailed information about the state of the game (player on turn, number of turns, etc.).
 */
void game_print_status(void);

// ============================================================================
// STATISTIKY A DETEKCE KONCE HRY
// ============================================================================

/** @brief Vypis statistiky hry */
void game_print_game_stats(void);
/** @brief Ziskej pocet vyher bileho */
uint32_t game_get_white_wins(void);
/** @brief Ziskej pocet vyher cerneho */
uint32_t game_get_black_wins(void);
/** @brief Ziskej pocet remiz */
uint32_t game_get_draws(void);
/** @brief Ziskej celkovy pocet her */
uint32_t game_get_total_games(void);
/** @brief Get the game state text string */
const char *game_get_game_state_string(void);
/** @brief Compute hash positions for repetition detection */
uint32_t game_calculate_position_hash(void);
/** @brief Verify if the position has been repeated */
bool game_is_position_repeated(void);
/** @brief Add position to history for repeat detection */
void game_add_position_to_history(void);
/** @brief Vypocitej materialovou rovnovahu */
int game_calculate_material_balance(int *white_material, int *black_material);
/** @brief Ziskej textovy retezec material balance */
void game_get_material_string(char *buffer, size_t buffer_size);
/** @brief Check if the king is in chess */
bool game_is_king_in_check(player_t player);
/** @brief Check if the player has legal moves */
bool game_has_legal_moves(player_t player);
/** @brief Overi podminky konce hry */
game_state_t game_check_end_game_conditions(void);

// ============================================================================
// EVENT MATRIX PROCESSED
// ============================================================================

/**
 * @brief Process event matrix (turns)
 *
 * Reads events from matrix_event_queue and handles pull detection.
 */
void game_process_matrix_events(void);

/**
 * @brief Highlight all moving pieces of the current player
 *
 * Display a soft yellow animation on all figures that can move.
 */
void game_highlight_movable_pieces(void);

/**
 * @brief Detect if the figures are in their default position
 *
 * Checks whether the pieces are spread out in rows 1, 2, 7, 8 (starting room
 * position).
 *
 * @return true if the pieces are in their default positions
 */
bool game_detect_new_game_setup(void);

// ============================================================================
// CASTLING ANIMATION SYSTEM
// ============================================================================

/**
 * @brief Check if dewdrop animation is active
 *
 * @return true if it is waiting for the turn of the dew animation
 */
bool game_is_castle_animation_active(void);

/**
 * @brief Check if dew is expected
 *
 * @return true if the king is up and about to rosa
 */
bool game_is_castling_expected(void);

/**
 * @brief Complete the dewdrop animation when moving
 *
 * @param from_row The source rows of the vehicle (0-7)
 * @param from_col The source column of the carrier (0-7)
 * @param to_row Destination rows (0-7)
 * @param to_col The destination column of the carriage (0-7)
 * @return true if the rosa was completed successfully
 */
bool game_complete_castle_animation(uint8_t from_row, uint8_t from_col,
                                    uint8_t to_row, uint8_t to_col);

/**
 * @brief Start looping the rope animation for the dewdrop
 */
void game_start_repeating_rook_animation(void);

/**
 * @brief Zastav opakovanou animaci veze
 */
void game_stop_repeating_rook_animation(void);

/**
 * @brief Process the DROP command (DN)
 *
 * @param cmd Drop command with position
 */
void game_process_drop_command(const chess_move_command_t *cmd);

/**
 * @brief Process the PICKUP display (UP)
 */
void game_process_pickup_command(const chess_move_command_t *cmd);

/**
 * @brief Process the CASTLE statement
 */
void game_process_castle_command(const chess_move_command_t *cmd);

/**
 * @brief Process the PROMOTE command
 */
void game_process_promote_command(const chess_move_command_t *cmd);

/**
 * @brief Timer callback pro opakovanou animaci veze
 *
 * @param xTimer Handle timeru
 */
void rook_animation_timer_callback(TimerHandle_t xTimer);

// ============================================================================
// ROZSIRENY SMART ERROR HANDLING
// ============================================================================

/**
 * @brief Zvyrazni neplatnou cilovou oblast cervenou LED
 *
 * @param row Radek neplatneho cile (0-7)
 * @param col Sloupec neplatneho cile (0-7)
 */
void game_highlight_invalid_target_area(uint8_t row, uint8_t col);

/**
 * @brief Highlight valid moves for a specific figure
 *
 * @param row Row of figurines (0-7)
 * @param col Column of the figure (0-7)
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col);

// ============================================================================
// ENHANCED CASTLING SYSTEM FEATURES
// ============================================================================

/**
 * @brief Display the LED guide for the cable pull during the rosade
 */
void game_show_castling_rook_guidance();

/**
 * @brief Show an animation of the finished dewdrop
 */
void show_castling_completion_animation();

/**
 * @brief Display a flashing red LED for an invalid move error
 *
 * @param error_row Row invalid position (0-7)
 * @param error_col Invalid position column (0-7)
 */
void game_show_invalid_move_error_with_blink(uint8_t error_row,
                                             uint8_t error_col);

// ============================================================================
// INTEGRACE TIMER SYSTEMU
// ============================================================================

/**
 * @brief Export the timer state to a JSON string
 *
 * @param[out] buffer Output buffer for JSON
 * @param size Buffer size
 * @return ESP_OK on success
 */
esp_err_t game_get_timer_json(char *buffer, size_t size);

/**
 * @brief Start the timer for the current player's turn
 *
 * @param is_white_turn Is the white on turn?
 * @return ESP_OK on success
 */
esp_err_t game_start_timer_move(bool is_white_turn);

/**
 * @brief End the timer for the turn
 *
 * @return ESP_OK on success
 */
esp_err_t game_end_timer_move(void);

/**
 * @brief Pause the game timer
 *
 * @return ESP_OK on success
 */
esp_err_t game_pause_timer(void);

/**
 * @brief Reset game timer
 *
 * @return ESP_OK on success
 */
esp_err_t game_resume_timer(void);

/**
 * @brief Reset the game timer
 *
 * @return ESP_OK on success
 */
esp_err_t game_reset_timer(void);

/**
 * @brief Get the player's remaining time
 *
 * @param is_white_turn Is the white on turn?
 * @return The remaining time in milliseconds
 */
uint32_t game_get_remaining_time(bool is_white_turn);

/**
 * @brief Initialize the timer system in the game task
 *
 * @return ESP_OK on success
 */
esp_err_t game_init_timer_system(void);

/**
 * @brief Process timer commands from the queue
 *
 * @param cmd Game command with timer operation
 * @return ESP_OK on success
 */
esp_err_t game_process_timer_command(const chess_move_command_t *cmd);

/**
 * @brief Handle the timeout
 *
 * @return ESP_OK on success
 */
esp_err_t game_handle_time_expiration(void);

/**
 * @brief Update the timer display and verify the warning
 *
 * @return ESP_OK on success
 */
esp_err_t game_update_timer_display(void);

/**
 * @brief Verify if the timer is active
 *
 * @return true if the timer is active
 */
bool game_is_timer_active(void);

/**
 * @brief Find out if graduation is available for players
 *
 * @param player Player (PLAYER_WHITE or PLAYER_BLACK)
 * @return true if graduation is pending completion for the given player
 */
bool game_is_promotion_available(player_t player);

// ============================================================================
// INITIAL POSITION MONITORING SETUP
// ============================================================================

/**
 * @brief Sets the initial position watch
 *
 * @param enabled true to enable, false to disable
 */
void game_set_starting_position_check(bool enabled);

/**
 * @brief Returns the initial position guard status
 *
 * @return true if watchdog is on, false otherwise
 */
bool game_get_starting_position_check(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_TASK_H
