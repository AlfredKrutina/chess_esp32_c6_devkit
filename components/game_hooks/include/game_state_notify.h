/**
 * @file game_state_notify.h
 * @brief Tenký hook z game_task — notifikace změny stavu (WebSocket push apod.).
 *
 * Slabá implementace v game_state_notify.c; silná v web_server_task.c přepíše ji.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Zavolej po stabilní změně herního stavu (tah, reset, nová hra, …). */
void czechmate_on_game_state_changed(void);

#ifdef __cplusplus
}
#endif
