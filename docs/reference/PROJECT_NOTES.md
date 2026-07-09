# Poznámky k projektu

Delší texty z původního kořenového README — učení, výzvy, verze, autoři, licence. Technický rozcestník: [docs/README.md](../README.md).

---

## Co jsme se naučili

### Embedded programování
- **FreeRTOS** — tasky, priority, plánování
- **GPIO** — pull-up/pull-down, multiplexing
- **Interrupt handling**, paměť (stack vs heap), **watchdog**

### Šachová logika
- Pravidla FIDE včetně en passant, rošády, promoce
- Validace tahů, šach/mat, reprezentace desky

### Web na MCU
- HTTP server ESP-IDF, WebSocket `/ws` (pokud `CONFIG_HTTPD_WS_SUPPORT`)
- Embedded JS, REST API pro klienty

### Architektura
- Moduly v `components/`, fronty a mutexy mezi tasky
- Recovery a logování — viz [diagramy](../diagrams/README.md)

### Hardware
- Reed matice, WS2812B timing, time-multiplexing pinů, spotřeba LED

---

## Výzvy

### Hardware
- **Reed matice** — 64 spínačů, správné piny a kontakt
- **Napájení LED** — až ~4,5 A u 73 WS2812B, externí 5 V + společná zem
- **Fyzická deska** — bez Matějovy práce nemá firmware co číst

### Software
- **Multiplex ~25 ms** — střídání matice a tlačítek bez kolizí stavů
- **Šachová logika** — hraniční případy (en passant…)
- **FreeRTOS** — fronty, mutexy, přestavby architektury
- **LED animace** — `unified_animation_manager` jako centrální řidič
- **Web na MCU** — úsporný embed JS a HTTP stack
- **Debugging** — multimetr + UART log

---

## Historie verzí

### 1.8.0 — aktuální (firmware + aplikace v repu, 2026)

Semver **`1.8.0`**: `CMakeLists.txt` (`PROJECT_VERSION`), `firmware/version.json`, Doxygen, Flutter `pubspec.yaml` **`1.8.0+3`**.

- Kompletní šachová logika; web s real-time aktualizací
- LED animace, unified animation manager, visual error system
- FreeRTOS, time-multiplexing GPIO (V1 reed)
- Matrix V1; příprava Hall/I2C V2 (`firmware/stm32_hall_c031/`)
- Bot (Stockfish), výuka, Flutter klient
- Vypnutý samostatný `animation_task`
- MQTT / HA (`ha_light_task`), BLE přes NimBLE

Starší interní označení „v2.4 / v2.5“ už nesledujeme paralelně — vše pod **1.8.0**.

---

## Možné další směry

- Trvalejší historie her ve flash
- Offline AI (prakticky přes telefon/API)
- Statistiky, opening book, tablebases
- Hlasové příkazy (experimentální)

---

## Autoři

### Alfred Krutina — software & firmware

FreeRTOS, šachová logika, web server, dokumentace, flow zařízení.

### Matěj Jager — hardware

**V1:** Reed matice, LED, multiplexing. **V2:** Hall, kompaktnější deska. Testování na fyzické desce.

---

## Licence

Kód je **veřejně dostupný**, ale **není klasická OSI open-source licence**. Hardware podklady **nejsou v repu** a nejsou open source. Bez **písemného souhlasu** nelze kód použít jako základ komerčního produktu nebo široce šířeného odvozeného díla. Prohlížení a studium vítáme; výjimky (škola, výzkum) po domluvě.

---

## Užitečné odkazy

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/)
- [ESP32-C6 datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [FreeRTOS](https://www.freertos.org/Documentation/RTOS_book.html)
- [FIDE Laws of Chess](https://www.fide.com/FIDE/handbook/LawsOfChess.pdf)

---

## Poděkování

Učitelé, ESP-IDF tým, Shawn Hymel (YouTube — ESP-IDF/FreeRTOS), Perplexity AI (brainstorm), komunita ESP32 a open source.

---

## Závěrečné myšlenky

Projekt spojuje embedded, RTOS, web na MCU, šachy a hardware v jednom. Největší odměna je moment, kdy LED, matice a logika sedí dohromady. Spolupráce s Matějem: FW ↔ dráty. AI nástroje jako turbo na nápady — do produkce jen to, co rozumím a ověřím v kódu.

Otázky a postřehy vítáme.
