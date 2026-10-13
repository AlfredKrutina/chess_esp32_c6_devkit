/**
 * @file board_pair_policy.h
 * @brief Pure commander / token-slot rules. No ESP, NVS, or BLE calls.
 *
 * One writer. Up to three remembered devices. A Wi-Fi command needs a token
 * from one of those slots. While a BLE commander is up, only that device's
 * token may write. Otherwise the token that holds the seat may write, and a
 * different valid token is told the board is busy.
 */
#ifndef BOARD_PAIR_POLICY_H
#define BOARD_PAIR_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOARD_PAIR_SLOT_COUNT 3
#define BOARD_PAIR_TOKEN_LEN 32

#define BOARD_PAIR_KIND_NONE 0
#define BOARD_PAIR_KIND_BLE 1
#define BOARD_PAIR_KIND_WIFI 2

#define BOARD_PAIR_CMD_ALLOW 0
#define BOARD_PAIR_CMD_NEED_TOKEN 1
#define BOARD_PAIR_CMD_BUSY 2

typedef struct {
  uint8_t kind;
  uint8_t addr_type;
  uint8_t addr[6];
  uint8_t token[BOARD_PAIR_TOKEN_LEN];
  uint32_t seq;
} board_pair_slot_t;

typedef struct {
  board_pair_slot_t slots[BOARD_PAIR_SLOT_COUNT];
  uint32_t next_seq;
  bool ble_up;
  uint8_t ble_addr_type;
  uint8_t ble_addr[6];
  int wifi_slot;
  int wifi_fd;
} board_pair_book_t;

void board_pair_book_init(board_pair_book_t *book);

/**
 * Remember a token. A BLE identity that is already stored keeps its slot.
 * A fourth device replaces the oldest slot.
 * @return slot index, or -1 if the token is empty.
 */
int board_pair_book_put(board_pair_book_t *book, uint8_t kind,
                        uint8_t addr_type, const uint8_t addr[6],
                        const uint8_t token[BOARD_PAIR_TOKEN_LEN]);

int board_pair_book_find_token(const board_pair_book_t *book,
                               const uint8_t token[BOARD_PAIR_TOKEN_LEN]);

/**
 * Decide a Wi-Fi command. When @p claim is true an allowed free seat is taken
 * by this token.
 */
int board_pair_book_command(board_pair_book_t *book,
                            const uint8_t token[BOARD_PAIR_TOKEN_LEN],
                            bool claim);

void board_pair_book_set_ble(board_pair_book_t *book, bool up,
                             uint8_t addr_type, const uint8_t addr[6]);

void board_pair_book_note_wifi_fd(board_pair_book_t *book, int fd);

/** Socket gone. Drops the Wi-Fi seat when BLE is not the commander. */
void board_pair_book_release_wifi_fd(board_pair_book_t *book, int fd);

/** Drop a POST lease that has no live socket. A held socket stays. */
void board_pair_book_release_wifi_lease(board_pair_book_t *book);

void board_pair_book_prune_wifi_fd(board_pair_book_t *book, const int *live,
                                   int live_count);

/** Second call while open cancels. @return true when the window is open. */
bool board_pair_window_toggle(bool *open);

#endif /* BOARD_PAIR_POLICY_H */
