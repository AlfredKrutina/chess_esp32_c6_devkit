/**
 * @file chess_gameplay_policy.h
 * @brief Central gameplay feature gates (matrix guard, error recovery, move hints).
 *
 * Kconfig symbols drive compile-time defaults; runtime helpers keep call sites thin.
 */

#ifndef CHESS_GAMEPLAY_POLICY_H
#define CHESS_GAMEPLAY_POLICY_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Matrix guard */
bool chess_policy_matrix_guard_enabled(void);
bool chess_policy_matrix_guard_should_freeze(void);
bool chess_policy_matrix_guard_auto_clear_enabled(void);
bool chess_policy_matrix_guard_nvs_resync_enabled(void);
bool chess_policy_matrix_guard_led_enabled(void);
bool chess_policy_mg_led_white_yellow(void);
bool chess_policy_mg_led_black_blue(void);
bool chess_policy_mg_led_ghost_orange(void);
bool chess_policy_mg_led_missing_white(void);

/* Error recovery */
bool chess_policy_error_recovery_enabled(void);
bool chess_policy_error_recovery_should_lock(void);
bool chess_policy_error_recovery_should_mutate_board(void);
bool chess_policy_error_recovery_led_red_persist(void);
bool chess_policy_error_recovery_led_red_blink(void);
bool chess_policy_error_recovery_led_valid_blue(void);
void chess_policy_error_recovery_enter_lock(void);

/* Move hints */
bool chess_policy_move_hints_enabled(void);
bool chess_policy_move_hints_legal_blue(void);
bool chess_policy_move_hints_castling_blue(void);
bool chess_policy_move_hints_promotion_blue(void);
bool chess_policy_move_hints_movable_yellow(void);
void chess_policy_highlight_movable_if_enabled(void);

/* Diagnostics */
bool chess_policy_uart_error_detail_enabled(void);

const char *chess_policy_profile_name(void);
void chess_policy_log_boot(void);

#ifdef __cplusplus
}
#endif

#endif /* CHESS_GAMEPLAY_POLICY_H */
