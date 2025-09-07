/**
 * @file enhanced_puzzle_system.h
 * @brief Enhanced Puzzle System with piece removal detection and visual guidance
 * @author Alfred Krutina
 * @version 2.5.0
 * @date 2025-09-06
 */

#ifndef ENHANCED_PUZZLE_SYSTEM_H
#define ENHANCED_PUZZLE_SYSTEM_H

#include "freertos_chess.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Puzzle states with clear progression
typedef enum {
    PUZZLE_STATE_INACTIVE = 0,
    PUZZLE_STATE_LOADING,          // Loading puzzle data
    PUZZLE_STATE_PIECE_REMOVAL,    // Waiting for piece removal
    PUZZLE_STATE_READY,            // Ready to solve
    PUZZLE_STATE_IN_PROGRESS,      // Solving in progress
    PUZZLE_STATE_HINT_SHOWN,       // Hint is displayed
    PUZZLE_STATE_COMPLETED,        // Puzzle solved
    PUZZLE_STATE_FAILED           // Puzzle failed (too many wrong moves)
} puzzle_state_t;

// Puzzle piece removal guidance
typedef struct {
    uint8_t pieces_to_remove[32];  // LED indices of pieces to remove
    uint8_t piece_count;
    uint8_t removed_count;
    bool pieces_removed[64];       // Track which pieces are removed
    uint32_t removal_start_time;
    uint32_t removal_timeout_ms;
} puzzle_removal_t;

// Enhanced puzzle structure with visual guidance
typedef struct {
    char name[32];
    char description[128];
    puzzle_difficulty_t difficulty;
    piece_t target_board[8][8];           // Final board after piece removal
    puzzle_removal_t removal_guidance;    // Piece removal guidance
    puzzle_step_t steps[16];
    uint8_t step_count;
    uint8_t current_step;
    puzzle_state_t state;
    uint32_t start_time;
    uint32_t hint_count;
    uint32_t wrong_moves;
    uint32_t max_wrong_moves;
} enhanced_puzzle_t;

// Puzzle system configuration
typedef struct {
    uint8_t max_puzzles;               // Maximum number of puzzles
    uint32_t removal_timeout_ms;       // Timeout for piece removal
    uint32_t hint_duration_ms;         // Default hint duration
    uint32_t max_wrong_moves;          // Maximum wrong moves before failure
    bool enable_visual_guidance;       // Enable LED guidance
    bool enable_sound_feedback;        // Enable sound feedback
    bool enable_progress_tracking;     // Track solving progress
} puzzle_system_config_t;

// Core puzzle API
esp_err_t puzzle_system_init(const puzzle_system_config_t* config);
esp_err_t puzzle_system_deinit(void);

// Puzzle management
esp_err_t puzzle_load(uint8_t puzzle_id);
esp_err_t puzzle_start_piece_removal(void);
esp_err_t puzzle_check_piece_removed(uint8_t row, uint8_t col);
esp_err_t puzzle_begin_solving(void);
esp_err_t puzzle_submit_move(const char* move_notation);
esp_err_t puzzle_request_hint(void);
esp_err_t puzzle_reset(void);
esp_err_t puzzle_next(void);

// Visual guidance functions
esp_err_t puzzle_show_removal_guidance(void);
esp_err_t puzzle_show_next_step_hint(void);
esp_err_t puzzle_show_solution_path(void);
esp_err_t puzzle_celebrate_completion(void);
esp_err_t puzzle_show_failure(void);

// State query functions
puzzle_state_t puzzle_get_state(void);
uint8_t puzzle_get_removal_progress(void);
bool puzzle_is_piece_removal_complete(void);
const char* puzzle_get_current_hint(void);
const char* puzzle_get_name(void);
const char* puzzle_get_description(void);
puzzle_difficulty_t puzzle_get_difficulty(void);

// Progress tracking
uint8_t puzzle_get_current_step(void);
uint8_t puzzle_get_total_steps(void);
uint32_t puzzle_get_solve_time_ms(void);
uint32_t puzzle_get_wrong_moves_count(void);
float puzzle_get_progress_percentage(void);

// Puzzle database functions
esp_err_t puzzle_get_available_puzzles(uint8_t* puzzle_ids, uint8_t* count);
esp_err_t puzzle_get_puzzles_by_difficulty(puzzle_difficulty_t difficulty, uint8_t* puzzle_ids, uint8_t* count);
const char* puzzle_get_difficulty_name(puzzle_difficulty_t difficulty);

// Matrix integration
esp_err_t puzzle_handle_matrix_event(uint8_t row, uint8_t col, bool piece_present);
esp_err_t puzzle_handle_up_command(const char* square);
esp_err_t puzzle_handle_dn_command(const char* square);
esp_err_t puzzle_handle_move_command(const char* move_notation);

// Configuration
esp_err_t puzzle_set_config(const puzzle_system_config_t* config);
esp_err_t puzzle_set_removal_timeout(uint32_t timeout_ms);
esp_err_t puzzle_set_max_wrong_moves(uint32_t max_moves);

// Status and debugging
esp_err_t puzzle_get_status(char* buffer, size_t buffer_size);
esp_err_t puzzle_get_current_puzzle_info(char* buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // ENHANCED_PUZZLE_SYSTEM_H
