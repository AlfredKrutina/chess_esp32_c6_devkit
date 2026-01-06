# ZAPOJENÍ ESP32-C6 Chess Board

## WS2812B LED STRIP (73 LED)
```
DIN  → GPIO7
GND  → GND
+5V  → 5V (externí zdroj)
```

## STATUS LED
```
LED+ → GPIO5 → LED → 220Ω → GND
```

## MATRIX ROWS (vystupy)
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

## MATRIX COLUMNS (vstupy)
```
Column A → GPIO0  (+ 10kΩ pull-up → 3.3V)
Column B → GPIO1  (+ 10kΩ pull-up → 3.3V)
Column C → GPIO2  (+ 10kΩ pull-up → 3.3V)
Column D → GPIO3  (+ 10kΩ pull-up → 3.3V)
Column E → GPIO6  (+ 10kΩ pull-up → 3.3V)
Column F → GPIO4  (+ 10kΩ pull-up → 3.3V)
Column G → GPIO16 (+ 10kΩ pull-up → 3.3V)
Column H → GPIO17 (+ 10kΩ pull-up → 3.3V)
```

## REED SWITCHES (64×)
```
Každé pole A1-H8:
  Column Pin ─── [Reed Switch] ─── Row Pin

Příklad:
  A1: GPIO0 ─── Reed ─── GPIO10
  H8: GPIO17 ─── Reed ─── GPIO23
```

## PROMOTION BUTTONS (4×)
```
Button QUEEN:
  GPIO0 ─── Button ───┬─ Diode → GPIO10
                      ├─ Diode → GPIO11
                      ├─ Diode → GPIO18
                      ├─ Diode → GPIO19
                      ├─ Diode → GPIO20
                      ├─ Diode → GPIO21
                      ├─ Diode → GPIO22
                      └─ Diode → GPIO23

Button ROOK:   GPIO1 ─── (stejné, 8× diody k rows)
Button BISHOP: GPIO2 ─── (stejné, 8× diody k rows)
Button KNIGHT: GPIO3 ─── (stejné, 8× diody k rows)

Diody: 1N4148 (anode k rows)
```

## RESET BUTTON
```
GPIO15 ─── Button ─── GND
(internal pull-up v software)
Note: GPIO27 neni dostupny na LaskaKit desce, pouzit GPIO15
```

## NAPÁJENÍ
```
ESP32-C6: USB-C (5V)
LED Strip: 5V/5A externí zdroj
GND: Společná pro všechny komponenty
```

---

**CELKEM POTŘEBA:**
- 64× Reed switches
- 5× Tactile buttons
- 32× Diody 1N4148 (4 buttons × 8 rows)
- 8× Rezistory 10kΩ (pull-up)
- 1× Rezistor 220Ω (status LED)
- 1× LED 3mm
- 73× WS2812B LED strip
- 5V/5A zdroj

