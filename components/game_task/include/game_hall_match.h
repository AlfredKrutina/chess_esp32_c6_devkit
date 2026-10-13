/**
 * @file game_hall_match.h
 * @brief Compare engine piece_t with the uncalibrated Hall type/color POC.
 *
 * Compiled only for CONFIG_CHESS_MATRIX_INPUT_I2C_HALL. Hall type and color
 * come from hall_i2c_squares and are not a calibrated identification.
 * Callers on the live pickup, drop, castling, and promotion paths must keep
 * using these functions; do not add a second classifier.
 */
#ifndef GAME_HALL_MATCH_H
#define GAME_HALL_MATCH_H

#include "chess_types.h"
#include "hall_i2c_squares.h"
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

/**
 * @brief Map an engine piece to the Hall POC type and color.
 * @param piece Engine piece.
 * @param[out] out_type Hall type. Required.
 * @param[out] out_color Hall color. Required.
 * @return false if either out pointer is NULL or the piece has no mapping.
 */
bool game_hall_engine_to_sensor(piece_t piece, hall_piece_type_t *out_type,
                                hall_piece_color_t *out_color);

/**
 * @brief True when a stable Hall reading matches the engine piece (type and color).
 * @param engine_piece Expected piece.
 * @param ht Stable Hall type.
 * @param hc Stable Hall color.
 * @return true on type and color match.
 */
bool game_hall_stable_reading_matches(piece_t engine_piece,
                                      hall_piece_type_t ht,
                                      hall_piece_color_t hc);

/**
 * @brief Pickup check. The square may already read empty.
 *
 * Uses the live stable reading, or the last reading while the square was occupied.
 * @param row 0…7.
 * @param col 0…7.
 * @param engine_piece Piece the engine believes was lifted.
 * @return true if that piece is acceptable for this lift.
 */
bool game_hall_validate_pickup_square(uint8_t row, uint8_t col,
                                     piece_t engine_piece);

/**
 * @brief Drop check. Destination must stably show the piece being moved.
 * @param row 0…7.
 * @param col 0…7.
 * @param moving_piece Piece the engine is placing.
 * @return true if the stable Hall reading matches moving_piece.
 */
bool game_hall_validate_drop_square(uint8_t row, uint8_t col,
                                    piece_t moving_piece);

/**
 * @brief Promotion entry (swap_then_choose).
 *
 * Hall may show the pawn or an already swapped Q/R/B/N of the same color
 * before the user presses PROMOTE.
 * @param row 0…7.
 * @param col 0…7.
 * @param moving_pawn Pawn the engine is promoting.
 * @return true if the reading is that pawn or a same-color Q/R/B/N.
 */
bool game_hall_validate_promotion_entry_drop(uint8_t row, uint8_t col,
                                             piece_t moving_pawn);

/**
 * @brief Castling completion: rook destination and king square must both match.
 * @param rook_to_row Rook destination row.
 * @param rook_to_col Rook destination column.
 * @param rook Rook piece.
 * @param king_row King square row.
 * @param king_col King square column.
 * @param king King piece.
 * @return true when both squares match Hall.
 */
bool game_hall_validate_castling_rook_drop(uint8_t rook_to_row, uint8_t rook_to_col,
                                           piece_t rook, uint8_t king_row,
                                           uint8_t king_col, piece_t king);

/**
 * @brief Matrix guard / resync: one logic square versus stable Hall and occupancy.
 * @param square Square index 0…63.
 * @param engine_piece Logical piece on that square.
 * @param sensor_occ Occupancy byte from the scan (0 empty, non-zero occupied).
 * @return true when an empty logic square has occupancy 0 and Hall type NONE,
 *         or an occupied square's type and color match the engine piece.
 */
bool game_hall_square_agrees_with_logic(uint8_t square, piece_t engine_piece,
                                        uint8_t sensor_occ);

/**
 * @brief Map a debounced Hall reading to an engine piece for the promoting side.
 *
 * Allows pawn and Q/R/B/N of that color only.
 * @param ht Hall type.
 * @param hc Hall color.
 * @param promoting_player Side that is promoting.
 * @param[out] out_piece Engine piece. Required.
 * @return false for king, the other color, unknown type, or a NULL out_piece.
 */
bool game_hall_piece_for_promoting_player(hall_piece_type_t ht,
                                          hall_piece_color_t hc,
                                          player_t promoting_player,
                                          piece_t *out_piece);

/**
 * @brief True when board[] matches stable Hall on every square.
 * @param sensor_occ 64 occupancy bytes from the scan.
 * @return true on a full match, false if sensor_occ is NULL or any square differs.
 */
bool game_hall_logic_matches_stable_sensors(const uint8_t sensor_occ[64]);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_MATCH_H */
