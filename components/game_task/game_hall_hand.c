/**
 * @brief Lift-order hand. The newest slot is the last one pushed.
 */

#include "game_hall_hand.h"

#include "esp_timer.h"

typedef struct {
  piece_t piece;
  uint8_t row;
  uint8_t col;
  uint32_t at_ms;
} hand_slot_t;

static hand_slot_t s_hand[GAME_HALL_HAND_MAX];
static int s_count;
static piece_t s_captured[8];
static int s_captured_n;

void game_hall_hand_reset(void) {
  s_count = 0;
  s_captured_n = 0;
}

bool game_hall_hand_push(piece_t piece, uint8_t row, uint8_t col) {
  if (piece == PIECE_EMPTY || s_count >= GAME_HALL_HAND_MAX) {
    return false;
  }
  for (int i = 0; i < s_count; i++) {
    if (s_hand[i].row == row && s_hand[i].col == col) {
      return true;
    }
  }
  s_hand[s_count].piece = piece;
  s_hand[s_count].row = row;
  s_hand[s_count].col = col;
  s_hand[s_count].at_ms = (uint32_t)(esp_timer_get_time() / 1000);
  s_count++;
  return true;
}

int game_hall_hand_count(void) { return s_count; }

bool game_hall_hand_get(int index, piece_t *piece, uint8_t *row, uint8_t *col) {
  if (index < 0 || index >= s_count || piece == NULL || row == NULL ||
      col == NULL) {
    return false;
  }
  *piece = s_hand[index].piece;
  *row = s_hand[index].row;
  *col = s_hand[index].col;
  return true;
}

void game_hall_hand_remove_at(int index) {
  if (index < 0 || index >= s_count) {
    return;
  }
  for (int i = index; i < s_count - 1; i++) {
    s_hand[i] = s_hand[i + 1];
  }
  s_count--;
}

void game_hall_hand_remove_origin(uint8_t row, uint8_t col) {
  for (int i = 0; i < s_count; i++) {
    if (s_hand[i].row == row && s_hand[i].col == col) {
      game_hall_hand_remove_at(i);
      return;
    }
  }
}

void game_hall_hand_note_captured(piece_t piece) {
  if (piece == PIECE_EMPTY || s_captured_n >= 8) {
    return;
  }
  s_captured[s_captured_n++] = piece;
}

piece_t game_hall_hand_last_captured(void) {
  if (s_captured_n <= 0) {
    return PIECE_EMPTY;
  }
  return s_captured[s_captured_n - 1];
}
