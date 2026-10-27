/* Archived from components/game_task/game_task.c.
 * game_process_squares_events() is an empty stub. Its only call site was commented out.
 * game_print_board_enhanced() has no callers.
 * Not part of the firmware build.
 */

/**
 * @brief [DEPRECATED] Process matrix events from queue
 * @deprecated Matrix events are now sent as chess_move_command_t to
 * game_command_queue and processed by game_process_pickup_command() /
 * game_process_drop_command(). This function is kept for compatibility but
 * does nothing.
 */
void game_process_squares_events(void) {
  // Matrix events are now processed via game_command_queue
  // This function is kept for compatibility but does nothing
}

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
    char *ptr = line;

    ptr += sprintf(ptr, " %d │", row + 1);

    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      const char *symbol;

      switch (piece) {
      case PIECE_WHITE_PAWN:
        symbol = "♙";
        break;
      case PIECE_WHITE_KNIGHT:
        symbol = "♘";
        break;
      case PIECE_WHITE_BISHOP:
        symbol = "♗";
        break;
      case PIECE_WHITE_ROOK:
        symbol = "♖";
        break;
      case PIECE_WHITE_QUEEN:
        symbol = "♕";
        break;
      case PIECE_WHITE_KING:
        symbol = "♔";
        break;
      case PIECE_BLACK_PAWN:
        symbol = "♟";
        break;
      case PIECE_BLACK_KNIGHT:
        symbol = "♞";
        break;
      case PIECE_BLACK_BISHOP:
        symbol = "♝";
        break;
      case PIECE_BLACK_ROOK:
        symbol = "♜";
        break;
      case PIECE_BLACK_QUEEN:
        symbol = "♛";
        break;
      case PIECE_BLACK_KING:
        symbol = "♚";
        break;
      default:
        symbol = "·";
        break;
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
