# Architektura tasků — stručný rozcestník

Kompletní přehled (tabulky konstant, obrázky SVG, mutexy, BLE, komponenty) je v **[README.md ve stejné složce](README.md)** — tam drž hlavní dokumentaci.

## Export z `.mmd`

| Soubor | Výstup |
|--------|--------|
| [`sources/tasks_architecture.mmd`](sources/tasks_architecture.mmd) | [`tasks_architecture.svg`](tasks_architecture.svg) · [`tasks_architecture.png`](tasks_architecture.png) |

Příkaz z kořene repa: `./scripts/render_docs.sh` (vygeneruje **všechny** `sources/*.mmd`).

## Klíčové fronty (`freertos_chess`)

| Fronta | Konstanta | Kapacita |
|--------|-----------|----------|
| Herní příkazy | `GAME_QUEUE_SIZE` | 24 |
| Tlačítka | `BUTTON_QUEUE_SIZE` | 5 |
| UART odpovědi | `UART_QUEUE_SIZE` | 10 × `game_response_t` |
| Matrix příkazy | `MATRIX_QUEUE_SIZE` | 8 |

## LED

Mutex **`led_unified_mutex`** v `led_task.c` — batch commit a `led_strip_refresh()`.
