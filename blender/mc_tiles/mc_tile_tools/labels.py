"""Hash labels drawn in the 3D view under each tile of the active set (text only, no objects).

Tiles show their name ("00010001", "00010001_v2"); Arrange slots show "<hash> mirror" (light
blue) or "<hash> missing" (red). Toggle: Scene.mc_tiles_labels ("Hash Labels" in Arrange).
"""

import blf
import bpy
from bpy_extras.view3d_utils import location_3d_to_region_2d
from mathutils import Vector

from .tiles import tile_variants

OFFSET = Vector((0.0, -0.6, -0.5))  # under the tile, towards the front
COLOURS = {"TILE": (1.0, 1.0, 1.0, 0.9), "mirror": (0.55, 0.8, 1.0, 0.9), "missing": (1.0, 0.35, 0.35, 0.95)}
ARRANGE_LABEL = "mc_ar_label"  # set by arrange.py on slot objects
SIZE = 11

_handle = None


def _labels(context):
    """[(world position, text, colour)] for visible tiles of the active set and Arrange slots."""
    scene = context.scene
    collection = scene.mc_tiles_active
    out = []
    if collection is not None and collection.mc_tiles.is_tile_set:
        for objs in tile_variants(collection, collection.mc_tiles.kind).values():
            for obj in objs:
                if obj.visible_get():
                    out.append((obj.matrix_world.translation, obj.name, COLOURS["TILE"]))
    for obj in scene.objects:
        text = obj.get(ARRANGE_LABEL)
        if text and obj.visible_get():
            out.append((obj.matrix_world.translation, text, COLOURS[text.split()[-1]]))
    return out


def _draw():
    context = bpy.context
    if not getattr(context.scene, "mc_tiles_labels", False):
        return
    region, rv3d = context.region, context.region_data
    if region is None or rv3d is None:
        return
    blf.size(0, SIZE)
    for position, text, colour in _labels(context):
        point = location_3d_to_region_2d(region, rv3d, position + OFFSET)
        if point is None:
            continue
        width, _ = blf.dimensions(0, text)
        blf.color(0, *colour)
        blf.position(0, point.x - width / 2.0, point.y, 0.0)
        blf.draw(0, text)


def _redraw(_self, context):
    for area in context.screen.areas if context.screen else ():
        if area.type == "VIEW_3D":
            area.tag_redraw()


def register():
    global _handle
    bpy.types.Scene.mc_tiles_labels = bpy.props.BoolProperty(
        name="Hash Labels", description="Draw each tile's name under it in the 3D view", update=_redraw,
    )
    _handle = bpy.types.SpaceView3D.draw_handler_add(_draw, (), "WINDOW", "POST_PIXEL")


def unregister():
    global _handle
    if _handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_handle, "WINDOW")
        _handle = None
    del bpy.types.Scene.mc_tiles_labels
