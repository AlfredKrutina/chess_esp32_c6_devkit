# CzechMate → Flutter: plán, hotový základ, prompt na dokončení

## 1) Struktura iOS zdroje (mapování)

| Swift | Umístění |
|-------|----------|
| `GameViewState`, layout, prefs | `GameViewState.swift`, `GameContainerView` |
| Snapshot API | `Models/GameSnapshot.swift`, `JSONDecoder+GameSnapshot` |
| Šachovnice UI | `ChessBoardView.swift`, `SharedChessBoardView.swift` (shared) |
| BLE / WiFi | `ChessboardBLEClient.swift`, `BLEBoardTransport.swift`, `WiFiBoardTransport.swift` |
| HTTP | `ChessboardAPIClient+Extras.swift` |
| WebSocket | `ChessboardWebSocketClient.swift` |
| Discovery | `BoardDiscoveryView.swift`, `BoardConnectionStore.swift` |
| Coach / Gemma | `ChessGemmaInference.swift`, `ModelDownloadManager.swift` |
| FEN | `FENParser.swift`, `FENBuilder.swift`, `CZECHMATEShared/...` |

## 2) Fáze portu (pořadí)

1. **Modely + JSON** — `GameStatus`, normalizace desky (`GameSnapshot+BoardNormalization`), stejné klíče jako firmware.
2. **Stav** — rozšířit `GameState` o sandbox, výběr pole, remote moves, layout režimy; případně rozdělit na více notifierů (connection vs game).
3. **Šachovnice** — Unicode jen dočasně; převést `ChessPieceGlyph`; animace tahu (`AnimationController` + `Tween` offset nebo `Hero`); pinch (`InteractiveViewer`); highlight vrstvy jako ve Swift.
4. **Služby** — doplnit `BoardApiClient` (POST tah, timer, ETag), `WebSocketManager`, `BleClient` (UUID služeb z iOS).
5. **Discovery** — sloučit logiku LAN + BLE ze Swift do jednoho flow + ukládání poslední desky (`shared_preferences`).
6. **Coach** — cloud API nebo `tflite_flutter`; stáhnutí modelu analogicky k `ModelDownloadManager`.
7. **Tablet / phone** — rozšířit breakpointy a dva sloupce jako `GameWideLayoutView`.

## 3) Co už je ve workspace

- Složka **`flutter_czechmate/`**: `pubspec.yaml`, `lib/main.dart`, základ `core/`, `features/game`, `features/connection`.
- Spuštění: `cd flutter_czechmate && flutter pub get && flutter run --dart-define=STAGING=true`
- Staging logika: `lib/core/constants/app_environment.dart`

## 4) Prompt pro tebe (coding monkey část)

Zkopíruj agentovi / sobě:

---

Jsi pokračování portu CzechMate (Flutter) v repu `free_chess_v1_mqtt_HA`.

**Kontext:** V `flutter_czechmate/` je kostra (Riverpod `StateNotifier`, `GameSnapshot.fromJson`, `BoardApiClient.fetchSnapshot`, `ChessBoardWidget` s 8×8 a flip, `BoardDiscoveryScreen` → `GameScreen`). iOS zdroj je v `CZECHMATE/` a `CZECHMATE/CZECHMATEShared/`.

**Úkoly:**

1. Srovnej `GameSnapshot` / `GameStatus` s `JSONDecoder+GameSnapshot` a Swift modely — přidej typovaný `GameStatus` místo `Map`, včetně chybějících polí a testů (`context/snapshot_golden.json`).
2. Implementuj legální tahy a sandbox režim: převeď logiku z `GameViewState` (výběr, promoce, undo stack) — zvaž balíček `chess` nebo vlastní minimální pravidla konzistentní s firmware notací (`FirmwareSquareNotation.swift`).
3. `ChessBoardWidget`: gesta (tap), animace figur, `InteractiveViewer` pro zoom, styly wooden/sandbox, valid/chybný highlight podle Swift.
4. `BoardApiClient`: všechny endpointy z `ChessboardAPIClient+Extras.swift` + odeslání tahů, polling s `state_version` / ETag pokud existuje.
5. `WebSocketManager` podle `ChessboardWebSocketClient.swift` — reconnect, zprávy → invalidace providerů.
6. `BleClient` + discovery UI podle `BoardDiscoveryView` / `BoardConnectionStore` — použij `flutter_blue_plus`, permissions v `AndroidManifest` / iOS `Info.plist`.
7. `TimerDisplay` napoj na `clock` ze snapshotu nebo samostatný timer endpoint.
8. Přidej `features/coach/` a `features/settings/` s `shared_preferences` (@AppStorage ekvivalent).
9. Odstraň technický dluh: sjednoť `GameState.copyWith` (explicitní nullable), odstraň dočasné textové figury pokud přidáš glyphy/asset.

**Omezení:** Nemazej soubory bez schválení vlastníka; drž se struktury `lib/core`, `lib/features`; staging debug nech za `AppEnvironment.staging`.

---

## 5) Stav Flutter portu (2026)

- V `flutter_czechmate/` je aplikace s `NavigationBar` (Hra / Deska / Coach / Nastavení), `BoardSessionNotifier` (Wi‑Fi poll + WS + BLE notify + mock asset), typované modely (`GameStatus`, `BoardTimerState`), sandbox přes balíček `chess`, REST podle Swift API.
- **`command not found: flutter`**: nainstaluj SDK — viz `context/flutter_czechmate_port/INSTALACE_FLUTTER.txt` (Homebrew: `brew install --cask flutter`). Flutter není přes `pip`.
- Po instalaci: `./flutter_czechmate/run_dev.sh` (najde `flutter` v Homebrew cestách, spustí `pub get`, případně `flutter create .`, pak `run`).
- Ručně: `cd flutter_czechmate && flutter pub get && flutter run --dart-define=STAGING=true`
- Pokud chybí `android/` / `ios/`: `flutter create .` v `flutter_czechmate/`. Pro BLE na Androidu 12+ doplň oprávnění podle `flutter_blue_plus`.

## 6) Otevřené otázky (až narazíš)

- Přesný formát buněk v `board[][]` vs FEN normalizace (velikost písmen, prázdné řetězce) — ověř proti firmware a `GameSnapshot+BoardNormalization.swift`.
- Zda WebSocket posílá celý snapshot nebo jen delta — z `ChessboardWebSocketClient.swift`.
- Minimální Android/iOS BLE oprávnění pro váš hardware UUID.
