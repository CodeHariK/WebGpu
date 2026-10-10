"""Build sand_texture/sand_texture.blend: a TILEABLE sand material (wind ripples + grain specks).

Run:  Blender -b --python make_sand_texture.py
WARNING: rebuilds the .blend from scratch, so edits made in the Blender GUI are lost.
To only re-bake after tweaking the material, use bake_sand_texture.py instead.

Tileable trick: every noise is sampled on a 4D torus made from the plane's UV:
(cos 2πu, sin 2πu, cos 2πv, sin 2πv). Walking off one edge of the tile lands exactly on the other
edge, so the pattern repeats without a seam. The ripple stripes are u·A + v·B with whole numbers
A, B, so they also line up across the edges.

Second tile, MacroTile (80 m): node group SandMacro → Tone, Wet, Patches, Stripes, the big soft
noises of sand step 13 → sand_macro.png (bake_sand_macro.py). To add it to an existing .blend
without rebuilding (keeps your GUI edits):
  Blender -b sand_texture.blend --python-expr "import sys; sys.path.insert(0, '.'); import make_sand_texture as m; m.add_macro_and_save()"
"""
import math
import os
import sys

import bpy

OUT = os.path.dirname(os.path.abspath(__file__))   # this folder
TILE = 4.0          # metres per tile (one texture repeat)
MACRO_TILE = 80.0   # metres per repeat of the macro tile (sand step 13's macro_tile)
sys.path.insert(0, os.path.join(OUT, "..", "bake_lib"))
from blender_tile import Group, group_node, new_material, tile_plane  # noqa: E402


def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def pattern_group():
    """Node group SandPattern: outputs Ripple (0 trough … 1 crest), Grain (0/1 speck mask),
    GrainLight (0 dark speck / 1 light speck), Tone (big soft patches)."""
    ng = bpy.data.node_groups.new("SandPattern", "ShaderNodeTree")
    I = ng.interface
    for name, default, lo, hi in (("Ripple A", 2.0, -20, 20), ("Ripple B", 7.0, -20, 20),
                                  ("Warp", 0.45, 0, 3), ("Wiggle", 0.12, 0, 1),
                                  ("Grain Scale", 35.0, 1, 400), ("Grain Density", 0.12, 0, 1),
                                  ("Grain Size", 0.36, 0, 1)):
        s = I.new_socket(name, in_out="INPUT", socket_type="NodeSocketFloat")
        s.default_value, s.min_value, s.max_value = default, lo, hi
    for name in ("Ripple", "Grain", "GrainLight", "Tone"):
        I.new_socket(name, in_out="OUTPUT", socket_type="NodeSocketFloat")
    N, L = ng.nodes.new, ng.links.new
    gin = N("NodeGroupInput")
    gout = N("NodeGroupOutput")

    def math_node(op, a, b=None, c=None):
        n = N("ShaderNodeMath")
        n.operation = op
        for sock, v in zip(n.inputs, (a, b, c)):
            if v is None:
                continue
            if isinstance(v, bpy.types.NodeSocket):
                L(v, sock)
            else:
                sock.default_value = v
        return n.outputs[0]

    uv = N("ShaderNodeTexCoord").outputs["UV"]
    sep = N("ShaderNodeSeparateXYZ")
    L(uv, sep.inputs[0])
    u, v = sep.outputs["X"], sep.outputs["Y"]
    tau = 2 * math.pi
    cu, su = math_node("COSINE", math_node("MULTIPLY", u, tau)), math_node("SINE", math_node("MULTIPLY", u, tau))
    cv, sv = math_node("COSINE", math_node("MULTIPLY", v, tau)), math_node("SINE", math_node("MULTIPLY", v, tau))
    torus = N("ShaderNodeCombineXYZ")
    L(cu, torus.inputs[0]); L(su, torus.inputs[1]); L(cv, torus.inputs[2])

    def noise4d(scale, detail, seed_offset):
        n = N("ShaderNodeTexNoise")
        n.noise_dimensions = "4D"
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        off = N("ShaderNodeVectorMath")
        off.operation = "ADD"
        L(torus.outputs[0], off.inputs[0])
        off.inputs[1].default_value = (seed_offset, seed_offset * 1.7, seed_offset * 2.3)
        L(off.outputs[0], n.inputs["Vector"])
        L(math_node("ADD", sv, seed_offset * 3.1), n.inputs["W"])
        return n.outputs["Fac"]

    # ripples: phase = u·A + v·B (whole numbers → seamless), bent by big + small tileable noise
    phase = math_node("ADD", math_node("MULTIPLY", u, gin.outputs["Ripple A"]),
                      math_node("MULTIPLY", v, gin.outputs["Ripple B"]))
    warp = math_node("MULTIPLY", math_node("SUBTRACT", noise4d(0.9, 2.0, 0.0), 0.5), gin.outputs["Warp"])
    wiggle = math_node("MULTIPLY", math_node("SUBTRACT", noise4d(3.5, 2.0, 5.0), 0.5), gin.outputs["Wiggle"])
    # ×4 on warp: the noise above is in "ripples", and A/B give ~7 ripples per tile
    bent = math_node("ADD", phase, math_node("ADD", math_node("MULTIPLY", warp, 4.0), math_node("MULTIPLY", wiggle, 4.0)))
    saw = math_node("FRACT", bent)
    ripple = math_node("POWER", saw, 3.0)          # slow rise, sudden drop: the real ripple shape
    L(ripple, gout.inputs["Ripple"])

    # grain specks: 4D Voronoi on the torus; a few cells become a dot
    vor = N("ShaderNodeTexVoronoi")
    vor.voronoi_dimensions = "4D"
    vor.feature = "F1"
    L(torus.outputs[0], vor.inputs["Vector"])
    L(sv, vor.inputs["W"])
    L(math_node("DIVIDE", gin.outputs["Grain Scale"], tau), vor.inputs["Scale"])
    cell = N("ShaderNodeSeparateColor")
    L(vor.outputs["Color"], cell.inputs[0])
    picked = math_node("LESS_THAN", cell.outputs["Red"], gin.outputs["Grain Density"])
    dot = math_node("LESS_THAN", vor.outputs["Distance"], gin.outputs["Grain Size"])   # dot radius, in cells
    L(math_node("MULTIPLY", picked, dot), gout.inputs["Grain"])
    L(math_node("GREATER_THAN", cell.outputs["Green"], 0.5), gout.inputs["GrainLight"])

    # tone: big soft colour patches
    L(noise4d(0.6, 1.0, 9.0), gout.inputs["Tone"])
    return ng


def sand_material(group):
    mat = bpy.data.materials.new("SandTexture")
    nt = mat.node_tree
    nt.nodes.clear()
    N, L = nt.nodes.new, nt.links.new
    pat = N("ShaderNodeGroup")
    pat.node_tree = group
    pat.name = "SandPattern"

    def ramp(fac, stops):
        r = N("ShaderNodeValToRGB")
        for el, (pos, col) in zip(r.color_ramp.elements, stops):
            el.position, el.color = pos, (*col, 1)
        L(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    def mix(fac, a, b, blend="MIX"):
        m = N("ShaderNodeMix")
        m.data_type, m.blend_type = "RGBA", blend
        L(fac, m.inputs["Factor"]) if isinstance(fac, bpy.types.NodeSocket) else None
        if not isinstance(fac, bpy.types.NodeSocket):
            m.inputs["Factor"].default_value = fac
        ins = [s for s in m.inputs if s.type == "RGBA"]
        for s, val in ((ins[0], a), (ins[1], b)):
            if isinstance(val, bpy.types.NodeSocket):
                L(val, s)
            else:
                s.default_value = (*val, 1)
        return [s for s in m.outputs if s.type == "RGBA"][0]

    sand = ramp(pat.outputs["Tone"], [(0.35, (0.83, 0.66, 0.42)), (0.7, (0.92, 0.78, 0.55))])
    shade = ramp(pat.outputs["Ripple"], [(0.0, (0.9, 0.9, 0.9)), (1.0, (1.06, 1.05, 1.03))])
    col = mix(1.0, sand, shade, "MULTIPLY")
    speck = mix(pat.outputs["GrainLight"], (0.62, 0.48, 0.32), (1.0, 0.95, 0.85))
    col = mix(pat.outputs["Grain"], col, speck)
    bump = N("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.45
    bump.inputs["Distance"].default_value = 0.03
    L(pat.outputs["Ripple"], bump.inputs["Height"])
    bsdf = N("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.95
    L(col, bsdf.inputs["Base Color"])
    L(bump.outputs["Normal"], bsdf.inputs["Normal"])
    out = N("ShaderNodeOutputMaterial")
    L(bsdf.outputs[0], out.inputs["Surface"])
    # tidy layout
    for i, n in enumerate(nt.nodes):
        n.location = (-900 + (i % 6) * 220, 300 - (i // 6) * 260)
    out.location = (700, 0)
    bsdf.location = (420, 0)
    pat.location = (-900, 0)
    return mat


def scene(mat):
    col = bpy.data.collections.new("Sand")
    bpy.context.scene.collection.children.link(col)
    bpy.ops.mesh.primitive_plane_add(size=TILE, location=(0, 0, 0))
    tile = bpy.context.active_object
    tile.name = "SandTile"          # ONE tile = one texture repeat (UV 0..1): bake from this
    tile.data.materials.append(mat)
    for o in list(tile.users_collection):
        o.objects.unlink(tile)
    col.objects.link(tile)
    # tiling check: 3×3 copies sharing the mesh, right next to the tile → seams would show here
    check = bpy.data.collections.new("TilingCheck")
    bpy.context.scene.collection.children.link(check)
    for ix in range(-1, 2):
        for iy in range(-1, 2):
            o = bpy.data.objects.new(f"Tile_{ix + 1}{iy + 1}", tile.data)
            o.location = (ix * TILE + 10.0, iy * TILE, 0)
            check.objects.link(o)
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 3.0
    sun_data.angle = math.radians(3)
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(62), 0, math.radians(30))   # low sun: ripples cast relief
    bpy.context.scene.collection.objects.link(sun)
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.lens = 35
    cam = bpy.data.objects.new("Camera", cam_data)
    cam.location = (5.0, -13.5, 10.5)
    cam.rotation_euler = (math.radians(50), 0, 0)
    bpy.context.scene.collection.objects.link(cam)
    sc = bpy.context.scene
    sc.camera = cam
    world = bpy.data.worlds.new("World")
    sc.world = world
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.6, 0.75, 0.95, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.7
    sc.render.engine = "BLENDER_EEVEE"
    sc.view_settings.view_transform = "Standard"   # AgX greys the sand colours out
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900
    return tile


def macro_group() -> Group:
    """Four independent big soft noises (another seed each), one per texture channel.
    Cells = blobs across the 80 m tile (same sizes as step 12's noise2() calls)."""
    g = Group("SandMacro", [
        ("Tone Cells", 20.0, 1, 400, "Light/dark sand blobs (0.25 per metre)"),
        ("Wet Cells", 64.0, 1, 400, "Wobble of the wet-sand line (0.8 per metre)"),
        ("Patches Cells", 40.0, 1, 400, "Darker grass patches (0.5 per metre)"),
        ("Stripes Cells", 48.0, 1, 400, "Bend of the bank's earth stripes (0.6 per metre)"),
    ], ["Tone", "Wet", "Patches", "Stripes"])
    for i, name in enumerate(("Tone", "Wet", "Patches", "Stripes")):
        g.out(name, g.noise(g.inp(name + " Cells"), 41.0 + i * 7.0, label=name.lower()))
    g.tidy()
    return g


def macro_material(group: Group):
    """Preview: sand coloured by Tone, with the grass patches laid over it in green.
    Only for looking (the bake reads the group outputs); open node previews for each channel."""
    mat = new_material("SandMacro")
    nt = mat.node_tree
    pat = group_node(mat, group, "SandMacro")

    def ramp(fac, stops):
        r = nt.nodes.new("ShaderNodeValToRGB")
        for el, (pos, col) in zip(r.color_ramp.elements, stops):
            el.position, el.color = pos, (*col, 1)
        nt.links.new(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    sand = ramp(pat.outputs["Tone"], [(0.35, (0.98, 0.86, 0.62)), (0.75, (1.0, 0.92, 0.72))])
    grass = ramp(pat.outputs["Patches"], [(0.45, (0.42, 0.74, 0.3)), (0.6, (0.32, 0.62, 0.24))])
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type = "RGBA"
    m.inputs["Factor"].default_value = 0.35
    nt.links.new(sand, m.inputs[6])
    nt.links.new(grass, m.inputs[7])
    emit = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(m.outputs[2], emit.inputs["Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    emit.location, out.location = (200, 0), (500, 0)
    return mat


def add_macro():
    """MacroTile (80 m) + its 3×3 tiling check, well away from the 4 m ripple tile."""
    tile_plane("MacroTile", MACRO_TILE, macro_material(macro_group()), (120.0, 0.0, 0.0), "Macro")


def add_macro_and_save():
    add_macro()
    bpy.ops.wm.save_mainfile()
    print("macro tile added")


def main():
    clear()
    os.makedirs(OUT, exist_ok=True)
    mat = sand_material(pattern_group())
    scene(mat)
    add_macro()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "sand_texture.blend"))
    bpy.context.scene.render.filepath = "/tmp/sand_texture_preview.png"
    bpy.ops.render.render(write_still=True)
    print("sand texture done")


if __name__ == "__main__":
    main()
