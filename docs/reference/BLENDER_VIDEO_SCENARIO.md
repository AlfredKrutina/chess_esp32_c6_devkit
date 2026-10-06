# CzechMate — video scenario and LED truth table

Two films are generated headlessly in Blender 5.0.1 from `models/blender/`:

| Film | Builder | Length | Output |
|---|---|---|---|
| Exploded view | `build_rozklad.py` | 23 s (690 frames) | `models/blender/video/rozklad.mp4` |
| Presentation | `build_predstaveni.py` | 98 s (2946 frames) | `models/blender/video/predstaveni.mp4` |

Both render at 1920 × 1080, 30 fps, EEVEE with ray tracing, motion blur, bloom,
light dispersion and vignette. Shared scene code lives in `cine_common.py`.
A 60 fps version would double the frame count and the render time without
changing duration; Full HD at 30 fps keeps the same pacing.

## Pipeline

1. `led_effects.py` re-implements the firmware LED effects as lists of
   `(t_ms, full board state)`. Pure Python, no Blender.
2. `led_timeline.py` (run with `py -3.12`, needs `python-chess`) plays the game
   scenario, asks python-chess for legal moves exactly as the firmware does,
   and writes `timeline_presentation.json`: piece keys, LED states per frame,
   chapter markers and captions.
3. `build_predstaveni.py` turns the JSON into the scene. The camera decides
   only where to look; it never changes what the board shows.
4. Each film renders to a lossless intermediate in `%TEMP%\czechmate_render`,
   then a VSE pass adds captions and encodes the final H.264.

Test stills: `-- --test 120,400 --res 1280x720 --samples 16`. Without
`--test` the builder saves the `.blend` and then renders the whole film;
`--preview` renders it at 1280 × 720 and 12 samples.

## Board look

- **Cover glass:** the whole sheet is milky. The chessboard is only a deeper
  sandblast on the light squares (same milk, more etch), like frosted door
  glass. a1 stays the lighter etch, the way a dark square should.
- **LED diffusion:** each square has a 33 × 33 mm area light just under the
  diffuser foil and an emissive glow plane on the foil, so the whole square
  lights evenly with a slight hotspot, like the real diffuser.
- **Contact shadows:** a soft occlusion disc on the glass under every piece. It
  fades out once the base is 8 mm above the glass, so lifted pieces read as
  lifted and resting pieces as resting.
- **Hand moves:** baked per frame. The piece lifts with a small settle, sways
  slightly while held, travels on a soft arc with a smootherstep profile,
  tilts up to 6° along its direction of travel and sets down softly. A captured
  piece is carried off the board and put down on the table on the left.

## LED truth table (live path: `game_physical.c`)

Defaults: FULL profile, guidance level 5, brightness 50 %, LED task tick 33 ms.

| Situation | What the board shows | Source |
|---|---|---|
| Boot | Green (0,128,0) cumulative trail, 201 steps × 25 ms, fade 20 × 30 ms, clear | `led_task.c` |
| Idle | Yellow (255,255,0) on every piece of the side to move that has a legal move | `game_physical.c` |
| Lift own piece | Clear; source yellow; empty targets green (0,255,0); captures orange (255,165,0) | `game_physical.c` 1840–1963 |
| Lift opponent piece that a legal move can capture | Clear; purple (128,0,128) on that square; yellow on every legal attacker (guidance ≥ 3) | `game_physical.c` `game_enter_guided_capture` / `game_show_guided_capture_leds` |
| Guided capture, attacker then lifted | Clear; purple only on the victim square (drop target) | `game_physical.c` `game_show_guided_capture_leds` |
| Lift king | Source orange-red (255,128,0) from the resignation start; castling target blue (0,0,255) | `game_physical.c` 630–790, `game_resignation.c` 151–233 |
| Quiet drop | Lift LEDs stay → blue move-path trail (25 frames × 2 ms, 6 trails) → 8 blue breaths on target → clear | `led_task.c` 2100–2310 |
| Player change | Clear, 30 ms, dark-grey (31,31,31) Gaussian row wave toward the new side, 50 frames × 12 ms, clear | `led_task.c` 2100–2310 |
| Check | Pink (255,192,203) on the king, then yellow movables without clear | `led_task.c` 2563–2830 |
| Cancel (piece back on source) | Clear, yellow movables of the same side | `game_physical.c` |
| Capture, attacker first | Lift attacker as usual; lift victim → clear, yellow attacker source, purple (128,0,128) victim; place → clear, yellow movables | `game_physical.c` 1290–1330 |
| Capture, victim first | Same as “lift opponent piece that can be captured”, then attacker-lifted purple-only, then place | `game_physical.c` 578–587 |
| Illegal drop | Clear; dark until 300 ms; 10 toggles every 300 ms starting ON; solid red from 3000 ms | `game_task.c` 1827–1888 |
| Lift from the red square | Yellow on the wrong square, blue on the original square, green/orange targets from the original square | `game_error_recovery.c` |
| Correct placement after an error | Clear, yellow movables of the new side (no path, no wave) | `game_physical.c` |
| Castling, king on g1 | 3 pulses × 200 ms (brightness 0.75 / 0.966 / 0.534): silver on rook source, green on rook target; then static | `game_move_exec.c` 300–355 |
| Castling, rook lift | Clear, yellow on rook source, green on rook target | `game_task.c` 1333–1362 |
| Castling, rook drop | Gold (255,215,0) on rook target, 5 × (100 ms on / 100 ms off), rook source stays yellow; then player-change wave, movables, check pink | `game_move_exec.c` 440–560, 848–866 |
| Mate | Endless wave from the winning king: 100 ms steps, radius 1–14, 4 rings; winner pieces (30,255,80), loser (255,30,30), empty (30,100,255), winner king gold | `led_task.c` 2563–2830 |
| Promotion | Purple ↔ gold on the promotion square (1 s), buttons green/blue, LED 72 green, 3 s animation | `led_task.c` |
| Matrix guard | White mismatch yellow, black mismatch blue, ghost (255,140,0), missing (220,220,220) | `game_matrix_guard.c` |

LED index mapping (`led_mapping.c`): even rows `row·8 + (7 − col)`, odd rows
`row·8 + col`; LED 0 is h1.

## Key moments where the backlight changes

The LEDs change only on these events (a detected lift, a detected placement, a
timer inside a firmware animation). The camera is framed so each change is on
screen:

1. Boot trail finishes → idle yellow appears.
2. Piece lifted → board clears, targets appear once the foot has cleared the glass (at lift height, not mid-rise).
   **Invariant:** green / orange / castle-blue guidance never lights while the
   moving piece is still on the glass. Idle yellow movables on resting pieces
   are fine. `led_timeline.py` asserts this when regenerating the JSON.
3. Piece placed → blue path, then the grey wave crosses toward the opponent.
4. Check → pink on the king plus yellow on the defenders.
5. Illegal drop → 300 ms of darkness, blinking red, solid red after 3 s.
6. Victim lifted during a capture (attacker already in the air) → orange becomes purple on the victim, yellow stays on the attacker source.
6b. Victim lifted first, while it can still be captured → purple on that square and yellow on every piece that can take it; after an attacker is lifted, only the purple drop target remains.
7. King lands on g1 → three silver/green pulses; rook lands on f1 → gold blinks, then the wave.
8. Mating move → endgame wave starts immediately and loops.

## Presentation scenario

Opening: Légal's mate — 1. e4 e5 2. Ng1–g3 (illegal) → Nf3 2… Nc6 3. Bc4 d6
4. Nc3 Bg4 5. Nxe5 Bxd1 6. Bxf7+ Ke7 7. Nd5#.

The continuous film (`predstaveni.mp4` / `_nahled`) is the landing hero in full:
glass → boot → Hall magnets → guided play → illegal recovery → the sacrifice
and mate wave and logo. The web hero soft-loops from the board wake-up, not
the dark wall, and is cut just before the CzechMate overlay.

Check lighting is shown on 6. Bxf7+: pink on the king, yellow only on the
moves that get out of check. Captures lift the attacker first, then the
victim (orange → purple), so a captured pawn is never staged as Black's move.

### Camera

One continuous camera on smooth Bézier keys. After the pin and the queen
sacrifice the camera stays close enough to read Bxf7+ and Nd5#, then closes
on the mate. `build_predstaveni_zive.py` is a separate cut of the same timeline
(`video/predstaveni_zive.mp4`). It hard-cuts, and each move is its own shot
from the side of the path, travelling with the piece. The continuous film is
left as it is.

- **Focal length** changes with the story: 85 mm along the dark chassis wall,
  then 32–34 mm for a fly through the empty middle of the board, parallel to
  the two ranks of pieces. The camera rises over the centre before it travels
  back. Move coverage stays near 32–36 mm so a piece and its neighbours stay
  in frame. The Hall rank is a steep 26 mm look down the whole rank, so the
  pole colour reads and the end pieces are not cropped. The mate is a 36 mm
  view of d5 and the king on e7.
- **Rack focus** in the opening: focus stays on the aluminium face, then on the
  glass, and rides on the aim once the boot trail has pulled the camera back.
- **Follow focus:** while a piece is in the hand, the aim eases 28 % onto a
  point 20 mm above the piece and lets go about 0.6 s after it is set down.
  Area lights and the per-square LED lamps are hidden from the camera; the
  glow on the foil is what the lens sees. Boot sparks stay on the lip.

| Frames | Chapter | Camera | Board |
|---|---|---|---|
| 1–119 | Dark wall | 85 mm along the front face, looking down the metal so the top stays out of frame | off |
| 120–209 | Sandblast | Leaves the front face, then flies the empty middle of the board along the ranks, parallel to the two rows of pieces, about 17 mm above the glass | off, the frost is the subject |
| 210–285 | Boot | Rises over the centre, then back. The pieces enter as a board, not a crop | green boot trail (2.4 × speed), sparks on the lip |
| 286–345 | Idle | Descends toward the white side | yellow movables |
| 346–520 | Hall V2 | Hold white magnets, slow east orbit so the board stays oriented, then hold black (no 180° whip). Zive uses a short bridge cut between the two ranks | magnets glow; white shows blue on top, black shows red |
| 521–665 | 1. e4 | Follows the pawn, settles high enough to see the grey wave | green targets hold until the drop, then blue path and grey wave |
| 666–782 | 1… e5 | Swings around the right side to the black side | same flow |
| 783–1065 | Illegal knight → Nf3 | Crane over the king-side knight, red on g3, recover to f3 | red blink, then blue origin |
| 1066–1194 | 2… Nc6 | Black queenside knight onto c6 | green targets, then path and wave |
| 1195–1464 | 3. Bc4 d6 | Follows the bishop onto c4, then Black's d-pawn | idle yellow after each drop |
| 1465–1763 | 4. Nc3 Bg4 | White knight, then the pin on the queen | green, then path and wave |
| 1764–2334 | 5. Nxe5 Bxd1 | Front on the knight capture, then Black takes the queen | orange → purple on each victim |
| 1940–2458 | 6. Bxf7+ Ke7 | f7 pawn leaves first; camera closes on e8, then Ke7 | purple on f7, yellow on every legal capturer; after the bishop lifts, purple only. Then pink king |
| 2458–2626 | 7. Nd5# | Closes on d5 and the king | endgame wave from e1 |
| 2627–2713 | Outro | Rises to a high three-quarter view, logo | endgame wave continues |

A travelling piece follows one arch and moves at about 84 mm/s. A short move waits, then goes; a long one takes longer and lands on its beat. The arch stays low unless another piece is in the way. Square colour is
keyed again on the frame before it changes, so a green target stays green for
the whole carry and snaps to blue on the drop.

All firmware animations after a placement run at real speed: the blue move
path is over in about 50 ms, the eight blue breaths and the grey player-change
wave take about 0.6 s. Only the boot trail is sped up (2.4 ×).

Captions are short Czech lines centred at the bottom; hero lines and the logo
are centred in frame.

## Exploded-view scenario

Keynote style: black void, no floor, a top softbox, two cool vertical strip
lights, a high back rim, and a thin glass highlight strip. A light sweep crosses
the aluminium in the first two seconds.

| Frames | Action |
|---|---|
| 1–15 | Assembled hero, one front-right camera |
| 16–36 | Six self-tapping screws back straight out. No caption |
| 40–86 | Lid drops, then the chassis. The caption starts with that part |
| 100–128 | Pieces rise clear of the glass |
| 130–206 | Glass, foil, then the grid, each only after the one above has cleared |
| 220–236 | Four boards slide apart. Headers face the chassis |
| 250–353 | Boot trail |
| 354–474 | Pieces go transparent. Camera looks into the white rank, then the black rank. White magnets are blue on top, black magnets red on top |
| 486–626 | Every layer returns in one eased move and arrives together |
| 626–658 | Screws thread in after the lid is home |
| 666–690 | Hero, logo |

The four boards are copies of `board/czechmate_v3.glb` (one 140 mm quadrant from the KiCad STEP). The header body is not in that STEP, so it is built on the mating edge. Layer callouts fade in when that part starts to leave, and they are gone before the next part moves. The magnet callout stays up for the whole look into the pieces.

## Hall V2 piece recognition

Four STM32C031 segments (I²C 0x30–0x33, 16 squares each) read two Hall
sensors per square, 6.267 mm apart. In the films the polarity is the piece
colour, not the type. Every magnet in a black piece has south toward the
board (blue on the underside, red on top). Every magnet in a white piece is
flipped, so north faces the board. The type only changes how many magnets
there are and where they sit:

| Piece | Magnets |
|---|---|
| Pawn | one, centre |
| Rook | one, centre |
| Knight | one, offset 3.6 mm |
| Bishop | two, at ±3.6 mm |
| Queen | four, cross at 5.2 mm |
| King | four, cross at 5.2 mm |

The firmware has no LED feedback for piece-type recognition yet, so the
films show it only as graphics: translucent pieces, discs cut horizontally,
and cyan Hall pairs. No LED colour is invented for it. The exploded view uses four copies of the KiCad board czechmate_v3.

## Firmware observations found while building the timeline

- In check, the pink king LED is overwritten by the yellow movables whenever
  the king itself can move, so pink stays visible only when the king is stuck.
- A capture that gives check skips the check animation.
- `led_anim_checkmate` (red/white flashes) is never called; mate uses the
  endgame wave.
- The endgame wave starts inside `game_execute_move` before the move path,
  so the two interleave, and the wave can be initialised twice.
- The documentation describes move targets as blue; the code lights them
  green (empty) or orange (capture). Blue is used for castling targets, the
  move-path trail and the error-recovery source.
