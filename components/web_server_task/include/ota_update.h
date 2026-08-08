/**
 * @file ota_update.h
 * @brief OTA: HTTPS/HTTP from URL, or stream via BLE (chunks on CMD char).
 */
#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include "esp_err.h"
#include "esp_http_server.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_update_register_http_handlers(httpd_handle_t hd);

/**
 * @brief BLE JSON {"cmd":"ota_start","url":"https://…"}
 * @return ESP_OK if the command has been received and scheduled (or is already running), otherwise error.
 */
esp_err_t ota_update_ble_try_dispatch(const char *json_buf);

/** BLE JSON {"cmd":"ota_ble_begin","size":<bytes>} before `OB…` chunks. */
esp_err_t ota_update_ble_begin_from_json(const char *json_buf);

/** Cancels the ongoing BLE OTA stream (releases the semaphore). */
esp_err_t ota_update_ble_abort(void);

/**
 * Call from GAP disconnect (NimBLE).
 * After a line failure after `ota_ble_begin` otherwise the RX status + held semaphore will remain
 * and another `ota_ble_begin` ends with ESP_ERR_INVALID_STATE ("busy").
 */
void ota_update_ble_on_disconnect(void);

/**
 * BLE CMD write with `OB` header + uint16 LE chunk_idx + uint16 LE chunk_total + raw payload.
 * After the last byte according to `size` from begin, esp_ota_end and restart.
 */
esp_err_t ota_update_ble_feed_chunk(const uint8_t *data, size_t len);

/**
 * @brief Actively receiving BLE OTA chunks (not paused).
 * To suppress snapshot notify / secondary load during transfer.
 */
bool ota_update_ble_is_rx_active(void);

/**
 * @brief JSON for cmd_ack notify (≤ ~380 B): channel/cmd_ack + ota_ble_status field.
 * @return ESP_OK or ESP_ERR_INVALID_STATE if the buffer is small.
 */
esp_err_t ota_update_ble_build_status_ack_json(char *buf, size_t cap);

/** Same as HTTP POST /api/system/ota — from UART to STA and https URL. */
esp_err_t ota_update_try_start_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif
