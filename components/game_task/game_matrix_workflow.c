/**
 * @file game_matrix_workflow.c
 * @brief Matrix-driven LED workflow, promotion command, movable highlights.
 */

#include "game_board_transition.h"
#include "game_physical_undo.h"
#include "game_task.h"
#include "game_led_direct.h"
#include "game_events.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_policy.h"
#include "game_move_validate.h"
#include "game_hall_match.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_position.h"
#endif
#include "../squares_task/include/hall_i2c_squares.h"
#include "../squares_task/include/squares_task.h"

#include "../led_task/include/led_task.h"
#include "../led_task/include/led_gesture_anim.h"
#include "led_mapping.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "GAME_MATRIX";

/* Lift/place/move handlers had no callers.
 * Archived in kos/unused/components/game_task/matrix_event_handlers.c.
 */


/**
 * @brief Process promotion command from game_submit
 * @param cmd Promotion command structure
 */
void game_process_promotion_command(const game_cmd_t *cmd) {
  if (!cmd) {
    ESP_LOGE(TAG, "❌ Invalid promotion command");
    return;
  }

  ESP_LOGI(TAG, "👑 Processing promotion command");

  // #2 & #3: Atomic check with mutex to prevent double execution
  // Use a recursive mutex function ((*game_state_promotion_mutex()) is a recursive mutex)
  if (xSemaphoreTakeRecursive((*game_state_promotion_mutex()), pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex");
    return;
  }

  // Check if promotion is still pending (might have been processed by another
  // task)
  if (!(*game_state_promotion_state()).pending) {
    xSemaphoreGiveRecursive((*game_state_promotion_mutex()));
    ESP_LOGW(TAG, "⚠️ Promotion already processed by another task");
    return;
  }

  // CRITICAL: Keep mutex held during ENTIRE execution to prevent race!
  // DO NOT release here - release only after completion or on error

  // Determine promotion choice (default to Queen if invalid or not specified)
  promotion_choice_t choice = PROMOTION_QUEEN;

  {
    char piece = (cmd->kind == GCMD_PROMOTION_CHOICE) ? cmd->u.promo.piece : 0;
    if (piece == 'r' || piece == 'R') {
      choice = PROMOTION_ROOK;
    } else if (piece == 'b' || piece == 'B') {
      choice = PROMOTION_BISHOP;
    } else if (piece == 'n' || piece == 'N') {
      choice = PROMOTION_KNIGHT;
    } else {
      choice = PROMOTION_QUEEN;
      if (piece != 0 && piece != 'q' && piece != 'Q') {
        ESP_LOGW(TAG, "Invalid promotion piece '%c', defaulting to Queen",
                 piece);
      }
    }
    ESP_LOGI(TAG, "Promotion choice received: %d (%s)", choice,
             choice == PROMOTION_QUEEN    ? "Queen"
             : choice == PROMOTION_ROOK   ? "Rook"
             : choice == PROMOTION_BISHOP ? "Bishop"
                                          : "Knight");
  }

  // Execute the promotion with the selected piece
  if (game_execute_promotion(choice)) {
    ESP_LOGI(TAG, "✅ Promotion executed successfully");

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    {
      uint8_t promo_row = (*game_state_promotion_state()).square_row;
      uint8_t promo_col = (*game_state_promotion_state()).square_col;
      piece_t promoted_on_board =
          game_board_piece_at(promo_row, promo_col);
      if (!game_hall_validate_drop_square(promo_row, promo_col,
                                          promoted_on_board)) {
        (void)game_board_undo_promotion_choice();
        ESP_LOGW(TAG,
                 "❌ Hall type/color mismatch on promo square %c%d after choice",
                 'a' + promo_col, promo_row + 1);
        game_cmd_reply(
            "❌ Wrong piece on promotion square (Hall type/color) — fix "
            "physical piece and choose promotion again",
            true, (QueueHandle_t)cmd->response_queue);
        xSemaphoreGiveRecursive((*game_state_promotion_mutex()));
        return;
      }
    }
#endif

    (*game_state_promotion_state()).pending = false;
    game_move_undo_meta_on_promotion_completed(game_board_piece_at(
        (*game_state_promotion_state()).square_row,
        (*game_state_promotion_state()).square_col));
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    game_hall_snapshot_last_valid();
    game_hall_position_arm_idle_handover();
#endif
    game_print_board();
    game_promotion_finish_turn();

  } else {
    ESP_LOGE(TAG, "❌ Failed to execute promotion");
  }

  // CRITICAL: Release mutex at the end of the function to ensure it's always
  // released Use recursive mutex functions ((*game_state_promotion_mutex()) is a recursive mutex)
  xSemaphoreGiveRecursive((*game_state_promotion_mutex()));
}

/**
 * @brief Performs graduation pesce on the selected figure
 *
 * @param choice Choose figures for graduation (PROMOTION_QUEEN, PROMOTION_ROOK,
 * PROMOTION_BISHOP, PROMOTION_KNIGHT)
 * @return true if the graduation was successful, false if it failed
 *
 * @details
 * The function searches for a fish on the square board and raises it to the selected piece:
 * - They were pesac on the 8th row (row 7) -> promoted
 * - Black sand on the 1st row (row 0) -> promotion
 *
 * Process:
 * 1. Conversion of choice to the type of figure according to the color of the toy
 * 2. Search the fish plate on the power board
 * 3. Replaced pesce with a raised figure
 *
 * @note CRITICAL FIX in v2.4.1:
 * - BUG #10: Row indexing was REVERSED!
 * - Original: WHITE row==0, BLACK row==7 (SLEEP!)
 * - Fixed: WHITE row==7 (8th row), BLACK row==0 (1st row)
 * - Graduation now works 100% correctly
 */
bool game_execute_promotion(promotion_choice_t choice) {
  ESP_LOGI(TAG, "👑 Executing pawn promotion: %d", choice);

  // Find the pawn that needs to be promoted
  // This should be set when a pawn reaches the promotion rank
  // For now, we'll implement a basic version

  // Convert promotion choice to piece type
  piece_t promoted_piece;
  // Use (*game_state_promotion_state()).player instead of current_player
  // current_player has already been switched to the opponent by
  // game_execute_move() so we must use the player stored in the promotion
  // state
  if ((*game_state_promotion_state()).player == PLAYER_WHITE) {
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
  // The position was already found and stored in (*game_state_promotion_state()) by
  // game_check_promotion_needed()
  uint8_t row = (*game_state_promotion_state()).square_row;
  uint8_t col = (*game_state_promotion_state()).square_col;
  piece_t piece = game_get_piece(row, col);

  // Verify it's actually a pawn (safety check)
  if (piece != PIECE_WHITE_PAWN && piece != PIECE_BLACK_PAWN) {
    ESP_LOGE(TAG, "❌ Promotion failed: No pawn at stored position %c%d",
             'a' + col, row + 1);
    return false;
  }

  char promo_ch = 'q';
  switch (choice) {
  case PROMOTION_ROOK:
    promo_ch = 'r';
    break;
  case PROMOTION_BISHOP:
    promo_ch = 'b';
    break;
  case PROMOTION_KNIGHT:
    promo_ch = 'n';
    break;
  default:
    promo_ch = 'q';
    break;
  }
  if (!game_board_apply_promotion_piece(promo_ch)) {
    return false;
  }
  game_move_undo_meta_on_promotion_completed(promoted_piece);
  current_game_state = GAME_STATE_ACTIVE; // Restore game state to ACTIVE
  ESP_LOGI(TAG, "✅ Promoted %s pawn at %c%d to %s",
           current_player == PLAYER_WHITE ? "white" : "black", 'a' + col,
           row + 1,
           choice == PROMOTION_QUEEN    ? "Queen"
           : choice == PROMOTION_ROOK   ? "Rook"
           : choice == PROMOTION_BISHOP ? "Bishop"
                                        : "Knight");

  // Start promotion animation
  uint8_t promotion_led = chess_pos_to_led_index(row, col);
  game_led_flush();
  game_event_post_board_anim(GAME_EVT_ANIM_PROMOTE, promotion_led, 0);

  return true;
}

/**
 * @brief Highlight all movable pieces for current player
 */
void game_highlight_movable_pieces(void) {
#if HALL_DEBUG_LIGHT_WHERE_PIECE
  return;
#endif
  if (game_led_lamp_held() || game_led_gesture_busy()) {
    return;
  }
  if (!game_policy_move_hints_movable_yellow()) {
    return;
  }
  // Do not highlight pieces if system is booting
  if (game_led_boot_active()) {
    return;
  }

  // Do not highlight movable pieces if a piece is already
  // lifted This prevents overwriting the "valid moves" (green) with "movable
  // pieces" (yellow)
  if (piece_lifted) {
    ESP_LOGD(TAG, "Highlight skipped - piece is lifted");
    return;
  }

  // Do not run the highlight during casting - casting has its own LED indication
  if (castling_state.in_progress) {
    ESP_LOGD(TAG, "Highlight skipped - castling in progress");
    return;
  }

  if (!game_led_guidance_show_movable_yellow()) {
    ESP_LOGD(TAG, "Movable piece highlights skipped (LED guidance level %u)",
             (unsigned)game_get_led_guidance_level());
    return;
  }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* Logical pieces must not light an empty or not-yet-locked Hall board. */
  if (!hall_i2c_squares_readings_usable()) {
    return;
  }
  {
    uint8_t occ[64] = {0};
    bool any_piece = false;
    squares_get_state(occ);
    for (int i = 0; i < 64; i++) {
      if (occ[i] != 0) {
        any_piece = true;
        break;
      }
    }
    if (!any_piece) {
      return;
    }
  }
#endif

  ESP_LOGI(TAG, "🟡 Highlighting movable pieces for %s player",
           current_player == PLAYER_WHITE ? "white" : "black");
  const bool nested_frame = game_led_frame_is_open();
  if (!nested_frame) {
    game_led_frame_begin(0, false);
  }

  // Endgame detection is handled in game_analyze_position() called from
  // game_execute_move()

  // LED command queue removed - using direct LED calls now

  uint32_t highlighted_count = 0;

  // Scan all squares for current player's pieces
  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      piece_t piece = game_get_piece(row, col);

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
          game_led_frame_sq(row, col, 255, 255, 0);
          highlighted_count++;
        }
      }
    }
  }

  if (!nested_frame) {
    game_led_frame_end();
  }
  ESP_LOGI(TAG, "🟡 Highlighted %lu movable pieces", highlighted_count);
}
