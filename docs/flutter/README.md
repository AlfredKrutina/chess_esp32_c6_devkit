# Flutter aplikace (`flutter_czechmate/`)

**Mapa celé dokumentace (firmware + reference + diagramy):** [`docs/README.md`](../README.md).

Dart klient k desce CZECHMATE: **BLE** nebo **HTTP / WebSocket**. Stav drží hlavně **Riverpod**. **Šachová pravidla a stav partie na desce** běží ve firmware [`game_task`](../../components/game_task/); Flutter používá API snapshotů / streamů a lokálně např. balíček `chess` tam, kde je v kódu potřeba — není náhrada za `game_task.c`.

Spuštění: `cd flutter_czechmate && flutter pub get && flutter run`.

**APK + DMG z CI:** workflow [Flutter app release](../../.github/workflows/flutter-app-release.yml) při pushi na `main`/`master` (změny ve `flutter_czechmate/**`) automaticky buildí a publikuje na [Releases](https://github.com/alfredkrutina/chess_esp32_c6_devkit/releases); ručně lze spustit *Run workflow* nebo tag `app-*`.

Dlouhý lokální seznam nápadů na nové diagramy: **`docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`** (gitignored) — šablona začátku [`DIAGRAM_BACKLOG.local.example.md`](../diagrams/DIAGRAM_BACKLOG.local.example.md).

---

## Vrstvy (features → Riverpod → služby → deska)

![Vrstvy klienta](../diagrams/client_app_layers.svg)  
Zdroj Mermaid: [`../diagrams/sources/client_app_layers.mmd`](../diagrams/sources/client_app_layers.mmd)

Širší mapa složek `lib/` (features + core + navigace): [`../diagrams/flutter_app_structure.svg`](../diagrams/flutter_app_structure.svg) · [`sources/flutter_app_structure.mmd`](../diagrams/sources/flutter_app_structure.mmd).

```mermaid
%%{init: {'theme':'dark','themeVariables':{'lineColor':'#a78bfa','clusterBkg':'#0f172a','clusterBorder':'#334155','primaryTextColor':'#f1f5f9','edgeLabelBackground':'#1e293b','titleColor':'#f8fafc'}}}%%
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

  classDef u fill:#581c87,stroke:#c084fc,stroke-width:2px,color:#f3e8ff
  classDef r fill:#4c1d95,stroke:#a78bfa,stroke-width:2px,color:#ede9fe
  classDef s fill:#14532d,stroke:#4ade80,stroke-width:2px,color:#bbf7d0
  classDef e fill:#7c2d12,stroke:#fb923c,stroke-width:2px,color:#fed7aa
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
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#c084fc','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1'}}}%%
sequenceDiagram
  rect rgb(88, 28, 135)
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
%%{init: {'theme':'dark','themeVariables':{'clusterBkg':'#0f172a','lineColor':'#94a3b8','primaryTextColor':'#f1f5f9','titleColor':'#f8fafc'}}}%%
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
  classDef p fill:#581c87,stroke:#c084fc,stroke-width:2px,color:#f3e8ff
  classDef b fill:#312e81,stroke:#818cf8,stroke-width:2px,color:#e0e7ff
  classDef g fill:#7c2d12,stroke:#fb923c,stroke-width:2px,color:#fed7aa
```

Příkazy z BLE často jdou přes **`web_server_ble_command_dispatch`** na firmware — nemusí existovat úplně oddělený „BLE protokol“ od web API.

---

## Session stavy

```mermaid
%%{init: {'theme':'dark'}}%%
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
%%{init: {'theme':'dark','themeVariables':{'lineColor':'#a78bfa','primaryTextColor':'#f1f5f9'}}}%%
flowchart LR
  UI[Coach UI]:::u --> CM[coach_manager]:::r
  CM --> LLM[HTTP LLM]:::s
  CM --> SN[snapshot partie]:::x
  classDef u fill:#581c87,stroke:#c084fc,color:#f3e8ff
  classDef r fill:#4c1d95,stroke:#a78bfa,color:#ede9fe
  classDef s fill:#14532d,stroke:#4ade80,color:#bbf7d0
  classDef x fill:#7c2d12,stroke:#fb923c,color:#fed7aa
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
