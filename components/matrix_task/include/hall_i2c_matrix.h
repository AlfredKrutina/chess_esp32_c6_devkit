#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hall_i2c_matrix_init(void);

/**
 * Verifies that the Hall segment corresponds to I2C (short read pointer 0x00).
 * @param segment_0_to_3 Segment 0…3 (maps to CONFIG_CHESS_HALL_SEGx_ADDR).
 * @param timeout_ms Single transaction timeout.
 */
esp_err_t hall_i2c_matrix_probe_segment(uint8_t segment_0_to_3, int timeout_ms);

/** Fill matrix_state[64] with 0/1 values ​​from I2C Hall segments (call from matrix_scan_all). */
void hall_i2c_matrix_fill_state(uint8_t matrix_state[64]);

#ifdef __cplusplus
}
#endif
