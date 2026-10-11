/**
 * @file transport_policy.h
 * @brief V2 connection session policy: BLE primary realtime, WiFi bulk/failover.
 *
 * Central place for decisions that used to be spread across ble_task and
 * web_server_task. Keeps the matrix/game tasks free of network I/O.
 */
#ifndef TRANSPORT_POLICY_H
#define TRANSPORT_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize policy state and log boot-time transport decision.
 *
 * Safe to call before WiFi or BLE are started. Does not start radios by itself.
 */
void transport_policy_init(void);

/**
 * @brief Decide whether the WiFi subsystem should be started at boot.
 *
 * Returns true when STA credentials are stored in NVS or when the user has
 * explicitly enabled the board hotspot (AP). Keeps WiFi off on a fresh board
 * with no credentials to save airtime/power on V2.
 */
bool transport_policy_should_enable_wifi_on_boot(void);

/**
 * @brief Ensure WiFi is started before a connection attempt.
 *
 * Idempotent wrapper around esp_wifi_start(). Used by lazy-start paths.
 */
void transport_policy_ensure_wifi_started(void);

/**
 * @brief Called when a BLE GAP connection comes up.
 *
 * Logs the realtime-path change, sets preferred low-latency connection
 * parameters, and shifts coexistence toward BLE while the link lasts.
 */
void transport_policy_on_ble_connect(uint16_t conn_handle);

/**
 * @brief Called when the BLE GAP connection drops.
 *
 * Logs the loss and shifts coexistence back toward WiFi.
 */
void transport_policy_on_ble_disconnect(void);

/** @brief Log that realtime commands currently travel over BLE. */
void transport_policy_log_realtime_path_ble(void);

/** @brief Log that realtime commands currently travel over WiFi. */
void transport_policy_log_realtime_path_wifi(void);

/** @brief Log automatic failover from BLE to WiFi. */
void transport_policy_log_failover_to_wifi(void);

#ifdef __cplusplus
}
#endif

#endif /* TRANSPORT_POLICY_H */
