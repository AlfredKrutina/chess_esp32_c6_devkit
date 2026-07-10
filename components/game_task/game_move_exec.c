/**
 * @file game_move_exec.c
 * @brief Move execution, position analysis, and castling completion animation.
 */

#include "game_move_exec.h"
#include "game_move_validate.h"
#include "game_snapshot.h"
#include "game_task_internal.h"

#include "game_task.h"
#include "chess_gameplay_policy.h"
#include "game_led_animations.h"
#include "led_mapping.h"

#include "../led_task/include/led_task.h"

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

// Fifty-move rule counter
static uint32_t fifty_move_counter = 0;

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
      game_result = GAME_STATE_FINISHED;
      game_result_type_t result_type =
          player == PLAYER_WHITE ? RESULT_BLACK_WINS : RESULT_WHITE_WINS;
      current_result_type = result_type; // Uložit pro web API
      game_update_endgame_statistics(result_type);
      game_print_endgame_report_uart(result_type);
      ESP_LOGI(TAG, "🎯 CHECKMATE! %s wins!",
               player == PLAYER_WHITE ? "Black" : "White");
      return GAME_STATE_FINISHED;
    } else {
      // Stalemate
      game_result = GAME_STATE_FINISHED;
      current_result_type = RESULT_DRAW_STALEMATE; // Uložit pro web API
      game_update_endgame_statistics(RESULT_DRAW_STALEMATE);
      game_print_endgame_report_uart(RESULT_DRAW_STALEMATE);
      ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn!");
      return GAME_STATE_FINISHED;
    }
  }

  // Check for fifty-move rule
  if (fifty_move_counter >= 100) { // 50 moves per side
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_50_MOVE; // Uložit pro web API
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
      piece_t piece = board[row][col];
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
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_INSUFFICIENT; // Uložit pro web API
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

  const bool api_immediate_promo = s_uart_move_immediate_promotion;
  const promotion_choice_t api_promo_choice = s_uart_move_immediate_promotion_piece;
  s_uart_move_immediate_promotion = false;

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
  if (!error_recovery_state.waiting_for_move_correction) {
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
        // OKAMŽITĚ nastavit promotion_state před voláním
        // game_execute_move_enhanced() Toto zajistí, že
        // game_execute_move_enhanced() neprovádí auto-promoci Použít recursive
        // mutex funkce (promotion_mutex je recursive mutex)
        if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
            pdTRUE) {
          promotion_state.pending = true;
          promotion_state.square_row = move->to_row;
          promotion_state.square_col = move->to_col;
          promotion_state.player = current_player;
          xSemaphoreGiveRecursive(promotion_mutex);

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

  // Execute the enhanced move
  bool success = game_execute_move_enhanced(&extended_move);

  ESP_LOGI(TAG,
           "🔍 game_execute_move: success=%d, current_player before change=%s",
           success, current_player == PLAYER_WHITE ? "White" : "Black");

  if (success) {
    // Update last move tracking for future en passant detection
    last_move_from_row = move->from_row;
    last_move_from_col = move->from_col;
    last_move_to_row = move->to_row;
    last_move_to_col = move->to_col;
    has_last_move = true;

    // Rošáda = jeden tah v historii (král už byl přidán; tah věže nepřidávat)
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
      }

      // Update game state
      move_count++;
      last_move_time = esp_timer_get_time() / 1000;

      // Record material advantage pro graf
      game_record_material_advantage();
    } else {
      last_move_time = esp_timer_get_time() / 1000;
    }

    // ENHANCED CASTLING DETECTION
    if (extended_move.move_type == MOVE_TYPE_CASTLE_KING ||
        extended_move.move_type == MOVE_TYPE_CASTLE_QUEEN) {
      ESP_LOGI(TAG, "🏰 CASTLING DETECTED! King moved 2 squares");

      // Nastavit castling state pro tracking
      castling_state.in_progress = true;
      castling_state.player = current_player;
      castling_state.is_kingside = (move->to_col > move->from_col);

      // Vypočítat očekávané pozice věže
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

      // Ukázat LED indikaci pro věž s pulzováním pro lepší viditelnost
      led_clear_board_only();

      uint8_t rook_from_led = chess_pos_to_led_index(
          castling_state.rook_from_row, castling_state.rook_from_col);
      uint8_t rook_to_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                                   castling_state.rook_to_col);

      // Pulzování pro lepší viditelnost (3 cykly s plynulým přechodem)
      // Použít správný výpočet brightness pro plynulé pulzování
      for (int pulse = 0; pulse < 3; pulse++) {
        // Plynulé pulzování: 0.5 -> 1.0 -> 0.5
        // Použít sin() s normalizací - sin() vrací -1 až 1, normalizujeme na
        // 0-1, pak na 0.5-1.0
        float phase = (float)pulse * 2.0f * 3.14159f / 3.0f; // 0, 2π/3, 4π/3
        float brightness =
            0.5f + 0.5f * (1.0f + sin(phase)) /
                       2.0f; // Normalizace: sin() -> 0-1 -> 0.5-1.0

        // Stříbrná pro věž (source) s pulzováním
        led_set_pixel_safe(rook_from_led, (uint8_t)(192 * brightness),
                           (uint8_t)(192 * brightness),
                           (uint8_t)(192 * brightness));

        // Zelená pro cíl věže (destination) s pulzováním
        led_set_pixel_safe(rook_to_led, 0, (uint8_t)(255 * brightness), 0);

        vTaskDelay(pdMS_TO_TICKS(200));
      }

      // Finální statické zobrazení
      led_set_pixel_safe(rook_from_led, 192, 192, 192); // Stříbrná pro věž
      led_set_pixel_safe(rook_to_led, 0, 255, 0);       // Zelená pro cíl věže

      // NEMĚNIT HRÁČE pro castling!
      ESP_LOGI(
          TAG, "⏳ Waiting for rook move from %c%d to %c%d",
          'a' + castling_state.rook_from_col, castling_state.rook_from_row + 1,
          'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
      ESP_LOGI(TAG, "🏰 Castling in progress - player remains %s",
               current_player == PLAYER_WHITE ? "White" : "Black");
      game_snapshot_persist_after_valid_move();
      return success; // Return success but don't change player - hráč se změní
                      // až po dokončení rošády
    }

    // Check for promotion - start promotion animation (jen když nebyla API okamžitá)
    if (extended_move.move_type == MOVE_TYPE_PROMOTION) {
      const bool api_promo_inline =
          api_immediate_promo && api_promo_choice <= PROMOTION_KNIGHT;
      if (!api_promo_inline) {
        ESP_LOGI(TAG, "👑 PROMOTION DETECTED! Starting promotion animation...");

        // #2: Mutex protection for promotion_state
        // Použít recursive mutex funkce (promotion_mutex je recursive mutex)
        if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
            pdTRUE) {
          // Set promotion state to pending
          // This blocks further moves and enables selection via buttons/UART/web
          promotion_state.pending = true;
          promotion_state.square_row = move->to_row;
          promotion_state.square_col = move->to_col;
          promotion_state.player = current_player;
          xSemaphoreGiveRecursive(promotion_mutex);

          ESP_LOGI(TAG, "⏸️  Promotion pending at %c%d for %s player",
                   'a' + move->to_col, move->to_row + 1,
                   current_player == PLAYER_WHITE ? "White" : "Black");
        } else {
          ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex!");
          return false;
        }

        // Start promotion animation
        led_command_t promote_cmd = {
            .type = LED_CMD_ANIM_PROMOTE,
            .led_index = chess_pos_to_led_index(move->to_row, move->to_col),
            .red = 255,
            .green = 215,
            .blue = 0, // Gold
            .duration_ms = 2000,
            .data = NULL};
        led_execute_command_new(&promote_cmd);

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

    // KONTROLA DOKONČENÍ ROŠÁDY
    if (castling_state.in_progress) {
      // Je to tah věže pro dokončení rošády?
      if (move->from_row == castling_state.rook_from_row &&
          move->from_col == castling_state.rook_from_col &&
          move->to_row == castling_state.rook_to_row &&
          move->to_col == castling_state.rook_to_col) {

        ESP_LOGI(TAG, "✅ CASTLING COMPLETED! Rook moved correctly");

        // Provedeme posun věže v board[][]
        board[move->to_row][move->to_col] = move->piece;
        board[castling_state.rook_from_row][castling_state.rook_from_col] =
            PIECE_EMPTY;

        // Počet rošád pro výukový přehled / API
        if (castling_state.player == PLAYER_WHITE) {
          white_castles++;
        } else {
          black_castles++;
        }

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

        /* Vždy ukončit stav rošády před přepnutím hráče / kontrolou konce hry.
         * Dříve se in_progress vynulovalo jen v ne-endgame větvi — po matu/patu
         * zůstalo true a validace dál odmítala tahy jako „čeká se na věž“. */
        castling_state.in_progress = false;
        STAGING_LOGI(TAG,
                     "castling: completion cleared in_progress (before endgame "
                     "check)");

        // TEPRVE NYÍ změnit hráče po dokončení rošády
        player_t previous_player = current_player;
        current_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

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

        // Endgame kontrola PŘED player change animací!
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
                   "🎯 Game finished after castling! Starting endgame "
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

          // Zobrazit pohyblivé figury pro nového hráče
          led_clear_board_only();
          chess_policy_highlight_movable_if_enabled();

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
                uint8_t king_led_index =
                    chess_pos_to_led_index(king_row, king_col);
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

        // RETURN po dokončení rošády - hráč se už změnil
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
          led_clear_board_only();
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
                 led_clear_board_only();
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
          // Nezměnit hráče - stále čekáme na správný tah věže
          game_snapshot_persist_after_valid_move();
          return success; // Return success but don't change player
        }
      }
    }

    // Fallthrough for Aborted Castling (treat as normal move)

    // NORMÁLNÍ TAHY - změnit hráče (move animace se spouští v
    // game_process_drop_command) Switch player BEFORE checking
    // endgame conditions (podle starého projektu)
    player_t previous_player = current_player;
    ESP_LOGI(TAG, "🔄 Changing player from %s to %s",
             previous_player == PLAYER_WHITE ? "White" : "Black",
             (previous_player == PLAYER_WHITE) ? "Black" : "White");
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

    ESP_LOGI(TAG,
             "✅ Move executed successfully. %s to move (current_player=%s)",
             (current_player == PLAYER_WHITE) ? "White" : "Black",
             current_player == PLAYER_WHITE ? "White" : "Black");

    // Check for endgame conditions after move execution (podle
    // starého projektu)
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

/**
 * Převod PROMOTION_* na piece_t podle barvy pěšce.
 * POZOR: piece_t není seřazené Q,R,B,N za sebou — PIECE_WHITE_QUEEN+1 je KRAL,
 * ne věž (viz chess_types.h). Dříve „PIECE_WHITE_QUEEN + choice“ dávalo při
 * volbě věže druhého bílého krále na šachovnici.
 */
static piece_t game_piece_for_promotion_choice(piece_t pawn,
                                                promotion_choice_t choice) {
  if (pawn == PIECE_WHITE_PAWN) {
    switch (choice) {
    case PROMOTION_ROOK:
      return PIECE_WHITE_ROOK;
    case PROMOTION_BISHOP:
      return PIECE_WHITE_BISHOP;
    case PROMOTION_KNIGHT:
      return PIECE_WHITE_KNIGHT;
    case PROMOTION_QUEEN:
    default:
      return PIECE_WHITE_QUEEN;
    }
  }
  if (pawn == PIECE_BLACK_PAWN) {
    switch (choice) {
    case PROMOTION_ROOK:
      return PIECE_BLACK_ROOK;
    case PROMOTION_BISHOP:
      return PIECE_BLACK_BISHOP;
    case PROMOTION_KNIGHT:
      return PIECE_BLACK_KNIGHT;
    case PROMOTION_QUEEN:
    default:
      return PIECE_BLACK_QUEEN;
    }
  }
  ESP_LOGW(TAG,
           "game_piece_for_promotion_choice: očekáván pesec, mám piece=%d — "
           "vracím bílou damu",
           (int)pawn);
  return PIECE_WHITE_QUEEN;
}

/**
 * @brief Execute an enhanced chess move on the board
 */
bool game_execute_move_enhanced(chess_move_extended_t *move) {
  if (move == NULL)
    return false;

  // Nastavit typ posledního tahu pro detekci speciálních šachmatů
  switch (move->move_type) {
  case MOVE_TYPE_EN_PASSANT:
    // Remove the captured pawn
    board[en_passant_victim_row][en_passant_victim_col] = PIECE_EMPTY;
    last_move_type = LAST_MOVE_EN_PASSANT; // Označit jako en passant
    ESP_LOGI(TAG, "⚔️ En passant move executed");
    break;

  case MOVE_TYPE_CASTLE_KING:
    // Věž se nepřesunuje automaticky - hráč ji musí přesunout sám
    // Věž zůstává na původní pozici, animace donutí hráče ji přesunout
    last_move_type = LAST_MOVE_CASTLING; // Označit jako castling
    ESP_LOGI(TAG, "🏰 Kingside castling - rook stays in place, waiting for "
                  "player to move it");
    break;

  case MOVE_TYPE_CASTLE_QUEEN:
    // Věž se nepřesunuje automaticky - hráč ji musí přesunout sám
    // Věž zůstává na původní pozici, animace donutí hráče ji přesunout
    last_move_type = LAST_MOVE_CASTLING; // Označit jako castling
    ESP_LOGI(TAG, "🏰 Queenside castling - rook stays in place, waiting for "
                  "player to move it");
    break;

  case MOVE_TYPE_PROMOTION:
    // Handle promotion - the piece will be set below
    last_move_type = LAST_MOVE_PROMOTION; // Označit jako promotion
    break;

  default:
    // Normal move or capture - nothing special needed
    last_move_type = LAST_MOVE_NORMAL; // Resetovat na normální tah
    break;
  }

  // Make the basic move
  board[move->to_row][move->to_col] = move->piece;
  board[move->from_row][move->from_col] = PIECE_EMPTY;

  // Handle promotion
  if (move->move_type == MOVE_TYPE_PROMOTION) {
    // Zkontrolovat zda je promotion_state.pending
    // Pokud ano, pak uživatel ještě nevybral figurku - ponechat pěšce na místě
    // Pokud ne, pak promotion_piece je nastaveno a můžeme provést promoci
    if (promotion_state.pending) {
      // Promotion je pending - uživatel ještě nevybral figurku
      // Ponechat pěšce na promotion rank (už je tam přesunutý v předchozím
      // kroku)
      ESP_LOGI(TAG,
               "⏸️  Promotion pending - keeping pawn at %c%d, waiting for user "
               "selection",
               'a' + move->to_col, move->to_row + 1);
    } else {
      // Promotion není pending - promotion_piece je nastaveno, provést promoci
      piece_t promoted_piece =
          game_piece_for_promotion_choice(move->piece, move->promotion_piece);
      board[move->to_row][move->to_col] = promoted_piece;
      ESP_LOGI(TAG, "✅ Promotion executed immediately with piece %d",
               (int)move->promotion_piece);
    }
  }

  // Update castling flags
  if (move->piece == PIECE_WHITE_KING)
    white_king_moved = true;
  if (move->piece == PIECE_BLACK_KING)
    black_king_moved = true;
  if (move->piece == PIECE_WHITE_ROOK) {
    if (move->from_col == 0)
      white_rook_a_moved = true;
    if (move->from_col == 7)
      white_rook_h_moved = true;
  }
  if (move->piece == PIECE_BLACK_ROOK) {
    if (move->from_col == 0)
      black_rook_a_moved = true;
    if (move->from_col == 7)
      black_rook_h_moved = true;
  }

  // Update en passant state
  en_passant_available = false;
  if ((move->piece == PIECE_WHITE_PAWN || move->piece == PIECE_BLACK_PAWN) &&
      abs(move->to_row - move->from_row) == 2) {
    // Pawn made a double move - enable en passant
    en_passant_available = true;
    en_passant_target_row = (move->from_row + move->to_row) / 2;
    en_passant_target_col = move->from_col;
    en_passant_victim_row = move->to_row;
    en_passant_victim_col = move->to_col;
  }

  // Update fifty-move counter
  if (move->piece == PIECE_WHITE_PAWN || move->piece == PIECE_BLACK_PAWN ||
      move->captured_piece != PIECE_EMPTY) {
    fifty_move_counter = 0;
  } else {
    fifty_move_counter++;
  }

  // Update move counters and statistics
  if (current_player == PLAYER_WHITE) {
    white_moves_count++;
  } else {
    black_moves_count++;
  }

  // NEMĚNIT HRÁČE ZDE - hráč se mění v game_execute_move() po
  // dokončení tahu Switch players - REMOVED: player is switched in
  // game_execute_move() after move execution

  return true;
}

/**
 * @brief Show castling completion animation
 */
void show_castling_completion_animation() {
  // Zlatá animace úspěšné rošády
  uint8_t rook_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                            castling_state.rook_to_col);

  for (int i = 0; i < 5; i++) {
    led_set_pixel_safe(rook_led, 255, 215, 0); // Zlatá
    vTaskDelay(pdMS_TO_TICKS(100));
    led_set_pixel_safe(rook_led, 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  ESP_LOGI(TAG, "🏰✨ CASTLING ANIMATION COMPLETED");
}
