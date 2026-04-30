# Flutter aplikace (`flutter_czechmate/`)

Dart klient k šachovnici CZECHMATE. Komunikuje s ESP32 přes **BLE** (GATT) nebo přes **HTTP / WebSocket**, podle toho jak máš desku nahozenou a co aplikace zrovna používá. Stav aplikace drží hlavně **Riverpod**.

Spuštění: `cd flutter_czechmate && flutter pub get && flutter run`.

---

## Co kde je v `lib/`

```mermaid
flowchart TB
  subgraph UI["features/ — obrazovky"]
    G[game]
    C[connection]
    CO[coach]
    A[analysis]
    PU[puzzle]
    LE[learn]
    S[settings]
  end
  subgraph CORE["core/"]
    SV[services — BLE, API, prefs, hodinky, …]
    MD[models]
    UT[utils]
  end
  UI --> CORE
```

| Složka | Účel |
|--------|------|
| `features/game/` | Šachovnice, partie, hodiny, report |
| `features/connection/` | Hledání desky, session, diagnostika |
| `features/coach/` | Chat s AI / LLM backendy |
| `features/analysis/` | Evaluace tahů, grafy |
| `features/settings/` | Zařízení, vývojářské volby, MQTT/HA obrazovky |
| `core/services/` | `ble_czechmate_client`, `board_api_client`, `web_socket_manager`, Stockfish klient, Live Activity, hodinky |
| `core/models/` | Snapshot hry, time control, coach backend enumy |
| `app_providers.dart` | Globální Riverpod setup |
| `app_navigation.dart` | Routy |

---

## Jak proudí tah od uživatele k desce

```mermaid
sequenceDiagram
  participant U as UI widget
  participant N as Notifier Riverpod
  participant BLE as BleCzechMateClient / session
  participant D as ESP32 firmware
  U->>N: tap / drag figurky
  N->>BLE: příkaz nebo tah v JSON podle protokolu
  BLE->>D: GATT write nebo HTTP POST
  D-->>BLE: notifikace / odpověď / snapshot
  BLE-->>N: parse stav
  N-->>U: překreslení šachovnice
```

---

## BLE vs síť (zjednodušeně)

```mermaid
flowchart LR
  subgraph Mob["Telefon"]
    APP[Flutter]
  end
  subgraph Deska["ESP32"]
    NIM[NimBLE]
    WEB[web_server_task]
    GT[game_task]
  end
  APP <-->|GATT| NIM
  APP <-->|HTTP / WS| WEB
  NIM --> WEB
  WEB --> GT
```

Na firmware straně BLE příkazy často končí ve stejné logice jako web (`web_server_ble_command_dispatch` v `ble_nimble_impl.c`). Proto v aplikaci nemusí být úplně jiný „jazyk“ pro každý kanál — záleží na konkrétním endpointu/GATT charakteristikách implementovaných v repu.

---

## Životní cyklus „session“ k desce

```mermaid
stateDiagram-v2
  [*] --> Discovery: otevřu připojení
  Discovery --> Connecting: vybrané zařízení
  Connecting --> InGame: handshake OK
  InGame --> InGame: tahy / přestávky
  InGame --> Discovery: ztráta spojení / uživatel zpět
```

Implementace: `board_session_notifier.dart`, `board_session_state.dart`, obrazovky v `features/connection/`.

---

## Coach (AI)

```mermaid
flowchart LR
  UI[Coach UI] --> CM[coach_manager / notifiers]
  CM --> LLM[coach_llm_clients · HTTP API]
  CM --> LOC[Volitelně lokální / mock služby]
  CM --> SNAP[game snapshot z aktuální partie]
```

Soubory: `features/coach/*`, `core/services/coach_ai_completion_service.dart`, `coach_llm_clients.dart`.

---

## Nativní části mimo Dart

| Platforma | Co je navíc |
|-----------|-------------|
| **iOS** | Live Activities (`ios/ChessLiveActivityExtension/`, `LiveActivityNativeController.swift`), Watch bridge dle aktuálního stavu projektu |
| **Android** | Wear modul (`android/wear/`), notifikace šachových hodin v `MainActivity` / Kotlin pomocné třídy |

---

## Čtení na firmware stranu

Diagramy FreeRTOS a front: [`docs/diagrams/README.md`](../diagrams/README.md). Checklist integrace: [`docs/reference/CZECHMATE_INTEGRATION_CHECKLIST.md`](../reference/CZECHMATE_INTEGRATION_CHECKLIST.md).

---

Krátký úvod přímo ve složce appky: [`flutter_czechmate/README.md`](../../flutter_czechmate/README.md).
