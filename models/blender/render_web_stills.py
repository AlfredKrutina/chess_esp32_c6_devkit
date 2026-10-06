"""Product stills of the printed set. White PLA, no magnet ghosts.

  blender --background --factory-startup --python render_web_stills.py
"""
import math
import sys
from pathlib import Path

import bpy

sys.path.insert(0, str(Path(__file__).resolve().parent))
from mathutils import Vector

import cine_common as cc

OUT = cc.ROOT / "video" / "web_stills"
KINDS = ("rook", "knight", "bishop", "queen", "king", "pawn")
# The knight STL nose points +Y. Camera sits on -Y, so -90 degrees
# plus a little extra brings the snout toward the lens instead of the flat back.
KNIGHT_YAW = -(math.pi / 2) - 0.4


def place(templates, mats, coll):
    pieces = []
    for index, kind in enumerate(KINDS):
        obj = cc.make_piece(
            templates, kind, "w", kind, ((index - 2.5) * 0.062, 0.0, 0.0), mats, coll, magnets=False
        )
        if kind == "knight":
            obj.rotation_euler = (0.0, 0.0, KNIGHT_YAW)
        pieces.append(obj)
    return pieces


def shoot(scene, cam, target, loc, aim, lens, fstop, path, res):
    cam.location = Vector(loc)
    target.location = Vector(aim)
    cam.data.lens = lens
    cam.data.dof.aperture_fstop = fstop
    scene.render.resolution_x, scene.render.resolution_y = res
    cc.still_output(scene, path)
    bpy.ops.render.render(write_still=True)
    print("still", path.name)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    coll = scene.collection
    cc.black_world(scene)
    cc.floor(coll, size=4.0, color=(0.004, 0.005, 0.004), roughness=0.72)
    mats = cc.piece_materials()
    white = mats[0]
    bsdf = next(n for n in white.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    # Matte PLA reads brighter than its albedo under area lights. Keep the
    # base mid-grey and underexpose so facets show instead of clipping white.
    bsdf.inputs["Base Color"].default_value = (0.26, 0.25, 0.23, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.9
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.18
    elif "Specular" in bsdf.inputs:
        bsdf.inputs["Specular"].default_value = 0.18
    templates = cc.piece_templates(white)
    pieces = place(templates, mats, coll)
    mesh = templates["knight"].data
    xs = [v.co.x for v in mesh.vertices]
    ys = [v.co.y for v in mesh.vertices]
    zs = [v.co.z for v in mesh.vertices]
    print(
        f"knight bounds x {min(xs):.4f} {max(xs):.4f} "
        f"y {min(ys):.4f} {max(ys):.4f} z {min(zs):.4f} {max(zs):.4f}"
    )

    aim = bpy.data.objects.new("Light aim", None)
    aim.location = (0.0, 0.0, 0.04)
    coll.objects.link(aim)
    # Soft, dim keys so facets keep tonal steps on white PLA.
    cc.area(coll, "Key", (0.22, -0.08, 0.16), 0.16, 4.2, (1.0, 0.97, 0.92), aim, size_y=0.2)
    cc.area(coll, "Fill", (-0.2, -0.24, 0.08), 0.55, 0.9, (0.75, 0.85, 0.8), aim)
    cc.area(coll, "Rim", (-0.05, 0.22, 0.13), 0.12, 1.8, (0.55, 0.7, 0.62), aim)

    cam, target = cc.camera(coll, "Still", lens=50, fstop=8.0)
    scene.camera = cam
    cc.render_settings(scene, (3840, 2160), samples=64, motion_blur=False, exposure=-1.35)
    # Product stills need the quiet AgX base, not the punchy film look.
    for look in ("None", "AgX - Base Contrast", "AgX - Low Contrast"):
        try:
            scene.view_settings.look = look
            break
        except TypeError:
            continue
    print("look", scene.view_settings.look, "exposure", scene.view_settings.exposure)
    OUT.mkdir(parents=True, exist_ok=True)

    shoot(
        scene, cam, target,
        (0.0, -0.52, 0.07), (0.0, 0.0, 0.03),
        40, 16.0, OUT / "lineup.jpg", (3840, 2160),
    )

    # Whole piece, base included, filling the portrait. King and queen are
    # seen from above-front so the cross and crown read; the knight stays
    # in profile because that is the only angle the head is a horse.
    shots = {
        "rook": ((0.055, -0.18, 0.065), (0.0, 0.0, 0.028)),
        "knight": ((0.02, -0.17, 0.055), (0.0, 0.0, 0.028)),
        "bishop": ((0.05, -0.18, 0.068), (0.0, 0.0, 0.03)),
        "queen": ((0.065, -0.18, 0.075), (0.0, 0.0, 0.034)),
        "king": ((0.09, -0.16, 0.09), (0.0, 0.0, 0.038)),
        "pawn": ((0.045, -0.15, 0.05), (0.0, 0.0, 0.022)),
    }
    for obj, kind in zip(pieces, KINDS):
        for other in pieces:
            other.hide_render = other is not obj
        obj.location = (0.0, 0.0, 0.0)
        if kind == "knight":
            obj.rotation_euler = (0.0, 0.0, KNIGHT_YAW)
        loc, aim_at = shots[kind]
        shoot(scene, cam, target, loc, aim_at, 50, 16.0, OUT / f"{kind}.jpg", (1800, 3600))
    print("web stills", OUT)


if __name__ == "__main__":
    main()
