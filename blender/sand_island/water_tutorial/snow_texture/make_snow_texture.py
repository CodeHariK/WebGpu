"""Build snow_texture/snow_texture.blend: the snow's tileable patterns, to look at and tweak by eye.

Run:  Blender -b --python make_snow_texture.py
WARNING: rebuilds the .blend from scratch, so edits made in the Blender GUI are lost.
To only re-bake after tweaking, use bake_snow_texture.py instead.

Two tile planes (one plane = one texture repeat, UV 0..1), each with a 3×3 tiling check below it:
  DriftTile  20 m  node group SnowDrifts → Drift (0 trough … 1 crest)       → snow_drifts.png
  MacroTile  80 m  node group SnowMacro  → Tone, Strata, Ragged, Patches   → snow_macro.png
Tweak the inputs on the group nodes in each material (all noise sizes are "cells" = blobs across
the tile; any value tiles, thanks to the 4D-torus trick in bake_lib/blender_tile.py).
The sparkle texture is plain random numbers, so it has no scene: bake_snow_texture.py makes it.
"""
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
sys.path.insert(0, os.path.join(HERE, "..", "bake_lib"))
from blender_tile import Group, group_node, light_camera_world, new_material, tile_plane  # noqa: E402

DRIFT_TILE = 20.0   # metres per repeat (snow step 9's drift_tile)
MACRO_TILE = 80.0   # metres per repeat (snow step 9's macro_tile)


def drift_group() -> Group:
    """Wind drifts, the same idea as shader_lib/ripple.gdshaderinc, made tileable:
    phase = u·Drifts X − v·Drifts Z (whole numbers → the stripes meet across the edges;
    minus because Blender's v points the other way from Godot's z), bent by two noises,
    then fract → sawtooth and pow 3 → long gentle slope, short steep face."""
    g = Group("SnowDrifts", [
        ("Drifts X", 4.0, -20, 20, "Whole number: drifts across the tile along x"),
        ("Drifts Z", 10.0, -20, 20, "Whole number: drifts across the tile along z"),
        ("Warp", 3.0, 0, 10, "How far the big noise bends the drifts (in drifts)"),
        ("Warp Cells", 7.0, 1, 100, "Size of the big bends: blobs across the tile"),
        ("Wiggle", 0.6, 0, 5, "How far the small noise wiggles them"),
        ("Wiggle Cells", 22.0, 1, 200, "Size of the wiggles"),
        ("Sharpness", 3.0, 1, 8, "pow(): higher = longer gentle slope, steeper face"),
    ], ["Drift"])
    phase = g.math("SUBTRACT", g.math("MULTIPLY", g.u, g.inp("Drifts X")), g.math("MULTIPLY", g.v, g.inp("Drifts Z")))
    # ×2: Blender's noise swings about 2× less than the shaders' value noise
    warp = g.math("MULTIPLY", g.math("SUBTRACT", g.noise(g.inp("Warp Cells"), 0.0, label="warp"), 0.5),
                  g.math("MULTIPLY", g.inp("Warp"), 2.0))
    wiggle = g.math("MULTIPLY", g.math("SUBTRACT", g.noise(g.inp("Wiggle Cells"), 5.0, label="wiggle"), 0.5),
                    g.math("MULTIPLY", g.inp("Wiggle"), 2.0))
    saw = g.math("FRACT", g.math("ADD", phase, g.math("ADD", warp, wiggle)))
    g.out("Drift", g.math("POWER", saw, g.inp("Sharpness")))
    g.tidy()
    return g


def macro_group() -> Group:
    """Four independent big soft noises (another seed each), one per texture channel."""
    g = Group("SnowMacro", [
        ("Tone Cells", 32.0, 1, 400, "Rock light/dark blobs across 80 m (0.4 per metre)"),
        ("Strata Cells", 40.0, 1, 400, "Bend of the rock strata stripes"),
        ("Ragged Cells", 104.0, 1, 400, "Raggedness of the snow edge (small = smooth edge)"),
        ("Patches Cells", 24.0, 1, 400, "Blue-tinted snow patches"),
    ], ["Tone", "Strata", "Ragged", "Patches"])
    for i, name in enumerate(("Tone", "Strata", "Ragged", "Patches")):
        g.out(name, g.noise(g.inp(name + " Cells"), 11.0 + i * 7.0, label=name.lower()))
    g.tidy()
    return g


def ramp(nt, fac, stops):
    r = nt.nodes.new("ShaderNodeValToRGB")
    for el, (pos, col) in zip(r.color_ramp.elements, stops):
        el.position, el.color = pos, (*col, 1)
    nt.links.new(fac, r.inputs["Fac"])
    return r.outputs["Color"]


def drift_material(group: Group):
    """Preview: white snow, blue-grey troughs, the drift height as bump. Only for looking:
    the bake reads the group's Drift output, not this colour."""
    mat = new_material("SnowDrifts")
    nt = mat.node_tree
    pat = group_node(mat, group, "SnowDrifts")
    col = ramp(nt, pat.outputs["Drift"], [(0.0, (0.72, 0.78, 0.92)), (1.0, (0.97, 0.98, 1.0))])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.6
    bump.inputs["Distance"].default_value = 0.08
    nt.links.new(pat.outputs["Drift"], bump.inputs["Height"])
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.9
    nt.links.new(col, bsdf.inputs["Base Color"])
    nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(bsdf.outputs[0], out.inputs["Surface"])
    bsdf.location, out.location = (200, 0), (500, 0)
    return mat


def macro_material(group: Group):
    """Preview: the snow colour with the four noises applied roughly like step 9 does:
    tone tints the snow, patches turn it blue, ragged + strata show as darker rock-grey veins.
    Open the node previews to see each channel on its own."""
    mat = new_material("SnowMacro")
    nt = mat.node_tree
    pat = group_node(mat, group, "SnowMacro")
    snow = ramp(nt, pat.outputs["Patches"], [(0.45, (0.86, 0.88, 0.92)), (0.85, (0.74, 0.8, 0.93))])
    tone = ramp(nt, pat.outputs["Tone"], [(0.0, (0.9, 0.9, 0.9)), (1.0, (1.05, 1.05, 1.05))])
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type, m.blend_type = "RGBA", "MULTIPLY"
    m.inputs["Factor"].default_value = 1.0
    nt.links.new(snow, m.inputs[6])
    nt.links.new(tone, m.inputs[7])
    veins = ramp(nt, pat.outputs["Ragged"], [(0.75, (1, 1, 1)), (0.9, (0.55, 0.58, 0.68))])
    m2 = nt.nodes.new("ShaderNodeMix")
    m2.data_type, m2.blend_type = "RGBA", "MULTIPLY"
    m2.inputs["Factor"].default_value = 1.0
    nt.links.new(m.outputs[2], m2.inputs[6])
    nt.links.new(veins, m2.inputs[7])
    emit = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(m2.outputs[2], emit.inputs["Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    emit.location, out.location = (200, 0), (500, 0)
    return mat


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    tile_plane("DriftTile", DRIFT_TILE, drift_material(drift_group()), (0.0, 0.0, 0.0), "Drifts")
    tile_plane("MacroTile", MACRO_TILE, macro_material(macro_group()), (120.0, 0.0, 0.0), "Macro")
    light_camera_world(cam_loc=(0.0, -20.0, 60.0), cam_rot_deg=(0, 0, 0), ortho_scale=70.0)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "snow_texture.blend"))
    print("snow texture scene done")


main()
