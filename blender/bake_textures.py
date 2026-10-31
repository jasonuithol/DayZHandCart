"""Bakes weathered textures for the hand truck and writes the DayZ texture set.

Run after build_handcart.py:
    blender --background --python blender/bake_textures.py
    blender --background --python blender/bake_textures.py -- --previews-only

The look is built from procedural shaders in object space (so patterns flow
across UV seams), then baked to the visual LOD's two UV texture sets:

  frame  - painted steel with peeling paint, primer edges, rust underneath,
           rust drips, chips, scraped nose plate and grime. Baked once per
           paint colour (red, blue, yellow); everything but colour is shared.
  wheels - rubber tyres with tread wear, sidewall cracks, dust and grit;
           zinc rims/axle with white rust and rust spots; grimy hand grip.

Outputs:
    textures/*.png                    - source images
    HandCart/data/*.paa, *.rvmat      - game files (PAA via tools/img2paa.sh)
    models/handcart.blend             - updated to preview the baked textures
    models/handcart_variants.png, models/handcart_closeup.png
"""

import math
import os
import subprocess
import sys

import bpy
import numpy as np
from mathutils import Vector

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLEND = os.path.join(ROOT, "models", "handcart.blend")
TEX_DIR = os.path.join(ROOT, "textures")
DATA_DIR = os.path.join(ROOT, "HandCart", "data")
DATA = "HandCart\\data\\"

TEX_SIZE = 2048
SAMPLES = 24
# DayZ/Arma _nohq maps are generally treated as DirectX-style (green = -Y).
# Blender bakes OpenGL-style, so flip green. Set False if bumps look inverted in game.
FLIP_NORMAL_GREEN = True

PAINTS = {  # linear RGB
    "blue": (0.10, 0.22, 0.45),
    "red": (0.42, 0.035, 0.025),
    "yellow": (0.72, 0.46, 0.02),
}
DEFAULT_PAINT = "blue"   # what the P3D references; others swap in via hiddenSelections

RUST_DARK = (0.13, 0.05, 0.025)
RUST_ORANGE = (0.42, 0.15, 0.04)
RUST_PIT = (0.06, 0.03, 0.02)
RUST_STAIN = (0.30, 0.12, 0.045)
BARE_STEEL = (0.24, 0.24, 0.25)
PRIMER = (0.16, 0.16, 0.15)
GRIME = (0.07, 0.06, 0.045)
DUST = (0.20, 0.17, 0.13)

MATERIAL_SET = {"frame": "frame", "rubber": "wheels", "zinc": "wheels"}


class Graph:
    """Tiny helper for building shader node graphs from Python expressions."""

    def __init__(self, mat):
        mat.use_nodes = True
        self.nt = mat.node_tree
        self.nt.nodes.clear()
        tc = self.new("ShaderNodeTexCoord")
        self.co = tc.outputs["Object"]
        geo = self.new("ShaderNodeNewGeometry")
        sep = self.new("ShaderNodeSeparateXYZ")
        self.link(self.co, sep.inputs[0])
        self.x, self.y, self.z = sep.outputs
        nsep = self.new("ShaderNodeSeparateXYZ")
        self.link(geo.outputs["Normal"], nsep.inputs[0])
        self.nx, self.ny, self.nz = nsep.outputs

    def new(self, kind, **attrs):
        node = self.nt.nodes.new(kind)
        for k, v in attrs.items():
            setattr(node, k, v)
        return node

    def link(self, a, b):
        self.nt.links.new(a, b)

    def put(self, socket, value):
        if isinstance(value, bpy.types.NodeSocket):
            self.link(value, socket)
        elif socket.type == "RGBA":
            socket.default_value = (*value, 1.0) if len(value) == 3 else value
        else:
            socket.default_value = value

    @staticmethod
    def by_id(sockets, identifier):
        return next(s for s in sockets if s.identifier == identifier)

    # --- maths ---
    def math(self, op, a, b=0.0, clamp=False):
        n = self.new("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.put(n.inputs[0], a)
        self.put(n.inputs[1], b)
        return n.outputs[0]

    def add(self, a, b, clamp=False):
        return self.math("ADD", a, b, clamp)

    def sub(self, a, b, clamp=False):
        return self.math("SUBTRACT", a, b, clamp)

    def mul(self, a, b, clamp=False):
        return self.math("MULTIPLY", a, b, clamp)

    def vec(self, op, a, b):
        n = self.new("ShaderNodeVectorMath", operation=op)
        self.put(n.inputs[0], a)
        self.put(n.inputs[1], b)
        return n.outputs["Vector"]

    def smooth(self, x, lo, hi):
        n = self.new("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP", clamp=True)
        self.put(n.inputs["Value"], x)
        n.inputs["From Min"].default_value = lo
        n.inputs["From Max"].default_value = hi
        return n.outputs["Result"]

    def lerp(self, a, b, fac):
        n = self.new("ShaderNodeMix", data_type="FLOAT", clamp_factor=True)
        self.put(self.by_id(n.inputs, "Factor_Float"), fac)
        self.put(self.by_id(n.inputs, "A_Float"), a)
        self.put(self.by_id(n.inputs, "B_Float"), b)
        return self.by_id(n.outputs, "Result_Float")

    def mix(self, a, b, fac):
        n = self.new("ShaderNodeMix", data_type="RGBA", clamp_factor=True)
        self.put(self.by_id(n.inputs, "Factor_Float"), fac)
        self.put(self.by_id(n.inputs, "A_Color"), a)
        self.put(self.by_id(n.inputs, "B_Color"), b)
        return self.by_id(n.outputs, "Result_Color")

    # --- textures ---
    def noise(self, vec, scale, detail=2.0, rough=0.5, distortion=0.0):
        n = self.new("ShaderNodeTexNoise", noise_dimensions="3D")
        self.put(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        return n.outputs["Fac"]

    def voronoi(self, vec, scale, output="Distance"):
        n = self.new("ShaderNodeTexVoronoi", voronoi_dimensions="3D")
        self.put(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        return n.outputs[output]

    def cavity(self, distance):
        """1 in crevices and contact areas, 0 on open surfaces."""
        n = self.new("ShaderNodeAmbientOcclusion", samples=16)
        n.inputs["Distance"].default_value = distance
        return self.sub(1.0, n.outputs["AO"], clamp=True)

    def finish(self, channels, bump_distance):
        """Wire up outputs: an emission shader for colour/data bakes and a
        Principled BSDF with bump for the normal bake."""
        self.channels = channels
        self.out = self.new("ShaderNodeOutputMaterial")
        self.emit = self.new("ShaderNodeEmission")
        bump = self.new("ShaderNodeBump")
        bump.inputs["Distance"].default_value = bump_distance
        self.put(bump.inputs["Height"], channels["height"])
        self.bsdf = self.new("ShaderNodeBsdfPrincipled")
        self.link(bump.outputs["Normal"], self.bsdf.inputs["Normal"])
        self.image_node = self.new("ShaderNodeTexImage")
        self.nt.nodes.active = self.image_node

    def show(self, channel):
        if channel == "normal":
            self.link(self.bsdf.outputs[0], self.out.inputs["Surface"])
        else:
            self.link(self.channels[channel], self.emit.inputs["Color"])
            self.link(self.emit.outputs[0], self.out.inputs["Surface"])


def frame_graph(mat):
    g = Graph(mat)
    co = g.co
    paint = g.new("ShaderNodeRGB")
    paint.label = "paint"
    paint_col = paint.outputs[0]

    low = g.sub(1.0, g.smooth(g.z, 0.0, 0.45))              # near the ground
    high = g.smooth(g.z, 0.95, 1.2)                        # where hands grab
    nose_top = g.mul(g.smooth(g.nz, 0.6, 0.9), g.sub(1.0, g.smooth(g.z, 0.012, 0.03)))

    def peel_value(vec):
        big = g.noise(vec, 3.0, detail=10, rough=0.62, distortion=0.35)
        fine = g.noise(vec, 22.0, detail=4)
        v = g.add(big, g.mul(g.sub(fine, 0.5), 0.22))
        v = g.add(v, g.mul(low, 0.16))
        v = g.add(v, g.mul(high, 0.10))
        return g.sub(v, g.mul(nose_top, 0.06))   # offsets most of the near-ground bias

    pv = peel_value(co)
    peel = g.smooth(pv, 0.655, 0.665)
    primer = g.sub(g.smooth(pv, 0.643, 0.652), peel, clamp=True)   # thin band round each peel
    lifted = g.smooth(pv, 0.615, 0.65)                             # cracked paint near the edge

    chip_zone = g.smooth(g.noise(co, 5.0, detail=3), 0.5, 0.62)
    chips = g.mul(g.sub(1.0, g.smooth(g.voronoi(co, 18.0), 0.08, 0.13)), chip_zone)
    exposed = g.math("MAXIMUM", peel, chips)

    # What shows through: mostly rust, scraped bright steel on worn spots
    rn = g.smooth(g.noise(co, 35.0, detail=8, rough=0.7), 0.3, 0.7)
    rust = g.mix(RUST_DARK, RUST_ORANGE, rn)
    pits = g.smooth(g.noise(co, 140.0, detail=2), 0.6, 0.75)
    rust = g.mix(rust, RUST_PIT, pits)
    scraped = g.add(g.mul(g.smooth(g.noise(co, 9.0, detail=4), 0.62, 0.72), 0.5),
                    g.mul(nose_top, 0.75), clamp=True)
    under = g.mix(rust, BARE_STEEL, scraped)

    # Paint: tonal variation, sun fading, rust drips running down from peels
    pvar = g.smooth(g.noise(co, 10.0, detail=3), 0.3, 0.7)
    paint_c = g.mix(g.mix(paint_col, (0, 0, 0), 0.22), paint_col, pvar)
    fade = g.add(g.mul(g.smooth(g.noise(co, 1.5, detail=2), 0.45, 0.75), 0.45),
                 g.mul(g.smooth(g.nz, 0.5, 1.0), 0.15), clamp=True)
    paint_c = g.mix(paint_c, g.mix(paint_col, (0.75, 0.74, 0.70), 0.35), fade)
    pv_above = peel_value(g.vec("ADD", co, (0.0, 0.0, 0.07)))
    streaks = g.smooth(g.noise(g.vec("MULTIPLY", co, (25.0, 25.0, 1.2)), 1.0, detail=2), 0.45, 0.62)
    stain = g.mul(g.mul(g.smooth(pv_above, 0.56, 0.66), streaks), 0.55)

    grime_n = g.smooth(g.noise(co, 6.0, detail=8), 0.4, 0.75)
    dirt = g.add(g.mul(g.cavity(0.06), 0.9),
                 g.add(g.mul(g.mul(low, grime_n), 0.35), g.mul(grime_n, 0.12)), clamp=True)

    c = g.mix(paint_c, RUST_STAIN, stain)
    c = g.mix(c, GRIME, g.mul(lifted, 0.3))
    c = g.mix(c, PRIMER, primer)
    c = g.mix(c, under, exposed)
    c = g.mix(c, GRIME, g.mul(dirt, 0.75))

    r = g.add(0.38, g.mul(pvar, 0.15))
    r = g.lerp(r, 0.55, stain)
    r = g.lerp(r, 0.7, primer)
    r = g.lerp(r, g.lerp(0.88, 0.45, scraped), exposed)
    r = g.lerp(r, 0.95, g.mul(dirt, 0.8))

    s = g.lerp(0.45, 0.25, stain)
    s = g.lerp(s, 0.2, primer)
    s = g.lerp(s, g.lerp(0.08, 0.7, scraped), exposed)
    s = g.lerp(s, 0.05, g.mul(dirt, 0.8))

    h_under = g.add(0.15, g.mul(g.noise(co, 90.0, detail=4), 0.2))
    h = g.add(1.0, g.mul(lifted, 0.15))
    h = g.lerp(h, 0.55, primer)
    h = g.lerp(h, h_under, exposed)

    g.finish({"color": c, "rough": r, "spec": s, "height": h}, bump_distance=0.0015)
    g.paint = paint
    return g


def rubber_graph(mat):
    g = Graph(mat)
    co = g.co
    sidewall = g.smooth(g.math("ABSOLUTE", g.nx), 0.35, 0.7)
    tread = g.sub(1.0, sidewall)
    grip = g.smooth(g.z, 0.8, 0.9)                          # the handle grip, not a tyre
    tyre = g.sub(1.0, grip)

    rv = g.smooth(g.noise(co, 30.0, detail=4), 0.3, 0.7)
    c = g.mix((0.025, 0.025, 0.025), (0.045, 0.045, 0.043), rv)

    # Tread: polished grey wear lines running round the circumference
    lines = g.smooth(g.noise(g.vec("MULTIPLY", co, (180.0, 5.0, 5.0)), 1.0, detail=3), 0.4, 0.7)
    tread_wear = g.mul(tread, tyre)
    c = g.mix(c, g.mix((0.06, 0.06, 0.058), (0.11, 0.105, 0.10), lines), tread_wear)

    # Perished sidewalls: fine cracks in patches
    cn = g.noise(co, 28.0, detail=3, distortion=0.5)
    crack = g.sub(1.0, g.smooth(g.math("ABSOLUTE", g.sub(cn, 0.5)), 0.0, 0.012))
    crack = g.mul(g.mul(crack, sidewall), g.smooth(g.noise(co, 4.0), 0.45, 0.6))
    c = g.mix(c, (0.006, 0.006, 0.006), crack)

    # Dust and dried mud, worst in crevices and the sidewalls
    dn = g.smooth(g.noise(co, 7.0, detail=10, rough=0.65), 0.35, 0.75)
    dust = g.add(g.mul(g.mul(dn, 0.55), g.sub(1.0, g.mul(tread, 0.6))), g.mul(g.cavity(0.05), 0.8), clamp=True)
    dust = g.mul(dust, tyre)
    c = g.mix(c, DUST, dust)

    # Grit: sand-coloured and dark specks
    grit_mask = g.mul(g.smooth(g.noise(co, 9.0), 0.52, 0.64), tyre)
    light_grit = g.mul(g.sub(1.0, g.smooth(g.voronoi(co, 260.0), 0.08, 0.14)), grit_mask)
    dark_grit = g.mul(g.sub(1.0, g.smooth(g.voronoi(g.vec("ADD", co, (3.1, 1.7, 0.4)), 330.0), 0.1, 0.17)),
                      grit_mask)
    c = g.mix(c, (0.20, 0.17, 0.13), light_grit)
    c = g.mix(c, (0.012, 0.011, 0.01), dark_grit)

    # Grip: hand-polished in places, grime worked into the rest
    polish = g.smooth(g.noise(co, 14.0, detail=3), 0.5, 0.65)
    grip_c = g.mix(g.mix((0.03, 0.028, 0.025), (0.06, 0.05, 0.04), g.smooth(g.noise(co, 40.0), 0.3, 0.7)),
                   (0.018, 0.018, 0.018), polish)
    c = g.mix(c, grip_c, grip)

    r = g.lerp(0.85, 0.7, tread_wear)
    r = g.lerp(r, 0.97, dust)
    r = g.lerp(r, 0.95, g.math("MAXIMUM", light_grit, dark_grit))
    r = g.lerp(r, g.lerp(0.6, 0.35, polish), grip)

    s = g.lerp(0.12, 0.18, tread_wear)
    s = g.lerp(s, 0.04, dust)
    s = g.lerp(s, 0.3, grip)

    h = g.add(0.5, g.mul(rv, 0.1))
    h = g.add(h, g.mul(g.sub(lines, 0.5), g.mul(tread_wear, 0.2)))
    h = g.lerp(h, 0.0, crack)
    h = g.add(h, g.mul(g.math("MAXIMUM", light_grit, dark_grit), 0.5))

    g.finish({"color": c, "rough": r, "spec": s, "height": h}, bump_distance=0.001)
    return g


def zinc_graph(mat):
    g = Graph(mat)
    co = g.co
    spangle = g.voronoi(co, 25.0, output="Color")   # galvanising crystals
    c = g.mix((0.16, 0.165, 0.17), (0.26, 0.265, 0.27), spangle)

    white_rust = g.smooth(g.noise(co, 12.0, detail=8), 0.58, 0.7)
    c = g.mix(c, (0.42, 0.41, 0.37), white_rust)
    rn = g.smooth(g.noise(co, 35.0, detail=8, rough=0.7), 0.3, 0.7)
    red_rust = g.smooth(g.noise(co, 20.0, detail=10, rough=0.7), 0.68, 0.74)
    c = g.mix(c, g.mix(RUST_DARK, RUST_ORANGE, rn), red_rust)
    grime = g.add(g.mul(g.cavity(0.04), 0.9),
                  g.mul(g.smooth(g.noise(co, 8.0, detail=8), 0.45, 0.8), 0.4), clamp=True)
    c = g.mix(c, GRIME, g.mul(grime, 0.8))

    r = g.add(0.3, g.mul(spangle, 0.1))
    r = g.lerp(r, 0.8, white_rust)
    r = g.lerp(r, 0.85, red_rust)
    r = g.lerp(r, 0.9, grime)

    s = g.lerp(0.75, 0.25, white_rust)
    s = g.lerp(s, 0.08, red_rust)
    s = g.lerp(s, 0.1, grime)

    h = g.add(0.5, g.mul(white_rust, g.mul(g.noise(co, 120.0), 0.3)))
    h = g.lerp(h, g.add(0.35, g.mul(rn, 0.2)), red_rust)

    g.finish({"color": c, "rough": r, "spec": s, "height": h}, bump_distance=0.001)
    return g


def new_image(name, colorspace):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, TEX_SIZE, TEX_SIZE, alpha=False)
    img.colorspace_settings.name = colorspace
    return img


def pixels(img):
    arr = np.empty(TEX_SIZE * TEX_SIZE * 4, dtype=np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(TEX_SIZE, TEX_SIZE, 4)


def save_array(arr, name, colorspace="Non-Color"):
    img = new_image(name, colorspace)
    img.pixels.foreach_set(arr.astype(np.float32).ravel())
    path = os.path.join(TEX_DIR, name + ".png")
    img.save(filepath=path)
    return path


def bake(visual, graphs, channel, images):
    """Bake one channel for every material into its texture set's image."""
    for mat_name, g in graphs.items():
        g.show(channel)
        g.image_node.image = images[MATERIAL_SET[mat_name]]
        g.nt.nodes.active = g.image_node
    scene = bpy.context.scene
    if channel == "normal":
        scene.cycles.samples = 1
        bpy.ops.object.bake(type="NORMAL", normal_space="TANGENT")
    else:
        scene.cycles.samples = SAMPLES
        bpy.ops.object.bake(type="EMIT")
    print(f"[bake] {channel} done")


def rvmat(name, specular, spec_power, fresnel):
    uv = ('uvSource="tex"; class uvTransform {aside[]={1,0,0}; up[]={0,1,0}; '
          'dir[]={0,0,0}; pos[]={0,0,0};};')
    stages = [
        f'texture="{DATA}{name}_nohq.paa"; {uv}',
        f'texture="#(argb,8,8,3)color(0.5,0.5,0.5,1,DT)"; {uv}',
        f'texture="#(argb,8,8,3)color(0,0,0,0,MC)"; {uv}',
        f'texture="#(argb,8,8,3)color(1,1,1,1,AS)"; {uv}',
        f'texture="{DATA}{name}_smdi.paa"; {uv}',
        f'texture="#(ai,64,64,1)fresnel({fresnel[0]},{fresnel[1]})"; uvSource="none";',
        'texture="dz\\data\\data\\env_land_co.paa"; useWorldEnvMap="true"; uvSource="none";',
    ]
    lines = [
        "ambient[]={1,1,1,1};",
        "diffuse[]={1,1,1,1};",
        "forcedDiffuse[]={0,0,0,0};",
        "emmisive[]={0,0,0,0};",
        f"specular[]={{{specular},{specular},{specular},1}};",
        f"specularPower={spec_power};",
        'PixelShaderID="Super";',
        'VertexShaderID="Super";',
    ]
    for i, body in enumerate(stages, start=1):
        lines.append(f"class Stage{i}\n{{\n\t{body}\n}};")
    with open(os.path.join(DATA_DIR, name + ".rvmat"), "w", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")


def preview_material(mat, co_path, nohq_path, smdi_path):
    """Replace a material's nodes with a viewport/render preview of the baked maps."""
    g = Graph(mat)
    bsdf = g.new("ShaderNodeBsdfPrincipled")
    out = g.new("ShaderNodeOutputMaterial")
    g.link(bsdf.outputs[0], out.inputs["Surface"])

    def tex(path, colorspace):
        n = g.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(path, check_existing=True)
        n.image.colorspace_settings.name = colorspace
        return n

    g.link(tex(co_path, "sRGB").outputs["Color"], bsdf.inputs["Base Color"])
    smdi = g.new("ShaderNodeSeparateColor")
    g.link(tex(smdi_path, "Non-Color").outputs["Color"], smdi.inputs[0])
    g.link(g.sub(1.0, smdi.outputs["Blue"]), bsdf.inputs["Roughness"])
    g.link(smdi.outputs["Green"], bsdf.inputs["Specular IOR Level"])
    nsep = g.new("ShaderNodeSeparateColor")
    g.link(tex(nohq_path, "Non-Color").outputs["Color"], nsep.inputs[0])
    ncomb = g.new("ShaderNodeCombineColor")
    g.link(nsep.outputs["Red"], ncomb.inputs["Red"])
    green = g.sub(1.0, nsep.outputs["Green"]) if FLIP_NORMAL_GREEN else nsep.outputs["Green"]
    g.link(green, ncomb.inputs["Green"])
    g.link(nsep.outputs["Blue"], ncomb.inputs["Blue"])
    nmap = g.new("ShaderNodeNormalMap")
    g.link(ncomb.outputs[0], nmap.inputs["Color"])
    g.link(nmap.outputs[0], bsdf.inputs["Normal"])


def render_previews(visual, frame_paths, tex_paths):
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE_NEXT"
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.6, 0.65, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    scene.world = world

    preview = bpy.data.collections.new("Preview")
    scene.collection.children.link(preview)
    ground = bpy.data.meshes.new("ground")
    ground.from_pydata([(-6, -6, 0), (6, -6, 0), (6, 6, 0), (-6, 6, 0)], [], [(0, 1, 2, 3)])
    gmat = bpy.data.materials.new("ground")
    gmat.use_nodes = True
    gmat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.3, 0.3, 0.3, 1)
    ground.materials.append(gmat)
    preview.objects.link(bpy.data.objects.new("PreviewGround", ground))
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(-40))
    preview.objects.link(sun)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    preview.objects.link(cam)
    scene.camera = cam
    for c in bpy.data.collections:
        if c.name not in ("LOD0_Visual", "Preview"):
            c.hide_render = True

    # One truck per paint colour, side by side
    visual.hide_render = True
    for i, (paint, path) in enumerate(frame_paths.items()):
        mat = bpy.data.materials.new(f"frame_{paint}_preview")
        preview_material(mat, path, *tex_paths["frame"])
        dup = visual.copy()
        dup.hide_render = False
        dup.location.x = (i - 1) * 0.85
        dup.rotation_euler.z = math.radians(-12)
        dup.material_slots[0].link = "OBJECT"
        dup.material_slots[0].material = mat
        preview.objects.link(dup)

    def shoot(path, loc, target, lens, res):
        cam.location = loc
        cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = lens
        scene.render.resolution_x, scene.render.resolution_y = res
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)

    shoot(os.path.join(ROOT, "models", "handcart_variants.png"),
          (1.3, 3.2, 1.35), (0, 0, 0.6), 35, (1600, 1000))
    shoot(os.path.join(ROOT, "models", "handcart_closeup.png"),
          (0.15, 0.75, 0.42), (-0.35, -0.05, 0.13), 50, (1400, 1000))


def existing_paths():
    """Texture paths from a previous full run, for --previews-only."""
    frame_paths = {p: os.path.join(TEX_DIR, f"handcart_frame_{p}_co.png") for p in PAINTS}
    tex_paths = {s: (os.path.join(TEX_DIR, f"handcart_{s}_nohq.png"),
                     os.path.join(TEX_DIR, f"handcart_{s}_smdi.png")) for s in ("frame", "wheels")}
    return frame_paths, tex_paths


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND)
    if "--previews-only" in sys.argv:
        render_previews(bpy.data.objects["HandCart_LOD0"], *existing_paths())
        return
    os.makedirs(TEX_DIR, exist_ok=True)
    os.makedirs(DATA_DIR, exist_ok=True)

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.render.bake.margin = 16
    scene.render.bake.margin_type = "EXTEND"
    scene.render.bake.use_clear = True

    visual = bpy.data.objects["HandCart_LOD0"]
    # Only the visual LOD may take part: collision boxes would show up in the AO
    others = [o for o in scene.objects if o is not visual]
    for o in others:
        o.hide_render = True
    bpy.ops.object.select_all(action="DESELECT")
    visual.select_set(True)
    bpy.context.view_layer.objects.active = visual

    graphs = {
        "frame": frame_graph(bpy.data.materials["frame"]),
        "rubber": rubber_graph(bpy.data.materials["rubber"]),
        "zinc": zinc_graph(bpy.data.materials["zinc"]),
    }

    def images(channel, colorspace="Non-Color"):
        return {s: new_image(f"{s}_{channel}", colorspace) for s in ("frame", "wheels")}

    data = {}
    for channel in ("normal", "rough", "spec"):
        imgs = images(channel)
        bake(visual, graphs, channel, imgs)
        data[channel] = {s: pixels(img) for s, img in imgs.items()}

    png = {}
    for s in ("frame", "wheels"):
        nohq = data["normal"][s].copy()
        if FLIP_NORMAL_GREEN:
            nohq[..., 1] = 1.0 - nohq[..., 1]
        png[f"{s}_nohq"] = save_array(nohq, f"handcart_{s}_nohq")
        # SMDI: R unused (white), G specular intensity, B glossiness
        smdi = np.ones_like(nohq)
        smdi[..., 1] = data["spec"][s][..., 0]
        smdi[..., 2] = 1.0 - data["rough"][s][..., 0]
        png[f"{s}_smdi"] = save_array(smdi, f"handcart_{s}_smdi")

    frame_paths = {}
    colour = images("color", "sRGB")
    for paint, rgb in PAINTS.items():
        graphs["frame"].paint.outputs[0].default_value = (*rgb, 1.0)
        bake(visual, graphs, "color", colour)
        # Copy out rather than saving the bake target: a file-backed image can't be re-baked
        path = save_array(pixels(colour["frame"]), f"handcart_frame_{paint}_co", "sRGB")
        frame_paths[paint] = path
        png[f"frame_{paint}_co"] = path
    png["wheels_co"] = save_array(pixels(colour["wheels"]), "handcart_wheels_co", "sRGB")

    rvmat("handcart_frame", specular=0.6, spec_power=60, fresnel=(1.3, 0.7))
    rvmat("handcart_wheels", specular=0.3, spec_power=30, fresnel=(1.0, 0.3))

    subprocess.run([os.path.join(ROOT, "tools", "img2paa.sh"), *png.values(), DATA_DIR], check=True)

    # Leave the .blend previewing the baked textures (default paint colour)
    for o in others:
        o.hide_render = False
    tex_paths = {s: (png[f"{s}_nohq"], png[f"{s}_smdi"]) for s in ("frame", "wheels")}
    preview_material(bpy.data.materials["frame"], frame_paths[DEFAULT_PAINT], *tex_paths["frame"])
    for name in ("rubber", "zinc"):
        preview_material(bpy.data.materials[name], png["wheels_co"], *tex_paths["wheels"])
    for img in list(bpy.data.images):
        if img.users == 0:
            bpy.data.images.remove(img)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=BLEND, relative_remap=True)

    render_previews(visual, frame_paths, tex_paths)


main()
