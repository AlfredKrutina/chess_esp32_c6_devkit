/**
 * @file game_hall_match.h
 * @brief Map engine piece_t to stable Hall type/color and compare sensors.
 */

#ifndef GAME_HALL_MATCH_H
#define GAME_HALL_MATCH_H

#include "chess_types.h"
#include "hall_i2c_squares.h"
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

bool game_hall_engine_to_sensor(piece_t piece, hall_piece_type_t *out_type,
                                hall_piece_color_t *out_color);

/** True when stable Hall reading matches the engine piece (type + color). */
bool game_hall_stable_reading_matches(piece_t engine_piece,
                                      hall_piece_type_t ht,
                                      hall_piece_color_t hc);

/**
 * Pickup: square may already read empty; uses live stable reading or last
 * reading while the square was occupied.
 */
bool game_hall_validate_pickup_square(uint8_t row, uint8_t col,
                                     piece_t engine_piece);

/** Drop: destination must stably show the piece the engine thinks is moving. */
bool game_hall_validate_drop_square(uint8_t row, uint8_t col,
                                    piece_t moving_piece);

/**
 * Promotion entry (swap_then_choose): Hall may show the pawn or an already
 * swapped Q/R/B/N of the same color before the user presses PROMOTE.
 */
bool game_hall_validate_promotion_entry_drop(uint8_t row, uint8_t col,
                                             piece_t moving_pawn);

/**
 * Castling completion: rook destination and king square must both match Hall.
 */
bool game_hall_validate_castling_rook_drop(uint8_t rook_to_row, uint8_t rook_to_col,
                                           piece_t rook, uint8_t king_row,
                                           uint8_t king_col, piece_t king);

/**
 * Matrix guard / resync: compare one logic square to stable Hall (+ occupancy).
 */
bool game_hall_square_agrees_with_logic(uint8_t square, piece_t engine_piece,
                                        uint8_t sensor_occ);

/**
 * Map debounced Hall type/color to an engine piece for the promoting player.
 * Allows pawn and Q/R/B/N only (same color). Returns false for king, opponent
 * color, or unknown type.
 */
bool game_hall_piece_for_promoting_player(hall_piece_type_t ht,
                                          hall_piece_color_t hc,
                                          player_t promoting_player,
                                          piece_t *out_piece);

/** True when logic board[] matches stable Hall on every square (start setup). */
bool game_hall_logic_matches_stable_sensors(const uint8_t sensor_occ[64]);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_MATCH_H */
