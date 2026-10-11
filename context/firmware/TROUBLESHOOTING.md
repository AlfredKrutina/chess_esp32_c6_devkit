# Troubleshooting

Practical steps when debugging firmware, hardware, and the web UI. Intro and build: [README.md](../../README.md). Task architecture: [TASK_COMMUNICATION.md](TASK_COMMUNICATION.md), [diagrams](../diagrams/README.md).

---

## Debugging and tests

### Debug mode

Enable via `menuconfig` or a build macro:

```c
#define CHESS_DEBUG_MODE 1
```

In `menuconfig`:

```
Component config → Chess System → Enable debug mode
```

Logs then show more detail about tasks and memory.

### Test task

With `CONFIG_CHESS_ENABLE_TEST_TASK` enabled in the monitor:

```bash
idf.py -p /dev/ttyUSB0 monitor
# in console: test
```

---

## Known limits

- Doxygen RTF can grow large (~10 MB) — HTML or PDF is easier to read.
- The old puzzle system was removed (the interface stays reasonably extensible).
- After Wi‑Fi drops, a board restart sometimes helps.
- On very long sessions, watch heap / watchdog.
- Reed contacts can degrade over time — purely a hardware issue.

---

## When something fails — first steps

### Hardware (V1 Reed Matrix)

| Problem | What to check |
|---------|-----------------|
| **LEDs off** | 5 V supply for the strip, common ground with ESP, data on **GPIO7** to DIN |
| **Matrix sees nothing** | Reed wiring, pull-ups on columns (~10 kΩ), row outputs (see [hardware/v1-reed-matrix/README.md](../../hardware/v1-reed-matrix/README.md)) |
| **Buttons** | 1N4148 diodes on all rows, polarity |

### Hardware (V2 Hall Sensors & STM32 Subsystem)

Detailed V2 architecture: [hardware/v2-hall-stm32/README.md](../../hardware/v2-hall-stm32/README.md) · Wiring and bootloader details: [WIRING_ESP_STM4.md](WIRING_ESP_STM4.md).

| Problem | What to check |
|---------|-----------------|
| **I2C NACK on segment read** (`0x30`–`0x33`) | Verify bus wiring (SDA GPIO12, SCL GPIO13), 3.3 V power, and presence of **4.7 kΩ hardware pull-ups** to 3.3 V on SDA/SCL. Verify STM32 firmware is running (`hall_probe` log). |
| **Bootloader GET_ID NACK** (`0x63`) | **BOOT0 must be HIGH** when NRST transitions LOW→HIGH. Verify BOOT0 line (GPIO14) is driven HIGH during the reset pulse. |
| **Bootloader bus collision** | **Only ONE STM32 chip may have NRST released at a time** when communicating with the ST ROM bootloader at address `0x63`. All other STM32 chips must be held in reset (NRST held LOW) to avoid multi-slave bus contention. |
| **Virgin STM32 fails auto-flash** | Log `STM32_AUTO: AUTO_USE_BOOT0 … BOOT0_GPIO=-1` indicates missing BOOT0 GPIO definition. In console, use `CLI STM32 PROBE 0 1` and `CLI STM32 FLASH_PART 0 stm32_fw 1` for manual diagnostic flash, followed by `CLI HALL PROBE 0`. |

### Software

| Problem | What to check |
|---------|-----------------|
| **Frozen** | Watchdog should reset — last log line before hang; increase stack of suspect task |
| **Chess “doesn’t make sense”** | Move log, UART `board` command, matrix vs reality |
| **Web not working** | Log: `WiFi initialized …`, `Starting HTTP server` / `HTTP server started`. Board IP (STA or `192.168.4.1` on hotspot), LAN firewall, live `web_server_task`. Older FW with AP off could end on `ESP_ERR_WIFI_MODE` — current code skips AP config in STA-only |

---

## UART — quick reference

```
help     - help
move e2e4 - move
reset    - new game
status   - game state
board    - ASCII board
test     - tests (if test_task enabled)
```

---

## OTA / app

- Firmware manifest: `firmware/version.json` on `main`
- Binary on Pages: `gh-pages/firmware/esp32_chess_v24.bin`
- Channel details: [ota_architecture.md](ota_architecture.md)
