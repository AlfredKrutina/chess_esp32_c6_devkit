# CzechMate Hardware V2 — Hall Sensor & STM32 Subsystem

This document provides the technical specification and operational status for the **CzechMate V2 hardware architecture**. V2 is the next-generation hardware platform featuring Hall effect sensors and distributed STM32 co-processors.

---

## 1. System Architecture Overview

The V2 architecture replaces the direct GPIO reed switch matrix of V1 with a modular, distributed sensing subsystem coordinated over an I2C bus:

- **Master Controller:** ESP32-C6-WROOM-1-N8 running FreeRTOS, the primary chess engine, per-square RGB indicators, web server, and BLE stack. Square LEDs use **SK6812MINI** (one-wire timing compatible with WS2812-class drivers; not WS2812 silicon).
- **Board chain:** Four PCBs — **one master** (ESP32-C6, USB-C, CH224A PD sink) plus **three slaves**. Each PCB carries **one STM32C031F6P6** and serves a 16-square quadrant; the ESP32 drives **RESET1–RESET4** (one dedicated NRST per STM32).
- **Co-Processors:** **4× STM32C031F6P6** (32 KiB Flash, 12 KiB SRAM, TSSOP20) on a shared Hall/flashing I²C bus. Each acts as an I²C slave for one segment of the 64-square board.
- **Sensors:** On each PCB, **32× Texas Instruments DRV5055A1QDBZR** linear Hall sensors (**16 squares × 2** differential channels). Four boards → **128 sensors / 64 pairs** board-wide.
- **Analog mux (per PCB):** **2× CD74HC4067M96** 16-channel multiplexers (see §4).
- **Development / Evaluation Target:** `NUCLEO-C031C6` with STM32C031C6 (32 KiB Flash, 12 KiB SRAM, LQFP48) for bench firmware bring-up.

---

## 2. Power Source & CH224A

On the **master PCB only**, a **WCH CH224A** USB Power Delivery sink negotiates
5 V / 9 V / 12 V / 15 V / 20 V from USB-C. It is **not** part of STM32
programming or the Hall sensor I²C network. Its pins are:

- `VHV`, `VBUS`, `CC1`, `CC2`, `DP`, `DM` — connected to the USB-C connector.
- `CFG2/SCL` and `CFG3/SDA` are configuration straps for the CH224A, not an
  I²C bus usable by the ESP32.
- `PG` — power-good output.
- `GND` — common ground.

The CH224A is on the **power path only**; it is not involved in STM32 flashing
and does not share I²C with the Hall/BOOTLOADER bus. Flashing uses the
separate ESP32 → STM32 I²C1 bus described below.

---

## 3. Electrical Specifications & I2C Bus Constraints

All 4 STM32 companion chips share power, clock, and communication lines with the ESP32-C6 host.

### Bus Interconnect

```
ESP32-C6 (Master)                   Up to 4× STM32C031 (Slaves / Segments 0–3)
─────────────────                   ──────────────────────────────────────────
GPIO 10 (I2C SDA) ────────┬──────── PB7 (I2C1_SDA, TSSOP20 pin 1) [Shared]
GPIO 11 (I2C SCL) ────────┼──────── PB6 (I2C1_SCL, TSSOP20 pin 20) [Shared]
GPIO 18 (BOOT0_OUT) ──────┼──────── BOOT0 (TSSOP20 pin 19 / PA14) [Shared]
GND ──────────────────────┴──────── GND [Shared]
3.3V ────────────────────────────── VDD [Shared]

GPIO 1 (NRST_SEG0) ──────────────── NRST (Chip 0 pin 6 / PF2) [Dedicated]
GPIO 2 (NRST_SEG1) ──────────────── NRST (Chip 1 pin 6 / PF2) [Dedicated]
GPIO 3 (NRST_SEG2) ──────────────── NRST (Chip 2 pin 6 / PF2) [Dedicated]
GPIO 4 (NRST_SEG3) ──────────────── NRST (Chip 3 pin 6 / PF2) [Dedicated]
```

> The values above are taken from the `czechmate_CH224A` KiCad schematic.
> The previous bench/devkit profile used GPIO 12/13/14/15; that profile is
> preserved in `sdkconfig.defaults.hall_v2` for the Nucleo test setup.
> Use `sdkconfig.defaults.hall_pcb` for this board.

### Critical Electrical & Flashing Rules

1. **Pull-Up Resistors:** Dedicated 4.7 kΩ hardware pull-up resistors to 3.3 V are required on both SDA and SCL lines. ESP32-C6 internal pull-ups act only as supplements.
2. **Shared Lines:** SDA, SCL, BOOT0, GND, and 3.3 V are common to all 4 STM32 chips.
3. **Dedicated Reset Lines:** Each STM32 chip has a unique, dedicated NRST GPIO controlled by the ESP32-C6.
4. **Flashing Bus Contention Rule:** The ST factory ROM bootloader operates at fixed 7-bit I2C address **`0x63`** (per ST AN2606 / AN4221). Because this address is hard-coded in silicon ROM across all STM32C031 devices, **only one STM32 chip may be released from reset at any given time during flashing**. When programming chip \(N\), chip \(N\)'s NRST is released HIGH with BOOT0 held HIGH, while all other chips must be held firmly in reset (NRST held LOW). Violating this creates multi-slave I2C bus contention on address `0x63`.
5. **Runtime Addresses:** Once flashed and booted into user firmware with BOOT0 held LOW, each segment responds on its unique application I2C address. The square block below is the measured map (magnet walk, 2026-10-17), not the earlier a1-quadrant guess:
   - Segment 0 `0x30`: e5–h8
   - Segment 1 `0x31`: a5–d8
   - Segment 2 `0x32`: a1–d4
   - Segment 3 `0x33`: e1–h4

---

## 4. Sensor Geometry & Multiplexing

- **Dual Hall arrangement:** 2 Hall effect sensors per square forming a differential pair (`sensor 0` and `sensor 1`).
- **Pair spacing:** The center-to-center pitch between the two Hall sensors within one square is precisely **6.267 mm**.
- **Analog front end:** Each STM32C031F6P6 uses two **CD74HC4067M96** 16-channel analog multiplexers. On the PCB they are U6 and U7; the STM32 firmware still calls those roles U37 and U38.
  - U6 / U37: `COM1` to `PA0` (ADC channel 0). Scanned first. Its 16 inputs are local squares H_OUT_1..H_OUT_8.
  - U7 / U38: `COM2` to `PA1` (ADC channel 1). Local squares H_OUT_9..H_OUT_16.
  - Channel select: `PA2` (`S0`) through `PA5` (`S3`), shared by both muxes. Enable `PA6` / `PA7`, active LOW.
- **Pairing:** The two Hall sensors of one square are adjacent inputs on the same mux (`I0`+`I1` = H_OUT_1, `I2`+`I3` = H_OUT_2, …). The STM32 payload already stores that pair as one field. The ESP driver does not reorder the bytes. Field `N` is local square `N+1` (0..7 on U6, 8..15 on U7).

Field order inside each segment, confirmed on the board:

| Field | Seg 0 `0x30` | Seg 1 `0x31` | Seg 2 `0x32` | Seg 3 `0x33` |
| ---: | --- | --- | --- | --- |
| 0 | h8 | a8 | a1 | h1 |
| 1 | g8 | a7 | b1 | h2 |
| 2 | f8 | a6 | c1 | h3 |
| 3 | e8 | a5 | d1 | h4 |
| 4 | h7 | b8 | a2 | g1 |
| 5 | g7 | b7 | b2 | g2 |
| 6 | f7 | b6 | c2 | g3 |
| 7 | e7 | b5 | d2 | g4 |
| 8 | h6 | c8 | a3 | f1 |
| 9 | g6 | c7 | b3 | f2 |
| 10 | f6 | c6 | c3 | f3 |
| 11 | e6 | c5 | d3 | f4 |
| 12 | h5 | d8 | a4 | e1 |
| 13 | g5 | d7 | b4 | e2 |
| 14 | f5 | d6 | c4 | e3 |
| 15 | e5 | d5 | d4 | e4 |

Square index in firmware is `(rank-1)*8 + file` with `a = 0`. The same numbers are `s_hall_square` in `components/squares_task/hall_i2c_squares.c`.

---

## 5. Current Firmware Implementation vs Design Goal

It is critical to distinguish between what the firmware implements today and the long-term hardware design goal:

### Current Behavior (What Runs Today)

1. The STM32 companion firmware (`firmware/stm32_hall_c031/`) performs round-robin ADC sampling across the 16 dual-Hall squares and packages the raw 12-bit ADC results into a 64-byte payload transmitted over I2C.
2. The ESP32 driver (`components/squares_task/hall_i2c_squares.c`) reads this raw payload every 25 ms.
3. The driver evaluates square occupancy by calculating differential magnitude:
   $$\Delta = |r_0 - r_1|$$
   If $\Delta \ge \text{CONFIG\_CHESS\_HALL\_PAIR\_DIFF\_THRESHOLD}$, the square is marked **occupied**; otherwise it is marked **empty**.
4. This differential occupancy calculation outputs a standard 64-bit occupancy bitmask. The bitmask is passed directly to the existing move detection logic in `squares_task.c`, generating the exact same `GAME_CMD_PICKUP` and `GAME_CMD_DROP` events as the V1 reed matrix.
5. In current firmware, **chess piece identity is tracked entirely in software** from the initial board setup and move history.

### Design Goal (Next Version Development)

Magnet layouts in physical piece bases may support automated type/color decoding in firmware, but the **KiCad `czechmate_CH224A` project does not define a piece-encoding lookup table**—only the analog front end and bus wiring. **Piece type and color from Hall energy (POC-style thresholds) is implemented in `hall_i2c_squares.c` on the ESP32**; long-term magnet geometry vs. threshold tuning remains hardware/firmware integration work.

---

## 6. Firmware Locations & Build Commands

### Firmware Source Tree

- **STM32 C031 Source:** `firmware/stm32_hall_c031/`
- **Embedded STM32 Binary:** `embedded/stm32_fw_embedded.bin` (flashed to ESP32 flash partition `stm32_fw` at offset `0x380000`)
- **ESP32 I2C Matrix Driver:** `components/squares_task/hall_i2c_squares.c`
- **ESP32 Bootloader Host:** `components/stm32_i2c_bootloader/`
- **Hall V2 Development Profile:** `sdkconfig.defaults.hall_v2` (Nucleo bench setup)
- **CH224A PCB Production Profile:** `sdkconfig.defaults.hall_pcb` (KiCad `czechmate_CH224A` pins)

### Building STM32 Co-Processor Firmware

```bash
cd firmware/stm32_hall_c031

# Build for NUCLEO-C031C6 development board (32 KiB flash)
make BOARD_NUCLEO_C031=1 FLASH_KB=32

# Build for production STM32C031F6P6 on czechmate_CH224A (32 KiB flash)
make FLASH_KB=32

# Copy binary to embedded image for ESP auto-flashing
make copy-embedded
```

### Building ESP32-C6 Firmware with Hall V2 Profile

```bash
# Separate sdkconfig: defaults do not override keys already in the root sdkconfig.
idf.py -B build-hall-v2 -D SDKCONFIG=sdkconfig.hall_v2 -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build

# Flash ESP32 and monitor
idf.py -B build-hall-v2 -D SDKCONFIG=sdkconfig.hall_v2 -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" -p PORT flash monitor
```

### Building ESP32-C6 Firmware for the CH224A PCB

```bash
# Separate sdkconfig. hall_v2 selects I2C Hall; hall_pcb then sets PCB pins.
idf.py -B build-hall-pcb -D SDKCONFIG=sdkconfig.hall_pcb -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2;sdkconfig.defaults.hall_pcb" build

idf.py -B build-hall-pcb -D SDKCONFIG=sdkconfig.hall_pcb -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2;sdkconfig.defaults.hall_pcb" -p PORT flash monitor
```

> The `sdkconfig.defaults.hall_pcb` fragment matches the `czechmate_CH224A`
> KiCad schematic. If you are using the older bench wiring with GPIO 12/13/14/15
> for NRST, use `sdkconfig.defaults.hall_v2` instead.

---

## 7. Related Documentation

- [Wiring ESP-STM4](../../context/hardware/WIRING_ESP_STM4.md) — Physical wiring diagram, pin conflict details, and boot sequence diagnostics.
- [Hardware Versions Comparison](../../context/hardware/HARDWARE_VERSIONS.md) — Feature and parameter matrix comparing V1 and V2.
- [Troubleshooting](../../context/firmware/TROUBLESHOOTING.md) — I2C NACK, BOOT0, and bus communication diagnostics.
- [STM32 Build Notes](../../firmware/stm32_hall_c031/BUILD.txt) — Makefile targets and toolchain requirements.
