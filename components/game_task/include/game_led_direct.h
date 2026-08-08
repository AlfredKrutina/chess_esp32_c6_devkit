/**
 * @file game_led_direct.h
 * @brief PRIME LED FUNCTION FOR GAME TASK - Back Queue Hell
 * 
 * RESOLUTION: Use prime LED call instead of xQueueSend(led_command_queue).
 * - Back queue hell
 * - No timing problems
 * - Instant LED updates
 * 
 * @author Alfred Krutina
 * @version 1.8.0
 * @date 2025-09-02
 * 
 * @details
 * This module provides prime LED functions for game tasks without using queues.
 * Eliminates queue hell and timing problems by calling the LED function directly.
 * All functions are thread-safe thanks to the use of a mutex in led_task.
 */

#ifndef GAME_LED_DIRECT_H
#define GAME_LED_DIRECT_H

#include <stdint.h>
#include "chess_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PRIME LED FUNCTION - REAR QUEUE HELL
// ============================================================================

/**
 * @brief Display a move with direct LED calls
 * 
 * Displays a move animation with the source and target fields gradually lighting up.
 * Uses colors: yellow (source), green (target), red (capture).
 * 
 * @param from_row Source rows (0-7)
 * @param from_col Source column (0-7)
 * @param to_row Target rows (0-7)
 * @param to_col Target column (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Display raised figures with prime LED calls
 * 
 * Displays a gentle pulsating yellow-orange animation at the raised figure position.
 * 
 * @param row Row of figurines (0-7)
 * @param col Column of the figure (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_piece_lift_direct(uint8_t row, uint8_t col);

/**
 * @brief Display valid moves with direct LED calls
 * 
 * Displays a soft green wave at all positions where the figure can pull out.
 * 
 * @param valid_positions LED index array of valid positions
 * @param count Number of valid positions
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

/**
 * @brief Display error with LED prime calls
 * 
 * Displays a red LED at the position where the error occurred.
 * 
 * @param row Error row (0-7)
 * @param col Error column (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_error_direct(uint8_t row, uint8_t col);

/**
 * @brief Clear highlighting with direct LED calls
 * 
 * Clears all LED highlights on the box (save button).
 * 
 * @note PRIME CALLS - no queue
 */
void game_clear_highlights_direct(void);

/**
 * @brief Show game state with prime LED calls
 * 
 * Displays the current position of all pieces on the chest with the help of LEDs.
 * White figures = white color, black figures = blue color.
 * 
 * @param board Pointer to the inbox state array (64 elements)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_state_direct(piece_t *board);

/**
 * @brief Display sach with prime LED calls
 * 
 * Displays a red flashing LED on the king position in chess.
 * 
 * @param king_row King row (0-7)
 * @param king_col King column (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_check_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Show checkmate with prime LED calls
 * 
 * Displays an intense red flashing animation at the checkmate king position.
 * 
 * @param king_row King row (0-7)
 * @param king_col King column (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Show graduation with prime LED calls
 * 
 * Displays a gold-white flashing animation at the position of the graduated sand.
 * 
 * @param row Graduation row (0-7)
 * @param col Graduation column (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_promotion_direct(uint8_t row, uint8_t col);

/**
 * @brief Display a row with prime LED calls
 * 
 * Show rosady animation - king and rook movement.
 * 
 * @param king_from_row King source rows (0-7)
 * @param king_from_col Source king column (0-7)
 * @param king_to_row Target rows of the king (0-7)
 * @param king_to_col Target column of the king (0-7)
 * @param rook_from_row Source rows rook (0-7)
 * @param rook_from_col Source column of rook (0-7)
 * @param rook_to_row Target rook rows (0-7)
 * @param rook_to_col Destination column of rook (0-7)
 * 
 * @note PRIME CALLS - no queue
 */
void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col, 
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col);

#ifdef __cplusplus
}
#endif

#endif // GAME_LED_DIRECT_H
