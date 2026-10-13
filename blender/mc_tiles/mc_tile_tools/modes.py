"""Cube tile-set modes: which tiles a set must contain.

| Mode (id)                                      | Rotations used in Godot | Tiles to model |
|------------------------------------------------|-------------------------|----------------|
| X=Z, Y Asymmetric (68)                         | 4 around Y              | 68             |
| Complete (256)                                 | none                    | 254            |
| X=Z, Y Asymmetric Heightmap (22), id HEIGHTMAP | 4 around Y, no overhang | 22             |

X=Z, Y Asymmetric (both modes): the cell is square in X/Z so quarter turns about Y fit; tops
and bottoms differ, so tiles are never turned or flipped upside down.

Empty (00000000) is never drawn. Full (11111111) is optional: not counted,
but Godot draws it if the set has one.
The panel uses the counts computed from the rotations in cube.py; these typed
numbers are kept as documentation and as a cross-check for tests.
"""

MODE_ITEMS = [
    # Identifiers ("68", "256", ...) are saved in .blend files: rename labels only.
    # Godot needs no mode: it turns whatever tiles it loads about Y and fills the gaps.
    ("68", "X=Z, Y Asymmetric (68)", "Square in X/Z: Godot turns tiles about Y only, never upside down"),
    ("256", "Complete (256)", "Every corner combination is modelled, no rotation"),
    ("HEIGHTMAP", "X=Z, Y Asymmetric Heightmap (22)", "Like X=Z, Y Asymmetric but no overhangs: a top corner needs the corner below it"),
]

EXPECTED_TILES = {"68": 68, "256": 254, "HEIGHTMAP": 22}


def has_overhang(h):
    """True when a top corner (C4..C7) is solid but the corner below (C0..C3) isn't."""
    top = (h >> 4) & 0b1111
    bottom = h & 0b1111
    return (top & ~bottom) != 0


def guess_mode(hashes):
    """Best-guess cube mode from the tiles already modelled."""
    count = len(hashes)
    if count <= EXPECTED_TILES["HEIGHTMAP"] and not any(has_overhang(h) for h in hashes):
        return "HEIGHTMAP"
    if count <= EXPECTED_TILES["68"]:
        return "68"
    return "256"
