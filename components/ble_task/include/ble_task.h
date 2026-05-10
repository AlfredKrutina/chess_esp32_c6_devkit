/**
 * @file ble_task.h
 * @brief BLE rozhraní CZECHMATE — NimBLE GATT při CONFIG_BT_ENABLED.
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

/** True pokud je aktivní GATT spojení se zapnutým link encryption (SMP). */
bool ble_task_conn_is_encrypted(void);

/**
 * True, pokud je BLE link aktivní a centrál má zapnuté notify na snapshot CCC.
 * Jinak je push snapshot no-op — web server nemusí každé 3 s skládat JSON jen „do prázdna“.
 */
bool ble_task_should_push_snapshot(void);

/**
 * Odešle JSON snapshot připojenému centrálu (chunkovaně, hlavička CM).
 * Bez CONFIG_BT_ENABLED nebo bez BLE spojení je no-op.
 */
void ble_task_push_snapshot_json(const uint8_t *data, size_t len);

/**
 * Odešle network info (IP, SSID, online status) připojenému centrálu.
 * Volat při změně IP adresy nebo WiFi statusu.
 * Bez CONFIG_BT_ENABLED nebo bez BLE spojení je no-op.
 */
void ble_task_push_network_info(void);

/** Jedna řádka stavu BLE (NimBLE / GATT) pro UART příkaz BLE. */
void ble_task_format_status(char *buf, size_t cap);

/**
 * Po zpracování GATT zápisu na CMD charakteristiku odešle JSON na cmd_ack notify
 * (pokud je centrál přihlášen k notifikacím). Viz kanál "cmd_ack" v protokolu.
 */
void ble_task_notify_command_result(esp_err_t err, const char *json_body);

/** Celý JSON pro cmd_ack notify (např. wifi_survey až ~2 KiB). Použití jen z web_server BLE dispatch. */
void ble_task_notify_cmd_ack_json(const char *json_utf8);

#ifdef __cplusplus
}
#endif
