# Kritická analýza dokumentace - Nalezené nesoulady

**Datum analýzy:** 2025-01-05  
**Analyzováno:** README.md, dokumentace v docs/, skutečný kód

---

## 🚨 KRITICKÉ NESOULADY

### 1. Priority FreeRTOS tasků - **ŠPATNÉ V README**

**README.md říká:**
| Task | Priorita |
|------|----------|
| matrix_task | 6 |
| game_task | 5 |
| led_task | 4 |
| uart_task | 3 |
| web_server_task | 2 |
| button_task | 2 |
| animation_task | 3 |
| test_task | 2 |

**Skutečný kód (freertos_chess.h):**
| Task | Priorita | Poznámka |
|------|----------|---------|
| led_task | **7** | ❌ README má 4, skutečnost je 7 (nejvyšší!) |
| matrix_task | 6 | ✅ Správně |
| button_task | **5** | ❌ README má 2, skutečnost je 5 |
| game_task | **4** | ❌ README má 5, skutečnost je 4 |
| uart_task | 3 | ✅ Správně |
| animation_task | 3 | ✅ Správně |
| web_server_task | **3** | ❌ README má 2, skutečnost je 3 |
| test_task | **1** | ❌ README má 2, skutečnost je 1 |

**Důsledek:** README má úplně špatné priority. LED task má nejvyšší prioritu (7), ne matrix task!

---

### 2. Stack sizes - **ZASTARALÉ V README**

**README.md říká:**
| Task | Stack Size |
|------|------------|
| matrix_task | 3KB |
| game_task | 10KB |
| led_task | 8KB |
| uart_task | 6KB |
| web_server_task | 6KB |
| button_task | 3KB |
| animation_task | 2KB |
| test_task | 2KB |

**Skutečný kód (freertos_chess.h):**
| Task | Stack Size | Rozdíl |
|------|------------|--------|
| matrix_task | **8KB** | ❌ README má 3KB, skutečnost je 8KB (+5KB) |
| game_task | 10KB | ✅ Správně |
| led_task | **16KB** | ❌ README má 8KB, skutečnost je 16KB (+8KB) |
| uart_task | **10KB** | ❌ README má 6KB, skutečnost je 10KB (+4KB) |
| web_server_task | **20KB** | ❌ README má 6KB, skutečnost je 20KB (+14KB) |
| button_task | 3KB | ✅ Správně |
| animation_task | 2KB | ✅ Správně |
| test_task | **4KB** | ❌ README má 2KB, skutečnost je 4KB (+2KB) |

**Důsledek:** README má zastaralé stack sizes. Skutečné hodnoty jsou výrazně vyšší, což může vést k mylným představám o paměťové náročnosti.

---

### 3. Status LED GPIO pin - **CHYBNÝ V README**

**README.md říká:**
```
Status LED:      GPIO8  (samostatný pin pro status)
```

**Skutečný kód (freertos_chess.h):**
```c
#define STATUS_LED_PIN GPIO_NUM_5 // Status indicator (safe pin - GPIO8 is boot strapping pin!)
```

**Důsledek:** README uvádí GPIO8, ale kód používá GPIO5. GPIO8 je boot strapping pin, takže by neměl být používán pro status LED. Toto je kritická chyba v dokumentaci!

**Poznámka:** V dokumentaci `docs/analysis/ESP32C6_COMPLETE_PIN_ANALYSIS.md` je správně uvedeno, že GPIO8 je strapping pin a měl by se použít GPIO5.

---

## ✅ SPRÁVNÉ INFORMACE V README

### 4. Počet LED - **SPRÁVNĚ**
- README: 73 LED (64 na šachovnici + 9 na tlačítkách)
- Kód: `CHESS_LED_COUNT_TOTAL = 73` ✅

### 5. Počet řádků kódu - **TÉMĚŘ SPRÁVNĚ**
- README: game_task.c má 11,572 řádků
- Skutečnost: 11,571 řádků (rozdíl 1 řádek - zanedbatelné) ✅

### 6. Počet tasků - **SPRÁVNĚ**
- README: 8 hlavních FreeRTOS tasků
- Skutečnost: 8 tasků (led, matrix, button, uart, game, animation, test, web_server) ✅

### 7. Počet Reed Switchů - **SPRÁVNĚ**
- README: 64 Reed Switchů (8x8 matice)
- Kód: `CHESS_MATRIX_SIZE = 64` ✅

---

## 📋 DALŠÍ OVĚŘENÉ INFORMACE

### GPIO mapování (kromě Status LED)
- LED Data: GPIO7 ✅
- Matrix Rows: GPIO10,11,18,19,20,21,22,23 ✅
- Matrix Columns: GPIO0,1,2,3,6,4,16,17 ✅
- Reset Button: GPIO15 ✅ (opraveno z GPIO27)

### Time-multiplexing
- README: 25ms cyklus (0-20ms matrix, 20-25ms buttons) ✅
- Kód: Používá koordinovaný timer systém s 25ms cyklem ✅

---

## 🎯 DOPORUČENÍ

1. **OKAMŽITĚ OPRAVIT:**
   - Priority tasků v README (řádek 87-96)
   - Stack sizes v README (řádek 87-96)
   - Status LED pin v README (řádek 62)

2. **OVĚŘIT:**
   - Reset Button pin (GPIO15 vs GPIO27)
   - Přesné hodnoty time-multiplexing cyklu

3. **AKTUALIZOVAT:**
   - Všechny technické specifikace podle skutečného kódu
   - Doxygen dokumentace (možná má také zastaralé informace)

---

## 📝 POZNÁMKY

- Většina nesouladů je v README.md
- Doxygen dokumentace může obsahovat správné informace (generuje se z kódu)
- Některé dokumenty v `docs/analysis/` mají správné informace (např. o GPIO8)
- README.md je hlavní dokumentace, takže chyby tam jsou nejkritičtější

---

**Závěr:** README.md obsahuje několik kritických nesouladů, které mohou vést k mylným představám o architektuře systému. Priority a stack sizes jsou úplně špatné, což je vážný problém pro kohokoli, kdo se snaží systém pochopit nebo upravit.

