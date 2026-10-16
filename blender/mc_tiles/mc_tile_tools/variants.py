"""Mesh variants: more than one mesh for the same hash, tagged by height, group and number.

A tile's tag is the "_<height><group><number>" token after its hash digits:
    height   l / m / h   ground / middle / roof. Not interchangeable: the grid picks the height
                         (L only on the ground layer, H only on roof cells, side neighbours always
                         the same height).
    group    a .. z      a style family; one group is used per region, falling back to "a".
    number   1, 2, ..    interchangeable alternatives of one tile; picked per cell at random.
    "00001111_ma1"      middle, group a, 1 (the clean form always carries the tag)
    "00001111"          also middle, group a, 1 (no tag: the defaults; still read)
    "00001111_ma2"      middle, group a, 2
    "00001111_hb1"      roof, group b, 1
    "00001111_v2"       old form: middle, group a, 2 (still read, so older sets keep working)
Blender's own ".001" is ignored ("00001111_ma2.001" is still ma2). A tile's "role" is its
(height, group): Check, Playground and the like work on one role; Export writes every role.

Godot picks a variant number per cell from the cell's grid position; cell_seed / variant_index
are a copy of src/marching_cubes/core/mc_variant.h, so the playground shows the same choice.
"""

import re

_VARIANT = re.compile(r"_v(\d+)(?:_\d+)?$")  # "_v2", or "_v2_001" as Godot renames ".001"
_TAG = re.compile(r"^([lmh])([a-z])(\d+)$")  # "ma2"
_MASK = 0xFFFFFFFF

HEIGHTS = ("l", "m", "h")
MAIN_ROLE = ("m", "a")  # middle, group a: what an untagged tile is
HEIGHT_NAMES = {"l": "Ground (L)", "m": "Middle (M)", "h": "Roof (H)"}


def name_tag(name):
    """(height, group, number) of a tile name: ("m", "a", 2) for "00001111_ma2" or the old
    "00001111_circle_v2"; ("m", "a", 1) when there is no tag."""
    for token in name.split(".")[0].split("_")[1:]:
        match = _TAG.match(token)
        if match:
            return match.group(1), match.group(2), max(1, int(match.group(3)))
    match = _VARIANT.search(name.split(".")[0])
    return MAIN_ROLE + (max(1, int(match.group(1))) if match else 1,)


def name_role(name):
    """(height, group) of a tile name."""
    return name_tag(name)[:2]


def tag_text(height, group, number):
    """"ma2" for ("m", "a", 2)."""
    return f"{height}{group}{number}"


def role_text(role):
    """"M a" for ("m", "a")."""
    return f"{role[0].upper()} {role[1]}"


def variant_number(name):
    """The tile's number within its role: 2 for "00001111_ma2" or "00001111_circle_v2" (or
    "..._v2.001"), 1 when there is none."""
    return name_tag(name)[2]


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
