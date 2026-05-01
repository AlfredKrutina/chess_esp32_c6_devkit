/**
 * @file ota_update.h
 * @brief HTTPS OTA — ESP si stáhne .bin z URL (telefon jen pošle odkaz).
 */
#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_update_register_http_handlers(httpd_handle_t hd);

/**
 * @brief BLE JSON {"cmd":"ota_start","url":"https://…"}
 * @return ESP_OK pokud příkaz byl přijat a naplánován (nebo už běží), jinak chyba.
 */
esp_err_t ota_update_ble_try_dispatch(const char *json_buf);

#ifdef __cplusplus
}
#endif

#endif
