"""Presentation film driven by timeline_presentation.json (see led_timeline.py).

  blender -b --factory-startup --python build_predstaveni.py -- [--preview] [--test 30,400] [--res 3840x2160] [--samples 32]

--preview renders 1280x720 at 12 samples to video/predstaveni_nahled.mp4.

Black void, amber backlight, one continuous camera with follow focus. The
only cut is the endgame time jump: framing continues, only the pieces change.
Every LED state comes from the firmware-faithful timeline. Hand moves are
baked per frame (lift, arc, tilt, soft landing) with contact shadows.
Output: video/predstaveni.mp4
"""

import json
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cine_common as cc  # noqa: E402
import led_effects as fx  # noqa: E402

TIMELINE = cc.ROOT / "timeline_presentation.json"
BLEND = cc.ROOT / "czechmate_predstaveni.blend"
RAW = cc.RENDER_TMP / "predstaveni_raw.mp4"
FINAL = cc.VIDEO_DIR / "predstaveni.mp4"
AMBER = (1.0, 0.43, 0.1)


class Stage:
    def __init__(self, scene, rig):
        self.scene = scene
        self.rig = rig
        self.coll = scene.collection

    def at(self, square, z=0.03):
        ri, fi = fx.rc(square)
        return (self.rig.xs[fi], self.rig.ys[ri], z)


SET_ASIDE = (-0.205, 0.05, -0.0002)  # captured piece goes onto the table left of the board
CONTACT_FADE = 0.008  # contact shadow is gone once the base is 8 mm above the glass
BOARD_HALF = 0.145


def clamp01(t):
    return max(0.0, min(1.0, t))


def pace(t):
    """Almost linear, with a short ease only so the piece doesn't kick or slam."""
    t = clamp01(t)
    return 0.82 * t + 0.18 * (t * t * (3.0 - 2.0 * t))


# Lowest visible hop, and how far a base must pass above someone it actually crosses.
MIN_HOP = 0.015
CLEAR_OVER = 0.018
# Foot radius. Queen and king use the wider royal base. A path clips a neighbour
# when the centres come closer than the two feet, which a diagonal does at 25 mm.
# Keep in step with led_timeline.FOOT / HEIGHT.
BODY = {
    "pawn": 0.014,
    "knight": 0.014,
    "bishop": 0.014,
    "rook": 0.014,
    "queen": 0.017,
    "king": 0.017,
}
# One pace for every piece. A neighbouring square is about half a second.
SPEED = 0.0028


class Move:
    """One path from f0 to f4. Empty moves are a low hill. A crossing rises only to the obstacle, then stays there."""

    def __init__(self, f0, f1, f2, f3, f4, src, dst, lift):
        self.f0, self.f1, self.f2, self.f3, self.f4 = f0, f1, f2, f3, f4
        self.src, self.dst = Vector(src), Vector(dst)
        self.lift = lift
        self.t0 = f0
        self.t1 = f4
        self.set_arch(MIN_HOP, 0.48)

    def set_arch(self, peak, rise, fall=None):
        self.peak = peak
        # Rise and fall are separate. A piece beside the landing square used to
        # be hit because the drop started as early as the climb.
        self.rise = min(0.90, max(0.0, rise))
        self.fall = self.rise if fall is None else min(0.90, max(0.0, fall))
        if self.rise + self.fall > 0.98:
            self.rise = max(0.0, 0.98 - self.fall)
        steps = 64
        prev = self._point(0.0)
        acc = 0.0
        self._lut = [(0.0, 0.0)]
        for i in range(1, steps + 1):
            s = i / steps
            pt = self._point(s)
            acc += (pt - prev).length
            prev = pt
            self._lut.append((s, acc))
        self._length = acc or 1.0

    def _factor(self, u):
        if self.rise > 1e-4 and u < self.rise:
            t = u / self.rise
        elif self.fall > 1e-4 and u > 1.0 - self.fall:
            t = (1.0 - u) / self.fall
        else:
            return 1.0
        return t * t * (3.0 - 2.0 * t)

    def _point(self, s):
        pos = self.src.lerp(self.dst, s)
        pos.z += self.peak * self._factor(s)
        return pos

    def position(self, f):
        t = (f - self.t0) / max(self.t1 - self.t0, 1)
        if t <= 0.0:
            return self.src.copy(), 0.0
        if t >= 1.0:
            return self.dst.copy(), 0.0
        target = pace(t) * self._length
        lo = 1
        hi = len(self._lut) - 1
        while lo < hi:
            mid = (lo + hi) // 2
            if self._lut[mid][1] < target:
                lo = mid + 1
            else:
                hi = mid
        s1, length1 = self._lut[lo]
        s0, length0 = self._lut[lo - 1]
        frac = 0.0 if length1 <= length0 else (target - length0) / (length1 - length0)
        s = s0 + (s1 - s0) * frac
        return self._point(s), math.sin(math.pi * s)

    def tilt(self, f):
        """A few degrees toward the destination, eased in and out. Zero at liftoff and landing."""
        span = max(self.t1 - self.t0, 1)
        t = clamp01((f - self.t0) / span)
        envelope = math.sin(math.pi * t) ** 2
        flat = self.dst.xy - self.src.xy
        if flat.length < 1e-6 or envelope < 1e-4:
            return 0.0, Vector((1.0, 0.0, 0.0))
        direction = Vector((flat.x, flat.y, 0.0)).normalized()
        return 0.10 * envelope, Vector((-direction.y, direction.x, 0.0))


def parse_moves(stage, keys):
    seat = stage.rig.seat
    moves = []
    i = 0
    while i < len(keys) - 1:
        if keys[i]["h"] > 0 or keys[i + 1]["h"] <= 0:
            i += 1
            continue
        j = next(n for n in range(i + 1, len(keys)) if keys[n]["h"] <= 0)
        seg = keys[i:j + 1]
        src_sq, land = seg[0]["sq"], seg[-1]
        lift = max(k["h"] for k in seg)
        f0, f1 = seg[0]["f"], seg[1]["f"]
        f2 = max(k["f"] for k in seg if k["sq"] == src_sq and k["h"] > 0)
        src = stage.at(src_sq, seat)
        if "aside" in land or "off" in land:
            f3 = land["f"] + 4
            dest = Vector(land["aside"]) if "aside" in land else SET_ASIDE
            moves.append(Move(f0, f1, f1 + 2, f3, f3 + 14, src, dest, lift))
        else:
            f3 = min((k["f"] for k in seg if k["sq"] == land["sq"] and k["h"] > 0), default=land["f"])
            moves.append(Move(f0, f1, f2, f3, land["f"], src, stage.at(land["sq"], seat), lift))
        i = j
    return moves


def build_pieces(stage, data, mats, templates):
    rig = stage.rig
    seat = rig.seat
    pieces, contacts = {}, {}
    for pid, info in data["pieces"].items():
        first = data["keys"][pid][0]
        obj = cc.make_piece(templates, info["kind"], info["color"], pid, stage.at(first["sq"], seat), mats, stage.coll)
        pieces[pid] = obj
        contacts[pid] = cc.contact_shadow(pid, stage.coll)

    moves_of = {pid: parse_moves(stage, keys) for pid, keys in data["keys"].items()}
    fit_peaks(stage, data, moves_of)
    equalize_speed(moves_of)
    for pid, keys in data["keys"].items():
        obj = pieces[pid]
        decal, decal_mat = contacts[pid]
        base_rot = obj.rotation_euler.to_matrix()
        moves = moves_of[pid]
        busy = [(mv.f0, mv.t1) for mv in moves]
        constant_frames = []

        def key_contact(frame, loc, fade):
            decal.location = (loc[0], loc[1], seat + 0.00005)
            decal.keyframe_insert("location", frame=frame)
            cc.key_fade(decal_mat, frame, fade)

        prev = None
        for k in keys:
            loc = stage.at(k["sq"], seat)
            if not any(a < k["f"] < b for a, b in busy):
                if k.get("snap") and prev is not None:
                    cc.key_loc(obj, k["f"] - 1, prev)
                    key_contact(k["f"] - 1, prev, 1.0)
                    constant_frames.append(k["f"] - 1)
                if "off" not in k:
                    cc.key_loc(obj, k["f"], loc)
                    key_contact(k["f"], loc, 1.0)
                    prev = loc
            if "off" in k:
                prev = SET_ASIDE

        # The first tilt key is the takeoff pitch. Without a rest key before it,
        # Blender holds that lean from frame 1 and the piece stands on one edge.
        rest = obj.rotation_euler.copy()
        rest_frames = {1}
        obj.keyframe_insert("rotation_euler", frame=1)
        for index, mv in enumerate(moves):
            before = max(1, mv.f0 - 1)
            obj.rotation_euler = rest
            obj.keyframe_insert("rotation_euler", frame=before)
            rest_frames.add(before)
            rotation = rest.copy()
            for f in range(mv.t0, mv.t1 + 1):
                pos, _ = mv.position(f)
                cc.key_loc(obj, f, pos)
                angle, axis = mv.tilt(f)
                rotation = (Matrix.Rotation(angle, 3, axis) @ base_rot).to_euler("XYZ", rotation)
                obj.rotation_euler = rotation
                obj.keyframe_insert("rotation_euler", frame=f)
                on_board = abs(pos.x) < BOARD_HALF and abs(pos.y) < BOARD_HALF
                key_contact(f, pos, clamp01(1.0 - (pos.z - seat) / CONTACT_FADE) if on_board else 0.0)
            after = mv.t1 + 1
            nxt = moves[index + 1].f0 if index + 1 < len(moves) else None
            if nxt != after:
                obj.rotation_euler = rest
                obj.keyframe_insert("rotation_euler", frame=after)
                rest_frames.add(after)

        for idblock in (obj, decal, decal_mat.node_tree):
            for curve in cc.fcurves_of(cc.action_of(idblock)):
                rotating = "rotation_euler" in curve.data_path
                for point in curve.keyframe_points:
                    frame = round(point.co.x)
                    held = frame in constant_frames or (rotating and frame in rest_frames)
                    point.interpolation = "CONSTANT" if held else "LINEAR"

    for item in data["hidden"]:
        pid = item["id"]
        frame = item["f"]
        obj = pieces[pid]

        def walk(node):
            yield node
            for child in node.children:
                yield from walk(child)

        for part in (*walk(obj), contacts[pid][0]):
            part.hide_render = False
            part.keyframe_insert("hide_render", frame=1)
            part.hide_render = True
            part.keyframe_insert("hide_render", frame=frame)
    return pieces, moves_of


TOP = {
    "pawn": 0.048,
    "knight": 0.056,
    "bishop": 0.064,
    "rook": 0.060,
    "queen": 0.072,
    "king": 0.084,
}


def equalize_speed(moves_of):
    """Every piece travels at one pace and lands on its beat. A short move waits, then goes."""
    moves = [(pid, mv) for pid, group in moves_of.items() for mv in group]
    squeezed = []
    for pid, mv in moves:
        room = max(8, mv.f4 - mv.f0)
        need = max(10, int(round(mv._length / SPEED)))
        span = min(room, need)
        mv.t1 = mv.f4
        mv.t0 = mv.f4 - span
        if need > room + 1:
            squeezed.append(f"{pid} {mv._length * 1000:.0f}mm/{span}f")
    speeds = [mv._length / max(mv.t1 - mv.t0, 1) * 1000.0 for _, mv in moves]
    print(
        f"move speed {min(speeds):.1f}..{max(speeds):.1f} mm/frame, "
        f"{len(squeezed)} of {len(moves)} had to go faster to land on time"
    )
    if squeezed:
        print("faster:", ", ".join(squeezed))


def fit_peaks(stage, data, moves_of):
    """Lift only enough to clear a piece the path actually crosses. Empty moves stay a low hop."""
    seat = stage.rig.seat

    def standing(pid, frame):
        for mv in moves_of[pid]:
            if mv.f0 <= frame <= mv.f4:
                return mv.position(frame)[0]
        loc = None
        for key in data["keys"][pid]:
            if key["f"] > frame:
                break
            if "aside" in key:
                loc = Vector(key["aside"])
            elif "off" in key:
                loc = Vector(SET_ASIDE)
            else:
                loc = Vector(stage.at(key["sq"], seat))
        return loc

    highs = []
    for pid, moves in moves_of.items():
        mover = data["pieces"][pid]["kind"]
        for mv in moves:
            peak = MIN_HOP
            enter = 1.0
            leave = 0.0
            delta = mv.dst.xy - mv.src.xy
            span2 = delta.length_squared
            if span2 > 1e-8:
                for other, info in data["pieces"].items():
                    if other == pid:
                        continue
                    there = standing(other, mv.f0 + (mv.f4 - mv.f0) // 2)
                    if there is None:
                        continue
                    along = (there.xy - mv.src.xy).dot(delta) / span2
                    u = clamp01(along)
                    closest = mv.src.xy + delta * u
                    dist = (closest - there.xy).length
                    reach = BODY[mover] + BODY[info["kind"]] + 0.002
                    if dist > reach or u < 0.02 or u > 0.98:
                        continue
                    half = math.sqrt(max(0.0, reach * reach - dist * dist)) / math.sqrt(span2)
                    enter = min(enter, max(0.0, u - half))
                    leave = max(leave, min(1.0, u + half))
                    peak = max(peak, there.z + TOP[info["kind"]] + CLEAR_OVER - seat)
            if peak <= MIN_HOP + 0.001:
                mv.set_arch(MIN_HOP, 0.48)
            else:
                # Full height before the first piece, and still full height
                # until the foot is past the last one. Then it drops.
                rise = min(0.35, enter * 0.65)
                fall = min(0.35, max(0.0, 1.0 - leave) * 0.65)
                mv.set_arch(peak, rise, fall)
                highs.append((mv.peak, pid))
    if highs:
        need, pid = max(highs)
        print(f"highest arc {need * 1000:.0f} mm {pid} ({len(highs)} moves clear someone)")
    else:
        print(f"every arc is the low hop {MIN_HOP * 1000:.0f} mm")


def check_clearance(stage, data, moves_of):
    """A travelling base has to clear whatever it passes over."""
    seat = stage.rig.seat

    def place(pid, frame):
        for mv in moves_of[pid]:
            if mv.t0 <= frame <= mv.t1:
                return mv.position(frame)[0]
        loc = None
        for key in data["keys"][pid]:
            if key["f"] > frame:
                break
            if "aside" in key:
                loc = Vector(key["aside"])
            elif "off" in key:
                loc = Vector(SET_ASIDE)
            else:
                loc = Vector(stage.at(key["sq"], seat))
        return loc

    worst = None
    for pid, moves in moves_of.items():
        for mv in moves:
            for frame in range(mv.t0 + 1, mv.t1):
                pos, _ = mv.position(frame)
                for other, info in data["pieces"].items():
                    if other == pid:
                        continue
                    there = place(other, frame)
                    if there is None:
                        continue
                    dist = (pos.xy - there.xy).length
                    reach = BODY[data["pieces"][pid]["kind"]] + BODY[info["kind"]]
                    if dist >= reach:
                        continue
                    # Feet overlap in plan. The mover's body starts at its base;
                    # the other may be in the air as well, so either can be clear.
                    over = pos.z - (there.z + TOP[info["kind"]])
                    under = there.z - (pos.z + TOP[data["pieces"][pid]["kind"]])
                    gap = max(over, under)
                    if worst is None or gap < worst[0]:
                        worst = (gap, pid, other, frame, pos.z - seat)
                    if gap < -0.001:
                        raise RuntimeError(
                            f"{pid} passes through {other} at frame {frame} "
                            f"({gap * 1000:.0f} mm, {dist * 1000:.0f} mm apart)"
                        )
    if worst:
        gap, pid, other, frame, height = worst
        print(f"clearance {gap * 1000:.0f} mm {pid} over {other} frame {frame} base {height * 1000:.0f} mm")


def build_hall_overlay(stage, start, peak, end):
    """Graphic x-ray of the Hall pairs under the glass, not a firmware LED state."""
    rig = stage.rig
    boxes = []
    for x in rig.xs:
        for y in rig.ys:
            for sx in (-cc.HALL_PITCH / 2, cc.HALL_PITCH / 2):
                boxes.append(((x + sx, y, rig.surface + 0.0004), (0.0042, 0.0026, 0.0003)))
            boxes.append(((x, y, rig.surface + 0.0003), (cc.HALL_PITCH, 0.0005, 0.0002)))
    mat = cc.fade_emissive("Hall overlay", (0.05, 0.62, 1.0), 14.0)
    cc.mesh_obj("Hall overlay", cc.box_mesh("Hall overlay", boxes), mat, stage.coll).visible_shadow = False
    cc.key_fade(mat, 1, 0.0)
    cc.key_fade(mat, start, 0.0)
    cc.key_fade(mat, peak, 1.0)
    cc.key_fade(mat, end - 12, 1.0)
    cc.key_fade(mat, end, 0.0)


def build_embers(stage, start, end):
    """Glowing sparks that leave the board. They are born on the lip in front of
    the piece feet and rise toward the camera, so they never enter a mesh."""
    rng = random.Random(11)
    xs = [float(v) for v in stage.rig.xs]
    near = min(float(v) for v in stage.rig.ys)
    # Chassis front is y=-0.1445 and the nearest feet reach it. Sparks start
    # just off that face and rise, so they leave the board without entering a piece.
    lip_y = -0.149
    print(f"embers lip_y {lip_y:.4f} near {near:.4f} surface {stage.rig.surface:.4f} seat {stage.rig.seat:.4f}")
    warm = cc.emissive("Ember", AMBER, 12.0)
    hot = cc.emissive("Ember hot", (1.0, 0.82, 0.55), 16.0)
    mesh = bpy.data.meshes.new("Ember")
    mesh.from_pydata(
        [(0, 0, 1), (1, 0, 0), (0, 1, 0), (-1, 0, 0), (0, -1, 0), (0, 0, -1)],
        [],
        [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 1), (5, 2, 1), (5, 3, 2), (5, 4, 3), (5, 1, 4)],
    )
    mesh.materials.append(warm)
    spots = []
    for i, x in enumerate(xs):
        spots.append((x, lip_y))
        spots.append((x, lip_y - 0.007))
        if i + 1 < len(xs):
            spots.append(((x + xs[i + 1]) * 0.5, lip_y - 0.003))
    for i, (x, y) in enumerate(spots):
        obj = bpy.data.objects.new(f"ember {i}", mesh)
        stage.coll.objects.link(obj)
        if i % 4 == 0:
            obj.material_slots[0].link = "OBJECT"
            obj.material_slots[0].material = hot
        size = rng.uniform(0.0011, 0.0020)
        delay = rng.randint(0, 28)
        life = rng.randint(26, 44)
        f0 = start + delay
        f1 = min(end, f0 + life)
        origin = Vector((
            x + rng.uniform(-0.004, 0.004),
            y + rng.uniform(-0.004, 0.0),
            rng.uniform(0.006, 0.014),
        ))
        # Stay on the lip. Flying toward the lens turned a 3 mm spark into a
        # glowing square in the black beside the board.
        far = origin + Vector((
            rng.uniform(-0.002, 0.002),
            rng.uniform(-0.004, -0.001),
            rng.uniform(0.018, 0.035),
        ))
        obj.scale = (0.0, 0.0, 0.0)
        obj.keyframe_insert("scale", frame=max(1, f0 - 1))
        obj.scale = (size, size, size * rng.uniform(1.4, 2.2))
        obj.keyframe_insert("scale", frame=f0 + 5)
        obj.keyframe_insert("scale", frame=max(f0 + 8, f1 - 8))
        obj.scale = (0.0, 0.0, 0.0)
        obj.keyframe_insert("scale", frame=f1)
        cc.set_interpolation(obj, "LINEAR", "scale")
        cc.key_loc(obj, max(1, f0), origin)
        cc.key_loc(obj, f1, far)
        cc.set_interpolation(obj, "BEZIER", "location")
        obj.rotation_euler = (rng.uniform(0, 1.0), 0.0, rng.uniform(0, 6.28))
        obj.keyframe_insert("rotation_euler", frame=max(1, f0))
        obj.rotation_euler = (
            obj.rotation_euler.x + rng.uniform(-0.4, 0.4),
            rng.uniform(-0.3, 0.3),
            obj.rotation_euler.z + rng.uniform(-1.2, 1.2),
        )
        obj.keyframe_insert("rotation_euler", frame=f1)
        obj.visible_shadow = False
        obj.hide_render = False
        obj.keyframe_insert("hide_render", frame=1)
        obj.hide_render = True
        obj.keyframe_insert("hide_render", frame=f1 + 1)
        cc.set_interpolation(obj, "CONSTANT", "hide_render")


def build_lights(stage, card_mat, m):
    coll = stage.coll
    aim = bpy.data.objects.new("Aim", None)
    aim.location = (0, 0, 0.03)
    coll.objects.link(aim)
    lip = bpy.data.objects.new("Lip aim", None)
    lip.location = (0.0, -0.145, 0.008)
    coll.objects.link(lip)
    frost = bpy.data.objects.new("Frost aim", None)
    frost.location = (0.0, 0.0, 0.023)
    coll.objects.link(frost)
    key = cc.area(coll, "Key", (-0.55, -0.55, 0.75), 0.5, 9, (1.0, 0.96, 0.92), aim, glossy=True)
    fill = cc.area(coll, "Top fill", (0.0, 0.05, 1.0), 0.8, 3, (0.95, 0.97, 1.0), aim, glossy=True)
    rim_l = cc.area(coll, "Rim amber L", (-0.55, 0.48, 0.2), 0.04, 12, AMBER, aim, "RECTANGLE", 0.9, glossy=True)
    rim_r = cc.area(coll, "Rim amber R", (0.55, 0.48, 0.2), 0.04, 12, AMBER, aim, "RECTANGLE", 0.9, glossy=True)
    kicker = cc.area(coll, "Kicker cool", (0.68, -0.18, 0.14), 0.04, 5, (0.55, 0.7, 1.0), aim, "RECTANGLE", 0.7, glossy=True)
    rim_cool = cc.area(coll, "Rim cool", (-0.1, 0.6, 0.55), 0.5, 3, (0.8, 0.88, 1.0), aim, glossy=True)
    # A strip on the vertical face only. It must not clear the top edge onto the pieces.
    graze = cc.area(
        coll, "Edge graze", (0.0, -0.28, 0.009), 0.36, 2.4,
        (0.75, 0.82, 0.9), lip, "RECTANGLE", 0.02, math.radians(60), glossy=True,
    )
    wall_l = bpy.data.objects.new("Wall L", None)
    wall_l.location = (-0.145, 0.0, 0.01)
    coll.objects.link(wall_l)
    wall_r = bpy.data.objects.new("Wall R", None)
    wall_r.location = (0.145, 0.0, 0.01)
    coll.objects.link(wall_r)
    side_l = cc.area(
        coll, "Side graze L", (-0.30, 0.0, 0.012), 0.30, 3.2,
        (0.82, 0.88, 1.0), wall_l, "RECTANGLE", 0.02, math.radians(55), glossy=True,
    )
    side_r = cc.area(
        coll, "Side graze R", (0.30, 0.0, 0.012), 0.30, 3.2,
        (0.82, 0.88, 1.0), wall_r, "RECTANGLE", 0.02, math.radians(55), glossy=True,
    )
    # Hard skim across the pane so the sandblast grain and the heavier squares catch.
    rake = cc.area(coll, "Glass rake", (0.16, 0.14, 0.045), 0.06, 0, (0.86, 0.91, 1.0), frost, "RECTANGLE", 0.12)

    def key_energy(lamp, pairs):
        for frame, energy in pairs:
            lamp.data.energy = energy
            lamp.data.keyframe_insert("energy", frame=frame)

    # Nothing but the wall graze until the glass detail. The room comes up with the boot.
    key_energy(key, ((1, 0.0), (m["boot"], 0.0), (m["boot"] + 10, 2.2), (m["boot"] + 36, 9)))
    key_energy(fill, ((1, 0.0), (m["boot"] + 16, 0.0), (m["boot"] + 40, 4)))
    key_energy(kicker, ((1, 0.0), (m["boot"] + 18, 1.5), (m["boot"] + 40, 8)))
    key_energy(rim_cool, ((1, 0.0), (m["boot"] + 28, 4)))
    key_energy(graze, ((1, 0.6), (40, 1.3), (m["glass"] - 16, 1.6), (m["glass"] + 12, 1.1), (m["idle"], 2.4), (m["end"], 2.6)))
    key_energy(side_l, ((1, 0.0), (m["boot"] + 20, 0.0), (m["boot"] + 40, 3.2), (m["end"], 3.4)))
    key_energy(side_r, ((1, 0.0), (m["boot"] + 20, 0.0), (m["boot"] + 40, 3.2), (m["end"], 3.4)))
    key_energy(rake, ((1, 0.0), (m["glass"] - 6, 0.0), (m["glass"] + 18, 6.5), (m["boot"] - 4, 5.0), (m["boot"] + 36, 0.0)))
    for frame, level in ((1, 0.0), (m["boot"], 0.0), (m["boot"] + 8, 0.6),
                         (m["boot"] + 14, 1.5), (m["boot"] + 44, 1.15), (m["idle"], 0.95),
                         (m["endgame_cut"] - 2, 0.95), (m["timelapse_end"], 1.05), (m["mate"], 0.9),
                         (m["mate"] + 3, 2.8), (m["mate"] + 40, 1.15), (m["outro"], 1.15), (m["end"], 1.35)):
        cc.key_card(card_mat, frame, level)
    for lamp in (rim_l, rim_r):
        key_energy(lamp, ((1, 0.0), (m["boot"] + 8, 0.0), (m["boot"] + 24, 16), (m["idle"], 10),
                          (m["mate"], 10), (m["mate"] + 3, 24), (m["mate"] + 40, 14), (m["end"], 16)))


FOLLOW = 0.28  # drift toward the piece; 0.7 locked the frame on one figure


def build_camera(stage, m, pieces, moves_of):
    """One continuous camera. After the pin and the queen sacrifice it stays
    high over the whole board, then closes on the mating knight."""
    at = stage.at
    centre = (0.0, 0.0, 0.02)
    keys = [
        # Along the vertical face, looking slightly down so the top and the pieces
        # stay out of frame. Then a tight skim of the sandblast, then the reveal.
        # Aim stays on the face. Looking past the right-hand end was just black.
        # Close to the face and aimed down it, so the pieces above the lip stay out.
        (1, (-0.06, -0.214, 0.009), (0.06, -0.145, 0.006), 85, 5.6),
        (m["glass"] - 18, (0.02, -0.210, 0.009), (0.13, -0.145, 0.006), 85, 5.6),
        # Onto the glass at the a-file, then a fly down the empty middle,
        # parallel to the two ranks of pieces (white left of frame, black right).
        (m["glass"] + 2, (-0.15, -0.08, 0.034), (-0.08, 0.0, 0.023), 42, 4.5),
        (m["glass"] + 18, (-0.11, 0.0, 0.040), (-0.02, 0.0, 0.023), 34, 3.6),
        (m["glass"] + 44, (0.00, 0.0, 0.040), (0.10, 0.0, 0.023), 32, 3.2),
        (m["glass"] + 70, (0.11, 0.0, 0.042), (0.20, 0.0, 0.024), 32, 3.6),
        # Open the lens before travelling. A long lens on the glass smears,
        # and the same lens in front of the white rank crops the board.
        (m["boot"] + 8, (0.10, -0.045, 0.095), (-0.02, -0.01, 0.022), 32, 8.0),
        (m["boot"] + 58, (0.06, -0.46, 0.38), centre, 36, 8.0),
        (m["idle"], (0.18, -0.58, 0.36), centre, 34, 7.1),
        # White magnets, then a slow board-orbit so the cut to black does not
        # flip the viewer 180°. Hold each side; the east key is the compass.
        (m["hall"] - 10, (0.10, -0.42, 0.32), (0.0, -0.08, 0.03), 32, 8.0),
        (m["hall"] + 36, (0.0, -0.34, 0.38), (0.0, -0.10, 0.04), 28, 8.0),
        (m["hall"] + 68, (0.0, -0.34, 0.38), (0.0, -0.10, 0.04), 28, 8.0),
        (m["hall"] + 88, (0.22, -0.22, 0.36), (0.0, -0.02, 0.035), 30, 8.0),
        (m["hall"] + 110, (0.36, 0.0, 0.38), centre, 30, 8.0),
        (m["hall"] + 130, (0.20, 0.24, 0.36), (0.0, 0.06, 0.035), 30, 8.0),
        (m["hall"] + 150, (0.0, 0.34, 0.38), (0.0, 0.10, 0.04), 28, 8.0),
        (m["hall"] + 168, (0.0, 0.34, 0.38), (0.0, 0.10, 0.04), 28, 8.0),
        (m["e4"] + 24, (0.08, -0.16, 0.40), centre, 34, 8.0),
        (m["e4"] + 56, (0.16, -0.46, 0.34), at("e2", 0.03), 36, 7.1),
        (m["e4_land"], (0.20, -0.42, 0.36), (0.02, -0.02, 0.02), 34, 8.0),
        (m["d6"] - 10, (0.36, 0.04, 0.38), (0.0, 0.02, 0.02), 34, 8.0),
        (m["d6"] + 50, (0.22, 0.50, 0.32), at("e5", 0.03), 36, 6.3),
        (m["illegal"] - 10, (0.06, 0.02, 0.50), (0.04, -0.04, 0.02), 32, 8.0),
        (m["illegal"] + 22, (0.10, -0.18, 0.48), centre, 32, 8.0),
        (m["illegal"] + 50, (0.20, -0.50, 0.40), at("g2", 0.04), 34, 8.0),
        (m["illegal_land"] + 20, (0.36, -0.24, 0.32), at("g3", 0.04), 34, 7.1),
        (m["recover"] - 5, (0.34, -0.28, 0.32), at("g3", 0.04), 34, 7.1),
        (m["recover"] + 60, (0.16, -0.50, 0.34), at("f2", 0.03), 34, 6.3),
        (m["c6"] - 8, (0.0, 0.18, 0.50), centre, 32, 8.0),
        (m["c6"] + 36, (-0.28, 0.44, 0.38), at("b8", 0.03), 34, 8.0),
        (m["c6"] + 60, (-0.16, 0.40, 0.36), at("c6", 0.03), 34, 8.0),
        (m["bb5"], (0.40, -0.28, 0.38), at("f1", 0.04), 34, 7.1),
        (m["bb5"] + 60, (0.08, -0.42, 0.36), at("c4", 0.03), 34, 7.1),
        (m["castle"] - 8, (0.32, -0.48, 0.34), at("b1", 0.04), 34, 7.1),
        (m["castle"] + 24, (0.18, -0.44, 0.34), at("c3", 0.03), 36, 8.0),
        (m["castle_king"] - 12, (-0.22, 0.48, 0.36), at("c8", 0.04), 34, 8.0),
        (m["castle_king"] + 36, (0.18, 0.42, 0.34), at("g4", 0.04), 34, 7.1),
        (m["castle_done"], (0.22, -0.08, 0.42), centre, 34, 8.0),
        (m["capture"], (0.10, -0.46, 0.32), at("f3", 0.04), 36, 8.0),
        (m["capture_victim"], (0.06, 0.08, 0.38), at("e5", 0.04), 34, 7.1),
        (m["capture_queen"] + 10, (0.22, 0.40, 0.36), at("g4", 0.04), 34, 7.1),
        (m["capture_queen"] + 70, (0.10, -0.48, 0.34), at("d1", 0.04), 34, 8.0),
        (m["check"] - 80, (0.16, -0.36, 0.38), at("c4", 0.04), 34, 7.1),
        (m["check"] + 5, (0.02, -0.08, 0.52), at("e8", 0.03), 32, 8.0),
        (m["endgame_cut"] - 1, (0.10, -0.28, 0.40), at("f7", 0.04), 32, 8.0),
        (m["timelapse_end"], (0.04, -0.36, 0.44), at("e7", 0.03), 32, 8.0),
        (m["mate_move"] + 8, (0.12, -0.38, 0.36), at("c3", 0.04), 34, 8.0),
        (m["mate"] - 8, (0.10, -0.22, 0.38), at("d5", 0.05), 34, 8.0),
        (m["mate"], (0.08, -0.18, 0.40), at("e7", 0.05), 36, 8.0),
        (m["mate"] + 60, (0.22, -0.46, 0.46), centre, 34, 8.0),
        (m["outro"], (0.0, -0.56, 0.46), (0.0, 0.0, 0.02), 40, 8.0),
        (m["end"], (0.0, -0.68, 0.56), (0.0, 0.0, 0.02), 38, 8.0),
    ]
    cam, aim = cc.camera(stage.coll, "Camera", lens=keys[0][3], fstop=keys[0][4])
    cc.shot(stage.scene, cam, aim, keys)
    cc.add_noise(cam, "location", 0.0004, 46.0, 3.7)

    # Focus normally rides on the aim; in the opening it racks on its own.
    focus = bpy.data.objects.new("Camera focus", None)
    stage.coll.objects.link(focus)
    cam.data.dof.focus_object = focus
    for frame, loc in ((1, (-0.04, -0.145, 0.008)), (m["glass"] - 24, (0.14, -0.145, 0.008)),
                       (m["glass"] + 8, (-0.06, 0.0, 0.023)),
                       (m["glass"] + 44, (0.08, 0.0, 0.023)),
                       (m["glass"] + 72, (0.16, 0.0, 0.023)),
                       (m["boot"] + 4, (0.0, 0.0, 0.023))):
        cc.key_loc(focus, frame, loc)
    ride = focus.constraints.new("COPY_LOCATION")
    ride.target = aim
    for frame, value in ((1, 0.0), (m["boot"] + 8, 0.0), (m["boot"] + 48, 1.0)):
        ride.influence = value
        ride.keyframe_insert("influence", frame=frame)

    # Follow focus on every carried piece: the aim eases onto the piece while it
    # is in the hand and lets go after it is set down.
    for pid, moves in moves_of.items():
        for n, mv in enumerate(moves):
            if mv.t1 - mv.t0 < 20 or mv.dst.z < stage.rig.surface - 0.001:
                continue
            handle = bpy.data.objects.new(f"{pid} handle {n}", None)
            stage.coll.objects.link(handle)
            handle.parent = pieces[pid]
            handle.location = (0.0, 0.0, 0.02 / pieces[pid].scale.z)
            con = aim.constraints.new("COPY_LOCATION")
            con.target = handle
            span = mv.t1 - mv.t0
            on = mv.t0 + max(8, span // 4)
            for frame, value in ((mv.t0 - 4, 0.0), (on, FOLLOW), (mv.t1, FOLLOW), (mv.t1 + 18, 0.0)):
                con.influence = value
                con.keyframe_insert("influence", frame=frame)
    return cam


def render_edit(res, frames, captions, out, name="Edit"):
    edit = cc.edit_scene(RAW, out, res, frames, captions, name=name)
    bpy.ops.render.render(animation=True, scene=edit.name)
    print("final", out)


def active_camera(scene, frame):
    """The camera a cut marker has bound, once that frame is reached."""
    chosen = scene.camera
    best = -1
    for marker in scene.timeline_markers:
        if marker.camera is None or marker.frame > frame or marker.frame < best:
            continue
        best = marker.frame
        chosen = marker.camera
    if chosen is not None:
        scene.camera = chosen
    return chosen


def main():
    global RAW, FINAL, BLEND
    preview = "--preview" in cc.args()
    variant = cc.arg_value("--variant")
    if variant:
        stem = f"predstaveni_{variant}"
        RAW = cc.RENDER_TMP / (f"{stem}_nahled_raw.mp4" if preview else f"{stem}_raw.mp4")
        FINAL = cc.VIDEO_DIR / (f"{stem}_nahled.mp4" if preview else f"{stem}.mp4")
        BLEND = cc.ROOT / f"czechmate_{stem}.blend"
        labeled_name = f"{stem}_nahled_popisky.mp4" if preview else f"{stem}_popisky.mp4"
        print("variant", variant, "final", FINAL, "blend", BLEND)
    elif preview:
        RAW = cc.RENDER_TMP / "predstaveni_nahled_raw.mp4"
        FINAL = cc.VIDEO_DIR / "predstaveni_nahled.mp4"
        labeled_name = "predstaveni_nahled_popisky.mp4"
    else:
        labeled_name = "predstaveni_popisky.mp4"
    res = tuple(int(v) for v in cc.arg_value("--res", "1280x720" if preview else "1920x1080").split("x"))
    samples = int(cc.arg_value("--samples", "12" if preview else "24"))
    uhd = (not preview) and res[1] >= 2160
    if uhd:
        if variant:
            stem = f"predstaveni_{variant}_4k"
            RAW = cc.RENDER_TMP / f"{stem}_raw.mp4"
            FINAL = cc.VIDEO_DIR / f"{stem}.mp4"
            labeled_name = f"{stem}_popisky.mp4"
        else:
            RAW = cc.RENDER_TMP / "predstaveni_4k_raw.mp4"
            FINAL = cc.VIDEO_DIR / "predstaveni_4k.mp4"
            labeled_name = "predstaveni_4k_popisky.mp4"
        print("uhd", res, "final", FINAL)
    test = cc.arg_value("--test")
    data = json.loads(TIMELINE.read_text(encoding="utf-8"))
    m = data["markers"]
    frames = data["frame_end"]

    bpy.ops.wm.read_factory_settings(use_empty=True)
    if "--edit-only" in cc.args():
        logo = [c for c in data["captions"] if c.get("style") in ("logo", "tagline")]
        render_edit(res, frames, logo, FINAL, "Edit")
        labeled = cc.VIDEO_DIR / labeled_name
        render_edit(res, frames, data.get("labels", data["captions"]), labeled, "EditPopisky")
        return
    scene = bpy.context.scene
    scene.name = "Zive" if variant == "zive" else "Predstaveni"
    rig = cc.build_board(scene.collection, with_pcb=False)
    stage = Stage(scene, rig)

    mats = cc.piece_materials()
    templates = cc.piece_templates(mats[0])
    pieces, moves_of = build_pieces(stage, data, mats, templates)
    check_clearance(stage, data, moves_of)

    leds = cc.Leds(rig, scene.collection, glow_strength=6.4, light_energy=0.55, shadows=True, dies=False)
    leds.key(1, {})
    for entry in data["leds"]:
        leds.key(entry["f"], {sq: tuple(rgb) for sq, rgb in entry["leds"].items()})
    leds.finish()

    hall = m["hall"]
    cc.key_magnets(mats, 1, 0.0)
    cc.key_magnets(mats, hall + 6, 0.0)
    cc.key_magnets(mats, hall + 22, 3.5)
    cc.key_magnets(mats, m["e4"] - 12, 3.5)
    cc.key_magnets(mats, m["e4"], 0.0)
    for mat in mats[:2]:
        cc.key_ghost(mat, 1, 0.0)
        cc.key_ghost(mat, hall + 4, 0.0)
        cc.key_ghost(mat, hall + 22, 0.85)
        cc.key_ghost(mat, m["e4"] - 12, 0.85)
        cc.key_ghost(mat, m["e4"], 0.0)
    build_hall_overlay(stage, hall + 66, hall + 84, m["e4"])

    cc.black_world(scene)
    # No studio floor: a finite plane clips the lip skim and glass fly (anything
    # under z≈0 vanishes). The board sits in black void; grazes light the metal.
    _card, card_mat = cc.gradient_card(scene.collection, "Amber backlight", (0.0, 1.15, 0.3), (2.8, 1.5), AMBER, 1.0)
    build_lights(stage, card_mat, m)
    build_embers(stage, m["boot"], m["boot"] + 16)
    if variant == "zive":
        import build_predstaveni_zive as zive
        scene.camera = zive.build_camera(stage, m, pieces, moves_of)
    else:
        scene.camera = build_camera(stage, m, pieces, moves_of)

    scene.frame_start = 1
    scene.frame_end = frames
    if variant == "zive":
        exposure = float(cc.arg_value("--exposure", "-1.0"))
        cc.render_settings(scene, res, samples=samples, exposure=exposure)
        cc.compositor(scene, bloom=0.4, threshold=1.5, dispersion=0.01, vignette=0.32)
    else:
        cc.render_settings(scene, res, samples=samples, exposure=float(cc.arg_value("--exposure", "-1.0")))
        cc.compositor(scene, bloom=0.4, threshold=1.4, dispersion=0.01, vignette=0.34)
    cc.RENDER_TMP.mkdir(parents=True, exist_ok=True)
    cc.VIDEO_DIR.mkdir(parents=True, exist_ok=True)
    cc.video_output(scene, RAW, quality="HIGH" if uhd else "PERC_LOSSLESS")
    print("scene ready", frames)

    if test:
        tag = f"pred_{variant}_" if variant else "pred_"
        for frame in (int(v) for v in test.split(",")):
            scene.frame_set(frame)
            active_camera(scene, frame)
            print("camera", scene.camera.name if scene.camera else None)
            cc.still_output(scene, cc.RENDER_TMP / f"{tag}{frame:04d}.jpg")
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", frame)
        scene.frame_set(int(test.split(",")[-1]))
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
        print("saved", BLEND)
        return
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    print("saved", BLEND)
    bpy.ops.render.render(animation=True, scene=scene.name)
    print("raw", RAW)
    logo = [c for c in data["captions"] if c.get("style") in ("logo", "tagline")]
    render_edit(res, frames, logo, FINAL, "Edit")
    labeled = cc.VIDEO_DIR / labeled_name
    render_edit(res, frames, data.get("labels", data["captions"]), labeled, "EditPopisky")


if __name__ == "__main__":
    main()
