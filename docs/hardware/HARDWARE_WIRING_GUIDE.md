# ESP32-C6 Chess Board - HARDWARE ZAPOJENÍ

## 🔌 QUICK WIRING GUIDE

### **1. WS2812B LED STRIP (73 LED)**
```
LED Strip DIN → GPIO7
LED Strip GND → GND
LED Strip +5V → 5V (externí zdroj!)
```

### **2. STATUS LED**
```
LED Anode (+) → GPIO5
LED Cathode (-) → Rezistor 220Ω → GND
```

### **3. MATRIX REED SWITCHES (8×8 = 64)**

**Rows (Výstupy):**
```
Row 0 → GPIO10
Row 1 → GPIO11
Row 2 → GPIO18
Row 3 → GPIO19
Row 4 → GPIO20
Row 5 → GPIO21
Row 6 → GPIO22
Row 7 → GPIO23
```

**Columns (Vstupy s pull-up):**
```
Column 0 (A) → GPIO0  + 10kΩ pull-up → 3.3V
Column 1 (B) → GPIO1  + 10kΩ pull-up → 3.3V
Column 2 (C) → GPIO2  + 10kΩ pull-up → 3.3V
Column 3 (D) → GPIO3  + 10kΩ pull-up → 3.3V
Column 4 (E) → GPIO6  + 10kΩ pull-up → 3.3V
Column 5 (F) → GPIO4  + 10kΩ pull-up → 3.3V
Column 6 (G) → GPIO16 + 10kΩ pull-up → 3.3V
Column 7 (H) → GPIO17 + 10kΩ pull-up → 3.3V
```

**Reed Switch Zapojení:**
```
Každý square (A1-H8):
  Column Pin ─── [Reed Switch] ─── Row Pin
  
Příklad:
  A1: GPIO0 ─── [Reed] ─── GPIO10
  E4: GPIO6 ─── [Reed] ─── GPIO19
```

### **4. PROMOTION BUTTONS (4 tlačítka)**

**⚠️ KRITICKÉ: Buttons MUSÍ být zapojené přes ALL ROWS!**

```
Button 0 (QUEEN):
  GPIO0 ─── [Button] ───┬─[Diode 1N4148]→ GPIO10
                        ├─[Diode 1N4148]→ GPIO11
                        ├─[Diode 1N4148]→ GPIO18
                        ├─[Diode 1N4148]→ GPIO19
                        ├─[Diode 1N4148]→ GPIO20
                        ├─[Diode 1N4148]→ GPIO21
                        ├─[Diode 1N4148]→ GPIO22
                        └─[Diode 1N4148]→ GPIO23

Button 1 (ROOK):
  GPIO1 ─── [Button] ───┬─[Diode 1N4148]→ GPIO10-23 (všechny rows)
  
Button 2 (BISHOP):
  GPIO2 ─── [Button] ───┬─[Diode 1N4148]→ GPIO10-23 (všechny rows)
  
Button 3 (KNIGHT):
  GPIO3 ─── [Button] ───┬─[Diode 1N4148]→ GPIO10-23 (všechny rows)
```

**Diody:** 1N4148 nebo jakékoli signal diody (anode k rows)

**Proč diody?**
- Izolují rows mezi sebou
- Umožňují OR logic (button funguje když ANY row HIGH)
- Během button scan: ALL rows HIGH → 100% detekce

### **5. RESET BUTTON**
```
GPIO15 ─── [Button] ─── GND
(Internal pull-up aktivní v software)
```

---

## 📐 SCHEMATICKÝ DIAGRAM:

```
ESP32-C6 DevKitM-1
                    Pull-up 10kΩ
┌─────────────┐         │
│ GPIO0 (A) ◄─┼─────────┼───┬─[Reed A1]─ GPIO10 (Row 0)
│             │         │   ├─[Reed A2]─ GPIO11 (Row 1)
│             │         │   ├─ ...
│             │         │   └─[Reed A8]─ GPIO23 (Row 7)
│             │         │   
│             │         └───[Button QUEEN]─┬─►─ GPIO10
│             │                            ├─►─ GPIO11
│             │                            ├─ ...
│             │                            └─►─ GPIO23
│             │                            (8× diody 1N4148)
│             │
│ GPIO1 (B) ◄─┼─── (stejné jako GPIO0)
│ GPIO2 (C) ◄─┼─── (stejné jako GPIO0)
│ GPIO3 (D) ◄─┼─── (stejné jako GPIO0)
│ GPIO6 (E) ◄─┼─── (bez button)
│ ...         │
│             │
│ GPIO7 ────► │─── WS2812B LED Strip DIN
│ GPIO5 ────► │─── Status LED (+) → 220Ω → GND
│             │
│ GPIO15 ◄────┼─── Reset Button → GND
│             │    (internal pull-up)
│             │
│ GPIO10-23 ►─┴─── Matrix Rows (výstupy)
└─────────────┘
```

---

## 🔧 KOMPONENTY POTŘEBNÉ:

| Komponenta | Množství | Poznámka |
|------------|----------|----------|
| ESP32-C6 DevKitM-1 | 1× | Hlavní MCU |
| WS2812B LED strip | 73 LED | 64 board + 9 buttons |
| Reed switches | 64× | NO (normally open) |
| Tactile buttons | 5× | 4 promotion + 1 reset |
| Diody 1N4148 | 32× | 4 buttons × 8 rows |
| Rezistory 10kΩ | 8× | Pull-up pro columns |
| Rezistor 220Ω | 1× | Pro status LED |
| LED 3mm | 1× | Status indikátor |
| Napájení 5V | 1× | Pro LED strip (3-4A) |

---

## ⚡ POWER REQUIREMENTS:

```
ESP32-C6: ~200mA (max)
WS2812B: 73 LED × 60mA = 4.38A (max, všechny bílé)
Status LED: ~20mA

TOTAL: ~4.6A @ 5V (maximální)
TYPICAL: ~1A @ 5V (normální hra)

💡 Doporučení: 5V/5A zdroj (USB-C PD nebo adaptér)
```

---

## ✅ CHECKLIST PŘED ZAPNUTÍM:

- [ ] Všechny column piny mají 10kΩ pull-up
- [ ] Buttons mají diody k VŠEM rows
- [ ] Reset button jde na GND (ne VCC!)
- [ ] LED strip má externí 5V napájení
- [ ] Status LED má rezistor 220Ω
- [ ] GPIO5 použit pro status LED (NE GPIO8!)
- [ ] Žádné zkraty mezi piny
- [ ] GND společná pro všechny komponenty

---

## 🎯 TEST PROCEDURA:

1. **Připojit jen ESP32-C6** (bez periferií) → Boot OK?
2. **Přidat status LED** → Boot OK?
3. **Přidat WS2812B** → LED svítí?
4. **Přidat matrix** → Scan funguje?
5. **Přidat buttons** → Detection funguje?

---

**S tímto zapojením bude vše 100% funkční!** 🎉

