/**
 * @file game_mode.h
 * @brief One active session mode: puzzle, opening, tutorial, or guard.
 *
 * NULL means the board plays as it does with no mode installed.
 */

#ifndef GAME_MODE_H
#define GAME_MODE_H

#include "position.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct game_mode {
  void (*on_lift)(const position_t *pos, uint8_t square);
  void (*on_drop)(const position_t *pos, uint8_t from, uint8_t to, bool has_from);
  void (*on_move_applied)(const position_t *pos, position_move_t mv);
  void (*leds)(const position_t *pos);
} game_mode_t;

void game_mode_set(const game_mode_t *mode);
const game_mode_t *game_mode_current(void);

/** Drop `mode` if it is active or waiting under the guard. */
void game_mode_clear(const game_mode_t *mode);

/** Guard becomes the only active mode and keeps the previous one. */
void game_mode_suspend_for(const game_mode_t *mode);
void game_mode_resume(const game_mode_t *mode);

/** on_drop asks the board machine to reject this drop. */
void game_mode_reject_drop(void);
bool game_mode_take_drop_reject(void);

void game_mode_call_lift(const position_t *pos, uint8_t square);
void game_mode_call_drop(const position_t *pos, uint8_t from, uint8_t to,
                         bool has_from);
void game_mode_call_move_applied(const position_t *pos, position_move_t mv);
void game_mode_call_leds(const position_t *pos);

#endif /* GAME_MODE_H */
