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

### Hardware

| Problem | What to check |
|---------|-----------------|
| **LEDs off** | 5 V supply for the strip, common ground with ESP, data on **GPIO7** to DIN |
| **Matrix sees nothing** | Reed wiring, pull-ups on columns (~10 kΩ), row outputs |
| **Buttons** | 1N4148 diodes on all rows, polarity |

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
- Channel details: [ota_architecture.md](../ota_architecture.md)
