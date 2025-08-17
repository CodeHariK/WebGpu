import bpy
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
from bpy.types import Operator, Panel, UIList
from . import type
from . import save

# Update add_library_item to use separate face collections
face_collections = [
    "face_front",
    "face_back",
    "face_left",
    "face_right",
    "face_top",
    "face_bottom",
]


class LIBRARY_OT_add_tag(Operator):
    bl_idname = "celebi.library_add_tag"
    bl_label = "Add Tag"

    new_tag: StringProperty(name="New Tag")

    def execute(self, context):
        c = type.celebi()

        tag = self.new_tag.strip()
        if tag and tag not in c.getLibraryTags():
            c.appendTag(tag)

            # Add this tag to every existing LibraryItem's tags collection
            for item in c.getLibraryItems():
                if tag not in [t.name for t in item.tags]:
                    c.addLibraryItem(tag, False, None)

        return {"FINISHED"}


class LIBRARY_OT_add_objects(Operator):
    bl_idname = "celebi.library_add_objects"
    bl_label = "Add Selected to Library"

    def execute(self, context):
        c = type.celebi()
        for obj in context.selected_objects:
            if not any(item.obj == obj for item in c.getLibraryItems()):
                item = c.addLibraryItem(None, None, obj)

                for tag_item in c.getLibraryTags():
                    item.appendTag(tag_item.name, False)

                # Populate configs with all enum options
                for identifier, name, desc in type.cube_configurations(None, None):
                    cfg = item.configs.add()
                    cfg.name = identifier
                    cfg.enabled = False

                # for col_name in face_collections:
                #     face_collection = getattr(item, col_name)
                #     f = face_collection.add()

        return {"FINISHED"}


class LIBRARY_UL_items(UIList):
    name = "LIBRARY_UL_items"

    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname, index
    ):
        obj = item.obj
        if obj is not None:
            row = layout.row()

            # We'll add a toggle button that calls operator to toggle selection
            row.prop(obj, "name", text="", emboss=False, icon="OBJECT_DATA")

            # Checkbox reflects obj selection state
            selected = obj.select_get()
            op = row.operator(
                LIBRARY_OT_toggle_object_selection.bl_idname,
                text="",
                icon="CHECKBOX_HLT" if selected else "CHECKBOX_DEHLT",
                emboss=True,
            )
            op.obj_name = obj.name


class LIBRARY_OT_toggle_object_selection(Operator):
    bl_idname = "celebi.library_toggle_object_selection"
    bl_label = "Toggle Object Selection"
    bl_description = "Toggle selection of the object in the viewport"
    bl_options = {"INTERNAL"}

    obj_name: StringProperty(name="Object Name")

    def execute(self, context):
        obj = bpy.data.objects.get(self.obj_name)
        if obj is None:
            self.report({"WARNING"}, "Object not found")
            return {"CANCELLED"}
        obj.select_set(not obj.select_get())
        if obj.select_get():
            context.view_layer.objects.active = obj
        elif context.view_layer.objects.active == obj:
            context.view_layer.objects.active = None
        return {"FINISHED"}


class LIBRARY_PT_panel(Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        layout = self.layout
        c = type.celebi()

        grid = layout.grid_flow(row_major=True, columns=2)

        grid.operator(save.LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        grid.operator(save.LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        grid.operator(save.LIBRARY_OT_clear.bl_idname, icon="TRASH")
        grid.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")

        layout.separator(type="LINE")

        tagrow = layout.row()
        tagrow.prop(c, "T_new_tag_name", text="")
        tagrow.operator(
            LIBRARY_OT_add_tag.bl_idname, text="", icon="ADD"
        ).new_tag = c.T_new_tag_name

        layout.separator(type="LINE")

        layout.template_list(
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
                layout.label(text="Tags:")
                for tag in item.tags:
                    layout.prop(tag, "enabled", text=tag.name)

                layout.separator_spacer()

                layout.label(text="Configurations:")
                for config in item.configs:
                    row = layout.row()
                    row.prop(config, "enabled", text=config.name)
                    # if config.enabled:
                    #     obj_name = item.obj.name if item.obj else "Unnamed"
                    #     print(f"Config {config.name} enabled for object {obj_name}")

                layout.separator_spacer()

                # Update panel draw for separate face collections
                layout.label(text="Face Links:")
                for col_name in face_collections:
                    face_collection = getattr(item, col_name)
                    for face_entry in face_collection:
                        row = layout.row()
                        row.label(text=col_name.lstrip("face_"))
                        row.prop(face_entry, "library_item")
                        row.prop(face_entry, "config", text="")


classes = (
    LIBRARY_OT_add_tag,
    LIBRARY_OT_add_objects,
    LIBRARY_OT_toggle_object_selection,
    LIBRARY_UL_items,
    LIBRARY_PT_panel,
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
