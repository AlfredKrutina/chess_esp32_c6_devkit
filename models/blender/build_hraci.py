"""Two people playing a legal Queen's Gambit on CzechMate.

Cycles, 24 fps. The board and the knight_low pieces are the product meshes.
The room is CC0 scans (Poly Haven): dining table and chairs, herringbone floor,
plaster, linen curtains, a pendant lamp, a plant, a framed picture and a side
table. People are CC0 MakeHuman characters (make_players.py). Run stills with
--test before --final. Do not pass --final until asked.
"""

import json
import math
import shutil
import subprocess
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cine_common as cc  # noqa: E402
import led_effects as fx  # noqa: E402

ROOT = Path(__file__).resolve().parent
TABLE_DIR = ROOT / "set" / "table"
ROOM = ROOT / "set" / "room"
BLEND = ROOT / "czechmate_hraci.blend"
FINAL = cc.VIDEO_DIR / "hraci.mp4"
PREVIEW = cc.VIDEO_DIR / "hraci_720.mp4"
FPS = 24
TABLE_Z = 0.7402
LIFT = 0.098
FILES = "abcdefgh"

# grip height, body radius, top, all metres from the foot
SPECS = {
    "pawn": (0.018, 0.010, 0.045),
    "rook": (0.022, 0.013, 0.056),
    "knight": (0.026, 0.012, 0.055),
    "bishop": (0.024, 0.012, 0.060),
    "queen": (0.028, 0.012, 0.065),
    "king": (0.034, 0.013, 0.080),
}
# palm sits this far past the radius, toward the player, so the fingertips
# stop on the near side of the piece instead of inside it
PALM_GAP = 0.002

# 1. d4 d5 2. c4 e6 3. Nc3 Nf6 4. Bg5 Be7 5. e3 O-O
# castling is king, then rook
GAME = [
    ("w", "w_pawn_d", "pawn", "d2", "d4"),
    ("b", "b_pawn_d", "pawn", "d7", "d5"),
    ("w", "w_pawn_c", "pawn", "c2", "c4"),
    ("b", "b_pawn_e", "pawn", "e7", "e6"),
    ("w", "w_knight_b", "knight", "b1", "c3"),
    ("b", "b_knight_g", "knight", "g8", "f6"),
    ("w", "w_bishop_c", "bishop", "c1", "g5"),
    ("b", "b_bishop_f", "bishop", "f8", "e7"),
    ("w", "w_pawn_e", "pawn", "e2", "e3"),
    ("b", "b_king_e", "king", "e8", "g8"),
    ("b", "b_rook_h", "rook", "h8", "f8"),
]
DUSK_MOVE = 3
CLOSE_MOVE = 2
NIGHT_MOVE = 6


def activate(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def apply_modifier(obj, name):
    activate(obj)
    bpy.ops.object.modifier_apply(modifier=name)


def cube_uv(obj, size):
    activate(obj)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.cube_project(cube_size=size, correct_aspect=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def load_img(path, color=True):
    image = bpy.data.images.load(str(path))
    if not color:
        image.colorspace_settings.name = "Non-Color"
    return image


def pbr(name, diff, nor, arm, specular=0.28, normal_strength=0.55, projection="FLAT", scale=1.0):
    mat, nodes, links = cc.nodes_mat(name)
    out = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Specular IOR Level"].default_value = specular
    bsdf.inputs["Metallic"].default_value = 0.0
    coord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (scale, scale, scale)
    src = coord.outputs["Object"] if projection == "BOX" else coord.outputs["UV"]
    links.new(src, mapping.inputs["Vector"])
    images = (
        (diff, True, "Color"),
        (nor, False, "Normal"),
        (arm, False, "Arm"),
    )
    tex = {}
    for path, is_color, key in images:
        node = nodes.new("ShaderNodeTexImage")
        node.image = load_img(path, is_color)
        if projection == "BOX":
            node.projection = "BOX"
            node.projection_blend = 0.2
        links.new(mapping.outputs["Vector"], node.inputs["Vector"])
        tex[key] = node
    nmap = nodes.new("ShaderNodeNormalMap")
    nmap.inputs["Strength"].default_value = normal_strength
    sep = nodes.new("ShaderNodeSeparateColor")
    ao = nodes.new("ShaderNodeMix")
    ao.data_type = "RGBA"
    ao.blend_type = "MULTIPLY"
    ao.inputs["Factor"].default_value = 0.65
    links.new(tex["Color"].outputs["Color"], ao.inputs["A"])
    links.new(tex["Arm"].outputs["Color"], sep.inputs["Color"])
    links.new(sep.outputs["Red"], ao.inputs["B"])
    links.new(ao.outputs["Result"], bsdf.inputs["Base Color"])
    links.new(tex["Normal"].outputs["Color"], nmap.inputs["Color"])
    links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def put_box(name, boxes, material, collection, uv=0.4):
    obj = cc.mesh_obj(name, cc.box_mesh(name, boxes), material, collection)
    for poly in obj.data.polygons:
        poly.use_smooth = False
    if uv:
        cube_uv(obj, uv)
    return obj


def import_gltf(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    return [obj for obj in bpy.data.objects if obj not in before]


def hide_obj(obj):
    obj.hide_render = True
    obj.hide_viewport = True


def scene_audit():
    """Meshes that render below the floor, and any cloth still in the scene."""
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    print("--- audit ---")
    low = 0
    for obj in bpy.data.objects:
        if obj.type != "MESH" or obj.hide_render:
            continue
        if "cloth" in obj.name.lower():
            print("CLOTH", obj.name)
            low += 1
        ev = obj.evaluated_get(deps)
        mesh = ev.to_mesh()
        if not mesh.vertices:
            ev.to_mesh_clear()
            continue
        z0 = min((ev.matrix_world @ vert.co).z for vert in mesh.vertices)
        ev.to_mesh_clear()
        if z0 < -0.001:
            print("LOW", obj.name, round(z0, 3))
            low += 1
    if low == 0:
        print("audit clear")


def parent_new(objs, name, collection):
    empty = bpy.data.objects.new(name, None)
    collection.objects.link(empty)
    for obj in objs:
        cc.parent_keep(obj, empty)
    return empty


def duplicate_tree(root, name, collection):
    copy = root.copy()
    copy.name = name
    collection.objects.link(copy)
    for child in list(root.children):
        cloned = child.copy()
        cloned.data = child.data
        collection.objects.link(cloned)
        cloned.parent = copy
        cloned.matrix_parent_inverse = child.matrix_parent_inverse.copy()
    return copy


def mesh_zs(obj, limit=0.12):
    found = []
    feet = []
    for vert in obj.data.vertices:
        world = obj.matrix_world @ vert.co
        feet.append(world.z)
        if abs(world.x) < limit and abs(world.y) < limit:
            found.append(world.z)
    return feet, found


def curtain_mesh(name, x, y0, y1, z0, z1):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    ny, nz = 22, 36
    grid = []
    for j in range(nz + 1):
        row = []
        v = j / nz
        z = z1 + (z0 - z1) * v
        for i in range(ny + 1):
            u = i / ny
            y = y0 + (y1 - y0) * u
            gather = math.sin(u * math.pi)
            fold = math.sin((u * 5.0 + v * 0.35) * math.tau) * (0.012 + 0.016 * gather)
            row.append(bm.verts.new((x + fold, y, z - 0.02 * gather * math.sin(v * math.pi))))
        grid.append(row)
    for j in range(nz):
        for i in range(ny):
            face = bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i]))
            for loop in face.loops:
                co = loop.vert.co
                loop[uv_layer].uv = ((co.y - y0) / 0.28, (z1 - co.z) / 0.55)
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def surface_mesh(name, sx, sy, z, tile, thickness=0.0):
    mesh = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.new("UVMap")
    verts = [
        bm.verts.new((-sx / 2, -sy / 2, z)),
        bm.verts.new((sx / 2, -sy / 2, z)),
        bm.verts.new((sx / 2, sy / 2, z)),
        bm.verts.new((-sx / 2, sy / 2, z)),
    ]
    face = bm.faces.new(verts)
    for loop in face.loops:
        co = loop.vert.co
        loop[uv_layer].uv = (co.x / tile, co.y / tile)
    if thickness:
        extruded = bmesh.ops.extrude_face_region(bm, geom=[face])["geom"]
        bmesh.ops.translate(bm, verts=[v for v in extruded if isinstance(v, bmesh.types.BMVert)], vec=(0.0, 0.0, -thickness))
    bm.normal_update()
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def ensure_side_textures():
    dest = TABLE_DIR / "textures"
    dest.mkdir(parents=True, exist_ok=True)
    for name in ("WoodenTable_02_diff_2k.jpg", "WoodenTable_02_nor_gl_2k.jpg", "WoodenTable_02_arm_2k.jpg"):
        target = dest / name
        if not target.exists():
            shutil.copy(TABLE_DIR / name, target)


def clear_frame_glass(mesh):
    for slot in mesh.material_slots:
        mat = slot.material
        if not mat or "glass" not in mat.name:
            continue
        nodes = mat.node_tree.nodes
        links = mat.node_tree.links
        nodes.clear()
        out = nodes.new("ShaderNodeOutputMaterial")
        bsdf = nodes.new("ShaderNodeBsdfPrincipled")
        bsdf.inputs["Base Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.04
        bsdf.inputs["Transmission Weight"].default_value = 1.0
        bsdf.inputs["IOR"].default_value = 1.5
        links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])


def hang_artwork(mesh, path):
    image = load_img(path)
    for slot in mesh.material_slots:
        mat = slot.material
        if not mat or "artwork" not in mat.name:
            continue
        for node in mat.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image and "diff" in node.image.name:
                node.image = image


def add_globe_glow(lamp_obj):
    globe = None
    for slot in lamp_obj.material_slots:
        if slot.material and "globe" in slot.material.name:
            globe = slot.material
    if globe is None:
        raise RuntimeError("ceiling lamp has no globe material")
    nodes = globe.node_tree.nodes
    links = globe.node_tree.links
    out = next(node for node in nodes if node.type == "OUTPUT_MATERIAL")
    incoming = out.inputs["Surface"].links[0].from_socket
    links.remove(out.inputs["Surface"].links[0])
    emit = nodes.new("ShaderNodeEmission")
    emit.name = "Emission"
    emit.inputs["Color"].default_value = (1.0, 0.78, 0.48, 1.0)
    emit.inputs["Strength"].default_value = 0.0
    add = nodes.new("ShaderNodeAddShader")
    links.new(incoming, add.inputs[0])
    links.new(emit.outputs["Emission"], add.inputs[1])
    links.new(add.outputs["Shader"], out.inputs["Surface"])
    return globe


def build_room(collection):
    # The scan is 2.05 x 1.15 m. Width stays 1.46 m so both players fit.
    # Depth is shallower: at 0.82 m the near edge cut through the jacket.
    # 0.60 m puts the edge in front of the belly and still frames the board.
    table_sxy = 0.82 / 1.152
    table_sy = 0.60 / 1.152
    table_sz = TABLE_Z / 0.786
    inner_x, inner_y, back_x = 1.82, 2.18, -2.35
    ceil, thick = 2.70, 0.16
    win_y, win_z0, win_z1 = 0.70, 0.88, 2.20
    wall_x = inner_x + thick / 2
    span_y = inner_y * 2
    span_x = inner_x - back_x

    floor_mat = pbr(
        "Parquet",
        ROOM / "floor" / "herringbone_parquet_Diffuse_2k.jpg",
        ROOM / "floor" / "herringbone_parquet_nor_gl_2k.jpg",
        ROOM / "floor" / "herringbone_parquet_arm_2k.jpg",
        specular=0.22, normal_strength=0.45,
    )
    plaster = pbr(
        "Plaster",
        ROOM / "wall" / "painted_plaster_wall_Diffuse_2k.jpg",
        ROOM / "wall" / "painted_plaster_wall_nor_gl_2k.jpg",
        ROOM / "wall" / "painted_plaster_wall_arm_2k.jpg",
        specular=0.12, normal_strength=0.35, projection="BOX", scale=0.42,
    )
    linen = pbr(
        "Linen",
        ROOM / "linen" / "rough_linen_Diffuse_2k.jpg",
        ROOM / "linen" / "rough_linen_nor_gl_2k.jpg",
        ROOM / "linen" / "rough_linen_arm_2k.jpg",
        specular=0.16, normal_strength=0.7,
    )
    wool = pbr(
        "Wool",
        ROOM / "rug" / "poly_wool_herringbone_Diffuse_2k.jpg",
        ROOM / "rug" / "poly_wool_herringbone_nor_gl_2k.jpg",
        ROOM / "rug" / "poly_wool_herringbone_arm_2k.jpg",
        specular=0.12, normal_strength=0.85,
    )
    trim = cc.principled("Trim", (0.78, 0.76, 0.72), 0.34, specular=0.22)
    baseboard_mat = cc.principled("Baseboard", (0.70, 0.68, 0.64), 0.48, specular=0.18)
    rod_mat = cc.principled("Rod", (0.18, 0.16, 0.14), 0.32, metallic=0.85, specular=0.4)
    glass = cc.principled("Window glass", (0.94, 0.97, 0.96), 0.03, specular=0.5)
    glass_bsdf = glass.node_tree.nodes["Principled BSDF"]
    glass_bsdf.inputs["Transmission Weight"].default_value = 1.0
    glass_bsdf.inputs["IOR"].default_value = 1.45

    cc.mesh_obj("Floor", surface_mesh("Floor", 7.2, 6.4, 0.0, 1.15), floor_mat, collection)
    cc.mesh_obj("Rug", surface_mesh("Rug", 2.30, 1.85, 0.010, 0.42, thickness=0.008), wool, collection)

    put_box("Wall window", [
        ((wall_x, 0.0, win_z0 / 2), (thick, span_y, win_z0)),
        ((wall_x, 0.0, (win_z1 + ceil) / 2), (thick, span_y, ceil - win_z1)),
        ((wall_x, -(inner_y + win_y) / 2, (win_z0 + win_z1) / 2), (thick, inner_y - win_y, win_z1 - win_z0)),
        ((wall_x, (inner_y + win_y) / 2, (win_z0 + win_z1) / 2), (thick, inner_y - win_y, win_z1 - win_z0)),
    ], plaster, collection, uv=2.4)
    put_box("Wall side L", [(( (inner_x + back_x) / 2, -inner_y - thick / 2, ceil / 2), (span_x, thick, ceil))], plaster, collection, uv=2.4)
    put_box("Wall side R", [(( (inner_x + back_x) / 2, inner_y + thick / 2, ceil / 2), (span_x, thick, ceil))], plaster, collection, uv=2.4)
    put_box("Wall camera", [((back_x - thick / 2, 0.0, ceil / 2), (thick, span_y + thick * 2, ceil))], plaster, collection, uv=2.4)
    put_box("Ceiling", [(((inner_x + back_x) / 2, 0.0, ceil + 0.03), (span_x + thick, span_y + thick * 2, 0.06))], plaster, collection, uv=2.4)

    put_box("Baseboards", [
        ((inner_x - 0.012, 0.0, 0.055), (0.020, span_y, 0.10)),
        (((inner_x + back_x) / 2, -inner_y + 0.012, 0.055), (span_x, 0.020, 0.10)),
        (((inner_x + back_x) / 2, inner_y - 0.012, 0.055), (span_x, 0.020, 0.10)),
        ((back_x + 0.012, 0.0, 0.055), (0.020, span_y, 0.10)),
    ], baseboard_mat, collection, uv=0)

    frame_x = inner_x - 0.025
    opening_h = win_z1 - win_z0
    put_box("Window frame", [
        ((frame_x, 0.0, win_z0), (0.055, win_y * 2 + 0.08, 0.045)),
        ((frame_x, 0.0, win_z1), (0.055, win_y * 2 + 0.08, 0.045)),
        ((frame_x, -win_y, (win_z0 + win_z1) / 2), (0.055, 0.045, opening_h)),
        ((frame_x, win_y, (win_z0 + win_z1) / 2), (0.055, 0.045, opening_h)),
        ((frame_x - 0.004, 0.0, (win_z0 + win_z1) / 2), (0.028, 0.032, opening_h - 0.06)),
        ((frame_x - 0.004, 0.0, 1.48), (0.028, win_y * 2 - 0.04, 0.032)),
    ], trim, collection, uv=0)
    put_box("Sill", [((inner_x - 0.07, 0.0, win_z0 - 0.028), (0.16, win_y * 2 + 0.16, 0.028))], trim, collection, uv=0)
    put_box("Window glass", [((inner_x + 0.01, 0.0, (win_z0 + win_z1) / 2), (0.008, win_y * 2 - 0.06, opening_h - 0.06))], glass, collection, uv=0)

    for name, y0, y1 in (("Curtain L", -1.08, -0.58), ("Curtain R", 0.58, 1.08)):
        cc.mesh_obj(name, curtain_mesh(name, inner_x - 0.07, y0, y1, win_z0 + 0.02, win_z1 + 0.02), linen, collection)
    rod = bpy.data.meshes.new("Curtain rod")
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=18, radius1=0.011, radius2=0.011, depth=2.32)
    bmesh.ops.rotate(bm, verts=list(bm.verts), cent=(0.0, 0.0, 0.0), matrix=Matrix.Rotation(math.pi / 2.0, 4, "X"))
    bmesh.ops.translate(bm, verts=list(bm.verts), vec=(inner_x - 0.055, 0.0, win_z1 + 0.045))
    bm.to_mesh(rod)
    bm.free()
    cc.mesh_obj("Curtain rod", rod, rod_mat, collection)

    table_objs = import_gltf(ROOM / "dining_table" / "dining_table_2k.gltf")
    wood = next(obj for obj in table_objs if obj.name == "dining_table")
    # The scan's cloth is a separate sheet that sits above the wood.
    for obj in table_objs:
        if obj is not wood:
            bpy.data.objects.remove(obj, do_unlink=True)
    table_root = parent_new([wood], "Dining table", collection)
    table_root.scale = (table_sxy, table_sy, table_sz)
    bpy.context.view_layer.update()
    feet, center = mesh_zs(wood)
    table_root.location.z += 0.010 - min(feet)
    bpy.context.view_layer.update()
    feet, center = mesh_zs(wood)
    table_top = max(center) if center else max(feet)
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    ev = wood.evaluated_get(deps)
    span = [ev.matrix_world @ Vector(corner) for corner in ev.bound_box]
    table_half = min(abs(min(c.y for c in span)), abs(max(c.y for c in span)))
    print("table top", round(table_top, 4), "feet", round(min(feet), 4), "half", round(table_half, 3))

    chair_objs = import_gltf(ROOM / "dining_chair" / "dining_chair_02_2k.gltf")
    black_chair = parent_new(chair_objs, "Chair black", collection)
    white_chair = duplicate_tree(black_chair, "Chair white", collection)
    black_chair.location = (0.0, 0.50, 0.010)
    white_chair.location = (0.0, -0.50, 0.010)
    white_chair.rotation_euler.z = math.pi

    lamp_objs = import_gltf(ROOM / "lamp" / "modern_ceiling_lamp_01_1k.gltf")
    lamp_mesh = next(obj for obj in lamp_objs if obj.type == "MESH")
    lamp_root = parent_new(lamp_objs, "Pendant", collection)
    lamp_root.location = (0.0, 0.0, ceil - 0.02 - 1.173)
    bulb_mat = add_globe_glow(lamp_mesh)

    frame_objs = import_gltf(ROOM / "frame" / "hanging_picture_frame_01_1k.gltf")
    frame_root = parent_new(frame_objs, "Picture", collection)
    frame_root.location = (inner_x - 0.02, -1.20, 1.42)
    frame_root.rotation_euler.z = math.pi / 2.0
    bpy.context.view_layer.update()
    art = Vector()
    frame_mesh = next(obj for obj in frame_objs if obj.type == "MESH")
    for poly in frame_mesh.data.polygons:
        slot = frame_mesh.material_slots[poly.material_index]
        if slot.material and "artwork" in slot.material.name:
            art += frame_mesh.matrix_world.to_3x3() @ poly.normal
    if art.x > 0.0:
        frame_root.rotation_euler.z += math.pi
    clear_frame_glass(frame_mesh)
    hang_artwork(frame_mesh, ROOM / "art" / "kloofendal.jpg")
    print("picture facing", tuple(round(v, 3) for v in art))

    plant_objs = import_gltf(ROOM / "plant" / "pachira_aquatica_01_1k.gltf")
    for obj in plant_objs:
        if abs(obj.location.x + 2.0) < 0.3:
            obj.location = (1.22, 1.42, 0.02)
        else:
            obj.location.z = -8.0
            hide_obj(obj)

    ensure_side_textures()
    side_objs = import_gltf(TABLE_DIR / "WoodenTable_02.gltf")
    side = parent_new(side_objs, "Side table", collection)
    side.location = (1.40, -1.18, 0.0)
    side.rotation_euler.z = math.radians(12)
    vase_objs = import_gltf(ROOM / "vase" / "ceramic_vase_01_1k.gltf")
    vase = parent_new(vase_objs, "Vase", collection)
    vase.location = (1.40, -1.18, 0.418)
    vase.rotation_euler.z = math.radians(-12)

    aim = bpy.data.objects.new("Light aim", None)
    collection.objects.link(aim)
    aim.location = (0.0, 0.0, 0.95)
    aim.hide_render = True

    def area(name, location, size, size_y, energy, color):
        light = bpy.data.lights.new(name, "AREA")
        light.shape = "RECTANGLE"
        light.size = size
        light.size_y = size_y
        light.energy = energy
        light.color = color
        obj = bpy.data.objects.new(name + " light", light)
        obj.location = location
        collection.objects.link(obj)
        track = obj.constraints.new("TRACK_TO")
        track.target = aim
        track.track_axis = "TRACK_NEGATIVE_Z"
        track.up_axis = "UP_Y"
        # The window must show the HDRI, not a white rectangle of the lamp itself.
        obj.visible_camera = False
        obj.visible_glossy = False
        obj.visible_transmission = False
        return obj

    win_obj = area("Window", (inner_x + 0.42, 0.0, (win_z0 + win_z1) / 2), 1.25, 1.15, 80.0, (1.0, 0.72, 0.46))
    fill_obj = area("Fill", (-1.20, -0.10, 1.35), 1.6, 1.0, 8.0, (1.0, 0.86, 0.74))
    lamp = bpy.data.lights.new("Lamp", "AREA")
    lamp.size = 0.28
    lamp.energy = 0.0
    lamp.color = (1.0, 0.72, 0.46)
    lamp_obj = bpy.data.objects.new("Lamp light", lamp)
    lamp_obj.location = (0.0, 0.0, lamp_root.location.z + 0.42)
    lamp_obj.visible_camera = False
    lamp_obj.visible_glossy = False
    lamp_obj.visible_transmission = False
    collection.objects.link(lamp_obj)

    world = bpy.data.worlds.new("Room")
    world.use_nodes = True
    world.node_tree.nodes.clear()
    bg = world.node_tree.nodes.new("ShaderNodeBackground")
    bg.inputs["Strength"].default_value = 0.8
    env = world.node_tree.nodes.new("ShaderNodeTexEnvironment")
    env.image = load_img(ROOM / "hdri" / "belfast_sunset_2k.hdr")
    coord = world.node_tree.nodes.new("ShaderNodeTexCoord")
    mapping = world.node_tree.nodes.new("ShaderNodeMapping")
    mapping.inputs["Rotation"].default_value[0] = math.radians(14)
    mapping.inputs["Rotation"].default_value[2] = math.radians(100)
    wout = world.node_tree.nodes.new("ShaderNodeOutputWorld")
    world.node_tree.links.new(coord.outputs["Generated"], mapping.inputs["Vector"])
    world.node_tree.links.new(mapping.outputs["Vector"], env.inputs["Vector"])
    cap = world.node_tree.nodes.new("ShaderNodeVectorMath")
    cap.operation = "MINIMUM"
    cap.inputs[1].default_value = (4.0, 4.0, 4.0)
    world.node_tree.links.new(env.outputs["Color"], cap.inputs[0])
    world.node_tree.links.new(cap.outputs["Vector"], bg.inputs["Color"])
    world.node_tree.links.new(bg.outputs["Background"], wout.inputs["Surface"])
    bpy.context.scene.world = world
    return {
        "window": win_obj,
        "fill": fill_obj,
        "lamp": lamp_obj,
        "world": bg,
        "sky": None,
        "bulb": bulb_mat,
        "mapping": mapping,
        "table_top": table_top,
        "table_half": table_half,
    }


def raise_board(rig, top):
    anchor = bpy.data.objects.new("Board anchor", None)
    bpy.context.scene.collection.objects.link(anchor)
    for obj in (rig.chassis, rig.lid, rig.grid, rig.foil, rig.glass, rig.button):
        cc.parent_keep(obj, anchor)
    anchor.location = (0.0, 0.0, top)
    bpy.context.view_layer.update()
    rig.xs = board_mod().square_centers(board_mod().world_coords(rig.grid, 0))
    rig.ys = board_mod().square_centers(board_mod().world_coords(rig.grid, 1))
    if len(rig.xs) != 8 or len(rig.ys) != 8:
        raise RuntimeError(f"square detection failed {len(rig.xs)} {len(rig.ys)}")
    rig.surface = max(board_mod().world_coords(rig.glass, 2))
    print("board", "surface", round(rig.surface, 4), "a1", round(rig.xs[0], 4), round(rig.ys[0], 4))
    return rig


def board_mod():
    return sys.modules["build_scene"]


def square(rig, name, z):
    return Vector((rig.xs[FILES.index(name[0])], rig.ys[int(name[1]) - 1], z))


def between(src, dst):
    dx = FILES.index(dst[0]) - FILES.index(src[0])
    dy = int(dst[1]) - int(src[1])
    ax, ay = abs(dx), abs(dy)
    if ax == 0 and ay == 0:
        return []
    if ax != 0 and ay != 0 and ax != ay:
        return []
    steps = max(ax, ay)
    sx = 0 if dx == 0 else (1 if dx > 0 else -1)
    sy = 0 if dy == 0 else (1 if dy > 0 else -1)
    return [FILES[FILES.index(src[0]) + sx * i] + str(int(src[1]) + sy * i) for i in range(1, steps)]


def build_pieces(rig, collection):
    mats = cc.piece_materials()
    templates = cc.piece_templates(mats[0])
    pieces = {}
    home = {}
    back = ["rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook"]
    for color, rank, pawn_rank in (("w", 1, 2), ("b", 8, 7)):
        for i, kind in enumerate(back):
            sq = FILES[i] + str(rank)
            pid = f"{color}_{kind}_{FILES[i]}"
            loc = square(rig, sq, rig.surface)
            obj = cc.make_piece(templates, kind, color, pid, loc, mats, collection)
            obj["kind"] = kind
            pieces[pid] = obj
            home[pid] = sq
        for i in range(8):
            sq = FILES[i] + str(pawn_rank)
            pid = f"{color}_pawn_{FILES[i]}"
            loc = square(rig, sq, rig.surface)
            obj = cc.make_piece(templates, "pawn", color, pid, loc, mats, collection)
            obj["kind"] = "pawn"
            pieces[pid] = obj
            home[pid] = sq
    return pieces, home


PLAYERS_BLEND = ROOT / "set" / "people" / "players.blend"
HIP_Z = 0.56
# Wrist stays on the player's side of the piece. The fingers reach the rest of the way.
REACH = 0.108
SEAT = {
    "w": {"name": "White", "y": -0.52, "yaw": math.pi, "hand": "l", "approach": Vector((0.0, -1.0, 0.0)), "rest": Vector((-0.26, -0.36, 0.775))},
    "b": {"name": "Black", "y": 0.52, "yaw": 0.0, "hand": "r", "approach": Vector((0.0, 1.0, 0.0)), "rest": Vector((-0.26, 0.36, 0.775))},
}


def aim_bone(pose_bone, target):
    direction = Vector(target) - pose_bone.head
    if direction.length < 1e-5:
        return
    y_axis = direction.normalized()
    up = Vector((0.0, 0.0, 1.0))
    if abs(y_axis.dot(up)) > 0.9:
        up = Vector((1.0, 0.0, 0.0))
    x_axis = y_axis.cross(up).normalized()
    z_axis = x_axis.cross(y_axis).normalized()
    x_axis = y_axis.cross(z_axis).normalized()
    rotation = Matrix((x_axis, y_axis, z_axis)).transposed().to_4x4()
    pose_bone.matrix = Matrix.Translation(pose_bone.head) @ rotation
    bpy.context.view_layer.update()


def sit(arm, drop=0.42, reach=0.06):
    """Bend both legs so the knees point the way the character is facing."""
    for side, outward in (("l", 1.0), ("r", -1.0)):
        hip = arm.pose.bones[f"thigh_{side}"].head
        aim_bone(arm.pose.bones[f"thigh_{side}"], hip + Vector((outward * 0.05, -0.22, -0.08)))
        knee = arm.pose.bones[f"thigh_{side}"].tail
        aim_bone(arm.pose.bones[f"calf_{side}"], knee + Vector((outward * 0.02, -reach, -drop)))
        ankle = arm.pose.bones[f"calf_{side}"].tail
        aim_bone(arm.pose.bones[f"foot_{side}"], ankle + Vector((0.0, -0.16, 0.02)))


def rest_arm(arm, side):
    """Off hand folds back onto the thigh. Forward is armature -Y, so +Y is behind the body."""
    sign = -1.0 if side == "r" else 1.0
    shoulder = arm.pose.bones[f"upperarm_{side}"].head
    aim_bone(arm.pose.bones[f"upperarm_{side}"], shoulder + Vector((sign * 0.20, 0.04, -0.14)))
    elbow = arm.pose.bones[f"upperarm_{side}"].tail
    aim_bone(arm.pose.bones[f"lowerarm_{side}"], elbow + Vector((sign * 0.02, 0.14, -0.12)))
    wrist = arm.pose.bones[f"lowerarm_{side}"].tail
    aim_bone(arm.pose.bones[f"hand_{side}"], wrist + Vector((0.0, 0.02, -0.08)))


def load_players(collection, table_top, table_half):
    if not PLAYERS_BLEND.is_file():
        raise RuntimeError(f"missing {PLAYERS_BLEND}; run make_players.py")
    players = {}
    for who, spec in SEAT.items():
        directory = PLAYERS_BLEND.as_posix() + "/Collection/"
        bpy.ops.wm.append(directory=directory, filename=spec["name"])
        arm = bpy.data.objects[spec["name"]]
        arm.location = (0.0, spec["y"], 0.0)
        arm.rotation_euler = (0.0, 0.0, spec["yaw"])
        arm.hide_render = True
        sit(arm)
        other = "r" if spec["hand"] == "l" else "l"
        rest_arm(arm, other)
        bpy.context.view_layer.update()
        pelvis = arm.matrix_world @ arm.pose.bones["pelvis"].head
        arm.location.z += HIP_Z - pelvis.z
        bpy.context.view_layer.update()
        ankle_z = (arm.matrix_world @ arm.pose.bones["foot_l"].head).z
        if ankle_z < 0.015:
            sit(arm, drop=0.24, reach=0.20)
            rest_arm(arm, other)
            bpy.context.view_layer.update()
            pelvis = arm.matrix_world @ arm.pose.bones["pelvis"].head
            arm.location.z += HIP_Z - pelvis.z
            bpy.context.view_layer.update()
        # The ankle bone can sit above the floor while the shoe and the
        # trouser hem still cut through it.
        bpy.context.view_layer.update()
        deps = bpy.context.evaluated_depsgraph_get()
        lowest = 0.0
        for obj in arm.children_recursive:
            if obj.type != "MESH":
                continue
            ev = obj.evaluated_get(deps)
            mesh = ev.to_mesh()
            if mesh.vertices:
                lowest = min(lowest, min((ev.matrix_world @ vert.co).z for vert in mesh.vertices))
            ev.to_mesh_clear()
        if lowest < 0.004:
            arm.location.z += 0.004 - lowest
            bpy.context.view_layer.update()
            print(spec["name"], "floor lift", round(0.004 - lowest, 3))
        hands = {}
        for side_name in ("l", "r"):
            # Facing the board, the player's left is world -X for white and world +X for black.
            on_camera = (who == "w" and side_name == "l") or (who == "b" and side_name == "r")
            shoulder_bone = arm.matrix_world @ arm.pose.bones[f"upperarm_{side_name}"].head
            outward = -1.0 if on_camera else 1.0
            # Palm flat on the near margin. Fingers stay curled and stop
            # short of the pieces; the wrist stays just outside the edge.
            # Black's left palm is thicker and needs a little more air under it.
            # Clear of the tabletop. The open fingers hang below the wrist bone,
            # and a lower rest buried the pads in the wood.
            rest_z = table_top + (0.062 if who == "b" else 0.052)
            rest = Vector((shoulder_bone.x, math.copysign(table_half + 0.035, spec["y"]), rest_z))
            ik = bpy.data.objects.new(f"{spec['name']} ik {side_name}", None)
            collection.objects.link(ik)
            ik.hide_render = True
            ik.empty_display_size = 0.02
            forearm = arm.pose.bones[f"lowerarm_{side_name}"]
            constraint = forearm.constraints.new("IK")
            constraint.target = ik
            constraint.chain_count = 2
            constraint.use_stretch = False
            constraint.iterations = 500
            # Without a pole the far elbow flips up over the shoulder.
            pole = bpy.data.objects.new(f"{spec['name']} pole {side_name}", None)
            collection.objects.link(pole)
            pole.hide_render = True
            pole.empty_display_size = 0.02
            pole.location = shoulder_bone + Vector((outward * 0.32, math.copysign(0.10, spec["y"]), -0.16))
            constraint.pole_target = pole
            # Tuned so the elbow stays outside the ribs and below the shoulder.
            # The two arms are not mirrors, and white is yawed 180 degrees.
            if who == "w":
                constraint.pole_angle = math.radians(-90.0 if side_name == "l" else 180.0)
            else:
                constraint.pole_angle = math.radians(0.0 if side_name == "l" else -60.0)
            pinch = bpy.data.objects.new(f"{spec['name']} pinch {side_name}", None)
            collection.objects.link(pinch)
            pinch.hide_render = True
            pinch.empty_display_size = 0.015
            bone = arm.pose.bones[f"hand_{side_name}"]
            # Points the fingers at the piece only while gripping. At rest the
            # influence is 0, so the hand stays on the forearm and the wrist
            # does not twist.
            track = bone.constraints.new("DAMPED_TRACK")
            track.target = pinch
            # Palm faces the piece. Finger IK bends the tips onto the head.
            # Aiming the finger axis itself left the index pointing down.
            track.track_axis = "TRACK_Z"
            track.influence = 0.0
            roll = bpy.data.objects.new(f"{spec['name']} roll {side_name}", None)
            collection.objects.link(roll)
            roll.hide_render = True
            roll.empty_display_size = 0.012
            roll.location = pinch.location
            locked = bone.constraints.new("LOCKED_TRACK")
            locked.target = roll
            locked.track_axis = "TRACK_Y"
            locked.lock_axis = "LOCK_Z"
            locked.influence = 0.0
            tips = {}
            finger_ik = {}
            finger_poles = {}
            for finger in ("index", "middle", "thumb"):
                tip = bpy.data.objects.new(f"{spec['name']} {finger} {side_name}", None)
                collection.objects.link(tip)
                tip.hide_render = True
                tip.empty_display_size = 0.008
                tip.location = rest
                bend = bpy.data.objects.new(f"{spec['name']} {finger} pole {side_name}", None)
                collection.objects.link(bend)
                bend.hide_render = True
                bend.empty_display_size = 0.008
                bend.location = rest
                distal = arm.pose.bones[f"{finger}_03_{side_name}"]
                hold = distal.constraints.new("IK")
                hold.target = tip
                hold.chain_count = 3
                hold.use_stretch = False
                hold.iterations = 200
                hold.influence = 0.0
                for index in (1, 2, 3):
                    bone = arm.pose.bones[f"{finger}_{index:02d}_{side_name}"]
                    # Positive X closes into the palm. Negative X folds the
                    # finger out through the back of the hand.
                    bone.use_ik_limit_x = True
                    bone.ik_min_x = math.radians(-8.0)
                    bone.ik_max_x = math.radians(95.0)
                    bone.lock_ik_y = finger != "thumb"
                    bone.use_ik_limit_z = True
                    spread = 70.0 if finger == "thumb" else 30.0
                    bone.ik_min_z = math.radians(-spread)
                    bone.ik_max_z = math.radians(spread)
                tips[finger] = tip
                finger_ik[finger] = hold
                finger_poles[finger] = bend
            ik.location = rest
            pinch.location = Vector((rest.x, rest.y - spec["approach"].y * 0.055, rest.z))
            hands[side_name] = {
                "ik": ik,
                "pinch": pinch,
                "roll": roll,
                "locked": locked,
                "pole": pole,
                "rest": rest,
                "rest_aim": Vector(pinch.location),
                "tips": tips,
                "finger_ik": finger_ik,
                "finger_poles": finger_poles,
                "shoulder": Vector((outward, 0.0, 0.0)),
            }
        meshes = [obj for obj in arm.children_recursive if obj.type == "MESH"]
        look = bpy.data.objects.new(f"{spec['name']} look", None)
        collection.objects.link(look)
        look.hide_render = True
        look.empty_display_size = 0.03
        look.location = (0.0, 0.0, 0.96)
        for bone_name, influence in (("neck_01", 0.22), ("head", 0.62)):
            glance = arm.pose.bones[bone_name].constraints.new("DAMPED_TRACK")
            glance.target = look
            # The face is the head bone's local +Z once the player is seated.
            glance.track_axis = "TRACK_Z"
            glance.influence = influence
        # After the look constraint, so the cock is not cancelled by it.
        tilt = bpy.data.objects.new(f"{spec['name']} tilt", None)
        collection.objects.link(tilt)
        tilt.hide_render = True
        tilt.empty_display_size = 0.02
        tilt.rotation_mode = "XYZ"
        cock = arm.pose.bones["head"].constraints.new("COPY_ROTATION")
        cock.target = tilt
        cock.mix_mode = "AFTER"
        cock.use_x = False
        cock.use_y = False
        cock.use_z = True
        cock.target_space = "LOCAL"
        cock.owner_space = "LOCAL"
        players[who] = {
            "arm": arm,
            "hands": hands,
            "digits": meshes,
            "look": look,
            "tilt": tilt,
            "approach": spec["approach"],
            "name": spec["name"],
        }
        ankle = arm.matrix_world @ arm.pose.bones["foot_l"].head
        print(spec["name"], "pelvis", tuple(round(v, 3) for v in (arm.matrix_world @ arm.pose.bones["pelvis"].head)), "ankle", tuple(round(v, 3) for v in ankle), "meshes", len(meshes))
    return players


def seat_chairs(players):
    """Seat under the thigh, backrest behind the jacket. Feet stay on the floor."""
    bpy.context.view_layer.update()
    for who, chair_name in (("w", "Chair white"), ("b", "Chair black")):
        player = players[who]
        arm = player["arm"]
        chair = bpy.data.objects[chair_name]
        pelvis = arm.matrix_world @ arm.pose.bones["pelvis"].head
        spine = arm.matrix_world @ arm.pose.bones["spine_02"].head
        body = []
        for mesh in player["digits"]:
            verts, _polys = world_verts(mesh)
            body.extend(verts)
        near = [
            vert for vert in body
            if (vert.x - pelvis.x) ** 2 + (vert.y - pelvis.y) ** 2 < 0.16 ** 2 and vert.z < pelvis.z
        ]
        if len(near) < 20:
            print(chair_name, "no butt")
            continue
        near.sort(key=lambda vert: vert.z)
        butt = near[int(len(near) * 0.12)].z
        back = [vert for vert in body if abs(vert.z - spine.z) < 0.14 and abs(vert.x - spine.x) < 0.14]
        spine_back = (min(vert.y for vert in back) if who == "w" else max(vert.y for vert in back)) if back else pelvis.y

        def chair_verts():
            found = []
            for part in [chair, *chair.children_recursive]:
                if part.type != "MESH":
                    continue
                verts, _polys = world_verts(part)
                found.extend(verts)
            return found

        cv = chair_verts()
        z0, z1 = min(vert.z for vert in cv), max(vert.z for vert in cv)
        seat_cut = z0 + (z1 - z0) * 0.58
        seat_top = max(vert.z for vert in cv if vert.z <= seat_cut)
        want = butt - 0.004
        span = seat_top - chair.location.z
        if span > 0.05 and want > chair.location.z + 0.20:
            scale = min(1.0, max(0.70, (want - chair.location.z) / span))
            chair.scale = (scale, scale, scale)
            bpy.context.view_layer.update()
        cv = chair_verts()
        z0, z1 = min(vert.z for vert in cv), max(vert.z for vert in cv)
        seat_cut = z0 + (z1 - z0) * 0.58
        seat_top = max(vert.z for vert in cv if vert.z <= seat_cut)
        lift = want - seat_top
        if chair.location.z + lift < 0.0:
            lift = -chair.location.z
        chair.location.z += lift
        bpy.context.view_layer.update()
        cv = chair_verts()
        z0, z1 = min(vert.z for vert in cv), max(vert.z for vert in cv)
        seat_cut = z0 + (z1 - z0) * 0.58
        backrest = [vert for vert in cv if vert.z > seat_cut]
        if backrest:
            back_face = max(vert.y for vert in backrest) if who == "w" else min(vert.y for vert in backrest)
            target = spine_back - 0.03 if who == "w" else spine_back + 0.03
            chair.location.y += target - back_face
            bpy.context.view_layer.update()
        print(
            chair_name, "scale", round(chair.scale.z, 3),
            "butt", round(butt, 3), "seat", round(want, 3),
            "back", round(spine_back, 3), "y", round(chair.location.y, 3),
        )


# Degrees. The open hand is already a little bent. The pinch is thumb against
# index; the other fingers curl further so they stay behind the piece.
OPEN = {
    "index": [(40, 0, 0), (34, 0, 0), (20, 0, 0)],
    "middle": [(42, 0, 3), (36, 0, 0), (22, 0, 0)],
    "ring": [(46, 0, 6), (40, 0, 0), (24, 0, 0)],
    "pinky": [(50, 0, 8), (42, 0, 0), (26, 0, 0)],
    "thumb": [(18, 6, 10), (10, 3, 3), (6, 0, 0)],
}
# Palm down. Thumb, index and middle meet on the piece. The middle adducts
# toward the index so both pads land on the same stone. Ring and pinky stay
# in the palm.
PINCH = {
    "index": [(58, 0, 2), (64, 0, 0), (42, 0, 0)],
    "middle": [(60, 0, 5), (66, 0, 0), (44, 0, 0)],
    "ring": [(74, 0, 7), (76, 0, 0), (42, 0, 0)],
    "pinky": [(78, 0, 9), (78, 0, 0), (44, 0, 0)],
    "thumb": [(46, 16, 18), (24, 4, 4), (12, 0, 0)],
}
# Right-hand spread is not a mirror of the left. The thumb must not twist
# across the other fingers.
PINCH_R = {
    "index": [(58, 0, -2), (64, 0, 0), (42, 0, 0)],
    "middle": [(60, 0, -5), (66, 0, 0), (44, 0, 0)],
    "ring": [(74, 0, 7), (76, 0, 0), (42, 0, 0)],
    "pinky": [(78, 0, 9), (78, 0, 0), (44, 0, 0)],
    "thumb": [(44, -14, -16), (22, -4, -4), (12, 0, 0)],
}
# Same pinch. Each move changes tightness, a few degrees of thumb opposition, and height.
GRIP_VAR = (
    (1.02, -4, 1.85),
    (0.98, 5, 1.70),
    (1.06, -6, 1.95),
    (1.00, 3, 1.60),
    (1.00, -3, 0.85),
    (0.96, 6, 0.95),
    (0.98, 2, 1.15),
    (1.04, -5, 1.30),
    (1.00, 0, 1.75),
    (0.94, 4, 1.45),
    (1.02, -2, 1.55),
)


def playing_hand(who, src, dst, rig):
    """Hand on the same side as the piece being picked up, so the forearm does not cross the chest."""
    del dst
    left_side = square(rig, src, 0.0).x <= 0.0
    if who == "w":
        return "l" if left_side else "r"
    return "r" if left_side else "l"


def finger_pose(kind, scale, amount, thumb_extra=0.0, side="l"):
    tight = {"pawn": 1.0, "knight": 1.0, "bishop": 0.98, "rook": 1.0, "queen": 0.96, "king": 0.94}[kind]
    pose = {}
    closed_pose = PINCH_R if side == "r" else PINCH
    for finger, closed in closed_pose.items():
        pose[finger] = []
        for opened, shut in zip(OPEN[finger], closed):
            target = list(shut)
            target[0] = shut[0] * tight * scale
            if finger == "thumb":
                target[2] = shut[2] + thumb_extra
            pose[finger].append(tuple(
                opened[axis] + (target[axis] - opened[axis]) * amount
                for axis in range(3)
            ))
    return pose


def set_fingers(arm, hand, pose, frame):
    for finger, angles in pose.items():
        for index, angle in enumerate(angles, start=1):
            bone = arm.pose.bones[f"{finger}_{index:02d}_{hand}"]
            bone.rotation_mode = "XYZ"
            bone.rotation_euler = tuple(math.radians(v) for v in angle)
            bone.keyframe_insert("rotation_euler", frame=frame)



def lay_palms(players):
    """Turn each resting hand about the forearm until the palm faces the table.

    The twist stays in the hand's local rotation, so it follows the forearm
    instead of kinking the wrist. While gripping, the damped track bends the
    wrist down and keeps this pronation.
    """
    bpy.context.scene.frame_set(1)
    for player in players.values():
        for side in ("l", "r"):
            for constraint in player["arm"].pose.bones[f"hand_{side}"].constraints:
                if constraint.type == "DAMPED_TRACK":
                    constraint.influence = 0.0
    bpy.context.view_layer.update()
    for player in players.values():
        arm = player["arm"]
        for side in ("l", "r"):
            hand = arm.pose.bones[f"hand_{side}"]
            fore = arm.pose.bones[f"lowerarm_{side}"]
            forearm = ((arm.matrix_world @ fore.matrix).col[1]).xyz.normalized()
            knuckle = arm.matrix_world @ arm.pose.bones[f"middle_01_{side}"].head
            tip = arm.matrix_world @ arm.pose.bones[f"middle_03_{side}"].tail
            finger = ((arm.matrix_world @ hand.matrix).col[1]).xyz.normalized()
            bend = tip - knuckle
            bend = bend - finger * bend.dot(finger)
            if bend.length < 1e-4:
                continue
            palm = bend.normalized()
            palm_flat = palm - forearm * palm.dot(forearm)
            down = Vector((0.0, 0.0, -1.0))
            down_flat = down - forearm * down.dot(forearm)
            if palm_flat.length < 1e-4 or down_flat.length < 1e-4:
                continue
            palm_flat.normalize()
            down_flat.normalize()
            angle = math.atan2(palm_flat.cross(down_flat).dot(forearm), palm_flat.dot(down_flat))
            wrist = arm.matrix_world @ Vector(hand.head)
            turn = Matrix.Rotation(angle, 4, forearm)
            moved = Matrix.Translation(wrist) @ turn @ Matrix.Translation(-wrist)
            hand.rotation_mode = "XYZ"
            hand.matrix = arm.matrix_world.inverted() @ moved @ (arm.matrix_world @ hand.matrix)
            bpy.context.view_layer.update()
            hand.keyframe_insert("rotation_euler", frame=1)
            print(player["name"], side, "pronate", round(math.degrees(angle), 1))
    # Pronation faces the palm down but can leave the fingers diving into the
    # wood or pointing up. Pitch about the wrist, in the finger's own vertical
    # plane, until they lie just above the table. That is a bend, not a twist.
    bpy.context.view_layer.update()
    for player in players.values():
        arm = player["arm"]
        for side in ("l", "r"):
            hand = arm.pose.bones[f"hand_{side}"]
            finger = ((arm.matrix_world @ hand.matrix).col[1]).xyz
            flat = Vector((finger.x, finger.y, 0.0))
            if flat.length < 1e-4:
                continue
            # Slightly up. The open curl then brings the pads down onto the
            # wood. Pointing the bones down buried the fingers in the table.
            target = flat.normalized() * finger.length * 0.99 + Vector((0.0, 0.0, 0.12 * finger.length))
            axis = finger.cross(target)
            if axis.length < 1e-6:
                continue
            angle = max(-0.7, min(0.7, finger.angle(target)))
            if finger.cross(target).dot(axis) < 0.0:
                angle = -angle
            wrist = arm.matrix_world @ Vector(hand.head)
            turn = Matrix.Rotation(angle, 4, axis)
            moved = Matrix.Translation(wrist) @ turn @ Matrix.Translation(-wrist)
            hand.matrix = arm.matrix_world.inverted() @ moved @ (arm.matrix_world @ hand.matrix)
            bpy.context.view_layer.update()
            hand.keyframe_insert("rotation_euler", frame=1)
            print(player["name"], side, "pitch", round(math.degrees(angle), 1))
    # The stock hand is wider than a square. A modest scale keeps the five
    # fingers and lets the pads meet a pawn without covering its neighbours.
    for player in players.values():
        for side in ("l", "r"):
            hand = player["arm"].pose.bones[f"hand_{side}"]
            hand.scale = (0.80, 0.80, 0.80)
            hand.keyframe_insert("scale", frame=1)


def key_index(arm, side, frame, angles):
    for index, angle in enumerate(angles, start=1):
        bone = arm.pose.bones[f"index_{index:02d}_{side}"]
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = tuple(math.radians(v) for v in angle)
        bone.keyframe_insert("rotation_euler", frame=frame)


def key_spine(arm, frame, yaw, pitch):
    """Turn the chest a little. The lean itself is in the neck, so the shoulders stay put and the arms do not sink into the table."""
    for name, yaw_share, pitch_share in (("spine_02", 0.20, 0.0), ("spine_03", 1.0, 0.45)):
        bone = arm.pose.bones[name]
        bone.rotation_mode = "XYZ"
        bone.rotation_euler = (pitch * pitch_share, yaw * yaw_share, 0.0)
        bone.keyframe_insert("rotation_euler", frame=frame)
    neck = arm.pose.bones["neck_01"]
    neck.rotation_mode = "XYZ"
    neck.rotation_euler = (pitch * 1.15, yaw * 0.35, 0.0)
    neck.keyframe_insert("rotation_euler", frame=frame)


def key_roll(player, frame, roll):
    """Cock the head around the face axis, after the look constraint."""
    tilt = player["tilt"]
    tilt.rotation_euler = (0.0, 0.0, roll)
    tilt.keyframe_insert("rotation_euler", frame=frame)


def key_glance(player, frame, pos, head_inf, neck_inf):
    cc.key_loc(player["look"], frame, pos)
    for bone_name, influence in (("head", head_inf), ("neck_01", neck_inf)):
        glance = next(
            c for c in player["arm"].pose.bones[bone_name].constraints if c.type == "DAMPED_TRACK"
        )
        glance.influence = influence
        glance.keyframe_insert("influence", frame=frame)


def animate(players, pieces, home, rig):
    by_square = {sq: pid for pid, sq in home.items()}
    surface = rig.surface
    frame = 1
    marks = []
    for move_i, (who, pid, kind, src, dst) in enumerate(GAME):
        blocked = [sq for sq in between(src, dst) if sq in by_square and by_square[sq] != pid]
        # The rook of O-O passes over the king that just landed on g8. The lift clears him.
        if pid == "b_rook_h" and blocked == ["g8"]:
            blocked = []
        if blocked or src not in by_square or by_square[src] != pid or dst in by_square:
            raise RuntimeError(f"illegal {src}{dst} blocked={blocked}")
        player = players[who]
        grip, _radius, _top = SPECS[kind]
        scale, thumb_extra, height = GRIP_VAR[move_i]
        side_name = playing_hand(who, src, dst, rig)
        hand = player["hands"][side_name]
        other_name = "r" if side_name == "l" else "l"
        other = player["hands"][other_name]
        approach = player["approach"]
        contact_z = surface + grip * height
        high_z = contact_z + LIFT

        def wrist_at(sq, z, lift, clear=0.0):
            base = square(rig, sq, 0.0)
            contact = Vector((base.x, base.y, z))
            # Palm faces the head. Fingers stay level and curl down onto it.
            # Far enough back that the knuckles stay beside the head and the
            # fingers reach in, instead of draping through the crown.
            wrist = contact + approach * 0.10 + Vector((0.0, 0.0, 0.05 + lift + clear))
            aim = Vector(contact)
            # g5 is past a seated arm. Stop the wrist on the reachable sphere
            # instead of giving the solver a target it cannot hit.
            pose_arm = player["arm"]
            shoulder = pose_arm.matrix_world @ pose_arm.pose.bones[f"upperarm_{side_name}"].head
            arm_len = (
                pose_arm.pose.bones[f"upperarm_{side_name}"].length
                + pose_arm.pose.bones[f"lowerarm_{side_name}"].length
            )
            delta = wrist - shoulder
            if arm_len > 0.0 and delta.length > arm_len * 0.96:
                wrist = shoulder + delta.normalized() * arm_len * 0.96
            return wrist, aim

        # Lift off the wood, arc over the board with an open hand, close only
        # once the fingers are on the piece, carry, then open and arc back.
        lifted = hand["rest"] + Vector((0.0, 0.0, 0.09))
        lifted_aim = hand["rest_aim"] + Vector((0.0, 0.0, 0.07))
        keys = [
            (frame, hand["rest"], hand["rest_aim"], 0.0),
            (frame + 6, lifted, lifted_aim, 0.0),
            (frame + 12, *wrist_at(src, contact_z, 0.08), 0.0),
            (frame + 16, *wrist_at(src, contact_z, -0.005, 0.0), 0.4),
            (frame + 20, *wrist_at(src, contact_z, -0.005, 0.0), 1.0),
            (frame + 24, *wrist_at(src, high_z, 0.0), 1.0),
            (frame + 34, *wrist_at(dst, high_z, 0.0), 1.0),
            (frame + 38, *wrist_at(dst, contact_z, -0.005, 0.0), 1.0),
            (frame + 44, *wrist_at(dst, contact_z, 0.08), 0.0),
            (frame + 52, hand["rest"], hand["rest_aim"], 0.0),
        ]
        # Bone sits just outside the head. The pad, not the bone, meets the stone.
        radius = SPECS[kind][1] + 0.007
        head = grip * height
        for at, wrist, aim, amount in keys:
            cc.key_loc(hand["ik"], at, wrist)
            # Palm straight down, fingers level toward the piece.
            cc.key_loc(hand["pinch"], at, Vector((wrist.x, wrist.y, wrist.z - 0.08)))
            cc.key_loc(hand["roll"], at, Vector((aim.x, aim.y, wrist.z)))
            set_fingers(player["arm"], side_name, finger_pose(kind, scale, amount, thumb_extra, side_name), at)
            cc.key_loc(other["ik"], at, other["rest"])
            cc.key_loc(other["pinch"], at, other["rest_aim"])
            set_fingers(player["arm"], other_name, finger_pose(kind, 1.0, 0.0, 0.0, other_name), at)
            on_src = at < frame + 28
            sq = src if on_src else dst
            piece_z = surface + head + (LIFT if frame + 22 <= at <= frame + 36 else 0.0)
            base = square(rig, sq, piece_z)
            spots = {
                "index": base + Vector((radius, 0.0, 0.0)),
                "thumb": base + Vector((-radius, 0.0, 0.0)),
                "middle": base + approach * radius,
            }
            for finger, loc in spots.items():
                cc.key_loc(hand["tips"][finger], at, loc)
                # Above the pad, so the knuckle stays on the back of the finger.
                cc.key_loc(hand["finger_poles"][finger], at, loc + Vector((0.0, 0.0, 0.04)))
                hold = hand["finger_ik"][finger]
                hold.influence = 1.0 if amount >= 0.4 else 0.0
                hold.keyframe_insert("influence", frame=at)
                other["finger_ik"][finger].influence = 0.0
                other["finger_ik"][finger].keyframe_insert("influence", frame=at)
        track = next(c for c in player["arm"].pose.bones[f"hand_{side_name}"].constraints if c.type == "DAMPED_TRACK")
        for at, influence in (
            (frame, 0.0), (frame + 8, 0.0), (frame + 12, 1.0),
            (frame + 40, 1.0), (frame + 46, 0.0), (frame + 52, 0.0),
        ):
            track.influence = influence
            track.keyframe_insert("influence", frame=at)
            hand["locked"].influence = influence
            hand["locked"].keyframe_insert("influence", frame=at)
        src_pos = square(rig, src, surface)
        dst_pos = square(rig, dst, surface)
        piece = pieces[pid]
        cc.key_loc(piece, frame, src_pos)
        cc.key_loc(piece, frame + 20, src_pos)
        cc.key_loc(piece, frame + 24, Vector((src_pos.x, src_pos.y, surface + LIFT)))
        cc.key_loc(piece, frame + 34, Vector((dst_pos.x, dst_pos.y, surface + LIFT)))
        cc.key_loc(piece, frame + 38, dst_pos)
        crown = surface + max(_top * 0.75, 0.035)
        src_look = Vector((src_pos.x, src_pos.y, crown))
        air_src = Vector((src_pos.x, src_pos.y, surface + LIFT + 0.03))
        mid = (src_pos + dst_pos) * 0.5
        air_mid = Vector((mid.x, mid.y, surface + LIFT + 0.04))
        dst_look = Vector((dst_pos.x, dst_pos.y, crown))
        nod = Vector((dst_pos.x, dst_pos.y, crown - 0.05))
        center = Vector((0.0, 0.0, 0.90))
        watcher_id = "b" if who == "w" else "w"
        watcher = players[watcher_id]

        def yaw_of(owner, x):
            # Positive local Y turns the chest toward that player's left.
            toward = -x if owner == "w" else x
            return max(-0.12, min(0.12, toward * 1.2))

        wing = 0.17 if abs(src_pos.x) >= 0.06 else 0.13
        mover_keys = (
            (frame, center, 0.28, 0.08, 0.0, 0.0, 0.0),
            (frame + 4, center, 0.32, 0.08, 0.0, -0.04, 0.0),
            (frame + 12, src_look, 0.92, 0.18, yaw_of(who, src_pos.x), wing, yaw_of(who, src_pos.x) * 0.85),
            (frame + 22, air_src, 0.94, 0.16, yaw_of(who, src_pos.x), wing, yaw_of(who, src_pos.x) * 0.85),
            (frame + 30, air_mid, 0.94, 0.16, yaw_of(who, mid.x), wing * 0.75, yaw_of(who, mid.x) * 0.70),
            (frame + 38, dst_look, 0.90, 0.14, yaw_of(who, dst_pos.x), wing * 0.40, yaw_of(who, dst_pos.x) * 0.55),
            (frame + 43, nod, 0.90, 0.20, yaw_of(who, dst_pos.x) * 0.5, wing * 0.22, yaw_of(who, dst_pos.x) * 0.20),
            (frame + 52, center, 0.28, 0.08, 0.0, 0.0, 0.0),
        )
        for at, pos, head_inf, neck_inf, yaw, pitch, roll in mover_keys:
            key_glance(player, at, pos, head_inf, neck_inf)
            key_spine(player["arm"], at, yaw, pitch)
            key_roll(player, at, roll)
        # He answers a few frames later, and he does not rock back.
        for at, pos, head_inf, neck_inf, yaw, pitch, roll in mover_keys:
            when = min(frame + 52, at + 6)
            key_glance(watcher, when, pos, max(0.22, head_inf * 0.62), max(0.06, neck_inf * 0.45))
            key_spine(watcher["arm"], when, yaw * 0.45, max(0.0, pitch) * 0.35)
            key_roll(watcher, when, roll * 0.6)
        # Index taps the wood. The wrist lifts with it so the gesture reads
        # from the wide camera; the other fingers stay in the resting curl.
        # Knuckle up, tip curled back, so it reads as a tap and not as pointing at a piece.
        tap_up = ((58, 0, 2), (40, 0, 0), (22, 0, 0))
        tap_down = ((46, 0, 0), (52, 0, 0), (34, 0, 0))
        idle = OPEN["index"]
        tap_side = "l" if watcher_id == "w" else "r"
        tap_hand = watcher["hands"][tap_side]
        for at, pose, lift in (
            (frame + 13, idle, 0.0),
            (frame + 17, tap_up, 0.05),
            (frame + 21, tap_down, 0.0),
            (frame + 26, tap_up, 0.04),
            (frame + 30, tap_down, 0.0),
            (frame + 42, idle, 0.0),
        ):
            key_index(watcher["arm"], tap_side, at, pose)
            raised = Vector((0.0, 0.0, lift))
            cc.key_loc(tap_hand["ik"], at, tap_hand["rest"] + raised)
            cc.key_loc(tap_hand["pinch"], at, tap_hand["rest_aim"] + raised)
        marks.append({
            "move": f"{src}{dst}", "who": who, "pid": pid, "hand": side_name, "src": src,
            "start": frame, "grip": frame + 20, "high": frame + 28, "kind": kind,
            "grip_h": grip * height,
        })
        del by_square[src]
        by_square[dst] = pid
        frame += 52
    for piece in pieces.values():
        if piece.animation_data:
            cc.set_interpolation(piece, "LINEAR", "location")
    # Clamped bezier: the wrist arcs between the keys and does not dip
    # through the table or overshoot the pinch.
    posed = []
    for player in players.values():
        posed.append(player["arm"])
        posed.append(player["look"])
        posed.append(player["tilt"])
        for hand in player["hands"].values():
            posed.append(hand["ik"])
            posed.append(hand["pinch"])
    for obj in posed:
        data = obj.animation_data
        if data is None or data.action is None:
            continue
        slot = getattr(data, "action_slot", None)
        for curve in cc.fcurves_of(data.action, slot):
            if not (
                curve.data_path == "location"
                or curve.data_path.endswith("rotation_euler")
                or curve.data_path.endswith("influence")
            ):
                continue
            for key in curve.keyframe_points:
                key.interpolation = "BEZIER"
                key.handle_left_type = "AUTO_CLAMPED"
                key.handle_right_type = "AUTO_CLAMPED"
            curve.keyframe_points.update()
    # The piece travels in a straight line. The fingers have to use that same
    # line, or they slide through the stone while it is in the air.
    hold_frames = set()
    for mark in marks:
        start = mark["start"]
        hold_frames.update((start + 20, start + 24, start + 34))
    for player in players.values():
        for hand in player["hands"].values():
            targets = [
                hand["ik"], hand["pinch"], hand["roll"],
                *hand["tips"].values(), *hand["finger_poles"].values(),
            ]
            for obj in targets:
                data = obj.animation_data
                if data is None or data.action is None:
                    continue
                slot = getattr(data, "action_slot", None)
                for curve in cc.fcurves_of(data.action, slot):
                    if curve.data_path != "location":
                        continue
                    for key in curve.keyframe_points:
                        if int(round(key.co[0])) in hold_frames:
                            key.interpolation = "LINEAR"
                    curve.keyframe_points.update()
    return marks, frame


def key_light(light, frame, energy, color):
    light.data.energy = energy
    light.data.color = color
    light.data.keyframe_insert("energy", frame=frame)
    light.data.keyframe_insert("color", frame=frame)


def light_chapters(room, marks):
    # The whole game stays at night. The afternoon and dusk keys are gone,
    # so the lamp, the dark window and the board LEDs do not crossfade.
    del marks
    dusk = night = 1
    chapters = [
        # Lamp stays warm but softer so the square LEDs stay readable.
        (1, 0.7, (0.28, 0.34, 0.50), 0.9, 32.0, math.radians(-10), 0.012),
    ]
    bg = room["world"]
    sky_node = room["sky"]
    for frame, window, color, fill, lamp, elevation, strength in chapters:
        key_light(room["window"], frame, window, color)
        key_light(room["fill"], frame, fill, (1.0, 0.84, 0.70))
        key_light(room["lamp"], frame, lamp, (1.0, 0.70, 0.42))
        bg.inputs["Strength"].default_value = strength
        bg.inputs["Strength"].keyframe_insert("default_value", frame=frame)
        if sky_node is not None:
            try:
                sky_node.sun_elevation = elevation
                sky_node.keyframe_insert("sun_elevation", frame=frame)
            except (TypeError, RuntimeError, AttributeError):
                pass
    tree = bpy.context.scene.world.node_tree
    for light in (room["window"], room["fill"], room["lamp"]):
        cc.set_interpolation(light.data, "CONSTANT")
    cc.set_interpolation(tree, "CONSTANT")
    # the bulb only reads once the lamp is on
    bulb = room["bulb"].node_tree.nodes["Emission"]
    bulb.inputs["Strength"].default_value = 5.0
    bulb.inputs["Strength"].keyframe_insert("default_value", frame=1)
    cc.set_interpolation(room["bulb"].node_tree, "CONSTANT")
    # Horizon stays level. The disk sits where the night grade was tuned.
    rotation = room["mapping"].inputs["Rotation"]
    rotation.default_value[0] = math.radians(14)
    rotation.default_value[2] = math.radians(100)
    rotation.keyframe_insert("default_value", frame=1)
    cc.set_interpolation(bpy.context.scene.world.node_tree, "CONSTANT")
    return dusk, night


def build_camera(collection, rig, marks):
    focus = bpy.data.objects.new("Focus", None)
    collection.objects.link(focus)
    focus.empty_display_size = 0.03
    data = bpy.data.cameras.new("Camera")
    data.lens = 32
    data.dof.use_dof = True
    data.dof.focus_object = focus
    data.dof.aperture_fstop = 8.0
    data.clip_start = 0.05
    cam = bpy.data.objects.new("Camera", data)
    collection.objects.link(cam)
    track = cam.constraints.new("TRACK_TO")
    track.target = focus
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    # One locked wide shot. No punch-in on the pawn.
    del rig, marks
    cc.key_loc(cam, 1, Vector((-1.78, 0.0, 1.36)))
    cc.key_loc(focus, 1, Vector((0.0, 0.0, 1.06)))
    data.keyframe_insert("lens", frame=1)
    return cam


def cycles_settings(scene, res, samples):
    scene.render.engine = "CYCLES"
    scene.render.fps = FPS
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    # Viewport render must stay light. 1024 preview samples freeze the window.
    scene.cycles.preview_samples = 16
    scene.cycles.use_preview_denoising = False
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = "OPENIMAGEDENOISE"
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.adaptive_threshold = 0.01 if samples <= 32 else 0.006
    scene.cycles.max_bounces = 8
    scene.cycles.diffuse_bounces = 3
    scene.cycles.glossy_bounces = 3
    scene.cycles.transmission_bounces = 6
    scene.cycles.sample_clamp_indirect = 8.0
    scene.cycles.film_exposure = 1.0
    scene.view_settings.view_transform = "AgX"
    try:
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    prefs = bpy.context.preferences.addons["cycles"].preferences
    scene.cycles.device = "CPU"
    for dtype in ("OPTIX", "CUDA", "HIP", "METAL", "ONEAPI"):
        try:
            prefs.compute_device_type = dtype
        except TypeError:
            continue
        prefs.get_devices()
        gpus = [d for d in prefs.devices if d.type != "CPU"]
        if gpus:
            for device in prefs.devices:
                device.use = device.type != "CPU"
            scene.cycles.device = "GPU"
            print("cycles", dtype, [d.name for d in gpus])
            break


def comfort_viewport(scene):
    """Frame the room and drop the helper empties out of the view.

    A fresh file opens eighteen metres from the floor, so orbit and zoom
    barely move the board. The ik targets also draw lines on every redraw.
    """
    # From the camera side, horizon level. The previous quaternion rolled
    # the floor so the ground plane sat crooked in the window.
    eye = Vector((-2.6, -0.35, 1.50))
    target = Vector((0.0, 0.0, 0.85))
    rotation = (target - eye).to_track_quat("-Z", "Y")
    hidden = (" ik ", " pole ", " pinch ", " roll ", " look", " tilt", " index ", " middle ", " thumb ")
    for obj in scene.objects:
        if obj.type == "EMPTY" and any(token in obj.name for token in hidden):
            obj.hide_viewport = True
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type != "VIEW_3D":
                continue
            space = area.spaces.active
            space.shading.type = "SOLID"
            space.shading.light = "STUDIO"
            space.shading.color_type = "MATERIAL"
            space.clip_start = 0.01
            space.clip_end = 80.0
            space.overlay.show_relationship_lines = False
            region = space.region_3d
            region.view_perspective = "PERSP"
            region.view_location = target
            region.view_distance = (target - eye).length
            region.view_rotation = rotation


def bone_tail(arm, name):
    return arm.matrix_world @ Vector(arm.pose.bones[name].tail)


def pinch_report(marks, pieces, players):
    for mark in marks:
        bpy.context.scene.frame_set(mark["grip"])
        bpy.context.view_layer.update()
        piece = pieces[mark["pid"]]
        player = players[mark["who"]]
        side = mark["hand"]
        origin = piece.matrix_world.translation
        index = bone_tail(player["arm"], f"index_03_{side}")
        thumb = bone_tail(player["arm"], f"thumb_03_{side}")
        middle = bone_tail(player["arm"], f"middle_03_{side}")
        mid = (index + thumb) * 0.5
        radius = SPECS[mark["kind"]][1]
        off = math.hypot(mid.x - origin.x, mid.y - origin.y)
        mid_off = math.hypot(middle.x - origin.x, middle.y - origin.y)
        wrist = player["hands"][side]["ik"].matrix_world.translation
        forearm = bone_tail(player["arm"], f"lowerarm_{side}")
        delta = mid - origin
        arm = player["arm"]
        shoulder = arm.matrix_world @ Vector(arm.pose.bones[f"upperarm_{side}"].head)
        elbow = arm.matrix_world @ Vector(arm.pose.bones[f"upperarm_{side}"].tail)
        span = wrist - shoulder
        if span.length_squared > 1e-8:
            on_line = shoulder + span * (span.dot(elbow - shoulder) / span.length_squared)
        else:
            on_line = shoulder
        # Positive means the elbow stays outside the shoulder, away from the chest.
        outer = (elbow - on_line).dot(player["hands"][side]["shoulder"])
        reach = span.length / max((elbow - shoulder).length + (forearm - elbow).length, 1e-4)
        print(
            mark["move"], side,
            "dxy", round(delta.x, 3), round(delta.y, 3),
            "gap", round((index - thumb).length, 3),
            "z", round(delta.z, 3),
            "want", round(mark["grip_h"], 3),
            "gz", round(index.z - thumb.z, 3),
            "middle", round(mid_off, 3),
            "ik", round((forearm - wrist).length, 4),
            "elbow", round(outer, 3),
            "reach", round(reach, 2),
        )


def grip_stills(scene, marks, pieces, players):
    """Close stills of the pinch. A temporary lamp is not part of the film."""
    light = bpy.data.lights.new("Grip fill", "AREA")
    light.energy = 36
    light.size = 0.5
    light.color = (1.0, 0.93, 0.84)
    lamp = bpy.data.objects.new("Grip fill", light)
    scene.collection.objects.link(lamp)
    lamp.visible_camera = False
    lamp.visible_glossy = False
    lamp.visible_transmission = False
    data = bpy.data.cameras.new("Grip cam")
    data.lens = 50
    data.clip_start = 0.01
    data.dof.use_dof = False
    cam = bpy.data.objects.new("Grip cam", data)
    scene.collection.objects.link(cam)
    focus = bpy.data.objects.new("Grip focus", None)
    scene.collection.objects.link(focus)
    track = cam.constraints.new("TRACK_TO")
    track.target = focus
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.camera = cam
    scene.view_settings.exposure = -1.6
    wanted = {
        "d2d4": ("front",),
        "b1c3": ("front", "lift"),
        "e2e3": ("front",),
        "e8g8": ("front",),
    }
    for mark in marks:
        shots = wanted.get(mark["move"])
        if not shots:
            continue
        start = mark["start"]
        frames = {"front": start + 14, "lift": start + 19}
        for shot in shots:
            scene.frame_set(frames[shot])
            bpy.context.view_layer.update()
            piece = pieces[mark["pid"]].matrix_world.translation
            approach = players[mark["who"]]["approach"]
            shoulder = players[mark["who"]]["hands"][mark["hand"]]["shoulder"]
            # Across the board from the player, so the fingertips face the lens
            # and the camera is not inside the torso.
            cam.location = piece - approach * 0.18 + shoulder * 0.07 + Vector((0.0, 0.0, 0.10))
            data.lens = 58
            focus.location = piece + Vector((0.0, 0.0, 0.025))
            lamp.location = cam.location + Vector((0.0, 0.0, 0.08))
            lamp.rotation_euler = (focus.location - lamp.location).to_track_quat("-Z", "Y").to_euler()
            path = cc.VIDEO_DIR / f"hraci_k_{mark['move']}_{shot}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", mark["move"], shot, path)


def world_verts(obj):
    deps = bpy.context.evaluated_depsgraph_get()
    ev = obj.evaluated_get(deps)
    mesh = ev.to_mesh()
    mesh.transform(ev.matrix_world)
    verts = [v.co.copy() for v in mesh.vertices]
    polys = [tuple(p.vertices) for p in mesh.polygons]
    ev.to_mesh_clear()
    return verts, polys


def inside_mesh(bvh, co):
    direction = Vector((1.0, 0.0, 0.0))
    origin = Vector(co)
    hits = 0
    for _ in range(8):
        hit, _normal, _index, dist = bvh.ray_cast(origin, direction)
        if hit is None:
            break
        hits += 1
        origin = hit + direction * 0.002
    return hits % 2 == 1


def collide_report(players, rig, marks):
    """How far bodies cut the table at the rest pose."""
    from mathutils.bvhtree import BVHTree

    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    table = bpy.data.objects["dining_table"]
    tverts, tpolys = world_verts(table)
    tbvh = BVHTree.FromPolygons(tverts, tpolys)
    ys = [v.y for v in tverts]
    top = max(v.z for v in tverts)
    print("table y", round(min(ys), 3), round(max(ys), 3), "top", round(top, 3))
    bands = ((0.0, 0.40, "shin"), (0.40, 0.72, "lap"), (0.72, 0.92, "waist"), (0.92, 1.40, "chest"))
    bone_names = (
        "hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "upperarm_l", "upperarm_r",
        "thigh_l", "thigh_r", "calf_l", "calf_r", "pelvis", "spine_01", "spine_02", "spine_03",
    )
    for who, player in players.items():
        arm = player["arm"]
        heads = {name: arm.matrix_world @ arm.pose.bones[name].head for name in bone_names}
        edge = min(ys) if who == "w" else max(ys)
        front = {label: None for _a, _b, label in bands}
        counts = {}
        for mesh in player["digits"]:
            verts, polys = world_verts(mesh)
            if not verts:
                continue
            for vert in verts:
                for lo, hi, label in bands:
                    if lo <= vert.z < hi and (front[label] is None or abs(vert.y) < abs(front[label])):
                        front[label] = vert.y
            if not polys:
                continue
            bvh = BVHTree.FromPolygons(verts, polys)
            for _table_i, body_i in tbvh.overlap(bvh):
                for index in polys[body_i]:
                    co = verts[index]
                    bone = min(heads, key=lambda name: (heads[name] - co).length)
                    counts[bone] = counts.get(bone, 0) + 1
        bits = " ".join(f"{label} {front[label]:.3f}" for _a, _b, label in bands if front[label] is not None)
        print(player["name"], "edge", round(edge, 3), bits)
        arm_bones = (
            "hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "upperarm_l", "upperarm_r",
            "thumb_01_l", "thumb_01_r", "index_01_l", "index_01_r",
        )
        arm_heads = {name: heads[name] for name in arm_bones if name in heads}
        stomach = None
        stomach_bone = None
        for mesh in player["digits"]:
            verts, _polys = world_verts(mesh)
            for vert in verts:
                if not 0.78 <= vert.z <= 1.05:
                    continue
                bone = min(heads, key=lambda name: (heads[name] - vert).length)
                if bone in arm_heads or bone.startswith(("hand", "thumb", "index", "middle", "ring", "pinky", "lowerarm", "upperarm")):
                    continue
                if stomach is None or abs(vert.y) < abs(stomach):
                    stomach = vert.y
                    stomach_bone = bone
        upper = arm.pose.bones["upperarm_r"].length
        lower = arm.pose.bones["lowerarm_r"].length
        print(
            player["name"], "stomach", round(stomach, 3) if stomach is not None else None,
            stomach_bone, "arm", round(upper + lower, 3),
        )
        if counts:
            ordered = sorted(counts.items(), key=lambda item: -item[1])
            print(player["name"], "table bones", " ".join(f"{name}:{count}" for name, count in ordered[:6]))
        for side in ("l", "r"):
            elbow = arm.matrix_world @ Vector(arm.pose.bones[f"upperarm_{side}"].tail)
            wrist = player["hands"][side]["ik"].matrix_world.translation
            aim = player["hands"][side]["pinch"].matrix_world.translation
            tips = []
            for finger in ("thumb_03", "index_03", "middle_03", "ring_03", "pinky_03"):
                tip = arm.matrix_world @ Vector(arm.pose.bones[f"{finger}_{side}"].tail)
                tips.append(f"{finger[0]}{tip.z:.2f}/{tip.y:.2f}")
            print(player["name"], side, "wristz", round(wrist.z, 3), "tips", " ".join(tips))
    for chair_name in ("Chair black", "Chair white"):
        chair = bpy.data.objects[chair_name]
        who = "b" if "black" in chair_name else "w"
        player = players[who]
        arm = player["arm"]
        heads = {name: arm.matrix_world @ arm.pose.bones[name].head for name in bone_names}
        counts = {}
        cy, cz = [], []
        for part in [chair] + list(chair.children_recursive):
            if part.type != "MESH":
                continue
            cv, cp = world_verts(part)
            cy.extend(v.y for v in cv)
            cz.extend(v.z for v in cv)
            if not cp:
                continue
            cbvh = BVHTree.FromPolygons(cv, cp)
            for mesh in player["digits"]:
                verts, polys = world_verts(mesh)
                if not polys:
                    continue
                bbvh = BVHTree.FromPolygons(verts, polys)
                for _ci, body_i in cbvh.overlap(bbvh):
                    for index in polys[body_i]:
                        co = verts[index]
                        bone = min(heads, key=lambda name: (heads[name] - co).length)
                        counts[bone] = counts.get(bone, 0) + 1
        if cy:
            print(chair_name, "mesh y", round(min(cy), 3), round(max(cy), 3), "z", round(min(cz), 3), round(max(cz), 3))
        if counts:
            ordered = sorted(counts.items(), key=lambda item: -item[1])
            print(chair_name, "bones", " ".join(f"{name}:{count}" for name, count in ordered[:6]))
    seen = []
    for who, player in players.items():
        arm = player["arm"]
        heads = {name: arm.matrix_world @ arm.pose.bones[name].head for name in bone_names}
        tip = arm.matrix_world @ Vector(arm.pose.bones["index_03_r"].tail)
        for mesh in player["digits"]:
            verts, polys = world_verts(mesh)
            if not verts:
                continue
            nearest = min(verts, key=lambda vert: (vert - tip).length)
            if (nearest - tip).length < 0.04:
                print(player["name"], mesh.name[-24:], "follows index", round((nearest - tip).length, 3), "z", round(nearest.z, 3))
            if not polys:
                continue
            bvh = BVHTree.FromPolygons(verts, polys)
            for _table_i, body_i in tbvh.overlap(bvh):
                for index in polys[body_i]:
                    seen.append((who, mesh.name[-24:], verts[index]))
    if not seen:
        print("hands clear")
    else:
        for who in ("w", "b"):
            pts = [co for owner, _mesh_name, co in seen if owner == who]
            names = sorted({mesh_name for owner, mesh_name, _co in seen if owner == who})
            print(who, "meshes", " ".join(names))
            if not pts:
                continue
            print(
                who, "hit", len(pts),
                "x", round(min(p.x for p in pts), 3), round(max(p.x for p in pts), 3),
                "y", round(min(p.y for p in pts), 3), round(max(p.y for p in pts), 3),
                "z", round(min(p.z for p in pts), 3), round(max(p.z for p in pts), 3),
            )
    board_verts = []
    board_polys = []
    for part in (rig.chassis, rig.glass, rig.lid):
        verts, polys = world_verts(part)
        offset = len(board_verts)
        board_verts.extend(verts)
        board_polys.extend(tuple(index + offset for index in poly) for poly in polys)
    board_bvh = BVHTree.FromPolygons(board_verts, board_polys)
    bpy.context.scene.frame_set(1)
    bpy.context.view_layer.update()
    rest_board = 0
    for player in players.values():
        for mesh in player["digits"]:
            verts, polys = world_verts(mesh)
            if polys:
                rest_board += len(board_bvh.overlap(BVHTree.FromPolygons(verts, polys)))
    print("rest board", rest_board)
    for mark in marks:
        for label, frame in (("approach", mark["start"] + 8), ("grip", mark["grip"]), ("high", mark["start"] + 29)):
            bpy.context.scene.frame_set(frame)
            bpy.context.view_layer.update()
            table_n = board_n = 0
            for player in players.values():
                for mesh in player["digits"]:
                    verts, polys = world_verts(mesh)
                    if not polys:
                        continue
                    bvh = BVHTree.FromPolygons(verts, polys)
                    table_n += len(tbvh.overlap(bvh))
                    board_n += len(board_bvh.overlap(bvh))
            if table_n or board_n:
                print("move", mark["move"], label, "table", table_n, "board", board_n)
            if label != "grip":
                continue
            piece_n = []
            for obj in bpy.data.objects:
                if "kind" not in obj.keys() or obj.type != "MESH":
                    continue
                pv, pp = world_verts(obj)
                if not pp:
                    continue
                pbvh = BVHTree.FromPolygons(pv, pp)
                hits = 0
                for player in players.values():
                    for mesh in player["digits"]:
                        verts, polys = world_verts(mesh)
                        if not polys:
                            continue
                        hits += len(pbvh.overlap(BVHTree.FromPolygons(verts, polys)))
                if hits:
                    piece_n.append(f"{obj.name}:{hits}")
            if piece_n:
                print("move", mark["move"], "pieces", " ".join(piece_n))


def motion_sweep(scene, players, pieces):
    """Every second frame. Print only poses that fold, miss the hand target, or enter the table."""
    table_top = 0.755
    fails = []
    for frame in range(1, int(scene.frame_end) + 1, 2):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for who, player in players.items():
            arm = player["arm"]
            head = arm.pose.bones["head"]
            face = ((arm.matrix_world @ head.matrix).col[2]).xyz.normalized()
            up = ((arm.matrix_world @ head.matrix).col[1]).xyz.normalized()
            if up.z < 0.35:
                fails.append(f"f {frame} {who} head up {up.z:.2f}")
            forward = face.y if who == "w" else -face.y
            if forward < 0.15:
                fails.append(f"f {frame} {who} face back {tuple(round(v, 2) for v in face)}")
            origin = arm.matrix_world @ head.head
            look = player["look"].matrix_world.translation - origin
            track = next(c for c in head.constraints if c.type == "DAMPED_TRACK")
            if look.length and track.influence > 0.75:
                ang = math.degrees(face.angle(look.normalized()))
                if ang > 20.0:
                    fails.append(f"f {frame} {who} look {ang:.0f} inf {track.influence:.2f}")
            for side in ("l", "r"):
                tip = arm.matrix_world @ arm.pose.bones[f"index_03_{side}"].tail
                if tip.z < table_top - 0.01:
                    fails.append(f"f {frame} {who} {side} index {tip.z:.3f}")
                wrist = arm.matrix_world @ arm.pose.bones[f"hand_{side}"].head
                if wrist.z < table_top - 0.01:
                    fails.append(f"f {frame} {who} {side} wrist {wrist.z:.3f}")
                shoulder = arm.matrix_world @ arm.pose.bones[f"upperarm_{side}"].head
                elbow = arm.matrix_world @ arm.pose.bones[f"upperarm_{side}"].tail
                if elbow.z > shoulder.z + 0.04:
                    fails.append(f"f {frame} {who} {side} elbow above shoulder")
                target = player["hands"][side]["ik"].matrix_world.translation
                miss = (arm.matrix_world @ arm.pose.bones[f"lowerarm_{side}"].tail - target).length
                if miss > 0.025:
                    fails.append(f"f {frame} {who} {side} ik {miss:.3f}")
        for pid, obj in pieces.items():
            if obj.matrix_world.translation.z < 0.77:
                fails.append(f"f {frame} piece {pid} z {obj.matrix_world.translation.z:.3f}")
                break
    print("sweep", len(fails))
    for item in fails[:30]:
        print("FAIL", item)


def acting_report(scene, players, pieces, marks):
    """Head aim, chest clearance and the tapping fingertip. Optional stills."""
    del marks
    from mathutils.bvhtree import BVHTree

    scene.frame_set(1)
    bpy.context.view_layer.update()
    table = bpy.data.objects["dining_table"]
    tverts, tpolys = world_verts(table)
    tbvh = BVHTree.FromPolygons(tverts, tpolys)
    edge = {"w": min(v.y for v in tverts), "b": max(v.y for v in tverts)}

    def bone_axis(arm, bone, col):
        return ((arm.matrix_world @ bone.matrix).col[col]).xyz.normalized()

    def line(frame, who, side):
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        player = players[who]
        arm = player["arm"]
        head = arm.pose.bones["head"]
        face = bone_axis(arm, head, 2)
        up = bone_axis(arm, head, 1)
        origin = arm.matrix_world @ head.head
        look = player["look"].matrix_world.translation
        to_look = look - origin
        angle = math.degrees(face.angle(to_look.normalized())) if to_look.length else 999.0
        tip = arm.matrix_world @ arm.pose.bones[f"index_03_{side}"].tail
        pelvis = arm.matrix_world @ arm.pose.bones["pelvis"].head
        front = None
        front_z = None
        hits = 0
        for mesh in player["digits"]:
            verts, polys = world_verts(mesh)
            for vert in verts:
                if not 0.75 <= vert.z <= 1.20:
                    continue
                if (vert.x - pelvis.x) ** 2 + (vert.y - pelvis.y) ** 2 > 0.22 ** 2:
                    continue
                if front is None or abs(vert.y) < abs(front):
                    front = vert.y
                    front_z = vert.z
            if polys:
                hits += len(tbvh.overlap(BVHTree.FromPolygons(verts, polys)))
        gap = None if front is None else (edge[who] - front if who == "w" else front - edge[who])
        shoulder = arm.matrix_world @ arm.pose.bones[f"upperarm_{side}"].head
        elbow = arm.matrix_world @ arm.pose.bones[f"upperarm_{side}"].tail
        wrist = arm.matrix_world @ arm.pose.bones[f"hand_{side}"].head
        length = arm.pose.bones[f"upperarm_{side}"].length + arm.pose.bones[f"lowerarm_{side}"].length
        reach = (wrist - shoulder).length / length if length else 0.0
        print(
            "f", frame, player["name"],
            "ang", round(angle, 1),
            "face", tuple(round(v, 2) for v in face),
            "up", tuple(round(v, 2) for v in up),
            "look", tuple(round(v, 3) for v in look),
            "hy", round(origin.y, 3),
            "gap", None if gap is None else round(gap, 3),
            "bz", None if front_z is None else round(front_z, 3),
            "body", hits,
            "reach", round(reach, 2),
            "edz", round(elbow.z - shoulder.z, 3),
            "tip", tuple(round(v, 3) for v in tip),
            "spine", round(math.degrees(arm.pose.bones["spine_03"].rotation_euler.x), 1),
        )

    # Rest, tap up, grip, piece in the air, nod, wing knight, the reply.
    line(1, "w", "l")
    line(1, "b", "r")
    line(17, "b", "r")
    line(21, "w", "l")
    line(21, "b", "r")
    line(30, "w", "l")
    line(43, "w", "l")
    line(70, "w", "l")
    line(229, "w", "l")
    line(281, "b", "l")
    line(437, "w", "r")
    motion_sweep(scene, players, pieces)
    if "--stills" not in cc.args():
        print("act only")
        return
    focus = bpy.data.objects["Focus"]
    cam = scene.camera
    for frame, name in ((17, "tap"), (21, "grip"), (30, "air"), (70, "reply")):
        scene.frame_set(frame)
        path = cc.VIDEO_DIR / f"hraci_act2_{name}.jpg"
        cc.still_output(scene, path)
        bpy.ops.render.render(write_still=True, scene=scene.name)
        print("still", name, path)
    if cam.animation_data:
        cam.animation_data_clear()
    if focus.animation_data:
        focus.animation_data_clear()
    cam.data.dof.use_dof = False
    close = (
        (17, "tapc", Vector((-0.62, 0.18, 1.02)), Vector((-0.08, 0.32, 0.86)), 40),
        (229, "wide", Vector((-1.78, 0.0, 1.36)), Vector((0.0, 0.0, 1.06)), 32),
        (229, "wingc", Vector((-0.48, -0.22, 1.22)), Vector((-0.02, -0.40, 1.02)), 48),
    )
    for frame, name, loc, aim, lens in close:
        scene.frame_set(frame)
        cam.location = loc
        focus.location = aim
        cam.data.lens = lens
        path = cc.VIDEO_DIR / f"hraci_act2_{name}.jpg"
        cc.still_output(scene, path)
        bpy.ops.render.render(write_still=True, scene=scene.name)
        print("still", name, path)
    print("act stills")


def play_leds(leds, start, seq):
    """Firmware milliseconds on this film's 24 fps clock. cine_common.play uses 30."""
    end = start
    for t_ms, state in seq:
        frame = start + round(t_ms / 1000.0 * FPS)
        leds.key(frame, state)
        end = max(end, frame)
    return end


def light_board(rig, collection, marks):
    """Square LEDs follow the firmware: yellow movers, lift targets, then the
    blue path and the grey wave. Castling names the rook, then blinks gold."""
    ucis = [src + dst for _who, _pid, _kind, src, dst in GAME]
    script = ROOT / "hraci_led_plan.py"
    py = Path(r"C:\Users\alfid\AppData\Local\Programs\Python\Python312\python.exe")
    cmd = [str(py) if py.is_file() else "py", "-3.12", str(script), *ucis]
    if py.is_file():
        cmd = [str(py), str(script), *ucis]
    raw = subprocess.check_output(cmd, cwd=str(ROOT), text=True)
    plan = json.loads(raw)
    if len(plan["moves"]) != len(marks):
        raise RuntimeError(f"led plan {len(plan['moves'])} != moves {len(marks)}")
    leds = cc.Leds(rig, collection, glow_strength=10.5, light_energy=1.05, shadows=False, dies=False)
    lift = bpy.data.objects["Board anchor"].location.z
    for obj in bpy.data.objects:
        if obj.name.startswith("glow "):
            obj.location.z += lift
        elif obj.name.startswith("die "):
            obj.location.z += lift
            obj.hide_viewport = True
        elif obj.type == "LIGHT" and obj.name.startswith("led "):
            obj.location.z += lift
            obj.hide_viewport = True
    glow = bpy.data.objects["glow a1"]
    print("led z", round(glow.location.z, 4), "surface", round(rig.surface, 4))
    if not (rig.surface - 0.008 < glow.location.z < rig.surface):
        raise RuntimeError(f"glow a1 at {glow.location.z} is not under the glass {rig.surface}")
    leds.key(1, {sq: tuple(rgb) for sq, rgb in plan["idle"].items()})
    led_end = 1
    for mark, spec in zip(marks, plan["moves"]):
        if spec["uci"] != mark["move"]:
            raise RuntimeError(f"led {spec['uci']} != gesture {mark['move']}")
        start = mark["start"]
        lift_state = {sq: tuple(rgb) for sq, rgb in spec["lift"].items()}
        # Path / capture targets only once the piece is clearly in the air
        # (mark["high"]), never while it is still settling off the square.
        leds.key(mark["high"], lift_state)
        land = start + 38
        kind = spec.get("castle")
        if kind == "king":
            led_end = play_leds(leds, land, fx.castling_rook_pulses(spec["rook_from"], spec["rook_to"]))
            continue
        if kind == "rook":
            done = play_leds(leds, land, fx.castling_completion(spec["dst"], {spec["src"]: fx.YELLOW})[0])
        else:
            done = play_leds(leds, land, fx.move_path(spec["src"], spec["dst"]))
        done = play_leds(leds, done, fx.player_change(spec["to_white"]))
        idle = {sq: tuple(rgb) for sq, rgb in spec["idle"].items()}
        leds.key(done, idle)
        led_end = done + 18
    leds.finish()
    print("led end", led_end, "idle", " ".join(sorted(plan["idle"])))
    return led_end


def main():
    test = cc.arg_value("--test")
    preview = "--preview" in cc.args()
    final = "--final" in cc.args() or preview
    default_res = "1280x720" if preview else ("960x540" if test else "1920x1080")
    default_samples = "24" if preview else ("16" if test else "96")
    res = tuple(int(v) for v in cc.arg_value("--res", default_res).split("x"))
    samples = int(cc.arg_value("--samples", default_samples))
    uhd = (not preview) and res[1] >= 2160

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Hraci"
    room = build_room(scene.collection)
    rig = cc.build_board(scene.collection, with_pcb=False)
    raise_board(rig, room["table_top"])
    pieces, home = build_pieces(rig, scene.collection)
    players = load_players(scene.collection, room["table_top"], room["table_half"])
    seat_chairs(players)
    for player in players.values():
        for side_name, hand in player["hands"].items():
            cc.key_loc(hand["ik"], 1, hand["rest"])
            cc.key_loc(hand["pinch"], 1, hand["rest_aim"])
            set_fingers(player["arm"], side_name, finger_pose("pawn", 1.0, 0.0, 0.0, side_name), 1)
    marks, end = animate(players, pieces, home, rig)
    lay_palms(players)
    dusk, night = light_chapters(room, marks)
    scene.camera = build_camera(scene.collection, rig, marks)
    led_end = light_board(rig, scene.collection, marks)
    scene.frame_start = 1
    scene.frame_end = max(end, led_end)
    cycles_settings(scene, res, samples)
    comfort_viewport(scene)
    print("frames", scene.frame_end, "motion", end, "dusk", dusk, "night", night)
    for mark in marks:
        print("move", mark["move"], "start", mark["start"], "grip", mark["grip"])

    cc.RENDER_TMP.mkdir(parents=True, exist_ok=True)
    cc.VIDEO_DIR.mkdir(parents=True, exist_ok=True)
    if "--act" in cc.args():
        acting_report(scene, players, pieces, marks)
        if "--stills" not in cc.args():
            pinch_report(marks, pieces, players)
            collide_report(players, rig, marks)
        return
    if "--check" in cc.args():
        scene_audit()
        collide_report(players, rig, marks)
        pinch_report(marks, pieces, players)
        for frame, name in (
            (1, "wide"),
            (marks[CLOSE_MOVE]["grip"], "grip"),
            (dusk, "dusk"),
            (night, "night"),
        ):
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"hraci_chk9_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, path)
        print("check only")
        return
    if "--hands" in cc.args():
        scene.view_settings.exposure = 0.35
        scene.camera.data.dof.use_dof = False
        fill_data = bpy.data.lights.new("Hand fill", "AREA")
        fill_data.energy = 60
        fill_data.size = 1.4
        fill = bpy.data.objects.new("Hand fill", fill_data)
        scene.collection.objects.link(fill)
        fill.location = (0.0, 0.0, 1.55)
        focus = bpy.data.objects["Focus"]
        if scene.camera.animation_data:
            scene.camera.animation_data_clear()
        if focus.animation_data:
            focus.animation_data_clear()
        pinch_report(marks, pieces, players)
        shots = (
            (7, "lift", Vector((-0.55, -0.42, 1.12)), Vector((-0.08, -0.28, 0.86)), 36),
            (13, "hover", Vector((-0.42, -0.28, 1.08)), Vector((-0.04, -0.12, 0.88)), 40),
            (21, "grip", Vector((-0.34, -0.09, 0.98)), Vector((-0.02, -0.09, 0.84)), 42),
            (30, "carry", Vector((-0.35, -0.22, 1.15)), Vector((-0.02, -0.02, 0.92)), 38),
            (45, "back", Vector((-0.55, -0.42, 1.12)), Vector((-0.08, -0.28, 0.86)), 36),
        )
        for frame, name, loc, aim, lens in shots:
            scene.frame_set(frame)
            scene.camera.location = loc
            focus.location = aim
            scene.camera.data.lens = lens
            path = cc.VIDEO_DIR / f"hraci_m_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, path)
        print("hands only")
        return
    if "--axes" in cc.args():
        scene.frame_set(1)
        bpy.context.view_layer.update()

        def bone_col(arm, bone, col):
            return ((arm.matrix_world @ bone.matrix).col[col]).xyz.normalized()

        def show(tag):
            bpy.context.view_layer.update()
            for who, player in players.items():
                arm = player["arm"]
                for side in ("l", "r"):
                    hand = arm.pose.bones[f"hand_{side}"]
                    fore = arm.pose.bones[f"lowerarm_{side}"]
                    knuckle = arm.matrix_world @ arm.pose.bones[f"middle_01_{side}"].head
                    tip = arm.matrix_world @ arm.pose.bones[f"middle_03_{side}"].tail
                    finger = bone_col(arm, hand, 1)
                    bend = tip - knuckle
                    bend = bend - finger * bend.dot(finger)
                    if bend.length > 1e-4:
                        bend.normalize()
                    print(
                        tag, who, side,
                        "fore", tuple(round(v, 2) for v in bone_col(arm, fore, 1)),
                        "handY", tuple(round(v, 2) for v in finger),
                        "curl", tuple(round(v, 2) for v in bend),
                    )

        show("track")
        for player in players.values():
            for side in ("l", "r"):
                for constraint in player["arm"].pose.bones[f"hand_{side}"].constraints:
                    constraint.mute = True
        show("free")
        names = [bone.name for bone in players["w"]["arm"].pose.bones if "arm" in bone.name or "hand" in bone.name or "twist" in bone.name]
        print("bones", " ".join(names))
        print("axes only")
        return
    if "--collide" in cc.args():
        collide_report(players, rig, marks)
        if "--still" in cc.args():
            scene.frame_set(1)
            path = cc.VIDEO_DIR / "hraci_u_wide.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", path)
        print("collide only")
        return
    if test or "--probe" in cc.args() or "--grips" in cc.args():
        pinch_report(marks, pieces, players)
    if "--probe" in cc.args():
        print("probe only")
        return
    if "--leds" in cc.args():
        scene.cycles.samples = 16
        scene.render.resolution_x = 1280
        scene.render.resolution_y = 720
        scene.view_settings.exposure = 0.0
        for frame, name in ((1, "wide"), (marks[0]["start"] + 32, "widelift")):
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"hraci_led3_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, frame, path)
        focus = bpy.data.objects["Focus"]
        if scene.camera.animation_data:
            scene.camera.animation_data_clear()
        if focus.animation_data:
            focus.animation_data_clear()
        scene.camera.data.dof.use_dof = False
        scene.camera.data.lens = 28
        # Above the near edge, so the seated head stays behind the lens.
        scene.camera.location = (0.0, -0.36, rig.surface + 0.92)
        focus.location = (0.0, 0.0, rig.surface)
        shots = (
            (1, "idle"),
            (marks[0]["start"] + 32, "lift"),
            (marks[0]["start"] + 40, "path"),
            (marks[0]["start"] + 48, "wave"),
            (marks[1]["start"] + 12, "black"),
            (marks[9]["start"] + 46, "guide"),
            (marks[10]["start"] + 44, "gold"),
        )
        for frame, name in shots:
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"hraci_led3_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, frame, path)
        print("leds only")
        return
    if "--look" in cc.args():
        pinch_report(marks, pieces, players)
        for frame, name in ((1, "rest"), (137, "lift")):
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"hraci_ledbright2_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, path)
        print("look only")
        return
    if "--grips" in cc.args():
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
        print("saved", BLEND)
        grip_stills(scene, marks, pieces, players)
        return
    if test:
        wanted = [1, marks[CLOSE_MOVE]["grip"], night]
        if test != "auto":
            wanted = [int(v) for v in test.split(",")]
        names = {1: "hraci_s_wide", marks[CLOSE_MOVE]["grip"]: "hraci_s_hand", night: "hraci_s_night"}
        for frame in wanted:
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"{names.get(frame, 'hraci_' + str(frame))}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", frame, path)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
        print("saved", BLEND)
        return
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    print("saved", BLEND)
    if not final:
        print("stills first, pass --final to render the film")
        return
    out_arg = cc.arg_value("--out")
    if out_arg:
        out = Path(out_arg)
        if not out.is_absolute():
            out = cc.VIDEO_DIR / out
    elif uhd:
        out = cc.VIDEO_DIR / "hraci_4k.mp4"
    else:
        out = PREVIEW if preview else FINAL
    cc.video_output(scene, out, quality="HIGH" if uhd else "PERC_LOSSLESS")
    bpy.ops.render.render(animation=True, scene=scene.name)
    print("final", out)


if __name__ == "__main__":
    main()
