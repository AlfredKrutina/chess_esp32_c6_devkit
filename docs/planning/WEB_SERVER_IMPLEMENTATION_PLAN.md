# 🎯 Web Server Implementation Plan - ESP32-C6 Chess System

## 📊 Projekt Analýza - Hotovo ✅

### Architektura systému
```
ESP32-C6 Chess System v2.4
├── FreeRTOS Multi-Task System
│   ├── LED Task (8KB, prio 7) - WS2812B LED control
│   ├── Matrix Task (3KB, prio 6) - 8x8 Reed switch scanning
│   ├── Button Task (3KB, prio 5) - Button events
│   ├── UART Task (6KB, prio 3) - Console commands
│   ├── Game Task (10KB, prio 4) - Chess engine logic
│   ├── Animation Task (2KB, prio 3) - LED animations
│   ├── Web Server Task (6KB, prio 3) - ⚠️ PLACEHOLDER (to be implemented)
│   └── Matter Task (8KB, prio 4) - IoT protocol
│
├── Hardware
│   ├── 73x WS2812B LEDs (64 board + 9 buttons)
│   ├── 8x8 Reed Switch Matrix (piece detection)
│   ├── 9x Buttons (time-multiplexed with matrix)
│   └── USB Serial JTAG (console)
│
└── Communication
    ├── FreeRTOS Queues (inter-task communication)
    ├── Mutexes (thread-safe access)
    └── Timers (periodic tasks)
```

### Board State Representation
```c
// game_task.c - line 99
static piece_t board[8][8] = {0};

// Piece types (chess_types.h)
PIECE_EMPTY = 0
PIECE_WHITE_PAWN = 1    PIECE_BLACK_PAWN = 7
PIECE_WHITE_KNIGHT = 2  PIECE_BLACK_KNIGHT = 8
PIECE_WHITE_BISHOP = 3  PIECE_BLACK_BISHOP = 9
PIECE_WHITE_ROOK = 4    PIECE_BLACK_ROOK = 10
PIECE_WHITE_QUEEN = 5   PIECE_BLACK_QUEEN = 11
PIECE_WHITE_KING = 6    PIECE_BLACK_KING = 12
```

### Lifted Piece Detection
```c
// game_task.c - lines 103-106
static bool piece_lifted = false;
static uint8_t lifted_piece_row = 0;
static uint8_t lifted_piece_col = 0;
static piece_t lifted_piece = PIECE_EMPTY;

// Function to handle piece lifted event
void game_handle_piece_lifted(uint8_t row, uint8_t col);
```

### Game State
```c
// game_task.c - lines 74-76
static game_state_t current_game_state = GAME_STATE_IDLE;
static player_t current_player = PLAYER_WHITE;
static uint32_t move_count = 0;

// Game states
GAME_STATE_IDLE = 0
GAME_STATE_ACTIVE = 2
GAME_STATE_PAUSED = 3
GAME_STATE_FINISHED = 4
GAME_STATE_PROMOTION = 7
```

### Current Web Server Status
**web_server_task.c** je pouze placeholder:
- ❌ No HTTP server implementation
- ❌ No WiFi AP setup
- ❌ No REST API endpoints
- ❌ No WebSocket support
- ❌ No HTML/CSS/JS frontend

---

## 🎯 Implementation Plan

### Phase 1: WiFi AP Setup

**Files to modify:**
- `components/web_server_task/web_server_task.c`
- `components/web_server_task/include/web_server_task.h`

**Configuration:**
```c
#define WIFI_AP_SSID "ESP32-Chess"
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_GATEWAY "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"
```

**Initialization order (CRITICAL!):**
```c
1. nvs_flash_init()           // MUST BE FIRST!
2. esp_netif_init()
3. esp_event_loop_create_default()
4. esp_wifi_init(&cfg)
5. esp_wifi_set_mode(WIFI_MODE_AP)
6. esp_wifi_set_config(WIFI_IF_AP, &ap_config)
7. esp_wifi_start()
8. esp_netif_dhcps_start()    // DHCP server
```

**Required includes:**
```c
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "cJSON.h"
```

### Phase 2: HTTP Server Implementation

**Server configuration:**
```c
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.server_port = 80;
config.max_uri_handlers = 10;
config.max_open_sockets = 4;
config.lru_purge_enable = true;
config.recv_wait_timeout = 10;
config.send_wait_timeout = 10;
```

**Required endpoints:**
```
GET  /                    → HTML page (chess board UI)
GET  /api/board           → JSON board state
GET  /api/status          → JSON game status
GET  /api/piece_lifted    → JSON lifted piece info
POST /api/move            → Execute move (JSON)
POST /api/new_game        → Start new game
WS   /ws                  → WebSocket for real-time updates
```

### Phase 3: REST API Endpoints

#### GET /api/board
**Response:**
```json
{
  "board": [
    ["R","N","B","Q","K","B","N","R"],
    ["P","P","P","P","P","P","P","P"],
    [" "," "," "," "," "," "," "," "],
    [" "," "," "," "," "," "," "," "],
    [" "," "," "," "," "," "," "," "],
    [" "," "," "," "," "," "," "," "],
    ["p","p","p","p","p","p","p","p"],
    ["r","n","b","q","k","b","n","r"]
  ],
  "timestamp": 1234567890
}
```

#### GET /api/status
**Response:**
```json
{
  "game_state": "active",
  "current_player": "white",
  "move_count": 15,
  "white_time": 1200,
  "black_time": 1180,
  "white_captures": 2,
  "black_captures": 1,
  "in_check": false,
  "checkmate": false,
  "stalemate": false
}
```

#### GET /api/piece_lifted
**Response:**
```json
{
  "piece_lifted": true,
  "row": 6,
  "col": 4,
  "piece": "P",
  "notation": "e2",
  "timestamp": 1234567890
}
```

#### POST /api/move
**Request:**
```json
{
  "from": "e2",
  "to": "e4"
}
```

**Response:**
```json
{
  "success": true,
  "move": {
    "from": "e2",
    "to": "e4",
    "piece": "P",
    "captured": null,
    "is_check": false,
    "is_checkmate": false
  },
  "game_state": "active",
  "current_player": "black"
}
```

### Phase 4: WebSocket Implementation

**Purpose:** Real-time updates without polling

**Events to send:**
```json
// Piece lifted
{
  "event": "piece_lifted",
  "data": {
    "row": 6,
    "col": 4,
    "piece": "P",
    "notation": "e2"
  }
}

// Move executed
{
  "event": "move_executed",
  "data": {
    "from": "e2",
    "to": "e4",
    "piece": "P"
  }
}

// Player changed
{
  "event": "player_changed",
  "data": {
    "player": "black"
  }
}

// Game state changed
{
  "event": "game_state_changed",
  "data": {
    "state": "checkmate",
    "winner": "white"
  }
}
```

### Phase 5: HTML/CSS/JS Frontend

**Features:**
- 8x8 chess board visualization
- Piece images (Unicode characters or images)
- Real-time updates via WebSocket
- Move input (click from, click to)
- Game status display
- Move history
- Responsive design

**HTML Structure:**
```html
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-C6 Chess Board</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="style.css">
</head>
<body>
    <div class="container">
        <h1>ESP32-C6 Chess Board</h1>
        <div class="board-container">
            <div id="chess-board"></div>
        </div>
        <div class="info-panel">
            <div id="game-status"></div>
            <div id="move-history"></div>
        </div>
    </div>
    <script src="app.js"></script>
</body>
</html>
```

**CSS Features:**
- Grid layout for 8x8 board
- Piece styling
- Highlighting for lifted piece
- Responsive design
- Dark/Light theme

**JavaScript Features:**
- WebSocket connection
- Board rendering
- Click handlers
- Move validation (client-side)
- Real-time updates

---

## 🔧 Technical Implementation Details

### Memory Optimization
- **Stack size:** 6KB (already configured)
- **Heap usage:** Minimize JSON allocations
- **String pool:** Reuse common strings
- **Buffer management:** Use static buffers where possible

### Thread Safety
- **Mutex protection:** Use `game_mutex` for board access
- **Queue communication:** Use `web_command_queue` for commands
- **Atomic operations:** For simple flags

### Error Handling
- **WiFi errors:** Retry with exponential backoff
- **HTTP errors:** Return proper status codes
- **WebSocket errors:** Reconnect automatically
- **Memory errors:** Graceful degradation

### Performance Considerations
- **Polling interval:** 100ms for board updates
- **WebSocket buffer:** 512 bytes
- **JSON size:** Minimize payload size
- **Compression:** Consider gzip for HTML/CSS/JS

---

## 📝 Implementation Checklist

### Phase 1: WiFi AP Setup
- [ ] Add WiFi includes to web_server_task.c
- [ ] Implement wifi_init_ap() function
- [ ] Configure WiFi AP settings
- [ ] Start DHCP server
- [ ] Test WiFi connection

### Phase 2: HTTP Server
- [ ] Add HTTP server includes
- [ ] Implement httpd_start_server() function
- [ ] Configure HTTP server
- [ ] Test HTTP server

### Phase 3: REST API Endpoints
- [ ] Implement GET /api/board handler
- [ ] Implement GET /api/status handler
- [ ] Implement GET /api/piece_lifted handler
- [ ] Implement POST /api/move handler
- [ ] Implement POST /api/new_game handler
- [ ] Test all endpoints

### Phase 4: WebSocket
- [ ] Implement WebSocket handler
- [ ] Add event broadcasting
- [ ] Implement reconnection logic
- [ ] Test WebSocket connection

### Phase 5: Frontend
- [ ] Create HTML file
- [ ] Create CSS file
- [ ] Create JavaScript file
- [ ] Implement board rendering
- [ ] Implement WebSocket client
- [ ] Implement move input
- [ ] Test frontend

### Phase 6: Integration
- [ ] Integrate with game_task
- [ ] Add board state access functions
- [ ] Add piece lifted tracking
- [ ] Add move execution
- [ ] Test full system

### Phase 7: Testing
- [ ] Test WiFi connection from multiple devices
- [ ] Test HTTP endpoints with curl
- [ ] Test WebSocket with browser
- [ ] Test real-time updates
- [ ] Test move execution
- [ ] Test error handling
- [ ] Performance testing

---

## 🚀 Next Steps

1. **Review this plan** - Make sure everything is clear
2. **Start with Phase 1** - WiFi AP setup
3. **Test incrementally** - After each phase
4. **Iterate** - Based on testing results
5. **Document** - Keep notes on what works

---

## 📚 Reference Documentation

- [ESP-IDF HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)
- [ESP-IDF WiFi](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html)
- [ESP-IDF WebSocket](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html#websocket)
- [cJSON](https://github.com/DaveGamble/cJSON)

---

**Status:** ✅ Analysis Complete  
**Next:** Awaiting user approval to start implementation  
**Estimated Time:** 4-6 hours for full implementation

