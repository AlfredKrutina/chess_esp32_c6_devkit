/**
 * @file board_api_auth.h
 * @brief Board REST API token (NVS) + HTTP Bearer kontrola pro citlivé endpointy.
 *
 * ## Auth matrix (HTTP)
 * - Veřejné GET (bez Bearer): např. `/api/status`, `/api/board`, `/api/game/snapshot`,
 *   `/api/wifi/status`, `/api/system/firmware`, `/api/system/ota/status` (onboarding).
 * - Admin POST (`/api/system/ota`, `/api/system/factory_reset`, cesty pod `/api/wifi/`, …):
 *   platný Bearer vždy stačí. Pokud je **WEB_LOCK zapnutý** a Bearer chybí nebo neplatí → 403 `web_locked`.
 *   Pokud je **WEB_LOCK vypnutý** (výchozí), POST z LAN projde bez hlavičky (stejně jako otevřené GET API).
 *
 * Token: 32 B náhodných, v hlavičce jako `Authorization: Bearer <64 hex znaků>`.
 *
 * ## BLE (viz ble_task_conn_is_encrypted)
 * - `ota_start`, `ota_ble_begin`, `ota_ble_abort`, `ota_ble_status`, `wifi_sta_config`,
 *   `wifi_ap_set`, `factory_reset`: vyžadují aktivní šifrovaný odkaz (po SMP).
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/** NVS inicializace / generace tokenu při prvním spuštění. */
esp_err_t board_api_auth_init(void);

/**
 * HTTP admin akce: zkontroluje Bearer token vs NVS a web lock.
 * @return true pokud už byl odeslán 403 a handler má vrátit ESP_OK.
 */
bool board_api_auth_admin_http_denied(httpd_req_t *req);

/** UART / diagnostika: hex řetězec (64 znaků) + '\\0'. */
esp_err_t board_api_auth_get_token_hex(char *out_hex, size_t cap);

/** Vygeneruje nový token (starý přestane platit). */
esp_err_t board_api_auth_rotate_token(void);

#ifdef __cplusplus
}
#endif
