/**
 * @file game_led_direct.c
 * @brief Legacy LED entry points; drawing is via game events in led_task.
 */

#include "game_task.h"
#include "game_board_core.h"
#include "game_policy.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_events.h"
#include "led_mapping.h"
#include "esp_log.h"

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
    game_policy_refresh_idle_board_leds();
    return;
  }
  game_led_frame_begin(0, false);
  if (cmd->u.hint.from_sq < 64) {
    uint8_t sq = cmd->u.hint.from_sq;
    game_led_frame_sq((uint8_t)(sq / 8u), (uint8_t)(sq % 8u), 0, 255, 255);
  }
  if (cmd->u.hint.to_sq < 64) {
    uint8_t sq = cmd->u.hint.to_sq;
    game_led_frame_sq((uint8_t)(sq / 8u), (uint8_t)(sq % 8u), 255, 140, 0);
  }
  game_led_frame_end();
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
