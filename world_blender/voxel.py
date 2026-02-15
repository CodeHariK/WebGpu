import time
import math
import bpy
from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
from bpy.types import Operator, Panel, UIList, PropertyGroup, Object
from mathutils import Vector
from bpy_extras import view3d_utils

from . import type
from . import preview
from . import gizmo


class VOXEL_OT_delete(Operator):
    bl_idname = "world_blender.voxel_delete"
    bl_label = "Delete Voxel"
    bl_description = "Delete the selected voxel"

    voxel_name: StringProperty()

    def execute(self, context):
        c = type.world_blender()

        voxel_obj = bpy.data.objects.get(self.voxel_name)
        if voxel_obj:
            bpy.data.objects.remove(voxel_obj, do_unlink=True)

        c.deleteVoxelByName(self.voxel_name)

        return {"FINISHED"}


class VOXEL_OT_delete_all(Operator):
    bl_idname = "world_blender.voxel_delete_all"
    bl_label = "Delete all voxels"

    def execute(self, context):
        c = type.world_blender()

        items = c.getVoxels()
        for i, item in enumerate(items):
            voxel_obj = bpy.data.objects.get(item.name)
            if voxel_obj:
                bpy.data.objects.remove(voxel_obj, do_unlink=True)

        c.purgeVoxels()

        return {"FINISHED"}


VOXEL_PREVIEW = "VOXEL_PREVIEW"


class VOXEL_OT_hover(Operator):
    bl_idname = "world_blender.voxel_hover"
    bl_label = "Voxel Hover"
    bl_description = "Spawn voxel by hovering over surfaces"
    bl_options = {"REGISTER", "UNDO"}

    _left_down = False
    _right_down = False
    _last_spawn_time = 0.0

    def spawn_voxel(
        self, c: type.world_blenderData, snapped_location: Vector, now: float
    ):
        libItem = c.getCurrentLibraryItem()
        if not libItem:
            return
        libObj = libItem.obj
        if not libObj or libObj.name not in bpy.data.objects:
            return

        # Prevent duplicates
        for item in c.getVoxels():
            voxel = bpy.data.objects.get(item.name)
            if voxel and (voxel.location - snapped_location).length < 0.001:
                return

        src_obj = bpy.data.objects[libObj.name]
        new_voxel = src_obj.copy()
        new_voxel.data = src_obj.data
        new_voxel.location = snapped_location
        new_voxel.name = type.voxel_name(src_obj.name, snapped_location)
        type.objLinkCollection(new_voxel)
        c.addVoxel(new_voxel.name)

        self._last_spawn_time = now

    def deleteVoxel(self, c, ray_obj: Object, now: float):
        c.deleteVoxelByName(ray_obj.name)
        bpy.data.objects.remove(ray_obj, do_unlink=True)
        self._last_spawn_time = now

    def modal(self, context, event):
        c = type.world_blender()

        _preview_voxel = bpy.data.objects.get(VOXEL_PREVIEW)

        if event.type in {"ESC"}:
            if _preview_voxel:
                bpy.data.objects.remove(_preview_voxel, do_unlink=True)
                _preview_voxel = None

            c.T_voxel_hover_running = False

            return {"CANCELLED"}

        coord = (event.mouse_region_x, event.mouse_region_y)

        preview_hidden = False
        if _preview_voxel and not _preview_voxel.hide_viewport:
            _preview_voxel.hide_viewport = True
            preview_hidden = True

        ray_origin = view3d_utils.region_2d_to_origin_3d(
            context.region, context.region_data, coord
        )
        ray_direction = view3d_utils.region_2d_to_vector_3d(
            context.region, context.region_data, coord
        )
        ray_result, ray_location, ray_normal, ray_index, ray_obj, ray_matrix = (
            context.scene.ray_cast(
                context.view_layer.depsgraph, ray_origin, ray_direction
            )
        )

        # Restore preview voxel visibility
        if preview_hidden:
            _preview_voxel.hide_viewport = False

        now = time.time()
        debounce = (now - self._last_spawn_time) > 0.2

        offset_location = ray_location + ray_normal * 0.1
        snapped_location = Vector(
            (
                round(offset_location.x - 0.5) + 0.5,
                round(offset_location.y + 0.5) - 0.5,
                round(offset_location.z - 0.5) + 0.5,
            )
        )

        if event.type == "LEFTMOUSE" and event.value == "PRESS":
            self._left_down = True
            self.spawn_voxel(c, snapped_location, now)
            return {"RUNNING_MODAL"}
        if event.type == "RIGHTMOUSE" and event.value == "PRESS":
            self._right_down = True
            self.deleteVoxel(c=c, ray_obj=ray_obj, now=now)
            return {"RUNNING_MODAL"}

        if event.type == "LEFTMOUSE" and event.value == "RELEASE":
            self._left_down = False
            return {"RUNNING_MODAL"}
        if event.type == "RIGHTMOUSE" and event.value == "RELEASE":
            self._right_down = False
            return {"RUNNING_MODAL"}

        if ray_result:
            if not _preview_voxel:
                bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                _preview_voxel = context.active_object
                _preview_voxel.name = VOXEL_PREVIEW
                _preview_voxel.hide_select = True
                _preview_voxel.display_type = "WIRE"
                _preview_voxel.show_in_front = True
                type.objLinkCollection(_preview_voxel)
            else:
                _preview_voxel.hide_viewport = False
                _preview_voxel.location = snapped_location
        elif _preview_voxel:
            _preview_voxel.hide_viewport = True

        if self._left_down and ray_result and debounce:
            self.spawn_voxel(c, snapped_location, now)
            return {"RUNNING_MODAL"}

        if (
            self._right_down
            and ray_obj
            and ray_obj.name.startswith("voxel_")
            and debounce
        ):
            self.deleteVoxel(c=c, ray_obj=ray_obj, now=now)
            return {"RUNNING_MODAL"}

        return {"PASS_THROUGH"}

    def invoke(self, context, event):
        c = type.world_blender()
        # if c.T_voxel_hover_running:
        #     self.report({"WARNING"}, "Voxel Hover is already running")
        #     return {"CANCELLED"}

        c.T_voxel_hover_running = True

        context.window_manager.modal_handler_add(self)

        return {"RUNNING_MODAL"}


class VOXEL_OT_toggle_object_selection(Operator):
    bl_idname = "world_blender.voxel_toggle_object_selection"
    bl_label = "Toggle Voxel Object Selection"
    bl_description = "Toggle selection of the voxel object in the viewport"
    bl_options = {"INTERNAL"}

    obj_name: StringProperty()

    def execute(self, context):
        obj = bpy.data.objects.get(self.obj_name)
        if obj is None:
            self.report({"WARNING"}, f"Object '{self.obj_name}' not found")
            return {"CANCELLED"}

        # Toggle selection
        obj.select_set(not obj.select_get())

        # Update active object if selected
        if obj.select_get():
            context.view_layer.objects.active = obj
        else:
            # If deselected and it was active, clear active
            if context.view_layer.objects.active == obj:
                context.view_layer.objects.active = None

        return {"FINISHED"}


class VOXEL_UL_items(UIList):
    def draw_item(
        self, context, layout, data, item, icon, active_data, active_propname, index
    ):
        obj = bpy.data.objects.get(item.name)
        if obj:
            row = layout.row(align=True)

            toggle_obj_selection_ot = row.operator(
                VOXEL_OT_toggle_object_selection.bl_idname,
                text="",
                icon="CHECKBOX_HLT" if obj.select_get() else "CHECKBOX_DEHLT",
                emboss=False,
            )
            toggle_obj_selection_ot.obj_name = obj.name

            row.label(text=obj.name)

            op_del = row.operator(VOXEL_OT_delete.bl_idname, text="", icon="X")
            op_del.voxel_name = obj.name


class VOXEL_PT_panel(Panel):
    bl_label = "Voxel Panel"
    bl_idname = "VOXEL_PT_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "world_blender"

    def draw(self, context):
        l = self.layout
        c = type.world_blender()

        c.purgeVoxels()

        # Sync selection state from objects to voxel items
        selected_objs = set(obj.name for obj in bpy.context.selected_objects)
        for item in c.getVoxels():
            item.selected = item.name in selected_objs

        l.operator(
            VOXEL_OT_hover.bl_idname,
            text="Hover Running" if c.T_voxel_hover_running else "Start Hover",
            icon="PAUSE" if c.T_voxel_hover_running else "PLAY",
            depress=c.T_voxel_hover_running,
        )

        l.operator(VOXEL_OT_delete_all.bl_idname, text=VOXEL_OT_delete_all.bl_label)

        l.operator(
            gizmo.VOXEL_OT_toggle_gizmo.bl_idname,
            text=gizmo.VOXEL_OT_toggle_gizmo.bl_label,
        )

        active = bpy.context.active_object
        currentLibItem = c.getCurrentLibraryItem()
        if active and currentLibItem and currentLibItem.obj:
            # if active and active.name.startswith("voxel"):

            l.label(text=currentLibItem.obj.name)

            l.prop(c, "T_dim_x", slider=False)
            l.prop(c, "T_dim_y", slider=True)
            l.prop(c, "T_dim_z", slider=True)

            row = l.row(align=True)
            op = row.operator(
                VOXEL_OT_action.bl_idname, text="Confirm", icon="CHECKMARK"
            )
            op.action = "CONFIRM"
            op = row.operator(VOXEL_OT_action.bl_idname, text="Cancel", icon="CANCEL")
            op.action = "CANCEL"

        l.label(text="Voxels:")
        l.template_list("VOXEL_UL_items", "", c, "voxels", c, "T_voxels_index", rows=4)


class VOXEL_OT_action(Operator):
    bl_idname = "world_blender.voxel_ot_action"
    bl_label = "Voxel Action"

    action: StringProperty()

    def execute(self, context):
        if self.action == "CONFIRM":
            preview.my_confirm_function()
        elif self.action == "CANCEL":
            preview.my_cancel_function()
        return {"FINISHED"}


classes = (
    VOXEL_OT_hover,
    VOXEL_OT_delete_all,
    VOXEL_OT_delete,
    VOXEL_OT_action,
    VOXEL_OT_toggle_object_selection,
    VOXEL_UL_items,
    VOXEL_PT_panel,
    gizmo.VOXEL_GGT_offset_gizmo,
    gizmo.VOXEL_OT_toggle_gizmo,
)

# registration handled centrally by package `__init__.py`
