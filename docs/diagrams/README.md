# Diagramy (firmware)

Čísla ber z [`freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h) a [`main/main.c`](../../main/main.c). Delší popis komunikace: [`reference/KOMUNIKACE_MEZI_TASKY.md`](../reference/KOMUNIKACE_MEZI_TASKY.md).

**Nápady na další grafy** si piš lokálně do souboru **`LOCAL_DIAGRAM_BACKLOG.md`** — je v `.gitignore`, do remote neleze. Start šablony: [`DIAGRAM_BACKLOG.local.example.md`](DIAGRAM_BACKLOG.local.example.md).

SVG z této složky: `./scripts/render_docs.sh` nad [`sources/*.mmd`](sources/).

---

## Legenda šipek

- Plná šipka na frontu ≈ `xQueueSend` / `xQueueReceive`.
- Čárkovaná = optional (`menuconfig`) nebo nepřímé volání (BLE přes web dispatch).
- `main_system_init()` včetně `ble_task_init()` doběhne **před** `create_system_tasks()`.
- `animation_task` / `matter_task` se z `main.c` nespouštějí.

---

## Tasky — priorita · stack

| Task | P | Stack | Poznámka |
|------|---|-------|----------|
| led_task | 7 | 8 KiB | WS2812B |
| matrix_task | 6 | 4 KiB | reed |
| button_task | 5 | 3 KiB | multiplex |
| game_task | 4 | 6 KiB | ~100 ms smyčka |
| uart_task | 3 | 5 KiB | resume po boot LED |
| web_server_task | 3 | 20 KiB | WiFi HTTP |
| ha_light_task | 3 | 8 KiB | MQTT |
| test_task | 1 | 4 KiB | jen menuconfig |

---

## Fronty (kapacity)

| Konstanta | Počet |
|-----------|-------|
| GAME_QUEUE_SIZE | 24 |
| BUTTON_QUEUE_SIZE | 5 |
| UART_QUEUE_SIZE | 10 |
| MATRIX_QUEUE_SIZE | 8 |
| ANIMATION_QUEUE_SIZE | 5 |
| WEB_SERVER_QUEUE_SIZE | 10 |
| SCREEN_SAVER_QUEUE_SIZE | 3 |
| TEST_COMMAND_QUEUE_SIZE | 16 |

---

## Init → BLE → tasky

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'actorBkg':'#e3f2fd','actorBorder':'#1565c0','signalColor':'#37474f'}}}%%
sequenceDiagram
  participant AM as app_main
  participant SYS as main_system_init
  participant FC as chess_system_init
  participant CR as create_system_tasks
  rect rgb(227, 242, 253)
    AM->>SYS: uart_mutex · timery · kontrola front
  end
  rect rgb(255, 243, 224)
    SYS->>FC: fronty · mutexy
    FC-->>SYS: OK
  end
  rect rgb(232, 245, 233)
    SYS->>SYS: endgame · UART registry · BLE
    SYS-->>AM: ESP_OK
    AM->>CR: xTaskCreate
    CR->>CR: boot LED · init hry · resume UART
  end
```

![boot_sequence.svg](boot_sequence.svg)

---

## Pořadí tasků + runtime fronty

![tasks_architecture.svg](tasks_architecture.svg)

---

## Produkce → `game_command_queue` → `game_task`

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#455a64'}}}%%
flowchart TB
  subgraph PROD["Posílá příkazy"]
    MT[matrix_task]:::taskN
    BTN[button_task]:::taskN
    UART[uart_task]:::taskN
    WEB[web_server_task]:::taskN
    BLE[NimBLE dispatch]:::bleN
  end
  GQ[("game_command_queue")]:::queueN
  BQ[("button_event_queue")]:::queueN
  GT[game_task]:::gameN

  MT --> GQ
  UART --> GQ
  WEB --> GQ
  BLE --> GQ
  BTN --> BQ
  GQ --> GT
  BQ --> GT

  classDef taskN fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20
  classDef queueN fill:#fff3e0,stroke:#ef6c00,stroke-width:2px,color:#bf360c
  classDef gameN fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#0d47a1
  classDef bleN fill:#e8eaf6,stroke:#3949ab,stroke-width:2px,color:#1a237e
```

![queues_flow.svg](queues_flow.svg)

---

## UART tam a zpět

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#455a64'}}}%%
flowchart LR
  SER[USB serial]:::hw
  UT[uart_task]:::t
  GQ[(game_command_queue)]:::q
  GT[game_task]:::g
  URQ[(uart_response_queue)]:::q

  SER <--> UT
  UT --> GQ
  GQ --> GT
  GT --> URQ
  URQ --> UT

  classDef hw fill:#eceff1,stroke:#546e7a,stroke-width:2px,color:#263238
  classDef t fill:#fff8e1,stroke:#f9a825,stroke-width:2px,color:#f57f17
  classDef q fill:#fff3e0,stroke:#ef6c00,stroke-width:2px,color:#bf360c
  classDef g fill:#e3f2fd,stroke:#1565c0,stroke-width:2px,color:#0d47a1
```

---

## Tlačítko → fronta → `game_task`

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'signalColor':'#37474f'}}}%%
sequenceDiagram
  rect rgb(232, 245, 233)
    participant BT as button_task
    participant BQ as button_event_queue
    participant GT as game_task
    BT->>BQ: událost
    loop tick ~100 ms
      GT->>BQ: receive (non-blocking)
      GT->>GT: promoce / reset / …
    end
  end
```

---

## Matrix → tah

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'signalColor':'#37474f'}}}%%
sequenceDiagram
  rect rgb(227, 242, 253)
    participant MT as matrix_task
    participant GQ as game_command_queue
    participant GT as game_task
    MT->>GQ: PICKUP / DROP
    loop tick
      GT->>GQ: drain příkazů
      GT->>GT: pravidla
    end
  end
```

---

## LED batch (zjednodušení)

![led_pipeline.svg](led_pipeline.svg)  
Zdroj: [`sources/led_pipeline.mmd`](sources/led_pipeline.mmd)

---

## Vedlejší fronty

![auxiliary_queues.svg](auxiliary_queues.svg)

---

## Mutexy

![mutex_map.svg](mutex_map.svg)

```mermaid
%%{init: {'theme':'base', 'themeVariables': {'lineColor':'#78909c'}}}%%
flowchart TB
  UM[uart_mutex]:::mx
  MM[matrix_mutex]:::mx
  GM[game_mutex]:::mx
  LM[led_unified_mutex]:::mx
  UT[uart_task]:::t
  MT[matrix_task]:::t
  GT[game_task]:::t
  LT[led_task]:::t
  UM -.-> UT
  MM -.-> MT
  GM -.-> GT
  LM -.-> LT
  GT -.-> LM
  GT -.-> MM
  classDef mx fill:#fce4ec,stroke:#c2185b,stroke-width:2px,color:#880e4f
  classDef t fill:#e8f5e9,stroke:#2e7d32,stroke-width:2px,color:#1b5e20
```

---

## Topologie vstupů

![system_topology.svg](system_topology.svg)

---

## Flutter vrstvy (stejný export co `render_docs`)

Obrázek je vygenerovaný z [`sources/client_app_layers.mmd`](sources/client_app_layers.mmd) — používá ho i [`docs/flutter/README.md`](../flutter/README.md).

![client_app_layers.svg](client_app_layers.svg)

---

## CMake komponenty bez tasku z `main.c`

| Složka | Poznámka |
|--------|----------|
| animation_task | build ano, `xTaskCreate` ne |
| matter_task | vypnuto |
| promotion_button_task, reset_button_task, screen_saver_task | žádný task v současném `main.c` |

---

## Sekvenční HTML

`mermaid_diagrams.txt` → `diagrams_mermaid.html` přes `generate_mermaid_html.py` nebo `./scripts/render_docs.sh`. Dlouhý jeden diagram: `main_flow_diagram.txt`.

---

*Firmware verze: `CMakeLists.txt` → `PROJECT_VERSION`.*
