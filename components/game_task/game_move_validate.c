/**
 * @file game_move_validate.c
 * @brief Move validation (piece rules, castling, en passant, check simulation).
 */

#include "game_move_validate.h"
#include "game_task_internal.h"

#include "game_task.h"

#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "GAME_VALIDATE";

// ============================================================================
// MOVE VALIDATION FUNCTIONS
// ============================================================================

move_error_t game_is_valid_move(const chess_move_t *move) {
  if (!move) {
    return MOVE_ERROR_INVALID_MOVE_STRUCTURE;
  }

  if (!game_is_valid_position(move->from_row, move->from_col) ||
      !game_is_valid_position(move->to_row, move->to_col)) {
    return MOVE_ERROR_OUT_OF_BOUNDS;
  }

  if (!game_active) {
    return MOVE_ERROR_GAME_NOT_ACTIVE;
  }

  // Check if source position has a piece
  piece_t source_piece = game_get_piece(move->from_row, move->from_col);

  // If source_piece is empty (the king is raised during the resignation timer),
  // use move->piece instead of source_piece
  if (source_piece == PIECE_EMPTY) {
    // If resignation timer is active and move->piece is king, use
    // move->piece
    if (resignation_state.active && move->piece != PIECE_EMPTY &&
        (move->piece == PIECE_WHITE_KING || move->piece == PIECE_BLACK_KING) &&
        resignation_state.king_row == move->from_row &&
        resignation_state.king_col == move->from_col) {
      source_piece = move->piece;
      ESP_LOGI(TAG,
               "🏰 game_is_valid_move: Using move->piece (%d) because king is "
               "lifted (resignation timer active)",
               move->piece);
    } else {
      return MOVE_ERROR_NO_PIECE;
    }
  }

  // Check if source piece belongs to current player
  if ((current_player == PLAYER_WHITE && !game_is_white_piece(source_piece)) ||
      (current_player == PLAYER_BLACK && !game_is_black_piece(source_piece))) {
    return MOVE_ERROR_WRONG_COLOR;
  }

  // Check if destination is occupied by own piece
  piece_t dest_piece = game_get_piece(move->to_row, move->to_col);
  if (dest_piece != PIECE_EMPTY &&
      game_is_same_color(source_piece, dest_piece)) {
    return MOVE_ERROR_DESTINATION_OCCUPIED;
  }

  // CHECKING THE ROLL IN PROGRESS - BEFORE validating the figure's movement
  // If it's a correct cast rook move, skip the standard move validation
  // pieces (which would otherwise find the path blocked by the king)
  bool is_castling_rook_move = false;
  if (castling_state.in_progress) {
    ESP_LOGI(TAG, "🏰 game_is_valid_move: Castling in progress - checking if "
                  "this is the expected rook move");
    ESP_LOGI(TAG, "🏰 Expected: from %c%d to %c%d, actual: from %c%d to %c%d",
             'a' + castling_state.rook_from_col,
             castling_state.rook_from_row + 1, 'a' + castling_state.rook_to_col,
             castling_state.rook_to_row + 1, 'a' + move->from_col,
             move->from_row + 1, 'a' + move->to_col, move->to_row + 1);

    // We expect a rook move
    if (move->from_row != castling_state.rook_from_row ||
        move->from_col != castling_state.rook_from_col) {
      ESP_LOGE(TAG, "❌ Castling in progress - expected rook move from %c%d",
               'a' + castling_state.rook_from_col,
               castling_state.rook_from_row + 1);
      return MOVE_ERROR_CASTLING_BLOCKED; // We will use the existing error code
    }

    if (move->to_row != castling_state.rook_to_row ||
        move->to_col != castling_state.rook_to_col) {
      ESP_LOGE(
          TAG, "❌ Incorrect rook destination during castling - expected %c%d",
          'a' + castling_state.rook_to_col, castling_state.rook_to_row + 1);
      return MOVE_ERROR_CASTLING_BLOCKED; // We will use the existing error code
    }

    // This is a proper cast rook move - skip the standard move validation
    // figurines
    is_castling_rook_move = true;
    ESP_LOGI(TAG, "✅ Correct rook move for castling completion - skipping "
                  "standard piece validation (path would be blocked by king)");
  }

  // Validate move based on piece type (ONLY if casting is NOT tower move)
  if (!is_castling_rook_move) {
    move_error_t piece_error =
        game_validate_piece_move_enhanced(move, source_piece);
    if (piece_error != MOVE_ERROR_NONE) {
      return piece_error;
    }
  } else {
    ESP_LOGI(TAG,
             "🏰 Skipping standard piece validation for castling rook move");
  }

  // Check if move would leave king in check
  if (game_would_move_leave_king_in_check(move)) {
    return MOVE_ERROR_KING_IN_CHECK;
  }

  return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_is_valid_move_bool(const chess_move_t *move) {
  return (game_is_valid_move(move) == MOVE_ERROR_NONE);
}

move_error_t game_validate_piece_move_enhanced(const chess_move_t *move,
                                               piece_t piece) {
  // int row_diff = move->to_row - move->from_row;
  // int col_diff = move->to_col - move->from_col;
  // int abs_row_diff = abs(row_diff);
  // int abs_col_diff = abs(col_diff);

  switch (piece) {
  case PIECE_WHITE_PAWN:
  case PIECE_BLACK_PAWN:
    return game_validate_pawn_move_enhanced(move, piece);

  case PIECE_WHITE_KNIGHT:
  case PIECE_BLACK_KNIGHT:
    return game_validate_knight_move_enhanced(move);

  case PIECE_WHITE_BISHOP:
  case PIECE_BLACK_BISHOP:
    return game_validate_bishop_move_enhanced(move);

  case PIECE_WHITE_ROOK:
  case PIECE_BLACK_ROOK:
    return game_validate_rook_move_enhanced(move);

  case PIECE_WHITE_QUEEN:
  case PIECE_BLACK_QUEEN:
    return game_validate_queen_move_enhanced(move);

  case PIECE_WHITE_KING:
  case PIECE_BLACK_KING:
    return game_validate_king_move_enhanced(move);

  default:
    return MOVE_ERROR_INVALID_PATTERN;
  }
}

// Keep the old function for backward compatibility
bool game_validate_piece_move(const chess_move_t *move, piece_t piece) {
  return (game_validate_piece_move_enhanced(move, piece) == MOVE_ERROR_NONE);
}

/**
 * @brief Validates a pesce move with extended error detection
 *
 * @param move A pointer to a move structure
 * @param piece Piece type (PIECE_WHITE_PAWN or PIECE_BLACK_PAWN)
 * @return MOVE_ERROR_NONE if the move is valid, otherwise error code
 *
 * @details
 * This function validates all pesce move types:
 * - Move forward 1 square
 * - Move forward 2 squares from starting position
 * - Defense diagonally (1 square diagonally with the enemy)
 * - En passant
 *
 * The function also checks:
 * - Road blocking (bile i cerne pesce)
 * - Reverse moves (invalids)
 * - Diagonal strokes on empty fields (without en passant)
 *
 * @note Fixed in v2.4.1:
 * - BUG #1: Path blocking now works for both white and black fish (abs(row_diff))
 * - BUG #8: Backstrokes are now detected as INVALID_PATTERN
 * - Added diagnostic logs for debugging
 */
move_error_t game_validate_pawn_move_enhanced(const chess_move_t *move,
                                              piece_t piece) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;
  int abs_col_diff = abs(col_diff);

  bool is_white = game_is_white_piece(piece);
  int direction = is_white ? 1 : -1;
  int start_row = is_white ? 1 : 6;

  //  Logovani validace pesce
  char from_sq[3], to_sq[3];
  game_coords_to_square(move->from_row, move->from_col, from_sq);
  game_coords_to_square(move->to_row, move->to_col, to_sq);
  piece_t dest_piece = game_get_piece(move->to_row, move->to_col);

  // Forward move
  if (col_diff == 0) {
    // Check if moving backwards (invalid for pawns)
    if (row_diff * direction < 0) {
      ESP_LOGD(TAG, "❌ Pawn %s→%s: cannot move backwards", from_sq, to_sq);
      return MOVE_ERROR_INVALID_PATTERN;
    }

    // Single square move
    if (row_diff == direction) {
      if (game_is_empty(move->to_row, move->to_col)) {
        return MOVE_ERROR_NONE;
      } else {
        ESP_LOGD(TAG, "🔍 Pawn %s->%s: blocked by piece", from_sq, to_sq);
        return MOVE_ERROR_BLOCKED_PATH;
      }
    }

    // Double square move from starting position
    if (row_diff == 2 * direction && move->from_row == start_row) {
      if (game_is_empty(move->from_row + direction, move->from_col) &&
          game_is_empty(move->to_row, move->to_col)) {
        return MOVE_ERROR_NONE;
      } else {
        ESP_LOGD(TAG, "🔍 Pawn %s->%s: blocked path double move", from_sq,
                 to_sq);
        return MOVE_ERROR_BLOCKED_PATH;
      }
    }

    ESP_LOGD(TAG, "🔍 Pawn %s->%s: invalid forward distance", from_sq, to_sq);
    return MOVE_ERROR_INVALID_PATTERN;
  }

  // Capture move (diagonal)
  if (abs_col_diff == 1 && row_diff == direction) {
    if (dest_piece != PIECE_EMPTY && !game_is_same_color(piece, dest_piece)) {
      ESP_LOGD(TAG, "✅ Pawn %s→%s: valid capture of %s", from_sq, to_sq,
               game_get_piece_name(dest_piece));
      return MOVE_ERROR_NONE;
    }

    // Check for en passant
    if (game_is_en_passant_possible(move)) {
      ESP_LOGD(TAG, "✅ Pawn %s→%s: valid en passant", from_sq, to_sq);
      return MOVE_ERROR_NONE;
    }

    if (dest_piece == PIECE_EMPTY) {
      ESP_LOGD(TAG, "❌ Pawn %s→%s: diagonal to empty square (not en passant)",
               from_sq, to_sq);
    } else {
      ESP_LOGD(TAG, "❌ Pawn %s→%s: trying to capture own piece", from_sq,
               to_sq);
    }
    return MOVE_ERROR_INVALID_PATTERN;
  }

  ESP_LOGD(TAG,
           "❌ Pawn %s→%s: invalid move pattern (col_diff=%d, row_diff=%d)",
           from_sq, to_sq, col_diff, row_diff);
  return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_pawn_move(const chess_move_t *move, piece_t piece) {
  return (game_validate_pawn_move_enhanced(move, piece) == MOVE_ERROR_NONE);
}

move_error_t game_validate_knight_move_enhanced(const chess_move_t *move) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;
  int abs_row_diff = abs(row_diff);
  int abs_col_diff = abs(col_diff);

  // Knight moves in L-shape: 2 squares in one direction, 1 square perpendicular
  if ((abs_row_diff == 2 && abs_col_diff == 1) ||
      (abs_row_diff == 1 && abs_col_diff == 2)) {
    return MOVE_ERROR_NONE;
  }

  return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_knight_move(const chess_move_t *move) {
  return (game_validate_knight_move_enhanced(move) == MOVE_ERROR_NONE);
}

/**
 * @brief Validates shooter move with extended error detection
 *
 * @param move A pointer to a move structure
 * @return MOVE_ERROR_NONE if the move is valid, otherwise error code
 *
 * @details
 * The shooter only moves diagonally. The function checks for:
 * - Valid diagonal movement (abs_row_diff == abs_col_diff)
 * - Blocking the path between the start and finish position
 * - All fields on the path must be empty
 *
 * @note Fixed in v2.4.1:
 * - BUG #2: While loop now uses OR instead of AND to properly check up to
 * goals
 * - Added safety check against infinite loop (max 8 steps)
 * - Added diagnostic logs for blocking
 */
move_error_t game_validate_bishop_move_enhanced(const chess_move_t *move) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;
  int abs_row_diff = abs(row_diff);
  int abs_col_diff = abs(col_diff);

  // Bishop moves diagonally
  if (abs_row_diff != abs_col_diff) {
    return MOVE_ERROR_INVALID_PATTERN;
  }

  // Check if path is blocked
  // Originally: while (...&& ...) - terminated prematurely
  // New: while (...|| ...) - checks until the destination
  int row_step = (row_diff > 0) ? 1 : -1;
  int col_step = (col_diff > 0) ? 1 : -1;

  int current_row = move->from_row + row_step;
  int current_col = move->from_col + col_step;

  // SAFETY: Prevent infinite loop (max 8 squares on diagonal)
  int steps = 0;
  while ((current_row != move->to_row || current_col != move->to_col) &&
         steps < 8) {
    if (!game_is_empty(current_row, current_col)) {
      ESP_LOGD(TAG, "🔍 Bishop blocked at %c%d", 'a' + current_col,
               current_row + 1);
      return MOVE_ERROR_BLOCKED_PATH;
    }
    current_row += row_step;
    current_col += col_step;
    steps++;
  }

  // Safety check: if loop ended without reaching target, something is wrong
  if (steps >= 8 &&
      (current_row != move->to_row || current_col != move->to_col)) {
    ESP_LOGE(TAG, "🔍 Bishop validation error: loop exceeded max steps");
    return MOVE_ERROR_INVALID_PATTERN;
  }

  return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_validate_bishop_move(const chess_move_t *move) {
  return (game_validate_bishop_move_enhanced(move) == MOVE_ERROR_NONE);
}

move_error_t game_validate_rook_move_enhanced(const chess_move_t *move) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;

  // Rook moves horizontally or vertically
  if (row_diff != 0 && col_diff != 0) {
    return MOVE_ERROR_INVALID_PATTERN;
  }

  // Check if path is blocked
  if (row_diff == 0) {
    // Horizontal move
    int col_step = (col_diff > 0) ? 1 : -1;
    int current_col = move->from_col + col_step;

    while (current_col != move->to_col) {
      if (!game_is_empty(move->from_row, current_col)) {
        return MOVE_ERROR_BLOCKED_PATH;
      }
      current_col += col_step;
    }
  } else {
    // Vertical move
    int row_step = (row_diff > 0) ? 1 : -1;
    int current_row = move->from_row + row_step;

    while (current_row != move->to_row) {
      if (!game_is_empty(current_row, move->from_col)) {
        return MOVE_ERROR_BLOCKED_PATH;
      }
      current_row += row_step;
    }
  }

  return MOVE_ERROR_NONE;
}

// Keep the old function for backward compatibility
bool game_validate_rook_move(const chess_move_t *move) {
  return (game_validate_rook_move_enhanced(move) == MOVE_ERROR_NONE);
}

move_error_t game_validate_queen_move_enhanced(const chess_move_t *move) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;
  int abs_row_diff = abs(row_diff);
  int abs_col_diff = abs(col_diff);

  // Queen moves like rook (horizontally/vertically) or bishop (diagonally)
  if (row_diff == 0 || col_diff == 0) {
    // Rook-like move
    return game_validate_rook_move_enhanced(move);
  } else if (abs_row_diff == abs_col_diff) {
    // Bishop-like move
    return game_validate_bishop_move_enhanced(move);
  }

  return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_queen_move(const chess_move_t *move) {
  return (game_validate_queen_move_enhanced(move) == MOVE_ERROR_NONE);
}

/**
 * @brief Validates a king move with extended error detection
 *
 * @param move A pointer to a move structure
 * @return MOVE_ERROR_NONE if the move is valid, otherwise error code
 *
 * @details
 * The king can move 1 square in any direction or make rooks.
 * The function checks for:
 * - Move 1 space (8 directions)
 * - Move 2 squares horizontally (Castling)
 * - The move must be non-zero (you cannot move on the same field)
 *
 * Delegates validation to game_validate_castling() for castling.
 *
 */
move_error_t game_validate_king_move_enhanced(const chess_move_t *move) {
  int row_diff = move->to_row - move->from_row;
  int col_diff = move->to_col - move->from_col;
  int abs_row_diff = abs(row_diff);
  int abs_col_diff = abs(col_diff);

  // King moves one square in any direction
  // Must move at least 1 square (not 0.0)
  // Puvodni: povoloval tah na stejne pole
  if (abs_row_diff <= 1 && abs_col_diff <= 1 &&
      (abs_row_diff > 0 || abs_col_diff > 0)) {
    return MOVE_ERROR_NONE;
  }

  // Check for castling
  if (abs_row_diff == 0 && abs_col_diff == 2) {
    return game_validate_castling(move);
  }

  return MOVE_ERROR_INVALID_PATTERN;
}

// Keep the old function for backward compatibility
bool game_validate_king_move(const chess_move_t *move) {
  return (game_validate_king_move_enhanced(move) == MOVE_ERROR_NONE);
}

/**
 * @brief Checks whether the move would leave the king in check
 *
 * @param move A pointer to the move structure to test
 * @return true if the move would leave the king in check, false if the move is safe
 *
 * @details
 * The function simulates a move on the board and checks whether the resulting position would
 * she left her own king in the sack. It is used to validate all moves.
 *
 * Process:
 * 1. Saves the original state of the board
 * 2. Performs the move temporarily
 * 3. Checks whether the king is in chess
 * 4. Restore the original state of the board
 *
 */
bool game_would_move_leave_king_in_check(const chess_move_t *move) {
  if (!move)
    return true; // Safety check

  // Save original board state
  piece_t original_from_piece = board[move->from_row][move->from_col];
  piece_t original_to_piece = board[move->to_row][move->to_col];

  // If from_piece is empty (the king is raised during the resignation timer),
  // use move->piece instead of original_from_piece
  if (original_from_piece == PIECE_EMPTY && move->piece != PIECE_EMPTY) {
    original_from_piece = move->piece;
    ESP_LOGD(TAG,
             "🏰 game_would_move_leave_king_in_check: Using move->piece (%d) "
             "because from square is empty (king lifted)",
             move->piece);
  }

  // Check if this is an en passant move
  bool is_en_passant = game_is_en_passant_possible(move);
  piece_t original_en_passant_piece = PIECE_EMPTY;
  uint8_t en_passant_victim_row_local = 0;
  uint8_t en_passant_victim_col_local = 0;

  // If en passant, save the victim pawn position
  if (is_en_passant) {
    original_en_passant_piece =
        board[en_passant_victim_row][en_passant_victim_col];
    en_passant_victim_row_local = en_passant_victim_row;
    en_passant_victim_col_local = en_passant_victim_col;
    // Remove the victim pawn for simulation
    board[en_passant_victim_row][en_passant_victim_col] = PIECE_EMPTY;
  }

  // Make the move temporarily
  board[move->to_row][move->to_col] = original_from_piece;
  board[move->from_row][move->from_col] = PIECE_EMPTY;

  // Determine which player's king to check
  player_t player =
      (game_is_white_piece(original_from_piece)) ? PLAYER_WHITE : PLAYER_BLACK;

  // Check if king is in check after this move
  bool king_in_check = game_is_king_in_check(player);

  // Undo the move
  board[move->from_row][move->from_col] = original_from_piece;
  board[move->to_row][move->to_col] = original_to_piece;

  // Restore en passant victim if it was removed
  if (is_en_passant) {
    board[en_passant_victim_row_local][en_passant_victim_col_local] =
        original_en_passant_piece;
  }

  return king_in_check;
}

/**
 * @brief Checks if en passant is possible for the given move
 *
 * @param move A pointer to the move structure to test
 * @return true if en passant is valid, false if not possible
 *
 * @details
 * En passant is only possible if:
 * 1. The last move was the king moving 2 squares
 * 2. The attacking pawn is on the same line as the right moved pawn
 * 3. The attacking pesce is right next to the right moved pesce
 * 4. The target field is in the middle between the start and the target position of the opponent's fish
 *
 */
bool game_is_en_passant_possible(const chess_move_t *move) {
  if (!has_last_move) {
    ESP_LOGD(TAG, "🔍 En passant: no last move");
    return false;
  }

  // Check if the last move was a pawn moving two squares
  piece_t last_piece = board[last_move_to_row][last_move_to_col];
  bool last_was_pawn =
      (last_piece == PIECE_WHITE_PAWN || last_piece == PIECE_BLACK_PAWN);

  if (!last_was_pawn) {
    ESP_LOGD(TAG, "🔍 En passant: last move was not pawn");
    return false;
  }

  // Check if last move was a double square move
  int last_row_diff = abs((int)last_move_to_row - (int)last_move_from_row);
  if (last_row_diff != 2) {
    ESP_LOGD(TAG, "🔍 En passant: last move was not 2 squares (diff=%d)",
             last_row_diff);
    return false;
  }

  // Check if the attacking pawn is on the same row as the current one
  // moved pawn
  if (move->from_row != last_move_to_row) {
    ESP_LOGD(TAG,
             "🔍 En passant: attacking pawn not on same row (from_row=%d, "
             "last_to_row=%d)",
             move->from_row, last_move_to_row);
    return false;
  }

  // Check if the attacking pawn is exactly next to the currently moved one
  // on foot
  int col_diff = abs((int)move->from_col - (int)last_move_to_col);
  if (col_diff != 1) {
    ESP_LOGD(TAG, "🔍 En passant: pawn not adjacent (col_diff=%d)", col_diff);
    return false;
  }

  // En passant field is always midway between from and to (simple mean)
  int en_passant_target_row = (last_move_from_row + last_move_to_row) / 2;

  ESP_LOGD(TAG,
           "🔍 En passant check: last=%c%d→%c%d, target=%c%d, move=%c%d→%c%d",
           'a' + last_move_from_col, last_move_from_row + 1,
           'a' + last_move_to_col, last_move_to_row + 1, 'a' + last_move_to_col,
           en_passant_target_row + 1, 'a' + move->from_col, move->from_row + 1,
           'a' + move->to_col, move->to_row + 1);

  if (move->to_row == en_passant_target_row &&
      move->to_col == last_move_to_col) {
    ESP_LOGD(TAG, "✅ En passant VALID!");
    return true;
  }

  ESP_LOGD(TAG,
           "❌ En passant: target mismatch (to_row=%d vs target=%d, to_col=%d "
           "vs last_col=%d)",
           move->to_row, en_passant_target_row, move->to_col, last_move_to_col);
  return false;
}

/**
 * @brief Validates castling with comprehensive control of all conditions
 *
 * @param move Pointer to move structure (move king 2 spaces horizontally)
 * @return MOVE_ERROR_NONE if castling is valid, otherwise error code
 *
 * @details
 * Castling must meet all these conditions:
 * 1. The figure must be a king
 * 2. The king hasn't moved yet
 * 3. She hasn't moved yet
 * 4. Vez exists in the correct position
 * 5. The road between the king and the prison is empty
 * 6. The king is not in chess
 * 7. The king does not pass through the saved square
 * 8. The king of eternity in sax
 *
 * Castling is possible:
 * - Kingside (O-O): king e1->g1, take h1->f1
 * - Queenside (O-O-O): king e1->c1, take a1->d1
 *
 */
move_error_t game_validate_castling(const chess_move_t *move) {
  if (!move) {
    return MOVE_ERROR_INVALID_MOVE_STRUCTURE;
  }

  piece_t piece = move->piece;
  bool is_white = game_is_white_piece(piece);

  // Check if it's a king
  if (piece != PIECE_WHITE_KING && piece != PIECE_BLACK_KING) {
    return MOVE_ERROR_INVALID_PATTERN;
  }

  // Check if king has moved
  if ((is_white && white_king_moved) || (!is_white && black_king_moved)) {
    ESP_LOGD(TAG, "❌ Castling: king has moved");
    return MOVE_ERROR_CASTLING_BLOCKED;
  }

  // If the resignation timer is active and the king is raised,
  // use the saved king position from resignation_state instead of checking the board
  int king_row = is_white ? 0 : 7;
  int expected_king_col = 4; // e-file

  // If the resignation timer is active and the king belongs to the current player,
  // use saved king position from resignation_state
  if (resignation_state.active &&
      resignation_state.player == (is_white ? PLAYER_WHITE : PLAYER_BLACK)) {
    // King is raised - use saved position from resignation_state
    king_row = resignation_state.king_row;
    expected_king_col = resignation_state.king_col;
    ESP_LOGI(TAG,
             "🏰 Castling validation: Using stored king position from "
             "resignation_state: %c%d",
             'a' + expected_king_col, king_row + 1);
  }

  // Checking that from_row/from_col matches the expected position of the king
  // (either from board or resignation_state)
  if (move->from_row != king_row || move->from_col != expected_king_col) {
    ESP_LOGD(
        TAG,
        "❌ Castling: king not in starting position (from %c%d, expected %c%d)",
        'a' + move->from_col, move->from_row + 1, 'a' + expected_king_col,
        king_row + 1);
    return MOVE_ERROR_CASTLING_BLOCKED;
  }

  // Determine castling direction
  bool is_kingside = (move->to_col == 6);
  bool is_queenside = (move->to_col == 2);

  if (!is_kingside && !is_queenside) {
    return MOVE_ERROR_INVALID_PATTERN;
  }

  // Determine rook position
  int rook_col = is_kingside ? 7 : 0;

  // Check if rook exists at expected position
  piece_t expected_rook = is_white ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
  piece_t rook_piece = board[king_row][rook_col];
  if (rook_piece != expected_rook) {
    ESP_LOGD(TAG, "❌ Castling: rook not found at expected position %c%d",
             'a' + rook_col, king_row + 1);
    return MOVE_ERROR_CASTLING_BLOCKED;
  }

  // Check if corresponding rook has moved
  if (is_white) {
    if (is_kingside && white_rook_h_moved) {
      ESP_LOGD(TAG, "❌ Castling: kingside rook has moved");
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
    if (is_queenside && white_rook_a_moved) {
      ESP_LOGD(TAG, "❌ Castling: queenside rook has moved");
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
  } else {
    if (is_kingside && black_rook_h_moved) {
      ESP_LOGD(TAG, "❌ Castling: kingside rook has moved");
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
    if (is_queenside && black_rook_a_moved) {
      ESP_LOGD(TAG, "❌ Castling: queenside rook has moved");
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
  }

  // Check if squares between king and rook are empty
  // rook_col already defined above
  int start_col =
      (move->from_col < rook_col) ? move->from_col + 1 : rook_col + 1;
  int end_col = (move->from_col < rook_col) ? rook_col : move->from_col;

  for (int col = start_col; col < end_col; col++) {
    if (!game_is_empty(king_row, col)) {
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
  }

  // Check if king is currently in check
  if (game_is_king_in_check(is_white ? PLAYER_WHITE : PLAYER_BLACK)) {
    return MOVE_ERROR_CASTLING_BLOCKED;
  }

  // Check if king would move through check or end in check
  int step = is_kingside ? 1 : -1;
  for (int col = move->from_col; col != move->to_col + step; col += step) {
    if (col == move->from_col)
      continue; // Skip starting position

    // Create temporary move to test this square
    chess_move_t temp_move = {.from_row = move->from_row,
                              .from_col = move->from_col,
                              .to_row = move->to_row,
                              .to_col = col,
                              .piece = piece,
                              .captured_piece = PIECE_EMPTY,
                              .timestamp = 0};

    if (game_would_move_leave_king_in_check(&temp_move)) {
      return MOVE_ERROR_CASTLING_BLOCKED;
    }
  }

  return MOVE_ERROR_NONE;
}
