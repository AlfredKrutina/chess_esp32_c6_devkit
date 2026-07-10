/**
 * @file uart_handlers_wifi.c
 * @brief Timer, WiFi, web, BLE, and MQTT UART command handlers.
 */

#include "uart_task_internal.h"
#include "uart_parse.h"

#include "uart_task.h"
#include "config_manager.h"
#include "freertos_chess.h"
#include "game_task.h"

#include "../ble_task/include/ble_task.h"
#include "../timer_system/include/timer_system.h"
#include "../web_server_task/include/board_api_auth.h"
#include "../web_server_task/include/web_server_task.h"
#include "../ha_light_task/include/ha_light_task.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "UART_WIFI";

// ============================================================================
// PRIKAZY TIMERU (zrcadli webove rozhrani)
// ============================================================================

/**
 * @brief Format time in milliseconds to MM:SS format
 */
static void format_time_mmss(char *buffer, size_t size, uint32_t time_ms) {
  uint32_t total_seconds = time_ms / 1000;
  uint32_t minutes = total_seconds / 60;
  uint32_t seconds = total_seconds % 60;
  snprintf(buffer, size, "%02u:%02u", (unsigned int)minutes,
           (unsigned int)seconds);
}

/**
 * @brief Zobrazi stav timeru (lidsky citelny format)
 */
command_result_t uart_cmd_timer(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  chess_timer_t timer_state;
  if (timer_get_state(&timer_state) != ESP_OK) {
    uart_send_error("❌ Failed to get timer state");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_colored_line(COLOR_INFO, "⏱️ Timer State");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Time control info
  if (timer_state.config.type == TIME_CONTROL_NONE) {
    uart_send_formatted("⏸️  Time Control: None (No time limit)");
    uart_send_line("");
    return CMD_SUCCESS;
  }

  uart_send_formatted("📋 Time Control: %s", timer_state.config.name);
  // Description is a const char* - check if it's not empty (skip NULL check as
  // it's always set in TIME_CONTROLS array)
  if (timer_state.config.description[0] != '\0') {
    uart_send_formatted("   (%s)", timer_state.config.description);
  }

  // Format increment info
  if (timer_state.config.increment_ms > 0) {
    char inc_str[32];
    format_time_mmss(inc_str, sizeof(inc_str), timer_state.config.increment_ms);
    uart_send_formatted("   Increment: %s per move", inc_str);
  }
  uart_send_line("");

  // Player times
  char white_time_str[16], black_time_str[16];
  format_time_mmss(white_time_str, sizeof(white_time_str),
                   timer_state.white_time_ms);
  format_time_mmss(black_time_str, sizeof(black_time_str),
                   timer_state.black_time_ms);

  // Determine active player indicator
  const char *white_indicator =
      timer_state.is_white_turn && timer_state.timer_running ? "⏱️ " : "   ";
  const char *black_indicator =
      !timer_state.is_white_turn && timer_state.timer_running ? "⏱️ " : "   ";

  uart_send_formatted("%s White: %s", white_indicator, white_time_str);
  if (timer_state.is_white_turn && timer_state.timer_running) {
    uart_send_formatted(" (running)");
  }
  uart_send_line("");

  uart_send_formatted("%s Black: %s", black_indicator, black_time_str);
  if (!timer_state.is_white_turn && timer_state.timer_running) {
    uart_send_formatted(" (running)");
  }
  uart_send_line("");

  // Status
  if (timer_state.game_paused) {
    uart_send_colored_line(COLOR_WARNING, "⏸️  Timer: PAUSED");
  } else if (timer_state.timer_running) {
    uart_send_formatted("▶️  Timer: RUNNING");
  } else {
    uart_send_formatted("⏸️  Timer: STOPPED");
  }

  if (timer_state.time_expired) {
    uart_send_colored_line(COLOR_ERROR, "⚠️  TIME EXPIRED!");
  }

  // Statistics
  if (timer_state.total_moves > 0) {
    uart_send_formatted("📊 Total moves: %lu", timer_state.total_moves);
    if (timer_state.avg_move_time_ms > 0) {
      char avg_time_str[16];
      format_time_mmss(avg_time_str, sizeof(avg_time_str),
                       timer_state.avg_move_time_ms);
      uart_send_formatted(" | Avg move time: %s", avg_time_str);
    }
    uart_send_line("");
  }

  uart_send_line("");
  return CMD_SUCCESS;
}

/**
 * @brief Nastavi casovy limit nebo zobrazi dostupne moznosti
 */
command_result_t uart_cmd_timer_config(const char *args) {
  SAFE_WDT_RESET();

  // If no args or "options", show all available time controls
  if (!args || strlen(args) == 0 || strcmp(args, "options") == 0 ||
      strcmp(args, "OPTIONS") == 0 || strcmp(args, "list") == 0 ||
      strcmp(args, "LIST") == 0) {

    uart_send_colored_line(COLOR_INFO, "⏱️ Available Time Controls");
    uart_send_formatted(
        "═══════════════════════════════════════════════════════════════");

    time_control_config_t controls[16];
    uint32_t count = timer_get_available_controls(controls, 16);

    for (uint32_t i = 0; i < count; i++) {
      char time_str[32], increment_str[16];

      // Format time
      uint32_t total_sec = controls[i].initial_time_ms / 1000;
      uint32_t minutes = total_sec / 60;
      uint32_t seconds = total_sec % 60;
      if (minutes >= 60) {
        unsigned long hours = (unsigned long)(minutes / 60);
        unsigned long mins = (unsigned long)(minutes % 60);
        snprintf(time_str, sizeof(time_str), "%luh %02lum", hours, mins);
      } else if (seconds > 0) {
        unsigned long mins = (unsigned long)minutes;
        unsigned long secs = (unsigned long)seconds;
        snprintf(time_str, sizeof(time_str), "%lum %02lus", mins, secs);
      } else {
        unsigned long mins = (unsigned long)minutes;
        snprintf(time_str, sizeof(time_str), "%lum", mins);
      }

      // Format increment
      if (controls[i].increment_ms > 0) {
        unsigned long inc_sec =
            (unsigned long)(controls[i].increment_ms / 1000);
        snprintf(increment_str, sizeof(increment_str), "+%lus", inc_sec);
      } else {
        snprintf(increment_str, sizeof(increment_str), "+0s");
      }

      // Display with format: ID: Name (time + increment) - Description
      const char *speed_indicator = controls[i].is_fast ? "⚡" : "🕐";

      uart_send_formatted("%2lu: %s %s (%s %s)", i, speed_indicator,
                          controls[i].name, time_str, increment_str);
      if (strlen(controls[i].description) > 0) {
        uart_send_formatted("    %s", controls[i].description);
      }
    }

    uart_send_formatted("");
    uart_send_formatted("Usage:");
    uart_send_formatted(
        "  TIMER_CONFIG <0-14>              - Set predefined time control");
    uart_send_formatted("  TIMER_CONFIG custom <min> <inc>  - Set custom time "
                        "(1-180 min, 0-60s increment)");
    uart_send_formatted("");
    uart_send_formatted("Examples:");
    uart_send_formatted("  TIMER_CONFIG 1       - Set Bullet 1+0");
    uart_send_formatted("  TIMER_CONFIG 8       - Set Rapid 10+0");
    uart_send_formatted(
        "  TIMER_CONFIG custom 15 5  - Set custom 15min + 5s increment");
    uart_send_formatted("");

    return CMD_SUCCESS;
  }

  if (game_command_queue == NULL) {
    uart_send_error("❌ Game task not available");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  chess_move_command_t cmd = (chess_move_command_t){0};
  cmd.type = GAME_CMD_SET_TIME_CONTROL;

  char arg1[32], arg2[32], arg3[32];
  int parsed = sscanf(args, "%31s %31s %31s", arg1, arg2, arg3);

  if (strcmp(arg1, "custom") == 0 || strcmp(arg1, "CUSTOM") == 0) {
    if (parsed < 3) {
      uart_send_error(
          "❌ Usage: TIMER_CONFIG custom <minutes> <increment_sec>");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    cmd.timer_data.timer_config.time_control_type = TIME_CONTROL_CUSTOM;
    cmd.timer_data.timer_config.custom_minutes = (uint32_t)atoi(arg2);
    cmd.timer_data.timer_config.custom_increment = (uint32_t)atoi(arg3);

    // Validate custom values
    if (cmd.timer_data.timer_config.custom_minutes < 1 ||
        cmd.timer_data.timer_config.custom_minutes > 180) {
      uart_send_error("❌ Minutes must be between 1 and 180");
      return CMD_ERROR_INVALID_PARAMETER;
    }
    if (cmd.timer_data.timer_config.custom_increment > 60) {
      uart_send_error("❌ Increment must be between 0 and 60 seconds");
      return CMD_ERROR_INVALID_PARAMETER;
    }
  } else {
    int type = atoi(arg1);
    if (type < 0 || type >= TIME_CONTROL_MAX) {
      char error_msg[128];
      snprintf(error_msg, sizeof(error_msg),
               "❌ Invalid type. Use 0-%d or 'custom'. Use 'TIMER_CONFIG "
               "options' to see all.",
               TIME_CONTROL_MAX - 1);
      uart_send_error(error_msg);
      return CMD_ERROR_INVALID_SYNTAX;
    }
    cmd.timer_data.timer_config.time_control_type = (uint8_t)type;
    if (type == TIME_CONTROL_CUSTOM && parsed >= 3) {
      cmd.timer_data.timer_config.custom_minutes = (uint32_t)atoi(arg2);
      cmd.timer_data.timer_config.custom_increment = (uint32_t)atoi(arg3);
    }
  }

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    uart_send_formatted("✅ Time control update sent");
  } else {
    uart_send_error("❌ Failed to send time control");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  return CMD_SUCCESS;
}

/**
 * @brief Pause timer
 */
command_result_t uart_cmd_timer_pause(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  if (game_command_queue == NULL) {
    uart_send_error("❌ Game task not available");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  chess_move_command_t cmd = (chess_move_command_t){0};
  cmd.type = GAME_CMD_PAUSE_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    uart_send_formatted("✅ Timer pause requested");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Failed to pause timer");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

/**
 * @brief Resume timer
 */
command_result_t uart_cmd_timer_resume(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  if (game_command_queue == NULL) {
    uart_send_error("❌ Game task not available");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  chess_move_command_t cmd = (chess_move_command_t){0};
  cmd.type = GAME_CMD_RESUME_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    uart_send_formatted("✅ Timer resume requested");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Failed to resume timer");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

/**
 * @brief Reset timer
 */
command_result_t uart_cmd_timer_reset(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  if (game_command_queue == NULL) {
    uart_send_error("❌ Game task not available");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  chess_move_command_t cmd = (chess_move_command_t){0};
  cmd.type = GAME_CMD_RESET_TIMER;

  if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    uart_send_formatted("✅ Timer reset requested");
    return CMD_SUCCESS;
  } else {
    uart_send_error("❌ Failed to reset timer");
    return CMD_ERROR_SYSTEM_ERROR;
  }
}

// ============================================================================
// WIFI UART PRIKAZY
// ============================================================================

/**
 * @brief Konfiguruje WiFi STA (SSID a heslo)
 *
 * @param args SSID a heslo (muze byt v uvozovkach pro mezery)
 * @return command_result_t
 *
 * @details
 * Parsuje argumenty: WIFI <ssid> <password>
 * Podporuje uvozovky pro SSID/heslo s mezerami: WIFI "My Home WiFi" "My
 * Password"
 */
command_result_t uart_cmd_wifi(const char *args) {
  SAFE_WDT_RESET();

  if (!args || strlen(args) == 0) {
    uart_send_error("❌ Usage: WIFI <ssid> <password>");
    uart_send_formatted("   Example: WIFI MyHomeWiFi MyPassword123");
    uart_send_formatted("   Example: WIFI \"My Home WiFi\" \"My Password\"");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  char ssid[33] = {0};
  char password[65] = {0};

  // Parsovat argumenty - podporovat uvozovky
  const char *p = args;
  while (*p == ' ')
    p++; // Skip leading spaces

  // Parsovat SSID
  if (*p == '"') {
    // SSID v uvozovkach
    p++;
    const char *end = strchr(p, '"');
    if (end == NULL) {
      uart_send_error("❌ Missing closing quote for SSID");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    size_t len = end - p;
    if (len >= sizeof(ssid))
      len = sizeof(ssid) - 1;
    strncpy(ssid, p, len);
    ssid[len] = '\0';
    p = end + 1;
  } else {
    // SSID bez uvozovek - do prvni mezery
    const char *space = strchr(p, ' ');
    if (space == NULL) {
      uart_send_error("❌ Missing password");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    size_t len = space - p;
    if (len >= sizeof(ssid))
      len = sizeof(ssid) - 1;
    strncpy(ssid, p, len);
    ssid[len] = '\0';
    p = space;
  }

  // Skip spaces
  while (*p == ' ')
    p++;

  // Parsovat password
  if (*p == '"') {
    // Password v uvozovkach
    p++;
    const char *end = strchr(p, '"');
    if (end == NULL) {
      uart_send_error("❌ Missing closing quote for password");
      return CMD_ERROR_INVALID_SYNTAX;
    }
    size_t len = end - p;
    if (len >= sizeof(password))
      len = sizeof(password) - 1;
    strncpy(password, p, len);
    password[len] = '\0';
  } else {
    // Password bez uvozovek - zbytek retezce
    size_t len = strlen(p);
    if (len >= sizeof(password))
      len = sizeof(password) - 1;
    strncpy(password, p, len);
    password[len] = '\0';
  }

  // Validovat
  if (strlen(ssid) == 0 || strlen(ssid) > 32) {
    uart_send_error("❌ SSID too long (max 32 characters)");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  if (strlen(password) == 0 || strlen(password) > 64) {
    uart_send_error("❌ Password too long (max 64 characters)");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  // Ulozit do NVS
  esp_err_t ret = wifi_save_config_to_nvs(ssid, password);
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to save WiFi configuration to NVS");
    uart_send_formatted("   Error: %s", esp_err_to_name(ret));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ WiFi configuration saved");
  uart_send_formatted("   SSID: %s", ssid);
  uart_send_formatted("   Use 'WIFI_CONNECT' to connect");

  return CMD_SUCCESS;
}

/**
 * @brief Pripoji ESP32 k WiFi site
 *
 * @param args Nepouzivany
 * @return command_result_t
 */
command_result_t uart_cmd_wifi_connect(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_formatted("🔌 Connecting to WiFi...");

  esp_err_t ret = wifi_connect_sta();
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to connect to WiFi");
    uart_send_formatted("   Error: %s", esp_err_to_name(ret));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ Connected to WiFi successfully");

  return CMD_SUCCESS;
}

/**
 * @brief Odpoji ESP32 od WiFi site
 *
 * @param args Nepouzivany
 * @return command_result_t
 */
command_result_t uart_cmd_wifi_disconnect(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_formatted("🔌 Disconnecting from WiFi...");

  esp_err_t ret = wifi_disconnect_sta();
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to disconnect from WiFi");
    uart_send_formatted("   Error: %s", esp_err_to_name(ret));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ Disconnected from WiFi");

  return CMD_SUCCESS;
}

/**
 * @brief Zobrazi WiFi status (AP i STA)
 *
 * @param args Nepouzivany
 * @return command_result_t
 */
command_result_t uart_cmd_wifi_status(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "📡 WiFi Status");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // AP info
  uart_send_formatted("Access Point (AP):");
  uart_send_formatted("  SSID: %s", web_server_get_ap_ssid());
  uart_send_formatted("  IP: 192.168.4.1");
  // Client count by mel byt globalni promenna, ale pro jednoduchost pouzijeme
  // externi
  extern uint32_t client_count;
  uart_send_formatted("  Clients: %lu", (unsigned long)client_count);

  uart_send_formatted("");
  uart_send_formatted("Station (STA):");

  // Nacist STA konfiguraci z NVS
  char ssid[33] = {0};
  char password[65] = {0};
  esp_err_t nvs_ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));

  if (nvs_ret == ESP_OK) {
    uart_send_formatted("  SSID: %s", ssid);
  } else {
    uart_send_formatted("  SSID: Not configured");
  }

  if (wifi_is_sta_connected()) {
    extern char sta_ip[16];
    uart_send_formatted("  IP: %s", sta_ip);
    uart_send_formatted("  Connected: true");
  } else {
    uart_send_formatted("  IP: Not connected");
    uart_send_formatted("  Connected: false");
  }

  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

/**
 * @brief Nastavi lock/unlock web rozhrani
 *
 * @param args "ON" nebo "OFF"
 * @return command_result_t
 */
command_result_t uart_cmd_web_lock(const char *args) {
  SAFE_WDT_RESET();

  if (args == NULL || strlen(args) == 0) {
    uart_send_error("Usage: WEB_LOCK ON|OFF");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  // Parsovat argumenty
  char arg_upper[8];
  strncpy(arg_upper, args, sizeof(arg_upper) - 1);
  arg_upper[sizeof(arg_upper) - 1] = '\0';

  // Prevest na velka pismena
  for (int i = 0; arg_upper[i]; i++) {
    arg_upper[i] = toupper(arg_upper[i]);
  }

  bool lock = false;
  if (strcmp(arg_upper, "ON") == 0) {
    lock = true;
  } else if (strcmp(arg_upper, "OFF") == 0) {
    lock = false;
  } else {
    uart_send_error("Invalid argument. Use ON or OFF");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  // Nastavit lock stav
  esp_err_t ret = web_lock_set(lock);
  if (ret != ESP_OK) {
    uart_send_error("Failed to set web lock");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ Web interface %s", lock ? "locked" : "unlocked");
  uart_send_formatted("   Game settings changes from web are now %s",
                      lock ? "blocked" : "allowed");

  return CMD_SUCCESS;
}

/**
 * @brief Zobrazi status web rozhrani
 *
 * @param args Nepouzivany
 * @return command_result_t
 */
command_result_t uart_cmd_web_status(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🌐 Web Interface Status");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Lock status
  bool locked = web_is_locked();
  uart_send_formatted("Lock Status: %s", locked ? "🔒 LOCKED" : "🔓 UNLOCKED");
  uart_send_formatted("  Game settings changes from web: %s",
                      locked ? "BLOCKED" : "ALLOWED");

  uart_send_formatted("");

  // Online status
  bool sta_conn = wifi_is_sta_connected();
  extern char sta_ip[16];
  bool online = sta_conn && sta_ip[0] != '\0';

  uart_send_formatted("Internet Status: %s",
                      online ? "🟢 ONLINE" : "🔴 OFFLINE");

  if (sta_conn) {
    uart_send_formatted("  STA IP: %s",
                        sta_ip[0] != '\0' ? sta_ip : "Not assigned");
  } else {
    uart_send_formatted("  STA: Not connected");
  }

  bool http_active = web_server_is_active();
  esp_err_t http_err = web_server_get_last_http_error();
  uart_send_formatted("HTTP Server: %s",
                      http_active ? "🟢 ACTIVE" : "🔴 INACTIVE");
  uart_send_formatted("  Last HTTP start error: %s", esp_err_to_name(http_err));
  uart_send_formatted("Matrix guard pause: %s",
                      game_is_matrix_guard_active() ? "🟡 ACTIVE" : "⚪ OFF");
  uart_send_formatted("  Matrix conflicts: %u",
                      (unsigned int)game_get_matrix_guard_conflict_count());
  uart_send_formatted("Snapshot loaded on boot: %s",
                      game_was_snapshot_loaded_on_boot() ? "YES" : "NO");
  uart_send_formatted("Snapshot fallback mode: %s",
                      game_is_snapshot_fallback_used() ? "YES" : "NO");
  uart_send_formatted("Snapshot restore error: %s",
                      game_has_snapshot_restore_failure() ? "YES" : "NO");
  uart_send_formatted("Snapshot save error: %s",
                      game_has_snapshot_save_failure() ? "YES" : "NO");
  uart_send_formatted("Resync required after restore: %s",
                      game_is_resync_required_after_restore() ? "YES" : "NO");
  uart_send_formatted("Boot-triggered new game: %s",
                      game_was_boot_new_game_triggered() ? "YES" : "NO");
  if (sta_conn && sta_ip[0] != '\0') {
    uart_send_formatted("  URL: http://%s/", sta_ip);
  } else {
    uart_send_formatted("  URL (AP): http://192.168.4.1/");
  }

  // WiFi info
  char ssid[33] = {0};
  char password[65] = {0};
  esp_err_t nvs_ret =
      wifi_load_config_from_nvs(ssid, sizeof(ssid), password, sizeof(password));

  if (nvs_ret == ESP_OK) {
    uart_send_formatted("  STA SSID: %s", ssid);
  } else {
    uart_send_formatted("  STA SSID: Not configured");
  }

  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

command_result_t uart_cmd_api_token(const char *args) {
  SAFE_WDT_RESET();
  const char *p = args;
  if (p != NULL) {
    while (*p == ' ' || *p == '\t') {
      p++;
    }
  }
  bool rotate = false;
  if (p != NULL && p[0] != '\0') {
    char u[12] = {0};
    size_t i = 0;
    while (p[i] && i < sizeof(u) - 1) {
      char c = p[i];
      if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
      }
      u[i] = c;
      i++;
    }
    if (strcmp(u, "ROTATE") == 0 || strcmp(u, "NEW") == 0) {
      rotate = true;
    }
  }

  if (rotate) {
    esp_err_t e = board_api_auth_rotate_token();
    if (e != ESP_OK) {
      uart_send_formatted("API_TOKEN ROTATE failed: %s", esp_err_to_name(e));
      return CMD_ERROR_SYSTEM_ERROR;
    }
    uart_send_formatted("New board HTTP API token:");
  } else {
    uart_send_formatted("Board HTTP API token — use header:");
    uart_send_formatted("  Authorization: Bearer <hex>");
    uart_send_formatted("Hex (64 chars):");
  }

  char hex[72];
  if (board_api_auth_get_token_hex(hex, sizeof(hex)) != ESP_OK) {
    uart_send_error("Token unavailable (NVS / init error)");
    return CMD_ERROR_SYSTEM_ERROR;
  }
  uart_send_formatted("  %s", hex);
  if (!rotate) {
    uart_send_formatted("Rotate: API_TOKEN ROTATE");
  }
  return CMD_SUCCESS;
}

command_result_t uart_cmd_ble(const char *args) {
  (void)args;
  SAFE_WDT_RESET();
  uart_send_colored_line(COLOR_INFO, "Bluetooth LE (CZECHMATE GATT)");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  char line[160];
  ble_task_format_status(line, sizeof(line));
  uart_send_formatted("%s", line);
  uart_send_formatted("");
  uart_send_formatted("UUIDs (128-bit, viz CZECHMATEBLEUUIDs.swift):");
  uart_send_formatted("  Service   A0B40001-9267-4AB6-BDCC-E8336F8A8D9E");
  uart_send_formatted(
      "  Snapshot  A0B40002-9267-4AB6-BDCC-E8336F8A8D9E  (READ+NOTIFY)");
  uart_send_formatted(
      "  Command   A0B40003-9267-4AB6-BDCC-E8336F8A8D9E  (WRITE)");
  uart_send_formatted("Notifikace snapshotu: po subscribe + změně hry "
                      "(czechmate_on_game_state_changed).");
  uart_send_formatted("Detail: HELP APP");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");
  return CMD_SUCCESS;
}

/**
 * @brief Nastaví MQTT konfiguraci
 *
 * @param args Parametry: <host> [port] [username] [password]
 * @return command_result_t
 */
command_result_t uart_cmd_mqtt_config(const char *args) {
  if (!args || strlen(args) == 0) {
    uart_send_error(
        "❌ Usage: MQTT_CONFIG <host> [port] [username] [password]");
    uart_send_formatted("   Example: MQTT_CONFIG broker.example.com 1883");
    uart_send_formatted(
        "   Example: MQTT_CONFIG broker.example.com 1883 user pass");
    return CMD_ERROR_INVALID_SYNTAX;
  }

  SAFE_WDT_RESET();

  // Parse arguments
  char host[128] = {0};
  uint16_t port = 1883; // Default
  char username[64] = {0};
  char password[64] = {0};

  int parsed =
      sscanf(args, "%127s %hu %63s %63s", host, &port, username, password);
  if (parsed < 1) {
    uart_send_error("❌ Failed to parse arguments");
    uart_send_formatted(
        "   Usage: MQTT_CONFIG <host> [port] [username] [password]");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  /* uint16_t is always <= 65535; only 0 is invalid */
  if (port == 0) {
    uart_send_error("❌ Invalid port (must be 1-65535)");
    return CMD_ERROR_INVALID_PARAMETER;
  }

  // Save to NVS
  const char *username_ptr = (strlen(username) > 0) ? username : NULL;
  const char *password_ptr = (strlen(password) > 0) ? password : NULL;

  esp_err_t ret =
      mqtt_save_config_to_nvs(host, port, username_ptr, password_ptr);
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to save MQTT configuration");
    uart_send_formatted("   Error: %s", esp_err_to_name(ret));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ MQTT configuration saved");
  uart_send_formatted("   Host: %s", host);
  uart_send_formatted("   Port: %d", port);
  uart_send_formatted("   Username: %s",
                      username_ptr ? username_ptr : "(none)");
  uart_send_formatted("   Password: %s", password_ptr ? "***" : "(none)");
  uart_send_formatted("");

  // Reinit MQTT client if WiFi STA is connected
  extern bool wifi_is_sta_connected(void);
  if (wifi_is_sta_connected()) {
    uart_send_formatted(
        "🔄 Reinitializing MQTT client with new configuration...");
    esp_err_t reinit_ret = ha_light_reinit_mqtt();
    if (reinit_ret == ESP_OK) {
      uart_send_formatted("✅ MQTT client reinicialized");
    } else {
      uart_send_formatted("⚠️  Failed to reinit MQTT client: %s",
                          esp_err_to_name(reinit_ret));
      uart_send_formatted("   Client will reconnect on next WiFi connection");
    }
  } else {
    uart_send_formatted("ℹ️  MQTT client will reconnect with new configuration "
                        "on next WiFi connection");
  }

  return CMD_SUCCESS;
}

/**
 * @brief Zobrazí MQTT status
 *
 * @param args Nepoužitý
 * @return command_result_t
 */
command_result_t uart_cmd_mqtt_status(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "📡 MQTT Status");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Get MQTT config
  char host[128] = {0};
  char username[64] = {0};
  char password[64] = {0};
  uint16_t port = 1883;

  esp_err_t ret = mqtt_get_config(host, sizeof(host), &port, username,
                                  sizeof(username), password, sizeof(password));
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to get MQTT configuration");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("Configuration:");
  uart_send_formatted("  Broker: %s:%d", host, port);
  uart_send_formatted("  Username: %s", username[0] ? username : "(none)");
  uart_send_formatted("  Password: %s", password[0] ? "***" : "(none)");

  uart_send_formatted("");

  // Get WiFi STA status (required for MQTT)
  bool sta_connected = wifi_is_sta_connected();
  uart_send_formatted("WiFi STA: %s",
                      sta_connected ? "🟢 CONNECTED" : "🔴 DISCONNECTED");
  if (!sta_connected) {
    uart_send_formatted("  ℹ️  MQTT requires WiFi STA connection");
  }

  uart_send_formatted("");

  // Get MQTT connection status
  bool mqtt_connected = ha_light_is_mqtt_connected();
  uart_send_formatted("MQTT Client: %s",
                      mqtt_connected ? "🟢 CONNECTED" : "🔴 DISCONNECTED");

  uart_send_formatted("");

  // Get HA Light mode
  ha_mode_t mode = ha_light_get_mode();
  uart_send_formatted("HA Mode: %s",
                      (mode == HA_MODE_GAME) ? "🎮 GAME" : "💡 HA");

  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

/**
 * @brief Otestuje MQTT připojení
 *
 * @param args Nepoužitý
 * @return command_result_t
 */
command_result_t uart_cmd_mqtt_test(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_colored_line(COLOR_INFO, "🧪 MQTT Connection Test");
  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  // Check WiFi STA
  bool sta_connected = wifi_is_sta_connected();
  if (!sta_connected) {
    uart_send_error("❌ WiFi STA not connected");
    uart_send_formatted("   MQTT requires WiFi STA connection");
    uart_send_formatted("   Use 'WIFI_CONNECT' to connect");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  // Get MQTT config
  char host[128] = {0};
  uint16_t port = 1883;
  char username[64] = {0};
  char password[64] = {0};

  esp_err_t ret = mqtt_get_config(host, sizeof(host), &port, username,
                                  sizeof(username), password, sizeof(password));
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to get MQTT configuration");
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("Testing connection to: %s:%d", host, port);
  uart_send_formatted("");

  uart_send_formatted(
      "ℹ️  MQTT connection is handled automatically by ha_light_task");
  uart_send_formatted(
      "   When WiFi STA connects, MQTT client will connect automatically");
  uart_send_formatted("   Check 'MQTT_STATUS' to see current state");

  uart_send_formatted(
      "═══════════════════════════════════════════════════════════════");

  return CMD_SUCCESS;
}

/**
 * @brief Vymaze WiFi konfiguraci z NVS
 *
 * @param args Nepouzivany
 * @return command_result_t
 */
command_result_t uart_cmd_wifi_clear(const char *args) {
  (void)args; // Unused
  SAFE_WDT_RESET();

  uart_send_formatted("🗑️  Clearing WiFi configuration...");

  esp_err_t ret = wifi_clear_config_from_nvs();
  if (ret != ESP_OK) {
    uart_send_error("❌ Failed to clear WiFi configuration");
    uart_send_formatted("   Error: %s", esp_err_to_name(ret));
    return CMD_ERROR_SYSTEM_ERROR;
  }

  uart_send_formatted("✅ WiFi configuration cleared");

  return CMD_SUCCESS;
}
