# OTA firmwaru ESP32-C6 (FW + Flutter)

ESP32 si umí stáhnout nový aplikační obraz po **HTTPS** (internet, vyžaduje STA), po **HTTP** z LAN (typicky telefon hostuje `.bin` na hotspotu desky), nebo ho přijmout po **BLE** jako stream chunků `OB`. Flutter řídí start, polling stavu přes HTTP (kde jde), nebo nahrává chunky přes GATT.

STM32 hall MCU se tímto protokolem neaktualizuje. Flash jen přes `factory` bez `ota_0`/`ota_1` vrací `ota_supported: false` a HTTP OTA vrací 503 — pak zbývá UART / esptool.

**Roadmap:** Později chci přejít z vlastní OTA logiky přímo ve firmware na **OTAvo** (aktuální chování v `ota_update.c` / související BLE a HTTP handlery pak nahradí nebo obalí ten stack).

---

## 1. Partition a synchronizace

- `ota_partition_layout_ok()` v `components/web_server_task/ota_update.c` kontroluje přítomnost **oba** oddílů `APP_OTA_0` a `APP_OTA_1`. Jinak `GET /api/system/firmware` → `ota_supported: false`, `POST /api/system/ota` → **503**.
- Po úspěchu: `esp_ota_set_boot_partition()` + `esp_restart()` (všechny tři cesty).
- `s_ota_sem`: jen jeden běžící OTA. Druhý start → HTTP **409**, BLE často `ESP_ERR_INVALID_STATE` („busy“).

---

## 2. Kanály

| | URL | Kdo stahuje / píše | STA |
|---|-----|-------------------|-----|
| HTTPS | `https://…` | `esp_https_ota` + CA bundle | povinné |
| HTTP | `http://…` | `esp_http_client` + `esp_ota_write` | ne (LAN / AP) |
| BLE | — | telefon `OB` write na CMD char | ne |

**Debug:** `CHESS_DEBUG_MODE` → v `ota_update.c` logy `[STAGING]`; u GATT `ble_nimble_impl.c`. Chunk bez šifrování: `OTA BLE chunk rejected: link not encrypted`.

---

## 3. Síť

- AP desky typicky `192.168.4.1`; telefon na hotspotu `192.168.4.x`. `FirmwarePhoneHostOta.ipv4OnBoardApSubnet()` vybírá IP do URL.
- Na domácí LAN: `ipv4OnSameSubnet24As(boardStaIp)` pokud telefon není na 4.x.
- `FirmwarePhoneHostOta.startServingBin`: `HttpServer` na `0.0.0.0`, volný port, jen `GET /czechmate_ota.bin`, `Content-Length`, stream souboru.

---

## 4. REST (`ota_update_register_http_handlers`)

### `GET /api/system/firmware`

Bez Bearer. `version`, `project_name`, `idf`, `ota_supported`.

### `GET /api/system/ota/status`

Bez Bearer. `state`: `idle` | `downloading` | `done` | `error`; `percent`; `message` (poslední chyba z `s_last_err`). Platí i během BLE streamu (stejný globální stav).

### `POST /api/system/ota`

Admin: Bearer + web lock podle `board_api_auth.h`. Tělo:

```json
{"url":"https://example/firmware.bin"}
```

`http_post_ota` čte tělo do ~1536 B — drž JSON krátký.

| Kód | Význam |
|-----|--------|
| 202 | `schedule_ota` OK |
| 409 | busy |
| 428 | HTTPS bez STA |
| 400 | prázdné / JSON / špatná URL |
| 503 | chybí OTA oddíly |
| 403 | token / web lock |
| 500 | fronta / interní |

Worker: `ota_https_worker_task` nebo `ota_http_worker_task`.

---

## 5. Workery na desce

**HTTPS:** `esp_https_ota_*`, timeout klienta 120 s, progress z velikosti obrazu.

**HTTP:** `esp_http_client_open`, status 200, `esp_ota_begin` → read loop → `esp_ota_write`, timeout 300 s, progress z `Content-Length` nebo z velikosti partition.

Chyba: `led_ota_restore_board_after_update_abort()`, `xSemaphoreGive(s_ota_sem)`.

---

## 6. BLE — JSON (`web_server_ble_command_dispatch`)

Šifrovaný link povinný (`ble_task_conn_is_encrypted`). Jinak `needs_encryption` přes `ble_dispatch_ack_needs_encryption`.

| Příkaz | Akce |
|--------|------|
| `ota_start` + `url` | `schedule_ota(url)` — stejné jako POST |
| `ota_ble_begin` + `size` | Stream; `size` ≥ 32 KiB, ≤ partition |
| `ota_ble_abort` | abort + uvolnění semaforu |
| `ota_ble_status` | JSON z `ota_update_ble_build_status_ack_json` + notify |

`ble_task_notify_command_result`: mapuje `esp_err` na `code` / `message`, v JSON je i `"esp": <číslo>`. `ESP_ERR_NOT_ALLOWED` (HTTPS bez STA) spadne do default větve — UART log je konkrétnější.

---

## 7. BLE — chunky `OB`

Stejná GATT CMD charakteristika jako JSON.

| Bajt | Význam |
|------|--------|
| 0–1 | `'O'` `'B'` |
| 2–3 | `chunk_idx` LE u16 |
| 4–5 | `chunk_total` LE u16 |
| 6+ | payload |

Validace v `ota_update_ble_feed_chunk`: pořadí indexů, shoda `chunk_total`, součet payloadů = `size` z `ota_begin`, poslední chunk = `chunk_total - 1` při plném součtu.

**NimBLE:** prvních max 768 B z mbuf do stacku — jeden write od klienta musí vejít do ATT MTU (kvůli chunkům drží Flutter `payloadMax` konzervativně, hlavně iOS).

Nešifrovaný link → `INSUFFICIENT_AUTHOR`. Špatný chunk → notify `{"cmd":"ota_ble_chunk","ok":false}`; úspěšné chunky notify nemají (iOS fronta).

**Stavy:** `IDLE` → `RX` po begin. Disconnect v `RX` → `SUSPENDED` + timer **24 h**; po timeoutu abort. První platný chunk po suspend → zase `RX`.

Klient po výpadku: reconnect, `ota_ble_status`, pokračovat od `bytes` / `next_chunk` — viz `BleCzechmateClient.uploadFirmwareBle`.

---

## 8. Flutter

```mermaid
sequenceDiagram
  participant UI as FirmwareUpdateSection
  participant Runner as FirmwareOtaRunner
  participant Sess as BoardSessionNotifier
  participant API as BoardApiClient
  participant BLE as BleCzechmateClient

  Note over UI,BLE: Phone-host + poll
  UI->>Runner: execute + preferHttpOtaStart
  Runner->>API: firmware info, WiFi pokud https
  Runner->>Sess: requestFirmwareOta
  Sess->>API: postBoardOtaStart
  Runner->>API: poll ota/status 500 ms

  Note over UI,BLE: BLE stream
  UI->>Sess: uploadFirmwareOtaBle
  Sess->>BLE: uploadFirmwareBle OB chunks
```

- **Bearer:** `BoardApiClient.resolveBoardApiBearerToken` ← `PrefsRepository.boardApiToken` (`app_providers.dart`). `postBoardOtaStart` mapuje 409, 428, 503, 403 na `BoardApiException`.
- **`FirmwareOtaRunner.execute`:** vyřeší `baseUrl` (`board_http_base_url.dart`), zkontroluje `ota_supported`, u `https://` ověří STA přes `fetchWiFiStatus`, zavolá `requestFirmwareOta`, pak `_pollOta` (500 ms, max 1200 cyklů). Po `downloading` a výpadku HTTP bere úspěch jako reboot heuristika v kódu.
- **`requestFirmwareOta`:** `preferHttpOtaStart` → vždy HTTP POST na zadanou base URL i při BLE session (HTTP na AP běží vedle BLE). Čistý BLE bez toho → `_ble.postOtaStart` (`ota_start`). HTTP URL z telefonu patří spíš přes `preferHttpOtaStart`.
- **`uploadFirmwareBle`:** MTU 517, `payloadMax` pod platformu, retry na chunk, iOS gap; resume přes `OtaBleStatus` po disconnect.
- **UI:** `firmware_update_section.dart` — cache bin přes prefs, phone-host + `FirmwareOtaRunner`, nebo `_sendFirmwareViaBle` jen s lokálním `onProgress` (bez runner poll).

---

## 9. Shrnutí chování

| Režim | Start | Progress v app |
|-------|--------|----------------|
| HTTPS | POST nebo BLE `ota_start` | `GET .../ota/status` v runneru |
| HTTP z telefonu | POST + `preferHttpOtaStart` | stejně |
| BLE stream | `ota_ble_begin` + OB | callback bajtů; HTTP status lze číst paralelně pokud znáš base URL |

---

## 10. Ladění

| Projev | Kde |
|--------|-----|
| 428 | `schedule_ota`, Flutter WiFi status před startem |
| 409 | paralelní OTA |
| 403 | token / lock |
| needs_encryption | bonding / SMP |
| iOS GATT 8 | menší payload, delay — `ble_czechmate_client.dart` |
| po disconnect nový begin „busy“ | 24 h suspend nebo chybějící abort — `ota_update.h` |

---

## 11. Soubory

| FW | Dart |
|----|------|
| `components/web_server_task/ota_update.c` | `features/settings/firmware_ota_runner.dart` |
| `components/web_server_task/include/ota_update.h` | `core/services/board_api_client.dart` |
| `components/web_server_task/web_server_task.c` | `features/connection/board_session_notifier.dart` |
| `components/ble_task/ble_nimble_impl.c` | `core/services/ble_czechmate_client.dart` |
| `components/web_server_task/include/board_api_auth.h` | `core/services/firmware_phone_host_ota.dart` |
| | `features/settings/widgets/firmware_update_section.dart` |
