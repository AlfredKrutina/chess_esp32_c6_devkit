# Flutter CzechMate

Klient pro projekt **CZECHMATE** (ESP32 šachovnice). V gitu je hlavní mobilní klient tady; nativní Xcode projekt `CZECHMATE/` je záměrně mimo git.

[`docs/README.md`](../docs/README.md) — rozcestník dokumentace v repu.

## Spuštění

```bash
cd flutter_czechmate
flutter pub get
flutter run
```

## Stack

Flutter 3.x, Riverpod, `flutter_blue_plus`, HTTP + případně WebSocket, balíček `chess` pro pravidla na zařízení.

## Dokumentace

- [`docs/flutter/README.md`](../docs/flutter/README.md) — struktura klienta, BLE/HTTP, diagramy
- [`docs/ota_architecture.md`](../docs/ota_architecture.md) — OTA firmwaru desky
- [`docs/README.md`](../docs/README.md) — zbytek (firmware diagramy, reference, …)
