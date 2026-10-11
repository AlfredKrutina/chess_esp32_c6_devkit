#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t hall_i2c_squares_init(void);

/**
 * Verifies that the Hall segment corresponds to I2C (short read pointer 0x00).
 * @param segment_0_to_3 Segment 0…3 (maps to CONFIG_CHESS_HALL_SEGx_ADDR).
 * @param timeout_ms Single transaction timeout.
 */
esp_err_t hall_i2c_squares_probe_segment(uint8_t segment_0_to_3, int timeout_ms);

/** Fill squares_state[64] with 0/1 values ​​from I2C Hall segments (call from squares_scan_all). */
void hall_i2c_squares_fill_state(uint8_t squares_state[64]);

/**
 * True once every square has a baseline that was not taken from a magnet,
 * the trimmed-mean ring is full, and debounce has locked once.
 * Reed builds have no Hall baseline and always return true.
 */
bool hall_i2c_squares_readings_usable(void);

/**
 * Tell Hall which squares the logical game says are empty (1 = empty).
 * Only those squares may be zeroed from live ADC. Occupied squares reuse a
 * stored empty baseline or a provisional copy of confirmed empty squares.
 */
void hall_i2c_squares_set_logical_empty(const uint8_t empty_squares[64]);

/* ========================================================================
 * Hall piece classification (ported from HAL_czechmate_POC single-square
 * algorithm).  Kept separate from engine piece_t; game_task maps and compares.
 * ======================================================================== */

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

/** Stable piece type for the given square (result of POC hysteresis + debounce). */
hall_piece_type_t hall_i2c_squares_get_piece_type(uint8_t square);

/** Stable piece color for the given square (result of POC hysteresis + debounce). */
hall_piece_color_t hall_i2c_squares_get_piece_color(uint8_t square);

/** Human readable type name, "None" for HALL_PIECE_TYPE_NONE. */
const char* hall_i2c_squares_piece_type_name(hall_piece_type_t type);

/** Human readable color name, "----" for HALL_PIECE_COLOR_NONE. */
const char* hall_i2c_squares_piece_color_name(hall_piece_color_t color);

#ifdef __cplusplus
}
#endif
