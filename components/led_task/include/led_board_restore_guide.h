/**
 * @file led_board_restore_guide.h
 * @brief Smooth per-piece setup glow for the Hall board-restore guide.
 */

#ifndef LED_BOARD_RESTORE_GUIDE_H
#define LED_BOARD_RESTORE_GUIDE_H

#include "game_events.h"
#include <stdbool.h>
#include <stdint.h>

#define LED_RESTORE_GUIDE_MAX_PATHS GAME_EVT_RESTORE_MAX_PATHS
#define LED_RESTORE_GUIDE_MAX_PATH_LEN GAME_EVT_RESTORE_MAX_PATH_LEN
#define LED_RESTORE_GUIDE_MAX_MISSING GAME_EVT_RESTORE_MAX_MISSING
#define LED_RESTORE_GUIDE_MAX_EXTRA GAME_EVT_RESTORE_MAX_EXTRA
#define LED_RESTORE_GUIDE_MAX_MARKS GAME_EVT_RESTORE_MAX_MARKS

#define LED_RESTORE_MARK_HOME GAME_EVT_RESTORE_MARK_HOME
#define LED_RESTORE_MARK_SITTING GAME_EVT_RESTORE_MARK_SITTING
#define LED_RESTORE_MARK_EXTRA GAME_EVT_RESTORE_MARK_EXTRA

typedef game_restore_path_t led_restore_guide_path_t;
typedef game_restore_mark_t led_restore_guide_mark_t;
typedef game_restore_plan_t led_restore_guide_plan_t;

void led_board_restore_guide_apply(const led_restore_guide_plan_t *plan);
void led_board_restore_guide_clear(void);
/** Drop the plan and fade whatever is still lit. */
void led_board_restore_guide_fade_out(void);
bool led_board_restore_guide_is_active(void);
void led_board_restore_guide_tick(void);
/** Forget the last painted guide colors so the next tick draws them again. */
void led_board_restore_guide_yield(void);
/** Drop the guide without painting, so a demo or a game can own the squares. */
void led_board_restore_guide_cancel(void);
/** Drop the plan and fade every square toward off. */
void led_board_restore_guide_blank(void);

#endif /* LED_BOARD_RESTORE_GUIDE_H */
