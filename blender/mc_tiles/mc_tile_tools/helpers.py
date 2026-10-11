"""Move a set's loose helper meshes into its "Helpers" child collection.

A loose helper is a top-level object in the set that isn't named like a tile and isn't in a
Helpers / Playground child collection yet. It moves with all its child parts. Objects are only
unlinked from the set's own collections; links to collections outside the set are kept.
Collection names are global in Blender, so a second set's collection may be "Helpers.001";
the add-on treats any "Helpers.*" the same.
"""

import bpy

from .tiles import IGNORED_COLLECTIONS, ignored_objects, tile_hash
from .view import refresh as refresh_helpers


def loose_helpers(collection):
    """Top-level non-tile objects of the set that aren't in Helpers / Playground."""
    kind = collection.mc_tiles.kind
    ignored = ignored_objects(collection)
    return [
        o
        for o in collection.all_objects
        if o.parent is None and o not in ignored and tile_hash(o.name, kind) is None
    ]


def helpers_collection(collection):
    """The set's Helpers child collection, created if missing."""
    for child in collection.children:
        if child.name.split(".")[0] == "Helpers":
            return child
    child = bpy.data.collections.new("Helpers")
    collection.children.link(child)
    return child


def move_helpers(collection):
    """Move every loose helper (with its child parts) into Helpers. Returns how many moved."""
    helpers = loose_helpers(collection)
    if not helpers:
        return 0
    target = helpers_collection(collection)
    set_collections = [collection] + [
        c for c in collection.children_recursive if c.name.split(".")[0] not in IGNORED_COLLECTIONS
    ]
    for root in helpers:
        for obj in [root] + list(root.children_recursive):
            for c in set_collections:
                if obj.name in c.objects:
                    c.objects.unlink(obj)
            if obj.name not in target.objects:
                target.objects.link(obj)
    return len(helpers)


class MCTILES_OT_move_helpers(bpy.types.Operator):
    """Move the set's non-tile objects into its Helpers child collection (never exported)"""

    bl_idname = "mc_tiles.move_helpers"
    bl_label = "Move Helpers"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return context.scene.mc_tiles_active is not None

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        moved = move_helpers(collection)
        refresh_helpers(context.scene)
        self.report({"INFO"}, f"Moved {moved} helpers into {helpers_collection(collection).name}")
        return {"FINISHED"}


def draw_move_helpers(layout, collection):
    count = len(loose_helpers(collection))
    if count:
        layout.operator("mc_tiles.move_helpers", text=f"Move {count} helper objects into Helpers", icon="OUTLINER_COLLECTION")


CLASSES = (MCTILES_OT_move_helpers,)
