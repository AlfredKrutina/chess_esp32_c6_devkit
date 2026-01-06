# Analýza problému s historií tahů pomocí šipek nahoru/dolů

## Přehled problému

V UART rozhraní existuje historie příkazů (`command_history_t`), ale **není implementováno zpracování ANSI escape sekvencí pro šipky nahoru/dolů**, které by měly umožnit procházení historie příkazů.

## Současný stav

### Co funguje:
1. ✅ Historie příkazů je implementována (`command_history_t`)
2. ✅ Příkazy se přidávají do historie po zadání (`command_history_add()`)
3. ✅ Existují funkce pro získání předchozího/následujícího příkazu:
   - `command_history_get_previous()`
   - `command_history_get_next()`

### Co nefunguje:
1. ❌ **ANSI escape sekvence pro šipky nejsou zpracovávány**
2. ❌ **Funkce pro procházení historie nejsou volány z `uart_process_input()`**
3. ❌ **Není implementován stavový stroj pro detekci escape sekvencí**

## Detailní analýza

### Současná implementace `uart_process_input()`

```c
void uart_process_input(char c) {
  if (c == '\r' || c == '\n') {
    // Process command
    // ...
  } else if (c == '\b' || c == 127) {
    // Backspace
    // ...
  } else if (c >= 32 && c <= 126) {
    // Printable character
    // ...
  }
  // ❌ CHYBÍ: Zpracování ANSI escape sekvencí
}
```

### ANSI escape sekvence pro šipky

Když uživatel stiskne šipku nahoru/dolů, terminál pošle:
- **Arrow Up**: `\033[A` (3 byty: ESC, '[', 'A')
- **Arrow Down**: `\033[B` (3 byty: ESC, '[', 'B')

Kde:
- `\033` = `0x1B` = ESC character
- `[` = `0x5B`
- `A` = `0x41` (Arrow Up)
- `B` = `0x42` (Arrow Down)

### Problém v implementaci

1. **Chybí detekce ESC znaku**: `uart_process_input()` nezpracovává `CHAR_ESC` (0x1B)
2. **Chybí stavový stroj**: Není implementován stavový stroj pro detekci escape sekvencí
3. **Chybí volání historie**: Funkce `command_history_get_previous()` a `command_history_get_next()` existují, ale nejsou volány

## Návrh řešení

### 1. Přidat stavový stroj pro escape sekvence

```c
typedef enum {
  ESC_STATE_NONE,
  ESC_STATE_ESC,      // ESC znak přijat
  ESC_STATE_BRACKET   // ESC[ přijato, čekáme na finální znak
} esc_state_t;

static esc_state_t esc_state = ESC_STATE_NONE;
static int history_navigation_index = -1; // -1 = nejsme v navigaci
```

### 2. Rozšířit `uart_process_input()` o zpracování escape sekvencí

```c
void uart_process_input(char c) {
  // Zpracování escape sekvencí
  if (esc_state == ESC_STATE_NONE && c == CHAR_ESC) {
    esc_state = ESC_STATE_ESC;
    return; // Čekáme na další znak
  }
  
  if (esc_state == ESC_STATE_ESC) {
    if (c == '[') {
      esc_state = ESC_STATE_BRACKET;
      return; // Čekáme na finální znak
    } else {
      // Neplatná escape sekvence, resetovat
      esc_state = ESC_STATE_NONE;
      // Pokračovat normálním zpracováním
    }
  }
  
  if (esc_state == ESC_STATE_BRACKET) {
    esc_state = ESC_STATE_NONE; // Resetovat stav
    
    if (c == 'A') {
      // Arrow Up - předchozí příkaz z historie
      handle_arrow_up();
      return;
    } else if (c == 'B') {
      // Arrow Down - následující příkaz z historie
      handle_arrow_down();
      return;
    }
    // Jiná escape sekvence - ignorovat
    return;
  }
  
  // Původní zpracování...
  if (c == '\r' || c == '\n') {
    // ...
  }
  // ...
}
```

### 3. Implementovat funkce pro navigaci v historii

```c
void handle_arrow_up(void) {
  if (command_history.count == 0) {
    return; // Žádná historie
  }
  
  // Pokud jsme na začátku navigace, začneme od posledního příkazu
  if (history_navigation_index < 0) {
    history_navigation_index = command_history.count - 1;
  } else if (history_navigation_index > 0) {
    history_navigation_index--;
  }
  
  // Získat příkaz z historie
  int idx = (command_history.current - command_history.count + 
             history_navigation_index + command_history.max_size) % 
             command_history.max_size;
  
  const char *cmd = command_history.commands[idx];
  
  // Zobrazit příkaz v input bufferu
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;
  
  // Aktualizovat zobrazení v terminálu
  refresh_input_display();
}

void handle_arrow_down(void) {
  if (command_history.count == 0) {
    return; // Žádná historie
  }
  
  if (history_navigation_index < 0) {
    return; // Už jsme na konci
  }
  
  history_navigation_index++;
  
  if (history_navigation_index >= command_history.count) {
    // Jsme na konci historie - vyčistit input
    history_navigation_index = -1;
    input_buffer_clear(&input_buffer);
    refresh_input_display();
    return;
  }
  
  // Získat příkaz z historie
  int idx = (command_history.current - command_history.count + 
             history_navigation_index + command_history.max_size) % 
             command_history.max_size;
  
  const char *cmd = command_history.commands[idx];
  
  // Zobrazit příkaz v input bufferu
  input_buffer_clear(&input_buffer);
  strncpy(input_buffer.buffer, cmd, UART_CMD_BUFFER_SIZE - 1);
  input_buffer.buffer[UART_CMD_BUFFER_SIZE - 1] = '\0';
  input_buffer.length = strlen(input_buffer.buffer);
  input_buffer.pos = input_buffer.length;
  
  // Aktualizovat zobrazení v terminálu
  refresh_input_display();
}
```

### 4. Implementovat funkci pro aktualizaci zobrazení

```c
void refresh_input_display(void) {
  // Vymazat aktuální řádek
  if (UART_ENABLED) {
    uart_write_bytes(UART_PORT_NUM, ANSI_CLEAR_LINE, strlen(ANSI_CLEAR_LINE));
    // Zobrazit prompt
    if (color_enabled) {
      uart_write_bytes(UART_PORT_NUM, "\033[1;33m", 7);
    }
    // Zobrazit input buffer
    uart_write_bytes(UART_PORT_NUM, input_buffer.buffer, input_buffer.length);
    if (color_enabled) {
      uart_write_bytes(UART_PORT_NUM, "\033[0m", 4);
    }
  } else {
    printf(ANSI_CLEAR_LINE);
    if (color_enabled) {
      printf("\033[1;33m");
    }
    printf("%s", input_buffer.buffer);
    if (color_enabled) {
      printf("\033[0m");
    }
    fflush(stdout);
  }
}
```

### 5. Resetovat navigační index při zadání příkazu

```c
void uart_process_input(char c) {
  if (c == '\r' || c == '\n') {
    // Process command
    if (input_buffer.length > 0) {
      // ...
      // Resetovat navigační index
      history_navigation_index = -1;
      // ...
    }
  }
  // ...
}
```

## Potenciální problémy

### 1. Částečné escape sekvence
**Problém:** Pokud přijde pouze část escape sekvence (např. pouze ESC), může to způsobit problémy.

**Řešení:** Přidat timeout pro escape sekvence - pokud do 100ms nepřijde další znak, resetovat stav.

### 2. Konflikt s jinými escape sekvencemi
**Problém:** Terminál může posílat jiné escape sekvence (např. pro barvy).

**Řešení:** Ignorovat neznámé escape sekvence, zpracovávat pouze Arrow Up/Down.

### 3. Circular buffer indexování
**Problém:** `command_history` používá circular buffer, indexování může být složité.

**Řešení:** Použít správné modulo aritmetiky pro circular buffer.

## Testovací scénáře

1. **Základní navigace:**
   - Zadat několik příkazů
   - Stisknout Arrow Up → měl by se zobrazit poslední příkaz
   - Stisknout Arrow Up znovu → měl by se zobrazit předposlední příkaz
   - Stisknout Arrow Down → měl by se zobrazit poslední příkaz

2. **Hranice historie:**
   - Stisknout Arrow Up na začátku → měl by zůstat první příkaz
   - Stisknout Arrow Down na konci → měl by vyčistit input

3. **Prázdná historie:**
   - Stisknout Arrow Up/Down bez historie → nemělo by se nic stát

4. **Částečné escape sekvence:**
   - Odeslat pouze ESC → měl by se resetovat po timeoutu

## Priorita

**VYSOKÁ** - Toto je základní funkce pro použitelnost UART rozhraní. Bez této funkce je práce s terminálem nepohodlná.

## Související soubory

- `components/uart_task/uart_task.c` - hlavní implementace
- `components/uart_task/include/uart_task.h` - hlavičkové soubory

## Poznámky

- Historie příkazů je implementována, ale není využívána
- Funkce pro získání příkazů z historie existují, ale nejsou volány
- ANSI escape sekvence jsou standardní způsob komunikace s terminálem
- Implementace by měla být kompatibilní s různými terminály (PuTTY, minicom, screen, atd.)

