"""Bake snow_texture.blend into the three textures of snow step 9 (data PNGs, row 0 = top).

Run (from this folder, after tweaking the node groups in the GUI and saving):
  Blender -b snow_texture.blend --python bake_snow_texture.py
The .blend is not saved by the bake, so the temporary bake nodes never end up in it.

1. snow_drifts.png  512², DriftTile (20 m), group SnowDrifts, sampled filtered at world xz / 20
     R = drift height (0 trough … 1 crest) · G, B = drift slope along x, z (0.5 + slope / (2·S))
     A = 1 (unused; the GPU pads RGB to RGBA anyway)
   The slope is computed here from the height, wrapping around the edges (the tile repeats).
2. snow_macro.png   512², MacroTile (80 m), group SnowMacro, filtered at world xz / 80
     R = rock tone · G = strata bend · B = ragged snow edge · A = blue patches (0.5 + 0.5·v)
   Each channel is stretched to 0..1 (Blender's noise sits mostly in 0.25..0.75; step 9's
   thresholds were tuned for noise that uses the whole range).
3. snow_sparkle.png 128², read per grain with texelFetch (no filtering), like tex_hash
     R, G, B = the grain mirror's random tilt · A = random "is it a mirror?" (0.5 + 0.5·v)
   Plain random numbers, no scene needed.
A channels are stored as 0.5 + 0.5·v so the PNGs are never transparent; the shader does a·2 − 1.
Shared code: bake_lib/blender_tile.py (baking) and bake_lib/tile_noise.py (slope, saving).
"""
import os
import sys

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
sys.path.insert(0, os.path.join(HERE, "..", "bake_lib"))
from blender_tile import bake_outputs, stretch  # noqa: E402
from tile_noise import opaque, save_png, slope  # noqa: E402

SIZE = 512
S = 4.0              # slope range stored: ±4 height per metre (snow step 9's slope_range)
SPARKLE_SIZE = 128
SEED = 11

# --- 1. drifts
drift_tile = bpy.data.objects["DriftTile"].dimensions.x
height = bake_outputs("DriftTile", "SnowDrifts", ["Drift"], SIZE)["Drift"]
dx, dz = slope(height, drift_tile)
drifts = np.stack([height,
                   np.clip(0.5 + dx / (2 * S), 0, 1),
                   np.clip(0.5 + dz / (2 * S), 0, 1),
                   np.ones_like(height)], axis=-1)
print("drift tile %.0f m, slope clipped %.1f %%" % (drift_tile, float((np.abs(dx) > S).mean() * 100)))
save_png(drifts, os.path.join(HERE, "snow_drifts.png"))

# --- 2. macro
names = ["Tone", "Strata", "Ragged", "Patches"]
baked = bake_outputs("MacroTile", "SnowMacro", names, SIZE)
macro = [stretch(baked[n]) for n in names]
macro[3] = opaque(macro[3])
save_png(np.stack(macro, axis=-1), os.path.join(HERE, "snow_macro.png"))

# --- 3. sparkle grains: plain random numbers, one texel per grain
grains = np.random.default_rng(SEED).random((SPARKLE_SIZE, SPARKLE_SIZE, 4), dtype=np.float32)
grains[..., 3] = opaque(grains[..., 3])
save_png(grains, os.path.join(HERE, "snow_sparkle.png"))
print("snow bake done")
