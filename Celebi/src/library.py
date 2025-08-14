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
from bpy.app.handlers import persistent
from bpy.types import Operator, Panel, UIList, PropertyGroup
from mathutils import Vector
from bpy_extras import view3d_utils
from . import type

import json

class LIBRARY_OT_save(bpy.types.Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".celebi"):
            self.filepath += ".celebi"

        c = type.celebi(bpy.context)

        data = {"all_tags": [t.name for t in c.library_tags], "items": []}

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

        c = type.celebi(bpy.context)

        # Load all_tags into scene.library_tags
        c.library_tags.clear()
        for tag_name in data.get("all_tags", []):
            new_tag_entry = c.library_tags.add()
            new_tag_entry.name = tag_name

        c = type.celebi(bpy.context)

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
        c = type.celebi(bpy.context)

        c.library_items.clear()

        self.report({"INFO"}, "Library cleared")
        return {"FINISHED"}


class LIBRARY_OT_add_tag(bpy.types.Operator):
    bl_idname = "celebi.library_add_tag"
    bl_label = "Add Tag"
    new_tag: StringProperty(name="New Tag")

    def execute(self, context):
        c = type.celebi(bpy.context)

        tag = self.new_tag.strip()
        if tag and tag not in c.library_tags:
            new_tag_entry = c.library_tags.add()
            new_tag_entry.name = tag

            # Add this tag to every existing LibraryItem's tags collection
            for item in c.library_items:
                if tag not in [t.name for t in item.tags]:
                    new_tag_entry = item.tags.add()
                    new_tag_entry.name = tag
                    new_tag_entry.enabled = False

        return {"FINISHED"}


class LIBRARY_OT_add_objects(bpy.types.Operator):
    bl_idname = "celebi.library_add_objects"
    bl_label = "Add Selected to Library"

    def execute(self, context):
        c = type.celebi(bpy.context)
        for obj in context.selected_objects:
            if not any(item.obj == obj for item in c.library_items):
                item = c.library_items.add()
                item.obj = obj
                # Add a LibraryTag entry for each existing tag in scene.library_tags
            for tag_item in c.library_tags:
                tag_entry = item.tags.add()
                tag_entry.name = tag_item.name
                tag_entry.enabled = False
        return {"FINISHED"}


class LIBRARY_UL_items(bpy.types.UIList):
    name = "LIBRARY_UL_items"

    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname, index
    ):
        obj = item.obj
        if obj is not None:
            
            row = layout.row()
            # Checkbox reflects obj selection state
            selected = obj.select_get()
            # We'll add a toggle button that calls operator to toggle selection
            row.prop(obj, "name", text="", emboss=False, icon="OBJECT_DATA")
            op = row.operator(
                LIBRARY_OT_toggle_object_selection.bl_idname,
                text="",
                icon="CHECKBOX_HLT" if selected else "CHECKBOX_DEHLT",
                emboss=True,
            )
            op.obj_name = obj.name


class LIBRARY_OT_toggle_object_selection(bpy.types.Operator):
    bl_idname = "celebi.toggle_object_selection"
    bl_label = "Toggle Object Selection"
    bl_description = "Toggle selection of the object in the viewport"

    obj_name: bpy.props.StringProperty()

    def execute(self, context):
        obj = bpy.data.objects.get(self.obj_name)
        if obj is None:
            self.report({"WARNING"}, "Object not found")
            return {"CANCELLED"}
        obj.select_set(not obj.select_get())
        # Optionally, set active object if selected
        if obj.select_get():
            context.view_layer.objects.active = obj
        else:
            # If deselected and active object is this obj, clear active object
            if context.view_layer.objects.active == obj:
                context.view_layer.objects.active = None
        return {"FINISHED"}


class LIBRARY_VIEW3D_PT_library(bpy.types.Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        layout = self.layout
        c = type.celebi(bpy.context)

        grid = layout.grid_flow(row_major=True, columns=2)

        grid.operator(LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        grid.operator(LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        grid.operator(LIBRARY_OT_clear.bl_idname, icon="TRASH")
        grid.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")

        layout.separator(type="LINE")

        tagrow = layout.row()
        tagrow.prop(c, "new_tag_name", text="")
        tagrow.operator(
            LIBRARY_OT_add_tag.bl_idname, text="", icon="ADD"
        ).new_tag = c.new_tag_name

        layout.separator(type="LINE")

        layout.template_list(
            LIBRARY_UL_items.name,
            "",
            c,
            "library_items",
            c,
            "library_index",
        )

        lib = c.library_items

        # Show details if an item is selected
        if 0 <= c.library_index < len(lib):
            item = lib[c.library_index]
            if item.obj:
                layout.label(text="Tags:")
                for tag in item.tags:
                    layout.prop(tag, "enabled", text=tag.name)


def register():

    bpy.utils.register_class(LIBRARY_OT_save)
    bpy.utils.register_class(LIBRARY_OT_load)
    bpy.utils.register_class(LIBRARY_OT_clear)
    bpy.utils.register_class(LIBRARY_OT_add_tag)
    bpy.utils.register_class(LIBRARY_OT_add_objects)

    bpy.utils.register_class(LIBRARY_VIEW3D_PT_library)
    bpy.utils.register_class(LIBRARY_UL_items)

    bpy.utils.register_class(LIBRARY_OT_toggle_object_selection)


def unregister():

    bpy.utils.unregister_class(LIBRARY_OT_save)
    bpy.utils.unregister_class(LIBRARY_OT_load)
    bpy.utils.unregister_class(LIBRARY_OT_clear)
    bpy.utils.unregister_class(LIBRARY_OT_add_tag)
    bpy.utils.unregister_class(LIBRARY_OT_add_objects)

    bpy.utils.unregister_class(LIBRARY_VIEW3D_PT_library)
    bpy.utils.unregister_class(LIBRARY_UL_items)

    bpy.utils.unregister_class(LIBRARY_OT_toggle_object_selection)
