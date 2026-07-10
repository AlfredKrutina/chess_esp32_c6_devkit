/**
 * @file game_matrix_workflow.c
 * @brief Matrix-driven LED workflow, promotion command, movable highlights.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_move_validate.h"

#include "../led_task/include/led_task.h"
#include "../unified_animation_manager/include/unified_animation_manager.h"
#include "led_mapping.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "GAME_MATRIX";

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

  // Zkontrolovat, jestli jsou řádky 1, 2, 7, 8 obsazené figurkami
  // a řádky 3, 4, 5, 6 jsou prázdné

  // Zkontrolovat řádky 0, 1 (bílé figurky) a 6, 7 (černé figurky)
  bool rows_0_1_6_7_occupied = true;
  bool rows_2_3_4_5_empty = true;

  // Zkontrolovat řádky 0, 1, 6, 7 - musí být obsazené
  for (int row = 0; row < 8; row++) {
    bool row_has_pieces = false;
    for (int col = 0; col < 8; col++) {
      if (board[row][col] != PIECE_EMPTY) {
        row_has_pieces = true;
        break;
      }
    }

    if ((row == 0 || row == 1 || row == 6 || row == 7)) {
      // Tyto řádky musí být obsazené
      if (!row_has_pieces) {
        rows_0_1_6_7_occupied = false;
        break;
      }
    } else if (row >= 2 && row <= 5) {
      // Tyto řádky musí být prázdné
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

  // Po zvednutí figurky spustit animaci změny hráče a zobrazit
  // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
  // očekávána rosada)
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

  // Detekce nové hry po endgame animaci
  // Zkontrolovat, jestli se figurky rozestavily na startovní pozice (řádky 1,
  // 2, 7, 8)
  if (game_detect_new_game_setup()) {
    ESP_LOGI(TAG,
             "🎮 NEW GAME DETECTED! Pieces arranged in starting positions");

    // Zastavit všechny animace
    unified_animation_stop_all();

    // Spustit novou hru
    game_start_new_game();

    return; // Ukončit - nová hra byla spuštěna
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
        // The move will be completed in game_handle_matrix_move
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

  // Po umístění figurky spustit animaci změny hráče a zobrazit
  // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
  // očekávána rosada)
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
void game_handle_matrix_move(uint8_t from_row, uint8_t from_col, uint8_t to_row,
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

      .piece = board[from_row][from_col], // Skutečná figurka ze zdrojového pole
      .captured_piece =
          board[to_row][to_col], // Skutečná figurka z cílového pole
      .timestamp = esp_timer_get_time() / 1000};

  // KRITICKÁ OPRAVA: Validace tahu před voláním game_execute_move (stejně
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

  // Detekce rosady před voláním game_execute_move
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

    // KRITICKÁ OPRAVA: Kontrola checkmate/stalemate po každém tahu!
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;
      ESP_LOGI(TAG, "🎉 Game finished detected in matrix move!");
    }

    if (is_castling) {
      // Pro rosadu nespouštět game_highlight_opponent_pieces
      ESP_LOGI(TAG, "🏰 Castling move completed - waiting for rook animation");
    } else {
      // Po úspěšném tahu spustit animaci změny hráče a zobrazit
      // pohyblivé figurky (pouze pokud není aktivní animace rosady a není
      // očekávána rosada)
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

/**
 * @brief Process promotion command from UART
 * @param cmd Promotion command structure
 */
void game_process_promotion_command(const chess_move_command_t *cmd) {
  if (!cmd) {
    ESP_LOGE(TAG, "❌ Invalid promotion command");
    return;
  }

  ESP_LOGI(TAG, "👑 Processing promotion command");

  // #2 & #3: Atomic check with mutex to prevent double execution
  // Použít recursive mutex funkce (promotion_mutex je recursive mutex)
  if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex");
    return;
  }

  // Check if promotion is still pending (might have been processed by another
  // task)
  if (!promotion_state.pending) {
    xSemaphoreGiveRecursive(promotion_mutex);
    ESP_LOGW(TAG, "⚠️ Promotion already processed by another task");
    return;
  }

  // CRITICAL: Keep mutex held during ENTIRE execution to prevent race!
  // DO NOT release here - release only after completion or on error

  // Determine promotion choice (default to Queen if invalid or not specified)
  promotion_choice_t choice = PROMOTION_QUEEN;

  if (cmd->promotion_choice <= PROMOTION_KNIGHT) {
    choice = (promotion_choice_t)cmd->promotion_choice;
    ESP_LOGI(TAG, "👑 Promotion choice received: %d (%s)", choice,
             choice == PROMOTION_QUEEN    ? "Queen"
             : choice == PROMOTION_ROOK   ? "Rook"
             : choice == PROMOTION_BISHOP ? "Bishop"
                                          : "Knight");
  } else {
    ESP_LOGW(TAG,
             "⚠️ Invalid or missing promotion choice (%d), defaulting to Queen",
             cmd->promotion_choice);
  }

  // Execute the promotion with the selected piece
  if (game_execute_promotion(choice)) {
    ESP_LOGI(TAG, "✅ Promotion executed successfully");

    // Clear promotion state (mutex already held from line 10419)
    promotion_state.pending = false;

    // Clear promotion state and return to active gameplay
    current_game_state =
        GAME_STATE_ACTIVE; // Fix: use constant instead of magic number 6

    // Switch to next player
    player_t previous_player = current_player;
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Po dokončení promoce už žádná figurka není zvednutá – aby se zobrazily
    // movable pieces (game_highlight_movable_pieces nesmí skipnout kvůli
    // piece_lifted).
    piece_lifted = false;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    lifted_piece = PIECE_EMPTY;

    // #2: Timer switch AFTER promotion choice
    ESP_LOGI(TAG, "🔄 Timer switch (promotion): ending for %s, starting for %s",
             previous_player == PLAYER_WHITE ? "White" : "Black",
             current_player == PLAYER_WHITE ? "White" : "Black");
    game_end_timer_move();
    game_start_timer_move(current_player == PLAYER_WHITE);

    // KRITICKÉ: Endgame kontrola PŘED player change animací!
    // Pokud je endgame, player change se NESPOUŠTÍ
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;

      // Najít pozici krále vítěze pro endgame animaci
      player_t winner =
          (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
      uint8_t king_pos = 28; // default e4
      for (int i = 0; i < 64; i++) {
        piece_t piece = board[i / 8][i % 8];
        if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
            (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
          king_pos = i;
          break;
        }
      }

      ESP_LOGI(TAG,
               "🎯 Game finished after promotion command! Starting endgame "
               "animation at position %d",
               king_pos);

      // Spustit endgame animaci (wave z krále vítěze)
      led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                   .led_index = king_pos,
                                   .red = 255,
                                   .green = 255,
                                   .blue = 0,        // Yellow
                                   .duration_ms = 0, // Endless
                                   .data = NULL};
      led_execute_command_new(&endgame_cmd);

      ESP_LOGI(
          TAG,
          "✅ Endgame animation started - player change animation SKIPPED");
    } else {
      // Není endgame - spustit player change animaci
      uint8_t player_color =
          (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
      led_command_t player_change_cmd = {
          .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                              // ANIM_PLAYER_CHANGE místo
                                              // PLAYER_CHANGE
          .led_index = 0,
          .red = 0,
          .green = 0,
          .blue = 0,
          .duration_ms = 0,
          .data = &player_color // Předat barvu hráče
      };
      led_execute_command_new(&player_change_cmd);

      // Print updated board
      game_print_board();

      // Highlight pieces that the new current player can move
      game_highlight_movable_pieces();
      led_force_immediate_update(); // Commit highlight so movable pieces show
                                    // right after player change animation

      // Zkontrolovat, zda je nový hráč v šachu
      bool in_check = game_is_king_in_check(current_player);
      if (in_check) {
        // Najít pozici krále
        int king_row = -1, king_col = -1;
        piece_t king_piece = (current_player == PLAYER_WHITE)
                                 ? PIECE_WHITE_KING
                                 : PIECE_BLACK_KING;
        for (int row = 0; row < 8; row++) {
          for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
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
            // Spustit check animaci - růžové svícení na králi
            led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                       .led_index = king_led_index,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms =
                                           0, // Trvalé až do dalšího tahu
                                       .data = NULL};
            led_execute_command_new(&check_cmd);
            ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                     current_player == PLAYER_WHITE ? "White" : "Black");
          }
        }
      }
    }

  } else {
    ESP_LOGE(TAG, "❌ Failed to execute promotion");
  }

  // CRITICAL: Release mutex at the end of the function to ensure it's always
  // released Použít recursive mutex funkce (promotion_mutex je recursive mutex)
  xSemaphoreGiveRecursive(promotion_mutex);
}

/**
 * @brief Provede promoci pesce na vybranou figurku
 *
 * @param choice Vyber figurky pro promoci (PROMOTION_QUEEN, PROMOTION_ROOK,
 * PROMOTION_BISHOP, PROMOTION_KNIGHT)
 * @return true pokud byla promoce uspesna, false pokud selha
 *
 * @details
 * Funkce hleda pesce na promocni rade a povysuje ho na vybranou figurku:
 * - Bily pesec na 8. rade (row 7) -> povyseni
 * - Cerny pesec na 1. rade (row 0) -> povyseni
 *
 * Proces:
 * 1. Prevod choice na typ figurky podle barvy hrace
 * 2. Prohledani desky pro pesce na promocni rade
 * 3. Nahrazeni pesce povysenou figurkou
 *
 * @note KRITICKA OPRAVA v2.4.1:
 * - BUG #10: Row indexing byl OBRACENY!
 * - Puvodni: WHITE row==0, BLACK row==7 (SPATNE!)
 * - Opraveno: WHITE row==7 (8. rada), BLACK row==0 (1. rada)
 * - Promoce nyni funguje 100% spravne
 */
bool game_execute_promotion(promotion_choice_t choice) {
  ESP_LOGI(TAG, "👑 Executing pawn promotion: %d", choice);

  // Find the pawn that needs to be promoted
  // This should be set when a pawn reaches the promotion rank
  // For now, we'll implement a basic version

  // Convert promotion choice to piece type
  piece_t promoted_piece;
  // Use promotion_state.player instead of current_player
  // current_player has already been switched to the opponent by
  // game_execute_move() so we must use the player stored in the promotion
  // state
  if (promotion_state.player == PLAYER_WHITE) {
    switch (choice) {
    case PROMOTION_QUEEN:
      promoted_piece = PIECE_WHITE_QUEEN;
      break;
    case PROMOTION_ROOK:
      promoted_piece = PIECE_WHITE_ROOK;
      break;
    case PROMOTION_BISHOP:
      promoted_piece = PIECE_WHITE_BISHOP;
      break;
    case PROMOTION_KNIGHT:
      promoted_piece = PIECE_WHITE_KNIGHT;
      break;
    default:
      return false;
    }
  } else {
    switch (choice) {
    case PROMOTION_QUEEN:
      promoted_piece = PIECE_BLACK_QUEEN;
      break;
    case PROMOTION_ROOK:
      promoted_piece = PIECE_BLACK_ROOK;
      break;
    case PROMOTION_BISHOP:
      promoted_piece = PIECE_BLACK_BISHOP;
      break;
    case PROMOTION_KNIGHT:
      promoted_piece = PIECE_BLACK_KNIGHT;
      break;
    default:
      return false;
    }
  }

  // BUG #3: Use stored position instead of scanning entire board
  // The position was already found and stored in promotion_state by
  // game_check_promotion_needed()
  uint8_t row = promotion_state.square_row;
  uint8_t col = promotion_state.square_col;
  piece_t piece = board[row][col];

  // Verify it's actually a pawn (safety check)
  if (piece != PIECE_WHITE_PAWN && piece != PIECE_BLACK_PAWN) {
    ESP_LOGE(TAG, "❌ Promotion failed: No pawn at stored position %c%d",
             'a' + col, row + 1);
    return false;
  }

  // Promote the pawn
  board[row][col] = promoted_piece;
  current_game_state = GAME_STATE_ACTIVE; // Restore game state to ACTIVE
  ESP_LOGI(TAG, "✅ Promoted %s pawn at %c%d to %s",
           current_player == PLAYER_WHITE ? "white" : "black", 'a' + col,
           row + 1,
           choice == PROMOTION_QUEEN    ? "Queen"
           : choice == PROMOTION_ROOK   ? "Rook"
           : choice == PROMOTION_BISHOP ? "Bishop"
                                        : "Knight");

  // Spustit promotion animaci
  uint8_t promotion_led = chess_pos_to_led_index(row, col);
  led_command_t promote_cmd = {.type = LED_CMD_ANIM_PROMOTE,
                               .led_index = promotion_led,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 2000,
                               .data = NULL};
  led_execute_command_new(&promote_cmd);

  return true;
}

/**
 * @brief Highlight all movable pieces for current player
 */
void game_highlight_movable_pieces(void) {
  // Do not highlight pieces if system is booting
  if (led_is_booting()) {
    return;
  }

  // Do not highlight movable pieces if a piece is already
  // lifted This prevents overwriting the "valid moves" (green) with "movable
  // pieces" (yellow)
  if (piece_lifted) {
    ESP_LOGD(TAG, "Highlight skipped - piece is lifted");
    return;
  }

  // Nespouštět highlight během rošády - rošáda má vlastní LED indikaci
  if (castling_state.in_progress) {
    ESP_LOGD(TAG, "Highlight skipped - castling in progress");
    return;
  }

  if (!game_led_guidance_show_movable_yellow()) {
    ESP_LOGD(TAG, "Movable piece highlights skipped (LED guidance level %u)",
             (unsigned)game_get_led_guidance_level());
    return;
  }

  ESP_LOGI(TAG, "🟡 Highlighting movable pieces for %s player",
           current_player == PLAYER_WHITE ? "white" : "black");

  // Endgame detection is handled in game_analyze_position() called from
  // game_execute_move()

  // LED command queue removed - using direct LED calls now

  uint32_t highlighted_count = 0;

  // Scan all squares for current player's pieces
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      // Check if piece belongs to current player
      bool is_current_player_piece = false;
      if (current_player == PLAYER_WHITE) {
        is_current_player_piece =
            (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
      } else {
        is_current_player_piece =
            (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
      }

      if (is_current_player_piece) {
        // Check if piece has valid moves
        // OPTIMIZATION: Reduced buffer size from 64 to 1 to prevent stack
        // overflow in Timer Task We only need to know if count > 0, so asking
        // for 1 move is enough
        move_suggestion_t suggestions[1];
        uint32_t valid_moves =
            game_get_available_moves(row, col, suggestions, 1);

        if (valid_moves > 0) {
          // Highlight movable piece in yellow
          led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255,
                             0); // Yellow
          highlighted_count++;
        }
      }
    }
  }

  ESP_LOGI(TAG, "🟡 Highlighted %lu movable pieces", highlighted_count);
}
