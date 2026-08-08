/**
 * @file game_state_notify.c
 * @brief Default weak implementation (no web_server dependency).
 */
#include "game_state_notify.h"

void __attribute__((weak)) czechmate_on_game_state_changed(void) {}
