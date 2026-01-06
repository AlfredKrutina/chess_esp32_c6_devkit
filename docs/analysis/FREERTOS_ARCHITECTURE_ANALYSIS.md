# Analýza FreeRTOS architektury - Skutečný stav vs Dokumentace

**Datum analýzy:** 2025-01-05  
**Verze kódu:** 2.4.1  
**Analyzováno:** main/main.c, components/freertos_chess/

---

## 1. FreeRTOS Tasky - Skutečný stav

### 1.1 Aktivní tasky

| Task | Priorita | Stack Size | Status | Poznámka |
|------|----------|------------|--------|----------|
| `led_task` | **7** | **16KB** | ✅ Aktivní | Nejvyšší priorita pro kritický LED timing |
| `matrix_task` | **6** | **8KB** | ✅ Aktivní | Hardware vstup, realtime detekce |
| `button_task` | **5** | **3KB** | ✅ Aktivní | Uživatelský vstup |
| `game_task` | **4** | **10KB** | ✅ Aktivní | Šachová logika |
| `uart_task` | **3** | **10KB** | ✅ Aktivní | UART konzole (suspended při startu) |
| `animation_task` | **3** | **2KB** | ✅ Aktivní | LED animace |
| `web_server_task` | **3** | **20KB** | ✅ Aktivní | HTTP web server |
| `test_task` | **1** | **4KB** | ✅ Aktivní | Testovací funkce |

**Celkem aktivních tasků: 8**

### 1.2 Zakázané tasky

| Task | Priorita | Stack Size | Status | Důvod |
|------|----------|------------|--------|-------|
| `screen_saver_task` | 2 | 2KB | ❌ DISABLED | LED konflikty |
| `matter_task` | - | - | ❌ DISABLED | Matter není potřeba |

**Poznámka:** Screen saver fronty (`screen_saver_command_queue`, `screen_saver_status_queue`) jsou stále vytvářeny, ale task není spuštěn.

---

## 2. Fronty (Queues) - Skutečný stav

### 2.1 Aktivní fronty

| Fronta | Velikost | Typ zprávy | Status | Poznámka |
|--------|----------|------------|--------|----------|
| `matrix_event_queue` | MATRIX_QUEUE_SIZE | `matrix_event_t` | ✅ Aktivní | Matrix události (UP/DN) |
| `matrix_command_queue` | MATRIX_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Matrix příkazy |
| `matrix_response_queue` | MATRIX_QUEUE_SIZE | `game_response_t` | ✅ Aktivní | Matrix odpovědi |
| `button_event_queue` | BUTTON_QUEUE_SIZE | `button_event_t` | ✅ Aktivní | Button události |
| `button_command_queue` | BUTTON_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Button příkazy |
| `uart_command_queue` | UART_QUEUE_SIZE | `char[64]` | ✅ Aktivní | UART příkazy |
| `uart_response_queue` | UART_QUEUE_SIZE | `game_response_t` | ✅ Aktivní | UART odpovědi |
| `uart_output_queue` | 20 | 512 bytes | ✅ Aktivní | Centralizovaný UART výstup |
| `game_command_queue` | GAME_QUEUE_SIZE | `chess_move_command_t` | ✅ Aktivní | Game příkazy |
| `game_status_queue` | GAME_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Game status |
| `animation_command_queue` | ANIMATION_QUEUE_SIZE | `led_command_t` | ✅ Aktivní | Animation příkazy |
| `animation_status_queue` | ANIMATION_QUEUE_SIZE | `esp_err_t` | ✅ Aktivní | Animation status |
| `screen_saver_command_queue` | SCREEN_SAVER_QUEUE_SIZE | `uint8_t` | ⚠️ Existuje | Task je DISABLED |
| `screen_saver_status_queue` | SCREEN_SAVER_QUEUE_SIZE | `esp_err_t` | ⚠️ Existuje | Task je DISABLED |
| `web_command_queue` | WEB_SERVER_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Web příkazy |
| `web_server_command_queue` | WEB_SERVER_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Web server příkazy |
| `web_server_status_queue` | WEB_SERVER_QUEUE_SIZE | `esp_err_t` | ✅ Aktivní | Web server status |
| `test_command_queue` | LED_QUEUE_SIZE | `uint8_t` | ✅ Aktivní | Test příkazy |

**Celkem aktivních front: 18** (včetně 2 pro disabled screen_saver)

### 2.2 Odstraněné fronty

| Fronta | Status | Důvod |
|--------|--------|-------|
| `led_command_queue` | ❌ REMOVED | Nahrazeno přímými LED voláními |
| `led_status_queue` | ❌ REMOVED | Nahrazeno přímými LED voláními |

**Poznámka:** V kódu jsou tyto fronty zakomentované s poznámkou "Queue hell eliminated".

---

## 3. Mutexy - Skutečný stav

| Mutex | Status | Použití |
|-------|--------|---------|
| `led_mutex` | ✅ Aktivní | Ochrana LED bufferu |
| `matrix_mutex` | ✅ Aktivní | Ochrana matrix stavu |
| `button_mutex` | ✅ Aktivní | Ochrana button stavu |
| `game_mutex` | ✅ Aktivní | Ochrana game stavu (kritický!) |
| `system_mutex` | ✅ Aktivní | Ochrana systémových zdrojů |

**Celkem mutexů: 5**

**Poznámka:** `uart_mutex` je externí (definovaný v uart_task.c), není vytvářen v `chess_create_mutexes()`.

---

## 4. Timery - Skutečný stav

| Timer | Perioda | Status | Poznámka |
|-------|---------|--------|----------|
| `coordinated_multiplex_timer` | **25ms** | ✅ Aktivní | Koordinovaný time-multiplexing (0-20ms matrix, 20-25ms buttons) |
| `led_update_timer` | **25ms** | ✅ Aktivní | Periodické obnovení LED |
| `matrix_scan_timer` | MATRIX_SCAN_TIME_MS | ⚠️ LEGACY | Vytvořen, ale NEPOUŽÍVÁN (coordinated system) |
| `button_scan_timer` | BUTTON_SCAN_TIME_MS | ⚠️ LEGACY | Vytvořen, ale NEPOUŽÍVÁN (coordinated system) |
| `system_health_timer` | 30s | ❌ DISABLED | Zakomentovaný v kódu |

**Poznámka:** `coordinated_multiplex_timer` je hlavní timer pro time-multiplexing. Legacy timery jsou vytvářeny pro kompatibilitu, ale nejsou aktivně používány.

---

## 5. Watchdog Timer (WDT) - Skutečný stav

| Parametr | Hodnota | Poznámka |
|----------|---------|----------|
| **Timeout během init** | **10s** | Pro inicializaci (zvýšeno z 5s) |
| **Timeout po init** | **8s** | Normální provoz (zvýšeno z 5s pro web server) |
| **Trigger panic** | `true` | Systém se resetuje při timeoutu |
| **Registrace tasků** | Automatická | Každý task se registruje sám |

**Poznámka:** WDT timeout byl zvýšen na 8s kvůli web serveru, který může potřebovat více času.

---

## 6. Porovnání s dokumentací

### 6.1 README.md - Tasky

| Task | README Priorita | Skutečná Priorita | README Stack | Skutečný Stack | Status |
|------|-----------------|-------------------|--------------|----------------|--------|
| `led_task` | 4 | **7** | 8KB | **16KB** | ❌ NESOULAD |
| `matrix_task` | 6 | 6 | 3KB | **8KB** | ❌ NESOULAD |
| `button_task` | 2 | **5** | 3KB | 3KB | ❌ NESOULAD |
| `game_task` | 5 | **4** | 10KB | 10KB | ❌ NESOULAD |
| `uart_task` | 3 | 3 | 6KB | **10KB** | ❌ NESOULAD |
| `animation_task` | 3 | 3 | 2KB | 2KB | ✅ SPRÁVNĚ |
| `web_server_task` | 2 | **3** | 6KB | **20KB** | ❌ NESOULAD |
| `test_task` | 2 | **1** | 2KB | **4KB** | ❌ NESOULAD |

**Závěr:** README.md má **všechny priority a stack sizes špatně** (kromě animation_task).

### 6.2 README.md - Time-multiplexing

| Parametr | README | Skutečnost | Status |
|----------|--------|------------|--------|
| **Cyklus** | 30ms | **25ms** | ❌ NESOULAD |
| **Matrix window** | 0-20ms | 0-20ms | ✅ SPRÁVNĚ |
| **Button window** | 20-25ms | 20-25ms | ✅ SPRÁVNĚ |
| **LED update** | 25-30ms | Nezávisle (25ms timer) | ❌ NESOULAD |

**Závěr:** README.md má zastaralý popis time-multiplexingu (30ms vs 25ms).

### 6.3 main/main.c - Doxygen komentáře

**STARTUP SEKVENCE (řádky 38-46):**
- ✅ Priority jsou správné (7, 6, 5, 4, 3, 3, 3, 1)
- ✅ Počet tasků je správný (8)
- ✅ Zmínka o `led_command_queue` byla odstraněna

**FRONTY (QUEUES) - KOMUNIKACE MEZI TASKY (řádky 57-70):**
- ✅ Zmínka o `led_command_queue` byla odstraněna
- ⚠️ Nejsou uvedeny všechny fronty (jen game_command_queue a button_event_queue)

**TASK PRIORITY (řádky 73-101):**
- ✅ Priority jsou správné
- ✅ Popis je správný

### 6.4 freertos_chess.c - Doxygen komentáře

**Status LED GPIO:**
- ✅ Opraveno z GPIO8 na GPIO5

**Time-multiplexing:**
- ✅ Opraveno z 30ms na 25ms
- ✅ Odstraněna zmínka o LED update fázi

---

## 7. Identifikované nesoulady

### 7.1 Kritické nesoulady

1. **README.md - Priority tasků**
   - Všechny priority jsou špatně (kromě animation_task)
   - LED task má prioritu 7, ne 4
   - Button task má prioritu 5, ne 2
   - Game task má prioritu 4, ne 5

2. **README.md - Stack sizes**
   - Všechny stack sizes jsou špatně (kromě animation_task a button_task)
   - LED task má 16KB, ne 8KB
   - Matrix task má 8KB, ne 3KB
   - Web server task má 20KB, ne 6KB

3. **README.md - Time-multiplexing**
   - Cyklus je 25ms, ne 30ms
   - LED update není součástí multiplexing cyklu

### 7.2 Významné nesoulady

1. **main/main.c - Doxygen komentáře**
   - Sekce "FRONTY (QUEUES)" neuvádí všechny fronty
   - Chybí popis všech 18 front

2. **Screen Saver task**
   - Fronty jsou vytvářeny, ale task je DISABLED
   - Dokumentace neuvádí, že screen saver je zakázán

### 7.3 Drobné nesoulady

1. **Legacy timery**
   - `matrix_scan_timer` a `button_scan_timer` jsou vytvářeny, ale nepoužívají se
   - Dokumentace to neuvádí

---

## 8. Doporučení

1. **OKAMŽITĚ OPRAVIT:**
   - README.md - tabulka tasků (priority a stack sizes)
   - README.md - time-multiplexing popis (25ms cyklus)

2. **AKTUALIZOVAT:**
   - main/main.c - Doxygen komentáře - přidat všechny fronty
   - Dokumentace - uvést, že screen saver je DISABLED

3. **VYČISTIT:**
   - Odstranit legacy timery nebo je označit jako nepoužívané
   - Zvážit odstranění screen saver front, pokud task není nikdy spuštěn

---

## 9. Shrnutí

**Skutečný stav:**
- 8 aktivních tasků
- 18 front (včetně 2 pro disabled screen saver)
- 5 mutexů
- 2 aktivní timery (coordinated_multiplex_timer, led_update_timer)
- 2 legacy timery (nepoužívané)
- WDT timeout: 8s (normální), 10s (init)

**Dokumentace:**
- README.md má **kritické nesoulady** v prioritách a stack sizes
- Time-multiplexing popis je zastaralý
- Doxygen komentáře jsou většinou správné (po předchozích opravách)

**Status:** ✅ Analýza dokončena, připraveno k opravě dokumentace

