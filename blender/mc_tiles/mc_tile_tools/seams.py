"""Seam check (pure logic, numpy, no bpy): is each wall of neighbouring tiles solid in the same places?

Why not a list of points: comparing the vertices on a wall flagged hundreds of pairs that look
fine, for three reasons: internal faces of overlapping parts (a block standing in a slab) put
vertices on the wall that are hidden inside the solid; flat wall faces cut up differently give extra
vertices; and values like 0.0175 round to different snaps. None of those show in the game.

Face coverage. What shows is the cross-section: which part of the shared wall is solid. Tiles are
closed meshes, so a tile's flat polygons lying on a cell wall ("caps") are exactly its solid
cross-section there. Each wall is sampled on an N x N grid (sample centres, so edges at whole
fractions never sit on a sample) and every sample inside a cap is solid. That bitmap is the face's
hash. A wall with no caps is all empty (open surfaces like a grass plane aren't checked).

    side faces   u = horizontal, as seen from OUTSIDE the cell; v = up (Z)
    top / bottom u, v = X, Y

Seen from outside, a side face keeps its bitmap when the tile turns about Z (Godot's Y); a mirror
copy flips u. Top / bottom bitmaps turn with the tile. So each tile caches 6 bitmaps.

Matching. A left / B right: A's right wall must equal B's left wall flipped in u (B sees it from the
other side); front / back the same; A below / B above: A's top equals B's bottom. Differences
smaller than TOLERANCE (fraction of the wall) are ignored.

Which pairs can touch. Two cells side by side share 4 corners; any A whose right wall has corner
pattern p can sit next to any B whose left wall has the same p. Checked on the library Godot builds
for the set's mode (every hash from the tiles, their turns, the mirror copies export makes, every
variant) plus an empty cell. Every tile pair with a clashing wall is listed (once, in its least
turned placement), not just one example per clash.

Corner layout in Blender axes (Z up, -Y front), same as Godot's C0..C7 (see playground.py).
"""

import numpy as np

from . import cube

N = 64  # samples per wall side
TOLERANCE = 0.004  # ignore differences smaller than this fraction of a wall (about 16 of 4096 samples)
WALL_TOL = 1e-3  # a vertex this close to a wall is on it
NEAR_TOL = 0.02  # closer than this but not on it: probably meant to be on the wall
CORNER_OFFSETS = (
    (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
    (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
)
FACES = tuple((axis, side) for axis in range(3) for side in (-1, 1))
FACE_NAMES = {
    (0, -1): "left", (0, 1): "right",
    (1, -1): "front", (1, 1): "back",
    (2, -1): "bottom", (2, 1): "top",
}
AXIS_NAMES = ("left | right", "front | back", "bottom | top")
CENTRES = (np.arange(N) + 0.5) / N - 0.5  # sample positions along u or v
EMPTY = np.zeros((N, N), dtype=bool)


class TileGeometry:
    """Vertices (cell-centred, as exported) and polygons (index lists) of one tile."""

    def __init__(self, points=(), polygons=()):
        self.points = [tuple(p) for p in points]
        self.polygons = [tuple(p) for p in polygons]


class TileFaces:
    """A modelled tile's 6 face bitmaps (bool [v, u]), cached."""

    def __init__(self, geometry):
        self.cover = {face: coverage(geometry, face) for face in FACES}


class Instance:
    """One library entry: hash h drawn by tile `name` turned `turns` x 90 degrees, maybe mirrored."""

    def __init__(self, h, name, faces, turns=0, mirrored=False, pieces=None):
        self.h = h
        self.name = name  # "" for the empty cell, "<hash> (built)" for a shape built from pieces
        self.faces = faces  # TileFaces
        self.turns = turns
        self.mirrored = mirrored
        self.pieces = pieces  # [(tile name, turns, mirrored)] when built from pieces, else None


class Pair:
    """A on the low side, B on the high side of `axis`; their shared wall isn't solid in the same places."""

    def __init__(self, axis, pattern, a, b, cover_a, cover_b):
        self.axis = axis  # 0 X (A left, B right), 1 Y (A front, B back), 2 Z (A below, B above)
        self.pattern = pattern  # the 4 shared corners
        self.a = a  # Instance
        self.b = b
        self.cover_a = cover_a  # bitmaps of the wall in A's frame (side +1), bool [v, u]
        self.cover_b = cover_b
        self.difference = float(np.mean(cover_a ^ cover_b))  # fraction of the wall

    def names(self):
        return self.a.name or "(empty)", self.b.name or "(empty)"


class Report:
    def __init__(self):
        self.pairs = []  # Pair
        self.near = []  # (name, count): vertices close to a wall but not on it
        self.outside = []  # (name, count): vertices outside the cell
        self.checked = 0  # library entries


# ---------------------------------------------------------------- transforms


def transform(p, turns, mirrored):
    """Flip X (mirror copy), then turn about Z by turns x 90 degrees, like the playground / Godot."""
    x, y, z = p
    if mirrored:
        x = -x
    for _ in range(turns % 4):
        x, y = -y, x
    return (x, y, z)


def inverse(p, turns, mirrored):
    x, y, z = p
    for _ in range(turns % 4):
        x, y = y, -x
    if mirrored:
        x = -x
    return (x, y, z)


def own_face(face, turns, mirrored):
    """The tile's own face that ends up as world face `face` after the transform."""
    axis, side = face
    normal = [0, 0, 0]
    normal[axis] = side
    back = inverse(tuple(normal), turns, mirrored)
    for a in range(3):
        if back[a]:
            return (a, 1 if back[a] > 0 else -1)
    raise ValueError(face)


# ---------------------------------------------------------------- face coverage


def face_uv(p, face):
    """(u, v) of a point on a wall: side faces as seen from outside, top / bottom in X, Y."""
    axis, side = face
    if axis == 0:
        return (side * p[1], p[2])  # right wall: u = +Y, left wall: u = -Y
    if axis == 1:
        return (-side * p[0], p[2])  # back wall: u = -X, front wall: u = +X
    return (p[0], p[1])


def _on_wall(p, face):
    axis, side = face
    return abs(p[axis] - side * 0.5) < WALL_TOL


def _fill(polygon, grid_u, grid_v):
    """Samples inside a 2D polygon (even-odd rule)."""
    inside = np.zeros(grid_u.shape, dtype=bool)
    count = len(polygon)
    for k in range(count):
        (u1, v1), (u2, v2) = polygon[k], polygon[(k + 1) % count]
        if v1 == v2:
            continue
        crosses = (v1 > grid_v) != (v2 > grid_v)
        at = (u2 - u1) * (grid_v - v1) / (v2 - v1) + u1
        inside ^= crosses & (grid_u < at)
    return inside


def coverage(geometry, face):
    """bool [v, u]: samples of the wall covered by the tile's flat polygons on that wall."""
    grid_u, grid_v = np.meshgrid(CENTRES, CENTRES)
    cover = np.zeros((N, N), dtype=bool)
    for poly in geometry.polygons:
        points = [geometry.points[i] for i in poly]
        if len(points) >= 3 and all(_on_wall(p, face) for p in points):
            cover |= _fill([face_uv(p, face) for p in points], grid_u, grid_v)
    return cover


def short(cover):
    """6-character id of a wall bitmap for the UI: "-" empty, "full" all solid."""
    if not cover.any():
        return "-"
    if cover.all():
        return "full"
    import hashlib

    return hashlib.md5(np.packbits(cover).tobytes()).hexdigest()[:6]


def world_cover(inst, face):
    """A world face's bitmap as the HIGH-side neighbour's frame sees it (u flipped on left / front walls),
    so the two faces of a wall show the same bitmap, and the same short() id, when they match."""
    cover = instance_cover(inst, face)
    return flip_u(cover) if face[0] < 2 and face[1] < 0 else cover


def flip_u(cover):
    """The same wall with u reversed (a mirror copy, or the wall seen from the other side)."""
    return cover[:, ::-1]


_TURN_MAPS = {}


def _turn_map(turns, mirrored):
    """(row, column) index arrays: new[v, u] = old[rows[v, u], columns[v, u]] for a top / bottom bitmap."""
    key = (turns % 4, mirrored)
    if key not in _TURN_MAPS:
        ints = np.arange(N) * 2 - (N - 1)  # sample centres as odd integers, symmetric about 0
        rows = np.empty((N, N), dtype=np.intp)
        columns = np.empty((N, N), dtype=np.intp)
        for vi, v in enumerate(ints):
            for ui, u in enumerate(ints):
                x, y, _ = inverse((int(u), int(v), 0), turns, mirrored)
                rows[vi, ui] = (y + N - 1) // 2
                columns[vi, ui] = (x + N - 1) // 2
        _TURN_MAPS[key] = (rows, columns)
    return _TURN_MAPS[key]


def _turn_flat(cover, turns, mirrored):
    """A top / bottom bitmap (u = X, v = Y) after the tile is mirrored and turned."""
    if not turns % 4 and not mirrored:
        return cover
    rows, columns = _turn_map(turns, mirrored)
    return cover[rows, columns]


def instance_cover(inst, face):
    """World face `face` of a library entry, from its tile's cached bitmaps."""
    own = own_face(face, inst.turns, inst.mirrored)
    cover = inst.faces.cover[own]
    if face[0] < 2:
        return flip_u(cover) if inst.mirrored else cover  # turning keeps a side face, mirroring flips u
    return _turn_flat(cover, inst.turns, inst.mirrored)


# ---------------------------------------------------------------- corner patterns


def _plane_axes(axis):
    return [a for a in range(3) if a != axis]


def face_pattern(h, axis, side):
    """The 4 corners of hash h on a wall, as 4 bits ordered by world position on that wall."""
    want = 1 if side > 0 else 0
    corners = []
    for bit, offset in enumerate(CORNER_OFFSETS):
        if offset[axis] == want:
            u, v = (offset[a] for a in _plane_axes(axis))
            corners.append(((v, u), (h >> bit) & 1))
    corners.sort()
    return sum(solid << i for i, (_, solid) in enumerate(corners))


def pattern_text(pattern):
    c = ["■" if (pattern >> i) & 1 else "□" for i in range(4)]
    return f"{c[0]}{c[1]} {c[2]}{c[3]}"


# ---------------------------------------------------------------- the check


def _geometry_problems(name, geometry, report):
    near = outside = 0
    for p in geometry.points:
        if any(abs(p[a]) > 0.5 + WALL_TOL for a in range(3)):
            outside += 1
        elif any(WALL_TOL <= 0.5 - abs(p[a]) < NEAR_TOL for a in range(3)):
            near += 1
    if near:
        report.near.append((name, near))
    if outside:
        report.outside.append((name, outside))


def _example(instances):
    """The clearest example: a real tile, untransformed if possible."""
    return min(instances, key=lambda i: (not i.name, i.mirrored, i.turns, i.name))


def _by_tile(instances):
    """One example per tile (and one for the empty cell) among entries sharing a wall."""
    tiles = {}
    for inst in instances:
        tiles.setdefault(inst.name, []).append(inst)
    return [_example(group) for _, group in sorted(tiles.items())]


def check(instances, geometries=None, tolerance=TOLERANCE):
    """All walls that can touch must be solid in the same places. Returns a Report."""
    report = Report()
    report.checked = len(instances)
    for name, geometry in sorted((geometries or {}).items()):
        _geometry_problems(name, geometry, report)

    for axis in range(3):
        low, high = {}, {}  # pattern -> {bitmap bytes: (bitmap, [Instance])}
        for inst in instances:
            cover = instance_cover(inst, (axis, 1))
            entry = low.setdefault(face_pattern(inst.h, axis, 1), {}).setdefault(cover.tobytes(), (cover, []))
            entry[1].append(inst)
            cover = instance_cover(inst, (axis, -1))
            if axis < 2:
                cover = flip_u(cover)  # seen from the other side
            entry = high.setdefault(face_pattern(inst.h, axis, -1), {}).setdefault(cover.tobytes(), (cover, []))
            entry[1].append(inst)
        for pattern in sorted(set(low) & set(high)):
            for key_a, (cover_a, a_list) in low[pattern].items():
                for key_b, (cover_b, b_list) in high[pattern].items():
                    if key_a == key_b:
                        continue
                    if float(np.mean(cover_a ^ cover_b)) <= tolerance:
                        continue
                    # Several tiles can share the same wall: list every tile pair, not one example.
                    for a in _by_tile(a_list):
                        for b in _by_tile(b_list):
                            report.pairs.append(Pair(axis, pattern, a, b, cover_a, cover_b))
    report.pairs.sort(key=lambda p: (-p.difference, p.a.turns + p.b.turns, p.a.mirrored + p.b.mirrored))
    # The same two tiles clash in several turns / mirrors: keep one (the least turned) per tile pair.
    seen = set()
    unique = []
    for pair in report.pairs:
        key = (pair.names(), pair.axis == 2, round(pair.difference, 4))
        if key not in seen:
            seen.add(key)
            unique.append(pair)
    report.pairs = unique
    return report


def library(variants, lookup, allowed, built=None):
    """[Instance] for every allowed hash (each variant of the tile that draws it) plus an empty cell.

    variants: {tile hash: [(name, TileGeometry)]}; lookup: {hash: (tile hash, turns, mirrored)}
    (playground.tile_lookup); allowed: hashes the mode uses; built: {hash: [(tile hash, turns,
    mirrored)]} for shapes built from pieces (their geometry is the pieces together).
    """
    faces = {name: TileFaces(g) for objs in variants.values() for name, g in objs}
    out = [Instance(cube.EMPTY, "", TileFaces(TileGeometry()))]
    for h in sorted(allowed | {cube.FULL}):
        if h not in lookup:
            continue
        source, turns, mirrored = lookup[h]
        for name, _ in variants[source]:
            out.append(Instance(h, name, faces[name], turns, mirrored))
    for h, placed in sorted((built or {}).items()):
        if h not in allowed:
            continue
        points, polygons, parts = [], [], []
        for source, turns, mirrored in placed:
            name, geometry = variants[source][0]
            base = len(points)
            points += [transform(p, turns, mirrored) for p in geometry.points]
            polygons += [tuple(base + i for i in poly) for poly in geometry.polygons]
            parts.append((name, turns, mirrored))
        whole = TileGeometry(points, polygons)
        out.append(Instance(h, f"{h:08b} (built)", TileFaces(whole), pieces=parts))
    return out
