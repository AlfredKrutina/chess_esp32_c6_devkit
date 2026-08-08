/**
 * @file ble_task.h
 * @brief BLE interface CZECHMATE — NimBLE GATT when CONFIG_BT_ENABLED.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_task_init(void);

/** True if there is an active GATT connection with enabled link encryption (SMP). */
bool ble_task_conn_is_encrypted(void);

/**
 * True if the BLE link is active and the central office has CCC snapshot notifications turned on.
 * Otherwise, push snapshot is a no-op — the web server doesn't have to compose JSON just "into the void" every 3 seconds.
 */
bool ble_task_should_push_snapshot(void);

/**
 * Sends a JSON snapshot to the connected central office (chunked, CM header).
 * Without CONFIG_BT_ENABLED or without BLE connection is a no-op.
 */
void ble_task_push_snapshot_json(const uint8_t *data, size_t len);

/**
 * Sends network info (IP, SSID, online status) to the connected central office.
 * Call when the IP address or WiFi status changes.
 * Without CONFIG_BT_ENABLED or without BLE connection is a no-op.
 */
void ble_task_push_network_info(void);

/** One BLE status line (NimBLE / GATT) for UART BLE command. */
void ble_task_format_status(char *buf, size_t cap);

/**
 * After processing GATT write to CMD characteristic sends JSON to cmd_ack notify
 * (if the central office is logged in to notifications). See the "cmd_ack" channel in the log.
 */
void ble_task_notify_command_result(esp_err_t err, const char *json_body);

/** Full JSON for cmd_ack notify (e.g. wifi_survey up to ~2 KiB). Use only from web_server BLE dispatch. */
void ble_task_notify_cmd_ack_json(const char *json_utf8);

#ifdef __cplusplus
}
#endif
