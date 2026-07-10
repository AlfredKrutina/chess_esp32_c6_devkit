/**
 * @file web_routes.c
 * @brief HTTP URI handler registration (extracted from web_server_task.c).
 */

#include "web_routes.h"
#include "web_server_internal.h"

#include "chess_piece_http.h"
#include "ota_update.h"
#include "web_server_task.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "WEB_ROUTES";

esp_err_t web_routes_register(httpd_handle_t handle) {
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  ESP_LOGI(TAG, "Registering URI handlers...");

  // Handler pro Chess JavaScript soubor
  httpd_uri_t chess_js_uri = {.uri = "/chess_app.js",
                              .method = HTTP_GET,
                              .handler = http_get_chess_js_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(handle, &chess_js_uri);

  // Handler pro root (JSON — aplikace místo prohlížeče)
  httpd_uri_t root_uri = {.uri = "/",
                          .method = HTTP_GET,
                          .handler = http_get_root_handler,
                          .user_ctx = NULL};
  httpd_register_uri_handler(handle, &root_uri);

  // Handlery pro API
  httpd_uri_t board_uri = {.uri = "/api/board",
                           .method = HTTP_GET,
                           .handler = http_get_board_handler,
                           .user_ctx = NULL};
  httpd_register_uri_handler(handle, &board_uri);

  httpd_uri_t game_snapshot_uri = {.uri = "/api/game/snapshot",
                                   .method = HTTP_GET,
                                   .handler = http_get_game_snapshot_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(handle, &game_snapshot_uri);

  httpd_uri_t status_uri = {.uri = "/api/status",
                            .method = HTTP_GET,
                            .handler = http_get_status_handler,
                            .user_ctx = NULL};
  httpd_register_uri_handler(handle, &status_uri);

  httpd_uri_t history_uri = {.uri = "/api/history",
                             .method = HTTP_GET,
                             .handler = http_get_history_handler,
                             .user_ctx = NULL};
  httpd_register_uri_handler(handle, &history_uri);

  httpd_uri_t captured_uri = {.uri = "/api/captured",
                              .method = HTTP_GET,
                              .handler = http_get_captured_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(handle, &captured_uri);

  httpd_uri_t advantage_uri = {.uri = "/api/advantage",
                               .method = HTTP_GET,
                               .handler = http_get_advantage_handler,
                               .user_ctx = NULL};
  httpd_register_uri_handler(handle, &advantage_uri);

  // Handlery pro Timer API
  httpd_uri_t timer_uri = {.uri = "/api/timer",
                           .method = HTTP_GET,
                           .handler = http_get_timer_handler,
                           .user_ctx = NULL};
  httpd_register_uri_handler(handle, &timer_uri);

  // Handler pro favicon.ico (silence 404 warnings)
  httpd_uri_t favicon_uri = {.uri = "/favicon.ico",
                             .method = HTTP_GET,
                             .handler = http_get_favicon_handler,
                             .user_ctx = NULL};
  httpd_register_uri_handler(handle, &favicon_uri);

  httpd_uri_t timer_config_uri = {.uri = "/api/timer/config",
                                  .method = HTTP_POST,
                                  .handler = http_post_timer_config_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(handle, &timer_config_uri);

  httpd_uri_t settings_brightness_uri = {
      .uri = "/api/settings/brightness",
      .method = HTTP_POST,
      .handler = http_post_settings_brightness_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_brightness_uri);

  httpd_uri_t settings_auto_lamp_timeout_uri = {
      .uri = "/api/settings/auto_lamp_timeout",
      .method = HTTP_POST,
      .handler = http_post_settings_auto_lamp_timeout_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_auto_lamp_timeout_uri);

  httpd_uri_t settings_guided_hints_uri = {
      .uri = "/api/settings/guided_hints",
      .method = HTTP_POST,
      .handler = http_post_settings_guided_hints_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_guided_hints_uri);

  httpd_uri_t settings_led_guidance_uri = {
      .uri = "/api/settings/led_guidance",
      .method = HTTP_POST,
      .handler = http_post_settings_led_guidance_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_led_guidance_uri);

  // Handlery pro Start Position Check nastaveni
  httpd_uri_t settings_start_pos_check_get_uri = {
      .uri = "/api/settings/start_pos_check",
      .method = HTTP_GET,
      .handler = http_get_settings_start_pos_check_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_start_pos_check_get_uri);

  httpd_uri_t settings_start_pos_check_post_uri = {
      .uri = "/api/settings/start_pos_check",
      .method = HTTP_POST,
      .handler = http_post_settings_start_pos_check_handler,
      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_start_pos_check_post_uri);

  httpd_uri_t timer_pause_uri = {.uri = "/api/timer/pause",
                                 .method = HTTP_POST,
                                 .handler = http_post_timer_pause_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &timer_pause_uri);

  httpd_uri_t timer_resume_uri = {.uri = "/api/timer/resume",
                                  .method = HTTP_POST,
                                  .handler = http_post_timer_resume_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(handle, &timer_resume_uri);

  httpd_uri_t timer_reset_uri = {.uri = "/api/timer/reset",
                                 .method = HTTP_POST,
                                 .handler = http_post_timer_reset_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &timer_reset_uri);

  // Handlery pro WiFi API
  httpd_uri_t wifi_config_uri = {.uri = "/api/wifi/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_wifi_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &wifi_config_uri);

  httpd_uri_t wifi_connect_uri = {.uri = "/api/wifi/connect",
                                  .method = HTTP_POST,
                                  .handler = http_post_wifi_connect_handler,
                                  .user_ctx = NULL};
  httpd_register_uri_handler(handle, &wifi_connect_uri);

  httpd_uri_t wifi_disconnect_uri = {.uri = "/api/wifi/disconnect",
                                     .method = HTTP_POST,
                                     .handler =
                                         http_post_wifi_disconnect_handler,
                                     .user_ctx = NULL};
  httpd_register_uri_handler(handle, &wifi_disconnect_uri);

  httpd_uri_t wifi_clear_uri = {.uri = "/api/wifi/clear",
                                .method = HTTP_POST,
                                .handler = http_post_wifi_clear_handler,
                                .user_ctx = NULL};
  httpd_register_uri_handler(handle, &wifi_clear_uri);

  httpd_uri_t factory_reset_uri = {.uri = "/api/system/factory_reset",
                                   .method = HTTP_POST,
                                   .handler = http_post_factory_reset_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(handle, &factory_reset_uri);

  ret = ota_update_register_http_handlers(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "ota_update_register_http_handlers failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

  httpd_uri_t wifi_status_uri = {.uri = "/api/wifi/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_wifi_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &wifi_status_uri);

  // Handler pro web lock status
  httpd_uri_t web_lock_status_uri = {.uri = "/api/web/lock-status",
                                     .method = HTTP_GET,
                                     .handler =
                                         http_get_web_lock_status_handler,
                                     .user_ctx = NULL};
  httpd_register_uri_handler(handle, &web_lock_status_uri);

  // Handlery pro Demo Mode
  httpd_uri_t demo_config_uri = {.uri = "/api/demo/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_demo_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &demo_config_uri);

  httpd_uri_t demo_start_uri = {.uri = "/api/demo/start",
                                .method = HTTP_POST,
                                .handler = http_post_demo_start_handler,
                                .user_ctx = NULL};
  httpd_register_uri_handler(handle, &demo_start_uri);

  httpd_uri_t demo_status_uri = {.uri = "/api/demo/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_demo_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &demo_status_uri);

  // Handler pro Tahy
  httpd_uri_t move_uri = {.uri = "/api/move",
                          .method = HTTP_POST,
                          .handler = http_post_game_move_handler,
                          .user_ctx = NULL};
  httpd_register_uri_handler(handle, &move_uri);

  // Handler pro Virtual Actions (Remote Control)
  httpd_uri_t virtual_action_uri = {.uri = "/api/game/virtual_action",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_virtual_action_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(handle, &virtual_action_uri);

  // Handler pro New Game
  httpd_uri_t new_game_uri = {.uri = "/api/game/new",
                              .method = HTTP_POST,
                              .handler = http_post_game_new_handler,
                              .user_ctx = NULL};
  httpd_register_uri_handler(handle, &new_game_uri);

  // Handler pro Hint highlight (LED)
  httpd_uri_t hint_highlight_uri = {.uri = "/api/game/hint_highlight",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_hint_highlight_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(handle, &hint_highlight_uri);

  httpd_uri_t hint_clear_uri = {.uri = "/api/game/hint_clear",
                               .method = HTTP_POST,
                               .handler = http_post_game_hint_clear_handler,
                               .user_ctx = NULL};
  httpd_register_uri_handler(handle, &hint_clear_uri);

  httpd_uri_t guard_clear_uri = {.uri = "/api/game/guard_clear",
                                 .method = HTTP_POST,
                                 .handler = http_post_game_guard_clear_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &guard_clear_uri);

  httpd_uri_t setup_tutorial_uri = {.uri = "/api/game/setup_tutorial",
                                    .method = HTTP_POST,
                                    .handler =
                                        http_post_game_setup_tutorial_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(handle, &setup_tutorial_uri);

  httpd_uri_t puzzle_uri = {.uri = "/api/game/puzzle",
                            .method = HTTP_POST,
                            .handler = http_post_game_puzzle_handler,
                            .user_ctx = NULL};
  httpd_register_uri_handler(handle, &puzzle_uri);

  httpd_uri_t opening_uri = {.uri = "/api/game/opening",
                             .method = HTTP_POST,
                             .handler = http_post_game_opening_handler,
                             .user_ctx = NULL};
  httpd_register_uri_handler(handle, &opening_uri);

  httpd_uri_t settings_ui_get_uri = {.uri = "/api/settings/ui",
                                   .method = HTTP_GET,
                                   .handler = http_get_settings_ui_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_ui_get_uri);

  httpd_uri_t settings_ui_post_uri = {.uri = "/api/settings/ui",
                                      .method = HTTP_POST,
                                      .handler = http_post_settings_ui_handler,
                                      .user_ctx = NULL};
  httpd_register_uri_handler(handle, &settings_ui_post_uri);

  // Handlery pro lampu (režim Lampa z webu)
  httpd_uri_t light_command_uri = {.uri = "/api/light/command",
                                   .method = HTTP_POST,
                                   .handler = http_post_light_command_handler,
                                   .user_ctx = NULL};
  httpd_register_uri_handler(handle, &light_command_uri);

  httpd_uri_t light_game_mode_uri = {.uri = "/api/light/game_mode",
                                    .method = HTTP_POST,
                                    .handler = http_post_light_game_mode_handler,
                                    .user_ctx = NULL};
  httpd_register_uri_handler(handle, &light_game_mode_uri);

  // Handlery pro MQTT API
  httpd_uri_t mqtt_status_uri = {.uri = "/api/mqtt/status",
                                 .method = HTTP_GET,
                                 .handler = http_get_mqtt_status_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &mqtt_status_uri);

  httpd_uri_t mqtt_config_uri = {.uri = "/api/mqtt/config",
                                 .method = HTTP_POST,
                                 .handler = http_post_mqtt_config_handler,
                                 .user_ctx = NULL};
  httpd_register_uri_handler(handle, &mqtt_config_uri);

  ret = chess_piece_register_http_uris(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "chess_piece_register_http_uris failed: %s",
             esp_err_to_name(ret));
    return ret;
  }

#if CONFIG_HTTPD_WS_SUPPORT
  httpd_uri_t ws_uri = {.uri = "/ws",
                        .method = HTTP_GET,
                        .handler = http_ws_handler,
                        .user_ctx = NULL,
                        .is_websocket = true};
  httpd_register_uri_handler(handle, &ws_uri);
  ESP_LOGI(TAG,
           "HTTP server on port %d: GET /api/game/snapshot + WS /ws, max_uri_handlers=64",
           WEB_HTTP_SERVER_PORT);
  web_server_websocket_init();
#else
  ESP_LOGW(TAG,
           "HTTP server on port %d: WebSocket /ws NENÍ v buildu — zapni "
           "CONFIG_HTTPD_WS_SUPPORT (iOS/watchOS WS jinak padá na -1011)",
           WEB_HTTP_SERVER_PORT);
#endif

  return ESP_OK;
}
