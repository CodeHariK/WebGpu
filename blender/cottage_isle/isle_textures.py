"""Painted detail textures, made with numpy (tileable, 512²) and box UVs to put them on.

Each texture is a grey "detail" map: 1.0 shows the palette colour as is, lower values are the
painted darker strokes, blotches and grain. Materials multiply palette colour × detail, so a
handful of maps cover every material (and the .glb gets baseColorTexture × baseColorFactor).

  grass   soft blotches with darker watercolour rims + brush strokes in random directions
  earth   wobbly strata bands + grit (cliffs, soil)
  sand    pale blotches + speckle (paths, plaza, stone)
  wood    grain lines + a few knots
  plaster soft mottling (walls, roof, cloth)
  leaf    blotches + short strokes (leaves, bushes, ferns)
  water   long streaks + ripples
  paper   fine fibres, for the watercolour paper overlay (screen space, Blender only)
"""
import math
import os

import bpy
import numpy as np

SIZE = 512
KINDS = ("grass", "earth", "sand", "wood", "plaster", "leaf", "water", "paper")
# which detail map each palette colour uses, and how many metres one tile of it covers
TEXTURE_OF = {
    "grass": "grass", "grass_dark": "grass", "dirt": "earth", "rock": "earth", "soil": "earth",
    "path": "sand", "tile": "sand", "groove": "sand", "stone": "sand", "wall": "plaster",
    "roof": "plaster", "roof_dark": "plaster", "awning": "plaster", "cloth": "plaster", "smoke": "plaster",
    "timber": "wood", "timber_dark": "wood", "wood": "wood", "door": "wood", "stake": "wood",
    "rail": "wood", "stump": "wood", "basket": "wood", "trunk": "wood", "leaf": "leaf",
    "leaf_light": "leaf", "bush": "leaf", "fern": "leaf", "sprout": "leaf", "water": "water",
}
TILE_METRES = {"grass": 3.0, "earth": 2.5, "sand": 2.0, "wood": 1.0, "plaster": 1.5, "leaf": 1.2, "water": 2.0}


# --- tileable noise ------------------------------------------------------------------------------

def value_noise(rng, cells):
    """Smooth noise that wraps at the edges (a random grid, cubic-interpolated with wrap-around)."""
    grid = rng.random((cells, cells))
    t = np.arange(SIZE) * cells / SIZE
    i0 = np.floor(t).astype(int)
    f = t - i0
    f = f * f * (3 - 2 * f)
    i1 = (i0 + 1) % cells
    rows = grid[i0][:, i0] * (1 - f)[None, :] + grid[i0][:, i1] * f[None, :]
    rows1 = grid[i1][:, i0] * (1 - f)[None, :] + grid[i1][:, i1] * f[None, :]
    return rows * (1 - f)[:, None] + rows1 * f[:, None]


def fbm(rng, cells=4, octaves=5):
    total, amp, norm = np.zeros((SIZE, SIZE)), 1.0, 0.0
    for o in range(octaves):
        total += amp * value_noise(rng, cells * 2 ** o)
        norm += amp
        amp *= 0.5
    return total / norm


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def rims(mask):
    """Watercolour edge darkening: pigment pools where a blotch ends (gradient magnitude)."""
    gx = np.roll(mask, 1, 1) - np.roll(mask, -1, 1)
    gy = np.roll(mask, 1, 0) - np.roll(mask, -1, 0)
    g = np.sqrt(gx * gx + gy * gy)
    return np.clip(g / (g.max() + 1e-6), 0, 1)


def strokes(rng, count, length, width, angle=None, jitter=math.pi):
    """Soft elongated brush dabs (signed: some lighter, some darker), wrapped."""
    canvas = np.zeros((SIZE, SIZE))
    half = int(length) + 2
    yy, xx = np.mgrid[-half:half + 1, -half:half + 1]
    for _ in range(count):
        a = (angle if angle is not None else 0.0) + rng.uniform(-jitter, jitter)
        u = xx * math.cos(a) + yy * math.sin(a)
        v = -xx * math.sin(a) + yy * math.cos(a)
        dab = np.exp(-(u / (length * rng.uniform(0.6, 1.0))) ** 2 * 2 - (v / width) ** 2)
        sign = rng.choice((-1.0, 1.0), p=(0.6, 0.4))
        cx, cy = rng.integers(0, SIZE, 2)
        rows = (np.arange(-half, half + 1) + cy) % SIZE
        cols = (np.arange(-half, half + 1) + cx) % SIZE
        canvas[np.ix_(rows, cols)] += sign * dab * rng.uniform(0.4, 1.0)
    return canvas


# --- the maps ------------------------------------------------------------------------------------

def _grass(rng):
    blot = smoothstep(0.42, 0.62, fbm(rng, 3, 4))
    d = 0.92 + 0.08 * blot - 0.16 * rims(blot) ** 0.7
    d += 0.05 * np.tanh(strokes(rng, 520, 26, 3.2))
    d -= 0.05 * smoothstep(0.6, 0.9, fbm(rng, 16, 3))
    return d


def _earth(rng):
    warp = fbm(rng, 3, 4)
    y = np.arange(SIZE)[:, None] / SIZE
    bands = 0.5 + 0.5 * np.sin((y * 9 + warp * 1.6) * math.tau)
    d = 0.86 + 0.1 * smoothstep(0.3, 0.8, bands) - 0.1 * rims(smoothstep(0.45, 0.55, bands))
    d += 0.05 * np.tanh(strokes(rng, 300, 30, 2.5, angle=0.0, jitter=0.25))
    d -= 0.06 * smoothstep(0.65, 0.85, fbm(rng, 32, 2))
    return d


def _sand(rng):
    blot = smoothstep(0.4, 0.65, fbm(rng, 4, 4))
    d = 0.9 + 0.08 * blot - 0.1 * rims(blot)
    d -= 0.07 * smoothstep(0.7, 0.9, fbm(rng, 48, 2))
    return d


def _wood(rng):
    warp = fbm(rng, 2, 4)
    y = np.arange(SIZE)[:, None] / SIZE
    grain = np.abs(np.sin((y * 14 + warp * 0.9) * math.tau)) ** 6
    d = 0.95 - 0.16 * grain + 0.05 * (fbm(rng, 8, 3) - 0.5)
    d += 0.04 * np.tanh(strokes(rng, 260, 40, 2.0, angle=0.0, jitter=0.08))
    return d


def _plaster(rng):
    blot = smoothstep(0.45, 0.6, fbm(rng, 3, 5))
    return 0.93 + 0.06 * blot - 0.09 * rims(blot) - 0.04 * smoothstep(0.7, 0.9, fbm(rng, 40, 2))


def _leaf(rng):
    blot = smoothstep(0.4, 0.6, fbm(rng, 4, 4))
    d = 0.88 + 0.1 * blot - 0.14 * rims(blot)
    return d + 0.06 * np.tanh(strokes(rng, 600, 14, 3.5))


def _water(rng):
    x = np.arange(SIZE)[None, :] / SIZE
    warp = fbm(rng, 3, 4)
    streak = np.abs(np.sin((x * 10 + warp * 0.7) * math.tau)) ** 8
    return 0.93 + 0.07 * streak + 0.04 * np.tanh(strokes(rng, 300, 50, 2.0, angle=math.pi / 2, jitter=0.1))


def _paper(rng):
    fibres = np.tanh(strokes(rng, 1400, 18, 0.8)) * 0.5 + 0.5
    return 0.94 + 0.06 * fibres - 0.03 * fbm(rng, 64, 2)


MAKERS = {"grass": _grass, "earth": _earth, "sand": _sand, "wood": _wood, "plaster": _plaster,
          "leaf": _leaf, "water": _water, "paper": _paper}


def image_name(kind):
    return f"tex_{kind}"


def make_textures(folder, seed=7):
    """Generate every map, save it as textures/<kind>.png next to the scene and load it."""
    os.makedirs(folder, exist_ok=True)
    for k, kind in enumerate(KINDS):
        detail = np.clip(MAKERS[kind](np.random.default_rng(seed + k)), 0, 1).astype(np.float32)
        name = image_name(kind)
        if name in bpy.data.images:
            bpy.data.images.remove(bpy.data.images[name])
        img = bpy.data.images.new(name, SIZE, SIZE, alpha=False)
        rgba = np.ones((SIZE, SIZE, 4), np.float32)
        rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = detail
        img.pixels.foreach_set(rgba.ravel())
        img.filepath_raw = os.path.join(folder, f"{kind}.png")
        img.file_format = "PNG"
        img.save()
        img.colorspace_settings.name = "Non-Color"


# --- box UVs -------------------------------------------------------------------------------------

def box_uv(obj, metres):
    """World-space box mapping: each face takes the two axes its normal isn't pointing along."""
    mesh = obj.data
    layer = mesh.uv_layers.get("UVMap") or mesh.uv_layers.new(name="UVMap")
    world = obj.matrix_world
    normal_matrix = world.to_3x3().inverted().transposed()
    for poly in mesh.polygons:
        n = normal_matrix @ poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        a, b = [(1, 2), (0, 2), (0, 1)][ax]
        for li in poly.loop_indices:
            p = world @ mesh.vertices[mesh.loops[li].vertex_index].co
            layer.data[li].uv = (p[a] / metres, p[b] / metres)


def uv_everything(objects):
    for obj in objects:
        if obj.type != "MESH" or not obj.data.materials or not obj.data.materials[0]:
            continue
        palette = obj.data.materials[0].get("palette", "")
        box_uv(obj, TILE_METRES.get(TEXTURE_OF.get(palette, ""), 2.0))
