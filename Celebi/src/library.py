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
from bpy.types import Operator, Panel, UIList, PropertyGroup
from mathutils import Vector
from bpy_extras import view3d_utils

import json


class LIBRARY_OT_save(bpy.types.Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".celebi"):
            self.filepath += ".celebi"

        celebi_data = context.window_manager.celebi_data

        data = {"all_tags": [t.name for t in celebi_data.library_tags], "items": []}

        library_items = celebi_data.library_items

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

        celebi_data = context.window_manager.celebi_data

        # Load all_tags into scene.library_tags
        celebi_data.library_tags.clear()
        for tag_name in data.get("all_tags", []):
            new_tag_entry = celebi_data.library_tags.add()
            new_tag_entry.name = tag_name

        celebi_data = context.window_manager.celebi_data

        # Clear existing
        celebi_data.library_items.clear()

        for entry in data.get("items", []):
            item = celebi_data.library_items.add()
            if entry["object_name"] in bpy.data.objects:
                item.obj = bpy.data.objects[entry["object_name"]]

            # Restore tags collection with enabled status
            enabled_tags = set(entry.get("tags", []))
            for tag_item in celebi_data.library_tags:
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
        celebi_data = context.window_manager.celebi_data

        celebi_data.library_items.clear()

        self.report({"INFO"}, "Library cleared")
        return {"FINISHED"}


def update_tags(self, context):
    celebi_data = context.window_manager.celebi_data

    for item in celebi_data.library_items:
        for tag_item in celebi_data.library_tags:  # tag_item is a TagItem
            if tag_item.name not in [t.name for t in item.tags]:
                new_tag = item.tags.add()
                new_tag.name = tag_item.name
                new_tag.enabled = False


# Operator to add new tag
class LIBRARY_OT_add_tag(bpy.types.Operator):
    bl_idname = "celebi.library_add_tag"
    bl_label = "Add Tag"
    new_tag: StringProperty(name="New Tag")

    def execute(self, context):
        celebi_data = context.window_manager.celebi_data

        tag = self.new_tag.strip()
        if tag and tag not in celebi_data.library_tags:
            new_tag_entry = celebi_data.library_tags.add()
            new_tag_entry.name = tag

            # Add this tag to every existing LibraryItem's tags collection

            for item in context.window_manager.celebi_data.library_items:
                if tag not in [t.name for t in item.tags]:
                    new_tag_entry = item.tags.add()
                    new_tag_entry.name = tag
                    new_tag_entry.enabled = False
            update_tags(None, context)
        return {"FINISHED"}


# Operator to add selected objects to library
class LIBRARY_OT_add_objects(bpy.types.Operator):
    bl_idname = "celebi.library_add_objects"
    bl_label = "Add Selected to Library"

    def execute(self, context):
        celebi_data = context.window_manager.celebi_data
        for obj in context.selected_objects:
            if not any(item.obj == obj for item in celebi_data.library_items):
                item = celebi_data.library_items.add()
                item.obj = obj
                # Add a LibraryTag entry for each existing tag in scene.library_tags
                for tag_name in celebi_data.library_tags:
                    tag_entry = item.tags.add()
                    tag_entry.name = tag_name
                    tag_entry.enabled = False
        return {"FINISHED"}


class LIBRARY_UL_items(bpy.types.UIList):
    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname
    ):
        if item.obj:
            layout.label(text=item.obj.name, icon="MESH_CUBE")
        else:
            layout.label(text="<Missing>", icon="ERROR")


class LIBRARY_VIEW3D_PT_library(bpy.types.Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def select_library_object(self):
        scene = bpy.context.scene
        celebi_data = bpy.context.window_manager.celebi_data
        index = celebi_data.library_index
        lib = celebi_data.library_items
        if index is not None and 0 <= index < len(lib):
            item = lib[index]
            if item.obj:
                bpy.ops.object.select_all(action="DESELECT")
                item.obj.select_set(True)
                bpy.context.view_layer.objects.active = item.obj

        return None  # No repeat

    def update_selection(self):
        scene = bpy.context.scene
        celebi_data = bpy.context.window_manager.celebi_data

        if celebi_data.library_index != celebi_data.last_library_index:

            def deferred_set():
                celebi_data.last_library_index = celebi_data.library_index
                self.select_library_object()
                return None

            bpy.app.timers.register(deferred_set, first_interval=0.01)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        celebi_data = bpy.context.window_manager.celebi_data

        row = layout.row()

        row.operator(LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        row.operator(LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        row.operator(LIBRARY_OT_clear.bl_idname, icon="TRASH")
        row.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")
        row.prop(celebi_data, "new_tag_name", text="")
        row.operator(
            LIBRARY_OT_add_tag.bl_idname, text="", icon="ADD"
        ).new_tag = celebi_data.new_tag_name

        layout.template_list(
            "LIBRARY_UL_items",
            "",
            context.window_manager.celebi_data,
            "library_items",
            context.window_manager.celebi_data,
            "library_index",
        )

        self.update_selection()

        celebi_data = context.window_manager.celebi_data
        lib = celebi_data.library_items

        # Show details if an item is selected
        if 0 <= celebi_data.library_index < len(lib):
            item = lib[celebi_data.library_index]
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


def unregister():
    bpy.utils.unregister_class(LIBRARY_OT_save)
    bpy.utils.unregister_class(LIBRARY_OT_load)
    bpy.utils.unregister_class(LIBRARY_OT_clear)
    bpy.utils.unregister_class(LIBRARY_OT_add_tag)
    bpy.utils.unregister_class(LIBRARY_OT_add_objects)

    bpy.utils.unregister_class(LIBRARY_VIEW3D_PT_library)
    bpy.utils.unregister_class(LIBRARY_UL_items)
