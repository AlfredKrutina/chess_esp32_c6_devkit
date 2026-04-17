# CZECHMATE — připravenost k nasazení (app + deska)

Dokument shrnuje, co z aplikace **skutečně** ovládáš přes **Bluetooth (GATT)** vs **HTTP (Wi‑Fi/LAN)**.

Poslední úprava kódu: BLE parita pro **demo** (`demo_config`, `demo_start`), **MQTT zápis do NVS** (`mqtt_config`), **auto lampa**, **LED guidance / guided hints**, **nová hra s FEN** (`new_game` + `fen`), **hint jen na cíl** (`hint_highlight` s polem `to`). Lampa RGB zůstává na `light_command` / `light_game_mode`.

---

## 1. Co aplikace umí přes BLE (bez HTTP)

| Oblast | Funkce | Poznámka |
|--------|--------|----------|
| Hra | Snímek (NOTIFY + read), tah, nová hra, **nová hra + FEN**, undo, promoce | `new_game` volitelně `"fen"` |
| Časovač | `timer_config`, pause / resume / reset | Stejná logika jako `/api/timer/*` |
| Virtuální tahy | pickup / drop / promote | `virtual_action` |
| LED nápověda | hint highlight (`from`+`to` nebo jen `to`), hint clear | Setup wizard přes BLE |
| Jas desky | `brightness` / `percent` | |
| Lampa | Zap/vyp + RGB, návrat herní režim | `light_command`, `light_game_mode` |
| Demo | `demo_config`, `demo_start` | Bez GATT read odpovědi; stav lze odvodit ze snapshotu / chování desky |
| MQTT | `mqtt_config` | Zápis do NVS + reinit při připojeném STA; **GET `/api/mqtt/status` zůstává jen HTTP** |
| Nastavení LED | `settings_led_guidance`, `settings_guided_hints`, `settings_auto_lamp_timeout` | Stejná logika jako příslušné POST na webu |

Omezení: **web lock** na desce může část příkazů blokovat (jako u webu).

**Stále jen HTTP:** `GET/POST /api/settings/ui` (velký JSON do NVS, typicky nad 768 B — nevhodné pro jeden GATT write), Wi‑Fi STA/AP API, `GET /api/mqtt/status`, `GET /api/demo/status` (app při BLE po uložení demo nemusí obnovit řádek stavu bez Wi‑Fi).

---

## 2. Co zůstává jen přes HTTP (Wi‑Fi k desce)

| Oblast | API / chování | Důsledek v app |
|--------|----------------|----------------|
| **NVS web UI** | `GET/POST /api/settings/ui` | Sync prefs z/do desky jen s HTTP |
| **Wi‑Fi STA/AP** | `/api/wifi/*` | Blok *Wi‑Fi na šachovnici* záměrně jen při HTTP |
| **HTTP GET časovače** | `GET /api/timer` | Přes BLE používej `clock` ve snapshotu |
| **MQTT / Demo stav (read)** | `GET /api/mqtt/status`, `GET /api/demo/status` | Bez Wi‑Fi nelze z desky načíst (zápis přes BLE možný) |
| **Ping RTT** přes desku | HTTP ping | U čistého BLE nemusí být dosažitelné |

---

## 3. Doporučené úkoly před „ready to deploy“

### A. Produkt / QA

- [ ] Otestovat **čisté BLE**: hra, čas, lampa, jas, nápověda, undo, **FEN nová hra**, **puzzle wizard**, demo/MQTT zápis.
- [ ] Otestovat **Wi‑Fi** po přidělení IP: NVS sync, MQTT/Demo **read** status.
- [ ] Ověřit **web lock** (403 / GATT) — srozumitelné `lastError` v UI.

### B. Firmware (volitelné)

- [ ] **Chunking nebo dílčí BLE příkazy** pro `/api/settings/ui`, pokud má být plná NVS synchronizace bez Wi‑Fi.

### C. App Store / provoz

- [ ] Texty v UI: sekce *Deska (web parity)* — NVS sync a stav MQTT z GET stále vyžadují Wi‑Fi.
- [ ] Minimální verze firmwaru (snapshot + GATT příkazy výše).

---

## 4. Shrnutí

- **Běžná hra, FEN, lampa, jas, LED nápověda, demo/MQTT zápis, LED guidance** jsou z aplikace použitelné **i přes Bluetooth** na aktuálním firmwaru s uvedenými `cmd`.
- **Plný sync NVS z web UI** a **HTTP GET stavy** zůstávají u **Wi‑Fi** nebo budoucího rozšíření BLE.

Tento soubor je podklad pro plánování; po změnách API ho aktualizuj.
