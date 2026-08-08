# How tasks and hardware communicate — CZECHMATE firmware **1.8.0**

> **Hardware:** This document details **prototype V1** — multiplexed **reed switch** matrix on ESP32-C6. Commercial **V2** uses **Hall sensors** (piece type per square); overview in [HARDWARE_VERSIONS.md](HARDWARE_VERSIONS.md).

**Index:** [docs/README.md](../README.md).

**Diagrams:** [docs/diagrams/README.md](../diagrams/README.md). Same topic here as a bridge to source files.

Queue capacities and stacks are in `components/freertos_chess/include/freertos_chess.h`. Boot and task order in `main/main.c` (`animation_task` is **not** created; BLE via `ble_task_init()` / NimBLE host task).

## 📋 Communication types overview

Firmware handles these data paths:

1. **Hardware → Task** — GPIO scan, UART read
2. **Task → Queue → Task** — async via FreeRTOS queues
3. **Task → Direct Call → Task** — sync thread-safe functions (mutex inside)
4. **Task → Hardware** — GPIO write, UART write, WS2812B

---

## 🔌 1. HARDWARE → TASK COMMUNICATION

### 1.1 Matrix hardware → matrix_task

**Type:** GPIO scan (hardware polling)  
**Mechanism:** Multiplexed scan of 8×8 reed matrix  
**Rate:** Timer callback with full multiplex cycle ~**25 ms** (~20 ms matrix scan + window for buttons); `matrix_task` loop may use `vTaskDelayUntil(10 ms)` for commands/WDT — see `matrix_task.c`.

**Hardware (current mapping, consistent with README):**
- **ROWS (outputs):** GPIO 10, 11, 18, 19, 20, 21, 22, 23  
- **COLS (inputs + pull-up):** GPIO 0, 1, 2, 3, 6, 4, 16, 17  
- **Reed switch:** LOW = piece present, HIGH = empty square

**Flow:**
```
Matrix Hardware (Reed Switches)
    ↓ [GPIO Multiplex Scan in timer callback ~25ms]
matrix_task::matrix_scan_all()
    ↓ [Debouncing - 3 scans = 30ms]
matrix_task::matrix_detect_moves()
    ↓ [Change detect: 1→0 (lift) or 0→1 (place)]
game_command_queue (GAME_CMD_PICKUP/DROP)
```

**Timeout:** 5 seconds to complete a move (piece_lifted timeout)

---

### 1.2 Button hardware → button_task

**Type:** GPIO scan  
**Mechanism:** Periodic read of 4 buttons  
**Rate:** Every 5 ms (in button_task main loop)

**Hardware:**
- **Buttons:** GPIO shared with matrix columns (time-multiplexed)
- **Time-multiplexing:** 20–25 ms in 25 ms cycle (matrix scan is 0–20 ms)

**Flow:**
```
Button Hardware (4 buttons)
    ↓ [GPIO Scan - every 5ms]
button_task::button_scan_all()
    ↓ [State change detect]
button_task::button_process_events()
    ↓ [Detect: PRESS, RELEASE, LONG_PRESS (>1s), DOUBLE_PRESS (<300ms)]
button_event_queue (button_event_t)
    ↓ [game_task processes in 100ms cycle]
game_task
```

**Event types:**
- `BUTTON_EVENT_PRESS` — button pressed
- `BUTTON_EVENT_RELEASE` — button released
- `BUTTON_EVENT_LONG_PRESS` — long press (>1s)
- `BUTTON_EVENT_DOUBLE_PRESS` — double press (<300ms)

---

### 1.3 UART hardware ↔ uart_task

**Type:** Bidirectional  
**Mechanism:** USB Serial JTAG (built-in)  
**Rate:** Non-blocking read/write (every 1 ms in main loop)

**Hardware:**
- **UART:** USB Serial JTAG (integrated, no external pins)
- **Baud rate:** 115200
- **Protocol:** ASCII text commands + responses

**Flow (READ — Hardware → Task):**
```
UART Hardware (USB Serial)
    ↓ [uart_read_bytes() - every 1ms]
uart_task::uart_main_loop()
    ↓ [Parse command (e.g. "move e2e4")]
uart_task::uart_process_command()
    ↓ [Create chess_move_command_t]
game_command_queue (GAME_CMD_MAKE_MOVE)
```

**Flow (WRITE — Task → Hardware):**
```
uart_task
    ↓ [xSemaphoreTake(uart_mutex)]
uart_write_bytes() / printf()
    ↓ [UART output]
UART Hardware (USB Serial)
```

**Protection:** `uart_mutex` — prevents overlapping output from multiple tasks

---

## 📨 2. TASK → QUEUE → TASK (Asynchronous)

### 2.1 game_command_queue (24 messages)

**Type:** FreeRTOS Queue (FIFO)  
**Size:** `GAME_QUEUE_SIZE` **24** messages (`chess_move_command_t`)  
**Send timeout:** typically 100 ms (`pdMS_TO_TICKS(100)`)  
**Receive timeout:** in `game_process_commands()` often non-blocking `0` — see `game_task`

**Message structure:** `chess_move_command_t`
```c
typedef struct {
    uint8_t type;                      // GAME_CMD_* type
    char from_notation[8];            // Source (e.g. "e2")
    char to_notation[8];              // Target (e.g. "e4")
    uint8_t player;                    // PLAYER_WHITE/BLACK
    QueueHandle_t response_queue;      // For responses
    uint8_t promotion_choice;         // For promotion
    // ... timer_data union ...
} chess_move_command_t;
```

#### 2.1.1 matrix_task → game_command_queue → game_task

**Command types:**
- `GAME_CMD_PICKUP` — piece lifted (piece_lifted)
- `GAME_CMD_DROP` — piece placed (piece_placed, may include from/to)

**Flow:**
```
matrix_task::matrix_detect_moves()
    ↓ [Detect piece_lifted]
matrix_send_pickup_command(square)
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task::game_process_commands() - every 100ms]
    ↓ [xQueueReceive(game_command_queue, &cmd, 100ms)]
game_task::game_process_pickup_command()
```

**Timeout:** If queue is full, `xQueueSend` returns `pdFALSE` (message lost)

---

#### 2.1.2 button_task → button_event_queue → game_task

**Type:** FreeRTOS Queue (FIFO)  
**Size:** 5 messages  
**Structure:** `button_event_t`

**Flow:**
```
button_task::button_process_events()
    ↓ [Detect state change]
xQueueSend(button_event_queue, &event, 100ms)
button_event_queue
    ↓ [game_task::game_process_commands() - every 100ms]
    ↓ [xQueueReceive(button_event_queue, &event, 100ms)]
game_task::game_process_button_event()
```

**Event types:**
- `BUTTON_EVENT_PRESS` — press
- `BUTTON_EVENT_RELEASE` — release
- `BUTTON_EVENT_LONG_PRESS` — long press
- `BUTTON_EVENT_DOUBLE_PRESS` — double press

---

#### 2.1.3 uart_task → game_command_queue → game_task

**Command types:**
- `GAME_CMD_MAKE_MOVE` — execute move (e.g. "move e2e4")
- `GAME_CMD_RESET_GAME` — reset game
- `GAME_CMD_GET_STATUS` — get game state
- `GAME_CMD_GET_BOARD` — get position
- `GAME_CMD_GET_HISTORY` — get move history
- ... and more (30+ command types)

**Flow:**
```
uart_task::uart_process_command("move e2e4")
    ↓ [Parse command]
chess_move_command_t cmd = {
    .type = GAME_CMD_MAKE_MOVE,
    .from_notation = "e2",
    .to_notation = "e4",
    .response_queue = uart_response_queue  // ← For reply
}
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task processes command]
game_task::game_process_chess_move()
    ↓ [Create response]
uart_response_queue (game_response_t)
```

**Response:** UART commands usually set `response_queue = uart_response_queue` for feedback

---

#### 2.1.4 web_server_task → game_command_queue → game_task

**Command types:**
- `GAME_CMD_MAKE_MOVE` — move from web (POST /api/move)
- `GAME_CMD_RESET_GAME` — reset from web (POST /api/reset)
- ... same as UART

**Flow:**
```
web_server_task::http_post_move_handler()
    ↓ [Parse JSON: {"from": "e2", "to": "e4"}]
chess_move_command_t cmd = {
    .type = GAME_CMD_MAKE_MOVE,
    .from_notation = "e2",
    .to_notation = "e4",
    .response_queue = NULL  // ← Web has no response queue (uses JSON)
}
    ↓ [xQueueSend(game_command_queue, &cmd, 100ms)]
game_command_queue
    ↓ [game_task processes]
game_task
```

**Note:** Web uses `game_get_status_json()` to read state (see Direct Calls)

---

### 2.2 uart_response_queue (10 messages)

**Type:** FreeRTOS Queue (FIFO)  
**Size:** `UART_QUEUE_SIZE` **10** items (`game_response_t`)  
**Direction:** game_task → uart_task

**Structure:** `game_response_t`
```c
typedef struct {
    game_response_type_t type;    // GAME_RESPONSE_SUCCESS/ERROR/BOARD/...
    uint8_t command_type;         // Original command type
    uint8_t error_code;           // Error code (if error)
    char message[64];             // Text message
    char data[256];               // Data (JSON, board, ...)
    uint32_t timestamp;           // Timestamp
} game_response_t;
```

**Flow:**
```
game_task::game_process_chess_move()
    ↓ [Process move]
game_send_response_to_uart(message, is_error, uart_response_queue)
    ↓ [xSemaphoreTake(game_mutex, 1000ms)]  // Protect state
    ↓ [xQueueSend(uart_response_queue, &response, 100ms)]
uart_response_queue
    ↓ [uart_task::uart_main_loop() - periodic check]
    ↓ [xQueueReceive(uart_response_queue, &response, timeout)]
uart_task::uart_display_response()
    ↓ [printf() with uart_mutex]
UART Hardware
```

**Timeouts:**
- `xQueueSend`: 100 ms (if full, message lost)
- `game_mutex`: 1000 ms (if unavailable, no reply sent)

---

## 🔗 3. TASK → DIRECT CALL → TASK (Synchronous, Thread-Safe)

### 3.1 game_task → led_task (LED control)

**Functions:** `led_set_pixel_safe()`, `led_clear_all_safe()`, ...  
**Type:** Thread-safe functions with mutex  
**Protection:** `led_unified_mutex` (in `led_task.c`; previously documented as `led_state_mutex`)

**Flow:**
```
game_task::game_execute_move()
    ↓ [Call thread-safe function]
led_set_pixel_safe(led_index, red, green, blue)
    ↓ [xSemaphoreTake(led_unified_mutex, timeout)]  // ← INSIDE function
    ↓ [led_set_pixel_internal() - direct LED buffer change]
    ↓ [xSemaphoreGive(led_unified_mutex)]
led_task::led_main_loop()  // Runs in parallel
    ↓ [Every 33ms]
    ↓ [xSemaphoreTake(led_unified_mutex, ...)]
    ↓ [led_privileged_batch_commit() - atomic commit]
    ↓ [led_strip_refresh() - WS2812B protocol]
LED Hardware
```

**Properties:**
- **Thread-safe:** Mutex inside function; caller does not take mutex
- **Non-blocking for game_task:** game_task sets pixel; LED refresh runs in led_task
- **Batch commit:** Changes committed atomically every 33 ms (30 FPS)

**Uses:**
- Square highlights (legal moves, check, mate)
- Error indication (red LED for illegal moves)
- Move animations

---

### 3.2 web_server_task → game_task (JSON read)

**Functions:**
- `game_get_status_json()` — game state JSON
- `game_get_board_json()` — position JSON
- `game_get_history_json()` — move history JSON
- `game_get_captured_json()` — captured pieces JSON

**Type:** Thread-safe functions with mutex  
**Protection:** `game_mutex` (inside function)

**Flow:**
```
web_server_task::http_get_status_handler()
    ↓ [HTTP request: GET /api/status]
game_get_status_json(buffer, size)
    ↓ [xSemaphoreTake(game_mutex, portMAX_DELAY)]  // ← INSIDE function
    ↓ [Read game state: current_player, move_count, board[][], ...]
    ↓ [JSON serialize: {"game_state": "playing", "current_player": "white", ...}]
    ↓ [xSemaphoreGive(game_mutex)]
    ↓ [httpd_resp_send(req, buffer, strlen(buffer))]
HTTP Response (JSON)
```

**Properties:**
- **Thread-safe:** Mutex inside function
- **Read-only:** Functions only read state
- **Timeout:** `game_mutex` with `portMAX_DELAY` (blocking but safe)

**Note:** Web server does **not** use queues to read state — only for commands (move, reset)

---

## 🔧 4. MUTEXES (Shared resource protection)

### 4.1 led_unified_mutex

**Protects:** LED buffer (`led_states[]`, pending changes, batch commit)  
**Used by:**
- `led_set_pixel_safe()` — takes mutex inside (with defined tick timeout)
- `led_task::led_privileged_batch_commit()` — takes mutex before commit and refresh

**Critical sections (concept):**
```c
// In led_set_pixel_safe():
xSemaphoreTake(led_unified_mutex, LED_TASK_MUTEX_TIMEOUT_TICKS);
// ... buffer change ...
xSemaphoreGive(led_unified_mutex);

// In led_task:
xSemaphoreTake(led_unified_mutex, ...);
led_privileged_batch_commit();
led_strip_refresh();
xSemaphoreGive(led_unified_mutex);
```

**Timeout:** `portMAX_DELAY` (blocking)

---

### 4.2 game_mutex

**Protects:** Game state (`board[][]`, `current_player`, `move_count`, ...)  
**Used by:**
- `game_get_status_json()` — takes mutex automatically
- `game_send_response_to_uart()` — takes mutex before sending response
- Internal game_task functions **do not** take mutex (game_task has exclusive access)

**Critical sections:**
```c
// In game_get_status_json() (calling task):
xSemaphoreTake(game_mutex, portMAX_DELAY);
// Read game state
xSemaphoreGive(game_mutex);

// In game_task::game_process_*():
// ❌ NO mutex — game_task has exclusive access to board[][]
```

**Timeout:** `portMAX_DELAY` or 1000 ms (per function)

---

### 4.3 uart_mutex

**Protects:** UART output (printf, uart_write_bytes)  
**Used by:** All UART output (uart_task, game_task, ...)

**Critical sections:**
```c
// In uart_task and others:
xSemaphoreTake(uart_mutex, portMAX_DELAY);
printf("Text\r\n");  // UART output
xSemaphoreGive(uart_mutex);
```

**Timeout:** `portMAX_DELAY` (blocking)

---

### 4.4 matrix_mutex

**Protects:** GPIO operations during matrix scan  
**Used by:** `matrix_task::matrix_scan_all()` — prevents conflicts during time-multiplexing

**Critical sections:**
```c
// In matrix_task:
xSemaphoreTake(matrix_mutex, ...);
matrix_scan_all();  // GPIO operations
xSemaphoreGive(matrix_mutex);
```

**Timeout:** Short timeout (non-blocking)

---

## 📤 5. TASK → HARDWARE COMMUNICATION

### 5.1 led_task → LED hardware (WS2812B)

**Type:** WS2812B protocol (800 kHz, timing-critical)  
**Rate:** Every 33 ms (30 FPS)  
**Mechanism:** RMT driver or bit-banging

**Flow:**
```
led_task::led_main_loop()
    ↓ [Every 33ms]
led_process_commands()  // Process queue commands (if any)
led_update_animation()  // Update animations
led_update_endgame_wave()  // Endgame effects
    ↓ [xSemaphoreTake(led_unified_mutex)]
led_privileged_batch_commit()  // Atomic commit of all changes
    ↓ [led_strip_refresh() - ONE refresh of all LEDs]
WS2812B Protocol (800kHz, timing-critical)
    ↓ [Hardware DMA or RMT]
64 LED (WS2812B) + 9 Button LEDs
```

**Properties:**
- **Timing-critical:** WS2812B requires precise timing (must not be interrupted)
- **Atomic commit:** All changes committed at once (batch)
- **led_task priority:** 7 (highest) — must not be preempted

---

### 5.2 uart_task → UART hardware (USB Serial)

**Type:** UART write (printf, uart_write_bytes)  
**Protection:** `uart_mutex`

**Flow:**
```
uart_task::uart_send_response()
    ↓ [xSemaphoreTake(uart_mutex)]
printf("Response: %s\r\n", response.data)
    ↓ [uart_write_bytes() - system function]
UART Hardware (USB Serial JTAG)
    ↓ [USB Serial]
Terminal (PC)
```

---

## 📊 Summary — communication table

| From → To | Type | Mechanism | Timeout | Protection |
|--------|-----|-------------|---------|---------|
| Matrix HW → matrix_task | GPIO Scan | Timer multiplex ~25ms + task loop | - | matrix_mutex (GPIO) |
| Button HW → button_task | GPIO Scan | Polling every 5ms | - | - |
| UART HW ↔ uart_task | UART Read/Write | Non-blocking | - | uart_mutex (write) |
| matrix_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| button_task → game_task | Queue | button_event_queue (5) | 100ms | - |
| uart_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| web_task → game_task | Queue | game_command_queue (24) | 100ms | - |
| game_task → uart_task | Queue | uart_response_queue (10) | 100ms | game_mutex |
| game_task → led_task | Direct Call | led_set_pixel_safe() | timeout in LED API | led_unified_mutex (inside) |
| web_task → game_task | Direct Call | game_get_status_json() | portMAX_DELAY | game_mutex (inside) |
| led_task → LED HW | WS2812B | led_strip_refresh() | - | led_unified_mutex |
| uart_task → UART HW | UART Write | printf() / uart_write_bytes() | - | uart_mutex |

---

## ⚠️ Where things sometimes break

### 1. Queue overflow
- When the queue is full, `xQueueSend` returns `pdFALSE` → message is dropped.
- I keep timeout ~100 ms and check the return value.
- Worst offenders: `game_command_queue` (24) and `button_event_queue` (5).

### 2. Mutex deadlock
- Holding a mutex too long at higher priority is a recipe for trouble.
- Prefer shorter timeouts; `portMAX_DELAY` only where truly needed.
- Sensitive: `game_mutex`, `led_unified_mutex`.

### 3. LED timing
- WS2812 refresh must not lag.
- `led_task` has priority 7 and mutex protects refresh.

### 4. Thread-safe functions
- Functions ending in `_safe()` take the mutex themselves (`led_set_pixel_safe()`, `game_get_status_json()`).
- Calling task does not handle mutex beforehand.

---

## 🎯 How I draw diagrams for this text

1. **Arrows:**
   - **→** queue (async)
   - **⇒** direct call (sync, thread-safe)
   - **⇄** bidirectional (UART)
   - **─ ─ →** read-only (JSON)

2. **Timeouts:** queues “100 ms”, mutexes key name (`led_unified_mutex`).

3. **Capacities:** `game_command_queue` 24 messages, `button_event_queue` 5.

4. **Priority / timing:** LED “P7 timing”, matrix “P6 multiplex ~25 ms”.

---

**Date:** 2026-04-30  
**Document / firmware version:** 1.8.0
