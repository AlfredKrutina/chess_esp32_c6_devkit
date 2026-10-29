/**
 * @file game_events.h
 * @brief Session-to-LED events (no WS2812 in game_task).
 */

#ifndef GAME_EVENTS_H
#define GAME_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#define GAME_EVT_HINT_MAX 32
#define GAME_EVT_HINT_PATCH 1u
#define GAME_EVT_HINT_REQUIRED_STEP 2u
#define GAME_EVT_HINT_FADE_MS 180u

#define GAME_EVT_BREATHE_MAX 4
#define GAME_EVT_BREATHE_TARGET 0
#define GAME_EVT_BREATHE_SQ 1

#define GAME_EVT_ANIM_MOVE_PATH 1u
#define GAME_EVT_ANIM_CASTLE 2u
#define GAME_EVT_ANIM_PROMOTE 3u
#define GAME_EVT_ANIM_HINT_PAIR 4u

#define GAME_EVT_CTRL_STOP_ENDGAME 1u
#define GAME_EVT_CTRL_RESIGN_CANCEL 2u
#define GAME_EVT_CTRL_RELEASE_HOLD 3u
#define GAME_EVT_CTRL_ARM_HANDOVER 4u
#define GAME_EVT_CTRL_LAMP 5u
#define GAME_EVT_CTRL_BLACKOUT 6u
#define GAME_EVT_CTRL_HA_COLOR 7u
#define GAME_EVT_CTRL_FORCE_UPDATE 8u
#define GAME_EVT_CTRL_CLEAR_ALL 9u
#define GAME_EVT_CTRL_TRAIL_CANCEL 10u
#define GAME_EVT_CTRL_TAKEBACK_CLEAR 11u
#define GAME_EVT_CTRL_RESIGN_STOP 12u
#define GAME_EVT_CTRL_RESIGN_START 13u
#define GAME_EVT_CTRL_SHOW_BOARD 14u
#define GAME_EVT_CTRL_COLLAPSE 15u
#define GAME_EVT_CTRL_PULSE 16u

#define GAME_EVT_RESTORE_MAX_PATHS 3
#define GAME_EVT_RESTORE_MAX_PATH_LEN 16
#define GAME_EVT_RESTORE_MAX_MISSING 32
#define GAME_EVT_RESTORE_MAX_EXTRA 16
#define GAME_EVT_RESTORE_MAX_MARKS 80

#define GAME_EVT_RESTORE_MARK_HOME 0
#define GAME_EVT_RESTORE_MARK_SITTING 1
#define GAME_EVT_RESTORE_MARK_EXTRA 2

#define GAME_EVT_RESTORE_OP_APPLY 0
#define GAME_EVT_RESTORE_OP_CLEAR 1
#define GAME_EVT_RESTORE_OP_CANCEL 2
#define GAME_EVT_RESTORE_OP_FADE_OUT 3
#define GAME_EVT_RESTORE_OP_BLANK 4
#define GAME_EVT_RESTORE_OP_TAKEBACK 5

typedef enum {
  GAME_EVT_MOVE_ACCEPTED = 1,
  GAME_EVT_MOVE_REJECTED,
  GAME_EVT_CHECK,
  GAME_EVT_GAME_END,
  GAME_EVT_RESTORE_BOARD,
  GAME_EVT_HINT_SQUARES,
  GAME_EVT_BOARD_ANIM,
  GAME_EVT_LED_CTRL
} game_event_kind_t;

typedef struct {
  uint8_t squares[GAME_EVT_RESTORE_MAX_PATH_LEN];
  uint8_t len;
  bool pulse_corridor;
} game_restore_path_t;

typedef struct {
  uint8_t square;
  uint8_t type;
  uint8_t color;
  uint8_t role;
} game_restore_mark_t;

typedef struct {
  bool active;
  uint8_t path_count;
  game_restore_path_t paths[GAME_EVT_RESTORE_MAX_PATHS];
  uint8_t missing_homes[GAME_EVT_RESTORE_MAX_MISSING];
  uint8_t missing_count;
  uint8_t extra_squares[GAME_EVT_RESTORE_MAX_EXTRA];
  uint8_t extra_count;
  uint8_t mark_count;
  game_restore_mark_t marks[GAME_EVT_RESTORE_MAX_MARKS];
} game_restore_plan_t;

typedef struct {
  uint8_t square;
  uint8_t r;
  uint8_t g;
  uint8_t b;
} game_evt_hint_pixel_t;

typedef struct {
  uint8_t square;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t mode;
  uint8_t floor;
  uint16_t period_ms;
  uint16_t phase_ms;
} game_evt_breathe_t;

typedef struct {
  game_event_kind_t kind;
  uint8_t from;
  uint8_t to;
  uint8_t square_count;
  uint8_t end_result;
  /** MOVE_REJECTED: bit0 persist red, bit1 clear pulse */
  uint8_t flags;
  uint8_t restore_op;
  game_restore_plan_t restore;
  uint8_t hint_lift_sq;
  game_evt_hint_pixel_t hints[GAME_EVT_HINT_MAX];
  uint8_t takeback_squares[8];
  uint8_t takeback_count;
  uint16_t fade_ms;
  uint8_t breathe_count;
  game_evt_breathe_t breathes[GAME_EVT_BREATHE_MAX];
} game_event_t;

/** Queue item (≤16 B); full payload lives in game_event slots. */
typedef struct {
  uint8_t kind;
  uint8_t from;
  uint8_t to;
  uint8_t flags;
  uint8_t aux0;
  uint8_t aux1;
  uint16_t fade_ms;
  uint8_t slot;
  uint8_t reserved;
} game_event_msg_t;

#define GAME_EVENT_QUEUE_DEPTH 12
#define GAME_EVENT_NO_SLOT 0xFFu

bool game_event_post(const game_event_t *ev);
/** Reconstruct full event for led_task (uses static slot storage). */
void game_event_resolve(const game_event_msg_t *msg, game_event_t *out);

void game_event_post_move_rejected(uint8_t square, bool persist_red);
void game_event_post_move_rejected_clear(void);
void game_event_post_check(uint8_t king_square);
void game_event_post_game_end(uint8_t king_square, uint8_t end_result);
void game_event_post_restore(const game_restore_plan_t *plan, uint8_t op);
void game_event_post_hints_clear(void);
void game_event_post_required_step_guide(const game_evt_hint_pixel_t *px,
                                         uint8_t count, uint16_t fade_ms);
void game_event_post_required_step_clear(void);
/** Drop the mirror only. No LED event, so a full queue cannot keep a stale guide. */
void game_event_required_step_disarm(void);
bool game_event_required_step_mirror_active(void);
uint8_t game_event_required_step_mirror_copy(game_evt_hint_pixel_t *out,
                                             uint8_t max_out, uint16_t *fade_ms);
/** Built in game_task from position_t and movegen. */
void game_event_publish_lift_hints(uint8_t lift_sq);

/**
 * Board picture. Squares are row*8+col. patch leaves other squares alone.
 * More than GAME_EVT_HINT_MAX squares are split; later chunks patch.
 */
void game_event_post_pixels(const game_evt_hint_pixel_t *px, uint8_t count,
                            const game_evt_breathe_t *breathes, uint8_t breathe_count,
                            uint16_t fade_ms, bool patch, uint8_t hint_lift_sq);
void game_event_post_move_accepted(uint8_t from, uint8_t to);
void game_event_post_takeback(const uint8_t *squares, uint8_t count);
/** led_a/led_b are chain LED indices, matching the animation commands. */
void game_event_post_board_anim(uint8_t anim, uint8_t led_a, uint8_t led_b);
void game_event_post_led_ctrl(uint8_t op, uint8_t a, uint8_t b, uint8_t c,
                              uint8_t d);

void game_led_overlay_report(bool board_show, bool lamp_held, bool gesture_busy,
                             bool booting);
bool game_led_overlay_blocks_restore(void);
bool game_led_lamp_held(void);
bool game_led_gesture_busy(void);
bool game_led_boot_active(void);
uint32_t game_led_board_picture_gen(void);
void game_led_board_picture_bump(void);

#endif /* GAME_EVENTS_H */
