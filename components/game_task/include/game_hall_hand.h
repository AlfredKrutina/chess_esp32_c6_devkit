/**
 * @brief Lift list for reed switches and UART up/dn.
 *
 * Hall play does not decide from this list. The physical snapshot does.
 * A Hall drop must not rename a held piece from one reading.
 */
#ifndef GAME_HALL_HAND_H
#define GAME_HALL_HAND_H

#include "sdkconfig.h"
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
void game_hall_hand_set_piece(int index, piece_t piece);
void game_hall_hand_note_captured(piece_t piece);
piece_t game_hall_hand_last_captured(void);

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

typedef enum {
  GAME_HALL_PICK_NONE = 0,
  GAME_HALL_PICK_OK = 1,
} game_hall_pick_status_t;

typedef struct {
  int index;
  piece_t piece;
  uint8_t row;
  uint8_t col;
  bool corrected;    /* hand slot relabeled from a plausible Hall reading */
  bool contradicted; /* reading matches nothing plausible; piece is rule fallback */
  bool sensor_hit;   /* type+color named this slot; false when rules chose it */
} game_hall_pick_t;

/**
 * Choose which held piece landed on (to_row, to_col).
 * Unknown or unusable readings stay on the rules: own origin, then a legal
 * destination, then the newest lift. King and queen share one magnet band.
 */
game_hall_pick_status_t game_hall_hand_pick_drop(uint8_t to_row, uint8_t to_col,
                                                game_hall_pick_t *out);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif
