/**
 * @file chess_gameplay_policy.c
 * @brief Kconfig-driven gameplay policy helpers.
 */

#include "chess_gameplay_policy.h"
#include "game_task_internal.h"
#include "game_task.h"

#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "GAMEPLAY_POLICY";

bool chess_policy_matrix_guard_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE
  return true;
#else
  return false;
#endif
}

bool chess_policy_matrix_guard_should_freeze(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_FREEZE_MOVES
  return true;
#else
  return false;
#endif
}

bool chess_policy_matrix_guard_auto_clear_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_AUTO_CLEAR
  return true;
#else
  return false;
#endif
}

bool chess_policy_matrix_guard_nvs_resync_enabled(void) {
#if CONFIG_CHESS_MG_ENABLE && CONFIG_CHESS_MG_NVS_RESYNC
  return true;
#else
  return false;
#endif
}

bool chess_policy_matrix_guard_led_enabled(void) {
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
  if (chess_policy_move_hints_movable_yellow()) {
    game_highlight_movable_pieces();
  }
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
           chess_policy_profile_name(), chess_policy_matrix_guard_enabled(),
           chess_policy_error_recovery_enabled(),
           chess_policy_move_hints_enabled(),
           chess_policy_uart_error_detail_enabled());
}
