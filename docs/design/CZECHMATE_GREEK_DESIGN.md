# 🏛️ CZECHMATE - Greek Temple Design

## Přehled

Web server byl kompletně přepracován v klasickém řeckém stylu inspirovaném logem projektu CZECHMATE s prvky řecké architektury.

## 🏛️ Design Features

### **Architektonické prvky:**

1. **Sloupy (Columns):**
   - Dva dekorativní sloupy po stranách boardu
   - Fluted design (vertikální žlábky)
   - Kapitel (trojúhelníkový vrchol)
   - Dark blue barva (`#2C3E50`)

2. **Oblouky (Arches):**
   - Clip-path polygon pro úhlové hrany
   - Pediment (trojúhelníková střecha) na sloupech
   - Geometric shapes pro klasický vzhled

3. **Frieze (Vlys):**
   - Repeating pattern na horní a dolní části
   - Alternující čtverce (`#2C3E50` ↔ `#34495E`)
   - Inspirováno řeckými vlysy

### **Barvy (Greek Temple Palette):**

- **Dark Blue:** `#2C3E50` (hlavní barva sloupů a bordů)
- **Medium Blue:** `#34495E` (alternující barva)
- **Dark Blue Shadow:** `#1A252F` (bordy a stíny)
- **Marble:** `#F5E6D3` (pozadí)
- **Gold:** `#D4A574` (akcenty)
- **Bronze:** `#8B7355` (doplňkové barvy)

### **Design prvky:**

1. **H1 Title:**
   - Dark blue background s gradientem
   - Frieze pattern nahoře a dole
   - Clip-path pro úhlové hrany
   - Velké písmo (3em)
   - Letter spacing (4px)
   - Text shadow s glow efektem

2. **Board Container:**
   - Clip-path polygon pro úhlové hrany
   - Frieze pattern nahoře a dole
   - Dark blue border (5px)
   - 3D shadow efekt

3. **Chess Board:**
   - Frieze pattern nahoře a dole
   - Dark blue border (4px)
   - Marble/bronze barvy polí
   - Inset shadow pro hloubku

4. **Info Panel:**
   - Matching design s board container
   - Frieze pattern nahoře a dole
   - Dark blue borders

5. **Status Boxes:**
   - Frieze pattern nahoře
   - Dark blue borders
   - Thick left border (6px)
   - Box shadow pro hloubku

6. **Columns:**
   - Fluted design (vertikální žlábky)
   - Kapitel (trojúhelníková střecha)
   - Dark blue barva
   - Inset shadow

### **CSS Highlights:**

```css
/* Frieze Pattern */
background: repeating-linear-gradient(90deg, 
    #2C3E50 0px, #2C3E50 10px, 
    #34495E 10px, #34495E 20px);

/* Clip-path pro úhlové hrany */
clip-path: polygon(0 5%, 5% 0, 95% 0, 100% 5%, 
                   100% 95%, 95% 100%, 5% 100%, 0 95%);

/* Fluted Column */
background: repeating-linear-gradient(180deg, 
    #2C3E50 0px, #2C3E50 15px, 
    #34495E 15px, #34495E 30px);

/* Column Kapitel */
clip-path: polygon(0 100%, 50% 0, 100% 100%);
```

## 🎨 Vizualizace

### **Layout:**
```
┌─────────────────────────────────────────┐
│         ⚔️ ⚖️ 🏛️                       │
│   ┌─────────────────────────────┐      │
│   │  🏛️ CZECHMATE 🏛️          │      │
│   └─────────────────────────────┘      │
│         ⚔️ ⚖️ 🏛️                       │
│                                         │
│  ┌──────────┐        ┌──────────┐     │
│  │  │       │        │       │  │     │
│  │  │ BOARD │        │  INFO │  │     │
│  │  │       │        │       │  │     │
│  └──────────┘        └──────────┘     │
│  ↑ ↑      ↑ ↑        ↑ ↑      ↑ ↑     │
│  │ │      │ │        │ │      │ │     │
│ Sloupy   Frieze     Frieze   Sloupy   │
└─────────────────────────────────────────┘
```

## 🏛️ Greek Architecture Elements

1. **Doric Columns:**
   - Fluted (žlábkované)
   - Kapitel (trojúhelníková střecha)
   - Base (základna)

2. **Pediment:**
   - Trojúhelníková střecha
   - Na sloupech

3. **Frieze:**
   - Alternující vzory
   - Na všech kontejnerech

4. **Entablature:**
   - Architrave (hlavní nosník)
   - Frieze
   - Cornice (římsa)

5. **Clip-path:**
   - Úhlové hrany
   - Geometric shapes

## 📱 Responsive Design

- **Desktop:** 2-column layout s sloupy
- **Mobile:** 1-column layout (sloupy skryté)
- **Font sizes:** VW units
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
- **Title:** 🏛️ CZECHMATE - ESP32

## ✨ Features

1. **Greek Temple Design** - Kompletní řecký styl
2. **Columns** - Dekorativní sloupy po stranách
3. **Frieze Patterns** - Alternující vzory
4. **Clip-path** - Úhlové hrany
5. **Dark Blue Palette** - Klasická řecká barva
6. **3D Effects** - Hloubka a stíny
7. **Responsive** - Adaptivní design

## 🎭 Design Philosophy

Web server je navržen jako **řecký chrám** kde se odehrává šachová bitva:
- **Columns** - Doric sloupy po stranách
- **Frieze** - Alternující vzory
- **Pediment** - Trojúhelníkové střechy
- **Dark Blue** - Klasická řecká barva
- **Marble** - Luxusní textury

---

**Status:** ✅ READY FOR TESTING

**Build:** ✅ SUCCESSFUL

**Style:** 🏛️ GREEK TEMPLE (CZECHMATE)

**Flash:** ⏳ READY

