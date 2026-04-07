# CZECHMATE — integrační checklist

- **Síť:** Telefon a ESP ve stejné LAN (AP `ESP32-CzechMate` nebo STA); v aplikaci `http://192.168.4.1` nebo IP ESP.
- **REST:** `GET /api/game/snapshot` vrací `state_version` a hlavičku `ETag`; `If-None-Match` → 304 bez těla.
- **Nastavení:** `POST /api/settings/brightness` s `{"brightness":0…100}` — iOS `SettingsTabView` / `ChessboardAPIClient.postBrightness`.
- **WebSocket:** `ws://<host>/ws`, stejný JSON jako snapshot; push při změně stavu + watchdog 3 s.
- **iOS:** Při živém WebSocket REST každých ~25 s jako watchdog; DEBUG logy `[staging]`.
- **BLE:** Zapnout `CONFIG_BT_ENABLED` a NimBLE v menuconfig; zatím `ble_task_init()` je stub.
- **Build firmware:** `source $IDF_PATH/export.sh && ./scripts/idf_build.sh`
- **Build iOS:** `xcodebuild -scheme CZECHMATE -project CZECHMATE/CZECHMATE.xcodeproj -destination 'platform=iOS Simulator,name=iPhone 17' build`
