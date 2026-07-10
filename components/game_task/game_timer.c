/**
 * @file game_timer.c
 * @brief Timer system integration wrappers and timeout handling.
 */

#include "game_task_internal.h"
#include "game_task.h"
#include "freertos_chess.h"

#include "../../timer_system/include/timer_system.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "GAME_TIMER";

// ============================================================================
// TIMER SYSTEM INTEGRATION IMPLEMENTATION
// ============================================================================


esp_err_t game_set_time_control(const time_control_config_t *config) {
  if (config == NULL) {
    ESP_LOGE(TAG, "Invalid config parameter");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = timer_set_time_control(config);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Time control set: %s", config->name);

    // Pokud je hra aktivní, spustit timer pro aktuálního hráče
    if (current_game_state == GAME_STATE_ACTIVE) {
      ESP_LOGI(TAG, "Game is active, starting timer for current player");
      timer_start_move(current_player == PLAYER_WHITE);
    } else {
      ESP_LOGI(TAG,
               "Game state is %d (not active), timer will start on first move",
               current_game_state);
    }
  }

  return ret;
}

esp_err_t game_start_timer_move(bool is_white_turn) {
  ESP_LOGI(
      TAG,
      "🎮 game_start_timer_move called: is_white_turn=%s, current_player=%s",
      is_white_turn ? "WHITE" : "BLACK",
      current_player == PLAYER_WHITE ? "WHITE" : "BLACK");
  esp_err_t ret = timer_start_move(is_white_turn);
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "✅ Timer started successfully for %s",
             is_white_turn ? "White" : "Black");
  } else {
    ESP_LOGE(TAG, "❌ Failed to start timer for %s: %s",
             is_white_turn ? "White" : "Black", esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t game_end_timer_move(void) {
  esp_err_t ret = timer_end_move();
  if (ret == ESP_OK) {
    ESP_LOGD(TAG, "Timer ended for move");
  }

  return ret;
}

esp_err_t game_pause_timer(void) {
  esp_err_t ret = timer_pause();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer paused");
  }

  return ret;
}

esp_err_t game_resume_timer(void) {
  esp_err_t ret = timer_resume();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer resumed");
  }

  return ret;
}

esp_err_t game_reset_timer(void) {
  esp_err_t ret = timer_reset();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer reset");
  }

  return ret;
}

bool game_check_timer_timeout(void) { return timer_check_timeout(); }

uint32_t game_get_remaining_time(bool is_white_turn) {
  return timer_get_remaining_time(is_white_turn);
}

esp_err_t game_get_timer_state(chess_timer_t *timer_data) {
  if (timer_data == NULL) {
    ESP_LOGE(TAG, "Invalid timer_data parameter");
    return ESP_ERR_INVALID_ARG;
  }

  return timer_get_state(timer_data);
}

esp_err_t game_init_timer_system(void) {
  ESP_LOGI(TAG, "Initializing timer system in game task...");

  esp_err_t ret = timer_system_init();
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Timer system initialized successfully");
  } else {
    ESP_LOGE(TAG, "Failed to initialize timer system: %s",
             esp_err_to_name(ret));
  }

  return ret;
}

esp_err_t game_process_timer_command(const chess_move_command_t *cmd) {
  if (cmd == NULL) {
    ESP_LOGE(TAG, "Invalid command parameter");
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  switch (cmd->type) {
  case GAME_CMD_SET_TIME_CONTROL: {
    time_control_config_t config;
    time_control_type_t type =
        (time_control_type_t)cmd->timer_data.timer_config.time_control_type;

    if (type == TIME_CONTROL_CUSTOM) {
      // Vlastní časová kontrola
      ret = timer_set_custom_time_control(
          cmd->timer_data.timer_config.custom_minutes,
          cmd->timer_data.timer_config.custom_increment);
    } else {
      // Předdefinovaná časová kontrola
      ret = timer_get_config_by_type(type, &config);
      if (ret == ESP_OK) {
        ret = timer_set_time_control(&config);
      }
    }

    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "Time control command processed successfully");
    } else {
      ESP_LOGE(TAG, "Failed to process time control command");
    }
  } break;

  case GAME_CMD_PAUSE_TIMER:
    ret = game_pause_timer();
    break;

  case GAME_CMD_RESUME_TIMER:
    ret = game_resume_timer();
    break;

  case GAME_CMD_RESET_TIMER:
    ret = game_reset_timer();
    break;

  case GAME_CMD_GET_TIMER_STATE:
    // Timer state je získáván přes JSON API
    ret = ESP_OK;
    break;

  case GAME_CMD_TIMER_TIMEOUT:
    ret = game_handle_time_expiration();
    break;

  default:
    ESP_LOGW(TAG, "Unknown timer command: %d", cmd->type);
    ret = ESP_ERR_INVALID_ARG;
    break;
  }

  return ret;
}

esp_err_t game_handle_time_expiration(void) {
  ESP_LOGW(TAG, "⏰ Time expired! Handling timeout...");

  // Získat stav timeru
  chess_timer_t timer_data;
  esp_err_t ret = timer_get_state(&timer_data);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get timer state");
    return ret;
  }

  // Určit vítěze
  player_t winner = timer_data.is_white_turn ? PLAYER_BLACK : PLAYER_WHITE;
  const char *winner_name = (winner == PLAYER_WHITE) ? "White" : "Black";
  const char *loser_name = (winner == PLAYER_WHITE) ? "Black" : "White";

  ESP_LOGW(TAG, "🏆 %s wins by timeout! %s ran out of time.", winner_name,
           loser_name);

  // Ukončit hru
  current_game_state = GAME_STATE_FINISHED;
  game_result = GAME_STATE_FINISHED;
  current_result_type =
      (winner == PLAYER_WHITE) ? RESULT_WHITE_WINS : RESULT_BLACK_WINS;
  current_endgame_reason = ENDGAME_REASON_TIMEOUT; // Označit jako timeout

  // Požadavek na endgame report (stejný fix jako u resignation)
  game_update_endgame_statistics(current_result_type);
  endgame_report_requested = true;

  // UNIFIED VICTORY ANIMATION
  // Timeout = Winner determined above
  game_trigger_victory_animation(winner);

  // Zobrazit výsledek na LED (použít existující LED funkce)
  // led_show_game_result(game_result); // Funkce neexistuje

  // Pozastavit timer
  timer_pause();

  ESP_LOGI(TAG, "Game ended due to timeout. Winner: %s", winner_name);

  return ESP_OK;
}

esp_err_t game_update_timer_display(void) {
  // Kontrola timeout
  if (timer_check_timeout()) {
    return game_handle_time_expiration();
  }

  return ESP_OK;
}

uint32_t game_get_available_time_controls(time_control_config_t *controls,
                                          uint32_t max_count) {
  if (controls == NULL || max_count == 0) {
    return 0;
  }

  return timer_get_available_controls(controls, max_count);
}

esp_err_t game_set_custom_time_control(uint32_t minutes,
                                       uint32_t increment_seconds) {
  return timer_set_custom_time_control(minutes, increment_seconds);
}

esp_err_t game_get_timer_stats(uint32_t *total_moves, uint32_t *avg_move_time) {
  if (total_moves == NULL || avg_move_time == NULL) {
    ESP_LOGE(TAG, "Invalid parameters");
    return ESP_ERR_INVALID_ARG;
  }

  *total_moves = timer_get_total_moves();
  *avg_move_time = timer_get_average_move_time();

  return ESP_OK;
}

bool game_is_timer_active(void) { return timer_is_active(); }

time_control_type_t game_get_current_time_control_type(void) {
  return timer_get_current_type();
}
