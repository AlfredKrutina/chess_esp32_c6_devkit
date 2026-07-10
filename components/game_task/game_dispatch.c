/**
 * @file game_dispatch.c
 * @brief Command queue dispatch, undo, and UART board streaming.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "game_matrix_guard.h"
#include "game_snapshot.h"
#include "freertos_chess.h"

#include "../led_task/include/led_task.h"
#include "../button_task/include/button_task.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../freertos_chess/include/streaming_output.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_DISPATCH";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

/**
 * @brief Send board data to UART task via response queue
 */
static void game_send_board_to_uart(QueueHandle_t response_queue) {
  if (response_queue == NULL) {
    ESP_LOGW(TAG, "No response queue available for board data");
    return;
  }

  // Reset WDT before starting operation
  game_task_wdt_reset_safe();

  // MEMORY OPTIMIZATION: Use streaming output instead of malloc(3072)
  // This eliminates heap fragmentation and reduces memory usage
  ESP_LOGI(TAG, "📡 Using streaming output for board display (no malloc)");

  // Configure streaming for queue output
  esp_err_t ret = streaming_set_queue_output(response_queue);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure streaming output: %s",
             esp_err_to_name(ret));
    return;
  }

  // MEMORY OPTIMIZATION: Use streaming output instead of large buffer

  // Stream board header
  stream_writeln("    a   b   c   d   e   f   g   h");
  stream_writeln("  +---+---+---+---+---+---+---+---+");

  // Stream board rows
  for (int row = 7; row >= 0; row--) {
    // Reset watchdog every row to prevent timeouts
    game_task_wdt_reset_safe();

    stream_printf("%d |", row + 1);

    for (int col = 0; col < 8; col++) {
      piece_t piece = board[row][col];
      const char *symbol;

      switch (piece) {
      case PIECE_WHITE_PAWN:
        symbol = "P";
        break;
      case PIECE_WHITE_KNIGHT:
        symbol = "N";
        break;
      case PIECE_WHITE_BISHOP:
        symbol = "B";
        break;
      case PIECE_WHITE_ROOK:
        symbol = "R";
        break;
      case PIECE_WHITE_QUEEN:
        symbol = "Q";
        break;
      case PIECE_WHITE_KING:
        symbol = "K";
        break;
      case PIECE_BLACK_PAWN:
        symbol = "p";
        break;
      case PIECE_BLACK_KNIGHT:
        symbol = "n";
        break;
      case PIECE_BLACK_BISHOP:
        symbol = "b";
        break;
      case PIECE_BLACK_ROOK:
        symbol = "r";
        break;
      case PIECE_BLACK_QUEEN:
        symbol = "q";
        break;
      case PIECE_BLACK_KING:
        symbol = "k";
        break;
      default:
        symbol = "·";
        break;
      }

      stream_printf(" %s |", symbol);
    }

    stream_printf(" %d\n", row + 1);

    if (row > 0) {
      stream_writeln("  +---+---+---+---+---+---+---+---+");
    }
  }

  // Stream board footer
  stream_writeln("  +---+---+---+---+---+---+---+---+");
  stream_writeln("    a   b   c   d   e   f   g   h");

  // Stream game status
  stream_printf("\nCurrent player: %s\n",
                current_player == PLAYER_WHITE ? "White" : "Black");

  stream_printf("Move count: %" PRIu32 "\n", move_count);

  // Stream helpful tip
  stream_writeln("💡 Use 'UP <square>' to lift piece, 'DN <square>' to place");

  // MEMORY OPTIMIZATION: Streaming output handles data transmission
  // Send completion message to response queue
  game_response_t completion_response = {
      .type = GAME_RESPONSE_SUCCESS,
      .command_type = GAME_CMD_SHOW_BOARD,
      .error_code = 0,
      .message = "Board display streaming completed successfully",
      .timestamp = esp_timer_get_time() / 1000};
  strcpy(completion_response.data, "streaming completed");

  if (response_queue != NULL) {
    xQueueSend(response_queue, &completion_response, pdMS_TO_TICKS(100));
  }

  // No need for manual queue management or large buffers
  ESP_LOGI(TAG, "✅ Board display streaming completed successfully");

  // Final WDT reset
  game_task_wdt_reset_safe();
}

static void undo_pop_last_capture_list(piece_t captured, bool mover_is_white) {
  if (captured == PIECE_EMPTY) {
    return;
  }
  if (mover_is_white) {
    if (white_captured_index > 0) {
      white_captured_index--;
      if (white_captured_count > 0) {
        white_captured_count--;
      }
      if (white_captures > 0) {
        white_captures--;
      }
    }
  } else {
    if (black_captured_index > 0) {
      black_captured_index--;
      if (black_captured_count > 0) {
        black_captured_count--;
      }
      if (black_captures > 0) {
        black_captures--;
      }
    }
  }
}

static bool game_undo_last_move_impl(void) {
  if (history_index == 0) {
    ESP_LOGW(TAG, "Undo: no moves in history");
    return false;
  }

  uint32_t idx = history_index - 1;
  chess_move_t m = move_history[idx];
  move_type_t kind = move_history_kind[idx];

  bool mover_white =
      (m.piece >= PIECE_WHITE_PAWN && m.piece <= PIECE_WHITE_KING);
  uint8_t fr = m.from_row;
  uint8_t fc = m.from_col;
  uint8_t tr = m.to_row;
  uint8_t tc = m.to_col;

  bool toggle_player_after = true;
  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    /* Rošáda: hráč se nemění po tahu krále; až po položení věže. Undo dokončené
     * rošády musí zase přepnout na protihráče → toggle true jen když věž už byla
     * v logice přesunutá na „mezilehlé“ pole. */
    uint8_t rook_castle_col =
        (tc > fc) ? (uint8_t)(tc - 1) : (uint8_t)(tc + 1);
    piece_t rook_piece =
        (m.piece == PIECE_WHITE_KING) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    toggle_player_after = (board[fr][rook_castle_col] == rook_piece);
  } else if (kind == MOVE_TYPE_PROMOTION) {
    piece_t at_dest = board[tr][tc];
    if ((at_dest == PIECE_WHITE_PAWN || at_dest == PIECE_BLACK_PAWN) &&
        promotion_state.pending) {
      toggle_player_after = false;
    }
  }

  switch (kind) {
  case MOVE_TYPE_EN_PASSANT: {
    board[tr][tc] = PIECE_EMPTY;
    board[fr][fc] = m.piece;
    piece_t victim_pawn =
        mover_white ? PIECE_BLACK_PAWN : PIECE_WHITE_PAWN;
    board[fr][tc] = victim_pawn;
    break;
  }
  case MOVE_TYPE_PROMOTION: {
    piece_t at_dest = board[tr][tc];
    if ((at_dest == PIECE_WHITE_PAWN || at_dest == PIECE_BLACK_PAWN) &&
        promotion_state.pending) {
      if (xSemaphoreTakeRecursive(promotion_mutex, pdMS_TO_TICKS(100)) ==
          pdTRUE) {
        promotion_state.pending = false;
        promotion_state.square_row = 0;
        promotion_state.square_col = 0;
        xSemaphoreGiveRecursive(promotion_mutex);
      }
    }
    board[tr][tc] = m.captured_piece;
    board[fr][fc] = m.piece;
    break;
  }
  default:
    board[tr][tc] = m.captured_piece;
    board[fr][fc] = m.piece;
    break;
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    uint8_t rook_home_col = (tc > fc) ? 7u : 0u;
    uint8_t rook_castle_col =
        (tc > fc) ? (uint8_t)(tc - 1) : (uint8_t)(tc + 1);
    piece_t rook_piece =
        (m.piece == PIECE_WHITE_KING) ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
    if (board[fr][rook_castle_col] == rook_piece) {
      board[fr][rook_castle_col] = PIECE_EMPTY;
      board[fr][rook_home_col] = rook_piece;
    }
  }

  if (kind == MOVE_TYPE_CAPTURE) {
    undo_pop_last_capture_list(m.captured_piece, mover_white);
  }

  if (kind == MOVE_TYPE_CASTLE_KING || kind == MOVE_TYPE_CASTLE_QUEEN) {
    castling_state.in_progress = false;
  }

  history_index--;
  if (move_count > 0) {
    move_count--;
  }
  if (advantage_history_count > 0) {
    advantage_history_count--;
  }

  if (mover_white) {
    if (white_moves_count > 0) {
      white_moves_count--;
    }
  } else {
    if (black_moves_count > 0) {
      black_moves_count--;
    }
  }

  if (toggle_player_after) {
    current_player = (current_player == PLAYER_WHITE) ? PLAYER_BLACK
                                                      : PLAYER_WHITE;
  }

  if (history_index > 0) {
    chess_move_t *p = &move_history[history_index - 1];
    last_move_from_row = p->from_row;
    last_move_from_col = p->from_col;
    last_move_to_row = p->to_row;
    last_move_to_col = p->to_col;
    has_last_move = true;
  } else {
    has_last_move = false;
  }

  if (current_game_state == GAME_STATE_FINISHED) {
    current_game_state = GAME_STATE_ACTIVE;
    game_active = true;
  }

  piece_lifted = false;
  lifted_piece_row = 0;
  lifted_piece_col = 0;
  lifted_piece = PIECE_EMPTY;

  game_end_timer_move();
  game_start_timer_move(current_player == PLAYER_WHITE);

  game_matrix_guard_check_resync_after_restore();
  if (!game_is_matrix_guard_active()) {
    led_clear_board_only();
    game_highlight_movable_pieces();
  } else {
    STAGING_LOGI(TAG,
                 "undo: matrix guard active — highlight squares until physical "
                 "board matches logic (conflicts=%u)",
                 (unsigned)game_get_matrix_guard_conflict_count());
  }

  game_snapshot_persist_after_valid_move();
  game_bump_revision_and_notify();

  ESP_LOGI(TAG, "Undo applied (kind=%d), side to move: %s", (int)kind,
           current_player == PLAYER_WHITE ? "White" : "Black");
  return true;
}

void game_process_commands(void) {
  // AKTUALIZOVAT non-blocking blink
  game_update_error_blink();

  // Update resignation visuals + timeout (safe task context)
  resignation_tick();

  // STABILITY FIX: Process ALL commands in queue (not just one) with limit
  // to prevent watchdog timeout
  if (game_command_queue != NULL) {
    chess_move_command_t chess_cmd;
    uint32_t commands_processed = 0;
    const uint32_t MAX_COMMANDS_PER_CYCLE =
        15; // Limit to prevent watchdog timeout

    // Process all commands in queue (up to limit)
    while (xQueueReceive(game_command_queue, &chess_cmd, 0) == pdTRUE) {
      commands_processed++;
      ESP_LOGI(TAG, "📥 Received command type: %d (processed: %lu/%lu)",
               chess_cmd.type, commands_processed, MAX_COMMANDS_PER_CYCLE);

      // Report activity to HA light task (except read-only commands)
      bool is_readonly = (chess_cmd.type == GAME_CMD_GET_STATUS ||
                          chess_cmd.type == GAME_CMD_GET_BOARD ||
                          chess_cmd.type == GAME_CMD_GET_VALID_MOVES);
      if (!is_readonly) {
        ha_light_report_activity("command");
      }

      switch (chess_cmd.type) {
      case GAME_CMD_NEW_GAME: // 0
        ESP_LOGI(TAG, "Processing NEW GAME command from UART");
        game_start_new_game();
        game_send_response_to_uart("New game started successfully!", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_NEW_GAME_FROM_FEN:
        ESP_LOGI(TAG, "Processing NEW_GAME_FROM_FEN");
        game_start_new_game_from_fen(chess_cmd.timer_data.fen_new_game.fen);
        game_send_response_to_uart("New game from FEN started", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_RESET_GAME: // 1
        ESP_LOGI(TAG, "Processing RESET GAME command from UART");
        game_reset_game();
        game_send_response_to_uart("Game reset to starting position!", false,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_MAKE_MOVE: // 2
        ESP_LOGI(TAG, "Processing MAKE MOVE command from UART: %s -> %s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_chess_move(&chess_cmd);
        break;

      case GAME_CMD_UNDO_MOVE: // 3
        ESP_LOGI(TAG, "Processing UNDO MOVE command from UART");
        if (game_undo_last_move_impl()) {
          game_send_response_to_uart("Last move undone", false,
                                     (QueueHandle_t)chess_cmd.response_queue);
        } else {
          game_send_response_to_uart("Nothing to undo", true,
                                     (QueueHandle_t)chess_cmd.response_queue);
        }
        break;

      case GAME_CMD_GET_STATUS: // 4
        ESP_LOGI(TAG, "Processing GET STATUS command from UART");
        game_print_status();
        break;

      case GAME_CMD_GET_BOARD: // 5
        ESP_LOGI(TAG, "Processing GET BOARD command from UART");
        game_send_board_to_uart((QueueHandle_t)chess_cmd.response_queue);
        break;

      case GAME_CMD_GET_VALID_MOVES: // 6
        ESP_LOGI(TAG, "Processing GET VALID MOVES command from UART");
        // TODO: Implement get valid moves
        break;

      case 7: // GAME_CMD_PICKUP_PIECE
        ESP_LOGI(TAG, "Processing PICKUP PIECE command from UART: %s",
                 chess_cmd.from_notation);
        game_process_pickup_command(&chess_cmd);
        break;

      case 8: // GAME_CMD_DROP_PIECE
        ESP_LOGI(TAG, "Processing DROP PIECE command from UART: %s",
                 chess_cmd.to_notation);
        game_process_drop_command(&chess_cmd);
        break;

      case 9: // GAME_CMD_PROMOTION
        ESP_LOGI(TAG, "Processing PROMOTION command from UART");
        game_process_promotion_command(&chess_cmd);
        break;

      case 10: // GAME_CMD_MOVE
        ESP_LOGI(TAG, "Processing MOVE command from UART: %s -> %s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_chess_move(&chess_cmd);
        break;

      case 11: // GAME_CMD_SHOW_BOARD
        ESP_LOGI(TAG, "Processing SHOW BOARD command from UART");
        game_send_board_to_uart((QueueHandle_t)chess_cmd.response_queue);
        break;

      case 12: // GAME_CMD_PICKUP
        ESP_LOGI(TAG, "Processing PICKUP command from UART: %s",
                 chess_cmd.from_notation);
        game_process_pickup_command(&chess_cmd);
        break;

      case 13: // GAME_CMD_DROP
        ESP_LOGI(TAG, "Processing DROP command from UART: %s",
                 chess_cmd.to_notation);
        game_process_drop_command(&chess_cmd);
        break;

      case 17: // GAME_CMD_EVALUATE
        ESP_LOGI(TAG, "Processing EVALUATE command from UART");
        game_process_evaluate_command(&chess_cmd);
        break;

      case 18: // GAME_CMD_SAVE
        ESP_LOGI(TAG, "Processing SAVE command from UART: %s",
                 chess_cmd.from_notation);
        game_process_save_command(&chess_cmd);
        break;

      case 19: // GAME_CMD_LOAD
        ESP_LOGI(TAG, "Processing LOAD command from UART: %s",
                 chess_cmd.from_notation);
        game_process_load_command(&chess_cmd);
        break;

      case 20: // GAME_CMD_RESERVED_SLOT_20
      case 29: // GAME_CMD_RESERVED_SLOT_29
      case 30: // GAME_CMD_RESERVED_SLOT_30
      case 31: // GAME_CMD_RESERVED_SLOT_31
      case 32: // GAME_CMD_RESERVED_SLOT_32
      case 38: // GAME_CMD_RESERVED_SLOT_38
        ESP_LOGW(TAG, "Ignored legacy puzzle command: %d", chess_cmd.type);
        game_send_response_to_uart("❌ Puzzle system neni dostupny", true,
                                   (QueueHandle_t)chess_cmd.response_queue);
        break;

      case 21: // GAME_CMD_CASTLE
        ESP_LOGI(TAG, "Processing CASTLE command from UART: %s",
                 chess_cmd.to_notation);
        game_process_castle_command(&chess_cmd);
        break;

      case 22: // GAME_CMD_PROMOTE
        ESP_LOGI(TAG, "Processing PROMOTE command from UART: %s=%s",
                 chess_cmd.from_notation, chess_cmd.to_notation);
        game_process_promote_command(&chess_cmd);
        break;

      case 23: // GAME_CMD_COMPONENT_OFF
        ESP_LOGI(TAG, "Processing COMPONENT_OFF command from UART");
        game_process_component_off_command(&chess_cmd);
        break;

      case 24: // GAME_CMD_COMPONENT_ON
        ESP_LOGI(TAG, "Processing COMPONENT_ON command from UART");
        game_process_component_on_command(&chess_cmd);
        break;

      case 25: // GAME_CMD_ENDGAME_WHITE
        ESP_LOGI(TAG, "Processing ENDGAME_WHITE command from UART");
        game_process_endgame_white_command(&chess_cmd);
        break;

      case 26: // GAME_CMD_ENDGAME_BLACK
        ESP_LOGI(TAG, "Processing ENDGAME_BLACK command from UART");
        game_process_endgame_black_command(&chess_cmd);
        break;

      case 27: // GAME_CMD_LIST_GAMES
        ESP_LOGI(TAG, "Processing LIST_GAMES command from UART");
        game_process_list_games_command(&chess_cmd);
        break;

      case 28: // GAME_CMD_DELETE_GAME
        ESP_LOGI(TAG, "Processing DELETE_GAME command from UART: %s",
                 chess_cmd.from_notation);
        game_process_delete_game_command(&chess_cmd);
        break;

      case 33: // GAME_CMD_TEST_MOVE_ANIM
        ESP_LOGI(TAG, "Processing TEST_MOVE_ANIM command from UART");
        game_test_move_animation();
        break;

      case 34: // GAME_CMD_TEST_PLAYER_ANIM
        ESP_LOGI(TAG, "Processing TEST_PLAYER_ANIM command from UART");
        game_test_player_change_animation();
        break;

      case 35: // GAME_CMD_TEST_CASTLE_ANIM
        ESP_LOGI(TAG, "Processing TEST_CASTLE_ANIM command from UART");
        game_test_castle_animation();
        break;

      case 36: // GAME_CMD_TEST_PROMOTE_ANIM
        ESP_LOGI(TAG, "Processing TEST_PROMOTE_ANIM command from UART");
        game_test_promote_animation();
        break;

      case 37: // GAME_CMD_TEST_ENDGAME_ANIM
        ESP_LOGI(TAG, "Processing TEST_ENDGAME_ANIM command from UART");
        game_test_endgame_animation();
        break;

      // Timer System Commands
      case GAME_CMD_SET_TIME_CONTROL: // 39
        ESP_LOGI(TAG, "Processing SET_TIME_CONTROL command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_PAUSE_TIMER: // 40
        ESP_LOGI(TAG, "Processing PAUSE_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_RESUME_TIMER: // 41
        ESP_LOGI(TAG, "Processing RESUME_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_RESET_TIMER: // 42
        ESP_LOGI(TAG, "Processing RESET_TIMER command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_GET_TIMER_STATE: // 43
        ESP_LOGI(TAG, "Processing GET_TIMER_STATE command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_TIMER_TIMEOUT: // 44
        ESP_LOGI(TAG, "Processing TIMER_TIMEOUT command");
        game_process_timer_command(&chess_cmd);
        break;

      case GAME_CMD_MATRIX_GUARD: // 45
        ESP_LOGW(TAG, "Processing MATRIX_GUARD command");
        game_matrix_guard_handle_command(&chess_cmd);
        break;

      case GAME_CMD_BOARD_SETUP_TUTORIAL: // 46
        ESP_LOGI(TAG, "BOARD_SETUP_TUTORIAL action=%u",
                 (unsigned)chess_cmd.promotion_choice);
        switch (chess_cmd.promotion_choice) {
        case 0:
          game_enter_board_setup_tutorial();
          break;
        case 1:
          game_exit_board_setup_tutorial(false);
          break;
        default:
          ESP_LOGW(TAG, "BOARD_SETUP_TUTORIAL unknown action");
          break;
        }
        break;

      case GAME_CMD_PUZZLE: // 47
        ESP_LOGI(TAG, "PUZZLE action/id=%u", (unsigned)chess_cmd.promotion_choice);
        if (chess_cmd.promotion_choice == 0U) {
          game_puzzle_cancel();
        } else if (chess_cmd.promotion_choice >= 101U &&
                   chess_cmd.promotion_choice <= 105U) {
          uint8_t prep_id =
              (uint8_t)(chess_cmd.promotion_choice - 100U);
          if (!game_puzzle_enter_setup(prep_id)) {
            ESP_LOGW(TAG, "PUZZLE prepare failed id=%u", (unsigned)prep_id);
          }
        } else if (chess_cmd.promotion_choice >= 1U &&
                   chess_cmd.promotion_choice <= 5U) {
          if (!game_puzzle_start(chess_cmd.promotion_choice)) {
            ESP_LOGW(TAG, "PUZZLE start failed for id=%u",
                     (unsigned)chess_cmd.promotion_choice);
          }
        } else {
          ESP_LOGW(TAG, "PUZZLE unknown promotion_choice=%u",
                   (unsigned)chess_cmd.promotion_choice);
        }
        break;

      case GAME_CMD_OPENING_TRAINER: // 49
        ESP_LOGI(TAG, "OPENING_TRAINER action=%u",
                 (unsigned)chess_cmd.promotion_choice);
        switch (chess_cmd.promotion_choice) {
        case 0:
          game_opening_cancel();
          break;
        case 1:
          if (!game_opening_start()) {
            ESP_LOGW(TAG, "OPENING start failed");
          }
          break;
        case 2:
          game_opening_hint();
          break;
        case 3:
          if (!game_opening_checkpoint_ack()) {
            ESP_LOGW(TAG, "OPENING checkpoint_ack failed");
          }
          break;
        default:
          ESP_LOGW(TAG, "OPENING unknown action");
          break;
        }
        break;

      default:
        ESP_LOGW(TAG, "Unknown game command: %d", chess_cmd.type);
        break;
      }

      // STABILITY FIX: Check limit to prevent watchdog timeout
      if (commands_processed >= MAX_COMMANDS_PER_CYCLE) {
        ESP_LOGW(TAG,
                 "Reached max commands per cycle (%lu), processing rest in "
                 "next cycle",
                 MAX_COMMANDS_PER_CYCLE);
        break; // Process remaining commands in next cycle
      }
    }

    // Log if we processed multiple commands
    if (commands_processed > 1) {
      ESP_LOGI(TAG, "✅ Processed %lu commands in this cycle",
               commands_processed);
    }
  }

  // ============================================================================
  // BUTTON EVENT PROCESSING - Physical button support
  // ============================================================================

  // Process button events from button_task
  if (button_event_queue != NULL) {
    button_event_t button_event;
    while (xQueueReceive(button_event_queue, &button_event, 0) == pdTRUE) {

      // Only process PRESS events (ignore release, long press, double press)
      if (button_event.type == BUTTON_EVENT_PRESS) {

        ESP_LOGI(TAG, "🎮 Button pressed: ID=%d", button_event.button_id);

        // Reset button (ID 8)
        if (button_event.button_id == 8) {
          ESP_LOGI(TAG, "🔄 Reset button pressed - resetting game");
          game_reset_game();
          game_check_promotion_needed(); // Update LED indications
          continue;
        }

        // Promotion buttons (ID 0-3) - physical buttons
        if (button_event.button_id <= 3) {
          ESP_LOGI(TAG, "👑 Promotion button %d pressed",
                   button_event.button_id);
          game_process_promotion_button(button_event.button_id);
          continue;
        }

        // Unknown button
        ESP_LOGW(TAG, "⚠️  Unknown button ID: %d", button_event.button_id);
      }
    }
  }

  // Keep promo square visually anchored (does not clear board)
  game_update_promotion_anchor_led();
}
