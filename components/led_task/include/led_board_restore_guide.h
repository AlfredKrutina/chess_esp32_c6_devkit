/**
 * @file led_board_restore_guide.h
 * @brief Non-blocking orange path animation for Hall board-restore guide.
 */

#ifndef LED_BOARD_RESTORE_GUIDE_H
#define LED_BOARD_RESTORE_GUIDE_H

#include <stdbool.h>
#include <stdint.h>

#define LED_RESTORE_GUIDE_MAX_PATHS 3
#define LED_RESTORE_GUIDE_MAX_PATH_LEN 16
#define LED_RESTORE_GUIDE_MAX_MISSING 16
#define LED_RESTORE_GUIDE_MAX_EXTRA 16

typedef struct {
  uint8_t squares[LED_RESTORE_GUIDE_MAX_PATH_LEN];
  uint8_t len;
  /** Swap pair: whole corridor pulses together (no traveling pip). */
  bool pulse_corridor;
} led_restore_guide_path_t;

typedef struct {
  bool active;
  uint8_t path_count;
  led_restore_guide_path_t paths[LED_RESTORE_GUIDE_MAX_PATHS];
  uint8_t missing_homes[LED_RESTORE_GUIDE_MAX_MISSING];
  uint8_t missing_count;
  uint8_t extra_squares[LED_RESTORE_GUIDE_MAX_EXTRA];
  uint8_t extra_count;
} led_restore_guide_plan_t;

void led_board_restore_guide_apply(const led_restore_guide_plan_t *plan);
void led_board_restore_guide_clear(void);
bool led_board_restore_guide_is_active(void);
void led_board_restore_guide_tick(void);

#endif /* LED_BOARD_RESTORE_GUIDE_H */
