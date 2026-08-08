# Flutter CzechMate

The main mobile / desktop client for **CzechMate** (ESP32 chess board) lives in this repo. The native Xcode project `CZECHMATE/` is intentionally not in this remote — I keep it locally only.

[`docs/README.md`](../docs/README.md) — rest of the documentation.

## Run

```bash
cd flutter_czechmate
flutter pub get
flutter run
```

## Stack

Flutter 3.x, Riverpod, `flutter_blue_plus`, HTTP + optional WebSocket, `chess` package for on-device rules.

## Further reading

- [`docs/flutter/README.md`](../docs/flutter/README.md) — client structure, BLE/HTTP, diagrams
- [`docs/ota_architecture.md`](../docs/ota_architecture.md) — board firmware OTA
- [`docs/README.md`](../docs/README.md) — firmware diagrams, reference, …
