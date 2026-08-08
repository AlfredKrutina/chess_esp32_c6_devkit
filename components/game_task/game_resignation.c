/**
 * @file game_resignation.c
 * @brief King resignation timer, button LED animation, timeout finalize.
 */

#include "game_task_internal.h"
#include "game_task.h"

#include "../button_task/include/button_task.h"
#include "../led_task/include/led_task.h"
#include "../../timer_system/include/timer_system.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include <stdio.h>

static const char *TAG = "GAME_RESIGN";

// KING RESIGNATION IMPLEMENTATION
// ============================================================================

/**
 * @brief Update button LEDs with fade animation
 * @param elapsed_ms Milliseconds elapsed since resignation start
 * 0-4s: the buttons do not change (the original colors remain).
 * 4-5s: buttons turn solid orange.
 * 5-9s: fade in LED order (Queen → Rook → Bishop → Knight on both sides).
 */
static void resignation_update_button_leds(uint32_t elapsed_ms) {
  const uint32_t ORANGE_START_MS = 4000; // Color to orange up to 4 s
  if (elapsed_ms < ORANGE_START_MS)
    return;

  const uint8_t WHITE_BASE = 64;
  const uint8_t BLACK_BASE = 68;

  const uint32_t FADE_START_MS = 5000;
  const uint32_t FADE_DURATION_MS = 1000;
  const uint32_t LED_OFFSET_MS = 1000;

  for (int i = 0; i < 4; i++) {
    uint32_t led_start_time = FADE_START_MS + (i * LED_OFFSET_MS);
    uint8_t brightness = 255;

    if (elapsed_ms >= led_start_time) {
      uint32_t led_elapsed = elapsed_ms - led_start_time;
      if (led_elapsed >= FADE_DURATION_MS) {
        brightness = 0;
      } else {
        brightness = 255 - (led_elapsed * 255 / FADE_DURATION_MS);
      }
    }

    uint8_t r = brightness;
    uint8_t g = brightness / 2;
    uint8_t b = 0;
    button_set_led_color(WHITE_BASE + i, r, g, b);
    button_set_led_color(BLACK_BASE + i, r, g, b);
  }
}

/**
 * @brief Animation timer callback - vola se kazdych 50ms
 */
static void resignation_animation_timer_callback(TimerHandle_t xTimer) {
  // IMPORTANT:
  // Timer callbacks run in FreeRTOS "Tmr Svc" task context.
  // Avoid heavy work here (printf/ESP_LOG*, mutex waits, LED commits, game
  // logic), otherwise Timer Service stack can overflow and crash the system.
  //
  // Resignation visuals and timeout are handled from the main `game_task` loop
  // via `resignation_tick()`.
  (void)xTimer;
}

/**
 * @brief Main timer callback - called after 10 seconds
 */
static void resignation_main_timer_callback(TimerHandle_t xTimer) {
  // See comment in `resignation_animation_timer_callback()`.
  // Finalization is handled from `game_task` context via `resignation_tick()`.
  (void)xTimer;
}

/**
 * @brief Finalize resignation after 10s (runs in game_task context)
 */
static void resignation_finalize_timeout(void) {
  if (!resignation_state.active) {
    return;
  }

  const char *player_name =
      (resignation_state.player == PLAYER_WHITE) ? "White" : "Black";
  const char *winner_name =
      (resignation_state.player == PLAYER_WHITE) ? "Black" : "White";

  ESP_LOGI(TAG, "🏳️ %s resigned! %s wins!", player_name, winner_name);

  // End the game
  current_game_state = GAME_STATE_FINISHED;
  game_result = GAME_STATE_FINISHED;
  current_result_type = (resignation_state.player == PLAYER_WHITE)
                            ? RESULT_BLACK_WINS
                            : RESULT_WHITE_WINS;
  current_endgame_reason = ENDGAME_REASON_RESIGNATION;
  game_active = false;

  // Print report later in game_task (stack-safe)
  game_update_endgame_statistics(current_result_type);
  endgame_report_requested = true;

  timer_pause();

  // Run the victory animation for the winner
  player_t winner_player =
      (resignation_state.player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
  game_trigger_victory_animation(winner_player);

  // Timeout = finalized cleanup (no cancel messages, do not restore king)
  resignation_stop(true);
}

/**
 * @brief Periodic resignation processing (runs in game_task context)
 */
void resignation_tick(void) {
  if (!resignation_state.active) {
    return;
  }

  uint64_t now_ms = esp_timer_get_time() / 1000;
  uint32_t elapsed_ms = (uint32_t)(now_ms - resignation_state.start_time_ms);

  // Update button LEDs (fade effect) in safe task context
  resignation_update_button_leds(elapsed_ms);

  // Finalize after 10 seconds
  if (elapsed_ms >= 10000) {
    resignation_finalize_timeout();
  }
}

/**
 * @brief Start the king resignation timer
 */
void resignation_start(player_t player, uint8_t row, uint8_t col) {
  // Stop an existing timer if it is running (restart = silent cleanup)
  if (resignation_state.active) {
    resignation_stop(true);
  }

  const char *player_name = (player == PLAYER_WHITE) ? "White" : "Black";
  printf("\r\n⚠️ %s king lifted - hold for 10+ seconds to resign!\r\n",
         player_name);
  printf("👑 Resignation timer started for %s player\r\n", player_name);
  printf("🎨 Button LED animation started (8 buttons, both sides)\r\n\r\n");

  // Initialize state
  resignation_state.active = true;
  resignation_state.player = player;
  resignation_state.king_row = row;
  resignation_state.king_col = col;
  resignation_state.start_time_ms = esp_timer_get_time() / 1000;
  resignation_state.last_countdown_sec = 10;

  // Remove the king from the board (like a normal pickup)
  // Otherwise, the drop code expects an empty field and drops
  board[row][col] = PIECE_EMPTY;
  ESP_LOGI(TAG, "✅ King removed from board[%d][%d] for resignation", row, col);

  // Combination of red (warning) and yellow (source square)
  // Orange-red mix instead of just red
  led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 128, 0);

  // Buttons turn orange only after 4s (in resignation_update_button_leds)
  // – the first 4 seconds remain in the original color

  // PRODUCTION STABILITY:
  // Do NOT run resignation logic from FreeRTOS timer callbacks ("Tmr Svc").
  // Handling is done in `resignation_tick()` from the main game_task loop.
  resignation_state.main_timer = NULL;
  resignation_state.animation_timer = NULL;
}

/**
 * @brief Stop the king resignation timer
 * @param finalize false = user canceled (placed king), return king +
 * announcements. true = finalized (timeout / reset / restart), just silent cleanup.
 */
void resignation_stop(bool finalize) {
  if (!resignation_state.active)
    return;

  if (!finalize) {
    printf("✅ King placed - resignation cancelled!\r\n");
    printf("🛑 Resignation timer stopped\r\n");
    printf("🎨 Button LED animation stopped\r\n\r\n");
  } else {
    ESP_LOGI(TAG,
             "🔄 [STAGING] Resignation finalized (cleanup, no cancel msgs)");
  }

  // Only when canceled by the user: return the king to its original position
  if (!finalize) {
    piece_t king_piece = (resignation_state.player == PLAYER_WHITE)
                             ? PIECE_WHITE_KING
                             : PIECE_BLACK_KING;

    if (board[resignation_state.king_row][resignation_state.king_col] ==
        PIECE_EMPTY) {
      board[resignation_state.king_row][resignation_state.king_col] =
          king_piece;
      ESP_LOGI(TAG,
               "✅ King returned to original position [%d][%d] (resignation "
               "cancelled)",
               resignation_state.king_row, resignation_state.king_col);
    } else {
      ESP_LOGW(
          TAG,
          "⚠️ Original king position [%d][%d] is occupied - drop processing "
          "will handle it",
          resignation_state.king_row, resignation_state.king_col);
    }

    led_set_pixel_safe(chess_pos_to_led_index(resignation_state.king_row,
                                              resignation_state.king_col),
                       0, 0, 0);
  }

  // Zastavit a smazat timery
  if (resignation_state.main_timer) {
    xTimerStop(resignation_state.main_timer, 0);
    xTimerDelete(resignation_state.main_timer, 0);
    resignation_state.main_timer = NULL;
  }

  if (resignation_state.animation_timer) {
    xTimerStop(resignation_state.animation_timer, 0);
    xTimerDelete(resignation_state.animation_timer, 0);
    resignation_state.animation_timer = NULL;
  }

  if (finalize) {
    // When the resignation is complete (timeout), turn off the button LEDs
    for (int i = 0; i < 8; i++) {
      button_set_led_color(64 + i, 0, 0, 0);
    }
  } else {
    // When canceling (king laid back) restore the normal state of the buttons
    // (green/blue according to graduation, LED 72 green)
    game_check_promotion_needed();
  }

  // Status reset
  resignation_state.active = false;
}
