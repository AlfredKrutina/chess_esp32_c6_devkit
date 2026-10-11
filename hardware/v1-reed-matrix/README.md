# CzechMate Hardware V1 — Reed Switch Matrix

This document provides the technical specification and operational description for the **CzechMate V1 prototype board**. This is the reference hardware running firmware version **1.8.0** in this repository.

---

## 1. Sensing Principle and Scope

The V1 board senses piece positions using an **8×8 matrix of mechanical reed switches** (64 switches total) positioned beneath the squares of the chessboard.

- **Sensing capability:** Binary square occupancy only (occupied or empty).
- **Physical actuation:** Permanent neodymium magnets embedded in the base of each chess piece pull the corresponding reed switch closed when placed on a square.
- **Electrical logic:** Active LOW (`0 = piece present / switch closed to row low`, `1 = square empty / pulled HIGH`).
- **Functional limitation:** The V1 hardware **cannot detect piece type, piece identity, or piece color**. The hardware reports only presence or absence on each square. Piece identities and game positions are tracked purely in software by `game_task`, initializing from the standard FIDE opening layout and maintaining board state via validated legal pickup and drop transitions.

---

## 2. GPIO Architecture & Pin Mapping

The 8×8 reed matrix is driven directly from the ESP32-C6 microcontroller using time-multiplexed row and column scanning.

### Pin Assignments (ESP32-C6 DevKit)

| Function | ESP32-C6 GPIO | Direction / Configuration |
|---|---|---|
| **Matrix Rows (8)** | `GPIO 10, 11, 18, 19, 20, 21, 22, 23` | Outputs (driven LOW sequentially during scan) |
| **Matrix Columns (8)** | `GPIO 0, 1, 2, 3, 6, 4, 16, 17` | Inputs with internal pull-up enabled |
| **Addressable LEDs** | `GPIO 7` | RMT / WS2812B data line (73 LEDs total) |
| **Status LED** | `GPIO 5` | Output (dedicated indicator, avoids strapping pins) |
| **Reset Button** | `GPIO 15` | Input with pull-up (separate pin) |
| **Promotion Buttons (4)**| Shared with matrix column lines | Time-multiplexed window within 25 ms cycle |
| **Console UART** | USB Serial JTAG | Native USB CDC / JTAG (no external GPIOs) |

> **Strapping Pin Safety:** GPIO 9 is explicitly avoided for column inputs to prevent unintended boot strapping resets. GPIO 17 (default UART0 RX) is explicitly reconfigured to standard GPIO mode in initialization.

---

## 3. Multiplexing & Scanning Timing

The GPIO scan loop is coordinated by FreeRTOS timers and tasks:

1. **Cycle timing:** The scan cycle executes every ~**25 ms**.
   - `0 ms – 20 ms`: Matrix rows are scanned sequentially (one row driven LOW at a time, 8 column states read per row).
   - `20 ms – 25 ms`: Matrix rows are held HIGH while promotion buttons sharing the column bus are sampled.
2. **Debouncing:** States are filtered across **3 consecutive scan cycles** (~30 ms debounce filter) to prevent contact bounce false-triggers.
3. **Move event dispatch:** State changes are converted to event commands:
   - Piece lifted (`1 → 0` occupancy transition): Dispatches `GAME_CMD_PICKUP` to `game_command_queue`.
   - Piece placed (`0 → 1` occupancy transition): Dispatches `GAME_CMD_DROP` to `game_command_queue`.
4. **Move completion timeout:** A 5-second window is allowed between pickup and drop before an incomplete move timeout or error condition is flagged.

---

## 4. Firmware Implementation

All code required to build and operate the V1 prototype is included in the default firmware build:

- **Build configuration:** `sdkconfig.defaults` (sets `CONFIG_CHESS_MATRIX_INPUT_GPIO_REED=y`).
- **Matrix driver:** `components/matrix_task/matrix_task.c` and `components/matrix_task/include/matrix_task.h`.
- **GPIO definitions & initialization:** `components/freertos_chess/freertos_chess.c` and `components/freertos_chess/include/freertos_chess.h`.
- **LED index mapping:** `components/freertos_chess/led_mapping.c` (converts row/column chessboard coordinates to serpentine WS2812B chain indices).
- **Game logic & state tracking:** `components/game_task/game_task.c` (consumes pickup/drop commands, validates moves, tracks piece identities).

---

## 5. Building and Flashing V1 Firmware

To build the default V1 firmware running on physical prototype boards:

```bash
# Set up ESP-IDF environment (v5.x recommended)
. $IDF_PATH/export.sh

# Build project with default configuration
idf.py build

# Flash to connected ESP32-C6 and open serial monitor
idf.py -p PORT flash monitor
```

---

## 6. Related Documentation

- [Hardware Versions Comparison](../../docs/reference/HARDWARE_VERSIONS.md) — Detailed comparison of V1 prototype vs V2 production target.
- [Task Communication Architecture](../../docs/reference/TASK_COMMUNICATION.md) — FreeRTOS task priorities, queues, and mutex protection details for V1.
- [System Troubleshooting](../../docs/reference/TROUBLESHOOTING.md) — Diagnosing matrix misreads, pull-up issues, and hardware faults.
- [Repository Layout](../../docs/reference/REPO_LAYOUT.md) — Directory organization and module inventories.
