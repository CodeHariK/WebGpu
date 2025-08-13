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

class LibraryTag(bpy.types.PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(name="Enabled", default=False)


class LibraryItem(bpy.types.PropertyGroup):
    obj: PointerProperty(type=bpy.types.Object)
    custom_name: StringProperty(name="Name")
    tags: CollectionProperty(type=LibraryTag)


class LIBRARY_OT_save(bpy.types.Operator):
    bl_idname = "celebi.library_save"
    bl_label = "Save Library"

    filepath: StringProperty(subtype="FILE_PATH")
    filter_glob: StringProperty(default="*.celebi", options={"HIDDEN"})

    def execute(self, context):
        if not self.filepath.lower().endswith(".celebi"):
            self.filepath += ".celebi"

        data = {
            "all_tags": list(context.scene.library_tags), 
            "items": []
        }

        for item in context.scene.object_library:
            enabled_tags = [t.name for t in item.tags if t.enabled]
            entry = {
                "object_name": item.obj.name if item.obj else None,
                "custom_name": item.custom_name,
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

        # Load all_tags into scene.library_tags
        context.scene.library_tags.clear()
        for tag_name in data.get("all_tags", []):
            context.scene.library_tags.append(tag_name)

        # Clear existing
        context.scene.object_library.clear()

        for entry in data.get("items", []):
            item = context.scene.object_library.add()
            if entry["object_name"] in bpy.data.objects:
                item.obj = bpy.data.objects[entry["object_name"]]
            item.custom_name = entry["custom_name"]

            # Restore tags collection with enabled status
            enabled_tags = set(entry.get("tags", []))
            for tag_name in context.scene.library_tags:
                tag_entry = item.tags.add()
                tag_entry.name = tag_name
                tag_entry.enabled = tag_name in enabled_tags

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
        context.scene.object_library.clear()
        self.report({"INFO"}, "Library cleared")
        return {"FINISHED"}


def update_tags(self, context):
    for item in context.scene.object_library:
        for tag_name in context.scene.library_tags:
            if tag_name not in [t.name for t in item.tags]:
                tag = item.tags.add()
                tag.name = tag_name
                tag.enabled = False

# Operator to add new tag
class LIBRARY_OT_add_tag(bpy.types.Operator):
    bl_idname = "celebi.library_add_tag"
    bl_label = "Add Tag"
    new_tag: StringProperty(name="New Tag")

    def execute(self, context):
        tag = self.new_tag.strip()
        if tag and tag not in context.scene.library_tags:
            context.scene.library_tags.append(tag)
            # Add this tag to every existing LibraryItem's tags collection
            for item in context.scene.object_library:
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
        lib = context.scene.object_library
        for obj in context.selected_objects:
            if not any(item.obj == obj for item in lib):
                item = lib.add()
                item.obj = obj
                item.custom_name = obj.name
                # Add a LibraryTag entry for each existing tag in scene.library_tags
                for tag_name in context.scene.library_tags:
                    tag_entry = item.tags.add()
                    tag_entry.name = tag_name
                    tag_entry.enabled = False
        return {"FINISHED"}


class LIBRARY_UL_items(bpy.types.UIList):
    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname
    ):
        if item.obj:
            layout.label(text=item.custom_name, icon="MESH_CUBE")
        else:
            layout.label(text="<Missing>", icon="ERROR")


class LIBRARY_VIEW3D_PT_library(bpy.types.Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Library"

    def select_library_object(self):
        scene = bpy.context.scene
        if not scene.selecting_from_library:
            return None
        scene.selecting_from_library = False

        print("no")

        scene = bpy.context.scene
        index = scene.library_index
        if index is not None and 0 <= index < len(scene.object_library):
            item = scene.object_library[index]
            if item.obj:
                bpy.ops.object.select_all(action="DESELECT")
                item.obj.select_set(True)
                bpy.context.view_layer.objects.active = item.obj

                print("Hello")

        return None  # No repeat


    def update_selection(self):
        scene = bpy.context.scene
        if scene.library_index != scene.get("_last_library_index", -1):
            def deferred_set():
                scene["_selecting_from_library"] = True
                scene["_last_library_index"] = scene.library_index
                self.select_library_object()
                print("Hi")
                return None
            bpy.app.timers.register(deferred_set, first_interval=0.01)

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        row = layout.row()

        row.operator(LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        row.operator(LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        row.operator(LIBRARY_OT_clear.bl_idname, icon="TRASH")
        row.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")
        row.prop(scene, "new_tag_name", text="")
        row.operator(
            LIBRARY_OT_add_tag.bl_idname, text="", icon="ADD"
        ).new_tag = scene.new_tag_name

        layout.template_list(
            "LIBRARY_UL_items", "", scene, "object_library", scene, "library_index"
        )

        self.update_selection()

        # Show details if an item is selected
        if 0 <= scene.library_index < len(scene.object_library):
            item = scene.object_library[scene.library_index]
            if item.obj:
                layout.prop(item, "custom_name")
                layout.label(text="Tags:")
                for tag in item.tags:
                    layout.prop(tag, "enabled", text=tag.name)


def register():
    bpy.utils.register_class(LibraryTag)
    bpy.utils.register_class(LibraryItem)

    bpy.utils.register_class(LIBRARY_OT_save)
    bpy.utils.register_class(LIBRARY_OT_load)
    bpy.utils.register_class(LIBRARY_OT_clear)
    bpy.utils.register_class(LIBRARY_OT_add_tag)
    bpy.utils.register_class(LIBRARY_OT_add_objects)

    bpy.utils.register_class(LIBRARY_VIEW3D_PT_library)
    bpy.utils.register_class(LIBRARY_UL_items)

    bpy.types.Scene.object_library = CollectionProperty(type=LibraryItem)

    bpy.types.Scene.last_library_index = bpy.props.IntProperty(default=-1)
    bpy.types.Scene.selecting_from_library = bpy.props.BoolProperty(default=False)


def unregister():
    bpy.utils.unregister_class(LibraryTag)
    bpy.utils.unregister_class(LibraryItem)

    bpy.utils.unregister_class(LIBRARY_OT_save)
    bpy.utils.unregister_class(LIBRARY_OT_load)
    bpy.utils.unregister_class(LIBRARY_OT_clear)
    bpy.utils.unregister_class(LIBRARY_OT_add_tag)
    bpy.utils.unregister_class(LIBRARY_OT_add_objects)

    bpy.utils.unregister_class(LIBRARY_VIEW3D_PT_library)
    bpy.utils.unregister_class(LIBRARY_UL_items)

    del bpy.types.Scene.last_library_index
    del bpy.types.Scene.selecting_from_library

