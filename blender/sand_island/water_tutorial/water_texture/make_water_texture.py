"""Build water_texture/water_texture.blend: the water's tileable noises, to look at and tweak by eye.

Run:  Blender -b --python make_water_texture.py
WARNING: rebuilds the .blend from scratch, so edits made in the Blender GUI are lost.
To only re-bake after tweaking, use bake_water_texture.py instead.

One tile plane (one texture repeat, UV 0..1) with a 3×3 tiling check below it:
  WaterTile  40 m  node group WaterNoise → Wobble A, Wobble B, Lines A, Lines B → water_macro.png
Water step 12 cross-fades A ↔ B with sin(TIME), so the two versions of each noise must be
different but the same size. Noise sizes are "cells" = blobs across the tile (any value tiles,
thanks to the 4D-torus trick in bake_lib/blender_tile.py).
"""
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
sys.path.insert(0, os.path.join(HERE, "..", "bake_lib"))
from blender_tile import Group, group_node, light_camera_world, new_material, tile_plane  # noqa: E402

TILE = 40.0   # metres per repeat (water step 12's macro_tile)


def noise_group() -> Group:
    g = Group("WaterNoise", [
        ("Wobble Cells", 24.0, 1, 400, "Colour wobble blobs across 40 m (0.6 per metre, as step 11)"),
        ("Lines Cells", 36.0, 1, 400, "Where the foam lines break up (0.9 per metre)"),
    ], ["Wobble A", "Wobble B", "Lines A", "Lines B"])
    g.out("Wobble A", g.noise(g.inp("Wobble Cells"), 3.0, label="wobble A"))
    g.out("Wobble B", g.noise(g.inp("Wobble Cells"), 13.0, label="wobble B"))
    g.out("Lines A", g.noise(g.inp("Lines Cells"), 23.0, label="lines A"))
    g.out("Lines B", g.noise(g.inp("Lines Cells"), 33.0, label="lines B"))
    g.tidy()
    return g


def ramp(nt, fac, stops):
    r = nt.nodes.new("ShaderNodeValToRGB")
    for el, (pos, col) in zip(r.color_ramp.elements, stops):
        el.position, el.color = pos, (*col, 1)
    nt.links.new(fac, r.inputs["Fac"])
    return r.outputs["Color"]


def water_material(group: Group):
    """Preview: blue water whose shade wobbles with Wobble A, white where Lines A passes the
    step's 0.45 threshold (in the game it is only drawn along the depth lines, not everywhere).
    Switch the links to the B outputs to see the other versions."""
    mat = new_material("WaterNoise")
    nt = mat.node_tree
    pat = group_node(mat, group, "WaterNoise")
    water = ramp(nt, pat.outputs["Wobble A"], [(0.0, (0.12, 0.42, 0.85)), (1.0, (0.25, 0.75, 0.88))])
    lines = ramp(nt, pat.outputs["Lines A"], [(0.44, (0, 0, 0)), (0.46, (0.35, 0.35, 0.35))])
    m = nt.nodes.new("ShaderNodeMix")
    m.data_type, m.blend_type = "RGBA", "ADD"
    m.inputs["Factor"].default_value = 1.0
    nt.links.new(water, m.inputs[6])
    nt.links.new(lines, m.inputs[7])
    emit = nt.nodes.new("ShaderNodeEmission")
    nt.links.new(m.outputs[2], emit.inputs["Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(emit.outputs[0], out.inputs["Surface"])
    emit.location, out.location = (200, 0), (500, 0)
    return mat


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    tile_plane("WaterTile", TILE, water_material(noise_group()), (0.0, 0.0, 0.0), "Water")
    light_camera_world(cam_loc=(0.0, -40.0, 120.0), cam_rot_deg=(0, 0, 0), ortho_scale=140.0)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "water_texture.blend"))
    print("water texture scene done")


main()
