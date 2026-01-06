# Analýza hardwarového mapování - Skutečný stav vs Dokumentace

**Datum analýzy:** 2025-01-05  
**Verze kódu:** 2.4.1  
**Analyzováno:** components/freertos_chess/include/freertos_chess.h, README.md, docs/hardware/

---

## 1. GPIO Pin Mapování - Skutečný stav

### 1.1 LED System

| Komponenta | GPIO Pin | Status | Poznámka |
|-----------|----------|--------|----------|
| **LED Data (WS2812B)** | **GPIO7** | ✅ Správně | RMT capable, bezpečný pin |
| **Status LED** | **GPIO5** | ✅ Správně | Opraveno z GPIO8 (strapping pin) |

### 1.2 Matrix Rows (Výstupy)

| Row | GPIO Pin | Status | Poznámka |
|-----|----------|--------|----------|
| Row 0 | **GPIO10** | ✅ Správně | |
| Row 1 | **GPIO11** | ✅ Správně | |
| Row 2 | **GPIO18** | ✅ Správně | |
| Row 3 | **GPIO19** | ✅ Správně | |
| Row 4 | **GPIO20** | ✅ Správně | |
| Row 5 | **GPIO21** | ✅ Správně | |
| Row 6 | **GPIO22** | ✅ Správně | |
| Row 7 | **GPIO23** | ✅ Správně | |

**Celkem: 8 výstupů (GPIO10,11,18,19,20,21,22,23)**

### 1.3 Matrix Columns (Vstupy s pull-up)

| Column | GPIO Pin | Status | Poznámka |
|--------|----------|--------|----------|
| Column 0 (A) | **GPIO0** | ✅ Správně | + BUTTON_QUEEN |
| Column 1 (B) | **GPIO1** | ✅ Správně | + BUTTON_ROOK |
| Column 2 (C) | **GPIO2** | ✅ Správně | + BUTTON_BISHOP |
| Column 3 (D) | **GPIO3** | ✅ Správně | + BUTTON_KNIGHT |
| Column 4 (E) | **GPIO6** | ✅ Správně | + BUTTON_PROMOTION_QUEEN |
| Column 5 (F) | **GPIO4** | ⚠️ **NESOULAD!** | Změněno z GPIO9 (strapping pin) |
| Column 6 (G) | **GPIO16** | ✅ Správně | + BUTTON_PROMOTION_BISHOP |
| Column 7 (H) | **GPIO17** | ✅ Správně | + BUTTON_PROMOTION_KNIGHT |

**Celkem: 8 vstupů (GPIO0,1,2,3,6,4,16,17)**

**⚠️ KRITICKÝ NESOULAD:** Column 5 je **GPIO4**, ne GPIO14!

### 1.4 Buttons

| Button | GPIO Pin | Status | Poznámka |
|--------|----------|--------|----------|
| **Reset Button** | **GPIO15** | ✅ Správně | Opraveno z GPIO27 |
| Queen (A1) | GPIO0 | ✅ Správně | Sdíleno s MATRIX_COL_0 |
| Rook (B1) | GPIO1 | ✅ Správně | Sdíleno s MATRIX_COL_1 |
| Bishop (C1) | GPIO2 | ✅ Správně | Sdíleno s MATRIX_COL_2 |
| Knight (D1) | GPIO3 | ✅ Správně | Sdíleno s MATRIX_COL_3 |
| Promotion Queen (E1) | GPIO6 | ✅ Správně | Sdíleno s MATRIX_COL_4 |
| Promotion Rook (F1) | GPIO4 | ✅ Správně | Sdíleno s MATRIX_COL_5 |
| Promotion Bishop (G1) | GPIO16 | ✅ Správně | Sdíleno s MATRIX_COL_6 |
| Promotion Knight (H1) | GPIO17 | ✅ Správně | Sdíleno s MATRIX_COL_7 |

**Celkem: 9 tlačítek** (1 reset + 8 promotion buttons)

---

## 2. Time-Multiplexing - Skutečný stav

| Parametr | Hodnota | Status | Poznámka |
|----------|---------|--------|----------|
| **Celkový cyklus** | **25ms** | ✅ Správně | Zredukováno z 30ms |
| **Matrix scan window** | **0-20ms** | ✅ Správně | |
| **Button scan window** | **20-25ms** | ✅ Správně | |
| **LED update** | **Nezávisle (25ms timer)** | ✅ Správně | Není součástí multiplexing cyklu |

**Poznámka:** LED aktualizace probíhá nezávisle na time-multiplexing cyklu pomocí `led_update_timer` (25ms perioda).

---

## 3. Strapping Pins - Analýza

### 3.1 ESP32-C6 Strapping Pins

| GPIO | Funkce | Status v projektu | Riziko |
|------|--------|-------------------|--------|
| GPIO4 | Boot mode | ⚠️ **POUŽITO** (MATRIX_COL_5) | Nízké (po bootu OK) |
| GPIO5 | Boot mode | ✅ **POUŽITO** (STATUS_LED) | Nízké (po bootu OK) |
| GPIO8 | Boot mode | ❌ **NEPOUŽITO** | - |
| GPIO9 | Boot mode | ❌ **NEPOUŽITO** | Změněno na GPIO4 |
| GPIO15 | ROM messages | ✅ **POUŽITO** (BUTTON_RESET) | Nízké (po bootu OK) |

**Závěr:** Všechny použité strapping piny jsou bezpečné po bootu. GPIO8 byl změněn na GPIO5 pro status LED.

---

## 4. Porovnání s dokumentací

### 4.1 README.md - GPIO mapování

| Komponenta | README | Skutečnost | Status |
|------------|--------|------------|--------|
| **LED Data** | GPIO7 | GPIO7 | ✅ SPRÁVNĚ |
| **Status LED** | GPIO5 | GPIO5 | ✅ SPRÁVNĚ (opraveno) |
| **Reset Button** | GPIO15 | GPIO15 | ✅ SPRÁVNĚ (opraveno) |
| **Matrix Rows** | GPIO10,11,18,19,20,21,22,23 | GPIO10,11,18,19,20,21,22,23 | ✅ SPRÁVNĚ |
| **Matrix Columns** | GPIO0,1,2,3,6,14,16,17 | GPIO0,1,2,3,6,**4**,16,17 | ❌ **NESOULAD!** |

**Kritický nesoulad:** README.md uvádí **GPIO14** pro Column 5, ale skutečný kód používá **GPIO4**!

### 4.2 README.md - Time-Multiplexing

| Parametr | README | Skutečnost | Status |
|----------|--------|------------|--------|
| **Cyklus** | 25ms | 25ms | ✅ SPRÁVNĚ (opraveno) |
| **Matrix window** | 0-20ms | 0-20ms | ✅ SPRÁVNĚ |
| **Button window** | 20-25ms | 20-25ms | ✅ SPRÁVNĚ |
| **LED update** | Nezávisle | Nezávisle (25ms timer) | ✅ SPRÁVNĚ (opraveno) |

### 4.3 docs/hardware/HARDWARE_WIRING_GUIDE.md

**Matrix Columns:**
```
Column 5 (F) → GPIO14 + 10kΩ pull-up → 3.3V  ❌ ŠPATNĚ!
```

**Skutečnost:**
```
Column 5 (F) → GPIO4 + 10kΩ pull-up → 3.3V  ✅ SPRÁVNĚ
```

**Reset Button:**
```
GPIO27 ─── [Button] ─── GND  ❌ ŠPATNĚ!
```

**Skutečnost:**
```
GPIO15 ─── [Button] ─── GND  ✅ SPRÁVNĚ
```

### 4.4 docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md

**Matrix Columns:**
- Uvádí GPIO14 pro MATRIX_COL_5 ❌ **ŠPATNĚ!**
- Skutečnost: GPIO4 ✅

**Reset Button:**
- Uvádí GPIO27 ❌ **ŠPATNĚ!**
- Skutečnost: GPIO15 ✅

**Status LED:**
- Uvádí GPIO8 ❌ **ŠPATNĚ!**
- Skutečnost: GPIO5 ✅ (ale dokumentace to už uvádí jako problém)

---

## 5. Identifikované nesoulady

### 5.1 Kritické nesoulady

1. **README.md - Matrix Columns**
   - Uvádí: `GPIO0,1,2,3,6,14,16,17`
   - Skutečnost: `GPIO0,1,2,3,6,4,16,17`
   - **Column 5 je GPIO4, ne GPIO14!**

2. **docs/hardware/HARDWARE_WIRING_GUIDE.md - Matrix Column 5**
   - Uvádí: `Column 5 (F) → GPIO14`
   - Skutečnost: `Column 5 (F) → GPIO4`

3. **docs/hardware/HARDWARE_WIRING_GUIDE.md - Reset Button**
   - Uvádí: `GPIO27 ─── [Button] ─── GND`
   - Skutečnost: `GPIO15 ─── [Button] ─── GND`

4. **docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Matrix Column 5**
   - Uvádí: GPIO14
   - Skutečnost: GPIO4

5. **docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Reset Button**
   - Uvádí: GPIO27
   - Skutečnost: GPIO15

### 5.2 Významné nesoulady

1. **docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Status LED**
   - Dokumentace uvádí GPIO8 jako problém, ale už je opraveno na GPIO5
   - Dokumentace by měla být aktualizována, že problém je vyřešen

---

## 6. Doporučení

### 6.1 OKAMŽITĚ OPRAVIT

1. **README.md - Matrix Columns**
   - Změnit: `GPIO0,1,2,3,6,14,16,17`
   - Na: `GPIO0,1,2,3,6,4,16,17`

2. **docs/hardware/HARDWARE_WIRING_GUIDE.md**
   - Opravit Column 5: GPIO14 → GPIO4
   - Opravit Reset Button: GPIO27 → GPIO15

3. **docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md**
   - Opravit Column 5: GPIO14 → GPIO4
   - Opravit Reset Button: GPIO27 → GPIO15
   - Aktualizovat Status LED sekci - problém je vyřešen (GPIO5)

### 6.2 OVĚŘIT

1. **docs/hardware/ZAPOJENI.md**
   - Zkontrolovat, zda obsahuje správné GPIO piny

2. **Všechny další dokumentační soubory**
   - Prohledat všechny soubory pro zmínky o GPIO14 a GPIO27

---

## 7. Shrnutí

**Skutečný stav:**
- LED Data: GPIO7 ✅
- Status LED: GPIO5 ✅
- Matrix Rows: GPIO10,11,18,19,20,21,22,23 ✅
- Matrix Columns: GPIO0,1,2,3,6,**4**,16,17 ⚠️ (Column 5 je GPIO4, ne GPIO14!)
- Reset Button: GPIO15 ✅
- Time-multiplexing: 25ms cyklus ✅

**Dokumentace:**
- README.md má **kritický nesoulad** v Matrix Columns (GPIO14 vs GPIO4)
- docs/hardware/HARDWARE_WIRING_GUIDE.md má **kritické nesoulady** v Column 5 a Reset Button
- docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md má **kritické nesoulady** v Column 5 a Reset Button

**Status:** ✅ Analýza dokončena, připraveno k opravě dokumentace

