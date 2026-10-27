/**
 * @file led_board_restore_guide.h
 * @brief Smooth per-piece setup glow for the Hall board-restore guide.
 */

#ifndef LED_BOARD_RESTORE_GUIDE_H
#define LED_BOARD_RESTORE_GUIDE_H

#include <stdbool.h>
#include <stdint.h>

#define LED_RESTORE_GUIDE_MAX_PATHS 3
#define LED_RESTORE_GUIDE_MAX_PATH_LEN 16
#define LED_RESTORE_GUIDE_MAX_MISSING 32
#define LED_RESTORE_GUIDE_MAX_EXTRA 16
#define LED_RESTORE_GUIDE_MAX_MARKS 80

#define LED_RESTORE_MARK_HOME 0
#define LED_RESTORE_MARK_SITTING 1
#define LED_RESTORE_MARK_EXTRA 2

typedef struct {
  uint8_t squares[LED_RESTORE_GUIDE_MAX_PATH_LEN];
  uint8_t len;
  /** Unused by the setup glow. Kept so path picking still compiles. */
  bool pulse_corridor;
} led_restore_guide_path_t;

typedef struct {
  uint8_t square;
  uint8_t type;
  uint8_t color;
  uint8_t role;
} led_restore_guide_mark_t;

typedef struct {
  bool active;
  uint8_t path_count;
  led_restore_guide_path_t paths[LED_RESTORE_GUIDE_MAX_PATHS];
  uint8_t missing_homes[LED_RESTORE_GUIDE_MAX_MISSING];
  uint8_t missing_count;
  uint8_t extra_squares[LED_RESTORE_GUIDE_MAX_EXTRA];
  uint8_t extra_count;
  uint8_t mark_count;
  led_restore_guide_mark_t marks[LED_RESTORE_GUIDE_MAX_MARKS];
} led_restore_guide_plan_t;

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
