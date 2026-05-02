# Kontext — úpravy CzechMate (Flutter)

Shrnutí změn z posledních session (pro AI / vývojáře).

## Remote tahy & neplatné tahy

- **Před odesláním** na desku: `validateRemoteMoveLegality()` v `lib/core/services/mock_board_simulator.dart` (stejná logika jako mock) — důležité pro **BLE**, kde chyba nejde spolehlivě vrátit z API.
- Při zamítnutí (validace nebo HTTP 400): haptika, zpráva, **červený pulz** na cílovém poli (`invalidDestinationPulseSquare` / `invalidDestinationPulseLit` v `game_ui_state.dart`).
- Pulz **zkrácený**: `Timer.periodic` **200 ms**, **4** ticky (~0,8 s), pak `clearInvalidDestinationPulse` — viz `GameUiNotifier._startInvalidDestinationPulse`.
- **Deska / error_state**: `chess_board_widget` — animovaný pulz serverového neplatného pole; klient vs. server barvy v `_BoardPainter`.
- `setHistoryReviewIndex` a návrat z historie: při klepnutí na šachovnici v režimu náhledu historie se jen ukončí náhled (`return`), **nepokračuje** se jako remote tah; reset `_remoteFrom`.

## Historie tahů (Move history)

- **Živá pozice** jako první řádek v listu; tahy **od nejnovějšího** dolů.
- **Žluté zvýraznění** posledního tahu: při `historyReviewMoveIndex != null` se bere `moves[index]`, jinak `moves.last` — `chess_board_widget.dart`.

## Onboarding

- `OnboardingScreen`: krok **jméno** → ukládá `PrefsRepository.setProfileDisplayName`; **oprávnění** (fotky, BT, na Androidu `nearbyWifiDevices`) → pak původní 5 slidů.
- `profileDisplayNameStoredOrNull` v `prefs_repository.dart` pro předvyplnění.
- Android: v manifestu `NEARBY_WIFI_DEVICES` (`neverForLocation`).

## Knihovna puzzlů

- Mazání položky: **dialog** s potvrzením (`puzzleLibraryRemoveTitle/Body/Confirm` v `extra_l10n_rows.py`); odstranění podle `id`, ne indexu.

## L10n / build

- Chybějící klíče (firmware, Wi‑Fi BLE, OTA, …) doplněné v `tool/extra_l10n_rows.py` — po změnách spustit `python3 tool/generate_arb.py` a `flutter gen-l10n`.

## Soubory často relevantní

| Oblast | Soubory |
|--------|---------|
| Remote tahy | `game_ui_notifier.dart`, `mock_board_simulator.dart`, `board_session_notifier.dart` |
| Deska UI | `chess_board_widget.dart`, `game_ui_state.dart` |
| Historie | `move_history_view.dart`, `game_ui_notifier.dart` (`effectiveBoard`) |
| Onboarding | `onboarding_screen.dart`, `main.dart` (`_StartupRouter`), `prefs_repository.dart` |
| Knihovna | `puzzle_screen.dart` |
| L10n zdroj | `tool/extra_l10n_rows.py`, `tool/generate_arb.py` |

---
*Aktualizováno: shrnutí session (invalid tahy, historie, onboarding, knihovna, l10n, zkrácení pulzu).*
