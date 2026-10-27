/**
 * @file chess_gameplay_policy.c
 * @brief Kconfig-driven gameplay policy helpers.
 */

#include "chess_gameplay_policy.h"
#include "game_task_internal.h"
#include "game_task.h"
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
#include "game_hall_position.h"
#include "game_hall_restore_guide.h"
#endif

#include "../led_task/include/led_task.h"
#include "led_mapping.h"

#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "GAMEPLAY_POLICY";

bool chess_policy_squares_guard_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE
  return true;
#else
  return false;
#endif
}

bool chess_policy_squares_guard_should_freeze(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_FREEZE_MOVES
  return true;
#else
  return false;
#endif
}

bool chess_policy_squares_guard_auto_clear_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_AUTO_CLEAR
  return true;
#else
  return false;
#endif
}

bool chess_policy_squares_guard_nvs_resync_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_NVS_RESYNC
  return true;
#else
  return false;
#endif
}

bool chess_policy_squares_guard_led_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE
  return true;
#else
  return false;
#endif
}

bool chess_policy_mg_led_white_yellow(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_WHITE_YELLOW
  return true;
#else
  return false;
#endif
}

bool chess_policy_mg_led_black_blue(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_BLACK_BLUE
  return true;
#else
  return false;
#endif
}

bool chess_policy_mg_led_ghost_orange(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_GHOST_ORANGE
  return true;
#else
  return false;
#endif
}

bool chess_policy_mg_led_missing_white(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_LED_ENABLE && \
    CONFIG_CHESS_MG_LED_MISSING_WHITE
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_enabled(void) {
#if CONFIG_CHESS_ER_ENABLE
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_should_lock(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LOCK_GAME
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_should_mutate_board(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_MUTATE_BOARD
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_led_red_persist(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_RED_PERSIST
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_led_red_blink(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_RED_BLINK
  return true;
#else
  return false;
#endif
}

bool chess_policy_error_recovery_led_valid_blue(void) {
#if CONFIG_CHESS_ER_ENABLE && CONFIG_CHESS_ER_LED_VALID_BLUE
  return true;
#else
  return false;
#endif
}

void chess_policy_error_recovery_enter_lock(void) {
  if (chess_policy_error_recovery_should_lock()) {
    error_recovery_state.waiting_for_move_correction = true;
  }
}

bool chess_policy_move_hints_enabled(void) {
#if CONFIG_CHESS_MH_ENABLE
  return true;
#else
  return false;
#endif
}

bool chess_policy_move_hints_legal_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_LEGAL_MOVES_BLUE
  return true;
#else
  return false;
#endif
}

bool chess_policy_move_hints_castling_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_CASTLING_BLUE
  return true;
#else
  return false;
#endif
}

bool chess_policy_move_hints_promotion_blue(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_PROMOTION_BLUE
  return true;
#else
  return false;
#endif
}

bool chess_policy_move_hints_movable_yellow(void) {
#if CONFIG_CHESS_MH_ENABLE && CONFIG_CHESS_MH_MOVABLE_YELLOW
  return true;
#else
  return false;
#endif
}

void chess_policy_highlight_movable_if_enabled(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (game_hall_restore_guide_is_active()) {
    return;
  }
#endif
  if (chess_policy_move_hints_movable_yellow()) {
    game_highlight_movable_pieces();
  }
}

void chess_policy_refresh_idle_board_leds(void) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (game_hall_restore_guide_is_active()) {
    return;
  }
  if (game_hall_position_blocks_idle_leds()) {
    return;
  }
#endif
  if (piece_lifted || castling_state.in_progress ||
      promotion_state.pending || game_ep_capture_waiting() ||
      game_is_castle_animation_active() || game_is_castling_expected() ||
      game_puzzle_suppress_move_hints()) {
    return;
  }
  if (current_game_state != GAME_STATE_ACTIVE &&
      current_game_state != GAME_STATE_IDLE) {
    return;
  }

  led_clear_board_only();
  chess_policy_highlight_movable_if_enabled();

  if (!game_is_king_in_check(current_player)) {
    return;
  }
  int king_row = -1;
  int king_col = -1;
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
  if (king_row == -1 || king_col == -1 ||
      !game_led_guidance_show_check_anim()) {
    return;
  }
  uint8_t king_led_index =
      chess_pos_to_led_index((uint8_t)king_row, (uint8_t)king_col);
  led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                             .led_index = king_led_index,
                             .red = 0,
                             .green = 0,
                             .blue = 0,
                             .duration_ms = 0,
                             .data = NULL};
  led_execute_command_new(&check_cmd);
}

bool chess_policy_uart_error_detail_enabled(void) {
#if CONFIG_CHESS_DIAG_UART_ERROR_DETAIL
  return true;
#else
  return false;
#endif
}

const char *chess_policy_profile_name(void) {
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

void chess_policy_log_boot(void) {
  ESP_LOGI(TAG,
           "Gameplay profile=%s MG=%d ER=%d MH=%d UART_detail=%d",
           chess_policy_profile_name(), chess_policy_squares_guard_enabled(),
           chess_policy_error_recovery_enabled(),
           chess_policy_move_hints_enabled(),
           chess_policy_uart_error_detail_enabled());
}
