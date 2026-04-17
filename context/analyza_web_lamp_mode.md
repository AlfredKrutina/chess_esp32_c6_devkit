# Analýza: Web UI vs. HA režim a ovládání desky jako lampy bez WiFi

Dokument shrnuje, jak funguje web, jak vypadá UI, jak funguje HA režim pro ovládání desky jako lampy, a **ovládání LED jako lampy z webu bez připojení na domácí WiFi** – toto je **implementováno** (sekce Zařízení v Nastavení, API níže).

---

## Implementováno

- **Backend:** `ha_light_task` – příkaz `HA_CMD_WEB_LAMP` do fronty, NVS persistence (`lamp_cfg`), getter `ha_light_get_state()`, `ha_light_request_web_lamp()` vrací `bool` (false = fronta plná/neinicializovaná → HTTP 503).
- **API:** GET `/api/status` rozšířen o `light_mode`, `light_state`, `light_r`, `light_g`, `light_b`. POST `/api/light/command` (body: `state`, `r`, `g`, `b`). POST `/api/light/game_mode` (přepnutí zpět na šachovnici).
- **UI:** V sekci Zařízení přepínač Šachovnice/Lampa, v režimu Lampa zapnutí a R/G/B slidery; chyby (síť, 503) se zobrazí uživateli na 2,5 s.
- **Robustnost:** Kontrola dostatku místa v JSON bufferu před injectem light_*; zápis MQTT effect pod mutexem; kontrola `nvs_commit` při ukládání lampy; 503 pokud task není připraven.

---

## 1. Jak funguje web

### Architektura
- **WiFi režim:** ESP32 běží jako **Access Point** (hotspot). SSID typicky „ESP32-Chess“ (nebo dle konfigurace), IP serveru **192.168.4.1**.
- **HTTP server:** Port 80, embedded v `web_server_task` (FreeRTOS task, priorita 2).
- **Připojení:** Uživatel se připojí na AP šachovnice → otevře `http://192.168.4.1/` → **není potřeba internet ani domácí WiFi**. Veškerá komunikace je lokální (klient ↔ ESP32).

### REST API (výběr důležitých)
| Endpoint | Metoda | Účel |
|---------|--------|------|
| `/api/status` | GET | Stav hry, `brightness`, `web_locked`, `light_mode`, `light_state`, `light_r`/`g`/`b` |
| `/api/board` | GET | Pozice figurek (JSON) |
| `/api/move` | POST | Tah `{from, to}` |
| `/api/settings/brightness` | POST | Globální jas LED (0–100 %) |
| `/api/light/command` | POST | Režim Lampa: body `{ state, r, g, b }`. Vrací 503 pokud task není připraven. |
| `/api/light/game_mode` | POST | Přepnutí zpět na režim Šachovnice (bez body) |
| `/api/game/new` | POST | Nová hra |
| `/api/game/hint_highlight` | POST | LED zvýraznění nápovědy (odkud–kam) |
| `/api/mqtt/status`, `/api/mqtt/config` | GET/POST | Stav/konfigurace MQTT |

### UI struktura
- **Dvě záložky:** „Hra“ a „Nastavení“ (`tab-hra`, `tab-nastaveni`).
- **Nastavení:** Sekce „Zařízení“ (režim Šachovnice/Lampa, zapnutí a R/G/B v režimu Lampa, jas, zámek, online), „Nápověda“, „Ovládání“, WiFi, MQTT, Demo mode atd.
- **Jas:** Jeden globální slider (0–100 %) platí pro šachovnici i pro režim Lampa.

HTML/CSS/JS jsou generované v `web_server_task.c` (chunky HTML + inline JS) a doplněné z `chess_app.js`. Styl: `.tab-nav`, `.tab-btn`, `.tab-content`, `.settings-section`, `.set-row`, `.set-input` atd.

---

## 2. Jak funguje HA režim (deska jako lampa)

### Režimy v `ha_light_task`
- **GAME MODE** (`HA_MODE_GAME`): LED zobrazují šachovnici a herní stav (výchozí).
- **HA MODE** (`HA_MODE_HA`): Všech 64 LED desky + 9 tlačítkových LED se chová jako **jedno RGB světlo** – stejná barva a jas na všech.

### Přepínání režimů
- **GAME → HA:**
  - Automaticky po **10 minutách** neaktivity (**a** současně musí být připojeno **WiFi STA** k domácí síti).
  - Nebo okamžitě při **MQTT příkazu** (z Home Assistant), pokud uplynulo aspoň **2 minuty** od poslední aktivity.
- **HA → GAME:**
  - Při jakékoli **aktivitě**: pohyb figurky (pickup/drop), herní příkaz z webu/UART → volá se `ha_light_report_activity()` → do fronty `ha_light_cmd_queue` přijde příkaz typu „activity“ → v `ha_light_task` se zavolá `ha_light_switch_to_game_mode()`.
  - Při **odpojení WiFi STA** (pokud byl v HA módu) se také přepne zpět do GAME.

### Kde se bere barva/jas v HA módu
- Stav drží **`ha_light_state`** (v `ha_light_task.c`): `state` (on/off), `brightness` (0–255), `r`, `g`, `b`, `effect`.
- **MQTT:** Příkazy přicházejí na topic `HA_TOPIC_LIGHT_COMMAND` (`esp32-chess/light/command`), payload JSON: `state`, `brightness`, `color` / `rgb_color`, `effect`. Handler parsuje JSON, nastaví `ha_light_state` a zavolá **`ha_light_switch_to_ha_mode()`** (lokálně, static).
- **Aplikace na LED:** `ha_light_switch_to_ha_mode()` volá `led_set_ha_color(ha_light_state.r, ha_light_state.g, ha_light_state.b, ha_light_state.brightness)`. Funkce `led_set_ha_color()` je v `led_task.c`: nastaví všech 64 polí + 9 tlačítek na stejnou (škálovanou) barvu a zavolá `led_force_immediate_update()`.

### Důležité body
- Do HA módu se dostaneš **jen** přes: (1) 10min timeout + WiFi STA, nebo (2) MQTT příkaz (tedy Home Assistant / broker na domácí WiFi).
- **Bez WiFi STA** (pouze AP pro web) se automatický přechod do HA módu **nikdy neprovede** (v kódu je explicitní kontrola `ha_light_check_wifi_sta_connected()`).
- **Žádné REST API** dnes nevolá přepnutí do HA módu ani nastavení RGB z webu.

---

## 3. Co znamená „ovládat LED jako Home Assistant, ale bez WiFi“

- **Cíl:** Stejné chování jako v HA módu (celá deska = jedno RGB světlo, zapnout/vypnout, barva, jas), ale **řízení z webu**, když je uživatel připojen **pouze na AP šachovnice** (žádné připojení na domácí WiFi, žádný MQTT).
- **Scénář:** Uživatel připojí telefon/notebook k „ESP32-Chess“, otevře web → v Nastavení (nebo nová záložka/sekce) má ovládání „Lampa“: zapnout/vypnout, výběr barvy (RGB), jas → deska se chová jako lampa bez potřeby HA/MQTT.

---

## 4. Co je potřeba (koncepčně)

### Backend (ESP32)
1. **Přepnutí do „lamp“ módu z webu**
   - `ha_light_switch_to_ha_mode()` a `ha_light_switch_to_game_mode()` jsou dnes **static** v `ha_light_task.c`. Buď:
     - Přidat **veřejné API** (např. `ha_light_set_local_lamp(r, g, b, brightness, on)` a `ha_light_switch_to_ha_mode_from_web()` / „switch to game from web“), které z web serveru (nebo z nového handleru) zavoláš, **nebo**
     - Zavádět „lokální lamp“ příkazy do **`ha_light_cmd_queue`** (nový typ zprávy, např. „web lamp command“) a v `ha_light_task` v hlavní smyčce tento typ zpracovat: nastavit `ha_light_state` a zavolat `ha_light_switch_to_ha_mode()`. Druhá varianta drží veškerou logiku režimu a stavu v jednom tasku a nedráždí mutexy z HTTP handleru.
2. **Přepnutí zpět do hry z webu**
   - Stačí vyvolat chování jako při „aktivitě“: buď existující `ha_light_report_activity("command")`, nebo nový explicitní endpoint „vrátit na šachovnici“, který pošle do fronty aktivitu a v HA tasku proběhne `ha_light_switch_to_game_mode()`.
3. **REST endpointy (příklad)**
   - `GET /api/light/status` – stav režimu (game / lamp), pokud lamp: on/off, r, g, b, brightness (konzistentní s `ha_light_state`).
   - `POST /api/light/command` – body např. `{ "state": "on"|"off", "brightness": 0–255, "r", "g", "b" }` → v handleru neblokovat, poslat do `ha_light_cmd_queue` (nebo zavolat nové veřejné funkce ha_light), aby přepnul do HA módu a nastavil barvu/jas.
   - Volitelně `POST /api/light/game_mode` – explicitně „zpět na šachovnici“ (volat `ha_light_report_activity` nebo ekvivalent).

4. **Synchronizace s MQTT**
   - Když později přijde MQTT příkaz z HA, měl by přepsat stav; když uživatel změní „lampu“ z webu, ideálně by se stav promítl do `ha_light_state`, aby při dalším MQTT publish byl konzistentní. To už dnes děláte v `ha_light_switch_to_ha_mode()` (čte `ha_light_state`), takže stačí při web příkazu tento stav nastavit (v HA tasku) a pak volat stejné přepnutí.

### Frontend (web UI)
- **Kde:** Logicky nová sekce v záložce **Nastavení** (např. „Lampa“ / „Světlo desky“), vedle „Zařízení“ (jas) a „Ovládání“. Případně vlastní pod-záložka, pokud nechceš míchat s ostatními nastaveními.
- **Prvky:**
  - Přepínač režimu: „Šachovnice“ vs „Lampa“ (odpovídá GAME / HA mode).
  - V režimu Lampa: zapnout/vypnout, color picker nebo 3 slidery R/G/B, slider jasu (0–100 % nebo 0–255 dle API). Jedna „barva“ pro celou desku, stejně jako v HA.
- **Komunikace:** Volání `GET /api/light/status` při načtení a po změně; při změně barvy/jasu/zapnuto volání `POST /api/light/command`. Přepnutí zpět na šachovnici → `POST /api/light/game_mode` nebo ekvivalent (aktivita).
- **Bez WiFi:** Vše běží přes stejný HTTP server na AP, takže **žádné WiFi STA ani MQTT není potřeba**. Režim „lampa“ z webu je čistě lokální.

### Bezpečnost a režim
- **Timeout 10 minut:** Dnes při běhu bez WiFi STA se do HA módu automaticky nepřepne. S novým „web lamp“ ovládáním může uživatel kdykoli přepnout do lampy ručně; 10min timeout může zůstat jen pro případ „HA + MQTT“.
- **Konflikt:** Pokud je zařízení v HA módu z webu a pak se připojí WiFi STA a přijde MQTT příkaz, je rozumné MQTT příkaz brát jako nadřazený (přepsat stav). Naopak při přepnutí z webu do GAME módu by se měl zrušit „lamp“ stav až do dalšího ručního nebo MQTT přepnutí.

---

## 5. Shrnutí

| Oblast | Stav | Poznámka |
|-------|------|----------|
| Web | AP na ESP32, HTTP na 192.168.4.1, REST API | Funguje bez domácí WiFi |
| UI | Záložky Hra / Nastavení, sekce Zařízení (jas), Nápověda, Ovládání, WiFi, MQTT… | Jas = globální brightness, ne lamp mode |
| HA režim | GAME (šachovnice) ↔ HA (jedna RGB lampa), přepínání přes timeout + WiFi STA nebo MQTT | Bez WiFi STA se do HA módu automaticky neprepne |
| Ovládání lampy bez WiFi | Dnes neexistuje | Potřeba: API pro nastavení RGB/on/off a přepnutí režimu + sekce v Nastavení |

**Závěr:** Web i hardware (LED, `led_set_ha_color`) jsou připravené. Chybí pouze **REST vrstva a zpracování „lokálního lamp“ příkazu** v `ha_light_task` (fronta nebo veřejné funkce) a **nová sekce v Nastavení** pro ovládání lampy (zapnout/vypnout, barva, jas, přepnout zpět na šachovnici). Tím získáte ovládání desky jako lampy stejné jako v HA, ale bez nutnosti být připojen na WiFi a MQTT.
