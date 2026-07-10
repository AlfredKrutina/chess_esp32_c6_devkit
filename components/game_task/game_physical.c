/**
 * @file game_physical.c
 * @brief Physical pickup/drop/castle/promote command processing.
 */

#include "game_physical.h"
#include "game_matrix_guard.h"
#include "game_move_exec.h"
#include "game_move_validate.h"
#include "game_snapshot.h"
#include "game_task_internal.h"

#include "game_task.h"
#include "game_led_animations.h"
#include "led_mapping.h"

#include "../led_task/include/led_task.h"
#include "../matrix_task/include/matrix_task.h"

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
 * Hledá vlastní figury, které mohou *sebrat na poli* (target_row,target_col).
 * En passant končí na prázdném poli za obětí — tah sem tedy typicky neprojde
 * přes game_is_valid_move(..., to=victim square); guided capture se v e.p.
 * situaci neaktivuje a hráč použije normální tah / 3-krokové braní.
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
 * - Útočník zvednutý: jen fialové pole oběti (kam položit).
 * - Výběr útočníka: vždy fialová oběť; žlutí jen legální útočníci pokud
 *   game_get_guided_capture_hints_enabled() NEBO led_guidance_level ≥ 3 (žluté figurky).
 *   Jinak jen fialová (úrovně 1–2).
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
  piece_lifted = false;
  lifted_piece = PIECE_EMPTY;
  lifted_piece_row = 0;
  lifted_piece_col = 0;

  if (restore_highlights) {
    led_clear_board_only();
    game_highlight_movable_pieces();
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

/**
 * @brief Process pickup command (UP)
 */
void game_process_pickup_command(const chess_move_command_t *cmd) {
  if (!cmd)
    return;

  // BUG FIX 1: Během čekání na rozestavení blokovat tahy
  if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    game_send_response_to_uart(
        "⏳ Hra čeká na fyzické rozestavení figurek. "
        "Umístěte všechny figurky na výchozí pozice.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_matrix_guard_active()) {
    if (game_cmd_is_matrix_origin(cmd)) {
      game_matrix_guard_try_clear_from_matrix();
      if (!game_is_matrix_guard_active()) {
        return;
      }
      game_matrix_guard_render_leds();
      return;
    }
    game_send_response_to_uart(
        "⏸️ Čeká se na srovnání fyzické desky (matrix guard). "
        "PICKUP z UART/Web je blokován.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  // PROMOTION (swap_then_choose, swap volitelný):
  // - Povolit fyzický UP/DN pouze na promočním poli (bez běžného pickup flow).
  // - Mimo promoční pole ignorovat (hra je logicky pozastavená) a poslat
  // krátkou hlášku.
  if (promotion_state.pending) {
    // Bezpečnostní kontrola notation stringu
    if (strlen(cmd->from_notation) == 0 || strlen(cmd->from_notation) > 7) {
      ESP_LOGE(TAG,
               "❌ Invalid or corrupted notation string (promotion pending)");
      game_send_response_to_uart("❌ Invalid notation format", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }

    // Parse square i během promotion pending
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
      ESP_LOGI(TAG,
               "👑 Promotion pending: PICKUP on promo square %c%d allowed "
               "(swap_then_choose)",
               'a' + p_col, p_row + 1);

      char info_msg[256];
      snprintf(info_msg, sizeof(info_msg),
               "👑 Promotion pending at %c%d\n"
               "🔁 You can swap the pawn here now (optional)\n"
               "➡️ Next: choose piece via green buttons / PROMOTE / web",
               'a' + promotion_state.square_col,
               promotion_state.square_row + 1);
      game_send_response_to_uart(info_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      return; // Neprovádět standardní pickup flow
    }

    ESP_LOGI(TAG,
             "⏸️  Promotion pending: PICKUP blocked outside promo square (%s)",
             cmd->from_notation);

    char info_msg[256];
    snprintf(info_msg, sizeof(info_msg),
             "⏸️ Promotion pending at %c%d\n"
             "🔁 Swap allowed only on %c%d (optional)\n"
             "➡️ Then choose piece via green buttons / PROMOTE / web",
             'a' + promotion_state.square_col, promotion_state.square_row + 1,
             'a' + promotion_state.square_col, promotion_state.square_row + 1);
    game_send_response_to_uart(info_msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // PŘERUŠIT BLIKÁNÍ při zvednutí figurky
  game_stop_error_blink();

  // Bezpečnostní kontrola notation stringu (array je vždy != NULL)
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

  // KONTROLA ERROR RECOVERY STAVU
  if (error_recovery_state.waiting_for_move_correction) {
    // KONTROLA: Je to správná neplatná pozice?
    if (from_row == error_recovery_state.invalid_row &&
        from_col == error_recovery_state.invalid_col) {

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

      // KLÍČOVÉ: Ukázat validní tahy z PŮVODNÍ pozice, ne z posledního
      // pokusu
      move_suggestion_t suggestions[64];
      uint32_t valid_moves = game_get_available_moves(
          error_recovery_state.original_valid_row,
          error_recovery_state.original_valid_col, suggestions, 64);

      ESP_LOGI(TAG, "💡 Found %lu valid moves from original position",
               valid_moves);

      // VYČISTIT A NASTAVIT LED
      led_clear_board_only();

      // Žlutá na aktuální pozici (kde je figurka nyní)
      led_set_pixel_safe(chess_pos_to_led_index(from_row, from_col), 255, 255,
                         0);

      // MODRÁ na původní validní pozici
      led_set_pixel_safe(
          chess_pos_to_led_index(error_recovery_state.original_valid_row,
                                 error_recovery_state.original_valid_col),
          0, 0, 255);

      // ZELENÉ LED pro validní cílová pole (z původní pozice)
      if (game_led_guidance_show_destinations()) {
        for (uint32_t i = 0; i < valid_moves; i++) {
          uint8_t dest_led = chess_pos_to_led_index(suggestions[i].to_row,
                                                    suggestions[i].to_col);

          // Nepřepisovat žlutou (aktuální pozici) a modrou (původní pozici)
          if (dest_led != chess_pos_to_led_index(from_row, from_col) &&
              dest_led != chess_pos_to_led_index(
                              error_recovery_state.original_valid_row,
                              error_recovery_state.original_valid_col)) {
            if (suggestions[i].is_capture) {
              led_set_pixel_safe(dest_led, 255, 165, 0); // Oranžová pro capture
            } else {
              led_set_pixel_safe(dest_led, 0, 255, 0); // Zelená pro normální tah
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
      // UP z jiné pozice během error recovery - ignorovat nebo resetovat?
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
    // Bezpečný snprintf s kontrolou návratové hodnoty
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

  // Check if piece belongs to current player
  bool is_white_piece =
      (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING);

  if ((current_player == PLAYER_WHITE && !is_white_piece) ||
      (current_player == PLAYER_BLACK && !is_black_piece)) {

    // 3-STEP CAPTURE FLOW: UP own → UP opponent (remove) → DN own (execute)
    // Rozlišit capture vs error

    // ISSUE 1: NESMÍ se povolit capture během error recovery!
    if (piece_lifted && lifted_piece != PIECE_EMPTY &&
        !error_recovery_state.waiting_for_move_correction &&
        ((current_player == PLAYER_WHITE && lifted_piece >= PIECE_WHITE_PAWN &&
          lifted_piece <= PIECE_WHITE_KING) ||
         (current_player == PLAYER_BLACK && lifted_piece >= PIECE_BLACK_PAWN &&
          lifted_piece <= PIECE_BLACK_KING))) {

      // STEP 2 of CAPTURE: Zkontrolovat zda lifted piece MŮŽE sebrat
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
        // VALIDNÍ CAPTURE: Remove opponent piece, wait for DN
        ESP_LOGI(TAG, "♟️  Step 2/3 Capture: Removing opponent piece at %s",
                 cmd->from_notation);

        // Odstranit opponent piece z boardu
        board[from_row][from_col] = PIECE_EMPTY;

        // Nastavit capture state
        capture_in_progress = true;
        capture_target_row = from_row;
        capture_target_col = from_col;
        capture_removed_piece = piece;

        // Decentní LED feedback: Žlutá na lifted piece, FIALOVÁ na target (kde
        // má polož it)
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
      ESP_LOGW(
          TAG,
          "⚠️ Opponent's piece lifted WITHOUT own piece lifted at %s - ERROR",
          safe_notation);
    }

    // KRITICKÉ: Nastavit error_recovery_state pro proper recovery flow
    error_recovery_state.has_invalid_piece = true;
    error_recovery_state.waiting_for_move_correction = true;
    error_recovery_state.piece_type = piece;
    error_recovery_state.invalid_row = from_row;
    error_recovery_state.invalid_col = from_col;
    error_recovery_state.original_valid_row =
        from_row; // Původní pozice = kde má být vrácena
    error_recovery_state.original_valid_col = from_col;
    error_recovery_state.error_count++;

    // Uložit informace o opponent piece (backward compatibility)
    opponent_piece_type = piece;
    opponent_original_row = from_row;
    opponent_original_col = from_col;
    opponent_current_row = from_row;
    opponent_current_col = from_col;
    opponent_piece_moved = true;

    // Odstranit figurku z boardu (byla zvednuta)
    board[from_row][from_col] = PIECE_EMPTY;

    // Nastavit recovery stav
    current_game_state = GAME_STATE_WAITING_FOR_RETURN;

    // LED: Spustit červené blikání na původní pozici
    led_clear_board_only();
    game_show_invalid_move_error_with_blink(from_row, from_col);

    // Nastavit lifted piece state pro případné drop
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
                    "🏰 Dokončete rošádu: přesuňte věž z %c%d na %c%d",
                    (char)('a' + castling_state.rook_from_col),
                    castling_state.rook_from_row + 1,
                    (char)('a' + castling_state.rook_to_col),
                    castling_state.rook_to_row + 1);
    if (n >= (int)sizeof(msg)) {
      strcpy(msg, "🏰 Dokončete rošádu: přesuňte věž na zelené pole");
    }
    game_send_response_to_uart(msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return; // Do not set piece_lifted – king stays on board
  }

  // KING RESIGNATION: Detekce zvednutí vlastního krále (skip v demo mode)
  if (!cmd->is_demo_mode &&
      ((piece == PIECE_WHITE_KING && current_player == PLAYER_WHITE) ||
       (piece == PIECE_BLACK_KING && current_player == PLAYER_BLACK))) {
    // Nejdřív spustit resignation timer (nastaví oranžovo-červenou na source)
    // Pak zobrazit valid moves (včetně castling moves jako modré)
    // Clear previous highlights
    led_clear_board_only();

    // Spustit resignation timer PŘED zobrazením valid moves
    // resignation_start() nastaví oranžovo-červenou mix na source square
    resignation_start(current_player, from_row, from_col);

    // Get valid moves pro krále (PO odstranění z boardu v resignation_start())
    // Poznámka: game_get_available_moves() musí fungovat i když je král
    // odstraněn z boardu (použije resignation_state.king_row/col pokud je
    // resignation aktivní)
    move_suggestion_t suggestions[64];
    uint32_t valid_moves =
        game_get_available_moves(from_row, from_col, suggestions, 64);

    ESP_LOGI(TAG, "🔄 Showing possible moves for king from %s (%lu moves)",
             cmd->from_notation, valid_moves);

    // Zobrazit valid moves (zelená/oranžová/modrá pro castling)
    if (valid_moves > 0 && game_led_guidance_show_destinations()) {
      for (uint32_t i = 0; i < valid_moves; i++) {
        uint8_t dest_row = suggestions[i].to_row;
        uint8_t dest_col = suggestions[i].to_col;
        uint8_t led_index = chess_pos_to_led_index(dest_row, dest_col);

        // Check if this is a castling move - zobrazit jako modré
        if (suggestions[i].is_castling) {
          // Blue for castling moves (special move - rošáda)
          led_set_pixel_safe(led_index, 0, 0, 255);
          ESP_LOGI(TAG,
                   "🏰 Castling move highlighted at %c%d (blue) during "
                   "resignation pickup",
                   'a' + dest_col, dest_row + 1);
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

    // Give FreeRTOS scheduler time to process LED commands
    vTaskDelay(pdMS_TO_TICKS(10)); // BUG FIX 3a: Změněno z 50ms na 10ms

    // Nastavit lifted piece state
    piece_lifted = true;
    lifted_piece_row = from_row;
    lifted_piece_col = from_col;
    lifted_piece = piece;

    // Poznámka: resignation_start() už nastaví oranžovo-červenou mix na source
    // square Valid moves (zelená/oranžová) zůstanou zobrazené, protože
    // resignation_start() nevolá led_clear_board_only()

    char msg[128];
    snprintf(msg, sizeof(msg),
             "👑 King lifted - resignation timer started (10s) - %" PRIu32
             " valid moves",
             valid_moves);
    game_send_response_to_uart(msg, false, (QueueHandle_t)cmd->response_queue);
    return; // Nepokračovat s normálním pickup flow
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

  // Give FreeRTOS scheduler time to process LED commands
  vTaskDelay(pdMS_TO_TICKS(10)); // BUG FIX 3a: Změněno z 50ms na 10ms

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

      // Check if this is a castling move - zobrazit jako speciální tah
      if (suggestions[i].is_castling) {
        // Blue for castling moves (special move - rošáda)
        led_set_pixel_safe(led_index, 0, 0, 255);
        ESP_LOGI(TAG, "🏰 Castling move highlighted at %c%d (blue)",
                 'a' + dest_col, dest_row + 1);
      } else {
        // Normal move handling (existing code)
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
  }

  // Give FreeRTOS scheduler time to process LED commands
  vTaskDelay(pdMS_TO_TICKS(10)); // BUG FIX 3a: Změněno z 50ms na 10ms

  // MATRIX COMPATIBILITY: Show opponent pieces after pickup (same as matrix
  // flow)
  if (!guided_capture_state.active && !game_is_castle_animation_active() &&
      !game_is_castling_expected()) {
    game_highlight_movable_pieces();
  }

  // Send success response
  char success_msg[128];
  // Bezpečný snprintf pro success message
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

  // BUG FIX 1: Během čekání na rozestavení blokovat tahy
  if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
    game_send_response_to_uart(
        "⏳ Hra čeká na fyzické rozestavení figurek. "
        "Umístěte všechny figurky na výchozí pozice.",
        true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  if (game_is_matrix_guard_active()) {
    if (game_cmd_is_matrix_origin(cmd)) {
      game_matrix_guard_try_clear_from_matrix();
      if (!game_is_matrix_guard_active()) {
        return;
      }
      game_matrix_guard_render_leds();
      return;
    }
    game_send_response_to_uart(
        "⏸️ Čeká se na srovnání fyzické desky (matrix guard). "
        "DROP z UART/Web je blokován.",
        true, (QueueHandle_t)cmd->response_queue);
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

  // Při každém položení figurky (DROP) vždy přerušit červené blikání – ať už
  // uživatel položil během animace nebo po ní; zvednutí–položení rychle za
  // sebou tak vždy zastaví blink před dalším překreslením.
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
    }

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

  // PROMOTION (swap_then_choose, swap volitelný):
  // - Povolit fyzický DN pouze na promočním poli (bez běžného drop/move flow).
  // - Mimo promoční pole ignorovat a poslat krátkou hlášku.
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
               "➡️ Next: choose piece via green buttons / PROMOTE / web",
               'a' + promotion_state.square_col,
               promotion_state.square_row + 1);
      game_send_response_to_uart(info_msg, false,
                                 (QueueHandle_t)cmd->response_queue);
      return; // Neprovádět standardní drop flow
    }

    ESP_LOGI(TAG,
             "⏸️  Promotion pending: DROP blocked outside promo square (%s)",
             cmd->to_notation);

    char info_msg[256];
    snprintf(info_msg, sizeof(info_msg),
             "⏸️ Promotion pending at %c%d\n"
             "🔁 Swap allowed only on %c%d (optional)\n"
             "➡️ Then choose piece via green buttons / PROMOTE / web",
             'a' + promotion_state.square_col, promotion_state.square_row + 1,
             'a' + promotion_state.square_col, promotion_state.square_row + 1);
    game_send_response_to_uart(info_msg, false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // KING RESIGNATION: Zrušit pouze když uživatel položí krále na jeho původní
  // pole. Při vrácení soupeřovy figury (opponent return) NIKDY nerušit – král
  // zůstává v ruce.
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
    // Po opponent-return jsme vyčistili piece_lifted; král se vrací bez tracku.
    if (!piece_lifted) {
      led_clear_board_only();
      game_highlight_movable_pieces();
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
    if (to_row == capture_target_row && to_col == capture_target_col) {
      ESP_LOGI(TAG, "♟️  Capture complete at %c%d", 'a' + to_col, to_row + 1);

      // KRITICKÁ OPRAVA: Pokud je resignation timer aktivní a král dělá capture
      // tah, zrušit resignation timer PŘED voláním game_execute_move()
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

        // NO manual player switch - game_execute_move() already does it!
        // Player is already switched by game_execute_move()

        // Timer already handled by game_execute_move() too
        // Just do post-move cleanup

        piece_lifted = false;
        lifted_piece_row = 0;
        lifted_piece_col = 0;
        lifted_piece = PIECE_EMPTY;

        // Pokud capture tah vyvolal promoci, NESPÍNAT normal post-move LED,
        // aby se nepřebil promotion UX (promo square anchor + zelená tlačítka).
        if (promotion_state.pending) {
          ESP_LOGI(TAG,
                   "👑 Capture resulted in promotion pending at %c%d - keeping "
                   "promotion UX",
                   'a' + promotion_state.square_col,
                   promotion_state.square_row + 1);

          // Udržet promo square anchor (bez clear)
          game_update_promotion_anchor_led();

          char promo_msg[256];
          snprintf(promo_msg, sizeof(promo_msg),
                   "👑 Promotion required at %c%d\n"
                   "🔁 You may swap on this square now (optional)\n"
                   "➡️ Next: choose piece via green buttons / PROMOTE / web",
                   'a' + promotion_state.square_col,
                   promotion_state.square_row + 1);
          game_send_response_to_uart(promo_msg, false,
                                     (QueueHandle_t)cmd->response_queue);
          return;
        }

        led_clear_board_only();
        game_highlight_movable_pieces();

        game_send_response_to_uart("✅ Capture complete!", false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    } else {
      // Cancel capture
      board[capture_target_row][capture_target_col] = capture_removed_piece;
      capture_in_progress = false;
      game_send_response_to_uart("⚠️ Capture cancelled", true,
                                 (QueueHandle_t)cmd->response_queue);
      return;
    }
  }

  // Kontrola opponent piece recovery
  if (current_game_state == GAME_STATE_WAITING_FOR_RETURN &&
      opponent_piece_moved) {
    ESP_LOGI(TAG, "🔄 Processing opponent piece return");

    if (to_row == opponent_original_row && to_col == opponent_original_col) {
      ESP_LOGI(TAG, "✅ Opponent piece returned to original position %c%d",
               'a' + to_col, to_row + 1);

      // Vrátit figurku na board
      board[to_row][to_col] = opponent_piece_type;

      // Resetovat error_recovery_state (byl nastaven při PICKUP)
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

      // Vyčistit LED a ukázat normální stav
      led_clear_board_only();
      game_highlight_movable_pieces();

      game_send_response_to_uart("✅ Opponent piece returned - continue game",
                                 false, (QueueHandle_t)cmd->response_queue);
      return;
    }

    // NESPRÁVNÉ POLOŽENÍ: Figurka se pokládá na jinou pozici
    ESP_LOGW(TAG,
             "⚠️ Opponent piece placed at wrong position %c%d (should be %c%d)",
             'a' + to_col, to_row + 1, 'a' + opponent_original_col,
             opponent_original_row + 1);

    // Aktualizovat aktuální pozici
    opponent_current_row = to_row;
    opponent_current_col = to_col;

    // Aktualizovat board (figurka je nyní na nové pozici)
    board[to_row][to_col] = opponent_piece_type;

    // Aktualizovat lifted piece state pro další pokus
    lifted_piece_row = to_row;
    lifted_piece_col = to_col;

    // LED: Zobrazit červenou LED na původní pozici (kde má být figurka
    // vrácena)
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

  // KONTROLA: Je figurka v error recovery stavu?
  if (error_recovery_state.waiting_for_move_correction) {
    ESP_LOGI(TAG, "🔄 Processing move correction from error state");

    // KONTROLA NÁVRATU NA PŮVODNÍ POZICI
    if (to_row == error_recovery_state.original_valid_row &&
        to_col == error_recovery_state.original_valid_col) {

      ESP_LOGI(TAG, "↩️ Piece returned to ORIGINAL valid position %c%d",
               'a' + to_col, to_row + 1);

      // KRITICKÉ: Opravit board stav
      // Odstranit figurku z neplatné pozice
      board[error_recovery_state.invalid_row]
           [error_recovery_state.invalid_col] = PIECE_EMPTY;

      // Vrátit figurku na původní validní pozici
      board[to_row][to_col] = error_recovery_state.piece_type;

      // RESETOVAT error stav
      game_reset_error_recovery_state();

      // RESETOVAT lifted piece state
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // Vyčistit LED a ukázat normální stav
      game_stop_error_blink(); // STOP blinking red LED!
      led_clear_board_only();
      game_highlight_movable_pieces();

      game_send_response_to_uart("✅ Piece returned to original valid position",
                                 false, (QueueHandle_t)cmd->response_queue);
      return;
    }

    // Pokus o jiný tah z error pozice - validovat jako normální tah
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
      // Validní korekční tah!
      ESP_LOGI(TAG, "✅ Valid correction move from error position");

      // Aktualizovat board stav
      board[error_recovery_state.invalid_row]
           [error_recovery_state.invalid_col] = PIECE_EMPTY;
      board[to_row][to_col] = error_recovery_state.piece_type;

      // Provést normální move execution
      if (game_execute_move(&correction_move)) {
        // Úspěšný tah - resetovat error stav
        game_reset_error_recovery_state();

        // Změnit hráče a přepnout timer
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

        // KRITICKÁ OPRAVA: Kontrola checkmate/stalemate po correction tahu!
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

        // Vyčistit LED a ukázat nový stav (blikání už přerušeno na začátku DROP)
        led_clear_board_only();
        game_highlight_movable_pieces();

        char success_msg[128];
        snprintf(success_msg, sizeof(success_msg),
                 "✅ Correction move successful: %s", cmd->to_notation);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    }

    // Stále neplatný tah - pokračovat v error stavu
    ESP_LOGE(TAG, "❌ Correction move still invalid: error %d", error);

    // KRITICKÝ: Aktualizovat board stav a invalid pozici!
    // Figurka se FYZICKY přesunula z A5 na H5, musíme to reflektovat

    // 1. Přesunout figurku na boardu z old invalid na new invalid
    board[error_recovery_state.invalid_row][error_recovery_state.invalid_col] =
        PIECE_EMPTY;
    board[to_row][to_col] = error_recovery_state.piece_type;

    // 2. Aktualizovat invalid pozici na novou
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

    // 4. Poslat odpověď UART
    char error_msg[256];
    snprintf(error_msg, sizeof(error_msg),
             "❌ Still invalid move to %c%d - return to ORIGINAL %c%d or find "
             "valid move",
             'a' + to_col, to_row + 1,
             'a' + error_recovery_state.original_valid_col,
             error_recovery_state.original_valid_row + 1);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);

    // 5. Blikat červeně na NOVÉ invalid pozici (H5, ne A5!)
    game_show_invalid_move_error_with_blink(error_recovery_state.invalid_row,
                                            error_recovery_state.invalid_col);
    return;
  }

  // NORMÁLNÍ PROCESSING - kontrola jestli máme lifted piece
  if (!piece_lifted) {
    ESP_LOGE(TAG, "❌ No piece was lifted - use UP command first");
    game_send_response_to_uart("❌ No piece was lifted - use UP command first",
                               true, (QueueHandle_t)cmd->response_queue);
    return;
  }

  // KONTROLA ZRUŠENÍ TAHU (drop na stejné pole)
  if (to_row == lifted_piece_row && to_col == lifted_piece_col) {
    ESP_LOGI(TAG, "🔄 Move cancelled: piece placed on source square %c%d",
             'a' + to_col, to_row + 1);

    // Reset lifted piece state
    piece_lifted = false;
    lifted_piece_row = 0;
    lifted_piece_col = 0;
    lifted_piece = PIECE_EMPTY;

    // Zobrazit pohyblivé figurky (blikání už přerušeno na začátku DROP)
    led_clear_board_only();
    game_highlight_movable_pieces();

    game_send_response_to_uart("✅ Move cancelled", false,
                               (QueueHandle_t)cmd->response_queue);
    return;
  }

  // VALIDACE TAHU
  chess_move_t move = {.from_row = lifted_piece_row,
                       .from_col = lifted_piece_col,
                       .to_row = to_row,
                       .to_col = to_col,
                       .piece = lifted_piece,
                       .captured_piece = board[to_row][to_col],
                       .timestamp = esp_timer_get_time() / 1000};

  // Detekce castling PŘED validací (pro resignation timer handling)
  bool is_castling =
      (lifted_piece == PIECE_WHITE_KING || lifted_piece == PIECE_BLACK_KING) &&
      abs((int)to_col - (int)lifted_piece_col) == 2;

  // Pokud je castling tah a resignation timer je aktivní, zrušit resignation
  if (is_castling && resignation_state.active) {
    ESP_LOGI(TAG, "🏰 Castling detected during resignation timer - cancelling "
                  "resignation");
    resignation_stop(false);
    ESP_LOGI(TAG, "🏰 Resignation cancelled - continuing with castling flow");
  }

  move_error_t error = game_is_valid_move(&move);
  if (error == MOVE_ERROR_NONE) {
    // VALIDNÍ TAH - normální processing
    ESP_LOGI(TAG, "✅ Valid move detected");

    if (puzzle_active) {
      bool is_expected_solution = (lifted_piece_row == puzzle_solution_from_row &&
                                   lifted_piece_col == puzzle_solution_from_col &&
                                   to_row == puzzle_solution_to_row &&
                                   to_col == puzzle_solution_to_col);
      if (!is_expected_solution) {
        bool already_in_error = error_recovery_state.waiting_for_move_correction;
        puzzle_feedback = PUZZLE_FEEDBACK_WRONG;
        STAGING_LOGI(TAG, "puzzle: wrong move %c%d -> %c%d for id=%u",
                     'a' + lifted_piece_col, lifted_piece_row + 1, 'a' + to_col,
                     to_row + 1, (unsigned)puzzle_active_id);

        error_recovery_state.waiting_for_move_correction = true;
        error_recovery_state.has_invalid_piece = true;
        error_recovery_state.piece_type = lifted_piece;
        error_recovery_state.invalid_row = to_row;
        error_recovery_state.invalid_col = to_col;

        if (!already_in_error) {
          error_recovery_state.original_valid_row = lifted_piece_row;
          error_recovery_state.original_valid_col = lifted_piece_col;
        }

        board[lifted_piece_row][lifted_piece_col] = PIECE_EMPTY;
        board[to_row][to_col] = lifted_piece;
        lifted_piece_row = to_row;
        lifted_piece_col = to_col;

        game_show_invalid_move_error_with_blink(to_row, to_col);

        char error_msg[256];
        snprintf(
            error_msg, sizeof(error_msg),
            "❌ Puzzle: to neni ono (%s). Vrat figurku na ORIGINAL %c%d.",
            cmd->to_notation, 'a' + error_recovery_state.original_valid_col,
            error_recovery_state.original_valid_row + 1);
        game_send_response_to_uart(error_msg, true,
                                   (QueueHandle_t)cmd->response_queue);
        return;
      }
    }

    // KRITICKÁ OPRAVA: Pokud je resignation timer aktivní a král udělá validní
    // tah (ne castling, ne položití zpět), zrušit resignation timer Castling už
    // je ošetřen výše, takže tady kontrolujeme normální tahy krále
    if (resignation_state.active &&
        (lifted_piece == PIECE_WHITE_KING ||
         lifted_piece == PIECE_BLACK_KING) &&
        !is_castling) {
      // Kontrola, zda se král nepokládá zpět na původní pozici (to je handled
      // jinde) Pokud je to jiná pozice než původní, je to normální tah → zrušit
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

    // KRITICKÁ OPRAVA: Zkontrolovat castling_state.in_progress PŘED voláním
    // game_execute_move() protože game_execute_move_enhanced() resetuje
    // castling_state.in_progress = false Pokud je castling_state.in_progress,
    // player change animace už se spustí v game_execute_move_enhanced()
    bool is_castling_completion_before_move = castling_state.in_progress;

    ESP_LOGI(TAG,
             "🔍 game_process_drop_command: BEFORE game_execute_move, "
             "current_player=%s, is_castling_completion_before_move=%d",
             current_player == PLAYER_WHITE ? "White" : "Black",
             is_castling_completion_before_move);
    bool move_success = game_execute_move(&move);
    ESP_LOGI(TAG,
             "🔍 game_process_drop_command: AFTER game_execute_move, "
             "move_success=%d",
             move_success);

    if (move_success) {
      if (puzzle_active) {
        puzzle_active = false;
        puzzle_feedback = PUZZLE_FEEDBACK_SOLVED;
        STAGING_LOGI(TAG, "puzzle: solved id=%u", (unsigned)puzzle_active_id);
      }
      // KRITICKÁ OPRAVA: Reset error state při jakémkoliv validním tahu!
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
        ESP_LOGI(TAG, "⏸️  Awaiting promotion selection via buttons/UART/web");
        ESP_LOGI(TAG, "⏸️  Timer remains with %s player",
                 current_player == PLAYER_WHITE ? "White" : "Black");

        // Keep promo square visually anchored (does not clear board)
        game_update_promotion_anchor_led();

        // Send success response without animations
        char success_msg[256];
        snprintf(
            success_msg, sizeof(success_msg),
            "✅ Valid move: %c%d → %c%d\n"
            "👑 Promotion required at %c%d\n"
            "🔁 Optional: swap on %c%d now\n"
            "➡️ Then choose piece (green buttons / PROMOTE / web)",
            'a' + move.from_col, move.from_row + 1, 'a' + to_col, to_row + 1,
            'a' + promotion_state.square_col, promotion_state.square_row + 1,
            'a' + promotion_state.square_col, promotion_state.square_row + 1);
        game_send_response_to_uart(success_msg, false,
                                   (QueueHandle_t)cmd->response_queue);
        return; // Exit here - no timer switch, no animations
      }

      // Použít is_castling_completion_before_move (uložené PŘED
      // game_execute_move()) protože game_execute_move_enhanced() resetuje
      // castling_state.in_progress = false Pokud je
      // is_castling_completion_before_move, player change animace už se
      // spustila v game_execute_move_enhanced()

      if (!is_castling && !is_castling_completion_before_move) {
        // Spustit move path animaci PŘED změnou hráče (podle starého
        // projektu)
        uint8_t from_led = chess_pos_to_led_index(move.from_row, move.from_col);
        uint8_t to_led = chess_pos_to_led_index(move.to_row, move.to_col);
        led_command_t move_path_cmd = {
            .type = LED_CMD_ANIM_MOVE_PATH,
            .led_index = from_led,
            .red = 255,
            .green = 255,
            .blue = 0, // Yellow
            .duration_ms = 1000,
            .data = &to_led // Cílová pozice v data
        };
        led_execute_command_new(&move_path_cmd);

        // STABILITY FIX: Animace běží asynchronně, neblokujeme zpracování
        // Animace jsou spuštěny v led_task a běží nezávisle
        // (vTaskDelay odstraněn pro lepší throughput)

        // HRÁČ SE UŽ ZMĚNIL V game_execute_move() - použít aktuálního hráče
        // Timer integration: End timer for previous player and start for new
        // player
        player_t previous_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
        ESP_LOGI(TAG, "🔄 Timer switch (drop): ending for %s, starting for %s",
                 previous_player == PLAYER_WHITE ? "White" : "Black",
                 current_player == PLAYER_WHITE ? "White" : "Black");
        game_end_timer_move();
        game_start_timer_move(current_player == PLAYER_WHITE);

        // KRITICKÉ: Endgame kontrola PŘED player change animací!
        // Pokud je endgame, player change se NESPOUŠTÍ
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;

          ESP_LOGI(TAG, "✅ Game finished detected - animation triggered by "
                        "check function");
          ESP_LOGI(
              TAG,
              "✅ Endgame animation started - player change animation SKIPPED");
        } else if (!is_castling_completion_before_move) {
          // Nespouštět player change animaci pokud je to castling dokončení
          // (player change animace už se spustila v
          // game_execute_move_enhanced()) Není endgame a není castling
          // dokončení - spustit player change animaci s NOVÝM hráčem
          // (current_player)
          uint8_t player_color =
              (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
          led_command_t player_change_cmd = {
              .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                                  // ANIM_PLAYER_CHANGE místo
                                                  // PLAYER_CHANGE
              .led_index = 0,
              .red = 0,
              .green = 0,
              .blue = 0,
              .duration_ms = 0,
              .data = &player_color // Předat barvu NOVÉHO hráče
          };
          led_execute_command_new(&player_change_cmd);
        } else {
          // Castling dokončení - player change animace už se spustila v
          // game_execute_move_enhanced()
          ESP_LOGI(TAG, "🏰 Castling completion - player change animation "
                        "already triggered in game_execute_move_enhanced()");
        }
      } // Konec bloku if (!is_castling && !is_castling_completion_before_move)

      // Check animace se spustí vždy (i při castling dokončení)
      // Zkontrolovat, zda je nový hráč v šachu (pouze pokud move_success)
      bool in_check = game_is_king_in_check(current_player);
      if (in_check) {
        // Najít pozici krále
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
            // Spustit check animaci - růžové svícení na králi
            led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                       .led_index = king_led_index,
                                       .red = 0,
                                       .green = 0,
                                       .blue = 0,
                                       .duration_ms =
                                           0, // Trvalé až do dalšího tahu
                                       .data = NULL};
            led_execute_command_new(&check_cmd);
            ESP_LOGI(TAG, "⚠️ CHECK! %s is in check",
                     current_player == PLAYER_WHITE ? "White" : "Black");
          }
        }
      }

      // Reset lifted piece state po úspěšném tahu
      piece_lifted = false;
      lifted_piece_row = 0;
      lifted_piece_col = 0;
      lifted_piece = PIECE_EMPTY;

      // MATRIX COMPATIBILITY: Show opponent pieces after successful move
      // (same as matrix flow) - ONLY if game is active
      if (current_game_state == GAME_STATE_ACTIVE &&
          !game_is_castle_animation_active() && !game_is_castling_expected()) {
        game_highlight_movable_pieces();
      }

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
    // NEVALIDNÍ TAH - ENHANCED ERROR HANDLING
    ESP_LOGW(TAG, "❌ Invalid move attempt: %s", cmd->to_notation);
    if (puzzle_active) {
      puzzle_feedback = PUZZLE_FEEDBACK_ILLEGAL;
    }

    // KRITICKÝ: Rozlišit první error vs další errory
    bool already_in_error = error_recovery_state.waiting_for_move_correction;

    // SPOLEČNÉ NASTAVENÍ pro všechny chyby
    error_recovery_state.waiting_for_move_correction = true;
    error_recovery_state.has_invalid_piece = true;
    error_recovery_state.piece_type = lifted_piece;
    error_recovery_state.invalid_row =
        to_row; // Aktualizovat na novou neplatnou pozici
    error_recovery_state.invalid_col = to_col;

    // KRITICKÝ: Nastavit original_valid POUZE při PRVNÍM erroru!
    // Při dalších errorech NEPŘEPISOVAT - original zůstává stejné (A2)
    if (!already_in_error) {
      error_recovery_state.original_valid_row =
          lifted_piece_row; // A2 - první validní pozice
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

    // AKTUALIZOVAT board stav - figurka je nyní na neplatném poli (fyzická
    // realita) Board musí odpovídat fyzickému stavu, aby PICKUP fungoval
    // správně!
    board[lifted_piece_row][lifted_piece_col] =
        PIECE_EMPTY;                      // původní pole prázdné
    board[to_row][to_col] = lifted_piece; // neplatné pole má figurku

    // AKTUALIZOVAT lifted_piece tracking na novou neplatnou pozici
    lifted_piece_row = to_row;
    lifted_piece_col = to_col;
    // piece_lifted = true; // Zůstává true!

    // LED INDIKACE - červené blikání na neplatném poli
    game_show_invalid_move_error_with_blink(to_row, to_col);

    char error_msg[256];
    snprintf(
        error_msg, sizeof(error_msg),
        "❌ Invalid move to %s - return to ORIGINAL %c%d or find valid move",
        cmd->to_notation, 'a' + error_recovery_state.original_valid_col,
        error_recovery_state.original_valid_row + 1);
    game_send_response_to_uart(error_msg, true,
                               (QueueHandle_t)cmd->response_queue);
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
      if (is_kingside)
        white_rook_h_moved = true;
      else
        white_rook_a_moved = true;
    } else {
      black_king_moved = true;
      if (is_kingside)
        black_rook_h_moved = true;
      else
        black_rook_a_moved = true;
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

  // KRITICKÁ OPRAVA: Bezpečnostní kontrola všech notation stringů (arrays
  // jsou vždy != NULL)
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
        "⚠️ Promotion pending! Use PROMOTE command or physical buttons.", true,
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

        // Odstraněno modré bliknutí - animace se řeší v
        // game_process_drop_command LED se zhasínají v
        // game_process_drop_command po úspěšném tahu

        // Spustit move path animaci PŘED změnou hráče (podle starého
        // projektu)
        uint8_t from_led = chess_pos_to_led_index(move.from_row, move.from_col);
        uint8_t to_led = chess_pos_to_led_index(move.to_row, move.to_col);
        led_command_t move_path_cmd = {
            .type = LED_CMD_ANIM_MOVE_PATH,
            .led_index = from_led,
            .red = 255,
            .green = 255,
            .blue = 0, // Yellow
            .duration_ms = 1000,
            .data = &to_led // Cílová pozice v data
        };
        led_execute_command_new(&move_path_cmd);

        // STABILITY FIX: Animace běží asynchronně, neblokujeme zpracování
        // Animace jsou spuštěny v led_task a běží nezávisle
        // (vTaskDelay odstraněn pro lepší throughput)

        // HRÁČ SE UŽ ZMĚNIL V game_execute_move() - použít aktuálního
        // hráče
        player_t previous_player =
            (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

        // KRITICKÉ: Endgame kontrola PŘED player change animací!
        // Pokud je endgame, player change se NESPOUŠTÍ
        game_state_t end_game_result = game_check_end_game_conditions();
        if (end_game_result == GAME_STATE_FINISHED) {
          current_game_state = GAME_STATE_FINISHED;
          game_active = false;

          // Najít pozici krále vítěze pro endgame animaci
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

          // Spustit endgame animaci (wave z krále vítěze)
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
          // Není endgame - spustit player change animaci
          uint8_t player_color =
              (current_player == PLAYER_WHITE) ? 1 : 0; // 1=white, 0=black
          led_command_t player_change_cmd = {
              .type = LED_CMD_ANIM_PLAYER_CHANGE, // Použít
                                                  // ANIM_PLAYER_CHANGE místo
                                                  // PLAYER_CHANGE
              .led_index = 0,
              .red = 0,
              .green = 0,
              .blue = 0,
              .duration_ms = 0,
              .data = &player_color // Předat barvu hráče
          };
          led_execute_command_new(&player_change_cmd);

          // Zkontrolovat, zda je nový hráč v šachu
          bool in_check = game_is_king_in_check(current_player);
          if (in_check) {
            // Najít pozici krále
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
                // Spustit check animaci - růžové svícení na králi
                led_command_t check_cmd = {.type = LED_CMD_ANIM_CHECK,
                                           .led_index = king_led_index,
                                           .red = 0,
                                           .green = 0,
                                           .blue = 0,
                                           .duration_ms =
                                               0, // Trvalé až do dalšího tahu
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

        // Zobrazit pohyblivé figurky pro nového hráče
        game_highlight_movable_pieces();
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
