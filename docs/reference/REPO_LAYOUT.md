# Rozložení repozitáře

Kanónický inventář cest v repu. Vstupní rozcestník: [docs/README.md](../README.md). Rychlý přehled pro nové čtenáře: [README.md](../../README.md).

**Firmware v repu:** **1.8.0** (prototyp **V1**, reed). **V2** = Hall — [HARDWARE_VERZE.md](HARDWARE_VERZE.md).

---

## Co nesmíš přesunout (bez velké migrace)

| Cesta | Důvod |
|-------|--------|
| `main/`, `components/`, kořenové `CMakeLists.txt`, `sdkconfig*` | ESP-IDF očekává projekt v kořeni |
| `firmware/version.json` | OTA manifest — raw URL v aplikaci a CI |
| `embedded/stm32_fw_embedded.bin` | Flash oddíl `stm32_fw` v `CMakeLists.txt` |
| `gh-pages-ready/` | GitHub Pages workflow kopíruje konkrétní soubory |
| `flutter_czechmate/` | CI, package name, desítky odkazů v docs |

---

## Kořen repozitáře

```
chess_esp32_c6_devkit/
├── main/                    # Boot ESP-IDF, start tasků
├── components/              # FreeRTOS moduly (viz tabulka níže)
├── flutter_czechmate/       # Flutter klient (BLE / HTTP / WS)
├── docs/                    # Dokumentace lidí + diagramy
├── scripts/                 # Automatizace — viz scripts/README.md
├── firmware/
│   ├── version.json         # ESP OTA manifest (semver + URL .bin)
│   └── stm32_hall_c031/     # STM32 zdroj (Hall V2)
├── embedded/
│   └── stm32_fw_embedded.bin  # Binárka pro flash oddíl ESP
├── gh-pages-ready/          # Zdroj statického webu (downloads, landing)
├── .github/workflows/       # CI: Pages, diagramy, Flutter release, firmware build, flutter test
├── partitions*.csv          # Tabulky oddílů flash
├── Doxyfile                 # Doxygen konfigurace
├── generate_docs.sh         # Wrapper → scripts/docs/generate_docs.sh
└── README.md                # Úvod projektu
```

Generované / gitignored: `build/`, `managed_components/`, `docs/doxygen/html/`, `context/`, `.cache/`.

---

## `components/` — skupiny

| Skupina | Komponenty | Poznámka |
|---------|------------|----------|
| **Jádro** | `freertos_chess`, `game_task`, `game_hooks`, `timer_system` | Fronty, šachová logika, čas |
| **Vstupy** | `matrix_task`, `button_task`, `stm32_i2c_bootloader` | Reed V1 / příprava I²C Hall V2 |
| **Výstupy** | `led_task`, `led_state_manager`, `unified_animation_manager`, `game_led_animations`, `visual_error_system` | WS2812B, animace, chyby |
| **Konektivita** | `uart_task`, `uart_commands_extended`, `web_server_task`, `ble_task`, `ha_light_task` | Konzole, HTTP, BLE, MQTT |
| **Podpora** | `config_manager`, `test_task` | NVS/konfigurace; testy (menuconfig) |
| **Legacy / neaktivní task** | `animation_task`, `enhanced_castling_system`, `promotion_button_task`, `reset_button_task` | Task v `main.c` nevytvářen nebo není linkován |

Detailní strom souborů (zjednodušený):

```
components/
├── freertos_chess/          # Fronty, mutexy, mapování LED
├── game_task/               # Šachová logika (největší modul)
├── matrix_task/             # 8×8 reed sken
├── led_task/                # WS2812B + animační pipeline
├── button_task/             # Tlačítka (promoce, reset, …)
├── uart_task/               # USB Serial JTAG konzole
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
├── test_task/               # Volitelný (menuconfig)
├── animation_task/          # Legacy — task vypnutý v main.c
├── enhanced_castling_system/  # Není linkován z game_task
├── promotion_button_task/   # Orphan — logika v button_task
└── reset_button_task/       # Orphan — logika v button_task
```

---

## Klient a web

| Cesta | Obsah |
|-------|--------|
| `flutter_czechmate/lib/` | UI, Riverpod, služby (BLE, API, Stockfish) |
| `flutter_czechmate/ios/`, `android/`, `windows/`, … | Platformní projekty |
| `components/web_server_task/web/chess_app.js` | Zdroj web UI (embed do firmware) |
| `components/web_server_task/web/piece_assets/` | PNG figurek pro HTTP embed |
| `components/web_server_task/tools/` | embed_chess_js.py, process_piece_pngs.py, … |
| `gh-pages-ready/downloads.html` | Stránka stažení aplikace |
| `gh-pages-ready/app_update.json` | Manifest verze Flutter klienta |

---

## Dokumentace a skripty

| Cesta | Obsah |
|-------|--------|
| `docs/diagrams/` | `sources/*.mmd`, SVG, `diagrams_mermaid.html` |
| `docs/reference/` | Delší texty (tento soubor, komunikace tasků, …) |
| `docs/flutter/` | Přehled Flutter klienta |
| `docs/ota_architecture.md` | OTA kanály ESP ↔ aplikace |
| `scripts/docs/` | `generate_docs.sh`, `generate_mermaid_html.py`, PDF |
| `scripts/render_docs.sh` | Přegenerování diagramů |

Příkazy: [docs/README.md](../README.md#typické-příkazy). Skripty: [scripts/README.md](../../scripts/README.md).

---

## Lokální složky (gitignore)

| Cesta | Účel |
|-------|------|
| `context/` | Podklady pro AI, OTA logy, zapojení HW |
| `docs/diagrams/LOCAL_DIAGRAM_BACKLOG.md` | Osobní backlog diagramů |
| `private-notes/` | Checklisty mimo Git |
| `CZECHMATE/` | Xcode projekt (jen lokálně) |
