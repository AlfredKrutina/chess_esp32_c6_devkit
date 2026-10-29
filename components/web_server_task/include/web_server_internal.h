/**
 * @file web_server_internal.h
 * @brief Internal API between web_server_task.c sibling files.
 */
#ifndef WEB_SERVER_INTERNAL_H
#define WEB_SERVER_INTERNAL_H

#include "sdkconfig.h"
#include "esp_err.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct cJSON;

#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "esp_http_server.h"
#else
typedef void *httpd_handle_t;
#endif

#define JSON_BUFFER_SIZE 8192
#define SNAPSHOT_BUFFER_SIZE 20480
#define TIMER_HTTP_JSON_MAX 1024
#define WIFI_STATUS_JSON_MAX 768

/** Large snapshot JSON; valid until web_snapshot_buffer_release(). */
extern char *snapshot_buffer;

void web_snapshot_buffer_release(void);
extern SemaphoreHandle_t snapshot_build_mutex;
extern uint8_t cached_brightness;
extern bool cached_brightness_valid;

esp_err_t web_server_task_wdt_reset_safe(void);
esp_err_t build_snapshot_json(size_t *out_len);
esp_err_t web_server_apply_hint_highlight_json_body(const char *buf);
bool web_game_submit_hint_clear(void);
/** True only when body is one finished JSON object and was not cut short. */
bool web_json_body_complete(const char *body, int received, int content_len);
#if CONFIG_CHESS_ENABLE_WEB_SERVER
/** Read the whole body. False means drop it: do not paint LEDs. */
bool web_recv_json_body(httpd_req_t *req, char *buf, size_t cap);
#endif
esp_err_t web_server_opening_dispatch_body(const char *json);
esp_err_t web_server_puzzle_dispatch_body(const char *json, char *response,
                                         size_t response_len);
esp_err_t web_server_opening_dispatch_json(struct cJSON *root);
uint8_t web_opening_action_code(const char *action);
void inject_web_status_fields(char *buf, size_t buf_size);
void setup_tutorial_reset_finish_cooldown(void);
bool setup_tutorial_finish_in_cooldown(void);
void setup_tutorial_note_finish_conflict(void);

bool json_body_has_factory_confirm(const char *json);
esp_err_t factory_reset_schedule(void);

httpd_handle_t web_server_get_httpd_handle(void);
void czechmate_ensure_snapshot_notify_queue(void);
#if CONFIG_CHESS_ENABLE_WEB_SERVER
void ws_broadcast_snapshot(void);
/** Send `snapshot_buffer` of `len` bytes. The caller already built it. */
void ws_broadcast_prepared(size_t len);
void web_ws_shutdown(void);
#endif

extern QueueHandle_t snapshot_notify_queue;

esp_err_t wifi_get_sta_status_json(char *buffer, size_t buffer_size);

#if CONFIG_CHESS_ENABLE_WEB_SERVER
esp_err_t web_get_advantage_handler(httpd_req_t *req);
esp_err_t web_get_board_handler(httpd_req_t *req);
esp_err_t web_get_captured_handler(httpd_req_t *req);
esp_err_t web_get_chess_js_handler(httpd_req_t *req);
esp_err_t web_get_demo_status_handler(httpd_req_t *req);
esp_err_t web_get_favicon_handler(httpd_req_t *req);
esp_err_t web_get_game_snapshot_handler(httpd_req_t *req);
esp_err_t web_get_history_handler(httpd_req_t *req);
esp_err_t web_get_mqtt_status_handler(httpd_req_t *req);
esp_err_t web_get_root_handler(httpd_req_t *req);
esp_err_t web_get_settings_start_pos_check_handler(httpd_req_t *req);
esp_err_t web_get_settings_ui_handler(httpd_req_t *req);
esp_err_t web_get_status_handler(httpd_req_t *req);
esp_err_t web_get_timer_handler(httpd_req_t *req);
esp_err_t web_get_web_lock_status_handler(httpd_req_t *req);
esp_err_t web_get_wifi_status_handler(httpd_req_t *req);
esp_err_t web_post_demo_config_handler(httpd_req_t *req);
esp_err_t web_post_demo_start_handler(httpd_req_t *req);
esp_err_t web_post_factory_reset_handler(httpd_req_t *req);
esp_err_t web_post_game_guard_clear_handler(httpd_req_t *req);
esp_err_t web_post_game_hint_clear_handler(httpd_req_t *req);
esp_err_t web_post_game_hint_highlight_handler(httpd_req_t *req);
esp_err_t web_post_game_move_handler(httpd_req_t *req);

/** Queue one move. @p err receives the game-task message on rejection. */
esp_err_t web_game_enqueue_move(const char *from, const char *to,
                                const char *promotion, char *err, size_t err_cap,
                                bool *accepted_async);
/** BLE game cmds; returns ESP_ERR_NOT_FOUND if cmd is not a game command. */
esp_err_t web_server_ble_game_command_dispatch(const char *cmd, char *buf);
esp_err_t web_server_ble_system_command_dispatch(const char *cmd, char *buf);
esp_err_t web_post_game_new_handler(httpd_req_t *req);
esp_err_t web_post_game_resign_handler(httpd_req_t *req);
esp_err_t web_post_game_draw_handler(httpd_req_t *req);
esp_err_t web_server_draw_dispatch(const char **out_error);
/** Shared HTTP/BLE resign; @p out_error gets a short code on refusal. */
esp_err_t web_server_resign_dispatch_body(const char *json,
                                          const char **out_error);
esp_err_t web_post_game_puzzle_handler(httpd_req_t *req);
esp_err_t web_post_game_opening_handler(httpd_req_t *req);
esp_err_t web_post_game_setup_tutorial_handler(httpd_req_t *req);
esp_err_t web_post_game_virtual_action_handler(httpd_req_t *req);
esp_err_t web_post_light_command_handler(httpd_req_t *req);
esp_err_t web_post_light_game_mode_handler(httpd_req_t *req);
esp_err_t web_post_mqtt_config_handler(httpd_req_t *req);
esp_err_t web_post_settings_auto_lamp_timeout_handler(httpd_req_t *req);
esp_err_t web_post_settings_brightness_handler(httpd_req_t *req);
esp_err_t web_post_settings_hall_rest_handler(httpd_req_t *req);
esp_err_t web_post_settings_guided_hints_handler(httpd_req_t *req);
esp_err_t web_post_settings_led_guidance_handler(httpd_req_t *req);
esp_err_t web_post_settings_start_pos_check_handler(httpd_req_t *req);
esp_err_t web_post_settings_ui_handler(httpd_req_t *req);
esp_err_t web_post_timer_config_handler(httpd_req_t *req);
esp_err_t web_post_timer_pause_handler(httpd_req_t *req);
esp_err_t web_post_timer_reset_handler(httpd_req_t *req);
esp_err_t web_post_timer_resume_handler(httpd_req_t *req);
esp_err_t web_post_wifi_clear_handler(httpd_req_t *req);
esp_err_t web_post_wifi_config_handler(httpd_req_t *req);
esp_err_t web_post_wifi_connect_handler(httpd_req_t *req);
esp_err_t web_post_wifi_disconnect_handler(httpd_req_t *req);
esp_err_t web_ws_handler(httpd_req_t *req);
#endif /* CONFIG_CHESS_ENABLE_WEB_SERVER */

#endif /* WEB_SERVER_INTERNAL_H */
