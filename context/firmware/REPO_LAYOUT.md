# Repository layout

Canonical path inventory in the repo. Entry index: [docs/README.md](../README.md). Quick overview for new readers: [README.md](../../README.md).

**Firmware in repo:** **1.8.0** (prototype **V1**, reed matrix — occupied/empty). **V2** = Hall sensors (currently differential occupancy; piece type/color is the hardware design goal) — see [HARDWARE_VERSIONS.md](HARDWARE_VERSIONS.md), [hardware/v1-reed-matrix/README.md](../../hardware/v1-reed-matrix/README.md), and [hardware/v2-hall-stm32/README.md](../../hardware/v2-hall-stm32/README.md).

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
├── hardware/
│   ├── v1-reed-matrix/      # V1 prototype hardware docs (reed switch matrix)
│   └── v2-hall-stm32/       # V2 production hardware docs (Hall sensors + STM32)
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

Generated / gitignored: `build/`, `managed_components/`, `doxygen/html/`, `context/local/`, `.cache/`.

---

## `components/` — groups

| Group | Components | Note |
|---------|------------|----------|
| **Core** | `freertos_chess`, `game_task`, `game_hooks`, `timer_system` | Queues, chess logic, clock |
| **Inputs** | `matrix_task`, `button_task`, `stm32_i2c_bootloader` | Reed V1 / I²C Hall V2 (differential occupancy; piece-type decoding is not in matrix_task) |
| **Outputs** | `led_task`, `led_state_manager`, `unified_animation_manager`, `game_led_animations`, `visual_error_system` | WS2812B, animation, errors |
| **Connectivity** | `uart_task`, `uart_commands_extended`, `web_server_task`, `ble_task`, `ha_light_task` | Console, HTTP, BLE, MQTT |
| **Support** | `config_manager`, `test_task` | NVS/config; tests (menuconfig) |
| **Legacy / inactive task** | `animation_task`, `enhanced_castling_system`, `promotion_button_task`, `reset_button_task` | Task not created in `main.c` or not linked |

Simplified file tree:

```
components/
├── freertos_chess/          # Queues, mutexes, LED mapping
├── game_task/               # Chess logic (largest module)
├── matrix_task/             # 8×8 reed scan / I2C Hall differential occupancy
├── led_task/                # WS2812B + animation pipeline
├── button_task/             # Buttons (promotion, reset, …)
├── uart_task/               # USB Serial JTAG console
├── web_server_task/         # WiFi + HTTP /api + /ws (browser page is in kos/unused/web_ui)
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
| `kos/unused/web_ui/web/` | Archived browser chess page (JS, piece PNGs). Not linked into firmware. |
| `kos/unused/web_ui/tools/` | Old JS/PNG pack scripts. They do not patch the live HTTP task. |
| `components/web_server_task/tools/mqtt_panel_snippet.txt` | Home Assistant MQTT panel snippet |
| `gh-pages-ready/downloads.html` | App download page |
| `gh-pages-ready/app_update.json` | Flutter client version manifest |

---

## Documentation and scripts

| Path | Content |
|-------|--------|
| `context/diagrams/` | `sources/*.mmd`, SVG, `diagrams_mermaid.html` |
| `context/firmware/`, `context/hardware/`, … | Descriptive Markdown (this file, task communication, …) |
| `docs/flutter/` | Flutter client overview |
| `doxygen/` | Generated C API (gitignored HTML) |
| `dokumentace/` | Word documents (`.doc` / `.docx`) |
| `scripts/docs/` | `generate_docs.sh`, `generate_mermaid_html.py`, PDF |
| `scripts/render_docs.sh` | Regenerate diagrams |

Commands: [docs/README.md](../../docs/README.md). Scripts: [scripts/README.md](../../scripts/README.md).

---

## Local folders (gitignore)

| Path | Purpose |
|-------|--------|
| `context/local/` | Local AI scratch (gitignore) |
| `context/diagrams/LOCAL_DIAGRAM_BACKLOG.md` | Personal diagram backlog |
| `private-notes/` | Checklists outside Git |
| `CZECHMATE/` | Xcode project (local only) |
