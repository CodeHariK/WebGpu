"""Where a set's pieces come from: any tile in the .blend, not only the set's own.

Each minimum piece (pieces.minimum_set) has a source picked in the Pieces box: a dropdown of every
tile object in the file whose hash is that piece or one of its Y turns / mirrors (from any
collection, tile set or not). "Find Pieces" fills the dropdowns: it keeps a valid choice, else
prefers a modelled tile (not a placeholder), from the set itself first, then from other collections.

A piece chosen from another collection gets a linked copy in the set (same mesh data, so editing
the original updates it), named "<hash>.<set>" because the bare hash name belongs to the original
(Blender object names are unique per file; export writes "<hash>_<set>" either way). Changing the
dropdown swaps the copy; choosing a tile of the set itself removes it. So the set always holds its
pieces and everything else (arrange, export, playground...) only looks at the set.

set_tiles(collection): the set's tiles, with each piece group reduced to its chosen piece (other
tiles of that group in the set, e.g. a placeholder, are left out). Choices are saved on the set
(Collection.mc_tiles.piece_sources).
"""

import bpy

from . import cube
from .pieces import minimum_set
from .tiles import tile_hash, tile_variants

SKIP_COLLECTIONS = {"Playground", "Arrange", "MC_Export_Temp"}
GENERATED = "mc_piece_of"  # generated tiles (piece_ops.generate_built): never offered as pieces
LINKED = "mc_piece_link"  # a set's linked copy of a piece from another collection: the original's name


def piece_group(h):
    """h with every Y turn and left/right mirror: any of them can stand in for the piece."""
    turns = (cube.ROTATE_Y,)
    return cube.orbit(h, turns) | cube.orbit(cube.mirror(h), turns)


def candidates(piece):
    """Tile objects anywhere in the file that can be this piece, best first."""
    group = piece_group(piece)
    out = []
    for obj in bpy.data.objects:
        if obj.parent is not None or obj.name.startswith(("pg_", "ar_")) or GENERATED in obj or LINKED in obj:
            continue
        if not obj.users_collection or any(c.name.split(".")[0] in SKIP_COLLECTIONS for c in obj.users_collection):
            continue
        h = tile_hash(obj.name, "CUBE")
        if h in group:
            out.append(obj)
    return sorted(out, key=lambda o: (is_placeholder(o), o.name))


def is_placeholder(obj):
    return obj.type == "MESH" and obj.data is not None and obj.data.name.split(".")[0].endswith("_placeholder")


def _source_poll(item, obj):
    """Dropdown filter: only tiles that can be this item's piece."""
    if obj.parent is not None or obj.name.startswith(("pg_", "ar_")) or GENERATED in obj or LINKED in obj:
        return False
    return tile_hash(obj.name, "CUBE") in piece_group(item.hash)


def _on_source(item, _context):
    """Dropdown changed: swap the set's linked copy."""
    sync_copies(item.id_data)


class MCTilesPieceSource(bpy.types.PropertyGroup):
    hash: bpy.props.IntProperty(min=0, max=255)
    source: bpy.props.PointerProperty(
        name="Piece", type=bpy.types.Object, poll=_source_poll, update=_on_source,
        description="The tile used for this piece (any collection; any turn / mirror of it works)",
    )


def find_pieces(collection):
    """Fill the set's piece choices for its mode. Returns {piece: chosen object or None}."""
    settings = collection.mc_tiles
    own = {o for objs in tile_variants(collection, "CUBE").values() for o in objs}
    chosen = {}
    keep = {item.hash: item.source for item in settings.piece_sources}
    settings.piece_sources.clear()
    for piece in minimum_set(settings.mode):
        found = candidates(piece)
        current = keep.get(piece)
        if current is not None and current in found:
            pick = current
        else:
            ranked = sorted(found, key=lambda o: (is_placeholder(o), o not in own, o.name))
            pick = ranked[0] if ranked else None
        item = settings.piece_sources.add()
        item.hash = piece
        item.source = pick
        chosen[piece] = pick
    sync_copies(collection)
    return chosen


def _linked_copy(collection, piece):
    """The set's linked copy made for this piece, or None."""
    for obj in collection.all_objects:
        if obj.parent is None and LINKED in obj and tile_hash(obj.name, "CUBE") in piece_group(piece):
            return obj
    return None


def _remove(obj):
    for part in list(obj.children_recursive) + [obj]:
        bpy.data.objects.remove(part, do_unlink=True)


def sync_copies(collection):
    """Make sure every piece from another collection has a linked copy in the set (and no stale ones)."""
    from .names import set_slug
    from .piece_ops import _copy_parts, _free_positions

    own = {o for objs in tile_variants(collection, "CUBE").values() for o in objs if LINKED not in o}
    wanted = {}
    for item in collection.mc_tiles.piece_sources:
        if item.source is not None and item.source not in own:
            wanted[item.hash] = item.source
    for obj in [o for o in collection.all_objects if o.parent is None and LINKED in o]:
        piece = next((p for p, src in wanted.items() if tile_hash(obj.name, "CUBE") in piece_group(p)), None)
        if piece is None or obj[LINKED] != wanted[piece].name or obj.data != wanted[piece].data:
            _remove(obj)
    missing = [(p, src) for p, src in wanted.items() if _linked_copy(collection, p) is None]
    for (piece, source), position in zip(missing, _free_positions(collection, len(missing))):
        matrix = source.matrix_world.copy()
        matrix.translation = position
        copy = _copy_parts(source, collection, matrix)
        copy.name = f"{source.name.split('.')[0]}.{set_slug(collection)}"
        copy[LINKED] = source.name


def set_tiles(collection):
    """{hash: [objects]} the set draws with: its tiles, each piece group swapped for its chosen source."""
    settings = collection.mc_tiles
    tiles = tile_variants(collection, settings.kind)
    if settings.kind != "CUBE" or not len(settings.piece_sources):
        return tiles
    out = dict(tiles)
    own = {o for objs in tiles.values() for o in objs}
    for item in settings.piece_sources:
        source = item.source
        if source is None:
            continue
        group = piece_group(item.hash)
        mine = source if source in own else _linked_copy(collection, item.hash)
        if mine is None:
            continue
        h = tile_hash(mine.name, "CUBE")
        for g in group:  # only the chosen piece draws its group (a placeholder of the set is left out)
            out.pop(g, None)
        out[h] = [mine]
    return out



def set_roots(collection):
    return {h: objs[0] for h, objs in set_tiles(collection).items()}


def register():
    bpy.utils.register_class(MCTilesPieceSource)


def unregister():
    bpy.utils.unregister_class(MCTilesPieceSource)
