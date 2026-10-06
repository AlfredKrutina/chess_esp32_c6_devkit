"""Shared builder for the CzechMate film scenes (Blender 5, EEVEE).

Board parts come from build_scene (czechmate2 GLBs). The electronics layer
(PCB, 64 WS2812B, 128 Hall sensors, ESP32-C6, 4x STM32C031) is modelled here
from the V2 docs: two Hall sensors 6.267 mm apart per square, four STM32
segments of 16 squares each.
"""

import math
import os
import random
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
import build_scene as board  # noqa: E402
import led_effects as fx  # noqa: E402

VIDEO_DIR = ROOT / "video"
RENDER_TMP = Path(os.environ.get("TEMP", str(ROOT))) / "czechmate_render"
FONT_DIR = Path("C:/Windows/Fonts")
FPS = 30
BEAT = 30  # authored frame counts; F() keeps wall-clock time if FPS changes


def F(n):
    return int(round(n * FPS / BEAT))

PCB_Z = 0.0114
PCB_T = 0.0016
LED_GLOW_Z = 0.02115  # on the foil top, under the glass
LED_LIGHT_Z = 0.0199  # top of the grid, just under the foil
CELL = 0.033
HALL_PITCH = 0.006267
# Placement only. Polarity is the piece colour, not the type: every magnet in a
# black piece has south toward the board, every magnet in a white piece north.
MAGNET_POS = {
    "pawn": ((0.0, 0.0),),
    "rook": ((0.0, 0.0),),
    "knight": ((3.6, 0.0),),
    "bishop": ((3.6, 0.0), (-3.6, 0.0)),
    "queen": ((5.2, 0.0), (-5.2, 0.0), (0.0, 5.2), (0.0, -5.2)),
    "king": ((5.2, 0.0), (-5.2, 0.0), (0.0, 5.2), (0.0, -5.2)),
}


def args():
    return sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []


def arg_value(flag, default=None):
    a = args()
    if flag in a:
        i = a.index(flag)
        if i + 1 < len(a):
            return a[i + 1]
    return default


def smoothstep(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


# ----------------------------------------------------------------- animation


def fcurves_of(action, slot=None):
    if action is None:
        return
    seen = set()
    legacy = getattr(action, "fcurves", None)
    if legacy:
        for curve in legacy:
            seen.add(id(curve))
            yield curve
    for layer in getattr(action, "layers", []) or []:
        for strip in getattr(layer, "strips", []) or []:
            bags = list(getattr(strip, "channelbags", []) or [])
            getter = getattr(strip, "channelbag", None)
            if callable(getter):
                slots = [slot] if slot is not None else list(getattr(action, "slots", []) or [])
                for sl in slots:
                    try:
                        bag = getter(sl)
                    except TypeError:
                        bag = None
                    if bag is not None:
                        bags.append(bag)
            for bag in bags:
                for curve in getattr(bag, "fcurves", []) or []:
                    if id(curve) not in seen:
                        seen.add(id(curve))
                        yield curve


def action_of(idblock):
    data = getattr(idblock, "animation_data", None)
    return data.action if data else None


def set_interpolation(idblock, mode, path_prefix=None):
    ad = getattr(idblock, "animation_data", None)
    action = ad.action if ad else None
    slot = getattr(ad, "action_slot", None) if ad else None
    for curve in fcurves_of(action, slot):
        if path_prefix and not curve.data_path.startswith(path_prefix):
            continue
        for key in curve.keyframe_points:
            key.interpolation = mode


def ease_out(idblock, path_prefix=None, mode="EXPO"):
    for curve in fcurves_of(action_of(idblock)):
        if path_prefix and not curve.data_path.startswith(path_prefix):
            continue
        for key in curve.keyframe_points:
            key.interpolation = mode
            key.easing = "EASE_OUT"


def add_noise(idblock, path, strength, scale, seed):
    for curve in fcurves_of(action_of(idblock)):
        if curve.data_path == path:
            mod = curve.modifiers.new("NOISE")
            mod.strength = strength
            mod.scale = scale
            mod.phase = seed + curve.array_index * 13.7


def key_loc(obj, frame, loc):
    obj.location = loc
    obj.keyframe_insert("location", frame=frame)


# ----------------------------------------------------------------- materials


def nodes_mat(name):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.node_tree.nodes.clear()
    return mat, mat.node_tree.nodes, mat.node_tree.links


def principled(name, color, roughness=0.5, metallic=0.0, specular=0.5, coat=0.0):
    mat, nodes, links = nodes_mat(name)
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Specular IOR Level"].default_value = specular
    if coat:
        bsdf.inputs["Coat Weight"].default_value = coat
        bsdf.inputs["Coat Roughness"].default_value = 0.08
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def emissive(name, color, strength):
    mat, nodes, links = nodes_mat(name)
    out = nodes.new("ShaderNodeOutputMaterial")
    emit = nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (*color, 1.0)
    emit.inputs["Strength"].default_value = strength
    links.new(emit.outputs["Emission"], out.inputs["Surface"])
    return mat


def fade_emissive(name, color, strength):
    """Emission over transparency. Key the 'Fade' value node 0..1."""
    mat, nodes, links = nodes_mat(name)
    out = nodes.new("ShaderNodeOutputMaterial")
    emit = nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (*color, 1.0)
    emit.inputs["Strength"].default_value = strength
    clear = nodes.new("ShaderNodeBsdfTransparent")
    mix = nodes.new("ShaderNodeMixShader")
    fade = nodes.new("ShaderNodeValue")
    fade.name = "Fade"
    fade.outputs[0].default_value = 0.0
    links.new(fade.outputs[0], mix.inputs["Fac"])
    links.new(clear.outputs["BSDF"], mix.inputs[1])
    links.new(emit.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs["Shader"], out.inputs["Surface"])
    return mat


def key_fade(mat, frame, value):
    node = mat.node_tree.nodes["Fade"]
    node.outputs[0].default_value = value
    node.outputs[0].keyframe_insert("default_value", frame=frame)


def pla(name, color, roughness, specular):
    """FDM plastic from build_scene with a keyable x-ray 'Ghost' mix."""
    mat = board.printed(name, color, roughness, specular)
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    out = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    clear = nodes.new("ShaderNodeBsdfTransparent")
    rim = nodes.new("ShaderNodeEmission")
    rim.inputs["Color"].default_value = (0.75, 0.85, 1.0, 1.0)
    rim.inputs["Strength"].default_value = 1.6
    fresnel = nodes.new("ShaderNodeLayerWeight")
    fresnel.inputs["Blend"].default_value = 0.35
    edge = nodes.new("ShaderNodeMixShader")
    links.new(fresnel.outputs["Facing"], edge.inputs["Fac"])
    links.new(clear.outputs["BSDF"], edge.inputs[1])
    links.new(rim.outputs["Emission"], edge.inputs[2])
    ghost = nodes.new("ShaderNodeValue")
    ghost.name = "Ghost"
    ghost.outputs[0].default_value = 0.0
    mix = nodes.new("ShaderNodeMixShader")
    links.new(ghost.outputs[0], mix.inputs["Fac"])
    links.new(bsdf.outputs["BSDF"], mix.inputs[1])
    links.new(edge.outputs["Shader"], mix.inputs[2])
    for link in list(links):
        if link.to_node == out:
            links.remove(link)
    links.new(mix.outputs["Shader"], out.inputs["Surface"])
    return mat


def key_ghost(mat, frame, value):
    node = mat.node_tree.nodes["Ghost"]
    node.outputs[0].default_value = value
    node.outputs[0].keyframe_insert("default_value", frame=frame)


def smudged_glass():
    mat = board.glass("Cover glass", (0.80, 0.82, 0.84), 0.28, 0.85)
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = next(n for n in nodes if n.type == "BSDF_PRINCIPLED")
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 60.0
    noise.inputs["Detail"].default_value = 4.0
    ramp = nodes.new("ShaderNodeMapRange")
    ramp.inputs["From Min"].default_value = 0.45
    ramp.inputs["From Max"].default_value = 0.75
    ramp.inputs["To Min"].default_value = 0.22
    ramp.inputs["To Max"].default_value = 0.55
    links.new(noise.outputs["Fac"], ramp.inputs["Value"])
    links.new(ramp.outputs["Result"], bsdf.inputs["Roughness"])
    etch_squares(mat)
    return mat


ETCH_HALF = 0.142857  # 8 squares of 35.71 mm, a1 corner at (-HALF, -HALF)
ETCH_PITCH = 2 * ETCH_HALF / 8


def etch_squares(mat):
    """Whole pane is milky. The checker is only a heavier sandblast, like etched door glass."""
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    out = next(n for n in nodes if n.type == "OUTPUT_MATERIAL")
    glass = out.inputs["Surface"].links[0].from_socket
    geo = nodes.new("ShaderNodeNewGeometry")
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(geo.outputs["Position"], split.inputs["Vector"])

    def math(op, a, b=None):
        node = nodes.new("ShaderNodeMath")
        node.operation = op
        for i, value in enumerate((a, b)):
            if value is None:
                continue
            if isinstance(value, (int, float)):
                node.inputs[i].default_value = value
            else:
                links.new(value, node.inputs[i])
        return node.outputs["Value"]

    cells = []
    inside = []
    for axis in ("X", "Y"):
        coord = split.outputs[axis]
        cells.append(math("FLOOR", math("DIVIDE", math("ADD", coord, ETCH_HALF), ETCH_PITCH)))
        inside.append(math("LESS_THAN", math("ABSOLUTE", coord), ETCH_HALF))
    parity = math("FLOORED_MODULO", math("ADD", cells[0], cells[1]), 2.0)
    on_board = math("MULTIPLY", inside[0], inside[1])
    light = math("MULTIPLY", parity, on_board)
    normal = nodes.new("ShaderNodeSeparateXYZ")
    links.new(geo.outputs["Normal"], normal.inputs["Vector"])
    top = math("GREATER_THAN", normal.outputs["Z"], 0.5)

    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 320.0
    grain.inputs["Detail"].default_value = 8.0
    grain.inputs["Roughness"].default_value = 0.65
    speck = nodes.new("ShaderNodeMapRange")
    speck.inputs["From Min"].default_value = 0.35
    speck.inputs["From Max"].default_value = 0.75
    speck.inputs["To Min"].default_value = 0.0
    speck.inputs["To Max"].default_value = 0.045
    links.new(grain.outputs["Fac"], speck.inputs["Value"])

    cloud = nodes.new("ShaderNodeTexNoise")
    cloud.inputs["Scale"].default_value = 52.0
    cloud.inputs["Detail"].default_value = 4.0
    blot = nodes.new("ShaderNodeMapRange")
    blot.inputs["From Min"].default_value = 0.35
    blot.inputs["From Max"].default_value = 0.7
    blot.inputs["To Min"].default_value = -0.02
    blot.inputs["To Max"].default_value = 0.02
    links.new(cloud.outputs["Fac"], blot.inputs["Value"])

    def cell_edge(coord):
        scaled = math("DIVIDE", math("ADD", coord, ETCH_HALF), ETCH_PITCH)
        frac = math("SUBTRACT", scaled, math("FLOOR", scaled))
        return math("MINIMUM", frac, math("SUBTRACT", 1.0, frac))

    edge = math("MINIMUM", cell_edge(split.outputs["X"]), cell_edge(split.outputs["Y"]))
    # About 1.4 mm of extra blast on the square boundary, the drawn part of the mask.
    line = math("MULTIPLY", math("GREATER_THAN", math("SUBTRACT", 0.055, edge), 0.0), on_board)

    # One milk. The checker is a little stronger than the fine ripples in it.
    density = math(
        "ADD",
        math("ADD", math("ADD", 0.40, math("MULTIPLY", light, 0.52)), math("MULTIPLY", line, 0.20)),
        math("ADD", speck.outputs["Result"], blot.outputs["Result"]),
    )
    density.node.use_clamp = True
    density = math("MULTIPLY", density, top)

    milk = nodes.new("ShaderNodeBsdfPrincipled")
    milk.inputs["Base Color"].default_value = (0.86, 0.88, 0.90, 1.0)
    milk.inputs["Roughness"].default_value = 0.78
    milk.inputs["Metallic"].default_value = 0.0
    if "Transmission Weight" in milk.inputs:
        milk.inputs["Transmission Weight"].default_value = 0.16
    relief = math("ADD", grain.outputs["Fac"], math("ADD", math("MULTIPLY", light, 0.7), line))
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.9
    bump.inputs["Distance"].default_value = 0.0005
    links.new(relief, bump.inputs["Height"])
    links.new(bump.outputs["Normal"], milk.inputs["Normal"])

    final = nodes.new("ShaderNodeMixShader")
    links.new(density, final.inputs["Fac"])
    links.new(glass, final.inputs[1])
    links.new(milk.outputs["BSDF"], final.inputs[2])
    links.new(final.outputs["Shader"], out.inputs["Surface"])


def contact_shadow(label, collection, size=0.038, darkness=0.6):
    """Soft occlusion disc under a piece. Key its 'Fade' node 0..1."""
    mat, nodes, links = nodes_mat(f"Contact {label}")
    out = nodes.new("ShaderNodeOutputMaterial")
    coord = nodes.new("ShaderNodeTexCoord")
    split = nodes.new("ShaderNodeSeparateXYZ")
    flat = nodes.new("ShaderNodeCombineXYZ")
    center = nodes.new("ShaderNodeVectorMath")
    center.operation = "SUBTRACT"
    center.inputs[1].default_value = (0.5, 0.5, 0.0)
    radius = nodes.new("ShaderNodeVectorMath")
    radius.operation = "LENGTH"
    falloff = nodes.new("ShaderNodeMapRange")
    falloff.interpolation_type = "SMOOTHSTEP"
    falloff.inputs["From Min"].default_value = 0.3
    falloff.inputs["From Max"].default_value = 0.5
    falloff.inputs["To Min"].default_value = darkness
    falloff.inputs["To Max"].default_value = 0.0
    fade = nodes.new("ShaderNodeValue")
    fade.name = "Fade"
    fade.outputs[0].default_value = 1.0
    alpha = nodes.new("ShaderNodeMath")
    alpha.operation = "MULTIPLY"
    clear = nodes.new("ShaderNodeBsdfTransparent")
    black = nodes.new("ShaderNodeEmission")
    black.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
    black.inputs["Strength"].default_value = 0.0
    mix = nodes.new("ShaderNodeMixShader")
    links.new(coord.outputs["Generated"], split.inputs["Vector"])
    links.new(split.outputs["X"], flat.inputs["X"])
    links.new(split.outputs["Y"], flat.inputs["Y"])
    links.new(flat.outputs["Vector"], center.inputs[0])
    links.new(center.outputs["Vector"], radius.inputs[0])
    links.new(radius.outputs["Value"], falloff.inputs["Value"])
    links.new(falloff.outputs["Result"], alpha.inputs[0])
    links.new(fade.outputs[0], alpha.inputs[1])
    links.new(alpha.outputs["Value"], mix.inputs["Fac"])
    links.new(clear.outputs["BSDF"], mix.inputs[1])
    links.new(black.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs["Shader"], out.inputs["Surface"])
    mat.surface_render_method = "BLENDED"
    obj = mesh_obj(f"contact {label}", plane_mesh(f"contact {label}", size), mat, collection)
    obj.visible_shadow = False
    return obj, mat


# ----------------------------------------------------------------- geometry


def link(obj, collection):
    for coll in list(obj.users_collection):
        coll.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def parent_keep(child, parent):
    bpy.context.view_layer.update()
    world = child.matrix_world.copy()
    child.parent = parent
    child.matrix_parent_inverse = parent.matrix_world.inverted()
    child.matrix_world = world


def box_mesh(name, boxes):
    """boxes: iterable of (center xyz, size xyz)."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    for center, size in boxes:
        geom = bmesh.ops.create_cube(bm, size=1.0)
        verts = geom["verts"]
        bmesh.ops.scale(bm, vec=Vector(size), verts=verts)
        bmesh.ops.translate(bm, vec=Vector(center), verts=verts)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def mesh_obj(name, mesh, material, collection):
    obj = bpy.data.objects.new(name, mesh)
    mesh.materials.append(material)
    collection.objects.link(obj)
    return obj


def plane_mesh(name, size):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=size / 2.0)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def half_disc_mesh(name, radius, depth, segments=24, top=True):
    """Upper or lower half of a disc. The cut is horizontal, so each pole is a face."""
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    half = depth / 2.0
    bmesh.ops.create_cone(
        bm, cap_ends=True, segments=segments,
        radius1=radius, radius2=radius, depth=half,
    )
    bmesh.ops.translate(bm, verts=bm.verts, vec=(0.0, 0.0, half / 2.0 if top else -half / 2.0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    return mesh


def cylinder_mesh(name, radius, depth, segments=32):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=depth)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    return mesh


class Rig:
    pass


def build_board(collection, with_pcb=True):
    rig = Rig()
    before = set(bpy.data.objects)
    for part in board.BOARD_PARTS:
        board.import_board_part(board.BOARD_DIR / part)
    imported = [o for o in bpy.data.objects if o not in before]
    for obj in imported:
        if obj.parent is not None:
            world = obj.matrix_world.copy()
            obj.parent = None
            obj.matrix_world = world
        link(obj, collection)
    meshes = [o for o in imported if o.type == "MESH"]

    def part(token):
        found = [o for o in meshes if token.lower() in o.name.lower()]
        return max(found, key=lambda o: len(o.data.vertices))

    rig.chassis = part("Chasie")
    rig.lid = part("Dekl")
    rig.grid = part("Mrizka")
    rig.foil = part("Folie")
    rig.glass = part("Sklo")
    rig.button = part("Tlacitko")
    for obj in imported:
        if obj.type != "MESH" and not obj.children:
            bpy.data.objects.remove(obj)
    bpy.context.view_layer.update()
    rig.xs = board.square_centers(board.world_coords(rig.grid, 0))
    rig.ys = board.square_centers(board.world_coords(rig.grid, 1))
    if len(rig.xs) != 8 or len(rig.ys) != 8:
        raise RuntimeError("square detection failed")
    rig.surface = max(board.world_coords(rig.glass, 2))
    # Soles sit on the glass. The foil is the pane underneath; seating there
    # plants the pieces inside the glass.
    rig.seat = rig.surface + 0.0004

    dark = board.flat("Square dark", (0.02, 0.021, 0.023), 0.55)
    light = board.flat("Square light", (0.88, 0.86, 0.80), 0.42)
    grid_body = board.flat("Grid plastic", (0.5, 0.5, 0.49), 0.6)
    board.paint_squares(rig.grid, rig.xs, rig.ys, dark, light, grid_body)
    assign = (
        (rig.chassis, board.aluminum("Chassis aluminium")),
        (rig.lid, principled("Inner lid", (0.05, 0.05, 0.055), 0.5)),
        (rig.foil, board.diffuser("Diffuser film")),
        (rig.glass, smudged_glass()),
        (rig.button, principled("Button", (0.01, 0.01, 0.011), 0.35, specular=0.6)),
    )
    for obj, mat in assign:
        obj.data.materials.clear()
        obj.data.materials.append(mat)
    for node in rig.foil.data.materials[0].node_tree.nodes:
        if node.type == "MIX_SHADER":
            node.inputs["Fac"].default_value = 0.42
    for obj in (rig.glass, rig.foil):
        obj.visible_shadow = False
    for obj in meshes:
        for poly in obj.data.polygons:
            poly.use_smooth = False

    rig.pcb_root = None
    if with_pcb:
        build_pcb(rig, collection)
    return rig


def build_pcb(rig, collection):
    root = bpy.data.objects.new("PCB assembly", None)
    collection.objects.link(root)
    rig.pcb_root = root
    top = PCB_Z + PCB_T
    half = 0.2795 / 2
    slab = mesh_obj(
        "PCB",
        box_mesh("PCB", [((0.0, 0.0, PCB_Z + PCB_T / 2), (2 * half, 2 * half, PCB_T))]),
        principled("Solder mask", (0.012, 0.016, 0.014), 0.62, specular=0.3),
        collection,
    )
    leds, halls, pads, silk = [], [], [], []
    for x in rig.xs:
        for y in rig.ys:
            leds.append(((x, y + 0.0085, top + 0.0008), (0.005, 0.005, 0.0016)))
            pads.append(((x, y + 0.0085, top + 0.00004), (0.0064, 0.0064, 0.00008)))
            for sx in (-HALL_PITCH / 2, HALL_PITCH / 2):
                halls.append(((x + sx, y, top + 0.0005), (0.0029, 0.0016, 0.001)))
                pads.append(((x + sx, y, top + 0.00004), (0.0036, 0.0024, 0.00008)))
            for dx, dy, sx, sy in ((0, 0.0158, 0.031, 0.0003), (0, -0.0158, 0.031, 0.0003),
                                   (0.0158, 0, 0.0003, 0.031), (-0.0158, 0, 0.0003, 0.031)):
                silk.append(((x + dx, y + dy, top + 0.00002), (sx, sy, 0.00004)))
    parts = [
        mesh_obj("WS2812B x64", box_mesh("WS2812B", leds), principled("LED body", (0.92, 0.92, 0.9), 0.3), collection),
        mesh_obj("Hall sensors x128", box_mesh("Hall", halls), principled("SOT-23", (0.015, 0.015, 0.016), 0.45), collection),
        mesh_obj("Pads", box_mesh("Pads", pads), principled("ENIG gold", (1.0, 0.78, 0.42), 0.22, metallic=1.0), collection),
        mesh_obj("Silkscreen", box_mesh("Silk", silk), principled("Silkscreen", (0.85, 0.85, 0.82), 0.6), collection),
    ]
    bottom = PCB_Z
    chips = []
    for seg in range(4):
        y = (rig.ys[2 * seg] + rig.ys[2 * seg + 1]) / 2
        chips.append(((0.0, y, bottom - 0.0006), (0.0065, 0.0044, 0.0012)))
    parts.append(mesh_obj("STM32C031 x4", box_mesh("STM32", chips), principled("IC", (0.02, 0.02, 0.022), 0.5), collection))
    module = [((-0.095, -0.115, bottom - 0.0005), (0.018, 0.0255, 0.001))]
    shield = [((-0.095, -0.111, bottom - 0.0022), (0.016, 0.017, 0.0024))]
    parts.append(mesh_obj("ESP32-C6 module", box_mesh("Module", module), principled("Module PCB", (0.02, 0.05, 0.12), 0.4), collection))
    parts.append(mesh_obj("ESP32-C6 shield", box_mesh("Shield", shield), principled("Shield", (0.8, 0.8, 0.82), 0.25, metallic=1.0), collection))
    for obj in [slab] + parts:
        obj.parent = root
    rig.pcb_parts = [slab] + parts


# ----------------------------------------------------------------- LEDs


class Leds:
    """Per square: an evenly lit 33 mm glow on the diffuser foil, the LED die
    on the PCB and an upward 33 mm area light under the glass, all driven by
    the same keys. The foil spreads one WS2812B over the whole cell, so the
    light source is the cell, not the LED."""

    def __init__(self, rig, collection, glow_strength=7.0, light_energy=0.35, glow_parent=None, light_parent=None, shadows=False, dies=True):
        self.rig = rig
        self.glow_strength = glow_strength
        self.light_energy = light_energy
        self.mats = {}
        self.lights = {}
        self.last = {}
        glow_mesh = plane_mesh("LED glow", CELL)
        die_mesh = plane_mesh("LED die", 0.0034)
        placeholder = emissive("LED placeholder", (0, 0, 0), 0.0)
        glow_mesh.materials.append(placeholder)
        die_mesh.materials.append(placeholder)
        for fi, x in enumerate(rig.xs):
            for ri, y in enumerate(rig.ys):
                square = fx.name(ri, fi)
                mat = self.material(square)
                glow = bpy.data.objects.new(f"glow {square}", glow_mesh)
                glow.location = (x, y, LED_GLOW_Z)
                collection.objects.link(glow)
                glow.material_slots[0].link = "OBJECT"
                glow.material_slots[0].material = mat
                glow.visible_shadow = False
                die = bpy.data.objects.new(f"die {square}", die_mesh)
                die.location = (x, y + 0.0085, PCB_Z + PCB_T + 0.00165)
                collection.objects.link(die)
                die.material_slots[0].link = "OBJECT"
                die.material_slots[0].material = mat
                die.visible_shadow = False
                die.hide_render = not dies
                data = bpy.data.lights.new(f"led {square}", "AREA")
                data.shape = "SQUARE"
                data.size = CELL
                data.energy = 0.0
                data.use_shadow = shadows
                if shadows and hasattr(data, "shadow_maximum_resolution"):
                    data.shadow_maximum_resolution = 0.02
                lamp = bpy.data.objects.new(f"led {square}", data)
                lamp.location = (x, y, LED_LIGHT_Z)
                lamp.rotation_euler = (math.pi, 0.0, 0.0)
                collection.objects.link(lamp)
                lamp.visible_camera = False
                lamp.visible_glossy = True
                lamp.visible_transmission = False
                if glow_parent is not None:
                    parent_keep(glow, glow_parent)
                if light_parent is not None:
                    parent_keep(die, light_parent)
                    parent_keep(lamp, light_parent)
                self.mats[square] = mat
                self.lights[square] = data

    def material(self, square):
        mat, nodes, links = nodes_mat(f"LED {square}")
        out = nodes.new("ShaderNodeOutputMaterial")
        coord = nodes.new("ShaderNodeTexCoord")
        center = nodes.new("ShaderNodeVectorMath")
        center.operation = "SUBTRACT"
        center.inputs[1].default_value = (0.5, 0.5, 0.5)
        stretch = nodes.new("ShaderNodeVectorMath")
        stretch.operation = "SCALE"
        stretch.inputs["Scale"].default_value = 2.0
        split = nodes.new("ShaderNodeSeparateXYZ")
        abs_u = nodes.new("ShaderNodeMath")
        abs_u.operation = "ABSOLUTE"
        abs_v = nodes.new("ShaderNodeMath")
        abs_v.operation = "ABSOLUTE"
        edge = nodes.new("ShaderNodeMath")
        edge.operation = "MAXIMUM"
        # Flat across the cell, soft roll-off in the last few mm at the grid wall.
        box = nodes.new("ShaderNodeMapRange")
        box.interpolation_type = "SMOOTHSTEP"
        box.inputs["From Min"].default_value = 0.7
        box.inputs["From Max"].default_value = 1.0
        box.inputs["To Min"].default_value = 1.0
        box.inputs["To Max"].default_value = 0.35
        radius = nodes.new("ShaderNodeVectorMath")
        radius.operation = "LENGTH"
        hot = nodes.new("ShaderNodeMapRange")
        hot.inputs["From Min"].default_value = 0.0
        hot.inputs["From Max"].default_value = 1.4
        hot.inputs["To Min"].default_value = 1.12
        hot.inputs["To Max"].default_value = 0.88
        shape = nodes.new("ShaderNodeMath")
        shape.operation = "MULTIPLY"
        level = nodes.new("ShaderNodeMath")
        level.operation = "MULTIPLY"
        level.name = "Level"
        level.inputs[1].default_value = 0.0
        emit = nodes.new("ShaderNodeEmission")
        emit.name = "LED"
        emit.inputs["Color"].default_value = (0, 0, 0, 1)
        clear = nodes.new("ShaderNodeBsdfTransparent")
        add = nodes.new("ShaderNodeAddShader")
        links.new(coord.outputs["Generated"], center.inputs[0])
        links.new(center.outputs["Vector"], stretch.inputs[0])
        links.new(stretch.outputs["Vector"], split.inputs["Vector"])
        links.new(split.outputs["X"], abs_u.inputs[0])
        links.new(split.outputs["Y"], abs_v.inputs[0])
        links.new(abs_u.outputs["Value"], edge.inputs[0])
        links.new(abs_v.outputs["Value"], edge.inputs[1])
        links.new(edge.outputs["Value"], box.inputs["Value"])
        flat = nodes.new("ShaderNodeCombineXYZ")
        links.new(split.outputs["X"], flat.inputs["X"])
        links.new(split.outputs["Y"], flat.inputs["Y"])
        links.new(flat.outputs["Vector"], radius.inputs[0])
        links.new(radius.outputs["Value"], hot.inputs["Value"])
        links.new(box.outputs["Result"], shape.inputs[0])
        links.new(hot.outputs["Result"], shape.inputs[1])
        links.new(shape.outputs["Value"], level.inputs[0])
        links.new(level.outputs["Value"], emit.inputs["Strength"])
        links.new(clear.outputs["BSDF"], add.inputs[0])
        links.new(emit.outputs["Emission"], add.inputs[1])
        links.new(add.outputs["Shader"], out.inputs["Surface"])
        # Dithered transparency vanishes under the blended glass and foil.
        mat.surface_render_method = "BLENDED"
        return mat

    def _write_led(self, square, frame, rgb):
        mat = self.mats[square]
        peak = max(rgb)
        color = (0.0, 0.0, 0.0) if peak == 0 else tuple(c / peak for c in rgb)
        level = peak / 255.0
        emit = mat.node_tree.nodes["LED"]
        mult = mat.node_tree.nodes["Level"]
        emit.inputs["Color"].default_value = (*color, 1.0)
        mult.inputs[1].default_value = level * self.glow_strength
        emit.inputs["Color"].keyframe_insert("default_value", frame=frame)
        mult.inputs[1].keyframe_insert("default_value", frame=frame)
        light = self.lights[square]
        light.color = color if peak else (1.0, 1.0, 1.0)
        light.energy = level * self.light_energy
        light.keyframe_insert("color", frame=frame)
        light.keyframe_insert("energy", frame=frame)
        return mat.node_tree, light

    def key(self, frame, state):
        """state: square -> RGB 0..255 for the whole board.

        The old colour is keyed again on the frame before a change, so the
        blend from green to blue cannot run through the whole move.
        """
        touched = []
        for square in self.mats:
            rgb = tuple(state.get(square, (0, 0, 0)))
            prev = self.last.get(square)
            if prev == rgb:
                continue
            if prev is not None and frame > 1:
                touched.extend(self._write_led(square, frame - 1, prev))
            touched.extend(self._write_led(square, frame, rgb))
            self.last[square] = rgb
        for block in touched:
            set_interpolation(block, "CONSTANT")

    def play(self, start, seq, speed=1.0):
        end = start
        for t_ms, state in seq:
            frame = start + round(t_ms / 1000.0 * FPS / speed)
            self.key(frame, state)
            end = max(end, frame)
        return end

    def finish(self):
        for mat in self.mats.values():
            set_interpolation(mat.node_tree, "CONSTANT")
        for light in self.lights.values():
            set_interpolation(light, "CONSTANT")


# ----------------------------------------------------------------- pieces


def piece_materials():
    white = pla("PLA white", (0.84, 0.83, 0.79), 0.5, 0.35)
    black = pla("PLA black", (0.008, 0.008, 0.009), 0.55, 0.4)
    north = emissive("Magnet N", (1.0, 0.16, 0.1), 0.0)
    south = emissive("Magnet S", (0.12, 0.38, 1.0), 0.0)
    return white, black, north, south


def piece_templates(white):
    templates = {}
    for kind in ("pawn", "rook", "bishop", "queen", "king", "knight"):
        path = board.KNIGHT if kind == "knight" else board.PIECE_DIR / f"{kind}.stl"
        obj = board.load_piece(path, 0.001, white)
        obj.hide_render = True
        obj.hide_viewport = True
        templates[kind] = obj
    return templates


def make_piece(templates, kind, color, label, location, mats, collection, magnets=True):
    white, black, north, south = mats
    base = templates[kind]
    obj = bpy.data.objects.new(label, base.data)
    obj.location = location
    obj.scale = base.scale
    if kind == "knight" and color == "b":
        obj.rotation_euler = (0.0, 0.0, math.pi)
    obj.material_slots[0].link = "OBJECT"
    obj.material_slots[0].material = white if color == "w" else black
    collection.objects.link(obj)
    if magnets:
        red_mesh = bpy.data.meshes.get("Magnet red half")
        blue_mesh = bpy.data.meshes.get("Magnet blue half")
        if red_mesh is None:
            red_mesh = half_disc_mesh("Magnet red half", 3.2, 2.2, top=True)
            blue_mesh = half_disc_mesh("Magnet blue half", 3.2, 2.2, top=False)
            red_mesh.materials.append(north)
            blue_mesh.materials.append(south)
        # Disc is built with north (red) on +Z and south (blue) on -Z, so an
        # unflipped magnet already has south toward the board. White flips.
        for i, (mx, my) in enumerate(MAGNET_POS[kind]):
            root = bpy.data.objects.new(f"{label} magnet {i}", None)
            root.parent = obj
            root.matrix_parent_inverse = Matrix.Identity(4)
            root.location = (mx, my, 1.9)
            if color == "w":
                root.rotation_euler = (math.pi, 0.0, 0.0)
            collection.objects.link(root)
            red = bpy.data.objects.new(f"{label} magnet {i} N", red_mesh)
            blue = bpy.data.objects.new(f"{label} magnet {i} S", blue_mesh)
            for half in (red, blue):
                half.parent = root
                half.matrix_parent_inverse = Matrix.Identity(4)
                collection.objects.link(half)
    return obj


def key_magnets(mats, frame, strength):
    for mat in mats[2:]:
        node = next(n for n in mat.node_tree.nodes if n.type == "EMISSION")
        node.inputs["Strength"].default_value = strength
        node.inputs["Strength"].keyframe_insert("default_value", frame=frame)


# ----------------------------------------------------------------- cameras and light


def camera(collection, label, lens=50.0, fstop=4.0, sensor=36.0):
    data = bpy.data.cameras.new(label)
    data.lens = lens
    data.sensor_width = sensor
    data.clip_start = 0.01
    data.clip_end = 30.0
    data.dof.use_dof = True
    data.dof.aperture_fstop = fstop
    cam = bpy.data.objects.new(label, data)
    target = bpy.data.objects.new(label + " target", None)
    collection.objects.link(cam)
    collection.objects.link(target)
    data.dof.focus_object = target
    con = cam.constraints.new("TRACK_TO")
    con.target = target
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    return cam, target


def shot(scene, cam, target, keys, roll=None):
    """keys: list of (frame, cam_loc, target_loc, lens[, fstop])."""
    for item in keys:
        frame, loc, aim, lens = item[:4]
        key_loc(cam, frame, loc)
        key_loc(target, frame, aim)
        cam.data.lens = lens
        cam.data.keyframe_insert("lens", frame=frame)
        if len(item) > 4:
            cam.data.dof.aperture_fstop = item[4]
            cam.data.dof.keyframe_insert("aperture_fstop", frame=frame)
    # Auto Bézier overshoots and the camera dives between keys. Clamped handles
    # stay inside the keys, so a wide shot cannot pass through the pieces.
    for block in (cam, target, cam.data):
        ad = getattr(block, "animation_data", None)
        slot = getattr(ad, "action_slot", None) if ad else None
        for curve in fcurves_of(ad.action if ad else None, slot):
            for key in curve.keyframe_points:
                key.handle_left_type = "AUTO_CLAMPED"
                key.handle_right_type = "AUTO_CLAMPED"


def cut(scene, frame, cam):
    marker = scene.timeline_markers.new(cam.name, frame=frame)
    marker.camera = cam


def area(collection, label, loc, size, energy, color, aim, shape="SQUARE", size_y=None, spread=None, glossy=False):
    data = bpy.data.lights.new(label, "AREA")
    data.energy = energy
    data.color = color
    data.shape = shape
    data.size = size
    if size_y is not None:
        data.size_y = size_y
    if spread is not None:
        data.spread = spread
    lamp = bpy.data.objects.new(label, data)
    lamp.location = loc
    collection.objects.link(lamp)
    con = lamp.constraints.new("TRACK_TO")
    con.target = aim
    con.track_axis = "TRACK_NEGATIVE_Z"
    con.up_axis = "UP_Y"
    # Hide the card from the camera. Glossy on so brushed aluminium can catch it.
    lamp.visible_camera = False
    lamp.visible_glossy = glossy
    lamp.visible_transmission = False
    return lamp


def black_world(scene, color=(0.0, 0.0, 0.0), strength=0.0, haze=0.0):
    world = bpy.data.worlds.new("Void")
    world.use_nodes = True
    nodes = world.node_tree.nodes
    links = world.node_tree.links
    bg = nodes.get("Background")
    bg.inputs["Color"].default_value = (*color, 1.0)
    bg.inputs["Strength"].default_value = strength
    if haze > 0:
        out = nodes.get("World Output")
        vol = nodes.new("ShaderNodeVolumeScatter")
        vol.inputs["Density"].default_value = haze
        vol.inputs["Anisotropy"].default_value = 0.55
        links.new(vol.outputs["Volume"], out.inputs["Volume"])
    scene.world = world
    return world


def gradient_card(collection, label, loc, size, color, strength, rotation=(math.pi / 2, 0, 0)):
    mat, nodes, links = nodes_mat(label)
    out = nodes.new("ShaderNodeOutputMaterial")
    coord = nodes.new("ShaderNodeTexCoord")
    center = nodes.new("ShaderNodeVectorMath")
    center.operation = "SUBTRACT"
    center.inputs[1].default_value = (0.5, 0.5, 0.5)
    stretch = nodes.new("ShaderNodeVectorMath")
    stretch.operation = "SCALE"
    stretch.inputs["Scale"].default_value = 2.0
    grad = nodes.new("ShaderNodeTexGradient")
    grad.gradient_type = "QUADRATIC_SPHERE"
    power = nodes.new("ShaderNodeMath")
    power.operation = "POWER"
    power.inputs[1].default_value = 1.6
    level = nodes.new("ShaderNodeMath")
    level.operation = "MULTIPLY"
    level.name = "Level"
    level.inputs[1].default_value = strength
    emit = nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (*color, 1.0)
    links.new(coord.outputs["Generated"], center.inputs[0])
    links.new(center.outputs["Vector"], stretch.inputs[0])
    links.new(stretch.outputs["Vector"], grad.inputs["Vector"])
    links.new(grad.outputs["Fac"], power.inputs[0])
    links.new(power.outputs["Value"], level.inputs[0])
    links.new(level.outputs["Value"], emit.inputs["Strength"])
    links.new(emit.outputs["Emission"], out.inputs["Surface"])
    obj = mesh_obj(label, plane_mesh(label, 1.0), mat, collection)
    obj.location = loc
    obj.scale = (size[0], size[1], 1.0)
    obj.rotation_euler = rotation
    obj.visible_shadow = False
    return obj, mat


def key_card(mat, frame, value):
    node = mat.node_tree.nodes["Level"]
    node.inputs[1].default_value = value
    node.inputs[1].keyframe_insert("default_value", frame=frame)


def floor(collection, size=6.0, color=(0.006, 0.006, 0.007), roughness=0.22):
    mat, nodes, links = nodes_mat("Floor")
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Specular IOR Level"].default_value = 0.6
    coord = nodes.new("ShaderNodeTexCoord")
    dist = nodes.new("ShaderNodeVectorMath")
    dist.operation = "LENGTH"
    rough = nodes.new("ShaderNodeMapRange")
    rough.inputs["From Min"].default_value = 0.3
    rough.inputs["From Max"].default_value = 1.6
    rough.inputs["To Min"].default_value = roughness
    rough.inputs["To Max"].default_value = 0.9
    links.new(coord.outputs["Object"], dist.inputs[0])
    links.new(dist.outputs["Value"], rough.inputs["Value"])
    links.new(rough.outputs["Result"], bsdf.inputs["Roughness"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    obj = mesh_obj("Floor", plane_mesh("Floor", size), mat, collection)
    obj.location = (0.0, 0.0, -0.0002)
    return obj


# ----------------------------------------------------------------- render


def render_settings(scene, res, samples=32, motion_blur=True, exposure=0.0):
    scene.render.engine = "BLENDER_EEVEE"
    eevee = scene.eevee
    eevee.taa_render_samples = samples
    eevee.use_raytracing = True
    try:
        eevee.ray_tracing_method = "SCREEN"
    except TypeError:
        pass
    opts = eevee.ray_tracing_options
    for attr, value in (("resolution_scale", "2"), ("use_denoise", True), ("screen_trace_quality", 0.5)):
        if hasattr(opts, attr):
            try:
                setattr(opts, attr, value)
            except TypeError:
                pass
    eevee.use_fast_gi = True
    eevee.volumetric_tile_size = "8"
    eevee.volumetric_samples = 48
    eevee.use_volumetric_shadows = True
    eevee.clamp_surface_indirect = 10.0
    eevee.shadow_resolution_scale = 1.0
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    scene.render.use_motion_blur = motion_blur
    scene.render.motion_blur_shutter = 0.45
    scene.render.film_transparent = False
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.exposure = exposure
    for look in ("AgX - Medium High Contrast", "AgX - Punchy", "None"):
        try:
            scene.view_settings.look = look
            break
        except TypeError:
            continue


def compositor(scene, bloom=0.55, threshold=1.2, dispersion=0.012, vignette=0.32):
    tree = bpy.data.node_groups.new(f"{scene.name} grade", "CompositorNodeTree")
    tree.interface.new_socket("Image", in_out="OUTPUT", socket_type="NodeSocketColor")
    nodes = tree.nodes
    links = tree.links
    rl = nodes.new("CompositorNodeRLayers")
    rl.scene = scene
    glare = nodes.new("CompositorNodeGlare")
    glare.inputs["Type"].default_value = "Bloom"
    glare.inputs["Quality"].default_value = "High"
    glare.inputs["Threshold"].default_value = threshold
    glare.inputs["Strength"].default_value = bloom
    glare.inputs["Size"].default_value = 0.7
    lens = nodes.new("CompositorNodeLensdist")
    lens.inputs["Dispersion"].default_value = dispersion
    mask = nodes.new("CompositorNodeEllipseMask")
    mask.inputs["Size"].default_value = (1.25, 1.05)
    blur = nodes.new("CompositorNodeBlur")
    blur.inputs["Size"].default_value = (300, 300) if blur.inputs["Size"].type == "VECTOR" else 300
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = vignette
    out = nodes.new("NodeGroupOutput")
    links.new(rl.outputs["Image"], glare.inputs["Image"])
    links.new(glare.outputs["Image"], lens.inputs["Image"])
    links.new(mask.outputs["Mask"], blur.inputs["Image"])
    links.new(lens.outputs["Image"], mix.inputs[6])
    links.new(blur.outputs["Image"], mix.inputs[7])
    links.new(mix.outputs[2], out.inputs[0])
    scene.compositing_node_group = tree
    scene.render.use_compositing = True
    return tree


def video_output(scene, path, quality="PERC_LOSSLESS"):
    scene.render.image_settings.media_type = "VIDEO"
    scene.render.image_settings.file_format = "FFMPEG"
    ff = scene.render.ffmpeg
    ff.format = "MPEG4"
    ff.codec = "H264"
    ff.constant_rate_factor = quality
    ff.ffmpeg_preset = "GOOD"
    ff.gopsize = max(1, FPS // 2)
    ff.audio_codec = "NONE"
    scene.render.filepath = str(path)


def still_output(scene, path):
    scene.render.image_settings.media_type = "IMAGE"
    scene.render.image_settings.file_format = "JPEG"
    scene.render.image_settings.quality = 92
    scene.render.filepath = str(path)


CAPTION_STYLES = {
    "hero": {"size": 0.072, "y": 0.50, "font": "segoeuib.ttf"},
    "main": {"size": 0.046, "y": 0.13, "font": "seguisb.ttf"},
    "small": {"size": 0.036, "y": 0.13, "font": "seguisb.ttf"},
    "logo": {"size": 0.11, "y": 0.53, "font": "segoeuib.ttf"},
    "tagline": {"size": 0.034, "y": 0.43, "font": "segoeuil.ttf"},
    "label": {"size": 0.03, "y": 0.1, "font": "seguisb.ttf"},
}


def edit_scene(raw_path, out_path, res, frame_end, captions, name="Edit"):
    edit = bpy.data.scenes.new(name)
    edit.render.resolution_x, edit.render.resolution_y = res
    edit.render.resolution_percentage = 100
    edit.render.fps = FPS
    edit.frame_start = 1
    edit.frame_end = frame_end
    edit.view_settings.view_transform = "Standard"
    se = edit.sequence_editor_create()
    se.strips.new_movie("film", str(raw_path), channel=1, frame_start=1)
    fonts = {}
    for i, cap in enumerate(captions):
        style = CAPTION_STYLES[cap.get("style", "main")]
        length = max(2, cap["f1"] - cap["f0"])
        strip = se.strips.new_effect(name=f"cap {i}", type="TEXT", channel=2 + (i % 3), frame_start=cap["f0"], length=length)
        strip.text = cap["text"]
        font_file = style["font"]
        if font_file not in fonts:
            fonts[font_file] = bpy.data.fonts.load(str(FONT_DIR / font_file), check_existing=True)
        strip.font = fonts[font_file]
        strip.font_size = int(res[1] * style["size"])
        strip.color = (1.0, 0.985, 0.96, 1.0)
        strip.use_shadow = True
        strip.shadow_color = (0.0, 0.0, 0.0, 0.55)
        if hasattr(strip, "shadow_blur"):
            strip.shadow_blur = 0.6
        strip.location = (0.5, cap.get("y", style["y"]))
        strip.alignment_x = "CENTER"
        strip.anchor_x = "CENTER"
        strip.anchor_y = "CENTER"
        strip.blend_type = "ALPHA_OVER"
        fade = min(F(6), max(2, length // 3))
        start, end = cap["f0"], cap["f0"] + length
        keys = [(start, 0.0), (start + fade, 1.0)]
        if end < frame_end:
            keys += [(end - fade, 1.0), (end, 0.0)]
        for frame, alpha in keys:
            strip.blend_alpha = alpha
            strip.keyframe_insert("blend_alpha", frame=frame)
    video_output(edit, out_path, "HIGH")
    edit.render.use_compositing = False
    edit.render.use_sequencer = True
    return edit
