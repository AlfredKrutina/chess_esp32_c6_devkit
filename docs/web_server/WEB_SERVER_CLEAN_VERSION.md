# 🏛️ CZECHMATE - Clean Web Variant k UART Rozhraní

## Přehled

Web server je **clean grafická varianta** k UART příkazovému rozhraní. Všechny funkce z UART jsou dostupné přes web interface s krásným řeckým designem.

## 🎨 Design

### **Řecký Chrám Styl:**
- **Dark Blue** (`#2C3E50`) - hlavní barva
- **Marble** (`#F5E6D3`) - pozadí
- **Gold** (`#D4A574`) - akcenty
- **Frieze patterns** - alternující vzory
- **Clip-path** - úhlové hrany

### **Responzivní Layout:**
- **Desktop:** 2-column (board + info panel)
- **Mobile:** 1-column (stacked)
- **Touch-friendly:** Velké klikací oblasti

## 📡 REST API Endpoints (ekvivalent UART příkazů)

### **1. GET /api/board**
**UART ekvivalent:** `BOARD`

Vrací aktuální stav boardu:
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

### **2. GET /api/status**
**UART ekvivalent:** `STATUS`

Vrací game status:
```json
{
  "game_state": "active",
  "current_player": "white",
  "move_count": 15,
  "piece_lifted": {
    "row": 6,
    "col": 4,
    "piece": "P"
  },
  "white_time": 1200,
  "black_time": 1180,
  "in_check": false,
  "checkmate": false,
  "stalemate": false
}
```

### **3. GET /api/history**
**UART ekvivalent:** `GAME_HISTORY`

Vrací historii tahů:
```json
{
  "moves": [
    {
      "from": "e2",
      "to": "e4",
      "piece": "P",
      "captured": null,
      "timestamp": 1234567890
    },
    {
      "from": "e7",
      "to": "e5",
      "piece": "p",
      "captured": null,
      "timestamp": 1234567891
    }
  ]
}
```

### **4. GET /api/captured**
**UART ekvivalent:** `CAPTURED`

Vrací captured pieces:
```json
{
  "white_captured": ["p","n"],
  "black_captured": ["P"]
}
```

### **5. POST /api/move**
**UART ekvivalent:** `MOVE e2 e4`

Provede tah:
```json
Request: {
  "from": "e2",
  "to": "e4"
}

Response: {
  "success": true,
  "from": "e2",
  "to": "e4"
}
```

## 🖥️ Web Interface Features

### **1. Real-time Board Display**
- 8x8 šachovnice s Unicode chess symbols
- Automatické obnovování každých 500ms
- Highlight pro zvednutou figurku (golden pulse)
- Marble/bronze barvy polí

### **2. Game Status Panel**
- **State:** active/checkmate/stalemate
- **Current Player:** white/black
- **Move Count:** počet tahů
- **In Check:** true/false

### **3. Lifted Piece Info**
- **Piece:** typ figury (P, N, B, R, Q, K)
- **Position:** souřadnice (např. e2)

### **4. Captured Pieces**
- **White Captured:** seznam zajatých bílých figur
- **Black Captured:** seznam zajatých černých figur

### **5. Move History**
- Scrollovatelný seznam všech tahů
- Formát: `from → to` (např. `e2 → e4`)
- Automatické přidávání nových tahů

## 🔄 Polling System

Web automaticky obnovuje data každých **500ms**:

```javascript
setInterval(() => {
    fetch('/api/board').then(...)
    fetch('/api/status').then(...)
    fetch('/api/history').then(...)
    fetch('/api/captured').then(...)
}, 500);
```

## 🎯 UART vs Web Comparison

| UART Příkaz | Web Endpoint | Popis |
|-------------|--------------|-------|
| `BOARD` | `GET /api/board` | Aktuální board |
| `STATUS` | `GET /api/status` | Game status |
| `GAME_HISTORY` | `GET /api/history` | Historie tahů |
| `CAPTURED` | `GET /api/captured` | Zajaté figury |
| `MOVE e2 e4` | `POST /api/move` | Provedení tahu |
| `UP e2` | (automatické) | Zvednutí figury |
| `DN e4` | (automatické) | Položení figury |

## 🚀 Build & Flash

```bash
cd /Users/alfred/Documents/my_local_projects/free_chess_v1
idf.py build
idf.py flash
```

## 🌐 Access

1. **WiFi:**
   - SSID: `ESP32-Chess`
   - Password: `12345678`

2. **Browser:**
   - URL: `http://192.168.4.1`
   - Captive portal: Automatické otevření

3. **API:**
   - Base URL: `http://192.168.4.1/api/`
   - Format: JSON
   - Method: GET/POST

## 📊 Features

✅ **Real-time Updates** - Polling každých 500ms  
✅ **Greek Temple Design** - Klasický řecký styl  
✅ **Responsive Layout** - Desktop/Mobile  
✅ **REST API** - JSON endpoints  
✅ **Captive Portal** - Auto-open  
✅ **Clean Interface** - Jednoduchý a přehledný  
✅ **Board Visualization** - Grafická šachovnice  
✅ **Move History** - Scrollovatelná historie  
✅ **Captured Pieces** - Zobrazení zajatých figur  

## 🔧 Technical Details

- **Stack Size:** 10KB (zvýšeno z 6KB)
- **WiFi:** AP mode, 4 max connections
- **HTTP Server:** Port 80, 4 concurrent connections
- **JSON Buffer:** 4KB static buffer
- **Polling Interval:** 500ms
- **Thread Safety:** Mutex pro game state

## 🎨 Design Elements

### **Board Container:**
- Frieze pattern nahoře a dole
- Clip-path pro úhlové hrany
- Dark blue border (5px)
- Marble gradient background

### **Chess Board:**
- 8x8 grid
- Light squares: `#F5E6D3` (marble)
- Dark squares: `#8B7355` (bronze)
- Dark blue border (4px)
- Frieze pattern nahoře a dole

### **Lifted Piece:**
- Golden gradient: `#D4A574 → #FFD700`
- Pulse animation
- Glowing effect

### **Info Panel:**
- Matching design s board container
- Frieze pattern nahoře a dole
- Dark blue borders
- Status boxes s frieze

## 📝 Notes

- **No external fonts** - Používá system fonts (Georgia, Courier New)
- **No external resources** - Vše embedded
- **Fast loading** - Žádné CDN závislosti
- **Clean code** - Jednoduchý a udržovatelný

---

**Status:** ✅ READY FOR TESTING

**Build:** ✅ SUCCESSFUL

**Style:** 🏛️ GREEK TEMPLE (CZECHMATE)

**Flash:** ⏳ READY

