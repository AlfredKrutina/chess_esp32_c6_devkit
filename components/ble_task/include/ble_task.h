/**
 * @file ble_task.h
 * @brief CzechMate BLE GATT (NimBLE) when CONFIG_BT_ENABLED.
 *
 * Snapshot and command-ack notifies are no-ops without a connection or when
 * Bluetooth is compiled out. This header does not define the JSON schema;
 * callers pass bytes that already match the firmware contract.
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Start the NimBLE host task. No-op when CONFIG_BT_ENABLED is off. */
void ble_task_init(void);

/**
 * @brief Whether the current GATT link has encryption enabled (SMP).
 * @return false when Bluetooth is off or no encrypted connection is up.
 */
bool ble_task_conn_is_encrypted(void);

/**
 * @brief NimBLE host synced with the controller (GATT + OTA reachable over BLE).
 * Used by main as one of the health conditions before confirming a new OTA image.
 * @return false when Bluetooth is off or the host has not synced yet.
 */
bool ble_task_host_synced(void);

/**
 * @brief Whether a central is connected and has snapshot CCC notifications on.
 *
 * When this is false, a snapshot push is a no-op. The web server should not
 * build JSON that would only be discarded. Also false while an OTA receive
 * is active on the link.
 * @return true only when a push would be delivered.
 */
bool ble_task_should_push_snapshot(void);

/**
 * @brief Send a JSON snapshot to the connected central (chunked, CM header).
 *
 * No-op without CONFIG_BT_ENABLED or without a connection that wants snapshots.
 * @param data UTF-8 JSON bytes. Ignored when len is 0 or data is NULL.
 * @param len Byte length of data.
 */
void ble_task_push_snapshot_json(const uint8_t *data, size_t len);

/**
 * @brief Notify IP, SSID, and online status.
 *
 * Call when the address or Wi-Fi status changes. No-op without Bluetooth
 * or without a connection.
 */
void ble_task_push_network_info(void);

/**
 * @brief Write one BLE status line (NimBLE / GATT) for the UART BLE command.
 * @param buf Destination. NULL is ignored.
 * @param cap Capacity of buf, including the terminating NUL.
 */
void ble_task_format_status(char *buf, size_t cap);

/**
 * @brief Notify the cmd_ack characteristic after a GATT command write.
 *
 * Sent only when the central has subscribed. The log line uses channel "cmd_ack".
 * @param err Result of the command.
 * @param json_body Optional JSON body. NULL sends the status only.
 */
void ble_task_notify_command_result(esp_err_t err, const char *json_body);

/**
 * @brief Notify a full cmd_ack JSON payload (for example a wifi survey, up to ~2 KiB).
 *
 * Use only from the web_server BLE dispatch path.
 * @param json_utf8 NUL-terminated JSON. NULL is ignored.
 */
void ble_task_notify_cmd_ack_json(const char *json_utf8);

/**
 * @brief Open the 60 s pairing window, or cancel it if it is already open.
 *
 * Advertising itself stays up either way. A new bond is accepted only while
 * the window is open. A second press before it expires cancels it.
 */
void ble_task_request_pairing(void);

/** Kept for callers. Does not open pairing; only a triple click does. */
void ble_task_pair_after_first_flash(void);

/** @return true while a new device may complete pairing. */
bool ble_task_pairing_window_is_open(void);

/** @return true while the calm pairing pulse should cover the board. */
bool ble_task_pairing_pulse_wanted(void);

/**
 * @brief First successful new device takes the window.
 * @return false if the window was already closed or taken.
 */
bool ble_task_claim_pairing_success(void);

/** Stop the window timer and drop an unfinished new bond. NimBLE context or queued. */
void ble_task_finish_pairing_window(void);

#ifdef __cplusplus
}
#endif
