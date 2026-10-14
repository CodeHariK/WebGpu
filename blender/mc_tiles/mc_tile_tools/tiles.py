"""Tile naming: which objects in a collection are tiles, and their hash.

A tile is a root object (no parent) whose name starts with a binary string,
one digit per corner: 8 digits for a cube, 6 for a prism. Anything after the
digits ("00110011.house", "00110011_circle") is ignored, except a "_v2" ending, which makes the
object an extra mesh variant for the same hash (see variants.py).

The string is written most significant bit first, exactly like Godot's
mc::binary_to_hash: the LAST character is corner 0, the first is corner 7.
So int(name, 2) gives the same hash Godot uses (bit i = corner i).
"""

from .variants import variant_number

# Child collections with these names are never tiles and never exported.
IGNORED_COLLECTIONS = {"Helpers", "Playground"}

CORNERS = {"CUBE": 8, "PRISM": 6}


def name_digits(name):
    """The leading binary digits of an object name ("" if none)."""
    head = name.split(".")[0].split("_")[0]
    return head if head and set(head) <= {"0", "1"} else ""


def tile_hash(name, kind):
    """Hash of a tile name for this kind, or None when the name isn't a tile."""
    digits = name_digits(name)
    if len(digits) != CORNERS[kind]:
        return None
    return int(digits, 2)


def guess_kind(collection):
    """CUBE or PRISM, by which name length is more common in the collection."""
    cubes = prisms = 0
    for obj in collection.all_objects:
        length = len(name_digits(obj.name))
        cubes += length == 8
        prisms += length == 6
    return "PRISM" if prisms > cubes else "CUBE"


def ignored_objects(collection):
    """Objects inside Helpers / Playground child collections, and inside child collections that are
    tile sets of their own (a set nested in another set belongs only to itself)."""
    ignored = set()
    for child in collection.children_recursive:
        if child.name.split(".")[0] in IGNORED_COLLECTIONS or child.mc_tiles.is_tile_set:
            ignored.update(child.all_objects)
    return ignored


def tile_variants(collection, kind):
    """{hash: [object, ...]} for every tile, variants in number order ("_v2" after the base)."""
    ignored = ignored_objects(collection)
    found = {}
    for obj in collection.all_objects:
        if obj.parent is not None or obj in ignored:
            continue
        h = tile_hash(obj.name, kind)
        if h is not None:
            found.setdefault(h, []).append(obj)
    for objs in found.values():
        objs.sort(key=lambda o: (variant_number(o.name), o.name))
    return found


def variant_clashes(collection):
    """[(hash, variant, "name, name")] where two objects claim the same hash and variant."""
    clashes = []
    for h, objs in sorted(tile_variants(collection, "CUBE").items()):
        seen = {}
        for obj in objs:
            seen.setdefault(variant_number(obj.name), []).append(obj.name)
        clashes += [(h, v, ", ".join(names)) for v, names in sorted(seen.items()) if len(names) > 1]
    return clashes


def tile_roots(collection, kind):
    """{hash: object} for every tile: the first variant of each hash."""
    return {h: objs[0] for h, objs in tile_variants(collection, kind).items()}
