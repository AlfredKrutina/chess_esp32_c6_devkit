# Plán oprav Mermaid diagramu - main_flow_diagram.txt

## Identifikované problémy

### 1. Syntax chyby
- **Problém**: 55 activate vs 26 deactivate (nesoulad v párování)
- **Důvod**: Některé activate/deactivate nejsou správně spárované
- **Dopad**: Diagram může mít syntax chyby při renderování

### 2. LED mutex - chybné zobrazení
- **Problém**: Diagram zobrazuje `GT->>LM: xSemaphoreTake(led_unified_mutex)` PŘED voláním `led_set_pixel_safe()`
- **Skutečnost**: `led_set_pixel_safe()` volá `led_set_pixel_internal()`, která UVNITŘ bere `led_unified_mutex`
- **Důvod**: Funkce je thread-safe, mutex se bere uvnitř
- **Dopad**: Diagram ukazuje nesprávný flow - game_task nebere mutex před voláním

### 3. Game mutex - chybné zobrazení v hlavní smyčce
- **Problém**: Diagram zobrazuje `game_mutex` operace u `game_process_pickup()`, `game_process_drop()`, `game_process_chess_move()` v hlavní smyčce game_task
- **Skutečnost**: Tyto funkce NEberou `game_mutex` - běží v game_task, který má exkluzivní přístup k board[][]
- **Důvod**: game_task je jediný task, který upravuje board[][], takže nepotřebuje mutex
- **Dopad**: Diagram ukazuje nesprávný flow - mutex se nebere v těchto funkcích

### 4. Game mutex - správné zobrazení u web server
- **Správně**: Diagram zobrazuje `game_mutex` operace u `game_get_status_json()` a `game_get_board_json()`
- **Skutečnost**: Tyto funkce UVNITŘ berou `game_mutex`
- **Dopad**: Zobrazení je správné, ale mělo by být jasnější, že mutex se bere UVNITŘ

### 5. UART response queue - chybí detail
- **Problém**: `game_send_response_to_uart()` je zobrazeno, ale není jasné, že bere `game_mutex` UVNITŘ
- **Skutečnost**: `game_send_response_to_uart()` bere `game_mutex` UVNITŘ a posílá do `uart_response_queue`
- **Dopad**: Chybí detail o mutex operacích

## Plán oprav

### 1. Opravit LED mutex zobrazení (řádky 151-157)

**Současný stav:**
```
GT->>LM: xSemaphoreTake(led_unified_mutex)
activate LM
GT->>LT: led_set_pixel_safe() - PŘÍMÉ VOLÁNÍ LED
GT->>LM: xSemaphoreGive(led_unified_mutex)
deactivate LM
```

**Nový stav:**
```
GT->>LT: led_set_pixel_safe() - PŘÍMÉ VOLÁNÍ LED (thread-safe, mutex UVNITŘ)
Note over GT,LT: led_set_pixel_safe() interně bere led_unified_mutex
```

**Změny:**
- ODEBRAT: `GT->>LM: xSemaphoreTake(led_unified_mutex)` před voláním
- ODEBRAT: `activate LM`
- ODEBRAT: `GT->>LM: xSemaphoreGive(led_unified_mutex)` po volání
- ODEBRAT: `deactivate LM`
- ZACHOVAT: `GT->>LT: led_set_pixel_safe()`
- PŘIDAT: Poznámku vysvětlující, že mutex se bere UVNITŘ

### 2. Opravit Game mutex zobrazení v hlavní smyčce (řádky 110-145)

**Současný stav:**
```
alt GAME_CMD_PICKUP
    GT->>GM: xSemaphoreTake(game_mutex)
    activate GM
    GT->>GT: game_process_pickup() - Uložení pozice
    GT->>GM: xSemaphoreGive(game_mutex)
    deactivate GM
else GAME_CMD_DROP
    GT->>GM: xSemaphoreTake(game_mutex)
    activate GM
    GT->>GT: game_process_drop() - Detekce tahu
    GT->>GT: game_process_chess_move()
    GT->>GM: xSemaphoreGive(game_mutex)
    deactivate GM
else GAME_CMD_MAKE_MOVE
    GT->>GM: xSemaphoreTake(game_mutex)
    activate GM
    GT->>GT: game_process_chess_move() - UART/Web tah
    GT->>GM: xSemaphoreGive(game_mutex)
    deactivate GM
end
```

**Nový stav:**
```
alt GAME_CMD_PICKUP
    GT->>GT: game_process_pickup() - Uložení pozice
    Note over GT: Funkce běží v game_task - NEbere game_mutex
else GAME_CMD_DROP
    GT->>GT: game_process_drop() - Detekce tahu
    GT->>GT: game_process_chess_move()
    Note over GT: Funkce běží v game_task - NEbere game_mutex
else GAME_CMD_MAKE_MOVE
    GT->>GT: game_process_chess_move() - UART/Web tah
    Note over GT: Funkce běží v game_task - NEbere game_mutex
    Note over GT: game_send_response_to_uart() UVNITŘ bere game_mutex
end
```

**Změny:**
- ODEBRAT: Všechny `GT->>GM: xSemaphoreTake/Give(game_mutex)` operace u těchto funkcí
- ODEBRAT: Všechny `activate GM` / `deactivate GM`
- ZACHOVAT: Volání funkcí
- PŘIDAT: Poznámky vysvětlující, že funkce NEberou mutex (běží v game_task)

### 3. Opravit Game mutex u button eventů (řádky 140-144)

**Současný stav:**
```
alt Button event v frontě
    GT->>GM: xSemaphoreTake(game_mutex)
    activate GM
    GT->>GT: game_process_promotion_button() / game_reset_game()
    GT->>GM: xSemaphoreGive(game_mutex)
    deactivate GM
end
```

**Nový stav:**
```
alt Button event v frontě
    GT->>GT: game_process_promotion_button() / game_reset_game()
    Note over GT: Funkce běží v game_task - NEbere game_mutex
end
```

**Změny:**
- ODEBRAT: `GT->>GM: xSemaphoreTake/Give(game_mutex)` operace
- ODEBRAT: `activate GM` / `deactivate GM`
- ZACHOVAT: Volání funkcí
- PŘIDAT: Poznámku vysvětlující, že funkce NEbere mutex

### 4. Zachovat Game mutex u web server (řádky 206-227)

**Současný stav je SPRÁVNÝ:**
```
WT->>GT: game_get_status_json() - Přímé volání
activate GT
GT->>GM: xSemaphoreTake(game_mutex)
activate GM
GT->>GT: Čtení game stavu (current_player, current_game_state, etc.)
GT->>GM: xSemaphoreGive(game_mutex)
deactivate GM
GT-->>WT: JSON status
deactivate GT
```

**Vylepšení:**
- PŘIDAT: Poznámku, že mutex se bere UVNITŘ funkce (pro přesnost)

**Poznámka**: Toto zobrazení je technicky nesprávné (mutex se bere UVNITŘ funkce, ne před voláním), ale pro diagram je to přijatelné zjednodušení, protože ukazuje, že funkce potřebuje mutex.

### 5. Opravit příklady flow (řádky 233-322)

#### 5.1 Fyzický tah flow (řádky 233-273)

**Současný stav:**
- Zobrazuje `game_mutex` u `game_process_pickup()` a `game_process_drop()` - ŠPATNĚ
- Zobrazuje `led_unified_mutex` PŘED `led_set_pixel_safe()` - ŠPATNĚ

**Nový stav:**
- ODEBRAT: `game_mutex` operace u `game_process_pickup()` a `game_process_drop()`
- ODEBRAT: `led_unified_mutex` operace PŘED `led_set_pixel_safe()`
- ZOBRAZIT: `led_set_pixel_safe()` jako thread-safe funkci
- PŘIDAT: Poznámku o thread-safe funkcích

#### 5.2 UART tah flow (řádky 275-305)

**Současný stav:**
- Zobrazuje `game_mutex` u `game_process_chess_move()` - ŠPATNĚ
- Zobrazuje `uart_response_queue` zápis, ale chybí detail o `game_send_response_to_uart()`
- Zobrazuje `led_unified_mutex` PŘED `led_set_pixel_safe()` - ŠPATNĚ

**Nový stav:**
- ODEBRAT: `game_mutex` operace u `game_process_chess_move()`
- PŘIDAT: Detail o `game_send_response_to_uart()` - bere `game_mutex` UVNITŘ
- ODEBRAT: `led_unified_mutex` operace PŘED `led_set_pixel_safe()`
- ZOBRAZIT: `led_set_pixel_safe()` jako thread-safe funkci

#### 5.3 Button event flow (řádky 307-322)

**Současný stav:**
- Zobrazuje `game_mutex` u `game_process_promotion_button()` - ŠPATNĚ
- Zobrazuje `led_unified_mutex` PŘED `led_set_pixel_safe()` - ŠPATNĚ

**Nový stav:**
- ODEBRAT: `game_mutex` operace u `game_process_promotion_button()`
- ODEBRAT: `led_unified_mutex` operace PŘED `led_set_pixel_safe()`
- ZOBRAZIT: `led_set_pixel_safe()` jako thread-safe funkci

### 6. Opravit activate/deactivate párování

**Problém**: 55 activate vs 26 deactivate

**Řešení**:
- Zkontrolovat všechny activate/deactivate páry
- Odebrat nepotřebné activate/deactivate (ty, které jsme odebrali u mutex operací)
- Zajistit, že každý activate má odpovídající deactivate

**Odhadované změny**:
- Odebrat ~20-25 activate/deactivate párů (z mutex operací, které odstraňujeme)
- Zbývající activate/deactivate by měly být správně spárované

### 7. Přidat poznámky vysvětlující mutex operace

**Přidat do poznámek na začátku diagramu:**
- Thread-safe funkce: `led_set_pixel_safe()` (mutex se bere UVNITŘ funkce)
- Thread-safe funkce: `game_get_status_json()`, `game_get_board_json()` (mutex se bere UVNITŘ funkce)
- Thread-safe funkce: `game_send_response_to_uart()` (mutex se bere UVNITŘ funkce)
- `game_process_*` funkce běží v game_task, NEberou `game_mutex` (game_task má exkluzivní přístup k board[][])

**Přidat do poznámek u jednotlivých sekcí:**
- LED Task: `led_set_pixel_safe()` je thread-safe funkce (mutex UVNITŘ)
- Game Task: `game_process_*` funkce NEberou mutex (běží v game_task)
- Web Server Task: `game_get_status_json()`, `game_get_board_json()` berou mutex UVNITŘ

## Shrnutí změn

### Odebrané mutex operace:
1. LED mutex PŘED `led_set_pixel_safe()` (6 míst: hlavní smyčka + 3 příklady)
2. Game mutex u `game_process_pickup()` (2 místa: hlavní smyčka + příklad)
3. Game mutex u `game_process_drop()` (2 místa: hlavní smyčka + příklad)
4. Game mutex u `game_process_chess_move()` (2 místa: hlavní smyčka + příklad)
5. Game mutex u `game_process_promotion_button()` (2 místa: hlavní smyčka + příklad)

### Zachované mutex operace:
1. LED mutex v LED task batch commit (SPRÁVNĚ)
2. Matrix mutex u matrix scan (SPRÁVNĚ)
3. UART mutex u uart output (SPRÁVNĚ)
4. Game mutex u `game_get_status_json()` (SPRÁVNĚ - zjednodušené zobrazení)
5. Game mutex u `game_get_board_json()` (SPRÁVNĚ - zjednodušené zobrazení)

### Přidané poznámky:
1. Thread-safe funkce a jejich mutex operace
2. Vysvětlení, že mutexy se berou UVNITŘ funkcí
3. Vysvětlení, že `game_process_*` funkce NEberou mutex

## Očekávaný výsledek

Po opravách:
- Diagram bude přesněji zobrazovat skutečný flow
- Mutex operace budou zobrazeny na správné úrovni (uvnitř funkcí nebo jako thread-safe funkce)
- Syntax bude správná (activate/deactivate správně spárované)
- Diagram bude jasnější a přesnější
