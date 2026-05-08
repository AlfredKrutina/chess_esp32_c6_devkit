# Flutter CzechMate

V gitu je hlavní mobilní / desktop klient pro **CzechMate** (ESP32 šachovnice). Nativní Xcode projekt `CZECHMATE/` záměrně není v tomhle remote — držím ho jen lokálně.

[`docs/README.md`](../docs/README.md) — zbytek dokumentace.

## Spuštění

```bash
cd flutter_czechmate
flutter pub get
flutter run
```

## Stack

Flutter 3.x, Riverpod, `flutter_blue_plus`, HTTP + případně WebSocket, balíček `chess` pro pravidla na zařízení.

## Kam dál číst

- [`docs/flutter/README.md`](../docs/flutter/README.md) — struktura klienta, BLE/HTTP, diagramy
- [`docs/ota_architecture.md`](../docs/ota_architecture.md) — OTA firmwaru desky
- [`docs/README.md`](../docs/README.md) — firmware diagramy, reference, …
