# Flutter CzechMate

Klient pro projekt **CZECHMATE** (ESP32 šachovnice). V gitu je hlavní mobilní klient tady; nativní Xcode projekt `CZECHMATE/` je záměrně mimo git.

## Spuštění

```bash
cd flutter_czechmate
flutter pub get
flutter run
```

## Stack

Flutter 3.x, Riverpod, `flutter_blue_plus`, HTTP + případně WebSocket, balíček `chess` pro pravidla na zařízení.

## Dokumentace

**Rozpis obrazovek, BLE vs HTTP, diagramy:** [`docs/flutter/README.md`](../docs/flutter/README.md)

Mapa celého repa (firmware + Flutter): [`docs/README.md`](../docs/README.md)
