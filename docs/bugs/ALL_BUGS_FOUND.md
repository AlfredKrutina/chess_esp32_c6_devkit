# 🐛 KOMPLETNÍ SEZNAM VŠECH NALEZENÝCH BUGŮ

## ⚠️ KRITICKÉ BUGY (MUSÍ se opravit)

### 🐛 BUG #4: En Passant - Check validation NEDETEKUJE správně!
**Lokace:** `game_would_move_leave_king_in_check()` řádek 1629

**Problém:**
```c
bool game_would_move_leave_king_in_check(const chess_move_t* move)
{
    // Make the move temporarily
    board[move->to_row][move->to_col] = original_from_piece;
    board[move->from_row][move->from_col] = PIECE_EMPTY;
    
    // ❌ CHYBÍ: Odstranění en passant pěšce!
    // Pro en passant musí odstranit pěšce z en_passant_victim_row/col, ne z to_row/to_col!
}
```

**Scénář:**
1. Bílý pěšec d5
2. Černý pěšec e7 → e5 (2 pole)
3. Bílý pěšec d5 → e6 (en passant)
4. **ALE:** Když validujeme check, simulujeme jen d5→e6
5. **NENÍ ODSTRÁNĚN** černý pěšec z e5!
6. Check validation je **ŠPATNÁ**!

**Důsledek:** En passant tahy mohou být povolené i když by ponechaly krále v šachu!

**Fix:** Funkce musí používat `chess_move_extended_t` nebo musí dostat informaci o en passant!

---

### 🐛 BUG #5: Pěšec - Blokování kontroly i pro single square!
**Lokace:** `game_validate_pawn_move_enhanced()` řádek 1427

**Problém:**
```c
// Single square move
if (row_diff == direction && game_is_empty(move->to_row, move->to_col)) {
    return MOVE_ERROR_NONE;  // ✅ OK
}

// Double square move...
if (...) {
    return MOVE_ERROR_NONE;
}

// ✅ OPRAVA BUG #1: Check if path is blocked
if (abs(row_diff) > 0 && !game_is_empty(move->from_row + direction, move->from_col)) {
    return MOVE_ERROR_BLOCKED_PATH;  // ❌ VYKONÁ SE I PRO SINGLE SQUARE!
}
```

**Scénář:**
1. Bílý pěšec na e2
2. Bílá figurka na e3
3. Pokus o tah e2 → e3 (single square)
4. `row_diff == direction` (1 == 1): ✅ true
5. Ale `game_is_empty(e3, e4)` = FALSE (figurka tam je)
6. První podmínka selže
7. Druhá podmínka (double) také selže
8. **Pak přijde třetí kontrola:**
   - `abs(row_diff) > 0` = true
   - `!game_is_empty(e2 + 1, e2)` = `!game_is_empty(e3, e2)` = false (prázdné)
   - Vrátí BLOCKED_PATH ❌

**ALE VLASTNĚ:** To je správně! Když je na e3 figurka, e2→e3 nemůže být valid.

**JINÝ SCÉNÁŘ:**
1. Bílý pěšec na e3
2. Pokus o tah e3 → e2 (zpět!) - `row_diff = -1`, `direction = 1`
3. První kontrola selže (`-1 != 1`)
4. Druhá kontrola selže
5. Třetí kontrola: `abs(-1) > 0` = true
6. `!game_is_empty(e3 + 1, e3)` = `!game_is_empty(e4, e3)` - může být false
7. Vrátí BLOCKED_PATH ❌ **ALE to je správně!**

**VE SKUTEČNOSTI:** Tato logika je **SPRÁVNÁ**! Kontroluje blokování i pro zpětné tahy (což jsou invalid).

---

### 🐛 BUG #6: Střelec - While loop může selhat na neplatných diagonálách!
**Lokace:** `game_validate_bishop_move_enhanced()` řádek 1512

**Problém:**
```c
while (current_row != move->to_row || current_col != move->to_col) {
    if (!game_is_empty(current_row, current_col)) {
        return MOVE_ERROR_BLOCKED_PATH;
    }
    current_row += row_step;
    current_col += col_step;
}
```

**Edge case:**
1. Střelec na a1
2. Pokus o tah a1 → h2 (NE diagonála! row_diff=1, col_diff=7)
3. Před tímto while loop je kontrola: `if (abs_row_diff != abs_col_diff) return INVALID`
4. **TAKŽE:** Tato kontrola už byla provedena ✅

**ALE:** Co když někdo zavolá tuto funkci s neplatnou diagonálou? Loop může běžet navždy!

**Scénář nekonečného loopu:**
- Pokud kontrola `abs_row_diff != abs_col_diff` NENÍ provedena
- Loop běží: current_row += row_step, current_col += col_step
- Pokud `current_row` a `current_col` nikdy nedosáhnou cíle (nejsou na diagonále)
- Loop běží navždy!

**Fix:** Přidat safety check nebo ověřit že loop konverguje.

---

### 🐛 BUG #7: Pěšec - Double square může být blokovaný, ale kontrola je špatně!
**Lokace:** `game_validate_pawn_move_enhanced()` řádek 1427

**Scénář:**
1. Bílý pěšec na e2
2. Bílá figurka na e4 (cílová pozice)
3. Pokus o tah e2 → e4

**Co se stane:**
1. První kontrola (single): `row_diff == direction` = `2 == 1` = FALSE ❌
2. Druhá kontrola (double):
   - `row_diff == 2 * direction` = `2 == 2` = TRUE ✅
   - `move->from_row == start_row` = `1 == 1` = TRUE ✅
   - `game_is_empty(e2 + 1, e2)` = `game_is_empty(e3, e2)` - pokud prázdné = TRUE ✅
   - `game_is_empty(e4, e2)` = FALSE (figurka tam je!) ❌
   - Vrátí MOVE_ERROR_NONE? ❌ NÉ, vrátí se dál...

**VE SKUTEČNOSTI:** Pokud je na e4 figurka, `game_is_empty(e4, e2)` vrací FALSE, takže druhá kontrola také selže ✅

**Pak přijde třetí kontrola:**
- `abs(row_diff) > 0` = true
- `!game_is_empty(e2 + 1, e2)` = `!game_is_empty(e3, e2)` - pokud prázdné, vrátí false
- Vrátí INVALID_PATTERN ✅

**TAKŽE:** Tato logika je vlastně správná!

---

## ⚠️ STŘEDNÍ BUGY (MĚLO BY se opravit)

### 🐛 BUG #8: Pěšec - Zpětný tah není správně detekován jako invalid!
**Lokace:** `game_validate_pawn_move_enhanced()` řádek 1411-1434

**Scénář:**
1. Bílý pěšec na e4
2. Pokus o tah e4 → e3 (zpět)

**Co se stane:**
1. `col_diff == 0` = TRUE ✅
2. `row_diff == direction` = `-1 == 1` = FALSE ❌
3. `row_diff == 2 * direction` = `-1 == 2` = FALSE ❌
4. Třetí kontrola: `abs(-1) > 0` = true
   - `!game_is_empty(e4 + 1, e4)` = `!game_is_empty(e5, e4)` - může být false
   - Vrátí BLOCKED_PATH ❌ **ALE to je špatně!**

**Problém:** Zpětný tah pěšce by měl být INVALID_PATTERN, ne BLOCKED_PATH!

**Fix:** Přidat kontrolu směru před kontrolu blokování:
```c
// Check if moving backwards
if (row_diff * direction < 0) {
    return MOVE_ERROR_INVALID_PATTERN;
}
```

---

## 🔍 JINÉ POTENCIÁLNÍ PROBLÉMY

### ⚠️ POZNÁMKA #1: Check validation pro en passant
V `game_would_move_leave_king_in_check()` se nepoužívá `move->move_type`, takže en passant není správně simulován!

**Řešení:**
1. Buď změnit signature na `chess_move_extended_t*`
2. Nebo přidat parametr `bool is_en_passant`
3. Nebo kontrolovat `game_is_en_passant_possible(move)` v této funkci

---

### ⚠️ POZNÁMKA #2: Střelec while loop safety
While loop by měl mít timeout nebo kontrolu že konverguje k cíli.

**Současný stav:**
- Loop může teoreticky běžet navždy pokud je volán s neplatnými parametry
- ALE předchozí kontrola `abs_row_diff != abs_col_diff` to už zachytí

**Bezpečnostní fix:**
```c
int steps = 0;
while ((current_row != move->to_row || current_col != move->to_col) && steps < 8) {
    // ...
    steps++;
}
if (steps >= 8) {
    return MOVE_ERROR_INVALID_PATTERN; // Safety
}
```

---

## 📊 SOUHRN

| Bug # | Závažnost | Status | Popis |
|-------|-----------|--------|-------|
| #1 | Kritický | ✅ Opraveno | Černý pěšec blokování |
| #2 | Kritický | ✅ Opraveno | Střelec blokování na konci |
| #3 | Střední | ✅ Opraveno | Král tah na stejné pole |
| #4 | Kritický | ❌ **NEOBRAVENO** | En passant check validation |
| #5 | Falešný alarm | ✅ OK | Pěšec blokování je správně |
| #6 | Nízký | ⚠️ K diskusi | Střelec while loop safety |
| #7 | Falešný alarm | ✅ OK | Double square blokování |
| #8 | Střední | ❌ **NEOBRAVENO** | Zpětný tah pěšce |

---

## 🎯 CO JE POTŘEBA OPRAVIT

1. ✅ **BUG #4**: En passant check validation - KRITICKÉ!
2. ✅ **BUG #8**: Zpětný tah pěšce detekce - střední

