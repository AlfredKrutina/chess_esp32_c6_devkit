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
5. **Runtime Addresses:** Once flashed and booted into user firmware with BOOT0 held LOW, each segment responds on its unique application I2C address:
   - Segment 0 (squares a1–d4): `0x30`
   - Segment 1 (squares e1–h4): `0x31`
   - Segment 2 (squares e5–h8): `0x32`
   - Segment 3 (squares a5–d8): `0x33`

---

## 4. Sensor Geometry & Multiplexing

- **Dual Hall arrangement:** 2 Hall effect sensors per square forming a differential pair (`sensor 0` and `sensor 1`).
- **Pair spacing:** The center-to-center pitch between the two Hall sensors within one square is precisely **6.267 mm**.
- **Analog front end:** Each STM32C031F6P6 uses two **CD74HC4067M96** 16-channel analog multiplexers:
  - Multiplexer U37: `COM1` connects to `PA0` (ADC Channel 0).
  - Multiplexer U38: `COM2` connects to `PA1` (ADC Channel 1).
  - Channel select: Lines `PA2` (`S0`) through `PA5` (`S3`) drive the address pins of both multiplexers simultaneously.
  - Enable: `PA6` (`E1`) and `PA7` (`E2`), active LOW.

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
