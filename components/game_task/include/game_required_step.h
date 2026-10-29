/**
 * @file game_required_step.h
 * @brief Mandatory physical steps: persistent LEDs and idle timeout.
 */

#ifndef GAME_REQUIRED_STEP_H
#define GAME_REQUIRED_STEP_H

#include <stdbool.h>
#include <stdint.h>

void game_required_step_clear(void);
void game_required_step_refresh(void);
void game_required_step_service(void);
bool game_required_step_blocks_restore(void);

#endif /* GAME_REQUIRED_STEP_H */
