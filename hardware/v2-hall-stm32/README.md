# CzechMate Hardware V2 — Hall Sensor & STM32 Subsystem

This document provides the technical specification and operational status for the **CzechMate V2 hardware architecture**. V2 is the next-generation hardware platform featuring Hall effect sensors and distributed STM32 co-processors.

---

## 1. System Architecture Overview

The V2 architecture replaces the direct GPIO reed switch matrix of V1 with a modular, distributed sensing subsystem coordinated over an I2C bus:

- **Master Controller:** ESP32-C6 DevKit running FreeRTOS, the primary chess engine, LED drivers, web server, and BLE stack.
- **Co-Processors:** Up to **4× STM32C031 microcontrollers** on a single shared I2C bus. Each STM32 acts as an I2C slave handling one 16-square quadrant (segment) of the 64-square chessboard.
- **Sensors:** 128 analog Hall effect sensors arranged as **64 differential pairs** (2 sensors per square).
- **Target Boards & Parts:**
  - **Development / Evaluation Target:** `NUCLEO-C031C6` board with STM32C031C6 (32 KiB Flash, 12 KiB SRAM, LQFP48).
  - **Production Module Target:** STM32C031F4 (16 KiB Flash, 12 KiB SRAM, TSSOP20 package).
  - Note: In STM32 naming conventions, `F4` denotes TSSOP20 package with 16 KiB flash; it is a member of the mainstream Arm Cortex-M0+ STM32C0 family, not an STM32F4-series device.

---

## 2. Electrical Specifications & I2C Bus Constraints

All 4 STM32 companion chips share power, clock, and communication lines with the ESP32-C6 host.

### Bus Interconnect

```
ESP32-C6 (Master)                   Up to 4× STM32C031 (Slaves / Segments 0–3)
─────────────────                   ──────────────────────────────────────────
GPIO 12 (I2C SDA) ────────┬──────── PB7 (I2C1_SDA, TSSOP20 pin 1) [Shared]
GPIO 13 (I2C SCL) ────────┼──────── PB6 (I2C1_SCL, TSSOP20 pin 20) [Shared]
GPIO 14 (BOOT0_OUT) ──────┼──────── BOOT0 (TSSOP20 pin 9) [Shared]
GND ──────────────────────┴──────── GND [Shared]
3.3V ────────────────────────────── VDD [Shared]

GPIO 15 (NRST_SEG0) ─────────────── NRST (Chip 0 pin 4) [Dedicated]
GPIO xx (NRST_SEG1) ─────────────── NRST (Chip 1 pin 4) [Dedicated]
GPIO yy (NRST_SEG2) ─────────────── NRST (Chip 2 pin 4) [Dedicated]
GPIO zz (NRST_SEG3) ─────────────── NRST (Chip 3 pin 4) [Dedicated]
```

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

## 3. Sensor Geometry & Multiplexing

- **Dual Hall arrangement:** 2 Hall effect sensors per square forming a differential pair (`sensor 0` and `sensor 1`).
- **Pair spacing:** The center-to-center pitch between the two Hall sensors within one square is precisely **6.267 mm**.
- **Analog front end:** Each STM32C031 uses two CD74HC4067 16-channel analog multiplexers:
  - Multiplexer U37: `COM1` connects to `PA0` (ADC Channel 0).
  - Multiplexer U38: `COM2` connects to `PA1` (ADC Channel 1).
  - Channel select: Lines `PA2` (`S0`) through `PA5` (`S3`) drive the address pins of both multiplexers simultaneously.
  - Enable: `PA6` (`E1`) and `PA7` (`E2`), active LOW.

---

## 4. Current Firmware Implementation vs Design Goal

It is critical to distinguish between what the firmware implements today and the long-term hardware design goal:

### Current Behavior (What Runs Today)

1. The STM32 companion firmware (`firmware/stm32_hall_c031/`) performs round-robin ADC sampling across the 16 dual-Hall squares and packages the raw 12-bit ADC results into a 64-byte payload transmitted over I2C.
2. The ESP32 driver (`components/matrix_task/hall_i2c_matrix.c`) reads this raw payload every 25 ms.
3. The driver evaluates square occupancy by calculating differential magnitude:
   $$\Delta = |r_0 - r_1|$$
   If $\Delta \ge \text{CONFIG\_CHESS\_HALL\_PAIR\_DIFF\_THRESHOLD}$, the square is marked **occupied**; otherwise it is marked **empty**.
4. This differential occupancy calculation outputs a standard 64-bit occupancy bitmask. The bitmask is passed directly to the existing move detection logic in `matrix_task.c`, generating the exact same `GAME_CMD_PICKUP` and `GAME_CMD_DROP` events as the V1 reed matrix.
5. In current firmware, **chess piece identity is tracked entirely in software** from the initial board setup and move history.

### Design Goal (Next Version Development)

The physical magnet patterns in the piece bases (differing polarities and field intensities across the dual sensors) are engineered to support automated piece type and color decoding. **Piece type and color classification from raw ADC vectors is NOT yet implemented in C firmware**. It represents the future feature milestone for the V2 platform.

---

## 5. Firmware Locations & Build Commands

### Firmware Source Tree

- **STM32 C031 Source:** `firmware/stm32_hall_c031/`
- **Embedded STM32 Binary:** `embedded/stm32_fw_embedded.bin` (flashed to ESP32 flash partition `stm32_fw` at offset `0x380000`)
- **ESP32 I2C Matrix Driver:** `components/matrix_task/hall_i2c_matrix.c`
- **ESP32 Bootloader Host:** `components/stm32_i2c_bootloader/`
- **Build Profile:** `sdkconfig.defaults.hall_v2`

### Building STM32 Co-Processor Firmware

```bash
cd firmware/stm32_hall_c031

# Build for NUCLEO-C031C6 development board (32 KiB flash)
make BOARD_NUCLEO_C031=1 FLASH_KB=32

# Build for production STM32C031F4 (16 KiB flash)
make FLASH_KB=16

# Copy binary to embedded image for ESP auto-flashing
make copy-embedded
```

### Building ESP32-C6 Firmware with Hall V2 Profile

```bash
# Set SDKCONFIG_DEFAULTS to include both base defaults and the hall_v2 overlay
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" build

# Flash ESP32 and monitor
idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.hall_v2" -p PORT flash monitor
```

---

## 6. Related Documentation

- [Wiring ESP-STM4](../../docs/reference/WIRING_ESP_STM4.md) — Physical wiring diagram, pin conflict details, and boot sequence diagnostics.
- [Hardware Versions Comparison](../../docs/reference/HARDWARE_VERSIONS.md) — Feature and parameter matrix comparing V1 and V2.
- [Troubleshooting](../../docs/reference/TROUBLESHOOTING.md) — I2C NACK, BOOT0, and bus communication diagnostics.
- [STM32 Build Notes](../../firmware/stm32_hall_c031/BUILD.txt) — Makefile targets and toolchain requirements.
