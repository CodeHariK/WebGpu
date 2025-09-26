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
from . import type
from . import save
from . import voxel_hash


class LIBRARY_OT_add_objects(Operator):
    bl_idname = "celebi.library_add_objects"
    bl_label = "Add Selected to Library"

    def execute(self, context):
        c = type.celebi()
        for obj in context.selected_objects:
            if not any(item.obj == obj for item in c.getLibraryItems()):
                item = c.addLibraryItem(obj)

        return {"FINISHED"}


class LIBRARY_UL_items(UIList):
    name = "LIBRARY_UL_items"

    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname, index
    ):
        c = type.celebi()
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
    bl_category = "Celebi"

    def draw(self, context):
        l = self.layout
        c = type.celebi()

        l.operator(save.EXPORT_OT_gltf.bl_idname, icon="EXPORT")
        l.separator()

        grid = l.grid_flow(row_major=True, columns=3)

        grid.operator(save.LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        grid.operator(save.LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        grid.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")

        l.separator(type="LINE")

        l.operator(
            voxel_hash.VOXEL_OT_face_hash.bl_idname,
            text=voxel_hash.VOXEL_OT_face_hash.bl_label,
        )

        l.operator(save.LIBRARY_OT_clear.bl_idname, icon="TRASH")

        l.prop(c, "T_show_hash", text="Show Hashes")

        l.separator(type="LINE")

        l.template_list(
            LIBRARY_UL_items.name,
            "",
            c,
            "library_items",
            c,
            "T_library_index",
        )

        libItems = c.getLibraryItems()

        # Show details if an item is selected
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


classes = (
    LIBRARY_OT_add_objects,
    LIBRARY_UL_items,
    LIBRARY_PT_panel,
    voxel_hash.VOXEL_OT_face_hash,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
