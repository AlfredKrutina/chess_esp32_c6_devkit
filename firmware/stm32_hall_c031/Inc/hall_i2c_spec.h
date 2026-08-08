/**
 * @file hall_i2c_spec.h
 * @brief I2C protocol between ESP32 (master) and 4× STM32 — 128 Hall channels → 64 squares.
 *
 * Copy this file into the STM32 project (no ESP-IDF dependency).
 *
 * === Topology ===
 * - 4 identical segments (PCBs), each one I2C slave (own 7-bit address).
 * - Per segment: 16 chess squares × 2 Hall sensors = 32 ADC channels → 32× uint16.
 * - ESP reads all 4 addresses sequentially every scan cycle.
 *
 * === I2C transaction (required for ESP) ===
 * - Master: START + SlaveAddr(W) + 1 byte “pointer register” + REPSTART +
 *   SlaveAddr(R) + read 64 bytes + STOP.
 * - Pointer 0x00 = block of raw samples (see below).
 * - Number format: little-endian (low byte first).
 *
 * === Register / pointer map ===
 * Values match the default CONFIG_CHESS_HALL_REG_START in Kconfig.
 */
#pragma once

#include <stdint.h>

#define HALL_I2C_REG_POINTER_RAW 0x00u

/** Optional on STM: after reading 1 B the master verifies compatibility (ESP does not read yet). */
#define HALL_I2C_REG_POINTER_PROTO_VER 0x01u
#define HALL_I2C_PROTOCOL_VERSION 1u

/** Number of squares on one segment (quarter of the board). */
#define HALL_I2C_FIELDS_PER_SEGMENT 16u
/** Hall sensors per square (differential pair / mux channel pair). */
#define HALL_I2C_SENSORS_PER_FIELD 2u
/** uint16 samples per segment (16 × 2). */
#define HALL_I2C_UINT16_PER_SEGMENT                                                \
  (HALL_I2C_FIELDS_PER_SEGMENT * HALL_I2C_SENSORS_PER_FIELD)
/** Bytes transferred when reading pointer 0x00 (32 × 2). */
#define HALL_I2C_PAYLOAD_BYTES (HALL_I2C_UINT16_PER_SEGMENT * sizeof(uint16_t))

/**
 * === Payload layout 0x00 (64 B) ===
 * Byte offset: field 0..15, sensor 0..1
 *   off = field * 4 + sensor * 2
 *   uint16_t v = buf[off] | (buf[off + 1] << 8);
 *
 * Logical mux channel: sample_index = field * 2 + sensor  (0..31).
 * “Channels 0+1 = square 0, 2+3 = square 1” means field = sample_index / 2.
 *
 * === “field” order on the segment PCB (STM must fill exactly this way) ===
 * Coordinates within the 4×4 quarter (one PCB only):
 *   col = 0..3  — file direction from the inner corner of the quarter outward
 *                 (on the ESP-side segment: col 0 = a, col 3 = d).
 *   row = 0..3  — direction from White’s side toward Black (row 0 = rank 1 on the
 *                 bottom quarters, row 3 = rank 4 on the bottom quarters).
 *   field = row * 4 + col
 *
 * Example segment 0 (BOARD with RESET near ESP, squares a1–d4):
 *   field 0 = a1, 1 = b1, 2 = c1, 3 = d1,
 *   field 4 = a2, … , 15 = d4.
 *
 * The other three segments are the same “row-major” layout in their local 4×4;
 * global a–h / 1–8 mapping is done by ESP firmware (hall_map_segment_field_to_square).
 *
 * === Accuracy recommendations (STM) ===
 * - 12-bit ADC is fine; keep Vref and integration time constant.
 * - Before writing the I2C buffer, apply short averaging (e.g. 8–32 samples
 *   per channel) in one mux cycle.
 * - Update all 64 B atomically (same snapshot) — e.g. double-buffer:
 *   ADC fills A, I2C reads B, then swap.
 * - Hall pairs for one field should come from the same mux “tick”, not from two
 *   different switches, if possible.
 *
 * === Default I2C addresses (7-bit; changeable via straps / EEPROM on segment) ===
 * 0x30, 0x31, 0x32, 0x33 — matches CONFIG_CHESS_HALL_SEGx_ADDR on ESP.
 */

static inline uint16_t hall_i2c_spec_read_le16(const uint8_t *p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void hall_i2c_spec_write_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFFu);
  p[1] = (uint8_t)((v >> 8) & 0xFFu);
}
