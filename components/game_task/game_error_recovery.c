/**
 * @file game_error_recovery.c
 * @brief Invalid move recovery, move command processing, and LED animations.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_move_validate.h"
#include "chess_gameplay_policy.h"

#include "../led_task/include/led_task.h"
#include "led_mapping.h"
#include "freertos_chess.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_ERROR";

// ============================================================================
// ENHANCED SMART ERROR HANDLING SYSTEM
// ============================================================================

/**
 * @brief Enhanced smart error handling for invalid moves
 * @param move Invalid move that was attempted
 * @param error Type of error that occurred
 */
void game_handle_invalid_move_smart(const chess_move_t *move,
                                    move_error_t error) {
  ESP_LOGI(TAG, "🚫 INVALID MOVE - Starting smart recovery");

  if (!move) {
    ESP_LOGE(TAG, "❌ Critical error: NULL move pointer in error handling");
    return;
  }

  // 1. Červené bliknutí chybného tahu
  uint8_t from_led = chess_pos_to_led_index(move->from_row, move->from_col);
  uint8_t to_led = chess_pos_to_led_index(move->to_row, move->to_col);

  // Flash error - 5 rychlých červených bliknutí
  for (int i = 0; i < 5; i++) {
    led_set_pixel_safe(from_led, 255, 0, 0); // Red
    led_set_pixel_safe(to_led, 255, 0, 0);   // Red
    vTaskDelay(pdMS_TO_TICKS(100));
    led_clear_board_only();
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // 2. Rosvítit zeleně zdrojové pole (kde je figurka)
  led_set_pixel_safe(from_led, 0, 255, 0); // Green - kde vzít figurku

  // 3. Rosvítit červeně kolem nevalidního cíle
  game_highlight_invalid_target_area(move->to_row, move->to_col);

  // 4. Rosvítit validní tahy pro tuto figurku
  game_highlight_valid_moves_for_piece(move->from_row, move->from_col);

  // 5. KRITICKÉ: Neměnit hráče, nechat ho opravit tah
  ESP_LOGI(TAG, "💡 Please return piece to correct square. Valid moves are "
                "highlighted.");

  // 6. Nastavit internal stav "waiting for correction"
  chess_policy_error_recovery_enter_lock();
  error_recovery_state.invalid_row = move->from_row;
  error_recovery_state.invalid_col = move->from_col;
}

/**
 * @brief Highlight invalid target area with red LEDs
 * @param row Row of invalid target
 * @param col Column of invalid target
 */
void game_highlight_invalid_target_area(uint8_t row, uint8_t col) {
  // Rosvítit červeně pole okolo nevalidního cíle
  for (int dr = -1; dr <= 1; dr++) {
    for (int dc = -1; dc <= 1; dc++) {
      int new_row = row + dr;
      int new_col = col + dc;

      if (new_row >= 0 && new_row < 8 && new_col >= 0 && new_col < 8) {
        uint8_t led_index = chess_pos_to_led_index(new_row, new_col);
        led_set_pixel_safe(led_index, 255, 50, 50); // Light red
      }
    }
  }
}

/**
 * @brief Highlight valid moves for a specific piece
 * @param row Row of the piece
 * @param col Column of the piece
 */
void game_highlight_valid_moves_for_piece(uint8_t row, uint8_t col) {
  piece_t piece = board[row][col];
  if (piece == PIECE_EMPTY)
    return;

  // Najít všechny validní tahy pro tuto figurku
  for (int to_row = 0; to_row < 8; to_row++) {
    for (int to_col = 0; to_col < 8; to_col++) {
      chess_move_t test_move = {.from_row = row,
                                .from_col = col,
                                .to_row = to_row,
                                .to_col = to_col,
                                .piece = piece,
                                .captured_piece = board[to_row][to_col]};

      if (game_is_valid_move(&test_move) == MOVE_ERROR_NONE) {
        uint8_t led_index = chess_pos_to_led_index(to_row, to_col);
        led_set_pixel_safe(led_index, 0, 0, 255); // Blue - validní tahy
      }
    }
  }
}

/**
 * @brief Legacy function for backward compatibility
 */
void game_handle_invalid_move(move_error_t error, const chess_move_t *move) {
  ESP_LOGI(TAG, "🚨 ENTERING game_handle_invalid_move - error: %d", error);

  if (!chess_policy_error_recovery_enabled()) {
    ESP_LOGW(TAG, "Invalid move ignored (error recovery disabled)");
    return;
  }

  // KRITICKÁ OPRAVA: Bezpečnostní kontrola move pointeru
  if (!move) {
    ESP_LOGE(TAG, "❌ Critical error: NULL move pointer in error handling");
    return;
  }

  // Kontrola validity coordinates před použitím
  if (move->from_row >= 8 || move->from_col >= 8 || move->to_row >= 8 ||
      move->to_col >= 8) {
    ESP_LOGE(TAG, "❌ Critical error: Invalid coordinates in move structure");
    return;
  }

  ESP_LOGI(TAG, "🚨 Invalid move detected - smart error handling");
  ESP_LOGI(TAG, "   Move: %c%d -> %c%d", 'a' + move->from_col,
           move->from_row + 1, 'a' + move->to_col, move->to_row + 1);

  // Remote / zastaralý UI může poslat tah z prázdného pole. Simulace „figurka je
  // fyzicky na to“ kopíruje board[from] na board[to]; když je from prázdné,
  // smaže se figurka na cíli (logicky „zmizí“). HW recovery v tomto případě
  // nedává smysl — desku neměnit.
  piece_t src_piece = board[move->from_row][move->from_col];
  if (src_piece == PIECE_EMPTY) {
    ESP_LOGW(TAG,
             "[STAGING] game_handle_invalid_move: empty source square — "
             "skipping board mutation and LED recovery (error=%d)",
             (int)error);
    return;
  }

  // Chytrý error handling podle požadavků

  // 1. Nastavit červené pole na neplatné pozici
  error_recovery_state.has_invalid_piece = true;
  error_recovery_state.invalid_row = move->to_row;
  error_recovery_state.invalid_col = move->to_col;
  error_recovery_state.piece_type = board[move->from_row][move->from_col];

  // 2. Zapamatovat si původní validní pozici (odkud byl tah)
  error_recovery_state.original_valid_row = move->from_row;
  error_recovery_state.original_valid_col = move->from_col;

  // 3. Přesunout figurku na neplatnou pozici (simulace HW reality)
  if (chess_policy_error_recovery_should_mutate_board()) {
    board[move->to_row][move->to_col] = board[move->from_row][move->from_col];
    board[move->from_row][move->from_col] = PIECE_EMPTY;
  }

  // 4. JASNÉ VIZUÁLNÍ UPOZORNĚNÍ - červené pole + blikání pro upoutání
  // pozornosti
  if (chess_policy_error_recovery_led_red_blink() ||
      chess_policy_error_recovery_led_red_persist()) {
    led_clear_all_safe();
    uint8_t invalid_led = chess_pos_to_led_index(move->to_row, move->to_col);

    ESP_LOGI(TAG, "🚨 STARTING ERROR ANIMATION - RED BLINKING");

    if (chess_policy_error_recovery_led_red_blink()) {
      for (int blink = 0; blink < 4; blink++) {
        led_set_pixel_safe(invalid_led, 255, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
        led_set_pixel_safe(invalid_led, 0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(150));
      }
    }

    if (chess_policy_error_recovery_led_red_persist()) {
      led_set_pixel_safe(invalid_led, 255, 0, 0);
    }
  }

  ESP_LOGI(TAG, "🔴 RED SQUARE ACTIVE: %c%d (piece must be lifted from here)",
           'a' + move->to_col, move->to_row + 1);
  ESP_LOGI(TAG,
           "📍 RECOVERY TARGET: Valid moves will show from %c%d when piece is "
           "lifted",
           'a' + move->from_col, move->from_row + 1);
  ESP_LOGI(TAG,
           "💡 USER ACTION REQUIRED: Lift piece from red square to continue");

  // KRITICKÁ OPRAVA: NON-BLOCKING error handling - žádné loops!
  // Červené pole zůstane rozsvícené dokud se figurka nezvedne
  // Recovery se řeší v game_process_pickup_command()

  ESP_LOGI(TAG, "💡 Error recovery active - red square will stay lit until "
                "piece is lifted");

  chess_policy_error_recovery_enter_lock();
}

/**
 * @brief Process move command from UART
 * @param move_cmd Move command structure with coordinates
 */
void game_process_move_command(const void *move_cmd_ptr) {
  typedef struct {
    uint8_t command_type;
    uint8_t from_row;
    uint8_t from_col;
    uint8_t to_row;
    uint8_t to_col;
  } move_command_t;

  const move_command_t *move_cmd = (const move_command_t *)move_cmd_ptr;

  ESP_LOGI(TAG, "Processing move: [%d,%d] -> [%d,%d]", move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Validate coordinates
  if (move_cmd->from_row >= 8 || move_cmd->from_col >= 8 ||
      move_cmd->to_row >= 8 || move_cmd->to_col >= 8) {
    ESP_LOGE(TAG, "Invalid coordinates: out of board range");
    return;
  }

  // Get pieces at source and destination
  piece_t from_piece = board[move_cmd->from_row][move_cmd->from_col];
  piece_t to_piece = board[move_cmd->to_row][move_cmd->to_col];

  // Check if source square has a piece
  if (from_piece == PIECE_EMPTY) {
    ESP_LOGE(TAG, "Invalid move: no piece at [%d,%d]", move_cmd->from_row,
             move_cmd->from_col);
    return;
  }

  // Check if piece belongs to current player
  bool is_white_piece =
      (from_piece >= PIECE_WHITE_PAWN && from_piece <= PIECE_WHITE_KING);
  bool is_black_piece =
      (from_piece >= PIECE_BLACK_PAWN && from_piece <= PIECE_BLACK_KING);

  if ((current_player == PLAYER_WHITE && !is_white_piece) ||
      (current_player == PLAYER_BLACK && !is_black_piece)) {
    ESP_LOGE(TAG, "Invalid move: cannot move opponent's piece");
    return;
  }

  // Check if destination is occupied by own piece
  if (to_piece != PIECE_EMPTY) {
    bool dest_is_white =
        (to_piece >= PIECE_WHITE_PAWN && to_piece <= PIECE_WHITE_KING);
    bool dest_is_black =
        (to_piece >= PIECE_BLACK_PAWN && to_piece <= PIECE_BLACK_KING);

    if ((current_player == PLAYER_WHITE && dest_is_white) ||
        (current_player == PLAYER_BLACK && dest_is_black)) {
      ESP_LOGE(TAG, "Invalid move: destination occupied by own piece");
      return;
    }
  }

  // TODO: Add proper chess move validation (piece-specific rules)
  // For now, just allow any move that passes basic checks

  // Create chess move structure for validation
  chess_move_t chess_move = {.from_row = move_cmd->from_row,
                             .from_col = move_cmd->from_col,
                             .to_row = move_cmd->to_row,
                             .to_col = move_cmd->to_col,
                             .piece = from_piece,
                             .captured_piece = to_piece,
                             .timestamp = esp_timer_get_time() / 1000};

  // Validate move using enhanced validation
  move_error_t move_error = game_is_valid_move(&chess_move);
  if (move_error != MOVE_ERROR_NONE) {
    // NEW ERROR HANDLING: Don't return, handle invalid move properly
    game_handle_invalid_move(move_error, &chess_move);
    return;
  }

  // Use game_execute_move() instead of direct board
  // manipulation This ensures all game state is properly updated (castling
  // flags, en passant, etc.) and player is changed correctly
  ESP_LOGI(TAG, "Executing move: %s piece from [%d,%d] to [%d,%d]",
           (is_white_piece ? "White" : "Black"), move_cmd->from_row,
           move_cmd->from_col, move_cmd->to_row, move_cmd->to_col);

  // Execute the move using proper game_execute_move() function
  if (!game_execute_move(&chess_move)) {
    ESP_LOGE(TAG, "Failed to execute move from [%d,%d] to [%d,%d]",
             move_cmd->from_row, move_cmd->from_col, move_cmd->to_row,
             move_cmd->to_col);
    // Note: move_cmd doesn't have response_queue, this is legacy function
    return;
  }

  // HRÁČ SE UŽ ZMĚNIL V game_execute_move() - použít aktuálního hráče
  player_t previous_player =
      (current_player == PLAYER_WHITE) ? PLAYER_BLACK : PLAYER_WHITE;

  // Spustit move path animaci PŘED změnou hráče (podle starého projektu)
  uint8_t from_led =
      chess_pos_to_led_index(move_cmd->from_row, move_cmd->from_col);
  uint8_t to_led = chess_pos_to_led_index(move_cmd->to_row, move_cmd->to_col);
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

  // KRITICKÉ: Endgame kontrola PŘED player change animací!
  // Pokud je endgame, player change se NESPOUŠTÍ
  game_state_t end_game_result = game_check_end_game_conditions();
  if (end_game_result == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_FINISHED;
    game_active = false;

    // Najít pozici krále vítěze pro endgame animaci
    player_t winner =
        previous_player;   // Vítěz je předchozí hráč (ten, kdo udělal tah)
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
             "🎯 Game finished in move command! Starting endgame animation at "
             "position %d",
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

    ESP_LOGI(TAG,
             "✅ Endgame animation started - player change animation SKIPPED");
  } else {
    // Není endgame - spustit player change animaci s NOVÝM hráčem
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

    // Zkontrolovat, zda je nový hráč v šachu
    bool in_check = game_is_king_in_check(current_player);
    if (in_check) {
      // Najít pozici krále
      int king_row = -1, king_col = -1;
      piece_t king_piece = (current_player == PLAYER_WHITE) ? PIECE_WHITE_KING
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
  }

  // History and last_move tracking is already done in
  // game_execute_move() - no need to duplicate here

  // Track captured pieces
  if (to_piece != PIECE_EMPTY) {
    if (current_player == PLAYER_WHITE) {
      // Black piece was captured by white
      if (black_captured_index < 16) {
        black_captured_pieces[black_captured_index++] = to_piece;
        black_captured_count++;
      }
    } else {
      // White piece was captured by black
      if (white_captured_index < 16) {
        white_captured_pieces[white_captured_index++] = to_piece;
        white_captured_count++;
      }
    }
    moves_without_capture = 0; // Reset counter on capture
  } else {
    moves_without_capture++; // Increment counter for non-capture moves
    if (moves_without_capture > max_moves_without_capture) {
      max_moves_without_capture = moves_without_capture;
    }
  }

  // Update time statistics
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t move_time = current_time - last_move_time;

  if (current_player == PLAYER_WHITE) {
    // Black just moved
    black_time_total += move_time;
    black_moves_count++;
  } else {
    // White just moved
    white_time_total += move_time;
    white_moves_count++;
  }

  // Update position history for draw detection
  game_add_position_to_history();

  // Note: End-game conditions already checked earlier in this function (line
  // 6612) If game finished, it was already handled with endgame animation

  // Check if king is in check
  if (game_is_king_in_check(current_player)) {
    if (current_player == PLAYER_WHITE) {
      white_checks++;
    } else {
      black_checks++;
    }
    ESP_LOGI(TAG, "⚠️  CHECK! %s king is under attack!",
             (current_player == PLAYER_WHITE) ? "White" : "Black");
  }

  // Update last move time for next move
  last_move_time = current_time;

  // Display updated board (check auto-display setting)
  // Note: Auto-display control is in UART task, board is always shown for now
  game_print_board();

  // Check for check/checkmate conditions
  game_check_game_conditions();
}

/**
 * @brief Show move animation with ASCII art
 * @param from_row Source row
 * @param from_col Source column
 * @param to_row Destination row
 * @param to_col Destination column
 * @param piece Piece being moved
 * @param captured Piece captured (if any)
 */
void game_show_move_animation(uint8_t from_row, uint8_t from_col,
                              uint8_t to_row, uint8_t to_col, piece_t piece,
                              piece_t captured) {
  // Convert coordinates to chess notation
  char from_square[4], to_square[4];
  game_coords_to_square(from_row, from_col, from_square);
  game_coords_to_square(to_row, to_col, to_square);

  // Get piece symbol and name
  const char *piece_symbol = piece_symbols[piece];
  const char *piece_name = game_get_piece_name(piece);

  ESP_LOGI(TAG, "╭─────────────────────────────────╮");
  ESP_LOGI(TAG, "│        MOVE ANIMATION          │");
  ESP_LOGI(TAG, "├─────────────────────────────────┤");
  ESP_LOGI(TAG, "│  %s %s moves from %s to %s  │", piece_symbol, piece_name,
           from_square, to_square);

  if (captured != PIECE_EMPTY) {
    const char *captured_symbol = piece_symbols[captured];
    const char *captured_name = game_get_piece_name(captured);
    ESP_LOGI(TAG, "│  Captures: %s %s                │", captured_symbol,
             captured_name);
  } else {
    ESP_LOGI(TAG, "│  No capture                     │");
  }

  ESP_LOGI(TAG, "╰─────────────────────────────────╯");

  // ED: LED animations are now handled in game_process_drop_command
  // This function only shows text information

  ESP_LOGI(TAG, "🎯 Move: %s -> %s", from_square, to_square);
  ESP_LOGI(TAG, "♟️  %s %s moves...", piece_symbol, piece_name);
  ESP_LOGI(TAG, "✨ ...to %s", to_square);

  if (captured != PIECE_EMPTY) {
    ESP_LOGI(TAG, "💥 %s captured!", game_get_piece_name(captured));
  }

  ESP_LOGI(TAG, "✅ Move completed!");
}

/**
 * @brief Show player change animation
 * @param previous_player Previous player
 * @param current_player Current player
 */
void game_show_player_change_animation(player_t previous_player,
                                       player_t current_player) {
  ESP_LOGI(TAG, "🔄 Showing player change animation: %s -> %s",
           previous_player == PLAYER_WHITE ? "White" : "Black",
           current_player == PLAYER_WHITE ? "White" : "Black");

  // Clear board first
  led_clear_board_only();
  vTaskDelay(pdMS_TO_TICKS(200));

  // ED: Find all pieces first, then animate all columns simultaneously
  int prev_pieces[8] = {-1, -1, -1, -1,
                        -1, -1, -1, -1}; // Row positions for each column
  int curr_pieces[8] = {-1, -1, -1, -1,
                        -1, -1, -1, -1}; // Row positions for each column

  // Find closest pieces for all columns
  for (int col = 0; col < 8; col++) {
    // Find closest piece of previous player (closest to opponent's side)
    if (previous_player == PLAYER_WHITE) {
      // Look from top (row 7) down to find white piece closest to black side
      for (int row = 7; row >= 0; row--) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
          prev_pieces[col] = row;
          break;
        }
      }
    } else {
      // Look from bottom (row 0) up to find black piece closest to white side
      for (int row = 0; row < 8; row++) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
          prev_pieces[col] = row;
          break;
        }
      }
    }

    // Find closest piece of current player (closest to previous player's
    // side)
    if (current_player == PLAYER_WHITE) {
      // Look from bottom (row 0) up to find white piece closest to black side
      for (int row = 0; row < 8; row++) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_WHITE_PAWN && piece <= PIECE_WHITE_KING) {
          curr_pieces[col] = row;
          break;
        }
      }
    } else {
      // Look from top (row 7) down to find black piece closest to white side
      for (int row = 7; row >= 0; row--) {
        piece_t piece = board[row][col];
        if (piece >= PIECE_BLACK_PAWN && piece <= PIECE_BLACK_KING) {
          curr_pieces[col] = row;
          break;
        }
      }
    }
  }

  // ENHANCED: Ultra-smooth player change animation with harmonious colors
  for (int step = 0; step < 8; step++) { // Reduced steps for speed
    float progress = (float)step / 7.0f;

    // Clear board first
    led_clear_board_only();

    // Animate all columns at once with ultra-smooth trail effect
    for (int col = 0; col < 8; col++) {
      if (prev_pieces[col] != -1 && curr_pieces[col] != -1) {
        // Create ultra-smooth trail effect - show multiple points along the
        // path
        for (int trail = 0; trail < 5; trail++) {
          float trail_progress = progress - (trail * 0.12f);
          if (trail_progress < 0)
            continue;
          if (trail_progress > 1)
            break;

          // Calculate intermediate position with smooth easing
          float eased_progress = trail_progress * trail_progress *
                                 (3.0f - 2.0f * trail_progress); // Smooth step
          int inter_row =
              prev_pieces[col] +
              (curr_pieces[col] - prev_pieces[col]) * eased_progress;
          int inter_col = col;
          uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);

          // Harmonious color transition: Deep Blue -> Purple -> Magenta ->
          // Pink
          // -> Gold -> Yellow
          uint8_t red, green, blue;
          if (trail_progress < 0.16f) {
            // Deep Blue to Purple
            float local_progress = trail_progress / 0.16f;
            red = (uint8_t)(128 * local_progress);
            green = 0;
            blue = 255;
          } else if (trail_progress < 0.33f) {
            // Purple to Magenta
            float local_progress = (trail_progress - 0.16f) / 0.17f;
            red = 128 + (uint8_t)(127 * local_progress);
            green = 0;
            blue = 255;
          } else if (trail_progress < 0.5f) {
            // Magenta to Pink
            float local_progress = (trail_progress - 0.33f) / 0.17f;
            red = 255;
            green = (uint8_t)(128 * local_progress);
            blue = 255 - (uint8_t)(128 * local_progress);
          } else if (trail_progress < 0.66f) {
            // Pink to Gold
            float local_progress = (trail_progress - 0.5f) / 0.16f;
            red = 255;
            green = 128 + (uint8_t)(127 * local_progress);
            blue = 127 - (uint8_t)(127 * local_progress);
          } else if (trail_progress < 0.83f) {
            // Gold to Yellow
            red = 255;
            green = 255;
            blue = 0;
          } else {
            // Yellow to Bright White
            float local_progress = (trail_progress - 0.83f) / 0.17f;
            red = 255;
            green = 255;
            blue = (uint8_t)(255 * local_progress);
          }

          // Enhanced trail brightness effect - smoother gradient
          float trail_brightness = 1.0f - (trail * 0.18f);
          red = (uint8_t)(red * trail_brightness);
          green = (uint8_t)(green * trail_brightness);
          blue = (uint8_t)(blue * trail_brightness);

          // Enhanced pulsing effect with multiple harmonics
          float pulse1 = 0.7f + 0.3f * sin(progress * 6.28f + trail * 1.26f);
          float pulse2 = 0.9f + 0.1f * sin(progress * 12.56f + trail * 2.51f);
          float combined_pulse = pulse1 * pulse2;

          red = (uint8_t)(red * combined_pulse);
          green = (uint8_t)(green * combined_pulse);
          blue = (uint8_t)(blue * combined_pulse);

          led_set_pixel_safe(inter_led, red, green, blue);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(25)); // Faster animation - half the time
  }

  // Clear board after animation
  led_clear_board_only();
  vTaskDelay(pdMS_TO_TICKS(200));

  // Finally, highlight movable pieces for current player
  chess_policy_highlight_movable_if_enabled();
}

/**
 * @brief Test move animation
 */
void game_test_move_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing move animation...");

  // Test move from e2 to e4
  // uint8_t to_led = chess_pos_to_led_index(3, 4);   // e4 - removed unused
  // variable

  // Progressive color animation from green to blue - ZRYCHLENO
  for (int step = 0; step < 6; step++) { // ZRYCHLENO: z 10 na 6 kroků
    float progress = (float)step / 5.0f;

    // Calculate intermediate position
    int inter_row = 1 + (3 - 1) * progress;
    int inter_col = 4;
    uint8_t inter_led = chess_pos_to_led_index(inter_row, inter_col);

    // Calculate color transition (green -> blue)
    uint8_t red = 0;
    uint8_t green = 255 - (255 * progress); // 255 -> 0
    uint8_t blue = 0 + (255 * progress);    // 0 -> 255

    led_clear_board_only();
    led_set_pixel_safe(inter_led, red, green, blue);
    vTaskDelay(pdMS_TO_TICKS(50)); // ZRYCHLENO: z 100ms na 50ms
  }

  // Odstraněno modré bliknutí na konci - animace končí plynule
}

/**
 * @brief Test player change animation
 */
void game_test_player_change_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing player change animation...");
  game_show_player_change_animation(PLAYER_WHITE, PLAYER_BLACK);
}

/**
 * @brief Test castling animation
 */
void game_test_castle_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing castling animation...");

  // Test white kingside castling
  uint8_t king_from_led = chess_pos_to_led_index(0, 4); // e1
  uint8_t king_to_led = chess_pos_to_led_index(0, 6);   // g1
  uint8_t rook_from_led = chess_pos_to_led_index(0, 7); // h1
  uint8_t rook_to_led = chess_pos_to_led_index(0, 5);   // f1

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

  // 3. Highlight final positions (green)
  led_set_pixel_safe(king_to_led, 0, 255, 0); // Green
  led_set_pixel_safe(rook_to_led, 0, 255, 0); // Green
  vTaskDelay(pdMS_TO_TICKS(500));

  // 4. Clear highlights
  led_clear_board_only();
}

/**
 * @brief Test promotion animation
 */
void game_test_promote_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing promotion animation...");

  // Test promotion on a8
  uint8_t promotion_led = chess_pos_to_led_index(7, 0); // a8

  // Show enhanced promotion animation
  led_command_t promote_cmd = {.type = LED_CMD_ANIM_PROMOTE,
                               .led_index = promotion_led,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 2000,
                               .data = NULL};
  led_execute_command_new(&promote_cmd);
  vTaskDelay(pdMS_TO_TICKS(1000)); // Faster animation

  // Clear highlights
  led_clear_board_only();
}

/**
 * @brief Test endgame animation
 */
void game_test_endgame_animation(void) {
  ESP_LOGI(TAG, "🎬 Testing endgame animation...");

  // Test endgame animation using direct LED command
  uint8_t king_pos = 27; // d4 square

  led_command_t endgame_cmd = {.type = LED_CMD_ANIM_ENDGAME,
                               .led_index = king_pos,
                               .red = 255,
                               .green = 215,
                               .blue = 0, // Gold
                               .duration_ms = 3000,
                               .data = NULL};
  led_execute_command_new(&endgame_cmd);
  vTaskDelay(pdMS_TO_TICKS(1500)); // Faster animation

  // Clear board
  led_clear_board_only();
}

/**
 * @brief Check for check/checkmate conditions
 */
void game_check_game_conditions(void) {
  // TODO: Implement check detection
  // For now, just log the current state

  ESP_LOGI(TAG, "🔍 Checking game conditions...");

  // Check if king is in check (simplified)
  bool white_king_found = false;
  bool black_king_found = false;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (board[row][col] == PIECE_WHITE_KING) {
        white_king_found = true;
      } else if (board[row][col] == PIECE_BLACK_KING) {
        black_king_found = true;
      }
    }
  }

  if (!white_king_found) {
    ESP_LOGW(TAG, "⚠️  WHITE KING MISSING - Black wins!");
    current_game_state = GAME_STATE_FINISHED;
  } else if (!black_king_found) {
    ESP_LOGW(TAG, "⚠️  BLACK KING MISSING - White wins!");
    current_game_state = GAME_STATE_FINISHED;
  }

  ESP_LOGI(TAG, "Game state: %s",
           (current_game_state == GAME_STATE_ACTIVE)   ? "Active"
           : (current_game_state == GAME_STATE_IDLE)   ? "Idle"
           : (current_game_state == GAME_STATE_PAUSED) ? "Paused"
                                                       : "Finished");
}
