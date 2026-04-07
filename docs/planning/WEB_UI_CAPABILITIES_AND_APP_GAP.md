# Webové rozhraní šachovnice (ESP) — přehled funkcí a srovnání s aplikací CZECHMATE

Dokument vychází z kódu ve workspace: `components/web_server_task/web_server_task.c` (HTTP handlery), `components/web_server_task/chess_app.js` (UI a logika v prohlížeči) a z appky `CZECHMATE/`. Popisuje **co web umí**, ne konkrétní vzhled pixel-perfect.

---

## 1. Architektura webu

| Prvek | Popis |
|--------|--------|
| **Přístup** | HTTP server na ESP (typicky AP `192.168.4.1`, port 80), hlavní stránka `GET /`. |
| **Frontend** | Velký HTML/CSS/JS blob v `web_server_task.c` + externí `/chess_app.js`. |
| **Struktura UI** | Záložky **Hra** / **Nastavení** (`switchTab`), sekce v Nastavení (Zařízení, nápověda, časovač, Wi‑Fi, MQTT, demo, … dle HTML). |
| **Realtime** | Polling `GET /api/game/snapshot` (nebo fallback na `/api/board`, `/api/status`, `/api/history`, `/api/captured`) + volitelně **WebSocket `GET /ws`** (pokud je build s `CONFIG_HTTPD_WS_SUPPORT`). |
| **Externí síť** | Nápověda a zhodnocení tahů volají **Stockfish přes veřejné API** (`chess-api.com/v1` v `fetchStockfishBestMove`) — vyžaduje internet v prohlížeči, ne na ESP. |

---

## 2. REST API na desce (registrované handlery)

Níže přehled podle `web_server_task.c` (metody a účel; detaily těla jsou ve firmware).

### Statické soubory a úvod

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/` | GET | Hlavní HTML rozhraní |
| `/chess_app.js` | GET | Klientský skript |
| `/test` | GET | Minimální test (timer apod.) |
| `/favicon.ico` | GET | Ikona |

### Hra a stav

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/board` | GET | Samotní šachovnice (JSON) |
| `/api/status` | GET | Stav hry (+ injektovaná pole: `web_locked`, `internet_connected`, jas, matrix guard, guided hints, …) |
| `/api/game/snapshot` | GET | **Jeden** JSON: deska + `status` + `history` + `captured` (preferovaný zdroj pro polling) |
| `/api/history` | GET | Historie tahů |
| `/api/captured` | GET | Sebrané figurky |
| `/api/advantage` | GET | Data pro graf materiálu / výhody (`advantageData` v JS) |
| `/api/move` | POST | Tah `{from,to}` (případně rozšíření dle firmware) |
| `/api/game/new` | POST | Nová hra |
| `/api/game/virtual_action` | POST | Virtuální pickup/drop (stejné koncepty jako v appce) |
| `/api/game/hint_highlight` | POST | Zvýraznění nápovědy na LED (odkud–kam) |
| `/api/game/hint_clear` | POST | Zrušení nápovědy na LED |
| `/api/game/setup_tutorial` | GET/POST | Výukové postavení figurek (rozestavění) |
| `/api/game/puzzle` | GET/POST | Režim puzzle na desky (aktivace, feedback, …) |

### Časovač

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/timer` | GET | Stav časovače |
| `/api/timer/config` | POST | Konfigurace typu času, přírůstky, … |
| `/api/timer/pause` | POST | Pauza |
| `/api/timer/resume` | POST | Pokračovat |
| `/api/timer/reset` | POST | Reset |

### Nastavení desky (LED, chování)

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/settings/brightness` | POST | Jas LED 0–100 % |
| `/api/settings/auto_lamp_timeout` | POST | Auto zhasnutí lampy (sekundy) |
| `/api/settings/guided_hints` | POST | Navedení při braní (guided capture) |
| `/api/settings/led_guidance` | POST | Úroveň LED nápovědy |
| `/api/settings/ui` | GET/POST | **Preferencí webového UI uložené v NVS** (viz sekce 4) |

### Světlo / lampa (mimo čistou šachovnici)

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/light/command` | POST | RGB / zapnutí lampy (`state`, `r`, `g`, `b`) — režim „lampa“ |
| `/api/light/game_mode` | POST | Návrat do herního režimu LED |

### Wi‑Fi (STA/AP)

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/wifi/config` | POST | Uložení SSID/hesla |
| `/api/wifi/connect` | POST | Připojit STA |
| `/api/wifi/disconnect` | POST | Odpojit STA |
| `/api/wifi/clear` | POST | Smazat uložené z NVS |
| `/api/wifi/status` | GET | Stav AP/STA (SSID, IP, klienti, …) |

### Zámek webu, demo, MQTT

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/api/web/lock-status` | GET | Stav zámku webového rozhraní |
| `/api/demo/config` | POST | Konfigurace demo režimu |
| `/api/demo/start` | POST | Start demo |
| `/api/demo/status` | GET | Stav demo |
| `/api/mqtt/status` | GET | Stav MQTT |
| `/api/mqtt/config` | GET/POST | Konfigurace brokeru (host, port, credentials) |

### WebSocket

| Endpoint | Metoda | Účel |
|----------|--------|------|
| `/ws` | GET (upgrade) | Push aktualizací (snapshot / stav), pokud je zapnuto v buildu |

---

## 3. Co konkrétně umí **webové UI** (logika v `chess_app.js`)

### Šachovnice a režimy

- Zobrazení desky, výběr polí, **tahy** (včetně režimu vzdáleného ovládání — posílání příkazů na ESP místo jen lokálního náhledu).
- **Review režim** — procházení historie tahů (návrat k pozici z historie).
- **Sandbox režim** — lokální úpravy pozice v prohlížeči (undo stack), bez okamžitého zápisu do hry na MCU (vhodné pro zkoušení).
- Zobrazení **sebraných figurek**, **historie** s klikáním na tah.

### Nápověda (Stockfish)

- Volání externího API (`chess-api.com`), hloubka z `getHintDepth()` (default z `devicePrefs.chessHintDepth`).
- Zobrazení nápovědy na šachovnici (třídy `hint-from` / `hint-to`), odeslání na LED přes `/api/game/hint_highlight`.
- Textová nápověda („child-friendly“) + překlad částí do češtiny (`hintTextToCzech`).
- **Odměny za nápovědy**: limity počtu nápověd, odměny za nejlepší / dobrý / braní (`chessHintAward*` v `devicePrefs`), volitelně statistiky (`chessShowHintStats`).

### Zhodnocení tahů (eval)

- Při zapnutí **„Zhodnocení tahu“** (`chessEvaluateMove` v NVS přes `/api/settings/ui`) se po každém novém tahu (když není rozjetá rošáda / speciální stav) volá evaluace proti předchozímu FEN.
- Výsledky se ukládají do `moveEvaluations` a zobrazují u položek v **historii** jako badge (best / good / inaccuracy / …).
- Hloubka evaluace je v kódu navázaná na smysluplné minimum (komentáře v JS odkazují na hloubku ≥ 12 pro smysluplné evaly, max 18).

### Hráč vs bot (vizualizace)

- Režim **`pvp` / `bot`**: bot **nehýbe** figurkou za hráče — počítač jen navrhuje tah (včetně LED) a hráč hraje fyzicky.
- Nastavení síly (`strength` mapované na ELO-like hodnoty), strana bota, panel stavu bota.
- Volitelně LED cíl bota jen po zvednutí figurky (`chessBotLedTargetOnlyAfterLift`).

### Další herní UI

- **Konec partie** — report (`showEndgameReport`), přepínač zobrazení.
- **Chybové stavy** z MCU (neplatný tah, návrat figurky), **matrix guard** (texty v banneru), **rošáda v průběhu**, **restore snapshot** varování.
- **Puzzle** panel — stav z `status.puzzle`, offline cache `lastPuzzleSnapshotForOffline`.
- **Výukový režim postavení** (`board_setup_tutorial`, setup tutorial API).
- **Web lock** — při `web_locked` web deaktivuje např. Nová hra a Nápověda (stejný koncept jako v appce).

### Časovač a demo

- Načtení `/api/timer`, tlačítka pauza / pokračovat / reset, konfigurace přes POST `/api/timer/config`.
- **Demo režim** — konfigurace a start přes `/api/demo/*`, stav přes `/api/demo/status`.

### Nastavení zařízení na webu

- **Jas** (globální slider), **LED guidance**, **guided capture**, **auto lamp timeout** (API výše).
- **Režim Šachovnice / Lampa** — přepnutí a RGB (`/api/light/command`, `/api/light/game_mode`) v souladu s firmware.
- **Wi‑Fi** — stejné akce jako v appce (config, connect, disconnect, clear, čtení statusu).
- **MQTT** — zobrazení stavu a uložení konfigurace.

### Preferencí uložené na MCU (`/api/settings/ui`)

Do NVS se synchronizuje mimo jiné (viz `devicePrefs` / `buildUiPrefsPayload`):

- `chessHintDepth` (1–18)
- `chessEvaluateMove` (zhodnocení tahu)
- `chessHintLimit`, `chessHintAwardBest`, `chessHintAwardGood`, `chessHintAwardCapture`
- `chessShowHintStats`
- `chessBotLedTargetOnlyAfterLift`
- `chessTutorialsEnabled`
- `chess_confirm_new_game`
- `botSettings` (`mode`, `strength`, `side`)

Migrace ze starého `localStorage` do NVS při prvním načtení.

---

## 4. Zhodnocení tahů — jak to na webu funguje

1. Po každém novém tahu (detekce z historie + FEN před/po) se při zapnutém přepínači evaluace zavolá **Stockfish API v prohlížeči** (ne na ESP32).
2. Výsledek se mapuje na **grade** + textová zpráva, uloží se pod index tahu a zobrazí u řádku v historii.
3. **ESP** u toho poskytuje jen pravdivou historii a stav partie; samotný výpočet evaluace je **čistě klientský** (internet v zařízení, kde běží prohlížeč).

Aplikace CZECHMATE dělá obdobně eval přes vlastní `StockfishAPIClient` — konceptově stejné, ale **není svázaná s NVS předvolbami webu** (`/api/settings/ui`).

---

## 5. Co v aplikaci CZECHMATE **chybí nebo je jen částečně** oproti webu

### Plně nebo výrazně chybí

| Oblast | Web | Appka |
|--------|-----|--------|
| **Synchronizace preferencí s deskou** | `GET/POST /api/settings/ui` (NVS) | Ukládání většiny v **UserDefaults** na iPhonu; **neřeší stejné klíče jako web** → po přepnutí mezi webem a appkou nemusí sedět hloubka nápovědy, eval přepínače, bot, odměny za nápovědy. |
| **Režim Bot** | Plná UI + LED návrhy | **Není** (jen PvP / fyzická deska). |
| **Sandbox / review historie** | Lokální sandbox, klikací historie s náhledem pozice | Historie v appce je spíš **textový výpis** bez „přehrát pozici z tahu“ jako na webu. |
| **Demo režim** | `/api/demo/*` | **Není** v UI appky. |
| **MQTT** | Konfigurace + status | **Není** (Help říká, že plné věci zůstávají na webu — stále platí). |
| **Nápověda: limity a odměny** | Hint limit, odměny za best/good/capture, statistiky | Appka má **hloubku** a přepínač evaluace; **ne** stejné „herní ekonomické“ pravidla nápovědy jako web. |
| **Potvrzení nové hry** | `chess_confirm_new_game` v NVS | **Není** propojeno s MCU preferencí. |
| **Tutoriály / výuka rozestavení** | Přepínač + `setup_tutorial` API | Appka zobrazí stav ze snapshotu; **nemá** stejný průvodce jako web. |
| **Kompletní lampa RGB** | Plné `/api/light/command` | Appka: jas + metadata ze snapshotu; **plné RGB ovládání jako na webu** může chybět nebo být omezené podle aktuálního `BoardWiFiLampControls`. |
| **POST vedlejších nastavení** | `guided_hints`, `led_guidance`, `auto_lamp_timeout` z webu | Appka často jen **čte** z `GameStatus`; **nemusí** posílat všechny stejné POSTy jako web. |

### Částečná shoda (dobré pokrytí, ale ne 1:1)

| Oblast | Poznámka |
|--------|----------|
| **Časovač** | Appka: pauza / resume / reset / načtení stavu — **shoda s webem** na úrovni API. |
| **Wi‑Fi** | Appka má status, konfiguraci, lampu v pokročilých ovládáních — **blízko webu**. |
| **Nápověda Stockfish + LED** | Appka: hint + clear — **shoda**; texty nápovědy web má bohatší (CZ fráze, child-friendly). |
| **Zhodnocení tahu** | Appka má eval po tahu a zobrazení — **koncept shodný**, parametry hloubky nejsou synchronizované s NVS webu. |
| **WebSocket** | Appka používá WS + REST — **parita** s možností webu. |
| **Graf materiálu / výhody** | Web: `/api/advantage`; Appka: **eval historie** v Analýze — podobný záměr, jiný zdroj dat. |

---

## 6. Doporučení (priorita)

1. **Jednotný zdroj pravdy pro „chess UI prefs“** — buď číst/zapisovat `/api/settings/ui` z iOS, nebo dokumentovat, že iPhone je jen lokální profil.
2. **Rozhodnutí o Bot / Demo / MQTT** — buď záměrně „jen web“, nebo postupně přidat do appky s minimálním UI.
3. **Historie „review“** — pokud má být parita s webem, přidat náhled pozice z historie (minimálně přes FEN z historie MCU).
4. **RGB lampa** — srovnat s webovou sekcí Zařízení; doplnit POSTy tam, kde appka jen čte snapshot.

---

*Vygenerováno z analýzy repozitáře (duben 2026). Po velké změně firmware aktualizovat zejména sekci 2 (nové URI).*
