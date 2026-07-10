/**
 * @file web_server_internal.h
 * @brief Internal API between web_server_task.c sibling files.
 */
#ifndef WEB_SERVER_INTERNAL_H
#define WEB_SERVER_INTERNAL_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JSON_BUFFER_SIZE 8192
#define SNAPSHOT_BUFFER_SIZE 20480
#define TIMER_HTTP_JSON_MAX 1024

extern char json_buffer[JSON_BUFFER_SIZE];
extern char snapshot_buffer[SNAPSHOT_BUFFER_SIZE];
extern SemaphoreHandle_t snapshot_build_mutex;
extern uint8_t cached_brightness;
extern bool cached_brightness_valid;

esp_err_t web_server_task_wdt_reset_safe(void);
esp_err_t build_snapshot_json(size_t *out_len);
esp_err_t web_server_apply_hint_highlight_json_body(const char *buf);
void inject_web_status_fields(char *buf, size_t buf_size);
void setup_tutorial_reset_finish_cooldown(void);
bool setup_tutorial_finish_in_cooldown(void);
void setup_tutorial_note_finish_conflict(void);

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
