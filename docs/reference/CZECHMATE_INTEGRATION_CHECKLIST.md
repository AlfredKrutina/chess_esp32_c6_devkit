# CzechMate — co kontroluju při integraci klienta

- **Síť:** telefon a ESP ve stejné LAN přes **STA** desky (domácí Wi‑Fi), nebo přes **hotspot desky** (`192.168.4.x`) — hotspot je ve výchozím stavu často **vypnutý** a zapíná se z aplikace přes BLE. Base URL v appce typicky `http://<STA_IP>` nebo `http://192.168.4.1` když je telefon na síti hotspotu.
- **REST:** `GET /api/game/snapshot` vrací `state_version` a hlavičku `ETag`; s `If-None-Match` dostanu **304** bez těla.
- **Jas:** `POST /api/settings/brightness` s `{"brightness":0…100}` — na iOS z `SettingsTabView` / `ChessboardAPIClient.postBrightness`.
- **WebSocket:** `ws://<host>/ws`, stejný JSON jako snapshot; push při změně + watchdog ~3 s.
- **iOS:** při živém WebSocket volám REST každých ~25 s jako pojistku; v DEBUG loguju `[staging]`.
- **BLE:** `CONFIG_BT_ENABLED` + NimBLE (`sdkconfig.defaults`). `ble_task_init()` volá **`ble_nimble_stack_init()`** → GATT v [`ble_nimble_impl.c`](../../components/ble_task/ble_nimble_impl.c). Bez BT jen hláška „BLE vypnuto“.
- **Build firmware:** `source $IDF_PATH/export.sh && ./scripts/idf_build.sh`
- **Build iOS (lokální Xcode projekt):** `xcodebuild -scheme CZECHMATE -project CZECHMATE/CZECHMATE.xcodeproj -destination 'platform=iOS Simulator,name=iPhone 17' build`
