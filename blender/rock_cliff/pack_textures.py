"""Shrink the limestone PBR set to what a mobile game needs: 2 textures instead of 6.

  albedo   ← albedo × ambient occlusion (AO baked in, so no separate AO texture)
  normal   ← normal (OpenGL / +Y, which both glTF and Godot expect)
Dropped: metallic (all black), roughness (almost constant ≈ 0.95 → a material value),
height (used only in Blender to shape the mesh), the separate AO (baked into albedo).

Usage: python3 pack_textures.py [size]   (default 1024)
"""
import sys
from pathlib import Path

from PIL import Image, ImageChops

SRC = Path(__file__).resolve().parent.parent / "texture" / "limestone-cliffs-bl"
OUT = Path(__file__).resolve().parent / "textures"
AO_STRENGTH = 0.8  # 1 = full AO, 0 = none


def load(name, size, mode="RGB"):
    return Image.open(SRC / f"limestone-cliffs_{name}.png").convert(mode).resize((size, size), Image.LANCZOS)


def main(size):
    OUT.mkdir(exist_ok=True)
    albedo = load("albedo", size)
    ao = load("ao", size, "L").point(lambda v: int(255 - (255 - v) * AO_STRENGTH))
    ImageChops.multiply(albedo, Image.merge("RGB", (ao, ao, ao))).save(OUT / f"limestone_albedo_{size}.png", optimize=True)
    load("normal-ogl", size).save(OUT / f"limestone_normal_{size}.png", optimize=True)


if __name__ == "__main__":
    main(int(sys.argv[1]) if len(sys.argv) > 1 else 1024)
