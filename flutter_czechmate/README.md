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

## Azure AI Foundry (Coach)

Copy [`.env.example`](.env.example) → `.env`, fill the Azure placeholders, then add **Azure AI Foundry** under Settings → Coach & AI. Settings override `.env`. Foundry is the coach LLM provider. It is not firmware and it has no screen of its own. Details: [`docs/flutter/README.md`](../docs/flutter/README.md#azure-ai-foundry).

## Board behaviour

Czech write-up of the current tree: [`context/app/deska-a-aplikace.md`](../context/app/deska-a-aplikace.md).

- A known board reconnects when the main shell is up (`tryResumeFromPrefs`: saved BLE, then the Wi‑Fi URL), including when the app returns to the foreground. The first launch shows onboarding before that shell. A new board is accepted only while the 60 s pairing window is open (physical triple-click; a second triple-click closes it). The board remembers up to three devices and one commander. Wi‑Fi game commands need the Bearer token issued at pairing. A second known client, while the first holds the link, is told the board is busy and the move is refused. There is no spectator mode in the app.
- Four tabs: Play, Puzzle, Learn, Settings. Analysis is a page pushed from Play, not a tab. Dark theme and Czech are the defaults. During a game both clocks stay visible. Coordinates stay on (rank numbers on the left, file letters along the bottom; black-at-bottom puts h8 at the lower left).
- Puzzle: the app posts a FEN and a solver-first line (`load`). The board lights setup (missing / extra) and accepts a move only after the position matches, then judges the move immediately. At most 12 plies, session kept in RAM.
- Resign is in the Play “more” sheet, with a confirmation that names the side to move. The board command is `resign` with `side` `white` or `black` (BLE and `POST /api/game/resign`). There is no draw-offer command.

## Further reading

- [`docs/flutter/README.md`](../docs/flutter/README.md) — client structure, BLE/HTTP, diagrams
- [`ota_architecture.md`](../context/firmware/ota_architecture.md) — board firmware OTA
- [`docs/README.md`](../docs/README.md) — firmware diagrams, reference, …
