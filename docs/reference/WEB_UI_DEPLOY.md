# Web UI — jak znovu zabalím JS do firmware

## Po úpravách `web/chess_app.js`

JavaScript na ESP32 servíruju z **vnořeného pole** v `web_server_task.c`. Když něco měním v `web/chess_app.js`, musím znovu vygenerovat C pole:

```bash
python3 components/web_server_task/tools/embed_chess_js.py
```

Skript přepíše `chess_app_js_content[]` v `web_server_task.c` obsahem aktuálního `web/chess_app.js`.

## Build a flash

```bash
idf.py build
idf.py flash monitor
```

## Nápověda (Stockfish) na webu

- Tlačítko **Nápověda** v záložce Hra (vedle Nová hra a Zkusit tahy) ukáže aktuálnímu hráči nejlepší tah.
- **FEN** skládám na klientu z `/api/board`, `/api/status` a `/api/history` (zjednodušeně castling/en passant). Před odesláním kontroluju rozumnou pozici (8×8 board, délka FEN).
- **Stockfish API:** používám **Chess-API.com** (starší Stockfish.online `/api/single` už mi vracelo 404).
  - Endpoint: `POST https://chess-api.com/v1`, tělo: `{ "fen": "...", "depth": 10 }` (depth max 18).
  - **Formát odpovědi**, který parser umí: tah buď `from` + `to` (řetězce), nebo `move` (4 znaky). Eval: `eval`, případně `centipawns`/`cp`, nebo `evaluation`/`score`. Volitelně `text`, `san`, `continuationArr`, `mate`, `winChance`. Odpověď může být v kořeni JSON nebo pod `data` / `result`. Odpověď chess-api.com typicky `{ "from": "e2", "to": "e4", "move": "e2e4", "eval": ..., ... }` — eval je z pohledu bílého (záporné = líp černý).
- **Chyby:** timeout 15 s (AbortController), kontrola `res.ok`, JSON s fallbackem. Při selhání na tlačítku krátce „Nedostupné“, při špatné pozici „Chyba pozice“, při výjimce „Chyba“. Loguju `[Hint]` do konzole.
- **Web:** po návratu tahu přidám `.hint-from` / `.hint-to`; při dalším `fetchData` / `updateBoard` hint zmizí.
- **LED:** po hintu posílám **POST /api/game/hint_highlight** s `{ "from": "e2", "to": "e4" }` → backend přepíše na LED indexy a `LED_CMD_HIGHLIGHT_HINT`.
- **CORS / rate limit:** kdyby mě něco blokovalo z prohlížeče, šlo by přidat proxy přes ESP32. Při HTTP 429 ukážu „Nedostupné“ — spamování tlačítkem nedává smysl.

## Rychlý checklist před releasem web části

- [ ] Záložka **Hra** — šachovnice, čas, stav, Nová hra, Nápověda, Zkusit tahy, historie
- [ ] Záložka **Nastavení** — LED, WiFi, stav webu, dálkové ovládání, zvednutá figurka, demo, MQTT
- [ ] Přepínání záložek bez červených chyb v konzoli
- [ ] Timer, Nová hra, sandbox, historie, review
- [ ] Na Nastavení: WiFi, demo checkbox a rychlost, uložení MQTT
- [ ] Bannery (Review, Sandbox, Endgame) nepřekrývají záložky
- [ ] Nápověda — za běžné partie načte Stockfish, ukáže se na webu i na LED

## Poznámky

- `embed_chess_js.py` v `tools/` nahrazuje blok od řádku s `// TEST PAGE - MINIMAL TIMER TEST` do řádku před `static esp_err_t http_get_chess_js_handler`.
- Starší `js_to_c.py` (také v `tools/`) jen vypisuje C pole na stdout; pro automatický zápis do `.c` používám `embed_chess_js.py`.
