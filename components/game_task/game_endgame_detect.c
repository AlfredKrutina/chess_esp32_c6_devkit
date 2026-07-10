/**
 * @file game_endgame_detect.c
 * @brief Check, stalemate, draw detection, and end-game condition analysis.
 */

#include "game_task_internal.h"
#include "game_task.h"

#include "../../timer_system/include/timer_system.h"

#include "esp_log.h"

#include <inttypes.h>

static const char *TAG = "GAME_ENDGAME";

// ============================================================================
// END-GAME DETECTION
// ============================================================================

/**
 * @brief Check if king is in check
 * @param player Player to check
 * @return true if king is in check
 */
bool game_is_king_in_check(player_t player) {
  // Find king position
  int king_row = -1, king_col = -1;
  piece_t king_piece =
      (player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;

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

  if (king_row == -1)
    return false; // King not found

  // Check if any opponent piece can capture the king
  piece_t opponent_pieces_start =
      (player == PLAYER_WHITE) ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
  piece_t opponent_pieces_end =
      (player == PLAYER_WHITE) ? PIECE_BLACK_KING : PIECE_WHITE_KING;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      if (piece >= opponent_pieces_start && piece <= opponent_pieces_end) {
        // Create temporary move to check if it can capture king
        chess_move_t temp_move = {.from_row = row,
                                  .from_col = col,
                                  .to_row = king_row,
                                  .to_col = king_col,
                                  .piece = piece,
                                  .captured_piece = king_piece,
                                  .timestamp = 0};

        // Check if this move is valid (would capture king)
        if (game_validate_piece_move_enhanced(&temp_move, piece) ==
            MOVE_ERROR_NONE) {
          return true; // King is in check
        }
      }
    }
  }

  return false;
}

/**
 * @brief Check if player has any legal moves (optimized version)
 * @param player Player to check
 * @return true if player has legal moves
 */
bool game_has_legal_moves(player_t player) {
  // Use existing move generation system instead of brute force
  uint32_t moves_count = game_generate_legal_moves(player);

  return (moves_count > 0);
}

/**
 * @brief Check if current position has insufficient material for checkmate
 * @return true if insufficient material (draw)
 */
/**
 * @brief Check if piece belongs to opponent
 */
bool game_is_enemy_piece(piece_t piece, player_t player) {
  if (piece == PIECE_EMPTY)
    return false;
  return (player == PLAYER_WHITE) ? game_is_black_piece(piece)
                                  : game_is_white_piece(piece);
}

bool game_is_insufficient_material(void) {
  int white_pieces = 0, black_pieces = 0;
  int white_minors = 0, black_minors = 0; // Knights and bishops
  bool white_has_bishop = false, black_has_bishop = false;
  bool white_has_knight = false, black_has_knight = false;
  bool white_bishop_color = false,
       black_bishop_color = false; // false = light, true = dark

  // Count pieces and analyze material
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      switch (piece) {
      case PIECE_WHITE_PAWN:
      case PIECE_WHITE_ROOK:
      case PIECE_WHITE_QUEEN:
        white_pieces++;
        break;
      case PIECE_WHITE_BISHOP:
        white_pieces++;
        white_minors++;
        white_has_bishop = true;
        white_bishop_color = ((row + col) % 2 == 0); // Light squares = false
        break;
      case PIECE_WHITE_KNIGHT:
        white_pieces++;
        white_minors++;
        white_has_knight = true;
        break;
      case PIECE_BLACK_PAWN:
      case PIECE_BLACK_ROOK:
      case PIECE_BLACK_QUEEN:
        black_pieces++;
        break;
      case PIECE_BLACK_BISHOP:
        black_pieces++;
        black_minors++;
        black_has_bishop = true;
        black_bishop_color = ((row + col) % 2 == 0); // Light squares = false
        break;
      case PIECE_BLACK_KNIGHT:
        black_pieces++;
        black_minors++;
        black_has_knight = true;
        break;
      default:
        break;
      }
    }
  }

  // King vs King - always insufficient
  if (white_pieces == 0 && black_pieces == 0) {
    return true;
  }

  // King + minor vs King - insufficient
  if ((white_pieces == 1 && black_pieces == 0) ||
      (white_pieces == 0 && black_pieces == 1)) {
    return true;
  }

  // King + Bishop vs King + Bishop (same color) - insufficient
  if (white_pieces == 1 && black_pieces == 1 && white_has_bishop &&
      black_has_bishop && white_bishop_color == black_bishop_color) {
    return true;
  }

  // King + Knight vs King + Knight - insufficient
  if (white_pieces == 1 && black_pieces == 1 && white_has_knight &&
      black_has_knight) {
    return true;
  }

  // King + 2 Knights vs King - insufficient (very rare, but technically
  // insufficient)
  if ((white_pieces == 2 && black_pieces == 0 && white_minors == 2 &&
       white_has_knight) ||
      (white_pieces == 0 && black_pieces == 2 && black_minors == 2 &&
       black_has_knight)) {
    return true;
  }

  return false; // Sufficient material for checkmate
}

/**
 * @brief Kontroluje podminky pro konec hry (sachmat, pat, remizy)
 *
 * @return GAME_STATE_FINISHED pokud hra skoncila, GAME_STATE_ACTIVE pokud
 * pokracuje
 *
 * @details
 * Funkce kontroluje vsechny mozne zpusoby konce hry:
 *
 * Vyherne podminky:
 * - Sachmat: kral v sachu && zadne validni tahy
 *
 * Remizove podminky:
 * - Pat (Stalemate): kral NENI v sachu && zadne validni tahy
 * - 50-Move Rule: 100 pultahu bez brani nebo posunu pesce
 * - Threefold Repetition: stejna pozice 3x opakována
 * - Insufficient Material: nedostatecny material pro mat
 *
 * Funkce take:
 * - Aktualizuje statistiky
 * - Zastavuje timer
 * - Generuje endgame report
 * - Rozlisuje typy sachmatu (en passant, castling, promotion, discovered)
 *
 * @note Opraveno v2.4.1:
 * - BUG #13: 50-Move Rule nyni kontroluje >= 100 pultahu (50 tahu na stranu)
 * - Puvodni kontrola >= 50 byla spatna (polovicni pocet)
 */
game_state_t game_check_end_game_conditions(void) {
  // Check if current player is in check
  bool in_check = game_is_king_in_check(current_player);

  // Check if current player has legal moves
  bool has_moves = game_has_legal_moves(current_player);

  if (in_check && !has_moves) {
    // Checkmate - určit typ podle posledního tahu
    game_result = GAME_STATE_FINISHED;
    current_result_type = (current_player == PLAYER_WHITE) ? RESULT_BLACK_WINS
                                                           : RESULT_WHITE_WINS;

    // Rozlišit typ šachmatu podle posledního tahu
    switch (last_move_type) {
    case LAST_MOVE_EN_PASSANT:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_EN_PASSANT;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY EN PASSANT! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_CASTLING:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_CASTLING;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY CASTLING! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_PROMOTION:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_PROMOTION;
      ESP_LOGI(TAG, "🎯 CHECKMATE BY PROMOTION! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    case LAST_MOVE_DISCOVERED:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE_DISCOVERED;
      ESP_LOGI(
          TAG,
          "🎯 CHECKMATE BY DISCOVERED CHECK! %s wins in %" PRIu32 " moves!",
          (current_player == PLAYER_WHITE) ? "Black" : "White", move_count);
      break;
    default:
      current_endgame_reason = ENDGAME_REASON_CHECKMATE;
      ESP_LOGI(TAG, "🎯 CHECKMATE! %s wins in %" PRIu32 " moves!",
               (current_player == PLAYER_WHITE) ? "Black" : "White",
               move_count);
      break;
    }

    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    // UNIFIED VICTORY ANIMATION
    // Checkmate = Victory for current_player (who delivered mate? No wait.)
    // Logic: if current_player is in check & no moves, they LOST.
    // Winner is the OPPONENT.
    player_t winner =
        (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;
    game_trigger_victory_animation(winner);

    return GAME_STATE_FINISHED;
  } else if (!in_check && !has_moves) {
    // Stalemate
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_STALEMATE;
    current_endgame_reason = ENDGAME_REASON_STALEMATE; // Označit jako stalemate
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn in %" PRIu32 " moves", move_count);
    return GAME_STATE_FINISHED;
  }

  // BUG #13: Check for draw conditions
  // 50-move rule: 50 plných tahů = 100 půltahů (half-moves/ply)
  // moves_without_capture počítá půltahy, takže >= 100
  if (moves_without_capture >= 100) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_50_MOVE;
    current_endgame_reason =
        ENDGAME_REASON_50_MOVE; // Označit jako 50-move rule
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! 50 moves without capture (50-move rule)");
    return GAME_STATE_FINISHED;
  }

  if (game_is_position_repeated()) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_REPETITION;
    current_endgame_reason =
        ENDGAME_REASON_REPETITION; // Označit jako threefold repetition
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! Position repeated (draw by repetition)");
    return GAME_STATE_FINISHED;
  }

  // Check for insufficient material
  if (game_is_insufficient_material()) {
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_INSUFFICIENT;
    current_endgame_reason =
        ENDGAME_REASON_INSUFFICIENT; // Označit jako insufficient material
    game_update_endgame_statistics(current_result_type);
    endgame_report_requested = true; // Požadavek na report

    // Zastavit timer (hra skončila)
    timer_pause();

    ESP_LOGI(TAG, "🤝 DRAW! Insufficient material to checkmate");
    return GAME_STATE_FINISHED;
  }

  return GAME_STATE_ACTIVE;
}
