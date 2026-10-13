/**
 * @file board_pair_policy.c
 * @brief Commander seat and the three token slots. See board_pair_policy.h.
 */
#include "board_pair_policy.h"

#include <string.h>

static bool token_empty(const uint8_t token[BOARD_PAIR_TOKEN_LEN]) {
  uint8_t acc = 0;
  for (int i = 0; i < BOARD_PAIR_TOKEN_LEN; i++) {
    acc |= token[i];
  }
  return acc == 0;
}

static bool token_eq(const uint8_t *a, const uint8_t *b) {
  uint8_t diff = 0;
  for (int i = 0; i < BOARD_PAIR_TOKEN_LEN; i++) {
    diff |= (uint8_t)(a[i] ^ b[i]);
  }
  return diff == 0;
}

static bool ble_addr_same(const board_pair_slot_t *slot, uint8_t addr_type,
                          const uint8_t addr[6]) {
  return slot->kind == BOARD_PAIR_KIND_BLE && slot->addr_type == addr_type &&
         addr != NULL && memcmp(slot->addr, addr, 6) == 0;
}

void board_pair_book_init(board_pair_book_t *book) {
  if (book == NULL) {
    return;
  }
  memset(book, 0, sizeof(*book));
  book->wifi_slot = -1;
  book->wifi_fd = -1;
}

int board_pair_book_put(board_pair_book_t *book, uint8_t kind,
                        uint8_t addr_type, const uint8_t addr[6],
                        const uint8_t token[BOARD_PAIR_TOKEN_LEN]) {
  if (book == NULL || token == NULL || token_empty(token)) {
    return -1;
  }
  if (kind != BOARD_PAIR_KIND_BLE && kind != BOARD_PAIR_KIND_WIFI) {
    return -1;
  }

  int dest = -1;
  if (kind == BOARD_PAIR_KIND_BLE && addr != NULL) {
    for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
      if (ble_addr_same(&book->slots[i], addr_type, addr)) {
        dest = i;
        break;
      }
    }
  }
  if (dest < 0) {
    for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
      if (book->slots[i].kind == BOARD_PAIR_KIND_NONE) {
        dest = i;
        break;
      }
    }
  }
  if (dest < 0) {
    uint32_t oldest = UINT32_MAX;
    for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
      if (book->slots[i].seq < oldest) {
        oldest = book->slots[i].seq;
        dest = i;
      }
    }
  }
  if (dest < 0) {
    return -1;
  }

  board_pair_slot_t *slot = &book->slots[dest];
  memset(slot, 0, sizeof(*slot));
  slot->kind = kind;
  slot->addr_type = addr_type;
  if (addr != NULL) {
    memcpy(slot->addr, addr, 6);
  }
  memcpy(slot->token, token, BOARD_PAIR_TOKEN_LEN);
  book->next_seq++;
  slot->seq = book->next_seq;
  if (book->wifi_slot == dest) {
    book->wifi_slot = -1;
    book->wifi_fd = -1;
  }
  return dest;
}

int board_pair_book_find_token(const board_pair_book_t *book,
                               const uint8_t token[BOARD_PAIR_TOKEN_LEN]) {
  if (book == NULL || token == NULL || token_empty(token)) {
    return -1;
  }
  int found = -1;
  for (int i = 0; i < BOARD_PAIR_SLOT_COUNT; i++) {
    if (book->slots[i].kind == BOARD_PAIR_KIND_NONE) {
      continue;
    }
    if (token_eq(book->slots[i].token, token)) {
      found = i;
    }
  }
  return found;
}

static bool slot_is_ble_commander(const board_pair_book_t *book, int slot) {
  if (!book->ble_up || slot < 0 || slot >= BOARD_PAIR_SLOT_COUNT) {
    return false;
  }
  return ble_addr_same(&book->slots[slot], book->ble_addr_type, book->ble_addr);
}

int board_pair_book_command(board_pair_book_t *book,
                            const uint8_t token[BOARD_PAIR_TOKEN_LEN],
                            bool claim) {
  if (book == NULL) {
    return BOARD_PAIR_CMD_NEED_TOKEN;
  }
  int slot = board_pair_book_find_token(book, token);
  if (slot < 0) {
    return BOARD_PAIR_CMD_NEED_TOKEN;
  }
  if (book->ble_up) {
    if (!slot_is_ble_commander(book, slot)) {
      return BOARD_PAIR_CMD_BUSY;
    }
    if (claim) {
      book->wifi_slot = slot;
    }
    return BOARD_PAIR_CMD_ALLOW;
  }
  if (book->wifi_slot < 0) {
    if (claim) {
      book->wifi_slot = slot;
    }
    return BOARD_PAIR_CMD_ALLOW;
  }
  if (book->wifi_slot == slot) {
    return BOARD_PAIR_CMD_ALLOW;
  }
  return BOARD_PAIR_CMD_BUSY;
}

void board_pair_book_set_ble(board_pair_book_t *book, bool up,
                             uint8_t addr_type, const uint8_t addr[6]) {
  if (book == NULL) {
    return;
  }
  book->ble_up = up;
  if (!up || addr == NULL) {
    book->ble_addr_type = 0;
    memset(book->ble_addr, 0, sizeof(book->ble_addr));
    return;
  }
  book->ble_addr_type = addr_type;
  memcpy(book->ble_addr, addr, 6);
}

void board_pair_book_note_wifi_fd(board_pair_book_t *book, int fd) {
  if (book == NULL || book->wifi_slot < 0) {
    return;
  }
  book->wifi_fd = fd;
}

void board_pair_book_release_wifi_fd(board_pair_book_t *book, int fd) {
  if (book == NULL || fd < 0 || book->wifi_fd != fd) {
    return;
  }
  book->wifi_fd = -1;
  if (!book->ble_up) {
    book->wifi_slot = -1;
  }
}

void board_pair_book_release_wifi_lease(board_pair_book_t *book) {
  if (book == NULL || book->wifi_fd >= 0) {
    return;
  }
  book->wifi_slot = -1;
}

void board_pair_book_prune_wifi_fd(board_pair_book_t *book, const int *live,
                                   int live_count) {
  if (book == NULL || book->wifi_fd < 0) {
    return;
  }
  if (live != NULL) {
    for (int i = 0; i < live_count; i++) {
      if (live[i] == book->wifi_fd) {
        return;
      }
    }
  }
  board_pair_book_release_wifi_fd(book, book->wifi_fd);
}

bool board_pair_window_toggle(bool *open) {
  if (open == NULL) {
    return false;
  }
  *open = !*open;
  return *open;
}
