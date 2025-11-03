# ESP32-C6 Chess System v2.4 - FreeChess

## 🎯 Prehled Projektu

Profesionalni system pro elektronickou sachovnici postavenou na ESP32-C6 mikrokontroleru s WiFi podporou, LED vizualizaci, fyzickym detekovanim figurek pomoci reed kontaktu a webovym rozhranim.

**Autor:** Alfred Krutina  
**Verze:** 2.4  
**Platforma:** ESP32-C6 (RISC-V)  
**Framework:** ESP-IDF v5.5.1 + FreeRTOS  

---

## 📋 Obsah

- [Popis Systemu](#-popis-systemu)
- [Hlavni Funkce](#-hlavni-funkce)
- [Hardware Specifikace](#️-hardware-specifikace)
- [Architektura Software](#-architektura-software)
- [Instalace a Kompilace](#-instalace-a-kompilace)
- [Pouziti](#-pouziti)
- [UART Prikazy](#-uart-prikazy)
- [Webove Rozhrani](#-webove-rozhrani)
- [LED System](#-led-system)
- [Komponenty](#-komponenty)
- [Konfigurace](#️-konfigurace)
- [Troubleshooting](#-troubleshooting)
- [Budouci Vylepseni](#-budouci-vylepseni)

---

## 🎮 Popis Systemu

Free Chess je kompletni implementation elektronicke sachovnice s nasledujicimi vlastnostmi:

### Core Features
- **Automaticke Detekovani Tahu** - Reed kontakty pod kazdym polem
- **LED Vizualizace** - WS2812B LED strip (64 poli + 9 tlacitek)
- **WiFi Web Interface** - Vzdalne ovladani a monitoring
- **UART Control** - Profesionalni CLI rozhrani
- **Chess Engine Integration** - Plna podpora pro chess rules
- **Timer System** - Sachy s casomira
- **Matter Protocol** - Smart home integrace (pripraven)

### Pokrocile Funkce
- **Enhanced Castling** - Vizualni navod pro rosadu
- **Puzzle Mode** - Sacove hadanky s krokovym resenim
- **Endgame Animations** - 6 ruznych animaci pro konec hry
- **Visual Error System** - Inteligentni indikace chyb s navodem
- **Screen Saver** - Usporne animace pri neci nnosti
- **Multi-layer LED** - Vrstvovy system pro slozite efekty

---

## ⚡ Hlavni Funkce

### 1. Sachovnice (8x8)
- **64 reed kontaktu** pro automaticke detekovani figurek
- **64 RGB LED** pro vizualizaci pozic, tahu, chyb
- **Time-multiplexed scanning** - sdilene piny s tlacitkama
- **Debounce system** - stabilni detekce bez false triggeru

### 2. LED Systém
- **WS2812B RGB LED Strip** - 73 LED (64 + 9)
- **8-vrstvovy kompozitni system**:
  - `BACKGROUND` - zakladni barva pozadi
  - `PIECES` - zobrazeni figurek
  - `MOVES` - legalni tahy
  - `SELECTION` - vyber figurky
  - `ANIMATION` - dynamicke animace
  - `STATUS` - check/checkmate indikace
  - `ERROR` - chybove stavy
  - `GUI` - overlay pro tlacitka

### 3. Fyzicka Tlacitka
- **Reset Button** - restart hry s LED feedback
- **Promotion Buttons (4x)** - vyber figurky pro promoci
  - Queen (zlata)
  - Rook (modra)
  - Bishop (zelena)
  - Knight (oranzova)
- **Mode Buttons (4x)** - prepinani modu
- **Debouncing** - hardwarovy + softwarovy
- **Long-press detection** - 2s pro specialni funkce

### 4. Web Server
- **WiFi AP Mode** - `ESP32_Chess_XXXXXX`
- **REST API** - JSON komunikace
- **Live Game Status** - real-time aktualizace
- **Move History** - kompletni historie partie
- **Timer Display** - zobrazeni casu obou hracu
- **Greek-inspired Design** - elegantni UI s modernim vzhledem

### 5. UART Interface
- **Baud Rate:** 115200
- **Commands:** 50+ prikazu pro kompletni ovladani
- **Auto-complete** - TAB completion
- **Command History** - sipky nahoru/dolu
- **Help System** - integrovana napoveda
- **Streaming Output** - optimalizovany pro velke vystupy

### 6. Game Logic
- **Full Chess Rules** - vsechna oficialni pravidla
- **Move Validation** - kontrola legalnosti tahu
- **Check Detection** - automaticka detekce sachu
- **Checkmate/Stalemate** - detekce konce hry
- **En Passant** - podpora pro en passant
- **Castling** - kingside i queenside rosada s navodem
- **Promotion** - interaktivni vyber figurky

### 7. Timer System
- **Increment Support** - Fischer timer (napr. 5+3)
- **Countdown** - presny odpocet
- **Time Warnings** - LED + audio pri malo casu
- **Pause/Resume** - moznost pozastaveni
- **Time Display** - zobrazeni na webu i UART

---

## 🛠️ Hardware Specifikace

### Mikrokontroler
- **Model:** ESP32-C6 (RISC-V 32-bit)
- **Frekvence:** 160 MHz
- **RAM:** 512 KB SRAM
- **Flash:** 4 MB
- **WiFi:** 802.11 b/g/n (2.4 GHz)
- **Bluetooth:** BLE 5.0
- **GPIO:** 30 pinu (22 pouzito)

### Periferie
- **LED Strip:** WS2812B, 73 LED, RMT protokol
- **Matrix:** 8x8 reed kontakty (time-multiplexed)
- **Buttons:** 9x fyzickych tlacitek s pull-up
- **UART:** USB-JTAG-Serial pro debug a control
- **WiFi:** Access Point pro web server

### GPIO Maping

#### LED System
- `GPIO 8` - WS2812B Data (RMT Channel 0)

#### Matrix Scanning (Reed Contacts)
**Rows (Output):**
- GPIO 15, 16, 17, 18, 19, 20, 21, 22

**Columns (Input with Pull-Up):**
- GPIO 0, 1, 2, 3, 4, 5, 6, 7

#### Tlacitka (Time-Multiplexed s Matrix)
**Promotion Buttons:**
- GPIO 0 - Queen (zlata)
- GPIO 1 - Rook (modra)
- GPIO 2 - Bishop (zelena)
- GPIO 3 - Knight (oranzova)

**Mode Buttons:**
- GPIO 4, 5, 6, 7

**Reset:**
- GPIO 9 - Reset Button (dedicated)

### Power Requirements
- **Napeti:** 5V DC (USB)
- **Proud:** ~2A (pri full brightness LED)
- **Spotreba:** ~10W max

---

## 🏗️ Architektura Software

### FreeRTOS Tasks

Projekt pouziva **10 dedicovanych FreeRTOS tasku** s prioritami:

| Task | Priorita | Stack | Frekvence | Popis |
|------|----------|-------|-----------|-------|
| `matrix_task` | 5 | 3KB | 50 Hz | Scan reed contact matrix |
| `button_task` | 5 | 3KB | 20 Hz | Debounce a detect button press |
| `led_task` | 4 | 4KB | 60 Hz | Update WS2812B LED strip |
| `game_task` | 3 | 8KB | Event | Core chess logic |
| `uart_task` | 3 | 6KB | Event | UART command processing |
| `web_server_task` | 3 | 6KB | Event | HTTP server handling |
| `timer_system` | 3 | 2KB | 10 Hz | Chess clock management |
| `animation_task` | 2 | 3KB | 30 Hz | LED animations |
| `promotion_task` | 2 | 2KB | Event | Promotion button handling |
| `reset_task` | 2 | 2KB | Event | Reset button handling |

### Inter-Task Komunikace

**FreeRTOS Queues:**
- `matrix_event_queue` - Matrix udalosti → Game Task
- `button_event_queue` - Button udalosti → Game Task
- `led_command_queue` - LED prikazy → LED Task
- `game_event_queue` - Game udalosti → ostatni tasky
- `uart_response_queue` - Odpovedi → UART Task
- `promotion_queue` - Promoce udalosti → Game Task

**Semaphores & Mutexes:**
- `game_state_mutex` - Ochrana globalniho stavu hry
- `led_buffer_mutex` - Ochrana LED bufferu
- `uart_mutex` - Serializ ace UART vystupu
- `web_clients_mutex` - Ochrana seznamu klientu

---

## 🚀 Instalace a Kompilace

### Predpoklady
```bash
# ESP-IDF v5.5.1 nebo novejsi
# Python 3.9+
# Git
```

### Setup ESP-IDF (Windows)
```powershell
cd C:\Users\alfid\esp\esp-idf
.\export.ps1
```

### Setup ESP-IDF (macOS/Linux)
```bash
cd ~/esp/esp-idf
source ./export.sh
```

### Kompilace
```bash
# 1. Clone repository
git clone <repository-url>
cd free_chess_v1

# 2. Update managed components
idf.py update-dependencies

# 3. Configure project (optional)
idf.py menuconfig

# 4. Build
idf.py build

# 5. Flash
idf.py -p /dev/ttyUSB0 flash

# 6. Monitor (optional)
idf.py -p /dev/ttyUSB0 monitor
```

### Quick Build
```bash
idf.py build flash
```

---

## 💻 Pouziti

### 1. První Spuštění

Po nahrani firmware:
1. **LED Test** - Krátké "loading" animace
2. **WiFi AP Start** - SSID: `ESP32_Chess_XXXXXX`
3. **Ready State** - Zelene LED na boardu

### 2. Zahajeni Hry

**Pres UART:**
```
game_new
```

**Pres Web:**
- Pripoj se k WiFi: `ESP32_Chess_XXXXXX`
- Otevri: `http://192.168.4.1`
- Klikni "New Game"

### 3. Hraci Tah

**Fyzicky:**
1. Zvedni figurku (LED ukazou legalni tahy)
2. Polož na cilove pole
3. Animace potvrzeni

**UART:**
```
move e2e4
```

### 4. Promoce

Kdyz pesec dosahne konce:
1. LED blikaji na promovane pozici
2. Stiskni tlacitko pro vyber:
   - **Zlata** = Queen
   - **Modra** = Rook
   - **Zelena** = Bishop
   - **Oranzova** = Knight

### 5. Rosada

System automaticky detekuje rosadu:
- **Kingside:** e1g1 (white) nebo e8g8 (black)
- **Queenside:** e1c1 (white) nebo e8c8 (black)
- **Vizualni navod** - LED ukazou kam presunou veze

---

## 📡 UART Prikazy

### Game Control
```
game_new              - Nova hra
game_status           - Status aktualni hry
game_reset            - Reset do start pozice
game_undo             - Vrat posledni tah
board                 - Zobraz sachovnici
moves                 - Zobraz vsechny legalni tahy
history               - Historie tahu
```

### Move Commands
```
move e2e4             - Proved tah
move e7e8q            - Tah s promoci
suggest               - Navrh tahu (pokud je AI)
```

### LED Control
```
led_test              - Test LED (rainbow)
led_brightness <0-255> - Nastav jas
led_off               - Vypni vsechny LED
led_pattern <pattern>  - Zobraz pattern
led_chess_position    - Zobraz sachovnici
```

### Timer Commands
```
timer_start           - Start timer
timer_pause           - Pauza
timer_resume          - Obnov
timer_set <white> <black> - Nastav cas (sekundy)
timer_increment <sec>  - Nastav increment
timer_status          - Status timeru
```

### System Commands
```
help                  - Zobraz napovedu
version               - Verze firmware
stats                 - Systemove statistiky
tasks                 - FreeRTOS task info
heap                  - Pamet info
restart               - Restart ESP32
wifi_status           - WiFi status
```

### Debug Commands
```
debug_matrix          - Matrix scan debug
debug_buttons         - Button state debug
debug_led             - LED system debug
test_animations       - Test vsech animaci
```

---

## 🌐 Webove Rozhrani

### Pripojeni
1. Pripoj se k WiFi: `ESP32_Chess_XXXXXX`
2. Heslo: (volitelne, defaultne zadne)
3. Otevri prohlizec: `http://192.168.4.1`

### Web API Endpoints

**Game Status:**
```
GET /api/status
Response: {
  "gameActive": true,
  "currentTurn": "white",
  "boardFEN": "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
  "moveHistory": ["e2e4", "e7e5", ...],
  "whiteTime": 300,
  "blackTime": 300,
  "inCheck": false,
  "gameResult": null
}
```

**Make Move:**
```
POST /api/move
Body: {"move": "e2e4"}
Response: {"success": true, "message": "Move executed"}
```

**New Game:**
```
POST /api/new_game
Body: {"timeWhite": 600, "timeBlack": 600, "increment": 3}
Response: {"success": true}
```

**Timer Control:**
```
POST /api/timer/pause
POST /api/timer/resume
GET /api/timer/status
```

---

## 💡 LED System

### LED Layout

**Sachovnice (64 LED):**
```
  a  b  c  d  e  f  g  h
8 [56][57][58][59][60][61][62][63]  8
7 [48][49][50][51][52][53][54][55]  7
6 [40][41][42][43][44][45][46][47]  6
5 [32][33][34][35][36][37][38][39]  5
4 [24][25][26][27][28][29][30][31]  4
3 [16][17][18][19][20][21][22][23]  3
2 [08][09][10][11][12][13][14][15]  2
1 [00][01][02][03][04][05][06][07]  1
  a  b  c  d  e  f  g  h
```

**Tlacitka (9 LED):**
- LED 64-67: Promotion buttons
- LED 68-71: Mode buttons  
- LED 72: Reset button

### Barvy

| Funkce | Barva | RGB |
|--------|-------|-----|
| White pieces | Bila | 255,255,255 |
| Black pieces | Cerna | 0,0,0 |
| Legal moves | Zelena | 0,255,0 |
| Selected piece | Zluta | 255,255,0 |
| Check | Cervena | 255,0,0 |
| Error | Cervena blikajici | 255,0,0 |
| Checkmate | Stribrna vlna | (animace) |
| Stalemate | Fialova spirala | (animace) |

### LED Animace

**Herni Animace:**
- `MOVE_PATH` - Plynula cesta figurky
- `CAPTURE` - Explozni efekt sebrani
- `CHECK_WARNING` - Pulzujici cervena na krali
- `LEGAL_MOVES` - Zobrazeni moznych tahu
- `CASTLING_GUIDE` - Navod pro rosadu

**Endgame Animace:**
- `ENDGAME_WAVE` - Vlna od vitezne strany
- `ENDGAME_CIRCLES` - Expandujici kruhy
- `ENDGAME_CASCADE` - Kaskadove padani barev
- `ENDGAME_FIREWORKS` - Ohnostroj celebrace
- `ENDGAME_DRAW_SPIRAL` - Spirala pro remizu
- `ENDGAME_DRAW_PULSE` - Duhove pulzovani

---

## 🧩 Komponenty

### Core Components

#### 1. **freertos_chess**
- Centralni header s GPIO definicemi
- Globalni handles (queues, mutexes)
- Spolecne utility funkce
- Chess types (move_t, piece_t, atd.)

#### 2. **game_task**
- Core chess engine
- Move validation
- Game state management
- FEN import/export
- PGN support

#### 3. **matrix_task**
- 8x8 reed contact scanning
- Time-multiplexing s tlacitkama
- Debouncing algorithm
- Change detection
- Task watchdog reset

#### 4. **led_task**
- WS2812B strip control (RMT)
- Frame buffering
- Gamma correction
- Brightness control
- 60 FPS refresh rate

#### 5. **button_task**
- Multi-button management
- Software debouncing (50ms)
- Long-press detection (2s)
- LED feedback
- Event publishing

### Advanced Components

#### 6. **unified_animation_manager**
- Centralizovana sprava animaci
- Priority system
- Plynule prechody
- 15+ preddefinovanych animaci
- Custom animation support

#### 7. **led_state_manager**
- 8-vrstvovy kompozitni system
- Alpha blending
- Dirty-pixel optimization
- Thread-safe pristup
- HSV→RGB konverze

#### 8. **visual_error_system**
- Inteligentni indikace chyb
- Textove zpravy v cestine
- Recovery hints
- LED flash patterns
- Error type mapping

#### 9. **enhanced_castling_system**
- Vizualni navod pro rosadu
- Detekce castling legality
- Krokove provedeni
- Error recovery
- LED guidance

#### 10. **timer_system**
- Fischer timer (increment)
- Precise countdown
- Low-time warnings
- Pause/resume
- Web integration

### Support Components

#### 11. **web_server_task**
- HTTP server (porty 80, 8080)
- REST API
- Static file serving
- WebSocket (pripraven)
- CORS support

#### 12. **uart_task**
- Command line interface
- Tab completion
- History buffer
- Streaming output
- Help system

#### 13. **config_manager**
- NVS storage
- Persistent config
- Factory reset
- Config validation

#### 14. **screen_saver_task**
- Timeout detection
- Ambient animations
- Power saving
- Auto-wake

---

## ⚙️ Konfigurace

### sdkconfig Highlights

```ini
# ESP32-C6 Specific
CONFIG_IDF_TARGET="esp32c6"
CONFIG_ESP_SYSTEM_SINGLE_CORE_MODE=y

# WiFi
CONFIG_ESP_WIFI_SSID="ESP32_Chess"
CONFIG_ESP_WIFI_PASSWORD=""
CONFIG_ESP_WIFI_CHANNEL=1
CONFIG_ESP_MAX_STA_CONN=4

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=y
CONFIG_FREERTOS_USE_TRACE_FACILITY=y

# Task Watchdog
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_ESP_TASK_WDT_PANIC=y

# Memory
CONFIG_ESP_SYSTEM_EVENT_QUEUE_SIZE=32
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=3072

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
CONFIG_LOG_MAXIMUM_LEVEL_VERBOSE=y
```

### Runtime Konfigurace

**LED Manager:**
```c
led_manager_config_t led_config = {
    .max_brightness = 255,
    .default_brightness = 128,
    .enable_smooth_transitions = true,
    .enable_layer_compositing = true,
    .update_frequency_hz = 60,
    .transition_duration_ms = 300
};
```

**Animation Manager:**
```c
animation_config_t anim_config = {
    .max_concurrent_animations = 8,
    .update_frequency_hz = 30,
    .enable_smooth_interpolation = true,
    .enable_trail_effects = true,
    .default_duration_ms = 1000
};
```

---

## 🔧 Troubleshooting

### Problem: LED Neblikaji
**Reseni:**
1. Zkontroluj GPIO 8 zapojeni
2. Zkontroluj napajeni 5V pro LED
3. UART: `led_test` - test LED
4. Zkontroluj `sdkconfig` - `CONFIG_LED_STRIP_*`

### Problem: Matrix Nedetekuje Tahy
**Reseni:**
1. UART: `debug_matrix` - zobraz raw data
2. Zkontroluj reed kontakty (odpor < 100Ω pri sepnuti)
3. Zkontroluj pull-up rezistory na column pinech
4. Overit time-multiplexing timing

### Problem: Web Server Nefunguje
**Reseni:**
1. UART: `wifi_status` - zkontroluj WiFi
2. Zkontroluj IP: 192.168.4.1
3. Restart ESP32: `restart`
4. Zkontroluj firewall v PC

### Problem: Build Chyby
**Reseni:**
```bash
# Vycisti build
rm -rf build
idf.py fullclean

# Rebuild
idf.py build
```

### Problem: Stack Overflow
**Reseni:**
1. Zkontroluj task stack sizes v `main.c`
2. Zvedni `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
3. Optimalizuj lokalni promenne ve funkcich
4. Pouzij heap misto stack pro velke buffery

---

## 🎯 Budouci Vylepseni

### Planovane Funkce (viz CHESS_IMPROVEMENTS_PLAN.md)

1. **Move History Display** - Kliknutelna historie na webu
2. **Enhanced Endgame Report** - Vice statistik
3. **Sandbox Mode** - Zkouseni tahu bez ovlivneni boardu
4. **Multi-Client Support** - Vice zarizeni soucasne
5. **Captured Pieces Info** - Interaktivni info o sebranych figurkach
6. **AI Integration** - Stockfish nebo jiný engine
7. **Bluetooth Control** - BLE rozhrani
8. **Mobile App** - Dedicovana aplikace

### Technicka Vylepseni

1. **WebSocket Integration** - Real-time updates
2. **Audio Feedback** - Zvuky pro tahy a udalosti
3. **E-Paper Display** - Externi displej pro timer
4. **Battery Operation** - Li-Po baterie + charging
5. **Tournament Mode** - FIDE-compliant timer
6. **Cloud Sync** - Ukladani partii do cloudu

---

## 📚 Dokumentace

### Doxygen

Projekt ma **kompletni Doxygen dokumentaci v cestine bez diakritiky**:

```bash
# Generate documentation
doxygen Doxyfile

# Open dokumentace
open docs/html/index.html
```

### Key Files

- `CHESS_IMPROVEMENTS_PLAN.md` - Planovane vylepseni
- `WEB_SERVER_IMPLEMENTATION_COMPLETE.md` - Web server details
- `BUILD_SUCCESS_WEB_SERVER.md` - Build notes
- `GREEK_STYLE_WEB_SERVER.md` - UI design guide

---

## 🏆 Vlastnosti Projektu

### Performance
- **Matrix Scan:** 50 Hz (20ms response time)
- **LED Refresh:** 60 FPS
- **Web Latency:** < 100ms
- **Move Validation:** < 1ms

### Memory Usage
- **Heap:** ~180 KB used / 512 KB total
- **Stack:** ~25 KB (vsechny tasky)
- **Flash:** ~2.1 MB / 4 MB

### Stability
- **Watchdog:** All tasks registered
- **Error Handling:** Comprehensive error recovery
- **Thread Safety:** Mutex-protected critical sections
- **Memory Safety:** No heap fragmentation

### Code Quality
- **Modular Design:** 20+ independent components
- **Clean API:** Well-defined interfaces
- **Documentation:** 100% Doxygen coverage
- **Error Handling:** Graceful degradation

---

## 🤝 Contributing

Projekt je v aktivnim vyvoji. Contributions vitany!

### Development Workflow
1. Fork repository
2. Create feature branch
3. Implement changes
4. Test thoroughly
5. Submit pull request

### Coding Standards
- **Language:** C17
- **Style:** ESP-IDF style guide
- **Comments:** Czech without diacritics
- **Documentation:** Doxygen format
- **Testing:** Manual + UART commands

---

## 📄 License

(Doplnit podle pozadavku)

---

## 🙏 Credits

- **ESP-IDF:** Espressif Systems
- **FreeRTOS:** Amazon Web Services
- **LED Strip Driver:** Espressif Component Registry
- **Chess Logic:** Custom implementation

---

## 📞 Kontakt

Alfred Krutina
- Project: Free Chess v1
- Platform: ESP32-C6
- Year: 2025

---

## 🔍 Technical Details

### Memory Architecture
```
SRAM Layout:
├── Instruction Cache: 16 KB
├── Data Cache: 16 KB  
├── Heap: ~400 KB
│   ├── Tasks Stacks: ~25 KB
│   ├── Queue Buffers: ~8 KB
│   ├── LED Buffers: ~1 KB
│   ├── Game State: ~4 KB
│   ├── Web Server: ~60 KB
│   └── Free: ~180 KB
└── Static: ~50 KB
```

### Task Priorities Rationale
- **Matrix (5)** - Highest, must not miss piece movements
- **Button (5)** - Same as matrix for synchronized scanning
- **LED (4)** - High for smooth 60 FPS
- **Game (3)** - Medium, event-driven
- **Web/UART (3)** - Medium, not time-critical
- **Animations (2)** - Low, can be delayed

### Power States
1. **Active Gaming** - Full brightness, all systems on
2. **Idle** - Screen saver, reduced brightness
3. **Deep Sleep** - (pripraven) WiFi off, matrix scan only

---

## 🎨 Design Philosophy

### User Experience
- **Intuitive:** LED feedback for every action
- **Forgiving:** Clear error messages with recovery hints
- **Responsive:** < 50ms latency for physical actions
- **Beautiful:** Smooth animations, professional look

### Technical Excellence
- **Robust:** Comprehensive error handling
- **Efficient:** Optimized memory and CPU usage
- **Maintainable:** Clean modular architecture
- **Documented:** Every function documented

### Future-Proof
- **Extensible:** Easy to add new features
- **Scalable:** Ready for more complex modes
- **Upgradeable:** OTA update ready
- **Compatible:** Standard protocols (HTTP, Matter)

---

## 📊 Statistics

- **Lines of Code:** ~25,000
- **Components:** 20
- **Functions:** ~500
- **UART Commands:** 50+
- **LED Animations:** 15+
- **Supported Games:** Standard chess + puzzles
- **Max Concurrent Animations:** 8
- **Max Web Clients:** 4

---

**Enjoy playing chess on your smart ESP32-C6 board! ♟️**
