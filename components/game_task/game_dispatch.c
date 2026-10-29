/**
 * @file game_dispatch.c
 * @brief Command queue dispatch, undo, and UART board streaming.
 */

#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_gesture.h"
#include "game_physical_undo.h"
#include "game_squares_guard.h"
#include "game_policy.h"
#include "game_snapshot.h"
#include "freertos_chess.h"
#include "game_led_direct.h"

#include "../button_task/include/button_task.h"
#include "../button_task/include/button_gesture.h"
#include "../ha_light_task/include/ha_light_task.h"
#include "../freertos_chess/include/streaming_output.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

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
      piece_t piece = game_get_piece(row, col);
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
      .command_type = 0,
      .error_code = 0,
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

bool game_submit(const game_cmd_t *cmd) {
  if (cmd == NULL || game_command_queue == NULL) {
    return false;
  }
  return xQueueSend(game_command_queue, cmd, pdMS_TO_TICKS(100)) == pdTRUE;
}

/*
 * Command queue carries game_cmd_t. Only game_submit() calls xQueueSend.
 *
 * kind | sender | receiver
 * GCMD_NEW_GAME          main.c initialize_chess_game (skip if snapshot or boot
 *                        already started), execute_demo_move; game_task.c auto
 *                        start; web/BLE new | game_start_new_game
 * GCMD_NEW_GAME_FEN      web/BLE new + fen | game_start_new_game_from_fen
 * GCMD_UNDO              BLE cmd undo (no HTTP undo route) | game_undo_rewind_last_move
 * GCMD_MOVE              web_game_enqueue_move; BLE move | game_process_chess_move
 * GCMD_BOARD_LIFT        squares pickup; virtual pickup; demo | game_process_pickup_command
 * GCMD_BOARD_DROP        reed drop (from and to); Hall place (empty from);
 *                        virtual drop; demo | game_process_drop_command
 * GCMD_BOARD_GUARD       squares_send_guard_command (four masks and action) |
 *                        game_squares_guard_handle_command
 * GCMD_RESIGN            web_resign_dispatch | game_resign_from_app
 * GCMD_AGREE_DRAW        web_resign_dispatch draw | game_agree_draw
 * GCMD_TIMER_SET         HTTP/BLE timer config | game_process_timer_command
 * GCMD_TIMER_PAUSE       HTTP/BLE timer pause | game_process_timer_command
 * GCMD_TIMER_RESUME      HTTP/BLE timer resume | game_process_timer_command
 * GCMD_TIMER_RESET       HTTP/BLE timer reset | game_process_timer_command
 * GCMD_MODE_PUZZLE       web puzzle parser (210/211 -> apply/start staged) | puzzle acts
 * GCMD_MODE_OPENING      web_opening_dispatch cancel/start | opening acts
 * GCMD_MODE_TUTORIAL     tutorial start/cancel/finish | tutorial functions
 * GCMD_PROMOTION_CHOICE  virtual promote HTTP/BLE | game_process_promotion_command
 *
 * Not this queue:
 * - button_event_queue in this function: id 0-3 -> game_process_promotion_button;
 *   id 8 -> game_reset_game
 * - timer expiry: game_update_timer_display in the game_task loop
 * - uart_task does not enqueue; BLE calls web_server_ble_command_dispatch
 * - status, board, history: game_get_*_json
 */
void game_process_commands(void) {
  game_led_flush();
  game_physical_undo_tick();
  resignation_tick();

  if (game_command_queue != NULL) {
    game_cmd_t cmd;
    uint32_t commands_processed = 0;
    const uint32_t MAX_COMMANDS_PER_CYCLE = 15;
    TickType_t wait = pdMS_TO_TICKS(25);

    while (xQueueReceive(game_command_queue, &cmd, wait) == pdTRUE) {
      wait = 0;
      commands_processed++;
      ESP_LOGI(TAG, "Received command kind: %d (processed: %lu/%lu)",
               (int)cmd.kind, commands_processed, MAX_COMMANDS_PER_CYCLE);
      ha_light_report_activity("command");

      switch (cmd.kind) {
      case GCMD_NEW_GAME:
        ESP_LOGI(TAG, "Processing NEW GAME");
        game_start_new_game();
        if (current_game_state == GAME_STATE_WAITING_FOR_BOARD_SETUP) {
          game_cmd_reply("Waiting for pieces on start squares",
                                     false, cmd.response_queue);
        } else {
          game_cmd_reply("New game started, white to move", false,
                                     cmd.response_queue);
        }
        break;

      case GCMD_NEW_GAME_FEN:
        ESP_LOGI(TAG, "Processing NEW_GAME_FEN");
        if (!game_start_new_game_from_fen(cmd.u.fen.fen)) {
          game_cmd_reply("FEN rejected", true, cmd.response_queue);
        } else if (current_player == PLAYER_BLACK) {
          game_cmd_reply("New game from FEN, black to move", false,
                                     cmd.response_queue);
        } else {
          game_cmd_reply("New game from FEN, white to move", false,
                                     cmd.response_queue);
        }
        break;

      case GCMD_UNDO:
        ESP_LOGI(TAG, "Processing UNDO");
        if (game_undo_rewind_last_move()) {
          game_cmd_reply("Last move undone", false,
                                     cmd.response_queue);
        } else {
          game_cmd_reply("Nothing to undo", true, cmd.response_queue);
        }
        break;

      case GCMD_MOVE:
        ESP_LOGI(TAG, "Processing MOVE: %s -> %s", game_cmd_from(&cmd),
                 game_cmd_to(&cmd));
        game_process_chess_move(&cmd);
        break;

      case GCMD_BOARD_LIFT:
        ESP_LOGI(TAG, "Processing BOARD_LIFT: %s", game_cmd_from(&cmd));
        game_process_pickup_command(&cmd);
        break;

      case GCMD_BOARD_DROP:
        ESP_LOGI(TAG, "Processing BOARD_DROP: %s", game_cmd_to(&cmd));
        game_process_drop_command(&cmd);
        break;

      case GCMD_BOARD_GUARD:
        ESP_LOGW(TAG, "Processing BOARD_GUARD");
        game_squares_guard_handle_command(&cmd);
        break;

      case GCMD_RESIGN:
        ESP_LOGI(TAG, "Processing RESIGN (side=%u)",
                 (unsigned)cmd.u.resign.side);
        if (!game_resign_from_app(cmd.u.resign.side)) {
          game_cmd_reply("Resign refused", true, cmd.response_queue);
        }
        break;

      case GCMD_AGREE_DRAW:
        ESP_LOGI(TAG, "Processing DRAW");
        if (!game_agree_draw()) {
          game_cmd_reply("Draw refused", true, cmd.response_queue);
        } else {
          game_cmd_reply("Draw agreed", false, cmd.response_queue);
        }
        break;

      case GCMD_TIMER_SET:
      case GCMD_TIMER_PAUSE:
      case GCMD_TIMER_RESUME:
      case GCMD_TIMER_RESET:
        ESP_LOGI(TAG, "Processing timer kind %d", (int)cmd.kind);
        game_process_timer_command(&cmd);
        break;

      case GCMD_MODE_TUTORIAL:
        ESP_LOGI(TAG, "TUTORIAL act=%u", (unsigned)cmd.u.mode.act);
        switch (cmd.u.mode.act) {
        case TUTORIAL_ACT_START:
          game_enter_board_setup_tutorial();
          break;
        case TUTORIAL_ACT_CANCEL:
          game_exit_board_setup_tutorial(false);
          break;
        case TUTORIAL_ACT_FINISH:
          if (!game_finish_board_setup_tutorial_from_web()) {
            game_cmd_reply("Tutorial finish refused", true,
                                       cmd.response_queue);
          } else {
            game_cmd_reply("New game started", false,
                                       cmd.response_queue);
          }
          break;
        default:
          ESP_LOGW(TAG, "TUTORIAL unknown action");
          break;
        }
        break;

      case GCMD_MODE_PUZZLE: {
        uint8_t act = cmd.u.mode.act;
        uint8_t id = cmd.u.mode.builtin_id;
        ESP_LOGI(TAG, "PUZZLE act=%u id=%u", (unsigned)act, (unsigned)id);
        if (act == PUZZLE_ACT_CANCEL) {
          game_puzzle_cancel();
        } else if (act == PUZZLE_ACT_APPLY_SETUP) {
          if (!game_puzzle_apply_staged_setup()) {
            ESP_LOGW(TAG, "PUZZLE apply setup failed (no staged line)");
          }
        } else if (act == PUZZLE_ACT_START_STAGED) {
          if (!game_puzzle_try_start_staged()) {
            ESP_LOGW(TAG, "PUZZLE staged start refused");
          }
        } else if (act == PUZZLE_ACT_PREPARE) {
          if (!game_puzzle_enter_setup(id)) {
            ESP_LOGW(TAG, "PUZZLE prepare failed id=%u", (unsigned)id);
          }
        } else if (act == PUZZLE_ACT_START) {
          if (!game_puzzle_start(id)) {
            ESP_LOGW(TAG, "PUZZLE start failed for id=%u", (unsigned)id);
          }
        } else {
          ESP_LOGW(TAG, "PUZZLE unknown act=%u", (unsigned)act);
        }
        break;
      }

      case GCMD_MODE_OPENING:
        ESP_LOGI(TAG, "OPENING act=%u", (unsigned)cmd.u.mode.act);
        switch (cmd.u.mode.act) {
        case OPENING_ACT_CANCEL:
          game_opening_cancel();
          break;
        case OPENING_ACT_START:
          if (!game_opening_start()) {
            ESP_LOGW(TAG, "OPENING start failed");
          }
          break;
        case OPENING_ACT_HINT:
          game_opening_hint();
          break;
        case OPENING_ACT_CHECKPOINT:
          if (!game_opening_checkpoint_ack()) {
            ESP_LOGW(TAG, "OPENING checkpoint_ack failed");
          }
          break;
        default:
          ESP_LOGW(TAG, "OPENING unknown action");
          break;
        }
        break;

      case GCMD_PROMOTION_CHOICE:
        ESP_LOGI(TAG, "Processing PROMOTION_CHOICE");
        game_process_promotion_command(&cmd);
        break;

      case GCMD_WEB_HINT:
        game_process_web_hint(&cmd);
        break;

      default:
        ESP_LOGW(TAG, "Unknown game command kind: %d", (int)cmd.kind);
        break;
      }

      game_session_snap_refresh();

      if (commands_processed >= MAX_COMMANDS_PER_CYCLE) {
        ESP_LOGW(TAG,
                 "Reached max commands per cycle (%lu), processing rest in "
                 "next cycle",
                 MAX_COMMANDS_PER_CYCLE);
        break;
      }
    }

    if (commands_processed > 1) {
      ESP_LOGI(TAG, "Processed %lu commands in this cycle", commands_processed);
    }
  } else {
    game_led_flush();
    game_led_flush(); vTaskDelay(pdMS_TO_TICKS(25));
  }
  game_led_flush();

  if (button_event_queue != NULL) {
    button_event_t button_event;
    while (xQueueReceive(button_event_queue, &button_event, 0) == pdTRUE) {
      if (button_event.type == BUTTON_EVENT_PRESS) {
        ESP_LOGI(TAG, "Button pressed: ID=%d", button_event.button_id);
        if (button_event.button_id == 8) {
          ESP_LOGI(TAG, "Reset button pressed - resetting game");
          game_reset_game();
          game_check_promotion_needed();
          continue;
        }
        if (button_event.button_id <= 3) {
          ESP_LOGI(TAG, "Promotion button %d pressed", button_event.button_id);
          game_process_promotion_button(button_event.button_id);
          continue;
        }
        ESP_LOGW(TAG, "Unknown button ID: %d", button_event.button_id);
      }
    }
  }

  game_update_promotion_anchor_led();
  game_gesture_poll();
}
