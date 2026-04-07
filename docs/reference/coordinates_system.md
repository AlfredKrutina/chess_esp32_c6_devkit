# Systém souřadnic v CZECHMATE v2.4

## Převod šachové notace na souřadnice

Game_task používá funkci `convert_notation_to_coords()` pro převod šachové notace (např. "e2", "e4") na souřadnice pole.

### Funkce `convert_notation_to_coords()`

```c
bool convert_notation_to_coords(const char *notation, uint8_t *row, uint8_t *col)
```

**Algoritmus převodu:**
- **Sloupec (col)**: `col = notation[0] - 'a'`
  - a = 0, b = 1, c = 2, d = 3, e = 4, f = 5, g = 6, h = 7
- **Řádek (row)**: `row = notation[1] - '1'`
  - 1 = 0, 2 = 1, 3 = 2, 4 = 3, 5 = 4, 6 = 5, 7 = 6, 8 = 7

### Příklady převodu

- **e2 → [1, 4]**
  - col = 'e' - 'a' = 4
  - row = '2' - '1' = 1

- **e4 → [3, 4]**
  - col = 'e' - 'a' = 4
  - row = '4' - '1' = 3

- **a1 → [0, 0]**
  - col = 'a' - 'a' = 0
  - row = '1' - '1' = 0

- **h8 → [7, 7]**
  - col = 'h' - 'a' = 7
  - row = '8' - '1' = 7

## Reprezentace šachovnice

Šachovnice je reprezentována jako 2D pole `board[8][8]`, kde:
- `board[row][col]` - typ figurky na pozici (row, col)
- `row` - řádek (0-7), kde 0 = rank 1 (bílé figurky), 7 = rank 8 (černé figurky)
- `col` - sloupec (0-7), kde 0 = file a, 7 = file h

## Orientace šachovnice

### Souřadnice rohů:

- **board[0][0]** = **a1** (bílá věž) - levý dolní roh
- **board[0][7]** = **h1** (bílá věž) - pravý dolní roh  
- **board[7][0]** = **a8** (černá věž) - levý horní roh
- **board[7][7]** = **h8** (černá věž) - pravý horní roh

### Počáteční pozice figurek:

**Bílé figurky (row 0 a 1):**
- board[0][0] = PIECE_WHITE_ROOK   (a1)
- board[0][1] = PIECE_WHITE_KNIGHT (b1)
- board[0][2] = PIECE_WHITE_BISHOP (c1)
- board[0][3] = PIECE_WHITE_QUEEN  (d1)
- board[0][4] = PIECE_WHITE_KING   (e1)
- board[0][5] = PIECE_WHITE_BISHOP (f1)
- board[0][6] = PIECE_WHITE_KNIGHT (g1)
- board[0][7] = PIECE_WHITE_ROOK   (h1)
- board[1][0-7] = PIECE_WHITE_PAWN (a2-h2)

**Černé figurky (row 6 a 7):**
- board[7][0] = PIECE_BLACK_ROOK   (a8)
- board[7][1] = PIECE_BLACK_KNIGHT (b8)
- board[7][2] = PIECE_BLACK_BISHOP (c8)
- board[7][3] = PIECE_BLACK_QUEEN  (d8)
- board[7][4] = PIECE_BLACK_KING   (e8)
- board[7][5] = PIECE_BLACK_BISHOP (f8)
- board[7][6] = PIECE_BLACK_KNIGHT (g8)
- board[7][7] = PIECE_BLACK_ROOK   (h8)
- board[6][0-7] = PIECE_BLACK_PAWN (a7-h7)

## Opačný převod: souřadnice → notace

Funkce `convert_coords_to_notation()` provádí opačný převod:

```c
bool convert_coords_to_notation(uint8_t row, uint8_t col, char *notation)
```

**Algoritmus:**
- notation[0] = 'a' + col
- notation[1] = '1' + row
- notation[2] = '\0'

**Příklady:**
- [0, 0] → "a1"
- [1, 4] → "e2"
- [3, 4] → "e4"
- [7, 7] → "h8"

## Vizualizace

```
     a   b   c   d   e   f   g   h
   +---+---+---+---+---+---+---+---+
8  |   |   |   |   |   |   |   |   |  row 7 (rank 8) - Černé figurky
   +---+---+---+---+---+---+---+---+
7  | P | P | P | P | P | P | P | P |  row 6 (rank 7) - Černé pěšci
   +---+---+---+---+---+---+---+---+
6  |   |   |   |   |   |   |   |   |  row 5 (rank 6)
   +---+---+---+---+---+---+---+---+
5  |   |   |   |   |   |   |   |   |  row 4 (rank 5)
   +---+---+---+---+---+---+---+---+
4  |   |   |   |   |   |   |   |   |  row 3 (rank 4)
   +---+---+---+---+---+---+---+---+
3  |   |   |   |   |   |   |   |   |  row 2 (rank 3)
   +---+---+---+---+---+---+---+---+
2  | p | p | p | p | p | p | p | p |  row 1 (rank 2) - Bílé pěšci
   +---+---+---+---+---+---+---+---+
1  | r | n | b | q | k | b | n | r |  row 0 (rank 1) - Bílé figurky
   +---+---+---+---+---+---+---+---+

col: 0   1   2   3   4   5   6   7
```

## Shrnutí

- **0,0** = **a1** (levý dolní roh, bílá věž)
- **7,7** = **h8** (pravý horní roh, černá věž)
- **Row 0** = Rank 1 (bílé figurky)
- **Row 7** = Rank 8 (černé figurky)
- **Col 0** = File a
- **Col 7** = File h
- **e2 → [1, 4]** ✓
- **e4 → [3, 4]** ✓
