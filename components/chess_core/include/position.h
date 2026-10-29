/**
 * @file position.h
 * @brief Standalone chess position. No FreeRTOS, LED, queues, or game_task.
 */

#ifndef POSITION_H
#define POSITION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Numeric values match piece_t so a publish can copy board bytes. */
typedef enum {
  POS_EMPTY = 0,
  POS_WHITE_PAWN = 1,
  POS_WHITE_KNIGHT = 2,
  POS_WHITE_BISHOP = 3,
  POS_WHITE_ROOK = 4,
  POS_WHITE_QUEEN = 5,
  POS_WHITE_KING = 6,
  POS_BLACK_PAWN = 7,
  POS_BLACK_KNIGHT = 8,
  POS_BLACK_BISHOP = 9,
  POS_BLACK_ROOK = 10,
  POS_BLACK_QUEEN = 11,
  POS_BLACK_KING = 12
} position_piece_t;

#define POS_CASTLE_WK 0x01u
#define POS_CASTLE_WQ 0x02u
#define POS_CASTLE_BK 0x04u
#define POS_CASTLE_BQ 0x08u
#define POSITION_SQUARE_NONE 0xFFu
#define POSITION_HIST_CAP 200 /* same count as MAX_MOVES_HISTORY */

typedef enum {
  POS_PROMO_NONE = 0,
  POS_PROMO_Q = 1,
  POS_PROMO_R = 2,
  POS_PROMO_B = 3,
  POS_PROMO_N = 4
} position_promo_t;

typedef struct {
  uint8_t from; /* 0..63, same index squares_task sends */
  uint8_t to;
  position_promo_t promo;
} position_move_t;

typedef struct {
  position_move_t mv;
  uint8_t captured;
  uint8_t castling_before;
  uint8_t ep_before;
  uint8_t side_before;
  uint16_t halfmove_before;
  uint16_t fullmove_before;
} position_hist_t;

typedef struct position {
  uint8_t board[8][8];
  uint8_t side;      /* 0 white, 1 black, same numbers as player_t */
  uint8_t castling;  /* POS_CASTLE_* */
  uint8_t ep_square; /* 0..63 or POSITION_SQUARE_NONE */
  uint16_t halfmove;
  uint16_t fullmove;
  position_hist_t hist[POSITION_HIST_CAP];
  uint16_t hist_len;
} position_t;

typedef struct {
  bool in_check;
  bool checkmate;
  bool stalemate;
  bool fifty_move;
  bool repetition;
  bool insufficient_material;
} position_status_t;

void position_reset(position_t *p);
bool position_from_fen(position_t *p, const char *fen);
bool position_to_fen(const position_t *p, char *out, size_t out_cap);
bool position_legal(const position_t *p, position_move_t mv);
bool position_apply(position_t *p, position_move_t mv);
bool position_undo(position_t *p);
position_status_t position_status(const position_t *p);

#endif /* POSITION_H */
