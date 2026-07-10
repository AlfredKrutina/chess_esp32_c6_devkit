# Diagramy — firmware architektura

[Rozcestník všech textů](../README.md).

Boot sekvence, fronty, mutexy, smyčky tasků, šachové pipeline a občas návaznost na Flutter — to všechno je tady graficky. Totéž téma slovy a s poznámkami k HW je v [`reference/KOMUNIKACE_MEZI_TASKY.md`](../reference/KOMUNIKACE_MEZI_TASKY.md). K C API slouží lokálně `./generate_docs.sh` → `docs/doxygen/html/`.

Čísla front a stacků jsou v [`freertos_chess.h`](../../components/freertos_chess/include/freertos_chess.h), pořadí životního cyklu v [`main/main.c`](../../main/main.c).

Nápady na nové grafy si píšu do `LOCAL_DIAGRAM_BACKLOG.md` (gitignore); šablona startu je [DIAGRAM_BACKLOG.local.example.md](DIAGRAM_BACKLOG.local.example.md).

SVG přegeneruju z kořene: `./scripts/render_docs.sh` (zdroje v [`sources/*.mmd`](sources/)).

### Poster — tok algoritmu

Stručný barevný přehled pro tisk / prezentaci: [`poster_algorithm_flow.svg`](poster_algorithm_flow.svg) (zdroj Mermaid: [`sources/poster_algorithm_flow.mmd`](sources/poster_algorithm_flow.mmd)).

**Upřesnění:** zdroj pravdy je [`sources/poster_algorithm_flow.mmd`](sources/poster_algorithm_flow.mmd) — layout **2×2 sloupce** (A|B, fronta, C|D, E) ≈ čtverec; barvy pastel. Tisk: [`poster_algorithm_flow.svg`](poster_algorithm_flow.svg) **800×800**, nebo export Mermaid na A4.

---

## Legenda šipek

- Plná šipka na frontu ≈ `xQueueSend` / `xQueueReceive`.
- Čárkovaná = volitelné (`menuconfig`) nebo nepřímé volání (BLE přes web dispatch).
- `main_system_init()` včetně `ble_task_init()` doběhne **před** `create_system_tasks()`.
- `animation_task` / `matter_task` z `main.c` nevyužívám.

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
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','noteBkgColor':'#334155','noteTextColor':'#f1f5f9','noteBorderColor':'#475569','loopTextColor':'#e2e8f0','labelBoxBkgColor':'#334155','labelTextColor':'#f8fafc'}}}%%
sequenceDiagram
  participant AM as app_main
  participant SYS as main_system_init
  participant FC as chess_system_init
  participant CR as create_system_tasks
  rect rgb(30, 58, 138)
    AM->>SYS: uart_mutex · timery · kontrola front
  end
  rect rgb(120, 53, 15)
    SYS->>FC: fronty · mutexy
    FC-->>SYS: OK
  end
  rect rgb(22, 101, 52)
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
%%{init: {'theme':'dark','themeVariables':{'clusterBkg':'#0f172a','clusterBorder':'#334155','lineColor':'#94a3b8','primaryTextColor':'#f1f5f9','edgeLabelBackground':'#1e293b','titleColor':'#f8fafc'}}}%%
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

  classDef taskN fill:#14532d,stroke:#4ade80,stroke-width:2px,color:#bbf7d0
  classDef queueN fill:#7c2d12,stroke:#fb923c,stroke-width:2px,color:#fed7aa
  classDef gameN fill:#1e3a8a,stroke:#38bdf8,stroke-width:2px,color:#e0f2fe
  classDef bleN fill:#312e81,stroke:#818cf8,stroke-width:2px,color:#e0e7ff
```

![queues_flow.svg](queues_flow.svg)

---

## UART tam a zpět

Konzole a `uart_task` jsou jako ping‑pong přes fronty — příkaz dolů, odpověď nahoru.

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

## Tlačítko → fronta → `game_task`

Sekvenčně: `button_task` jen cpe události a `game_task` je v tick smyčce tahá z fronty.

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','loopTextColor':'#e2e8f0'}}}%%
sequenceDiagram
  rect rgb(22, 101, 52)
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

Stejná idea jako u tlačítek: matrice posílá PICKUP/DROP a `game_task` si frontu sama vybírá.

```mermaid
%%{init: {'theme':'dark','themeVariables':{'actorBkg':'#1e293b','actorBorder':'#38bdf8','actorTextColor':'#f1f5f9','signalColor':'#cbd5e1','loopTextColor':'#e2e8f0'}}}%%
sequenceDiagram
  rect rgb(30, 58, 138)
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

Batchování přes `led_task` je samostatná „trubka“ — detail je ve SVG, zdroj níže.

![led_pipeline.svg](led_pipeline.svg)  
Zdroj: [`sources/led_pipeline.mmd`](sources/led_pipeline.mmd)

---

## Vedlejší fronty

![auxiliary_queues.svg](auxiliary_queues.svg)

---

## Mutexy

Kdo drží který mutex a kdo na něj sahá — statický obrázek plus zjednodušený flow níže.

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

## Topologie vstupů

Kde se fyzické věci napojují na tasky — jeden přehledový graf.

![system_topology.svg](system_topology.svg)

---

## Flutter vrstvy (stejný export co `render_docs`)

Stejný export jako po `./scripts/render_docs.sh`; zdroj je [`sources/client_app_layers.mmd`](sources/client_app_layers.mmd) a stejný obrázek odkazuju i z [`docs/flutter/README.md`](../flutter/README.md).

![client_app_layers.svg](client_app_layers.svg)

---

## Aplikace a klienti

Přehled celého „landscape“ — firmware vs mobil vs případný nativní klient.

| Diagram | Obsah |
|---------|--------|
| [`applications_landscape.mmd`](sources/applications_landscape.mmd) | Firmware vs `flutter_czechmate` vs volitelný nativní Xcode klient |

![applications_landscape.svg](applications_landscape.svg)

---

## Flutter — mapa `lib/` (features + core)

Zjednodušená mapa adresáře — hlavně kvůli orientaci, kde co hledám.

| Diagram | Obsah |
|---------|--------|
| [`flutter_app_structure.mmd`](sources/flutter_app_structure.mmd) | Obrazovky vs služby vs modely (zjednodušení adresáře `flutter_czechmate/lib/`) |

![flutter_app_structure.svg](flutter_app_structure.svg)  
Detail vrstev UI→Riverpod→služby: [`docs/flutter/README.md`](../flutter/README.md).

---

## Taskové smyčky — jeden diagram na task z `main.c`

Každý aktivní task z `main.c` má vlastní smyčkový diagram v `sources/`; implementace běží v komponentách (`*_task_start`). BLE nemá vlastní `xTaskCreate` — host task vzniká z `ble_task_init`.

**web_server_task:** `wifi_init_apsta()` nastaví **AP+STA** jen když je v NVS zapnutý uživatelský hotspot desky; jinak **STA-only** (bez konfigurace AP rozhraní). Poté následuje prodleva, volitelný `wifi_connect_sta()` z NVS a start HTTP serveru — viz [`task_web_loop.mmd`](sources/task_web_loop.mmd) a `web_server_task.c`.

| Task | Soubor zdroje | Hlavní soubor kódu |
|------|---------------|-------------------|
| **game_task** | [`task_game_loop.mmd`](sources/task_game_loop.mmd) | `components/game_task/game_task.c` |
| **led_task** | [`task_led_loop.mmd`](sources/task_led_loop.mmd) | `components/led_task/led_task.c` |
| **matrix_task** | [`task_matrix_loop.mmd`](sources/task_matrix_loop.mmd) | `components/matrix_task/matrix_task.c` |
| **button_task** | [`task_button_loop.mmd`](sources/task_button_loop.mmd) | `components/button_task/button_task.c` |
| **uart_task** | [`task_uart_loop.mmd`](sources/task_uart_loop.mmd) | `components/uart_task/uart_task.c` |
| **web_server_task** | [`task_web_loop.mmd`](sources/task_web_loop.mmd) | `components/web_server_task/web_server_task.c` |
| **ha_light_task** | [`task_ha_light_loop.mmd`](sources/task_ha_light_loop.mmd) | `components/ha_light_task/ha_light_task.c` |
| **test_task** (menuconfig) | [`task_test_optional.mmd`](sources/task_test_optional.mmd) | `components/test_task/test_task.c` |
| **NimBLE / BLE** | [`task_ble_stack.mmd`](sources/task_ble_stack.mmd) | `components/ble_task/ble_nimble_impl.c` + dispatch ve web vrstvě |

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

## Šachová logika — validace, generování tahů, příkazy

Většina je v `components/game_task/game_task.c` (jeden velký modul). Grafy jsou zjednodušené — přesné větve `switch` a hraniční případy řeším v kódu.

| Téma | Diagram |
|------|---------|
| Kontrola tahu před exekucí | [`chess_validation_pipeline.mmd`](sources/chess_validation_pipeline.mmd) → `game_is_valid_move` |
| Pravidla podle figurky | [`chess_piece_validators.mmd`](sources/chess_piece_validators.mmd) → `game_validate_*_enhanced` |
| Legální tahy do bufferu | [`chess_legal_moves_generation.mmd`](sources/chess_legal_moves_generation.mmd) → `game_generate_*` + `game_simulate_move_check` |
| Provedení tahu | [`chess_execute_move_pipeline.mmd`](sources/chess_execute_move_pipeline.mmd) → `game_execute_move` / `game_execute_move_enhanced` |
| Fronta příkazů ke hře | [`game_command_dispatch_overview.mmd`](sources/game_command_dispatch_overview.mmd) → `game_process_commands` |

![chess_validation_pipeline.svg](chess_validation_pipeline.svg)
![chess_piece_validators.svg](chess_piece_validators.svg)
![chess_legal_moves_generation.svg](chess_legal_moves_generation.svg)
![chess_execute_move_pipeline.svg](chess_execute_move_pipeline.svg)
![game_command_dispatch_overview.svg](game_command_dispatch_overview.svg)

---

## Speciální tahy a fyzická deska (`game_task.c`)

Rošáda, promoce, en passant atd. — každý má vlastní flow; tabulka níže je můj index na `.mmd` a funkce.

| Téma | Diagram |
|------|---------|
| **Rošáda** — král první, věž dodělá ručně | [`chess_flow_castling.mmd`](sources/chess_flow_castling.mmd) · `castling_state`, `game_validate_castling` |
| **Promoce** — čekání na Q/R/B/N | [`chess_flow_promotion.mmd`](sources/chess_flow_promotion.mmd) · `promotion_state`, `game_process_promotion_command` |
| **En passant** — dvojtah pěšce + útok | [`chess_flow_en_passant.mmd`](sources/chess_flow_en_passant.mmd) · `game_is_en_passant_possible`, `MOVE_TYPE_EN_PASSANT` |
| **Braní** — `capture_in_progress` | [`chess_flow_guided_capture.mmd`](sources/chess_flow_guided_capture.mmd) · `game_process_drop_command` |
| **Boot / NVS** — snapshot vs nová hra | [`chess_flow_boot_nvs.mmd`](sources/chess_flow_boot_nvs.mmd) · `game_task_start`, `game_load_snapshot_from_nvs` |

![chess_flow_castling.svg](chess_flow_castling.svg)
![chess_flow_promotion.svg](chess_flow_promotion.svg)
![chess_flow_en_passant.svg](chess_flow_en_passant.svg)
![chess_flow_guided_capture.svg](chess_flow_guided_capture.svg)
![chess_flow_boot_nvs.svg](chess_flow_boot_nvs.svg)

---

## Matrix guard, recovery, rezignace, undo

Behaviour kolem rozporů senzorů, návratu figurek, rezignace králem a undo — opět jedna tabulka jako rozcestník.

| Téma | Diagram |
|------|---------|
| **Matrix guard** — rozpor senzor vs `board[]` | [`chess_flow_matrix_guard.mmd`](sources/chess_flow_matrix_guard.mmd) · [MATRIX_GUARD.md](../reference/MATRIX_GUARD.md) |
| **Chybný pickup** — vrácení soupeřovy figury | [`chess_flow_error_recovery.mmd`](sources/chess_flow_error_recovery.mmd) · `GAME_STATE_WAITING_FOR_RETURN`, `error_recovery_state` |
| **Rezignace** — zvednutí krále 10 s | [`chess_flow_resignation.mmd`](sources/chess_flow_resignation.mmd) · `resignation_start` / `resignation_tick` / `resignation_finalize_timeout` |
| **Undo** | [`chess_flow_undo.mmd`](sources/chess_flow_undo.mmd) · `game_undo_last_move_impl` |

![chess_flow_matrix_guard.svg](chess_flow_matrix_guard.svg)
![chess_flow_error_recovery.svg](chess_flow_error_recovery.svg)
![chess_flow_resignation.svg](chess_flow_resignation.svg)
![chess_flow_undo.svg](chess_flow_undo.svg)

---

## CMake komponenty bez tasku z `main.c`

Komponenty, které se do image dostanou, ale aktuálně nemají vlastní task z `main.c` — pořád na ně sahá CMake / závislosti.

| Složka | Poznámka |
|--------|----------|
| animation_task | build ano, `xTaskCreate` ne |
| matter_task | vypnuto |
| promotion_button_task, reset_button_task, screen_saver_task | žádný task v současném `main.c` |

---

## Sekvenční HTML

Dlouhý „scroll“ všech diagramů vzniká z `mermaid_diagrams.txt` → `diagrams_mermaid.html` přes `generate_mermaid_html.py` nebo po `./scripts/render_docs.sh`. Samostatný dlouhý tok je ještě `main_flow_diagram.txt`.

**`mermaid_diagrams.txt` není jeden diagram.** Obsahuje komentáře `#` a 26 bloků `sequenceDiagram`. Když ho vložíte celý do [mermaid.live](https://mermaid.live/) nebo otevřete v náhledu Mermaid v editoru, dostanete `UnknownDiagramError: No diagram type detected`.

| Potřebujete | Postup |
|-------------|--------|
| Všechny diagramy v prohlížeči | `./scripts/render_docs.sh` → otevřít `diagrams_mermaid.html` |
| Jeden diagram (A1, B1, …) | `python3 scripts/docs/generate_mermaid_html.py --extract A1` nebo po renderu soubor `extracted/a1_*.mmd` |
| Seznam ID | `python3 scripts/docs/generate_mermaid_html.py --list` |
| mermaid.live | Zkopírovat obsah jednoho `extracted/*.mmd` (začíná `sequenceDiagram`) |

---

*Firmware verze: `CMakeLists.txt` → `PROJECT_VERSION`.*
