#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hall_i2c_matrix_init(void);

/** Vyplní matrix_state[64] hodnotami 0/1 z I2C Hall segmentů (volat z matrix_scan_all). */
void hall_i2c_matrix_fill_state(uint8_t matrix_state[64]);

#ifdef __cplusplus
}
#endif
