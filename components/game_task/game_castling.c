/**
 * @file game_castling.c
 * @brief Castling detection, rook guidance, legacy animation, LED refresh.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_matrix_guard.h"

#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "game_led_animations.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "GAME_CASTLING";

static chess_move_extended_t pending_castle_move;
static TimerHandle_t rook_animation_timer = NULL;
static bool rook_animation_active = false;

// ============================================================================
// PUZZLE COMMAND IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Detect and handle castling in DROP command
 * @param move Move that was attempted
 */
void game_detect_and_handle_castling(const chess_move_t *move) {
  piece_t piece = move->piece;

  // Je to král a pohybuje se o 2 pole?
  if ((piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING) &&
      abs((int)move->to_col - (int)move->from_col) == 2) {

    ESP_LOGI(TAG, "🏰 CASTLING DETECTED! King moved 2 squares");

    // Nastavit stav rošády
    castling_state.in_progress = true;
    castling_state.player = current_player;

    // Vypočítat pozice věže
    bool is_kingside = (move->to_col == 6);
    castling_state.rook_from_row = move->to_row;
    castling_state.rook_from_col = is_kingside ? 7 : 0;
    castling_state.rook_to_row = move->to_row;
    castling_state.rook_to_col = is_kingside ? 5 : 3;

    // Zobrazit LED guidance pro věž
    game_show_castling_rook_guidance();

    // NEZMĚNIT HRÁČE - čekat na dokončení rošády
    ESP_LOGI(TAG, "⏳ Waiting for rook move to complete castling...");
  }
}

/**
 * @brief Show LED guidance for castling rook move
 */
void game_show_castling_rook_guidance() {
  // Vyčistit board
  led_clear_board_only();

  // Postupná animace od věže k cíli
  uint8_t rook_from_led = chess_pos_to_led_index(castling_state.rook_from_row,
                                                 castling_state.rook_from_col);
  uint8_t rook_to_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                               castling_state.rook_to_col);

  // Zvýraznit věž (žlutá)
  led_set_pixel_safe(rook_from_led, 255, 255, 0); // Žlutá - zvedni odtud

  // Postupně rozsvítit pole od věže k cíli
  int from_col = castling_state.rook_from_col;
  int to_col = castling_state.rook_to_col;
  int step = (to_col > from_col) ? 1 : -1;

  for (int col = from_col + step; col != to_col + step; col += step) {
    uint8_t led_index =
        chess_pos_to_led_index(castling_state.rook_from_row, col);
    led_set_pixel_safe(led_index, 0, 255, 0); // Zelená - cesta
    vTaskDelay(pdMS_TO_TICKS(300));
  }

  // Zvýraznit cíl (zelená)
  led_set_pixel_safe(rook_to_led, 0, 255, 0); // Zelená - polož sem

  ESP_LOGI(TAG, "💡 Move rook from %c%d to %c%d to complete castling",
           'a' + castling_state.rook_from_col, castling_state.rook_from_row + 1,
           'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
}

/**
 * @brief Check if castling is completed in DROP command
 * @param move Move that was attempted
 * @return true if castling was completed
 */
bool game_check_castling_completion(const chess_move_t *move) {
  if (!castling_state.in_progress)
    return false;

  // Je to správný tah věže?
  if (move->from_row == castling_state.rook_from_row &&
      move->from_col == castling_state.rook_from_col &&
      move->to_row == castling_state.rook_to_row &&
      move->to_col == castling_state.rook_to_col) {

    ESP_LOGI(TAG, "✅ CASTLING COMPLETED! Rook moved correctly");

    // Provedeme posun věže v board[][]
    board[move->to_row][move->to_col] = move->piece;
    board[castling_state.rook_from_row][castling_state.rook_from_col] =
        PIECE_EMPTY;

    // Aktualizovat příznaky pro krále a věž
    if (castling_state.player == PLAYER_WHITE) {
      white_king_moved = true;
      if (castling_state.rook_from_col == 7)
        white_rook_h_moved = true;
      else
        white_rook_a_moved = true;
    } else {
      black_king_moved = true;
      if (castling_state.rook_from_col == 7)
        black_rook_h_moved = true;
      else
        black_rook_a_moved = true;
    }

    // Zlatá animace dokončení rošády
    show_castling_completion_animation();

    // Změnit hráče TEPRVE TEĎ
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

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
               "🎯 Game finished after castling completion! Starting endgame "
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

      // Vyčistit stav rošády
      memset(&castling_state, 0, sizeof(castling_state));

      // Zobrazit pohyblivé figury pro nového hráče
      led_clear_board_only();
      game_highlight_movable_pieces();

      ESP_LOGI(TAG, "🏰 Castling completed! %s to move",
               (current_player == PLAYER_WHITE) ? "White" : "Black");

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

    return true;
  } else {
    // Nesprávný tah věže - ukázat znovu guidance
    ESP_LOGI(TAG, "❌ Incorrect rook move for castling");
    game_show_castling_rook_guidance();
    return false;
  }
}


// ============================================================================
// CASTLING ANIMATION SYSTEM (LEGACY)
// ============================================================================

/**
 * @brief Start castle animation - highlight rook that needs to be moved
 * @param move Castle move (king has already moved)
 */
void game_start_castle_animation(const chess_move_extended_t *move) {
  if (!move)
    return;

  ESP_LOGI(TAG, "🏰 Starting castle animation for %s",
           (move->move_type == MOVE_TYPE_CASTLE_KING) ? "kingside"
                                                      : "queenside");

  // Calculate rook positions
  uint8_t king_row = (current_player == PLAYER_WHITE) ? 0 : 7;

  if (move->move_type == MOVE_TYPE_CASTLE_KING) {
    // Kingside: rook from h-file to f-file
    rook_from_row = king_row;
    rook_from_col = 7; // h-file
    rook_to_row = king_row;
    rook_to_col = 5; // f-file
  } else {
    // Queenside: rook from a-file to d-file
    rook_from_row = king_row;
    rook_from_col = 0; // a-file
    rook_to_row = king_row;
    rook_to_col = 3; // d-file
  }

  // Store castle move for completion
  pending_castle_move = *move;
  castle_animation_active = true;

  // Clear board and start repeating rook move animation
  led_clear_board_only();

  // Spustit opakující se animaci pohybu věže
  game_start_repeating_rook_animation();

  // Show initial animation immediately
  uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
  uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
  led_set_pixel_safe(from_led, 255, 255, 0); // Yellow for rook position
  led_set_pixel_safe(to_led, 0, 255, 0);     // Green for destination

  ESP_LOGI(TAG, "🏰 Rook at %c%d needs to move to %c%d", 'a' + rook_from_col,
           rook_from_row + 1, 'a' + rook_to_col, rook_to_row + 1);
}

/**
 * @brief Timer callback for repeating rook animation
 * @param xTimer Timer handle
 */
void rook_animation_timer_callback(TimerHandle_t xTimer) {
  if (!rook_animation_active || !castle_animation_active) {
    return;
  }

  // Simple pulsing animation - show rook position and destination
  uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
  uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);

  // Clear board first
  led_clear_board_only();

  // Show rook position in yellow (pulsing)
  static bool pulse_state = false;
  pulse_state = !pulse_state;

  if (pulse_state) {
    // Bright yellow for rook position
    led_set_pixel_safe(from_led, 255, 255, 0);
    // Bright green for destination
    led_set_pixel_safe(to_led, 0, 255, 0);
  } else {
    // Dim yellow for rook position
    led_set_pixel_safe(from_led, 128, 128, 0);
    // Dim green for destination
    led_set_pixel_safe(to_led, 0, 128, 0);
  }

  ESP_LOGI(TAG, "🏰 Rook animation: %c%d -> %c%d (%s)", 'a' + rook_from_col,
           rook_from_row + 1, 'a' + rook_to_col, rook_to_row + 1,
           pulse_state ? "bright" : "dim");
}

/**
 * @brief Start repeating rook animation
 */
void game_start_repeating_rook_animation(void) {
  if (rook_animation_timer == NULL) {
    // Create timer for rook animation
    rook_animation_timer =
        xTimerCreate("rook_anim_timer",
                     pdMS_TO_TICKS(500), // 500ms interval (snappier)
                     pdTRUE,             // Auto-reload
                     NULL, rook_animation_timer_callback);

    if (rook_animation_timer == NULL) {
      ESP_LOGE(TAG, "❌ Failed to create rook animation timer");
      return;
    }
  }

  rook_animation_active = true;

  // Start the timer
  if (xTimerStart(rook_animation_timer, 0) != pdPASS) {
    ESP_LOGE(TAG, "❌ Failed to start rook animation timer");
    rook_animation_active = false;
    return;
  }

  ESP_LOGI(TAG, "🏰 Started repeating rook animation");
}

/**
 * @brief Stop repeating rook animation
 */
void game_stop_repeating_rook_animation(void) {
  if (rook_animation_timer != NULL) {
    xTimerStop(rook_animation_timer, 0);
  }
  rook_animation_active = false;
  ESP_LOGI(TAG, "🏰 Stopped repeating rook animation");
}

/**
 * @brief Check if castle animation is active
 * @return true if castle animation is waiting for rook move
 */
bool game_is_castle_animation_active(void) { return castle_animation_active; }

/**
 * @brief Check if castling is expected (king lifted and about to be placed 2
 * squares away)
 * @return true if castling is expected
 */
bool game_is_castling_expected(void) {
  // Check if a king is lifted and about to be placed 2 squares away
  if (piece_lifted &&
      (lifted_piece == PIECE_WHITE_KING || lifted_piece == PIECE_BLACK_KING)) {
    // This function is called from game_handle_piece_placed
    // We need to check if the king is being placed 2 squares away from its
    // original position But we don't have access to the destination
    // coordinates here So we return true to prevent premature opponent piece
    // highlighting The actual castling detection will happen in
    // game_handle_matrix_move
    ESP_LOGI(TAG, "🏰 King is lifted - castling might be expected");
    return true;
  }
  return false;
}

/**
 * @brief Complete castle animation when rook is moved
 * @param from_row Rook source row
 * @param from_col Rook source column
 * @param to_row Rook destination row
 * @param to_col Rook destination column
 * @return true if castle was completed successfully
 */
bool game_complete_castle_animation(uint8_t from_row, uint8_t from_col,
                                    uint8_t to_row, uint8_t to_col) {
  if (!castle_animation_active) {
    return false;
  }

  // Check if rook was moved to correct position
  if (from_row == rook_from_row && from_col == rook_from_col &&
      to_row == rook_to_row && to_col == rook_to_col) {

    ESP_LOGI(TAG, "✅ Castle animation completed! Rook moved correctly.");

    // Skutečně přesunout věž až když hráč udělá správný tah
    piece_t rook_piece = board[rook_from_row][rook_from_col];
    board[rook_from_row][rook_from_col] = PIECE_EMPTY;
    board[rook_to_row][rook_to_col] = rook_piece;

    // Stop repeating rook animation
    game_stop_repeating_rook_animation();

    // Complete the castle move
    castle_animation_active = false;

    // Switch players now that castle is complete
    player_t previous_player = current_player;
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    // Po úspěšné rošádě spustit animaci změny hráče
    game_show_player_change_animation(previous_player, current_player);

    // Clear board and show movable pieces for new player
    led_clear_board_only();
    game_highlight_movable_pieces();

    ESP_LOGI(TAG, "🏰 Castling completed! %s to move",
             (current_player == PLAYER_WHITE) ? "White" : "Black");

    return true;
  } else {
    ESP_LOGW(TAG,
             "❌ Invalid rook move for castling: %c%d -> %c%d (expected: %c%d "
             "-> %c%d)",
             'a' + from_col, from_row + 1, 'a' + to_col, to_row + 1,
             'a' + rook_from_col, rook_from_row + 1, 'a' + rook_to_col,
             rook_to_row + 1);

    // Keep castle animation active, wait for correct move
    return false;
  }
}



/**
 * @brief Force refresh of LED state based on current game status
 * Called when switching back from HA mode or other interruptions
 */
void game_refresh_leds(void) {
  ESP_LOGI(TAG, "🔄 Refreshing LED state based on game status...");

  // If game is not active, just show the board pattern
  if (!game_active && current_game_state != GAME_STATE_PROMOTION) {
    // If we are in "setup" or "idle" mode
    led_show_chess_board();
    return;
  }

  // Clear board first (to remove HA colors)
  led_clear_board_only();

  if (game_is_matrix_guard_active()) {
    ESP_LOGI(TAG, "  - Restoring MATRIX GUARD LEDs (physical vs logic)");
    game_matrix_guard_render_leds();
    game_check_promotion_needed();
    return;
  }

  // 1. Check for Error Recovery State
  if (error_recovery_state.waiting_for_move_correction) {
    ESP_LOGI(TAG, "  - Restoring ERROR state LEDs");
    // Show validation error
    game_show_invalid_move_error_with_blink(error_recovery_state.invalid_row,
                                            error_recovery_state.invalid_col);

    // Also highlight the original valid position (Blue)
    led_set_pixel_safe(
        chess_pos_to_led_index(error_recovery_state.original_valid_row,
                               error_recovery_state.original_valid_col),
        0, 0, 255);

    return;
  }

  // 2. Check for Lifted Piece State
  if (piece_lifted) {
    ESP_LOGI(TAG, "  - Restoring LIFTED piece state LEDs at %c%d",
             'a' + lifted_piece_col, lifted_piece_row + 1);

    // Highlight source square (Yellow)
    led_set_pixel_safe(
        chess_pos_to_led_index(lifted_piece_row, lifted_piece_col), 255, 255,
        0);

    // Show valid moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves = game_get_available_moves(
        lifted_piece_row, lifted_piece_col, suggestions, 64);

    if (valid_moves > 0) {
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
          led_set_pixel_safe(led_index, 255, 165, 0); // Orange for capture
        } else {
          led_set_pixel_safe(led_index, 0, 255, 0); // Green for move
        }
      }
    }
    return;
  }

  // 3. Check for Check state
  if (game_is_king_in_check(current_player)) {
    // Find King
    int king_row = -1, king_col = -1;
    piece_t king_piece =
        (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
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

    if (king_row != -1) {
      if (game_led_guidance_show_check_anim()) {
        ESP_LOGI(TAG, "  - Restoring CHECK state LEDs");
        uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
        led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                   .led_index = king_led_index,
                                   .red = 0,
                                   .green = 0,
                                   .blue = 0,
                                   .duration_ms = 0,
                                   .data = NULL};
        led_execute_command_new(&check_cmd);
      }
    }
  }

  // 4. Default Active State - Highlight movable pieces
  ESP_LOGI(TAG, "  - Restoring ACTIVE state LEDs (movable pieces)");
  game_highlight_movable_pieces();

  // Update button LEDs (64-71) to match current game state (promotion green/blue)
  // so they are correct after switching back to game mode (e.g. from HA).
  game_check_promotion_needed();
}
