"""Numpy helpers shared by the bake scripts (sand_texture/, snow_texture/, water_texture/):
slope from height, the opaque-alpha encoding, and saving a data PNG. The patterns themselves
come from the .blend scenes (bake_lib/blender_tile.py bakes them).

Orientation: every array here is [row, column] = [z, x] with row 0 at the TOP, which is how Godot
reads a texture (v = 0 at the top). A shader that samples texture(tex, world_pos.xz / tile) then
sees exactly this array laid over the ground. save_png flips it for Blender (bottom-up rows).
"""
import numpy as np


def slope(height: np.ndarray, tile: float) -> tuple:
    """Height change per metre along x and along z, from neighbouring pixels (wrapping around the
    edges, because the tile repeats). Returns (d/dx, d/dz)."""
    per_metre = height.shape[0] / tile
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * per_metre
    dz = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * per_metre
    return dx, dz


def opaque(v: np.ndarray) -> np.ndarray:
    """Store a 0..1 value in ALPHA as 0.5..1, so the PNG is never (nearly) transparent: viewers show
    it normally and no tool throws away the RGB under transparent pixels. Shader: v = a * 2 - 1."""
    return 0.5 + 0.5 * v


def save_png(rgba: np.ndarray, path: str) -> None:
    """Save a [rows, columns, 4] float array (row 0 = top) as an 8-bit RGBA data PNG."""
    import bpy

    rows, cols, _ = rgba.shape
    img = bpy.data.images.new("bake", cols, rows, alpha=True)
    img.colorspace_settings.name = "Non-Color"
    img.alpha_mode = "STRAIGHT"
    img.pixels = np.ascontiguousarray(rgba[::-1], dtype=np.float32).ravel()   # Blender: bottom-up
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    for name, ch in zip("RGBA", np.moveaxis(rgba, -1, 0)):
        print("  %s min %.3f max %.3f" % (name, float(ch.min()), float(ch.max())))
    print("saved", path)
