/**
 * @file game_policy.h
 * @brief Compile-time gameplay gates. Kconfig is the source of truth.
 *
 * Each helper returns a menuconfig flag (or a conjunction of flags). Call
 * sites stay thin. Runtime state is not stored here, except
 * game_policy_error_recovery_enter_lock(), which sets the recovery latch
 * when locking is enabled.
 */
#ifndef CHESS_GAMEPLAY_POLICY_H
#define CHESS_GAMEPLAY_POLICY_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @return true when CONFIG_CHESS_MG_ENABLE is set. */
bool game_policy_squares_guard_enabled(void);

/** @return true when the guard is enabled and CONFIG_CHESS_MG_FREEZE_MOVES is set. */
bool game_policy_squares_guard_should_freeze(void);

/** @return true when the guard is enabled and CONFIG_CHESS_MG_AUTO_CLEAR is set. */
bool game_policy_squares_guard_auto_clear_enabled(void);

/** @return true when the guard is enabled and CONFIG_CHESS_MG_NVS_RESYNC is set. */
bool game_policy_squares_guard_nvs_resync_enabled(void);

/** @return true when the guard is enabled and CONFIG_CHESS_MG_LED_ENABLE is set. */
bool game_policy_squares_guard_led_enabled(void);

/** @return true when guard LEDs are on and CONFIG_CHESS_MG_LED_WHITE_YELLOW is set. */
bool game_policy_mg_led_white_yellow(void);

/** @return true when guard LEDs are on and CONFIG_CHESS_MG_LED_BLACK_BLUE is set. */
bool game_policy_mg_led_black_blue(void);

/** @return true when guard LEDs are on and CONFIG_CHESS_MG_LED_GHOST_ORANGE is set. */
bool game_policy_mg_led_ghost_orange(void);

/** @return true when guard LEDs are on and CONFIG_CHESS_MG_LED_MISSING_WHITE is set. */
bool game_policy_mg_led_missing_white(void);

/** @return true when CONFIG_CHESS_ER_ENABLE is set. */
bool game_policy_error_recovery_enabled(void);

/** @return true when error recovery is on and CONFIG_CHESS_ER_LOCK_GAME is set. */
bool game_policy_error_recovery_should_lock(void);

/** @return true when error recovery is on and CONFIG_CHESS_ER_MUTATE_BOARD is set. */
bool game_policy_error_recovery_should_mutate_board(void);

/** @return true when error recovery is on and CONFIG_CHESS_ER_LED_RED_PERSIST is set. */
bool game_policy_error_recovery_led_red_persist(void);

/** @return true when error recovery is on and CONFIG_CHESS_ER_LED_RED_BLINK is set. */
bool game_policy_error_recovery_led_red_blink(void);

/** @return true when error recovery is on and CONFIG_CHESS_ER_LED_VALID_BLUE is set. */
bool game_policy_error_recovery_led_valid_blue(void);

/**
 * @brief Set waiting_for_move_correction when lock-on-error is enabled.
 *
 * No-op when game_policy_error_recovery_should_lock() is false.
 */
void game_policy_error_recovery_enter_lock(void);

/** @return true when CONFIG_CHESS_MH_ENABLE is set. */
bool game_policy_move_hints_enabled(void);

/** @return true when move hints are on and CONFIG_CHESS_MH_LEGAL_MOVES_BLUE is set. */
bool game_policy_move_hints_legal_blue(void);

/** @return true when move hints are on and CONFIG_CHESS_MH_CASTLING_BLUE is set. */
bool game_policy_move_hints_castling_blue(void);

/** @return true when move hints are on and CONFIG_CHESS_MH_PROMOTION_BLUE is set. */
bool game_policy_move_hints_promotion_blue(void);

/** @return true when move hints are on and CONFIG_CHESS_MH_MOVABLE_YELLOW is set. */
bool game_policy_move_hints_movable_yellow(void);

/**
 * @brief Highlight movable pieces when that hint is enabled.
 *
 * Skipped while the Hall restore guide is showing its own LEDs.
 */
void game_policy_highlight_movable_if_enabled(void);

/**
 * @brief Clear the board, draw movable hints, then check overlay if needed.
 *
 * No-op while restore guide LEDs are active or special move UX is pending.
 */
void game_policy_refresh_idle_board_leds(void);

/** @return true when CONFIG_CHESS_DIAG_UART_ERROR_DETAIL is set. */
bool game_policy_uart_error_detail_enabled(void);

/**
 * @brief Active gameplay profile name from Kconfig.
 * @return "FULL", "DEV", "LITE", "FACTORY", or "CUSTOM".
 */
const char *game_policy_profile_name(void);

/** @brief Log the profile and whether guard, error recovery, hints, and UART detail are on. */
void game_policy_log_boot(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESS_GAMEPLAY_POLICY_H */
