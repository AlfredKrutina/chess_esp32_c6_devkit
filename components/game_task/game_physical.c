/**
 * @file game_physical.c
 * @brief Physical pickup/drop/castle/promote command processing.
 */

#include "game_physical.h"
#include "game_physical_undo.h"
#include "chess_gameplay_policy.h"
#include "game_squares_guard.h"
#include "game_hall_match.h"
#include "game_hall_hand.h"
#include "game_hall_restore_guide.h"
#include "game_hall_position.h"
#include "game_move_exec.h"
#include "game_move_validate.h"
#include "game_snapshot.h"
#include "game_task_internal.h"

#include "game_task.h"
#include "game_led_animations.h"
#include "led_mapping.h"

#include "../led_task/include/led_task.h"
#include "../squares_task/include/squares_task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

static const char *TAG = "GAME_PHYSICAL";

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
static bool s_hall_drop_contradicts;
static bool s_hall_rook_parked;

static void hall_note_restore(const chess_move_command_t *cmd) {
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  game_hall_hand_reset();
  capture_in_progress = false;
  game_reset_guided_capture_state();
  game_reset_error_recovery_state();
  if (current_game_state == GAME_STATE_WAITING_FOR_RETURN) {
    current_game_state = GAME_STATE_ACTIVE;
  }
  game_send_response_to_uart(
      "Put the pieces back. The last legal position is unchanged.", false,
      cmd != NULL ? (QueueHandle_t)cmd->response_queue : NULL);
}
#endif

static int hand_manhattan(uint8_t r0, uint8_t c0, uint8_t r1, uint8_t c1) {
  int dr = (int)r0 - (int)r1;
  int dc = (int)c0 - (int)c1;
  if (dr < 0) {
    dr = -dr;
  }
  if (dc < 0) {
    dc = -dc;
  }
  return dr + dc;
}

static void hand_flash_pair(uint8_t r0, uint8_t c0, uint8_t r1, uint8_t c1) {
  for (int n = 0; n < 2; n++) {
    led_set_pixel_safe(chess_pos_to_led_index(r0, c0), 255, 180, 0);
    led_set_pixel_safe(chess_pos_to_led_index(r1, c1), 0, 180, 255);
    vTaskDelay(pdMS_TO_TICKS(160));
    led_set_pixel_safe(chess_pos_to_led_index(r0, c0), 0, 0, 0);
    led_set_pixel_safe(chess_pos_to_led_index(r1, c1), 0, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
  }
}

/** Which piece in the hand just landed. Blinks the two origins when both are legal. */
static int hand_choose_landing(uint8_t to_row, uint8_t to_col) {
  int best = -1;
  int best_score = -1;
  int second = -1;
  int second_score = -1;
  uint8_t sq = (uint8_t)(to_row * 8 + to_col);
  hall_piece_type_t seen = squares_get_hall_piece_type(sq);
  for (int i = 0; i < game_hall_hand_count(); i++) {
    piece_t piece;
    uint8_t row;
    uint8_t col;
    if (!game_hall_hand_get(i, &piece, &row, &col)) {
      continue;
    }
    int score = 0;
    if (row == to_row && col == to_col) {
      score = 1000;
    } else {
      chess_move_t move = {.from_row = row,
                           .from_col = col,
                           .to_row = to_row,
                           .to_col = to_col,
                           .piece = piece,
                           .captured_piece = board[to_row][to_col]};
      if (game_is_valid_move(&move) == MOVE_ERROR_NONE) {
        score = 200;
      }
      hall_piece_type_t want = HALL_PIECE_TYPE_NONE;
      hall_piece_color_t want_color = HALL_PIECE_COLOR_NONE;
      if (game_hall_engine_to_sensor(piece, &want, &want_color) &&
          want == seen && seen != HALL_PIECE_TYPE_NONE) {
        score += 40;
      }
      score += 14 - hand_manhattan(row, col, to_row, to_col);
    }
    if (score > best_score) {
      second = best;
      second_score = best_score;
      best = i;
      best_score = score;
    } else if (score > second_score) {
      second = i;
      second_score = score;
    }
  }
  if (best >= 0 && second >= 0 && best_score < 1000 &&
      (best_score - second_score) <= 10 && second_score >= 200) {
    piece_t p0;
    piece_t p1;
    uint8_t r0, c0, r1, c1;
    game_hall_hand_get(best, &p0, &r0, &c0);
    game_hall_hand_get(second, &p1, &r1, &c1);
    (void)p0;
    (void)p1;
    hand_flash_pair(r0, c0, r1, c1);
    ESP_LOGW(TAG, "two pieces fit %c%d, keeping the higher score",
             'a' + to_col, to_row + 1);
  }
  return best;
}

static void game_stamp_misplaced_piece(uint8_t from_row, uint8_t from_col,
                                       uint8_t to_row, uint8_t to_col,
                                       piece_t piece) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  (void)from_row;
  (void)from_col;
  (void)to_row;
  (void)to_col;
  (void)piece;
  return;
#else
  board[from_row][from_col] = PIECE_EMPTY;
  board[to_row][to_col] = piece;
#endif
}

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
static bool hall_piece_is_white(piece_t piece) {
  return piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING;
}

static void hall_show_hand_moves(void) {
  if (game_hall_hand_count() <= 0) {
    piece_lifted = false;
    lifted_piece = PIECE_EMPTY;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    led_clear_board_only();
    chess_policy_highlight_movable_if_enabled();
    return;
  }

  int active = game_hall_hand_count() - 1;
  for (int i = game_hall_hand_count() - 1; i >= 0; i--) {
    piece_t held;
    uint8_t hr;
    uint8_t hc;
    if (!game_hall_hand_get(i, &held, &hr, &hc)) {
      continue;
    }
    bool own = (current_player == PLAYER_WHITE && hall_piece_is_white(held)) ||
               (current_player == PLAYER_BLACK && !hall_piece_is_white(held) &&
                held != PIECE_EMPTY);
    if (own) {
      active = i;
      break;
    }
  }

  piece_t piece = PIECE_EMPTY;
  uint8_t row = 0;
  uint8_t col = 0;
  game_hall_hand_get(active, &piece, &row, &col);
  piece_lifted = true;
  lifted_piece = piece;
  lifted_piece_row = row;
  lifted_piece_col = col;

  led_clear_board_only();
  for (int i = 0; i < game_hall_hand_count(); i++) {
    piece_t held;
    uint8_t hr;
    uint8_t hc;
    if (i == active || !game_hall_hand_get(i, &held, &hr, &hc)) {
      continue;
    }
    led_set_pixel_safe(chess_pos_to_led_index(hr, hc), 255, 180, 0);
  }
  led_set_pixel_safe(chess_pos_to_led_index(row, col), 255, 255, 0);

  move_suggestion_t suggestions[64];
  uint32_t valid_moves = game_get_available_moves(row, col, suggestions, 64);
  if (valid_moves > 0 && game_led_guidance_show_destinations()) {
    for (uint32_t i = 0; i < valid_moves; i++) {
      uint8_t dest_row = suggestions[i].to_row;
      uint8_t dest_col = suggestions[i].to_col;
      uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);
      if (suggestions[i].is_castling) {
        if (chess_policy_move_hints_castling_blue()) {
          led_set_pixel_safe(led_index, 0, 0, 255);
        }
      } else {
        piece_t dest_piece = board[dest_row][dest_col];
        bool opponent = (current_player == PLAYER_WHITE &&
                         dest_piece >= PIECE_BLACK_PAWN &&
                         dest_piece <= PIECE_BLACK_KING) ||
                        (current_player == PLAYER_BLACK &&
                         dest_piece >= PIECE_WHITE_PAWN &&
                         dest_piece <= PIECE_WHITE_KING);
        if (suggestions[i].is_check) {
          led_set_pixel_safe(led_index, 255, 255, 255);
        } else if (opponent) {
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }
    }
  }
}

static bool hall_rook_park_square(piece_t piece, uint8_t to_row, uint8_t to_col) {
  if (piece != PIECE_WHITE_ROOK && piece != PIECE_BLACK_ROOK) {
    return false;
  }
  if (to_col != 3 && to_col != 5) {
    return false;
  }
  piece_t king =
      piece == PIECE_WHITE_ROOK ? PIECE_WHITE_KING : PIECE_BLACK_KING;
  for (int i = 0; i < game_hall_hand_count(); i++) {
    piece_t held;
    uint8_t hr;
    uint8_t hc;
    if (!game_hall_hand_get(i, &held, &hr, &hc)) {
      continue;
    }
    if (held == king && hr == to_row && hc == 4) {
      return true;
    }
  }
  return false;
}

static bool hall_put_back(game_hall_pick_t *pick, uint8_t to_row, uint8_t to_col,
                          const chess_move_command_t *cmd) {
  if (board[to_row][to_col] != pick->piece) {
    board[to_row][to_col] = pick->piece;
  }
  if ((pick->piece == PIECE_WHITE_ROOK || pick->piece == PIECE_BLACK_ROOK)) {
    s_hall_rook_parked = false;
  }
  game_hall_hand_remove_at(pick->index);
  if (error_recovery_state.waiting_for_move_correction &&
      to_row == error_recovery_state.original_valid_row &&
      to_col == error_recovery_state.original_valid_col) {
    game_reset_error_recovery_state();
  }
  hall_show_hand_moves();
  game_send_response_to_uart("↩️ Piece put back.", false,
                             (QueueHandle_t)cmd->response_queue);
  return true;
}

/**
 * Bind lifted_* to the piece the sensors (or the rules) say landed.
 * @return true when this drop is finished (put back, parked rook, or rejected).
 */
static bool hall_bind_drop(const chess_move_command_t *cmd, uint8_t to_row,
                           uint8_t to_col) {
  s_hall_drop_contradicts = false;
  game_hall_pick_t pick;
  if (game_hall_hand_pick_drop(to_row, to_col, &pick) != GAME_HALL_PICK_OK) {
    return false;
  }
  lifted_piece = pick.piece;
  lifted_piece_row = pick.row;
  lifted_piece_col = pick.col;
  piece_lifted = true;
  if (pick.corrected) {
    board[pick.row][pick.col] = pick.piece;
  }

  if (pick.row == to_row && pick.col == to_col) {
    piece_t occ = board[to_row][to_col];
    if (occ != PIECE_EMPTY && occ != pick.piece) {
      hall_show_hand_moves();
      game_show_invalid_move_error_with_blink(to_row, to_col);
      game_send_response_to_uart(
          "⚠️ That square holds the other piece. Lift it before putting this one back.",
          true, (QueueHandle_t)cmd->response_queue);
      return true;
    }
    return hall_put_back(&pick, to_row, to_col, cmd);
  }

  if (!pick.contradicted &&
      hall_rook_park_square(pick.piece, to_row, to_col)) {
    s_hall_rook_parked = true;
    hall_show_hand_moves();
    game_send_response_to_uart(
        "🏰 Rook is on the castle square. Place the king to castle.", false,
        (QueueHandle_t)cmd->response_queue);
    return true;
  }

  int other = -1;
  for (int i = 0; i < game_hall_hand_count(); i++) {
    piece_t held;
    uint8_t hr;
    uint8_t hc;
    if (i == pick.index || !game_hall_hand_get(i, &held, &hr, &hc)) {
      continue;
    }
    if (hr == to_row && hc == to_col) {
      other = i;
      break;
    }
  }
  bool legal = false;
  if (other >= 0) {
    chess_move_t move = {.from_row = pick.row,
                         .from_col = pick.col,
                         .to_row = to_row,
                         .to_col = to_col,
                         .piece = pick.piece,
                         .captured_piece = board[to_row][to_col]};
    legal = game_is_valid_move(&move) == MOVE_ERROR_NONE;
  }
  if (other >= 0 && !legal && !pick.contradicted) {
    ESP_LOGW(TAG, "pieces crossed at %c%d — last legal position kept",
             'a' + to_col, to_row + 1);
    hall_note_restore(cmd);
    return true;
  }

  if (pick.contradicted) {
    s_hall_drop_contradicts = true;
  }
  return false;
}
#endif

void game_reset_guided_capture_state(void) {
  guided_capture_state.active = false;
  guided_capture_state.target_row = 0;
  guided_capture_state.target_col = 0;
  guided_capture_state.target_piece = PIECE_EMPTY;
  guided_capture_state.attacker_count = 0;
  memset(guided_capture_state.attacker_rows, 0,
         sizeof(guided_capture_state.attacker_rows));
  memset(guided_capture_state.attacker_cols, 0,
         sizeof(guided_capture_state.attacker_cols));
  memset(guided_capture_state.attacker_pieces, 0,
         sizeof(guided_capture_state.attacker_pieces));
}

static bool game_guided_capture_has_mode_conflict(void) {
  if (promotion_state.pending || castling_state.in_progress ||
      castle_animation_active || resignation_state.active ||
      error_recovery_state.waiting_for_move_correction ||
      current_game_state == GAME_STATE_WAITING_FOR_RETURN ||
      current_game_state == GAME_STATE_FINISHED) {
    return true;
  }
  return false;
}

/**
 * Searches for custom pieces that can *pick up on the field* (target_row,target_col).
 * En passant ends on an empty field behind the victim — so the move typically doesn't go through here
 * via game_is_valid_move(..., to=victim square); guided capture in the e.p.
 * does not activate the situation and the player uses a normal move / 3-step parry.
 */
static bool game_find_legal_attackers_to_square(uint8_t target_row,
                                                uint8_t target_col,
                                                piece_t target_piece) {
  guided_capture_state.attacker_count = 0;

  piece_t current_player_king =
      (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;

  for (uint8_t row = 0; row < 8; row++) {
    for (uint8_t col = 0; col < 8; col++) {
      piece_t candidate_piece = board[row][col];
      if (candidate_piece == PIECE_EMPTY ||
          !game_is_same_color(candidate_piece, current_player_king)) {
        continue;
      }

      chess_move_t capture_attempt = {.from_row = row,
                                      .from_col = col,
                                      .to_row = target_row,
                                      .to_col = target_col,
                                      .piece = candidate_piece,
                                      .captured_piece = target_piece,
                                      .timestamp = esp_timer_get_time() / 1000};

      if (game_is_valid_move(&capture_attempt) == MOVE_ERROR_NONE &&
          guided_capture_state.attacker_count < GUIDED_CAPTURE_MAX_ATTACKERS) {
        uint8_t idx = guided_capture_state.attacker_count++;
        guided_capture_state.attacker_rows[idx] = row;
        guided_capture_state.attacker_cols[idx] = col;
        guided_capture_state.attacker_pieces[idx] = candidate_piece;
      }
    }
  }

  return guided_capture_state.attacker_count > 0;
}

static bool game_refresh_guided_attackers(void) {
  if (!guided_capture_state.active) {
    return false;
  }
  return game_find_legal_attackers_to_square(guided_capture_state.target_row,
                                             guided_capture_state.target_col,
                                             guided_capture_state.target_piece);
}

/** True if the currently lifted piece is one of the legal guided attackers. */
static bool game_guided_capture_is_selected_attacker_lifted(void) {
  if (!guided_capture_state.active || !piece_lifted ||
      lifted_piece == PIECE_EMPTY) {
    return false;
  }
  for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
    if (guided_capture_state.attacker_rows[i] == lifted_piece_row &&
        guided_capture_state.attacker_cols[i] == lifted_piece_col &&
        guided_capture_state.attacker_pieces[i] == lifted_piece) {
      return true;
    }
  }
  return false;
}

/**
 * Refresh guided-capture LEDs:
 * - Attacker raised: only purple victim field (where to lay).
 * - Attacker selection: always purple victim; yellow only legal attackers if
 * game_get_guided_capture_hints_enabled() OR led_guidance_level ≥ 3 (yellow pieces).
 * Otherwise only purple (levels 1-2).
 */
void game_show_guided_capture_leds(void) {
  if (!guided_capture_state.active) {
    return;
  }

  if (game_guided_capture_is_selected_attacker_lifted()) {
    led_clear_board_only();
    led_set_pixel_safe(
        chess_pos_to_led_index(guided_capture_state.target_row,
                               guided_capture_state.target_col),
        128, 0, 128); // Purple = drop here to complete capture
    return;
  }

  led_clear_board_only();
  led_set_pixel_safe(
      chess_pos_to_led_index(guided_capture_state.target_row,
                             guided_capture_state.target_col),
      128, 0, 128); // Purple = opponent square / capture target

  bool show_attacker_squares = game_get_guided_capture_hints_enabled() ||
                               game_led_guidance_show_movable_yellow();
  if (show_attacker_squares) {
    for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
      led_set_pixel_safe(
          chess_pos_to_led_index(guided_capture_state.attacker_rows[i],
                                 guided_capture_state.attacker_cols[i]),
          255, 255, 0); // Yellow = pieces that can capture here
    }
  }
}

static void game_exit_guided_capture(bool restore_highlights) {
  game_reset_guided_capture_state();
  game_hall_hand_reset();
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;

  if (restore_highlights) {
    chess_policy_refresh_idle_board_leds();
  }
}

static void game_enter_guided_capture(uint8_t target_row, uint8_t target_col,
                                      piece_t target_piece) {
  guided_capture_state.active = true;
  guided_capture_state.target_row = target_row;
  guided_capture_state.target_col = target_col;
  guided_capture_state.target_piece = target_piece;
  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;
}

static uint32_t s_pickup_handle_ms;

static void game_log_pickup_latency(const chess_move_command_t *cmd) {
  if (cmd == NULL || cmd->event_ms == 0 || s_pickup_handle_ms == 0) {
    return;
  }
  uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
  static char s_last[8];
  static uint32_t s_last_ms;
  if (s_last[0] != '\0' &&
      strncmp(s_last, cmd->from_notation, sizeof(s_last)) == 0 &&
      (uint32_t)(now - s_last_ms) < 1000U) {
    return;
  }
  strncpy(s_last, cmd->from_notation, sizeof(s_last) - 1);
  s_last[sizeof(s_last) - 1] = '\0';
  s_last_ms = now;
  uint32_t to_handle = (s_pickup_handle_ms >= cmd->event_ms)
                           ? (s_pickup_handle_ms - cmd->event_ms)
                           : 0;
  uint32_t to_leds = (now >= s_pickup_handle_ms) ? (now - s_pickup_handle_ms) : 0;
  ESP_LOGW(TAG,
           "pickup %s: sensor->handle %u ms, handle->leds %u ms, total %u ms",
           cmd->from_notation, (unsigned)to_handle, (unsigned)to_leds,
           (unsigned)(to_handle + to_leds));
}

/**
 * @brief Process pickup command (UP)
 */
void game_process_pickup_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;
  s_pickup_handle_ms = (uint32_t)(esp_timer_get_time() / 1000);
  game_squares_guard_release_hall_hold();

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  /* Matrix events wake the snapshot diff. They are not a move. */
  if (game_cmd_is_squares_origin(cmd) && cmd->event_ms != 0) {
    game_hall_position_on_matrix_event(cmd);
    game_log_pickup_latency(cmd);
    return;
  }
#endif

  // BUG FIX 1: Block moves while waiting for deployment
  if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    game_send_response_to_uart(
        "⏳ The game is waiting for the physical arrangement of the pieces. "
        "Place all pieces in their starting positions.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_squares_guard_active()) {
    if (game_cmd_is_squares_origin(cmd)) {
      game_squares_guard_try_clear_from_matrix();
      if (!game_is_squares_guard_active()) {
        return;
      }
      game_squares_guard_render_leds();
      return;
    }
    game_send_response_to_uart(
        "⏸️ Pending comparison of physical board (matrix guard). "
        "PICKUP from UART/Web is blocked.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_puzzle_setup_active() || game_puzzle_sync_hold()) {
    game_puzzle_service_leds();
    game_send_response_to_uart(
        "⏸️ Puzzle position is not ready. Fix the lit squares first.", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }
  if (game_puzzle_is_solved()) {
    game_send_response_to_uart("✅ Puzzle already solved.", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // PROMOTION (swap_then_choose, swap optional):
  // - Allow physical UP/DN only on graduation field (no normal pickup flow).
  // - Ignore outside the graduation field (the game is logically suspended) and send
  // a short message.
  if (promotion_state.pending) {
    // Notation string security check
    if (strlen(cmd->from_notation) == 0 || strlen(cmd->from_notation) > 7) {
      ESP_LOGE(TAG,
               "❌ Invalid or corrupted notation string (promotion pending)");
      game_send_response_to_uart("❌ Invalid notation format", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    // Parse square even during promotion pending
    uint8_t p_row, p_col;
    if (!convert_notation_to_coords(cmd->from_notation, &p_row, &p_col)) {
      ESP_LOGE(TAG, "❌ Invalid notation during promotion: %s",
               cmd->from_notation);
      game_send_response_to_uart("❌ Invalid square notation", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    bool is_promo_square = (p_row == promotion_state.square_row &&
                            p_col == promotion_state.square_col);

    if (is_promo_square) {
      piece_t promo_piece = board[p_row][p_col];
      if (game_physical_undo_handle_pickup(cmd, p_row, p_col, promo_piece)) {
        game_send_response_to_uart(
            "↩️ Takeback — return pawn to blue square (promotion cancelled).",
            false, (QueueHandle_t)cmd->response_queue);
        return;
      }
      ESP_LOGI(TAG,
               "👑 Promotion pending: PICKUP on promo square %c%d allowed "
               "(swap_then_choose)",
               'a' + p_col, p_row + 1);

      char info_msg[256];
      snprintf(info_msg, sizeof(info_msg),
               "👑 Promotion pending at %c%d\n"
               "🔁 You can swap the pawn here now (optional)\n"
               "➡️ Put a queen, rook, bishop, or knight of your color on this square",
               'a' + promotion_state.square_col,
               promotion_state.square_row + 1);
      game_send_response_to_uart(info_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      game_promotion_sync_hall_if_pending();
#endif
      return; // Don't do standard pickup flow
    }

    ESP_LOGI(TAG,
             "⏸️  Promotion pending: PICKUP blocked outside promo square (%s)",
             cmd->from_notation);

    char info_msg[256];
    snprintf(info_msg, sizeof(info_msg),
             "⏸️ Promotion pending at %c%d\n"
             "🔁 Swap allowed only on %c%d (optional)\n"
             "➡️ Put a queen, rook, bishop, or knight of your color on that square",
             'a' + promotion_state.square_col, promotion_state.square_row + 1,
             'a' + promotion_state.square_col, promotion_state.square_row + 1);
    game_send_response_to_uart(info_msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // CANCEL FLASHING when figure is picked up
  game_stop_error_blink();

  // String notation security check (array is always != NULL)
  if (strlen(cmd->from_notation) == 0 || strlen(cmd->from_notation) > 7) {
    ESP_LOGE(TAG, "❌ Invalid or corrupted notation string");
    game_send_response_to_uart("❌ Invalid notation format", true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  ESP_LOGI(TAG, "🎯 Processing PICKUP command: %s", cmd->from_notation);

  // Convert notation to coordinates
  uint8_t from_row, from_col;
  if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col)) {
    ESP_LOGE(TAG, "❌ Invalid notation: %s", cmd->from_notation);
    game_send_response_to_uart("❌ Invalid square notation", true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_physical_undo_intercept_pickup(from_row, from_col)) {
    game_send_response_to_uart(
        "↩️ Physical takeback — follow blue return squares.", false,
        (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_ep_capture_waiting()) {
    if (!game_ep_capture_is_victim(from_row, from_col)) {
      game_ep_capture_service();
      game_send_response_to_uart(
          "⚔️ En passant: lift the pawn on the orange square.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }
    piece_t victim = (current_player == PLAYER_WHITE) ? PIECE_WHITE_PAWN
                                                       : PIECE_BLACK_PAWN;
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = victim;
    game_ep_capture_service();
    return;
  }

  // ERROR RECOVERY STATUS CHECK
  if (error_recovery_state.waiting_for_move_correction) {
    // CHECK: Is this a valid invalid position?
    if (from_row == error_recovery_state.invalid_row &&
        from_col == error_recovery_state.invalid_col) {

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (!cmd->is_demo_mode &&
          !game_hall_validate_pickup_square(
              from_row, from_col, error_recovery_state.piece_type)) {
        game_show_invalid_move_error_with_blink(from_row, from_col);
        game_send_response_to_uart(
            "❌ Hall sensor disagrees with the piece on this square", true,
            (QueueHandle_t)cmd->response_queue);
        return;
      }
#endif

      ESP_LOGI(TAG, "🔄 Lifting piece from invalid position %s (error #%d)",
               cmd->from_notation, error_recovery_state.error_count);
      ESP_LOGI(TAG, "💡 Showing valid moves from ORIGINAL position %c%d",
               'a' + error_recovery_state.original_valid_col,
               error_recovery_state.original_valid_row + 1);

      // Aktualizovat lifted piece state
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = error_recovery_state.piece_type;

      // KEY: Show valid moves from the ORIGINAL position, not the last one
      // pokusu
      move_suggestion_t suggestions[64];
      uint32_t valid_moves = game_get_available_moves(
          error_recovery_state.original_valid_row,
          error_recovery_state.original_valid_col, suggestions, 64);

      ESP_LOGI(TAG, "💡 Found %lu valid moves from original position",
               valid_moves);

      // CLEAN AND SET LED
      led_clear_board_only();

      // Yellow at current position (where the figure is now)
      led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255,
                         0);

      // BLUE to original valid position
      if (chess_policy_error_recovery_led_valid_blue()) {
        led_set_pixel_safe(
            chess_pos_to_led_index(error_recovery_state.original_valid_row,
                                   error_recovery_state.original_valid_col),
            0, 0, 255);
      }

      // GREEN LED for valid target fields (from original position)
      if (game_led_guidance_show_destinations()) {
        for (uint32_t i = 0; i < valid_moves; i++) {
          uint8_t dest_led = chess_pos_to_led_index(suggestions[i].to_row,
                                                    suggestions[i].to_col);

          // Do not overwrite yellow (current position) and blue (original position)
          if (dest_led != chess_pos_to_led_index(from_row, from_col) &&
              dest_led != chess_pos_to_led_index(
                              error_recovery_state.original_valid_row,
                              error_recovery_state.original_valid_col)) {
            if (suggestions[i].is_capture) {
              led_set_pixel_safe(dest_led, 255, 165, 0); // Orange for capture
            } else {
              led_set_pixel_safe(dest_led, 0, 255, 0); // Green for normal stroke
            }
          }
        }
      }

      char success_msg[256];
      snprintf(success_msg, sizeof(success_msg),
               "🔄 Lifted from invalid position - %u valid moves from ORIGINAL "
               "%c%d (blue)",
               (unsigned int)valid_moves,
               'a' + error_recovery_state.original_valid_col,
               error_recovery_state.original_valid_row + 1);
      game_send_response_to_uart(success_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    } else {
      // UP from another position during error recovery - ignore or reset?
      ESP_LOGW(TAG,
               "⚠️ UP from different position during error recovery: %s "
               "(expected %c%d)",
               cmd->from_notation, 'a' + error_recovery_state.invalid_col,
               error_recovery_state.invalid_row + 1);

      char warning_msg[128];
      snprintf(warning_msg, sizeof(warning_msg),
               "⚠️ Error recovery active - lift piece from %c%d first",
               'a' + error_recovery_state.invalid_col,
               error_recovery_state.invalid_row + 1);
      game_send_response_to_uart(warning_msg, true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }
  }

  // Check if there's a piece at the square
  piece_t piece = board[from_row][from_col];
  if (piece == PIECE_EMPTY) {
    char error_msg[128];
    // Safe snprintf with return value checking
    int written = snprintf(error_msg, sizeof(error_msg), "❌ No piece at %s",
                           cmd->from_notation);
    if (written >= sizeof(error_msg)) {
      strcpy(error_msg, "❌ No piece at square");
    }
    ESP_LOGE(TAG, "❌ No piece at %s", cmd->from_notation);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (!cmd->is_demo_mode &&
      !game_hall_validate_pickup_square(from_row, from_col, piece)) {
    game_show_invalid_move_error_with_blink(from_row, from_col);
    game_send_response_to_uart(
        "❌ Hall sensor disagrees with the piece on this square", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }
#endif

  if (game_physical_undo_handle_pickup(cmd, from_row, from_col, piece)) {
    game_send_response_to_uart(
        game_physical_undo_is_active()
            ? "↩️ Physical takeback — follow blue return squares."
            : "↩️ Takeback hint active.",
        false, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (current_game_state == GAME_STATE_FINISHED) {
    game_send_response_to_uart(
        "🏁 Game over — no new moves. Take back the last move only if takeback "
        "is enabled.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (guided_capture_state.active) {
    if (game_guided_capture_has_mode_conflict()) {
      STAGING_LOGI(TAG, "Guided capture cancelled due to mode conflict (pickup)");
      game_exit_guided_capture(true);
      game_send_response_to_uart(
          "⚠️ Guided capture cancelled due to higher-priority game state.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }
    if (!game_refresh_guided_attackers()) {
      STAGING_LOGI(TAG, "Guided capture cancelled (no legal attackers after refresh)");
      game_exit_guided_capture(true);
      game_send_response_to_uart(
          "⚠️ Guided capture cancelled - no legal capture available now.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    bool is_target_square = (from_row == guided_capture_state.target_row &&
                             from_col == guided_capture_state.target_col);
    bool is_allowed_attacker = false;

    for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
      if (guided_capture_state.attacker_rows[i] == from_row &&
          guided_capture_state.attacker_cols[i] == from_col &&
          guided_capture_state.attacker_pieces[i] == piece) {
        is_allowed_attacker = true;
        break;
      }
    }

    if (is_target_square) {
      // Cancel/escape path: user picked up target opponent piece again.
      game_exit_guided_capture(true);
      game_send_response_to_uart(
          "↩️ Guided capture cancelled. Opponent piece returned to normal flow.",
          false,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    if (!is_allowed_attacker) {
      game_show_guided_capture_leds();
      game_send_response_to_uart(
          game_get_guided_capture_hints_enabled()
              ? "⚠️ Guided capture active: lift highlighted YELLOW piece only."
              : "⚠️ Guided capture active: lift your piece, then drop on PURPLE.",
          true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;
    game_show_guided_capture_leds();
    game_send_response_to_uart(
        "✅ Attacker lifted. Drop on PURPLE target square to capture.", false,
        (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Opening trainer: opponent physically moves their pieces (bot-style).
  if (game_is_opening_trainer_active() && game_opening_awaiting_opponent_physical()) {
    if (game_opening_validate_opponent_pickup(from_row, from_col)) {
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = piece;
      game_opening_on_opponent_piece_lifted();
      game_send_response_to_uart(
          "✅ Lift the opponent piece — place it on the highlighted square.", false,
          (QueueHandle_t)cmd->response_queue);
      return;
    }
    game_show_invalid_move_error_with_blink(from_row, from_col);
    game_send_response_to_uart(
        "⚠️ Opponent move: lift the piece from the correct square (LED hint).", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_puzzle_awaiting_opponent()) {
    if (from_row == puzzle_solution_from_row &&
        from_col == puzzle_solution_from_col) {
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = piece;
      game_send_response_to_uart(
          "✅ Opponent piece lifted — place it on the blue square.", false,
          (QueueHandle_t)cmd->response_queue);
      return;
    }
    game_show_invalid_move_error_with_blink(from_row, from_col);
    game_send_response_to_uart(
        "⚠️ Puzzle: lift the yellow opponent piece.", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Check if piece belongs to current player
  bool is_white_piece =
      (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);

  if ((current_player == PLAYER_WHITE && !is_white_piece) ||
      (current_player == PLAYER_BLACK && !is_black_piece)) {

    // 3-STEP CAPTURE FLOW: UP own → UP opponent (remove) → DN own (execute)
    // Distinguish capture vs error

    // ISSUE 1: Capture must NOT be enabled during error recovery!
    if (piece_lifted && lifted_piece != PIECE_EMPTY &&
        !error_recovery_state.waiting_for_move_correction &&
        ((current_player == PLAYER_WHITE && lifted_piece >= PIECE_WHITE_PAWN &&
          lifted_piece <= PIECE_WHITE_KING) ||
         (current_player == PLAYER_BLACK && lifted_piece >= PIECE_BLACK_PAWN &&
          lifted_piece <= PIECE_BLACK_KING))) {

      // STEP 2 of CAPTURE: Check if the lifted piece CAN be picked up
      // opponent piece
      chess_move_t capture_attempt = {.from_row = lifted_piece_row,
                                      .from_col = lifted_piece_col,
                                      .to_row = from_row,
                                      .to_col = from_col,
                                      .piece = lifted_piece,
                                      .captured_piece = piece,
                                      .timestamp = esp_timer_get_time() / 1000};

      move_error_t capture_valid = game_is_valid_move(&capture_attempt);

      if (capture_valid == MOVE_ERROR_NONE) {
        // VALID CAPTURE: Remove opponent piece, wait for DN
        ESP_LOGI(TAG, "♟️  Step 2/3 Capture: Removing opponent piece at %s",
                 cmd->from_notation);

        // Victim stays on board[] until game_execute_move(). Hall already
        // sees the square empty; removing it here made a cancelled capture lie.
#if !CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        board[from_row][from_col] = PIECE_EMPTY;
        game_hall_hand_note_captured(piece);
#endif

        // Nastavit capture state
        capture_in_progress = true;
        capture_target_row = from_row;
        capture_target_col = from_col;
        capture_removed_piece = piece;

        // Discreet LED feedback: Yellow on lifted piece, PURPLE on target (where
        // has to put it)
        led_clear_board_only();
        led_set_pixel_safe(
            chess_pos_to_led_index(lifted_piece_row, lifted_piece_col), 255,
            255, 0); // Yellow
        led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 128, 0,
                           128); // Purple = target

        // Poslat info message
        char info_msg[128];
        snprintf(info_msg, sizeof(info_msg),
                 "♟️  Step 2/3: Opponent piece removed. Place %s on PURPLE "
                 "square (%s) to capture",
                 game_get_piece_name(lifted_piece), cmd->from_notation);
        game_send_response_to_uart(info_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      } else {
        // INVALID CAPTURE: Proceed to error flow
        ESP_LOGW(TAG,
                 "⚠️ Invalid capture attempt: %s from %c%d CANNOT capture at %s",
                 game_get_piece_name(lifted_piece), 'a' + lifted_piece_col,
                 lifted_piece_row + 1, cmd->from_notation);
      }
    }

    const char *safe_notation =
        (strlen(cmd->from_notation) > 0) ? cmd->from_notation : "??";

    if (!error_recovery_state.waiting_for_move_correction && !piece_lifted &&
        !game_guided_capture_has_mode_conflict() &&
        game_find_legal_attackers_to_square(from_row, from_col, piece)) {
      /* Opponent piece lifted first: purple origin + yellow capturers. */
      game_enter_guided_capture(from_row, from_col, piece);

      STAGING_LOGI(TAG,
                   "Guided capture armed for %s, attackers=%u, player=%s",
                   safe_notation, guided_capture_state.attacker_count,
                   current_player == PLAYER_WHITE ? "White" : "Black");
      game_show_guided_capture_leds();

      char guided_msg[256];
      snprintf(guided_msg, sizeof(guided_msg),
               "♟️ Capture is legal on %s\n",
               safe_notation);
      if (game_get_guided_capture_hints_enabled()) {
        strncat(guided_msg,
                "🟡 Lift any YELLOW attacker and place it on PURPLE target",
                sizeof(guided_msg) - strlen(guided_msg) - 1);
      } else {
        strncat(guided_msg,
                "ℹ️ Guided capture hints are OFF. Lift your piece and drop on target square.",
                sizeof(guided_msg) - strlen(guided_msg) - 1);
      }
      game_send_response_to_uart(guided_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    STAGING_LOGI(TAG, "No legal attacker for %s - fallback to red blink",
                 safe_notation);

    // ERROR FLOW: Wrong color piece lifted inappropriately
    game_reset_guided_capture_state();

    if (error_recovery_state.waiting_for_move_correction) {
      ESP_LOGW(TAG,
               "⚠️ Cannot capture during error recovery - opponent piece at %s "
               "ignored",
               safe_notation);
    } else if (piece_lifted) {
      ESP_LOGW(TAG,
               "⚠️ Cannot capture opponent piece at %s - invalid move for %s",
               safe_notation, game_get_piece_name(lifted_piece));
    } else {
      ESP_LOGW(TAG,
               "⚠️ Opponent's piece lifted WITHOUT own piece lifted at %c%d - ERROR",
               'a' + from_col, from_row + 1);
    }

    if (!piece_lifted && game_hall_hand_count() == 0) {
      game_hall_hand_push(piece, from_row, from_col);
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = piece;
      led_clear_board_only();
      led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 128, 0, 128);
      game_send_response_to_uart(
          "♟️ Opponent piece held. Lift your attacker, or put this piece back.",
          false, (QueueHandle_t)cmd->response_queue);
      return;
    }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    game_send_response_to_uart(
        "Put that piece back. The last legal position is unchanged.", false,
        (QueueHandle_t)cmd->response_queue);
    return;
#else
    // CRITICAL: Set error_recovery_state for proper recovery flow
    error_recovery_state.has_invalid_piece = true;
    chess_policy_error_recovery_enter_lock();
    error_recovery_state.piece_type = piece;
    error_recovery_state.invalid_row = from_row;
    error_recovery_state.invalid_col = from_col;
    error_recovery_state.original_valid_row =
        from_row; // Original position = where to return
    error_recovery_state.original_valid_col = from_col;
    error_recovery_state.error_count++;

    // Save opponent piece information (backward compatibility)
    opponent_piece_type = piece;
    opponent_original_row = from_row;
    opponent_original_col = from_col;
    opponent_current_row = from_row;
    opponent_current_col = from_col;
    opponent_piece_moved = true;

    // Remove a piece from the board (it was picked up)
    if (chess_policy_error_recovery_should_mutate_board()) {
      board[from_row][from_col] = PIECE_EMPTY;
    }

    // Nastavit recovery stav
    current_game_state = GAME_STATE_WAITING_FOR_RETURN;

    // LED: Start red flashing to original position
    led_clear_board_only();
    game_show_invalid_move_error_with_blink(from_row, from_col);

    // Set lifted piece state for possible drop
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;

    char error_msg[256];
    int written =
        snprintf(error_msg, sizeof(error_msg),
                 "❌ Wrong color piece! %s belongs to opponent\n"
                 "🔴 Return piece to BLINKING RED square (%s) to continue\n"
                 "💡 To capture: lift YOUR piece first, then opponent's piece",
                 safe_notation, safe_notation);
    if (written >= sizeof(error_msg)) {
      strcpy(error_msg, "❌ Return opponent's piece to RED square");
    }

    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
#endif
  }

  // CASTLING IN PROGRESS: User lifted the king (already moved). Do NOT start
  // resignation – we are waiting for the rook move. Show guidance instead.
  if (castling_state.in_progress &&
      (piece == PIECE_WHITE_KING || piece == PIECE_BLACK_KING)) {
    led_clear_board_only();
    uint8_t from_led = chess_pos_to_led_index(castling_state.rook_from_row,
                                              castling_state.rook_from_col);
    uint8_t to_led = chess_pos_to_led_index(castling_state.rook_to_row,
                                             castling_state.rook_to_col);
    led_set_pixel_safe(from_led, 255, 255, 0);   // Yellow – rook source
    led_set_pixel_safe(to_led, 0, 255, 0);      // Green – rook destination
    char msg[128];
    int n = snprintf(msg, sizeof(msg),
                    "🏰 Complete castling: move the rook from %c%d to %c%d",
                    (char)('a' + castling_state.rook_from_col),
                    castling_state.rook_from_row + 1,
                    (char)('a' + castling_state.rook_to_col),
                    castling_state.rook_to_row + 1);
    if (n >= (int)sizeof(msg)) {
      strcpy(msg, "🏰 Complete castling: move the rook to the green square");
    }
    game_send_response_to_uart(msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return; // Do not set piece_lifted – king stays on board
  }

  // KING RESIGNATION: Detection of raising own king (skip in demo mode)
  if (!cmd->is_demo_mode &&
      ((piece == PIECE_WHITE_KING && current_player == PLAYER_WHITE) ||
       (piece == PIECE_BLACK_KING && current_player == PLAYER_BLACK))) {
    // First start resignation timer (sets orange-red to source)
    // Then show valid moves (including castling moves as blue)
    // Clear previous highlights
    led_clear_board_only();

    // Start resignation timer BEFORE displaying valid moves
    // resignation_start() sets the orange-red mix to the source square
    resignation_start(current_player, from_row, from_col);

    // Get valid moves for king (AFTER removal from board in resignation_start())
    // Note: game_get_available_moves() must work even if there is a king
    // removed from board (uses resignation_state.king_row/col if there is
    // resignation active)
    move_suggestion_t suggestions[64];
    uint32_t valid_moves =
        game_get_available_moves(from_row, from_col, suggestions, 64);

    ESP_LOGI(TAG, "🔄 Showing possible moves for king from %s (%lu moves)",
             cmd->from_notation, valid_moves);

    // Show valid moves (green/orange/blue for castling)
    if (valid_moves > 0 && game_led_guidance_show_destinations()) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if this is a castling move - show as blue
        if (suggestions[i].is_castling) {
          if (chess_policy_move_hints_castling_blue()) {
            led_set_pixel_safe(led_index, 0, 0, 255);
            ESP_LOGI(TAG,
                     "🏰 Castling move highlighted at %c%d (blue) during "
                     "resignation pickup",
                     'a' + dest_col, dest_row + 1);
          }
        } else {
          // Normal move handling
          piece_t dest_piece = board[dest_row][dest_col];
          bool is_opponent_piece = (current_player == PLAYER_WHITE &&
                                    dest_piece >= PIECE_BLACK_PAWN &&
                                    dest_piece <= PIECE_BLACK_KING) ||
                                   (current_player == PLAYER_BLACK &&
                                    dest_piece >= PIECE_WHITE_PAWN &&
                                    dest_piece <= PIECE_WHITE_KING);

          if (is_opponent_piece) {
            // Orange for opponent's pieces (capture)
            led_set_pixel_safe(led_index, 255, 165, 0);
          } else {
            // Green for empty squares
            led_set_pixel_safe(led_index, 0, 255, 0);
          }
        }
      }
    }

    game_log_pickup_latency(cmd);

    // Nastavit lifted piece state
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;

    // Note: resignation_start() already sets the orange-red mix to source
    // square Valid moves (green/orange) will remain displayed because
    // resignation_start() does not call led_clear_board_only()

    char msg[128];
    snprintf(msg, sizeof(msg),
             "👑 King lifted - resignation timer started (10s) - %" PRIu32
             " valid moves",
             valid_moves);
    game_send_response_to_uart(msg, false, (QueueHandle_t)cmd->response_queue);
    return; // Do not continue with normal pickup flow
  }

  if (castling_state.in_progress) {
    bool rook_src = (from_row == castling_state.rook_from_row &&
                     from_col == castling_state.rook_from_col);
    game_show_castling_rook_guidance();
    if (!rook_src) {
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = piece;
      led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 0, 0);
      game_send_response_to_uart(
          "🏰 Move the rook from yellow to green.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;
    return;
  }

  // CASTLING INTERCEPTION: Re-lift of rook during active castling
  // If we are in castling mode (standard timer based), suppress standard move
  // hints and just reinforce guidance
  if (game_is_castle_animation_active()) {
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;

    // Force refresh of castling guidance immediately
    uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
    uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
    led_clear_board_only();
    led_set_pixel_safe(from_led, 255, 255, 0); // Yellow
    led_set_pixel_safe(to_led, 0, 255, 0);     // Green

    char msg[128];
    snprintf(msg, sizeof(msg),
             "🏰 Continue castling: Move to flashing GREEN square");
    game_send_response_to_uart(msg, false, (QueueHandle_t)cmd->response_queue);

    return; // SKIP standard move calculation
  }

  // ED: No animation when lifting piece, just highlight possible moves
  if (game_hall_hand_count() > 0 &&
      piece != PIECE_WHITE_KING && piece != PIECE_BLACK_KING) {
    bool own = (current_player == PLAYER_WHITE &&
                piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) ||
               (current_player == PLAYER_BLACK &&
                piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);
    if (own) {
      for (int i = 0; i < game_hall_hand_count(); i++) {
        piece_t held;
        uint8_t hr;
        uint8_t hc;
        if (!game_hall_hand_get(i, &held, &hr, &hc)) {
          continue;
        }
        bool held_own =
            (current_player == PLAYER_WHITE && held >= PIECE_WHITE_PAWN &&
             held <= PIECE_WHITE_KING) ||
            (current_player == PLAYER_BLACK && held >= PIECE_BLACK_PAWN &&
             held <= PIECE_BLACK_KING);
        if (held_own) {
          continue;
        }
        chess_move_t capture_attempt = {.from_row = from_row,
                                        .from_col = from_col,
                                        .to_row = hr,
                                        .to_col = hc,
                                        .piece = piece,
                                        .captured_piece = held};
        if (game_is_valid_move(&capture_attempt) != MOVE_ERROR_NONE) {
          continue;
        }
#if !CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        board[hr][hc] = PIECE_EMPTY;
        game_hall_hand_note_captured(held);
#endif
        game_hall_hand_remove_at(i);
        capture_in_progress = true;
        capture_target_row = hr;
        capture_target_col = hc;
        capture_removed_piece = held;
        piece_lifted = true;
        lifted_piece_row = from_row;
        lifted_piece_col = from_col;
        lifted_piece = piece;
        game_hall_hand_push(piece, from_row, from_col);
        led_clear_board_only();
        led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255,
                           0);
        led_set_pixel_safe(chess_pos_to_led_index(hr, hc), 128, 0, 128);
        game_send_response_to_uart(
            "♟️ Attacker lifted after the victim. Place it on the purple square.",
            false, (QueueHandle_t)cmd->response_queue);
        return;
      }
      game_hall_hand_push(piece, from_row, from_col);
      piece_lifted = true;
      lifted_piece_row = from_row;
      lifted_piece_col = from_col;
      lifted_piece = piece;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      hall_show_hand_moves();
      game_send_response_to_uart(
          "✋ Two pieces in hand. Put one down; the sensor names which one.",
          false, (QueueHandle_t)cmd->response_queue);
#else
      led_clear_board_only();
      for (int i = 0; i < game_hall_hand_count(); i++) {
        piece_t held;
        uint8_t hr;
        uint8_t hc;
        if (game_hall_hand_get(i, &held, &hr, &hc)) {
          led_set_pixel_safe(chess_pos_to_led_index(hr, hc), 255, 180, 0);
        }
      }
      game_send_response_to_uart(
          "✋ Two pieces in hand. Put one down; the board picks by the rules.",
          false, (QueueHandle_t)cmd->response_queue);
#endif
      return;
    }
  }

  ESP_LOGI(TAG, "🔄 Piece lifted from %s - showing possible moves",
           cmd->from_notation);

  // Clear previous highlights before showing new ones
  led_clear_board_only();

  // Send LED command to highlight source square using new game state logic
  led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255,
                     0); // Yellow for source

  // Track the lifted piece
  piece_lifted = true;
  lifted_piece_row = from_row;
  lifted_piece_col = from_col;
  lifted_piece = piece;
  game_hall_hand_push(piece, from_row, from_col);
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  game_hall_restore_guide_end_for_play();
#endif

  // Show possible moves
  ESP_LOGI(TAG, "🔄 Showing possible moves from %s", cmd->from_notation);

  // Get valid moves for this piece
  move_suggestion_t suggestions[64];
  uint32_t valid_moves =
      game_get_available_moves(from_row, from_col, suggestions, 64);

  ESP_LOGI(TAG, "Found %lu valid moves for piece at %s", valid_moves,
           cmd->from_notation);

  // Send LED commands to highlight possible destinations
  if (valid_moves > 0 && game_led_guidance_show_destinations()) {
    for (uint32_t i = 0; i < valid_moves; i++) {
      uint8_t dest_row = suggestions[i].to_row;
      uint8_t dest_col = suggestions[i].to_col;
      uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

      // Check if this is a castling move - show as a special move
      if (suggestions[i].is_castling) {
        if (chess_policy_move_hints_castling_blue()) {
          led_set_pixel_safe(led_index, 0, 0, 255);
          ESP_LOGI(TAG, "🏰 Castling move highlighted at %c%d (blue)",
                   'a' + dest_col, dest_row + 1);
        }
      } else {
        piece_t dest_piece = board[dest_row][dest_col];
        bool is_opponent_piece =
            (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
             dest_piece <= PIECE_BLACK_KING) ||
            (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
             dest_piece <= PIECE_WHITE_KING);

        if (suggestions[i].is_check) {
          led_set_pixel_safe(led_index, 255, 255, 255);
        } else if (is_opponent_piece) {
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          // Green for empty squares
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }
    }
  }

  game_log_pickup_latency(cmd);

  // MATRIX COMPATIBILITY: Show opponent pieces after pickup (same as matrix
  // flow)
  if (!guided_capture_state.active && !game_is_castle_animation_active() &&
      !game_is_castling_expected()) {
    chess_policy_highlight_movable_if_enabled();
  }

  // Send success response
  char success_msg[128];
  // Safe snprintf for success message
  int written = snprintf(success_msg, sizeof(success_msg),
                         "✅ Piece lifted from %s - %" PRIu32 " possible moves",
                         cmd->from_notation, valid_moves);
  if (written >= sizeof(success_msg)) {
    snprintf(success_msg, sizeof(success_msg),
             "✅ Piece lifted - %" PRIu32 " moves", valid_moves);
  }
  game_send_response_to_uart(success_msg, false,
                             (QueueHandle_t)cmd->response_queue);
}

/**
 * @brief Enhanced drop command processing with smart error handling
 * @param cmd Drop command
 */

/**
 * @brief Process drop command (DN)
 */
/**
 * @brief Final integrated drop command with all fixes
 * @param cmd Drop command
 */

/**
 * @brief Process drop command (DN) - Legacy function
 */
void game_process_drop_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;
  game_squares_guard_release_hall_hold();

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (game_cmd_is_squares_origin(cmd) && cmd->event_ms != 0) {
    game_hall_position_on_matrix_event(cmd);
    return;
  }
  s_hall_drop_contradicts = false;
#endif

  // BUG FIX 1: Block moves while waiting for deployment
  if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    game_send_response_to_uart(
        "⏳ The game is waiting for the physical arrangement of the pieces. "
        "Place all pieces in their starting positions.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_squares_guard_active()) {
    if (game_cmd_is_squares_origin(cmd)) {
      game_squares_guard_try_clear_from_matrix();
      if (!game_is_squares_guard_active()) {
        return;
      }
      game_squares_guard_render_leds();
      return;
    }
    game_send_response_to_uart(
        "⏸️ Pending comparison of physical board (matrix guard). "
        "DROP from UART/Web is blocked.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_puzzle_setup_active() || game_puzzle_sync_hold()) {
    game_puzzle_service_leds();
    game_send_response_to_uart(
        "⏸️ Puzzle position is not ready. Fix the lit squares first.", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }
  if (game_puzzle_is_solved()) {
    game_send_response_to_uart("✅ Puzzle already solved.", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  ESP_LOGI(TAG, "🎯 Processing DROP command: %s (START)", cmd->to_notation);

  // MOVED: New game detection moved to end of function to avoid false
  // positives during move sequence

  // Convert notation to coordinates
  uint8_t to_row, to_col;
  if (!convert_notation_to_coords(cmd->to_notation, &to_row, &to_col)) {
    ESP_LOGE(TAG, "❌ Invalid notation: %s", cmd->to_notation);
    game_send_response_to_uart("❌ Invalid square notation", true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Always interrupt the red flashing every time you place a piece (DROP) - whatever
  // the user placed during or after the animation; pick up–put down quickly
  // thus always stopping the blinking before the next redraw.
  game_stop_error_blink();

  if (guided_capture_state.active) {
    if (game_guided_capture_has_mode_conflict()) {
      STAGING_LOGI(TAG, "Guided capture cancelled due to mode conflict (drop)");
      game_exit_guided_capture(true);
      game_send_response_to_uart(
          "⚠️ Guided capture cancelled due to higher-priority game state.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    if (!game_refresh_guided_attackers()) {
      STAGING_LOGI(TAG, "Guided capture cancelled (no legal attackers at drop)");
      game_exit_guided_capture(true);
      game_send_response_to_uart(
          "⚠️ Guided capture cancelled - no legal capture available now.", true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    if (!piece_lifted || lifted_piece == PIECE_EMPTY) {
      // Escape/cancel path: opponent piece dropped back to target square.
      if (to_row == guided_capture_state.target_row &&
          to_col == guided_capture_state.target_col) {
        game_exit_guided_capture(true);
        game_send_response_to_uart(
            "↩️ Guided capture cancelled. Opponent piece returned to target square.",
            false, (QueueHandle_t)cmd->response_queue);
        return;
      }
      game_show_guided_capture_leds();
      game_send_response_to_uart(
          game_get_guided_capture_hints_enabled()
              ? "⚠️ Guided capture active: lift highlighted YELLOW piece first."
              : "⚠️ Guided capture active: lift your piece first, then drop on PURPLE.",
          true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    bool is_allowed_attacker = false;
    for (uint8_t i = 0; i < guided_capture_state.attacker_count; i++) {
      if (guided_capture_state.attacker_rows[i] == lifted_piece_row &&
          guided_capture_state.attacker_cols[i] == lifted_piece_col &&
          guided_capture_state.attacker_pieces[i] == lifted_piece) {
        is_allowed_attacker = true;
        break;
      }
    }

    if (!is_allowed_attacker) {
      // Prevent stale "piece still lifted" state when attacker set changed.
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      game_show_guided_capture_leds();
      game_send_response_to_uart(
          game_get_guided_capture_hints_enabled()
              ? "⚠️ This piece is not allowed. Use highlighted YELLOW attacker."
              : "⚠️ This piece is not allowed. Use a piece that can capture.",
          true,
          (QueueHandle_t)cmd->response_queue);
      return;
    }

    if (to_row != guided_capture_state.target_row ||
        to_col != guided_capture_state.target_col) {
      // User put the attacker back on source square -> graceful cancel.
      if (to_row == lifted_piece_row && to_col == lifted_piece_col) {
        game_exit_guided_capture(true);
        game_send_response_to_uart(
            "↩️ Guided capture cancelled. Attacker returned to source square.",
            false, (QueueHandle_t)cmd->response_queue);
        return;
      }

      // Wrong destination was used; clear lifted state to avoid ghost-lift.
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      game_exit_guided_capture(false);
      hall_note_restore(cmd);
      return;
#else
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      game_show_guided_capture_leds();
      game_send_response_to_uart(
          "⚠️ Wrong destination. Lift attacker again and drop on PURPLE target.",
          true,
          (QueueHandle_t)cmd->response_queue);
      return;
#endif
    }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    {
      uint8_t land = (uint8_t)(to_row * 8 + to_col);
      hall_piece_type_t ht = squares_get_hall_piece_type(land);
      hall_piece_color_t hc = squares_get_hall_piece_color(land);
      if (!game_hall_identity_matches(lifted_piece, ht, hc)) {
        game_exit_guided_capture(false);
        hall_note_restore(cmd);
        return;
      }
    }
#endif
    uint8_t source_row = lifted_piece_row;
    uint8_t source_col = lifted_piece_col;
    chess_move_t guided_capture_move = {.from_row = source_row,
                                        .from_col = source_col,
                                        .to_row = to_row,
                                        .to_col = to_col,
                                        .piece = lifted_piece,
                                        .captured_piece =
                                            guided_capture_state.target_piece,
                                        .timestamp =
                                            esp_timer_get_time() / 1000};

    if (!game_execute_move(&guided_capture_move)) {
      STAGING_LOGI(TAG, "Guided capture rejected %c%d -> %c%d", 'a' + source_col,
                   source_row + 1, 'a' + to_col, to_row + 1);
      game_show_guided_capture_leds();
      game_send_response_to_uart(
          "❌ Guided capture rejected by legality checks. Try another attacker.",
          true, (QueueHandle_t)cmd->response_queue);
      return;
    }

    STAGING_LOGI(TAG, "Guided capture completed %c%d -> %c%d", 'a' + source_col,
                 source_row + 1, 'a' + to_col, to_row + 1);

    game_exit_guided_capture(true);
    game_send_response_to_uart("✅ Guided capture complete!", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_ep_capture_waiting()) {
    bool lifted_victim =
        piece_lifted && game_ep_capture_is_victim(lifted_piece_row, lifted_piece_col);
    if (lifted_victim) {
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
    }
    game_ep_capture_service();
    if (game_ep_capture_waiting()) {
      game_send_response_to_uart(
          "⚔️ En passant: take the pawn off the orange square.", true,
          (QueueHandle_t)cmd->response_queue);
    }
    return;
  }

  if (game_physical_undo_handle_drop(cmd, to_row, to_col)) {
    if (game_physical_undo_is_active()) {
      game_send_response_to_uart(
          "↩️ Takeback in progress — restore missing pieces (red blink).",
          false, (QueueHandle_t)cmd->response_queue);
    } else {
      game_send_response_to_uart("↩️ Takeback cancelled.", false,
                                 (QueueHandle_t)cmd->response_queue);
    }
    return;
  }

  // CASTLING INTERCEPTION: Handle rook moves during castling phase
  // This logic must be BEFORE standard validation to avoid "Blocked by King"
  // errors
  if (game_is_castle_animation_active()) {
    // 1. Check for completion (Drop on destination)
    // Use lifted_piece position if available, otherwise assume auto-recovery
    // from rook_from
    uint8_t effective_from_row =
        piece_lifted ? lifted_piece_row : rook_from_row;
    uint8_t effective_from_col =
        piece_lifted ? lifted_piece_col : rook_from_col;

    if (game_complete_castle_animation(effective_from_row, effective_from_col,
                                       to_row, to_col)) {
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      game_send_response_to_uart("✅ Castling completed", false,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    // 2. Check for "put back" (Drop on source) - Allow retry
    if (to_row == rook_from_row && to_col == rook_from_col) {
      ESP_LOGI(TAG, "🏰 Rook put back on start square - waiting for lift");
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      game_send_response_to_uart(
          "🏰 Rook put back - lift it again to complete castling", false,
          (QueueHandle_t)cmd->response_queue);
      return; // Keep castling active
    }

    // 3. Intermediate Move (Dynamic Source) - Robustness for "fumbled" moves
    // If dropped on an empty square that is NOT the destination, move the rook
    // there and update the animation source!
    if (board[to_row][to_col] == PIECE_EMPTY) {
      ESP_LOGI(TAG, "🏰 Intermediate rook move to %c%d (updating source)",
               'a' + to_col, to_row + 1);

      // Move piece in board simulation
      board[to_row][to_col] = board[rook_from_row][rook_from_col];
      board[rook_from_row][rook_from_col] = PIECE_EMPTY;

      // Update Static Vars for animation source
      rook_from_row = to_row;
      rook_from_col = to_col;

      // Update Lifted State
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;

      // Calculate LED indices for immediate feedback
      uint8_t from_led = chess_pos_to_led_index(rook_from_row, rook_from_col);
      uint8_t to_led = chess_pos_to_led_index(rook_to_row, rook_to_col);
      led_set_pixel_safe(from_led, 255, 255, 0); // Yellow (New Source)
      led_set_pixel_safe(to_led, 0, 255, 0);     // Green (Target)

      char msg[128];
      snprintf(msg, sizeof(msg),
               "🏰 Rook moved to %c%d - now move to GREEN pulsing square",
               'a' + to_col, to_row + 1);
      game_send_response_to_uart(msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    // 4. Invalid move (Occupied square)
    ESP_LOGW(TAG, "❌ Invalid rook move during castling to %c%d (Occupied)",
             'a' + to_col, to_row + 1);
    game_send_response_to_uart(
        "❌ Square occupied - move rook to flashing green square", true,
        (QueueHandle_t)cmd->response_queue);
    // Do not clear state, allow user to try again
    return;
  }

  // PROMOTION (swap_then_choose, swap optional):
  // - Allow physical DN only on the graduation field (without normal drop/move flow).
  // - Ignore outside the graduation field and send a short message.
  if (promotion_state.pending) {
    bool is_promo_square = (to_row == promotion_state.square_row &&
                            to_col == promotion_state.square_col);

    if (is_promo_square) {
      ESP_LOGI(TAG,
               "👑 Promotion pending: DROP on promo square %c%d allowed "
               "(swap_then_choose)",
               'a' + to_col, to_row + 1);

      char info_msg[256];
      snprintf(info_msg, sizeof(info_msg),
               "👑 Promotion pending at %c%d\n"
               "✅ Swap on promo square registered (optional)\n"
               "➡️ Put a queen, rook, bishop, or knight of your color on this square",
               'a' + promotion_state.square_col,
               promotion_state.square_row + 1);
      game_send_response_to_uart(info_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      game_promotion_sync_hall_if_pending();
#endif
      return; // Do not perform standard drop flow
    }

    ESP_LOGI(TAG,
             "⏸️  Promotion pending: DROP blocked outside promo square (%s)",
             cmd->to_notation);

    char info_msg[256];
    snprintf(info_msg, sizeof(info_msg),
             "⏸️ Promotion pending at %c%d\n"
             "🔁 Swap allowed only on %c%d (optional)\n"
             "➡️ Put a queen, rook, bishop, or knight of your color on that square",
             'a' + promotion_state.square_col, promotion_state.square_row + 1,
             'a' + promotion_state.square_col, promotion_state.square_row + 1);
    game_send_response_to_uart(info_msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // KING RESIGNATION: Resign only when the user places the king on his original
  // field. When returning an opponent's piece (opponent return) NEVER interfere - king
  // remains in the hand.
  bool in_opponent_return =
      (current_game_state == GAME_STATE_WAITING_FOR_RETURN &&
       opponent_piece_moved);
  if (resignation_state.active && in_opponent_return) {
    ESP_LOGI(TAG,
             "🔄 [STAGING] DROP during opponent return – keeping resignation "
             "timer (king still in hand)");
  }
  if (resignation_state.active && !in_opponent_return &&
      to_row == resignation_state.king_row &&
      to_col == resignation_state.king_col) {
    ESP_LOGI(TAG,
             "👑 DROP on king square %c%d – cancelling resignation (king "
             "placed back)",
             'a' + to_col, to_row + 1);
    resignation_stop(false);
    // We cleared piece_lifted after opponent-return; the king returns without a track.
    if (!piece_lifted) {
      led_clear_board_only();
      chess_policy_highlight_movable_if_enabled();
      game_send_response_to_uart("✅ King placed - resignation cancelled",
                                 false, (QueueHandle_t)cmd->response_queue);
      return;
    }
  }

  // POST-GAME PERSISTENCE: If game is finished, just update board visuals
  // This handles the "King Resignation" scenario where the user puts the King
  // down AFTER the timer has already ended the game. We must not lose the King!
  if (current_game_state == GAME_STATE_FINISHED && piece_lifted) {
    ESP_LOGI(TAG, "🏁 Game finished - preserving piece drop at %s",
             cmd->to_notation);

    // Force update board to match physical reality
    board[to_row][to_col] = lifted_piece;

    // Clear lifted status
    piece_lifted = false;
    lifted_piece = PIECE_EMPTY;
    lifted_piece_row = 0;
    lifted_piece_col = 0;

    // Update LED to show the piece is there (maybe gold/winner color? or just
    // standard) For now, let the victory animation take over, or just clear. We
    // send a generic response.
    game_send_response_to_uart("✅ Board synced (Game Over)", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // 3-STEP CAPTURE: Check if completing capture
  if (capture_in_progress) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    if (!cmd->is_demo_mode && game_hall_hand_count() > 0) {
      game_hall_pick_t cap_pick;
      if (game_hall_hand_pick_drop(to_row, to_col, &cap_pick) ==
              GAME_HALL_PICK_OK &&
          !cap_pick.contradicted && cap_pick.sensor_hit &&
          (cap_pick.piece != lifted_piece || cap_pick.row != lifted_piece_row ||
           cap_pick.col != lifted_piece_col)) {
        if (cap_pick.row == to_row && cap_pick.col == to_col) {
          if (board[to_row][to_col] == PIECE_EMPTY) {
            board[to_row][to_col] = cap_pick.piece;
          }
          game_hall_hand_remove_at(cap_pick.index);
          hall_show_hand_moves();
          game_send_response_to_uart("↩️ Piece put back.", false,
                                     (QueueHandle_t)cmd->response_queue);
          return;
        }
        lifted_piece = cap_pick.piece;
        lifted_piece_row = cap_pick.row;
        lifted_piece_col = cap_pick.col;
        piece_lifted = true;
      }
    }
#endif
    if (to_row == capture_target_row && to_col == capture_target_col) {
      ESP_LOGI(TAG, "♟️  Capture complete at %c%d", 'a' + to_col, to_row + 1);

      // CRITICAL FIX: If the resignation timer is active and the king is doing a capture
      // move, cancel resignation timer BEFORE calling game_execute_move()
      if (resignation_state.active && (lifted_piece == PIECE_WHITE_KING ||
                                       lifted_piece == PIECE_BLACK_KING)) {
        ESP_LOGI(TAG, "👑 King made a capture move during resignation timer - "
                      "cancelling resignation");
        resignation_stop(false);
        ESP_LOGI(TAG, "👑 Resignation cancelled - king captured piece at %c%d",
                 'a' + to_col, to_row + 1);
      }

      chess_move_t capture_move = {.from_row = lifted_piece_row,
                                   .from_col = lifted_piece_col,
                                   .to_row = to_row,
                                   .to_col = to_col,
                                   .piece = lifted_piece,
                                   .captured_piece = capture_removed_piece,
                                   .timestamp = esp_timer_get_time() / 1000};

      // Temporarily restore captured piece for validation
      // game_is_valid_move needs to see the opponent piece to validate capture
      board[to_row][to_col] = capture_removed_piece;

      if (game_execute_move(&capture_move)) {
        capture_in_progress = false;
        capture_target_row = 0;
        capture_target_col = 0;
        capture_removed_piece = PIECE_EMPTY;
        game_hall_hand_remove_origin(capture_move.from_row, capture_move.from_col);

        // NO manual player switch - game_execute_move() already does it!
        // Player is already switched by game_execute_move()

        // Timer already handled by game_execute_move() too
        // Just do post-move cleanup

        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;

        // If the capture move triggered graduation, DO NOT turn on the normal post-move LED,
        // so as not to overwhelm the promotion UX (promo square anchor + green buttons).
        if (promotion_state.pending) {
          ESP_LOGI(TAG,
                   "👑 Capture resulted in promotion pending at %c%d - keeping "
                   "promotion UX",
                   'a' + promotion_state.square_col,
                   promotion_state.square_row + 1);

          // Keep promo square anchor (no clear)
          game_update_promotion_anchor_led();

          char promo_msg[256];
          snprintf(promo_msg, sizeof(promo_msg),
                   "👑 Promotion required at %c%d\n"
                   "🔁 You may swap on this square now (optional)\n"
                   "➡️ Put a queen, rook, bishop, or knight of your color on this square",
                   'a' + promotion_state.square_col,
                   promotion_state.square_row + 1);
          game_send_response_to_uart(promo_msg, false,
                                     (QueueHandle_t)cmd->response_queue);
          return;
        }

        chess_policy_refresh_idle_board_leds();

        game_send_response_to_uart("✅ Capture complete!", false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    } else {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      capture_in_progress = false;
#else
      // Cancel capture
      board[capture_target_row][capture_target_col] = capture_removed_piece;
      capture_in_progress = false;
      game_send_response_to_uart("⚠️ Capture cancelled", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
#endif
    }
  }

  // Kontrola opponent piece recovery
  if (current_game_state == GAME_STATE_WAITING_FOR_RETURN &&
      opponent_piece_moved) {
    ESP_LOGI(TAG, "🔄 Processing opponent piece return");

    if (to_row == opponent_original_row && to_col == opponent_original_col) {
      ESP_LOGI(TAG, "✅ Opponent piece returned to original position %c%d",
               'a' + to_col, to_row + 1);

      // Return the piece to the board
      board[to_row][to_col] = opponent_piece_type;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      game_hall_hand_remove_origin(to_row, to_col);
#endif

      // Reset error_recovery_state (was set at PICKUP)
      if (error_recovery_state.waiting_for_move_correction) {
        ESP_LOGI(
            TAG,
            "🔄 Clearing error_recovery_state after opponent piece return");
        game_reset_error_recovery_state();
      }

      // Resetovat opponent piece recovery stav
      opponent_piece_moved = false;
      opponent_piece_type = PIECE_EMPTY;
      opponent_original_row = 0;
      opponent_original_col = 0;
      opponent_current_row = 0;
      opponent_current_col = 0;
      current_game_state = GAME_STATE_ACTIVE;

      // Resetovat lifted piece state
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // Resetovat lifted piece state
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // Clear LED and show normal status
      led_clear_board_only();
      chess_policy_highlight_movable_if_enabled();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (game_hall_hand_count() > 0) {
        hall_show_hand_moves();
      }
#endif

      game_send_response_to_uart("✅ Opponent piece returned - continue game",
                                 false, (QueueHandle_t)cmd->response_queue);
      return;
    }

    // INCORRECT POSITION: The figure is placed in a different position
    ESP_LOGW(TAG,
             "⚠️ Opponent piece placed at wrong position %c%d (should be %c%d)",
             'a' + to_col, to_row + 1, 'a' + opponent_original_col,
             opponent_original_row + 1);

    // Update current position
    opponent_current_row = to_row;
    opponent_current_col = to_col;

    // Update the board (the piece is now in a new position)
    board[to_row][to_col] = opponent_piece_type;

    // Update lifted piece state for next attempt
    lifted_piece_row = to_row;
    lifted_piece_col = to_col;

    // LED: Display the red LED at the original position (where the figure should be
    // returned)
    led_clear_board_only();
    led_set_pixel_safe(
        chess_pos_to_led_index(opponent_original_row, opponent_original_col),
        255, 0, 0); // Solid red

    char error_msg[256];
    int written =
        snprintf(error_msg, sizeof(error_msg),
                 "⚠️ Opponent piece at wrong position %c%d\n"
                 "🔴 Return to RED square (%c%d) to continue",
                 'a' + to_col, to_row + 1, 'a' + opponent_original_col,
                 opponent_original_row + 1);
    if (written >= sizeof(error_msg)) {
      strcpy(error_msg, "⚠️ Return opponent piece to RED square");
    }

    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // CHECK: Is the figure in error recovery state?
  if (error_recovery_state.waiting_for_move_correction) {
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    hall_note_restore(cmd);
    return;
#else
    ESP_LOGI(TAG, "🔄 Processing move correction from error state");

    // CHECK RETURN TO THE ORIGINAL POSITION
    if (to_row == error_recovery_state.original_valid_row &&
        to_col == error_recovery_state.original_valid_col) {

      ESP_LOGI(TAG, "↩️ Piece returned to ORIGINAL valid position %c%d",
               'a' + to_col, to_row + 1);

      // CRITICAL: Fix board status
      // Remove a figure from an invalid position
      board[error_recovery_state.invalid_row]
           [error_recovery_state.invalid_col] = PIECE_EMPTY;

      // Return the piece to its original valid position
      board[to_row][to_col] = error_recovery_state.piece_type;

      // RESETOVAT error stav
      game_reset_error_recovery_state();

      // RESETOVAT lifted piece state
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // Clear LED and show normal status
      game_stop_error_blink(); // STOP blinking red LED!
      led_clear_board_only();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      game_hall_hand_remove_origin(to_row, to_col);
      if (game_hall_hand_count() > 0) {
        hall_show_hand_moves();
      } else {
        chess_policy_highlight_movable_if_enabled();
      }
#else
      chess_policy_highlight_movable_if_enabled();
#endif

      game_send_response_to_uart("✅ Piece returned to original valid position",
                                 false, (QueueHandle_t)cmd->response_queue);
      return;
    }

    // Attempting another move from an error position - validate as a normal move
    chess_move_t correction_move = {
        .from_row = error_recovery_state.invalid_row,
        .from_col = error_recovery_state.invalid_col,
        .to_row = to_row,
        .to_col = to_col,
        .piece = error_recovery_state.piece_type,
        .captured_piece = board[to_row][to_col],
        .timestamp = esp_timer_get_time() / 1000};

    move_error_t error = game_is_valid_move(&correction_move);
    if (error == MOVE_ERROR_NONE) {
      // Valid corrective move!
      ESP_LOGI(TAG, "✅ Valid correction move from error position");

      // Aktualizovat board stav
      board[error_recovery_state.invalid_row]
           [error_recovery_state.invalid_col] = PIECE_EMPTY;
      board[to_row][to_col] = error_recovery_state.piece_type;

      // Perform a normal move execution
      if (game_execute_move(&correction_move)) {
        // Successful move - reset the error state
        game_reset_error_recovery_state();

        // Change player and switch timer
        player_t previous_player = current_player;
        current_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

        // Timer integration: End timer for previous player and start for new
        // player
        ESP_LOGI(TAG,
                 "🔄 Timer switch (correction): ending for %s, starting for %s",
                 previous_player == PLAYER_WHITE ? "White" : "Black",
                 current_player == PLAYER_WHITE ? "White" : "Black");
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);

        // CRITICAL FIX: Checkmate/stalemate after correction move!
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;
          ESP_LOGI(TAG, "🎉 Game finished detected in correction move!");
        }

        // Resetovat lifted piece state
        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;

        // Clear LED and show new state (blinking already interrupted at start of DROP)
        led_clear_board_only();
        chess_policy_highlight_movable_if_enabled();

        char success_msg[128];
        snprintf(success_msg, sizeof(success_msg),
                 "✅ Correction move successful: %s", cmd->to_notation);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    }

    // Still invalid move - continue in error state
    ESP_LOGE(TAG, "❌ Correction move still invalid: error %d", error);

    // CRITICAL: Update board status and invalid position!
    // The figure PHYSICALLY moved from A5 to H5, we have to reflect that

    // 1. Move the figure on the board from old invalid to new invalid
    board[error_recovery_state.invalid_row][error_recovery_state.invalid_col] =
        PIECE_EMPTY;
    board[to_row][to_col] = error_recovery_state.piece_type;

    // 2. Update the disabled position to a new one
    error_recovery_state.invalid_row = to_row; // H5
    error_recovery_state.invalid_col = to_col;

    // 3. Aktualizovat lifted piece tracking
    lifted_piece_row = to_row;
    lifted_piece_col = to_col;

    ESP_LOGI(TAG,
             "🔧 Successive invalid move: updated invalid position to %c%d "
             "(original_valid still %c%d)",
             'a' + to_col, to_row + 1,
             'a' + error_recovery_state.original_valid_col,
             error_recovery_state.original_valid_row + 1);

    // 4. Send UART response
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Still invalid move to %c%d - return to ORIGINAL %c%d or find "
             "valid move",
             'a' + to_col, to_row + 1,
             'a' + error_recovery_state.original_valid_col,
             error_recovery_state.original_valid_row + 1);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);

    // 5. Flash red on the NEW disabled position (H5, not A5!)
    game_show_invalid_move_error_with_blink(error_recovery_state.invalid_row,
                                            error_recovery_state.invalid_col);
    return;
#endif
  }

  // NORMAL PROCESSING - checking if we have a lifted piece
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  if (!cmd->is_demo_mode && game_hall_hand_count() > 0) {
    if (hall_bind_drop(cmd, to_row, to_col)) {
      return;
    }
  } else
#endif
  if (!piece_lifted && game_hall_hand_count() == 0) {
    ESP_LOGE(TAG, "❌ No piece was lifted - use UP command first");
    game_send_response_to_uart("❌ No piece was lifted - use UP command first",
                               true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  // CHECK CANCEL MOVE (drop on same field)
  if (piece_lifted && to_row == lifted_piece_row &&
      to_col == lifted_piece_col) {
    ESP_LOGI(TAG, "🔄 Move cancelled: piece placed on source square %c%d",
             'a' + to_col, to_row + 1);

    game_hall_hand_remove_origin(to_row, to_col);
    piece_t held = PIECE_EMPTY;
    uint8_t hr = 0;
    uint8_t hc = 0;
    if (game_hall_hand_count() > 0 &&
        game_hall_hand_get(0, &held, &hr, &hc)) {
      piece_lifted = true;
      lifted_piece = held;
      lifted_piece_row = hr;
      lifted_piece_col = hc;
    } else {
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;
    }

    if (castling_state.in_progress) {
      game_show_castling_rook_guidance();
    } else {
      led_clear_board_only();
      chess_policy_highlight_movable_if_enabled();
    }

    game_send_response_to_uart("✅ Move cancelled", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (castling_state.in_progress) {
    bool rook_done = lifted_piece_row == castling_state.rook_from_row &&
                     lifted_piece_col == castling_state.rook_from_col &&
                     to_row == castling_state.rook_to_row &&
                     to_col == castling_state.rook_to_col;
    if (!rook_done) {
      uint8_t wrong_row = to_row;
      uint8_t wrong_col = to_col;
      piece_lifted = false;
      lifted_piece = PIECE_EMPTY;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      game_show_castling_rook_guidance();
      led_set_pixel_safe(chess_pos_to_led_index(wrong_row, wrong_col), 255, 0,
                         0);
      game_send_response_to_uart("🏰 Move the rook from yellow to green.", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }
  }

  bool use_score_landing = true;
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  use_score_landing = cmd->is_demo_mode;
#endif
  if (use_score_landing &&
      (game_hall_hand_count() > 1 ||
       (!piece_lifted && game_hall_hand_count() > 0))) {
    int picked = hand_choose_landing(to_row, to_col);
    piece_t held;
    uint8_t hr;
    uint8_t hc;
    if (picked >= 0 && game_hall_hand_get(picked, &held, &hr, &hc)) {
      if (hr == to_row && hc == to_col) {
        game_hall_hand_remove_at(picked);
        if (game_hall_hand_count() == 0) {
          piece_lifted = false;
          lifted_piece = PIECE_EMPTY;
        } else if (game_hall_hand_get(0, &held, &hr, &hc)) {
          piece_lifted = true;
          lifted_piece = held;
          lifted_piece_row = hr;
          lifted_piece_col = hc;
        }
        game_send_response_to_uart("↩️ Piece put back.", false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
      lifted_piece = held;
      lifted_piece_row = hr;
      lifted_piece_col = hc;
    }
  }

  // PUSH VALIDATION
  chess_move_t move = {.from_row = lifted_piece_row,
                       .from_col = lifted_piece_col,
                       .to_row = to_row,
                       .to_col = to_col,
                       .piece = lifted_piece,
                       .captured_piece = board[to_row][to_col],
                       .timestamp = esp_timer_get_time() / 1000};

  // Castling detection BEFORE validation (for resignation timer handling)
  bool is_castling =
      (lifted_piece == PIECE_WHITE_KING || lifted_piece == PIECE_BLACK_KING) &&
      abs((int)to_col - (int)lifted_piece_col) == 2;

  // If there is a castling turn and the resignation timer is active, cancel the resignation
  if (is_castling && resignation_state.active) {
    ESP_LOGI(TAG, "🏰 Castling detected during resignation timer - cancelling "
                  "resignation");
    resignation_stop(false);
    ESP_LOGI(TAG, "🏰 Resignation cancelled - continuing with castling flow");
  }

  move_error_t error = game_is_valid_move(&move);
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
  bool hall_drop_ok = cmd->is_demo_mode;
  if (!hall_drop_ok && error == MOVE_ERROR_NONE) {
    bool completing_castle =
        castling_state.in_progress &&
        lifted_piece_row == castling_state.rook_from_row &&
        lifted_piece_col == castling_state.rook_from_col &&
        to_row == castling_state.rook_to_row &&
        to_col == castling_state.rook_to_col &&
        (lifted_piece == PIECE_WHITE_ROOK || lifted_piece == PIECE_BLACK_ROOK);

    if (completing_castle) {
      piece_t king_piece = (lifted_piece == PIECE_WHITE_ROOK) ? PIECE_WHITE_KING
                                                              : PIECE_BLACK_KING;
      int king_row = -1;
      int king_col = -1;
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
          if (board[r][c] == king_piece) {
            king_row = r;
            king_col = c;
            break;
          }
        }
        if (king_row >= 0) {
          break;
        }
      }
      hall_drop_ok =
          king_row >= 0 &&
          game_hall_validate_castling_rook_drop(
              to_row, to_col, lifted_piece, (uint8_t)king_row, (uint8_t)king_col,
              king_piece);
    } else if ((lifted_piece == PIECE_WHITE_PAWN && to_row == 7) ||
               (lifted_piece == PIECE_BLACK_PAWN && to_row == 0)) {
      hall_drop_ok = game_hall_validate_promotion_entry_drop(to_row, to_col,
                                                             lifted_piece);
    } else {
      hall_drop_ok =
          game_hall_validate_drop_square(to_row, to_col, lifted_piece);
    }
  }
  if (s_hall_drop_contradicts) {
    hall_drop_ok = false;
  }
#else
  bool hall_drop_ok = true;
#endif
  if (error == MOVE_ERROR_NONE && hall_drop_ok) {
    // VALID MOVE - normal processing
    ESP_LOGI(TAG, "✅ Valid move detected");

    if (game_is_opening_trainer_active()) {
      if (game_opening_awaiting_opponent_physical()) {
        if (!game_opening_validate_expected_move(lifted_piece_row, lifted_piece_col,
                                                 to_row, to_col)) {
          bool already_in_error = error_recovery_state.waiting_for_move_correction;
          game_opening_record_wrong_uci(lifted_piece_row, lifted_piece_col, to_row,
                                        to_col);
          game_opening_on_wrong_player_move();
          STAGING_LOGI(TAG, "opening: wrong opponent move %c%d -> %c%d",
                       'a' + lifted_piece_col, lifted_piece_row + 1, 'a' + to_col,
                       to_row + 1);
          chess_policy_error_recovery_enter_lock();
          error_recovery_state.has_invalid_piece = true;
          error_recovery_state.piece_type = lifted_piece;
          error_recovery_state.invalid_row = to_row;
          error_recovery_state.invalid_col = to_col;
          if (!already_in_error) {
            error_recovery_state.original_valid_row = lifted_piece_row;
            error_recovery_state.original_valid_col = lifted_piece_col;
          }
          if (chess_policy_error_recovery_should_mutate_board()) {
            game_stamp_misplaced_piece(lifted_piece_row, lifted_piece_col, to_row,
                                       to_col, lifted_piece);
            lifted_piece_row = to_row;
            lifted_piece_col = to_col;
          }
          game_show_invalid_move_error_with_blink(to_row, to_col);
          return;
        }
      } else if (!game_opening_validate_expected_move(lifted_piece_row, lifted_piece_col,
                                               to_row, to_col)) {
        bool already_in_error = error_recovery_state.waiting_for_move_correction;
        game_opening_record_wrong_uci(lifted_piece_row, lifted_piece_col, to_row,
                                      to_col);
        game_opening_on_wrong_player_move();
        STAGING_LOGI(TAG, "opening: wrong move %c%d -> %c%d",
                     'a' + lifted_piece_col, lifted_piece_row + 1, 'a' + to_col,
                     to_row + 1);

        chess_policy_error_recovery_enter_lock();
        error_recovery_state.has_invalid_piece = true;
        error_recovery_state.piece_type = lifted_piece;
        error_recovery_state.invalid_row = to_row;
        error_recovery_state.invalid_col = to_col;
        if (!already_in_error) {
          error_recovery_state.original_valid_row = lifted_piece_row;
          error_recovery_state.original_valid_col = lifted_piece_col;
        }
        if (chess_policy_error_recovery_should_mutate_board()) {
          game_stamp_misplaced_piece(lifted_piece_row, lifted_piece_col, to_row,
                                     to_col, lifted_piece);
          lifted_piece_row = to_row;
          lifted_piece_col = to_col;
        }
        game_show_invalid_move_error_with_blink(to_row, to_col);
        return;
      }
    }

    if (puzzle_active) {
      bool is_expected_solution = game_puzzle_drop_is_expected(
          lifted_piece_row, lifted_piece_col, to_row, to_col);
      if (!is_expected_solution) {
        bool already_in_error = error_recovery_state.waiting_for_move_correction;
        puzzle_feedback = PUZZLE_FEEDBACK_WRONG;
        STAGING_LOGI(TAG, "puzzle: wrong move %c%d -> %c%d for id=%u",
                     'a' + lifted_piece_col, lifted_piece_row + 1, 'a' + to_col,
                     to_row + 1, (unsigned)puzzle_active_id);

        chess_policy_error_recovery_enter_lock();
        error_recovery_state.has_invalid_piece = true;
        error_recovery_state.piece_type = lifted_piece;
        error_recovery_state.invalid_row = to_row;
        error_recovery_state.invalid_col = to_col;

        if (!already_in_error) {
          error_recovery_state.original_valid_row = lifted_piece_row;
          error_recovery_state.original_valid_col = lifted_piece_col;
        }

        if (chess_policy_error_recovery_should_mutate_board()) {
          game_stamp_misplaced_piece(lifted_piece_row, lifted_piece_col, to_row,
                                     to_col, lifted_piece);
          lifted_piece_row = to_row;
          lifted_piece_col = to_col;
        }

        game_show_invalid_move_error_with_blink(to_row, to_col);

        char error_msg[256];
        snprintf(
            error_msg, sizeof(error_msg),
            "❌ Puzzle: that's not it (%s). Return figure to ORIGINAL %c%d.",
            cmd->to_notation, 'a' + error_recovery_state.original_valid_col,
            error_recovery_state.original_valid_row + 1);
        game_send_response_to_uart(error_msg, true,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    }

    // CRITICAL FIX: If the resignation timer is active and the king makes valid
    // move (not castling, not laying back), cancel resignation timer Casting already
    // is covered above, so here we check the king's normal moves
    if (resignation_state.active &&
        (lifted_piece == PIECE_WHITE_KING ||
         lifted_piece == PIECE_BLACK_KING) &&
        !is_castling) {
      // Checking whether the king is placed back in the original position (it is handled
      // elsewhere) If it's a different position than the original, it's a normal move → cancel
      // resignation
      if (to_row != resignation_state.king_row ||
          to_col != resignation_state.king_col) {
        ESP_LOGI(TAG, "👑 King made a normal move during resignation timer - "
                      "cancelling resignation");
        resignation_stop(false);
        ESP_LOGI(TAG, "👑 Resignation cancelled - king moved to %c%d",
                 'a' + to_col, to_row + 1);
      }
    }

    // CRITICAL FIX: Check castling_state.in_progress BEFORE calling
    // game_execute_move() because game_execute_move_enhanced() resets
    // castling_state.in_progress = false If castling_state.in_progress,
    // player change animation will now start in game_execute_move_enhanced()
    bool is_castling_completion_before_move = castling_state.in_progress;

    ESP_LOGI(TAG,
             "🔍 game_process_drop_command: BEFORE game_execute_move, "
             "current_player=%s, is_castling_completion_before_move=%d",
             current_player == PLAYER_WHITE ? "White" : "Black",
             is_castling_completion_before_move);
    uint8_t hand_from_row = move.from_row;
    uint8_t hand_from_col = move.from_col;
    bool move_success = game_execute_move(&move);
    if (move_success) {
      game_hall_hand_remove_origin(hand_from_row, hand_from_col);
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (is_castling && s_hall_rook_parked && castling_state.in_progress) {
        piece_t rook = PIECE_EMPTY;
        uint8_t rr = 0;
        uint8_t rc = 0;
        bool have_rook = false;
        for (int i = 0; i < game_hall_hand_count(); i++) {
          piece_t held;
          uint8_t hr;
          uint8_t hc;
          if (!game_hall_hand_get(i, &held, &hr, &hc)) {
            continue;
          }
          if (hr == castling_state.rook_from_row &&
              hc == castling_state.rook_from_col) {
            rook = held;
            rr = hr;
            rc = hc;
            have_rook = true;
            break;
          }
        }
        s_hall_rook_parked = false;
        if (have_rook) {
          chess_move_t rook_move = {.from_row = rr,
                                    .from_col = rc,
                                    .to_row = castling_state.rook_to_row,
                                    .to_col = castling_state.rook_to_col,
                                    .piece = rook,
                                    .captured_piece = PIECE_EMPTY};
          if (game_execute_move(&rook_move)) {
            game_hall_hand_remove_origin(rr, rc);
          }
        }
      }
#endif
    }
    ESP_LOGI(TAG,
             "🔍 game_process_drop_command: AFTER game_execute_move, "
             "move_success=%d",
             move_success);

    if (move_success) {
      if (puzzle_active && !promotion_state.pending) {
        game_puzzle_after_accepted_move();
        char success_msg[128];
        convert_coords_to_notation(move.from_row, move.from_col, success_msg);
        snprintf(success_msg + strlen(success_msg),
                 sizeof(success_msg) - strlen(success_msg), " -> %s",
                 cmd->to_notation);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;
        return;
      }
      if (game_is_opening_trainer_active()) {
        if (game_opening_awaiting_opponent_physical()) {
          game_opening_advance_after_opponent_physical();
        } else {
          game_opening_advance_after_correct();
        }
        if (error_recovery_state.waiting_for_move_correction) {
          game_reset_error_recovery_state();
        }
        char opening_msg[128];
        snprintf(opening_msg, sizeof(opening_msg),
                 "✅ Opening move OK (%s)", game_opening_feedback_key());
        game_send_response_to_uart(opening_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
      // CRITICAL FIX: Reset error state on any valid move!
      if (error_recovery_state.waiting_for_move_correction) {
        ESP_LOGI(TAG, "✅ Valid move - clearing error recovery state");
        game_reset_error_recovery_state();
      }

      // #1: Check if promotion is pending - if so, SKIP
      // timer/animations They will execute in
      // game_process_promotion_command() after user selects piece
      if (promotion_state.pending) {
        ESP_LOGI(TAG,
                 "⏸️  Promotion pending at %c%d - skipping timer switch and "
                 "animations",
                 'a' + promotion_state.square_col,
                 promotion_state.square_row + 1);
        ESP_LOGI(TAG, "⏸️  Awaiting promotion piece on the lit square");
        ESP_LOGI(TAG, "⏸️  Timer remains with %s player",
                 current_player == PLAYER_WHITE ? "White" : "Black");

        // Keep promo square visually anchored (does not clear board)
        game_update_promotion_anchor_led();
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
        game_promotion_sync_hall_if_pending();
#endif

        // Send success response without animations
        char success_msg[256];
        snprintf(
            success_msg, sizeof(success_msg),
            "✅ Valid move: %c%d → %c%d\n"
            "👑 Promotion required at %c%d\n"
            "🔁 Optional: swap on %c%d now\n"
            "➡️ Put a queen, rook, bishop, or knight of your color on that square",
            'a' + move.from_col, move.from_row + 1, 'a' + to_col, to_row + 1,
            'a' + promotion_state.square_col, promotion_state.square_row + 1,
            'a' + promotion_state.square_col, promotion_state.square_row + 1);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return; // Exit here - no timer switch, no animations
      }

      // Use is_castling_completion_before_move (saved BEFORE
      // game_execute_move()) because game_execute_move_enhanced() resets
      // castling_state.in_progress = false If it is
      // is_castling_completion_before_move, player change animation already done
      // run in game_execute_move_enhanced()

      if (!is_castling && !is_castling_completion_before_move) {
        // PLAYER ALREADY CHANGED In game_execute_move() - use current player
        // Timer integration: End timer for previous player and start for new
        // player
        player_t previous_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        ESP_LOGI(TAG, "🔄 Timer switch (drop): ending for %s, starting for %s",
                 previous_player == PLAYER_WHITE ? "White" : "Black",
                 current_player == PLAYER_WHITE ? "White" : "Black");
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);

        // CRITICAL: Endgame check BEFORE player change animations!
        // If endgame, player change will NOT start
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;

          ESP_LOGI(TAG, "✅ Game finished detected - animation triggered by "
                        "check function");
          ESP_LOGI(
              TAG,
              "✅ Endgame animation started - player change animation SKIPPED");
        }
      } // Konec bloku if (!is_castling && !is_castling_completion_before_move)

      // Check animation always starts (even when castling is completed)
      // Check if new player is in check (only if move_success)
      bool in_check = game_is_king_in_check(current_player);
      if (in_check) {
        // Find the position of the king
        int king_row = -1, king_col = -1;
        piece_t king_piece = (current_player == PLAYER_WHITE)
                                 ? PIECE_WHITE_KING
                                 : PIECE_BLACK_KING;
        for (int row = 0; row < 8; row++) {
          for (int col = 0; col < 8; col++) {
            if (board[row][col] == king_piece) {
              king_row = row;
              king_col = col;
              break;
            }
          }
          if (king_row != -1)
            break;
        }

        if (king_row != -1 && king_col != -1) {
          if (game_led_guidance_show_check_anim()) {
            uint8_t king_led_index = chess_pos_to_led_index(king_row, king_col);
            // Start the check animation - pink lighting on the king
            led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                       .led_index = king_led_index,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms =
                                           0, // Permanent until next turn
                                       .data = NULL};
            led_execute_command_new(&check_cmd);
            ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                     current_player == PLAYER_WHITE ? "White" : "Black");
          }
        }
      }

      // Reset lifted piece state after a successful move
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // MATRIX COMPATIBILITY: Show opponent pieces after successful move
      // (same as matrix flow) - ONLY if game is active
      if (current_game_state == GAME_STATE_ACTIVE &&
          !game_is_castle_animation_active() && !game_is_castling_expected() &&
          !game_puzzle_suppress_move_hints()) {
        chess_policy_refresh_idle_board_leds();
      }
      if (!promotion_state.pending &&
          current_game_state != GAME_STATE_FINISHED) {
        led_move_trail_start(move.from_row, move.from_col, move.to_row,
                             move.to_col);
      } else {
        led_move_trail_cancel();
      }
#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
      if (castling_state.in_progress) {
        game_show_castling_rook_guidance();
      } else if (game_hall_hand_count() > 0 && !promotion_state.pending) {
        hall_show_hand_moves();
      }
#endif

      char success_msg[128];
      convert_coords_to_notation(move.from_row, move.from_col, success_msg);
      snprintf(success_msg + strlen(success_msg),
               sizeof(success_msg) - strlen(success_msg), " -> %s",
               cmd->to_notation);
      ESP_LOGI(TAG, "📤 Sending success response...");
      game_send_response_to_uart(success_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      ESP_LOGI(TAG, "📤 Success response sent");
    }
  } else {
    // INVALID MOVE - ENHANCED ERROR HANDLING
    if (error == MOVE_ERROR_NONE && !hall_drop_ok) {
      ESP_LOGW(TAG, "❌ Hall type/color mismatch on drop square %s",
               cmd->to_notation);
    } else {
      ESP_LOGW(TAG, "❌ Invalid move attempt: %s", cmd->to_notation);
    }
    if (puzzle_active) {
      puzzle_feedback = PUZZLE_FEEDBACK_ILLEGAL;
    }
    if (game_is_opening_trainer_active()) {
      game_opening_record_wrong_uci(lifted_piece_row, lifted_piece_col, to_row,
                                    to_col);
      game_opening_on_illegal_player_move();
    }

#if CONFIG_CHESS_MATRIX_INPUT_I2C_HALL
    hall_note_restore(cmd);
    return;
#else
    // CRITICAL: Distinguish the first error vs other errors
    bool already_in_error = error_recovery_state.waiting_for_move_correction;

    // COMMON SETTINGS for all errors
    chess_policy_error_recovery_enter_lock();
    error_recovery_state.has_invalid_piece = true;
    error_recovery_state.piece_type = lifted_piece;
    error_recovery_state.invalid_row =
        to_row; // Update to new invalid position
    error_recovery_state.invalid_col = to_col;

    // CRITICAL: Set original_valid ONLY on FIRST error!
    // In case of other errors, DO NOT rewrite - the original remains the same (A2)
    if (!already_in_error) {
      error_recovery_state.original_valid_row =
          lifted_piece_row; // A2 - first valid position
      error_recovery_state.original_valid_col = lifted_piece_col;
      ESP_LOGI(TAG, "🔧 First error: original_valid set to %c%d",
               'a' + lifted_piece_col, lifted_piece_row + 1);
    } else {
      ESP_LOGI(TAG,
               "🔧 Successive error: original_valid stays at %c%d (invalid "
               "moves to %c%d)",
               'a' + error_recovery_state.original_valid_col,
               error_recovery_state.original_valid_row + 1, 'a' + to_col,
               to_row + 1);
    }

    // UPDATE board status - piece is now on an invalid field (physical
    // reality) Board must match the physical state for PICKUP to work
    // correctly!
    if (chess_policy_error_recovery_should_mutate_board()) {
      game_stamp_misplaced_piece(lifted_piece_row, lifted_piece_col, to_row,
                                 to_col, lifted_piece);
      lifted_piece_row = to_row;
      lifted_piece_col = to_col;
    }

    // LED INDICATION - red flashing on an invalid field
    game_show_invalid_move_error_with_blink(to_row, to_col);

    char error_msg[256];
    if (error == MOVE_ERROR_NONE && !hall_drop_ok) {
      snprintf(error_msg, sizeof(error_msg),
               "❌ Wrong piece on %s (Hall type/color) - return to ORIGINAL "
               "%c%d",
               cmd->to_notation, 'a' + error_recovery_state.original_valid_col,
               error_recovery_state.original_valid_row + 1);
    } else {
      snprintf(
          error_msg, sizeof(error_msg),
          "❌ Invalid move to %s - return to ORIGINAL %c%d or find valid move",
          cmd->to_notation, 'a' + error_recovery_state.original_valid_col,
          error_recovery_state.original_valid_row + 1);
    }
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
#endif
  }
}
void game_process_castle_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "🏰 Processing CASTLE command: %s", cmd->to_notation);

  // Parse castle direction
  bool is_kingside = (strcmp(cmd->to_notation, "kingside") == 0);
  bool is_queenside = (strcmp(cmd->to_notation, "queenside") == 0);

  if (!is_kingside && !is_queenside) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Invalid castle direction: '%s'\n"
             "💡 Valid options: 'kingside' or 'queenside'\n"
             "💡 Examples: CASTLE kingside, CASTLE queenside\n"
             "💡 Notation: O-O (kingside), O-O-O (queenside)",
             cmd->to_notation);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Check if castling is legal (simplified check)
  bool can_castle = false;
  char castle_notation[8];

  if (current_player == PLAYER_WHITE) {
    if (is_kingside) {
      // Check if White can castle kingside (simplified)
      can_castle =
          (board[0][4] == PIECE_WHITE_KING && board[0][7] == PIECE_WHITE_ROOK);
      strcpy(castle_notation, "O-O");
    } else {
      // Check if White can castle queenside (simplified)
      can_castle =
          (board[0][4] == PIECE_WHITE_KING && board[0][0] == PIECE_WHITE_ROOK);
      strcpy(castle_notation, "O-O-O");
    }
  } else {
    if (is_kingside) {
      // Check if Black can castle kingside (simplified)
      can_castle =
          (board[7][4] == PIECE_BLACK_KING && board[7][7] == PIECE_BLACK_ROOK);
      strcpy(castle_notation, "O-O");
    } else {
      // Check if Black can castle queenside (simplified)
      can_castle =
          (board[7][4] == PIECE_BLACK_KING && board[7][0] == PIECE_BLACK_ROOK);
      strcpy(castle_notation, "O-O-O");
    }
  }

  if (can_castle) {
    // ED: Actually perform castling with LED animations
    uint8_t king_row = (current_player == PLAYER_WHITE) ? 0 : 7;
    uint8_t king_from_col = 4;
    uint8_t king_to_col = is_kingside ? 6 : 2;
    uint8_t rook_from_col = is_kingside ? 7 : 0;
    uint8_t rook_to_col = is_kingside ? 5 : 3;

    // Get LED indices for animation
    uint8_t king_from_led = chess_pos_to_led_index(king_row, king_from_col);
    uint8_t king_to_led = chess_pos_to_led_index(king_row, king_to_col);
    uint8_t rook_from_led = chess_pos_to_led_index(king_row, rook_from_col);
    uint8_t rook_to_led = chess_pos_to_led_index(king_row, rook_to_col);

    // 1. Highlight king and rook positions (gold)
    led_set_pixel_safe(king_from_led, 255, 215, 0); // Gold
    led_set_pixel_safe(rook_from_led, 255, 215, 0); // Gold
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. Show castling animation
    led_command_t castle_cmd = {.type = LED_CMD_ANIM_CASTLE,
                                .led_index = king_from_led,
                                .red = 255,
                                .green = 215,
                                .blue = 0, // Gold
                                .duration_ms = 1500,
                                .data = &king_to_led};
    led_execute_command_new(&castle_cmd);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 3. Actually perform the castling move
    piece_t king = board[king_row][king_from_col];
    piece_t rook = board[king_row][rook_from_col];

    // Move king
    board[king_row][king_from_col] = PIECE_EMPTY;
    board[king_row][king_to_col] = king;

    // Move rook
    board[king_row][rook_from_col] = PIECE_EMPTY;
    board[king_row][rook_to_col] = rook;

    // Update castling flags
    if (current_player == PLAYER_WHITE) {
      white_king_moved = true;
      piece_moved[0][4] = true;
      if (is_kingside) {
        white_rook_h_moved = true;
        piece_moved[0][7] = true;
      } else {
        white_rook_a_moved = true;
        piece_moved[0][0] = true;
      }
    } else {
      black_king_moved = true;
      piece_moved[7][4] = true;
      if (is_kingside) {
        black_rook_h_moved = true;
        piece_moved[7][7] = true;
      } else {
        black_rook_a_moved = true;
        piece_moved[7][0] = true;
      }
    }

    // 4. Highlight final positions (green)
    led_set_pixel_safe(king_to_led, 0, 255, 0); // Green
    led_set_pixel_safe(rook_to_led, 0, 255, 0); // Green
    vTaskDelay(pdMS_TO_TICKS(500));

    // 5. Clear highlights
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(200));

    char success_msg[256];
    snprintf(success_msg, sizeof(success_msg),
             "✅ Castling successful!\n"
             "  • Move: %s\n"
             "  • Player: %s\n"
             "  • Type: %s\n"
             "  • Notation: %s",
             castle_notation,
             current_player == PLAYER_WHITE ? "White" : "Black",
             is_kingside ? "Kingside" : "Queenside", castle_notation);
    game_send_response_to_uart(success_msg, false,
                               (QueueHandle_t)cmd->response_queue);

    // Switch player
    current_player =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
  } else {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Castling not possible!\n"
             "═══════════════════════════════════════════════════════════════\n"
             "🎯 Player: %s\n"
             "🎯 Attempted: %s castling\n"
             "🎯 Notation: %s\n"
             "\n"
             "🚫 Possible reasons:\n"
             "  • King has already moved\n"
             "  • Rook has already moved\n"
             "  • Path between king and rook is blocked\n"
             "  • King is currently in check\n"
             "  • King would pass through check\n"
             "  • King would end up in check\n"
             "\n"
             "💡 Castling requirements:\n"
             "  • King and rook must not have moved\n"
             "  • No pieces between king and rook\n"
             "  • King not in check\n"
             "  • King doesn't pass through or into check\n"
             "\n"
             "🔍 Check current position with 'BOARD' command",
             current_player == PLAYER_WHITE ? "White" : "Black",
             is_kingside ? "Kingside" : "Queenside",
             is_kingside ? "O-O" : "O-O-O");
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
  }
}
void game_process_promote_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  ESP_LOGI(TAG, "👑 Processing PROMOTE command: %s=%s", cmd->from_notation,
           cmd->to_notation);

  // Parse promotion square and piece
  uint8_t row, col;
  if (!convert_notation_to_coords(cmd->from_notation, &row, &col)) {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Invalid square notation: '%s'\n"
             "💡 Valid format: letter + number (e.g., 'e8', 'a1', 'h7')\n"
             "💡 Letters: a-h (columns)\n"
             "💡 Numbers: 1-8 (rows)\n"
             "💡 Example: PROMOTE e8=Q",
             cmd->from_notation);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Check if there's a pawn to promote
  piece_t current_piece = board[row][col];
  bool is_white_pawn = (current_piece == PIECE_WHITE_PAWN);
  bool is_black_pawn = (current_piece == PIECE_BLACK_PAWN);

  if (!is_white_pawn && !is_black_pawn) {
    char error_msg[1024];
    const char *piece_name = "Unknown";
    if (current_piece != PIECE_EMPTY) {
      switch (current_piece) {
      case PIECE_WHITE_KNIGHT:
        piece_name = "White Knight";
        break;
      case PIECE_WHITE_BISHOP:
        piece_name = "White Bishop";
        break;
      case PIECE_WHITE_ROOK:
        piece_name = "White Rook";
        break;
      case PIECE_WHITE_QUEEN:
        piece_name = "White Queen";
        break;
      case PIECE_WHITE_KING:
        piece_name = "White King";
        break;
      case PIECE_BLACK_PAWN:
        piece_name = "Black Pawn";
        break;
      case PIECE_BLACK_KNIGHT:
        piece_name = "Black Knight";
        break;
      case PIECE_BLACK_BISHOP:
        piece_name = "Black Bishop";
        break;
      case PIECE_BLACK_ROOK:
        piece_name = "Black Rook";
        break;
      case PIECE_BLACK_QUEEN:
        piece_name = "Black Queen";
        break;
      case PIECE_BLACK_KING:
        piece_name = "Black King";
        break;
      default:
        piece_name = "Unknown piece";
        break;
      }
    }

    snprintf(
        error_msg, sizeof(error_msg),
        "❌ No pawn to promote at %s\n"
        "═══════════════════════════════════════════════════════════════\n"
        "🎯 Square: %s\n"
        "🎯 Current piece: %s\n"
        "\n"
        "💡 Promotion requirements:\n"
        "  • Must be a pawn (not %s)\n"
        "  • Pawn must be on 8th rank (for White) or 1st rank (for Black)\n"
        "  • Square must not be empty\n"
        "\n"
        "🔍 Check current position with 'BOARD' command\n"
        "💡 Use 'MOVES pawn' to see all pawn positions",
        cmd->from_notation, cmd->from_notation,
        current_piece == PIECE_EMPTY ? "Empty square" : piece_name,
        current_piece == PIECE_EMPTY ? "empty" : "this piece");
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // Parse promotion piece
  piece_t promotion_piece;
  const char *piece_name;

  switch (toupper(cmd->to_notation[0])) {
  case 'Q':
    promotion_piece = is_white_pawn ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;
    piece_name = "Queen";
    break;
  case 'R':
    promotion_piece = is_white_pawn ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    piece_name = "Rook";
    break;
  case 'B':
    promotion_piece = is_white_pawn ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
    piece_name = "Bishop";
    break;
  case 'N':
    promotion_piece = is_white_pawn ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
    piece_name = "Knight";
    break;
  default: {
    char error_msg[1024];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Invalid promotion piece: '%s'\n"
             "═══════════════════════════════════════════════════════════════\n"
             "💡 Valid promotion pieces:\n"
             "  • Q = Queen (most common)\n"
             "  • R = Rook\n"
             "  • B = Bishop\n"
             "  • N = Knight\n"
             "\n"
             "💡 Examples:\n"
             "  • PROMOTE e8=Q (promote to Queen)\n"
             "  • PROMOTE a8=R (promote to Rook)\n"
             "  • PROMOTE h8=B (promote to Bishop)\n"
             "  • PROMOTE c8=N (promote to Knight)\n"
             "\n"
             "🎯 Note: Queen is the most powerful choice!",
             cmd->to_notation);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }
  }

  // ED: Add LED animations for promotion
  uint8_t promotion_led = chess_pos_to_led_index(row, col);

  // 1. Highlight promotion square (purple for special move)
  led_set_pixel_safe(promotion_led, 128, 0, 128); // Purple
  vTaskDelay(pdMS_TO_TICKS(500));

  // 2. Show transformation animation (pawn -> promoted piece)
  for (int i = 0; i < 3; i++) {
    // Flash between pawn color and promoted piece color
    if (i % 2 == 0) {
      // Pawn color (white/black)
      led_set_pixel_safe(promotion_led, is_white_pawn ? 255 : 100,
                         is_white_pawn ? 255 : 100, is_white_pawn ? 255 : 100);
    } else {
      // Promoted piece color (gold)
      led_set_pixel_safe(promotion_led, 255, 215, 0); // Gold
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  // 3. Perform promotion
  board[row][col] = promotion_piece;

  // 4. Show final promoted piece (gold)
  led_set_pixel_safe(promotion_led, 255, 215, 0); // Gold
  vTaskDelay(pdMS_TO_TICKS(500));

  // 5. Show celebration animation (rainbow colors)
  for (int i = 0; i < 5; i++) {
    uint8_t r = (i * 51) % 255;
    uint8_t g = ((i * 51) + 85) % 255;
    uint8_t b = ((i * 51) + 170) % 255;
    led_set_pixel_safe(promotion_led, r, g, b);
    vTaskDelay(pdMS_TO_TICKS(150));
  }

  // 6. Clear highlights
  led_clear_board_only();
  vTaskDelay(pdMS_TO_TICKS(200));

  char success_msg[256];
  snprintf(success_msg, sizeof(success_msg),
           "✅ Pawn promotion successful!\n"
           "  • Square: %s\n"
           "  • Promoted to: %s\n"
           "  • Player: %s\n"
           "  • Notation: %s=%s",
           cmd->from_notation, piece_name,
           current_player == PLAYER_WHITE ? "White" : "Black",
           cmd->from_notation, cmd->to_notation);

  game_send_response_to_uart(success_msg, false,
                             (QueueHandle_t)cmd->response_queue);

  // Switch player
  current_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
}
void game_process_chess_move(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  // CRITICAL FIX: Security check of all notation strings (arrays
  // are always != NULL)
  if (strlen(cmd->from_notation) == 0 || strlen(cmd->from_notation) > 7 ||
      strlen(cmd->to_notation) == 0 || strlen(cmd->to_notation) > 7) {
    ESP_LOGE(TAG, "❌ Invalid or corrupted move notation");
    return;
  }

  // BLOCK MOVES IF PROMOTION IS PENDING
  if (promotion_state.pending) {
    ESP_LOGW(TAG,
             "⚠️ Move ignored - promotion pending. Complete promotion first.");
    game_send_response_to_uart(
        "⚠️ Promotion pending. Put a queen, rook, bishop, or knight on the lit square.", true,
        (QueueHandle_t)cmd->response_queue);
    return;
  }

  ESP_LOGI(TAG, "🎯 Processing UART chess move: %s -> %s (player: %d)",
           cmd->from_notation, cmd->to_notation, cmd->player);

  // Convert notation to coordinates
  uint8_t from_row, from_col, to_row, to_col;
  if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col) ||
      !convert_notation_to_coords(cmd->to_notation, &to_row, &to_col)) {
    ESP_LOGE(TAG, "❌ Invalid notation: %s -> %s", cmd->from_notation,
             cmd->to_notation);
    return;
  }

  // Create chess move structure
  chess_move_t move = {.from_row = from_row,
                       .from_col = from_col,
                       .to_row = to_row,
                       .to_col = to_col,
                       .piece = board[from_row][from_col],
                       .captured_piece = board[to_row][to_col],
                       .timestamp = 0};

  // Validate move first
  move_error_t error = game_is_valid_move(&move);
  ESP_LOGI(TAG, "🔍 MOVE VALIDATION: error = %d", error);

  if (error == MOVE_ERROR_NONE) {
    ESP_LOGI(TAG, "✅ Move is valid, starting UART move animation...");

    // STEP 1: Show piece lifting animation (same as HW)
    ESP_LOGI(TAG, "🔄 Step 1: Lifting piece from %s", cmd->from_notation);
    // ED: No animation when lifting piece, just highlight possible moves

    // Send LED command to highlight source square
    led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255,
                       0); // Yellow for source

    // Give FreeRTOS scheduler time to process LED commands
    vTaskDelay(pdMS_TO_TICKS(
        50)); // 50ms - enough for scheduler, not too long for watchdog

    // STEP 2: Show possible moves (same as HW)
    ESP_LOGI(TAG, "🔄 Step 2: Showing possible moves from %s",
             cmd->from_notation);

    // Get valid moves for this piece
    move_suggestion_t suggestions[64];
    uint32_t valid_moves =
        game_get_available_moves(from_row, from_col, suggestions, 64);

    ESP_LOGI(TAG, "Found %lu valid moves for piece at %s", valid_moves,
             cmd->from_notation);

    // Send LED commands to highlight possible destinations
    if (valid_moves > 0) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if destination has opponent's piece
        piece_t dest_piece = board[dest_row][dest_col];
        bool is_opponent_piece =
            (current_player == PLAYER_WHITE && dest_piece >= PIECE_BLACK_PAWN &&
             dest_piece <= PIECE_BLACK_KING) ||
            (current_player == PLAYER_BLACK && dest_piece >= PIECE_WHITE_PAWN &&
             dest_piece <= PIECE_WHITE_KING);

        if (is_opponent_piece) {
          // Orange for opponent's pieces (capture)
          led_set_pixel_safe(led_index, 255, 165, 0);
        } else {
          // Green for empty squares
          led_set_pixel_safe(led_index, 0, 255, 0);
        }
      }
    }

    // Give FreeRTOS scheduler time to process LED commands
    vTaskDelay(pdMS_TO_TICKS(
        50)); // 50ms - enough for scheduler, not too long for watchdog

    // STEP 3: Execute the move immediately (same as HW)
    ESP_LOGI(TAG, "🔄 Step 3: Executing move %s -> %s", cmd->from_notation,
             cmd->to_notation);

    s_uart_move_immediate_promotion =
        (cmd->promotion_from_remote != 0) &&
        (cmd->promotion_choice <= (uint8_t)PROMOTION_KNIGHT);
    s_uart_move_immediate_promotion_piece =
        (promotion_choice_t)cmd->promotion_choice;

    if (game_execute_move(&move)) {
      ESP_LOGI(TAG, "✅ UART move executed successfully: %s -> %s",
               cmd->from_notation, cmd->to_notation);

      // Send success to web/UART immediately so HTTP handler can receive and
      // close queue before we run animations/highlights (prevents use-after-free
      // crash when handler times out and deletes the queue).
      if (cmd->response_queue) {
        char success_msg[128];
        snprintf(success_msg, sizeof(success_msg), "Move executed: %s -> %s",
                 cmd->from_notation, cmd->to_notation);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
      }

      // Check if this is a castling move
      bool is_castling =
          (move.piece == PIECE_WHITE_KING || move.piece == PIECE_BLACK_KING) &&
          abs((int)move.to_col - (int)move.from_col) == 2;

      if (is_castling) {
        // Castling move - don't change player, don't show player change
        // animation
        ESP_LOGI(
            TAG,
            "🏰 UART Castling move executed - waiting for rook to be moved");

        // Print successful castling move with colors
        printf("\r\n");
        printf("\033[92m🏰 "
               "\033[1m"
               "CASTLING MOVE EXECUTED!"
               "\033[0m"
               "\r\n");
        printf("\033[93m   • Move: "
               "\033[1m"
               "%s → %s"
               "\033[0m"
               "\r\n",
               cmd->from_notation, cmd->to_notation);
        printf("\033[93m   • Piece: "
               "\033[1m"
               "%s"
               "\033[0m"
               "\r\n",
               piece_symbols[move.piece]);
        printf("\033[96m   • Next: Move the rook to complete castling!"
               "\033[0m"
               "\r\n");
        printf("\r\n");
      } else {
        // Normal move - switch player and show animation
        // Print successful move with colors
        printf("\r\n");
        printf("\033[92m✅ "
               "\033[1m"
               "MOVE EXECUTED SUCCESSFULLY!"
               "\033[0m"
               "\r\n");
        printf("\033[93m   • Move: "
               "\033[1m"
               "%s → %s"
               "\033[0m"
               "\r\n",
               cmd->from_notation, cmd->to_notation);
        printf("\033[93m   • Piece: "
               "\033[1m"
               "%s"
               "\033[0m"
               "\r\n",
               piece_symbols[move.piece]);
        if (move.captured_piece != PIECE_EMPTY) {
          printf("\033[93m   • Captured: "
                 "\033[1m"
                 "%s"
                 "\033[0m"
                 "\r\n",
                 piece_symbols[move.captured_piece]);
        }
        printf("\r\n");

        // ED: LED animations are now handled in game_process_drop_command
        // This function only shows text information

        // Removed blue flashing - animation is resolved in
        // game_process_drop_command LEDs turn off in
        // game_process_drop_command after a successful turn

        // PLAYER ALREADY CHANGED IN game_execute_move() - use current one
        // player
        player_t previous_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

        // CRITICAL: Endgame check BEFORE player change animations!
        // If endgame, player change will NOT start
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;

          // Find the position of the winning king for the endgame animation
          player_t winner =
              (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
          uint8_t king_pos = 28; // default e4
          for (int i = 0; i < 64; i++) {
            piece_t piece = board[i / 8][i % 8];
            if ((winner == PLAYER_WHITE && piece == PIECE_WHITE_KING) ||
                (winner == PLAYER_BLACK && piece == PIECE_BLACK_KING)) {
              king_pos = i;
              break;
            }
          }

          ESP_LOGI(TAG,
                   "🎯 Game finished in UART move! Starting endgame animation "
                   "at position %d",
                   king_pos);

          // Start the endgame animation (wave from king winner)
          led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                                       .led_index = king_pos,
                                       .red = 255,
                                       .green = 255,
                                       .blue = 0,        // Yellow
                                       .duration_ms = 0, // Endless
                                       .data = NULL};
          led_execute_command_new(&endgame_cmd);

          ESP_LOGI(TAG, "✅ Endgame animation started - player change "
                        "animation SKIPPED");
        } else {
          bool in_check = game_is_king_in_check(current_player);
          if (in_check) {
            // Find the position of the king
            int king_row = -1, king_col = -1;
            piece_t king_piece = (current_player == PLAYER_WHITE)
                                     ? PIECE_WHITE_KING
                                     : PIECE_BLACK_KING;
            for (int row = 0; row < 8; row++) {
              for (int col = 0; col < 8; col++) {
                if (board[row][col] == king_piece) {
                  king_row = row;
                  king_col = col;
                  break;
                }
              }
              if (king_row != -1)
                break;
            }

            if (king_row != -1 && king_col != -1) {
              if (game_led_guidance_show_check_anim()) {
                uint8_t king_led_index =
                    chess_pos_to_led_index(king_row, king_col);
                // Start the check animation - pink lighting on the king
                led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                           .led_index = king_led_index,
                                           .red = 0,
                                           .green = 0,
                                           .blue = 0,
                                           .duration_ms =
                                               0, // Permanent until next turn
                                           .data = NULL};
                led_execute_command_new(&check_cmd);
                ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                         current_player == PLAYER_WHITE ? "White" : "Black");
              }
            }
          }
        }

        // Timer integration: End timer for previous player and start for new
        // player
        ESP_LOGI(TAG, "🔄 Timer switch: ending for %s, starting for %s",
                 previous_player == PLAYER_WHITE ? "White" : "Black",
                 current_player == PLAYER_WHITE ? "White" : "Black");
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);

        // Unlock auto new game detection (player has moved)
        if (auto_new_game_blocked_until_move) {
          auto_new_game_blocked_until_move = false;
          ESP_LOGI(TAG, "🔓 Auto new game detection unblocked by valid move");
        }

        // Show moving figures for the new player, then the from→to trail.
        chess_policy_refresh_idle_board_leds();
        led_move_trail_start(move.from_row, move.from_col, move.to_row,
                             move.to_col);
      }
    } else {
      ESP_LOGE(TAG, "❌ Failed to execute UART move");
      game_send_response_to_uart("Failed to execute move", true,
                                 (QueueHandle_t)cmd->response_queue);
    }
  } else {
    // Move validation failed
    ESP_LOGW(TAG, "⚠️ Move rejected by validation: error=%d", error);

    // Provide detailed error message
    const char *err_msg = "Invalid move";
    switch (error) {
    case MOVE_ERROR_WRONG_COLOR:
      err_msg = "That's not your piece!";
      break;
    case MOVE_ERROR_NO_PIECE:
      err_msg = "No piece at source square";
      break;
    case MOVE_ERROR_DESTINATION_OCCUPIED:
      err_msg = "Destination blocked by your piece";
      break;
    case MOVE_ERROR_BLOCKED_PATH:
      err_msg = "Path is blocked";
      break;
    case MOVE_ERROR_ILLEGAL_MOVE:
      err_msg = "Illegal move for this piece";
      break;
    case MOVE_ERROR_CASTLING_BLOCKED:
      err_msg = "Castling blocked or invalid";
      break;
    case MOVE_ERROR_KING_IN_CHECK:
      err_msg = "King would be in check!";
      break;
    case MOVE_ERROR_OUT_OF_BOUNDS:
      err_msg = "Move out of bounds";
      break;
    default:
      err_msg = "Invalid move";
      break;
    }

    // Send error to Web/UART queue
    if (cmd->response_queue) {
      game_send_response_to_uart(err_msg, true,
                                 (QueueHandle_t)cmd->response_queue);
    }

    // Also trigger physical feedback (LEDs)
    game_handle_invalid_move(error, &move);
  }
}
