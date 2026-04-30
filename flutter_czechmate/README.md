# Flutter CzechMate (`flutter_czechmate/`)

Multiplatformní klient k projektu **CZECHMATE** (ESP32 šachovnice + webová vrstva). Dart balíček v `pubspec.yaml` je pojmenovaný `czechmate`; složka v repu zůstává `flutter_czechmate/`.

## Stack

- Flutter 3.x, **Riverpod**
- **BLE:** `flutter_blue_plus`
- **Síť:** `http`, `web_socket_channel` (komunikace s deskou přes HTTP/WS dle nasazení)
- Šachová logika: balíček `chess`
- Analýza / engine: integrace Stockfish přes API klienta v repu (viz `lib/core/services/`)

## Spuštění

```bash
cd flutter_czechmate
flutter pub get
flutter run
```

Vyžaduje nainstalovaný [Flutter SDK](https://docs.flutter.dev/get-started/install).

## Oblasti v repu (orientace)

- `lib/features/game/` — hra, šachovnice, hodiny  
- `lib/features/connection/` — BLE / session  
- `lib/features/coach/` — AI trenér  
- `lib/core/services/` — BLE, board API, prefs, live activity, hodinky, …  
- **iOS:** rozšíření pro **Live Activities** (`ios/ChessLiveActivityExtension/`, `LiveActivityNativeController.swift`)  
- **Android:** modul **Wear OS** (`android/wear/`), notifikace šachových hodin  

Podrobnosti k firmware a nativní iOS aplikaci: kořenový [`README.md`](../README.md), [`docs/reference/`](../docs/reference/), [`docs/diagrams/`](../docs/diagrams/).
