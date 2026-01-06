/**
 * @file demo_mode_helpers.c
 * @brief Demo Mode Helper Functions for ESP32-C6 Chess System
 *
 * This file contains helper functions for demo mode operation including:
 * - Castling detection and execution
 * - Promotion detection
 * - Timing protection
 * - Highlight clearing
 *
 * These functions will be integrated into game_task.c
 */

// ============================================================================
// DEMO MODE HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Detect if a move is castling (king moves 2 squares)
 *
 * @param from_row Source row (0-7)
 * @param from_col Source column (0-7)
 * @param to_row Destination row (0-7)
 * @param to_col Destination column (0-7)
 * @param board Pointer to board array
 * @return true if move is castling, false otherwise
 */
static bool demo_is_castling_move(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col,
                                  piece_t board[8][8]) {
  // Must be on same rank
  if (from_row != to_row) {
    return false;
  }

  // Check if piece is a king
  piece_t piece = board[from_row][from_col];
  if (piece != PIECE_WHITE_KING && piece != PIECE_BLACK_KING) {
    return false;
  }

  // Check if king moves 2 squares (castling signature)
  int col_diff = abs((int)to_col - (int)from_col);
  return (col_diff == 2);
}

/**
 * @brief Calculate rook positions for castling move
 *
 * @param king_from_row King source row
 * @param king_from_col King source column
 * @param king_to_col King destination column
 * @param[out] rook_from_row Rook source row
 * @param[out] rook_from_col Rook source column
 * @param[out] rook_to_row Rook destination row
 * @param[out] rook_to_col Rook destination column
 */
static void
demo_get_castling_rook_squares(uint8_t king_from_row, uint8_t king_from_col,
                               uint8_t king_to_col, uint8_t *rook_from_row,
                               uint8_t *rook_from_col, uint8_t *rook_to_row,
                               uint8_t *rook_to_col) {
  *rook_from_row = king_from_row;
  *rook_to_row = king_from_row;

  // Kingside castling (king moves to g-file, col 6)
  if (king_to_col == 6) {
    *rook_from_col = 7; // h-file
    *rook_to_col = 5;   // f-file
  }
  // Queenside castling (king moves to c-file, col 2)
  else if (king_to_col == 2) {
    *rook_from_col = 0; // a-file
    *rook_to_col = 3;   // d-file
  } else {
    // Should not happen if is_castling_move returned true
    ESP_LOGE("DEMO", "Invalid castling target column: %d", king_to_col);
    *rook_from_col = 0;
    *rook_to_col = 0;
  }
}

/**
 * @brief Detect if a move is pawn promotion
 *
 * @param from_row Source row (0-7)
 * @param from_col Source column (0-7)
 * @param to_row Destination row (0-7)
 * @param board Pointer to board array
 * @return true if move is promotion, false otherwise
 */
static bool demo_is_promotion_move(uint8_t from_row, uint8_t from_col,
                                   uint8_t to_row, piece_t board[8][8]) {
  piece_t piece = board[from_row][from_col];

  // Check if piece is a pawn
  if (piece != PIECE_WHITE_PAWN && piece != PIECE_BLACK_PAWN) {
    return false;
  }

  // White pawn reaching rank 8 (row 7)
  if (piece == PIECE_WHITE_PAWN && to_row == 7) {
    return true;
  }

  // Black pawn reaching rank 1 (row 0)
  if (piece == PIECE_BLACK_PAWN && to_row == 0) {
    return true;
  }

  return false;
}

/**
 * @brief Ensure safe timing before executing demo move
 *
 * Waits if necessary to maintain minimum interval between moves.
 * Also clears highlights to prevent visual glitches.
 *
 * @param last_move_time_ms Timestamp of last move
 * @param interval_ms Required interval between moves
 */
static void demo_ensure_safe_timing(uint32_t *last_move_time_ms,
                                    uint32_t interval_ms) {
  uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
  uint32_t elapsed = now_ms - *last_move_time_ms;

  // Wait if interval not yet elapsed
  if (elapsed < interval_ms) {
    uint32_t wait_ms = interval_ms - elapsed;
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
  }

  // Give web UI time to process highlight clearing (100ms)
  vTaskDelay(pdMS_TO_TICKS(100));
}
