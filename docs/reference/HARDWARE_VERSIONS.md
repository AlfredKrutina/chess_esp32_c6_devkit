# CzechMate hardware versions (V1 vs V2)

This text aligns marketing (`gh-pages-ready`), README, and firmware — so it is clear that the board in the video is not the same as commercial V2.

| | **V1 (prototype in repo / on YouTube)** | **V2.0 (target commercial product)** |
|---|--------------------------------------|-------------------------------------|
| **Piece detection** | 8×8 **reed switch** matrix — occupied / empty only | **Hall sensors** — **piece type** on each square (magnetic patterns), more robust behavior |
| **Form factor** | Bulkier build, development wiring | More compact board, intended for production |
| **Firmware in this repo** | **`1.8.0`** — what actually runs on the prototype (see `CMakeLists.txt` → `PROJECT_VERSION`) | Same software ecosystem (OTA from app, API); Hall / STM32 segment changes per integration |
| **Video** | [YouTube intro](https://youtu.be/_MS6OP3x6Z4) — **V1** | See product site — renders and mocks are **V2** |
| **Pre-order / surveys** | No — reference & development | Yes — interest on the web refers to **V2** |

Technical description of reed matrix multiplexing and tasks for current **V1** is in [TASK_COMMUNICATION.md](TASK_COMMUNICATION.md). The **Hall over I2C** direction (`hall_i2c_matrix.h`, STM32 in `firmware/stm32_hall_c031/`) is preparation / parallel branch for **V2** — active build profile `sdkconfig.defaults.hall_v2`, wiring [WIRING_ESP_STM4.md](WIRING_ESP_STM4.md).

## V2 Hall geometry (PCB)

| Dimension | Value | Notes |
|-----------|-------|--------|
| **Pitch between the 2 Hall sensors on one square** | **6.267 mm** | Center-to-center of the differential pair (sensor 0 / sensor 1 per field) |

Firmware expects two channels per square (`HALL_I2C_SENSORS_PER_FIELD`); occupancy DIFF mode uses `|r0 − r1|` — keep this pair spacing when placing footprints.
