# ESP32-C6 DevKitM-1 PIN ANALÝZA

## 📌 POUŽITÉ PINY V PROJEKTU:

### **LED System:**
- **GPIO7**: WS2812B LED data (73 LED: 64 board + 9 buttons)
- **GPIO5**: Status LED indikátor (opraveno z GPIO8 - strapping pin)

### **Matrix Rows (8 výstupů):**
- **GPIO10**: MATRIX_ROW_0
- **GPIO11**: MATRIX_ROW_1
- **GPIO18**: MATRIX_ROW_2
- **GPIO19**: MATRIX_ROW_3
- **GPIO20**: MATRIX_ROW_4
- **GPIO21**: MATRIX_ROW_5
- **GPIO22**: MATRIX_ROW_6
- **GPIO23**: MATRIX_ROW_7

### **Matrix Columns (8 vstupů s pull-up):**
- **GPIO0**: MATRIX_COL_0 + BUTTON_QUEEN
- **GPIO1**: MATRIX_COL_1 + BUTTON_ROOK
- **GPIO2**: MATRIX_COL_2 + BUTTON_BISHOP
- **GPIO3**: MATRIX_COL_3 + BUTTON_KNIGHT
- **GPIO6**: MATRIX_COL_4
- **GPIO4**: MATRIX_COL_5 (opraveno z GPIO9 - strapping pin)
- **GPIO16**: MATRIX_COL_6
- **GPIO17**: MATRIX_COL_7

### **Buttons:**
- **GPIO15**: BUTTON_RESET (samostatný pin, opraveno z GPIO27)

### **Celkem použitých pinů:**
- LED: 2 piny (GPIO7, GPIO5)
- Matrix rows: 8 pinů (GPIO10,11,18,19,20,21,22,23)
- Matrix columns: 8 pinů (GPIO0,1,2,3,6,4,16,17)
- Reset button: 1 pin (GPIO15)
- **CELKEM: 19 GPIO pinů**

---

## ⚠️ ESP32-C6 STRAPPING PINS (KRITICKÉ!):

ESP32-C6 má tyto strapping pins které ovlivňují boot mode:

### **GPIO8** - Boot Mode Select
- **High během boot**: SPI boot mode
- **Low během boot**: Download boot mode (UART/USB)
- ❌ **POUŽÍVÁME PRO STATUS LED!**
- ⚠️ **MOŽNÝ PROBLÉM:** Pokud je status LED aktivní (LOW) během resetu → může přepnout do download mode!

### **GPIO9** - Boot Mode Select  
- **High během boot**: Normal boot
- **Low během boot**: Download boot
- ✅ **NEPOUŽÍVÁME** - správně změněno na GPIO4

### **GPIO15** - ROM Messages Print Level
- **High**: ROM boot messages enabled
- **Low**: ROM boot messages disabled  
- ✅ **NEPOUŽÍVÁME**

### **GPIO4** - JTAG Mode / Boot Mode
- Slouží k různým boot configurations
- ✅ **POUŽÍVÁME** - MATRIX_COL_5 (bezpečné po bootu)

---

## 🔍 ANALÝZA KAŽDÉHO POUŽITÉHO PINU:

### ✅ **BEZPEČNÉ PINY (No restrictions):**
- GPIO0, GPIO1, GPIO2, GPIO3 - Bezpečné ✅
- GPIO6 - Bezpečný ✅
- GPIO7 - Bezpečný (RMT podporuje LED) ✅
- GPIO10, GPIO11 - Bezpečné ✅
- GPIO4, GPIO5 - Bezpečné ✅ (strapping piny, ale bezpečné po bootu)
- GPIO14 - NEPOUŽÍVÁME ✅
- GPIO16, GPIO17 - Bezpečné ✅
- GPIO18, GPIO19, GPIO20, GPIO21, GPIO22, GPIO23 - Bezpečné ✅
- GPIO15 - Bezpečný ✅ (strapping pin, ale bezpečný po bootu)

### ✅ **VŠECHNY PINY BEZPEČNÉ:**
- **GPIO5** - STATUS LED (opraveno z GPIO8)
  - Strapping pin, ale bezpečný po bootu
  - ✅ Problém vyřešen změnou z GPIO8 na GPIO5

---

## 🎯 ESP32-C6 DEVKITM-1 DOSTUPNÉ PINY:

ESP32-C6 má **30 GPIO pinů** celkem (GPIO0-GPIO30).

### **Vyhrazené/nedostupné:**
- GPIO12, GPIO13 - Flash/PSRAM (SPI)
- GPIO24, GPIO25, GPIO26 - USB D+/D-/VBUS (USB Serial/JTAG)
- GPIO28, GPIO29, GPIO30 - Reserved

### **Dostupné GPIO:**
GPIO0-GPIO11, GPIO14-GPIO23, GPIO27

**POUŽÍVÁME: 19 pinů**
**DOSTUPNÝCH: ~24 pinů**

✅ **JE DOSTATEK PINŮ!**

---

## 🔌 KOMPLETNÍ PIN MAPPING:

```
ESP32-C6 DevKitM-1:
├─ GPIO0  → MATRIX_COL_0 + BUTTON_QUEEN  (IN, pull-up, time-mux)
├─ GPIO1  → MATRIX_COL_1 + BUTTON_ROOK   (IN, pull-up, time-mux)
├─ GPIO2  → MATRIX_COL_2 + BUTTON_BISHOP (IN, pull-up, time-mux)
├─ GPIO3  → MATRIX_COL_3 + BUTTON_KNIGHT (IN, pull-up, time-mux)
├─ GPIO4  → MATRIX_COL_5                 (IN, pull-up)
├─ GPIO5  → STATUS_LED                   (OUT)
├─ GPIO6  → MATRIX_COL_4                 (IN, pull-up)
├─ GPIO7  → WS2812B LED DATA             (OUT, RMT)
├─ GPIO8  → UNUSED (strapping pin avoided)
├─ GPIO9  → UNUSED (strapping pin avoided)
├─ GPIO10 → MATRIX_ROW_0                 (OUT)
├─ GPIO11 → MATRIX_ROW_1                 (OUT)
├─ GPIO12 → FLASH (Reserved)
├─ GPIO13 → FLASH (Reserved)
├─ GPIO14 → UNUSED
├─ GPIO15 → BUTTON_RESET                 (IN, pull-up)
├─ GPIO16 → MATRIX_COL_6                 (IN, pull-up)
├─ GPIO17 → MATRIX_COL_7                 (IN, pull-up)
├─ GPIO18 → MATRIX_ROW_2                 (OUT)
├─ GPIO19 → MATRIX_ROW_3                 (OUT)
├─ GPIO20 → MATRIX_ROW_4                 (OUT)
├─ GPIO21 → MATRIX_ROW_5                 (OUT)
├─ GPIO22 → MATRIX_ROW_6                 (OUT)
├─ GPIO23 → MATRIX_ROW_7                 (OUT)
├─ GPIO24 → USB D- (Reserved)
├─ GPIO25 → USB D+ (Reserved)
├─ GPIO26 → USB VBUS (Reserved)
├─ GPIO27 → UNUSED
├─ GPIO28-30 → Reserved
```

---

## ✅ FUNKČNOST - ANALÝZA:

### **1. Matrix Scanning (Reed Switches):**
```
Row piny (GPIO10,11,18-23): ✅ Bezpečné GPIO
Column piny (GPIO0-3,6,14,16-17): ✅ Bezpečné GPIO

Logika:
- Row aktivní (HIGH) → Column čte (LOW = piece present)
- Reed switch zavře obvod → Column pin pulled LOW
- ✅ FUNGUJE!
```

### **2. Button Scanning (Time-Multiplexed):**
```
Promotion buttons: GPIO0,1,2,3 (same as MATRIX_COL_0-3)
Reset button: GPIO15 (samostatný, opraveno z GPIO27)

Time-multiplex:
- Matrix scan: Rows active, columns read reed contacts
- Button scan: Rows ALL HIGH, columns read buttons
- ✅ FUNGUJE! (s koordinovaným timerem)
```

### **3. WS2812B LED (GPIO7):**
```
GPIO7 podporuje RMT peripheral ✅
73 LED × 24 bits = 1752 bits per frame
RMT timing: 800kHz = 1.25us per bit
Frame time: 1752 × 1.25us = 2.19ms
Reset time: >280us

✅ FUNGUJE!
```

### **4. Status LED (GPIO5):**
```
✅ OPRAVENO z GPIO8 na GPIO5
GPIO5 je strapping pin, ale bezpečný po bootu
Problém vyřešen změnou pinu
```

---

## ✅ VYŘEŠENÉ PROBLÉMY:

### **PROBLÉM #1: GPIO8 (Status LED) je Strapping Pin - ✅ VYŘEŠENO**

**Řešení:** Status LED změněn z GPIO8 na GPIO5
- GPIO5 je strapping pin, ale bezpečný po bootu
- ✅ Problém vyřešen

### **PROBLÉM #2: GPIO27 Může Být Problematický - ✅ VYŘEŠENO**

**Řešení:** Reset Button změněn z GPIO27 na GPIO15
- GPIO15 je strapping pin (ROM messages), ale bezpečný po bootu
- ✅ Problém vyřešen

---

### **PROBLÉM #3: Time-Multiplexing - Je Fyzicky Možný?**

```
Hardware požadavky:
1. Reed switches na row×column matrix
2. Buttons na stejných column pinech
3. Pull-up rezistory na columns

Timing:
- Matrix scan: Rows postupně HIGH, columns čtou
- Button scan: Rows ALL HIGH, columns čtou

✅ FYZICKY MOŽNÉ pokud:
   - Reed switches NEPŘEKÁŽEJÍ button detekci
   - Buttons jsou připojené na columns PŘED pull-up rezistory
   - Reed switches NEZPŮSOBÍ false button triggers
```

**Schéma:**
```
                 Pull-up (10kΩ)
                      │
Column Pin ───────────┼──────── [Reed Matrix] ─── Row Pins
      │               │
      └───────────────┼──────── [Button] ─────── GND
                      │
```

**Analýza:**
- Během matrix scan: Row HIGH + Reed closed → Column LOW ✅
- Během button scan: All Rows HIGH + Button pressed → Column LOW ✅
- **FUNGUJE pokud buttons jsou v sérii s rows!** ❌

**ČEKEJ!** To může být problém...

---

## 🔴 **KRITICKÝ PROBLÉM: Button Wiring!**

Buttons musí být připojené správně:

### **Option A: Buttons přímo na columns (BAD)**
```
Column ─── [Button] ─── GND

Během matrix scan:
- Row 0 HIGH, Row 1-7 LOW
- Pokud je button stisknutý → Column vždy LOW
- ❌ OVLIVŇUJE matrix scan!
```

### **Option B: Buttons přes rows (GOOD)**
```
Column ─┬─ [Reed to Row 0]
        ├─ [Reed to Row 1]
        ├─ ...
        └─ [Button] ─── Dedicated Row nebo Logic

Potřeba:
- Dedikovaný "button row" pin
- NEBO logika která aktivuje buttons jen během button window
```

**TOHLE NENÍ V HARDWARE POPISU!**

Musím zkontrolovat jak jsou buttons vlastně zapojené...

---

## ❓ OTÁZKA K UŽIVATELI:

**Jak jsou promotion buttons fyzicky zapojené?**

A) `Column ─── [Button] ─── GND` (přímé připojení)
B) `Column ─── [Button] ─── Special Row Pin` (přes row)
C) `Column ─── [Button] ─── Diode Logic` (s diodami)
D) Jinak?

To je KRITICKÉ pro funkčnost!

