# Web Server - Důkladný Popis Architektury

## 📋 Obsah
1. [Přehled](#přehled)
2. [Inicializace a Startup](#inicializace-a-startup)
3. [WiFi Access Point](#wifi-access-point)
4. [HTTP Server](#http-server)
5. [REST API Endpointy](#rest-api-endpointy)
6. [Komunikace s Game Task](#komunikace-s-game-task)
7. [Frontend (HTML/JavaScript)](#frontend-htmljavascript)
8. [Real-time Aktualizace](#real-time-aktualizace)
9. [Timer API](#timer-api)
10. [Captive Portal](#captive-portal)
11. [Memory Management](#memory-management)

---

## Přehled

Web server je implementován jako **FreeRTOS task** (`web_server_task`), který:
- Vytváří **WiFi Access Point** (hotspot)
- Spouští **HTTP server** na portu 80
- Poskytuje **REST API** pro získání stavu hry
- Slouží **HTML/JavaScript frontend** s real-time aktualizacemi
- Podporuje **captive portal** pro automatické otevření prohlížeče

**Soubory:**
- `components/web_server_task/web_server_task.c` - hlavní implementace
- `components/web_server_task/include/web_server_task.h` - hlavička
- `components/web_server_task/chess_app.js` - JavaScript frontend (externí soubor)

---

## Inicializace a Startup

### 1. Task Spuštění

```c
void web_server_task_start(void *pvParameters)
```

**Pořadí inicializace:**

1. **Registrace s Task Watchdog Timer (TWDT)**
   ```c
   esp_task_wdt_add(NULL);
   ```

2. **WiFi AP Inicializace**
   ```c
   wifi_init_ap();
   ```
   - Vytvoří netif, event loop, WiFi AP
   - Nastaví SSID, heslo, IP adresu
   - Spustí DHCP server

3. **Čekání na WiFi připravenost**
   ```c
   vTaskDelay(pdMS_TO_TICKS(2000));  // 2 sekundy
   ```

4. **HTTP Server Spuštění**
   ```c
   start_http_server();
   ```
   - Vytvoří HTTP server s konfigurací
   - Registruje všechny URI handlery

5. **Main Loop**
   - Zpracování příkazů z fronty
   - Aktualizace stavu serveru
   - Periodické logování (každých 100ms)

---

## WiFi Access Point

### Konfigurace

```c
#define WIFI_AP_SSID "ESP32-CzechMate"
#define WIFI_AP_PASSWORD "12345678"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4
#define WIFI_AP_IP "192.168.4.1"
#define WIFI_AP_GATEWAY "192.168.4.1"
#define WIFI_AP_NETMASK "255.255.255.0"
```

### Inicializace (`wifi_init_ap()`)

**Kroky:**

1. **Netif Initialization**
   ```c
   esp_netif_init();
   ```
   - Inicializuje síťové rozhraní

2. **Event Loop**
   ```c
   esp_event_loop_create_default();
   ```
   - Vytvoří default event loop pro WiFi eventy

3. **AP Netif Creation**
   ```c
   ap_netif = esp_netif_create_default_wifi_ap();
   ```
   - Vytvoří default WiFi AP netif

4. **WiFi Initialization**
   ```c
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   esp_wifi_init(&cfg);
   ```

5. **Event Handler Registration**
   ```c
   esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                      &wifi_event_handler, NULL, NULL);
   ```
   - Registruje handler pro sledování připojení/odpojení klientů

6. **WiFi Configuration**
   ```c
   wifi_config_t wifi_config = {
       .ap = {
           .ssid = WIFI_AP_SSID,
           .password = WIFI_AP_PASSWORD,
           .channel = WIFI_AP_CHANNEL,
           .max_connection = WIFI_AP_MAX_CONNECTIONS,
           .authmode = WIFI_AUTH_WPA2_PSK
       }
   };
   esp_wifi_set_mode(WIFI_MODE_AP);
   esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
   ```

7. **Start WiFi**
   ```c
   esp_wifi_start();
   ```

### Event Handler

```c
static void wifi_event_handler(...)
```

**Sledované eventy:**
- `WIFI_EVENT_AP_STACONNECTED` - klient se připojil
- `WIFI_EVENT_AP_STADISCONNECTED` - klient se odpojil

**Aktualizace:**
- `client_count++` při připojení
- `client_count--` při odpojení

---

## HTTP Server

### Konfigurace

```c
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
config.server_port = 80;
config.max_uri_handlers = 16;
config.max_open_sockets = 4;
config.lru_purge_enable = true;
config.recv_wait_timeout = 10;
config.send_wait_timeout = 1000;  // 1 sekunda
config.max_resp_headers = 8;
config.backlog_conn = 5;
config.stack_size = 8192;
```

### Spuštění

```c
esp_err_t ret = httpd_start(&httpd_handle, &config);
```

**Handle:** `static httpd_handle_t httpd_handle = NULL;`

---

## REST API Endpointy

### GET Endpointy (Read-Only)

#### 1. `GET /` - Hlavní HTML Stránka

**Handler:** `http_get_root_handler()`

**Funkce:**
- Vrací kompletní HTML stránku s inline CSS a JavaScript
- Obsahuje šachovnici, status panel, historii tahů
- Embeduje JavaScript z `/chess_app.js`

**Response:**
- Content-Type: `text/html`
- Velikost: ~50-60 KB (kompletní HTML s inline CSS/JS)

#### 2. `GET /chess_app.js` - JavaScript Frontend

**Handler:** `http_get_chess_js_handler()`

**Funkce:**
- Vrací externí JavaScript soubor
- Obsahuje logiku pro šachovnici, real-time updates, sandbox mode, review mode

**Response:**
- Content-Type: `application/javascript`
- Velikost: ~27 KB

#### 3. `GET /api/board` - Stav Šachovnice

**Handler:** `http_get_board_handler()`

**Funkce:**
- Získá stav šachovnice z `game_task` pomocí `game_get_board_json()`
- Vrací JSON s 8x8 maticí figurek

**Response:**
```json
{
  "board": [
    ["R","N","B","Q","K","B","N","R"],
    ["P","P","P","P","P","P","P","P"],
    [" "," "," "," "," "," "," "," "],
    ...
  ],
  "timestamp": 1234567890
}
```

**Thread Safety:**
- Používá `game_mutex` pro thread-safe přístup k board state

#### 4. `GET /api/status` - Stav Hry

**Handler:** `http_get_status_handler()`

**Funkce:**
- Získá stav hry z `game_task` pomocí `game_get_status_json()`

**Response:**
```json
{
  "game_state": "active",
  "current_player": "White",
  "move_count": 5,
  "in_check": false,
  "checkmate": false,
  "stalemate": false,
  ...
}
```

#### 5. `GET /api/history` - Historie Tahů

**Handler:** `http_get_history_handler()`

**Funkce:**
- Získá historii tahů z `game_task` pomocí `game_get_history_json()`

**Response:**
```json
{
  "moves": [
    {"from": "e2", "to": "e4", "piece": "P", "timestamp": 1234567890},
    {"from": "e7", "to": "e5", "piece": "p", "timestamp": 1234567900},
    ...
  ]
}
```

#### 6. `GET /api/captured` - Sebrané Figurky

**Handler:** `http_get_captured_handler()`

**Funkce:**
- Získá seznam sebraných figurek z `game_task` pomocí `game_get_captured_json()`

**Response:**
```json
{
  "white_captured": ["p", "n"],
  "black_captured": ["P"]
}
```

#### 7. `GET /api/advantage` - Material Advantage Graf

**Handler:** `http_get_advantage_handler()`

**Funkce:**
- Získá historii material advantage z `game_task` pomocí `game_get_advantage_json()`

**Response:**
```json
{
  "history": [0, 1, 2, -1, 0, ...],
  "count": 42,
  "white_checks": 5,
  "black_checks": 3,
  "white_castles": 1,
  "black_castles": 1,
  "game_duration_ms": 3600000,
  "avg_time_per_move_ms": 45000
}
```

#### 8. `GET /api/timer` - Stav Timeru

**Handler:** `http_get_timer_handler()`

**Funkce:**
- Získá stav časového systému z `game_task` pomocí `game_get_timer_json()`

**Response:**
```json
{
  "white_time_ms": 300000,
  "black_time_ms": 295000,
  "current_player": "White",
  "timer_running": true,
  "time_control_type": 1,
  ...
}
```

**Cache Control:**
- `Cache-Control: no-store` - zabraňuje cachování v prohlížeči

### POST Endpointy (Write)

#### 1. `POST /api/timer/config` - Konfigurace Timeru

**Handler:** `http_post_timer_config_handler()`

**Funkce:**
- Parsuje JSON s konfigurací časového systému
- Odesílá příkaz `GAME_CMD_SET_TIME_CONTROL` do `game_command_queue`

**Request Body:**
```json
{
  "type": 1,
  "custom_minutes": 10,
  "custom_increment": 5
}
```

**Validace:**
- `type`: 0-14 (time control types)
- `custom_minutes`: 1-180 (pouze pro type=14)
- `custom_increment`: 0-60 (pouze pro type=14)

#### 2. `POST /api/timer/pause` - Pozastavení Timeru

**Handler:** `http_post_timer_pause_handler()`

**Funkce:**
- Odesílá příkaz `GAME_CMD_PAUSE_TIMER` do `game_command_queue`

#### 3. `POST /api/timer/resume` - Obnovení Timeru

**Handler:** `http_post_timer_resume_handler()`

**Funkce:**
- Odesílá příkaz `GAME_CMD_RESUME_TIMER` do `game_command_queue`

#### 4. `POST /api/timer/reset` - Reset Timeru

**Handler:** `http_post_timer_reset_handler()`

**Funkce:**
- Odesílá příkaz `GAME_CMD_RESET_TIMER` do `game_command_queue`

---

## Komunikace s Game Task

### JSON Export Funkce

Všechny API endpointy používají funkce z `game_task` pro získání dat:

#### 1. `game_get_board_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje stav šachovnice do JSON stringu
- Používá **thread-safe přístup** pomocí `game_mutex`
- **Manuální JSON serializace** (efektivnější než cJSON)

**Implementace:**
```c
// Thread-safe access
xSemaphoreTake(game_mutex, pdMS_TO_TICKS(1000));

// Build JSON manually
snprintf(buffer, size, "{\"board\":[[\"R\",\"N\",...],...],\"timestamp\":%llu}", timestamp);

xSemaphoreGive(game_mutex);
```

#### 2. `game_get_status_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje stav hry (game_state, current_player, move_count, check/checkmate/stalemate)

#### 3. `game_get_history_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje historii tahů z `move_history[]` pole

#### 4. `game_get_captured_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje seznam sebraných figurek

#### 5. `game_get_advantage_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje historii material advantage pro graf

#### 6. `game_get_timer_json(char* buffer, size_t size)`

**Funkce:**
- Exportuje stav časového systému

### Command Queue

**POST endpointy** odesílají příkazy do `game_command_queue`:

```c
extern QueueHandle_t game_command_queue;

chess_move_command_t cmd = { 0 };
cmd.type = GAME_CMD_SET_TIME_CONTROL;
// ... nastavení parametrů ...
xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100));
```

**Typy příkazů:**
- `GAME_CMD_SET_TIME_CONTROL` - konfigurace timeru
- `GAME_CMD_PAUSE_TIMER` - pozastavení
- `GAME_CMD_RESUME_TIMER` - obnovení
- `GAME_CMD_RESET_TIMER` - reset

---

## Frontend (HTML/JavaScript)

### HTML Struktura

**Hlavní stránka (`GET /`):**

1. **HTML Head**
   - Meta tags (viewport, charset)
   - Inline CSS (kompletní styling)
   - Title: "ESP32 Chess Board"

2. **Body**
   - **Header** - název aplikace
   - **Main Container**
     - **Chess Board** (`<div id="board">`) - 8x8 grid
     - **Info Panel**
       - Status (game state, current player)
       - Captured pieces (white/black)
       - Timer display
       - History panel
     - **Controls**
       - Sandbox mode toggle
       - Review mode controls
       - Timer controls

3. **Inline JavaScript**
   - Základní inicializace
   - Načítání externího `/chess_app.js`

### JavaScript Frontend (`chess_app.js`)

#### Globální Proměnné

```javascript
let boardData = [];           // Aktuální stav šachovnice
let statusData = {};         // Stav hry
let historyData = [];        // Historie tahů
let capturedData = {};        // Sebrané figurky
let selectedSquare = null;   // Vybrané pole
let reviewMode = false;      // Review mode flag
let sandboxMode = false;     // Sandbox mode flag
```

#### Hlavní Funkce

**1. `createBoard()`**
- Vytvoří 8x8 grid šachovnice v DOM
- Každé pole má `data-row`, `data-col`, `data-index`
- Event handler: `handleSquareClick(row, col)`

**2. `updateBoard(board)`**
- Aktualizuje zobrazení figurek na šachovnici
- Používá Unicode chess symbols (♜, ♞, ♝, ...)
- Aktualizuje CSS třídy (white/black pieces)

**3. `updateStatus(status)`**
- Aktualizuje status panel (game state, current player, move count)
- Zobrazuje check/checkmate/stalemate

**4. `updateHistory(history)`**
- Zobrazuje historii tahů v panelu
- Každý tah je klikatelný pro review mode

**5. `updateCaptured(captured)`**
- Zobrazuje sebrané figurky (white/black)

**6. `fetchData()`**
- **Real-time aktualizace** - načítá data ze všech API endpointů
- Používá `Promise.all()` pro paralelní načítání
- Volá se každých **500ms** (`setInterval`)

```javascript
async function fetchData() {
    const [boardRes, statusRes, historyRes, capturedRes] = await Promise.all([
        fetch('/api/board'),
        fetch('/api/status'),
        fetch('/api/history'),
        fetch('/api/captured')
    ]);
    
    const board = await boardRes.json();
    const status = await statusRes.json();
    const history = await historyRes.json();
    const captured = await capturedRes.json();
    
    updateBoard(board.board);
    updateStatus(status);
    updateHistory(history);
    updateCaptured(captured);
}
```

#### Módy

**1. Normal Mode (Real Game)**
- Zobrazuje aktuální stav hry
- Real-time aktualizace každých 500ms
- Nelze provádět tahy (read-only)

**2. Sandbox Mode**
- Umožňuje zkoušet tahy lokálně (bez ovlivnění fyzického boardu)
- Undo funkce (max 10 tahů)
- Neovlivňuje skutečnou hru

**3. Review Mode**
- Prohlížení historických tahů
- Rekonstrukce boardu na konkrétní tah
- Navigace dopředu/dozadu v historii

---

## Real-time Aktualizace

### Mechanismus

**Frontend:**
```javascript
// Počáteční načtení
fetchData();

// Periodická aktualizace každých 500ms
setInterval(fetchData, 500);
```

**Backend:**
- Každý API endpoint vrací aktuální data z `game_task`
- Data jsou vždy fresh (žádné cachování)
- Thread-safe přístup pomocí `game_mutex`

### Optimalizace

1. **Paralelní Fetch**
   - `Promise.all()` pro současné načtení všech endpointů
   - Snižuje latenci

2. **JSON Buffer Pool**
   - Statický buffer `json_buffer[JSON_BUFFER_SIZE]` (2048 bytes)
   - Znovupoužitelný pro všechny endpointy
   - Minimalizuje heap alokace

3. **Manuální JSON Serializace**
   - Efektivnější než cJSON
   - Menší memory footprint

---

## Timer API

### GET `/api/timer`

**Funkce:**
- Vrací aktuální stav časového systému
- **Cache-Control: no-store** - zabraňuje cachování

**Response:**
```json
{
  "white_time_ms": 300000,
  "black_time_ms": 295000,
  "current_player": "White",
  "timer_running": true,
  "time_control_type": 1,
  "white_increment_ms": 5000,
  "black_increment_ms": 5000
}
```

### POST `/api/timer/config`

**Request:**
```json
{
  "type": 1,
  "custom_minutes": 10,
  "custom_increment": 5
}
```

**Validace:**
- Parsuje JSON pomocí `strstr()` a `sscanf()`
- Validuje rozsahy hodnot
- Odesílá příkaz do `game_command_queue`

### POST `/api/timer/pause`, `/api/timer/resume`, `/api/timer/reset`

**Funkce:**
- Jednoduché příkazy pro ovládání timeru
- Odesílají příkazy do `game_command_queue`

---

## Captive Portal

### Účel

Automatické otevření prohlížeče při připojení k WiFi hotspotu.

### Handlery

#### 1. `GET /generate_204` (Android)

**Handler:** `http_get_generate_204_handler()`

**Response:**
- Status: `204 No Content`
- Prázdné tělo

**Funkce:**
- Android detekuje captive portal, pokud `/generate_204` vrací 204
- Automaticky otevře prohlížeč

#### 2. `GET /hotspot-detect.html` (iOS)

**Handler:** `http_get_hotspot_handler()`

**Response:**
- Status: `302 Found`
- Header: `Location: /`
- Redirect na hlavní stránku

**Funkce:**
- iOS detekuje captive portal pomocí tohoto endpointu
- Automaticky otevře prohlížeč

#### 3. `GET /connecttest.txt` (Windows)

**Handler:** `http_get_connecttest_handler()`

**Response:**
- Status: `302 Found`
- Header: `Location: /`
- Redirect na hlavní stránku

**Funkce:**
- Windows detekuje captive portal pomocí tohoto endpointu
- Automaticky otevře prohlížeč

---

## Memory Management

### JSON Buffer Pool

**Statický buffer:**
```c
static char json_buffer[JSON_BUFFER_SIZE];  // 2048 bytes
```

**Použití:**
- Všechny GET endpointy používají stejný buffer
- **Race condition protection:** Timer endpoint používá lokální buffer
- Znovupoužitelný pro všechny requesty

### Optimalizace

1. **Manuální JSON Serializace**
   - Efektivnější než cJSON
   - Menší memory footprint
   - Rychlejší

2. **Statické Buffery**
   - Minimalizace heap alokací
   - Předvídatelná memory usage

3. **Thread Safety**
   - `game_mutex` pro thread-safe přístup
   - Zabránění race conditions

---

## Shrnutí Architektury

### Data Flow

```
┌─────────────┐
│   Browser   │
│  (Frontend) │
└──────┬──────┘
       │ HTTP GET/POST
       ▼
┌─────────────────┐
│  HTTP Server     │
│  (Port 80)       │
└──────┬───────────┘
       │
       │ API Handlers
       ▼
┌─────────────────┐
│  Web Server     │
│  Task           │
└──────┬───────────┘
       │
       │ game_get_*_json()
       │ game_command_queue
       ▼
┌─────────────────┐
│  Game Task      │
│  (Chess Logic)  │
└─────────────────┘
```

### Klíčové Komponenty

1. **WiFi AP** - vytváří hotspot "ESP32-CzechMate"
2. **HTTP Server** - obsluhuje HTTP requesty na portu 80
3. **REST API** - poskytuje JSON data o stavu hry
4. **Frontend** - HTML/JavaScript pro zobrazení šachovnice
5. **Real-time Updates** - periodické načítání dat každých 500ms
6. **Captive Portal** - automatické otevření prohlížeče
7. **Thread Safety** - mutexy pro bezpečný přístup k datům

### Výkon

- **Max klientů:** 4 současně
- **Update interval:** 500ms
- **JSON buffer:** 2048 bytes
- **Stack size:** 8192 bytes
- **Memory:** Statické buffery, minimální heap alokace

---

**Konec dokumentu**

