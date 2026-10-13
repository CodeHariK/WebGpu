"""Cube Y turns and the tile check (missing / rotated copies / not allowed).

Corner order and the Y-turn table are copied from Godot
(src/marching_cubes/core/mc_corners.h and mc_symmetry.cpp). Godot only ever turns
tiles about Y; mirrored or upside-down tiles are modelled here in Blender.

        7 ---- 6        bit i of a hash = corner i (1 = solid)
       /|     /|        0..3 bottom, 4..7 top
      4 ---- 5 |        0,1 near (+Z), 2,3 far (-Z)
      | 3 ---|-2        0,3 left (-X), 1,2 right (+X)
      |/     |/
      0 ---- 1

ROTATE_Y[i] is where corner i goes after one +90 degree turn about Y.
"""

from functools import cache

from .modes import has_overhang

ROTATE_Y = (1, 2, 3, 0, 5, 6, 7, 4)
# Left <-> right (Godot mirror_x). Godot never mirrors: Export Set writes a flipped copy of a tile
# whose mirror shape isn't modelled, so in X=Z, Y-asymmetric mode 53 modelled tiles give all 68.
MIRROR_X = (1, 0, 3, 2, 5, 4, 7, 6)

EMPTY = 0b00000000
FULL = 0b11111111

# Which turns make tiles equivalent in each mode (Complete: none, every hash is its own tile).
MODE_ROTATIONS = {
    "68": (ROTATE_Y,),
    "256": (),
    "HEIGHTMAP": (ROTATE_Y,),
}

def mirror(h):
    """Hash of the left/right mirror image (Godot's X axis = Blender's X)."""
    return permute(h, MIRROR_X)


def permute(h, table):
    """Move every solid corner i of hash h to corner table[i]."""
    out = 0
    for i in range(8):
        if (h >> i) & 1:
            out |= 1 << table[i]
    return out


def orbit(h, rotations):
    """Every hash reachable from h with the given quarter turns (h included)."""
    seen = {h}
    todo = [h]
    while todo:
        current = todo.pop()
        for table in rotations:
            nxt = permute(current, table)
            if nxt not in seen:
                seen.add(nxt)
                todo.append(nxt)
    return frozenset(seen)


def allowed(h, mode):
    """Is this hash one of the tiles the mode needs? (Empty and full never are.)"""
    if h in (EMPTY, FULL):
        return False
    return not (mode == "HEIGHTMAP" and has_overhang(h))


@cache
def orbits(mode):
    """The groups of hashes Godot treats as one tile: one tile must be modelled per group."""
    groups = []
    done = set()
    for h in range(256):
        if h in done or not allowed(h, mode):
            continue
        group = orbit(h, MODE_ROTATIONS[mode])
        done |= group
        groups.append(group)
    return tuple(groups)


def representative(group, mode):
    """The hash to name the tile for a group: the smallest (any member works in Godot)."""
    return min(group)


class Report:
    """Result of checking a set's tiles against its mode. All lists are sorted hashes.

    missing        groups with no tile and no mirror source: the hash to model for each
    mirrored       (hash, source): groups Export Set fills with a left/right flip of source
    partners       (hash, source): modelled tiles that are a flip of another modelled tile
                   (export would make them; delete one of the pair unless it differs on purpose)
    duplicates     (hash, kept_hash): a rotated copy of a tile already in the set
    not_allowed    tiles the mode can't use (overhangs in Heightmap, empty)
    """

    def __init__(self):
        self.missing = []
        self.mirrored = []
        self.partners = []
        self.duplicates = []
        self.not_allowed = []
        self.expected = 0


def check(hashes, mode):
    """Compare the set's tile hashes with what the mode needs."""
    hashes = set(hashes)
    report = Report()
    groups = orbits(mode)
    report.expected = len(groups)

    rotations = MODE_ROTATIONS[mode]
    for group in groups:
        present = sorted(group & hashes)
        sources = sorted(h for h in hashes if mirror(h) in group)
        if not present:
            if sources:
                report.mirrored.append((representative(group, mode), sources[0]))
            else:
                report.missing.append(representative(group, mode))
            continue
        # Godot loads every tile; extra ones in a group are rotated copies it would make anyway.
        kept = present[0]
        report.duplicates += [(h, kept) for h in present[1:]]
        # Both halves of a mirror pair modelled: flag the half with the higher hash.
        if sources and orbit(sources[0], rotations) != group and kept > min(orbit(sources[0], rotations) & hashes):
            report.partners.append((kept, sources[0]))

    # Full (11111111) is optional: never required, but Godot draws it if the .glb has one.
    report.not_allowed = sorted(h for h in hashes if h != FULL and not allowed(h, mode))
    report.missing.sort()
    report.mirrored.sort()
    report.partners.sort()
    report.duplicates.sort()
    return report


def corner_text(h):
    """Plan view of a hash for the UI: bottom and top layers, far row first.

    "■" solid, "□" empty. Example 00001111 -> "bottom ■■ ■■   top □□ □□"
    """
    def c(i):
        return "■" if (h >> i) & 1 else "□"

    return f"bottom {c(3)}{c(2)} {c(0)}{c(1)}   top {c(7)}{c(6)} {c(4)}{c(5)}"
