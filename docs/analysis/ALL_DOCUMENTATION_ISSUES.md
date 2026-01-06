# Kompletní seznam všech nesouladů v dokumentaci

**Datum:** 2025-01-05  
**Status:** Připraveno k opravě

---

## KRITICKÉ NESOULADY (Okamžitě opravit)

### 1. README.md - FreeRTOS Tasky (Priority a Stack Sizes)

**Lokace:** README.md, řádky 87-96

**Problém:**
- Všechny priority jsou špatně (kromě animation_task)
- Všechny stack sizes jsou špatně (kromě animation_task a button_task)

**Oprava:**
```markdown
| Task | Priorita | Popis | Stack Size |
|------|----------|-------|------------|
| `led_task` | 7 | Ovládání WS2812B LED - nejvyšší priorita pro kritický timing | 16KB |
| `matrix_task` | 6 | Realtime skenování 8x8 matice - vysoká priorita, nesmí zmeškat detekci pohybu | 8KB |
| `button_task` | 5 | Zpracování tlačítek - uživatelský vstup | 3KB |
| `game_task` | 4 | Šachová logika a stav hry - kritická pro odezvu | 10KB |
| `uart_task` | 3 | UART konzole a příkazy - může čekat | 10KB |
| `animation_task` | 3 | LED animace - non-blocking | 2KB |
| `web_server_task` | 3 | HTTP web server - komunikace | 20KB |
| `test_task` | 1 | Testovací funkce - nejnižší priorita | 4KB |
```

### 2. README.md - Hardware GPIO Mapování

**Lokace:** README.md, řádek 61

**Problém:**
- Matrix Columns uvádí GPIO14, ale skutečnost je GPIO4

**Oprava:**
```markdown
Matrix Columns: GPIO0,1,2,3,6,4,16,17 (8 vstupů s pull-up)
```

### 3. docs/hardware/HARDWARE_WIRING_GUIDE.md - Matrix Column 5

**Lokace:** docs/hardware/HARDWARE_WIRING_GUIDE.md, řádek 39

**Problém:**
- Uvádí GPIO14 pro Column 5

**Oprava:**
```markdown
Column 5 (F) → GPIO4  + 10kΩ pull-up → 3.3V
```

### 4. docs/hardware/HARDWARE_WIRING_GUIDE.md - Reset Button

**Lokace:** docs/hardware/HARDWARE_WIRING_GUIDE.md, řádek 88

**Problém:**
- Uvádí GPIO27 pro Reset Button

**Oprava:**
```markdown
GPIO15 ─── [Button] ─── GND
(Internal pull-up aktivní v software)
```

### 5. docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Matrix Column 5

**Lokace:** docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md, řádek 45

**Problém:**
- Uvádí GPIO14 pro MATRIX_COL_5

**Oprava:**
```markdown
- GPIO4 → MATRIX_COL_5 ✅ (changed from GPIO9 - CORRECT!)
```

### 6. docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Reset Button

**Lokace:** docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md, řádek 50

**Problém:**
- Uvádí GPIO27 pro BUTTON_RESET

**Oprava:**
```markdown
- GPIO15 → BUTTON_RESET ✅ (changed from GPIO27 - CORRECT!)
```

### 7. docs/analysis/DOCUMENTATION_INACCURACIES.md - Matrix Columns

**Lokace:** docs/analysis/DOCUMENTATION_INACCURACIES.md, řádek 113

**Problém:**
- Uvádí GPIO14 v seznamu Matrix Columns

**Oprava:**
```markdown
- Matrix Columns: GPIO0,1,2,3,6,4,16,17 ✅
```

---

## VÝZNAMNÉ NESOULADY (Aktualizovat)

### 8. docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md - Status LED

**Lokace:** docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md

**Problém:**
- Dokumentace uvádí GPIO8 jako problém, ale už je opraveno na GPIO5
- Mělo by být aktualizováno, že problém je vyřešen

**Oprava:**
- Přidat poznámku, že Status LED je nyní GPIO5 a problém je vyřešen

---

## DROBNÉ NESOULADY (Vyčistit)

### 9. docs/analysis/DOCUMENTATION_INACCURACIES.md - Time-multiplexing

**Lokace:** docs/analysis/DOCUMENTATION_INACCURACIES.md, řádek 117

**Problém:**
- Uvádí 30ms cyklus, ale skutečnost je 25ms

**Oprava:**
```markdown
- README: 25ms cyklus (0-20ms matrix, 20-25ms buttons) ✅
- Kód: Používá koordinovaný timer systém s 25ms cyklem ✅
```

---

## Shrnutí

**Kritické nesoulady:** 7  
**Významné nesoulady:** 1  
**Drobné nesoulady:** 1  

**Celkem:** 9 nesouladů k opravě

**Soubory k opravě:**
1. README.md
2. docs/hardware/HARDWARE_WIRING_GUIDE.md
3. docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md
4. docs/analysis/DOCUMENTATION_INACCURACIES.md

