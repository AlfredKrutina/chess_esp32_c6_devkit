# Repository layout

Canonical path inventory in the repo. Entry index: [docs/README.md](../README.md). Quick overview for new readers: [README.md](../../README.md).

**Firmware in repo:** **1.8.0** (prototype **V1**, reed). **V2** = Hall — [HARDWARE_VERSIONS.md](HARDWARE_VERSIONS.md).

---

## Do not move (without a large migration)

| Path | Reason |
|-------|--------|
| `main/`, `components/`, root `CMakeLists.txt`, `sdkconfig*` | ESP-IDF expects project at root |
| `firmware/version.json` | OTA manifest — raw URL in app and CI |
| `embedded/stm32_fw_embedded.bin` | Flash partition `stm32_fw` in `CMakeLists.txt` |
| `sdkconfig.defaults.hall_v2` | Hall V2 + STM32 auto-flash build profile |
| `gh-pages-ready/` | GitHub Pages workflow copies specific files |
| `flutter_czechmate/` | CI, package name, dozens of doc links |

---

## Repository root

```
chess_esp32_c6_devkit/
├── main/                    # ESP-IDF boot, task startup
├── components/              # FreeRTOS modules (see table below)
├── flutter_czechmate/       # Flutter client (BLE / HTTP / WS)
├── docs/                    # Human docs + diagrams
├── scripts/                 # Automation — see scripts/README.md
├── firmware/
│   ├── version.json         # ESP OTA manifest (semver + URL .bin)
│   └── stm32_hall_c031/     # STM32 source (Hall V2)
├── embedded/
│   └── stm32_fw_embedded.bin  # Binary for ESP flash partition
├── gh-pages-ready/          # Static web source (downloads, landing)
├── .github/workflows/       # CI: Pages, diagrams, Flutter release, firmware build, flutter test
├── partitions*.csv          # Flash partition tables
├── Doxyfile                 # Doxygen config
├── generate_docs.sh         # Wrapper → scripts/docs/generate_docs.sh
└── README.md                # Project intro
```

Generated / gitignored: `build/`, `managed_components/`, `docs/doxygen/html/`, `context/`, `.cache/`.

---

## `components/` — groups

| Group | Components | Note |
|---------|------------|----------|
| **Core** | `freertos_chess`, `game_task`, `game_hooks`, `timer_system` | Queues, chess logic, clock |
| **Inputs** | `matrix_task`, `button_task`, `stm32_i2c_bootloader` | Reed V1 / I²C Hall V2 prep |
| **Outputs** | `led_task`, `led_state_manager`, `unified_animation_manager`, `game_led_animations`, `visual_error_system` | WS2812B, animation, errors |
| **Connectivity** | `uart_task`, `uart_commands_extended`, `web_server_task`, `ble_task`, `ha_light_task` | Console, HTTP, BLE, MQTT |
| **Support** | `config_manager`, `test_task` | NVS/config; tests (menuconfig) |
| **Legacy / inactive task** | `animation_task`, `enhanced_castling_system`, `promotion_button_task`, `reset_button_task` | Task not created in `main.c` or not linked |

Simplified file tree:

```
components/
├── freertos_chess/          # Queues, mutexes, LED mapping
├── game_task/               # Chess logic (largest module)
├── matrix_task/             # 8×8 reed scan
├── led_task/                # WS2812B + animation pipeline
├── button_task/             # Buttons (promotion, reset, …)
├── uart_task/               # USB Serial JTAG console
├── web_server_task/         # HTTP, REST; web/chess_app.js, web/piece_assets/
├── ble_task/                # NimBLE GATT
├── ha_light_task/           # MQTT Home Assistant
├── unified_animation_manager/
├── game_led_animations/
├── led_state_manager/
├── visual_error_system/
├── timer_system/
├── config_manager/
├── stm32_i2c_bootloader/
├── uart_commands_extended/
├── game_hooks/
├── test_task/               # Optional (menuconfig)
├── animation_task/          # Legacy — task disabled in main.c
├── enhanced_castling_system/  # Not linked from game_task
├── promotion_button_task/   # Orphan — logic in button_task
└── reset_button_task/       # Orphan — logic in button_task
```

---

## Client and web

| Path | Content |
|-------|--------|
| `flutter_czechmate/lib/` | UI, Riverpod, services (BLE, API, Stockfish) |
| `flutter_czechmate/ios/`, `android/`, `windows/`, … | Platform projects |
| `components/web_server_task/web/chess_app.js` | Web UI source (embed into firmware) |
| `components/web_server_task/web/piece_assets/` | Piece PNGs for HTTP embed |
| `components/web_server_task/tools/` | embed_chess_js.py, process_piece_pngs.py, … |
| `gh-pages-ready/downloads.html` | App download page |
| `gh-pages-ready/app_update.json` | Flutter client version manifest |

---

## Documentation and scripts

| Path | Content |
|-------|--------|
| `docs/diagrams/` | `sources/*.mmd`, SVG, `diagrams_mermaid.html` |
| `docs/reference/` | Longer texts (this file, task communication, …) |
| `docs/flutter/` | Flutter client overview |
| `docs/ota_architecture.md` | OTA channels ESP ↔ app |
| `scripts/docs/` | `generate_docs.sh`, `generate_mermaid_html.py`, PDF |
| `scripts/render_docs.sh` | Regenerate diagrams |

Commands: [docs/README.md](../README.md#typical-commands). Scripts: [scripts/README.md](../../scripts/README.md).

---

## Local folders (gitignore)

| Path | Purpose |
|-------|--------|
| `context/` | AI context, OTA logs, HW wiring notes |
| `docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md` | Personal diagram backlog |
| `private-notes/` | Checklists outside Git |
| `CZECHMATE/` | Xcode project (local only) |
