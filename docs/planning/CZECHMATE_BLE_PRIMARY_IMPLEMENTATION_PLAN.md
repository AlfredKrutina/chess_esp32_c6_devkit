# CZECHMATE — Implementační plán: BLE jako hlavní kanál, Wi‑Fi jako záloha

> **Verze:** 1.0 · **Datum:** 2026-04-06  
> **Účel:** Navrhnout komplexní, proveditelný postup pro přepnutí aplikace na **primární Bluetooth LE** s **Wi‑Fi (HTTP/WS) jen jako fallback** nebo když BLE není k dispozici — a pro **jednoduché UI** pro neodborníky.

---

## 1. Cíle produktu

| Cíl | Popis |
|-----|--------|
| **Primární spojení** | Uživatel připojí telefon k šachovnici **přes Bluetooth** (bez zadávání IP, ideálně bez routeru). |
| **Záloha** | Pokud BLE selže, uživatel je vede **jednoduchý tok** na připojení přes **domácí Wi‑Fi** (URL / poslední uložená IP). |
| **Srozumitelnost** | Žádné technické termíny v hlavním toku; stav jako „Připojeno k šachovnici“ / „Hledám šachovnici“ / „Používáme Wi‑Fi“. |
| **Parita funkcí** | Stejná herní data (pozice, časy, historie) a stejné akce (nápověda na desku přes firmware), bez ohledu na transport — liší se jen způsob doručení. |

---

## 2. Princip architektury (jedna logika, dva transporty)

```
┌─────────────────────────────────────────────────────────────┐
│  SwiftUI (jednoduché obrazovky)                              │
└───────────────────────────┬─────────────────────────────────┘
                            │
┌───────────────────────────▼─────────────────────────────────┐
│  BoardConnectionController (nebo rozšířený Store)            │
│  • jednotný model: GameSnapshot + stav připojení             │
│  • výběr transportu: .bluetoothPreferred | .wifiOnly | .auto │
└───────┬─────────────────────────────────────┬───────────────┘
        │                                     │
        ▼                                     ▼
┌───────────────────┐               ┌───────────────────────┐
│ BoardTransport    │               │ BoardTransport       │
│ .ble (CoreBluetooth)               │ .wifi (REST + WS)    │
└─────────┬─────────┘               └──────────┬───────────┘
          │                                    │
          ▼                                    ▼
   ESP32 NimBLE GATT                    ESP32 HTTP server
   (notifikace + write)                 (snapshot + hint_*)
```

- **Stockfish / chess-api** zůstává na iOS přes internet — **nezávislé** na tom, zda jede BLE nebo Wi‑Fi k ESP.
- **Firmware** musí z obou cest dostat příkazy do **stejné fronty** (`game_task` / stejné handlery jako web), aby nebyla duplicitní šachová logika.

---

## 3. Firmware (ESP32) — co je potřeba

### 3.1 Současný stav

- `ble_task.c` je **stub**; plná implementace vyžaduje `CONFIG_BT_ENABLED` + NimBLE v `sdkconfig`.
- Web server už poskytuje **REST + WebSocket** — to zůstane jako **fallback kanál**.

### 3.2 Cílový stav BLE

1. **NimBLE GATT server** s jednou **vendor službou** (UUID projektu CZECHMATE).
2. **Charakteristiky (návrh k dohodě):**

   | Účel | Typ | Poznámka |
   |------|-----|----------|
   | Stav hry / snapshot | **Notify** (případně Indicate) | Kompaktní payload: buď **zkrácený binární** (64 B deska + hlavička) nebo **malý JSON** (MTU ~512 B; pozor na heap). |
   | Příkazy z telefonu | **Write** | Např. stejný význam jako web: `HINT`, `RESET`, volitelně surový tah — **mapovat na existující API** interně. |
   | Verze / jméno zařízení | **Read** | Pro zobrazení „CZECHMATE-ABC“ v UI. |

3. **Integrace s `game_task`:** při změně stavu hry stejný hook jako pro WS broadcast → **také** enqueue notifikace přes BLE (ne duplikovat serializaci zbytečně — sdílený buffer / jedna funkce `build_snapshot_*`).

4. **Souběh BLE + Wi‑Fi:** měření heapu, limity počtu BLE spojení, test dlouhé partie.

### 3.3 Fallback bez duplicit

- Uživatel na Wi‑Fi používá **stávající** `GET /api/game/snapshot` a `POST` hint — **žádná nová „pravidla“** na ESP pro Wi‑Fi.
- BLE jen **doručuje ekvivalent** dat a příkazů.

---

## 4. iOS aplikace

### 4.1 Transportní vrstva

- Protokol např. `BoardTransport: Actor` nebo `@MainActor` s metodami:
  - `connect()`, `disconnect()`, `snapshotStream` / delegate,
  - `sendHint(from:to:)` (nebo obecnější `sendCommand`),
  - `connectionState: enum { disconnected, scanning, connecting, connected, failed(reason) }`.
- Implementace:
  - **`WiFiBoardTransport`** — současný `ChessboardAPIClient` + `ChessboardWebSocketClient`.
  - **`BLEBoardTransport`** — `CBCentralManager`, scan (filtrovat podle názvu nebo UUID služby), `CBPeripheral`, subscribe notify, write.

### 4.2 `BoardConnectionStore` (nebo nový controller)

- **Výchozí režim:** `auto` = zkus BLE → při úspěchu používat BLE; při chybě / uživatelské volbě přepnout na Wi‑Fi.
- **Uložená preference:** UserDefaults: `preferBluetooth`, `lastWiFiURL`, `lastTransport`.
- **Jeden zdroj pravdy pro UI:** `GameSnapshot?` + `transport: .ble | .wifi` + lidský `statusMessage`.

### 4.3 Oprávnění a Info.plist

- `NSBluetoothAlwaysUsageDescription` — už existuje; text **zjednodušit** pro uživatele (viz §5).
- Volitelně **background** pro BLE central — jen pokud chceme notifikace při zamčeném telefonu (fáze 2+).

### 4.4 Stockfish

- Beze změny: stále přes síť na telefonu; při vypnutém internetu zobrazit stávající hlášku.

---

## 5. UI/UX — jednoduché a jasné

### 5.1 Zásady

- **Jedna primární akce na obrazovku** připojení (např. velké tlačítko „Připojit šachovnici“).
- **Stav ve 3–4 slovech:** „Hledám šachovnici…“, „Připojeno“, „Zkusíme Wi‑Fi…“.
- **Technické detaily** (IP, RTT, UUID) schovat do **„Podrobnosti“** nebo **sekce pro pokročilé**.
- **Konfigurace Wi‑Fi** až ve **druhém kroku** nebo pod „Nemůžu použít Bluetooth“.

### 5.2 Navrhovaný tok (wireframe logika)

1. **Úvod / připojení**  
   - Tlačítko: **„Připojit přes Bluetooth“** (doporučeno).  
   - Pod tím odkaz: **„Použít Wi‑Fi místo Bluetooth“** (otevře jednoduché zadání URL + krátké vysvětlení).

2. **Během skenování**  
   - Animace + text: „Zapni Bluetooth na telefonu a měj šachovnici zapnutou poblíž.“  
   - Seznam nalezených zařízení s **čitelnými jmény** (ne jen MAC).

3. **Po připojení**  
   - Hlavní obrazovka hry **stejná jako dnes**; malý štítek: **„Bluetooth“** nebo **„Wi‑Fi“**.

4. **Chyba BLE**  
   - Krátká věta + tlačítko **„Zkusit Wi‑Fi“** (předvyplnit poslední URL, pokud existuje).

### 5.3 Co vynechat z hlavní cesty

- WebSocket, ETag, polling interval — pouze **diagnostika** / nastavení pro pokročilé.

---

## 6. Fáze implementace (doporučené pořadí)

| Fáze | Obsah | Výstup |
|------|--------|--------|
| **F0** | Dohodnout **GATT mapu** (UUID, velikost payloadů, formát příkazů) — dokument + schválení | Specifikace |
| **F1** | Firmware: NimBLE služba + **notify** snapshotu + **write** jednoho typu příkazu (např. ping / echo) | Ověření z nRF Connect / vlastní skript |
| **F2** | Firmware: napojení na `game_task`, hint stejně jako z HTTP | Parita chování s webem |
| **F3** | iOS: `BLEBoardTransport` + jednoduchá **testovací obrazovka** (scan, connect, zobrazit raw snapshot) | End-to-end BLE |
| **F4** | iOS: sloučit do `BoardConnectionStore`, přepínač BLE / Wi‑Fi, **zjednodušený Connection flow** | Produkční tok |
| **F5** | UX copy, onboarding update, **skrytí expert nastavení** | Připraveno pro TestFlight |
| **F6** | Stabilita: reconnect, odpojení baterie ESP, dlouhá partie | Pevné chování |

---

## 7. Rizika a mitigace

| Riziko | Mitigace |
|--------|----------|
| MTU / velikost JSON | Binární nebo delta snapshot; ne posílat 20 kB každý tah |
| iOS background | Ne slibovat „vždy na pozadí“ ve v1; dokumentovat omezení |
| Dvě současná spojení (BLE+Wi‑Fi) | V UI jeden aktivní transport; druhý vypnout nebo jen pasivní |
| Párování vs „just works“ | BLE často bez PIN; případný PIN v dokumentaci k desce |

---

## 8. Související dokumenty

- [CZECHMATE_MASTERPLAN.md](./CZECHMATE_MASTERPLAN.md) — celková architektura, Fáze 3 BLE  
- [CZECHMATE_IOS_APP_DESIGN.md](./CZECHMATE_IOS_APP_DESIGN.md) — původní BLE návrh  
- [CZECHMATE_COMPLETION_PLAN.md](./CZECHMATE_COMPLETION_PLAN.md) — stav dodělávek  

---

## 9. Otázky k upřesnění (odpověz prosím — upřesní priority)

1. **Stockfish přes internet** zůstává vždy? (Doporučeno ano — BLE jen k desce.)  
2. **Má šachovnice na sobě display** s vlastní IP — má uživatel **někdy** zadávat URL ručně, nebo vždy jen BLE + případně automatická detekce?  
3. **Kolik BLE zařízení** v místnosti současně (turnaj) — potřeba výběr z listu vs. jedna „nejblíže“?  
4. **PIN / bonding** — vyžaduje hardware bezpečnostní párování, nebo stačí anonymní BLE?  
5. **Priorita vývoje:** nejdřív **funkční BLE snapshot** nebo **nejprve jednoduché UI** s mock daty?  
6. **watchOS** — má se synchronizovat stav i přes BLE k telefonu stejně jako dnes přes WC?  

---

*Po odpovědích na §9 lze plán zpřesnit (UUID, texty UI, pořadí fází F1–F3).*

---

## 10. Rozhodnutí 2026-04 — co je v kódu

| Téma | Implementace |
|------|----------------|
| Stockfish jen s internetem | `NetworkStatusMonitor.isInternetLikelyForStockfish` (satisfied + ne constrained) |
| Bez ruční URL | `BoardDiscoveryView` + `LANBoardDiscoveryService` (`_http._tcp`), ESP mDNS |
| Více desek | Seznam BLE + seznam Bonjour |
| Bez PINu | Bez bonding v GATT |
| UI dřív než backend | Mock transport + `useMockBoard`; pak Wi‑Fi / BLE |
| Hodinky | Šablona v [context/watch_standalone/](../../context/watch_standalone/README.md) |
| UUID | `CZECHMATEBLEUUIDs.swift` ↔ `ble_nimble_impl.c` (stejné 128-bit hodnoty) |
