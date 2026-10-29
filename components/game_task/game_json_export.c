/**
 * @file game_json_export.c
 * @brief JSON serialization of game state for web server and UART APIs.
 */

#include "game_task.h"
#include "game_board_core.h"
#include "game_board_machine.h"
#include "game_snapshot.h"
#include "game_squares_guard.h"
#include "game_snapshot.h"
#include "position.h"
#include "game_policy.h"
#include "freertos_chess.h"

#include "../squares_task/include/squares_task.h"
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

static StaticSemaphore_t s_export_lock_buf;
static SemaphoreHandle_t s_export_lock;
static portMUX_TYPE s_export_init_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t export_snap_take(void) {
  if (s_export_lock == NULL) {
    taskENTER_CRITICAL(&s_export_init_mux);
    if (s_export_lock == NULL) {
      s_export_lock = xSemaphoreCreateMutexStatic(&s_export_lock_buf);
    }
    taskEXIT_CRITICAL(&s_export_init_mux);
  }
  if (s_export_lock == NULL) {
    return ESP_ERR_NO_MEM;
  }
  if (xSemaphoreTake(s_export_lock, pdMS_TO_TICKS(2000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}

static void export_snap_give(void) {
  if (s_export_lock != NULL) {
    xSemaphoreGive(s_export_lock);
  }
}

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

  esp_err_t lock_ret = export_snap_take();
  if (lock_ret != ESP_OK) {
    return lock_ret;
  }
  const game_session_snap_t *snap = game_session_snap_acquire();
  if (snap == NULL) {
    export_snap_give();
    return ESP_ERR_TIMEOUT;
  }

  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"board\":[");

  for (int row = 0; row < 8; row++) {
    offset += snprintf(buffer + offset, size - offset, "[");
    for (int col = 0; col < 8; col++) {
      char piece_char =
          piece_to_char((piece_t)snap->pos.board[row][col]);
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

  game_session_snap_release();
  export_snap_give();
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

static esp_err_t game_status_json_append(size_t *offset, char *buffer, size_t size,
                                        const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  if (*offset >= size) {
    va_end(ap);
    ESP_LOGE(TAG, "game_get_status_json: offset invalid %zu", *offset);
    return ESP_ERR_NO_MEM;
  }
  size_t rem = size - *offset;
  if (rem <= 1) {
    va_end(ap);
    return ESP_ERR_NO_MEM;
  }
  int n = vsnprintf(buffer + *offset, rem, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= rem) {
    ESP_LOGE(TAG,
             "game_get_status_json: append overflow need=%d rem=%zu off=%zu", n,
             rem, *offset);
    return ESP_ERR_NO_MEM;
  }
  *offset += (size_t)n;
  return ESP_OK;
}

static void game_json_escape(char *dst, size_t dst_len, const char *src) {
  size_t j = 0;
  if (dst == NULL || dst_len == 0) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }
  for (size_t i = 0; src[i] != '\0' && j + 2 < dst_len; i++) {
    unsigned char c = (unsigned char)src[i];
    if (c < 0x20) {
      continue;
    }
    if (c == '"' || c == '\\') {
      dst[j++] = '\\';
    }
    dst[j++] = (char)c;
  }
  dst[j] = '\0';
}

#define GAME_STATUS_JSON_APPEND(...)                                           \
  do {                                                                         \
    if (game_status_json_append(&offset, buffer, size, __VA_ARGS__) !=         \
        ESP_OK) {                                                              \
      if (game_mutex != NULL)                                                  \
        xSemaphoreGive(game_mutex);                                            \
      game_session_snap_release();                                             \
      export_snap_give();                                                      \
      return ESP_ERR_NO_MEM;                                                   \
    }                                                                          \
  } while (0)

esp_err_t game_get_status_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  position_status_t pstat;
  game_state_t snap_state;

  esp_err_t lock_ret = export_snap_take();
  if (lock_ret != ESP_OK) {
    return lock_ret;
  }
  const game_session_snap_t *snap = game_session_snap_acquire();
  if (snap == NULL) {
    export_snap_give();
    return ESP_ERR_TIMEOUT;
  }
  pstat = snap->pos_status;
  snap_state = (game_state_t)snap->game_state;

  if (game_mutex != NULL) {
    if (xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
      game_session_snap_release();
      export_snap_give();
      return ESP_ERR_TIMEOUT;
    }
  }

  size_t offset = 0;

  const char *game_state_str = "idle";
  switch (snap_state) {
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

  bool in_check = pstat.in_check;
  bool checkmate = pstat.checkmate;
  bool stalemate = pstat.stalemate;

  GAME_STATUS_JSON_APPEND("{\"game_state\":\"%s\","
                          "\"current_player\":\"%s\","
                          "\"move_count\":%" PRIu32 ","
                          "\"white_time\":%" PRIu32 ","
                          "\"black_time\":%" PRIu32 ","
                          "\"in_check\":%s,"
                          "\"checkmate\":%s,"
                          "\"stalemate\":%s",
                          game_state_str,
                          (snap->pos.side == PLAYER_WHITE) ? "White" : "Black",
                          snap->move_count, snap->white_time_total,
                          snap->black_time_total, in_check ? "true" : "false",
                          checkmate ? "true" : "false",
                          stalemate ? "true" : "false");

  // Piece lifted info
  if (piece_lifted) {
    char piece_char = piece_to_char(lifted_piece);
    char notation[4] = {0};
    game_coords_to_notation(lifted_piece_row, lifted_piece_col, notation);
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

  /* Casting for 2 turns: the web must know to wait for the rook's turn (the board is an intermediate state). */
  if (castling_state.in_progress) {
    char cf[4] = {0}, ct[4] = {0};
    game_coords_to_notation(castling_state.rook_from_row,
                              castling_state.rook_from_col, cf);
    game_coords_to_notation(castling_state.rook_to_row,
                              castling_state.rook_to_col, ct);
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":true,"
                            "\"castling_from\":\"%s\","
                            "\"castling_to\":\"%s\"",
                            cf, ct);
  } else {
    GAME_STATUS_JSON_APPEND(",\"castling_in_progress\":false");
  }

  // Game end information - use (*game_state_current_endgame_reason()) for exact
  // distinction
  if (snap_state == GAME_STATE_FINISHED) {
    const char *end_reason = "Unknown";
    const char *winner = "None";
    const char *loser = "None";

    // First determine the winner
    if ((*game_state_current_result_type()) == RESULT_WHITE_WINS) {
      winner = "White";
      loser = "Black";
    } else if ((*game_state_current_result_type()) == RESULT_BLACK_WINS) {
      winner = "Black";
      loser = "White";
    } else {
      winner = "Draw";
      loser = "Draw";
    }

    // Then determine the reason by (*game_state_current_endgame_reason())
    switch ((*game_state_current_endgame_reason())) {
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
    case ENDGAME_REASON_AGREEMENT:
      end_reason = "Agreement";
      break;
    }

    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":true,\"reason\":\"%s\","
                            "\"winner\":\"%s\",\"loser\":\"%s\"}",
                            end_reason, winner, loser);
  } else {
    GAME_STATUS_JSON_APPEND(",\"game_end\":{\"ended\":false,\"reason\":\"\","
                            "\"winner\":\"\",\"loser\":\"\"}");
  }

  // Error recovery state for visual indication on the website
  if ((*game_state_error_recovery_state()).waiting_for_move_correction) {
    char invalid_notation[4] = {0};
    char original_notation[4] = {0};
    game_coords_to_notation((*game_state_error_recovery_state()).invalid_row,
                               (*game_state_error_recovery_state()).invalid_col,
                               invalid_notation);
    game_coords_to_notation((*game_state_error_recovery_state()).original_valid_row,
                               (*game_state_error_recovery_state()).original_valid_col,
                               original_notation);

    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":true,"
                            "\"invalid_pos\":\"%s\","
                            "\"original_pos\":\"%s\","
                            "\"error_count\":%d}",
                            invalid_notation, original_notation,
                            (*game_state_error_recovery_state()).error_count);
  } else {
    /* Close error_state object — previously missing } was breaking JSON (restore_state
     * nested in error_state). */
    GAME_STATUS_JSON_APPEND(",\"error_state\":{\"active\":false}");
  }

  GAME_STATUS_JSON_APPEND(",\"gameplay_profile\":\"%s\"",
                          game_policy_profile_name());

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
                            (*game_state_board_setup_tutorial_active()) ? "true" : "false");

  const game_puzzle_definition_t *pd_play =
      game_get_puzzle_definition((*game_state_puzzle_active_id()));
  const game_puzzle_definition_t *pd_setup =
      game_get_puzzle_definition((*game_state_puzzle_setup_id()));
  const game_puzzle_definition_t *pd =
      (*game_state_puzzle_active()) ? pd_play : ((*game_state_puzzle_setup_active()) ? pd_setup : NULL);
  bool puzzle_phys_match = game_puzzle_status_physical_match();
  const char *puzzle_fen_src = "";
  if (((*game_state_puzzle_setup_active()) || (*game_state_puzzle_active())) && pd != NULL && pd->fen != NULL) {
    puzzle_fen_src = pd->fen;
  } else if ((*game_state_puzzle_setup_active()) || (*game_state_puzzle_active())) {
    puzzle_fen_src = game_puzzle_staged_fen();
  }
  const char *puzzle_title_src =
      (pd != NULL && pd->title != NULL && pd->title[0] != '\0')
          ? pd->title
          : game_puzzle_title();
  char puzzle_fen_esc[160];
  char puzzle_title_esc[96];
  char puzzle_teaser_esc[160];
  char puzzle_msg_esc[200];
  game_json_escape(puzzle_fen_esc, sizeof(puzzle_fen_esc), puzzle_fen_src);
  game_json_escape(puzzle_title_esc, sizeof(puzzle_title_esc), puzzle_title_src);
  game_json_escape(puzzle_teaser_esc, sizeof(puzzle_teaser_esc),
                   pd ? pd->teaser : "");
  game_json_escape(puzzle_msg_esc, sizeof(puzzle_msg_esc),
                   game_puzzle_feedback_message());
  GAME_STATUS_JSON_APPEND(
      ",\"puzzle\":{\"active\":%s,\"setup_active\":%s,"
      "\"setup_id\":%u,\"physical_match\":%s,\"fen\":\"%s\","
      "\"id\":%u,\"difficulty\":%u,"
      "\"title\":\"%s\",\"teaser\":\"%s\",\"feedback\":\"%s\","
      "\"message\":\"%s\",\"ply_index\":%u,\"ply_total\":%u,"
      "\"expected_from\":\"%s\",\"expected_to\":\"%s\","
      "\"awaiting_opponent\":%s,\"last_uci\":\"%s\",\"session\":%u,"
      "\"solved\":%s,\"sync_hold\":%s}",
      (*game_state_puzzle_active()) ? "true" : "false",
      (*game_state_puzzle_setup_active()) ? "true" : "false", (unsigned)(*game_state_puzzle_setup_id()),
      puzzle_phys_match ? "true" : "false", puzzle_fen_esc,
      (unsigned)((*game_state_puzzle_active()) ? (*game_state_puzzle_active_id())
                               : ((*game_state_puzzle_setup_active()) ? (*game_state_puzzle_setup_id()) : 0)),
      (unsigned)(pd ? pd->difficulty : 0U), puzzle_title_esc, puzzle_teaser_esc,
      game_puzzle_feedback_key(), puzzle_msg_esc,
      (unsigned)game_puzzle_ply(), (unsigned)game_puzzle_ply_total(),
      game_puzzle_expected_from(), game_puzzle_expected_to(),
      game_puzzle_awaiting_opponent() ? "true" : "false",
      game_puzzle_last_uci(), (unsigned)game_puzzle_session_id(),
      game_puzzle_is_solved() ? "true" : "false",
      game_puzzle_sync_hold() ? "true" : "false");

  game_opening_export_status_json(buffer, size, &offset);

  if ((*game_state_board_setup_tutorial_active()) || (*game_state_puzzle_setup_active()) || (*game_state_puzzle_active()) ||
      game_opening_status_needs_squares()) {
    uint8_t occ[64];
    squares_get_state(occ);
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
    game_session_snap_release();
    export_snap_give();
    return ESP_FAIL;
  }

#ifndef NDEBUG
  STAGING_LOGI(TAG, "game_get_status_json: len=%d cap=%zu", offset, size);
#endif

  if (game_mutex != NULL) {
    xSemaphoreGive(game_mutex);
  }
  game_session_snap_release();
  export_snap_give();

  return ESP_OK;
}

esp_err_t game_get_history_json(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t lock_ret = export_snap_take();
  if (lock_ret != ESP_OK) {
    return lock_ret;
  }
  const game_session_snap_t *snap = game_session_snap_acquire();
  if (snap == NULL) {
    export_snap_give();
    return ESP_ERR_TIMEOUT;
  }

  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"moves\":[");

  for (uint32_t i = 0; i < snap->history_len && i < POSITION_HIST_CAP; i++) {
    char from_notation[4] = {0};
    char to_notation[4] = {0};
    game_coords_to_notation(snap->history_moves[i].from_row,
                               snap->history_moves[i].from_col, from_notation);
    game_coords_to_notation(snap->history_moves[i].to_row,
                               snap->history_moves[i].to_col, to_notation);
    char piece_char = piece_to_char(snap->history_moves[i].piece);

    offset += snprintf(buffer + offset, size - offset,
                       "{\"from\":\"%s\",\"to\":\"%s\",\"piece\":\"%c\","
                       "\"timestamp\":%" PRIu32 "}",
                       from_notation, to_notation, piece_char,
                       snap->history_moves[i].timestamp);

    if (i + 1 < snap->history_len) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "]}");

  game_session_snap_release();
  export_snap_give();
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
  for (uint32_t i = 0; i < (*game_state_white_captured_count()); i++) {
    char piece_char = piece_to_char((game_state_white_captured_pieces())[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < (*game_state_white_captured_count()) - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"black_captured\":[");

  // Add black captured pieces
  for (uint32_t i = 0; i < (*game_state_black_captured_count()); i++) {
    char piece_char = piece_to_char((game_state_black_captured_pieces())[i]);
    offset += snprintf(buffer + offset, size - offset, "\"%c\"", piece_char);
    if (i < (*game_state_black_captured_count()) - 1) {
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

  // Build JSON: {"history":[0,1,2,-1,0,...], "count":42, "(*game_state_white_checks())":5,
  // "(*game_state_black_checks())":3, ...}
  int offset = 0;
  offset += snprintf(buffer + offset, size - offset, "{\"history\":[");

  // Add advantage values
  for (uint32_t i = 0; i < (*game_state_advantage_history_count()); i++) {
    offset += snprintf(buffer + offset, size - offset, "%d",
                       (game_state_material_advantage_history())[i]);
    if (i < (*game_state_advantage_history_count()) - 1) {
      offset += snprintf(buffer + offset, size - offset, ",");
    }
  }

  offset += snprintf(buffer + offset, size - offset, "],\"count\":%" PRIu32,
                     (*game_state_advantage_history_count()));

  // Add more statistics
  offset += snprintf(buffer + offset, size - offset,
                     ",\"(*game_state_white_checks())\":%" PRIu32 ",\"(*game_state_black_checks())\":%" PRIu32,
                     (*game_state_white_checks()), (*game_state_black_checks()));
  offset +=
      snprintf(buffer + offset, size - offset,
               ",\"(*game_state_white_castles())\":%" PRIu32 ",\"(*game_state_black_castles())\":%" PRIu32,
               (*game_state_white_castles()), (*game_state_black_castles()));

  // Average time per move
  uint32_t current_time = esp_timer_get_time() / 1000;
  uint32_t game_duration =
      ((*game_state_game_start_time()) > 0) ? (current_time - (*game_state_game_start_time())) : 0;
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
