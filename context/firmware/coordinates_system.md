# Board coordinates — conventions (CZECHMATE firmware 1.8.0)

> Exact code is in `game_task` (`convert_notation_to_coords` / `convert_coords_to_notation`). Here is a summary of the **row/col** convention in firmware — standard **a1 = [0,0]**.

## Notation to square

In `game_task` I convert e.g. `"e2"` / `"e4"` via `convert_notation_to_coords()`.

### Function `convert_notation_to_coords()`

```c
bool convert_notation_to_coords(const char *notation, uint8_t *row, uint8_t *col)
```

**Conversion rules:**
- **Column (col)**: `col = notation[0] - 'a'`
  - a = 0, b = 1, c = 2, d = 3, e = 4, f = 5, g = 6, h = 7
- **Row (row)**: `row = notation[1] - '1'`
  - 1 = 0, 2 = 1, 3 = 2, 4 = 3, 5 = 4, 6 = 5, 7 = 6, 8 = 7

### Conversion examples

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

## Board representation

The board is stored as `board[8][8]`, where:
- `board[row][col]` — piece type at (row, col)
- `row` — row (0–7), 0 = rank 1 (white pieces), 7 = rank 8 (black pieces)
- `col` — column (0–7), 0 = file a, 7 = file h

## Board orientation

### Corner coordinates:

- **board[0][0]** = **a1** (white rook) — bottom-left
- **board[0][7]** = **h1** (white rook) — bottom-right  
- **board[7][0]** = **a8** (black rook) — top-left
- **board[7][7]** = **h8** (black rook) — top-right

### Starting piece layout:

**White pieces (row 0 and 1):**
- board[0][0] = PIECE_WHITE_ROOK   (a1)
- board[0][1] = PIECE_WHITE_KNIGHT (b1)
- board[0][2] = PIECE_WHITE_BISHOP (c1)
- board[0][3] = PIECE_WHITE_QUEEN  (d1)
- board[0][4] = PIECE_WHITE_KING   (e1)
- board[0][5] = PIECE_WHITE_BISHOP (f1)
- board[0][6] = PIECE_WHITE_KNIGHT (g1)
- board[0][7] = PIECE_WHITE_ROOK   (h1)
- board[1][0-7] = PIECE_WHITE_PAWN (a2-h2)

**Black pieces (row 6 and 7):**
- board[7][0] = PIECE_BLACK_ROOK   (a8)
- board[7][1] = PIECE_BLACK_KNIGHT (b8)
- board[7][2] = PIECE_BLACK_BISHOP (c8)
- board[7][3] = PIECE_BLACK_QUEEN  (d8)
- board[7][4] = PIECE_BLACK_KING   (e8)
- board[7][5] = PIECE_BLACK_BISHOP (f8)
- board[7][6] = PIECE_BLACK_KNIGHT (g8)
- board[7][7] = PIECE_BLACK_ROOK   (h8)
- board[6][0-7] = PIECE_BLACK_PAWN (a7-h7)

## Reverse conversion: coordinates → notation

Back to notation via `convert_coords_to_notation()`:

```c
bool convert_coords_to_notation(uint8_t row, uint8_t col, char *notation)
```

**Formula:**
- notation[0] = 'a' + col
- notation[1] = '1' + row
- notation[2] = '\0'

**Examples:**
- [0, 0] → "a1"
- [1, 4] → "e2"
- [3, 4] → "e4"
- [7, 7] → "h8"

## Visualization

```
     a   b   c   d   e   f   g   h
   +---+---+---+---+---+---+---+---+
8  |   |   |   |   |   |   |   |   |  row 7 (rank 8) - Black pieces
   +---+---+---+---+---+---+---+---+
7  | P | P | P | P | P | P | P | P |  row 6 (rank 7) - Black pawns
   +---+---+---+---+---+---+---+---+
6  |   |   |   |   |   |   |   |   |  row 5 (rank 6)
   +---+---+---+---+---+---+---+---+
5  |   |   |   |   |   |   |   |   |  row 4 (rank 5)
   +---+---+---+---+---+---+---+---+
4  |   |   |   |   |   |   |   |   |  row 3 (rank 4)
   +---+---+---+---+---+---+---+---+
3  |   |   |   |   |   |   |   |   |  row 2 (rank 3)
   +---+---+---+---+---+---+---+---+
2  | p | p | p | p | p | p | p | p |  row 1 (rank 2) - White pawns
   +---+---+---+---+---+---+---+---+
1  | r | n | b | q | k | b | n | r |  row 0 (rank 1) - White pieces
   +---+---+---+---+---+---+---+---+

col: 0   1   2   3   4   5   6   7
```

## Summary

- **0,0** = **a1** (bottom-left, white rook)
- **7,7** = **h8** (top-right, black rook)
- **Row 0** = Rank 1 (white pieces)
- **Row 7** = Rank 8 (black pieces)
- **Col 0** = File a
- **Col 7** = File h
- **e2 → [1, 4]** ✓
- **e4 → [3, 4]** ✓

---

**Last unified:** 2026-04-30
