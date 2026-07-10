/**
 * @file web_server_internal.h
 * @brief Internal API between web_server_task.c sibling files.
 */
#ifndef WEB_SERVER_INTERNAL_H
#define WEB_SERVER_INTERNAL_H

#include "esp_http_server.h"

esp_err_t http_get_advantage_handler(httpd_req_t *req);
esp_err_t http_get_board_handler(httpd_req_t *req);
esp_err_t http_get_captured_handler(httpd_req_t *req);
esp_err_t http_get_chess_js_handler(httpd_req_t *req);
esp_err_t http_get_demo_status_handler(httpd_req_t *req);
esp_err_t http_get_favicon_handler(httpd_req_t *req);
esp_err_t http_get_game_snapshot_handler(httpd_req_t *req);
esp_err_t http_get_history_handler(httpd_req_t *req);
esp_err_t http_get_mqtt_status_handler(httpd_req_t *req);
esp_err_t http_get_root_handler(httpd_req_t *req);
esp_err_t http_get_settings_start_pos_check_handler(httpd_req_t *req);
esp_err_t http_get_settings_ui_handler(httpd_req_t *req);
esp_err_t http_get_status_handler(httpd_req_t *req);
esp_err_t http_get_timer_handler(httpd_req_t *req);
esp_err_t http_get_web_lock_status_handler(httpd_req_t *req);
esp_err_t http_get_wifi_status_handler(httpd_req_t *req);
esp_err_t http_post_demo_config_handler(httpd_req_t *req);
esp_err_t http_post_demo_start_handler(httpd_req_t *req);
esp_err_t http_post_factory_reset_handler(httpd_req_t *req);
esp_err_t http_post_game_guard_clear_handler(httpd_req_t *req);
esp_err_t http_post_game_hint_clear_handler(httpd_req_t *req);
esp_err_t http_post_game_hint_highlight_handler(httpd_req_t *req);
esp_err_t http_post_game_move_handler(httpd_req_t *req);
esp_err_t http_post_game_new_handler(httpd_req_t *req);
esp_err_t http_post_game_puzzle_handler(httpd_req_t *req);
esp_err_t http_post_game_setup_tutorial_handler(httpd_req_t *req);
esp_err_t http_post_game_virtual_action_handler(httpd_req_t *req);
esp_err_t http_post_light_command_handler(httpd_req_t *req);
esp_err_t http_post_light_game_mode_handler(httpd_req_t *req);
esp_err_t http_post_mqtt_config_handler(httpd_req_t *req);
esp_err_t http_post_settings_auto_lamp_timeout_handler(httpd_req_t *req);
esp_err_t http_post_settings_brightness_handler(httpd_req_t *req);
esp_err_t http_post_settings_guided_hints_handler(httpd_req_t *req);
esp_err_t http_post_settings_led_guidance_handler(httpd_req_t *req);
esp_err_t http_post_settings_start_pos_check_handler(httpd_req_t *req);
esp_err_t http_post_settings_ui_handler(httpd_req_t *req);
esp_err_t http_post_timer_config_handler(httpd_req_t *req);
esp_err_t http_post_timer_pause_handler(httpd_req_t *req);
esp_err_t http_post_timer_reset_handler(httpd_req_t *req);
esp_err_t http_post_timer_resume_handler(httpd_req_t *req);
esp_err_t http_post_wifi_clear_handler(httpd_req_t *req);
esp_err_t http_post_wifi_config_handler(httpd_req_t *req);
esp_err_t http_post_wifi_connect_handler(httpd_req_t *req);
esp_err_t http_post_wifi_disconnect_handler(httpd_req_t *req);
esp_err_t http_ws_handler(httpd_req_t *req);

#endif /* WEB_SERVER_INTERNAL_H */
