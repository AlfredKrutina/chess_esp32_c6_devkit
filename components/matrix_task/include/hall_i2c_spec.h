/**
 * @file hall_i2c_spec.h
 * @brief I2C standard between ESP32 (master) and 4× STM32 — 128 Hall channels → 64 fields.
 *
 * Copy this file to the STM32 project (without dependency on ESP-IDF).
 *
 * === Topology ===
 * - 4 identical segments (planes), each one I2C slave (own 7-bit address).
 * - On a segment 16 checkerboards × 2 Hall sensors = 32 ADC channels → 32× uint16.
 * - ESP reads sequentially all 4 addresses every scan cycle.
 *
 * === I2C transaction (required for ESP) ===
 * - Master: START + SlaveAddr(W) + 1 byte "pointer register" + REPSTART +
 * SlaveAddr(R) + read 64 bytes + STOP.
 * - Pointer 0x00 = block of raw samples (see below).
 * - Number format: little-endian (low byte first).
 *
 * === Register / pointer map ===
 * Values are the same as the default CONFIG_CHESS_HALL_REG_START in Kconfig.
 */
#pragma once

#include <stdint.h>

#define HALL_I2C_REG_POINTER_RAW 0x00u

/** Optional on STM: after reading 1 B, master verifies compatibility (ESP does not read yet). */
#define HALL_I2C_REG_POINTER_PROTO_VER 0x01u
#define HALL_I2C_PROTOCOL_VERSION 1u

/** Number of squares on one segment (a quarter of the board). */
#define HALL_I2C_FIELDS_PER_SEGMENT 16u
/** Hall sensors per array (differential pair / pair of mux channels). */
#define HALL_I2C_SENSORS_PER_FIELD 2u
/** uint16 samples per segment (16 × 2). */
#define HALL_I2C_UINT16_PER_SEGMENT                                                \
  (HALL_I2C_FIELDS_PER_SEGMENT * HALL_I2C_SENSORS_PER_FIELD)
/** Bytes transferred when reading pointer 0x00 (32 × 2). */
#define HALL_I2C_PAYLOAD_BYTES (HALL_I2C_UINT16_PER_SEGMENT * sizeof(uint16_t))

/**
 * === Payload layout 0x00 (64 B) ===
 * Offset in bytes: field 0..15, sensor 0..1
 * off = field * 4 + sensor * 2
 * uint16_t v = buf[off] | (buf[off + 1] << 8);
 *
 * Logical mux channel: sample_index = field * 2 + sensor (0..31).
 * "Channels 0+1 = field 0, 2+3 = field 1" corresponds to field = sample_index / 2.
 *
 * === The order of the "field" on the segment board (STM must perform exactly like this) ===
 * Coordinates within a 4×4 quarter (only one plane):
 * col = 0..3 — file direction from the inner corner of the quarter to the outer
 * (for the ESP segment: col 0 = a, col 3 = d).
 * row = 0..3 — direction from the white player to black (row 0 = row 1 for the bottom
 * quarters, row 3 = row 4 for lower quarters).
 * field = row * 4 + col
 *
 * Example segment 0 (BOARD with RESET at ESP, checkerboard a1–d4):
 * field 0 = a1, 1 = b1, 2 = c1, 3 = d1,
 * field 4 = a2, … , 15 = d4.
 *
 * The other three segments are equally "row-major" in their local 4×4;
 * global mapping a–h / 1–8 is solved by ESP firmware (hall_map_segment_field_to_square).
 *
 * === Accuracy Recommendation (STM) ===
 * - ADC 12 bits is OK; keep Vref and integration time constant.
 * - Before writing to the I2C buffer, arrange a short averaging (e.g. 8-32 samples
 * per channel) in one mux cycle.
 * - Update the entire 64 B atomically (same snapshot) — e.g. double buffer:
 * ADC fills A, I2C reads B, then swap.
 * - Hall pairs for one field should be from the same "tick" mux, not from two different ones
 * switching if applicable.
 *
 * === Default I2C addresses (7 bits, can be changed straps / EEPROM per segment) ===
 * 0x30, 0x31, 0x32, 0x33 — corresponds to CONFIG_CHESS_HALL_SEGx_ADDR on ESP.
 */

static inline uint16_t hall_i2c_spec_read_le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void hall_i2c_spec_write_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
