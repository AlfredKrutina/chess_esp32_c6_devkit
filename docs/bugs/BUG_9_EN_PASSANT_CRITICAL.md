# 🐛 KRITICKÝ BUG #9: EN PASSANT TARGET ROW VÝPOČET BYL OBRÁCENÝ!

## ⚠️ NEJZÁVAŽNĚJŠÍ BUG DOSUD NALEZENÝ

**Status:** ✅ **OPRAVENO**

---

## 🔍 PROBLÉM

### Původní kód (ŠPATNÝ):
```c
bool is_white_pawn = game_is_white_piece(move->piece);
int en_passant_row = is_white_pawn ? last_move_to_row - 1 : last_move_to_row + 1;
```

### Proč to bylo ŠPATNĚ:

**Scénář:** Černý pěšec táhne c7→c5, bílý pěšec na b5 chce sebrat en passant

| Krok | Hodnota | Výpočet | Výsledek |
|------|---------|---------|----------|
| 1 | Černý pěšec c7→c5 | row 6→4 | `last_move_from_row=6, last_move_to_row=4` |
| 2 | Bílý pěšec na b5 | row 4, col 1 | Útočící pěšec |
| 3 | En passant cíl? | `is_white_pawn ? last_move_to_row - 1` | `4 - 1 = 3` ❌ |
| 4 | Pokus b5→c6 | to_row = 5 | `5 == 3`? **FALSE** ❌ |
| 5 | **Výsledek** | | **EN PASSANT SELHAL!** |

**Správný výpočet by měl být:**
- En passant pole je vždy **uprostřed** mezi startem a cílem protivníkova pěšce
- Pro c7→c5: en passant pole = c6 = row 5
- Výpočet: `(6 + 4) / 2 = 5` ✅

---

## ✅ OPRAVA

### Nový kód (SPRÁVNÝ):
```c
// Určit směr pohybu posledního pěšce
int last_pawn_direction = (last_move_to_row > last_move_from_row) ? 1 : -1;

// En passant pole je vždy uprostřed mezi from a to
int en_passant_target_row = (last_move_from_row + last_move_to_row) / 2;

ESP_LOGD(TAG, "🔍 En passant check: last=%c%d→%c%d, target=%c%d, move=%c%d→%c%d", 
         'a' + last_move_from_col, last_move_from_row + 1,
         'a' + last_move_to_col, last_move_to_row + 1,
         'a' + last_move_to_col, en_passant_target_row + 1,
         'a' + move->from_col, move->from_row + 1,
         'a' + move->to_col, move->to_row + 1);

if (move->to_row == en_passant_target_row && move->to_col == last_move_to_col) {
    ESP_LOGD(TAG, "✅ En passant VALID!");
    return true;
}
```

---

## 📊 TESTOVACÍ SCÉNÁŘE

### Test Case #1: Bílý útočí na černého
```
Pozice:
8 | · | · | · | · | · | · | · | · |
7 | · | · | p | · | · | · | · | · |  <- Černý pěšec start
6 | · | · | · | · | · | · | · | · |
5 | · | P | p | · | · | · | · | · |  <- Černý pěšec cíl, Bílý b5
4 | · | · | · | · | · | · | · | · |
3 | · | · | · | · | · | · | · | · |
2 | · | · | · | · | · | · | · | · |
1 | · | · | · | · | · | · | · | · |
    a   b   c   d   e   f   g   h

Poslední tah: c7→c5 (row 6→4)
Bílý pěšec: b5 (row 4, col 1)
En passant pokus: b5→c6
```

**Před opravou:**
```c
is_white_pawn = true
en_passant_row = last_move_to_row - 1 = 4 - 1 = 3
move->to_row = 5 (c6)
5 == 3? FALSE ❌
Result: EN PASSANT NEFUNGUJE ❌
```

**Po opravě:**
```c
en_passant_target_row = (6 + 4) / 2 = 5
move->to_row = 5 (c6)
5 == 5? TRUE ✅
Result: EN PASSANT FUNGUJE ✅
```

---

### Test Case #2: Černý útočí na bílého
```
Pozice:
8 | · | · | · | · | · | · | · | · |
7 | · | · | · | · | · | · | · | · |
6 | · | · | · | · | · | · | · | · |
5 | · | · | · | · | · | · | · | · |
4 | · | · | p | P | · | · | · | · |  <- Bílý d2→d4, Černý c4
3 | · | · | · | · | · | · | · | · |
2 | · | · | · | P | · | · | · | · |  <- Bílý pěšec start
1 | · | · | · | · | · | · | · | · |
    a   b   c   d   e   f   g   h

Poslední tah: d2→d4 (row 1→3)
Černý pěšec: c4 (row 3, col 2)
En passant pokus: c4→d3
```

**Před opravou:**
```c
is_white_pawn = false (černý útočí)
en_passant_row = last_move_to_row + 1 = 3 + 1 = 4
move->to_row = 2 (d3)
2 == 4? FALSE ❌
Result: EN PASSANT NEFUNGUJE ❌
```

**Po opravě:**
```c
en_passant_target_row = (1 + 3) / 2 = 2
move->to_row = 2 (d3)
2 == 2? TRUE ✅
Result: EN PASSANT FUNGUJE ✅
```

---

## 🎯 PROČ TO BYLO OBRÁCENÉ?

Původní autor mylně předpokládal:
- "Když WHITE útočí, cíl je -1"
- "Když BLACK útočí, cíl je +1"

**ALE VE SKUTEČNOSTI:**
- En passant cílové pole je **VŽDY** uprostřed mezi startem a cílem protivníkova pěšce
- Nezáleží na tom, kdo útočí!
- Správný vzorec: `(from_row + to_row) / 2`

---

## 📈 DIAGNOSTICKÉ LOGY

Přidány debug logy pro sledování en passant detekce:

```
🔍 En passant: no last move
🔍 En passant: last move was not pawn
🔍 En passant: last move was not 2 squares (diff=1)
🔍 En passant: attacking pawn not on same row (from_row=3, last_to_row=4)
🔍 En passant: pawn not adjacent (col_diff=2)
🔍 En passant check: last=c7→c5, target=c6, move=b5→c6
✅ En passant VALID!
❌ En passant: target mismatch (to_row=5 vs target=3, to_col=2 vs last_col=2)
```

---

## ✅ OVĚŘENÍ OPRAVY

### Nová pozice z terminálu (řádek 989-1014):
```
Board:
5 | · | P | p | · | · | · | · | · | 5  <- Bílý b5, Černý c5
```

**Pokud byl poslední tah: c7→c5**

Nový výpočet:
```
last_move_from_row = 6 (c7)
last_move_to_row = 4 (c5)
en_passant_target_row = (6 + 4) / 2 = 5 (c6) ✅

Pokus b5→c6:
move->from_row = 4 (b5)
move->to_row = 5 (c6)
move->to_col = 2 (c)
last_move_to_col = 2 (c)

Kontroly:
✅ has_last_move: true
✅ last_piece: Black Pawn
✅ last_row_diff: 2
✅ from_row == last_move_to_row: 4 == 4
✅ col_diff: |1 - 2| = 1
✅ to_row == en_passant_target_row: 5 == 5
✅ to_col == last_move_to_col: 2 == 2

RESULT: ✅ EN PASSANT VALID!
```

---

## 🎉 ZÁVĚR

**BUG #9 byl KRITICKÝ a způsobil, že en passant VŮBEC NEFUNGOVAL!**

- ✅ Opraveno použitím správného matematického vzorce
- ✅ Přidány diagnostické logy pro debugging
- ✅ Funkce nyní správně detekuje en passant pro bílé i černé

**En passant by nyní měl fungovat 100% správně!** 🎯

