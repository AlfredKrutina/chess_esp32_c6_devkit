# CZECHMATE iOS Aplikace — Komplexní návrh

> **Verze návrhu:** 1.0 | **Datum:** 2026-04-06  
> **Cílová platforma:** iOS 16+ (SwiftUI), budoucí Android přepis  
> **Inspirace:** chess.com + fyzická šachovnice integrace  
> **Související:** [CZECHMATE_MASTERPLAN.md](./CZECHMATE_MASTERPLAN.md) — postup implementace, Stockfish, web server

---

## 1. Vize aplikace

CZECHMATE iOS app je **kompletní průvodce fyzické šachové hry** — kombinuje eleganci chess.com s unikátní funkcí připojení k fyzické šachovnici (ESP32-C6). Aplikace funguje:

1. **S fyzickou šachovnicí** — sleduje a validuje fyzické tahy, ovládá LED, notifikuje o tahu bota
2. **Bez fyzické šachovnice** — samostatný šachový klient (hra přes obrazovku, analýza, puzzle)
3. **As a remote control** — dálkové ovládání šachovnice (jas LED, MQTT, nastavení)

---

## 2. Komunikační architektura

### 2.1 Primární: Bluetooth LE (BLE)

```
iOS App (CoreBluetooth)
    |
    +-- Scan & Connect --> ESP32-C6 BLE GATT Server
    |   [ble_task, priorita 3]
    |
    +-- Subscribe Notifications --> BOARD_STATE (64 bytes)
    |                           --> GAME_STATUS (JSON)
    |                           --> TIMER_STATE (8 bytes)
    |
    +-- Write Commands ------> MOVE_CMD ("e2e4")
                           --> CTRL_CMD ("NEW|HINT|RESET")
```

**Proč BLE:**
- Automatické párování a reconnect (jako sluchátka)
- Funguje bez WiFi routeru (portabilita, turnaje)
- Push notifikace v reálném čase
- Background mode — iOS příjme notifikaci i se zamčenou obrazovkou
- Stejné ESP32 API → snadný Android přepis

### 2.2 Sekundární: WiFi REST + WebSocket

```
iOS App (URLSession)
    |
    +-- REST API -----------> http://192.168.4.1/ (AP hotspot)
    |                     --> http://<IP>/ (domácí síť)
    |
    +-- WebSocket ----------> ws://192.168.4.1/ws (po implementaci)
```

**Kdy používat WiFi místo BLE:**
- Větší přenosy dat (download herní historie, firmware update)
- Nastavení WiFi/MQTT (přes webový view)
- Stockfish AI (vyžaduje internet, přes STA)

### 2.3 Discovery mechanismus

Aplikace v tomto pořadí zkusí připojení:

```
1. BLE scan --> "CZECHMATE" device --> connect (preferováno)
2. BLE cached device --> rychlý reconnect
3. WiFi multicast DNS (Bonjour) --> "czechmate.local"
4. Manuální IP zadání (fallback)
5. LAN scan pro port 80 (poslední možnost)
```

---

## 3. Hlavní obrazovky aplikace

### 3.1 Přehled obrazovek

```
TabBar:
├── [♟] Hra          -- aktivní šachová partie
├── [📊] Analýza      -- přehled partie, hodnocení tahů
├── [🧩] Puzzle       -- denní puzzle (Lichess API)
├── [🏅] Statistiky   -- historie her, ELO graf, achievements
└── [⚙️] Nastavení    -- šachovnice, HA, profil
```

### 3.2 Obrazovka: HRA (hlavní)

**Layout — portrait:**
```
+------------------------------------------+
|  [Nastavení]  CZECHMATE  [Hodinky ⏱]     |
|                                          |
|  Černý hráč: ♟ 3 min 45 s               |
|  Sebrané figurky: ♗♙♙                  |
|                                          |
|  +------------------------------------+  |
|  |                                    |  |
|  |        ŠACHOVNICE 3D               |  |
|  |        8×8 pole                    |  |
|  |        Figurky s animacemi         |  |
|  |        LED shoda (barvy dle ESP32) |  |
|  |                                    |  |
|  +------------------------------------+  |
|                                          |
|  Bílý hráč: ♔ 5 min 22 s               |
|  Sebrané figurky: ♟♙                   |
|                                          |
|  Tah: [e2 → e4] [Nápověda 💡] [⟲ Undo] |
|                                          |
|  Hodnocení tahu: ✅ Výborný tah!        |
+------------------------------------------+
```

**Klíčové prvky:**
- **3D nebo 2D šachovnice** s možností přepnutí (nastavení)
- **Animace tahů** — figurka se "přesune" po desce
- **LED synchronizace** — barvy na obrazovce odpovídají fyzickým LED
  - 🟢 Zelená = dostupné tahy
  - 🔴 Červená = neplatný tah, šach
  - 🔵 Modrá = nápověda, vybraná figurka
  - 🟡 Žlutá = poslední tah
- **Fyzický tah overlay** — když hráč zvedne figurku na šachovnici, iOS zobrazí dostupné cíle
- **Bot tah navigace** — LED šipka na fyzické desce + animace na obrazovce

### 3.3 Obrazovka: ANALÝZA

```
+------------------------------------------+
|  Analýza partie                     [X]  |
|                                          |
|  Partita: Bílý vs. Černý               |
|  Výsledek: Bílý vyhrál — Šachmat!      |
|  Počet tahů: 32                         |
|                                          |
|  Průměrná přesnost: ●●●●○ 4.2/5        |
|  Bílý: 89% | Černý: 76%               |
|                                          |
|  Graf výhody: [══════════════════]      |
|               Bílý          Černý       |
|                                          |
|  Historie tahů:                         |
|  1. e4 ✅ Výborný | e5 🟡 Slabší       |
|  2. Nf3 ✅ Dobrý  | Nc6 ✅ Dobrý       |
|  ...                                    |
|                                          |
|  [Zobrazit na šachovnici] [Sdílet PGN] |
+------------------------------------------+
```

### 3.4 Obrazovka: PUZZLE

```
+------------------------------------------+
|  Denní puzzle            Série: 🔥 7 dní |
|                                          |
|  Bílý hraje a dá mat ve 2 tahech        |
|                                          |
|  +------------------------------------+  |
|  |        PUZZLE ŠACHOVNICE           |  |
|  |        (interaktivní)              |  |
|  +------------------------------------+  |
|                                          |
|  Nápověda: [Zobrazit] [Přeskočit]       |
|  Rating: ★★★☆☆ (1450 Elo)             |
|                                          |
|  Puzzle zdroj: Lichess Open Database    |
|                                          |
|  [Poslat výzvu na fyzickou desku]        |
+------------------------------------------+
```

### 3.5 Obrazovka: NASTAVENÍ šachovnice

```
+------------------------------------------+
|  Fyzická šachovnice                       |
|                                          |
|  Připojení:                              |
|  ● BLE: CZECHMATE-A1B2 [Odpojit]        |
|  ○ WiFi: 192.168.4.1 [Připojit]        |
|                                          |
|  LED nastavení:                          |
|  Jas: [══════════════░░] 75%            |
|  Téma barev: [Výchozí ▾]               |
|                                          |
|  Herní nastavení:                        |
|  Zhodnocení tahů: [ON]                  |
|  Nápověda (limit): [3 na hráče ▾]       |
|  Chess hodiny: [10 min + 5 s ▾]         |
|  Hráč vs Bot: [ELO 1200 ▾]            |
|                                          |
|  Home Assistant:                         |
|  MQTT broker: homeassistant.local       |
|  Auto HA mód: [5 minut ▾]             |
|                                          |
|  WiFi šachovnice:                        |
|  SSID domácí sítě: Moje_WiFi           |
|  [Nastavit WiFi přes šachovnici]       |
+------------------------------------------+
```

---

## 4. Klíčové funkce

### 4.1 Fyzická šachovnice — funkcionalita

| Funkce | Popis |
|--------|-------|
| **Live board sync** | Obrazovka v reálném čase kopíruje fyzický stav šachovnice |
| **Physically guided moves** | Zvedni figurku → iOS zobrazí validní cíle → Polož → tah hotov |
| **Bot led guidance** | Bot zvolí tah → LED na fyzické desce zobrazí od-kam-kam, iOS zobrazí animaci |
| **Check warning** | Šach → červené LED + iOS notification + vibrace |
| **Endgame animation** | Mat → vítězná LED animace + confetti na iOS |
| **Setup tutorial** | LED navigace při kladení figurek na začátku hry |
| **Puzzle on board** | Denní puzzle přenesen na fyzickou šachovnici s LED guide |

### 4.2 Herní módy

| Mód | Popis |
|-----|-------|
| **Hráč vs Hráč (fyzicky)** | Oba hráči u fyzické šachovnice, iOS jen zobrazuje |
| **Hráč vs Hráč (online)** | Vzdálení hráči přes server (vyžaduje backend) |
| **Hráč vs Bot** | Stockfish na nastavitelné ELO, tah bota = LED navigace na fyzické desce |
| **Analýza** | Přehrávání party s hodnocením tahů |
| **Sandbox** | Volné experimentování s tahy (neohrožuje hru) |
| **Puzzle mód** | Lichess puzzle databáze, denní výzva |
| **Výuka** | Interaktivní lekce s LED výukou na fyzické desce |

### 4.3 Nápověda a analýza (Stockfish)

- **Hint button** — zobrazí nejlepší tah (LED na desce + šipka na obrazovce)
- **After-move evaluation** — hodnocení každého tahu po zahrání
  - ✅ Best / Good (zelená)
  - 🟡 Inaccuracy (žlutá)
  - 🟠 Mistake (oranžová)
  - 🔴 Blunder (červená)
- **System odměn** — za Best/Good tah hráč získá nápovědu navíc
- **Depth nastavení** — hloubka analýzy Stockfishe (1-20)

*Stockfish API: chess-api.com (stejná jako v existujícím web řešení)*

### 4.4 Home Assistant integrace (přes MQTT)

- **HA Light entita** — šachovnice jako RGB světlo v Home Assistant
- **Lamp mode** — iOS app přepne šachovnici do modu lampy
- **RGB picker** — výběr barvy lampy přímo z iOS
- **Auto timeout** — nastavení doby neaktivity pro přepnutí do lamp módu
- **HA dashboard** — zobrazení stavu v Apple Home (přes Matter v budoucnu)

### 4.5 Statistiky a gamifikace

- **Elo tracking** — vlastní elo pro hru na fyzické šachovnici
- **Win/loss/draw statistiky**
- **Streak** — série denních puzzle (🔥 motivace)
- **Achievements** — odznaky (první vítězství, 100 tahů, atd.)
- **Move accuracy** — průměrná přesnost za posledních 10 her
- **Opening repertoire** — jaká zahájení hráč používá
- **Heat map** — která pole hráč nejvíce využívá

---

## 5. Technická architektura iOS aplikace

### 5.1 SwiftUI architektura

```
CZECHMATEApp
├── AppState (ObservableObject)
│   ├── gameState: GameState
│   ├── boardState: BoardState  
│   ├── connectionState: ConnectionState
│   └── settingsState: SettingsState
│
├── Services
│   ├── BLEService (CoreBluetooth)
│   │   ├── ChessboardPeripheral
│   │   ├── GATTCharacteristicManager
│   │   └── BLEConnectionManager
│   │
│   ├── NetworkService (URLSession)
│   │   ├── ChessAPIClient (chess-api.com)
│   │   ├── PuzzleAPIClient (Lichess)
│   │   └── ESP32HTTPClient
│   │
│   ├── WebSocketService (URLSessionWebSocketTask)
│   │   └── ESP32WebSocketClient
│   │
│   └── StorageService
│       ├── CoreData (herní historie)
│       └── UserDefaults (nastavení)
│
└── Views
    ├── GameView          -- hlavní herní obrazovka
    │   ├── ChessBoardView    -- interaktivní šachovnice
    │   ├── PlayerInfoView    -- info o hráčích
    │   ├── MoveHistoryView   -- história tahů
    │   └── ControlsView      -- tlačítka
    ├── AnalysisView      -- analýza party
    ├── PuzzleView        -- denní puzzle
    ├── StatisticsView    -- statistiky
    └── SettingsView      -- nastavení
```

### 5.2 BLE GATT Client (CoreBluetooth)

```swift
// Návrh BLE Service UUIDs
let CZECHMATE_SERVICE_UUID = CBUUID(string: "1234-CZECH-MATE-CHESS-SERVICE")
let BOARD_STATE_UUID      = CBUUID(string: "...")  // notify+read, 64 bytes
let GAME_STATUS_UUID      = CBUUID(string: "...")  // notify+read, JSON
let MOVE_CMD_UUID          = CBUUID(string: "...")  // write, 4 bytes
let CTRL_CMD_UUID          = CBUUID(string: "...")  // write, string
let TIMER_STATE_UUID       = CBUUID(string: "...")  // notify, 8 bytes
let SETTINGS_UUID          = CBUUID(string: "...")  // read+write

// Lifecycle
class BLEService: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    func startScanning() -> AnyPublisher<ChessboardPeripheral, Error>
    func connect(peripheral: CBPeripheral)
    func sendMove(from: String, to: String) // "e2", "e4"
    func sendControl(command: ControlCommand) // .newGame, .hint, .reset
    var boardStatePublisher: AnyPublisher<BoardState, Never>
    var gameStatusPublisher: AnyPublisher<GameStatus, Never>
    var timerStatePublisher: AnyPublisher<TimerState, Never>
}
```

### 5.3 Šachovnice UI (SwiftUI)

```swift
struct ChessBoardView: View {
    @ObservedObject var game: GameViewModel
    @State private var selectedSquare: Square?
    @State private var highlightedSquares: [Square: HighlightType] = [:]
    
    // Gestures: tap pro výběr, drag & drop pro pohyb
    // Animace: figurky se pohybují s spring animation
    // LED synchronizace: barvy odpovídají fyzickým LED
    // 3D effect: perspektiva a stíny (SceneKit nebo vlastní rendering)
}

enum HighlightType {
    case selected       // modrá - vybraná figurka
    case validMove     // zelená - dostupný cíl
    case lastMove      // žlutá - poslední tah
    case check         // červená - šach
    case hint          // fialová - Stockfish nápověda
    case ledSync       // barva z ESP32 LED
}
```

### 5.4 Data modely

```swift
// Stav šachovnice (zrcadlo ESP32 JSON)
struct BoardState: Codable {
    let board: [[ChessPiece?]]  // 8x8
    let timestamp: Date
}

// Stav hry (zrcadlo /api/status)
struct GameStatus: Codable {
    let gameState: GameState    // active, promotion, ended
    let currentPlayer: Player   // white, black
    let moveCount: Int
    let inCheck: Bool
    let gameEnd: GameEnd?
    let internetConnected: Bool
    let webLocked: Bool
    let isDemo: Bool
    let brightness: Int
    let haMode: String
}

// Herní příkaz
enum GameCommand {
    case move(from: String, to: String)
    case newGame
    case reset
    case hint
    case puzzle(config: PuzzleConfig)
    case setTimer(white: Int, black: Int, increment: Int)
    case setBrightness(Int)
    case setHAColor(r: Int, g: Int, b: Int)
}

// Typ figurky
enum ChessPiece: String, Codable {
    case wK, wQ, wR, wB, wN, wP  // bílé
    case bK, bQ, bR, bB, bN, bP  // černé
}
```

### 5.5 Offline/Online synchronizace

```
App spuštěna
    |
    +-- BLE dostupné? --> Připoj se, subscribe notifications
    |       |
    |       v
    |   BLE connected --> Primární zdroj dat
    |
    +-- WiFi dostupné? --> Fallback REST polling (1s)
    |
    +-- Offline --> Zobraz cached stav, puzzle data z CoreData
```

---

## 6. Design systém

### 6.1 Vizuální identita

**Barevná paleta:**
- Primární: `#1a1a2e` (tmavě modrá — elegantní, moderní)
- Akcent: `#e94560` (červená — šach, mat, nebezpečí)
- Zlatá: `#f0a500` (trofeje, achievements)
- Zelená: `#00b894` (validní tahy, dobré tahy)
- Pozadí: `#16213e` (dark mode, přátelský pro oči)
- Text: `#eaeaea` (bílošedá pro dark mode)

**Typografie:**
- Heading: SF Pro Rounded (Apple nativní, moderní)
- Body: SF Pro Text
- Monospace (notace tahů): SF Mono

**Vyznění:** Chess.com + Apple design guidelines — elegantní, minimalistický, tmavý

### 6.2 Šachovnicový design

**Figurky:** 
- Možnost výběru z sad: Classic, Neo, 3D Wood, Modern Flat
- Plynulé animace pohybu (spring physics)
- Figurky vrhají stíny (shadow rendering)

**Deska:**
- 2D variant: klasická zelená nebo moderní modrá/béžová
- 3D variant: dřevěná textura s perspektivou
- Orientace: bílý dole (default), možnost otočení

**LED overlay:**
- Pole šachovnice na obrazovce dostávají glowový efekt odpovídající fyzickým LED
- Zelené buňky = validní tahy, červené = šach, modré = nápověda

### 6.3 Animace a micro-interactions

- **Výběr figurky:** Jemné zdvihnutí (scale up 1.1×) + shadow
- **Pohyb figurky:** Spring animation s odeznívajícím pohybem
- **Tah bota:** Slide animace figurky po desce
- **Šach:** Červený puls king pole + vibrace haptic feedback
- **Endgame:** Confetti + vítězný sound
- **BLE connect:** Loading pulse animation + "Šachovnice připojena! ✓"
- **Push notification (mat):** Banner + custom sound

---

## 7. Onboarding

### 7.1 První spuštění

```
Slide 1: "Vítej v CZECHMATE"
  → Logo animace + tagline

Slide 2: "Připoj fyzickou šachovnici"
  → Animace BLE párování
  → "Zapni šachovnici a drž ji blízko"
  → [Spárovat] nebo [Přeskočit — hraj virtuálně]

Slide 3: "Nastav WiFi šachovnice"
  → WiFi manager (přes hotspot 192.168.4.1)
  → Nebo přeskočit

Slide 4: "Připraven na hru!"
  → Stručný přehled funkcí
  → [Začít hrát]
```

### 7.2 Contextual help

- Při prvním otevření Analýzy → tooltip co hodnocení tahů znamenají
- Při prvním puzzle → animovaná ukázka interakce
- Při prvním BLE odpojení → instrukce jak znovu připojit

---

## 8. Push notifikace a background

### 8.1 Typy notifikací

| Notifikace | Trigger | Způsob |
|-----------|---------|--------|
| Tah bota hotov | After bot move | BLE notify → local notification |
| Šach! | game_status.in_check | BLE notify → vibrace + banner |
| Hra skončila | game_end.ended | BLE notify → modal v app |
| Denní puzzle | 9:00 ráno | Scheduled local notification |
| Streak připomenutí | 20:00, pokud nerozehrán | Conditional local notification |
| Šachovnice připojena | BLE reconnect | Local notification |

### 8.2 Background BLE (CoreBluetooth)

iOS umožňuje BLE notifikace i v pozadí s `bluetooth-central` background mode:
```xml
<!-- Info.plist -->
<key>UIBackgroundModes</key>
<array>
    <string>bluetooth-central</string>
</array>
```

Toto umožní příjem herních eventů (tah bota, šach) i se zavřenou aplikací.

---

## 9. Přístupnost

- **VoiceOver:** Každé pole šachovnice má accessibility label ("Bílý král na e1")
- **Dynamic Type:** Velikost písma se přizpůsobuje systémovému nastavení
- **High Contrast:** Tmavý režim s vysokým kontrastem pro slabozraké
- **Haptic feedback:** Potvrzení tahu, šach, validace pohybu
- **Sound design:** Zvuky tahů, šachu, konce hry (zapínat/vypínat)

---

## 10. Monetizace a distribuce

### 10.1 Distribuce (plán)

- **Fáze 1:** TestFlight (beta testování)
- **Fáze 2:** App Store (zdarma, možná freemium)

### 10.2 Freemium model (návrh)

**Zdarma:**
- Hra s fyzickou šachovnicí (plná funkcionalita)
- 5 denních puzzle
- Základní statistiky
- Stockfish nápověda (5× za hru)

**Premium:**
- Neomezené puzzle
- Detailní analýza s grafem
- Neomezena nápověda
- Prémiové sady figurek
- Exportovat PGN/FEN
- Elo tracking a leaderboard

---

## 11. Roadmapa

### v1.0 — MVP (iOS monolit, WiFi REST)
- Základní šachovnice UI
- WiFi REST pripojení (polling)
- Fyzická šachovnice sync (board state)
- Nová hra, reset
- Bot support (Stockfish)

### v1.5 — Realtime (WebSocket)
- WebSocket připojení (po implementaci na ESP32)
- Push events (konec hry, šach)
- Animace tahů

### v2.0 — BLE First
- BLE GATT připojení jako primární
- Background background mode
- Push notifikace (BLE)
- Onboarding s párováním

### v2.5 — Gamifikace
- Statistiky a Elo
- Achievements
- Streak systém

### v3.0 — Social & Online
- Online multiplayer (backend potřeba)
- Sdílení partií (PGN)
- Leaderboard
- Android port

---

## 12. Backend (budoucnost)

Pro online multiplayer a cloudové statistiky bude potřeba backend:
- **Framework:** Swift Vapor nebo Node.js + Fastify
- **Database:** PostgreSQL (herní historie, uživatelé)
- **Real-time:** WebSocket server pro online hru
- **API:** RESTful + WebSocket
- **Auth:** Apple Sign In (nejjednodušší pro iOS uživatele)
- **Firebase:** Push notifikace (APNs přes FCM)

---

## 13. Srovnání s chess.com

| Funkce | chess.com | CZECHMATE |
|--------|-----------|-----------|
| Online multiplayer | ✅ | Budoucnost |
| Offline hra | ✅ | ✅ |
| Fyzická šachovnice | ❌ | ✅ |
| LED feedback | ❌ | ✅ |
| BLE připojení | ❌ | ✅ |
| Home Assistant | ❌ | ✅ |
| Denní puzzle | ✅ | ✅ (Lichess API) |
| Stockfish analýza | ✅ | ✅ |
| Elo tracking | ✅ | Plánováno |
| Dark mode | ✅ | ✅ |
| Apple Watch | ✅ | Budoucnost |

**Naše výhoda:** Fyzická šachovnice integrace — unikátní kombinace digitálního a fyzického herního zážitku.

---

## 14. ESP32-C6 změny potřebné pro iOS app

### 14.1 Fáze 1 (bez změn firmware)

- Žádné změny ESP32 kódu
- iOS app polluje REST API přes WiFi

### 14.2 Fáze 2 (WebSocket — malé změny)

Přidat WebSocket handler do existujícího HTTP serveru:
```c
// web_server_task.c — přidat handler
static esp_err_t http_get_websocket_handler(httpd_req_t *req) {
    // Upgrade connection to WebSocket
    // Register client in list
    // Send updates on game state changes
}

// Volat ze game_task při každé změně stavu:
void ws_broadcast_game_state(void) {
    // Send JSON to all WebSocket clients
}
```

### 14.3 Fáze 3 (BLE — nový task)

Přidat nový `ble_task` (priorita 3, stack 8KB):
```c
// ble_task.c — nový soubor
void ble_task_start(void *pvParameters) {
    // Init NimBLE nebo Bluedroid
    // Register GATT service/characteristics
    // Handle iOS connect/disconnect
    // Subscribe to game state changes
    while (1) {
        // Send board state via GATT notification
        // Handle incoming MOVE_CMD
        // Handle incoming CTRL_CMD
    }
}
```

**GATT Characteristics:**
```
BOARD_STATE  (notify): 64 bytes — 1 byte per square, piece encoding
GAME_STATUS  (notify): JSON string — game_state, player, in_check, etc.
TIMER_STATE  (notify): 8 bytes — white_ms (4 bytes) + black_ms (4 bytes)
MOVE_CMD    (write):  5 bytes — "e2e4\0"
CTRL_CMD    (write):  variable — "NEW", "RESET", "HINT", "PAUSE"
SETTINGS    (r/w):    JSON string — brightness, timer_config, ha_config
```

**Piece encoding (1 byte per square):**
```
0x00 = empty
0x01-0x06 = white K,Q,R,B,N,P
0x07-0x0C = black K,Q,R,B,N,P
```

---

*Návrh aplikace CZECHMATE iOS v1.0 — vytvořen na základě analýzy existujícího projektu.*
