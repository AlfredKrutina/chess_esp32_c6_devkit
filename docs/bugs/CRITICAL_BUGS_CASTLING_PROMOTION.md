# 🚨 KRITICKÉ BUGY: ROŠÁDA, PROMOCE, PAT

## ⚠️ NALEZENÉ ZÁVAŽNÉ PROBLÉMY

---

## 🐛 BUG #10: PROMOCE - OBRÁCENÉ ŘADY! (KRITICKÝ!)

**Lokace:** `game_execute_promotion()` řádek 8244 a 8252

### Problém:
```c
❌ ŠPATNĚ:
if (current_player == PLAYER_WHITE && piece == PIECE_WHITE_PAWN && row == 0) {
    // White pawn on rank 8 - promote
```

**ROW 0 = 1. ŘADA (a1, b1, ...), NE 8. ŘADA!**

```c
❌ ŠPATNĚ:
} else if (current_player == PLAYER_BLACK && piece == PIECE_BLACK_PAWN && row == 7) {
    // Black pawn on rank 1 - promote
```

**ROW 7 = 8. ŘADA (a8, b8, ...), NE 1. ŘADA!**

### Správně by mělo být:
```c
✅ SPRÁVNĚ:
if (current_player == PLAYER_WHITE && piece == PIECE_WHITE_PAWN && row == 7) {
    // White pawn on rank 8 (row 7) - promote
```

```c
✅ SPRÁVNĚ:
} else if (current_player == PLAYER_BLACK && piece == PIECE_BLACK_PAWN && row == 0) {
    // Black pawn on rank 1 (row 0) - promote
```

### Dopad:
- **Promoce NIKDY NEFUNGUJE!**
- Bílý pěšec na 8. řadě (row 7) se hledá na row 0
- Černý pěšec na 1. řadě (row 0) se hledá na row 7
- **Promotion je 100% nefunkční!**

---

## 🐛 BUG #11: ROŠÁDA - Missing Rook Validation!

**Lokace:** `game_validate_castling()` řádek 1754-1842

### Kontroluje se:
- ✅ Král se pohyboval
- ✅ Věž se pohybovala
- ✅ Cesta mezi králem a věží je prázdná
- ✅ Král není v šachu
- ✅ Král by neprošel přes šachované pole
- ✅ Král by neskončil v šachu

### ❌ NEKTROLUJE SE:
1. **Zda věž vůbec EXISTUJE!**
2. **Zda na pozici věže je opravdu věž!**

### Scénář selhání:
```
1. Vezmu věž h1 pryč (nějakým nevalidním způsobem)
2. Král e1, věž NENÍ na h1
3. Pokus o rošádu e1→g1
4. Validace PROJDE ✅ (protože všechny kontroly jsou OK)
5. Rošáda se provede BEZ VĚŽE!
```

### Fix:
```c
// Check if rook exists at expected position
piece_t expected_rook = is_white ? PIECE_WHITE_ROOK : PIECE_BLACK_ROOK;
piece_t rook_piece = board[king_row][rook_col];
if (rook_piece != expected_rook) {
    return MOVE_ERROR_CASTLING_BLOCKED;
}
```

---

## 🐛 BUG #12: ROŠÁDA - Queenside kontrola cesty je ŠPATNĚ!

**Lokace:** `game_validate_castling()` řádek 1806-1807

```c
int start_col = (move->from_col < rook_col) ? move->from_col + 1 : rook_col + 1;
int end_col = (move->from_col < rook_col) ? rook_col : move->from_col;
```

### Analýza pro Queenside (O-O-O):
```
Král: e1 (col 4)
Věž: a1 (col 0)
rook_col = 0

start_col = (4 < 0) ? 4 + 1 : 0 + 1
         = false ? 5 : 1
         = 1  ✅ (b1)

end_col = (4 < 0) ? 0 : 4
        = false ? 0 : 4
        = 4  ✅ (e1)

Loop: for (col = 1; col < 4; col++)
      Kontroluje: b1, c1, d1  ✅

ALE! NEKONTROLUJE pole a1 (věž)!
```

### ❌ Pole mezi králem a věží pro queenside:
- b1, c1, d1 - ✅ kontrolováno
- **a1 - ❌ NENÍ kontrolováno!**

**ALE:** Pole a1 je pole věže, takže je tam věž (pokud neprošla kontrola existence).

**VE SKUTEČNOSTI:** Toto je OK! Pole věže se nekontroluje, protože tam JE věž.

---

## ✅ PAT (STALEMATE) - FUNGUJE SPRÁVNĚ

**Lokace:** `game_check_end_game_conditions()` řádek 6710

```c
} else if (!in_check && !has_moves) {
    // Stalemate
    game_result = GAME_STATE_FINISHED;
    current_result_type = RESULT_DRAW_STALEMATE;
    ESP_LOGI(TAG, "🤝 STALEMATE! Game drawn in %" PRIu32 " moves", move_count);
    return GAME_STATE_FINISHED;
}
```

**Kontrola:**
- ✅ Král není v šachu: `!in_check`
- ✅ Hráč nemá žádné validní tahy: `!has_moves`
- ✅ Nastavuje se správný result type
- ✅ Statistiky se aktualizují
- ✅ Timer se zastaví

**PAT FUNGUJE SPRÁVNĚ!** ✅

---

## ⚠️ DALŠÍ POTENCIÁLNÍ PROBLÉMY

### 🔍 ROŠÁDA - Check validation loop může selhat

**Lokace:** Řádek 1822

```c
for (int col = move->from_col; col != move->to_col + step; col += step) {
```

### Analýza Kingside:
```
from_col = 4 (e1)
to_col = 6 (g1)
step = 1

Loop: col != 6 + 1 = 7
      col: 4, 5, 6
      Kontroluje: e1 (skip), f1, g1  ✅
```

### Analýza Queenside:
```
from_col = 4 (e1)
to_col = 2 (c1)
step = -1

Loop: col != 2 + (-1) = 1
      col: 4, 3, 2
      Kontroluje: e1 (skip), d1, c1  ✅
```

**Vypadá OK!** ✅

---

## 📊 SOUHRN BUGŮ V ROŠÁDĚ A PROMOCÍCH

| Bug | Komponenta | Závažnost | Dopad | Status |
|-----|-----------|-----------|-------|--------|
| #10 | Promoce řady | **KRITICKÝ** | **Promoce 100% nefunkční** | ❌ **NEOPRAVENO** |
| #11 | Rošáda - věž check | Střední | Rare edge case | ❌ **NEOPRAVENO** |
| #12 | Rošáda - cesta | Falešný alarm | Pole věže se nemusí kontrolovat | ✅ OK |

---

## ✅ CO FUNGUJE SPRÁVNĚ

### PAT (Stalemate):
- ✅ Detekuje !in_check && !has_moves
- ✅ Nastavuje správný result type
- ✅ Zastavuje timer
- ✅ Generuje endgame report

### ROŠÁDA - Většina validací:
- ✅ Král se pohyboval
- ✅ Věž se pohybovala
- ✅ Král není v šachu
- ✅ Cesta je prázdná
- ✅ Král neprochází šachem
- ⚠️ Chybí kontrola existence věže

### 50-MOVE RULE:
- ✅ Počítá tahy bez braní
- ✅ Detekuje >= 50
- ✅ Vyhlašuje remízu

### THREEFOLD REPETITION:
- ✅ Hashuje pozice
- ✅ Detekuje opakování
- ✅ Vyhlašuje remízu

### INSUFFICIENT MATERIAL:
- ✅ Kontroluje král vs král
- ✅ Kontroluje král + střelec vs král
- ✅ Další varianty

---

## 🎯 CO OPRAVIT TEĎ

1. ✅ **BUG #10**: Promoce - opravit row čísla (KRITICKÉ!)
2. ✅ **BUG #11**: Rošáda - přidat kontrolu existence věže (důležité)

---

## 📝 POZNÁMKY

### O koordinátovém systému:
```
ROW INDEXING (0-based od spodu):
row 0 = 1. řada (a1, b1, ..., h1) - START BÍLÝCH
row 1 = 2. řada (a2, b2, ..., h2)
row 2 = 3. řada
...
row 6 = 7. řada (a7, b7, ..., h7)
row 7 = 8. řada (a8, b8, ..., h8) - START ČERNÝCH
```

### Promoce by měla být:
- **Bílý** pěšec dosáhne **row 7** (8. řada) → promote
- **Černý** pěšec dosáhne **row 0** (1. řada) → promote

### Současný kód má to OBRÁCENÉ!

