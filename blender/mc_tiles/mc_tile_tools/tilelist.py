"""Searchable list of the tiles in the active set. Clicking a row selects the tile.

The list draws Collection.all_objects and hides everything that isn't a tile,
sorted by hash. The search box (the list's own filter field) matches:
    "0011"   tiles whose 8 digits contain 0011
    "#3"     tiles with exactly 3 solid corners
    "house"  anything else is matched against the full object name
"""

import bpy

from . import cube
from .ops import select_object
from .tiles import tile_hash, tile_roots


def _matches(search, h, name):
    if not search:
        return True
    if search.startswith("#") and search[1:].isdigit():
        return bin(h).count("1") == int(search[1:])
    if set(search) <= {"0", "1"}:
        return search in f"{h:08b}"
    return search.lower() in name.lower()


class MCTILES_UL_tiles(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_property, index=0):
        kind = data.mc_tiles.kind
        h = tile_hash(item.name, kind)
        row = layout.row(align=True)
        row.label(text=item.name, icon="MESH_CUBE")
        if h is not None and kind == "CUBE":
            row.label(text=cube.corner_text(h))

    def draw_filter(self, context, layout):
        layout.prop(self, "filter_name", text="", icon="VIEWZOOM")

    def filter_items(self, context, data, propname):
        objects = getattr(data, propname)
        settings = data.mc_tiles
        tiles = tile_roots(data, settings.kind)
        tile_names = {obj.name: h for h, obj in tiles.items()}

        flags = []
        order = []
        for obj in objects:
            h = tile_names.get(obj.name)
            visible = h is not None and _matches(self.filter_name.strip(), h, obj.name)
            flags.append(self.bitflag_filter_item if visible else 0)
            order.append(h if h is not None else 1000)
        # Sort rows by hash: new_order[i] is the row position of item i.
        ranked = sorted(range(len(order)), key=lambda i: order[i])
        new_order = [0] * len(order)
        for position, i in enumerate(ranked):
            new_order[i] = position
        return flags, new_order


def _select_active_row(scene, _context):
    """When a row is clicked, select that tile in the viewport."""
    collection = scene.mc_tiles_active
    index = scene.mc_tiles_index
    if collection is None or not 0 <= index < len(collection.all_objects):
        return
    select_object(bpy.context, collection.all_objects[index])


def register():
    bpy.utils.register_class(MCTILES_UL_tiles)
    bpy.types.Scene.mc_tiles_index = bpy.props.IntProperty(
        name="Tile", default=-1, update=_select_active_row
    )


def unregister():
    del bpy.types.Scene.mc_tiles_index
    bpy.utils.unregister_class(MCTILES_UL_tiles)
