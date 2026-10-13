/**
 * @file board_api_auth.h
 * @brief Board REST API token (NVS) + HTTP Bearer check for sensitive endpoints.
 *
 * ## Auth matrix (HTTP)
 * - Public GET (No Bearer): eg `/api/status`, `/api/board`, `/api/game/snapshot`,
 * `/api/wifi/status`, `/api/system/firmware`, `/api/system/ota/status` (onboarding).
 * - Admin POST (`/api/system/ota`, `/api/system/factory_reset`, paths under `/api/wifi/`, …):
 * a valid Bearer is always sufficient. If **WEB_LOCK is on** and Bearer is missing or invalid → 403 `web_locked`.
 * If **WEB_LOCK is off** (default), those admin POSTs from LAN still go through without a header.
 *
 * Command token (separate from `api_tok`): issued when a device finishes pairing.
 * `POST /api/move` and a command frame on `/ws` always require one of the three
 * device tokens, even when WEB_LOCK is off. Snapshot reads stay open.
 * While one commander holds the board, a different valid token gets `board_busy`.
 *
 * Token: 32B random, in header as `Authorization: Bearer <64 hex characters>`.
 *
 * ## BLE (see ble_task_conn_is_encrypted)
 * - `ota_start`, `ota_ble_begin`, `ota_ble_abort`, `ota_ble_status`, `wifi_sta_config`,
 * `wifi_ap_set`, `factory_reset`: require an active encrypted link (after SMP).
 */
#pragma once

#include "sdkconfig.h"
#include "board_pair_policy.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_CHESS_ENABLE_WEB_SERVER
#include "esp_http_server.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** NVS initialization / token generation on first boot. */
esp_err_t board_api_auth_init(void);

#if CONFIG_CHESS_ENABLE_WEB_SERVER
/**
 * HTTP admin action: checks Bearer token vs NVS and web lock.
 * @return true if a 403 has already been sent and the handler should return ESP_OK.
 */
bool board_api_auth_admin_http_denied(httpd_req_t *req);
#endif

/** UART / diagnostics: hex string (64 characters) + '\\0'. */
esp_err_t board_api_auth_get_token_hex(char *out_hex, size_t cap);

/** Generates a new token (the old one becomes invalid). */
esp_err_t board_api_auth_rotate_token(void);

/**
 * Device command tokens (max 3), stored beside the admin `api_tok`.
 * A phone learns its token from the BLE cmd_ack notify `code=paired`.
 * A host without BLE learns it from POST /api/pair, and only while the
 * pairing window is open. Both paths use the same 32-byte token style.
 */
esp_err_t board_pair_issue_for_peer(uint8_t addr_type, const uint8_t addr[6],
                                    char *out_hex, size_t cap);
esp_err_t board_pair_issue_wifi(char *out_hex, size_t cap);
bool board_pair_copy_token_hex(uint8_t addr_type, const uint8_t addr[6],
                               char *out_hex, size_t cap);
void board_pair_on_ble_up(uint8_t addr_type, const uint8_t addr[6]);
void board_pair_on_ble_down(void);

#if CONFIG_CHESS_ENABLE_WEB_SERVER
/** POST /api/move and command WebSocket: 403 already sent when true. */
bool board_pair_http_command_denied(httpd_req_t *req);
/**
 * Command frame on an already-open WebSocket. Snapshot pushes are not this.
 * @return BOARD_PAIR_CMD_ALLOW, BOARD_PAIR_CMD_NEED_TOKEN, or BOARD_PAIR_CMD_BUSY.
 */
int board_pair_ws_command(const uint8_t token[32], int fd);
void board_pair_ws_release_fd(int fd);
void board_pair_ws_prune(const int *live_fds, int live_count);
esp_err_t board_pair_http_handler(httpd_req_t *req);
#endif

#ifdef __cplusplus
}
#endif
