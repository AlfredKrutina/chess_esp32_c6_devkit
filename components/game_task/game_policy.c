/**
 * @file game_policy.c
 * @brief Kconfig-driven gameplay policy helpers.
 */

#include "game_policy.h"
#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_position.h"
#include "game_hall_restore_guide.h"
#endif

#include "game_led_direct.h"
#include "game_events.h"

#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "GAMEPLAY_POLICY";

bool game_policy_squares_guard_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE
  return true;
#else
  return false;
#endif
}

bool game_policy_squares_guard_should_freeze(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_FREEZE_MOVES
  return true;
#else
  return false;
#endif
}

bool game_policy_squares_guard_auto_clear_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_AUTO_CLEAR
  return true;
#else
  return false;
#endif
}

bool game_policy_squares_guard_nvs_resync_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_NVS_RESYNC
  return true;
#else
  return false;
#endif
}

bool game_policy_squares_guard_led_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE
  return true;
#else
  return false;
#endif
}

bool game_policy_mg_led_white_yellow(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_WHITE_YELLOW
  return true;
#else
  return false;
#endif
}

bool game_policy_mg_led_black_blue(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_BLACK_BLUE
  return true;
#else
  return false;
#endif
}

bool game_policy_mg_led_ghost_orange(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_GHOST_ORANGE
  return true;
#else
  return false;
#endif
}

bool game_policy_mg_led_missing_white(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_MISSING_WHITE
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_enabled(void) {
#if CONFIG_CHESS_ER_ENABLE
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_should_lock(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LOCK_GAME
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_should_mutate_board(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_MUTATE_BOARD
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_led_red_persist(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_RED_PERSIST
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_led_red_blink(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_RED_BLINK
  return true;
#else
  return false;
#endif
}

bool game_policy_error_recovery_led_valid_blue(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_VALID_BLUE
  return true;
#else
  return false;
#endif
}

void game_policy_error_recovery_enter_lock(void) {
  if (game_policy_error_recovery_should_lock()) {
    /* Caller must post GAME_EVT_MOVE_REJECTED before returning to the player. */
    (*game_state_error_recovery_state()).waiting_for_move_correction = true;
  }
}

bool game_policy_move_hints_enabled(void) {
#if CONFIG_CHESS_MH_ENABLE
  return true;
#else
  return false;
#endif
}

bool game_policy_move_hints_legal_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_LEGAL_MOVES_BLUE
  return true;
#else
  return false;
#endif
}

bool game_policy_move_hints_castling_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_CASTLING_BLUE
  return true;
#else
  return false;
#endif
}

bool game_policy_move_hints_promotion_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_PROMOTION_BLUE
  return true;
#else
  return false;
#endif
}

bool game_policy_move_hints_movable_yellow(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_MOVABLE_YELLOW
  return true;
#else
  return false;
#endif
}

void game_policy_highlight_movable_if_enabled(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (game_hall_restore_guide_is_active()) {
    return;
  }
#endif
  if (game_policy_move_hints_movable_yellow()) {
    game_highlight_movable_pieces();
  }
}

void game_policy_refresh_idle_board_leds(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* The app's best move must still light when a guide owns the idle picture. */
  if (!game_coach_hint_is_set()) {
    if (game_hall_restore_guide_is_active()) {
      return;
    }
    if (game_hall_position_blocks_idle_leds()) {
      return;
    }
  }
#endif
  if (piece_lifted || castling_state.in_progress ||
      (*game_state_promotion_state()).pending || game_ep_capture_waiting() ||
      game_is_castle_animation_active() || game_is_castling_expected() ||
      game_puzzle_suppress_move_hints() || game_is_puzzle_active() ||
      game_is_puzzle_setup_active()) {
    return;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_IDLE) {
    return;
  }

  /* Guide tick must not paint over this full frame if its clear event is dropped. */
  game_event_required_step_disarm();
  game_led_frame_begin(GAME_EVT_HINT_FADE_MS, false);
  game_policy_highlight_movable_if_enabled();

  if (game_is_king_in_check(current_player) &&
      game_led_guidance_show_check_anim()) {
    int king_row = -1;
    int king_col = -1;
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
      if (king_row != -1) {
        break;
      }
    }
    if (king_row != -1 && king_col != -1) {
      game_led_frame_breathe((uint8_t)king_row, (uint8_t)king_col, 255, 192,
                             203, GAME_EVT_BREATHE_TARGET, 0, 0, 0);
    }
  }
  game_coach_hint_paint();
  game_led_frame_end();
}

bool game_policy_uart_error_detail_enabled(void) {
#if CONFIG_CHESS_DIAG_UART_ERROR_DETAIL
  return true;
#else
  return false;
#endif
}

const char *game_policy_profile_name(void) {
#if CONFIG_CHESS_GAMEPLAY_PROFILE_FULL
  return "FULL";
#elif CONFIG_CHESS_GAMEPLAY_PROFILE_DEV
  return "DEV";
#elif CONFIG_CHESS_GAMEPLAY_PROFILE_LITE
  return "LITE";
#elif CONFIG_CHESS_GAMEPLAY_PROFILE_FACTORY
  return "FACTORY";
#else
  return "CUSTOM";
#endif
}

void game_policy_log_boot(void) {
  ESP_LOGI(TAG,
           "Gameplay profile=%s MG=%d ER=%d MH=%d UART_detail=%d",
           game_policy_profile_name(), game_policy_squares_guard_enabled(),
           game_policy_error_recovery_enabled(),
           game_policy_move_hints_enabled(),
           game_policy_uart_error_detail_enabled());
}
