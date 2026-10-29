/**
 * @file game_led_direct.c
 * @brief Legacy LED entry points; drawing is via game events in led_task.
 */

#include "game_task.h"
#include "game_led_direct.h"
#include "game_led_direct.h"
#include "game_board_core.h"
#include "game_policy.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_events.h"
#include "led_mapping.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "GAME_LED_DIRECT";

static bool s_frame_open;
static bool s_frame_patch;
static uint16_t s_frame_fade;
static game_evt_hint_pixel_t s_frame_px[64];
static uint8_t s_frame_n;
static game_evt_breathe_t s_frame_br[GAME_EVT_BREATHE_MAX];
static uint8_t s_frame_bn;
static game_evt_hint_pixel_t s_acc[64];
static uint8_t s_acc_n;

static bool s_coach_on;
static uint8_t s_coach_from = 255;
static uint8_t s_coach_to = 255;
static bool s_flash_on;
static uint32_t s_flash_until_ms;
static void (*s_flash_then)(void);

#define BOARD_FLASH_MS 300U

void game_board_flash(uint8_t r, uint8_t g, uint8_t b, void (*then)(void)) {
  uint8_t row;
  uint8_t col;
  s_flash_then = then;
  s_flash_until_ms = (uint32_t)(esp_timer_get_time() / 1000) + BOARD_FLASH_MS;
  s_flash_on = true;
  game_led_frame_begin(0, false);
  for (row = 0; row < 8; row++) {
    for (col = 0; col < 8; col++) {
      game_led_frame_sq(row, col, r, g, b);
    }
  }
  game_led_frame_end();
}

bool game_board_flash_active(void) { return s_flash_on; }

void game_board_flash_poll(void) {
  void (*then)(void);
  uint32_t now_ms;
  if (!s_flash_on) {
    return;
  }
  now_ms = (uint32_t)(esp_timer_get_time() / 1000);
  if ((int32_t)(now_ms - s_flash_until_ms) < 0) {
    return;
  }
  s_flash_on = false;
  then = s_flash_then;
  s_flash_then = NULL;
  if (then != NULL) {
    then();
  }
}

void game_coach_hint_clear(void) {
  s_coach_on = false;
  s_coach_from = 255;
  s_coach_to = 255;
}

bool game_coach_hint_is_set(void) { return s_coach_on; }

void game_coach_hint_paint(void) {
  uint8_t row;
  uint8_t col;
  piece_t dest;
  bool opponent;

  if (!s_coach_on || !s_frame_open) {
    return;
  }
  if (s_coach_from < 64) {
    game_led_frame_sq((uint8_t)(s_coach_from / 8u), (uint8_t)(s_coach_from % 8u),
                      255, 255, 0);
  }
  if (s_coach_to >= 64) {
    return;
  }
  row = (uint8_t)(s_coach_to / 8u);
  col = (uint8_t)(s_coach_to % 8u);
  dest = game_get_piece(row, col);
  opponent = (current_player == PLAYER_WHITE && dest >= PIECE_BLACK_PAWN &&
              dest <= PIECE_BLACK_KING) ||
             (current_player == PLAYER_BLACK && dest >= PIECE_WHITE_PAWN &&
              dest <= PIECE_WHITE_KING);
  if (opponent) {
    game_led_frame_sq(row, col, 255, 165, 0);
  } else {
    game_led_frame_sq(row, col, 0, 255, 0);
  }
}

void game_led_flush(void) {
  uint8_t n;
  if (s_frame_open || s_acc_n == 0) {
    return;
  }
  n = s_acc_n;
  s_acc_n = 0;
  game_event_post_pixels(s_acc, n, NULL, 0, 0, false, 255U);
}

void game_led_frame_begin(uint16_t fade_ms, bool patch) {
  game_led_flush();
  s_frame_open = true;
  s_frame_patch = patch;
  s_frame_fade = fade_ms;
  s_frame_n = 0;
  s_frame_bn = 0;
}

bool game_led_frame_is_open(void) { return s_frame_open; }

void game_led_frame_sq(uint8_t row, uint8_t col, uint8_t r, uint8_t g,
                       uint8_t b) {
  if (row >= 8 || col >= 8) {
    return;
  }
  if (!s_frame_open) {
    if (s_acc_n >= 64) {
      game_led_flush();
    }
    s_acc[s_acc_n].square = (uint8_t)(row * 8u + col);
    s_acc[s_acc_n].r = r;
    s_acc[s_acc_n].g = g;
    s_acc[s_acc_n].b = b;
    s_acc_n++;
    return;
  }
  if (s_frame_n >= 64) {
    return;
  }
  s_frame_px[s_frame_n].square = (uint8_t)(row * 8u + col);
  s_frame_px[s_frame_n].r = r;
  s_frame_px[s_frame_n].g = g;
  s_frame_px[s_frame_n].b = b;
  s_frame_n++;
}

void game_led_frame_breathe(uint8_t row, uint8_t col, uint8_t r, uint8_t g,
                            uint8_t b, uint8_t mode, uint8_t floor,
                            uint16_t period_ms, uint16_t phase_ms) {
  game_evt_breathe_t br;
  if (row >= 8 || col >= 8) {
    return;
  }
  br.square = (uint8_t)(row * 8u + col);
  br.r = r;
  br.g = g;
  br.b = b;
  br.mode = mode;
  br.floor = floor;
  br.period_ms = period_ms;
  br.phase_ms = phase_ms;
  if (!s_frame_open) {
    game_event_post_pixels(NULL, 0, &br, 1, 0, true, 255U);
    return;
  }
  if (s_frame_bn >= GAME_EVT_BREATHE_MAX) {
    return;
  }
  s_frame_br[s_frame_bn++] = br;
}

void game_led_frame_end(void) {
  if (!s_frame_open) {
    return;
  }
  s_frame_open = false;
  game_event_post_pixels(s_frame_px, s_frame_n, s_frame_br, s_frame_bn,
                         s_frame_fade, s_frame_patch, 255U);
}

void game_led_clear_board(void) {
  game_led_flush();
  game_event_post_hints_clear();
}

void game_led_paint_led(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t row = 0;
  uint8_t col = 0;
  if (led >= 64U) {
    return;
  }
  led_index_to_chess_pos(led, &row, &col);
  game_led_frame_sq(row, col, r, g, b);
}

void game_show_move_direct(uint8_t from_row, uint8_t from_col, uint8_t to_row,
                           uint8_t to_col) {
  uint8_t from = (uint8_t)(from_row * 8u + from_col);
  uint8_t to = (uint8_t)(to_row * 8u + to_col);
  game_event_post_move_accepted(from, to);
  ESP_LOGD(TAG, "move hint event %u->%u", (unsigned)from, (unsigned)to);
}

void game_show_piece_lift_direct(uint8_t row, uint8_t col) {
  game_event_publish_lift_hints((uint8_t)(row * 8u + col));
}

void game_show_error_direct(uint8_t row, uint8_t col) {
  game_event_post_move_rejected((uint8_t)(row * 8u + col), false);
}

void game_clear_highlights_direct(void) {
  game_event_post_hints_clear();
}

void game_show_state_direct(piece_t *board) {
  (void)board;
}

void game_show_check_direct(uint8_t king_row, uint8_t king_col) {
  game_event_post_check((uint8_t)(king_row * 8u + king_col));
}

void game_show_invalid_move_error(uint8_t from_row, uint8_t from_col,
                                  uint8_t to_row, uint8_t to_col) {
  (void)from_row;
  (void)from_col;
  game_event_post_move_rejected((uint8_t)(to_row * 8u + to_col), false);
}

void game_show_button_error(uint8_t button_id) {
  (void)button_id;
}

void game_show_castling_guidance(uint8_t king_row, uint8_t king_col,
                                 uint8_t rook_row, uint8_t rook_col,
                                 bool is_kingside) {
  (void)king_row;
  (void)king_col;
  (void)rook_row;
  (void)rook_col;
  (void)is_kingside;
}

void game_show_checkmate_direct(uint8_t king_row, uint8_t king_col) {
  game_event_post_game_end((uint8_t)(king_row * 8u + king_col), 0);
}

void game_show_promotion_direct(uint8_t row, uint8_t col) {
  (void)row;
  (void)col;
}

void game_process_web_hint(const game_cmd_t *cmd) {
  if (cmd == NULL) {
    return;
  }
  if (cmd->u.hint.clear) {
    game_coach_hint_clear();
  } else if (cmd->u.hint.from_sq >= 64 && cmd->u.hint.to_sq >= 64) {
    game_coach_hint_clear();
  } else {
    s_coach_on = true;
    s_coach_from = cmd->u.hint.from_sq;
    s_coach_to = cmd->u.hint.to_sq;
  }
  if (game_is_puzzle_active() && !game_is_puzzle_setup_active() &&
      !game_puzzle_sync_hold()) {
    game_puzzle_refresh_play_leds();
    return;
  }
  if (game_is_puzzle_setup_active()) {
    return;
  }
  game_policy_refresh_idle_board_leds();
}

void game_show_castling_direct(uint8_t king_from_row, uint8_t king_from_col,
                               uint8_t king_to_row, uint8_t king_to_col,
                               uint8_t rook_from_row, uint8_t rook_from_col,
                               uint8_t rook_to_row, uint8_t rook_to_col) {
  (void)king_from_row;
  (void)king_from_col;
  (void)king_to_row;
  (void)king_to_col;
  (void)rook_from_row;
  (void)rook_from_col;
  (void)rook_to_row;
  (void)rook_to_col;
}
