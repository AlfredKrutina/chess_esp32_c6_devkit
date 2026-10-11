# CzechMate — client integration checklist

- **Network:** phone and ESP on the same LAN via board **STA** (home Wi‑Fi), or via **board hotspot** (`192.168.4.x`) — hotspot is often **off** by default and enabled from the app over BLE. Base URL in the app is typically `http://<STA_IP>` or `http://192.168.4.1` when the phone is on the hotspot network.
- **REST:** `GET /api/game/snapshot` returns `state_version` and an `ETag` header; with `If-None-Match` I get **304** with no body.
- **Brightness:** `POST /api/settings/brightness` with `{"brightness":0…100}` — on iOS from `SettingsTabView` / `ChessboardAPIClient.postBrightness`.
- **WebSocket:** `ws://<host>/ws`, same JSON as snapshot; push on change + ~3 s watchdog.
- **iOS:** with live WebSocket I poll REST every ~25 s as a safety net; in DEBUG I log `[staging]`.
- **BLE:** `CONFIG_BT_ENABLED` + NimBLE (`sdkconfig.defaults`). `ble_task_init()` calls **`ble_nimble_stack_init()`** → GATT in [`ble_nimble_impl.c`](../../components/ble_task/ble_nimble_impl.c). Without BT only a “BLE disabled” message.
- **Firmware build:** `source $IDF_PATH/export.sh && ./scripts/idf_build.sh`
- **Matrix guard** (pause on sensor vs logic mismatch): see [MATRIX_GUARD.md](MATRIX_GUARD.md). Snapshot/status fields: `matrix_guard_active`, `matrix_guard_conflicts`, `matrix_guard_*_mask_*`, `restore_state.resync_required`. Emergency clear: `POST /api/game/guard_clear`, BLE `{"cmd":"guard_clear"}`, UART `GUARD_CLEAR`.
- **iOS build (local Xcode project):** `xcodebuild -scheme CZECHMATE -project CZECHMATE/CZECHMATE.xcodeproj -destination 'platform=iOS Simulator,name=iPhone 17' build`
