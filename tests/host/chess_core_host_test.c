/**
 * Host-only chess_core smoke tests (no ESP-IDF / FreeRTOS).
 * Compile from repo root; see plan phase 6.
 */
#include "position.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQ(r, c) ((uint8_t)((r)*8u + (c)))

static int g_pass;
static int g_fail;

static void expect_true(int cond, const char *name) {
  if (cond) {
    g_pass++;
  } else {
    g_fail++;
    fprintf(stderr, "FAIL: %s\n", name);
  }
}

static void expect_false(int cond, const char *name) {
  expect_true(!cond, name);
}

static position_move_t mv(uint8_t from, uint8_t to, position_promo_t promo) {
  position_move_t m;
  m.from = from;
  m.to = to;
  m.promo = promo;
  return m;
}

static void test_checkmate(void) {
  position_t p;
  position_status_t st;

  expect_true(position_from_fen(&p, "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"),
              "checkmate fen parse");
  st = position_status(&p);
  expect_true(st.checkmate, "fools mate is checkmate");
  expect_true(st.in_check, "checkmate side in check");
}

static void test_castling(void) {
  position_t p;
  position_move_t castle_ok;
  position_move_t castle_bad;

  expect_true(
      position_from_fen(&p, "rnbqkb1r/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQK2R w KQkq - 0 1"),
      "castling setup fen");
  castle_ok = mv(SQ(0, 4), SQ(0, 6), POS_PROMO_NONE);
  expect_true(position_legal(&p, castle_ok), "white O-O legal");
  expect_true(position_apply(&p, castle_ok), "white O-O apply");
  expect_true(p.board[0][6] == POS_WHITE_KING, "king on g1");
  expect_true(p.board[0][5] == POS_WHITE_ROOK, "rook on f1");

  expect_true(
      position_from_fen(&p, "rnbqkb1r/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQK2R w KQkq - 0 1"),
      "castling illegal setup fen");
  castle_bad = mv(SQ(0, 4), SQ(0, 2), POS_PROMO_NONE);
  expect_false(position_legal(&p, castle_bad), "white O-O-O illegal (b1 blocked by knight)");
  expect_false(position_apply(&p, castle_bad), "white O-O-O apply rejected");
}

static void test_en_passant(void) {
  position_t p;
  position_move_t ep_ok;
  position_move_t ep_bad;

  expect_true(position_from_fen(&p, "rnbqkbnr/ppp2ppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3"),
              "en passant setup fen");
  expect_true(p.ep_square == SQ(5, 3), "ep square d6 set");
  ep_ok = mv(SQ(4, 4), SQ(5, 3), POS_PROMO_NONE);
  expect_true(position_legal(&p, ep_ok), "exd6 en passant legal");
  expect_true(position_apply(&p, ep_ok), "en passant apply");
  expect_true(p.board[5][3] == POS_WHITE_PAWN, "pawn on d6");
  expect_true(p.board[4][3] == POS_EMPTY, "captured pawn removed from d5");

  expect_true(position_from_fen(&p, "rnbqkbnr/ppp2ppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 3"),
              "no ep rights fen");
  ep_bad = mv(SQ(4, 4), SQ(5, 3), POS_PROMO_NONE);
  expect_false(position_legal(&p, ep_bad), "en passant illegal without ep square");
}

static void test_promotion(void) {
  position_t p;
  position_move_t promo;

  expect_true(position_from_fen(&p, "8/4P3/8/8/8/8/8/4k2K w - - 0 1"), "promotion setup fen");
  promo = mv(SQ(6, 4), SQ(7, 4), POS_PROMO_Q);
  expect_true(position_legal(&p, promo), "e7-e8Q legal");
  expect_true(position_apply(&p, promo), "promotion apply");
  expect_true(p.board[7][4] == POS_WHITE_QUEEN, "white queen on e8");
}

int main(void) {
  g_pass = 0;
  g_fail = 0;

  test_checkmate();
  test_castling();
  test_en_passant();
  test_promotion();

  printf("chess_core host: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
