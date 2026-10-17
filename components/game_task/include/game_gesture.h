/**
 * @file game_gesture.h
 * @brief Game-task side of the V2 button: mode commit, new game, pairing, sleep.
 */
#ifndef GAME_GESTURE_H
#define GAME_GESTURE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void game_gesture_poll(void);

/** Outward lamp wave, then commit HA mode. Same picture as the button. */
void game_gesture_request_lamp_wave(void);

/** Live animation show. Nothing is stored; it ends on reset, stop, or a piece move. */
void game_demo_show_set(bool on);
bool game_demo_show_active(void);

#ifdef __cplusplus
}
#endif

#endif /* GAME_GESTURE_H */
