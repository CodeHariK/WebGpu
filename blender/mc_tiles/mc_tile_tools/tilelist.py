"""List of the tiles in the active set. Clicking a row selects the tile.

Columns: name · variant (1/2 = first of 2 meshes) · seam conflicts (after Check Seams; click to list
that tile's clashing pairs in the Check panel) · playground button (build the playground around the
tile, next to it).

The list draws Collection.all_objects, keeps the base tile of each hash that passes the
filter (tilefilter.py: search, shapes, corners, problems, family; the same filter can hide
tiles in the viewport) and sorts rows by hash.
"""

import bpy

from .ops import select_object
from .seams_ui import conflicts
from .tilefilter import shown_tiles
from .tiles import tile_hash, tile_variants
from .variants import variant_number


def _variant_counts(collection):
    """{hash: number of variant meshes} for the set."""
    return {h: len(objs) for h, objs in tile_variants(collection, collection.mc_tiles.kind).items()}


class MCTILES_UL_tiles(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_property, index=0):
        kind = data.mc_tiles.kind
        h = tile_hash(item.name, kind)
        row = layout.row(align=True)
        split = row.split(factor=0.45)
        split.label(text=item.name, icon="MESH_CUBE")
        rest = split.row(align=True)
        # Variant column: "1/2" = variant 1 of the 2 meshes this hash has; "1" when it has one.
        count = _variant_counts(data).get(h, 1)
        rest.label(text=f"{variant_number(item.name)}/{count}" if count > 1 else "1")
        if kind != "CUBE" or h is None:
            return
        n = conflicts(data, h)
        if n is None:
            rest.label(text="–")  # not checked yet
        else:
            op = rest.operator(
                "mc_tiles.filter_conflicts", text=str(n), icon="ERROR" if n else "CHECKMARK", emboss=n > 0,
            )
            op.name = item.name
        rest.operator("mc_tiles.playground_for", text="", icon="VIEW_ORTHO").name = item.name

    def draw_filter(self, context, layout):
        layout.label(text="Columns: name · variant · seam conflicts · playground")

    def filter_items(self, context, data, propname):
        objects = getattr(data, propname)
        kind = data.mc_tiles.kind
        shown = shown_tiles(data, context.scene.mc_tiles_filter)
        # One row per hash: the base tile, shown when the filter lets it through.
        tile_names = {objs[0].name: h for h, objs in tile_variants(data, kind).items()}

        flags = []
        order = []
        for obj in objects:
            h = tile_names.get(obj.name)
            visible = h is not None and shown.get(obj, False)
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
