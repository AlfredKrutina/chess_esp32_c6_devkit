# CZECHMATE — Komplexní analýza projektu & strategie iOS integrace

> **Verze analýzy:** 1.0 | **Datum:** 2026-04-06  
> **Autor:** AI analýza na základě zdrojového kódu projektu  
> **Související:** [CZECHMATE_MASTERPLAN.md](./CZECHMATE_MASTERPLAN.md) — sjednocený plán vývoje iOS a firmware

---

## 1. Přehled projektu

### 1.1 Co je CZECHMATE

CZECHMATE je fyzická šachovnice řízená mikrokontrolérem **ESP32-C6**, která kombinuje:
- 8×8 matici **Reed Switch** senzorů pro detekci figurek
- 73× **WS2812B** adresovatelné LED (64 na šachovnici + 9 na tlačítkách)
- Kompletní **šachovou logiku** (všechna pravidla: en passant, rošáda, promoce, šach/mat)
- **Webové rozhraní** dostupné přes WiFi (HTTP server embedded v ESP32)
- **MQTT/Home Assistant** integraci (šachovnice jako RGB světlo)
- **Stockfish AI** nápovědu a analýzu tahů (přes chess-api.com, vyžaduje internet)
- **FreeRTOS** multitasking s 8–10 paralelními tasky

### 1.2 Hardware shrnutí

| Komponenta | Detail |
|-----------|--------|
| **MCU** | ESP32-C6 (RISC-V, WiFi 6, BLE 5.0, Thread/Zigbee 802.15.4) |
| **LED** | 73× WS2812B (GPIO7, RMT) |
| **Matice** | 8×8 Reed Switch (Row: GPIO10-23, Col: GPIO0-4,6,16,17) |
| **Napájení LED** | Ext. 5V/5A |
| **Konzole** | USB Serial JTAG |
| **Uložiště** | NVS (WiFi, MQTT, herní stav, nastavení) |

### 1.3 Klíčová zjištění o ESP32-C6

ESP32-C6 je výjimečná platforma, protože jako jediný z ESP32 rodiny nativně integruje:
- **Wi-Fi 6** (IEEE 802.11ax)
- **Bluetooth 5.0 LE** (BLE) s Bluetooth Mesh
- **IEEE 802.15.4** (Thread + Zigbee) radio
- **Matter** protokol podporu

Toto otevírá unikátní komunikační možnosti pro iOS aplikaci.

---

## 2. Architektura FreeRTOS tasků

### 2.1 Task prioritní systém

```
Priorita 7 | led_task          | WS2812B timing (16KB stack)
Priorita 6 | matrix_task       | Realtime Reed scan (8KB)
Priorita 5 | button_task       | Tlačítka - promo, reset (3KB)
Priorita 4 | game_task         | Šachová logika 514KB! (10KB stack)
Priorita 3 | uart_task         | UART/USB konzole (10KB)
            | animation_task    | LED animace (2KB)
            | web_server_task   | HTTP + WiFi (20KB!)
            | ha_light_task     | MQTT/HA integrace
Priorita 1 | test_task         | Testovací funkce (4KB)
```

### 2.2 Inter-task komunikace

```
Matrix task  --PICKUP/DROP--> game_command_queue (20 zpráv) --> game_task
Button task  --EVENTS-------> button_event_queue (5 zpráv) --> button_task
Web server   --MOVE/RESET---> game_command_queue -----------> game_task
UART task    --COMMANDS-----> uart_command_queue -----------> uart_task
Main task    --DEMO MOVES---> game_command_queue -----------> game_task
HA task      ---------------- ha_light_cmd_queue (interní)
```

**Důležité:** LED se ovládají přímými voláními (bez fronty), mutex `led_state_mutex` chrání sdílený LED buffer.

### 2.3 game_task — jádro systému (514 KB zdrojového kódu!)

game_task je mozkem systému — zpracovává typy příkazů:
- `GAME_CMD_PICKUP` — zvednutí figurky (svítí validní cíle)
- `GAME_CMD_DROP` — položení figurky (validace, provedení tahu)
- `GAME_CMD_NEW_GAME` — nová hra
- `GAME_CMD_MATRIX_GUARD` — detekce ambiguity na desce
- `GAME_CMD_VIRTUAL_ACTION` — webový/UART příkaz
- `GAME_CMD_HINT_HIGHLIGHT` — LED nápověda
- `GAME_CMD_PUZZLE` — puzzle mód

---

## 3. Web Server — jak funguje

### 3.1 WiFi mód: APSTA (dual-mode)

ESP32-C6 běží simultánně jako:
1. **Access Point (AP):** SSID `ESP32-CzechMate`, heslo `12345678`, IP `192.168.4.1`
2. **Station (STA):** Připojuje se k domácí WiFi (SSID/heslo uloženo v NVS)

**Dva způsoby přístupu:**
- **AP mód:** Mobilní/PC se připojí k hotspotu `ESP32-CzechMate` → `http://192.168.4.1/`
- **APSTA mód:** ESP32 připojeno k domácí WiFi → přístup z celé LAN přes DHCP IP

### 3.2 REST API endpointy (kompletní seznam)

| Metoda | Endpoint | Popis |
|--------|----------|-------|
| GET | `/` | Hlavní HTML stránka (embedded, ~430KB C zdrojáku) |
| GET | `/chess_app.js` | Embedded JavaScript (~168KB) |
| GET | `/api/game/snapshot` | **Hlavní endpoint** — board + status + history + captured (20KB buffer) |
| GET | `/api/board` | 8×8 pole figurek (JSON) |
| GET | `/api/status` | Stav hry, hráč, šach, konec, web_locked, internet_connected |
| GET | `/api/history` | Historie tahů |
| GET | `/api/captured` | Sebrané figurky |
| GET | `/api/advantage` | Materiálová výhoda (graf) |
| GET | `/api/timer` | Chess hodiny — časy, running, paused |
| POST | `/api/game/move` | Provést tah `{"from":"e2","to":"e4"}` |
| POST | `/api/game/new` | Nová hra |
| POST | `/api/game/virtual_action` | Obecná herní akce |
| POST | `/api/game/hint_highlight` | LED nápověda na šachovnici |
| POST | `/api/game/hint_clear` | Vymazat LED nápovědu |
| POST | `/api/game/setup_tutorial` | Tutorial nastavení figurek |
| POST | `/api/game/puzzle` | Puzzle mód |
| POST | `/api/timer/config` | Nastavit chess hodiny |
| POST | `/api/timer/pause` | Pausovat hodiny |
| POST | `/api/timer/resume` | Obnovit hodiny |
| POST | `/api/timer/reset` | Reset hodiny |
| GET | `/api/wifi/status` | Stav WiFi (AP + STA) |
| POST | `/api/wifi/config` | Uložit WiFi přihlašovací údaje |
| POST | `/api/wifi/connect` | Připojit k WiFi STA |
| POST | `/api/wifi/disconnect` | Odpojit od WiFi STA |
| POST | `/api/wifi/clear` | Smazat WiFi config z NVS |
| GET | `/api/web/lock-status` | Stav web locku |
| GET | `/api/mqtt/status` | Stav MQTT připojení a konfigurace |
| POST | `/api/mqtt/config` | Nastavit MQTT broker |
| POST | `/api/demo/config` | Demo mód `{"enabled":true,"speed_ms":2000}` |
| GET | `/api/demo/status` | Stav demo módu |
| GET | `/api/settings/ui` | UI nastavení |
| POST | `/api/settings/ui` | Uložit UI nastavení |
| POST | `/api/settings/guided-hints` | Nastavení nápověd |
| POST | `/api/settings/led-guidance` | LED navedení |

### 3.3 Jak funguje aktualizace stavu — POLLING (ne WebSocket!)

> **KRITICKÉ ZJIŠTĚNÍ:** Aktuální systém **NEPOUŽÍVÁ WebSocket**.

JavaScript (`chess_app.js`, 168KB) polluje REST API:
- Primárně `GET /api/game/snapshot` — jeden velký dotaz (20KB buffer)
- Interval: pravděpodobně ~1-2 sekundy

WebSocket existuje jen jako prázdný placeholder:
```c
void web_server_websocket_init(void) {
  ESP_LOGI(TAG, "WebSocket support not yet implemented");  // řádek ~10323
}
```

Pro iOS aplikaci je polling suboptimální — zvyšuje latenci a zatěžuje ESP32.

### 3.4 Příklad `/api/status` JSON

```json
{
  "game_state": "active",
  "current_player": "White",
  "move_count": 12,
  "in_check": false,
  "game_end": {"ended": false, "winner": null},
  "internet_connected": true,
  "web_locked": false,
  "castling_in_progress": false,
  "is_demo_mode": false,
  "brightness": 75,
  "ha_mode": "game"
}
```

### 3.5 Web Lock systém

Pokud je `web_locked: true`, POST requesty (move, hint, atd.) jsou odmítnuty s HTTP 403. Odemknout lze pouze přes UART. **iOS app musí respektovat lock stav** a zobrazit uživateli instrukci pro odemčení.

---

## 4. MQTT / Home Assistant integrace

### 4.1 Automatické přepínání módů

```
GAME MODE (výchozí)
    |
    | 5 minut bez aktivity + WiFi STA connected
    v
HA MODE (lampa) -- všech 64 LED jako 1 RGB světlo
    |
    | Pohyb figurky / web příkaz / PICKUP/DROP event
    v
GAME MODE
```

Timeout konfigurovatelný: 5 s až 120 min (default: 300 s).

### 4.2 MQTT GATT profil pro HA

```
Topic: homeassistant/light/esp32_chess_light_<MAC>/set     <- příkazy z HA
Topic: homeassistant/light/esp32_chess_light_<MAC>/state  -> stav do HA
Topic: homeassistant/light/esp32_chess_light_<MAC>/availability -> online/offline
```

Podporuje: on/off, brightness (0-255), RGB color, effects (rainbow, pulse, static).

---

## 5. Analýza komunikačních možností pro iOS aplikaci

### 5.1 Přehled dostupných technologií na ESP32-C6

| Technologie | ESP32-C6 | iOS podpora | Poznámka |
|------------|----------|-------------|---------|
| WiFi HTTP REST | Implementováno | URLSession | Polling, existuje |
| WiFi WebSocket | Placeholder | URLSession iOS 13+ | Rychlé přidat na ESP32 |
| BLE 5.0 | Hardware ready | CoreBluetooth | Nová implementace |
| Thread 802.15.4 | Hardware ready | iOS 16+ omezeno | Jen přes border router |
| Matter | SDK dostupné | iOS 16+ | Jen pro HA Light část |
| Classic BT | NENÍ | - | ESP32-C6 nemá |
| USB | Jen JTAG | - | Nepraktické |

### 5.2 WiFi HTTP REST — hodnocení

**Pro:**
- Žádné změny firmware
- Kompletní API dokumentace
- Funguje přes AP i APSTA

**Proti:**
- Polling latence 1-3 s (pomalé UI)
- Uživatel musí ručně přepnout WiFi v nastavení
- iOS odpojí WiFi při přechodu do pozadí
- Žádné push notifikace

**Hodnocení: 3/5**

### 5.3 WiFi WebSocket — hodnocení

**Pro:**
- Realtime push (<100 ms latence)
- Placeholder na ESP32 ready — rychlá implementace
- URLSession nativně (iOS 13+)
- Stále WiFi — stejné připojení jako REST

**Proti:**
- Stále vyžaduje WiFi (hotspot nebo APSTA)
- iOS background mode komplikace
- Nutno doimplementovat na ESP32

**Hodnocení: 4/5** — nejlepší WiFi varianta

### 5.4 Bluetooth LE (BLE) — hodnocení

**Pro:**
- **Žádný WiFi requirement** — páruje se jednou jako sluchátka
- **Automatický reconnect** — iOS si pamatuje zařízení
- **Background mode** — CoreBluetooth běží i na pozadí
- **Push notifikace** — šach, mat, konec hry, tah bota
- ESP32-C6 má BLE **5.0** (rychlejší, delší dosah než 4.2)
- Funguje bez routeru, bez internetu
- Stejné API pro iOS i **budoucí Android** (přepsání)
- Nízká spotřeba energie

**Proti:**
- Nová implementace BLE GATT serveru na ESP32 (nový task)
- MTU limitace ~512 bytes (potřeba fragmentace pro větší data)
- `NSBluetoothAlwaysUsageDescription` povinně v iOS plistu
- BLE+WiFi coexistence (ale ESP-IDF to řeší automaticky)

**Navrhovaný GATT profil:**
```
Service: CZECHMATE_CHESS
  BOARD_STATE   (notify + read) - 64 bytes (1 byte/pole)
  GAME_STATUS   (notify + read) - zkomprimovaný JSON stav
  MOVE_CMD      (write)         - "e2e4" (4 bytes)
  CTRL_CMD      (write)         - "NEW|RESET|HINT|PAUSE"
  TIMER_STATE   (notify)        - časy bílý/černý
  SETTINGS      (read+write)    - jas, obtížnost, časovač
```

**Hodnocení: 5/5** — nejlepší UX, DOPORUČENO jako primární kanál

### 5.5 Thread — hodnocení

**Hodnocení: 1/5** — nevhodné, vyžaduje HomePod/Apple TV jako border router.

### 5.6 Matter — hodnocení

**Hodnocení: 2/5** — vhodné POUZE pro HA Light část (jako upgrade MQTT). Nelze vyjádřit šachovou logiku v Matter clusterech.

---

## 6. Doporučená strategie

### 6.1 Hybridní architektura

```
iOS CZECHMATE App
    |
    +-- [PRIMÁRNÍ]  BLE GATT --> ESP32-C6 BLE GATT Server (nový ble_task)
    |                             Herní stav, tahy, notifikace, bez WiFi
    |
    +-- [SEKUNDÁRNÍ] WiFi REST --> ESP32-C6 HTTP Server (existující)
                                   Nastavení WiFi/MQTT, webový přístup
```

### 6.2 Fáze implementace

| Fáze | Co | Kdy |
|------|----|----|
| **1 — MVP** | iOS + WiFi REST polling | Okamžitě, bez změn firmware |
| **2 — Realtime** | WebSocket na ESP32 + iOS | 1-2 dny práce na ESP32 |
| **3 — Best UX** | BLE GATT na ESP32 + iOS CoreBluetooth | Týden práce |

### 6.3 Pro budoucí Android přepis

> BLE je nejlepší volba pro multiplatformu.
> CoreBluetooth (iOS) a BluetoothGatt (Android) mají identický GATT model → stejné ESP32 API, jen jiný klient.

---

## 7. Technické limity ESP32-C6

| Resource | Hodnota |
|---------|---------|
| Flash | 2MB (konfig), pravděpodobně 4MB fyzicky |
| SRAM pro aplikaci | ~300KB |
| Snapshot JSON buffer | 20KB |
| Max AP klientů | 10 |
| BLE MTU | ~512 bytes (fragmentace potřeba) |
| WiFi kanál (APSTA) | STA a AP musí sdílet kanál |
| Spotřeba WiFi active | ~200 mA |
| BLE+WiFi coexistence | Automaticky řešeno ESP-IDF |

---

## 8. Stav iOS projektu CZECHMATE

### 8.1 Aktuální stav (prázdný Xcode template)

```
CZECHMATE/CZECHMATE/
├── CZECHMATEApp.swift   -- @main + WindowGroup (18 řádků)
├── ContentView.swift    -- prázdný placeholder
└── Assets.xcassets/
```

### 8.2 Co je potřeba přidat

- **Networking:** URLSession (REST) + URLSessionWebSocketTask + CoreBluetooth
- **Chess UI:** Interaktivní šachovnice, drag & drop figurek
- **Game State Model:** Swift structs zrcadlící ESP32 JSON (Board, Status, History, Timer)
- **BLE GATT Client:** Scan, pair, connect, subscribe to notifications
- **Push Notifications:** Konec hry, tah bota, šachmat
- **Settings:** WiFi manager, MQTT config, brightness slider
- **Stockfish:** Integrace přes chess-api.com (stejná jako web)
- **Offline mode:** Zobrazení šachovnice bez spojení s ESP32

---

## 9. Závěry

### 9.1 Klíčová zjištění

1. **REST API je kompletní** — ideální základ pro fázi 1 (iOS polling)
2. **WebSocket placeholder existuje** — rychlá cesta k realtimové komunikaci
3. **BLE hardware je ready** — nejlepší dlouhodobá volba pro iOS UX
4. **Thread/Matter nevhodné** pro herní komunikaci
5. **Snapshot endpoint** — výborný design, iOS ho využije jako primární
6. **game_task** je kompletní šachový engine — iOS ho jen konzumuje přes API
7. **Web lock** — iOS musí respektovat a instruovat uživatele

### 9.2 Nic v existujícím kódu se nemusí rozbít

Nové části přidáme jako:
- **`ble_task`** — nový FreeRTOS task s nižší prioritou (3)
- **WebSocket handler** — rozšíření existujícího HTTP serveru

Existující tasky (game, matrix, led, web, ha_light) zůstanou beze změn.

---

*Analýza vytvořena přímým studiem zdrojového kódu projektu CZECHMATE v2.5, firmware pro ESP32-C6.*
