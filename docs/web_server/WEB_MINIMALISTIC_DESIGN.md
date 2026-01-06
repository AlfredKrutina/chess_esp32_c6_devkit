# ♟️ ESP32 Chess - Minimalistický Web Design

## ✅ Nový Clean Design

Web byl kompletně přepracován na **minimalistický, praktický a intuitivní** design.

### **Design Principy:**

✅ **Dark Theme** - Tmavé pozadí pro pohodlné sledování  
✅ **Minimalismus** - Žádné zbytečné dekorace  
✅ **Kontrast** - Zelené akcenty (#4CAF50) pro důležité info  
✅ **Čitelnost** - System fonts, jasné barvy  
✅ **Praktičnost** - Všechny důležité informace na jednom místě  

---

## 🎨 Barevná Paleta

### **Pozadí:**
- **Dark Background:** `#1a1a1a` - Hlavní pozadí
- **Container:** `#2a2a2a` - Board a info panel kontejnery
- **Boxes:** `#333` - Status boxy

### **Akcenty:**
- **Primary Green:** `#4CAF50` - Nadpisy, zvýraznění
- **Text:** `#e0e0e0` - Hlavní text
- **Secondary Text:** `#aaa` - Sekundární text
- **Disabled:** `#888` - Neaktivní prvky

### **Šachovnice:**
- **Light Squares:** `#f0d9b5` - Světlá pole
- **Dark Squares:** `#b58863` - Tmavá pole
- **Lifted Piece:** `#4CAF50` - Zelené zvýraznění zvednuté figury

---

## 📐 Layout

### **Desktop (min-width: 769px):**
```
┌─────────────────────────────────────────────────────────┐
│                  ♟️ ESP32 Chess                         │
├──────────────────────────┬──────────────────────────────┤
│                          │  Game Status                 │
│                          │  ├─ State: active            │
│                          │  ├─ Player: white            │
│                          │  ├─ Moves: 15                │
│                          │  └─ In Check: No             │
│                          │                              │
│                          │  Lifted Piece                │
│                          │  ├─ Piece: P                 │
│                          │  └─ Position: e2             │
│                          │                              │
│                          │  Captured Pieces             │
│                          │  ├─ White: ♟ ♞              │
│                          │  └─ Black: ♙                │
│                          │                              │
│                          │  Move History                │
│                          │  ├─ e2 → e4                 │
│                          │  ├─ e7 → e5                 │
│                          │  └─ ...                     │
└──────────────────────────┴──────────────────────────────┘
```

### **Mobile (max-width: 768px):**
```
┌─────────────────────────┐
│   ♟️ ESP32 Chess        │
├─────────────────────────┤
│   [Chess Board 8x8]     │
├─────────────────────────┤
│   Game Status           │
│   ├─ State: active      │
│   ├─ Player: white      │
│   ├─ Moves: 15          │
│   └─ In Check: No       │
├─────────────────────────┤
│   Lifted Piece          │
│   ├─ Piece: P           │
│   └─ Position: e2       │
├─────────────────────────┤
│   Captured Pieces       │
│   ├─ White: ♟ ♞        │
│   └─ Black: ♙          │
├─────────────────────────┤
│   Move History          │
│   ├─ e2 → e4           │
│   ├─ e7 → e5           │
│   └─ ...               │
└─────────────────────────┘
```

---

## 🧩 Komponenty

### **1. Header**
```css
h1 {
    color: #4CAF50;
    text-align: center;
    margin-bottom: 20px;
    font-size: 1.5em;
    font-weight: 600;
}
```
- **Text:** "♟️ ESP32 Chess"
- **Barva:** Zelená (#4CAF50)
- **Velikost:** 1.5em

### **2. Chess Board**
```css
.board {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
    grid-template-rows: repeat(8, 1fr);
    border: 2px solid #3a3a3a;
    border-radius: 4px;
    overflow: hidden;
}
```

**Squares:**
- **Light:** `#f0d9b5`
- **Dark:** `#b58863`
- **Hover:** `#4a4a4a`
- **Lifted:** `#4CAF50` (green highlight)

### **3. Status Boxes**
```css
.status-box {
    background: #333;
    border-left: 3px solid #4CAF50;
    padding: 12px;
    margin-bottom: 10px;
    border-radius: 4px;
}
```

**Obsahuje:**
- Game Status (State, Player, Moves, In Check)
- Lifted Piece (Piece, Position)
- Captured Pieces (White/Black)
- Move History (scrollable)

### **4. Status Items**
```css
.status-item {
    display: flex;
    justify-content: space-between;
    margin: 4px 0;
    font-size: 13px;
}
```

**Formát:**
```
Label: Value
```

### **5. Move History**
```css
.history-box {
    max-height: 150px;
    overflow-y: auto;
    background: #333;
    padding: 8px;
    border-radius: 4px;
}
```

**Formát:**
```
from → to
```

**Scrollbar:**
- Width: 6px
- Track: `#2a2a2a`
- Thumb: `#4CAF50`

### **6. Captured Pieces**
```css
.captured-piece {
    font-size: 1.2em;
    color: #888;
}
```

**Zobrazení:**
- Unicode chess symbols
- Gray color (#888)
- Flex layout s wrap

---

## 🔄 Real-time Updates

### **Polling Interval:**
```javascript
setInterval(() => {
    fetch('/api/board').then(...)
    fetch('/api/status').then(...)
    fetch('/api/history').then(...)
    fetch('/api/captured').then(...)
}, 500); // 500ms
```

### **Aktualizace:**
- Board state
- Game status
- Lifted piece
- Move history
- Captured pieces

---

## 📱 Responsive Breakpoints

### **Desktop (769px+):**
- Grid layout: `1fr 280px`
- Board: Full width
- Info panel: 280px

### **Mobile (≤768px):**
- Flex layout: Column
- Board: 100% width
- Info panel: 100% width

---

## 🎯 Uživatelské Rozhraní

### **Interaktivita:**
- **Hover:** Zvýraznění polí při najetí myší
- **Lifted Piece:** Zelené zvýraznění zvednuté figury
- **Smooth Transitions:** 0.15s pro všechny přechody

### **Čitelnost:**
- **System Fonts:** -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto
- **Monospace:** Courier New pro hodnoty
- **Font Sizes:**
  - Header: 1.5em
  - Status boxes: 0.9em
  - Status items: 13px
  - History: 11px
  - Captured: 0.75em

### **Kontrast:**
- **High Contrast:** Tmavé pozadí + světlý text
- **Green Accents:** #4CAF50 pro důležité prvky
- **Gray Secondary:** #888 pro méně důležité prvky

---

## 🚀 Výhody Nového Designu

✅ **Rychlé načítání** - Žádné externí fonty, jednoduchý CSS  
✅ **Nízká paměť** - Minimalistický design, menší HTML  
✅ **Čitelnost** - Dark theme, high contrast  
✅ **Praktičnost** - Všechny důležité info na jednom místě  
✅ **Intuitivita** - Jasné oddělení sekcí, logické uspořádání  
✅ **Responzivita** - Funguje na všech zařízeních  
✅ **Moderní** - Clean, minimalistický vzhled  

---

## 📊 Porovnání Starý vs Nový

| **Aspekt** | **Starý (Greek)** | **Nový (Minimal)** |
|------------|-------------------|-------------------|
| **Pozadí** | Gradient + pattern | Jednoduché tmavé |
| **Dekorace** | Sloupy, frieze | Žádné |
| **Fonty** | Cinzel, MedievalSharp | System fonts |
| **Barvy** | Marble, Bronze, Gold | Dark + Green |
| **Velikost HTML** | ~15KB | ~8KB |
| **Načítání** | Pomalé (Google Fonts) | Rychlé |
| **Čitelnost** | Střední | Vysoká |
| **Praktičnost** | Dekorativní | Funkční |

---

## 🔧 Build Status

✅ **Build:** SUCCESSFUL  
✅ **Flash:** READY  
✅ **Testing:** READY  

---

## 📝 Poznámky

- **Žádné externí závislosti** - Vše embedded
- **System fonts** - Rychlé načítání
- **Dark theme** - Oko-šetrný
- **Green accents** - Jasné zvýraznění
- **Minimalistický** - Žádné zbytečnosti

---

**Status:** ✅ READY FOR TESTING

**Design:** ♟️ MINIMALISTIC & PRACTICAL

**Flash:** ⏳ READY

