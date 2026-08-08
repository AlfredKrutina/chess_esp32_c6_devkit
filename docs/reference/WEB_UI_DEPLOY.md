# Web UI — how I repackage JS into firmware

## After editing `web/chess_app.js`

Source is split under `web/js/` (Phase 4A). **`chess_app.js` is generated** — do not edit it by hand:

```bash
python3 components/web_server_task/tools/concat_web_js.py
```

Modules (concat order): `matrix_guard.js` → `api.js` → `prefs.js` → `app_main.js`.

For embed into firmware (if browser UI handler is re-enabled):

```bash
python3 components/web_server_task/tools/concat_web_js.py
python3 components/web_server_task/tools/embed_chess_js.py
```

## Build and flash

```bash
idf.py build
idf.py flash monitor
```

## Hint (Stockfish) on the web

- **Hint** button on the Game tab (next to New game and Try moves) shows the best move for the current player.
- **FEN** is built on the client from `/api/board`, `/api/status`, and `/api/history` (castling/en passant simplified). Before send I validate a reasonable position (8×8 board, FEN length).
- **Stockfish API:** I use **Chess-API.com** (older Stockfish.online `/api/single` returned 404).
  - Endpoint: `POST https://chess-api.com/v1`, body: `{ "fen": "...", "depth": 10 }` (depth max 18).
  - **Response format** the parser accepts: move as `from` + `to` (strings), or `move` (4 chars). Eval: `eval`, or `centipawns`/`cp`, or `evaluation`/`score`. Optional `text`, `san`, `continuationArr`, `mate`, `winChance`. JSON may be at root or under `data` / `result`. chess-api.com typically `{ "from": "e2", "to": "e4", "move": "e2e4", "eval": ..., ... }` — eval from White’s view (negative = better for Black).
- **Errors:** 15 s timeout (AbortController), check `res.ok`, JSON with fallback. On failure button briefly shows “Unavailable”; bad position “Position error”; exception “Error”. I log `[Hint]` to the console.
- **Web:** after a hint I add `.hint-from` / `.hint-to`; on next `fetchData` / `updateBoard` the hint clears.
- **LED:** after hint I send **POST /api/game/hint_highlight** with `{ "from": "e2", "to": "e4" }` → backend maps to LED indices and `LED_CMD_HIGHLIGHT_HINT`.
- **CORS / rate limit:** if blocked from the browser, a proxy via ESP32 could be added. On HTTP 429 I show “Unavailable” — spamming the button is pointless.

## Quick checklist before web release

- [ ] **Game** tab — board, clock, state, New game, Hint, Try moves, history
- [ ] **Settings** tab — LED, WiFi, web status, remote control, lifted piece, demo, MQTT
- [ ] Tab switching without red console errors
- [ ] Timer, New game, sandbox, history, review
- [ ] On Settings: WiFi, demo checkbox and speed, save MQTT
- [ ] Banners (Review, Sandbox, Endgame) do not cover tabs
- [ ] Hint — during a normal game loads Stockfish, shows on web and LEDs

## Notes

- `embed_chess_js.py` in `tools/` replaces the block from the line with `// TEST PAGE - MINIMAL TIMER TEST` to the line before `static esp_err_t http_get_chess_js_handler`.
- Older `js_to_c.py` (also in `tools/`) only prints a C array to stdout; for automatic write to `.c` I use `embed_chess_js.py`.
