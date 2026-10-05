"""Cybertruck pieces on the czechmate2 board.

Headless:
  blender --background --factory-startup --python build_scene.py

Board GLBs are meters, converted from the Onshape part studios in
Downloads/czechmate2. Each studio is one solid. Piece STLs are millimeters
and are scaled by 0.001. The printable knight is not used; the scene uses knight_low.
"""

import math
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parent
BOARD_DIR = ROOT / "board"
BOARD_PARTS = (
    "Part_Studio_1_-_Chasie.glb",
    "Part_Studio_1_-_Dekl.glb",
    "Part_Studio_1_-_Mrizka.glb",
    "Part_Studio_1_-_Folie.glb",
    "Part_Studio_1_-_Sklo.glb",
    "Part_Studio_1_-_Tlacitko_plast.glb",
)
PIECE_DIR = ROOT.parent / "chess_pieces" / "stl"
KNIGHT = ROOT / "meshes" / "knight_low.stl"
BLEND = ROOT / "czechmate_cyber.blend"
PREVIEW = ROOT / "preview.png"

BACK_RANK = ("rook", "knight", "bishop", "queen", "king", "bishop", "knight", "rook")


def aluminum(name):
    """Brushed aluminium. Brush runs along X, the long way around the rim."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    texcoord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    noise = nodes.new("ShaderNodeTexNoise")
    rough_map = nodes.new("ShaderNodeMapRange")
    bump = nodes.new("ShaderNodeBump")
    mapping.inputs["Scale"].default_value = (8.0, 80.0, 8.0)
    noise.inputs["Scale"].default_value = 1.0
    noise.inputs["Detail"].default_value = 8.0
    noise.inputs["Roughness"].default_value = 0.65
    rough_map.inputs["From Min"].default_value = 0.25
    rough_map.inputs["From Max"].default_value = 0.75
    rough_map.inputs["To Min"].default_value = 0.22
    rough_map.inputs["To Max"].default_value = 0.48
    bump.inputs["Strength"].default_value = 0.08
    bump.inputs["Distance"].default_value = 0.0004
    bsdf.inputs["Base Color"].default_value = (0.78, 0.79, 0.80, 1.0)
    bsdf.inputs["Metallic"].default_value = 1.0
    if "Anisotropic" in bsdf.inputs:
        bsdf.inputs["Anisotropic"].default_value = 0.85
    if "Anisotropic Rotation" in bsdf.inputs:
        bsdf.inputs["Anisotropic Rotation"].default_value = 0.0
    links.new(texcoord.outputs["Object"], mapping.inputs["Vector"])
    links.new(mapping.outputs["Vector"], noise.inputs["Vector"])
    links.new(noise.outputs["Fac"], rough_map.inputs["Value"])
    links.new(rough_map.outputs["Result"], bsdf.inputs["Roughness"])
    links.new(noise.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    return mat


def printed(name, color, roughness=0.55, specular=0.32):
    """FDM plastic. Fine bands follow Z so the facets still read as the piece."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    texcoord = nodes.new("ShaderNodeTexCoord")
    wave = nodes.new("ShaderNodeTexWave")
    noise = nodes.new("ShaderNodeTexNoise")
    bump = nodes.new("ShaderNodeBump")
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    wave.wave_type = "BANDS"
    wave.bands_direction = "Z"
    wave.inputs["Scale"].default_value = 900.0
    wave.inputs["Distortion"].default_value = 1.5
    noise.inputs["Scale"].default_value = 40.0
    noise.inputs["Detail"].default_value = 2.0
    mix.inputs["Factor"].default_value = 0.35
    mix.inputs["A"].default_value = (*color, 1.0)
    mix.inputs["B"].default_value = tuple(min(channel + 0.04, 1.0) for channel in color) + (1.0,)
    bump.inputs["Strength"].default_value = 0.02
    bump.inputs["Distance"].default_value = 0.00015
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = roughness
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = specular
    links.new(texcoord.outputs["Object"], wave.inputs["Vector"])
    links.new(noise.outputs["Fac"], mix.inputs["Factor"])
    links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
    links.new(wave.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
    return mat


def flat(name, color, roughness):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = roughness
    return mat


def glass(name, color=(0.95, 0.97, 0.98), roughness=0.02, cover=1.0):
    """Fresnel glass. EEVEE transmission on this sheet renders as a black lid."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    output = nodes.get("Material Output")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["IOR"].default_value = 1.45
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    transparent.inputs["Color"].default_value = (*color, 1.0)
    fresnel = nodes.new("ShaderNodeFresnel")
    fresnel.inputs["IOR"].default_value = 1.45
    scale = nodes.new("ShaderNodeMath")
    scale.operation = "MULTIPLY"
    scale.inputs[1].default_value = cover
    mix = nodes.new("ShaderNodeMixShader")
    for link in list(links):
        if link.to_node == output and link.to_socket.name == "Surface":
            links.remove(link)
    links.new(fresnel.outputs["Fac"], scale.inputs[0])
    links.new(scale.outputs["Value"], mix.inputs["Fac"])
    links.new(transparent.outputs["BSDF"], mix.inputs[1])
    links.new(bsdf.outputs["BSDF"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    mat.blend_method = "BLEND"
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "BLENDED"
    return mat


def diffuser(name):
    """Milky film between the grid and the glass. Scatter, with the squares still readable through it."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    output = nodes.get("Material Output")
    scatter = nodes.new("ShaderNodeBsdfTranslucent")
    scatter.inputs["Color"].default_value = (0.96, 0.96, 0.94, 1.0)
    clear = nodes.new("ShaderNodeBsdfTransparent")
    clear.inputs["Color"].default_value = (0.94, 0.95, 0.96, 1.0)
    grain = nodes.new("ShaderNodeTexNoise")
    grain.inputs["Scale"].default_value = 220.0
    grain.inputs["Detail"].default_value = 6.0
    fiber = nodes.new("ShaderNodeMapRange")
    fiber.inputs["From Min"].default_value = 0.3
    fiber.inputs["From Max"].default_value = 0.75
    fiber.inputs["To Min"].default_value = 0.0
    fiber.inputs["To Max"].default_value = 1.0
    tint = nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.inputs["A"].default_value = (0.90, 0.90, 0.88, 1.0)
    tint.inputs["B"].default_value = (0.99, 0.99, 0.97, 1.0)
    links.new(grain.outputs["Fac"], fiber.inputs["Value"])
    links.new(fiber.outputs["Result"], tint.inputs["Factor"])
    links.new(tint.outputs["Result"], scatter.inputs["Color"])
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs["Fac"].default_value = 0.55
    for link in list(links):
        if link.to_node == output and link.to_socket.name == "Surface":
            links.remove(link)
    links.new(clear.outputs["BSDF"], mix.inputs[1])
    links.new(scatter.outputs["BSDF"], mix.inputs[2])
    links.new(mix.outputs["Shader"], output.inputs["Surface"])
    mat.blend_method = "BLEND"
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "BLENDED"
    return mat


def world_coords(obj, axis):
    matrix = obj.matrix_world
    return [(matrix @ vertex.co)[axis] for vertex in obj.data.vertices]


def square_centers(values):
    counts = {}
    for value in values:
        key = round(value * 1000.0) / 1000.0
        counts[key] = counts.get(key, 0) + 1
    edges = sorted(key for key, count in counts.items() if count >= 40)
    centers = []
    index = 0
    while index + 1 < len(edges):
        span = edges[index + 1] - edges[index]
        if span > 0.02:
            centers.append((edges[index] + edges[index + 1]) * 0.5)
            index += 2
        else:
            index += 1
    return centers


def paint_squares(obj, xs, ys, dark, light, body):
    """Tops of the cells take the checker. Walls stay the printed grid."""
    mesh = obj.data
    mesh.materials.clear()
    mesh.materials.append(dark)
    mesh.materials.append(light)
    mesh.materials.append(body)
    top_z = max(world_coords(obj, 2))
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.faces.ensure_lookup_table()
    seen = set()
    islands = []
    for face in bm.faces:
        if face.index in seen:
            continue
        stack = [face]
        seen.add(face.index)
        island = []
        while stack:
            current = stack.pop()
            island.append(current)
            for edge in current.edges:
                for linked in edge.link_faces:
                    if linked.index not in seen:
                        seen.add(linked.index)
                        stack.append(linked)
        islands.append(island)

    matrix = obj.matrix_world
    painted = 0
    for island in islands:
        acc = Vector((0.0, 0.0, 0.0))
        count = 0
        for face in island:
            for vert in face.verts:
                acc += matrix @ vert.co
                count += 1
        center = acc / count
        file_i = min(range(len(xs)), key=lambda i: abs(xs[i] - center.x))
        rank_i = min(range(len(ys)), key=lambda i: abs(ys[i] - center.y))
        if abs(xs[file_i] - center.x) > 0.03 or abs(ys[rank_i] - center.y) > 0.03:
            for face in island:
                face.material_index = 2
            continue
        slot = 0 if (file_i + rank_i) % 2 == 0 else 1
        for face in island:
            world_z = (matrix @ face.calc_center_median()).z
            normal_z = (matrix.to_3x3() @ face.normal).z
            face.material_index = slot if normal_z > 0.65 and world_z > top_z - 0.0015 else 2
        painted += 1
    bm.to_mesh(mesh)
    bm.free()
    mesh.update()
    print(f"squares painted {painted} / islands {len(islands)}")
    return painted


def load_piece(path, scale, material):
    before = set(bpy.data.objects)
    bpy.ops.wm.stl_import(filepath=str(path), global_scale=scale)
    created = [obj for obj in bpy.data.objects if obj not in before and obj.type == "MESH"]
    if not created:
        raise RuntimeError(f"STL import produced no mesh: {path}")
    obj = created[0]
    obj.data.materials.append(material)
    return obj


def import_board_part(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    created = [obj for obj in bpy.data.objects if obj not in before]
    # cascadio stores Z-up. The glTF importer bakes a Y-up conversion into
    # the vertices, which stands every part up. Bake the same correction
    # into each part so they keep the shared Onshape origin.
    lay_flat = Matrix.Rotation(-math.pi / 2, 4, "X")
    for obj in created:
        obj.location = lay_flat @ obj.location
        if obj.type == "MESH":
            obj.data.transform(lay_flat)
            obj.data.update()
    return created


def mesh_named(board, token):
    matches = [obj for obj in board if token.lower() in obj.name.lower()]
    if not matches:
        raise RuntimeError(f"board part not found: {token}")
    return max(matches, key=lambda obj: len(obj.data.vertices))


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0

    for name in BOARD_PARTS:
        import_board_part(BOARD_DIR / name)
    board = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    panel = mesh_named(board, "Mrizka")
    glass_pane = mesh_named(board, "Sklo")
    foil = mesh_named(board, "Folie")
    button = mesh_named(board, "Tlacitko")

    bpy.context.view_layer.update()
    xs = square_centers(world_coords(panel, 0))
    ys = square_centers(world_coords(panel, 1))
    print("files", [round(v, 4) for v in xs])
    print("ranks", [round(v, 4) for v in ys])
    if len(xs) != 8 or len(ys) != 8:
        raise RuntimeError(f"expected 8x8 squares, got {len(xs)} x {len(ys)}")

    top = max(world_coords(glass_pane, 2))
    print("playing surface z", round(top, 4))

    dark = flat("Square dark", (0.025, 0.026, 0.028), 0.62)
    light = flat("Square light", (0.90, 0.88, 0.82), 0.48)
    grid_mat = flat("Grid plastic", (0.55, 0.55, 0.53), 0.62)
    chassis_mat = aluminum("Chassis aluminium")
    lid_mat = flat("Inner lid", (0.09, 0.09, 0.095), 0.58)
    foil_mat = diffuser("Diffuser film")
    button_mat = flat("Button plastic", (0.012, 0.012, 0.013), 0.4)
    glass_mat = glass("Cover glass", (0.55, 0.62, 0.66), 0.03, 0.3)
    paint_squares(panel, xs, ys, dark, light, grid_mat)
    for obj in board:
        if obj is panel:
            continue
        if obj is glass_pane:
            mat = glass_mat
        elif obj is foil:
            mat = foil_mat
        elif obj is button:
            mat = button_mat
        elif "dekl" in obj.name.lower():
            mat = lid_mat
        elif "chasie" in obj.name.lower():
            mat = chassis_mat
        else:
            mat = chassis_mat
        if obj.data.materials:
            obj.data.materials[0] = mat
        else:
            obj.data.materials.append(mat)

    white = printed("PLA white", (0.82, 0.81, 0.77))
    black = printed("PLA black", (0.004, 0.004, 0.005), 0.82, 0.08)
    templates = {}
    for kind in ("pawn", "rook", "bishop", "queen", "king", "knight"):
        path = KNIGHT if kind == "knight" else PIECE_DIR / f"{kind}.stl"
        # Import with the white material; black objects override the slot.
        templates[kind] = load_piece(path, 0.001, white)
        templates[kind].hide_render = True
        templates[kind].hide_viewport = True

    pieces = bpy.data.collections.new("Cyber pieces")
    scene.collection.children.link(pieces)

    def place(kind, file_i, rank_i, material, yaw, label):
        base = templates[kind]
        obj = bpy.data.objects.new(label, base.data)
        obj.location = (xs[file_i], ys[rank_i], top)
        obj.rotation_euler = (0.0, 0.0, yaw)
        obj.scale = base.scale
        obj.material_slots[0].link = "OBJECT"
        obj.material_slots[0].material = material
        pieces.objects.link(obj)

    for file_i, kind in enumerate(BACK_RANK):
        # Nose of the knight is +Y. White looks toward +Y, black toward -Y.
        white_yaw = 0.0
        black_yaw = math.pi if kind == "knight" else 0.0
        place(kind, file_i, 0, white, white_yaw, f"w_{kind}_{file_i}")
        place("pawn", file_i, 1, white, 0.0, f"w_pawn_{file_i}")
        place("pawn", file_i, 6, black, 0.0, f"b_pawn_{file_i}")
        place(kind, file_i, 7, black, black_yaw, f"b_{kind}_{file_i}")

    bpy.context.view_layer.update()
    print("placed", len(pieces.objects))

    aim = bpy.data.objects.new("Aim", None)
    aim.location = (0.0, 0.0, top)
    scene.collection.objects.link(aim)
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 48
    cam = bpy.data.objects.new("Camera", cam_data)
    cam.location = (0.38, -0.36, 0.28)
    track = cam.constraints.new(type="TRACK_TO")
    track.target = aim
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.collection.objects.link(cam)
    scene.camera = cam

    world = bpy.data.worlds.new("Studio")
    scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.22, 0.23, 0.25, 1.0)
    background.inputs["Strength"].default_value = 0.15

    def area(name, location, size, energy, color):
        light_data = bpy.data.lights.new(name, "AREA")
        light_data.energy = energy
        light_data.size = size
        light_data.color = color
        light = bpy.data.objects.new(name, light_data)
        light.location = location
        constraint = light.constraints.new(type="TRACK_TO")
        constraint.target = aim
        constraint.track_axis = "TRACK_NEGATIVE_Z"
        constraint.up_axis = "UP_Y"
        scene.collection.objects.link(light)

    area("Key", (0.35, -0.45, 0.55), 0.55, 35, (1.0, 0.97, 0.92))
    area("Fill", (-0.5, -0.15, 0.28), 0.8, 8, (0.75, 0.82, 0.9))
    area("Rim", (0.1, 0.55, 0.4), 0.4, 18, (1.0, 1.0, 1.0))

    for engine in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "use_raytracing"):
        scene.eevee.use_raytracing = True
    if hasattr(scene, "eevee") and hasattr(scene.eevee, "taa_render_samples"):
        scene.eevee.taa_render_samples = 32
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW)

    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    bpy.ops.render.render(write_still=True)
    print("saved", BLEND)


if __name__ == "__main__":
    main()
