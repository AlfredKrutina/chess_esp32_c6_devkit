# CzechMate lamp film

Nightstand loop for the product site: empty board, warm HA colour on all 64 squares, no other lights in the room.

## Why the nightstand, not the wall

LEDs fire through the glass face. Hang the board and the wash goes into the ceiling; the squares disappear. Firmware lamp mode is a table lamp.

## Shot

- Script: `models/blender/build_lampa.py`
- Scene: `models/blender/czechmate_lampa.blend` (rewritten each run)
- Master: `models/blender/video/lampa.mp4` — 1920×1080, 30 fps, 240 frames (~8 s), Cycles GPU
- Frames: `models/blender/video/lampa_frames/f####.png` (`--final` wipes and rewrites)
- Colour: warm → amber → rose → violet → blue → teal → warm (HA MQTT RGB)
- Shot: high three-quarter, nightstand pulled into the room (no hard wall light cut)
- Web: `gh-pages-ready/landing/assets/czm-lamp.mp4` + HLS `hls/lamp/` via `encode_web.py --only lamp`

Look stills before a final:

```
blender --background --factory-startup --python models/blender/build_lampa.py -- --look
```

Final:

```
blender --background --factory-startup --python models/blender/build_lampa.py -- --final
py -3.12 models/blender/encode_web.py --only lamp
```

Pulse is cosine warm RGB `(255, 176, 88)` keyed with `Leds.key` hold + CONSTANT, same as the other films. `with_pcb=False`, no pieces.
