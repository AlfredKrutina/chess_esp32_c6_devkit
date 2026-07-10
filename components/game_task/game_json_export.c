/**
 * @file game_json_export.c
 * @brief JSON serialization of game state for web server and UART APIs.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "freertos_chess.h"

#include "../matrix_task/include/matrix_task.h"
#include "../../timer_system/include/timer_system.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "GAME_JSON";

#ifndef NDEBUG
#define STAGING_LOGI(tag, fmt, ...) ESP_LOGI(tag, "[STAGING] " fmt, ##__VA_ARGS__)
#else
#define STAGING_LOGI(tag, fmt, ...) ((void)0)
#endif

// ============================================================================
// WEB SERVER JSON EXPORT FUNCTIONS
// ============================================================================

esp_err_t game_get_board_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to board
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string manually (more efficient than cJSON)
  int offset = 0;

  offset += snprintf(buffer + offset, size - offset, "{\"board\":[");

  for (int row = 0; row < 8; row++) {
    offset += snprintf(buffer + offset, size - offset, "[");
    for (int col = 0; col < 8; col++) {
      char piece_char = piece_to_char(board[row][col]);
      offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
      if (col < 7) {
        offset += snprintf(buffer + offset, size - offset, ",");
      }
    }
    offset += snprintf(buffer + offset, size - offset, "]");
    if (row < 7) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  uint64_t timestamp = esp_timer_get_time() / 1000;
  offset += snprintf(buffer + offset, size - offset, "],\"timestamp\":%llu}",
                     timestamp);

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

/**
 * @brief Get current move count safely
 */
uint32_t game_get_move_count(void) {
  uint32_t count = 0;
  if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    count = move_count;
    xSemaphoreGive(game_mutex);
  }
  return count;
}

static esp_err_t game_status_json_append(int *offset, char *buffer, size_t size,
                                        const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (*offset < 0 || (size_t)*offset >= size) {
    va_end(ap);
    ESP_LOGE(TAG, "game_get_status_json: offset invalid %d", *offset);
    return ESP_ERR_NO_MEM;
  }
  size_t rem = size - (size_t)*offset;
  if (rem <= 1) {
    va_end(ap);
    return ESP_ERR_NO_MEM;
  }
  int n = vsnprintf(buffer + *offset, rem, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= rem) {
    ESP_LOGE(TAG,
             "game_get_status_json: append overflow need=%d rem=%zu off=%d", n,
             rem, *offset);
    return ESP_ERR_NO_MEM;
  }
  *offset += n;
  return ESP_OK;
}

#define GAME_STATUS_JSON_APPEND(...)                                           \
  do {                                                                         \
    if (game_status_json_append(&offset, buffer, size, __VA_ARGS__) !=         \
        ESP_OK) {                                                              \
      if (game_mutex != NULL)                                                  \
        xSemaphoreGive(game_mutex);                                            \
      return ESP_ERR_NO_MEM;                                                   \
    }                                                                          \
  } while (0)

esp_err_t game_get_status_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to game state
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;

  // Game state string
  const char *game_state_str = "idle";
  switch (current_game_state) {
  case GAME_STATE_ACTIVE:
    game_state_str = "active";
    break;
  case GAME_STATE_PAUSED:
    game_state_str = "paused";
    break;
  case GAME_STATE_FINISHED:
    game_state_str = "finished";
    break;
  case GAME_STATE_PROMOTION:
    game_state_str = "promotion";
    break;
  case GAME_STATE_WAITING_FOR_RETURN:
    game_state_str = "active";
    break;
  case GAME_STATE_PLAYING:
    game_state_str = "playing";
    break;
  default:
    game_state_str = "idle";
    break;
  }

  // Check detection
  bool in_check = game_is_king_in_check(current_player);

  // Checkmate detection (simplified - king in check and no legal moves)
  bool checkmate = false;
  if (in_check) {
    // Try to generate legal moves - if none, it's checkmate
    uint32_t legal_moves = game_generate_legal_moves(current_player);
    checkmate = (legal_moves == 0);
  }

  // Stalemate detection (simplified - not in check but no legal moves)
  bool stalemate = false;
  if (!in_check) {
    uint32_t legal_moves = game_generate_legal_moves(current_player);
    stalemate = (legal_moves == 0);
  }

  GAME_STATUS_JSON_APPEND("{\"game_state\":\"%s\","
                          "\"current_player\":\"%s\","
                          "\"move_count\":%" PRIu32 ","
                          "\"white_time\":%" PRIu32 ","
                          "\"black_time\":%" PRIu32 ","
                          "\"in_check\":%s,"
                          "\"checkmate\":%s,"
                          "\"stalemate\":%s",
                          game_state_str,
                          (current_player == PLAYER_WHITE) ? "White" : "Black",
                          move_count, white_time_total, black_time_total,
                          in_check ? "true" : "false", checkmate ? "true" : "false",
                          stalemate ? "true" : "false");

  // Piece lifted info
  if (piece_lifted) {
    char piece_char = piece_to_char(lifted_piece);
    char notation[4] = {0};
    convert_coords_to_notation(lifted_piece_row, lifted_piece_col, notation);
    GAME_STATUS_JSON_APPEND(",\"piece_lifted\":{\"lifted\":true,\"row\":%d,"
                            "\"col\":%d,"
                            "\"piece\":\"%c\",\"notation\":\"%s\"}",
                            lifted_piece_row, lifted_piece_col, piece_char,
                            notation);
  } else {
    GAME_STATUS_JSON_APPEND(",\"piece_lifted\":{\"lifted\":false,\"row\":0,"
                            "\"col\":"
                            "0,\"piece\":\" \",\"notation\":\"\"}");
  }

  /* Rošáda na 2 tahy: web musí vědět, že má čekat na tah věže (deska je mezistav). */
  if (castling_state.in_progress) {
    char cf[4] = {0}, ct[4] = {0};
    convert_coords_to_notation(castling_state.rook_from_row,
                              castling_state.rook_from_col, cf);
    convert_coords_to_notation(castling_state.rook_to_row,
                              castling_state.rook_to_col, ct);
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":true,"
                            "\"castling_from\":\"%s\","
                            "\"castling_to\":\"%s\"",
                            cf, ct);
  } else {
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":false");
  }

  // Game end information - použít current_endgame_reason pro přesné
  // rozlišení
  if (current_game_state == GAME_STATE_FINISHED) {
    const char *end_reason = "Unknown";
    const char *winner = "None";
    const char *loser = "None";

    // Nejprve určit vítěze
    if (current_result_type == RESULT_WHITE_WINS) {
      winner = "White";
      loser = "Black";
    } else if (current_result_type == RESULT_BLACK_WINS) {
      winner = "Black";
      loser = "White";
    } else {
      winner = "Draw";
      loser = "Draw";
    }

    // Pak určit důvod podle current_endgame_reason
    switch (current_endgame_reason) {
    case ENDGAME_REASON_CHECKMATE:
      end_reason = "Checkmate";
      break;
    case ENDGAME_REASON_CHECKMATE_EN_PASSANT:
      end_reason = "Checkmate (En Passant)";
      break;
    case ENDGAME_REASON_CHECKMATE_CASTLING:
      end_reason = "Checkmate (Castling)";
      break;
    case ENDGAME_REASON_CHECKMATE_PROMOTION:
      end_reason = "Checkmate (Promotion)";
      break;
    case ENDGAME_REASON_CHECKMATE_DISCOVERED:
      end_reason = "Checkmate (Discovered Check)";
      break;
    case ENDGAME_REASON_RESIGNATION:
      end_reason = "Resignation";
      break;
    case ENDGAME_REASON_TIMEOUT:
      end_reason = "Timeout";
      break;
    case ENDGAME_REASON_STALEMATE:
      end_reason = "Stalemate";
      break;
    case ENDGAME_REASON_50_MOVE:
      end_reason = "50-move rule";
      break;
    case ENDGAME_REASON_REPETITION:
      end_reason = "Threefold repetition";
      break;
    case ENDGAME_REASON_INSUFFICIENT:
      end_reason = "Insufficient material";
      break;
    }

    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":true,\"reason\":\"%s\","
                            "\"winner\":\"%s\",\"loser\":\"%s\"}",
                            end_reason, winner, loser);
  } else {
    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":false,\"reason\":\"\","
                            "\"winner\":\"\",\"loser\":\"\"}");
  }

  // Error recovery state pro vizuální indikaci na webu
  if (error_recovery_state.waiting_for_move_correction) {
    char invalid_notation[4] = {0};
    char original_notation[4] = {0};
    convert_coords_to_notation(error_recovery_state.invalid_row,
                               error_recovery_state.invalid_col,
                               invalid_notation);
    convert_coords_to_notation(error_recovery_state.original_valid_row,
                               error_recovery_state.original_valid_col,
                               original_notation);

    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":true,"
                            "\"invalid_pos\":\"%s\","
                            "\"original_pos\":\"%s\","
                            "\"error_count\":%d}",
                            invalid_notation, original_notation,
                            error_recovery_state.error_count);
  } else {
    /* Uzavřít objekt error_state — dříve chybějící } rozbíjelo JSON (restore_state
     * vnořený do error_state). */
    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":false}");
  }

  GAME_STATUS_JSON_APPEND(
      ",\"restore_state\":{\"snapshot_loaded\":%s,\"snapshot_fallback_used\":%s,"
      "\"snapshot_restore_failed\":%s,\"snapshot_save_failed\":%s,"
      "\"resync_required\":%s,"
      "\"boot_new_game_triggered\":%s}",
      game_was_snapshot_loaded_on_boot() ? "true" : "false",
      game_is_snapshot_fallback_used() ? "true" : "false",
      game_has_snapshot_restore_failure() ? "true" : "false",
      game_has_snapshot_save_failure() ? "true" : "false",
      resync_required_after_restore ? "true" : "false",
      game_was_boot_new_game_triggered() ? "true" : "false");

  GAME_STATUS_JSON_APPEND(",\"board_setup_tutorial\":%s",
                            board_setup_tutorial_active ? "true" : "false");

  const game_puzzle_definition_t *pd_play =
      game_get_puzzle_definition(puzzle_active_id);
  const game_puzzle_definition_t *pd_setup =
      game_get_puzzle_definition(puzzle_setup_id);
  const game_puzzle_definition_t *pd =
      puzzle_active ? pd_play : (puzzle_setup_active ? pd_setup : NULL);
  bool puzzle_phys_match = false;
  if (puzzle_setup_active && pd_setup != NULL) {
    puzzle_phys_match = game_puzzle_physical_matches_fen(pd_setup->fen);
  }
  const char *puzzle_fen_json = "";
  if (puzzle_setup_active && pd_setup != NULL) {
    puzzle_fen_json = pd_setup->fen;
  } else if (puzzle_active && pd_play != NULL) {
    puzzle_fen_json = pd_play->fen;
  }
  GAME_STATUS_JSON_APPEND(",\"puzzle\":{\"active\":%s,\"setup_active\":%s,"
                          "\"setup_id\":%u,\"physical_match\":%s,\"fen\":\"%s\","
                          "\"id\":%u,\"difficulty\":%u,"
                          "\"title\":\"%s\",\"teaser\":\"%s\",\"feedback\":\"%s\","
                          "\"message\":\"%s\"}",
                          puzzle_active ? "true" : "false",
                          puzzle_setup_active ? "true" : "false",
                          (unsigned)puzzle_setup_id,
                          puzzle_phys_match ? "true" : "false", puzzle_fen_json,
                          (unsigned)(puzzle_active ? puzzle_active_id
                                                   : (puzzle_setup_active
                                                          ? puzzle_setup_id
                                                          : 0)),
                          (unsigned)(pd ? pd->difficulty : 0U), pd ? pd->title : "",
                          pd ? pd->teaser : "", game_puzzle_feedback_key(),
                          game_puzzle_feedback_message());

  if (board_setup_tutorial_active || puzzle_setup_active) {
    uint8_t occ[64];
    matrix_get_state(occ);
    GAME_STATUS_JSON_APPEND(",\"matrix_occupied\":[");
    for (int i = 0; i < 64; i++) {
      GAME_STATUS_JSON_APPEND("%s%u", (i > 0) ? "," : "",
                              (unsigned int)occ[i]);
    }
    GAME_STATUS_JSON_APPEND("]");
  }

  GAME_STATUS_JSON_APPEND("}");

#undef GAME_STATUS_JSON_APPEND

  buffer[offset] = '\0';
  if (strstr(buffer, "\"game_state\"") == NULL) {
    ESP_LOGE(TAG, "game_get_status_json: missing game_state in output");
    if (game_mutex != NULL) {
      xSemaphoreGive(game_mutex);
    }
    return ESP_FAIL;
  }

#ifndef NDEBUG
  STAGING_LOGI(TAG, "game_get_status_json: len=%d cap=%zu", offset, size);
#endif

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

esp_err_t game_get_history_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to move history
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"moves\":[");

  for (uint32_t i = 0; i < history_index && i < GAME_TASK_MAX_MOVES_HISTORY; i++) {
    char from_notation[4] = {0};
    char to_notation[4] = {0};
    convert_coords_to_notation(move_history[i].from_row,
                               move_history[i].from_col, from_notation);
    convert_coords_to_notation(move_history[i].to_row, move_history[i].to_col,
                               to_notation);
    char piece_char = piece_to_char(move_history[i].piece);

    offset += snprintf(buffer + offset, size - offset,
                       "{\"from\":\"%s\",\"to\":\"%s\",\"piece\":\"%c\","
                       "\"timestamp\":%" PRIu32 "}",
                       from_notation, to_notation, piece_char,
                       move_history[i].timestamp);

    if (i < history_index - 1 && i < GAME_TASK_MAX_MOVES_HISTORY - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "]}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

esp_err_t game_get_captured_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access to captured pieces
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON string
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"white_captured\":[");

  // Add white captured pieces
  for (uint32_t i = 0; i < white_captured_count; i++) {
    char piece_char = piece_to_char(white_captured_pieces[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < white_captured_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"black_captured\":[");

  // Add black captured pieces
  for (uint32_t i = 0; i < black_captured_count; i++) {
    char piece_char = piece_to_char(black_captured_pieces[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < black_captured_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "]}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

/**
 * @brief Export material advantage history to JSON string (pro graf)
 * @param buffer Output buffer for JSON string
 * @param size Buffer size
 * @return ESP_OK on success, error code on failure
 */
esp_err_t game_get_advantage_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  // Thread-safe access
  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      return ESP_ERR_TIMEOUT;
    }
  }

  // Build JSON: {"history":[0,1,2,-1,0,...], "count":42, "white_checks":5,
  // "black_checks":3, ...}
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"history\":[");

  // Add advantage values
  for (uint32_t i = 0; i < advantage_history_count; i++) {
    offset += snprintf(buffer + offset, size - offset, "%d",
                       material_advantage_history[i]);
    if (i < advantage_history_count - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"count\":%" PRIu32,
                     advantage_history_count);

  // Přidat další statistiky
  offset += snprintf(buffer + offset, size - offset,
                     ",\"white_checks\":%" PRIu32 ",\"black_checks\":%" PRIu32,
                     white_checks, black_checks);
  offset +=
      snprintf(buffer + offset, size - offset,
               ",\"white_castles\":%" PRIu32 ",\"black_castles\":%" PRIu32,
               white_castles, black_castles);

  // Průměrný čas na tah
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      (game_start_time > 0) ? (current_time - game_start_time) : 0;
  uint32_t avg_time_per_move =
      (move_count > 0) ? (game_duration / move_count) : 0;
  offset += snprintf(buffer + offset, size - offset,
                     ",\"game_duration_ms\":%" PRIu32
                     ",\"avg_time_per_move_ms\":%" PRIu32,
                     game_duration, avg_time_per_move);

  offset += snprintf(buffer + offset, size - offset, "}");

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }

  return ESP_OK;
}

esp_err_t game_get_timer_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    ESP_LOGE(TAG, "Invalid buffer parameters");
    return ESP_ERR_INVALID_ARG;
  }

  return timer_get_json(buffer, size);
}
