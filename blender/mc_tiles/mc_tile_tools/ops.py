"""Operators: turn a collection into a tile set, or back."""

import bpy

from .modes import MODE_ITEMS, guess_mode
from .tiles import guess_kind, tile_roots
from .view import refresh as refresh_helpers


def _target_collection(context):
    """The collection selected in the Outliner, if it can become a tile set: not the scene root, not a
    set already, and not a sub-collection of a set (its tiles belong to that set)."""
    collection = context.collection
    if collection is None or collection == context.scene.collection or collection.mc_tiles.is_tile_set:
        return None
    if _parent_sets(collection):
        return None
    return collection


class MCTILES_OT_make_set(bpy.types.Operator):
    """Make the collection selected in the Outliner a tile set, guessing kind and mode from its tiles"""

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
            from .piece_lib import find_pieces

            find_pieces(collection)
        context.scene.mc_tiles_active = collection
        refresh_helpers(context.scene)
        self.report(
            {"INFO"},
            f"{collection.name}: {settings.kind.lower()} set, {len(hashes)} tiles",
        )
        return {"FINISHED"}


def _parent_sets(collection):
    """Tile sets that contain this collection somewhere below them."""
    return [c for c in bpy.data.collections if c.mc_tiles.is_tile_set and collection in c.children_recursive]


class MCTILES_OT_new_set(bpy.types.Operator):
    """Create a new, empty tile set collection (the name must not be taken by any collection)"""

    bl_idname = "mc_tiles.new_set"
    bl_label = "New Tile Set"
    bl_options = {"REGISTER", "UNDO"}

    name: bpy.props.StringProperty(name="Name", default="Tiles")
    mode: bpy.props.EnumProperty(name="Mode", items=MODE_ITEMS, default="HEIGHTMAP")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def draw(self, context):
        self.layout.prop(self, "name")
        self.layout.prop(self, "mode")
        taken = bpy.data.collections.get(self.name.strip())
        if taken is not None:
            where = _owner_name(taken)
            self.layout.label(text=f"'{taken.name}' is taken{where}: pick another name", icon="ERROR")

    def execute(self, context):
        name = self.name.strip()
        if not name:
            self.report({"ERROR"}, "Give the tile set a name")
            return {"CANCELLED"}
        taken = bpy.data.collections.get(name)
        if taken is not None:
            self.report({"ERROR"}, f"A collection named '{name}' already exists{_owner_name(taken)}")
            return {"CANCELLED"}
        collection = bpy.data.collections.new(name)
        context.scene.collection.children.link(collection)
        settings = collection.mc_tiles
        settings.is_tile_set = True
        settings.kind = "CUBE"
        settings.mode = self.mode  # also finds the pieces (props._on_mode)
        context.scene.mc_tiles_active = collection
        self.report({"INFO"}, f"New tile set {collection.name}")
        return {"FINISHED"}


def _owner_name(collection):
    """" (inside Cube)" when a collection sits inside another collection, else ""."""
    owners = [c.name for c in bpy.data.collections if collection.name in c.children]
    return f" (inside {', '.join(owners)})" if owners else ""


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


CLASSES = (MCTILES_OT_make_set, MCTILES_OT_new_set, MCTILES_OT_remove_set, MCTILES_OT_select_tile)
