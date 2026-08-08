/**
 * @file game_state_notify.h
 * @brief Thin hook from game_task — status change notification (WebSocket push, etc.).
 *
 * Poor implementation in game_state_notify.c; strong in web_server_task.c overrides it.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Call after a stable game state change (turn, reset, new game, ...). */
void czechmate_on_game_state_changed(void);

#ifdef __cplusplus
}
#endif
