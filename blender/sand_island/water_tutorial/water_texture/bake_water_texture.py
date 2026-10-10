"""Bake water_texture.blend into water_macro.png for water step 12 (data PNG, row 0 = top).

Run (from this folder, after tweaking the node group in the GUI and saving):
  Blender -b water_texture.blend --python bake_water_texture.py
The .blend is not saved by the bake, so the temporary bake nodes never end up in it.

Water step 11 per pixel: noise(vec3(p·0.6, TIME·0.2)) for the colour wobble and
noise(vec3(p·0.9, TIME·0.3)) for where the foam lines break up: 3D noise (2D + time), 2 texture
reads each. A baked texture cannot hold "all of time", so step 12 fakes the change over time:
two versions of each noise, cross-faded with a slow sin(TIME) while the texture drifts.

  water_macro.png  256², WaterTile (40 m), group WaterNoise, filtered at world xz / 40 + drift
    R, G = Wobble A, B · B, A = Lines A, B (A stored as 0.5 + 0.5·v: never transparent;
    the shader does a·2 − 1)
Each channel is stretched to 0..1 (Blender's noise sits mostly in 0.25..0.75; step 12's
thresholds were tuned for noise that uses the whole range).
256² is enough: the smallest blob is 1.1 m = 7 pixels wide, and the filter blends between pixels.
Shared code: bake_lib/blender_tile.py (baking) and bake_lib/tile_noise.py (saving).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
sys.path.insert(0, os.path.join(HERE, "..", "bake_lib"))
from blender_tile import bake_outputs, stretch  # noqa: E402
from tile_noise import opaque, save_png  # noqa: E402

SIZE = 256

names = ["Wobble A", "Wobble B", "Lines A", "Lines B"]
baked = bake_outputs("WaterTile", "WaterNoise", names, SIZE)
channels = [stretch(baked[n]) for n in names]
channels[3] = opaque(channels[3])
save_png(np.stack(channels, axis=-1), os.path.join(HERE, "water_macro.png"))
print("water bake done")
