/**
 * @file game_error_recovery.c
 * @brief Invalid move recovery, move command processing, and LED animations.
 */

#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_move_validate.h"
#include "game_policy.h"

#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_ERROR";
/* Invalid-move recovery shared with pickup and drop. */
static game_task_error_recovery_t s_error_recovery_state = {false, 0, 0, 0, 0, PIECE_EMPTY, false, 0};

game_task_error_recovery_t *game_state_error_recovery_state(void) { return &s_error_recovery_state; }
#define error_recovery_state (*game_state_error_recovery_state())


// ============================================================================
// ENHANCED SMART ERROR HANDLING SYSTEM
// ============================================================================

/**
 * @brief Highlight invalid target area with red LEDs
 * @param row Row of invalid target
 * @param col Column of invalid target
 */
void game_highlight_invalid_target_area(uint8_t row, uint8_t col) {
  // Illuminate the field around an invalid target in red
  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      int new_row = row + dr;
      int new_col = col + dc;

      if (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
        uint8_t led_index = chess_pos_to_led_index(new_row, new_col);
        game_led_paint_led(led_index, 255, 50, 50); // Light red
      }
    }
  }
}

/**
 * @brief Highlight valid moves for a specific piece
 * @param row Row of the piece
 * @param col Column of the piece
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col) {
  piece_t piece = game_get_piece(row, col);
  if (piece == PIECE_EMPTY)
    return;

  // Find all valid moves for this piece
  for (int to_row = 0; to_row < 8; to_row++) {
    for (int to_col = 0; to_col < 8; to_col++) {
      chess_move_t test_move = {.from_row = row,
                                .from_col = col,
                                .to_row = to_row,
                                .to_col = to_col,
                                .piece = piece,
                                .captured_piece = game_get_piece(to_row, to_col)};

      if (game_is_valid_move(&test_move) == MOVE_ERROR_NONE) {
        uint8_t led_index = chess_pos_to_led_index(to_row, to_col);
        game_led_paint_led(led_index, 0, 0, 255); // Blue - valid moves
      }
    }
  }
}

/**
 * @brief Legacy function for backward compatibility
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t *move) {
  ESP_LOGI(TAG, "🚨 ENTERING game_handle_invalid_move - error: %d", error);

  if (!game_policy_error_recovery_enabled()) {
    ESP_LOGW(TAG, "Invalid move ignored (error recovery disabled)");
    return;
  }

  // CRITICAL FIX: Move pointer security check
  if (!move) {
    ESP_LOGE(TAG, "❌ Critical error: NULL move pointer in error handling");
    return;
  }

  // Checking the validity of coordinates before use
  if (move->from_row >= 8 || move->from_col >= 8 || move->to_row >= 8 ||
      move->to_col >= 8) {
    ESP_LOGE(TAG, "❌ Critical error: Invalid coordinates in move structure");
    return;
  }

  ESP_LOGI(TAG, "🚨 Invalid move detected - smart error handling");
  ESP_LOGI(TAG, "   Move: %c%d -> %c%d", 'a' + move->from_col,
           move->from_row + 1, 'a' + move->to_col, move->to_row + 1);

  // Remote / deprecated UI can send a move from an empty field. The simulation “figure is
  // physically to it" copies board[from] to board[to]; when from is empty,
  // the figure on the target is deleted (logically it "disappears"). HW recovery in this case
  // it doesn't make sense — don't change the board.
  piece_t src_piece = game_get_piece(move->from_row, move->from_col);
  if (src_piece == PIECE_EMPTY) {
    ESP_LOGW(TAG,
             "[STAGING] game_handle_invalid_move: empty source square — "
             "skipping board mutation and LED recovery (error=%d)",
             (int)error);
    return;
  }

  // Smart error handling according to requirements

  // 1. Set the red field to an invalid position
  error_recovery_state.has_invalid_piece = true;
  error_recovery_state.invalid_row = move->to_row;
  error_recovery_state.invalid_col = move->to_col;
  error_recovery_state.piece_type = game_get_piece(move->from_row, move->from_col);

  // 2. Remember the original valid position (from where the move came from)
  error_recovery_state.original_valid_row = move->from_row;
  error_recovery_state.original_valid_col = move->from_col;

  // 3. Legal board[] unchanged; recovery tracks invalid square in state.

  // 4. CLEAR VISUAL ALERT - red field + flashing for attention
  // pozornosti
  if (game_policy_error_recovery_led_red_blink() ||
      game_policy_error_recovery_led_red_persist()) {
    game_event_post_led_ctrl(GAME_EVT_CTRL_CLEAR_ALL, 0, 0, 0, 0);
    uint8_t invalid_led = chess_pos_to_led_index(move->to_row, move->to_col);

    ESP_LOGI(TAG, "🚨 STARTING ERROR ANIMATION - RED BLINKING");

    if (game_policy_error_recovery_led_red_blink()) {
      for (int blink = 0; blink < 4; blink++) {
        game_led_paint_led(invalid_led, 255, 0, 0);
        game_led_flush(); vTaskDelay(pdMS_TO_TICKS(150));
        game_led_paint_led(invalid_led, 0, 0, 0);
        game_led_flush(); vTaskDelay(pdMS_TO_TICKS(150));
      }
    }

    if (game_policy_error_recovery_led_red_persist()) {
      game_led_paint_led(invalid_led, 255, 0, 0);
    }
  }

  ESP_LOGI(TAG, "🔴 RED SQUARE ACTIVE: %c%d (piece must be lifted from here)",
           'a' + move->to_col, move->to_row + 1);
  ESP_LOGI(TAG,
           "📍 RECOVERY TARGET: Valid moves will show from %c%d when piece is "
           "lifted",
           'a' + move->from_col, move->from_row + 1);
  ESP_LOGI(TAG,
           "💡 USER ACTION REQUIRED: Lift piece from red square to continue");

  // CRITICAL FIX: NON-BLOCKING error handling - no loops!
  // The red field will remain lit until the figure is picked up
  // Recovery is handled in game_process_pickup_command()

  ESP_LOGI(TAG, "💡 Error recovery active - red square will stay lit until "
                "piece is lifted");

  game_policy_error_recovery_enter_lock();
  game_show_invalid_move_error_with_blink(move->to_row, move->to_col);
}

/**
 * @brief Process move command from game_submit
 * @param move_cmd Move command structure with coordinates
 */
void game_process_move_command(const void *move_cmd_ptr) {
  typedef struct {
    uint8_t command_type;
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
  } move_command_t;

  const move_command_t *move_cmd = (const move_command_t *)move_cmd_ptr;

  ESP_LOGI(TAG, "Processing move: [%d,%d] -> [%d,%d]", move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Validate coordinates
  if (move_cmd->from_row >= 8 || move_cmd->from_col >= 8 ||
      move_cmd->to_row >= 8 || move_cmd->to_col >= 8) {
    ESP_LOGE(TAG, "Invalid coordinates: out of board range");
    return;
  }

  // Get pieces at source and destination
  piece_t from_piece = game_get_piece(move_cmd->from_row, move_cmd->from_col);
  piece_t to_piece = game_get_piece(move_cmd->to_row, move_cmd->to_col);

  // Check if source square has a piece
  if (from_piece == PIECE_EMPTY) {
    ESP_LOGE(TAG, "Invalid move: no piece at [%d,%d]", move_cmd->from_row,
             move_cmd->from_col);
    return;
  }

  // Check if piece belongs to current player
  bool is_white_piece =
      (from_piece >= PIECE_WHITE_PAWN && from_piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (from_piece >= PIECE_BLACK_PAWN && from_piece <= PIECE_BLACK_KING);

  if ((current_player == PLAYER_WHITE && !is_white_piece) ||
      (current_player == PLAYER_BLACK && !is_black_piece)) {
    ESP_LOGE(TAG, "Invalid move: cannot move opponent's piece");
    return;
  }

  // Check if destination is occupied by own piece
  if (to_piece != PIECE_EMPTY) {
    bool dest_is_white =
        (to_piece >= PIECE_WHITE_PAWN && to_piece <= PIECE_WHITE_KING);
    bool dest_is_black =
        (to_piece >= PIECE_BLACK_PAWN && to_piece <= PIECE_BLACK_KING);

    if ((current_player == PLAYER_WHITE && dest_is_white) ||
        (current_player == PLAYER_BLACK && dest_is_black)) {
      ESP_LOGE(TAG, "Invalid move: destination occupied by own piece");
      return;
    }
  }

  // TODO: Add proper chess move validation (piece-specific rules)
  // For now, just allow any move that passes basic checks

  // Create chess move structure for validation
  chess_move_t chess_move = {.from_row = move_cmd->from_row,
                             .from_col = move_cmd->from_col,
                             .to_row = move_cmd->to_row,
                             .to_col = move_cmd->to_col,
                             .piece = from_piece,
                             .captured_piece = to_piece,
                             .timestamp = esp_timer_get_time() / 1000};

  // Validate move using enhanced validation
  move_error_t move_error = game_is_valid_move(&chess_move);
  if (move_error != MOVE_ERROR_NONE) {
    // NEW ERROR HANDLING: Don't return, handle invalid move properly
    game_handle_invalid_move(move_error, &chess_move);
    return;
  }

  // Use game_execute_move() instead of direct board
  // manipulation This ensures all game state is properly updated (castling
  // flags, en passant, etc.) and player is changed correctly
  ESP_LOGI(TAG, "Executing move: %s piece from [%d,%d] to [%d,%d]",
           (is_white_piece ? "White" : "Black"), move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Execute the move using proper game_execute_move() function
  if (!game_execute_move(&chess_move)) {
    ESP_LOGE(TAG, "Failed to execute move from [%d,%d] to [%d,%d]",
             move_cmd->from_row, move_cmd->from_col, move_cmd->to_row,
             move_cmd->to_col);
    // Note: move_cmd doesn't have response_queue, this is legacy function
    return;
  }

  // PLAYER ALREADY CHANGED In game_execute_move() - use current player
  player_t previous_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  // Run move path animation BEFORE changing player (according to old project)
  uint8_t from_led =
      chess_pos_to_led_index(move_cmd->from_row, move_cmd->from_col);
  uint8_t to_led = chess_pos_to_led_index(move_cmd->to_row, move_cmd->to_col);
  game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_MOVE_PATH, from_led, to_led);

  // STABILITY FIX: Animation runs asynchronously, we don't block processing
  // Animations are started in led_task and run independently
  // (vTaskDelay removed for better throughput)

  // CRITICAL: Endgame check BEFORE player change animations!
  // If endgame, player change will NOT start
  game_state_t end_game_result = game_check_end_game_conditions();
  if (end_game_result == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_FINISHED;
    game_active = false;

    // Find the position of the winning king for the endgame animation
    player_t winner =
        previous_player;   // The winner is the previous player (the one who made the move)
    uint8_t king_pos = 28; // default e4
    for (int i = 0; i < 64; i++) {
      piece_t piece = game_get_piece(i / 8, i % 8);
      if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
          (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
        king_pos = i;
        break;
      }
    }

    ESP_LOGI(TAG,
             "🎯 Game finished in move command! Starting endgame animation at "
             "position %d",
             king_pos);

    // Start the endgame animation (wave from king winner)
    game_led_flush();
  game_event_post_game_end(king_pos, 0);

    ESP_LOGI(TAG,
             "✅ Endgame animation started - player change animation SKIPPED");
  } else {
    bool in_check = game_is_king_in_check(current_player);
    if (in_check) {
      // Find the position of the king
      int king_row = -1, king_col = -1;
      piece_t king_piece = (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING
                                                            : PIECE_BLACK_KING;
      for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
          if (game_get_piece(row, col) == king_piece) {
            king_row = row;
            king_col = col;
            break;
          }
        }
        if (king_row != -1)
          break;
      }

      if (king_row != -1 && king_col != -1) {
        if (game_led_guidance_show_check_anim()) {
          uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
          // Start the check animation - pink lighting on the king
          game_led_flush();
  game_event_post_check(king_led_index);
          ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                   current_player == PLAYER_WHITE ? "White" : "Black");
        }
      }
    }
  }

  // History and last_move tracking is already done in
  // game_execute_move() - no need to duplicate here

  // Track captured pieces
  if (to_piece != PIECE_EMPTY) {
    if (current_player == PLAYER_WHITE) {
      // Black piece was captured by white
      if ((*game_state_black_captured_index()) < 16) {
        (game_state_black_captured_pieces())[(*game_state_black_captured_index())++] = to_piece;
        (*game_state_black_captured_count())++;
      }
    } else {
      // White piece was captured by black
      if ((*game_state_white_captured_index()) < 16) {
        (game_state_white_captured_pieces())[(*game_state_white_captured_index())++] = to_piece;
        (*game_state_white_captured_count())++;
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
  uint32_t move_time = current_time - (*game_state_last_move_time());

  if (current_player == PLAYER_WHITE) {
    // Black just moved
    (*game_state_black_time_total()) += move_time;
    (*game_state_black_moves_count())++;
  } else {
    // White just moved
    (*game_state_white_time_total()) += move_time;
    (*game_state_white_moves_count())++;
  }

  // Update position history for draw detection
  game_add_position_to_history();

  // Note: End-game conditions already checked earlier in this function (line
  // 6612) If game finished, it was already handled with endgame animation

  // Check if king is in check
  if (game_is_king_in_check(current_player)) {
    if (current_player == PLAYER_WHITE) {
      (*game_state_white_checks())++;
    } else {
      (*game_state_black_checks())++;
    }
    ESP_LOGI(TAG, "⚠️  CHECK! %s king is under attack!",
             (current_player == PLAYER_WHITE) ? "White" : "Black");
  }

  // Update last move time for next move
  (*game_state_last_move_time()) = current_time;

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
                              uint8_t to_row, uint8_t to_col, piece_t piece,
                              piece_t captured) {
  // Convert coordinates to chess notation
  char from_square[4], to_square[4];
  game_coords_to_square(from_row, from_col, from_square);
  game_coords_to_square(to_row, to_col, to_square);

  // Get piece symbol and name
  const char *piece_symbol = (game_state_piece_symbols())[piece];
  const char *piece_name = game_get_piece_name(piece);

  ESP_LOGI(TAG, "╭─────────────────────────────────╮");
  ESP_LOGI(TAG, "│        MOVE ANIMATION          │");
  ESP_LOGI(TAG, "├─────────────────────────────────┤");
  ESP_LOGI(TAG, "│  %s %s moves from %s to %s  │", piece_symbol, piece_name,
           from_square, to_square);

  if (captured != PIECE_EMPTY) {
    const char *captured_symbol = (game_state_piece_symbols())[captured];
    const char *captured_name = game_get_piece_name(captured);
    ESP_LOGI(TAG, "│  Captures: %s %s                │", captured_symbol,
             captured_name);
  } else {
    ESP_LOGI(TAG, "│  No capture                     │");
  }

  ESP_LOGI(TAG, "╰─────────────────────────────────╯");

  // ED: LED animations are now handled in game_process_drop_command
  // This function only shows text information

  ESP_LOGI(TAG, "🎯 Move: %s -> %s", from_square, to_square);
  ESP_LOGI(TAG, "♟️  %s %s moves...", piece_symbol, piece_name);
  ESP_LOGI(TAG, "✨ ...to %s", to_square);

  if (captured != PIECE_EMPTY) {
    ESP_LOGI(TAG, "💥 %s captured!", game_get_piece_name(captured));
  }

  ESP_LOGI(TAG, "✅ Move completed!");
}

