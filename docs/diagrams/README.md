# Diagramy (firmware)

Čísla priorit, velikostí front a stacků najdeš v [`freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h). Pořadí startu tasků v [`main/main.c`](../../main/main.c). Když potřebuješ popsat komunikaci řádek po řádku, je tam [`reference/KOMUNIKACE_MEZI_TASKY.md`](../reference/KOMUNIKACE_MEZI_TASKY.md).

Obrázky SVG ti vyrobí `./scripts/render_docs.sh` ze složky [`sources/`](sources/) (stejné jméno jako `.mmd`).

---

## Jak číst šipky

- Plná šipka na frontu = typicky někdo posílá (`xQueueSend`), druhý bere (`xQueueReceive`).
- Čárkovaná = volitelné (menuconfig) nebo nepřímé volání (např. BLE přes funkce web vrstvy).
- `main_system_init()` doběhne dřív než `create_system_tasks()` — včetně `ble_task_init()`.
- `animation_task` a `matter_task` se v `main.c` nevytváří (zakomentované).

---

## Tasky — priorita a stack

| Task | P | Stack | Poznámka |
|------|---|-------|----------|
| led_task | 7 | 8 KiB | WS2812B |
| matrix_task | 6 | 4 KiB | reed matice |
| button_task | 5 | 3 KiB | multiplex tlačítek |
| game_task | 4 | 6 KiB | šachy, NVS, smyčka ~100 ms |
| uart_task | 3 | 5 KiB | po boot animaci resume |
| web_server_task | 3 | 20 KiB | WiFi + HTTP |
| ha_light_task | 3 | 8 KiB | MQTT |
| test_task | 1 | 4 KiB | jen `CONFIG_CHESS_ENABLE_TEST_TASK` |

---

## Fronty (kapacity z hlavičky)

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

## Časová řada: init → BLE → tasky

```mermaid
sequenceDiagram
  participant AM as app_main
  participant SYS as main_system_init
  participant FC as chess_system_init
  participant CR as create_system_tasks
  AM->>SYS: mutex UART + FC + timery + endgame + UART příkazy + BLE
  SYS->>FC: fronty, matrix_mutex, game_mutex
  FC-->>SYS: OK
  SYS-->>AM: OK
  AM->>CR: xTaskCreate řada
  CR->>CR: 1 s čekání · boot LED · initialize_chess_game · resume UART
```

![Boot SVG](boot_sequence.svg)

---

## Pořadí xTaskCreate + kde tečou herní zprávy

Řádek **led → matrix → button → uart (nejprve suspend) → game → [test] → web → ha** je z `main.c`. Šipky dolů na fronty ukazují **provoz**, ne pořadí startu.

![Tasky + fronty](tasks_architecture.svg)

---

## Kdo feeduje game_command_queue a kdo ho čte

```mermaid
flowchart TB
  MT[matrix_task] --> GQ[(game_command_queue)]
  UT[uart_task] --> GQ
  WT[web_server_task] --> GQ
  BLE[NimBLE + web dispatch] --> GQ
  GQ --> GT[game_task]
  BT[button_task] --> BQ[(button_event_queue)]
  BQ --> GT
```

![Fronty SVG](queues_flow.svg)

---

## Tlačítko → fronta → hra

```mermaid
sequenceDiagram
  participant BT as button_task
  participant BQ as button_event_queue
  participant GT as game_task
  BT->>BQ: stisk / dlouhý stisk / …
  loop každý tick game_task
    GT->>BQ: receive non-blocking
    GT->>GT: promoce / reset / … dle typu
  end
```

---

## Matrix → tah

```mermaid
sequenceDiagram
  participant MT as matrix_task
  participant GQ as game_command_queue
  participant GT as game_task
  MT->>GQ: PICKUP / DROP
  loop ~100 ms
    GT->>GQ: vyprázdnění fronty příkazů
    GT->>GT: legálnost, pravidla
  end
```

---

## UART: vstup a odpověď zpět

```mermaid
flowchart LR
  SER[Vestavěný USB serial] --> UT[uart_task]
  UT --> GQ[(game_command_queue)]
  GQ --> GT[game_task]
  GT --> URQ[(uart_response_queue)]
  URQ --> UT
  UT --> SER
```

---

## Vedlejší fronty (UART test, matrix příkazy)

UART umí poslat příkaz i na `matrix_command_queue`; test task stejně (když je zapnutý).

![Aux SVG](auxiliary_queues.svg)

---

## Mutexy (kdo s čím počítá)

![Mutex SVG](mutex_map.svg)

```mermaid
flowchart TB
  uart_mutex -.-> uart_task
  matrix_mutex -.-> matrix_task
  game_mutex -.-> game_task
  led_unified_mutex -.-> led_task
  game_task -.->|led_set_pixel_safe atd.| led_unified_mutex
```

---

## Celkový obrázek vstupů

![Topology SVG](system_topology.svg)

---

## CMake komponenty bez vlastního tasku v main.c

| Složka | Realita |
|--------|---------|
| animation_task | knihovna v buildu, samostatný task z main.c ne |
| matter_task | vypnuto |
| promotion_button_task, reset_button_task, screen_saver_task | žádný `xTaskCreate` v současném main.c |

---

## Sekvenční HTML (hodně diagramů najednou)

Soubor `mermaid_diagrams.txt` se přegeneruje do `diagrams_mermaid.html` přes `python3 generate_mermaid_html.py` nebo `./scripts/render_docs.sh`. Dlouhá jednovětevnač je `main_flow_diagram.txt` (hodí se zkopírovat do mermaid.live).

---

*Firmware verze: `CMakeLists.txt` → `PROJECT_VERSION`.*
