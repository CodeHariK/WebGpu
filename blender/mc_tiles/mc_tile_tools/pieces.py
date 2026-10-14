"""Build from pieces (pure logic): a shape with no tile is made of smaller tiles overlapping.

Example: 00001101 (three bottom corners in an L) = 00001001 + the same edge turned, overlapping at
the shared corner. Tiles are closed meshes, so whatever one piece puts inside another's solid part is
hidden; where pieces cross on the outside you get a crease.

Rule: every solid corner, every solid cube edge (two neighbouring solid corners) and every fully solid
cube face (4 corners) must lie inside one piece. Edges: no notch between two corner pieces. Faces: a
flat side / slab is its own piece, no V-groove down the middle.
Minimum set: 68 -> 8 tiles, heightmap -> 5 (see minimum_set()).
A piece is any shape the set can already draw (a tile, its Y turns, its export mirror) that is a
proper subset of the shape. Pieces are picked greedily: the one covering most of what is still
uncovered, larger first. The full block (11111111) is never built: Godot draws nothing there.

Corner layout (same as cube.py / Godot): bit i = corner i; 0..3 bottom, 4..7 top.
"""

from . import cube

# Corner positions: x (0 left, 1 right), depth (0 near, 1 far), up.
_OFFSETS = (
    (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
    (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
)
EDGES = tuple(
    (1 << a) | (1 << b)
    for a in range(8) for b in range(a + 1, 8)
    if sum(x != y for x, y in zip(_OFFSETS[a], _OFFSETS[b])) == 1
)
FACES = tuple(
    sum(1 << i for i in range(8) if _OFFSETS[i][axis] == value)
    for axis in range(3) for value in (0, 1)
)


def requirements(shape):
    """Corner sets that must each lie inside one piece: corners, solid edges, full faces."""
    needs = [1 << i for i in range(8) if (shape >> i) & 1]
    needs += [e for e in EDGES if shape & e == e]
    needs += [f for f in FACES if shape & f == f]
    return needs


def drawable(hashes, mode):
    """Every shape the tiles can draw: their Y turns (if the mode turns) and export mirrors."""
    rotations = cube.MODE_ROTATIONS[mode]
    out = set()
    for h in hashes:
        out |= cube.orbit(h, rotations) | cube.orbit(cube.mirror(h), rotations)
    return out


def cover(shape, shapes):
    """Pieces (hashes from `shapes`, each a proper subset of `shape`) that build it, or None."""
    if shape in (cube.EMPTY, cube.FULL):
        return None
    candidates = [t for t in shapes if t & shape == t and t not in (shape, cube.EMPTY)]
    uncovered = set(requirements(shape))
    pieces = []
    while uncovered:
        best = max(
            candidates,
            key=lambda t: (sum(1 for r in uncovered if t & r == r), bin(t).count("1"), -t),
            default=None,
        )
        if best is None or not any(best & r == r for r in uncovered):
            return None  # something can't be covered: model this shape
        pieces.append(best)
        uncovered = {r for r in uncovered if best & r != r}
    return sorted(pieces)


def minimum_set(mode):
    """The shapes that can't be built from smaller ones: model these (one per Y-turn / mirror group)."""
    allowed = [h for h in range(1, 255) if cube.allowed(h, mode)]
    groups = {}
    for h in allowed:
        groups.setdefault(min(cube.orbit(h, (cube.ROTATE_Y,)) | cube.orbit(cube.mirror(h), (cube.ROTATE_Y,))), h)
    everything = set(allowed)
    return sorted(g for g in groups if cover(g, everything) is None)
