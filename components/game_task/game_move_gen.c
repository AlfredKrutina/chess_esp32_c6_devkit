/**
 * @file game_move_gen.c
 * @brief Legal move generation, attack detection, move simulation.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_board_core.h"
#include "game_move_validate.h"

#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char *TAG = "GAME_MOVE_GEN";

const int8_t knight_moves[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                   {1, -2},  {1, 2},  {2, -1},  {2, 1}};

static const int8_t king_moves[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                        {0, 1},   {1, -1}, {1, 0},  {1, 1}};

static const int8_t bishop_dirs[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

static const int8_t rook_dirs[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

chess_move_extended_t legal_moves_buffer[128];
uint32_t legal_moves_count = 0;

// ============================================================================
// BASIC UTILITY FUNCTIONS
// ============================================================================

/**
 * @brief Check if coordinates are valid
 */
bool game_is_valid_square(int row, int col) {
  return (row >= 0 && row < 8 && col >= 0 && col < 8);
}

/**
 * @brief Check if piece belongs to current player
 */
bool game_is_own_piece(piece_t piece, player_t player) {
  if (piece == PIECE_EMPTY)
    return false;
  return (player == PLAYER_WHITE) ? game_is_white_piece(piece)
                                  : game_is_black_piece(piece);
}

// ============================================================================
// ATTACK AND CHECK DETECTION
// ============================================================================

/**
 * @brief Check if square is attacked by opponent
 */
bool game_is_square_attacked(uint8_t row, uint8_t col, player_t by_player) {
  // Check pawn attacks
  int pawn_dir = (by_player == PLAYER_WHITE) ? 1 : -1;
  int pawn_row = row - pawn_dir;
  piece_t attacking_pawn =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_PAWN : PIECE_BLACK_PAWN;

  if (game_is_valid_square(pawn_row, col - 1) &&
      board[pawn_row][col - 1] == attacking_pawn)
    return true;
  if (game_is_valid_square(pawn_row, col + 1) &&
      board[pawn_row][col + 1] == attacking_pawn)
    return true;

  // Check knight attacks
  piece_t attacking_knight =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_KNIGHT : PIECE_BLACK_KNIGHT;
  for (int i = 0; i < 8; i++) {
    int nr = row + knight_moves[i][0];
    int nc = col + knight_moves[i][1];
    if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_knight) {
      return true;
    }
  }

  // Check bishop/queen diagonal attacks
  piece_t attacking_bishop =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_BISHOP : PIECE_BLACK_BISHOP;
  piece_t attacking_queen =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_QUEEN : PIECE_BLACK_QUEEN;

  for (int d = 0; d < 4; d++) {
    int dr = bishop_dirs[d][0];
    int dc = bishop_dirs[d][1];

    for (int nr = row + dr, nc = col + dc; game_is_valid_square(nr, nc);
         nr += dr, nc += dc) {
      piece_t piece = board[nr][nc];
      if (piece == PIECE_EMPTY)
        continue;

      if (piece == attacking_bishop || piece == attacking_queen) {
        return true;
      }
      break; // Blocked by any piece
    }
  }

  // Check rook/queen orthogonal attacks
  piece_t attacking_rook =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;

  for (int d = 0; d < 4; d++) {
    int dr = rook_dirs[d][0];
    int dc = rook_dirs[d][1];

    for (int nr = row + dr, nc = col + dc; game_is_valid_square(nr, nc);
         nr += dr, nc += dc) {
      piece_t piece = board[nr][nc];
      if (piece == PIECE_EMPTY)
        continue;

      if (piece == attacking_rook || piece == attacking_queen) {
        return true;
      }
      break; // Blocked by any piece
    }
  }

  // Check king attacks
  piece_t attacking_king =
      (by_player == PLAYER_WHITE) ? PIECE_WHITE_KING : PIECE_BLACK_KING;
  for (int i = 0; i < 8; i++) {
    int nr = row + king_moves[i][0];
    int nc = col + king_moves[i][1];
    if (game_is_valid_square(nr, nc) && board[nr][nc] == attacking_king) {
      return true;
    }
  }

  return false;
}

// ============================================================================
// MOVE VALIDATION AND SIMULATION
// ============================================================================

/**
 * @brief Simulate move and check if it leaves king in check
 */
bool game_simulate_move_check(chess_move_extended_t *move, player_t player) {
  // Save original state
  piece_t original_piece = board[move->to_row][move->to_col];
  piece_t original_en_passant = PIECE_EMPTY;

  // Handle en passant capture
  if (move->move_type == MOVE_TYPE_EN_PASSANT) {
    original_en_passant = board[en_passant_victim_row][en_passant_victim_col];
    board[en_passant_victim_row][en_passant_victim_col] = PIECE_EMPTY;
  }

  // Make the move
  board[move->to_row][move->to_col] = move->piece;
  board[move->from_row][move->from_col] = PIECE_EMPTY;

  // Check if king is in check after this move
  bool king_in_check = game_is_king_in_check(player);

  // Restore original state
  board[move->from_row][move->from_col] = move->piece;
  board[move->to_row][move->to_col] = original_piece;

  if (move->move_type == MOVE_TYPE_EN_PASSANT) {
    board[en_passant_victim_row][en_passant_victim_col] = original_en_passant;
  }

  return !king_in_check; // Return true if move is legal (doesn't leave king
                         // in check)
}

// ============================================================================
// MOVE GENERATION - INDIVIDUAL PIECES
// ============================================================================

/**
 * @brief Generate pawn moves
 */
void game_generate_pawn_moves(uint8_t from_row, uint8_t from_col,
                              player_t player) {
  piece_t pawn = board[from_row][from_col];
  bool is_white = (player == PLAYER_WHITE);
  int direction = is_white ? 1 : -1;
  int start_row = is_white ? 1 : 6;
  int promotion_row = is_white ? 7 : 0;

  // Forward move
  int to_row = from_row + direction;
  if (game_is_valid_square(to_row, from_col) &&
      board[to_row][from_col] == PIECE_EMPTY) {

    if (to_row == promotion_row) {
      // Promotion moves
      for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT; promo++) {
        if (legal_moves_count >= 128)
          return;

        chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row;
        move->to_col = from_col;
        move->piece = pawn;
        move->captured_piece = PIECE_EMPTY;
        move->move_type = MOVE_TYPE_PROMOTION;
        move->promotion_piece = (promotion_choice_t)promo;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    } else {
      // Normal forward move
      if (legal_moves_count >= 128)
        return;

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row;
      move->from_col = from_col;
      move->to_row = to_row;
      move->to_col = from_col;
      move->piece = pawn;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_NORMAL;

      if (game_simulate_move_check(move, player)) {
        legal_moves_count++;
      }

      // Double move from starting position
      if (from_row == start_row &&
          board[to_row + direction][from_col] == PIECE_EMPTY) {
        if (legal_moves_count >= 128)
          return;

        move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = to_row + direction;
        move->to_col = from_col;
        move->piece = pawn;
        move->captured_piece = PIECE_EMPTY;
        move->move_type = MOVE_TYPE_NORMAL;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    }
  }

  // Captures (diagonal)
  for (int dc = -1; dc <= 1; dc += 2) {
    int to_col = from_col + dc;
    if (game_is_valid_square(to_row, to_col)) {
      piece_t target = board[to_row][to_col];

      if (game_is_enemy_piece(target, player)) {
        if (to_row == promotion_row) {
          // Capture with promotion
          for (int promo = PROMOTION_QUEEN; promo <= PROMOTION_KNIGHT;
               promo++) {
            if (legal_moves_count >= 128)
              return;

            chess_move_extended_t *move =
                &legal_moves_buffer[legal_moves_count];
            move->from_row = from_row;
            move->from_col = from_col;
            move->to_row = to_row;
            move->to_col = to_col;
            move->piece = pawn;
            move->captured_piece = target;
            move->move_type = MOVE_TYPE_PROMOTION;
            move->promotion_piece = (promotion_choice_t)promo;

            if (game_simulate_move_check(move, player)) {
              legal_moves_count++;
            }
          }
        } else {
          // Normal capture
          if (legal_moves_count >= 128)
            return;

          chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
          move->from_row = from_row;
          move->from_col = from_col;
          move->to_row = to_row;
          move->to_col = to_col;
          move->piece = pawn;
          move->captured_piece = target;
          move->move_type = MOVE_TYPE_CAPTURE;

          if (game_simulate_move_check(move, player)) {
            legal_moves_count++;
          }
        }
      }
    }
  }

  // En passant
  if (en_passant_available && from_row == (is_white ? 4 : 3)) {
    for (int dc = -1; dc <= 1; dc += 2) {
      if (from_col + dc == en_passant_target_col) {
        if (legal_moves_count >= 128)
          return;

        chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
        move->from_row = from_row;
        move->from_col = from_col;
        move->to_row = en_passant_target_row;
        move->to_col = en_passant_target_col;
        move->piece = pawn;
        move->captured_piece =
            board[en_passant_victim_row][en_passant_victim_col];
        move->move_type = MOVE_TYPE_EN_PASSANT;

        if (game_simulate_move_check(move, player)) {
          legal_moves_count++;
        }
      }
    }
  }
}

/**
 * @brief Generate knight moves
 */
void game_generate_knight_moves(uint8_t from_row, uint8_t from_col,
                                player_t player) {
  piece_t knight = board[from_row][from_col];

  for (int i = 0; i < 8; i++) {
    int to_row = from_row + knight_moves[i][0];
    int to_col = from_col + knight_moves[i][1];

    if (!game_is_valid_square(to_row, to_col))
      continue;

    piece_t target = board[to_row][to_col];

    // Skip if occupied by own piece
    if (game_is_own_piece(target, player))
      continue;

    if (legal_moves_count >= 128)
      return;

    chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
    move->from_row = from_row;
    move->from_col = from_col;
    move->to_row = to_row;
    move->to_col = to_col;
    move->piece = knight;
    move->captured_piece = target;
    move->move_type =
        (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

    if (game_simulate_move_check(move, player)) {
      legal_moves_count++;
    }
  }
}

/**
 * @brief Generate sliding piece moves (bishop, rook, queen)
 */
void game_generate_sliding_moves(uint8_t from_row, uint8_t from_col,
                                 player_t player, const int8_t directions[][2],
                                 int num_directions) {
  piece_t piece = board[from_row][from_col];

  for (int d = 0; d < num_directions; d++) {
    int dr = directions[d][0];
    int dc = directions[d][1];

    for (int to_row = from_row + dr, to_col = from_col + dc;
         game_is_valid_square(to_row, to_col); to_row += dr, to_col += dc) {

      piece_t target = board[to_row][to_col];

      // Can't move to square occupied by own piece
      if (game_is_own_piece(target, player))
        break;

      if (legal_moves_count >= 128)
        return;

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row;
      move->from_col = from_col;
      move->to_row = to_row;
      move->to_col = to_col;
      move->piece = piece;
      move->captured_piece = target;
      move->move_type =
          (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

      if (game_simulate_move_check(move, player)) {
        legal_moves_count++;
      }

      // Stop if we hit any piece
      if (target != PIECE_EMPTY)
        break;
    }
  }
}

/**
 * @brief Generate king moves including castling
 */
void game_generate_king_moves(uint8_t from_row, uint8_t from_col,
                              player_t player) {
  piece_t king = board[from_row][from_col];

  // Normal king moves
  for (int i = 0; i < 8; i++) {
    int to_row = from_row + king_moves[i][0];
    int to_col = from_col + king_moves[i][1];

    if (!game_is_valid_square(to_row, to_col))
      continue;

    piece_t target = board[to_row][to_col];

    // Skip if occupied by own piece
    if (game_is_own_piece(target, player))
      continue;

    if (legal_moves_count >= 128)
      return;

    chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
    move->from_row = from_row;
    move->from_col = from_col;
    move->to_row = to_row;
    move->to_col = to_col;
    move->piece = king;
    move->captured_piece = target;
    move->move_type =
        (target == PIECE_EMPTY) ? MOVE_TYPE_NORMAL : MOVE_TYPE_CAPTURE;

    if (game_simulate_move_check(move, player)) {
      legal_moves_count++;
    }
  }

  // Castling - FIXED: Use proper bounds checking and correct coordinates
  if (game_is_king_in_check(player))
    return; // Can't castle when in check

  if (player == PLAYER_WHITE && !white_king_moved) {
    // White kingside castling
    if (!white_rook_h_moved && board[0][5] == PIECE_EMPTY &&
        board[0][6] == PIECE_EMPTY &&
        !game_is_square_attacked(0, 5, PLAYER_BLACK) &&
        !game_is_square_attacked(0, 6, PLAYER_BLACK)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 0;
      move->to_col = 6;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_KING;
      legal_moves_count++;
    }

    // White queenside castling
    if (!white_rook_a_moved && board[0][1] == PIECE_EMPTY &&
        board[0][2] == PIECE_EMPTY && board[0][3] == PIECE_EMPTY &&
        !game_is_square_attacked(0, 2, PLAYER_BLACK) &&
        !game_is_square_attacked(0, 3, PLAYER_BLACK)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 0;
      move->to_col = 2;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_QUEEN;
      legal_moves_count++;
    }
  }

  if (player == PLAYER_BLACK && !black_king_moved) {
    // Black kingside castling
    if (!black_rook_h_moved && board[7][5] == PIECE_EMPTY &&
        board[7][6] == PIECE_EMPTY &&
        !game_is_square_attacked(7, 5, PLAYER_WHITE) &&
        !game_is_square_attacked(7, 6, PLAYER_WHITE)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 7;
      move->to_col = 6;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_KING;
      legal_moves_count++;
    }

    // Black queenside castling
    if (!black_rook_a_moved && board[7][1] == PIECE_EMPTY &&
        board[7][2] == PIECE_EMPTY && board[7][3] == PIECE_EMPTY &&
        !game_is_square_attacked(7, 2, PLAYER_WHITE) &&
        !game_is_square_attacked(7, 3, PLAYER_WHITE)) {

      if (legal_moves_count >= 128)
        return; // ED: Proper bounds checking

      chess_move_extended_t *move = &legal_moves_buffer[legal_moves_count];
      move->from_row = from_row; // ED: Use actual king position
      move->from_col = from_col;
      move->to_row = 7;
      move->to_col = 2;
      move->piece = king;
      move->captured_piece = PIECE_EMPTY;
      move->move_type = MOVE_TYPE_CASTLE_QUEEN;
      legal_moves_count++;
    }
  }
}

// ============================================================================
// MAIN MOVE GENERATION
// ============================================================================

/**
 * @brief Generate all legal moves for current player
 */
uint32_t game_generate_legal_moves(player_t player) {
  legal_moves_count = 0;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];

      if (!game_is_own_piece(piece, player))
        continue;

      switch (piece) {
      case PIECE_WHITE_PAWN:
      case PIECE_BLACK_PAWN:
        game_generate_pawn_moves(row, col, player);
        break;

      case PIECE_WHITE_KNIGHT:
      case PIECE_BLACK_KNIGHT:
        game_generate_knight_moves(row, col, player);
        break;

      case PIECE_WHITE_BISHOP:
      case PIECE_BLACK_BISHOP:
        game_generate_sliding_moves(row, col, player, bishop_dirs, 4);
        break;

      case PIECE_WHITE_ROOK:
      case PIECE_BLACK_ROOK:
        game_generate_sliding_moves(row, col, player, rook_dirs, 4);
        break;

      case PIECE_WHITE_QUEEN:
      case PIECE_BLACK_QUEEN:
        game_generate_sliding_moves(row, col, player, bishop_dirs, 4);
        game_generate_sliding_moves(row, col, player, rook_dirs, 4);
        break;

      case PIECE_WHITE_KING:
      case PIECE_BLACK_KING:
        game_generate_king_moves(row, col, player);
        break;

      default:
        break;
      }
    }
  }

  return legal_moves_count;
}
// ============================================================================
// INTEGRATION WITH EXISTING GAME TASK
// ============================================================================

/**
 * @brief Enhanced move validation that replaces game_is_valid_move
 */
move_error_t game_validate_move_enhanced(uint8_t from_row, uint8_t from_col,
                                         uint8_t to_row, uint8_t to_col) {
  // Basic coordinate validation
  if (!game_is_valid_square(from_row, from_col) ||
      !game_is_valid_square(to_row, to_col)) {
    return MOVE_ERROR_INVALID_COORDINATES;
  }

  // Check if there's a piece at source
  piece_t piece = board[from_row][from_col];
  if (piece == PIECE_EMPTY) {
    return MOVE_ERROR_NO_PIECE;
  }

  // Check if it's player's piece
  if (!game_is_own_piece(piece, current_player)) {
    return MOVE_ERROR_WRONG_COLOR;
  }

  // Generate all legal moves and check if this move is in the list
  uint32_t num_legal = game_generate_legal_moves(current_player);

  for (uint32_t i = 0; i < num_legal; i++) {
    chess_move_extended_t *move = &legal_moves_buffer[i];
    if (move->from_row == from_row && move->from_col == from_col &&
        move->to_row == to_row && move->to_col == to_col) {
      return MOVE_ERROR_NONE; // Move is legal
    }
  }

  return MOVE_ERROR_ILLEGAL_MOVE;
}
