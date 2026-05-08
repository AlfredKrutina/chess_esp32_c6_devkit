# Jak spolu mluví tasky a hardware — CZECHMATE v2.5.1

**Rozcestník:** [docs/README.md](../README.md).

**Obrázky k tomu:** [docs/diagrams/README.md](../diagrams/README.md). Stejné téma je tady vypsané souvětím jako most k zdrojákům.

Kapacity front a stacků hledám v `components/freertos_chess/include/freertos_chess.h`. Pořadí bootu a tasků v `main/main.c` (`animation_task` se **nevytváří**; BLE přes `ble_task_init()` / NimBLE host task).

## 📋 Přehled typů komunikace

Ve firmware řeším hlavně tyto směry dat:

1. **Hardware → Task** — GPIO skenování, UART read
2. **Task → Queue → Task** — asynchronně přes FreeRTOS fronty
3. **Task → Direct Call → Task** — synchronní thread-safe funkce (mutex uvnitř)
4. **Task → Hardware** — GPIO write, UART write, WS2812B

---

## 🔌 1. HARDWARE → TASK KOMUNIKACE

### 1.1 Matrix Hardware → matrix_task

**Typ:** GPIO skenování (hardware polling)  
**Mechanismus:** Multiplexní skenování 8×8 matice reed switchů  
**Frekvence:** Timer callback s celkovým multiplex cyklem ~**25 ms** (cca 20 ms sken matice + okno pro tlačítka); vlastní smyčka `matrix_task` může běžet s `vTaskDelayUntil(10 ms)` pro příkazy/WDT — viz `matrix_task.c`.

**Hardware (aktuální mapování, konzistentní s README):**
- **ROWS (výstupy):** GPIO 10, 11, 18, 19, 20, 21, 22, 23  
- **COLS (vstupy + pull-up):** GPIO 0, 1, 2, 3, 6, 4, 16, 17  
- **Reed switch:** LOW = figurka přítomna, HIGH = prázdné pole

**Flow:**
```
Matrix Hardware (Reed Switches)
    ↓ [GPIO Multiplex Scan v timer callback ~25ms]
matrix_task::matrix_scan_all()
    ↓ [Debouncing - 3 skeny = 30ms]
matrix_task::matrix_detect_moves()
    ↓ [Detekce změn: 1→0 (lift) nebo 0→1 (place)]
game_command_queue (GAME_CMD_PICKUP/DROP)
```

**Timeout:** 5 sekund pro dokončení tahu (piece_lifted timeout)

---

### 1.2 Button Hardware → button_task

**Typ:** GPIO skenování  
**Mechanismus:** Periodické čtení 4 tlačítek  
**Frekvence:** Každých 5ms (v main loop button_task)

**Hardware:**
- **Buttons:** GPIO sdílené s matrix columns (time-multiplexed)
- **Time-multiplexing:** 20-25ms v 25ms cyklu (matrix scan je 0-20ms)

**Flow:**
```
Button Hardware (4 tlačítka)
    ↓ [GPIO Scan - každých 5ms]
button_task::button_scan_all()
    ↓ [Detekce změn stavu]
button_task::button_process_events()
    ↓ [Detekce: PRESS, RELEASE, LONG_PRESS (>1s), DOUBLE_PRESS (<300ms)]
button_event_queue (button_event_t)
    ↓ [game_task zpracuje v 100ms cyklu]
game_task
```

**Typy eventů:**
- `BUTTON_EVENT_PRESS` - tlačítko stisknuto
- `BUTTON_EVENT_RELEASE` - tlačítko uvolněno
- `BUTTON_EVENT_LONG_PRESS` - dlouhé stisknutí (>1s)
- `BUTTON_EVENT_DOUBLE_PRESS` - dvojité stisknutí (<300ms)

---

### 1.3 UART Hardware ↔ uart_task

**Typ:** Bidirekční komunikace  
**Mechanismus:** USB Serial JTAG (vestavěný)  
**Frekvence:** Non-blocking read/write (každých 1ms v main loop)

**Hardware:**
- **UART:** USB Serial JTAG (integrovaný, žádné externí piny)
- **Baud rate:** 115200
- **Protokol:** ASCII textové příkazy + odpovědi

**Flow (READ - Hardware → Task):**
```
UART Hardware (USB Serial)
    ↓ [uart_read_bytes() - každých 1ms]
uart_task::uart_main_loop()
    ↓ [Parsování příkazu (např. "move e2e4")]
uart_task::uart_process_command()
    ↓ [Vytvoření chess_move_command_t]
game_command_queue (GAME_CMD_MAKE_MOVE)
```

**Flow (WRITE - Task → Hardware):**
```
uart_task
    ↓ [xSemaphoreTake(uart_mutex)]
uart_write_bytes() / printf()
    ↓ [UART výstup]
UART Hardware (USB Serial)
```

**Ochrana:** `uart_mutex` - zabraňuje překrývání výstupu z více tasků

---

## 📨 2. TASK → QUEUE → TASK KOMUNIKACE (Asynchronní)

### 2.1 game_command_queue (24 zpráv)

**Typ:** FreeRTOS Queue (FIFO)  
**Velikost:** `GAME_QUEUE_SIZE` **24** zpráv (`chess_move_command_t`)  
**Timeout při odeslání:** typicky 100ms (`pdMS_TO_TICKS(100)`)  
**Timeout při přijetí:** v `game_process_commands()` často non-blocking `0` — viz `game_task`

**Struktura zprávy:** `chess_move_command_t`
```c
typedef struct {
    uint8_t type;                      // GAME_CMD_* typ
    char from_notation[8];            // Zdroj (např. "e2")
    char to_notation[8];              // Cíl (např. "e4")
    uint8_t player;                    // PLAYER_WHITE/BLACK
    QueueHandle_t response_queue;      // Pro odpovědi
    uint8_t promotion_choice;         // Pro promoci
    // ... timer_data union ...
} chess_move_command_t;
```

#### 2.1.1 matrix_task → game_command_queue → game_task

**Typy příkazů:**
- `GAME_CMD_PICKUP` - figurka zvednuta (piece_lifted)
- `GAME_CMD_DROP` - figurka položena (piece_placed, může obsahovat from/to)

**Flow:**
```
matrix_task::matrix_detect_moves()
    ↓ [Detekce piece_lifted]
matrix_send_pickup_command(square)
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task::game_process_commands() - každých 100ms]
    ↓ [xQueueReceive(game_command_queue, &cmd, 100ms)]
game_task::game_process_pickup_command()
```

**Timeout:** Pokud queue je plná, `xQueueSend` vrátí `pdFALSE` (zpráva se ztratí)

---

#### 2.1.2 button_task → button_event_queue → game_task

**Typ:** FreeRTOS Queue (FIFO)  
**Velikost:** 5 zpráv  
**Struktura:** `button_event_t`

**Flow:**
```
button_task::button_process_events()
    ↓ [Detekce změny stavu]
xQueueSend(button_event_queue, &event, 100ms)
button_event_queue
    ↓ [game_task::game_process_commands() - každých 100ms]
    ↓ [xQueueReceive(button_event_queue, &event, 100ms)]
game_task::game_process_button_event()
```

**Typy eventů:**
- `BUTTON_EVENT_PRESS` - stisknutí tlačítka
- `BUTTON_EVENT_RELEASE` - uvolnění tlačítka
- `BUTTON_EVENT_LONG_PRESS` - dlouhé stisknutí
- `BUTTON_EVENT_DOUBLE_PRESS` - dvojité stisknutí

---

#### 2.1.3 uart_task → game_command_queue → game_task

**Typy příkazů:**
- `GAME_CMD_MAKE_MOVE` - provedení tahu (např. "move e2e4")
- `GAME_CMD_RESET_GAME` - reset hry
- `GAME_CMD_GET_STATUS` - získání stavu hry
- `GAME_CMD_GET_BOARD` - získání pozice
- `GAME_CMD_GET_HISTORY` - získání historie tahu
- ... a další (30+ typů příkazů)

**Flow:**
```
uart_task::uart_process_command("move e2e4")
    ↓ [Parsování příkazu]
chess_move_command_t cmd = {
    .type = GAME_CMD_MAKE_MOVE,
    .from_notation = "e2",
    .to_notation = "e4",
    .response_queue = uart_response_queue  // ← Pro odpověď
}
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task zpracuje příkaz]
game_task::game_process_chess_move()
    ↓ [Vytvoření odpovědi]
uart_response_queue (game_response_t)
```

**Odpověď:** UART příkazy obvykle obsahují `response_queue = uart_response_queue` pro zpětnou vazbu

---

#### 2.1.4 web_server_task → game_command_queue → game_task

**Typy příkazů:**
- `GAME_CMD_MAKE_MOVE` - tah z webu (POST /api/move)
- `GAME_CMD_RESET_GAME` - reset z webu (POST /api/reset)
- ... stejné jako UART

**Flow:**
```
web_server_task::http_post_move_handler()
    ↓ [Parsování JSON: {"from": "e2", "to": "e4"}]
chess_move_command_t cmd = {
    .type = GAME_CMD_MAKE_MOVE,
    .from_notation = "e2",
    .to_notation = "e4",
    .response_queue = NULL  // ← Web nemá response queue (používá JSON)
}
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task zpracuje]
game_task
```

**Poznámka:** Web používá `game_get_status_json()` pro čtení stavu (viz Direct Calls)

---

### 2.2 uart_response_queue (10 zpráv)

**Typ:** FreeRTOS Queue (FIFO)  
**Velikost:** `UART_QUEUE_SIZE` **10** položek (`game_response_t`)  
**Směr:** game_task → uart_task

**Struktura:** `game_response_t`
```c
typedef struct {
    game_response_type_t type;    // GAME_RESPONSE_SUCCESS/ERROR/BOARD/...
    uint8_t command_type;         // Původní typ příkazu
    uint8_t error_code;           // Kód chyby (pokud error)
    char message[64];             // Textová zpráva
    char data[256];               // Data (JSON, board, ...)
    uint32_t timestamp;           // Časová značka
} game_response_t;
```

**Flow:**
```
game_task::game_process_chess_move()
    ↓ [Zpracování tahu]
game_send_response_to_uart(message, is_error, uart_response_queue)
    ↓ [xSemaphoreTake(game_mutex, 1000ms)]  // Ochrana stavu
    ↓ [xQueueSend(uart_response_queue, &response, 100ms)]
uart_response_queue
    ↓ [uart_task::uart_main_loop() - periodicky kontroluje]
    ↓ [xQueueReceive(uart_response_queue, &response, timeout)]
uart_task::uart_display_response()
    ↓ [printf() s uart_mutex]
UART Hardware
```

**Timeouty:**
- `xQueueSend`: 100ms (pokud plná, zpráva se ztratí)
- `game_mutex`: 1000ms (pokud nedostupný, odpověď se nepošle)

---

## 🔗 3. TASK → DIRECT CALL → TASK KOMUNIKACE (Synchronní, Thread-Safe)

### 3.1 game_task → led_task (LED ovládání)

**Funkce:** `led_set_pixel_safe()`, `led_clear_all_safe()`, ...  
**Typ:** Thread-safe funkce s mutex ochranou  
**Ochrana:** `led_unified_mutex` (v `led_task.c`; dříve dokumentováno jako `led_state_mutex`)

**Flow:**
```
game_task::game_execute_move()
    ↓ [Volání thread-safe funkce]
led_set_pixel_safe(led_index, red, green, blue)
    ↓ [xSemaphoreTake(led_unified_mutex, timeout)]  // ← UVNITŘ funkce
    ↓ [led_set_pixel_internal() - přímá změna LED bufferu]
    ↓ [xSemaphoreGive(led_unified_mutex)]
led_task::led_main_loop()  // Běží paralelně
    ↓ [Každých 33ms]
    ↓ [xSemaphoreTake(led_unified_mutex, ...)]
    ↓ [led_privileged_batch_commit() - atomický commit]
    ↓ [led_strip_refresh() - WS2812B protokol]
LED Hardware
```

**Vlastnosti:**
- **Thread-safe:** Mutex je uvnitř funkce, volající task nemusí brát mutex
- **Non-blocking pro game_task:** game_task jen nastaví pixel, LED refresh probíhá v led_task
- **Batch commit:** Změny se commitují atomicky každých 33ms (30 FPS)

**Použití:**
- Zvýraznění polí (validní tahy, šach, mat)
- Error indikace (červená LED pro nevalidní tahy)
- Animace tahů

---

### 3.2 web_server_task → game_task (JSON čtení)

**Funkce:**
- `game_get_status_json()` - JSON stav hry
- `game_get_board_json()` - JSON pozice
- `game_get_history_json()` - JSON historie tahu
- `game_get_captured_json()` - JSON sebraných figurek

**Typ:** Thread-safe funkce s mutex ochranou  
**Ochrana:** `game_mutex` (uvnitř funkce)

**Flow:**
```
web_server_task::http_get_status_handler()
    ↓ [HTTP request: GET /api/status]
game_get_status_json(buffer, size)
    ↓ [xSemaphoreTake(game_mutex, portMAX_DELAY)]  // ← UVNITŘ funkce
    ↓ [Čtení game state: current_player, move_count, board[][], ...]
    ↓ [JSON serializace: {"game_state": "playing", "current_player": "white", ...}]
    ↓ [xSemaphoreGive(game_mutex)]
    ↓ [httpd_resp_send(req, buffer, strlen(buffer))]
HTTP Response (JSON)
```

**Vlastnosti:**
- **Thread-safe:** Mutex je uvnitř funkce
- **Read-only:** Funkce jen čtou stav, nemění ho
- **Timeout:** `game_mutex` s `portMAX_DELAY` (blocking, ale bezpečné)

**Poznámka:** Web server **NE** používá fronty pro čtení stavu - pouze pro příkazy (move, reset)

---

## 🔧 4. MUTEXY (Ochrana sdílených zdrojů)

### 4.1 led_unified_mutex

**Ochrana:** LED buffer (`led_states[]`, pending změny, batch commit)  
**Použití:**
- `led_set_pixel_safe()` - bere mutex uvnitř (s definovaným timeoutem ticků)
- `led_task::led_privileged_batch_commit()` - bere mutex před commitem a refresh

**Kritické sekce (koncept):**
```c
// V led_set_pixel_safe():
xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS);
// ... změna bufferu ...
xSemaphoreGive(led_unified_mutex);

// V led_task:
xSemaphoreTake(led_unified_mutex, ...);
led_privileged_batch_commit();
led_strip_refresh();
xSemaphoreGive(led_unified_mutex);
```

**Timeout:** `portMAX_DELAY` (blocking)

---

### 4.2 game_mutex

**Ochrana:** Game state (`board[][]`, `current_player`, `move_count`, ...)  
**Použití:**
- `game_get_status_json()` - automaticky bere mutex
- `game_send_response_to_uart()` - bere mutex před odesláním odpovědi
- Interní game_task funkce **NEberou** mutex (game_task má exkluzivní přístup)

**Kritické sekce:**
```c
// V game_get_status_json() (volající task):
xSemaphoreTake(game_mutex, portMAX_DELAY);
// Čtení game state
xSemaphoreGive(game_mutex);

// V game_task::game_process_*():
// ❌ NEPOUŽÍVÁME mutex - game_task má exkluzivní přístup k board[][]
```

**Timeout:** `portMAX_DELAY` nebo 1000ms (podle funkce)

---

### 4.3 uart_mutex

**Ochrana:** UART výstup (printf, uart_write_bytes)  
**Použití:** Všechny výstupy na UART (uart_task, game_task, ...)

**Kritické sekce:**
```c
// V uart_task a dalších:
xSemaphoreTake(uart_mutex, portMAX_DELAY);
printf("Text\r\n");  // UART výstup
xSemaphoreGive(uart_mutex);
```

**Timeout:** `portMAX_DELAY` (blocking)

---

### 4.4 matrix_mutex

**Ochrana:** GPIO operace při skenování matice  
**Použití:** `matrix_task::matrix_scan_all()` - ochrana proti konfliktům při time-multiplexingu

**Kritické sekce:**
```c
// V matrix_task:
xSemaphoreTake(matrix_mutex, ...);
matrix_scan_all();  // GPIO operace
xSemaphoreGive(matrix_mutex);
```

**Timeout:** Krátký timeout (není blocking)

---

## 📤 5. TASK → HARDWARE KOMUNIKACE

### 5.1 led_task → LED Hardware (WS2812B)

**Typ:** WS2812B protokol (800kHz, timing-critical)  
**Frekvence:** Každých 33ms (30 FPS)  
**Mechanismus:** RMT (Remote Control) driver nebo bit-banging

**Flow:**
```
led_task::led_main_loop()
    ↓ [Každých 33ms]
led_process_commands()  // Zpracování příkazů z fronty (pokud existuje)
led_update_animation()  // Aktualizace animací
led_update_endgame_wave()  // Endgame efekty
    ↓ [xSemaphoreTake(led_unified_mutex)]
led_privileged_batch_commit()  // Atomický commit všech změn
    ↓ [led_strip_refresh() - JEDEN refresh všech LED]
WS2812B Protokol (800kHz, timing-critical)
    ↓ [Hardware DMA nebo RMT]
64 LED (WS2812B) + 9 Button LEDs
```

**Vlastnosti:**
- **Timing-critical:** WS2812B vyžaduje přesný timing (nesmí být přerušeno)
- **Atomický commit:** Všechny změny se commitují najednou (batch)
- **Priorita led_task:** 7 (nejvyšší) - nesmí být přerušeno

---

### 5.2 uart_task → UART Hardware (USB Serial)

**Typ:** UART write (printf, uart_write_bytes)  
**Ochrana:** `uart_mutex`

**Flow:**
```
uart_task::uart_send_response()
    ↓ [xSemaphoreTake(uart_mutex)]
printf("Response: %s\r\n", response.data)
    ↓ [uart_write_bytes() - systémová funkce]
UART Hardware (USB Serial JTAG)
    ↓ [USB Serial]
Terminal (PC)
```

---

## 📊 Shrnutí - Tabulka komunikace

| Z → Do | Typ | Mechanismus | Timeout | Ochrana |
|--------|-----|-------------|---------|---------|
| Matrix HW → matrix_task | GPIO Scan | Timer multiplex ~25ms + task smyčka | - | matrix_mutex (GPIO) |
| Button HW → button_task | GPIO Scan | Polling každých 5ms | - | - |
| UART HW ↔ uart_task | UART Read/Write | Non-blocking | - | uart_mutex (write) |
| matrix_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| button_task → game_task | Queue | button_event_queue (5) | 100ms | - |
| uart_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| web_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| game_task → uart_task | Queue | uart_response_queue (10) | 100ms | game_mutex |
| game_task → led_task | Direct Call | led_set_pixel_safe() | timeout v LED API | led_unified_mutex (uvnitř) |
| web_task → game_task | Direct Call | game_get_status_json() | portMAX_DELAY | game_mutex (uvnitř) |
| led_task → LED HW | WS2812B | led_strip_refresh() | - | led_unified_mutex |
| uart_task → UART HW | UART Write | printf() / uart_write_bytes() | - | uart_mutex |

---

## ⚠️ Kde mi to občas připálí

### 1. Queue Overflow
- Když je fronta plná, `xQueueSend` vrátí `pdFALSE` → zpráva propadne.
- Držím timeout ~100 ms a kontroluju návratovou hodnotu.
- Nejvíc mě bolí `game_command_queue` (24) a `button_event_queue` (5).

### 2. Mutex Deadlock
- Držet mutex dlouho při vyšších prioritách je recept na problém.
- Radši kratší timeouty a `portMAX_DELAY` jen kde opravdu musím.
- Citlivé jsou `game_mutex`, `led_unified_mutex`.

### 3. Timing kolem LED
- Refresh WS2812 nesmí nestíhat.
- `led_task` má prioritu 7 a mutex chrání refresh.

### 4. Thread-safe funkce
- Co končí na `_safe()`, mutex si bere funkce sama (`led_set_pixel_safe()`, `game_get_status_json()`).
- Volající task před tím mutex neřeší.

---

## 🎯 Jak kreslím diagramy k tomuhle textu

1. **Šipky:**
   - **→** fronta (async)
   - **⇒** přímé volání (sync, thread-safe)
   - **⇄** obousměrně (UART)
   - **─ ─ →** jen čtení (JSON)

2. **Timeouty:** u front „100 ms“, u mutexů klíčové jméno (`led_unified_mutex`).

3. **Kapacity:** `game_command_queue` 24 zpráv, `button_event_queue` 5.

4. **Priority / čas:** LED „P7 timing“, matrix „P6 multiplex ~25 ms“.

---

**Datum:** 2026-04-30  
**Verze:** 2.5.1
