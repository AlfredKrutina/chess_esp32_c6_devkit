/**
 * @file game_promotion.c
 * @brief Promotion anchor LED, detection, and physical button handling.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "chess_gameplay_policy.h"

#include "../led_task/include/led_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"

#include <math.h>
#include <stdio.h>

static const char *TAG = "GAME_PROMOTION";

// ============================================================================
// PROMOTION HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Keep promotion square visually anchored while promotion is pending
 *
 * UX goal (swap_then_choose, swap volitelný):
 * - Hráč vždy vidí, na kterém poli probíhá promoce (a kde může případně
 * swapovat)
 * - Nesahá na ostatní LED (jen 1 políčko na šachovnici)
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
 * @brief Zkontroluje zda je promoce mozna a aktualizuje LED indikaci tlacitek
 *
 * Tato funkce kontroluje vsechny pesce na promotion squares (row 0 a 7)
 * a aktualizuje promotion_state. Nasledne aktualizuje LED indikaci vsech
 * promotion tlacitek (zelena = aktivni, modra = neaktivni).
 *
 * @details
 * Funkce by mela byt volana:
 * - Po kazdem tahu
 * - Po promoci
 * - Pri inicializaci hry
 *
 * LED indikace:
 * - Zelena (0,255,0): Promoce je mozna pro aktualniho hrace
 * - Modra (0,0,255): Promoce neni mozna
 * - Reset tlacitko (LED 72): Vzdy zelena
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
 * @brief Zpracuje button event pro promoci
 *
 * @param button_id ID tlacitka (0-7 pro promotion, 8 pro reset)
 *
 * @details
 * Mapovani tlacitek:
 * - 0-3: White promotion (Queen, Rook, Bishop, Knight)
 * - 4-7: Black promotion (Queen, Rook, Bishop, Knight)
 * - 8: Reset button
 *
 * Fyzicky jsou jen 4 tlacitka (QUEEN, ROOK, BISHOP, KNIGHT),
 * ktere se mapuji na 0-3 nebo 4-7 podle current_player.
 */
void game_process_promotion_button(uint8_t button_id) {
  // #2 & #3: Atomic check with mutex
  // Použít recursive mutex funkce (promotion_mutex je recursive mutex)
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
    // Clear promotion state (mutex already held from beginning)
    promotion_state.pending = false;

    // Log success
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

    // Switch to next player after promotion
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

    // #3: Timer switch AFTER button promotion choice
    ESP_LOGI(
        TAG,
        "🔄 Timer switch (button promotion): ending for %s, starting for %s",
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
               "🎯 Game finished after promotion! Starting endgame animation "
               "at position %d",
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

      // Update LED indications
      game_check_promotion_needed();
      chess_policy_highlight_movable_if_enabled();
      led_force_immediate_update(); // Commit highlight so movable pieces show
                                    // right after player change animation
    }

    // Zkontrolovat, zda je nový hráč v šachu
    bool in_check = game_is_king_in_check(current_player);
    if (in_check) {
      // Najít pozici krále
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

    ESP_LOGI(TAG, "✅ Promotion completed successfully");
  } else {
    ESP_LOGE(TAG, "❌ Promotion execution failed");
  }
  // Release mutex at the end of the function to ensure it's always released
  xSemaphoreGiveRecursive(promotion_mutex);
}
