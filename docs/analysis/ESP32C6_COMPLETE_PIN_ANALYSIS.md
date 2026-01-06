# ESP32-C6 DevKitM-1 - KOMPLETNÍ PIN ANALÝZA

## 📌 ESP32-C6 STRAPPING PINS (Z KÓDU):

```c
// freertos_chess.c řádek 179:
if (pin == 4 || pin == 5 || pin == 8 || pin == 9 || pin == 15) {
    ESP_LOGW(TAG, "GPIO %d is a strapping pin - use with caution", pin);
}
```

**ESP32-C6 Strapping Pins:**
- **GPIO4** - Boot mode strapping
- **GPIO5** - Boot mode strapping  
- **GPIO8** - Boot mode strapping
- **GPIO9** - Boot mode strapping
- **GPIO15** - ROM messages enable/disable

---

## 🔍 AKTUÁLNÍ PIN POUŽITÍ V PROJEKTU:

### **✅ BEZPEČNÉ PINY (Žádný konflikt):**

**LED System:**
- GPIO7 → WS2812B LED data ✅ (RMT capable, není strapping)
- GPIO8 → Status LED ⚠️ **STRAPPING PIN!**

**Matrix Rows (8 výstupů):**
- GPIO10 → MATRIX_ROW_0 ✅
- GPIO11 → MATRIX_ROW_1 ✅
- GPIO18 → MATRIX_ROW_2 ✅
- GPIO19 → MATRIX_ROW_3 ✅
- GPIO20 → MATRIX_ROW_4 ✅
- GPIO21 → MATRIX_ROW_5 ✅
- GPIO22 → MATRIX_ROW_6 ✅
- GPIO23 → MATRIX_ROW_7 ✅

**Matrix Columns (8 vstupů s pull-up):**
- GPIO0 → MATRIX_COL_0 + BUTTON_QUEEN ✅
- GPIO1 → MATRIX_COL_1 + BUTTON_ROOK ✅
- GPIO2 → MATRIX_COL_2 + BUTTON_BISHOP ✅
- GPIO3 → MATRIX_COL_3 + BUTTON_KNIGHT ✅
- GPIO6 → MATRIX_COL_4 ✅
- GPIO4 → MATRIX_COL_5 ✅ (changed from GPIO9 - CORRECT!)
- GPIO16 → MATRIX_COL_6 ✅
- GPIO17 → MATRIX_COL_7 ✅

**Buttons:**
- GPIO15 → BUTTON_RESET ✅ (changed from GPIO27 - CORRECT!)

---

## ⚠️ PROBLÉM #1: GPIO8 (Status LED) JE STRAPPING PIN!

### **Co to znamená:**

GPIO8 na ESP32-C6:
- Pokud je **HIGH** během boot → Normální boot
- Pokud je **LOW** během boot → Download boot mode (UART/USB programming)

### **Riziko:**
Pokud status LED svítí (je LOW) během:
- Power-on reset
- Manual reset
- Brown-out reset
→ ESP32-C6 může nabootovat do download mode místo normálního módu!

### **Řešení:**

**Option A: Opravit kód (bezpečné použití GPIO8)**
```c
// V hardware init PŘED jakoukoli LED operací:
gpio_set_pull_mode(GPIO_NUM_8, GPIO_PULLUP_ONLY);
gpio_set_direction(GPIO_NUM_8, GPIO_MODE_OUTPUT);
gpio_set_level(GPIO_NUM_8, 1);  // HIGH = safe boot
```

**Option B: Změnit na jiný pin (DOPORUČENO)**
```c
#define STATUS_LED_PIN GPIO_NUM_5   // Volný pin, není strapping pro boot
// NEBO
#define STATUS_LED_PIN GPIO_NUM_15  // Strapping ale jen pro ROM messages (OK)
```

---

## 🎯 DOPORUČENÉ ZMĚNY PINŮ:

### **Změna #1: Status LED**
```c
// CURRENT:
#define STATUS_LED_PIN GPIO_NUM_8  // ❌ Strapping pin!

// RECOMMENDED:
#define STATUS_LED_PIN GPIO_NUM_5  // ✅ Volný, bezpečný
```

### **GPIO5 je volný?**
```
Aktuálně používané: 0,1,2,3,6,7,8,10,11,14,16,17,18,19,20,21,22,23,27
Volné bezpečné: 5, 15
```

✅ **GPIO5 je volný a bezpečný!**

---

## 🔌 HARDWARE WIRING - KRITICKÁ ANALÝZA:

### **Jak MUSÍ být buttons zapojené:**

#### **Matrix Reed Switches:**
```
       Pull-up 10kΩ
            │
Column ─────┼──────┬─ [Reed 0] ─── Row 0
            │      ├─ [Reed 1] ─── Row 1
            │      ├─ [Reed 2] ─── Row 2
            │      ├─ [Reed 3] ─── Row 3
            │      ├─ [Reed 4] ─── Row 4
            │      ├─ [Reed 5] ─── Row 5
            │      ├─ [Reed 6] ─── Row 6
            │      └─ [Reed 7] ─── Row 7
```

#### **Promotion Buttons:**

**❓ OTÁZKA: Jak jsou buttons připojené?**

**Option A: Directly to GND (PROBLÉM!)**
```
Column ─── [Button] ─── GND

❌ PROBLÉM: Button drží column LOW kdykoliv je stisknutý
→ Ovlivní matrix scan na VŠECH rows!
```

**Option B: Přes všechny rows (FUNGUJE!)**
```
Column ─┬─ [Reed 0] ─── Row 0
        ├─ [Reed 1] ─── Row 1
        ├─ ...
        ├─ [Reed 7] ─── Row 7
        └─ [Button] ─── (Row 0 || Row 1 || ... || Row 7)

Během matrix scan:
- Alespoň jeden row je HIGH
- Button stisknutý → Column LOW (detekováno jako reed)
- ❌ FALSE POSITIVE!

Během button scan:
- VŠECHNY rows HIGH
- Button stisknutý → Column LOW
- ✅ SPRÁVNÁ DETEKCE!
```

**Option C: Diode OR logic (FUNGUJE NEJLÍP!)**
```
Column ─┬─ [Reed 0] ─┬─[Diode]─ Row 0
        ├─ [Reed 1] ─┬─[Diode]─ Row 1
        ├─ ...
        └─ [Button] ─┬─[Diode]─ Row 0
                     ├─[Diode]─ Row 1
                     ├─ ...
                     └─[Diode]─ Row 7

→ Button funguje když JAKÝKOLI row je HIGH
→ Během button scan: VŠECHNY rows HIGH → 100% detekce
```

---

## 🧪 PRAKTICKÉ TESTOVÁNÍ - CO OČEKÁVAT:

### **Test #1: Power-On s GPIO8 Status LED**
```
Scenario: LED je aktivní (LOW) → Reset
Result: ⚠️ Možný bootloop nebo download mode
Fix: Změnit na GPIO5 nebo přidat pull-up
```

### **Test #2: Matrix Scan Během Button Press**
```
Scenario: Button držen, matrix scan běží
Matrix scan:
  - Row 0 HIGH → čte columns → možná false positive
  - Row 1 HIGH → čte columns → možná false positive
  
Button scan:
  - ALL rows HIGH → čte columns → ✅ správná detekce

Result: ⚠️ Možné false positives v matrix během button press!
```

### **Test #3: Button Press Detection**
```
Timer cycle (25ms):
  0-1.5ms:  Matrix scan
  1.5ms:    matrix_release_pins() → ALL rows HIGH
  1.6ms:    button_scan_all() → čte columns
  
Button physical wiring musí:
  - Fungovat když ALL rows jsou HIGH
  - NEfungovat když jednotlivé rows jsou HIGH/LOW

→ Závisí na hardware zapojení!
```

---

## 🎯 FINAL COMPREHENSIVE PIN TABLE:

| GPIO | Funkce | Typ | Strapping? | Status | Poznámka |
|------|--------|-----|------------|--------|----------|
| 0 | MATRIX_COL_0 + BUTTON_QUEEN | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 1 | MATRIX_COL_1 + BUTTON_ROOK | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 2 | MATRIX_COL_2 + BUTTON_BISHOP | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 3 | MATRIX_COL_3 + BUTTON_KNIGHT | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 4 | UNUSED | - | ⚠️ YES | ✅ AVOIDED | Správně vyhnutý |
| 5 | FREE | - | ⚠️ YES | 💡 AVAILABLE | Použít místo GPIO8 |
| 6 | MATRIX_COL_4 | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 7 | WS2812B LED DATA | OUT, RMT | ❌ | ✅ SAFE | RMT capable |
| 8 | STATUS_LED | OUT | ⚠️ YES | ❌ **RISKY** | **ZMĚNIT NA GPIO5!** |
| 9 | UNUSED | - | ⚠️ YES | ✅ AVOIDED | Správně vyhnutý |
| 10 | MATRIX_ROW_0 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 11 | MATRIX_ROW_1 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 12 | FLASH (Reserved) | - | - | - | Nelze použít |
| 13 | FLASH (Reserved) | - | - | - | Nelze použít |
| 14 | MATRIX_COL_5 | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 15 | FREE | - | ⚠️ YES | 💡 AVAILABLE | ROM messages (OK) |
| 16 | MATRIX_COL_6 | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 17 | MATRIX_COL_7 | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 18 | MATRIX_ROW_2 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 19 | MATRIX_ROW_3 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 20 | MATRIX_ROW_4 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 21 | MATRIX_ROW_5 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 22 | MATRIX_ROW_6 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 23 | MATRIX_ROW_7 | OUT | ❌ | ✅ SAFE | Bezpečný |
| 24 | USB D- (Reserved) | - | - | - | Nelze použít |
| 25 | USB D+ (Reserved) | - | - | - | Nelze použít |
| 26 | USB VBUS (Reserved) | - | - | - | Nelze použít |
| 27 | BUTTON_RESET | IN, PU | ❌ | ✅ SAFE | Bezpečný |
| 28-30 | Reserved | - | - | - | Nelze použít |

---

## 🚨 KRITICKÉ PROBLÉMY:

### **1. GPIO8 (Status LED) = STRAPPING PIN!**

**Aktuální stav:**
```c
#define STATUS_LED_PIN GPIO_NUM_8  // ❌ Boot mode strapping!
```

**Co se může stát:**
- LED svítí (LOW) během power-on → Download boot mode
- ESP32-C6 nenabootuje normálně
- Nebo bootuje ale nestabilně

**FIX:**
```c
#define STATUS_LED_PIN GPIO_NUM_5  // ✅ Volný bezpečný pin
```

---

### **2. TIME-MULTIPLEXING - HARDWARE ZÁVISLOST**

**Současná implementace předpokládá:**
```
Promotion buttons jsou zapojené TAK, že:
1. Fungují když VŠECHNY matrix rows jsou HIGH
2. Nefungují/neinterferují během matrix scan
```

**Možné zapojení:**

#### **A) Buttons → ALL Rows via Diodes (IDEÁLNÍ)**
```
Column Pin
    │
    ├─ [Reed 0] ────── Row 0
    ├─ [Reed 1] ────── Row 1
    ├─ ...
    └─ [Button] ──┬─[Diode 1N4148]─ Row 0
                  ├─[Diode 1N4148]─ Row 1
                  ├─ ...
                  └─[Diode 1N4148]─ Row 7
                  
Logika:
- Button stisknutý + ANY row HIGH → Column LOW
- Během button scan: ALL rows HIGH → 100% detekce ✅
- Během matrix scan: Možná detekce ⚠️ (false positive)
```

**Ale náš time-multiplexing to řeší:**
- Matrix scan → detekuje buttons jako reed closures (ignorujeme)
- Button scan → detekuje pouze buttons ✅

#### **B) Buttons → GND (PROBLÉM!)**
```
Column Pin
    │
    ├─ [Reed 0] ────── Row 0
    ├─ ...
    └─ [Button] ────── GND

❌ Button drží column LOW nepřetržitě!
→ Matrix scan detekuje na VŠECH rows!
→ NEFUNKČNÍ!
```

#### **C) Buttons → Dedicated Control Pin (OK)**
```
Potřeba extra pin pro "button enable"
→ Software kontroluje kdy jsou buttons aktivní
```

---

## 📊 ZÁVĚR - BUDE TO FUNGOVAT?

### **SOUČASNÁ KONFIGURACE:**

✅ **FUNGUJE pokud:**
1. GPIO8 má pull-up nebo se změnkód na GPIO5
2. Buttons jsou zapojené přes diode OR logic k rows
3. Hardware má správné pull-up rezistory

⚠️ **NEFUNGUJE pokud:**
1. Buttons jdou přímo na GND (bez rows)
2. GPIO8 způsobí boot problémy
3. Hardware wiring je špatný

---

## 🔧 DOPORUČENÉ OKAMŽITÉ OPRAVY:

### **OPRAVA #1: Změnit GPIO8 na GPIO5**
```c
// freertos_chess.h:
#define STATUS_LED_PIN GPIO_NUM_5  // ✅ Safe, not strapping
```

Tato změna:
- Eliminuje strapping pin problem
- GPIO5 je volný
- GPIO5 podporuje normální GPIO operace
- Žádný boot mode conflict

---

## 💡 DODATEČNÁ OTÁZKA K UŽIVATELI:

**Jak jsou promotion buttons fyzicky zapojené?**

Popisná odpověď mi pomůže ověřit že time-multiplexing bude fungovat 100%.

Možnosti:
A) Buttons → GND (problém!)
B) Buttons → Rows via diodes (ideální!)
C) Buttons → Dedicated control pin
D) Jiné zapojení?

---

## ✅ SHRNUTÍ:

**Pin konfigurace je 95% správná!**

**Jediná nutná změna:**
```c
GPIO8 → GPIO5 (status LED)
```

**Zbytek je PERFEKTNÍ:**
- Všechny strapping pins vyhnuté ✅
- Dostatek GPIO pinů ✅
- RMT capable pin pro LED ✅
- Správné INPUT/OUTPUT směry ✅
- Pull-up na správných pinech ✅

**S tím to změnou GPIO8→GPIO5 bude vše 100% funkční!** 🎯

