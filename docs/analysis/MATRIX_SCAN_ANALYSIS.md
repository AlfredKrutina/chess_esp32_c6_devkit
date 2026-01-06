# Hluboká analýza logiky scanování a flow - Nalezené problémy

## Kritické problémy

### 🔴 PROBLÉM #1: DROP s from_notation není zpracováván správně

**Lokace:** `components/game_task/game_task.c` - `game_process_drop_command()`

**Popis:**
- Matrix task posílá DROP s `from_notation` vyplněným při kompletním tahu (`matrix_send_drop_command_with_from()`)
- Ale `game_process_drop_command()` **vždy používá `lifted_piece_row/col`** místo `cmd->from_notation`!
- Pokud se pošle DROP s `from_notation="e2"` a `to_notation="e4"`, ale `lifted_piece_row/col` je z jiného PICKUP (např. "a1"), provede se špatný tah!

**Kód:**
```c
// Řádek 3712-3719
chess_move_t move = {
    .from_row = lifted_piece_row,  // ❌ MĚLO BY BÝT: cmd->from_notation pokud je vyplněné!
    .from_col = lifted_piece_col,  // ❌ MĚLO BY BÝT: cmd->from_notation pokud je vyplněné!
    .to_row = to_row,
    .to_col = to_col,
    .piece = lifted_piece,
    ...
};
```

**Důsledek:**
- Pokud matrix pošle PICKUP z "a1" a pak DROP s from="e2" to="e4", provede se tah "a1->e4" místo "e2->e4"!
- To způsobuje špatné tahy a nekonzistentní stav boardu.

**Řešení:**
- Zkontrolovat, zda `cmd->from_notation` je vyplněné
- Pokud ano, použít `convert_notation_to_coords(cmd->from_notation, &from_row, &from_col)` místo `lifted_piece_row/col`
- Pokud ne, použít `lifted_piece_row/col` (pro UART flow)

---

### 🔴 PROBLÉM #2: DROP bez from může selhat pokud timeout resetoval last_piece_lifted

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_detect_moves()`

**Popis:**
- Pokud se zvedne figurka a timeout (5s) nastane před položením, `last_piece_lifted` se resetuje na 255
- Pak když se figurka položí, pošle se DROP bez from (`matrix_send_drop_command()`)
- Ale `game_process_drop_command()` očekává `piece_lifted == true` a `lifted_piece_row/col` nastavené
- Pokud timeout resetoval `last_piece_lifted` v matrix_task, ale `piece_lifted` v game_task je stále true z předchozího PICKUP, může dojít k nekonzistenci

**Kód:**
```c
// Řádek 463-470 v matrix_task.c
if (last_piece_lifted != 255 && last_piece_lifted != piece_placed) {
    matrix_send_drop_command_with_from(last_piece_lifted, piece_placed);
    last_piece_lifted = 255;
} else {
    matrix_send_drop_command(piece_placed);  // ❌ Může selhat pokud piece_lifted není nastaveno!
}
```

**Důsledek:**
- Pokud timeout resetuje `last_piece_lifted` před DROP, pošle se DROP bez from
- `game_process_drop_command()` může použít staré `lifted_piece_row/col` z jiného PICKUP
- To způsobuje špatné tahy

**Řešení:**
- Nikdy neposílat DROP bez from z matrix_task
- Pokud `last_piece_lifted == 255`, ignorovat DROP nebo použít poslední známou pozici
- Nebo posílat DROP s from i když je timeout (z posledního známého PICKUP)

---

### 🟡 PROBLÉM #3: Race condition při posílání příkazů do queue

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_send_pickup_command()`, `matrix_send_drop_command()`

**Popis:**
- Helper funkce volají `xQueueSend()` s timeoutem 100ms
- Pokud queue je plná nebo game_task je zaneprázdněný, příkaz se může ztratit
- Neexistuje žádná retry logika nebo error handling kromě logování

**Kód:**
```c
// Řádek 343-347
if (xQueueSend(game_command_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    ESP_LOGI(TAG, "PICKUP command sent...");
} else {
    ESP_LOGW(TAG, "Failed to send PICKUP command...");  // ❌ Jen logování, žádná retry!
}
```

**Důsledek:**
- Příkazy se mohou ztratit pokud queue je plná
- Matrix state a game state se mohou rozsynchronizovat
- Tahy se mohou ztratit

**Řešení:**
- Přidat retry logiku s exponenciálním backoff
- Nebo použít větší timeout
- Nebo použít non-blocking queue s bufferem

---

### 🟡 PROBLÉM #4: Mutex není držen během posílání příkazů do queue

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_scan_all()`, `matrix_detect_moves()`

**Popis:**
- `matrix_scan_all()` drží mutex během scanování a detekce změn
- `matrix_detect_moves()` je volána uvnitř mutexu
- Ale `matrix_send_pickup_command()` a `matrix_send_drop_command()` volají `xQueueSend()`, které může blokovat
- Pokud `xQueueSend()` blokuje, mutex je držen dlouho, což může způsobit problémy

**Kód:**
```c
// Řádek 248-279
if (xSemaphoreTake(matrix_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // ... scanování ...
    matrix_detect_moves();  // Volá matrix_send_pickup_command() -> xQueueSend() -> může blokovat!
    // ...
    xSemaphoreGive(matrix_mutex);
}
```

**Důsledek:**
- Mutex je držen během queue send, což může způsobit zpoždění
- Pokud queue send blokuje, další scanování může být zpožděno
- Může způsobit timeout při získávání mutexu

**Řešení:**
- Posílat příkazy do queue PŘED získáním mutexu nebo PO uvolnění mutexu
- Nebo použít non-blocking queue send s timeoutem
- Nebo použít separátní task pro posílání příkazů

---

### 🟡 PROBLÉM #5: Duplicitní PICKUP příkazy bez DROP

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_detect_moves()`

**Popis:**
- Pokud se zvedne figurka, pošle se PICKUP a nastaví se `last_piece_lifted`
- Pokud se zvedne další figurka před položením první, pošle se další PICKUP
- Ale `last_piece_lifted` se přepíše na novou hodnotu
- První PICKUP se "ztratí" - game_task může mít `piece_lifted=true` z prvního PICKUP, ale `last_piece_lifted` v matrix_task je z druhého

**Kód:**
```c
// Řádek 439-451
if (piece_lifted != 255) {
    last_piece_lifted = piece_lifted;  // ❌ Přepíše předchozí hodnotu!
    matrix_send_pickup_command(piece_lifted);
}
```

**Důsledek:**
- Pokud se zvednou 2 figurky rychle za sebou, první PICKUP se ztratí
- Game state může být nekonzistentní
- DROP může použít špatnou pozici

**Řešení:**
- Ignorovat další PICKUP pokud `last_piece_lifted != 255`
- Nebo resetovat `piece_lifted` v game_task před novým PICKUP
- Nebo použít queue pro tracking všech PICKUP příkazů

---

### 🟡 PROBLÉM #6: Vrácení figurky na stejné pole není správně zpracováno

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_detect_moves()`

**Popis:**
- Pokud se figurka zvedne a položí na stejné pole, podmínka `last_piece_lifted != piece_placed` je false
- Pošle se DROP bez from (`matrix_send_drop_command()`)
- Ale `game_process_drop_command()` očekává `piece_lifted == true` a provede kontrolu zrušení tahu
- To může fungovat, ale je to nekonzistentní s logikou kompletního tahu

**Kód:**
```c
// Řádek 463-470
if (last_piece_lifted != 255 && last_piece_lifted != piece_placed) {
    matrix_send_drop_command_with_from(last_piece_lifted, piece_placed);
} else {
    matrix_send_drop_command(piece_placed);  // ❌ Pro vrácení na stejné pole
}
```

**Důsledek:**
- Vrácení na stejné pole pošle DROP bez from
- Game_task musí správně zpracovat zrušení tahu
- Může být nekonzistentní pokud se očekává kompletní tah

**Řešení:**
- Explicitně zpracovat vrácení na stejné pole - poslat DROP s from=to
- Nebo ignorovat DROP pokud from==to v matrix_task

---

## Střední problémy

### 🟠 PROBLÉM #7: Fallback path bez mutexu může způsobit data corruption

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_scan_all()`

**Popis:**
- Pokud mutex není dostupný, používá se fallback path bez mutexu
- To může způsobit race condition pokud game_task čte matrix_state současně

**Kód:**
```c
// Řádek 284-305
} else {
    // Fallback if mutex not available
    for (int row = 0; row < 8; row++) {
        matrix_scan_row_internal(row);  // ❌ Bez mutexu!
    }
    // ...
}
```

**Důsledek:**
- Data corruption pokud game_task čte matrix_state během scanování
- Nekonzistentní stav

**Řešení:**
- Vždy používat mutex nebo fail gracefully
- Nebo použít atomické operace

---

### 🟠 PROBLÉM #8: Timeout kontrola je v matrix_detect_moves() ale může být zpožděná

**Lokace:** `components/matrix_task/matrix_task.c` - `matrix_detect_moves()`

**Popis:**
- Timeout kontrola je na konci `matrix_detect_moves()`
- Ale `matrix_detect_moves()` je volána pouze při scanování (každých ~20ms)
- Pokud scanování je zpožděno, timeout může být zpožděn také

**Kód:**
```c
// Řádek 473-480
if (last_piece_lifted != 255) {
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time > move_detection_timeout) {
        last_piece_lifted = 255;  // Reset
    }
}
```

**Důsledek:**
- Timeout může být nepřesný kvůli zpoždění scanování
- Může způsobit zpožděné resetování stavu

**Řešení:**
- Použít timer callback pro timeout místo kontroly v scan loop
- Nebo kontrolovat timeout častěji

---

## Nízké problémy

### 🟢 PROBLÉM #9: Coordinate conversion vypadá správně, ale mělo by být ověřeno

**Lokace:** `components/matrix_task/matrix_task.c`, `components/game_task/game_task.c`

**Popis:**
- `matrix_square_to_notation()` používá: `row = square / 8`, `col = square % 8`
- `convert_notation_to_coords()` používá: `*col = notation[0] - 'a'`, `*row = notation[1] - '1'`
- To vypadá konzistentně, ale mělo by být ověřeno testy

**Důsledek:**
- Pokud je chyba v coordinate conversion, všechny tahy budou špatné
- Mělo by být ověřeno unit testy

**Řešení:**
- Vytvořit unit testy pro coordinate conversion
- Ověřit všechny kombinace square index <-> notation <-> row/col

---

## Dodatečné problémy

### 🔴 PROBLÉM #10: game_process_drop_command() nikdy nepoužívá cmd->from_notation

**Lokace:** `components/game_task/game_task.c` - `game_process_drop_command()`

**Popis:**
- `game_process_drop_command()` **NIKDE nekontroluje** zda `cmd->from_notation` je vyplněné
- Vždy používá `lifted_piece_row/col` z předchozího PICKUP
- To znamená, že když matrix pošle DROP s `from_notation="e2"` a `to_notation="e4"`, ignoruje se `from_notation` a použije se `lifted_piece_row/col`!

**Kód:**
```c
// Řádek 3711-3720 - NIKDE není kontrola cmd->from_notation!
chess_move_t move = {
    .from_row = lifted_piece_row,  // ❌ VŽDY použije lifted_piece_row!
    .from_col = lifted_piece_col,  // ❌ VŽDY použije lifted_piece_col!
    .to_row = to_row,
    .to_col = to_col,
    ...
};
```

**Důsledek:**
- Matrix pošle: PICKUP "a1" -> DROP from="e2" to="e4"
- Game_task provede: tah "a1->e4" místo "e2->e4"!
- To je přesně problém, který uživatel popisuje - tahy se provádějí z špatné pozice!

**Řešení:**
- Přidat kontrolu na začátku `game_process_drop_command()`:
  ```c
  uint8_t from_row, from_col;
  if (strlen(cmd->from_notation) > 0) {
      // Matrix poslal kompletní tah s from_notation
      if (!convert_notation_to_coords(cmd->from_notation, &from_row, &from_col)) {
          ESP_LOGE(TAG, "❌ Invalid from_notation: %s", cmd->from_notation);
          return;
      }
      // Použít from_notation místo lifted_piece_row/col
  } else {
      // UART flow - použít lifted_piece_row/col
      if (!piece_lifted) {
          ESP_LOGE(TAG, "❌ No piece was lifted");
          return;
      }
      from_row = lifted_piece_row;
      from_col = lifted_piece_col;
  }
  ```

---

## Shrnutí

### Kritické problémy (MUSÍ být opraveny okamžitě):
1. **🔴 DROP s from_notation není zpracováván** - `game_process_drop_command()` vždy používá `lifted_piece_row/col` místo `cmd->from_notation`
2. **🔴 DROP bez from může selhat** pokud timeout resetoval `last_piece_lifted`

### Střední problémy (měly by být opraveny):
3. Race condition při posílání příkazů do queue
4. Mutex není držen správně během queue send
5. Duplicitní PICKUP příkazy bez DROP
6. Vrácení figurky na stejné pole není správně zpracováno
7. Fallback path bez mutexu
8. Timeout kontrola může být zpožděná

### Nízké problémy (měly by být ověřeny):
9. Coordinate conversion by mělo být ověřeno testy

## Prioritizace oprav

### Priorita 1 (Kritické - opravit okamžitě):
1. **Opravit `game_process_drop_command()`** aby používala `cmd->from_notation` pokud je vyplněné
2. **Opravit `matrix_detect_moves()`** aby nikdy neposílala DROP bez from (vždy poslat s from i když je timeout)

### Priorita 2 (Důležité - opravit brzy):
3. Přidat retry logiku pro queue send
4. Přesunout queue send mimo mutex
5. Ignorovat duplicitní PICKUP pokud `last_piece_lifted != 255`

### Priorita 3 (Vylepšení - opravit později):
6. Explicitně zpracovat vrácení na stejné pole
7. Odstranit fallback path bez mutexu
8. Použít timer callback pro timeout
9. Vytvořit unit testy pro coordinate conversion

