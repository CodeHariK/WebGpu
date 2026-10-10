"""Bake sand_texture.blend's MacroTile into sand_macro.png: the four BIG soft noises of sand step 13.

Sand step 12 still called noise2() four times per pixel (4 texture reads + maths each):
  tone    noise2(p * 0.25)  sand light/dark blobs        → R
  wet     noise2(p * 0.8)   wobble of the wet-sand line  → G
  patches noise2(p * 0.5)   grass light/dark patches     → B
  stripes noise2(p * 0.6)   bend of the bank stripes     → A (stored as 0.5 + 0.5·v, see below)
MacroTile (80 m, node group SandMacro) makes these as Blender noise on the 4D torus, so the tile
repeats with no seam and the blobs are round (value noise, used before, lines its blobs up with
its square grid). Step 13 reads all four with one texture read.

Why not put them in sand_ripples.png? That tile repeats every 4 m; these blobs are 1–4 m wide, so
a 4 m repeat would show as a grid. They get their own big tile (80 m) instead.

Each channel is stretched to 0..1 (Blender's noise sits mostly in 0.25..0.75; step 13's
thresholds were tuned for noise that uses the whole range). A is remapped to 0.5..1 so the PNG
never has (nearly) transparent pixels: viewers show it normally and no tool can throw away the
RGB under transparent pixels. The shader undoes it with a·2 − 1.

Run (from this folder, after tweaking SandMacro in the GUI and saving):
  Blender -b sand_texture.blend --python bake_sand_macro.py
Shared code: bake_lib/blender_tile.py (baking) and bake_lib/tile_noise.py (saving).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
sys.path.insert(0, os.path.join(HERE, "..", "bake_lib"))
from blender_tile import bake_outputs, stretch  # noqa: E402
from tile_noise import opaque, save_png  # noqa: E402

SIZE = 512

names = ["Tone", "Wet", "Patches", "Stripes"]
baked = bake_outputs("MacroTile", "SandMacro", names, SIZE)
channels = [stretch(baked[n]) for n in names]
channels[3] = opaque(channels[3])
save_png(np.stack(channels, axis=-1), os.path.join(HERE, "sand_macro.png"))
print("sand macro bake done")
