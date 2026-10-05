"""Two CzechMate scenes and their videos.

  blender --background --factory-startup --python build_videos.py -- --test
  blender --background --factory-startup --python build_videos.py

Rozpojeni is the short exploded board. Prezentace plays the real LED language
from the firmware: yellow lifted piece, green empty destination, orange capture,
red illegal square, gold/silver castling, pink check, red/white mate, endgame wave.
"""

import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_scene as board

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "video"
BLEND = ROOT / "czechmate_video.blend"
FPS = 24

YELLOW = (1.0, 1.0, 0.0)
GREEN = (0.0, 1.0, 0.0)
ORANGE = (1.0, 165 / 255, 0.0)
RED = (1.0, 0.0, 0.0)
PINK = (1.0, 192 / 255, 203 / 255)
GOLD = (1.0, 215 / 255, 0.0)
SILVER = (0.75, 0.75, 0.75)
WHITE = (1.0, 1.0, 1.0)
LED_STRENGTH = 8.0

ARGV = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
TEST = "--test" in ARGV


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def lerp(a, b, t):
    return a + (b - a) * t


def axis_sample(index, values):
    i0 = max(0, min(7, int(math.floor(index))))
    i1 = max(0, min(7, i0 + 1))
    t = min(1.0, max(0.0, index - i0))
    return values[i0] * (1.0 - t) + values[i1] * t


def activate(scene):
    for window in bpy.context.window_manager.windows:
        window.scene = scene
    bpy.context.view_layer.update()


def fcurves_of(action):
    if action is None:
        return
    yielded = set()
    legacy = getattr(action, "fcurves", None)
    if legacy:
        for curve in legacy:
            yielded.add(id(curve))
            yield curve
    for layer in getattr(action, "layers", []) or []:
        for strip in getattr(layer, "strips", []) or []:
            bags = []
            direct = getattr(strip, "channelbag", None)
            if direct is not None and not callable(direct):
                bags.append(direct)
            channelbags = getattr(strip, "channelbags", None)
            if channelbags:
                bags.extend(channelbags)
            getter = getattr(strip, "channelbag", None)
            if callable(getter):
                for slot in getattr(action, "slots", []) or []:
                    bag = getter(slot)
                    if bag is not None:
                        bags.append(bag)
            for bag in bags:
                for curve in getattr(bag, "fcurves", []) or []:
                    if id(curve) in yielded:
                        continue
                    yielded.add(id(curve))
                    yield curve


def force_interpolation(action, mode):
    count = 0
    for curve in fcurves_of(action):
        for key in curve.keyframe_points:
            key.interpolation = mode
        count += 1
    return count


def emission_material(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emit = nodes.new("ShaderNodeEmission")
    emit.name = "LED"
    emit.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    emit.inputs["Strength"].default_value = 0.0
    links.new(emit.outputs["Emission"], output.inputs["Surface"])
    return mat


def led_plane_mesh():
    mesh = bpy.data.meshes.new("LED plane")
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=0.016)
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(emission_material("LED unused"))
    return mesh


def led_ring_mesh():
    before = set(bpy.data.objects)
    bpy.ops.mesh.primitive_torus_add(
        major_radius=0.013,
        minor_radius=0.0015,
        major_segments=24,
        minor_segments=6,
    )
    created = [obj for obj in bpy.data.objects if obj not in before]
    mesh = created[0].data
    bpy.data.objects.remove(created[0])
    mesh.materials.append(emission_material("LED ring unused"))
    return mesh


def add_led(mesh, ring_mesh, collection, name, x, y, z, ring_z):
    obj = bpy.data.objects.new(name, mesh)
    obj.location = (x, y, z)
    mat = emission_material(name)
    obj.material_slots[0].link = "OBJECT"
    obj.material_slots[0].material = mat
    obj.hide_render = True
    obj.hide_viewport = True
    obj.visible_shadow = False
    collection.objects.link(obj)
    ring = bpy.data.objects.new(name + " ring", ring_mesh)
    ring.parent = obj
    ring.matrix_parent_inverse = Matrix.Identity(4)
    ring.location = (0.0, 0.0, ring_z - z)
    ring.material_slots[0].link = "OBJECT"
    ring.material_slots[0].material = mat
    ring.hide_render = True
    ring.hide_viewport = True
    ring.visible_shadow = False
    collection.objects.link(ring)
    return obj, mat


def key_led(obj, mat, color, strength, frame):
    node = mat.node_tree.nodes["LED"]
    hidden = color is None
    if hidden:
        node.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        node.inputs["Strength"].default_value = 0.0
    else:
        node.inputs["Color"].default_value = (*color, 1.0)
        node.inputs["Strength"].default_value = strength
    node.inputs["Color"].keyframe_insert("default_value", frame=frame)
    node.inputs["Strength"].keyframe_insert("default_value", frame=frame)
    for target in (obj, *obj.children):
        target.hide_render = hidden
        target.hide_viewport = hidden
        target.keyframe_insert("hide_render", frame=frame)
        target.keyframe_insert("hide_viewport", frame=frame)


def apply_leds(leds, frame, colors, strength=LED_STRENGTH):
    for key, (obj, mat) in leds.items():
        key_led(obj, mat, colors.get(key), strength, frame)


def wood(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    coord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    noise = nodes.new("ShaderNodeTexNoise")
    ramp = nodes.new("ShaderNodeValToRGB")
    bump = nodes.new("ShaderNodeBump")
    mapping.inputs["Scale"].default_value = (1.2, 8.0, 1.2)
    noise.inputs["Scale"].default_value = 4.0
    noise.inputs["Detail"].default_value = 6.0
    ramp.color_ramp.elements[0].color = (0.035, 0.02, 0.012, 1.0)
    ramp.color_ramp.elements[1].color = (0.11, 0.055, 0.03, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.42
    bsdf.inputs["Metallic"].default_value = 0.0
    bump.inputs["Strength"].default_value = 0.18
    links.new(coord.outputs["Object"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], bsdf.inputs["Base Color"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    return mat


def flat_mat(name, color, roughness=0.5, metallic=0.0, emission=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if emission and "Emission Strength" in bsdf.inputs:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    return mat


def look(scene, camera, samples=16):
    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    eevee = getattr(scene, "eevee", None)
    if eevee is not None:
        if hasattr(eevee, "use_raytracing"):
            eevee.use_raytracing = False
        if hasattr(eevee, "taa_render_samples"):
            eevee.taa_render_samples = samples
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 900
    scene.render.resolution_percentage = 100
    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    scene.render.use_motion_blur = False
    scene.render.image_settings.media_type = "VIDEO"
    scene.render.image_settings.file_format = "FFMPEG"
    scene.render.ffmpeg.format = "MPEG4"
    scene.render.ffmpeg.codec = "H264"
    scene.render.ffmpeg.constant_rate_factor = "MEDIUM"
    scene.render.ffmpeg.ffmpeg_preset = "GOOD"
    scene.render.ffmpeg.audio_codec = "NONE"
    scene.camera = camera
    try:
        scene.view_settings.view_transform = "AgX"
    except TypeError:
        pass
    try:
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass


def track_to(obj, target):
    constraint = obj.constraints.new(type="TRACK_TO")
    constraint.target = target
    constraint.track_axis = "TRACK_NEGATIVE_Z"
    constraint.up_axis = "UP_Y"
    return constraint


def area_light(collection, name, location, size, energy, color, target):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.size = size
    data.color = color
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    track_to(obj, target)
    collection.objects.link(obj)
    return obj


def camera(collection, name, location, lens, target, fstop):
    data = bpy.data.cameras.new(name)
    data.lens = lens
    data.dof.use_dof = True
    data.dof.focus_object = target
    data.dof.aperture_fstop = fstop
    data.clip_start = 0.02
    obj = bpy.data.objects.new(name, data)
    obj.location = location
    track_to(obj, target)
    collection.objects.link(obj)
    return obj


def key_location(obj, frame, location):
    obj.location = location
    obj.keyframe_insert("location", frame=frame)


def clone_objects(objects, collection, prefix):
    clones = {}
    for obj in objects:
        dup = obj.copy()
        dup.data = obj.data
        dup.name = prefix + obj.name
        if dup.animation_data:
            dup.animation_data_clear()
        collection.objects.link(dup)
        clones[obj.name] = dup
    return clones


def mesh_object(collection, name, primitive, location, scale, material, rotation=(0.0, 0.0, 0.0)):
    before = set(bpy.data.objects)
    primitive()
    created = [obj for obj in bpy.data.objects if obj not in before]
    obj = created[0]
    obj.name = name
    obj.location = location
    obj.scale = scale
    obj.rotation_euler = rotation
    if obj.data.materials:
        obj.data.materials[0] = material
    else:
        obj.data.materials.append(material)
    for coll in list(obj.users_collection):
        coll.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def build_studio(collection):
    table = mesh_object(
        collection,
        "Table",
        lambda: bpy.ops.mesh.primitive_plane_add(size=2.4),
        (0.0, -0.05, -0.001),
        (1.0, 1.0, 1.0),
        wood("Walnut"),
    )
    wall_mat = flat_mat("Wall plaster", (0.015, 0.016, 0.018), 0.85)
    mesh_object(
        collection,
        "Wall",
        lambda: bpy.ops.mesh.primitive_plane_add(size=3.2),
        (0.0, 1.15, 1.15),
        (1.0, 1.0, 1.0),
        wall_mat,
        rotation=(math.pi / 2, 0.0, 0.0),
    )
    window_mat = flat_mat("Window", (1.0, 0.78, 0.55), 0.2, emission=8.0)
    mesh_object(
        collection,
        "Window",
        lambda: bpy.ops.mesh.primitive_plane_add(size=1.0),
        (-0.55, 1.14, 1.05),
        (0.72, 0.42, 1.0),
        window_mat,
        rotation=(math.pi / 2, 0.0, 0.0),
    )
    shade_mat = flat_mat("Lamp shade", (0.02, 0.018, 0.015), 0.55)
    mesh_object(
        collection,
        "Lamp shade",
        lambda: bpy.ops.mesh.primitive_cylinder_add(vertices=24),
        (-0.72, -0.15, 0.42),
        (0.09, 0.09, 0.07),
        shade_mat,
    )
    bulb_mat = flat_mat("Bulb", (1.0, 0.85, 0.65), 0.2, emission=12.0)
    mesh_object(
        collection,
        "Bulb",
        lambda: bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8),
        (-0.72, -0.15, 0.40),
        (0.025, 0.025, 0.025),
        bulb_mat,
    )
    book_colors = ((0.08, 0.12, 0.16), (0.22, 0.08, 0.06), (0.12, 0.10, 0.07))
    for index, color in enumerate(book_colors):
        mesh_object(
            collection,
            f"Book {index}",
            lambda: bpy.ops.mesh.primitive_cube_add(),
            (0.48, 0.32, 0.012 + index * 0.022),
            (0.09, 0.13, 0.01),
            flat_mat(f"Book cloth {index}", color, 0.7),
            rotation=(0.0, 0.0, -0.08 + index * 0.05),
        )
    cup_mat = flat_mat("Cup ceramic", (0.85, 0.84, 0.80), 0.28)
    mesh_object(
        collection,
        "Cup",
        lambda: bpy.ops.mesh.primitive_cylinder_add(vertices=24),
        (-0.48, 0.28, 0.028),
        (0.032, 0.032, 0.03),
        cup_mat,
    )
    return table


def part_named(objects, token):
    matches = [obj for obj in objects if token.lower() in obj.name.lower() and obj.type == "MESH"]
    if not matches:
        raise RuntimeError(token)
    return max(matches, key=lambda obj: len(obj.data.vertices))


def pose(home, segments, frame):
    """File, rank, lift, and whether the piece has left the board."""
    file_i, rank_i = float(home[0]), float(home[1])
    for seg in segments:
        if "world" in seg:
            if frame >= seg["f0"]:
                return -1.0, -1.0, 0.0, True
            continue
        if frame < seg["f0"]:
            return file_i, rank_i, 0.0, False
        span = max(1, seg["f1"] - seg["f0"])
        raw = min(1.0, (frame - seg["f0"]) / span)
        dest_f, dest_r = seg["to"]
        cur_f = lerp(file_i, dest_f, smooth(raw))
        cur_r = lerp(rank_i, dest_r, smooth(raw))
        lift = math.sin(raw * math.pi) * seg["lift"]
        if raw >= 1.0:
            file_i, rank_i = float(dest_f), float(dest_r)
        else:
            return cur_f, cur_r, lift, False
    return file_i, rank_i, 0.0, False


def presentation_moves():
    # Frames are 1-based and line up with the captions in captions_at().
    return {
        "w_pawn_4": [
            {"f0": 54, "f1": 126, "to": (4, 3), "lift": 0.035},
            {"f0": 246, "f1": 330, "to": (3, 4), "lift": 0.04},
        ],
        "b_pawn_3": [
            {"f0": 150, "f1": 222, "to": (3, 4), "lift": 0.035},
            {"f0": 300, "f1": 330, "world": (0.23, -0.08, 0.0), "lift": 0.05},
        ],
        "w_bishop_5": [{"f0": 330, "f1": 354, "to": (2, 3), "lift": 0.03}],
        "w_knight_6": [{"f0": 330, "f1": 354, "to": (5, 2), "lift": 0.03}],
        "w_king_4": [{"f0": 414, "f1": 468, "to": (6, 0), "lift": 0.018}],
        "w_rook_7": [{"f0": 414, "f1": 468, "to": (5, 0), "lift": 0.046}],
        "b_king_4": [{"f0": 468, "f1": 504, "to": (4, 7), "lift": 0.016}],
    }


def blink(frame, start, cycles, on_frames, off_frames):
    span = on_frames + off_frames
    local = frame - start
    if local < 0 or local >= cycles * span:
        return None
    return (local % span) < on_frames


def wave_colors(frame, occupied):
    start = 560
    if frame < start:
        return {}
    radius = 1.0 + ((frame - start) % 48) / 6.0
    colors = {(6, 0): GOLD}
    for file_i in range(8):
        for rank_i in range(8):
            if (file_i, rank_i) == (6, 0):
                continue
            dist = math.hypot(file_i - 6, rank_i - 0)
            if abs(dist - radius) > 1.15:
                continue
            side = occupied.get((file_i, rank_i))
            if side == "w":
                colors[(file_i, rank_i)] = (0.12, 1.0, 0.28)
            elif side == "b":
                colors[(file_i, rank_i)] = (1.0, 0.12, 0.12)
            else:
                colors[(file_i, rank_i)] = (0.12, 0.39, 1.0)
    return colors


def leds_at(frame, occupied):
    if frame < 18:
        return {}
    if frame < 54:
        colors = {}
        for file_i in range(8):
            colors[(file_i, 1)] = YELLOW
        colors[(1, 0)] = YELLOW
        colors[(6, 0)] = YELLOW
        return colors
    if frame < 126:
        return {(4, 1): YELLOW, (4, 2): GREEN, (4, 3): GREEN}
    if frame < 150:
        on = blink(frame, 126, 3, 5, 5)
        if on:
            return {(file_i, rank_i): WHITE for file_i in range(8) for rank_i in range(8)}
        if on is None:
            colors = {}
            for file_i in range(8):
                colors[(file_i, 6)] = YELLOW
            colors[(1, 7)] = YELLOW
            colors[(6, 7)] = YELLOW
            return colors
        return {}
    if frame < 222:
        return {(3, 6): YELLOW, (3, 5): GREEN, (3, 4): GREEN}
    if frame < 246:
        on = blink(frame, 222, 2, 5, 5)
        if on:
            return {(file_i, rank_i): WHITE for file_i in range(8) for rank_i in range(8)}
        return {(4, 3): YELLOW}
    if frame < 330:
        return {(4, 3): YELLOW, (4, 4): GREEN, (3, 4): ORANGE}
    if frame < 354:
        on = blink(frame, 330, 2, 6, 6)
        if on:
            return {(file_i, rank_i): WHITE for file_i in range(8) for rank_i in range(8)}
        return {}
    if frame < 384:
        on = blink(frame, 354, 3, 5, 5)
        if on:
            return {(0, 0): RED}
        return {}
    if frame < 414:
        return {(4, 0): GOLD, (7, 0): GOLD, (6, 0): GREEN, (5, 0): GREEN}
    if frame < 468:
        colors = {}
        king_f = lerp(4, 6, smooth((frame - 414) / 54))
        rook_f = lerp(7, 5, smooth((frame - 414) / 54))
        for file_i in range(8):
            king_fall = max(0.0, 1.0 - abs(file_i - king_f))
            rook_fall = max(0.0, 1.0 - abs(file_i - rook_f))
            if king_fall > rook_fall and king_fall > 0.15:
                colors[(file_i, 0)] = tuple(channel * king_fall for channel in GOLD)
            elif rook_fall > 0.15:
                colors[(file_i, 0)] = tuple(channel * rook_fall for channel in SILVER)
        colors[(6, 0)] = GREEN
        colors[(5, 0)] = GREEN
        return colors
    if frame < 504:
        return {(4, 7): PINK}
    if frame < 560:
        on = blink(frame, 504, 7, 4, 4)
        if not on:
            return {}
        color = RED if ((frame - 504) // 8) % 2 == 0 else WHITE
        return {(file_i, rank_i): color for file_i in range(8) for rank_i in range(8)}
    return wave_colors(frame, occupied)


def occupied_at(homes, moves, frame):
    occupied = {}
    for name, (file_i, rank_i, side) in homes.items():
        segs = moves.get(name, [])
        f_i, r_i, _lift, gone = pose((file_i, rank_i), segs, frame)
        if gone or f_i < 0:
            continue
        occupied[(int(round(f_i)), int(round(r_i)))] = side
    return occupied


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    exploded = bpy.context.scene
    exploded.name = "Rozpojeni"
    presentation = bpy.data.scenes.new("Prezentace")
    OUT.mkdir(parents=True, exist_ok=True)

    for name in board.BOARD_PARTS:
        board.import_board_part(board.BOARD_DIR / name)
    imported = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    grid = part_named(imported, "Mrizka")
    glass_pane = part_named(imported, "Sklo")
    foil = part_named(imported, "Folie")
    button = part_named(imported, "Tlacitko")
    chassis = part_named(imported, "Chasie")
    lid = part_named(imported, "Dekl")

    bpy.context.view_layer.update()
    xs = board.square_centers(board.world_coords(grid, 0))
    ys = board.square_centers(board.world_coords(grid, 1))
    if len(xs) != 8 or len(ys) != 8:
        raise RuntimeError(f"expected 8x8, got {len(xs)} x {len(ys)}")
    surface = max(board.world_coords(glass_pane, 2))
    print("files", [round(v, 4) for v in xs])
    print("ranks", [round(v, 4) for v in ys])
    print("surface", round(surface, 4))

    dark = board.flat("Square dark", (0.025, 0.026, 0.028), 0.62)
    light = board.flat("Square light", (0.90, 0.88, 0.82), 0.48)
    grid_mat = board.flat("Grid plastic", (0.55, 0.55, 0.53), 0.62)
    board.paint_squares(grid, xs, ys, dark, light, grid_mat)
    glass_mat = board.glass("Cover glass", (0.55, 0.62, 0.66), 0.03, 0.18)
    foil_mat = board.diffuser("Diffuser film")
    for node in foil_mat.node_tree.nodes:
        if node.type == "MIX_SHADER":
            node.inputs["Fac"].default_value = 0.1
    button_mat = board.flat("Button plastic", (0.012, 0.012, 0.013), 0.4)
    lid_mat = board.flat("Inner lid", (0.09, 0.09, 0.095), 0.58)
    chassis_mat = board.aluminum("Chassis aluminium")
    for obj, mat in (
        (glass_pane, glass_mat),
        (foil, foil_mat),
        (button, button_mat),
        (lid, lid_mat),
        (chassis, chassis_mat),
    ):
        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)

    white = board.printed("PLA white", (0.82, 0.81, 0.77))
    black = board.printed("PLA black", (0.004, 0.004, 0.005), 0.82, 0.08)
    templates = {}
    for kind in ("pawn", "rook", "bishop", "queen", "king", "knight"):
        path = board.KNIGHT if kind == "knight" else board.PIECE_DIR / f"{kind}.stl"
        templates[kind] = board.load_piece(path, 0.001, white)
        templates[kind].hide_render = True
        templates[kind].hide_viewport = True

    piece_coll = bpy.data.collections.new("Pieces")
    exploded.collection.children.link(piece_coll)
    homes = {}

    def place(kind, file_i, rank_i, material, yaw, label, side):
        base = templates[kind]
        obj = bpy.data.objects.new(label, base.data)
        obj.location = (xs[file_i], ys[rank_i], surface)
        obj.rotation_euler = (0.0, 0.0, yaw)
        obj.scale = base.scale
        obj.material_slots[0].link = "OBJECT"
        obj.material_slots[0].material = material
        piece_coll.objects.link(obj)
        homes[label] = (file_i, rank_i, side)
        return obj

    for file_i, kind in enumerate(board.BACK_RANK):
        white_yaw = 0.0
        black_yaw = math.pi if kind == "knight" else 0.0
        place(kind, file_i, 0, white, white_yaw, f"w_{kind}_{file_i}", "w")
        place("pawn", file_i, 1, white, 0.0, f"w_pawn_{file_i}", "w")
        place("pawn", file_i, 6, black, 0.0, f"b_pawn_{file_i}", "b")
        place(kind, file_i, 7, black, black_yaw, f"b_{kind}_{file_i}", "b")

    board_objects = [chassis, lid, grid, foil, glass_pane, button]
    show_board = bpy.data.collections.new("Board show")
    show_pieces = bpy.data.collections.new("Pieces show")
    presentation.collection.children.link(show_board)
    presentation.collection.children.link(show_pieces)
    board_copy = clone_objects(board_objects, show_board, "p_")
    piece_copy = clone_objects(list(piece_coll.objects), show_pieces, "p_")

    led_mesh = led_plane_mesh()
    ring_mesh = led_ring_mesh()
    led_z = 0.0206
    exp_leds_coll = bpy.data.collections.new("LEDs")
    pre_leds_coll = bpy.data.collections.new("LEDs show")
    exploded.collection.children.link(exp_leds_coll)
    presentation.collection.children.link(pre_leds_coll)
    exploded_leds = {}
    presentation_leds = {}
    for file_i, x in enumerate(xs):
        for rank_i, y in enumerate(ys):
            obj, mat = add_led(
                led_mesh, ring_mesh, exp_leds_coll, f"led_{file_i}_{rank_i}", x, y, led_z, surface + 0.0015
            )
            exploded_leds[(file_i, rank_i)] = (obj, mat)
            clone, clone_mat = add_led(
                led_mesh,
                ring_mesh,
                pre_leds_coll,
                f"p_led_{file_i}_{rank_i}",
                x,
                y,
                led_z,
                surface + 0.0015,
            )
            presentation_leds[(file_i, rank_i)] = (clone, clone_mat)

    studio = bpy.data.collections.new("Studio")
    exploded.collection.children.link(studio)
    presentation.collection.children.link(studio)
    build_studio(studio)

    aim = bpy.data.objects.new("Aim", None)
    aim.location = (0.0, 0.0, surface + 0.01)
    studio.objects.link(aim)
    aim_stack = bpy.data.objects.new("Aim stack", None)
    aim_stack.location = (0.0, 0.0, 0.11)
    exploded.collection.objects.link(aim_stack)
    exp_cam = camera(exploded.collection, "Camera exploded", (0.68, -0.62, 0.40), 38, aim_stack, 11.0)
    pre_cam = camera(presentation.collection, "Camera presentation", (0.40, -0.38, 0.30), 50, aim, 8.0)

    world_dark = bpy.data.worlds.new("Night")
    world_dark.use_nodes = True
    background = world_dark.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.01, 0.012, 0.016, 1.0)
    background.inputs["Strength"].default_value = 0.22
    exploded.world = world_dark
    presentation.world = world_dark

    for scene, prefix in ((exploded, "e"), (presentation, "p")):
        coll = scene.collection
        area_light(coll, f"{prefix} Key", (0.40, -0.55, 0.62), 0.5, 28, (1.0, 0.96, 0.90), aim)
        area_light(coll, f"{prefix} Fill", (-0.55, -0.25, 0.32), 0.7, 5, (0.65, 0.74, 0.85), aim)
        area_light(coll, f"{prefix} Rim", (0.05, 0.62, 0.48), 0.35, 16, (1.0, 0.95, 0.88), aim)
        area_light(coll, f"{prefix} Window", (-0.55, 0.7, 0.85), 0.45, 40, (1.0, 0.78, 0.55), aim)
        area_light(coll, f"{prefix} Lamp", (-0.72, -0.15, 0.36), 0.12, 18, (1.0, 0.82, 0.6), aim)

    font_path = Path("C:/Windows/Fonts/segoeui.ttf")
    font = bpy.data.fonts.load(str(font_path)) if font_path.exists() else None
    caption_mat = flat_mat("Caption ink", (1.0, 0.98, 0.94), 0.3, emission=2.4)
    caption_beats = (
        (1, "CzechMate"),
        (18, "Žlutá — může táhnout"),
        (54, "Zvednuto žlutě, cíl zeleně"),
        (126, "Střídání hráče"),
        (150, "Tah černého"),
        (246, "Braní je oranžové"),
        (330, "Cesta k rošádě"),
        (354, "Chyba je červená"),
        (384, "Rošáda"),
        (468, "Šach — růžová"),
        (504, "Mat"),
        (560, "Konec hry"),
    )
    captions = []
    for index, (start, text) in enumerate(caption_beats):
        curve = bpy.data.curves.new(f"Caption {index}", "FONT")
        curve.body = text
        curve.align_x = "CENTER"
        curve.align_y = "CENTER"
        if font is not None:
            curve.font = font
        curve.materials.append(caption_mat)
        caption = bpy.data.objects.new(f"Caption {index}", curve)
        caption.parent = pre_cam
        caption.matrix_parent_inverse = Matrix.Identity(4)
        caption.location = (0.0, 0.058, -0.5)
        caption.rotation_euler = (0.0, 0.0, 0.0)
        caption.scale = (0.0085, 0.0085, 0.0085)
        presentation.collection.objects.link(caption)
        captions.append((start, caption))
    for index, (start, caption) in enumerate(captions):
        nxt = captions[index + 1][0] if index + 1 < len(captions) else 621
        caption.hide_render = False
        caption.hide_viewport = False
        caption.keyframe_insert("hide_render", frame=start)
        caption.keyframe_insert("hide_viewport", frame=start)
        caption.hide_render = True
        caption.hide_viewport = True
        caption.keyframe_insert("hide_render", frame=nxt)
        caption.keyframe_insert("hide_viewport", frame=nxt)
        if start > 1:
            caption.keyframe_insert("hide_render", frame=1)
            caption.keyframe_insert("hide_viewport", frame=1)
    bar_mat = flat_mat("Caption bar", (0.0, 0.0, 0.0), 0.8)
    bar_mat.blend_method = "BLEND"
    if hasattr(bar_mat, "surface_render_method"):
        bar_mat.surface_render_method = "BLENDED"
    bar_mat.use_nodes = True
    bar_bsdf = bar_mat.node_tree.nodes.get("Principled BSDF")
    bar_bsdf.inputs["Alpha"].default_value = 0.72
    bar = mesh_object(
        presentation.collection,
        "Caption bar",
        lambda: bpy.ops.mesh.primitive_plane_add(size=1.0),
        (0.0, 0.0, 0.0),
        (1.0, 1.0, 1.0),
        bar_mat,
    )
    bar.parent = pre_cam
    bar.matrix_parent_inverse = Matrix.Identity(4)
    bar.location = (0.0, 0.058, -0.51)
    bar.rotation_euler = (0.0, 0.0, 0.0)
    bar.scale = (0.18, 0.016, 1.0)

    # Exploded offsets, metres. Pieces and glass travel together.
    offsets = {
        chassis: 0.0,
        button: 0.0,
        lid: 0.042,
        grid: 0.086,
        foil: 0.122,
        glass_pane: 0.162,
    }
    piece_bases = {obj: Vector(obj.location) for obj in piece_coll.objects}
    part_bases = {obj: Vector(obj.location) for obj in board_objects}
    led_bases = {pair[0]: Vector(pair[0].location) for pair in exploded_leds.values()}

    def exploded_shift(frame):
        if frame < 36:
            return 0.0
        if frame < 96:
            return smooth((frame - 36) / 60)
        if frame < 144:
            return 1.0
        return 1.0 - smooth((frame - 144) / 48)

    for frame in range(1, 193, 4):
        shift = exploded_shift(frame)
        for obj, base in part_bases.items():
            key_location(obj, frame, base + Vector((0.0, 0.0, offsets[obj] * shift)))
        for obj, base in piece_bases.items():
            key_location(obj, frame, base + Vector((0.0, 0.0, offsets[glass_pane] * shift)))
        for obj, base in led_bases.items():
            key_location(obj, frame, base + Vector((0.0, 0.0, offsets[grid] * shift)))
        glow = {}
        if shift > 0.85:
            for key in exploded_leds:
                if key[0] == 4 and key[1] % 2 == 0:
                    glow[key] = (1.0, 0.82, 0.45)
        apply_leds(exploded_leds, frame, glow, strength=18.0)
    key_location(exp_cam, 1, (0.68, -0.62, 0.40))
    key_location(exp_cam, 96, (0.22, -0.78, 0.38))
    key_location(exp_cam, 192, (0.60, -0.55, 0.34))

    moves = presentation_moves()
    show_bases = {name: Vector(obj.location) for name, obj in piece_copy.items()}
    previous_colors = None
    for frame in range(1, 621):
        occupied = occupied_at(homes, moves, frame)
        colors = leds_at(frame, occupied)
        if colors != previous_colors:
            apply_leds(presentation_leds, frame, colors)
            previous_colors = colors
        if frame % 2 == 1 and frame >= 54:
            for name, obj in piece_copy.items():
                home = homes[name]
                segs = moves.get(name, [])
                f_i, r_i, lift, gone = pose((home[0], home[1]), segs, frame)
                if gone:
                    continue
                if not segs or frame < segs[0]["f0"]:
                    continue
                loc = Vector((axis_sample(f_i, xs), axis_sample(r_i, ys), surface + lift))
                key_location(obj, frame, loc)
        elif frame == 1:
            for obj in piece_copy.values():
                obj.keyframe_insert("location", frame=1)

    # The capture exit uses absolute world coordinates. Re-bake that one piece
    # explicitly so the branch above cannot leave it on a square.
    captured = piece_copy["b_pawn_3"]
    for frame in range(300, 331, 2):
        t = smooth((frame - 300) / 30)
        lift = math.sin(t * math.pi) * 0.05
        start = Vector((xs[3], ys[4], surface))
        end = Vector((0.23, -0.08, 0.0))
        key_location(captured, frame, start.lerp(end, t) + Vector((0.0, 0.0, lift * (1.0 - t))))

    key_location(pre_cam, 1, (0.46, -0.42, 0.32))
    key_location(pre_cam, 300, (0.34, -0.34, 0.24))
    key_location(pre_cam, 620, (0.30, -0.30, 0.20))

    keyed = 0
    for mat in bpy.data.materials:
        action = mat.node_tree.animation_data.action if mat.node_tree.animation_data else None
        keyed += force_interpolation(action, "CONSTANT")
    for obj, _mat in list(exploded_leds.values()) + list(presentation_leds.values()):
        for target in (obj, *obj.children):
            action = target.animation_data.action if target.animation_data else None
            force_interpolation(action, "CONSTANT")
    for _start, caption in captions:
        action = caption.animation_data.action if caption.animation_data else None
        force_interpolation(action, "CONSTANT")
    print("constant fcurves", keyed)

    exploded.frame_start = 1
    exploded.frame_end = 192
    presentation.frame_start = 1
    presentation.frame_end = 620
    look(exploded, exp_cam)
    look(presentation, pre_cam)
    exploded.render.filepath = str(OUT / "rozpojeni.mp4")
    presentation.render.filepath = str(OUT / "prezentace.mp4")

    activate(presentation)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    print("saved", BLEND)

    if TEST:
        activate(exploded)
        exploded.frame_set(110)
        exploded.render.image_settings.media_type = "IMAGE"
        exploded.render.image_settings.file_format = "PNG"
        exploded.render.filepath = str(OUT / "test_exploded.png")
        bpy.ops.render.render(write_still=True)
        activate(presentation)
        presentation.render.image_settings.media_type = "IMAGE"
        presentation.render.image_settings.file_format = "PNG"
        for frame, filename in ((100, "test_move.png"), (310, "test_capture.png"), (480, "test_check.png")):
            presentation.frame_set(frame)
            presentation.render.filepath = str(OUT / filename)
            bpy.ops.render.render(write_still=True)
            print("still", filename)
        return

    activate(exploded)
    bpy.ops.render.render(animation=True)
    print("rendered", exploded.render.filepath)
    activate(presentation)
    bpy.ops.render.render(animation=True)
    print("rendered", presentation.render.filepath)


if __name__ == "__main__":
    main()
