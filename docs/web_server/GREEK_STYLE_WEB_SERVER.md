# 🏛️ Ancient Greek Style Web Server

## Přehled

Web server byl kompletně přepracován v antickém řeckém stylu s krásným vizuálním designem.

## 🎨 Design Features

### **Barvy (Ancient Greek Palette):**
- **Primary:** `#8B7355` (bronze)
- **Secondary:** `#D4A574` (gold)
- **Light:** `#F5E6D3` (marble)
- **Dark:** `#5D4E37` (dark wood)
- **Accent:** `#FFD700` (golden highlight)

### **Fonts:**
- **Heading:** `Cinzel` - elegantní serif font v řeckém stylu
- **Body:** `MedievalSharp` - dekorativní font pro hodnoty

### **Vizuální prvky:**

1. **Background:**
   - Gradient: `#8B7355 → #D4A574 → #F4E4BC`
   - Subtle pattern s řeckými vzory
   - Textura připomínající pergamen

2. **Board Container:**
   - Gradient background: `#F5E6D3 → #E8D5B7`
   - Golden border s gradient efektem
   - 3D shadow pro hloubku
   - Inset shadow pro reliéf

3. **Chess Board:**
   - Light squares: `#F5E6D3` (marble)
   - Dark squares: `#8B7355` (bronze)
   - Subtle temple emoji watermark (🏛️)
   - Dark wood border

4. **Lifted Piece:**
   - Golden gradient: `#D4A574 → #FFD700`
   - Glowing effect s animací
   - Pulse animation pro zvýraznění

5. **Info Panel:**
   - Matching gradient design
   - Golden borders
   - Inset shadows pro hloubku
   - Greek symbols v nadpisech

### **Greek Symbols:**
- 🏛️ - Temple (hlavní název)
- ⚔️ - Swords (Game Status)
- ⚡ - Lightning (Lifted Piece)
- 🛡️ - Shield (Captured Pieces)
- 📜 - Scroll (Move History)
- ⚖️ - Scales (dekorace)

## 🎯 HTML Structure

```html
<div class="container">
    <div class="greek-pattern">⚔️ ⚖️ 🏛️</div>
    <h1>🏛️ Ancient Chess Arena</h1>
    <div class="greek-pattern">⚔️ ⚖️ 🏛️</div>
    
    <div class="main-content">
        <div class="board-container">
            <div class="board">
                <!-- 8x8 grid -->
            </div>
        </div>
        
        <div class="info-panel">
            <div class="status-box">
                <h3>⚔️ Game Status</h3>
                <!-- Status items -->
            </div>
            
            <div class="status-box">
                <h3>⚡ Lifted Piece</h3>
                <!-- Lifted piece info -->
            </div>
            
            <div class="captured-box">
                <h3>🛡️ Captured Pieces</h3>
                <!-- Captured pieces -->
            </div>
            
            <div class="status-box">
                <h3>📜 Move History</h3>
                <!-- History -->
            </div>
        </div>
    </div>
</div>
```

## 🎨 CSS Highlights

### **Gradient Borders:**
```css
.board-container::before {
    content: '';
    position: absolute;
    background: linear-gradient(45deg, #D4A574, #8B7355, #D4A574);
    z-index: -1;
}
```

### **Golden Pulse Animation:**
```css
.square.lifted {
    background: linear-gradient(135deg, #D4A574 0%, #FFD700 100%);
    animation: pulse 1s infinite;
    box-shadow: 0 0 20px rgba(255,215,0,0.8);
}

@keyframes pulse {
    0%, 100% { 
        transform: scale(1); 
        box-shadow: 0 0 20px rgba(255,215,0,0.8); 
    }
    50% { 
        transform: scale(1.05); 
        box-shadow: 0 0 30px rgba(255,215,0,1); 
    }
}
```

### **3D Depth:**
```css
.board-container {
    box-shadow: 0 15px 40px rgba(0,0,0,0.4), 
                inset 0 2px 10px rgba(255,255,255,0.3);
}
```

## 📱 Responsive Design

- **Desktop:** 2-column layout (board + info panel)
- **Mobile:** 1-column layout (stacked)
- **Font sizes:** VW units pro škálování
- **Touch-friendly:** Velké klikací oblasti

## 🚀 Build & Flash

```bash
cd /Users/alfred/Documents/my_local_projects/free_chess_v1
idf.py build
idf.py flash
```

## 🌐 Access

- **SSID:** `ESP32-Chess`
- **Password:** `12345678`
- **URL:** `http://192.168.4.1`
- **Title:** 🏛️ Ancient Chess - ESP32

## ✨ Features

1. **Ancient Greek Theme** - Kompletní řecký design
2. **Golden Highlights** - Zlaté zvýraznění pro lifted pieces
3. **3D Effects** - Hloubka a stíny
4. **Greek Symbols** - Dekorativní emoji
5. **Responsive** - Adaptivní design
6. **Real-time Updates** - Polling každých 500ms
7. **Captive Portal** - Automatické otevření

## 🎭 Design Philosophy

Web server je navržen jako **antická aréna** kde se odehrává šachová bitva:
- **Bronze & Gold** - Řecké barvy
- **Marble textures** - Luxusní vzhled
- **Temple aesthetics** - Důstojnost a eleganci
- **Warrior symbols** - ⚔️ ⚖️ 🏛️

---

**Status:** ✅ READY FOR TESTING

**Build:** ✅ SUCCESSFUL

**Style:** 🏛️ ANCIENT GREEK

**Flash:** ⏳ READY

