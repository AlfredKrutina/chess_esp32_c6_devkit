# Square LEDs — chain, quadrants, what a glyph can light

Source of truth: `components/freertos_chess/led_mapping.c` (`pos_to_led`). Chess coordinates stay as in [coordinates_system.md](coordinates_system.md): `row 0` = rank 1, `col 0` = file a, white looking so that a1 is bottom-left and h1 is bottom-right.

The strip index is **not** `row * 8 + col`. Notes that say “LED 0 is h1” or “even rows are mirrored” (`context/media/BLENDER_VIDEO_SCENARIO.md`) describe an old map. Do not place a glyph with that formula.

`chess_pos_to_led_index(row, col)` is a lookup. It does **not** mirror the file. The comment in `led_gesture_anim.c` about a column mirror inside that function is stale.

## Chain

Four 4×4 boards. The chain starts at **e1 (LED 0)**. Each next board is the same serpentine, turned 90° counter-clockwise from the previous board.

| LEDs | Squares | On the bench | Order along the wire |
| --- | --- | --- | --- |
| 0–15 | e1–h4 | yes | e1 e2 e3 e4, f4 f3 f2 f1, g1 g2 g3 g4, h4 h3 h2 h1 |
| 16–31 | e5–h8 | yes | h5 g5 f5 e5, e6 f6 g6 h6, h7 g7 f7 e7, e8 f8 g8 h8 |
| 32–47 | a5–d8 | yes | d8 d7 d6 d5, c5 c6 c7 c8, b8 b7 b6 b5, a5 a6 a7 a8 |
| 48–63 | a1–d4 | **not fitted** | a4 b4 c4 d4, d3 c3 b3 a3, a2 b2 c2 d2, d1 c1 b1 a1 |

LED 72 is the gesture button at the h1 corner. It is not one of the 64 squares.

`test_led_mapping()` still expects e5 → 16. The table says **h5 = 16, e5 = 19**. Trust the table.

## Index of every square

Rank 8 is the far rank. File a is on the left. The number is the strip index.

```
     a   b   c   d   e   f   g   h
8   47  40  39  32  28  29  30  31
7   46  41  38  33  27  26  25  24
6   45  42  37  34  20  21  22  23
5   44  43  36  35  19  18  17  16
4   48  49  50  51   3   4  11  12
3   55  54  53  52   2   5  10  13
2   56  57  58  59   1   6   9  14
1   63  62  61  60   0   7   8  15
```

Indexes 48–63 (a1–d4, the near-left quadrant) are in the table so the firmware can address them, but those LEDs are not on the bench. A pixel aimed there does not appear.

## How a glyph is addressed

Animation pictures and symbol masks are chess coordinates. Slot `row * 8 + col`, row 0 = rank 1, col 0 = file a. The strip index appears only as the return value of `chess_pos_to_led_index(row, col)` inside the pixel write. Do not store a picture by strip index, and do not draw a mark by picking LED numbers off the table above.

`chess_pos_to_led_index` does not mirror the file. A mask written as an 8×8 with rank 8 at the top and file a on the left is the picture on the chessboard.

The a1–d4 quadrant (indexes 48–63) is not fitted on one bench. Those squares stay dark there. That does not change the coordinates: the mark is still authored on a1–h8, and the mapping drops the missing LEDs.

The UART dump in `led_task.c` labels rank `row + 1` but reads `chess_pos_to_led_index(7 - row, col)`. That printout is upside down. Do not use it to check a glyph.

---

**Written:** 2026-10-18
