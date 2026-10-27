/**
 * @file game_hall_position.h
 * @brief Last legal position compared with the live Hall snapshot.
 *
 * Hall only. board[][] stays the last legal position. Moves, takeback, and
 * restore are decided from that diff. Pickup and drop events do not commit.
 */
#ifndef GAME_HALL_POSITION_H
#define GAME_HALL_POSITION_H

#include "sdkconfig.h"
#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL

typedef enum {
  GAME_HALL_POS_EQUAL = 0,
  GAME_HALL_POS_ONE_LEGAL,
  GAME_HALL_POS_PREVIOUS,
  GAME_HALL_POS_CAPTURE_PARTIAL,
  GAME_HALL_POS_MOVE_PARTIAL,
  GAME_HALL_POS_RESTORE,
} game_hall_pos_class_t;

/** Copy board[][] into the last-legal snapshot. Call after a real position change. */
void game_hall_snapshot_last_valid(void);

/**
 * True when occupancy matches and no settled reading contradicts expected.
 * Empty type, unsettled readings, and the shared king/queen band do not fail.
 * No I2C. False when readings are not usable or expected is NULL.
 */
bool game_hall_positions_equal(const piece_t expected[8][8]);

/** Squares that differ from the snapshot, capped so callers can stop early. */
int game_hall_position_mismatch_count(void);

/**
 * Classify the cached sensors. One legal move commits only after the
 * occupancy stays still. A single lifted own piece gets destination LEDs
 * on the first diff. Does not read the pickup/drop queue.
 */
void game_hall_position_service(void);

/**
 * Matrix pickup/drop woke the game task. Record an identity hint and
 * classify now so a single lift can light destinations in this command.
 */
void game_hall_position_on_matrix_event(const chess_move_command_t *cmd);

/** True while lift or restore LEDs own the board. Idle redraw must wait. */
bool game_hall_position_blocks_idle_leds(void);

#endif /* CONFIG_CHESS_MATRIX_INPUT_I2C_HALL */

#endif /* GAME_HALL_POSITION_H */
