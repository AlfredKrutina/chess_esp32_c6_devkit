"""CzechMate as a nightstand lamp. No pieces. The board is the only light.

Home Assistant mode: all 64 squares one RGB colour. After idle the firmware
does this for real. The board sits on the nightstand; hanging it on the wall
would aim the LEDs at the ceiling and hide the glass.

  blender -b --factory-startup --python build_lampa.py -- --look
  blender -b --factory-startup --python build_lampa.py -- --final
"""

import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
import cine_common as cc  # noqa: E402

ROOT = Path(__file__).resolve().parent
ROOM = ROOT / "set" / "room"
TABLE_DIR = ROOT / "set" / "table"
BLEND = ROOT / "czechmate_lampa.blend"
FINAL = cc.VIDEO_DIR / "lampa.mp4"
FRAMES = cc.VIDEO_DIR / "lampa_frames"
FPS = 30
END = 240  # 8 s loop — one RGB cycle
# Home Assistant colour stops (loop closes on warm).
HA_STOPS = (
    (255, 176, 88),   # warm
    (255, 120, 70),   # amber
    (220, 70, 90),    # rose
    (140, 90, 220),   # violet
    (60, 130, 255),   # blue
    (50, 210, 180),   # teal
    (255, 176, 88),   # warm
)


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
    coord = nodes.new("ShaderNodeTexCoord")
    mapping = nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (scale, scale, scale)
    src = coord.outputs["Object"] if projection == "BOX" else coord.outputs["UV"]
    links.new(src, mapping.inputs["Vector"])
    tex = {}
    for path, is_color, key in ((diff, True, "Color"), (nor, False, "Normal"), (arm, False, "Arm")):
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
    ao.inputs["Factor"].default_value = 0.7
    links.new(tex["Color"].outputs["Color"], ao.inputs["A"])
    links.new(tex["Arm"].outputs["Color"], sep.inputs["Color"])
    links.new(sep.outputs["Red"], ao.inputs["B"])
    links.new(ao.outputs["Result"], bsdf.inputs["Base Color"])
    links.new(tex["Normal"].outputs["Color"], nmap.inputs["Color"])
    links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
    links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])
    return mat


def cube_uv(obj, size):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.cube_project(cube_size=size, correct_aspect=True)
    bpy.ops.object.mode_set(mode="OBJECT")


def put_box(name, boxes, material, collection, uv=0.5):
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


def parent_new(objs, name, collection):
    empty = bpy.data.objects.new(name, None)
    collection.objects.link(empty)
    for obj in objs:
        cc.parent_keep(obj, empty)
    return empty


def build_bedroom(collection):
    floor = pbr(
        "Parquet",
        ROOM / "floor" / "herringbone_parquet_Diffuse_2k.jpg",
        ROOM / "floor" / "herringbone_parquet_nor_gl_2k.jpg",
        ROOM / "floor" / "herringbone_parquet_arm_2k.jpg",
        specular=0.18, scale=1.1,
    )
    plaster = pbr(
        "Plaster",
        ROOM / "wall" / "painted_plaster_wall_Diffuse_2k.jpg",
        ROOM / "wall" / "painted_plaster_wall_nor_gl_2k.jpg",
        ROOM / "wall" / "painted_plaster_wall_arm_2k.jpg",
        specular=0.1, projection="BOX", scale=0.38,
    )
    linen = pbr(
        "Linen",
        ROOM / "linen" / "rough_linen_Diffuse_2k.jpg",
        ROOM / "linen" / "rough_linen_nor_gl_2k.jpg",
        ROOM / "linen" / "rough_linen_arm_2k.jpg",
        specular=0.14, normal_strength=0.75, scale=0.7,
    )
    cc.mesh_obj("Floor", cc.box_mesh("Floor", [((0.4, 0.2, -0.02), (5.2, 4.6, 0.04))]), floor, collection)
    put_box("Walls", [
        ((1.72, 0.15, 1.35), (0.08, 3.6, 2.7)),
        ((-0.9, -1.72, 1.35), (4.4, 0.08, 2.7)),
        ((-0.9, 1.95, 1.35), (4.4, 0.08, 2.7)),
        ((-2.05, 0.15, 1.35), (0.08, 3.6, 2.7)),
        ((-0.2, 0.15, 2.68), (4.6, 3.7, 0.08)),
    ], plaster, collection, uv=2.2)
    put_box("Baseboard", [
        ((1.66, 0.15, 0.06), (0.04, 3.4, 0.12)),
        ((-0.2, -1.66, 0.06), (4.2, 0.04, 0.12)),
    ], cc.principled("Base", (0.62, 0.58, 0.52), 0.5), collection, uv=0)
    # Pull the nightstand into the room so the board does not clip the wall
    # and cast that hard horizontal light/shadow cut.
    side = parent_new(import_gltf(TABLE_DIR / "WoodenTable_02.gltf"), "Nightstand", collection)
    side.location = (0.15, 0.05, 0.0)
    side.scale = (1.22, 1.22, 1.28)
    side.rotation_euler.z = math.radians(8)
    bpy.context.view_layer.update()
    top = max((side.matrix_world @ Vector((*corner, 1.0))).z for obj in side.children_recursive if obj.type == "MESH" for corner in obj.bound_box)
    print("nightstand top", round(top, 4))

    plant = parent_new(import_gltf(ROOM / "plant" / "pachira_aquatica_01_1k.gltf"), "Plant", collection)
    plant.location = (1.05, 0.85, 0.0)
    plant.scale = (0.7, 0.7, 0.7)
    return top


def raise_board(rig, top):
    anchor = bpy.data.objects.new("Board anchor", None)
    bpy.context.scene.collection.objects.link(anchor)
    for obj in (rig.chassis, rig.lid, rig.grid, rig.foil, rig.glass, rig.button):
        cc.parent_keep(obj, anchor)
    # Keep axis-aligned so LED square centres stay readable from the grid.
    anchor.location = (0.15, 0.05, top)
    bpy.context.view_layer.update()
    import build_scene as board
    rig.xs = board.square_centers(board.world_coords(rig.grid, 0))
    rig.ys = board.square_centers(board.world_coords(rig.grid, 1))
    rig.surface = max(board.world_coords(rig.glass, 2))
    print("board surface", round(rig.surface, 4), "a1", round(rig.xs[0], 4), round(rig.ys[0], 4))
    return rig


def lerp_rgb(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def ha_colour(t):
    """t in [0,1] — smooth cycle through HA_STOPS."""
    n = len(HA_STOPS) - 1
    x = t * n
    i = min(n - 1, int(x))
    return lerp_rgb(HA_STOPS[i], HA_STOPS[i + 1], x - i)


def light_lamp(rig, collection):
    # High area energy + transmission through the glass — foil glow alone barely washes the room.
    # FHD only (default --res 1920x1080). Shadows off: 64 area lights + transmission was crashing mid-render.
    leds = cc.Leds(rig, collection, glow_strength=1.6, light_energy=2.4, shadows=False, dies=False)
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
            obj.visible_transmission = True
            obj.visible_diffuse = True
            obj.visible_glossy = True
            obj.data.spread = 180.0
    for frame in range(1, END + 1):
        t = (frame - 1) / END  # seamless loop: frame END ≈ frame 1 colour
        level = 0.88 + 0.12 * (0.5 - 0.5 * math.cos(2.0 * math.pi * t))
        rgb = tuple(max(0, min(255, int(c * level))) for c in ha_colour(t))
        leds.key(frame, {sq: rgb for sq in leds.mats})
    leds.finish()
    return leds


def build_camera(collection, aim):
    focus = bpy.data.objects.new("Focus", None)
    collection.objects.link(focus)
    focus.location = aim
    data = bpy.data.cameras.new("Camera")
    data.lens = 32
    data.dof.use_dof = True
    data.dof.focus_object = focus
    data.dof.aperture_fstop = 4.0
    data.clip_start = 0.05
    cam = bpy.data.objects.new("Camera", data)
    collection.objects.link(cam)
    # High three-quarter: read the glass face + wood top; soft wall wash, no hard edge cut.
    cam.location = Vector((-0.72, -1.15, 1.48))
    track = cam.constraints.new("TRACK_TO")
    track.target = focus
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    return cam


def cycles_gpu(scene, res, samples):
    scene.render.engine = "CYCLES"
    scene.render.fps = FPS
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.cycles.samples = samples
    scene.cycles.preview_samples = 8
    scene.cycles.use_denoising = True
    scene.cycles.denoiser = "OPENIMAGEDENOISE"
    scene.cycles.use_adaptive_sampling = True
    scene.cycles.max_bounces = 8
    scene.cycles.transmission_bounces = 6
    scene.cycles.sample_clamp_indirect = 6.0
    scene.view_settings.view_transform = "AgX"
    try:
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    prefs = bpy.context.preferences.addons["cycles"].preferences
    scene.cycles.device = "CPU"
    for dtype in ("OPTIX", "CUDA"):
        try:
            prefs.compute_device_type = dtype
        except TypeError:
            continue
        prefs.get_devices()
        gpus = [d for d in prefs.devices if d.type != "CPU"]
        if not gpus:
            continue
        used = False
        for device in prefs.devices:
            if device.type == "CPU":
                device.use = False
            elif not used:
                device.use = True
                used = True
            else:
                device.use = False
        scene.cycles.device = "GPU"
        print("cycles", dtype, gpus[0].name)
        break


def dark_world():
    world = bpy.data.worlds.new("Night")
    world.use_nodes = True
    world.node_tree.nodes.clear()
    bg = world.node_tree.nodes.new("ShaderNodeBackground")
    bg.inputs["Color"].default_value = (0.01, 0.012, 0.02, 1)
    bg.inputs["Strength"].default_value = 0.07
    out = world.node_tree.nodes.new("ShaderNodeOutputWorld")
    world.node_tree.links.new(bg.outputs["Background"], out.inputs["Surface"])
    bpy.context.scene.world = world


def main():
    look = "--look" in cc.args()
    final = "--final" in cc.args()
    res = tuple(int(v) for v in cc.arg_value("--res", "1280x720" if look else "1920x1080").split("x"))
    samples = int(cc.arg_value("--samples", "16" if look else "16"))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Lampa"
    top = build_bedroom(scene.collection)
    rig = cc.build_board(scene.collection, with_pcb=False)
    raise_board(rig, top)
    light_lamp(rig, scene.collection)
    scene.camera = build_camera(scene.collection, Vector((0.15, 0.05, rig.surface + 0.01)))
    dark_world()
    scene.frame_start = 1
    scene.frame_end = END
    cycles_gpu(scene, res, samples)
    scene.view_settings.exposure = 0.05
    cc.compositor(scene, bloom=0.05, threshold=3.5, dispersion=0.002, vignette=0.26)
    cc.VIDEO_DIR.mkdir(parents=True, exist_ok=True)
    if look:
        for frame, name in ((1, "warm"), (80, "rose"), (140, "blue"), (200, "teal")):
            scene.frame_set(frame)
            path = cc.VIDEO_DIR / f"lampa8_{name}.jpg"
            cc.still_output(scene, path)
            bpy.ops.render.render(write_still=True, scene=scene.name)
            print("still", name, path)
        return
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    print("saved", BLEND)
    if not final:
        print("pass --final to render the loop")
        return
    if FRAMES.exists():
        for old in FRAMES.glob("f*.png"):
            old.unlink()
    FRAMES.mkdir(parents=True, exist_ok=True)
    scene.render.image_settings.media_type = "IMAGE"
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGB"
    scene.render.filepath = str(FRAMES / "f")
    done = [int(p.stem[1:]) for p in FRAMES.glob("f*.png") if p.stem[1:].isdigit()]
    start = (max(done) + 1) if done else 1
    if start <= END:
        scene.frame_start = start
        scene.frame_end = END
        print("png from", start, "to", END)
        bpy.ops.render.render(animation=True, scene=scene.name)
    mux_png(FRAMES, FINAL)
    print("final", FINAL)


def mux_png(folder, dest):
    import shutil
    import subprocess
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        # Blender's embedded Python often has no PATH ffmpeg; imageio ships one.
        try:
            sys.path.insert(0, str(Path(sys.executable).resolve().parents[2] / "Lib" / "site-packages"))
        except Exception:
            pass
        for root in (
            Path(r"C:\Users\alfid\AppData\Local\Programs\Python\Python312\Lib\site-packages"),
            Path.home() / "AppData" / "Local" / "Programs" / "Python" / "Python312" / "Lib" / "site-packages",
        ):
            if (root / "imageio_ffmpeg").is_dir():
                sys.path.insert(0, str(root))
                break
        try:
            import imageio_ffmpeg
            ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
        except Exception as exc:
            print("skip mux:", exc, "frames in", folder)
            return
    cmd = [
        ffmpeg, "-y", "-framerate", str(FPS),
        "-i", str(folder / "f%04d.png"),
        "-an", "-c:v", "libx264", "-preset", "slow", "-crf", "16",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart",
        str(dest),
    ]
    print("mux", dest)
    subprocess.check_call(cmd)


if __name__ == "__main__":
    main()
