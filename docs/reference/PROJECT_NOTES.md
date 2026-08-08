# Project notes

Longer text from the original root README — lessons learned, challenges, versions, license. Technical index: [docs/README.md](../README.md).

---

## What I learned

### Embedded programming
- **FreeRTOS** — tasks, priorities, scheduling
- **GPIO** — pull-up/pull-down, multiplexing
- **Interrupt handling**, memory (stack vs heap), **watchdog**

### Chess logic
- FIDE rules including en passant, castling, promotion
- Move validation, check/mate, board representation

### Web on MCU
- ESP-IDF HTTP server, WebSocket `/ws` (if `CONFIG_HTTPD_WS_SUPPORT`)
- Embedded JS, REST API for clients

### Architecture
- Modules in `components/`, queues and mutexes between tasks
- Recovery and logging — see [diagrams](../diagrams/README.md)

### Hardware
- Reed matrix, WS2812B timing, pin time-multiplexing, LED power draw

---

## Challenges

### Hardware
- **Reed matrix** — 64 switches, correct pins and contact
- **LED power** — up to ~4.5 A for 73 WS2812B, external 5 V + common ground
- **Physical board** — firmware needs a working sensor matrix to read moves

### Software
- **Multiplex ~25 ms** — matrix and buttons without state collisions
- **Chess logic** — edge cases (en passant…)
- **FreeRTOS** — queues, mutexes, architecture refactors
- **LED animation** — `unified_animation_manager` as central driver
- **Web on MCU** — compact embed JS and HTTP stack
- **Debugging** — multimeter + UART log

---

## Version history

### 1.8.0 — current (firmware + app in repo, 2026)

Semver **`1.8.0`**: `CMakeLists.txt` (`PROJECT_VERSION`), `firmware/version.json`, Doxygen, Flutter `pubspec.yaml` **`1.8.0+3`**.

- Full chess logic; web with real-time updates
- LED animation, unified animation manager, visual error system
- FreeRTOS, GPIO time-multiplexing (V1 reed)
- Matrix V1; Hall/I2C V2 prep (`firmware/stm32_hall_c031/`)
- Bot (Stockfish), training, Flutter client
- Standalone `animation_task` disabled
- MQTT / HA (`ha_light_task`), BLE via NimBLE

Older internal labels “v2.4 / v2.5” are no longer tracked in parallel — everything under **1.8.0**.

---

## Possible future directions

- Persistent game history in flash
- Offline AI (practically via phone/API)
- Statistics, opening book, tablebases
- Voice commands (experimental)

---

## License

Code is **publicly available**, but **not under a classic OSI open-source license**. Hardware design files **are not in the repo** and are not open source. Without **written consent** the code may not be used as the basis for a commercial product or widely distributed derivative. Browsing and study are welcome; exceptions (school, research) by agreement.

---

## Useful links

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/)
- [ESP32-C6 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [FreeRTOS](https://www.freertos.org/Documentation/RTOS_book.html)
- [FIDE Laws of Chess](https://www.fide.com/FIDE/handbook/LawsOfChess.pdf)

---

## Acknowledgments

Teachers, ESP-IDF team, Shawn Hymel (YouTube — ESP-IDF/FreeRTOS), Perplexity AI (brainstorm), ESP32 and open source community.

---

## Closing thoughts

The project combines embedded, RTOS, web on MCU, chess, and hardware in one place. The best reward is when LEDs, matrix, and logic finally align. AI tools act as a turbo for ideas — only what I understand and verify in code goes to production.

Questions and feedback are welcome.
