/**
 * @file game_move_exec.c
 * @brief Move execution, position analysis, and castling completion animation.
 */

#include "game_move_exec.h"
#include "game_move_validate.h"
#include "game_snapshot.h"
#include "position.h"
#include "game_physical_undo.h"

#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_policy.h"
#include "game_required_step.h"
#include "game_led_animations.h"
#include "led_mapping.h"

#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"
#include "../squares_task/include/hall_i2c_squares.h"
#include "game_hall_match.h"
#include "game_hall_position.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

static const char *TAG = "GAME_MOVE_EXEC";

// Fifty-move rule counter (halfmoves). Persisted by game_snapshot.
static uint32_t fifty_move_counter = 0;

uint32_t game_get_fifty_move_counter(void) { return fifty_move_counter; }

void game_set_fifty_move_counter(uint32_t value) { fifty_move_counter = value; }

// ============================================================================
// MOVE EXECUTION FUNCTIONS
// ============================================================================

/**
 * @brief Check for checkmate or stalemate
 */
game_state_t game_analyze_position(player_t player) {
  bool king_in_check = game_is_king_in_check(player);
  uint32_t legal_moves = game_generate_legal_moves(player);

  if (legal_moves == 0) {
    if (king_in_check) {
      // Checkmate
      (*game_state_game_result()) = GAME_STATE_FINISHED;
      game_result_type_t result_type =
          player == PLAYER_WHITE ? RESULT_BLACK_WINS : RESULT_WHITE_WINS;
      (*game_state_current_result_type()) = result_type; // Save for Web API
      game_update_endgame_statistics(result_type);
      game_print_endgame_report_uart(result_type);
      ESP_LOGI(TAG, "🎯 CHECKMATE! %s wins!",
               player == PLAYER_WHITE ? "Black" : "White");
      return GAME_STATE_FINISHED;
    } else {
      // Stalemate
      (*game_state_game_result()) = GAME_STATE_FINISHED;
      (*game_state_current_result_type()) = RESULT_DRAW_STALEMATE; // Save for Web API
      game_update_endgame_statistics(RESULT_DRAW_STALEMATE);
      game_print_endgame_report_uart(RESULT_DRAW_STALEMATE);
      ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn!");
      return GAME_STATE_FINISHED;
    }
  }

  // Check for fifty-move rule
  if (fifty_move_counter >= 100) { // 50 moves per side
    (*game_state_game_result()) = GAME_STATE_FINISHED;
    (*game_state_current_result_type()) = RESULT_DRAW_50_MOVE; // Save for Web API
    game_update_endgame_statistics(RESULT_DRAW_50_MOVE);
    game_print_endgame_report_uart(RESULT_DRAW_50_MOVE);
    ESP_LOGI(TAG, "🤝 DRAW! Fifty-move rule!");
    return GAME_STATE_FINISHED;
  }

  // Check for insufficient material (simplified)
  int white_pieces = 0, black_pieces = 0;
  bool white_has_major = false, black_has_major = false;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = game_get_piece(row, col);
      if (piece == PIECE_EMPTY)
        continue;

      if (game_is_white_piece(piece)) {
        white_pieces++;
        if (piece == PIECE_WHITE_QUEEN || piece == PIECE_WHITE_ROOK ||
            piece == PIECE_WHITE_PAWN) {
          white_has_major = true;
        }
      } else {
        black_pieces++;
        if (piece == PIECE_BLACK_QUEEN || piece == PIECE_BLACK_ROOK ||
            piece == PIECE_BLACK_PAWN) {
          black_has_major = true;
        }
      }
    }
  }

  if (white_pieces <= 2 && black_pieces <= 2 && !white_has_major &&
      !black_has_major) {
    (*game_state_game_result()) = GAME_STATE_FINISHED;
    (*game_state_current_result_type()) = RESULT_DRAW_INSUFFICIENT; // Save for Web API
    game_update_endgame_statistics(RESULT_DRAW_INSUFFICIENT);
    game_print_endgame_report_uart(RESULT_DRAW_INSUFFICIENT);
    ESP_LOGI(TAG, "🤝 DRAW! Insufficient material!");
    return GAME_STATE_FINISHED;
  }

  return GAME_STATE_ACTIVE;
}

bool game_execute_move(const chess_move_t *move) {
  if (!move) {
    return false;
  }

  const bool api_immediate_promo = (*game_state_s_uart_move_immediate_promotion());
  const promotion_choice_t api_promo_choice = (*game_state_s_uart_move_immediate_promotion_piece());
  (*game_state_s_uart_move_immediate_promotion()) = false;

  move_error_t error = game_is_valid_move(move);
  if (error != MOVE_ERROR_NONE) {
    ESP_LOGW(TAG, "Invalid move attempted: %d", error);
    return false;
  }

  ESP_LOGI(TAG, "Executing move: %c%d-%c%d", 'a' + move->from_col,
           move->from_row + 1, 'a' + move->to_col, move->to_row + 1);

  // Get pieces
  piece_t source_piece = game_get_piece(move->from_row, move->from_col);
  piece_t dest_piece = game_get_piece(move->to_row, move->to_col);

  // Create extended move for proper handling of special cases
  chess_move_extended_t extended_move = {.from_row = move->from_row,
                                         .from_col = move->from_col,
                                         .to_row = move->to_row,
                                         .to_col = move->to_col,
                                         .piece = source_piece,
                                         .captured_piece = dest_piece,
                                         .move_type = MOVE_TYPE_NORMAL,
                                         .promotion_piece = PROMOTION_QUEEN,
                                         .timestamp =
                                             esp_timer_get_time() / 1000,
                                         .is_check = false,
                                         .is_checkmate = false,
                                         .is_stalemate = false};

  // Detect special move types

  // Disable special move detection during error recovery
  //  During error recovery, ALL moves are treated as NORMAL moves to prevent
  //  state conflicts
  if (!(*game_state_error_recovery_state()).waiting_for_move_correction) {
    // 1. Check for castling
    if ((source_piece == PIECE_WHITE_KING ||
         source_piece == PIECE_BLACK_KING) &&
        abs(move->to_col - move->from_col) == 2) {
      extended_move.move_type = (move->to_col > move->from_col)
                                    ? MOVE_TYPE_CASTLE_KING
                                    : MOVE_TYPE_CASTLE_QUEEN;
    }

    // 2. Check for en passant
    else if ((source_piece == PIECE_WHITE_PAWN ||
              source_piece == PIECE_BLACK_PAWN) &&
             abs(move->to_col - move->from_col) == 1 &&
             dest_piece == PIECE_EMPTY) {
      if (game_is_en_passant_possible(move)) {
        extended_move.move_type = MOVE_TYPE_EN_PASSANT;
      }

    }

    // 3. Check for promotion
    else if ((source_piece == PIECE_WHITE_PAWN && move->to_row == 7) ||
             (source_piece == PIECE_BLACK_PAWN && move->to_row == 0)) {
      extended_move.move_type = MOVE_TYPE_PROMOTION;

      if (api_immediate_promo && api_promo_choice <= PROMOTION_KNIGHT) {
        extended_move.promotion_piece = api_promo_choice;
        ESP_LOGI(TAG,
                 "👑 API immediate promotion (choice=%d) — skip pending state",
                 (int)api_promo_choice);
      } else {
        // IMMEDIATELY set (*game_state_promotion_state()) before calling
        // game_execute_move_enhanced() This ensures that
        // game_execute_move_enhanced() does not auto-promote Use recursive
        // mutex function ((*game_state_promotion_mutex()) is a recursive mutex)
        if (xSemaphoreTakeRecursive((*game_state_promotion_mutex()), pdMS_TO_TICKS(100)) ==
            pdTRUE) {
          (*game_state_promotion_state()).pending = true;
          (*game_state_promotion_state()).square_row = move->to_row;
          (*game_state_promotion_state()).square_col = move->to_col;
          (*game_state_promotion_state()).player = current_player;
          xSemaphoreGiveRecursive((*game_state_promotion_mutex()));

          ESP_LOGI(TAG,
                   "⏸️  Promotion detected - setting pending state at %c%d for %s "
                   "player",
                   'a' + move->to_col, move->to_row + 1,
                   current_player == PLAYER_WHITE ? "White" : "Black");
        } else {
          ESP_LOGE(TAG,
                   "❌ Failed to acquire promotion mutex during move detection!");
        }
      }
    }

    // 4. Check for capture
    else if (dest_piece != PIECE_EMPTY) {
      extended_move.move_type = MOVE_TYPE_CAPTURE;
      ESP_LOGI(TAG, "Capture: %s captures %s",
               game_get_piece_name(source_piece),
               game_get_piece_name(dest_piece));

      // Add captured piece to tracking
      game_add_captured_piece(dest_piece);
    }
  } else {
    // During error recovery, treat everything as NORMAL move (or CAPTURE if
    // opponent piece present)
    if (dest_piece != PIECE_EMPTY) {
      extended_move.move_type = MOVE_TYPE_CAPTURE;
      ESP_LOGI(TAG, "Error recovery: treating as simple capture");
    } else {
      extended_move.move_type = MOVE_TYPE_NORMAL;
      ESP_LOGI(TAG, "Error recovery: treating as normal move");
    }
  }

  bool skip_undo_meta =
      (castling_state.in_progress &&
       move->from_row == castling_state.rook_from_row &&
       move->from_col == castling_state.rook_from_col &&
       move->to_row == castling_state.rook_to_row &&
       move->to_col == castling_state.rook_to_col);
  if (!skip_undo_meta && history_index < GAME_TASK_MAX_MOVES_HISTORY) {
    game_move_undo_meta_init_record(history_index, extended_move.move_type, move);
  }

  // Execute the enhanced move
  bool success = game_execute_move_enhanced(&extended_move);

  ESP_LOGI(TAG,
           "🔍 game_execute_move: success=%d, current_player before change=%s",
           success, current_player == PLAYER_WHITE ? "White" : "Black");

  if (success) {
    game_coach_hint_clear();
    // Update last move tracking for future en passant detection
    last_move_from_row = move->from_row;
    last_move_from_col = move->from_col;
    last_move_to_row = move->to_row;
    last_move_to_col = move->to_col;
    has_last_move = true;

    // Cast = one turn in history (King has already been added; don't add rook move)
    bool is_castling_rook_completion =
        (castling_state.in_progress &&
         move->from_row == castling_state.rook_from_row &&
         move->from_col == castling_state.rook_from_col &&
         move->to_row == castling_state.rook_to_row &&
         move->to_col == castling_state.rook_to_col);

    if (!is_castling_rook_completion) {
      // Add to move history
      if (history_index < GAME_TASK_MAX_MOVES_HISTORY) {
        move_history[history_index] = *move;
        move_history[history_index].piece = source_piece;
        move_history[history_index].captured_piece = dest_piece;
        move_history[history_index].timestamp = esp_timer_get_time() / 1000;
        move_history_kind[history_index] = extended_move.move_type;
        history_index++;
        if (extended_move.move_type == MOVE_TYPE_PROMOTION &&
            !(*game_state_promotion_state()).pending) {
          game_move_undo_meta_on_promotion_completed(
              game_get_piece(move->to_row, move->to_col));
        }
      }

      // Update game state
      move_count++;
      (*game_state_last_move_time()) = esp_timer_get_time() / 1000;

      // Record material advantage pro graf
      game_record_material_advantage();
    } else {
      (*game_state_last_move_time()) = esp_timer_get_time() / 1000;
    }

    // ENHANCED CASTLING DETECTION
    if (extended_move.move_type == MOVE_TYPE_CASTLE_KING ||
        extended_move.move_type == MOVE_TYPE_CASTLE_QUEEN) {
      ESP_LOGI(TAG, "🏰 CASTLING DETECTED! King moved 2 squares");

      // Nastavit castling state pro tracking
      castling_state.in_progress = true;
      castling_state.player =
          game_is_white_piece(source_piece) ? PLAYER_WHITE : PLAYER_BLACK;
      castling_state.is_kingside = (move->to_col > move->from_col);

      // Calculate expected tower positions
      uint8_t rook_row = move->from_row;
      if (castling_state.is_kingside) {
        castling_state.rook_from_row = rook_row;
        castling_state.rook_from_col = 7; // h-file
        castling_state.rook_to_row = rook_row;
        castling_state.rook_to_col = 5; // f-file
      } else {
        castling_state.rook_from_row = rook_row;
        castling_state.rook_from_col = 0; // a-file
        castling_state.rook_to_row = rook_row;
        castling_state.rook_to_col = 3; // d-file
      }

      // Hall already has the rook on its square. Finish without the second step.
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      {
        uint8_t rr = castling_state.rook_from_row;
        uint8_t rc = castling_state.rook_from_col;
        uint8_t tr = castling_state.rook_to_row;
        uint8_t tc = castling_state.rook_to_col;
        if (game_get_piece(rr, rc) != PIECE_EMPTY) {
          piece_t rook_piece = game_get_piece(rr, rc);
          game_set_piece(rr, rc, PIECE_EMPTY);
          game_set_piece(tr, tc, rook_piece);
        }
        piece_moved[rr][rc] = true;
        if (castling_state.player == PLAYER_WHITE) {
          (*game_state_white_castles())++;
          if (rc == 7) {
            white_rook_h_moved = true;
          } else {
            white_rook_a_moved = true;
          }
        } else {
          (*game_state_black_castles())++;
          if (rc == 7) {
            black_rook_h_moved = true;
          } else {
            black_rook_a_moved = true;
          }
        }
        castling_state.in_progress = false;
        if (current_player == castling_state.player) {
          current_player =
              (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        }
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);
        if (game_check_end_game_conditions() == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;
        }
        game_hall_snapshot_last_valid();
        game_snapshot_persist_after_valid_move();
        return success;
      }
#else
      if (game_get_piece(castling_state.rook_from_row,
                         castling_state.rook_from_col) == PIECE_EMPTY &&
          game_get_piece(castling_state.rook_to_row,
                         castling_state.rook_to_col) != PIECE_EMPTY) {
        game_set_piece(castling_state.rook_from_row,
                       castling_state.rook_from_col,
                       game_get_piece(castling_state.rook_to_row,
                                        castling_state.rook_to_col));
        game_set_piece(castling_state.rook_to_row, castling_state.rook_to_col,
                       PIECE_EMPTY);
      }
      current_player = castling_state.player;
      // Show LED indication for tower with pulsation for better visibility
      game_led_clear_board();

      uint8_t rook_from_led = chess_pos_to_led_index(
          castling_state.rook_from_row, castling_state.rook_from_col);
      uint8_t rook_to_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                                   castling_state.rook_to_col);

      // Pulsing for better visibility (3 cycles with a smooth transition)
      // Use the correct brightness calculation for smooth pulsing
      for (int pulse = 0; pulse < 3; pulse++) {
        // Smooth pulsation: 0.5 -> 1.0 -> 0.5
        // Use sin() with normalization - sin() returns -1 to 1, we normalize to
        // 0-1, pak na 0.5-1.0
        float phase = (float)pulse * 2.0f * 3.14159f / 3.0f; // 0, 2π/3, 4π/3
        float brightness =
            0.5f + 0.5f * (1.0f + sin(phase)) /
                       2.0f; // Normalizace: sin() -> 0-1 -> 0.5-1.0

        // Silver for tower (source) with pulsation
        game_led_frame_begin(0, false);
        game_led_paint_led(rook_from_led, (uint8_t)(192 * brightness), (uint8_t)(192 * brightness), (uint8_t)(192 * brightness));

        // Green for tower destination (destination) with pulsation
        game_led_paint_led(rook_to_led, 0, (uint8_t)(255 * brightness), 0);

        game_led_frame_end();
        vTaskDelay(pdMS_TO_TICKS(200));
      }

      // Final static display
      game_led_frame_begin(0, false);
      game_led_paint_led(rook_from_led, 192, 192, 192); // Silver for the tower
      game_led_paint_led(rook_to_led, 0, 255, 0);       // Green for tower target
      game_led_frame_end();

      // DO NOT CHANGE PLAYERS for castling!
      ESP_LOGI(
          TAG, "⏳ Waiting for rook move from %c%d to %c%d",
          'a' + castling_state.rook_from_col, castling_state.rook_from_row + 1,
          'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
      ESP_LOGI(TAG, "🏰 Castling in progress - player remains %s",
               current_player == PLAYER_WHITE ? "White" : "Black");
      game_snapshot_persist_after_valid_move();
      return success; // Return success but don't change player - the player will change
                      // only after the casting is complete
#endif
    }

    // Check for promotion - start promotion animation (only if the API was not immediate)
    if (extended_move.move_type == MOVE_TYPE_PROMOTION) {
      const bool api_promo_inline =
          api_immediate_promo && api_promo_choice <= PROMOTION_KNIGHT;
      if (!api_promo_inline) {
        ESP_LOGI(TAG, "👑 PROMOTION DETECTED! Starting promotion animation...");

        // #2: Mutex protection for (*game_state_promotion_state())
        // Use a recursive mutex function ((*game_state_promotion_mutex()) is a recursive mutex)
        if (xSemaphoreTakeRecursive((*game_state_promotion_mutex()), pdMS_TO_TICKS(100)) ==
            pdTRUE) {
          // Set promotion state to pending
          // This blocks further moves and enables selection via buttons/UART/web
          (*game_state_promotion_state()).pending = true;
          (*game_state_promotion_state()).square_row = move->to_row;
          (*game_state_promotion_state()).square_col = move->to_col;
          (*game_state_promotion_state()).player = current_player;
          xSemaphoreGiveRecursive((*game_state_promotion_mutex()));

          ESP_LOGI(TAG, "⏸️  Promotion pending at %c%d for %s player",
                   'a' + move->to_col, move->to_row + 1,
                   current_player == PLAYER_WHITE ? "White" : "Black");
        } else {
          ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex!");
          return false;
        }

        // Start promotion animation
        game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_PROMOTE,
                             chess_pos_to_led_index(move->to_row, move->to_col),
                             0);

        // Update LED button indications (green for promotion buttons)
        game_check_promotion_needed();

        // CRITICAL: Return here WITHOUT changing player
        // Player will change AFTER promotion selection in
        // game_process_promotion_command()
        ESP_LOGI(
            TAG,
            "🏁 Promotion flow started - awaiting selection (buttons/UART/web)");
        game_snapshot_persist_after_valid_move();
        return success; // Don't change player - wait for promotion choice
      }
      ESP_LOGI(TAG,
               "👑 Inline API promotion — continuing (player switch below)");
    }

    // INSPECTION OF COMPLETION OF CASTING
    if (castling_state.in_progress) {
      // Is it a rook move to complete the cast?
      if (move->from_row == castling_state.rook_from_row &&
          move->from_col == castling_state.rook_from_col &&
          move->to_row == castling_state.rook_to_row &&
          move->to_col == castling_state.rook_to_col) {

        ESP_LOGI(TAG, "✅ CASTLING COMPLETED! Rook moved correctly");

        // We will move the tower in the board[][]
        game_set_piece(move->to_row, move->to_col, move->piece);
        game_set_piece(castling_state.rook_from_row,
                       castling_state.rook_from_col, PIECE_EMPTY);

        // Number of Castings for Learning Overview / API
        if (castling_state.player == PLAYER_WHITE) {
          (*game_state_white_castles())++;
        } else {
          (*game_state_black_castles())++;
        }

        // Update flags for king and rook
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

        // Golden cast completion animation
        show_castling_completion_animation();

        /* Always end cast state before player switch / end game check.
         * Previously, in_progress reset to zero only in the non-endgame branch — after checkmate/pat
         * remained true and validation continued to reject moves as "waiting for tower". */
        castling_state.in_progress = false;
        STAGING_LOGI(TAG,
                     "castling: completion cleared in_progress (before endgame "
                     "check)");

        // Publish already switched side when the king move applied the castle.
        player_t previous_player = current_player;
        if (current_player == castling_state.player) {
          current_player =
              (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        }

        ESP_LOGI(TAG, "🎉 Castling completed! Player changed to %s",
                 current_player == PLAYER_WHITE ? "White" : "Black");

        // Timer integration: End timer for previous player and start for new
        // player
        ESP_LOGI(TAG,
                 "🔄 Timer switch (castling): ending for %s, starting for %s",
                 previous_player == PLAYER_WHITE ? "White" : "Black",
                 current_player == PLAYER_WHITE ? "White" : "Black");
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);

        // Endgame check BEFORE player change animations!
        // If endgame, player change will NOT start
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;

          // Find the position of the winning king for the endgame animation
          player_t winner =
              (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
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
                   "🎯 Game finished after castling! Starting endgame "
                   "animation at position %d",
                   king_pos);

          // Start the endgame animation (wave from king winner)
          game_led_flush();
  game_event_post_game_end(king_pos, 0);

          ESP_LOGI(
              TAG,
              "✅ Endgame animation started - player change animation SKIPPED");
        } else {
          game_led_clear_board();
          game_policy_highlight_movable_if_enabled();

          // Check if the new player is in check
          bool in_check = game_is_king_in_check(current_player);
          if (in_check) {
            // Find the position of the king
            int king_row = -1, king_col = -1;
            piece_t king_piece = (current_player == PLAYER_WHITE)
                                     ? PIECE_WHITE_KING
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
                uint8_t king_led_index =
                    chess_pos_to_led_index(king_row, king_col);
                // Start the check animation - pink lighting on the king
                game_led_flush();
  game_event_post_check(king_led_index);
                ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                         current_player == PLAYER_WHITE ? "White" : "Black");
              }
            }
          }
        }

        // RETURN after completing the roll - the player has already changed
        game_snapshot_persist_after_valid_move();
        return success;
      } else {
        // Wrong move during castling
        // Check if user is aborting by moving King back or elsewhere
        if (move->piece == PIECE_WHITE_KING ||
            move->piece == PIECE_BLACK_KING) {
          ESP_LOGW(TAG, "🚫 Castling ABORTED by King move %c%d-%c%d",
                   'a' + move->from_col, move->from_row + 1, 'a' + move->to_col,
                   move->to_row + 1);

          // Abort castling state
          castling_state.in_progress = false;
          // Clear LEDs
          game_led_clear_board();
          // Treat as normal move (Undo usually)
          // We return true (success) so the move is accepted update the board
          // But we MUST ensure the player switches/unswitches correctly?
          // Actually, if they undo the move, they are just moving King back.
          // But 'game_execute_move' logic for NORMAL moves switches player.
          // If King G1->E1 (Undo), it switches to opponent?
          // Yes. That's fine. It's a move.

          // Wait, if it was G1->E1, logic board updates G1->E1.
          // Player switches.
          // Game continues.

          // We need to fall through to NORMAL move processing below?
          // No, we are inside the 'if (castling_state.in_progress)' block which
          // is BEFORE normal processing? Wait, the structure is:
          /*
             if (castling_state.in_progress) {
                 if (is_rook_move) { ... return success; }
                 else { ... return success; }
             }
          */
          // So if we abort, we should probably fall through to normal logic?
          // But 'castling_state.in_progress' block returns.

          // Let's just Execute the move here manually or modify logic to fall
          // through. The cleanest way: Set in_progress = false, then fall
          // through? But the block wraps the return.

          // I will Copy-Paste the 'Normal Move' logic essentially, or better:
          // Just fall through! I will remove the 'return success' and 'else'
          // structure slightly.

          // REWRITE:
          /*
             if (castling_state.in_progress) {
                 if (is_rook_move) {
                     ...
                     return success;
                 }
                 // Not rook move - Abort?
                 castling_state.in_progress = false;
                 ESP_LOGW(TAG, "Castling aborted");
                 game_led_clear_board();
                 // Fall through to normal processing!
             }
          */
        } else {
          ESP_LOGE(
              TAG,
              "❌ Wrong move during castling - expected rook from %c%d to %c%d",
              'a' + castling_state.rook_from_col,
              castling_state.rook_from_row + 1,
              'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
          // Do not change players - we are still waiting for the right turn of the tower
          game_snapshot_persist_after_valid_move();
          return success; // Return success but don't change player
        }
      }
    }

    // Fallthrough for Aborted Castling (treat as normal move)

    // NORMAL MOVES - change player (move animation starts in
    // game_process_drop_command) Switch player BEFORE checking
    // endgame conditions (according to the old project)
    player_t mover =
        game_is_white_piece(source_piece) ? PLAYER_WHITE : PLAYER_BLACK;
    player_t previous_player = mover;
    if (current_player == mover) {
      ESP_LOGI(TAG, "🔄 Changing player from %s to %s",
               previous_player == PLAYER_WHITE ? "White" : "Black",
               (previous_player == PLAYER_WHITE) ? "Black" : "White");
      current_player =
          (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    }

    ESP_LOGI(TAG,
             "✅ Move executed successfully. %s to move (current_player=%s)",
             (current_player == PLAYER_WHITE) ? "White" : "Black",
             current_player == PLAYER_WHITE ? "White" : "Black");

    /* Hall quiet moves never reach the UART/drop timer switch. */
    game_end_timer_move();
    game_start_timer_move(current_player == PLAYER_WHITE);

    // Check for endgame conditions after move execution (podle
    // old project)
    game_state_t end_game_result = game_check_end_game_conditions();
    if (end_game_result == GAME_STATE_FINISHED) {
      ESP_LOGI(TAG, "🎯 Endgame detected after move execution");
      current_game_state = GAME_STATE_FINISHED;
      game_active = false;
    }
  }

  // Update promotion LED indications after move
  if (success) {
    game_check_promotion_needed();
    game_snapshot_persist_after_valid_move();
    game_bump_revision_and_notify();
  }

  return success;
}

static bool s_ep_wait;
static uint8_t s_ep_row;
static uint8_t s_ep_col;

static void game_ep_capture_show(void) {
  game_led_clear_board();
  game_led_frame_begin(0, false);
  game_led_frame_sq(s_ep_row, s_ep_col, 255, 80, 0);
  game_led_frame_end();
  game_required_step_refresh();
}

void game_ep_capture_arm(uint8_t row, uint8_t col) {
  s_ep_row = row;
  s_ep_col = col;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  {
    uint8_t phys[64] = {0};
    uint8_t sq = (uint8_t)(row * 8 + col);
    squares_get_state(phys);
    /* The move is committed only after the victim is already off the square. */
    if (phys[sq] == 0) {
      s_ep_wait = false;
      return;
    }
  }
#endif
  s_ep_wait = true;
  game_ep_capture_show();
}

bool game_ep_capture_waiting(void) { return s_ep_wait; }

bool game_ep_capture_is_victim(uint8_t row, uint8_t col) {
  return s_ep_wait && row == s_ep_row && col == s_ep_col;
}

bool game_ep_capture_victim_square(uint8_t *row, uint8_t *col) {
  if (!s_ep_wait || row == NULL || col == NULL) {
    return false;
  }
  *row = s_ep_row;
  *col = s_ep_col;
  return true;
}

void game_ep_capture_reset(void) {
  s_ep_wait = false;
  s_ep_row = 0;
  s_ep_col = 0;
}

void game_ep_capture_service(void) {
  if (!s_ep_wait) {
    return;
  }
  uint8_t phys[64] = {0};
  squares_get_state(phys);
  uint8_t sq = (uint8_t)(s_ep_row * 8 + s_ep_col);
  bool in_hand = piece_lifted && lifted_piece_row == s_ep_row &&
                 lifted_piece_col == s_ep_col;
  if (phys[sq] == 0 && !in_hand) {
    s_ep_wait = false;
    if (current_game_state != GAME_STATE_FINISHED) {
      game_policy_refresh_idle_board_leds();
    }
    return;
  }
  game_ep_capture_show();
}

/**
 * @brief Execute an enhanced chess move on the board
 */
bool game_execute_move_enhanced(chess_move_extended_t *move) {
  position_promo_t promo = POS_PROMO_NONE;
  uint8_t ep_victim_row = en_passant_victim_row;
  uint8_t ep_victim_col = en_passant_victim_col;

  if (move == NULL)
    return false;

  if (move->move_type == MOVE_TYPE_PROMOTION && !(*game_state_promotion_state()).pending) {
    switch (move->promotion_piece) {
    case PROMOTION_ROOK:
      promo = POS_PROMO_R;
      break;
    case PROMOTION_BISHOP:
      promo = POS_PROMO_B;
      break;
    case PROMOTION_KNIGHT:
      promo = POS_PROMO_N;
      break;
    case PROMOTION_QUEEN:
    default:
      promo = POS_PROMO_Q;
      break;
    }
  }
  if (!game_logic_play(move->from_row, move->from_col, move->to_row, move->to_col,
                       promo)) {
    return false;
  }

  // Set last move type for special chess detection
  switch (move->move_type) {
  case MOVE_TYPE_EN_PASSANT:
    game_ep_capture_arm(ep_victim_row, ep_victim_col);
    (*game_state_last_move_type()) = LAST_MOVE_EN_PASSANT;
    ESP_LOGI(TAG, "⚔️ En passant move executed");
    break;

  case MOVE_TYPE_CASTLE_KING:
    // The tower does not move automatically - the player must move it himself
    // The tower remains in its original position, the animation will force the player to move it
    (*game_state_last_move_type()) = LAST_MOVE_CASTLING; // Mark as castling
    ESP_LOGI(TAG, "🏰 Kingside castling - rook stays in place, waiting for "
                  "player to move it");
    break;

  case MOVE_TYPE_CASTLE_QUEEN:
    // The tower does not move automatically - the player must move it himself
    // The tower remains in its original position, the animation will force the player to move it
    (*game_state_last_move_type()) = LAST_MOVE_CASTLING; // Mark as castling
    ESP_LOGI(TAG, "🏰 Queenside castling - rook stays in place, waiting for "
                  "player to move it");
    break;

  case MOVE_TYPE_PROMOTION:
    // Handle promotion - the piece will be set below
    (*game_state_last_move_type()) = LAST_MOVE_PROMOTION; // Mark as promotion
    break;

  default:
    // Normal move or capture - nothing special needed
    (*game_state_last_move_type()) = LAST_MOVE_NORMAL; // Reset to normal move
    break;
  }

  if (move->move_type == MOVE_TYPE_PROMOTION && (*game_state_promotion_state()).pending) {
    /* Hold the promoting player until the piece is chosen. */
    current_player = (*game_state_promotion_state()).player;
    game_logic_position()->side = (uint8_t)current_player;
    ESP_LOGI(TAG,
             "⏸️  Promotion pending - keeping pawn at %c%d, waiting for user "
             "selection",
             'a' + move->to_col, move->to_row + 1);
  } else if (move->move_type == MOVE_TYPE_PROMOTION) {
    ESP_LOGI(TAG, "✅ Promotion executed immediately with piece %d",
             (int)move->promotion_piece);
  }

  // Squares the repetition hash reads. The rights themselves come from publish.
  if (move->piece == PIECE_WHITE_KING) {
    white_king_moved = true;
    piece_moved[0][4] = true;
  }
  if (move->piece == PIECE_BLACK_KING) {
    black_king_moved = true;
    piece_moved[7][4] = true;
  }
  if (move->piece == PIECE_WHITE_ROOK) {
    if (move->from_col == 0) {
      white_rook_a_moved = true;
      piece_moved[0][0] = true;
    }
    if (move->from_col == 7) {
      white_rook_h_moved = true;
      piece_moved[0][7] = true;
    }
  }
  if (move->piece == PIECE_BLACK_ROOK) {
    if (move->from_col == 0) {
      black_rook_a_moved = true;
      piece_moved[7][0] = true;
    }
    if (move->from_col == 7) {
      black_rook_h_moved = true;
      piece_moved[7][7] = true;
    }
  }

  /* Count the mover. sync already stored the side to move next. */
  if (game_is_white_piece(move->piece)) {
    (*game_state_white_moves_count())++;
  } else if (game_is_black_piece(move->piece)) {
    (*game_state_black_moves_count())++;
  }

  // DO NOT CHANGE PLAYER HERE - player is changed in game_execute_move() after
  // turn completion Switch players - REMOVED: player is switched in
  // game_execute_move() after move execution

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (move->move_type != MOVE_TYPE_PROMOTION) {
    piece_t landed = game_get_piece(move->to_row, move->to_col);
    hall_piece_type_t ht = HALL_PIECE_TYPE_NONE;
    hall_piece_color_t hc = HALL_PIECE_COLOR_NONE;
    if (game_hall_engine_to_sensor(landed, &ht, &hc)) {
      hall_i2c_squares_note_confirmed(
          (uint8_t)(move->to_row * 8 + move->to_col), ht, hc);
    }
  }
  /* Pawn still on the last rank is not a committed position. */
  if (!(*game_state_promotion_state()).pending) {
    game_hall_snapshot_last_valid();
  }
#endif

  return true;
}

/**
 * @brief Show castling completion animation
 */
void show_castling_completion_animation() {
  // Golden animation of a successful roll
  uint8_t rook_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                            castling_state.rook_to_col);

  for (int i = 0; i < 5; i++) {
    game_led_frame_begin(0, false);
    game_led_paint_led(rook_led, 255, 215, 0); // Golden
    game_led_frame_end();
    vTaskDelay(pdMS_TO_TICKS(100));
    game_led_frame_begin(0, false);
    game_led_frame_end();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG, "🏰✨ CASTLING ANIMATION COMPLETED");
}
