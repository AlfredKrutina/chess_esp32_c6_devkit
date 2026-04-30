# Architektura FreeRTOS tasků (zdroj pravdy: kód)

Popis odpovídá **`main/main.c`** (`create_system_tasks`, `main_system_init`) a konstantám ve **`components/freertos_chess/include/freertos_chess.h`**.

**Export obrázků:** [`sources/tasks_architecture.mmd`](sources/tasks_architecture.mmd) → `./scripts/render_docs.sh` vyrobí **`tasks_architecture.svg`** a **`.png`**.

**Přehled všech diagramů:** [README.md v této složce](README.md).

## Mermaid

```mermaid
flowchart TB
  subgraph Init["Před schedulerem (app_main)"]
    SYS["main_system_init()<br/>chess_system_init, timery,<br/>UART příkazy, ble_task_init()"]
  end

  subgraph Tasks["FreeRTOS tasky — create_system_tasks()"]
    LED["led_task<br/>P7 · 8 KiB"]
    MAT["matrix_task<br/>P6 · 4 KiB"]
    BTN["button_task<br/>P5 · 3 KiB"]
    UART["uart_task<br/>P3 · 5 KiB<br/>resume po boot animaci"]
    GAME["game_task<br/>P4 · 6 KiB"]
    WEB["web_server_task<br/>P3 · 20 KiB"]
    HA["ha_light_task<br/>P3 · 8 KiB"]
    TEST["test_task<br/>P1 · 4 KiB<br/>jen CONFIG_CHESS_ENABLE_TEST_TASK"]
  end

  subgraph BLE["Bluetooth LE"]
    NIM["NimBLE host task<br/>z ble_task_init()<br/>žádný vlastní ble_task"]
  end

  subgraph Disabled["V main.c vypnuto"]
    ANIM["animation_task — zakomentováno"]
    MATTER["matter_task — zakomentováno"]
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

## Klíčové fronty (`freertos_chess.c`)

| Fronta | Konstanta | Poznámka |
|--------|-----------|----------|
| Herní příkazy | `GAME_QUEUE_SIZE` | **24** |
| Tlačítka | `BUTTON_QUEUE_SIZE` | 5 |
| UART odpovědi | `UART_QUEUE_SIZE` | **10** (`game_response_t`) |
| Animation API | `ANIMATION_QUEUE_SIZE` | 5 — **`animation_task` se ve `main.c` nevytváří** |

## LED synchronizace

Mutex **`led_unified_mutex`** (`led_task.c`) — batch commit a `led_strip_refresh()`.
