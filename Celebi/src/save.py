import random

import bpy
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
import json

from . import type


class LIBRARY_OT_save(bpy.types.Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".celebi"):
            self.filepath += ".celebi"

        c = type.celebi()

        data = {
            "all_tags": [t.name for t in c.library_tags],
            "items": [],
            "voxels": [v.name for v in c.voxels],
        }

        library_items = c.library_items

        for item in library_items:
            enabled_tags = [t.name for t in item.tags if t.enabled]
            entry = {
                "object_name": item.obj.name if item.obj else None,
                "tags": enabled_tags,
            }
            data["items"].append(entry)

        with open(self.filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)

        self.report({"INFO"}, f"Library saved to {self.filepath}")

        return {"FINISHED"}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = "library.celebi"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class LIBRARY_OT_load(bpy.types.Operator):
    bl_idname = "celebi.library_load"
    bl_label = "Load Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        with open(self.filepath, "r", encoding="utf-8") as f:
            data = json.load(f)

        c = type.celebi()

        # Load all_tags into scene.library_tags
        c.library_tags.clear()
        for tag_name in data.get("all_tags", []):
            new_tag_entry = c.library_tags.add()
            new_tag_entry.name = tag_name

        c = type.celebi()

        # Clear existing
        c.library_items.clear()

        for entry in data.get("items", []):
            item = c.library_items.add()
            if entry["object_name"] in bpy.data.objects:
                item.obj = bpy.data.objects[entry["object_name"]]

            # Restore tags collection with enabled status
            enabled_tags = set(entry.get("tags", []))
            for tag_item in c.library_tags:
                tag_entry = item.tags.add()
                tag_entry.name = tag_item.name
                tag_entry.enabled = tag_item.name in enabled_tags

        c.voxels.clear()
        for voxel_name in data.get("voxels", []):
            voxel_item = c.voxels.add()
            voxel_item.name = voxel_name

        self.report({"INFO"}, f"Library loaded from {self.filepath}")
        return {"FINISHED"}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = "library.celebi"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class LIBRARY_OT_clear(bpy.types.Operator):
    bl_idname = "celebi.library_clear"
    bl_label = "Clear Library"

    def execute(self, context):
        c = type.celebi()

        c.library_items.clear()

        c.T_library_index = -1

        self.report({"INFO"}, "Library cleared")
        return {"FINISHED"}


classes = (
    LIBRARY_OT_save,
    LIBRARY_OT_load,
    LIBRARY_OT_clear,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
