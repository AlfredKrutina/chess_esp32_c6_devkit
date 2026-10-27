/* Archived from components/game_task/game_matrix_workflow.c.
 * No callers. Piece events go through GAME_CMD_PICKUP / GAME_CMD_DROP
 * in game_dispatch.c. Not part of the firmware build.
 *
 * Moved: game_highlight_opponent_pieces (decl), game_detect_new_game_setup,
 * game_handle_piece_lifted, game_handle_piece_placed,
 * game_handle_squares_move, game_highlight_opponent_pieces.
 */

void game_highlight_opponent_pieces(void);
// ============================================================================
// MATRIX LED WORKFLOW IMPLEMENTATION
// ============================================================================

/**
 * @brief Detect if pieces are arranged in starting positions (rows 1, 2, 7,
 * 8)
 * @return true if pieces are in starting positions, false otherwise
 */
bool game_detect_new_game_setup(void) {
  if (board_setup_tutorial_active) {
    return false;
  }

  // Check if rows 1, 2, 7, 8 are occupied by pieces
  // and lines 3, 4, 5, 6 are empty

  // Check rows 0, 1 (white pieces) and 6, 7 (black pieces)
  bool rows_0_1_6_7_occupied = true;
  bool rows_2_3_4_5_empty = true;

  // Check lines 0, 1, 6, 7 - they must be occupied
  for (int row = 0; row < 8; row++) {
    bool row_has_pieces = false;
    for (int col = 0; col < 8; col++) {
      if (board[row][col] != PIECE_EMPTY) {
        row_has_pieces = true;
        break;
      }
    }

    if ((row == 0 || row == 1 || row == 6 || row == 7)) {
      // These lines must be occupied
      if (!row_has_pieces) {
        rows_0_1_6_7_occupied = false;
        break;
      }
    } else if (row >= 2 && row <= 5) {
      // These lines must be empty
      if (row_has_pieces) {
        rows_2_3_4_5_empty = false;
        break;
      }
    }
  }

  bool is_starting_setup = rows_0_1_6_7_occupied && rows_2_3_4_5_empty;

  if (is_starting_setup) {
    ESP_LOGI(TAG, "🎮 Starting position detected: rows 0,1,6,7 occupied, rows "
                  "2,3,4,5 empty");
  }

  return is_starting_setup;
}

/**
 * @brief [DEPRECATED] Handle piece lifted event from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_PICKUP to
 * game_command_queue and processed by game_process_pickup_command(). This
 * function is kept for compatibility.
 *
 * Do not wire this back into the matrix path: it rejects opponent lifts and
 * would break guided capture (opponent-first) and related LED flows.
 *
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_lifted(uint8_t row, uint8_t col) {
  ESP_LOGI(TAG, "🖐️ Matrix: Piece lifted from %c%d", 'a' + col, row + 1);

  // Check if there's a piece at the square
  piece_t piece = board[row][col];
  if (piece == PIECE_EMPTY) {
    ESP_LOGW(TAG, "❌ No piece to lift at %c%d", 'a' + col, row + 1);
    return;
  }

  // Check if it's the current player's piece
  bool is_white_piece =
      (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
  bool is_current_player = (current_player == PLAYER_WHITE && is_white_piece) ||
                           (current_player == PLAYER_BLACK && !is_white_piece);

  if (!is_current_player) {
    ESP_LOGW(TAG, "❌ Cannot lift opponent's piece at %c%d", 'a' + col,
             row + 1);
    return;
  }

  // Check if castle animation is active and this is a rook
  if (game_is_castle_animation_active()) {
    bool is_rook = (piece == PIECE_WHITE_ROOK || piece == PIECE_BLACK_ROOK);

    if (is_rook) {
      // Check if rook is being lifted from the correct position for castling
      if (row == rook_from_row && col == rook_from_col) {
        ESP_LOGI(TAG, "✅ Rook lifted from correct position for castling");
        // Continue with normal move highlighting
      } else {
        // Rook lifted from wrong position - show error and valid moves from
        // correct position
        ESP_LOGW(TAG,
                 "❌ Rook lifted from wrong position for castling: %c%d "
                 "(expected: %c%d)",
                 'a' + col, row + 1, 'a' + rook_from_col, rook_from_row + 1);

        // Show error animation - blink red
        led_clear_board_only();
        for (int i = 0; i < 3; i++) {
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0,
                             0); // Red
          vTaskDelay(pdMS_TO_TICKS(200));
          led_clear_board_only();
          vTaskDelay(pdMS_TO_TICKS(200));
        }

        // Show valid moves from the correct rook position
        led_set_pixel_safe(chess_pos_to_led_index(rook_from_row, rook_from_col),
                           255, 255,
                           0); // Yellow
        led_set_pixel_safe(chess_pos_to_led_index(rook_to_row, rook_to_col), 0,
                           255, 0); // Green

        return;
      }
    } else {
      ESP_LOGI(TAG, "🏰 Castle animation active - only rook can be moved");
      return;
    }
  }

  // Show possible moves for this piece
  move_suggestion_t suggestions[64];
  uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);

  if (valid_moves > 0) {
    ESP_LOGI(TAG, "💡 Found %lu valid moves for piece at %c%d", valid_moves,
             'a' + col, row + 1);

    // Send LED commands to highlight possible moves
    // Highlight source square in yellow
    // Highlight source square (yellow)
    led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                       0); // Yellow for source

    // Highlight possible destinations
    if (game_led_guidance_show_destinations()) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if destination has opponent's piece
        piece_t dest_piece = board[dest_row][dest_col];
        bool is_opponent_piece =
            (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
             dest_piece <= PIECE_BLACK_KING) ||
            (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
             dest_piece <= PIECE_WHITE_KING);

        if (is_opponent_piece) {
          // Orange for opponent's pieces (capture)
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          // Green for empty squares
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }
    }
  } else {
    ESP_LOGI(TAG, "💡 No valid moves for piece at %c%d", 'a' + col, row + 1);
  }

  // After picking up the figure, start the player change animation and display
  // moving figures (only if the dew animation is not active and is not
  // dew expected)
  if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
    // After piece is lifted, show pieces that the opponent can move
    // This completes the cycle as requested by the user
    game_highlight_movable_pieces();
  }
}

/**
 * @brief [DEPRECATED] Handle piece placed event from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_DROP to
 * game_command_queue and processed by game_process_drop_command(). This
 * function is kept for compatibility.
 * @param row Row coordinate
 * @param col Column coordinate
 */
void game_handle_piece_placed(uint8_t row, uint8_t col) {
  ESP_LOGI(TAG, "✋ Matrix: Piece placed at %c%d", 'a' + col, row + 1);

  // New game detection after endgame animation
  // Check if the pieces have been placed in their starting positions (rows 1,
  // 2, 7, 8)
  if (game_detect_new_game_setup()) {
    ESP_LOGI(TAG,
             "🎮 NEW GAME DETECTED! Pieces arranged in starting positions");

    // Stop all animations
    unified_animation_stop_all();

    // Start a new game
    game_start_new_game();

    return; // Quit - a new game has been started
  }

  // Check if castle animation is active
  if (game_is_castle_animation_active()) {
    ESP_LOGI(
        TAG,
        "🏰 Castle animation active - checking if rook was placed correctly");

    // Check if this is a rook piece being placed
    piece_t piece = board[row][col];
    bool is_rook = (piece == PIECE_WHITE_ROOK || piece == PIECE_BLACK_ROOK);

    if (is_rook) {
      // Check if rook was placed on correct position for castling
      if (row == rook_to_row && col == rook_to_col) {
        ESP_LOGI(TAG, "✅ Rook placed on correct position for castling");
        // The move will be completed in game_handle_squares_move
        return;
      } else {
        // Rook placed on wrong position - show error
        ESP_LOGW(TAG,
                 "❌ Rook placed on wrong position for castling: %c%d "
                 "(expected: %c%d)",
                 'a' + col, row + 1, 'a' + rook_to_col, rook_to_row + 1);

        // Show error animation - blink red then show valid moves
        led_clear_board_only();

        // Blink the wrong position red
        for (int i = 0; i < 3; i++) {
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 0,
                             0); // Red
          vTaskDelay(pdMS_TO_TICKS(200));
          led_clear_board_only();
          vTaskDelay(pdMS_TO_TICKS(200));
        }

        // Show correct rook position and destination
        led_set_pixel_safe(chess_pos_to_led_index(rook_from_row, rook_from_col),
                           255, 255,
                           0); // Yellow
        led_set_pixel_safe(chess_pos_to_led_index(rook_to_row, rook_to_col), 0,
                           255, 0); // Green

        return;
      }
    } else {
      ESP_LOGI(TAG,
               "🏰 Castle animation active - waiting for rook to be moved");
      return;
    }
  }

  // Clear only board LEDs
  led_clear_board_only();

  // After placing the figure, start the player change animation and display
  // moving figures (only if the dew animation is not active and is not
  // dew expected)
  if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
    // After piece is placed, show pieces that the opponent can move
    // This completes the cycle as requested by the user
    game_highlight_opponent_pieces();
  }
}

/**
 * @brief [DEPRECATED] Handle complete move from matrix
 * @deprecated Matrix events are now sent as GAME_CMD_DROP to
 * game_command_queue and processed by game_process_drop_command(). This
 * function is kept for compatibility.
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 */
void game_handle_squares_move(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                             uint8_t to_col) {
  ESP_LOGI(TAG, "🎯 Matrix: Complete move %c%d -> %c%d", 'a' + from_col,
           from_row + 1, 'a' + to_col, to_row + 1);

  // Check if castle animation is active
  if (game_is_castle_animation_active()) {
    if (game_complete_castle_animation(from_row, from_col, to_row, to_col)) {
      ESP_LOGI(TAG, "✅ Castle animation completed via matrix move");
      return;
    } else {
      ESP_LOGW(TAG,
               "❌ Invalid rook move for castling - waiting for correct move");
      return;
    }
  }

  // Convert matrix event to chess move
  chess_move_t move = {
      .from_row = from_row,
      .from_col = from_col,
      .to_row = to_row,
      .to_col = to_col,

      .piece = board[from_row][from_col], // An actual figure from the source field
      .captured_piece =
          board[to_row][to_col], // An actual figure from the target field
      .timestamp = esp_timer_get_time() / 1000};

  // CRITICAL FIX: Move validation before calling game_execute_move (ditto
  // jako UART flow)
  move_error_t error = game_is_valid_move(&move);
  if (error != MOVE_ERROR_NONE) {
    ESP_LOGW(TAG, "❌ Invalid matrix move rejected: %c%d -> %c%d (error: %d)",
             'a' + from_col, from_row + 1, 'a' + to_col, to_row + 1, error);

    // Clear LEDs on invalid move
    led_clear_board_only();

    // Show error animation
    led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 0,
                       0); // Red for source
    led_set_pixel_safe(chess_pos_to_led_index(to_row, to_col), 255, 0,
                       0); // Red for destination

    return; // Reject invalid move
  }

  // Dew detection before calling game_execute_move
  bool is_castling =
      (move.piece == PIECE_WHITE_KING || move.piece == PIECE_BLACK_KING) &&
      abs((int)move.to_col - (int)move.from_col) == 2;

  if (is_castling) {
    ESP_LOGI(TAG, "🏰 CASTLING DETECTED in matrix move: %c%d -> %c%d",
             'a' + move.from_col, move.from_row + 1, 'a' + move.to_col,
             move.to_row + 1);
  }

  // Execute move (already validated)
  if (game_execute_move(&move)) {
    ESP_LOGI(TAG, "✅ Matrix move executed successfully");

    // CRITICAL FIX: Checkmate/stalemate check after every turn!
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;
      ESP_LOGI(TAG, "🎉 Game finished detected in matrix move!");
    }

    if (is_castling) {
      // Don't run game_highlight_opponent_pieces for rosadu
      ESP_LOGI(TAG, "🏰 Castling move completed - waiting for rook animation");
    } else {
      // After a successful turn, start the player change animation and display
      // moving figures (only if the dew animation is not active and is not
      // dew expected)
      if (!game_is_castle_animation_active() && !game_is_castling_expected()) {
        // After successful move, show pieces that the opponent can move
        game_highlight_movable_pieces();
      }
    }
  } else {
    ESP_LOGW(TAG, "❌ Invalid matrix move rejected");

    // Clear LEDs on invalid move
    led_clear_board_only();
  }
}

/**
 * @brief Highlight pieces that the opponent can move
 */
void game_highlight_opponent_pieces(void) {
  ESP_LOGI(TAG, "🔄 Highlighting opponent pieces that can move");

  // LED command queue removed - using direct LED calls now

  // Switch to opponent's perspective temporarily
  player_t opponent =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  // Find all pieces that the opponent can move
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece == PIECE_EMPTY)
        continue;

      // Check if it's opponent's piece
      bool is_opponent_piece =
          (opponent == PLAYER_WHITE && piece >= PIECE_WHITE_PAWN &&
           piece <= PIECE_WHITE_KING) ||
          (opponent == PLAYER_BLACK && piece >= PIECE_BLACK_PAWN &&
           piece <= PIECE_BLACK_KING);

      if (is_opponent_piece) {
        // Check if this piece has valid moves
        move_suggestion_t suggestions[64];
        uint32_t valid_moves =
            game_get_available_moves(row, col, suggestions, 64);

        if (valid_moves > 0) {
          // Highlight this piece in blue (indicating it can move)
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 0, 0,
                             255); // Blue for pieces that can move
        }
      }
    }
  }
}
