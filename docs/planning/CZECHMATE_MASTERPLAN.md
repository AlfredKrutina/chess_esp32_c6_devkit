# CZECHMATE — Masterplan implementace (iOS aplikace + firmware + web server)

> **Verze:** 1.1 · **Datum:** 2026-04-06  
> **Účel:** Jednotný, podrobný plán postupné implementace nativní iOS aplikace, optimalizace embedded web serveru a rozšíření ekosystému kolem ESP32-C6 šachovnice. Doplňuje dokumenty [CZECHMATE_IOS_APP_DESIGN.md](./CZECHMATE_IOS_APP_DESIGN.md) a [CZECHMATE_PROJECT_ANALYSIS.md](./CZECHMATE_PROJECT_ANALYSIS.md).

---

## 1. Cíle a rozsah

### 1.1 Produktová vize (shrnutí)

- **Fyzická šachovnice** zůstává zdrojem pravdy pro pozici na desce a pro LED feedback; šachová logika běží v `game_task` na ESP32.
- **iOS aplikace CZECHMATE** poskytuje obrazovkový zážitek (šachovnice, časovače, analýza), ovládá nápovědu a komunikuje s deskou přes WiFi (MVP) a později přes BLE.
- **Stockfish a hloubková analýza** probíhají na straně klienta přes veřejné API (**chess-api.com**), stejně jako u současného webového UI — ESP32 neřeší TLS ani velké výpočty enginu.

### 1.2 Co tento dokument řeší

| Oblast | Obsah masterplanu |
|--------|-------------------|
| iOS | Fáze od MVP (REST) přes WebSocket až po BLE; služby, modely, UX |
| Firmware | WebSocket broadcast, úspora REST zátěže, později `ble_task` |
| Stockfish | Tok dat: FEN → API → nápověda / hodnocení → deska přes `hint_highlight` |
| Rizika | RAM, MTU BLE, LED timing vs. radio — konkrétní mitigace |

### 1.3 Související dokumentace

- Návrh UI a roadmapa verzí: [CZECHMATE_IOS_APP_DESIGN.md](./CZECHMATE_IOS_APP_DESIGN.md)  
- **Dodělávky a aktuální stav (checklist):** [CZECHMATE_COMPLETION_PLAN.md](./CZECHMATE_COMPLETION_PLAN.md)  
- **BLE jako hlavní kanál (Wi‑Fi záloha):** [CZECHMATE_BLE_PRIMARY_IMPLEMENTATION_PLAN.md](./CZECHMATE_BLE_PRIMARY_IMPLEMENTATION_PLAN.md)  
- Analýza firmware, REST API a tasků: [CZECHMATE_PROJECT_ANALYSIS.md](./CZECHMATE_PROJECT_ANALYSIS.md)  
- Deploy web UI: [../reference/WEB_UI_DEPLOY.md](../reference/WEB_UI_DEPLOY.md)  
- Komunikace tasků: [../reference/KOMUNIKACE_MEZI_TASKY.md](../reference/KOMUNIKACE_MEZI_TASKY.md)

### 1.4 Multiplatform (iPhone, iPad, Mac, Apple Watch)

- **Sdílený SwiftUI kód** cílit na společné modely a služby (`BoardConnectionStore`, API klienti); UI modifikátory vždy oddělovat podle OS (`#if os(iOS)` pro `textInputAutocapitalization`, `keyboardType` atd. — na **macOS** nejsou).
- **iPhone / iPad:** primární rozhraní; plná šachovnice a nápověda.
- **Mac:** stejný target nebo „Designed for iPad“ / Catalyst — ověřovat build pro `platform=macOS`; síť (HTTP/WS) funguje stejně v LAN.
- **Apple Watch:** samostatný **watchOS** target nebo SwiftUI multiplatform s výrazně zjednodušeným UI (čas, stav „na tahu“, bez plné desky nebo s minimálním přehledem); sdílet jen malé moduly (např. jen stav přes `WatchConnectivity` z iPhonu). Watch přímo proti ESP HTTP/WS je možné, ale UX a baterie jsou horší než relay přes telefon.

---

## 2. Architektonický princip: kde co běží

### 2.1 Rozdělení odpovědností

```
┌─────────────────────────────────────────────────────────────────────────┐
│  iOS aplikace                                                            │
│  • UI, lokální herní zrcadlo (volitelně vlastní validace pro sandbox)   │
│  • URLSession → chess-api.com (Stockfish, eval, best move)               │
│  • URLSession / WebSocket / BLE → ESP32 (stav desky, příkazy, LED)       │
└─────────────────────────────────────────────────────────────────────────┘
         │                                    │
         │ HTTPS (internet)                   │ LAN / BLE
         ▼                                    ▼
┌─────────────────────┐            ┌──────────────────────────────────────┐
│  chess-api.com      │            │  ESP32-C6 (CZECHMATE firmware)        │
│  Stockfish backend  │            │  game_task, matrix, LED, web_server   │
└─────────────────────┘            │  POST /api/game/hint_highlight …      │
                                   └──────────────────────────────────────┘
```

- **Šachová pravidla a fyzická hra:** výhradně firmware (`game_task` + fronty z `matrix_task`).
- **Nápověda „kudy táhnout“:** iOS získá nejlepší tah z API, pak **pošle rozsvícení** na ESP přes `POST /api/game/hint_highlight` (stejný kontrakt jako `chess_app.js` — nevolat za bota tahy přes hint).
- **Po-pohybové hodnocení:** porovnání eval před/po tahu z API na iOS; případně zobrazení na UI bez nutnosti dalšího zatěžování ESP.

### 2.2 Proč ne Stockfish na ESP32

- RAM a CPU nestačí na běh enginu s rozumnou hloubkou; WiFi stack + HTTP + případné BLE už zatěžují heap.
- Externí API drží konzistenci s webem a zrychluje vývoj (jedna integrace).

### 2.3 Offline a „jen BLE“

- Bez internetu na telefonu **nejde** plnohodnotná nápověda přes chess-api — UI musí zobrazit stav „offline analýza nedostupná“; hra na desce dál funguje lokálně přes firmware.
- Pokud by v budoucnu vznikl **proxy** Stockfish na ESP přes STA, jde o samostatný projekt (TLS, paměť, bezpečnost) — není v tomto masterplanu předpokládán.

---

## 3. Fáze implementace (detail)

### Fáze 0 — Projektová kostra iOS a síťový MVP

**Cíl:** Vertikální řez: připojení k desce → čtení stavu → jeden zápis (např. virtuální akce / nápověda).

**Úkoly**

1. **Modely dat** odpovídající existujícímu API:
   - `GET /api/game/snapshot` (preferovaný agregovaný stav),
   - `GET /api/status`, `GET /api/timer`, případně lehké endpointy podle potřeby.
2. **Konfigurace základny URL:** AP `http://192.168.4.1`, uložená LAN IP, později Bonjour `czechmate.local` (až bude dostupní).
3. **Polling:** interval 0,5–1 s během aktivní hry; delší v nastavení; **exponenciální backoff** při chybách sítě.
4. **Stav `web_locked`:** při HTTP 403 zobrazit vysvětlení (odemykání přes UART podle firmware).
5. **Stockfish klient (iOS):**
   - stejné endpointy a formát odpovědi jako v embedded `chess_app.js` (chess-api.com v1),
   - parsování best move a eval (eval z perspektivy bílého — **nepřevádět**, jak je v komentářích u webu).
6. **Nápověda end-to-end:**
   - z aktuální pozice sestavit FEN (ze snapshotu nebo z pole figurek),
   - zavolat API → získat tah,
   - `POST /api/game/hint_highlight` s tělem kompatibilním s webem.

**Staging vs. produkce**

- Staging build: verbose logování HTTP (URL, status, latence), log FEN délky bez celého FEN při obavě o soukromí.
- Produkce: jen chyby a agregované metriky.

**Výstup fáze:** funkční „tenký“ klient, který dokáže synchronizovat šachovnici a provést nápovědu stejně jako prohlížeč.

---

### Fáze 1 — Herní UX, analýza tahů, nastavení

**Cíl:** Aplikace použitelná pro reálnou hru s fyzickou deskou; zpětná vazba na kvalitu tahů.

**Úkoly**

1. **Hlavní obrazovka hry:** šachovnice (2D MVP; 3D později), časovače, historie, indikace tahu na řadě.
2. **Po-tahová analýza:** po potvrzení tahu (detekovaného syncem se snapshotem) spustit krátkou analýzu z API, uložit eval pro další tah, vypsat klasifikaci (best / good / inaccuracy / …) podle rozdílu eval.
3. **Debouncing a cache:** stejný FEN během krátkého okna neanalyzovat opakovaně; omezit paralelní requesty.
4. **Nastavení:** limity nápověd, hloubka analýzy (1–20), jas — část přes `/api/settings/*` tam, kde firmware podporuje shodu s webem.
5. **Lokální persistence (základ):** UserDefaults pro IP a prefs; příprava na Core Data pro historii her (fáze 4).

**Závislost na síti:** mobilní data stačí pro chess-api i když je telefon na jiné WiFi než ESP — deska se ovládá přes lokální HTTP; uživatel musí rozumět tomu, že **obě cesty** (LAN + internet) musí fungovat současně.

---

### Fáze 2 — Optimalizace web serveru a real-time (WebSocket)

**Cíl:** Odstranit neustálé velké pollingy; snížit CPU na ESP a latenci UI.

**Firmware (ESP-IDF httpd)**

1. **Dokončit WebSocket** (v kódu dnes existuje placeholder `web_server_websocket_init`).
2. Po připojení klienta posílat **inkrementální aktualizace** při změně herního stavu (board, status, volitelně timer), ne celý 20KB snapshot při každém ticku.
3. **Broadcast z `game_task`** (nebo z jedné tenké vrstvy): při změně stavu serializovat minimální JSON a poslat všem WS klientům.
4. **Ochrana zdrojů:** limit počtu WS klientů; při odpojení uvolnit struktury.

**iOS**

1. `URLSessionWebSocketTask` — primární tok událostí.
2. REST ponechat jako **fallback** (reconnect, první načtení, nastavení WiFi/MQTT).
3. Adaptivní strategie: pokud WS žije, snížit polling na nulu nebo na „watchdog“ ping.

**REST optimalizace (paralelně)**

- Preferovat **`/api/board` + `/api/status`** místo permanentního snapshotu, pokud klient nepotřebuje najednou historii + captured + vše.
- Zvážit **verzi stavu** nebo ETag: klient pošle `If-None-Match` / `?since=version` → server odpoví 304 nebo prázdně — méně JSON generování na MCU.

**Staging na ESP:** logovat počet WS klientů, velikost posledního broadcastu, chyby enqueue.

---

### Fáze 3 — BLE jako primární kanál

**Cíl:** Připojení bez nutnosti přepínat WiFi na telefonu; lepší pozadí (CoreBluetooth).

**Firmware**

1. Nový **`ble_task`** (NimBLE nebo Bluedroid podle IDF), priorita níž než `led_task` / `matrix_task`.
2. **GATT služba CZECHMATE** — navržené charakteristiky v design dokumentu; pro časté notifikace použít **kompaktní binární formát** pro herní status (ne opakovaný velký JSON — šetří MTU ~512 B a CPU). JSON ponechat pro initial read / debug session.
3. **Piece encoding** 1 B na pole (již navrženo): konzistentní s `BOARD_STATE` 64 B.
4. **Koexistence BLE + WiFi:** sledovat heap; při úzkém místě testovat delší hry a reconnect.

**iOS**

1. CoreBluetooth: scan → pair → subscribe notifikace → write tahů / kontrol.
2. **Info.plist:** `NSBluetoothAlwaysUsageDescription`, background mode `bluetooth-central` podle potřeby.
3. Parita funkcí s REST: stejné příkazy jako `MOVE_CMD` / `CTRL_CMD` (NEW, RESET, HINT…).

**Bezpečnost:** minimálně ověření, že připojené zařízení je „tvoje“ šachovnice (název, UUID, případně out-of-band PIN v budoucnu).

---

### Fáze 4 — Produktové vrstvy a rozšíření

**Obsah (prioritizovat podle času)**

- Puzzle (Lichess API), denní výzva, série (streak).
- Statistiky, Elo odhad, achievements — lokálně + případně cloud později.
- Onboarding, TestFlight, App Store.
- **Online multiplayer** vyžaduje backend — samostatná architektura (mimo tento hardware masterplan).

---

## 4. Stockfish a nápověda — podrobný tok

### 4.1 Nápověda během partie

1. Aplikace načte aktuální pozici (snapshot nebo agregace z WS).
2. Ověří limit nápověd (lokální počítadlo + případně stav z API).
3. `POST https://chess-api.com/v1/...` (parametry jako na webu — hloubka, FEN).
4. Z odpovědi vezme doporučený tah (např. UCI).
5. **`POST http://<esp>/api/game/hint_highlight`** s JSON tělem odpovídajícím webovému klientovi — LED na desce zvýrazní pole.
6. UI zobrazí šipku / highlight na digitální šachovnici.

**Důležité:** Web klient záměrně **neprovádí** tah za bota přes hint endpoint — stejné pravidlo platí pro iOS.

### 4.2 Hodnocení tahu po zahrání

1. Před tahem uložit eval z API (nebo z předchozího kroku).
2. Po detekci nového tahu (sync desky) spočítat FEN nové pozice a získat nový eval.
3. Rozdíl převést na lidsky čitelnou kategorii a případně uložit do lokální historie.

### 4.3 Výkon a náklady

- Snižovat počet volání: batch eval jen při změně FEN.
- Respektovat ToS a limity chess-api; při produkci zvážit vlastní backend nebo API klíč, pokud poskytovatel vyžaduje.

---

## 5. Optimalizace web serveru (checklist)

| Oblast | Akce |
|--------|------|
| Polling | Nahradit WS push; REST jen fallback |
| Snapshot | Nepollovat neustále 20KB endpoint; použít částečné GET nebo verzi |
| WebSocket | Jedna serializace stavu, sdílený buffer kde jde (pozor na mutexy) |
| Souběh HTTP | Omezit počet souběžných klientů při slabém MCU |
| Logování | Ve stagingu měřit čas handlerů `/api/*` |

---

## 6. Rizika a mitigace (shrnutí z analýz)

| Riziko | Mitigace |
|--------|----------|
| **JSON přes BLE** | Binární nebo fixní struct pro GAME_STATUS; JSON jen zřídka |
| **RAM pod zátěží** | Postupné zapínání WS + BLE; test heap watermark; shared buffer pool (již téma v projektu) |
| **LED timing vs. radio** | `led_task` nejvyšší priorita; minimalizovat práce v ISR; BLE zpracování v tasku, ne v ISR |
| **Fragmentace heapu** | Stabilní velikosti bufferů, pooly, vyhnout se častému malloc/free v hot path |
| **iOS background** | BLE background mode má pravidla — netestovat „push“ jako na serverové notifikaci bez APNs |

---

## 7. Testovací strategie

- **Unit testy iOS:** parsování JSON z API a ze snapshotu; mapování FEN.
- **Integrace:** stejná síť jako ESP; scénáře: odpojení WiFi, reconnect, `web_locked`.
- **Firmware:** po přidání WS stress test (více klientů); po BLE dlouhá partie + MQTT současně.

---

## 8. Prioritizovaný časový sled (shrnutí)

1. iOS MVP: REST + modely + **Stockfish + hint_highlight** + základní UI.  
2. Firmware: WebSocket + úspora REST.  
3. iOS: přechod na WS, vyřadit agresivní polling.  
4. BLE na ESP + CoreBluetooth v iOS.  
5. Puzzle, statistiky, App Store příprava.

---

## 9. Změny dokumentu

| Verze | Datum | Poznámka |
|-------|--------|----------|
| 1.0 | 2026-04-06 | První verze masterplanu po konsolidaci plánovací dokumentace |
| 1.1 | 2026-04-06 | Multiplatform §1.4; WS/REST pokračování |

---

*Dokument slouží jako hlavní navigace pro implementaci; detailní UI specifikace zůstává v CZECHMATE_IOS_APP_DESIGN.md.*
