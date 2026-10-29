/**
 * @file cmd_wire.h
 * @brief Shared command id, session epoch, and last-success cache for BLE and HTTP.
 *
 * Ack means the board accepted or refused the command immediately. It does not
 * mean the move is already in the game. Callers must not block a GATT write
 * on game_task.
 */
#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Gate result for one incoming command JSON object. */
typedef enum {
  CMD_WIRE_PROCEED = 0,
  CMD_WIRE_DONE_OK = 1,
  CMD_WIRE_DONE_ERR = 2,
} cmd_wire_gate_t;

bool cmd_wire_json_u32(const char *json, const char *key, uint32_t *out);

/**
 * Command correlation id. Prefers `"req"` so a semantic `"id"` (puzzle
 * catalog) is not reused as the ack token. Echoed back as ack `"id"`.
 */
bool cmd_wire_correlation_id(const char *json, uint32_t *out);

bool cmd_wire_json_string(const char *json, const char *key, char *out,
                          size_t out_cap);

/** ping, wifi_survey, ota_ble_status, pairing, and session commands. */
bool cmd_wire_is_session_exempt(const char *cmd);

/**
 * Session and idempotency gate. On DONE_*, *out_err is the dispatch result
 * and the caller must not submit again.
 */
cmd_wire_gate_t cmd_wire_gate(const char *cmd, const char *json,
                              esp_err_t *out_err);

/** Remember a successful enqueue. busy / errors are not cached. */
void cmd_wire_note_dispatch_result(esp_err_t err);

/** Drop a BLE write owner. A Wi-Fi claim stays until release or a higher epoch. */
void cmd_wire_on_ble_disconnect(void);

/**
 * Prefix `"id":N,` into a JSON object when has_id is set.
 * @return false when the stamped text does not fit.
 */
bool cmd_wire_prefix_id(char *dst, size_t cap, const char *json, uint32_t id,
                        bool has_id);

/** Notify cmd_ack, adding id only when the current BLE request carried one. */
void cmd_wire_notify_with_current(const char *json);

/** Notify cmd_ack with an explicit id snapshot (async tasks). */
void cmd_wire_notify_with_id(const char *json, uint32_t id, bool has_id);

/** Copy the id of the BLE command currently inside dispatch. */
void cmd_wire_copy_request_id(uint32_t *id, bool *has_id);

bool cmd_wire_http_guard(httpd_req_t *req, const char *body, const char *cmd);

void cmd_wire_http_send(httpd_req_t *req, const char *status, const char *json,
                        const char *request, bool remember, const char *cmd);

esp_err_t cmd_wire_http_claim_handler(httpd_req_t *req);
esp_err_t cmd_wire_http_release_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif
