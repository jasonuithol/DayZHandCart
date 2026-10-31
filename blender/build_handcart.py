"""Procedurally builds a low-poly steel hand truck (sack truck / dolly) for a DayZ mod.

Run headless:
    blender --background --python blender/build_handcart.py

Outputs (relative to the project root):
    models/handcart.blend   - source scene, one collection per DayZ LOD
    HandCart/handcart.p3d   - MLOD P3D for DayZ (needs the Arma 3 Object Builder add-on)
    models/handcart.fbx     - visual LOD only, for import into Object Builder
    models/handcart_preview.png

Conventions: 1 Blender unit = 1 metre, origin at ground level, model standing
upright with the nose plate pointing +Y and the wheels behind the frame (-Y).
Vertex groups on the visual mesh become named selections in Object Builder
("frame", "wheel_l", "wheel_r") so wheels can be animated in model.cfg, and
"camo" (the painted frame faces) is the hidden selection for colour variants.

The visual LOD has two texture sets, each with its own 0-1 UV layout:
"frame" (painted steel) and "wheels" (rubber and zinc parts, plus the grip).
Textures are baked afterwards by blender/bake_textures.py.
"""

import math
import os

import addon_utils
import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "models")
ADDON_DIR = os.path.join(ROOT, "HandCart")   # becomes the HandCart PBO (prefix "HandCart")
DATA = "HandCart\\data\\"  # in-game path prefix for textures and rvmats

# --- Dimensions (metres) ---
RAIL_X = 0.20           # half-spacing of the two upright rails
RAIL_R = 0.016          # rail tube radius
RAIL_TOP = 1.05         # where the rails turn into the loop handle
LOOP_R = RAIL_X         # loop handle is a semicircle joining the rails
GRIP_R = 0.022
CROSSBAR_Z = (0.35, 0.60, 0.85)
CROSSBAR_R = 0.010
NOSE_W = 0.46
NOSE_D = 0.24           # how far the nose plate sticks out in front (+Y)
NOSE_T = 0.008
WHEEL_R = 0.13
WHEEL_W = 0.075
WHEEL_X = 0.275
AXLE_Y = -0.15
MASS_KG = 15.0

A3OB_MODULE = "bl_ext.user_default.Arma3ObjectBuilder"
# LOD type codes from the add-on's utilities/data.py (LOD class)
LOD_VISUAL, LOD_GEOMETRY, LOD_MEMORY, LOD_VIEW_GEOMETRY, LOD_FIRE_GEOMETRY = "0", "6", "9", "14", "15"

X_AXIS_ROT = Matrix.Rotation(math.radians(90), 4, "Y")  # turns a Z-aligned cylinder onto X


def reset_scene():
    # Clear the startup scene by hand: read_factory_settings would also unload
    # the Arma 3 Object Builder add-on needed for P3D export.
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials,
                 bpy.data.cameras, bpy.data.lights, bpy.data.collections):
        for block in list(coll):
            coll.remove(block)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0


def make_material(name, color, roughness, metallic=0.0, texture=None, rvmat=""):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    if texture is not None:
        props = mat.a3ob_properties_material
        props.texture_type = "CUSTOM"   # raw in-game path, written to the P3D as-is
        props.color_raw = texture
        props.material_path = rvmat
    return mat


class MeshBuilder:
    """Accumulates parts into one bmesh, tracking material and selection per face,
    and marking UV seams so each part unwraps into clean, undistorted islands."""

    def __init__(self):
        self.bm = bmesh.new()
        self.groups = {}  # selection name -> list of verts

    def _tag(self, verts, mat_index, groups):
        for f in {f for v in verts for f in v.link_faces}:
            f.material_index = mat_index
        for group in (groups,) if isinstance(groups, str) else groups:
            self.groups.setdefault(group, []).extend(verts)

    def _seam(self, a, b):
        self.bm.edges.get((a, b)).seam = True

    def _sharp_seams(self, verts, cut_side=False):
        """Seam every hard edge; with cut_side, also slit one smooth side edge
        so a cylinder's wall can unroll."""
        edges = {e for v in verts for e in v.link_edges}
        smooth = []
        for e in edges:
            if e.calc_face_angle(0.0) > math.radians(50):
                e.seam = True
            else:
                smooth.append(e)
        if cut_side and smooth:
            smooth[0].seam = True

    def box(self, size, loc, mat_index, group, rot=None):
        m = Matrix.Translation(loc)
        if rot is not None:
            m = m @ rot
        m = m @ Matrix.Diagonal((*size, 1.0))
        res = bmesh.ops.create_cube(self.bm, size=1.0, matrix=m)
        self._sharp_seams(res["verts"])
        self._tag(res["verts"], mat_index, group)

    def cylinder(self, radius, depth, loc, mat_index, group, segments=12, rot=None):
        m = Matrix.Translation(loc)
        if rot is not None:
            m = m @ rot
        res = bmesh.ops.create_cone(
            self.bm, cap_ends=True, segments=segments,
            radius1=radius, radius2=radius, depth=depth, matrix=m,
        )
        self._sharp_seams(res["verts"], cut_side=True)
        self._tag(res["verts"], mat_index, group)

    def sweep(self, path, radius, mat_index, group, segments=8, island_len=0.4):
        """Capped round tube along a polyline, using parallel-transported frames
        so bends don't twist. The tube is cut into UV islands about island_len
        long so long tubes pack efficiently into a square texture."""
        bm = self.bm
        dense = [Vector(path[0])]
        for a, b in zip(path, path[1:]):
            a, b = Vector(a), Vector(b)
            steps = max(1, math.ceil((b - a).length / island_len))
            dense += [a.lerp(b, i / steps) for i in range(1, steps + 1)]
        path = dense
        tangents = []
        for i in range(len(path)):
            a = path[max(i - 1, 0)]
            b = path[min(i + 1, len(path) - 1)]
            tangents.append((b - a).normalized())
        normal = tangents[0].orthogonal().normalized()
        rings = []
        for i, (p, t) in enumerate(zip(path, tangents)):
            if i:
                normal = tangents[i - 1].rotation_difference(t) @ normal
            binormal = t.cross(normal)
            ring = []
            for k in range(segments):
                a = 2 * math.pi * k / segments
                ring.append(bm.verts.new(p + radius * (math.cos(a) * normal + math.sin(a) * binormal)))
            rings.append(ring)
        for r0, r1 in zip(rings, rings[1:]):
            for k in range(segments):
                k2 = (k + 1) % segments
                bm.faces.new((r0[k], r0[k2], r1[k2], r1[k]))
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
        # Seams: round both caps, and one line along the tube so it unrolls flat
        for ring in (rings[0], rings[-1]):
            for k in range(segments):
                self._seam(ring[k], ring[(k + 1) % segments])
        for r0, r1 in zip(rings, rings[1:]):
            self._seam(r0[0], r1[0])
        run = 0.0
        for i in range(1, len(rings) - 1):
            run += (path[i] - path[i - 1]).length
            if run >= island_len * 0.99:
                run = 0.0
                for k in range(segments):
                    self._seam(rings[i][k], rings[i][(k + 1) % segments])
        verts = [v for r in rings for v in r]
        bmesh.ops.recalc_face_normals(bm, faces=list({f for v in verts for f in v.link_faces}))
        self._tag(verts, mat_index, group)

    def lathe(self, profile, center, mat_index, group, segments=20):
        """Revolve a closed (x_offset, radius) profile around the X axis."""
        bm = self.bm
        rings = []
        for x_off, r in profile:
            ring = []
            for k in range(segments):
                a = 2 * math.pi * k / segments
                ring.append(bm.verts.new(center + Vector((x_off, r * math.cos(a), r * math.sin(a)))))
            rings.append(ring)
        for j in range(len(rings)):
            r0, r1 = rings[j], rings[(j + 1) % len(rings)]
            for k in range(segments):
                k2 = (k + 1) % segments
                bm.faces.new((r0[k], r0[k2], r1[k2], r1[k]))
        # Seams: cut the ring open across the profile, and once around it
        for j in range(len(rings)):
            self._seam(rings[j][0], rings[(j + 1) % len(rings)][0])
        for k in range(segments):
            self._seam(rings[0][k], rings[0][(k + 1) % segments])
        verts = [v for r in rings for v in r]
        bmesh.ops.recalc_face_normals(bm, faces=list({f for v in verts for f in v.link_faces}))
        self._tag(verts, mat_index, group)

    def to_object(self, name, materials, triangulate_all=False):
        # P3D faces must be tris or quads; collision LODs should be fully
        # triangulated so convexity is unambiguous.
        faces = [f for f in self.bm.faces if triangulate_all or len(f.verts) > 4]
        bmesh.ops.triangulate(self.bm, faces=faces, quad_method="BEAUTY", ngon_method="BEAUTY")
        self.bm.verts.index_update()
        group_indices = {g: sorted({v.index for v in vs if v.is_valid}) for g, vs in self.groups.items()}
        self.bm.normal_update()
        mesh = bpy.data.meshes.new(name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        obj = bpy.data.objects.new(name, mesh)
        for mat in materials:
            mesh.materials.append(mat)
        for g, idx in group_indices.items():
            obj.vertex_groups.new(name=g).add(idx, 1.0, "REPLACE")
        return obj


def arc_points(start_deg, end_deg, steps):
    """Points on the loop handle semicircle (XZ plane, centred on the rail tops)."""
    pts = []
    for i in range(steps + 1):
        a = math.radians(start_deg + (end_deg - start_deg) * i / steps)
        pts.append((LOOP_R * math.cos(a), 0.0, RAIL_TOP + LOOP_R * math.sin(a)))
    return pts


def build_visual(steel, rubber, zinc):
    S, R, Z = 0, 1, 2
    PAINTED = ("frame", "camo")
    b = MeshBuilder()

    # Main frame: one continuous tube up the left rail, over the loop, down the right
    bottom = NOSE_T + 0.01
    frame_path = [(-RAIL_X, 0, bottom), (-RAIL_X, 0, RAIL_TOP - 0.15), (-RAIL_X, 0, RAIL_TOP)]
    frame_path += arc_points(180, 0, 12)[1:]
    frame_path += [(RAIL_X, 0, RAIL_TOP - 0.15), (RAIL_X, 0, bottom)]
    b.sweep(frame_path, RAIL_R, S, PAINTED, segments=10)

    # Rubber grip sleeve over the top of the loop
    b.sweep(arc_points(125, 55, 6), GRIP_R, R, "frame", segments=10)

    for z in CROSSBAR_Z:
        b.sweep([(-RAIL_X, 0, z), (RAIL_X, 0, z)], CROSSBAR_R, S, PAINTED, segments=8)

    # Nose plate plus a short upstand welded to the rails
    b.box((NOSE_W, NOSE_D + 0.02, NOSE_T), Vector((0, NOSE_D / 2 - 0.01, NOSE_T / 2)), S, PAINTED)
    b.box((NOSE_W, 0.008, 0.07), Vector((0, -0.01, NOSE_T + 0.035)), S, PAINTED)
    # Gussets under the nose plate edges
    for sx in (-1, 1):
        b.box((0.006, NOSE_D * 0.6, 0.03),
              Vector((sx * (NOSE_W / 2 - 0.02), NOSE_D * 0.3, NOSE_T + 0.015)), S, PAINTED)

    # Axle and triangular brackets from each rail back to the axle
    b.sweep([(-WHEEL_X - 0.04, AXLE_Y, WHEEL_R), (WHEEL_X + 0.04, AXLE_Y, WHEEL_R)], 0.010, Z, "frame")
    for sx in (-1, 1):
        x = sx * RAIL_X
        axle = (x, AXLE_Y, WHEEL_R)
        b.sweep([(x, 0, 0.30), axle], 0.011, S, PAINTED)
        b.sweep([(x, 0, 0.06), axle], 0.011, S, PAINTED)
        b.cylinder(0.02, 0.03, Vector(axle), S, PAINTED, segments=10, rot=X_AXIS_ROT)  # axle boss

    # Wheels: rounded rubber tyre on a zinc-plated rim with a protruding hub
    tyre_profile = [(-WHEEL_W / 2, 0.085), (-WHEEL_W / 2, 0.112), (-WHEEL_W * 0.35, 0.125),
                    (0, WHEEL_R), (WHEEL_W * 0.35, 0.125), (WHEEL_W / 2, 0.112),
                    (WHEEL_W / 2, 0.085)]
    rim_profile = [(-0.032, 0.03), (-0.032, 0.088), (0.032, 0.088), (0.032, 0.03)]
    for sx, grp in ((-1, "wheel_l"), (1, "wheel_r")):
        c = Vector((sx * WHEEL_X, AXLE_Y, WHEEL_R))
        b.lathe(tyre_profile, c, R, grp)
        b.lathe(rim_profile, c, Z, grp)
        b.cylinder(0.03, WHEEL_W + 0.02, c, Z, grp, segments=12, rot=X_AXIS_ROT)

    return b.to_object("HandCart_LOD0", [steel, rubber, zinc])


def build_geometry(name):
    """Simple convex collision components (each one = one component in O2)."""
    b = MeshBuilder()
    # Frame and loop handle
    b.box((2 * RAIL_X + 2 * RAIL_R, 0.04, RAIL_TOP + LOOP_R + RAIL_R),
          Vector((0, 0, (RAIL_TOP + LOOP_R + RAIL_R) / 2)), 0, "Component01")
    # Nose plate
    b.box((NOSE_W, NOSE_D + 0.02, 0.04), Vector((0, NOSE_D / 2 - 0.01, 0.02)), 0, "Component02")
    # Wheels (octagonal prisms are plenty)
    for i, sx in enumerate((-1, 1)):
        b.cylinder(WHEEL_R, WHEEL_W, Vector((sx * WHEEL_X, AXLE_Y, WHEEL_R)), 0,
                   f"Component0{3 + i}", segments=8, rot=X_AXIS_ROT)
    # Axle brackets
    b.box((2 * RAIL_X + 0.03, abs(AXLE_Y) + 0.03, 0.27),
          Vector((0, AXLE_Y / 2, 0.175)), 0, "Component05")
    return b.to_object(name, [], triangulate_all=True)


# --- Load proxies ---
# Where each loaded item sits: (proxy model, [(position, rotation XYZ degrees), ...]).
# A kind with several places gets one proxy model per place (handcart_crate1..4, bottom of
# the stack first), each mapped to its own slot in config.cpp: a proxy class listing
# several slots drew nothing in any of them. The position is the item's model origin, which is where the proxy puts
# it. Sizes and origins measured in game (mission/ -missiontest logs [HandCartBox]):
# safes, sea chests and crates have their origin at the bottom centre, wheels at the hub
# with the axle along the item's X. Item axes: X right, Y up, Z forward (Blender Z, Y).
LOAD_BACK = 0.03          # load rests against the frame, just in front of the rails
DECK = NOSE_T             # top of the nose plate
SAFE_HALF_DEPTH = 0.20    # all three Expansion safes are ~0.4 deep
CHEST = (0.41, 0.49)      # sea chest depth (its width, turned across the truck), height
CRATE = (0.40, 0.235)     # wooden crate depth (turned across), height
TRUCK_WHEEL_R = 0.46      # MS3 and M1025 wheels
CAR_WHEEL_R = 0.39        # biggest car wheel (Gunter)
CAR_WHEEL_W = 0.22
BARREL_R = 0.29           # barrels and fire barrels, origin at the bottom
GENERATOR_HALF_DEPTH = 0.205  # power generator: 0.70 wide, 0.70 tall, origin at the bottom
TENT_R = 0.16             # packed canopy tent: not measurable on a server (its packed
                          # state has no collision box), a guessed bundle radius
LOAD_PROXIES = {
    # Safe standing on the nose plate, door facing away from the frame
    "handcart_safe": [((0, LOAD_BACK + SAFE_HALF_DEPTH, DECK), (0, 0, 0))],
    # Sea chests turned across the truck, stacked two high
    "handcart_seachest": [((0, LOAD_BACK + CHEST[0] / 2, DECK + i * (CHEST[1] + 0.01)), (0, 0, 90))
                          for i in range(2)],
    # Wooden crates turned across the truck, stacked four high
    "handcart_crate": [((0, LOAD_BACK + CRATE[0] / 2, DECK + i * (CRATE[1] + 0.005)), (0, 0, 90))
                       for i in range(4)],
    # Wheels stand on their tread, face against the frame (axle front to back)
    "handcart_dualwheel": [((0, LOAD_BACK + 0.18, DECK + TRUCK_WHEEL_R), (0, 0, 90))],
    "handcart_truckwheel": [((0, LOAD_BACK + 0.10, DECK + TRUCK_WHEEL_R), (0, 0, 90)),
                            ((0, LOAD_BACK + 0.42, DECK + TRUCK_WHEEL_R), (0, 0, 90))],
    # Car wheels lie flat, stacked three high
    "handcart_carwheel": [((0, LOAD_BACK + CAR_WHEEL_R, DECK + 0.115 + i * (CAR_WHEEL_W + 0.005)), (0, -90, 0))
                          for i in range(3)],
    # Barrel standing on the nose plate
    "handcart_barrel": [((0, LOAD_BACK + BARREL_R, DECK), (0, 0, 0))],
    # Generator standing on the nose plate, its long side across the truck
    "handcart_generator": [((0, LOAD_BACK + GENERATOR_HALF_DEPTH, DECK + 0.014), (0, 0, 0))],
    # Packed canopy tents lying across the truck, stacked three high
    "handcart_canopytent": [((0, LOAD_BACK + TENT_R, DECK + i * (2 * TENT_R + 0.01)), (0, 0, 0))
                            for i in range(3)],
}


def add_load_proxies(lod_obj, collection):
    """Proxy triangles parented to a LOD object; the add-on merges them in on export."""
    for model, placements in LOAD_PROXIES.items():
        for index, (pos, rot) in enumerate(placements, start=1):
            name = model if len(placements) == 1 else f"{model}{index}"
            # Same right-angled triangle the add-on creates, scaled down: the right angle
            # marks the origin, the short leg points forward (+Y), the long leg up (+Z)
            mesh = bpy.data.meshes.new(f"proxy_{model}_{index}")
            mesh.from_pydata([(0, 0, 0), (0, 0, 0.1), (0, 0.05, 0)], [], [(0, 1, 2)])
            obj = bpy.data.objects.new(f"proxy_{model}_{index}", mesh)
            obj.location = pos
            obj.rotation_euler = [math.radians(a) for a in rot]
            obj.parent = lod_obj
            obj.hide_render = True
            props = obj.a3ob_properties_object_proxy
            props.is_a3_proxy = True
            props.proxy_path = f"HandCart\\proxies\\{name}.p3d"
            props.proxy_index = 1
            collection.objects.link(obj)


# --- Held pose ---
# In hands, the truck item itself is hidden and each client draws handcart_visual.p3d (the
# visual and memory LODs only, so it has no collision) attached to the player, at a
# transform computed in script (HandTruckVisual.c). Engine axes, for that script: Blender
# x, y, z arrive as -x, -z, y, so the truck's nose plate points down the engine's -z.


MODEL_CFG = '''// Generated by blender/build_handcart.py - edit it there.
class CfgSkeletons
{
	class Default
	{
		isDiscrete = 1;
		skeletonInherit = "";
		skeletonBones[] = {};
	};
};

class CfgModels
{
	class Default
	{
		sectionsInherit = "";
		sections[] = {};
		skeletonName = "";
	};
	// Must match the P3D file names. "camo" is listed so hiddenSelections can retexture it.
	class handcart: Default
	{
		sections[] = {"camo"};
	};
	class handcart_visual: handcart {};
};
'''


def build_memory():
    """Memory LOD: loose vertices, each in a vertex group named after the point."""
    points = {
        "wheel_l_axis_1": (-WHEEL_X - 0.05, AXLE_Y, WHEEL_R),
        "wheel_l_axis_2": (-WHEEL_X + 0.05, AXLE_Y, WHEEL_R),
        "wheel_r_axis_1": (WHEEL_X - 0.05, AXLE_Y, WHEEL_R),
        "wheel_r_axis_2": (WHEEL_X + 0.05, AXLE_Y, WHEEL_R),
        "handle": (0, 0, RAIL_TOP + LOOP_R),
        "invView": (1.2, 1.2, 0.9),
        "ce_center": (0, 0, 0.6),
        "ce_radius": (0, 0, 1.3),
    }
    mesh = bpy.data.meshes.new("HandCart_Memory")
    mesh.from_pydata(list(points.values()), [], [])
    obj = bpy.data.objects.new("HandCart_Memory", mesh)
    for i, name in enumerate(points):
        obj.vertex_groups.new(name=name).add([i], 1.0, "REPLACE")
    return obj


def set_lod(obj, lod, resolution=0, properties=()):
    props = obj.a3ob_properties_object
    props.is_a3_lod = True
    props.lod = lod
    props.resolution = resolution
    for name, value in properties:
        prop = props.properties.add()
        prop.name = name
        prop.value = value


def set_uniform_mass(obj, total):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    layer = bm.verts.layers.float.new("a3ob_mass")
    for v in bm.verts:
        v[layer] = total / len(bm.verts)
    bm.to_mesh(obj.data)
    bm.free()


def export_p3d(path):
    bpy.ops.a3ob.export_p3d(
        filepath=path,
        use_selection=False,
        validate_lods=True,
        validate_lods_warning_errors=False,
        lod_collisions="FAIL",
    )


def uv_unwrap(obj, texture_sets):
    """Unwrap each texture set (a list of material slot indices) into its own 0-1 space."""
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for slots in texture_sets:
        bpy.ops.mesh.select_all(action="DESELECT")
        for slot in slots:
            obj.active_material_index = slot
            bpy.ops.object.material_slot_select()
        bpy.ops.uv.unwrap(method="ANGLE_BASED", margin=0.004)
        bpy.ops.uv.average_islands_scale()   # even texel density across parts
        bpy.ops.uv.pack_islands(rotate=True, rotate_method="CARDINAL", margin=0.004)
    bpy.ops.object.mode_set(mode="OBJECT")
    obj.select_set(False)


def new_collection(name):
    coll = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(coll)
    return coll


def render_preview(path):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.resolution_x = 1000
    scene.render.resolution_y = 1100
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.6, 0.65, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    scene.world = world

    ground = bpy.data.meshes.new("ground")
    gbm = bmesh.new()
    bmesh.ops.create_grid(gbm, x_segments=1, y_segments=1, size=6)
    gbm.to_mesh(ground)
    gbm.free()
    g_obj = bpy.data.objects.new("PreviewGround", ground)
    ground.materials.append(make_material("ground", (0.3, 0.3, 0.3), 0.9))
    preview = new_collection("Preview")
    preview.objects.link(g_obj)

    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-40))
    preview.objects.link(sun)

    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.location = (1.7, 1.9, 1.25)
    direction = Vector((0, -0.05, 0.6)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 40
    preview.objects.link(cam)
    scene.camera = cam

    # Hide non-visual LODs from the render
    for c in bpy.data.collections:
        if c.name not in ("LOD0_Visual", "Preview"):
            c.hide_render = True

    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def main():
    addon_utils.enable(A3OB_MODULE, default_set=False)
    if not hasattr(bpy.ops.a3ob, "export_p3d"):
        raise RuntimeError(f"Arma 3 Object Builder add-on ({A3OB_MODULE}) is not installed")
    reset_scene()
    os.makedirs(OUT_DIR, exist_ok=True)

    # Blue is the model's default skin; red and yellow are swapped in through
    # the "camo" hidden selection in config.cpp.
    frame_tex = (DATA + "handcart_frame_blue_co.paa", DATA + "handcart_frame.rvmat")
    wheels_tex = (DATA + "handcart_wheels_co.paa", DATA + "handcart_wheels.rvmat")
    steel = make_material("frame", (0.10, 0.22, 0.45), 0.45, 0.6, *frame_tex)
    rubber = make_material("rubber", (0.03, 0.03, 0.03), 0.9, 0.0, *wheels_tex)
    zinc = make_material("zinc", (0.62, 0.63, 0.65), 0.35, 0.9, *wheels_tex)

    lod0 = new_collection("LOD0_Visual")
    visual = build_visual(steel, rubber, zinc)
    lod0.objects.link(visual)
    uv_unwrap(visual, [[0], [1, 2]])
    set_lod(visual, LOD_VISUAL, resolution=1)
    add_load_proxies(visual, lod0)

    geo = new_collection("Geometry")
    geometry = build_geometry("HandCart_Geometry")
    geo.objects.link(geometry)
    set_uniform_mass(geometry, MASS_KG)
    # autocenter=0 keeps the model origin where it is, so memory points line up
    set_lod(geometry, LOD_GEOMETRY, properties=(("autocenter", "0"),))
    fire = new_collection("FireGeometry")
    fire_geo = build_geometry("HandCart_FireGeometry")
    fire.objects.link(fire_geo)
    set_lod(fire_geo, LOD_FIRE_GEOMETRY)
    view = new_collection("ViewGeometry")
    view_geo = build_geometry("HandCart_ViewGeometry")
    view.objects.link(view_geo)
    set_lod(view_geo, LOD_VIEW_GEOMETRY)

    mem = new_collection("Memory")
    memory = build_memory()
    mem.objects.link(memory)
    set_lod(memory, LOD_MEMORY)

    for c in (geo, fire, view):
        for o in c.objects:
            o.display_type = "WIRE"

    tris = sum(len(p.vertices) - 2 for p in visual.data.polygons)
    print(f"[handcart] visual LOD: {len(visual.data.vertices)} verts, {tris} tris")

    # FBX: visual mesh only
    bpy.ops.object.select_all(action="DESELECT")
    visual.select_set(True)
    bpy.context.view_layer.objects.active = visual
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUT_DIR, "handcart.fbx"),
        use_selection=True,
        apply_scale_options="FBX_SCALE_UNITS",
        axis_forward="Y",
        axis_up="Z",
        mesh_smooth_type="FACE",
    )

    os.makedirs(ADDON_DIR, exist_ok=True)
    with open(os.path.join(ADDON_DIR, "model.cfg"), "w") as f:
        f.write(MODEL_CFG)
    export_p3d(os.path.join(ADDON_DIR, "handcart.p3d"))
    # The held-pose stand-in: the same model without collision LODs
    for c in (geo, fire, view):
        c.hide_viewport = True
        for o in c.objects:
            o.a3ob_properties_object.is_a3_lod = False
    export_p3d(os.path.join(ADDON_DIR, "handcart_visual.p3d"))
    for c in (geo, fire, view):
        c.hide_viewport = False
        for o in c.objects:
            o.a3ob_properties_object.is_a3_lod = True

    bpy.context.preferences.filepaths.save_version = 0  # no .blend1 backups
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "handcart.blend"))
    # Saved before the preview scene is added, so the .blend stays clean
    render_preview(os.path.join(OUT_DIR, "handcart_preview.png"))


main()
