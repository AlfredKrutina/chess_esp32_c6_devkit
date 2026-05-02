/**
 * @file ota_update.h
 * @brief OTA: HTTPS/HTTP z URL, nebo stream přes BLE (chunky na CMD char).
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
 * @return ESP_OK pokud příkaz byl přijat a naplánován (nebo už běží), jinak chyba.
 */
esp_err_t ota_update_ble_try_dispatch(const char *json_buf);

/** BLE JSON {"cmd":"ota_ble_begin","size":<bytes>} před chunky `OB…`. */
esp_err_t ota_update_ble_begin_from_json(const char *json_buf);

/** Zruší probíhající BLE stream OTA (uvolní semafor). */
esp_err_t ota_update_ble_abort(void);

/**
 * BLE CMD write s hlavičkou `OB` + uint16 LE chunk_idx + uint16 LE chunk_total + raw payload.
 * Po posledním bajtu podle `size` z begin provede esp_ota_end a restart.
 */
esp_err_t ota_update_ble_feed_chunk(const uint8_t *data, size_t len);

/** Stejné jako HTTP POST /api/system/ota — z UART po STA a https URL. */
esp_err_t ota_update_try_start_url(const char *url);

#ifdef __cplusplus
}
#endif

#endif
