# CzechMate hardware versions (V1 vs V2)

This text aligns marketing (`gh-pages-ready`), README, and firmware — so it is clear what runs today versus what the next-generation architecture is designed to achieve.

Hardware documentation is split by generation:
- **V1 (reed matrix prototype):** [../../hardware/v1-reed-matrix/README.md](../../hardware/v1-reed-matrix/README.md)
- **V2 (Hall sensor & STM32 subsystem):** [../../hardware/v2-hall-stm32/README.md](../../hardware/v2-hall-stm32/README.md)

| | **V1 (prototype in repo / on YouTube)** | **V2.0 (target commercial product)** |
|---|--------------------------------------|-------------------------------------|
| **Piece detection** | 8×8 **reed switch** matrix — occupied / empty only | **Hall sensors** — currently differential occupancy (`\|r0 − r1\|` vs threshold); automated **piece type & color** decoding from magnet patterns is the target of the new version, not current behavior |
| **Form factor** | Bulkier build, development wiring | More compact board, intended for production |
| **Firmware in this repo** | **`1.8.0`** — what actually plays on the prototype (see `CMakeLists.txt` → `PROJECT_VERSION`) | Same FreeRTOS game engine and client ecosystem; V2 driver (`hall_i2c_matrix.c`) evaluates differential occupancy from raw STM32 ADC data |
| **Video** | [YouTube intro](https://youtu.be/_MS6OP3x6Z4) — **V1** | See product site — renders and mocks show intended **V2** |
| **Pre-order / surveys** | No — reference & development | Yes — interest on the web refers to **V2** |

Technical description of reed matrix multiplexing and tasks for current **V1** is in [TASK_COMMUNICATION.md](TASK_COMMUNICATION.md) and [../../hardware/v1-reed-matrix/README.md](../../hardware/v1-reed-matrix/README.md). The **Hall over I2C** architecture (`hall_i2c_matrix.c`, STM32 co-processors in `firmware/stm32_hall_c031/`) is detailed in [../../hardware/v2-hall-stm32/README.md](../../hardware/v2-hall-stm32/README.md) — active build profile `sdkconfig.defaults.hall_v2`, wiring and bootloader flashing in [WIRING_ESP_STM4.md](WIRING_ESP_STM4.md).

## V2 Hall geometry (PCB)

| Dimension | Value | Notes |
|-----------|-------|--------|
| **Pitch between the 2 Hall sensors on one square** | **6.267 mm** | Center-to-center of the differential pair (sensor 0 / sensor 1 per field) |

Firmware expects two channels per square (`HALL_I2C_SENSORS_PER_FIELD`); occupancy DIFF mode uses `|r0 − r1|` — keep this pair spacing when placing footprints.
