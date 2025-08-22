import bpy
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
from bpy.types import Operator
import json

from . import type


class LIBRARY_OT_save(Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".celebi"):
            self.filepath += ".celebi"

        c = type.celebi()

        data = {
            "items": [],
            "voxels": [v.name for v in c.getVoxels()],
        }

        lib = c.getLibraryItems()
        for item in lib:
            data["items"].append(item.serialize())

        with open(self.filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=4)

        self.report({"INFO"}, f"Library saved to {self.filepath}")

        return {"FINISHED"}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = "library.celebi"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class LIBRARY_OT_load(Operator):
    bl_idname = "celebi.library_load"
    bl_label = "Load Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        with open(self.filepath, "r", encoding="utf-8") as f:
            data = json.load(f)

        c = type.celebi()

        c.clearLibraryItems()

        for entry in data.get("items", []):
            item = c.addLibraryItem(None)
            item.deserialize(entry)

        c.purgeVoxels()
        for voxelName in data.get("voxels", []):
            c.addVoxel(voxelName)

        self.report({"INFO"}, f"Library loaded from {self.filepath}")
        return {"FINISHED"}

    def invoke(self, context, event):
        if not self.filepath:
            self.filepath = "library.celebi"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class LIBRARY_OT_clear(Operator):
    bl_idname = "celebi.library_clear"
    bl_label = "Clear Library"

    def execute(self, context):
        c = type.celebi()

        c.clearLibraryItems()

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
