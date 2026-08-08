# Diagrams — firmware architecture

[Full documentation index](../README.md).

Boot sequence, queues, mutexes, task loops, chess pipelines, and occasional Flutter links — all covered graphically here. The same topics in prose with HW notes are in [`reference/TASK_COMMUNICATION.md`](../reference/TASK_COMMUNICATION.md). For C API, run `./generate_docs.sh` locally → `docs/doxygen/html/`.

Queue counts and stack sizes are in [`freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h); lifecycle order is in [`main/main.c`](../../main/main.c).

New diagram ideas go in `LOCAL_DIAGRAM_BACKLOG.md` (gitignore); the starter template is [DIAGRAM_BACKLOG.local.example.md](DIAGRAM_BACKLOG.local.example.md).

Regenerate SVG from repo root: `./scripts/render_docs.sh` (sources in [`sources/*.mmd`](sources/)).

### Poster — algorithm flow

Short color overview for print / presentation: [`poster_algorithm_flow.svg`](poster_algorithm_flow.svg) (Mermaid source: [`sources/poster_algorithm_flow.mmd`](sources/poster_algorithm_flow.mmd)).

**Note:** source of truth is [`sources/poster_algorithm_flow.mmd`](sources/poster_algorithm_flow.mmd) — layout **2×2 columns** (A|B, queue, C|D, E) ≈ square; pastel colors. Print: [`poster_algorithm_flow.svg`](poster_algorithm_flow.svg) **800×800**, or export Mermaid to A4.

---

## Arrow legend

- Solid arrow to a queue ≈ `xQueueSend` / `xQueueReceive`.
- Dashed = optional (`menuconfig`) or indirect call (BLE via web dispatch).
- `main_system_init()` including `ble_task_init()` finishes **before** `create_system_tasks()`.
- `animation_task` / `matter_task` from `main.c` are not used.

---

## Tasks — priority · stack

| Task | P | Stack | Note |
|------|---|-------|----------|
| led_task | 7 | 8 KiB | WS2812B |
| matrix_task | 6 | 4 KiB | reed |
| button_task | 5 | 3 KiB | multiplex |
| game_task | 4 | 6 KiB | ~100 ms loop |
| uart_task | 3 | 5 KiB | resume after boot LED |
| web_server_task | 3 | 20 KiB | WiFi HTTP |
| ha_light_task | 3 | 8 KiB | MQTT |
| test_task | 1 | 4 KiB | menuconfig only |

---

## Queues (capacities)

| Constant | Count |
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

## Init → BLE → tasks

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','noteBkgColor':'#334155','noteTextColor':'#f1f5f9','noteBorderColor':'#475569','loopTextColor':'#e2e8f0','labelBoxBkgColor':'#334155','labelTextColor':'#f8fafc'}}}%%
sequenceDiagram
  participant AM as app_main
  participant SYS as main_system_init
  participant FC as chess_system_init
  participant CR as create_system_tasks
  rect rgb(30, 58, 138)
    AM->>SYS: uart_mutex · timers · queue check
  end
  rect rgb(120, 53, 15)
    SYS->>FC: queues · mutexes
    FC-->>SYS: OK
  end
  rect rgb(22, 101, 52)
    SYS->>SYS: endgame · UART registry · BLE
    SYS-->>AM: ESP_OK
    AM->>CR: xTaskCreate
    CR->>CR: boot LED · game init · resume UART
  end
```

![boot_sequence.svg](boot_sequence.svg)

---

## Task order + runtime queues

![tasks_architecture.svg](tasks_architecture.svg)

---

## Producers → `game_command_queue` → `game_task`

```mermaid
%%{init: {'theme':'dark','themeVariables':{'clusterBkg':'#0f172a','clusterBorder':'#334155','lineColor':'#94a3b8','primaryTextColor':'#f1f5f9','edgeLabelBackground':'#1e293b','titleColor':'#f8fafc'}}}%%
flowchart TB
  subgraph PROD["Command senders"]
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

  classDef taskN fill:#14532d,stroke:#4ade80,stroke-width:2px,color:#bbf7d0
  classDef queueN fill:#7c2d12,stroke:#fb923c,stroke-width:2px,color:#fed7aa
  classDef gameN fill:#1e3a8a,stroke:#38bdf8,stroke-width:2px,color:#e0f2fe
  classDef bleN fill:#312e81,stroke:#818cf8,stroke-width:2px,color:#e0e7ff
```

![queues_flow.svg](queues_flow.svg)

---

## UART round trip

Console and `uart_task` ping-pong through queues — command down, response up.

```mermaid
%%{init: {'theme':'dark','themeVariables':{'clusterBkg':'#0f172a','lineColor':'#94a3b8','primaryTextColor':'#f1f5f9','edgeLabelBackground':'#1e293b','titleColor':'#f8fafc'}}}%%
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

  classDef hw fill:#334155,stroke:#94a3b8,stroke-width:2px,color:#f1f5f9
  classDef t fill:#713f12,stroke:#facc15,stroke-width:2px,color:#fef08a
  classDef q fill:#7c2d12,stroke:#fb923c,stroke-width:2px,color:#fed7aa
  classDef g fill:#1e3a8a,stroke:#38bdf8,stroke-width:2px,color:#e0f2fe
```

---

## Button → queue → `game_task`

Sequential: `button_task` only enqueues events and `game_task` pulls from the queue in its tick loop.

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','loopTextColor':'#e2e8f0'}}}%%
sequenceDiagram
  rect rgb(22, 101, 52)
    participant BT as button_task
    participant BQ as button_event_queue
    participant GT as game_task
    BT->>BQ: event
    loop tick ~100 ms
      GT->>BQ: receive (non-blocking)
      GT->>GT: promotion / reset / …
    end
  end
```

---

## Matrix → move

Same idea as buttons: matrix sends PICKUP/DROP and `game_task` drains the queue itself.

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','loopTextColor':'#e2e8f0'}}}%%
sequenceDiagram
  rect rgb(30, 58, 138)
    participant MT as matrix_task
    participant GQ as game_command_queue
    participant GT as game_task
    MT->>GQ: PICKUP / DROP
    loop tick
      GT->>GQ: drain commands
      GT->>GT: rules
    end
  end
```

---

## LED batch (simplified)

Batching through `led_task` is a separate “pipe” — detail in SVG, source below.

![led_pipeline.svg](led_pipeline.svg)  
Source: [`sources/led_pipeline.mmd`](sources/led_pipeline.mmd)

---

## Auxiliary queues

![auxiliary_queues.svg](auxiliary_queues.svg)

---

## Mutexes

Which task holds which mutex and who accesses it — static image plus simplified flow below.

![mutex_map.svg](mutex_map.svg)

```mermaid
%%{init: {'theme':'dark','themeVariables':{'clusterBkg':'#0f172a','lineColor':'#94a3b8','primaryTextColor':'#f1f5f9','edgeLabelBackground':'#1e293b','titleColor':'#f8fafc'}}}%%
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
  classDef mx fill:#831843,stroke:#f472b6,stroke-width:2px,color:#fce7f3
  classDef t fill:#14532d,stroke:#4ade80,stroke-width:2px,color:#bbf7d0
```

---

## Input topology

Where physical inputs connect to tasks — one overview graph.

![system_topology.svg](system_topology.svg)

---

## Flutter layers (same export as `render_docs`)

Same export as after `./scripts/render_docs.sh`; source is [`sources/client_app_layers.mmd`](sources/client_app_layers.mmd) and the same image is linked from [`docs/flutter/README.md`](../flutter/README.md).

![client_app_layers.svg](client_app_layers.svg)

---

## Applications and clients

Full landscape overview — firmware vs mobile vs optional native client.

| Diagram | Contents |
|---------|--------|
| [`applications_landscape.mmd`](sources/applications_landscape.mmd) | Firmware vs `flutter_czechmate` vs optional native Xcode client |

![applications_landscape.svg](applications_landscape.svg)

---

## Flutter — `lib/` map (features + core)

Simplified directory map — mainly for orientation.

| Diagram | Contents |
|---------|--------|
| [`flutter_app_structure.mmd`](sources/flutter_app_structure.mmd) | Screens vs services vs models (simplified `flutter_czechmate/lib/` directory) |

![flutter_app_structure.svg](flutter_app_structure.svg)  
Layer detail UI→Riverpod→services: [`docs/flutter/README.md`](../flutter/README.md).

---

## Task loops — one diagram per task from `main.c`

Each active task from `main.c` has its own loop diagram in `sources/`; implementation runs in components (`*_task_start`). BLE has no own `xTaskCreate` — host task starts from `ble_task_init`.

**web_server_task:** `wifi_init_apsta()` sets **AP+STA** only when the user board hotspot is enabled in NVS; otherwise **STA-only** (no AP interface config). Then delay, optional `wifi_connect_sta()` from NVS, and HTTP server start — see [`task_web_loop.mmd`](sources/task_web_loop.mmd) and `web_server_task.c`.

| Task | Source file | Main code file |
|------|---------------|-------------------|
| **game_task** | [`task_game_loop.mmd`](sources/task_game_loop.mmd) | `components/game_task/game_task.c` |
| **led_task** | [`task_led_loop.mmd`](sources/task_led_loop.mmd) | `components/led_task/led_task.c` |
| **matrix_task** | [`task_matrix_loop.mmd`](sources/task_matrix_loop.mmd) | `components/matrix_task/matrix_task.c` |
| **button_task** | [`task_button_loop.mmd`](sources/task_button_loop.mmd) | `components/button_task/button_task.c` |
| **uart_task** | [`task_uart_loop.mmd`](sources/task_uart_loop.mmd) | `components/uart_task/uart_task.c` |
| **web_server_task** | [`task_web_loop.mmd`](sources/task_web_loop.mmd) | `components/web_server_task/web_server_task.c` |
| **ha_light_task** | [`task_ha_light_loop.mmd`](sources/task_ha_light_loop.mmd) | `components/ha_light_task/ha_light_task.c` |
| **test_task** (menuconfig) | [`task_test_optional.mmd`](sources/task_test_optional.mmd) | `components/test_task/test_task.c` |
| **NimBLE / BLE** | [`task_ble_stack.mmd`](sources/task_ble_stack.mmd) | `components/ble_task/ble_nimble_impl.c` + dispatch in web layer |

![task_game_loop.svg](task_game_loop.svg)
![task_led_loop.svg](task_led_loop.svg)
![task_matrix_loop.svg](task_matrix_loop.svg)
![task_button_loop.svg](task_button_loop.svg)
![task_uart_loop.svg](task_uart_loop.svg)
![task_web_loop.svg](task_web_loop.svg)
![task_ha_light_loop.svg](task_ha_light_loop.svg)
![task_test_optional.svg](task_test_optional.svg)
![task_ble_stack.svg](task_ble_stack.svg)

---

## Chess logic — validation, move generation, commands

Most logic is in `components/game_task/game_task.c` (one large module). Graphs are simplified — exact `switch` branches and edge cases live in code.

| Topic | Diagram |
|------|---------|
| Move check before execution | [`chess_validation_pipeline.mmd`](sources/chess_validation_pipeline.mmd) → `game_is_valid_move` |
| Rules per piece type | [`chess_piece_validators.mmd`](sources/chess_piece_validators.mmd) → `game_validate_*_enhanced` |
| Legal moves into buffer | [`chess_legal_moves_generation.mmd`](sources/chess_legal_moves_generation.mmd) → `game_generate_*` + `game_simulate_move_check` |
| Move execution | [`chess_execute_move_pipeline.mmd`](sources/chess_execute_move_pipeline.mmd) → `game_execute_move` / `game_execute_move_enhanced` |
| Game command queue | [`game_command_dispatch_overview.mmd`](sources/game_command_dispatch_overview.mmd) → `game_process_commands` |

![chess_validation_pipeline.svg](chess_validation_pipeline.svg)
![chess_piece_validators.svg](chess_piece_validators.svg)
![chess_legal_moves_generation.svg](chess_legal_moves_generation.svg)
![chess_execute_move_pipeline.svg](chess_execute_move_pipeline.svg)
![game_command_dispatch_overview.svg](game_command_dispatch_overview.svg)

---

## Special moves and physical board (`game_task.c`)

Castling, promotion, en passant, etc. — each has its own flow; the table below indexes `.mmd` files and functions.

| Topic | Diagram |
|------|---------|
| **Castling** — king first, rook moved manually | [`chess_flow_castling.mmd`](sources/chess_flow_castling.mmd) · `castling_state`, `game_validate_castling` |
| **Promotion** — waiting for Q/R/B/N | [`chess_flow_promotion.mmd`](sources/chess_flow_promotion.mmd) · `promotion_state`, `game_process_promotion_command` |
| **En passant** — pawn double move + capture | [`chess_flow_en_passant.mmd`](sources/chess_flow_en_passant.mmd) · `game_is_en_passant_possible`, `MOVE_TYPE_EN_PASSANT` |
| **Capture** — `capture_in_progress` | [`chess_flow_guided_capture.mmd`](sources/chess_flow_guided_capture.mmd) · `game_process_drop_command` |
| **Boot / NVS** — snapshot vs new game | [`chess_flow_boot_nvs.mmd`](sources/chess_flow_boot_nvs.mmd) · `game_task_start`, `game_load_snapshot_from_nvs` |

![chess_flow_castling.svg](chess_flow_castling.svg)
![chess_flow_promotion.svg](chess_flow_promotion.svg)
![chess_flow_en_passant.svg](chess_flow_en_passant.svg)
![chess_flow_guided_capture.svg](chess_flow_guided_capture.svg)
![chess_flow_boot_nvs.svg](chess_flow_boot_nvs.svg)

---

## Matrix guard, recovery, resignation, undo

Behavior around sensor vs board mismatches, piece return, king resignation, and undo — index table.

| Topic | Diagram |
|------|---------|
| **Matrix guard** — sensor vs `board[]` mismatch | [`chess_flow_matrix_guard.mmd`](sources/chess_flow_matrix_guard.mmd) · [MATRIX_GUARD.md](../reference/MATRIX_GUARD.md) |
| **Wrong pickup** — returning opponent piece | [`chess_flow_error_recovery.mmd`](sources/chess_flow_error_recovery.mmd) · `GAME_STATE_WAITING_FOR_RETURN`, `error_recovery_state` |
| **Resignation** — lift king 10 s | [`chess_flow_resignation.mmd`](sources/chess_flow_resignation.mmd) · `resignation_start` / `resignation_tick` / `resignation_finalize_timeout` |
| **Undo** | [`chess_flow_undo.mmd`](sources/chess_flow_undo.mmd) · `game_undo_last_move_impl` |

![chess_flow_matrix_guard.svg](chess_flow_matrix_guard.svg)
![chess_flow_error_recovery.svg](chess_flow_error_recovery.svg)
![chess_flow_resignation.svg](chess_flow_resignation.svg)
![chess_flow_undo.svg](chess_flow_undo.svg)

---

## CMake components without a task from `main.c`

Components linked into the image but without their own task from `main.c` — still pulled in by CMake / dependencies.

| Folder | Note |
|--------|----------|
| animation_task | build yes, no `xTaskCreate` |
| matter_task | disabled |
| promotion_button_task, reset_button_task, screen_saver_task | no task in current `main.c` |

---

## Sequential HTML

Long scroll of all diagrams comes from `mermaid_diagrams.txt` → `diagrams_mermaid.html` via `generate_mermaid_html.py` or after `./scripts/render_docs.sh`. A separate long flow is also in `main_flow_diagram.txt`.

**`mermaid_diagrams.txt` is not one diagram.** It contains `#` comments and 26 `sequenceDiagram` blocks. Pasting the whole file into [mermaid.live](https://mermaid.live/) or a Mermaid editor preview yields `UnknownDiagramError: No diagram type detected`.

| Need | Steps |
|-------------|--------|
| All diagrams in browser | `./scripts/render_docs.sh` → open `diagrams_mermaid.html` |
| One diagram (A1, B1, …) | `python3 scripts/docs/generate_mermaid_html.py --extract A1` or after render `extracted/a1_*.mmd` |
| ID list | `python3 scripts/docs/generate_mermaid_html.py --list` |
| mermaid.live | Copy one `extracted/*.mmd` file (starts with `sequenceDiagram`) |

---

*Firmware version: `CMakeLists.txt` → `PROJECT_VERSION`.*
