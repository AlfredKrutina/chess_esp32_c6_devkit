/**
 * @file ble_task.h
 * @brief BLE rozhraní CZECHMATE — NimBLE GATT při CONFIG_BT_ENABLED.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ble_task_init(void);

/**
 * Odešle JSON snapshot připojenému centrálu (chunkovaně, hlavička CM).
 * Bez CONFIG_BT_ENABLED nebo bez BLE spojení je no-op.
 */
void ble_task_push_snapshot_json(const uint8_t *data, size_t len);

/** Jedna řádka stavu BLE (NimBLE / GATT) pro UART příkaz BLE. */
void ble_task_format_status(char *buf, size_t cap);

#ifdef __cplusplus
}
#endif
