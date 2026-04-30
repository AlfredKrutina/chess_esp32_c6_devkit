# Architektura FreeRTOS tasků (zdroj pravdy pro diagram)

Tento soubor popisuje **tasky skutečně vytvářené v** [`main/main.c`](../../main/main.c) a konstanty v [`components/freertos_chess/include/freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h).

**Generované obrázky:** stejný graf jako `sources/tasks_architecture.mmd` → po `./scripts/render_docs.sh` vznikne **`tasks_architecture.svg`** a **`tasks_architecture.png`** (starší ruční PNG může být nahrazeno). Při rozporu platí **kód** a `.mmd`.

## Mermaid (zkopíruj do [mermaid.live](https://mermaid.live))

```mermaid
flowchart TB
  subgraph Tasks["FreeRTOS tasky — create_system_tasks()"]
    LED["led_task<br/>P7 · 8 KiB stack"]
    MAT["matrix_task<br/>P6 · 4 KiB"]
    BTN["button_task<br/>P5 · 3 KiB"]
    UART["uart_task<br/>P3 · 5 KiB<br/>(po bootu resume)"]
    GAME["game_task<br/>P4 · 6 KiB"]
    WEB["web_server_task<br/>P3 · 20 KiB"]
    HA["ha_light_task<br/>P3 · 8 KiB"]
    TEST["test_task<br/>P1 · 4 KiB<br/>jen CONFIG_CHESS_ENABLE_TEST_TASK"]
  end

  subgraph BLE["Bluetooth LE"]
    NIM["NimBLE host task<br/>ble_task_init() → ble_nimble_stack_init()<br/>není vlastní xTaskCreate ble_task"]
  end

  subgraph Disabled["Nevytváří se"]
    ANIM["animation_task<br/>komentář v main.c — animace v led_task"]
    MATTER["matter_task<br/>zakázáno"]
  end

  MAIN[["app_main"]] --> Tasks
  MAIN --> NIM

  GQ[("game_command_queue<br/>24 × chess_move_command_t")]
  BQ[("button_event_queue<br/>5")]
  MAT --> GQ
  BTN --> BQ
  UART --> GQ
  WEB --> GQ
  GAME --> GQ
  GAME -.-> BQ
```

## Klíčové fronty (viz `freertos_chess.c`)

| Fronta | Konstanta | Položky |
|--------|-----------|---------|
| Herní příkazy | `GAME_QUEUE_SIZE` | **24** |
| Tlačítka | `BUTTON_QUEUE_SIZE` | 5 |
| UART odpovědi | `UART_QUEUE_SIZE` | **10** (`game_response_t`) |
| Animation API | `ANIMATION_QUEUE_SIZE` | 5 (fronty existují; **animation_task** se ve `main.c` **nevytváří**) |

## LED synchronizace

- Mutex: **`led_unified_mutex`** (`led_task.c`) — batch commit + `led_strip_refresh()`.
