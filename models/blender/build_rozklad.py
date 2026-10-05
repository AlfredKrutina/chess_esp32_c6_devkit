"""Exploded view of the board.

  blender -b --factory-startup --python build_rozklad.py -- [--preview] [--test 120,200]

The opening still lifts one layer at a time, so nothing passes through the
part in its path, but each move is short. A callout fades in when that part
starts, and it is gone before the next one leaves. Screws are not labelled.
The ending is the opposite: every layer returns together on one ease-in-out.
Screws stay under the lid for that move and thread in only after the lid is
home. Output: video/rozklad.mp4
"""

import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_scene as board  # noqa: E402
import cine_common as cc  # noqa: E402
import led_effects as fx  # noqa: E402
import chess_start  # noqa: E402

FRAMES = 690
BLEND = cc.ROOT / "czechmate_rozklad.blend"
RAW = cc.RENDER_TMP / "rozklad_raw.mp4"
FINAL = cc.VIDEO_DIR / "rozklad.mp4"
PCB_GLB = cc.ROOT / "board" / "czechmate_v3.glb"

OFFSETS = {"glass": 0.15, "foil": 0.105, "grid": 0.058, "chassis": -0.07, "lid": -0.16}
PIECE_RISE = 0.205
PIECE_TRAVEL = 14
# Head sits just under the lid. Clear is out of the lid. Drop is below the lid's exploded pose.
SCREW_SEAT = -0.0004
SCREW_CLEAR = -0.018
SCREW_DROP = -0.188
# Opening is sequential and each step is one second. The return is one shared window.
SCREW_OUT = (16, 36)
LID_OPEN = (40, 56)
CHASSIS_OPEN = (70, 86)
PIECE_UP0 = 100
# The last piece finishes at PIECE_UP0 + 8 + PIECE_TRAVEL. Glass waits for that.
GLASS_OPEN = (130, 146)
FOIL_OPEN = (160, 176)
GRID_OPEN = (190, 206)
PCB_OPEN = (220, 236)
BOOT = 250
MAGNET = 354
MAGNET_END = 474
ASSEMBLE = (486, 626)
PCB_CLOSE = ASSEMBLE
CHASSIS_CLOSE = ASSEMBLE
LID_CLOSE = ASSEMBLE
GRID_CLOSE = ASSEMBLE
FOIL_CLOSE = ASSEMBLE
GLASS_CLOSE = ASSEMBLE
PIECE_DOWN0 = ASSEMBLE[0]
# Screws follow the lid at the exploded gap, then enter the holes.
SCREW_UNDER = -0.028
SCREW_IN = (ASSEMBLE[1], ASSEMBLE[1] + 32)


def ease(obj, f0, f1, start, end):
    cc.key_loc(obj, f0, start)
    cc.key_loc(obj, f1, end)


def smooth_close(obj, frame):
    """The segment after this frame starts and stops gently."""
    for curve in cc.fcurves_of(cc.action_of(obj)):
        if not curve.data_path.startswith(("location", "rotation_euler")):
            continue
        for key in curve.keyframe_points:
            if int(round(key.co[0])) != frame:
                continue
            key.interpolation = "CUBIC"
            key.easing = "EASE_IN_OUT"


def hold(obj, frames, paths=("location", "rotation_euler")):
    """Keep a pose until the next key instead of creeping toward it."""
    for curve in cc.fcurves_of(cc.action_of(obj)):
        if not curve.data_path.startswith(paths):
            continue
        for key in curve.keyframe_points:
            if int(round(key.co[0])) in frames:
                key.interpolation = "CONSTANT"


def cone_mesh(name, radius, depth, segments=16):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=0.0, depth=depth)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def make_screws(collection):
    """M2.5 self-tappers: small pan head, coarse thread, gimlet point."""
    metal = cc.principled("Screw steel", (0.72, 0.73, 0.75), 0.32, metallic=1.0, specular=0.55)
    recess = cc.principled("Screw recess", (0.05, 0.05, 0.055), 0.5, metallic=1.0)
    inset = 0.126
    spots = (
        (inset, inset), (inset, -inset), (-inset, inset), (-inset, -inset),
        (inset, 0.0), (-inset, 0.0),
    )
    roots = []
    for index, (x, y) in enumerate(spots):
        root = bpy.data.objects.new(f"screw {index}", None)
        root.location = (x, y, SCREW_SEAT)
        root.rotation_mode = "XYZ"
        collection.objects.link(root)
        head = cc.mesh_obj(f"screw head {index}", cc.cylinder_mesh(f"screw head {index}", 0.00215, 0.0014, 20), metal, collection)
        head.location = (0.0, 0.0, 0.0007)
        shank = cc.mesh_obj(f"screw shank {index}", cc.cylinder_mesh(f"screw shank {index}", 0.00105, 0.0064, 14), metal, collection)
        shank.location = (0.0, 0.0, 0.0046)
        tip = cc.mesh_obj(f"screw tip {index}", cone_mesh(f"screw tip {index}", 0.00105, 0.0022), metal, collection)
        tip.location = (0.0, 0.0, 0.0089)
        parts = [head, shank, tip]
        for rib in range(5):
            disc = cc.mesh_obj(
                f"screw thread {index} {rib}",
                cc.cylinder_mesh(f"screw thread {index} {rib}", 0.00132, 0.00022, 14),
                metal, collection,
            )
            disc.location = (0.0, 0.0, 0.0024 + rib * 0.00115)
            parts.append(disc)
        slot_x = cc.mesh_obj(
            f"screw slot x {index}",
            cc.box_mesh(f"screw slot x {index}", [((0.0, 0.0, 0.00125), (0.0024, 0.00028, 0.00028))]),
            recess, collection,
        )
        slot_y = cc.mesh_obj(
            f"screw slot y {index}",
            cc.box_mesh(f"screw slot y {index}", [((0.0, 0.0, 0.00125), (0.00028, 0.0024, 0.00028))]),
            recess, collection,
        )
        parts.extend((slot_x, slot_y))
        for part in parts:
            part.parent = root
        roots.append(root)
    return roots


def animate_screws(roots):
    out0, out1 = SCREW_OUT
    rise0, rise1 = ASSEMBLE
    inn0, inn1 = SCREW_IN
    mid = (out0 + out1) // 2
    spin = math.tau * 3.0
    for root in roots:
        root.location.z = SCREW_SEAT
        root.rotation_euler.z = 0.0
        root.keyframe_insert("location", frame=1)
        root.keyframe_insert("rotation_euler", frame=1)
        root.keyframe_insert("location", frame=out0)
        root.keyframe_insert("rotation_euler", frame=out0)
        root.location.z = SCREW_CLEAR
        root.rotation_euler.z = spin
        root.keyframe_insert("location", frame=mid)
        root.keyframe_insert("rotation_euler", frame=mid)
        root.location.z = SCREW_DROP
        root.rotation_euler.z = spin * 1.6
        root.keyframe_insert("location", frame=out1)
        root.keyframe_insert("rotation_euler", frame=out1)
        root.keyframe_insert("location", frame=rise0)
        root.keyframe_insert("rotation_euler", frame=rise0)
        # Same gap under the lid as in the exploded pose, so the lid never meets them.
        root.location.z = SCREW_UNDER
        root.rotation_euler.z = spin * 2.2
        root.keyframe_insert("location", frame=rise1)
        root.keyframe_insert("rotation_euler", frame=rise1)
        root.location.z = SCREW_SEAT
        root.rotation_euler.z = spin * 3.0
        root.keyframe_insert("location", frame=inn1)
        root.keyframe_insert("rotation_euler", frame=inn1)
        cc.ease_out(root, "location")
        cc.ease_out(root, "rotation_euler")
        hold(root, (out1,))
        smooth_close(root, rise0)
        smooth_close(root, inn0)


def paint_pcb(meshes):
    mask = cc.principled("Solder mask", (0.012, 0.045, 0.028), 0.55, specular=0.25)
    copper = cc.principled("Copper", (0.72, 0.42, 0.24), 0.35, metallic=1.0)
    gold = cc.principled("ENIG", (0.95, 0.78, 0.42), 0.28, metallic=1.0)
    silk = cc.principled("Silkscreen", (0.85, 0.85, 0.8), 0.6)
    ranked = sorted(meshes, key=lambda obj: len(obj.data.vertices))
    # Substrate, two silk layers, pads, vias, copper pour.
    assign = [mask, silk, silk, gold, gold, copper]
    for obj, mat in zip(ranked, assign):
        obj.data.materials.clear()
        obj.data.materials.append(mat)
        for poly in obj.data.polygons:
            poly.use_smooth = False


def make_header(collection, parent, edge_x):
    """16-pin header on the mating edge. The STEP has the footprint, not the body."""
    plastic = cc.principled("Header body", (0.02, 0.02, 0.022), 0.45)
    pin = cc.principled("Header pin", (0.78, 0.74, 0.62), 0.3, metallic=1.0)
    pitch = 0.00254
    count = 16
    span = (count - 1) * pitch
    housing = cc.mesh_obj(
        "Header housing",
        cc.box_mesh("Header housing", [((edge_x + 0.004, 0.0, 0.0045), (0.0065, span + 0.004, 0.0065))]),
        plastic, collection,
    )
    pins = []
    for i in range(count):
        y = -span / 2 + i * pitch
        pins.append(((edge_x + 0.0035, y, 0.0045), (0.006, 0.00045, 0.00045)))
    pins_obj = cc.mesh_obj("Header pins", cc.box_mesh("Header pins", pins), pin, collection)
    for obj in (housing, pins_obj):
        obj.parent = parent
    return housing


def load_pcbs(collection):
    """Four copies of czechmate_v3, one per 4x4 quadrant, headers toward the centre seam.
    The STEP has the footprints on +Z. Turn each board over so the connectors
    face the chassis. Standing up, they run through the grid."""
    created = board.import_board_part(PCB_GLB)
    meshes = []
    for obj in created:
        if obj.type != "MESH":
            continue
        if obj.parent is not None:
            world = obj.matrix_world.copy()
            obj.parent = None
            obj.matrix_world = world
        link_ok = obj.name not in collection.objects
        if link_ok:
            collection.objects.link(obj)
        meshes.append(obj)
    if len(meshes) < 4:
        raise RuntimeError(f"PCB glb produced {len(meshes)} meshes")
    paint_pcb(meshes)
    body = min(meshes, key=lambda obj: len(obj.data.vertices))
    xs = [vertex.co.x for vertex in body.data.vertices]
    zs = [vertex.co.z for vertex in body.data.vertices]
    edge_x = min(xs)
    # The flip sends +Z downward, so the old underside becomes the top.
    floor_z = min(zs)
    root = bpy.data.objects.new("PCB 0", None)
    collection.objects.link(root)
    for obj in meshes:
        cc.parent_keep(obj, root)
    make_header(collection, root, edge_x)
    root.location.z = (cc.PCB_Z + cc.PCB_T) + floor_z

    def clone(name):
        new = bpy.data.objects.new(name, None)
        collection.objects.link(new)
        new.location = root.location.copy()
        for child in list(root.children):
            copy = child.copy()
            collection.objects.link(copy)
            copy.parent = new
            copy.matrix_parent_inverse = child.matrix_parent_inverse.copy()
        return new

    boards = [root, clone("PCB 1"), clone("PCB 2"), clone("PCB 3")]
    # sx, sy, yaw. Yaw pi turns the left-edge header toward the vertical seam.
    places = ((-1, -1, math.pi), (1, -1, 0.0), (-1, 1, math.pi), (1, 1, 0.0))
    home = []
    apart = []
    for obj, (sx, sy, yaw) in zip(boards, places):
        obj.rotation_euler.x = math.pi
        obj.rotation_euler.z = yaw
        centre = Vector((sx * 0.070, sy * 0.070, obj.location.z))
        spread = centre + Vector((sx * 0.022, sy * 0.022, 0.0))
        obj.location = centre
        home.append(centre)
        apart.append(spread)
    return boards, home, apart


def label_rig(collection, title, detail, anchor, cam, font, index):
    root = bpy.data.objects.new(f"label {index}", None)
    root.location = anchor
    collection.objects.link(root)
    ink = cc.fade_emissive(f"Label ink {index}", (0.96, 0.97, 0.98), 3.2)
    dim = cc.fade_emissive(f"Label dim {index}", (0.72, 0.76, 0.82), 1.6)
    line = cc.mesh_obj(
        f"label line {index}",
        cc.box_mesh(f"label line {index}", [((0.034, 0.0, 0.0), (0.062, 0.00028, 0.00028))]),
        ink, collection,
    )
    dot = cc.mesh_obj(f"label dot {index}", cc.cylinder_mesh(f"label dot {index}", 0.0015, 0.0004, 16), ink, collection)
    line.parent = root
    dot.parent = root

    def words(body, size, mat, z):
        curve = bpy.data.curves.new(f"label {index} {z}", "FONT")
        curve.body = body
        curve.font = font
        curve.size = size
        curve.align_x = "LEFT"
        curve.align_y = "CENTER"
        curve.resolution_u = 8
        curve.materials.append(mat)
        obj = bpy.data.objects.new(f"label text {index} {z}", curve)
        obj.location = (0.070, 0.0, z)
        obj.parent = root
        collection.objects.link(obj)
        con = obj.constraints.new("TRACK_TO")
        con.target = cam
        con.track_axis = "TRACK_Z"
        con.up_axis = "UP_Y"
        return obj

    words(title, 0.012, ink, 0.006)
    words(detail, 0.0076, dim, -0.008)
    return root, (ink, dim)


def fade_window(mats, start, end):
    rise = 6
    for mat in mats:
        cc.key_fade(mat, 1, 0.0)
        cc.key_fade(mat, start, 0.0)
        cc.key_fade(mat, start + rise, 1.0)
        cc.key_fade(mat, max(start + rise + 4, end - rise), 1.0)
        cc.key_fade(mat, end, 0.0)


CAPTIONS = [
    {"f0": FRAMES - 24, "f1": FRAMES + 1, "text": "CzechMate", "style": "logo", "y": 0.905},
    {"f0": FRAMES - 16, "f1": FRAMES + 1, "text": "uvnitř chytré šachovnice", "style": "tagline", "y": 0.835},
]


def render_edit(res):
    edit = cc.edit_scene(RAW, FINAL, res, FRAMES, CAPTIONS)
    bpy.ops.render.render(animation=True, scene=edit.name)
    print("final", FINAL)


def main():
    global RAW, FINAL
    preview = "--preview" in cc.args()
    if preview:
        RAW = cc.RENDER_TMP / "rozklad_nahled_raw.mp4"
        FINAL = cc.VIDEO_DIR / "rozklad_nahled.mp4"
    res = tuple(int(v) for v in cc.arg_value("--res", "1280x720" if preview else "1920x1080").split("x"))
    samples = int(cc.arg_value("--samples", "12" if preview else "24"))
    test = cc.arg_value("--test")

    bpy.ops.wm.read_factory_settings(use_empty=True)
    if "--edit-only" in cc.args():
        render_edit(res)
        return
    scene = bpy.context.scene
    scene.name = "Rozklad"
    coll = scene.collection
    rig = cc.build_board(coll, with_pcb=False)
    cc.parent_keep(rig.button, rig.chassis)

    mats = cc.piece_materials()
    templates = cc.piece_templates(mats[0])
    piece_coll = bpy.data.collections.new("Pieces")
    coll.children.link(piece_coll)
    pieces = []
    for pid, kind, color, square in chess_start.layout():
        ri, fi = fx.rc(square)
        loc = (rig.xs[fi], rig.ys[ri], rig.surface)
        obj = cc.make_piece(templates, kind, color, pid, loc, mats, piece_coll)
        pieces.append((obj, fi, ri))

    pcbs, pcb_home, pcb_apart = load_pcbs(coll)
    print("pcb count", len(pcbs), "home", [tuple(round(v, 4) for v in p) for p in pcb_home])

    leds = cc.Leds(
        rig, coll, glow_strength=4.5, light_energy=0.22,
        glow_parent=rig.foil, light_parent=rig.foil, shadows=True,
    )

    layers = {"glass": rig.glass, "foil": rig.foil, "grid": rig.grid, "lid": rig.lid, "chassis": rig.chassis}
    windows = {
        "lid": (LID_OPEN, LID_CLOSE),
        "chassis": (CHASSIS_OPEN, CHASSIS_CLOSE),
        "grid": (GRID_OPEN, GRID_CLOSE),
        "foil": (FOIL_OPEN, FOIL_CLOSE),
        "glass": (GLASS_OPEN, GLASS_CLOSE),
    }
    for key, obj in layers.items():
        base = Vector(obj.location)
        lift = Vector((0.0, 0.0, OFFSETS[key]))
        (o0, o1), (c0, c1) = windows[key]
        cc.key_loc(obj, 1, base)
        cc.key_loc(obj, o0, base)
        cc.key_loc(obj, o1, base + lift)
        cc.key_loc(obj, c0, base + lift)
        cc.key_loc(obj, c1, base)
        cc.ease_out(obj, "location")
        hold(obj, (o1,))
        smooth_close(obj, c0)

    screws = make_screws(coll)
    animate_screws(screws)

    for obj, home, apart in zip(pcbs, pcb_home, pcb_apart):
        o0, o1 = PCB_OPEN
        c0, c1 = PCB_CLOSE
        cc.key_loc(obj, 1, home)
        cc.key_loc(obj, o0, home)
        cc.key_loc(obj, o1, apart)
        cc.key_loc(obj, c0, apart)
        cc.key_loc(obj, c1, home)
        cc.ease_out(obj, "location")
        hold(obj, (o1,))
        smooth_close(obj, c0)

    max_order = 0.0
    for _obj, fi, ri in pieces:
        rank = ri if ri < 4 else 7 - ri
        max_order = max(max_order, rank * 2 + abs(fi - 3.5) * 0.6)
    stagger = min(1.0, 8.0 / max(max_order, 0.001))
    for obj, fi, ri in pieces:
        base = Vector(obj.location)
        rank = ri if ri < 4 else 7 - ri
        order = rank * 2 + abs(fi - 3.5) * 0.6
        delay = int(order * stagger)
        up0 = PIECE_UP0 + delay
        up1 = up0 + PIECE_TRAVEL
        down0, down1 = ASSEMBLE
        top = base + Vector((0.0, 0.0, PIECE_RISE + (ri - 3.5) * 0.002))
        cc.key_loc(obj, 1, base)
        cc.key_loc(obj, up0, base)
        cc.key_loc(obj, up1, top)
        cc.key_loc(obj, down0, top)
        cc.key_loc(obj, down1, base)
        cc.ease_out(obj, "location")
        hold(obj, (up1,))
        smooth_close(obj, down0)
        decal, decal_mat = cc.contact_shadow(obj.name, piece_coll)
        decal.location = (base.x, base.y, rig.surface + 0.00015)
        cc.parent_keep(decal, rig.glass)
        for frame, fade in ((1, 1.0), (up0, 1.0), (up0 + 8, 0.0), (down0, 0.0), (down1, 1.0)):
            cc.key_fade(decal_mat, frame, fade)

    leds.key(1, {})
    end = leds.play(BOOT, fx.boot(), speed=1.8)
    leds.key(min(end + 2, MAGNET - 4), chess_start.movable())
    leds.finish()

    cc.key_magnets(mats, 1, 0.0)
    cc.key_magnets(mats, MAGNET, 0.0)
    cc.key_magnets(mats, MAGNET + 14, 3.5)
    cc.key_magnets(mats, MAGNET_END, 3.5)
    cc.key_magnets(mats, ASSEMBLE[0], 0.0)
    for mat in mats[:2]:
        cc.key_ghost(mat, 1, 0.0)
        cc.key_ghost(mat, MAGNET, 0.0)
        cc.key_ghost(mat, MAGNET + 16, 0.82)
        cc.key_ghost(mat, MAGNET_END, 0.82)
        cc.key_ghost(mat, ASSEMBLE[0], 0.0)

    cam, target = cc.camera(coll, "Camera", lens=50, fstop=7.1)
    scene.camera = cam
    feet = 0.25
    cc.shot(scene, cam, target, [
        (1, (0.46, -0.62, 0.24), (0.0, 0.0, 0.02), 48, 8),
        (SCREW_OUT[0], (0.32, -0.58, -0.14), (0.0, 0.0, -0.03), 32, 8),
        (LID_OPEN[0], (0.34, -0.64, -0.06), (0.0, 0.0, -0.02), 34, 8),
        (CHASSIS_OPEN[0], (0.38, -0.72, 0.02), (0.0, 0.0, 0.0), 36, 8),
        (PIECE_UP0, (0.40, -0.86, 0.22), (0.0, 0.0, 0.10), 36, 8),
        (GLASS_OPEN[0], (0.42, -0.98, 0.48), (0.0, 0.0, 0.08), 34, 7.1),
        (BOOT, (0.40, -0.92, 0.44), (0.0, 0.0, 0.08), 36, 7.1),
        (MAGNET - 8, (0.16, -0.55, 0.40), (0.06, -0.08, 0.18), 34, 8),
        (MAGNET + 20, (-0.06, -0.38, 0.40), (0.12, -0.12, feet), 30, 11),
        (MAGNET + 56, (-0.06, -0.38, 0.40), (0.12, -0.12, feet), 30, 11),
        (MAGNET + 96, (-0.06, 0.38, 0.40), (0.12, 0.12, feet), 30, 11),
        (MAGNET_END, (-0.06, 0.38, 0.40), (0.12, 0.12, feet), 30, 11),
        (ASSEMBLE[0] + 8, (0.28, -0.62, 0.34), (0.0, 0.0, 0.08), 40, 8),
        (FRAMES, (0.44, -0.64, 0.24), (0.0, 0.0, 0.02), 48, 8),
    ])

    font = bpy.data.fonts.load(str(cc.FONT_DIR / "seguisb.ttf"), check_existing=True)
    labels = (
        ("lid", "Spodní víko", "Kryt zespodu", LID_OPEN[0], CHASSIS_OPEN[0]),
        ("chassis", "Hliníkové šasi", "Kartáčovaný plášť", CHASSIS_OPEN[0], PIECE_UP0),
        ("pieces", "Figurky", "Stojí na skle", PIECE_UP0, GLASS_OPEN[0]),
        ("glass", "Mléčné sklo", "Pískovaný vzor šachovnice", GLASS_OPEN[0], FOIL_OPEN[0]),
        ("foil", "Difuzní fólie", "Rozptýlí světlo diod", FOIL_OPEN[0], GRID_OPEN[0]),
        ("grid", "Světelná mřížka", "Osm na osm polí", GRID_OPEN[0], PCB_OPEN[0]),
        ("pcb", "Čtyři desky", "Spojené konektorem na hraně", PCB_OPEN[0], BOOT),
        ("magnets", "Magnety ve figurkách", "Bílá modrá, černá červená", MAGNET, MAGNET_END),
    )
    edge_x = 0.162
    for index, (key, title, detail, start, end) in enumerate(labels):
        if key == "pieces":
            z = rig.surface + PIECE_RISE + 0.02
            parent = None
        elif key == "magnets":
            z = rig.surface + PIECE_RISE + 0.012
            parent = None
        elif key == "pcb":
            z = cc.PCB_Z + 0.01
            parent = None
        else:
            obj = layers[key]
            z = max(corner[2] for corner in (obj.matrix_world @ Vector(bound) for bound in obj.bound_box)) + 0.004
            parent = obj
        y = 0.02
        root, label_mats = label_rig(coll, title, detail, (edge_x, y, z), cam, font, index)
        if parent is not None:
            cc.parent_keep(root, parent)
        fade_window(label_mats, start, end)

    aim = bpy.data.objects.new("Aim", None)
    aim.location = (0.0, 0.0, 0.04)
    coll.objects.link(aim)
    cc.black_world(scene, (0.0, 0.0, 0.0), 0.0)
    cc.gradient_card(coll, "Backdrop", (0.0, 2.2, 0.15), (5.0, 3.0), (0.015, 0.016, 0.018), 0.35)
    cc.area(coll, "Key", (0.22, -0.85, 0.72), 0.85, 11, (1.0, 0.985, 0.96), aim)
    cc.area(coll, "Fill", (-0.35, -0.9, 0.28), 1.4, 2.2, (0.82, 0.86, 0.92), aim)
    cc.area(coll, "Under", (0.0, -0.45, -0.35), 1.2, 3.5, (0.85, 0.88, 0.95), aim)
    cc.area(coll, "Rim", (0.05, 0.9, 0.42), 0.7, 9, (0.72, 0.82, 1.0), aim)
    cc.area(coll, "Edge", (-0.9, -0.2, 0.16), 0.035, 14, (1.0, 0.97, 0.92), aim, "RECTANGLE", 1.5)
    sweep = cc.area(coll, "Sweep", (-1.1, -0.35, 0.42), 0.025, 0.0, (1.0, 1.0, 1.0), aim, "RECTANGLE", 1.8)
    for frame, x, energy in (
        (1, -1.05, 0.0), (8, -0.9, 55.0), (36, 0.9, 55.0), (44, 1.05, 0.0),
        (PCB_OPEN[0], -1.05, 0.0), (PCB_OPEN[0] + 8, -0.85, 36.0), (BOOT + 16, 0.85, 36.0), (BOOT + 28, 1.05, 0.0),
        (ASSEMBLE[0] + 8, -1.05, 0.0), (ASSEMBLE[0] + 36, -0.8, 22.0), (ASSEMBLE[1] - 24, 0.9, 22.0), (ASSEMBLE[1], 1.05, 0.0),
    ):
        sweep.location.x = x
        sweep.keyframe_insert("location", index=0, frame=frame)
        sweep.data.energy = energy
        sweep.data.keyframe_insert("energy", frame=frame)

    cc.add_noise(cam, "location", 0.00025, 60.0, 3.0)
    scene.frame_start = 1
    scene.frame_end = FRAMES
    cc.render_settings(scene, res, samples=samples, exposure=float(cc.arg_value("--exposure", "-1.05")))
    cc.compositor(scene, bloom=0.25, threshold=2.0, dispersion=0.006, vignette=0.28)
    cc.RENDER_TMP.mkdir(parents=True, exist_ok=True)
    cc.VIDEO_DIR.mkdir(parents=True, exist_ok=True)
    cc.video_output(scene, RAW)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    print("saved", BLEND)

    if test:
        for frame in (int(v) for v in test.split(",")):
            scene.frame_set(frame)
            cc.still_output(scene, cc.RENDER_TMP / f"rozklad_{frame:04d}.jpg")
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", frame)
        return
    bpy.ops.render.render(animation=True, scene=scene.name)
    print("raw", RAW)
    render_edit(res)


if __name__ == "__main__":
    main()
