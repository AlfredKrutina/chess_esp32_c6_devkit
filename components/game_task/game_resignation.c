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
 * 0–4 s: tlačítka se nemění (zůstávají původní barvy).
 * 4–5 s: tlačítka přejdou na plnou oranžovou.
 * 5–9 s: fade v pořadí LED (Queen → Rook → Bishop → Knight na obou stranách).
 */
static void resignation_update_button_leds(uint32_t elapsed_ms) {
  const uint32_t ORANGE_START_MS = 4000; // Barva na oranžovou až po 4 s
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
 * @brief Hlavni timer callback - vola se po 10 sekundach
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

  // Ukončit hru
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

  // Spustit victory animaci pro vítěze
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
 * @brief Spustit king resignation timer
 */
void resignation_start(player_t player, uint8_t row, uint8_t col) {
  // Zastavit existující timer pokud běží (restart = tichý cleanup)
  if (resignation_state.active) {
    resignation_stop(true);
  }

  const char *player_name = (player == PLAYER_WHITE) ? "White" : "Black";
  printf("\r\n⚠️ %s king lifted - hold for 10+ seconds to resign!\r\n",
         player_name);
  printf("👑 Resignation timer started for %s player\r\n", player_name);
  printf("🎨 Button LED animation started (8 buttons, both sides)\r\n\r\n");

  // Inicializovat stav
  resignation_state.active = true;
  resignation_state.player = player;
  resignation_state.king_row = row;
  resignation_state.king_col = col;
  resignation_state.start_time_ms = esp_timer_get_time() / 1000;
  resignation_state.last_countdown_sec = 10;

  // Odstranit krále z boardu (jako normální pickup)
  // Jinak drop kód očekává prázdné pole a spadne
  board[row][col] = PIECE_EMPTY;
  ESP_LOGI(TAG, "✅ King removed from board[%d][%d] for resignation", row, col);

  // Kombinace červené (varování) a žluté (source square)
  // Oranžovo-červená mix místo jen červené
  led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 128, 0);

  // Tlačítka se na oranžovou změní až po 4 s (v resignation_update_button_leds)
  // – prvních 4 s zůstávají v původní barvě

  // PRODUCTION STABILITY:
  // Do NOT run resignation logic from FreeRTOS timer callbacks ("Tmr Svc").
  // Handling is done in `resignation_tick()` from the main game_task loop.
  resignation_state.main_timer = NULL;
  resignation_state.animation_timer = NULL;
}

/**
 * @brief Zastavit king resignation timer
 * @param finalize false = uživatel zrušil (položil krále), vrátit krále +
 * hlášky. true  = finalized (timeout / reset / restart), jen tiché cleanup.
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

  // Pouze při zrušení uživatelem: vrátit krále na původní pozici
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
    // Při dokončení rezignace (timeout) vypnout button LEDs
    for (int i = 0; i < 8; i++) {
      button_set_led_color(64 + i, 0, 0, 0);
    }
  } else {
    // Při zrušení (král položen zpět) obnovit normální stav tlačítek
    // (zelená/modrá podle promoce, LED 72 zelená)
    game_check_promotion_needed();
  }

  // Reset stavu
  resignation_state.active = false;
}
