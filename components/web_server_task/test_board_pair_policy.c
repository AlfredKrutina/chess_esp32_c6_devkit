/**
 * @file test_board_pair_policy.c
 * @brief Host test for the pairing window, token slots, and the busy seat.
 * Not part of the ESP-IDF image.
 */
#include "board_pair_policy.h"

#include <stdio.h>
#include <string.h>

static int g_fails;

static void expect_int(const char *name, int got, int want) {
  if (got != want) {
    fprintf(stderr, "FAIL %s: got %d want %d\n", name, got, want);
    g_fails++;
  }
}

static void expect_true(const char *name, bool ok) {
  if (!ok) {
    fprintf(stderr, "FAIL %s\n", name);
    g_fails++;
  }
}

static void fill_token(uint8_t token[BOARD_PAIR_TOKEN_LEN], uint8_t tag) {
  memset(token, tag, BOARD_PAIR_TOKEN_LEN);
  token[0] = tag;
  token[31] = (uint8_t)(0xA0 + tag);
}

int main(void) {
  bool window = false;
  expect_true("window opens", board_pair_window_toggle(&window));
  expect_true("window is open", window);
  expect_true("second press cancels", !board_pair_window_toggle(&window));
  expect_true("window is closed", !window);

  board_pair_book_t book;
  board_pair_book_init(&book);

  uint8_t ta[32], tb[32], tc[32], td[32], zero[32];
  uint8_t addr_a[6] = {1, 2, 3, 4, 5, 6};
  uint8_t addr_b[6] = {6, 5, 4, 3, 2, 1};
  uint8_t addr_d[6] = {8, 7, 6, 5, 4, 3};
  memset(zero, 0, sizeof(zero));
  fill_token(ta, 1);
  fill_token(tb, 2);
  fill_token(tc, 3);
  fill_token(td, 4);

  expect_int("empty token rejected",
             board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_a, zero),
             -1);
  expect_int("slot A",
             board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_a, ta), 0);
  expect_int("slot B",
             board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_b, tb), 1);
  expect_int("slot C wifi",
             board_pair_book_put(&book, BOARD_PAIR_KIND_WIFI, 0, NULL, tc), 2);
  expect_int("fourth replaces oldest",
             board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_d, td), 0);
  expect_true("oldest token is gone",
              board_pair_book_find_token(&book, ta) < 0);
  expect_int("D took slot 0", board_pair_book_find_token(&book, td), 0);
  expect_int("re-pair B keeps its slot",
             board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_b, tb), 1);

  board_pair_book_init(&book);
  board_pair_book_put(&book, BOARD_PAIR_KIND_BLE, 0, addr_a, ta);
  board_pair_book_put(&book, BOARD_PAIR_KIND_WIFI, 0, NULL, tc);

  expect_int("missing token",
             board_pair_book_command(&book, zero, true),
             BOARD_PAIR_CMD_NEED_TOKEN);
  expect_int("A claims the seat",
             board_pair_book_command(&book, ta, true), BOARD_PAIR_CMD_ALLOW);
  expect_int("C is busy while A holds it",
             board_pair_book_command(&book, tc, true), BOARD_PAIR_CMD_BUSY);
  board_pair_book_note_wifi_fd(&book, 7);
  int live_keep[] = {7, 8};
  board_pair_book_prune_wifi_fd(&book, live_keep, 2);
  expect_int("C still busy while the socket lives",
             board_pair_book_command(&book, tc, true), BOARD_PAIR_CMD_BUSY);
  int live_drop[] = {8};
  board_pair_book_prune_wifi_fd(&book, live_drop, 1);
  expect_int("C takes the seat after A drops",
             board_pair_book_command(&book, tc, true), BOARD_PAIR_CMD_ALLOW);

  board_pair_book_set_ble(&book, true, 0, addr_a);
  expect_int("Wi-Fi token is busy while BLE commander is up",
             board_pair_book_command(&book, tc, true), BOARD_PAIR_CMD_BUSY);
  expect_int("the BLE device's own token still writes",
             board_pair_book_command(&book, ta, true), BOARD_PAIR_CMD_ALLOW);
  board_pair_book_set_ble(&book, false, 0, NULL);
  board_pair_book_release_wifi_lease(&book);
  expect_int("after BLE drops a known token can take command",
             board_pair_book_command(&book, tc, true), BOARD_PAIR_CMD_ALLOW);

  if (g_fails != 0) {
    fprintf(stderr, "%d pairing policy check(s) failed\n", g_fails);
    return 1;
  }
  printf("board_pair_policy: ok\n");
  return 0;
}
