/**
 * @file game_state_notify.c
 * @brief Výchozí slabá implementace (bez závislosti na web_server).
 */
#include "game_state_notify.h"

void __attribute__((weak)) czechmate_on_game_state_changed(void) {}
