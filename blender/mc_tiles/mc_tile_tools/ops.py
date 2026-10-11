"""Operators: turn a collection into a tile set, or back."""

import bpy

from .modes import guess_mode
from .tiles import guess_kind, tile_roots
from .view import refresh as refresh_helpers


def _target_collection(context):
    """The collection selected in the Outliner, unless it's the scene root."""
    collection = context.collection
    if collection is None or collection == context.scene.collection:
        return None
    return collection


class MCTILES_OT_make_set(bpy.types.Operator):
    """Make the active collection a tile set, guessing kind and mode from its tiles"""

    bl_idname = "mc_tiles.make_set"
    bl_label = "Make Tile Set"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return _target_collection(context) is not None

    def execute(self, context):
        collection = _target_collection(context)
        settings = collection.mc_tiles
        settings.is_tile_set = True
        settings.kind = guess_kind(collection)
        hashes = list(tile_roots(collection, settings.kind))
        if settings.kind == "CUBE":
            settings.mode = guess_mode(hashes)
        context.scene.mc_tiles_active = collection
        refresh_helpers(context.scene)
        self.report(
            {"INFO"},
            f"{collection.name}: {settings.kind.lower()} set, {len(hashes)} tiles",
        )
        return {"FINISHED"}


class MCTILES_OT_remove_set(bpy.types.Operator):
    """Stop treating the shown collection as a tile set (objects are untouched)"""

    bl_idname = "mc_tiles.remove_set"
    bl_label = "Remove Tile Set"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return context.scene.mc_tiles_active is not None

    def execute(self, context):
        context.scene.mc_tiles_active.mc_tiles.is_tile_set = False
        context.scene.mc_tiles_active = None
        return {"FINISHED"}


def select_object(context, obj):
    """Make obj the only selected object and the active one. False if it can't be selected."""
    if obj.name not in context.view_layer.objects:
        return False
    for other in context.selected_objects:
        other.select_set(False)
    obj.select_set(True)
    context.view_layer.objects.active = obj
    return True


class MCTILES_OT_select_tile(bpy.types.Operator):
    """Select this tile and make it active"""

    bl_idname = "mc_tiles.select_tile"
    bl_label = "Select Tile"
    bl_options = {"REGISTER", "UNDO"}

    name: bpy.props.StringProperty()

    def execute(self, context):
        obj = bpy.data.objects.get(self.name)
        if obj is None:
            self.report({"WARNING"}, f"No object named {self.name}")
            return {"CANCELLED"}
        if not select_object(context, obj):
            self.report({"WARNING"}, f"{obj.name} is not in this view layer")
            return {"CANCELLED"}
        return {"FINISHED"}


CLASSES = (MCTILES_OT_make_set, MCTILES_OT_remove_set, MCTILES_OT_select_tile)
