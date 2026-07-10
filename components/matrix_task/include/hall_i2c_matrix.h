#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hall_i2c_matrix_init(void);

/**
 * Ověří, že Hall segment odpovídá na I2C (krátký read pointeru 0x00).
 * @param segment_0_to_3 Segment 0…3 (mapuje na CONFIG_CHESS_HALL_SEGx_ADDR).
 * @param timeout_ms Timeout jedné transakce.
 */
esp_err_t hall_i2c_matrix_probe_segment(uint8_t segment_0_to_3, int timeout_ms);

/** Vyplní matrix_state[64] hodnotami 0/1 z I2C Hall segmentů (volat z matrix_scan_all). */
void hall_i2c_matrix_fill_state(uint8_t matrix_state[64]);

#ifdef __cplusplus
}
#endif
