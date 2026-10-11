"""Mesh variants: more than one mesh for the same hash.

Name extra meshes with a "_v<number>" ending: "00001111_circle", "00001111_circle_v2",
"00001111_circle_v3". No ending counts as variant 1. Blender's own ".001" is ignored
("00001111_circle_v2.001" is still variant 2). Don't use ".2": Blender and the exporter cut
names at the first ".".

Godot picks a variant per cell from the cell's grid position; cell_seed / variant_index are a
copy of src/marching_cubes/core/mc_variant.h, so the playground shows the same choice.
"""

import re

_VARIANT = re.compile(r"_v(\d+)(?:_\d+)?$")  # "_v2", or "_v2_001" as Godot renames ".001"
_MASK = 0xFFFFFFFF


def variant_number(name):
    """2 for "00001111_circle_v2" (or "..._v2.001"), 1 when there is no _v ending."""
    match = _VARIANT.search(name.split(".")[0])
    if match is None:
        return 1
    return max(1, int(match.group(1)))


def cell_seed(x, y, z):
    """32-bit hash of a cell position: same maths as mc::cell_seed in C++ (uint32 wrap-around)."""
    h = ((x * 73856093) & _MASK) ^ ((y * 19349663) & _MASK) ^ ((z * 83492791) & _MASK)
    h ^= h >> 16
    h = (h * 0x85EBCA6B) & _MASK
    h ^= h >> 13
    h = (h * 0xC2B2AE35) & _MASK
    h ^= h >> 16
    return h


def variant_index(x, y, z, count):
    """Which of count variants the cell at (x, y, z) uses, in Godot grid coordinates."""
    return 0 if count <= 1 else cell_seed(x, y, z) % count
