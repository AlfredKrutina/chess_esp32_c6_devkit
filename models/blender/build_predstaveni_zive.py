"""A second presentation: hard cuts, each piece carrying its own shot.

The continuous film in build_predstaveni.py is unchanged. This one opens on
the sandblast, cuts down among the pieces as the board wakes, then gives every
move its own angle beside the path.

  blender -b --factory-startup --python build_predstaveni_zive.py -- --preview
  blender -b --factory-startup --python build_predstaveni_zive.py -- --preview --test 40,160,560

Output: video/predstaveni_zive.mp4, or predstaveni_zive_nahled.mp4 with --preview.
"""

import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_predstaveni as film  # noqa: E402
import cine_common as cc  # noqa: E402

FOLLOW = 0.72


def _alive(scene, depsgraph=None):
    film.active_camera(scene, scene.frame_current)


def _side(mv, flip):
    travel = Vector((mv.dst.x - mv.src.x, mv.dst.y - mv.src.y, 0.0))
    if travel.length < 1e-4:
        travel = Vector((0.0, -1.0, 0.0))
    else:
        travel.normalize()
    side = Vector((-travel.y, travel.x, 0.0))
    if flip:
        side.negate()
    return side, travel


def _clear(loc, aim):
    """Keep the lens beside the piece and above the heads."""
    loc = Vector(loc)
    away = Vector((loc.x - aim.x, loc.y - aim.y, 0.0))
    if away.length < 0.18:
        if away.length < 1e-4:
            away = Vector((0.0, -1.0, 0.0))
        else:
            away.normalize()
        loc = Vector((aim.x, aim.y, loc.z)) + away * 0.26
    if max(abs(loc.x), abs(loc.y)) < 0.17 and loc.z < 0.30:
        push = Vector((loc.x, loc.y, 0.0))
        if push.length < 1e-4:
            push = Vector((0.0, -1.0, 0.0))
        else:
            push.normalize()
        loc = Vector((push.x * 0.24, push.y * 0.24, max(loc.z, 0.22)))
    loc.z = max(0.14, loc.z)
    return loc


def _shot(stage, name, keys, move=None, piece=None):
    cam, aim = cc.camera(stage.coll, name, lens=keys[0][3], fstop=keys[0][4])
    cc.shot(stage.scene, cam, aim, keys)
    cc.add_noise(cam, "location", 0.0005, 30.0, 2.4)
    if move is None or piece is None:
        return cam
    handle = bpy.data.objects.new(name + " handle", None)
    stage.coll.objects.link(handle)
    handle.parent = piece
    handle.location = (0.0, 0.0, 0.02 / max(piece.scale.z, 1e-4))
    con = aim.constraints.new("COPY_LOCATION")
    con.target = handle
    span = max(move.t1 - move.t0, 1)
    on = move.t0 + max(4, span // 5)
    for frame, value in (
        (max(1, move.t0 - 6), 0.0),
        (on, FOLLOW),
        (move.t1, FOLLOW),
        (move.t1 + 14, 0.35),
    ):
        con.influence = value
        con.keyframe_insert("influence", frame=frame)
    return cam


def _moves(moves_of, earliest):
    found = []
    for pid, moves in moves_of.items():
        for mv in moves:
            if mv.t1 - mv.t0 < 14 or mv.t0 < earliest:
                continue
            found.append((mv.t0, pid, mv))
    found.sort(key=lambda item: item[0])
    kept = []
    for t0, pid, mv in found:
        if kept:
            prev = kept[-1][2]
            still_crossing = t0 < prev.t1 - 4 and abs(prev.dst.x) < 0.17 and abs(prev.dst.y) < 0.17
            if still_crossing or t0 - kept[-1][0] < 10:
                continue
        kept.append((t0, pid, mv))
    return kept


def build_camera(stage, m, pieces, moves_of):
    """Opening cuts, then one shot per move, seen from the side of its path."""
    if _alive not in bpy.app.handlers.frame_change_pre:
        bpy.app.handlers.frame_change_pre.append(_alive)

    centre = (0.0, 0.0, 0.03)
    # This cut opens on the glass, so the skim has to be on from the first frame.
    # The continuous film keeps it off until the later glass chapter.
    rake = stage.scene.objects.get("Glass rake")
    if rake is not None:
        for frame, energy in (
            (1, 6.0),
            (m["glass"] - 6, 6.0),
            (m["glass"] + 18, 6.5),
            (m["boot"] - 4, 5.0),
            (m["boot"] + 12, 0.0),
        ):
            rake.data.energy = energy
            rake.data.keyframe_insert("energy", frame=frame)
    # The front face stays black under a skim, and lighting it blows the edge
    # into a white bar. Open on the sandblast instead: the grain is the dark shot.
    glass = _shot(stage, "Zive glass", [
        (1, (0.13, -0.015, 0.070), (-0.04, 0.01, 0.023), 46, 8.0),
        (m["boot"] - 8, (0.03, -0.09, 0.054), (0.02, 0.04, 0.023), 52, 8.0),
    ])
    reveal = _shot(stage, "Zive reveal", [
        (m["boot"], (0.04, -0.24, 0.09), (0.0, -0.02, 0.04), 42, 8.0),
        (m["hall"] - 4, (0.22, -0.52, 0.30), centre, 34, 6.3),
    ])
    white = _shot(stage, "Zive white", [
        (m["hall"], (0.10, -0.24, 0.20), (0.0, -0.11, 0.045), 40, 5.6),
        (m["hall"] + 78, (0.02, -0.185, 0.16), (0.02, -0.12, 0.04), 48, 4.5),
    ])
    black = _shot(stage, "Zive black", [
        (m["hall"] + 86, (-0.06, 0.23, 0.19), (0.0, 0.11, 0.045), 40, 5.6),
        (m["e4"] - 14, (0.05, 0.175, 0.155), (-0.02, 0.12, 0.04), 50, 4.5),
    ])

    cuts = [
        (1, glass),
        (m["boot"], reveal),
        (m["hall"], white),
        (m["hall"] + 86, black),
    ]
    outro_at = m["outro"]
    for index, (t0, pid, mv) in enumerate(_moves(moves_of, m["e4"] - 2)):
        # f0 is the lift, t0 is only when it starts across the board.
        # Cutting at t0 left the previous shot up while the lights had already changed.
        start = max(m["e4"], mv.f0)
        if start >= outro_at - 12:
            break
        if start < cuts[-1][0] + 18:
            continue
        side, travel = _side(mv, index % 2 == 1)
        span = (mv.dst.xy - mv.src.xy).length
        dist = 0.32 if span > 0.10 else 0.27
        height = 0.15 if index % 2 == 0 else 0.23
        lens = 44 if span > 0.10 else 52
        aim0 = Vector((mv.src.x, mv.src.y, mv.src.z + 0.045))
        aim1 = Vector((mv.dst.x, mv.dst.y, max(mv.dst.z, mv.src.z) + 0.05))
        drift = Vector((mv.dst.x - mv.src.x, mv.dst.y - mv.src.y, 0.0))
        if drift.length > 0.07:
            drift = drift.normalized() * 0.07
        loc0 = _clear(aim0 + side * dist + travel * 0.03 + Vector((0.0, 0.0, height)), aim0)
        loc1 = _clear(loc0 + drift + Vector((0.0, 0.0, 0.03)), aim1)
        end = min(outro_at - 2, max(start + 12, mv.t1 + 10))
        cam = _shot(
            stage,
            f"Zive move {index}",
            [
                (start, tuple(loc0), tuple(aim0), lens, 4.0),
                (end, tuple(loc1), tuple(aim1), lens + 4, 4.5),
            ],
            mv,
            pieces[pid],
        )
        cuts.append((start, cam))

    hero = _shot(stage, "Zive hero", [
        (outro_at, (0.16, -0.40, 0.30), centre, 40, 5.6),
        (m["end"], (0.02, -0.64, 0.50), (0.0, 0.0, 0.02), 34, 8.0),
    ])
    cuts.append((outro_at, hero))
    for frame, cam in cuts:
        cc.cut(stage.scene, frame, cam)
    print("zive cuts", len(cuts), [(frame, cam.name) for frame, cam in cuts])
    return cuts[0][1]


def main():
    # cine_common only reads flags after "--". Blender drops that separator
    # when this file is launched on its own, and the film then writes the
    # continuous masters.
    if "--" not in sys.argv:
        sys.argv.append("--")
    if "--variant" not in sys.argv[sys.argv.index("--") + 1 :]:
        sys.argv.extend(["--variant", "zive"])
    film.main()


if __name__ == "__main__":
    main()
