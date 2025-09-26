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
from . import voxel_hash


class LIBRARY_OT_save(Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".json"):
            self.filepath += ".json"

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
            self.filepath = "library.json"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class LIBRARY_OT_load(Operator):
    bl_idname = "celebi.library_load"
    bl_label = "Load Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

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
            self.filepath = "library.json"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}


class EXPORT_OT_gltf(Operator):
    """Export the library items to a GLTF binary (.glb) with applied transforms"""
    bl_idname = "celebi.export_gltf"
    bl_label = "Export for Godot (.glb)"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.glb", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".glb"):
            self.filepath += ".glb"

        c = type.celebi()
        lib_items = c.getLibraryItems()
        if not lib_items:
            self.report({"WARNING"}, "Library is empty. Nothing to export.")
            return {"CANCELLED"}

        # --- Create temporary objects with baked transforms ---
        temp_collection = bpy.data.collections.new("TEMP_EXPORT")
        context.scene.collection.children.link(temp_collection)
        
        temp_objects = []
        original_selection = list(context.selected_objects)
        active_object = context.view_layer.objects.active

        try:
            for item in lib_items:
                if not item.obj:
                    continue

                # Create a full copy of the object and its data
                export_obj = item.obj.copy()
                export_obj.data = item.obj.data.copy()
                # Add a prefix to the name to avoid conflicts with originals.
                export_obj.name = f"c_{item.obj.name}"
                temp_collection.objects.link(export_obj)


                # Build a matrix with only rotation and scale, excluding translation.
                transform_matrix = item.obj.matrix_world.to_3x3().to_4x4()

                # Apply the object-level transform to the mesh data
                voxel_hash.apply_transform_to_mesh(export_obj, transform_matrix)

                # Now, set the object's location to its original world position.
                export_obj.location = item.obj.matrix_world.translation
                temp_objects.append(export_obj)

            # --- Export only the temporary objects ---
            bpy.ops.object.select_all(action='DESELECT')
            for obj in temp_objects:
                obj.select_set(True)

            bpy.ops.export_scene.gltf(
                filepath=self.filepath,
                export_format='GLB',
                use_selection=True,
                export_apply=False # Transforms are already baked
            )

        finally:
            # --- Cleanup ---
            bpy.data.collections.remove(temp_collection) # This also removes the temp objects
            bpy.ops.object.select_all(action='DESELECT')
            # Restore original selection
            for obj in original_selection:
                obj.select_set(True)
            context.view_layer.objects.active = active_object

        self.report({"INFO"}, f"Exported to {self.filepath}")
        return {"FINISHED"}

    def invoke(self, context, event):
        self.filepath = "Voxel.glb"
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
    EXPORT_OT_gltf,
    LIBRARY_OT_clear,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
