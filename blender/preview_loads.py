"""Renders the hand truck with stand-in boxes/cylinders on its load proxies, one image per
kind of load, to eyeball LOAD_PROXIES in build_handcart.py before testing in game.

    blender --background models/handcart.blend --python blender/preview_loads.py

Writes models/handcart_loads.png. With "-- --held" it renders the in-hands pose instead
(tilted and offset as HandCartSettings' defaults, origin = the hand, a grey floor 1 m
below it) to models/handcart_loads_held.png. Stand-in sizes are the same estimates the proxy
positions were derived from, in the item's own axes (X right, Y forward, Z up).
"""

import math
import os
import re
import sys

import bpy
from mathutils import Vector

HELD = "--held" in sys.argv
HOLD_TILT, HOLD_UP, HOLD_FORWARD = 35.0, -1.13, 0.67   # HandCartSettings defaults
HAND_HEIGHT = 1.0                                       # assumed: hand height above ground
AXLE = Vector((0, -0.15, 0.13))

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "models")

# Measured collision boxes (item space min, max: X right, Y up, Z forward), from the
# [HandCartBox] lines -missiontest logs
STAND_INS = {
    "handcart_safe": ((-0.31, 0.0, -0.195), (0.198, 0.714, 0.211)),         # ExpansionSafeLarge
    "handcart_seachest": ((-0.205, 0.0, -0.444), (0.205, 0.491, 0.444)),
    "handcart_crate": ((-0.197, 0.002, -0.396), (0.206, 0.233, 0.399)),
    "handcart_dualwheel": ((-0.179, -0.46, -0.436), (0.316, 0.459, 0.439)),
    "handcart_truckwheel": ((-0.089, -0.459, -0.435), (0.206, 0.459, 0.438)),  # M1025, the widest
    "handcart_carwheel": ((-0.105, -0.375, -0.384), (0.105, 0.375, 0.384)),   # Gunter, the biggest
}


def stand_in(box, matrix, mat):
    """A box the size of the item's collision box, offset from its origin like the item."""
    (x0, y0, z0), (x1, y1, z1) = box
    bpy.ops.mesh.primitive_cube_add(size=1)
    obj = bpy.context.object
    # item space (X, Y up, Z forward) -> Blender (X, Y forward, Z up)
    obj.scale = (x1 - x0, z1 - z0, y1 - y0)
    obj.location = ((x0 + x1) / 2, (z0 + z1) / 2, (y0 + y1) / 2)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    obj.matrix_world = matrix
    obj.data.materials.append(mat)
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def hold_matrix():
    """Same transform as model.cfg's HoldTilt then HoldUp/HoldForward, plus the hand height."""
    from mathutils import Matrix
    tilt = (Matrix.Translation(AXLE) @ Matrix.Rotation(math.radians(HOLD_TILT), 4, "X")
            @ Matrix.Translation(-AXLE))
    return Matrix.Translation((0, HOLD_FORWARD, HOLD_UP + HAND_HEIGHT)) @ tilt


def main():
    scene = bpy.context.scene
    proxies = [o for o in bpy.data.objects if o.a3ob_properties_object_proxy.is_a3_proxy]
    if HELD:
        m = hold_matrix()
        for o in bpy.data.objects:
            if o.parent is None and o.type == "MESH":
                o.matrix_world = m @ o.matrix_world
        bpy.context.view_layer.update()
        bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, 0))
        floor = bpy.context.object
        for c in list(floor.users_collection):
            c.objects.unlink(floor)
        scene.collection.objects.link(floor)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.04, location=(0, 0, HAND_HEIGHT))
        hand = bpy.context.object
        for c in list(hand.users_collection):
            c.objects.unlink(hand)
        scene.collection.objects.link(hand)
    for c in bpy.data.collections:
        c.hide_render = c.name != "LOD0_Visual"

    mat = bpy.data.materials.new("standin")
    mat.diffuse_color = (0.55, 0.45, 0.3, 1)
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.55, 0.45, 0.3, 1)

    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-40))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.location = (2.6, 0.2, 1.0) if HELD else (2.1, 1.9, 1.3)
    cam.rotation_euler = (Vector((0, 0.2, 0.6 if not HELD else 0.55)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 28
    scene.collection.objects.link(cam)
    scene.camera = cam
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 480
    scene.render.resolution_y = 480
    scene.world.color = (0.6, 0.6, 0.62)

    paths = []
    for model, box in STAND_INS.items():
        made = [stand_in(box, p.matrix_world.copy(), mat) for p in proxies
                if re.sub(r"\d*\.p3d$", "", p.a3ob_properties_object_proxy.proxy_path.lower()).endswith(model)]
        path = os.path.join(OUT, f"_load_{model}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)
        for o in made:
            bpy.data.objects.remove(o)

    # 3 x 2 contact sheet
    images = [bpy.data.images.load(p) for p in paths]
    w, h = images[0].size
    sheet = bpy.data.images.new("loads", 3 * w, 2 * h)
    pixels = [0.0] * (3 * w * 2 * h * 4)
    for i, img in enumerate(images):
        col, row = i % 3, 1 - i // 3
        src = list(img.pixels)
        for y in range(h):
            dst = ((row * h + y) * 3 * w + col * w) * 4
            pixels[dst:dst + w * 4] = src[y * w * 4:(y + 1) * w * 4]
    sheet.pixels = pixels
    sheet.filepath_raw = os.path.join(OUT, "handcart_loads_held.png" if HELD else "handcart_loads.png")
    sheet.file_format = "PNG"
    sheet.save()
    for p in paths:
        os.remove(p)


main()
