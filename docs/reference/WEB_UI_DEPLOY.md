# Web UI – deploy a ověření

## Před deployem (po změnách v `chess_app.js`)

JavaScript se na ESP32 servíruje z **vnořeného pole** v `web_server_task.c`. Po úpravách v `chess_app.js` je nutné znovu vložit JS do C:

```bash
cd components/web_server_task
python3 embed_chess_js.py
```

Skript přepíše pole `chess_app_js_content[]` v `web_server_task.c` obsahem aktuálního `chess_app.js`.

## Build a flash

```bash
idf.py build
idf.py flash monitor
```

## Nápověda (Stockfish)

- Tlačítko **Nápověda** v záložce Hra (vedle Nová hra a Zkusit tahy) zobrazí aktuálnímu hráči nejlepší tah.
- **FEN** se skládá na klientu z `/api/board`, `/api/status` a `/api/history` (zjednodušeně castling/en passant). Před odesláním se validuje (8×8 board, rozumná délka FEN).
- **Stockfish API:** Používá se **Chess-API.com** (Stockfish.online API v1 `/api/single` bylo zrušeno a vrací 404).
  - Endpoint: `POST https://chess-api.com/v1`, tělo: `{ "fen": "...", "depth": 10 }` (depth max 18).
  - **Očekávaný formát odpovědi** (pro nápovědu, hodnocení tahů i bota): tah buď `from` + `to` (řetězce, např. `"e2"`, `"e4"`), nebo `move` (4 znaky, např. `"e2e4"`). Eval pro hodnocení tahu: `eval` (číslo v pěšcích; záporné = lépe černý), nebo `centipawns`/`cp` (číslo / 100), případně `evaluation`/`score`. Volitelně `text`, `san`, `continuationArr`, `mate`, `winChance` pro text nápovědy. Odpověď může být v kořeni JSON nebo pod klíči `data` / `result` – kód to zohledňuje. Při jiném backendu stačí mapovat stejná pole (from, to, eval/centipawns).
  - Odpověď chess-api.com: `{ "from": "e2", "to": "e4", "move": "e2e4", "eval": ..., ... }`. Eval je v perspektivě bílého (negative = black winning).
- **Robustní error handling:** timeout 15 s (AbortController), kontrola `res.ok`, parsování JSON s fallback na null, kontrola `data.success` a `data.bestmove`. Při selhání nebo neplatném bestmove se zobrazí na tlačítku „Nedostupné“ (2,5 s), při chybné pozici „Chyba pozice“, při výjimce „Chyba“. Všechny chyby se logují do konzole (\[Hint\]).
- **Web:** po obdržení tahu se na šachovnici přidají třídy `.hint-from` (zelená) a `.hint-to` (žlutá). Při dalším načtení dat (`fetchData` / `updateBoard`) se hint smaže.
- **LED:** klient po zobrazení hintu pošle **POST /api/game/hint_highlight** s tělem `{ "from": "e2", "to": "e4" }`. Backend převede notaci na LED indexy a pošle příkaz `LED_CMD_HIGHLIGHT_HINT` (zelená = odkud, žlutá = kam). Při příštím refreshi LED (game state update / clear highlights) se hint na desce přepíše.
- **CORS / rate limit:** pokud by volání na Stockfish blokoval CORS, lze přidat proxy přes ESP32. Při HTTP 429 (rate limit) klient zobrazí „Nedostupné“; doporučuje se neklikat opakovaně.

## Rychlý checklist funkčnosti

- [ ] Záložka **Hra** – výchozí; šachovnice, čas, stav, Nová hra, Nápověda, Zkusit tahy, sebrání, historie
- [ ] Záložka **Nastavení** – LED, WiFi, Stav webu, Dálkové ovládání, Zvednutá figurka, Demo režim, MQTT
- [ ] Přepínání záložek bez chyby v konzoli
- [ ] Timer, Nová hra, sandbox (Zkusit tahy), historie tahů, review režim
- [ ] Na záložce Nastavení: WiFi, demo checkbox a rychlost, MQTT uložení
- [ ] Bannery (Review, Sandbox, Endgame) se zobrazují a nepřekrývají záložky
- [ ] Nápověda – během aktivní hry tlačítko „Nápověda“, načtení od Stockfish, zobrazení na webu a na LED

## Poznámky

- `embed_chess_js.py` nahrazuje blok od řádku s `// TEST PAGE - MINIMAL TIMER TEST` do řádku před `static esp_err_t http_get_chess_js_handler`.
- Původní `js_to_c.py` pouze vypisuje C pole na stdout; pro automatickou náhradu v C souboru slouží `embed_chess_js.py`.
