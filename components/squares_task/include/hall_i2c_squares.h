/**
 * @file hall_i2c_squares.h
 * @brief I2C Hall occupancy for the 64 squares, plus an uncalibrated piece-ID POC.
 *
 * Occupancy (0/1) is the product scan path used by squares_task. Piece type and
 * color are a separate proof-of-concept ported from HAL_czechmate_POC. Those
 * thresholds are not a calibrated product classifier; do not treat a Hall
 * type/color as ground truth. Reed builds (Hall Kconfig off) keep the same
 * entry points and return empty/usable defaults.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install the Hall I2C master and clear per-square POC state.
 *
 * Reed builds do not touch the bus.
 * @return ESP_OK, or the I2C driver error. Reed builds return ESP_OK.
 */
esp_err_t hall_i2c_squares_init(void);

/**
 * @brief Probe one Hall segment with a short read of pointer 0x00.
 * @param segment_0_to_3 Segment 0…3 (maps to CONFIG_CHESS_HALL_SEGx_ADDR).
 * @param timeout_ms Single transaction timeout. Non-positive becomes 80 ms.
 * @return ESP_OK if the slave answered, ESP_ERR_INVALID_ARG if the segment
 *         is out of range, ESP_ERR_INVALID_STATE if init has not run,
 *         ESP_ERR_NOT_SUPPORTED on a reed build, otherwise the I2C error.
 */
esp_err_t hall_i2c_squares_probe_segment(uint8_t segment_0_to_3, int timeout_ms);

/**
 * @brief Fill squares_state[64] with occupancy 0 (empty) or 1 (occupied).
 *
 * Called from the squares scan. A NULL buffer is ignored. Reed builds zero
 * the buffer and do not read I2C.
 * @param squares_state 64 occupancy bytes, index = square.
 */
void hall_i2c_squares_fill_state(uint8_t squares_state[64]);

/** How many segments this build reads (1…4). */
unsigned hall_i2c_squares_segment_count(void);

/**
 * One raw STM payload: 32 little-endian samples (16 fields × 2 sensors).
 * addr_out is set even when the read fails, so the console can show 0x30–0x33.
 */
esp_err_t hall_i2c_squares_read_segment_raw(uint8_t segment_0_to_3,
                                            uint8_t *addr_out,
                                            uint16_t raw[32]);

/**
 * @brief Whether occupancy readings are safe to compare with the logical board.
 *
 * Hall: true once every square has a baseline that was not taken from a magnet,
 * the trimmed-mean ring is full, and debounce has locked once.
 * Reed builds have no Hall baseline and always return true.
 * @return true when a scan may be trusted for guard / restore.
 */
bool hall_i2c_squares_readings_usable(void);

/**
 * @brief Tell Hall which squares the logical game says are empty.
 *
 * Only those squares may be zeroed from live ADC. Occupied squares reuse a
 * stored empty baseline or a provisional copy of confirmed empty squares.
 * Reed builds ignore the buffer.
 * @param empty_squares 64 bytes, 1 = logically empty. NULL is ignored.
 */
void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64]);

/**
 * After boot, light a1, h1, h8, a8 one at a time. Place a piece on the lit
 * square and leave it. Logs which segment/field moved. Does not change the map.
 */
void hall_i2c_squares_corner_probe(void);

/* Debug: light the square whose Hall moved off the fresh rest value.
 * Comment this out to return to the normal game scan. */
#define HALL_DEBUG_LIGHT_WHERE_PIECE 1

void hall_i2c_squares_debug_light_pieces(void);

void hall_i2c_squares_request_rest_measure(void);

/* Piece classification POC. Kept separate from engine piece_t; game_task
 * maps and compares. Thresholds are the POC constants in hall_i2c_squares.c,
 * not a calibrated measurement. */

typedef enum {
    HALL_PIECE_TYPE_NONE = 0,
    HALL_PIECE_TYPE_PAWN,
    HALL_PIECE_TYPE_KNIGHT,
    HALL_PIECE_TYPE_BISHOP,
    HALL_PIECE_TYPE_ROOK,
    HALL_PIECE_TYPE_QUEEN,
    HALL_PIECE_TYPE_KING
} hall_piece_type_t;

typedef enum {
    HALL_PIECE_COLOR_NONE = 0,
    HALL_PIECE_COLOR_WHITE,
    HALL_PIECE_COLOR_BLACK
} hall_piece_color_t;

/**
 * @brief Last debounced POC piece type for one square.
 *
 * Uncalibrated. Reed builds and an out-of-range index return NONE.
 * @param square Square index 0…63.
 * @return Debounced type, or HALL_PIECE_TYPE_NONE.
 */
hall_piece_type_t hall_i2c_squares_get_piece_type(uint8_t square);

/**
 * @brief Last debounced POC piece color for one square.
 *
 * Uncalibrated. Reed builds and an out-of-range index return NONE.
 * @param square Square index 0…63.
 * @return Debounced color, or HALL_PIECE_COLOR_NONE.
 */
hall_piece_color_t hall_i2c_squares_get_piece_color(uint8_t square);

/**
 * @brief Name for a POC type.
 * @param type Value from hall_piece_type_t.
 * @return "Pawn" … "King", or "None".
 */
const char* hall_i2c_squares_piece_type_name(hall_piece_type_t type);

/**
 * @brief Name for a POC color.
 * @param color Value from hall_piece_color_t.
 * @return "WHITE", "BLACK", or "----".
 */
const char* hall_i2c_squares_piece_color_name(hall_piece_color_t color);

#ifdef __cplusplus
}
#endif
