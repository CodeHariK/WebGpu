"""Settings saved on each tile-set collection (Collection.mc_tiles)."""

import bpy

from .modes import MODE_ITEMS
from .piece_lib import MCTilesPieceSource, find_pieces

KIND_ITEMS = [
    ("CUBE", "Cube", "8 corners per cell (marching cubes)"),
    ("PRISM", "Prism", "6 corners per cell (marching prism)"),
]


def _on_mode(self, context):
    """Mode changed: the filter's "Used by Mode" shows a different subset; the pieces change too."""
    from .tilefilter import apply

    _on_pieces(self, context)
    apply(context)


def _on_pieces(self, _context):
    """Mode changed: refill the piece choices (valid choices are kept)."""
    if self.is_tile_set and self.kind == "CUBE":
        find_pieces(self.id_data)


class MCTilesSetProps(bpy.types.PropertyGroup):
    is_tile_set: bpy.props.BoolProperty(name="Is Tile Set", default=False)
    kind: bpy.props.EnumProperty(name="Kind", items=KIND_ITEMS, default="CUBE")
    mode: bpy.props.EnumProperty(name="Mode", items=MODE_ITEMS, default="68", update=_on_mode)
    piece_sources: bpy.props.CollectionProperty(type=MCTilesPieceSource)
    build_from_pieces: bpy.props.BoolProperty(
        name="Build from Pieces",
        description="Shapes the set has no tile for (even turned / mirrored) are made from smaller tiles "
                    "overlapping: drawn in the playground, written by Export. Off: only the modelled tiles "
                    "(and their turns / mirrors) are used, for a set that already has every tile it needs",
        default=True,
    )
    export_path: bpy.props.StringProperty(
        name="Export",
        description="The .glb this set exports to",
        subtype="FILE_PATH",
    )


def _is_tile_set(_self, collection):
    return collection.mc_tiles.is_tile_set


def _on_active_set(_scene, context):
    """Switching sets: re-apply the viewport filter (unhides the old set's filtered tiles)."""
    from .tilefilter import apply

    apply(context)


def register():
    bpy.utils.register_class(MCTilesSetProps)
    bpy.types.Collection.mc_tiles = bpy.props.PointerProperty(type=MCTilesSetProps)
    # The set the panel is showing: a dropdown that only lists tile sets.
    bpy.types.Scene.mc_tiles_active = bpy.props.PointerProperty(
        name="Tile Set",
        type=bpy.types.Collection,
        poll=_is_tile_set,
        update=_on_active_set,
    )


def unregister():
    del bpy.types.Scene.mc_tiles_active
    del bpy.types.Collection.mc_tiles
    bpy.utils.unregister_class(MCTilesSetProps)
