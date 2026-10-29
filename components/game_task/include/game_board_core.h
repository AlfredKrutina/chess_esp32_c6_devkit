/**
 * @file game_board_core.h
 * @brief Board initialization, FEN load, and piece helpers (internal + public API in game_task.h).
 */

#ifndef GAME_BOARD_CORE_H
#define GAME_BOARD_CORE_H

#include "chess_types.h"
#include <stdbool.h>
#include <stdint.h>

/** Parsed FEN. Moved flags are true when that castling right is gone. */
typedef struct {
  piece_t squares[8][8];
  player_t side;
  bool white_king_moved;
  bool white_rook_a_moved;
  bool white_rook_h_moved;
  bool black_king_moved;
  bool black_rook_a_moved;
  bool black_rook_h_moved;
  bool ep;
  uint8_t ep_row;
  uint8_t ep_col;
  uint8_t victim_row;
  uint8_t victim_col;
  uint32_t halfmove;
  uint32_t move_count;
} game_fen_position_t;

/** Pure parse. Does not touch the live board. */
bool game_parse_fen(const char *fen, game_fen_position_t *out);

bool game_load_position_from_fen(const char *fen, player_t *active_player);

/* Live session side, castling rights, and en passant. Synced with position_t. */
extern game_state_t current_game_state;
extern player_t current_player;
extern uint32_t move_count;
extern bool game_active;

extern bool white_king_moved;
extern bool white_rook_a_moved;
extern bool white_rook_h_moved;
extern bool black_king_moved;
extern bool black_rook_a_moved;
extern bool black_rook_h_moved;

extern bool en_passant_available;
extern uint8_t en_passant_target_row;
extern uint8_t en_passant_target_col;
extern uint8_t en_passant_victim_row;
extern uint8_t en_passant_victim_col;

extern bool piece_moved[8][8];

extern uint32_t moves_without_capture;
extern uint32_t max_moves_without_capture;
extern uint32_t position_history_count;

#endif /* GAME_BOARD_CORE_H */
