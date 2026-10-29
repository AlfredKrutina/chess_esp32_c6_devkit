/**
 * @file game_cmd.h
 * @brief Tagged game command queued by game_submit.
 */
#ifndef GAME_CMD_H
#define GAME_CMD_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Who the sender expects to be on move. */
#define GAME_RESIGN_SIDE_TO_MOVE 0U
#define GAME_RESIGN_SIDE_WHITE 1U
#define GAME_RESIGN_SIDE_BLACK 2U

typedef enum {
  GCMD_NEW_GAME = 0,
  GCMD_NEW_GAME_FEN,
  GCMD_UNDO,
  GCMD_MOVE,
  GCMD_BOARD_LIFT,
  GCMD_BOARD_DROP,
  GCMD_BOARD_GUARD,
  GCMD_RESIGN,
  GCMD_AGREE_DRAW,
  GCMD_TIMER_SET,
  GCMD_TIMER_PAUSE,
  GCMD_TIMER_RESUME,
  GCMD_TIMER_RESET,
  GCMD_MODE_PUZZLE,
  GCMD_MODE_OPENING,
  GCMD_MODE_TUTORIAL,
  GCMD_PROMOTION_CHOICE,
  GCMD_WEB_HINT
} game_cmd_kind_t;

typedef struct {
  char from[8];
  char to[8];
  char promotion; /* 0 or q, r, b, n */
} game_cmd_move_fields_t;

typedef struct {
  char square[8];
} game_cmd_lift_fields_t;

typedef struct {
  char from[8];
  char to[8];
  uint8_t has_from; /* 1 reed, 0 Hall place */
} game_cmd_drop_fields_t;

typedef struct {
  uint32_t lifted_low;
  uint32_t lifted_high;
  uint32_t dropped_low;
  uint32_t dropped_high;
  uint8_t action; /* 1 hold, 0 release */
} game_cmd_guard_fields_t;

typedef struct {
  char fen[120];
} game_cmd_fen_fields_t;

typedef struct {
  uint8_t side; /* GAME_RESIGN_SIDE_* */
} game_cmd_resign_fields_t;

typedef struct {
  uint8_t time_control_type; /* 0..14, 14 = custom */
  uint32_t custom_minutes;
  uint32_t custom_increment;
} game_cmd_timer_fields_t;

typedef enum {
  PUZZLE_ACT_CANCEL = 0,
  PUZZLE_ACT_START = 1,
  PUZZLE_ACT_PREPARE = 2,
  PUZZLE_ACT_APPLY_SETUP = 3, /* parser maps former 210 */
  PUZZLE_ACT_START_STAGED = 4 /* parser maps former 211 */
} puzzle_act_t;

typedef enum {
  OPENING_ACT_CANCEL = 0,
  OPENING_ACT_START = 1,
  OPENING_ACT_HINT = 2,
  OPENING_ACT_CHECKPOINT = 3
} opening_act_t;

typedef enum {
  TUTORIAL_ACT_START = 0,
  TUTORIAL_ACT_CANCEL = 1,
  TUTORIAL_ACT_FINISH = 2
} tutorial_act_t;

typedef struct {
  uint8_t act;        /* puzzle_act_t, opening_act_t, or tutorial_act_t */
  uint8_t builtin_id; /* 1..5 for START and PREPARE, else 0 */
} game_cmd_mode_fields_t;

typedef struct {
  char piece; /* q, r, b, n */
} game_cmd_promo_fields_t;

typedef struct {
  uint8_t clear; /* 1 = refresh idle board (hint clear) */
  uint8_t from_sq; /* 0-63, or 255 if omitted */
  uint8_t to_sq; /* 0-63 */
} game_cmd_hint_fields_t;

/**
 * Queued command. response_queue, event_ms, and is_demo are the same
 * transport the board and HTTP move reply already use (scan time, demo,
 * optional reply). They are not timer_data and not old command numbers.
 */
typedef struct {
  game_cmd_kind_t kind;
  QueueHandle_t response_queue;
  uint32_t event_ms;
  uint8_t is_demo;
  union {
    game_cmd_move_fields_t move;
    game_cmd_lift_fields_t lift;
    game_cmd_drop_fields_t drop;
    game_cmd_guard_fields_t guard;
    game_cmd_fen_fields_t fen;
    game_cmd_resign_fields_t resign;
    game_cmd_timer_fields_t timer;
    game_cmd_mode_fields_t mode;
    game_cmd_promo_fields_t promo;
    game_cmd_hint_fields_t hint;
  } u;
} game_cmd_t;

static inline const char *game_cmd_from(const game_cmd_t *cmd) {
  if (cmd == NULL) {
    return "";
  }
  switch (cmd->kind) {
  case GCMD_BOARD_LIFT:
    return cmd->u.lift.square;
  case GCMD_BOARD_DROP:
    return cmd->u.drop.from;
  case GCMD_MOVE:
    return cmd->u.move.from;
  default:
    return "";
  }
}

static inline const char *game_cmd_to(const game_cmd_t *cmd) {
  if (cmd == NULL) {
    return "";
  }
  switch (cmd->kind) {
  case GCMD_BOARD_DROP:
    return cmd->u.drop.to;
  case GCMD_MOVE:
    return cmd->u.move.to;
  default:
    return "";
  }
}

#ifdef __cplusplus
}
#endif

#endif /* GAME_CMD_H */
