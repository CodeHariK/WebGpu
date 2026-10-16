"""Tile naming: which objects in a collection are tiles, and their hash.

A tile is a root object (no parent) whose name starts with a binary string,
one digit per corner: 8 digits for a cube, 6 for a prism. Anything after the
digits ("00110011.house", "00110011_circle") is ignored, except a tag ("_hb2": height, group,
number) or the older "_v2" ending, which make the object a variant for the same hash (variants.py).

The string is written most significant bit first, exactly like Godot's
mc::binary_to_hash: the LAST character is corner 0, the first is corner 7.
So int(name, 2) gives the same hash Godot uses (bit i = corner i).
"""

from .variants import MAIN_ROLE, name_role, name_tag, tag_text

# Child collections with these names are never tiles and never exported.
IGNORED_COLLECTIONS = {"Helpers", "Playground", "Decor", "SocketPreview"}  # Decor: socket pieces (sockets.py)

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


def tile_variants(collection, kind, role=MAIN_ROLE):
    """{hash: [object, ...]} for the tiles of one role (height, group; variants.py), numbers in
    order ("_ma2" after the base). The main role (M a) by default, which is what Check and the
    piece / coverage rules work on; role=None gives every role (main role first), for listing,
    labels, corner boxes and Arrange."""
    ignored = ignored_objects(collection)
    found = {}
    for obj in collection.all_objects:
        if obj.parent is not None or obj in ignored:
            continue
        if role is not None and name_role(obj.name) != role:
            continue
        h = tile_hash(obj.name, kind)
        if h is not None:
            found.setdefault(h, []).append(obj)
    for objs in found.values():
        objs.sort(key=lambda o: (role_order(name_role(o.name)), name_tag(o.name)[2], o.name))
    return found


def role_order(role):
    """Sort key for roles: the main role (M a) first, then L, M, H, each by group."""
    return role != MAIN_ROLE, "lmh".index(role[0]), role[1]


def tiles_by_role(collection, kind):
    """{(height, group): {hash: [object, ...]}} for every role the set has, main role first."""
    roles = {}
    for h, objs in tile_variants(collection, kind, role=None).items():
        for obj in objs:
            roles.setdefault(name_role(obj.name), {}).setdefault(h, []).append(obj)
    return dict(sorted(roles.items(), key=lambda kv: role_order(kv[0])))


def variant_clashes(collection):
    """[(hash, tag, "name, name")] where two objects claim the same hash and tag ("ma2")."""
    clashes = []
    for h, objs in sorted(tile_variants(collection, "CUBE", role=None).items()):
        seen = {}
        for obj in objs:
            seen.setdefault(tag_text(*name_tag(obj.name)), []).append(obj.name)
        clashes += [(h, tag, ", ".join(names)) for tag, names in sorted(seen.items()) if len(names) > 1]
    return clashes


def tile_roots(collection, kind):
    """{hash: object} for every tile: the first variant of each hash."""
    return {h: objs[0] for h, objs in tile_variants(collection, kind).items()}
