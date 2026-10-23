/**
 * @brief Pieces currently off the board, in lift order.
 *
 * One slot is a normal move. Two slots cover a capture in either order,
 * castling, or two same-color pieces. The drop path picks which slot landed.
 */
#ifndef GAME_HALL_HAND_H
#define GAME_HALL_HAND_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

#define GAME_HALL_HAND_MAX 4

void game_hall_hand_reset(void);
bool game_hall_hand_push(piece_t piece, uint8_t row, uint8_t col);
int game_hall_hand_count(void);
bool game_hall_hand_get(int index, piece_t *piece, uint8_t *row, uint8_t *col);
void game_hall_hand_remove_at(int index);
void game_hall_hand_remove_origin(uint8_t row, uint8_t col);
void game_hall_hand_note_captured(piece_t piece);
piece_t game_hall_hand_last_captured(void);

#endif
