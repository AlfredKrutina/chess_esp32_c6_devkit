# Diagramy — firmware CZECHMATE

**Verze:** viz [`CMakeLists.txt`](../../CMakeLists.txt) (`PROJECT_VERSION`). **Zdroj pravdy pro tasky:** [`main/main.c`](../../main/main.c) (`create_system_tasks`, `main_system_init`), konstanty front v [`freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h).

Úplná knihovna **sekvenčních** diagramů (generuje se do HTML): [`mermaid_diagrams.txt`](mermaid_diagrams.txt) → po `./scripts/render_docs.sh` otevři [`diagrams_mermaid.html`](diagrams_mermaid.html). **SVG/PNG** z `.mmd`: `./scripts/render_docs.sh` (složka [`sources/`](sources/)).

---

## FreeRTOS tasky a vztah k frontám

```mermaid
flowchart TB
  subgraph Init["Před stabilním během tasků"]
    SYS["main_system_init() · chess_system_init, timery, BLE init"]
  end

  subgraph Tasks["create_system_tasks()"]
    LED["led_task · P7 · 8 KiB"]
    MAT["matrix_task · P6 · 4 KiB"]
    BTN["button_task · P5 · 3 KiB"]
    UART["uart_task · P3 · 5 KiB"]
    GAME["game_task · P4 · 6 KiB"]
    WEB["web_server_task · P3 · 20 KiB"]
    HA["ha_light_task · P3 · 8 KiB"]
    TEST["test_task · P1 · menuconfig"]
  end

  subgraph BLE["BLE"]
    NIM["NimBLE host task · ble_task_init()"]
  end

  subgraph Off["Není xTaskCreate v main.c"]
    ANIM["animation_task"]
    MATTER["matter_task"]
  end

  MAIN[["app_main"]] --> SYS
  SYS --> Tasks
  MAIN --> NIM

  GQ[("game_command_queue · 24")]
  BQ[("button_event_queue · 5")]
  MAT --> GQ
  BTN --> BQ
  UART --> GQ
  WEB --> GQ
  BQ --> GAME
  GQ --> GAME
```

---

## Boot: `app_main` → init → tasky → UART resume

```mermaid
sequenceDiagram
  participant AM as app_main
  participant SYS as main_system_init
  participant CH as chess_system_init
  participant BT as create_system_tasks

  AM->>SYS: init_console, TWDT
  SYS->>CH: fronty, mutexy, timery
  CH-->>SYS: ESP_OK
  SYS->>SYS: register_extended_uart_commands
  SYS->>SYS: ble_task_init
  SYS-->>AM: ESP_OK

  AM->>BT: xTaskCreate sys tasky
  Note over BT: uart_task suspended
  BT->>BT: show_boot_animation_and_board
  BT->>BT: initialize_chess_game
  BT->>BT: vTaskResume(uart_task)
```

---

## Fronty: tahy a odpovědi konzoli

```mermaid
flowchart TB
  MT[matrix_task] -->|pickup/drop| GQ[("game_command_queue · 24")]
  BTN[button_task] -->|button_event_t| BQ[("button_event_queue · 5")]
  UART[uart_task] -->|příkazy/tahy| GQ
  WEB[web_server_task] -->|HTTP| GQ

  BQ -->|receive| GT[game_task]
  GQ -->|receive| GT

  GT -->|game_response_t| URQ[("uart_response_queue · 10")]
  URQ --> UART
```

---

## Tah ze senzorové matice (zjednodušeně)

```mermaid
sequenceDiagram
  participant MT as matrix_task
  participant GQ as game_command_queue
  participant GT as game_task

  MT->>GQ: GAME_CMD_PICKUP / přenos figury
  MT->>GQ: GAME_CMD_DROP / položení
  loop cyklus game_task (~100 ms)
    GT->>GQ: xQueueReceive (non-blocking batch)
    GT->>GT: validace, pravidla, stav hry
  end
```

---

## BLE a síťové rozhraní

```mermaid
flowchart LR
  BLE["ble_task_init()"] --> NIM["NimBLE host task"]
  WEB["web_server_task"] --> WIFI["WiFi AP/STA"]
  HA["ha_light_task"] --> MQTT["MQTT · Home Assistant RGB"]
  APP["Flutter / klient"] -.->|BLE GATT| NIM
  APP -.->|HTTP/WS| WEB
```

---

## Komponenty ve složce `components/` vs. task z `main.c`

Složky jako `animation_task`, `matter_task`, `promotion_button_task`, `reset_button_task`, `screen_saver_task` jsou v **CMake** přilinkované jako knihovny; **samostatný FreeRTOS task z `main.c`** z nich aktuálně nevzniká tam, kde je kód v `main.c` zakomentovaný nebo init se nevolá. Aktivní rozhraní pro vstup jsou především **matrix_task**, **button_task**, **uart_task**, **web_server_task** a **BLE stack**.

---

## Další soubory

| Soubor | Obsah |
|--------|--------|
| [`mermaid_diagrams.txt`](mermaid_diagrams.txt) | Sekvence A–J+ (kompletní toky, err handling, web, test…) |
| [`main_flow_diagram.txt`](main_flow_diagram.txt) | Jedna dlouhá sekvence „hlavní smyčka“ pro ladění |
| [`sources/*.mmd`](sources/) | Zdroje pro CLI / CI → SVG/PNG vedle stejného jména |
| [`tasks_architecture.md`](tasks_architecture.md) | Duplicitní embed stejného grafu + tabulka front |
