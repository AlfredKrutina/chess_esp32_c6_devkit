# Flutter klient (`flutter_czechmate/`)

[Rozcestník celého repa](../README.md).

Aplikace umí **BLE** nebo **HTTP / WebSocket**, stav držím přes **Riverpod**. Na **Windows** je v této codebase jen síťová větev (BLE stack chybí ve `flutter_blue_plus`). Partie sama o sobě žije ve firmware [`game_task`](../../components/game_task/) — klient v Dartu synchronizuje snapshoty a API; balíček `chess` používám tam, kde pomůže UI, ne jako náhradu celého `game_task`.

```bash
cd flutter_czechmate && flutter pub get && flutter run
```

### Windows desktop

- **Předpoklady:** Windows 10/11, [Flutter](https://docs.flutter.dev/get-started/install/windows) na stable kanálu, **Visual Studio 2022** s úlohou *Desktop development with C++* (CMake, MSVC, Windows SDK).
- **Spuštění:** `flutter pub get && flutter run -d windows`.
- **Release:** `flutter build windows` — spustitelná aplikace typicky v `build/windows/x64/runner/Release/` (zkopíruj celou složku včetně dat DLL).
- **Bluetooth:** knihovna `flutter_blue_plus` v tomto projektu **nemá** backend pro Windows. Klient BLE API nevolá; připojení k desce je přes **HTTP / WebSocket** (stejná síť jako počítač, URL z webového rozhraní desky nebo z telefonu po zprovoznění Wi‑Fi). BLE sken a OTA přes GATT vyžadují Android / iOS / macOS / Linux.
- **CI instalátor:** při pushi na `main`/`master`, který mění `flutter_czechmate/**`, běží [`.github/workflows/flutter-app-release.yml`](../../.github/workflows/flutter-app-release.yml) na GitHub Actions — job `windows` udělá `flutter build windows --release` a zabalí výstup Inno Setup skriptem `flutter_czechmate/installer/windows/CzechMateSetup.iss` do `czechmate-<ver>-windows-setup.exe` na Releases.

Hotové buildy: [GitHub Releases](https://github.com/alfredkrutina/chess_esp32_c6_devkit/releases).

Nápady na nové diagramy si píšu lokálně do `docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md`, vzor je [DIAGRAM_BACKLOG.local.example.md](../diagrams/DIAGRAM_BACKLOG.local.example.md).

---

## Vrstvy

![Vrstvy klienta](../diagrams/client_app_layers.svg)  
Mermaid: [client_app_layers.mmd](../diagrams/sources/client_app_layers.mmd)

Širší mapa `lib/`: [flutter_app_structure.svg](../diagrams/flutter_app_structure.svg) · [flutter_app_structure.mmd](../diagrams/sources/flutter_app_structure.mmd)

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

## `lib/`

| Složka | Role |
|--------|------|
| `features/game/` | Partie, šachovnice, hodiny, report |
| `features/connection/` | Scan, session |
| `features/coach/` | AI chat, LLM |
| `features/analysis/` | Evaluace |
| `features/settings/` | Zařízení, MQTT/HA, OTA (`firmware_update_section`, `firmware_ota_runner`, manifest) |
| `core/services/` | `ble_czechmate_client`, `board_api_client`, `firmware_phone_host_ota`, WS, Stockfish, … |
| `core/models/` | Snapshot, enumy |
| `app_providers.dart` | Providery |
| `app_navigation.dart` | Routy |

---

## Tok příkazu na desku

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#c084fc','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1'}}}%%
sequenceDiagram
  rect rgb(88, 28, 135)
    participant W as Widget
    participant N as Notifier
    participant X as BLE / HTTP
    participant D as ESP32
    W->>N: akce
    N->>X: příkaz
    X->>D: GATT nebo HTTP
    D-->>X: odpověď / snapshot
    X-->>N: parse
    N-->>W: rebuild
  end
```

---

## BLE vs HTTP na desce

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

JSON z BLE často končí ve `web_server_ble_command_dispatch` — stejná logika jako část web API.

---

## OTA firmwaru ESP32

[docs/ota_architecture.md](../ota_architecture.md) — HTTPS se STA, HTTP z telefonu, BLE chunky `OB`, REST, Bearer.

Dart: `BoardSessionNotifier.requestFirmwareOta` / `uploadFirmwareOtaBle`, `FirmwareOtaRunner`, `FirmwarePhoneHostOta`, `BleCzechmateClient.uploadFirmwareBle`.

E2E poznámky k OTA si lze vést lokálně (např. vlastní checklist); veřejný popis kanálů a API je v [`docs/ota_architecture.md`](../ota_architecture.md).

---

## Session

```mermaid
%%{init: {'theme':'dark'}}%%
stateDiagram-v2
  [*] --> Hledání
  Hledání --> Připojuji: výběr zařízení
  Připojuji --> Ve_hře: handshake OK
  Ve_hře --> Ve_hře: tahy
  Ve_hře --> Hledání: výpadek / zpět
```

Kód: `board_session_notifier.dart`, `features/connection/`.

---

## Coach

```mermaid
%%{init: {'theme':'dark','themeVariables':{'lineColor':'#a78bfa','primaryTextColor':'#f1f5f9'}}}%%
flowchart LR
  UI[Coach UI]:::u --> CM[coach_manager]:::r
  CM --> LLM[HTTP LLM]:::s
  CM --> SN[snapshot]:::x
  classDef u fill:#581c87,stroke:#c084fc,color:#f3e8ff
  classDef r fill:#4c1d95,stroke:#a78bfa,color:#ede9fe
  classDef s fill:#14532d,stroke:#4ade80,color:#bbf7d0
  classDef x fill:#7c2d12,stroke:#fb923c,color:#fed7aa
```

---

## Nativní vrstvy

| Platforma | |
|-----------|--|
| iOS | Live Activities, Watch |
| Android | Wear, notifikace |

---

## Firmware diagramy

[diagrams/README.md](../diagrams/README.md) — tasky, boot, LED pipeline.

[flutter_czechmate/README.md](../../flutter_czechmate/README.md) — krátký start z kořene aplikace.
