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


class LIBRARY_OT_add_tag(Operator):
    bl_idname = "celebi.library_add_tag"
    bl_label = "Add Tag"

    new_tag: StringProperty(name="New Tag")

    def execute(self, context):
        c = type.celebi()

        tag_name = self.new_tag.strip()
        if tag_name and tag_name not in c.getLibraryTags():
            c.appendTag(tag_name)

            # Add this tag to every existing LibraryItem's tags collection
            for item in c.getLibraryItems():
                if tag_name not in [t.name for t in item.tags]:
                    item.appendTag(tag_name=tag_name, enabled=False)

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
                    item.appendConfig(identifier, False)

                # for col_name in face_collections:
                #     face_collection = getattr(item, col_name)
                #     f = face_collection.add()

        return {"FINISHED"}


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


def spawn_config_variants(libItem):
    """Spawn this library item in +Y direction, one step per config"""
    base_obj = libItem.obj
    if not base_obj:
        return

    CONFIG_COLLECTION = type.getConfigCollection()

    for obj in list(CONFIG_COLLECTION.objects):
        bpy.data.objects.remove(obj, do_unlink=True)

    offset_index = 1
    for cfg in libItem.configs:
        if not cfg.enabled:
            continue

        # make linked duplicate
        new_obj = base_obj.copy()
        new_obj.data = base_obj.data
        new_obj.animation_data_clear()

        # apply transform
        mat = type.CONFIG_TRANSFORMS.get(cfg.name, Matrix.Identity(4))
        new_obj.matrix_world @= mat

        # name it
        new_obj.name = f"{base_obj.name}_{cfg.name}"

        # link to collection
        CONFIG_COLLECTION.objects.link(new_obj)

        new_obj.location = base_obj.location + Vector((0, offset_index, 0))
        print(base_obj.location, new_obj.location)

        offset_index += 1


class LIBRARY_OT_toggle_config(bpy.types.Operator):
    bl_idname = "celebi.toggle_config"
    bl_label = "Toggle Config"

    item_index: bpy.props.IntProperty()

    def execute(self, context):
        c = type.celebi()
        libItem = c.getCurrentLibraryItem()
        config = libItem.configs[self.item_index]

        config.enabled = not config.enabled

        for c in libItem.getConfigs():
            if c.enabled:
                spawn_config_variants(libItem)

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

            c.updateCurrentActiveLibraryObject()

            # Checkbox reflects obj selection state
            selected = obj.select_get()
            op = row.operator(
                LIBRARY_OT_toggle_object_selection.bl_idname,
                text="",
                icon="CHECKBOX_HLT" if selected else "CHECKBOX_DEHLT",
                emboss=True,
            )
            op.obj_name = obj.name


class LIBRARY_PT_panel(Panel):
    bl_label = "Object Library"
    bl_idname = "VIEW3D_PT_library"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        l = self.layout
        c = type.celebi()

        grid = l.grid_flow(row_major=True, columns=2)

        grid.operator(save.LIBRARY_OT_save.bl_idname, icon="FILE_TICK")
        grid.operator(save.LIBRARY_OT_load.bl_idname, icon="FILE_FOLDER")
        grid.operator(save.LIBRARY_OT_clear.bl_idname, icon="TRASH")
        grid.operator(LIBRARY_OT_add_objects.bl_idname, icon="PLUS")

        l.separator(type="LINE")

        tagrow = l.row()
        tagrow.prop(c, "T_new_tag_name", text="")
        tagrow.operator(
            LIBRARY_OT_add_tag.bl_idname, text="", icon="ADD"
        ).new_tag = c.T_new_tag_name

        l.separator(type="LINE")

        l.operator(
            voxel_hash.VOXEL_OT_face_hash.bl_idname,
            text=voxel_hash.VOXEL_OT_face_hash.bl_label,
        )

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
                l.label(text="Tags:")
                for tag in item.getTags():
                    l.prop(tag, "enabled", text=tag.name)

                l.separator(type="LINE")

                grid = l.grid_flow(columns=2, row_major=True)

                grid.label(text="NY")
                grid.label(text=f"{item.hash_NY}")

                grid.label(text="PY")
                grid.label(text=f"{item.hash_PY}")

                grid.label(text="NX")
                grid.label(text=f"{item.hash_NX}")

                grid.label(text="PX")
                grid.label(text=f"{item.hash_PX}")

                grid.label(text="PZ")
                grid.label(text=f"{item.hash_PZ}")

                grid.label(text="NZ")
                grid.label(text=f"{item.hash_NZ}")

                l.separator(type="LINE")

                l.label(text="Configurations:")
                configGrid = l.grid_flow(columns=3)
                for i, config in enumerate(item.configs):
                    # configGrid.prop(config, "enabled", text=config.name)
                    # # if config.enabled:
                    # #     print(f"Config {config.name} enabled for object {item.obj.name}")

                    op = configGrid.operator(
                        LIBRARY_OT_toggle_config.bl_idname,
                        text=config.name,
                        depress=config.enabled,
                    )
                    op.item_index = i

                l.separator(type="LINE")

                # Update panel draw for separate face collections
                l.label(text="Face Links:")
                for col_name in type.FACE_COLLECTIONS:
                    face_collection = getattr(item, col_name)
                    for face_entry in face_collection:
                        row = l.row()
                        row.label(text=col_name.lstrip("face_"))
                        row.prop(face_entry, "library_item")
                        row.prop(face_entry, "CONFIG", text="")


classes = (
    LIBRARY_OT_add_tag,
    LIBRARY_OT_add_objects,
    LIBRARY_OT_toggle_config,
    LIBRARY_OT_toggle_object_selection,
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
