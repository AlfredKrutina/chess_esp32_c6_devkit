/**
 * @file game_led_direct.h
 * @brief DIRECT LED FUNCTIONS FOR GAME TASK - No Queue Hell
 * 
 * ŘEŠENÍ: Místo xQueueSend(led_command_queue) používat direct LED calls
 * - Žádný queue hell
 * - Žádné timing problémy
 * - Okamžité LED updates
 * 
 * Author: Alfred Krutina
 * Version: 2.5 - DIRECT SYSTEM
 * Date: 2025-09-02
 */

#ifndef GAME_LED_DIRECT_H
#define GAME_LED_DIRECT_H

#include <stdint.h>
#include "chess_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// DIRECT LED FUNCTIONS - NO QUEUE HELL
// ============================================================================

/**
 * @brief Show move with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Show piece lift with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_piece_lift_direct(uint8_t row, uint8_t col);

/**
 * @brief Show valid moves with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_valid_moves_direct(uint8_t *valid_positions, uint8_t count);

/**
 * @brief Show error with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_error_direct(uint8_t row, uint8_t col);

/**
 * @brief Clear highlights with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_clear_highlights_direct(void);

/**
 * @brief Show game state with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_state_direct(piece_t *board);

/**
 * @brief Show puzzle with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_puzzle_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row, uint8_t to_col);

/**
 * @brief Show check with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_check_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Show checkmate with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col);

/**
 * @brief Show promotion with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_promotion_direct(uint8_t row, uint8_t col);

/**
 * @brief Show castling with direct LED calls
 * ✅ PŘÍMÉ VOLÁNÍ - žádný queue
 */
void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col, 
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col);

#ifdef __cplusplus
}
#endif

#endif // GAME_LED_DIRECT_H
