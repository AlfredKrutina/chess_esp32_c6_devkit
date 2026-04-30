# Flutter aplikace (`flutter_czechmate/`)

Dart klient k desce CZECHMATE: **BLE** nebo **HTTP / WebSocket**. Stav drží hlavně **Riverpod**.

Spuštění: `cd flutter_czechmate && flutter pub get && flutter run`.

Dlouhý lokální seznam nápadů na nové diagramy: **`docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`** (gitignored) — šablona začátku [`DIAGRAM_BACKLOG.local.example.md`](../diagrams/DIAGRAM_BACKLOG.local.example.md).

---

## Vrstvy (features → Riverpod → služby → deska)

![Vrstvy klienta](../diagrams/client_app_layers.svg)  
Zdroj Mermaid: [`../diagrams/sources/client_app_layers.mmd`](../diagrams/sources/client_app_layers.mmd)

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#7b1fa2','clusterBkg':'#fafafa'}}}%%
flowchart TB
  subgraph UI["features/"]
    direction LR
    GA[game]:::u
    CN[connection]:::u
    CH[coach]:::u
    XX[…]:::u
  end
  subgraph RP["Riverpod"]
    NT[notifiers]:::r
  end
  subgraph SV["core/services"]
    BLE[BLE]:::s
    HTTP[HTTP / WS]:::s
    PF[prefs · native]:::s
  end
  subgraph BD["ESP32"]
    FW[firmware]:::e
  end
  UI --> RP --> SV
  BLE <-->|GATT| FW
  HTTP <-->|TCP| FW

  classDef u fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px,color:#4a148c
  classDef r fill:#ede7f6,stroke:#5e35b1,stroke-width:2px,color:#311b92
  classDef s fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20
  classDef e fill:#fff3e0,stroke:#ef6c00,stroke-width:2px,color:#e65100
```

---

## Tabulka `lib/`

| Složka | Co tam je |
|--------|-----------|
| `features/game/` | Partie, šachovnice, hodiny, report |
| `features/connection/` | Scan, session, diagnostika |
| `features/coach/` | AI chat, LLM klienti |
| `features/analysis/` | Evaluace, grafy |
| `features/settings/` | Zařízení, MQTT/HA obrazovky |
| `core/services/` | `ble_czechmate_client`, `board_api_client`, `web_socket_manager`, Stockfish, Live Activity, hodinky |
| `core/models/` | Snapshot, časové kontroly, enumy |
| `app_providers.dart` | Globální providery |
| `app_navigation.dart` | Routy |

---

## Tah z UI na hardware

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'actorBkg':'#f3e5f5','actorBorder':'#7b1fa2','signalColor':'#37474f'}}}%%
sequenceDiagram
  rect rgb(243, 229, 245)
    participant W as Widget
    participant N as Notifier
    participant X as BLE / HTTP klient
    participant D as ESP32
    W->>N: gesto / stisk
    N->>X: příkaz / JSON
    X->>D: GATT nebo HTTP
    D-->>X: odpověď / snapshot
    X-->>N: parsovat
    N-->>W: překreslit šachovnici
  end
```

---

## BLE vs WiFi na desce

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#546e7a'}}}%%
flowchart LR
  subgraph Phone["Telefon"]
    APP[Flutter]:::p
  end
  subgraph Board["ESP32"]
    NIM[NimBLE]:::b
    WEB[web_server_task]:::b
    GT[game_task]:::g
  end
  APP <-->|GATT| NIM
  APP <-->|HTTP| WEB
  NIM --> GT
  WEB --> GT
  classDef p fill:#f3e5f5,stroke:#7b1fa2,stroke-width:2px,color:#4a148c
  classDef b fill:#e8eaf6,stroke:#3949ab,stroke-width:2px,color:#1a237e
  classDef g fill:#fff3e0,stroke:#ef6c00,stroke-width:2px,color:#e65100
```

Příkazy z BLE často jdou přes **`web_server_ble_command_dispatch`** na firmware — nemusí existovat úplně oddělený „BLE protokol“ od web API.

---

## Session stavy

```mermaid
%%{init: {'theme':'base'}}%%
stateDiagram-v2
  [*] --> Hledání
  Hledání --> Připojuji: výběr zařízení
  Připojuji --> Ve_hře: handshake OK
  Ve_hře --> Ve_hře: tahy
  Ve_hře --> Hledání: výpadek / zpět
```

Implementace: `board_session_notifier.dart`, `features/connection/`.

---

## Coach

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#6a1b9a'}}}%%
flowchart LR
  UI[Coach UI]:::u --> CM[coach_manager]:::r
  CM --> LLM[HTTP LLM]:::s
  CM --> SN[snapshot partie]:::x
  classDef u fill:#f3e5f5,stroke:#7b1fa2,color:#4a148c
  classDef r fill:#ede7f6,stroke:#5e35b1,color:#311b92
  classDef s fill:#e8f5e9,stroke:#2e7d32,color:#1b5e20
  classDef x fill:#fff3e0,stroke:#ef6c00,color:#e65100
```

---

## Nativní části

| Platforma | Extra |
|-----------|--------|
| iOS | Live Activities, případně Watch bridge |
| Android | Wear modul, notifikace hodin |

---

## Firmware diagramy

[`docs/diagrams/README.md`](../diagrams/README.md) — FreeRTOS, fronty, LED pipeline.

---

Krátký úvod u samotné appky: [`flutter_czechmate/README.md`](../../flutter_czechmate/README.md).
