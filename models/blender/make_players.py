"""Build two rigged MakeHuman players and save them for the chess film.

The procedural capsules in build_hraci cannot read as people. These are the
CC0 MakeHuman base, with the system clothes, hair and skin. Run once; the film
imports set/people/players.blend and does not need MPFB again.

    blender --background --factory-startup --python make_players.py -- --still
"""

import importlib
import sys
from pathlib import Path

import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "set" / "people" / "mpfb_user"
OUT = ROOT / "set" / "people" / "players.blend"
STILL = ROOT / "video" / "hraci_people.jpg"
MODULE = "bl_ext.user_default.mpfb"


def argv():
    if "--" not in sys.argv:
        return []
    return sys.argv[sys.argv.index("--") + 1:]


def services():
    if MODULE not in bpy.context.preferences.addons:
        bpy.ops.preferences.addon_enable(module=MODULE)
    loc = importlib.import_module(MODULE + ".services.locationservice").LocationService
    loc._user_home = str(ASSETS)
    loc._user_data = str(ASSETS / "data")
    if not (ASSETS / "data" / "clothes").is_dir():
        raise RuntimeError(f"missing assets in {ASSETS / 'data'}")
    human = importlib.import_module(MODULE + ".services.humanservice").HumanService
    assets = importlib.import_module(MODULE + ".services.assetservice").AssetService
    return human, assets


def macro(age, weight, muscle, height, african, asian, caucasian):
    return {
        "gender": 1.0,
        "age": age,
        "muscle": muscle,
        "weight": weight,
        "proportions": 0.5,
        "height": height,
        "cupsize": 0.0,
        "firmness": 0.5,
        "race": {"asian": asian, "caucasian": caucasian, "african": african},
    }


PLAYERS = (
    {
        "name": "White",
        "macro": macro(0.40, 0.42, 0.50, 0.52, 0.0, 0.0, 1.0),
        "skin": "young_caucasian_male.mhmat",
        "clothes": "male_elegantsuit01.mhclo",
        "hair": "short02.mhclo",
        "brows": "eyebrow001.mhclo",
        "shoes": "shoes01.mhclo",
        "x": -0.55,
    },
    {
        # The name is the side of the board. The body is a younger, tall white man.
        "name": "Black",
        "macro": macro(0.33, 0.36, 0.52, 0.80, 0.0, 0.0, 1.0),
        "skin": "young_caucasian_male2.mhmat",
        "clothes": "male_casualsuit01.mhclo",
        "hair": "short01.mhclo",
        "brows": "eyebrow002.mhclo",
        "shoes": "shoes03.mhclo",
        "x": 0.55,
    },
)


def add_asset(human, assets, basemesh, subdir, filename, asset_type):
    path = assets.find_asset_absolute_path(filename, asset_subdir=subdir)
    if not path:
        raise RuntimeError(f"missing {subdir}/{filename}")
    print("asset", filename)
    human.add_mhclo_asset(path, basemesh, asset_type=asset_type, material_type="MAKESKIN", subdiv_levels=1)


def adopt(name, before):
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    fresh = [obj for obj in bpy.data.objects if obj not in before]
    for obj in fresh:
        for owner in list(obj.users_collection):
            owner.objects.unlink(obj)
        collection.objects.link(obj)
    armature = next(obj for obj in fresh if obj.type == "ARMATURE")
    armature.name = name
    armature.data.name = name
    return collection, armature


def dump_bones(arm):
    bpy.context.view_layer.update()
    wanted = (
        "pelvis", "spine_03", "head", "thigh_l", "calf_l", "foot_l",
        "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
        "index_01_r", "index_03_r", "thumb_01_r", "thumb_03_r",
        "middle_01_r", "hand_l",
    )
    print("bones", arm.name, len(arm.data.bones))
    for bone_name in wanted:
        bone = arm.data.bones.get(bone_name)
        if bone is None:
            print(" missing", bone_name)
            continue
        head = Vector(bone.head_local)
        direction = Vector(bone.tail_local) - head
        parent = bone.parent.name if bone.parent else "-"
        print(
            f"  {bone_name:14} head ({head.x:7.3f},{head.y:7.3f},{head.z:7.3f})"
            f" dir ({direction.x:6.3f},{direction.y:6.3f},{direction.z:6.3f}) parent {parent}"
        )
    names = sorted(bone.name for bone in arm.data.bones)
    print(" names", " ".join(names))


def build_one(human, assets, spec):
    print("player", spec["name"])
    before = set(bpy.data.objects)
    basemesh = human.create_human(macro_detail_dict=spec["macro"], feet_on_ground=True, scale=0.1)
    skin = assets.find_asset_absolute_path(spec["skin"], asset_subdir="skins")
    if not skin:
        raise RuntimeError(spec["skin"])
    human.set_character_skin(skin, basemesh, skin_type="ENHANCED_SSS")
    rig = human.add_builtin_rig(basemesh, "game_engine")
    if rig is None:
        raise RuntimeError("rig failed")
    for subdir, filename, asset_type in (
        ("eyes", "high-poly.mhclo", "Eyes"),
        ("eyebrows", spec["brows"], "Eyebrows"),
        ("eyelashes", "eyelashes01.mhclo", "Eyelashes"),
        ("teeth", "teeth_base.mhclo", "Teeth"),
        ("tongue", "tongue01.mhclo", "Tongue"),
        ("hair", spec["hair"], "Hair"),
        ("clothes", spec["clothes"], "Clothes"),
        ("clothes", spec["shoes"], "Clothes"),
    ):
        add_asset(human, assets, basemesh, subdir, filename, asset_type)
    collection, armature = adopt(spec["name"], before)
    armature.location.x = spec["x"]
    dump_bones(armature)
    span = [obj.dimensions for obj in collection.objects if obj.type == "MESH"]
    height = max((item.z for item in span), default=0.0)
    print(spec["name"], "mesh height", round(height, 3), "objects", len(collection.objects))
    return collection


def still(scene):
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 16
    scene.cycles.use_denoising = True
    scene.render.resolution_x = 960
    scene.render.resolution_y = 540
    scene.render.image_settings.file_format = "JPEG"
    scene.render.image_settings.quality = 92
    scene.render.filepath = str(STILL)
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.get_devices()
    for dtype in ("OPTIX", "CUDA"):
        try:
            prefs.compute_device_type = dtype
        except TypeError:
            continue
        prefs.get_devices()
        if any(device.type != "CPU" for device in prefs.devices):
            for device in prefs.devices:
                device.use = device.type != "CPU"
            scene.cycles.device = "GPU"
            break
    world = bpy.data.worlds.new("People")
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.55, 0.52, 0.48, 1.0)
    bg.inputs["Strength"].default_value = 0.8
    scene.world = world
    light_data = bpy.data.lights.new("Key", "AREA")
    light_data.energy = 80
    light_data.size = 2.0
    light = bpy.data.objects.new("Key", light_data)
    light.location = (1.2, -2.4, 2.2)
    scene.collection.objects.link(light)
    cam_data = bpy.data.cameras.new("People cam")
    cam_data.lens = 40
    cam = bpy.data.objects.new("People cam", cam_data)
    cam.location = (0.0, -3.4, 1.15)
    scene.collection.objects.link(cam)
    track = cam.constraints.new("TRACK_TO")
    aim = bpy.data.objects.new("Aim", None)
    aim.location = (0.0, 0.0, 0.95)
    scene.collection.objects.link(aim)
    track.target = aim
    track.track_axis = "TRACK_NEGATIVE_Z"
    track.up_axis = "UP_Y"
    scene.camera = cam
    bpy.ops.render.render(write_still=True)
    print("still", STILL)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    human, assets = services()
    scene = bpy.context.scene
    for spec in PLAYERS:
        build_one(human, assets, spec)
    bpy.ops.file.pack_all()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT))
    print("saved", OUT)
    if "--still" in argv():
        still(scene)


if __name__ == "__main__":
    main()
