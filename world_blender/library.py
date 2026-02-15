import bpy
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
from mathutils import Vector, Matrix
from bpy.types import Operator, Panel, UIList

import json
from . import type
from . import voxel_hash

from . import debug
logger = debug.logger

class LIBRARY_UL_items(UIList):
    name = "LIBRARY_UL_items"

    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname, index
    ):
        c = type.world_blender()
        obj = item.obj
        if obj is not None:
            row = layout.row()

            # We'll add a toggle button that calls operator to toggle selection
            row.prop(obj, "name", text="", emboss=False, icon="OBJECT_DATA")

            c.purgeLibrary()
            c.updateCurrentActiveLibraryObject()


class LIBRARY_PT_panel(Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "world_blender"

    def draw(self, context):
        l = self.layout
        c = type.world_blender()

        l.operator(EXPORT_OT_export_gltf.bl_idname, icon="EXPORT")
        l.separator()

        grid = l.grid_flow(row_major=True, columns=3)

        grid.operator(LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        grid.operator(LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        grid.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")
        l.operator(LIBRARY_OT_clear.bl_idname, icon="TRASH")

        l.separator(type="LINE")
        l.prop(c, "T_show_hash", text="Show Hashes")
        l.prop(c, "T_hash_method", text="")

        # Non-blocking compute: show start button or cancel/progress while running
        row = l.row(align=True)
        if c.T_hash_running:
            row.operator(voxel_hash.VOXEL_OT_cancel_face_hash.bl_idname, icon="CANCEL", text="Cancel")
            row.label(text=f"Hashing {c.T_hash_index}/{c.T_hash_total}")
        else:
            row.operator(
                voxel_hash.VOXEL_OT_face_hash.bl_idname,
                text=voxel_hash.VOXEL_OT_face_hash.bl_label,
            )
        l.separator(type="LINE")

        ####### Show library items in a list
        l.template_list(
            LIBRARY_UL_items.name,
            "",
            c,
            "library_items",
            c,
            "T_library_index",
        )

        ####### Show details if an item is selected
        libItems = c.getLibraryItems()
        if 0 <= c.T_library_index < len(libItems):
            item = c.getCurrentLibraryItem()

            if item and item.obj:
                grid = l.grid_flow(columns=2, row_major=True)

                grid.label(text="NX")
                grid.label(text=f"{item.hash_NX}")

                grid.label(text="PX")
                grid.label(text=f"{item.hash_PX}")

                grid.label(text="NY")
                grid.label(text=f"{item.hash_NY}")

                grid.label(text="PY")
                grid.label(text=f"{item.hash_PY}")

                grid.label(text="PZ")
                grid.label(text=f"{item.hash_PZ}")

                grid.label(text="NZ")
                grid.label(text=f"{item.hash_NZ}")

class LIBRARY_OT_add_objects(Operator):
    bl_idname = "world_blender.library_add_objects"
    bl_label = "Add Selected to Library"

    def execute(self, context):
        c = type.world_blender()
        added = 0
        for obj in context.selected_objects:
            # skip non-mesh objects
            if not hasattr(obj, "type") or obj.type != "MESH":
                continue
            if not any(item.obj == obj for item in c.getLibraryItems()):
                c.addLibraryItem(obj)
                added += 1
        logger.info("Added %d objects to library", added)
        return {"FINISHED"}


class LIBRARY_OT_save(Operator):
    bl_idname = "world_blender.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".json"):
            self.filepath += ".json"

        c = type.world_blender()

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
    bl_idname = "world_blender.library_load"
    bl_label = "Load Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.json", options={"HIDDEN"})

    def execute(self, context):
        with open(self.filepath, "r", encoding="utf-8") as f:
            data = json.load(f)

        c = type.world_blender()

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


class LIBRARY_OT_clear(Operator):
    bl_idname = "world_blender.library_clear"
    bl_label = "Clear Library"

    def execute(self, context):
        c = type.world_blender()

        # Clear stored library entries
        c.clearLibraryItems()

        # Also clear any temporary/config objects placed in the CONFIG collection
        try:
            type.clearConfigCollection()
        except Exception:
            logger.exception("Failed to clear CONFIG collection during library clear")

        c.T_library_index = -1

        self.report({"INFO"}, "Library cleared (CONFIG collection cleaned)")
        return {"FINISHED"}

class EXPORT_OT_export_gltf(Operator):
    """Export the library items to a GLTF binary (.glb) with applied transforms"""

    bl_idname = "world_blender.export_gltf"
    bl_label = "Export for Godot (.glb)"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.glb", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".glb"):
            self.filepath += ".glb"

        c = type.world_blender()
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
            # Deselect existing selection safely
            try:
                for o_sel in list(context.selected_objects):
                    try:
                        o_sel.select_set(False)
                    except Exception:
                        pass
            except Exception:
                pass

            for obj in temp_objects:
                try:
                    obj.select_set(True)
                except Exception:
                    pass

            bpy.ops.export_scene.gltf(
                filepath=self.filepath,
                export_format="GLB",
                use_selection=True,
                export_apply=False,  # Transforms are already baked
            )

        finally:
            # --- Cleanup ---
            bpy.data.collections.remove(
                temp_collection
            )  # This also removes the temp objects
            try:
                for o_sel in list(context.selected_objects):
                    try:
                        o_sel.select_set(False)
                    except Exception:
                        pass
            except Exception:
                pass

            # Restore original selection (by name check)
            for obj in original_selection:
                if obj and obj.name in bpy.data.objects:
                    try:
                        bpy.data.objects[obj.name].select_set(True)
                    except Exception:
                        pass
            try:
                if active_object and getattr(active_object, 'name', None) in bpy.data.objects:
                    context.view_layer.objects.active = bpy.data.objects[active_object.name]
            except Exception:
                pass

        self.report({"INFO"}, f"Exported to {self.filepath}")
        return {"FINISHED"}

    def invoke(self, context, event):
        self.filepath = "Voxel.glb"
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}




classes = (
    LIBRARY_OT_add_objects,
    LIBRARY_UL_items,
    LIBRARY_PT_panel,
    LIBRARY_OT_save,
    LIBRARY_OT_load,
    LIBRARY_OT_clear,
    EXPORT_OT_export_gltf,
)
