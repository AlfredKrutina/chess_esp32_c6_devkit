/**
 * @file game_promotion.c
 * @brief Promotion anchor LED, detection, and physical button handling.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_snapshot.h"
#include "game_physical_undo.h"
#include "chess_gameplay_policy.h"
#include "game_hall_match.h"
#include "game_squares_guard.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_restore_guide.h"
#endif

#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <math.h>
#include <stdio.h>

static const char *TAG = "GAME_PROMOTION";

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
static bool s_promo_hall_swap_blink;
static piece_t s_promo_hall_seen = PIECE_EMPTY;
static uint8_t s_promo_hall_seen_count;

static bool game_promotion_is_final_piece(piece_t piece) {
  return piece == PIECE_WHITE_QUEEN || piece == PIECE_WHITE_ROOK ||
         piece == PIECE_WHITE_BISHOP || piece == PIECE_WHITE_KNIGHT ||
         piece == PIECE_BLACK_QUEEN || piece == PIECE_BLACK_ROOK ||
         piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_KNIGHT;
}
#endif

void game_promotion_finish_turn(void) {
  current_game_state = GAME_STATE_ACTIVE;

  player_t previous_player = current_player;
  current_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  ESP_LOGI(TAG, "🔄 Timer switch (promotion): ending for %s, starting for %s",
           previous_player == PLAYER_WHITE ? "White" : "Black",
           current_player == PLAYER_WHITE ? "White" : "Black");
  game_end_timer_move();
  game_start_timer_move(current_player == PLAYER_WHITE);

  game_state_t end_game_result = game_check_end_game_conditions();
  if (end_game_result == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_FINISHED;
    game_active = false;

    player_t winner =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    uint8_t king_pos = 28;
    for (int i = 0; i < 64; i++) {
      piece_t piece = board[i / 8][i % 8];
      if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
          (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
        king_pos = (uint8_t)i;
        break;
      }
    }

    led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                 .led_index = king_pos,
                                 .red = 255,
                                 .green = 255,
                                 .blue = 0,
                                 .duration_ms = 0,
                                 .data = NULL};
    led_execute_command_new(&endgame_cmd);
  } else {
    uint8_t player_color = (current_player == PLAYER_WHITE) ? 1 : 0;
    led_command_t player_change_cmd = {.type = LED_CMD_ANIM_PLAYER_CHANGE,
                                       .led_index = 0,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms = 0,
                                       .data = &player_color};
    led_execute_command_new(&player_change_cmd);

    game_check_promotion_needed();
    chess_policy_highlight_movable_if_enabled();
    led_force_immediate_update();
  }

  bool in_check = game_is_king_in_check(current_player);
  if (in_check) {
    int king_row = -1, king_col = -1;
    piece_t king_piece = (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING
                                                          : PIECE_BLACK_KING;
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 8; col++) {
        if (board[row][col] == king_piece) {
          king_row = row;
          king_col = col;
          break;
        }
      }
      if (king_row != -1) {
        break;
      }
    }

    if (king_row != -1 && king_col != -1 &&
        game_led_guidance_show_check_anim()) {
      uint8_t king_led_index = chess_pos_to_led_index((uint8_t)king_row,
                                                      (uint8_t)king_col);
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
  game_snapshot_persist_after_valid_move();
}

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
void game_promotion_sync_hall_if_pending(void) {
  if (!promotion_state.pending) {
    s_promo_hall_swap_blink = false;
    s_promo_hall_seen = PIECE_EMPTY;
    s_promo_hall_seen_count = 0;
    return;
  }

  if (xSemaphoreTakeRecursive(promotion_mutex, 0) != pdTRUE) {
    return;
  }

  uint8_t row = promotion_state.square_row;
  uint8_t col = promotion_state.square_col;
  uint8_t sq = (uint8_t)(row * 8 + col);
  uint8_t phys[64] = {0};
  squares_get_state(phys);

  hall_piece_type_t ht = squares_get_hall_piece_type(sq);
  hall_piece_color_t hc = squares_get_hall_piece_color(sq);

  if (phys[sq] == 0 || ht == HALL_PIECE_TYPE_NONE) {
    s_promo_hall_seen = PIECE_EMPTY;
    s_promo_hall_seen_count = 0;
    if (s_promo_hall_swap_blink) {
      game_stop_error_blink();
      s_promo_hall_swap_blink = false;
    }
    xSemaphoreGiveRecursive(promotion_mutex);
    return;
  }

  piece_t mapped = PIECE_EMPTY;
  if (!game_hall_piece_for_promoting_player(ht, hc, promotion_state.player,
                                            &mapped)) {
    s_promo_hall_seen = PIECE_EMPTY;
    s_promo_hall_seen_count = 0;
    if (!s_promo_hall_swap_blink) {
      game_show_invalid_move_error_with_blink(row, col);
      s_promo_hall_swap_blink = true;
    }
    xSemaphoreGiveRecursive(promotion_mutex);
    return;
  }

  if (!game_promotion_is_final_piece(mapped)) {
    s_promo_hall_seen = PIECE_EMPTY;
    s_promo_hall_seen_count = 0;
    if (s_promo_hall_swap_blink) {
      game_stop_error_blink();
      s_promo_hall_swap_blink = false;
    }
    game_update_promotion_anchor_led();
    xSemaphoreGiveRecursive(promotion_mutex);
    return;
  }

  if (mapped == s_promo_hall_seen) {
    if (s_promo_hall_seen_count < 2) {
      s_promo_hall_seen_count++;
    }
  } else {
    s_promo_hall_seen = mapped;
    s_promo_hall_seen_count = 1;
  }
  if (s_promo_hall_seen_count < 2) {
    game_update_promotion_anchor_led();
    xSemaphoreGiveRecursive(promotion_mutex);
    return;
  }

  if (s_promo_hall_swap_blink) {
    game_stop_error_blink();
    s_promo_hall_swap_blink = false;
  }

  board[row][col] = mapped;
  game_update_promotion_anchor_led();
  game_squares_guard_try_clear_from_matrix();

  if (game_promotion_is_final_piece(mapped)) {
    ESP_LOGI(TAG,
             "👑 Hall-stable promotion piece on %c%d — completing without extra "
             "button press",
             'a' + col, row + 1);
    promotion_state.pending = false;
    game_move_undo_meta_on_promotion_completed(mapped);
    game_promotion_finish_turn();
  }

  xSemaphoreGiveRecursive(promotion_mutex);
}
#endif

// ============================================================================
// PROMOTION HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Keep promotion square visually anchored while promotion is pending
 *
 * UX goal (swap_then_choose, swap optional):
 * - The player always sees on which square the graduation takes place (and where he can
 * swap)
 * - Does not extend to other LEDs (only 1 square on the board)
 */
void game_update_promotion_anchor_led(void) {
  if (!promotion_state.pending) {
    return;
  }

  uint8_t row = promotion_state.square_row;
  uint8_t col = promotion_state.square_col;
  uint8_t led_index = chess_pos_to_led_index(row, col);

  // Pulse between purple (pending) and gold (choice/upgrade) for clarity
  uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
  float t = (float)(now_ms % 1000U) / 1000.0f; // 0..1
  float phase = t * 2.0f * 3.14159f;
  float mix = 0.5f + 0.5f * sinf(phase); // 0..1

  // Purple base
  float pr = 128.0f, pg = 0.0f, pb = 128.0f;
  // Gold target
  float gr = 255.0f, gg = 215.0f, gb = 0.0f;

  uint8_t r = (uint8_t)(pr * (1.0f - mix) + gr * mix);
  uint8_t g = (uint8_t)(pg * (1.0f - mix) + gg * mix);
  uint8_t b = (uint8_t)(pb * (1.0f - mix) + gb * mix);

  led_set_pixel_safe(led_index, r, g, b);
}

/**
 * @brief Checks whether graduation is possible and updates the LED indication of the buttons
 *
 * This function checks all pesce on promotion squares (row 0 and 7)
 * and updates the promotion_state. Next, it updates the LED indication of all
 * promotion buttons (green = active, blue = inactive).
 *
 * @details
 * The function should be the steering wheel:
 * - After each turn
 * - After graduation
 * - When initializing the game
 *
 * LED indication:
 * - Green (0,255,0): Graduation is possible for the current player
 * - Blue (0,0,255): Graduation is not possible
 * - Reset button (LED 72): Always green
 */
void game_check_promotion_needed(void) {
  // Reset promotion state
  promotion_state.pending = false;
  promotion_state.square_row = 0;
  promotion_state.square_col = 0;
  promotion_state.player = current_player;

  // Check for pawns on promotion squares
  for (int col = 0; col < 8; col++) {
    // White pawn on rank 8 (row 7)
    if (board[7][col] == PIECE_WHITE_PAWN) {
      promotion_state.pending = true;
      promotion_state.square_row = 7;
      promotion_state.square_col = col;
      promotion_state.player = PLAYER_WHITE;
      current_game_state = GAME_STATE_PROMOTION; // Set game state to PROMOTION

      ESP_LOGI(TAG, "🎯 White promotion needed at %c%d", 'a' + col, 8);

      // Print promotion menu to UART
      printf("\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("👑 PAWN PROMOTION AVAILABLE!\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("📍 Square: %c%d\r\n", 'a' + col, 8);
      printf("🎮 Player: White\r\n");
      printf("\r\n");
      printf("🎯 Choose promotion piece:\r\n");
      printf("\r\n");
      printf("💡 PHYSICAL BUTTONS (press one):\r\n");
      printf("   • Button 0 (LED 64 🟢) → QUEEN  (most powerful)\r\n");
      printf("   • Button 1 (LED 65 🟢) → ROOK\r\n");
      printf("   • Button 2 (LED 66 🟢) → BISHOP\r\n");
      printf("   • Button 3 (LED 67 🟢) → KNIGHT\r\n");
      printf("\r\n");
      printf("📝 UART COMMANDS (alternative):\r\n");
      printf("   • PROMOTE %c%d=Q  → Queen\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=R  → Rook\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=B  → Bishop\r\n", 'a' + col, 8);
      printf("   • PROMOTE %c%d=N  → Knight\r\n", 'a' + col, 8);
      printf("\r\n");
      printf("⚠️  Note: Until promotion is completed, only the promotion square "
             "may be touched for optional swap.\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("\r\n");

      break;
    }

    // Black pawn on rank 1 (row 0)
    if (board[0][col] == PIECE_BLACK_PAWN) {
      promotion_state.pending = true;
      promotion_state.square_row = 0;
      promotion_state.square_col = col;
      promotion_state.player = PLAYER_BLACK;
      current_game_state = GAME_STATE_PROMOTION; // Set game state to PROMOTION

      ESP_LOGI(TAG, "🎯 Black promotion needed at %c%d", 'a' + col, 1);

      // Print promotion menu to UART
      printf("\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("👑 PAWN PROMOTION AVAILABLE!\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("📍 Square: %c%d\r\n", 'a' + col, 1);
      printf("🎮 Player: Black\r\n");
      printf("\r\n");
      printf("🎯 Choose promotion piece:\r\n");
      printf("\r\n");
      printf("💡 PHYSICAL BUTTONS (press one):\r\n");
      printf("   • Button 0 (LED 68 🟢) → QUEEN  (most powerful)\r\n");
      printf("   • Button 1 (LED 69 🟢) → ROOK\r\n");
      printf("   • Button 2 (LED 70 🟢) → BISHOP\r\n");
      printf("   • Button 3 (LED 71 🟢) → KNIGHT\r\n");
      printf("\r\n");
      printf("📝 UART COMMANDS (alternative):\r\n");
      printf("   • PROMOTE %c%d=Q  → Queen\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=R  → Rook\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=B  → Bishop\r\n", 'a' + col, 1);
      printf("   • PROMOTE %c%d=N  → Knight\r\n", 'a' + col, 1);
      printf("\r\n");
      printf("⚠️  Note: Until promotion is completed, only the promotion square "
             "may be touched for optional swap.\r\n");
      printf("═══════════════════════════════════════════════════════════════\r"
             "\n");
      printf("\r\n");

      break;
    }
  }

  // Update LED indications for all promotion buttons
  if (promotion_state.pending) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    game_hall_restore_guide_stop_leds();
#endif
    if (promotion_state.player == PLAYER_WHITE) {
      // White promotion buttons (64-67): GREEN
      for (int i = 64; i <= 67; i++) {
        led_set_pixel_safe(i, 0, 255, 0); // Green
      }
      // Black promotion buttons (68-71): BLUE
      if (chess_policy_move_hints_promotion_blue()) {
        for (int i = 68; i <= 71; i++) {
          led_set_pixel_safe(i, 0, 0, 255);
        }
      }
    } else {
      // White promotion buttons (64-67): BLUE
      if (chess_policy_move_hints_promotion_blue()) {
        for (int i = 64; i <= 67; i++) {
          led_set_pixel_safe(i, 0, 0, 255);
        }
      }
      // Black promotion buttons (68-71): GREEN
      for (int i = 68; i <= 71; i++) {
        led_set_pixel_safe(i, 0, 255, 0); // Green
      }
    }
  } else if (chess_policy_move_hints_promotion_blue()) {
    // No promotion pending - all promotion buttons BLUE
    for (int i = 64; i <= 71; i++) {
      led_set_pixel_safe(i, 0, 0, 255);
    }
  }

  // Reset button (LED 72): Always GREEN (always active)
  led_set_pixel_safe(72, 0, 255, 0); // Green
}

/**
 * @brief Handles the graduation button event
 *
 * @param button_id Button ID (0-7 for promotion, 8 for reset)
 *
 * @details
 * Button Mapping:
 * - 0-3: White promotion (Queen, Rook, Bishop, Knight)
 * - 4-7: Black promotion (Queen, Rook, Bishop, Knight)
 * - 8: Reset button
 *
 * There are only 4 physical buttons (QUEEN, ROOK, BISHOP, KNIGHT),
 * which maps to 0-3 or 4-7 according to current_player.
 */
void game_process_promotion_button(uint8_t button_id) {
  // #2 & #3: Atomic check with mutex
  // Use a recursive mutex function (promotion_mutex is a recursive mutex)
  if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
    ESP_LOGE(TAG, "❌ Failed to acquire promotion mutex");
    return;
  }

  // Check if promotion is pending
  if (!promotion_state.pending) {
    xSemaphoreGiveRecursive(promotion_mutex);
    ESP_LOGW(TAG, "⚠️ Promotion already processed");
    return;
  }

  // Map physical button ID (0-3) to promotion choice
  // Physical buttons are shared between players:
  // - Button 0: QUEEN (both white and black)
  // - Button 1: ROOK (both white and black)
  // - Button 2: BISHOP (both white and black)
  // - Button 3: KNIGHT (both white and black)
  promotion_choice_t choice;
  const char *piece_name;

  if (button_id <= 3) {
    // Valid physical button (0-3)
    choice = (promotion_choice_t)button_id;
  } else {
    xSemaphoreGiveRecursive(promotion_mutex); // Release mutex on invalid button
    ESP_LOGW(TAG, "⚠️  Invalid promotion button ID: %d (expected 0-3)",
             button_id);
    return;
  }

  ESP_LOGI(TAG, "👑 Physical promotion button pressed: button=%d, choice=%d",
           button_id, choice);

  // Get piece name
  switch (choice) {
  case PROMOTION_QUEEN:
    piece_name = "Queen";
    break;
  case PROMOTION_ROOK:
    piece_name = "Rook";
    break;
  case PROMOTION_BISHOP:
    piece_name = "Bishop";
    break;
  case PROMOTION_KNIGHT:
    piece_name = "Knight";
    break;
  default:
    piece_name = "Unknown";
    break;
  }

  ESP_LOGI(TAG, "👑 Processing promotion button %d: %s", button_id, piece_name);

  // Execute promotion
  if (game_execute_promotion(choice)) {
    promotion_state.pending = false;

    printf("\r\n");
    printf("═══════════════════════════════════════════════════════════════\r"
           "\n");
    printf("👑 PAWN PROMOTION SUCCESSFUL!\r\n");
    printf("  • Square: %c%d\r\n", 'a' + promotion_state.square_col,
           promotion_state.square_row + 1);
    printf("  • Promoted to: %s\r\n", piece_name);
    printf("  • Player: %s\r\n",
           promotion_state.player == PLAYER_WHITE ? "White" : "Black");
    printf("═══════════════════════════════════════════════════════════════\r"
           "\n");
    printf("\r\n");

    game_promotion_finish_turn();
    ESP_LOGI(TAG, "✅ Promotion completed successfully");
  } else {
    ESP_LOGE(TAG, "❌ Promotion execution failed");
  }
  // Release mutex at the end of the function to ensure it's always released
  xSemaphoreGiveRecursive(promotion_mutex);
}
