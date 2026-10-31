"""Writes the placeholder proxy models the hand truck's load slots point at.

Run headless (after build_handcart.py, which defines where the proxies sit):
    blender --background --python blender/build_proxies.py

Each HandCart/proxies/<name>.p3d is a single tiny triangle: an attachment proxy only
needs the file to exist and to be named in CfgNonAIVehicles; the item drawn in the
slot is the loaded item's own model.
"""

import os

import addon_utils
import bpy

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROXY_DIR = os.path.join(ROOT, "HandCart", "proxies")
A3OB_MODULE = "bl_ext.user_default.Arma3ObjectBuilder"
# One per load slot (see LOAD_PROXIES in build_handcart.py)
PROXY_MODELS = (["handcart_safe", "handcart_dualwheel", "handcart_generator", "handcart_barrel",
                 "handcart_stove", "handcart_sewingmachine"]
                + [f"handcart_canopytent{i}" for i in range(1, 4)]
                + [f"handcart_furniture_{name}" for name in ("sofa", "table", "officedesk", "medicaltable",
                   "shelf", "workbenchold", "wardrobe", "cupboard", "stretchergreen", "stretcherred",
                   "fridge", "tv", "pc")]
                + [f"handcart_seachest{i}" for i in range(1, 3)]
                + [f"handcart_crate{i}" for i in range(1, 5)]
                + [f"handcart_truckwheel{i}" for i in range(1, 3)]
                + [f"handcart_carwheel{i}" for i in range(1, 4)])


def main():
    addon_utils.enable(A3OB_MODULE, default_set=False)
    os.makedirs(PROXY_DIR, exist_ok=True)

    for name in PROXY_MODELS:
        for coll in (bpy.data.objects, bpy.data.meshes):
            for block in list(coll):
                coll.remove(block)

        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([(0, 0, 0), (0, 0, 0.01), (0, 0.005, 0)], [], [(0, 1, 2)])
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        props = obj.a3ob_properties_object
        props.is_a3_lod = True
        props.lod = "0"
        props.resolution = 1

        bpy.ops.a3ob.export_p3d(filepath=os.path.join(PROXY_DIR, name + ".p3d"), use_selection=False)
        print(f"[proxies] wrote {name}.p3d")


main()
