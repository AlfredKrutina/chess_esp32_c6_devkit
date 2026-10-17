/**
 * Brownout streak: after more than three supply sags in a row, LED output
 * is half of the brightness the rest of the firmware asked for. A real
 * power-on (unplug and plug back in) clears the cap and tries full again.
 */
#pragma once

#include "esp_system.h"
#include <stdbool.h>
#include <stdint.h>

void power_guard_on_boot(esp_reset_reason_t reason);

bool power_guard_output_half(void);

/** 50 while the cap is on, otherwise 100. */
uint8_t power_guard_scale_percent(void);

uint8_t power_guard_brownout_streak(void);

uint16_t power_guard_brownout_total(void);
