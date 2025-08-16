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
from bpy.types import Operator, Panel, UIList, PropertyGroup
from mathutils import Vector
from bpy_extras import view3d_utils

from . import type
from . import preview
from . import gizmo
from . import box

class VOXEL_OT_delete(bpy.types.Operator):
    bl_idname = "celebi.voxel_delete"
    bl_label = "Delete Voxel"
    bl_description = "Delete the selected voxel"

    voxel_name: StringProperty()

    def execute(self, context):
        c = type.celebi(context)
        voxel_obj = bpy.data.objects.get(self.voxel_name)
        if voxel_obj:
            bpy.data.objects.remove(voxel_obj, do_unlink=True)
        # Remove from voxel collection
        items = c.voxels
        for i, item in enumerate(items):
            if item.name == self.voxel_name:
                items.remove(i)
                break
        return {"FINISHED"}


class VOXEL_OT_cleanup(bpy.types.Operator):
    bl_idname = "celebi.voxel_cleanup"
    bl_label = "Cleanup Voxel"

    def execute(self, context):
        c = type.celebi(context)
        voxels_to_keep = [
            item.name for item in c.voxels if bpy.data.objects.get(item.name)
        ]

        c.voxels.clear()
        for name in voxels_to_keep:
            new_item = c.voxels.add()
            new_item.name = name

        c.T_voxel_hover_running = False

        return {"FINISHED"}


class VOXEL_OT_delete_all(bpy.types.Operator):
    bl_idname = "celebi.voxel_delete_all"
    bl_label = "Delete all voxels"

    def execute(self, context):
        c = type.celebi(context)

        items = c.voxels
        for i, item in enumerate(items):
            voxel_obj = bpy.data.objects.get(item.name)
            if voxel_obj:
                bpy.data.objects.remove(voxel_obj, do_unlink=True)

        c.voxels.clear()

        return {"FINISHED"}


class VOXEL_OT_hover(bpy.types.Operator):
    bl_idname = "celebi.voxel_hover"
    bl_label = "Voxel Hover"
    bl_description = "Spawn voxel by hovering over surfaces"
    bl_options = {"REGISTER", "UNDO"}

    _preview_voxel = None

    def modal(self, context, event):
        c = type.celebi(context)

        if event.type in {"RIGHTMOUSE", "ESC"}:
            if self._preview_voxel:
                bpy.data.objects.remove(self._preview_voxel, do_unlink=True)
                self._preview_voxel = None

            c.T_voxel_hover_running = False

            return {"CANCELLED"}

        coord = (event.mouse_region_x, event.mouse_region_y)

        preview_hidden = False
        if self._preview_voxel and not self._preview_voxel.hide_viewport:
            self._preview_voxel.hide_viewport = True
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
            self._preview_voxel.hide_viewport = False

        offset_location = ray_location + ray_normal * 0.1
        snapped_location = Vector(
            (
                round(offset_location.x - 0.5) + 0.5,
                round(offset_location.y + 0.5) - 0.5,
                round(offset_location.z - 0.5) + 0.5,
            )
        )

        if event.type == "MOUSEMOVE":
            pass
            if ray_result:
                if self._preview_voxel is None:
                    bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                    self._preview_voxel = context.active_object
                    self._preview_voxel.name = "Voxel_Preview_voxel"
                    self._preview_voxel.hide_select = True
                    self._preview_voxel.display_type = "WIRE"
                    self._preview_voxel.show_in_front = True
                else:
                    self._preview_voxel.hide_viewport = False
                    self._preview_voxel.location = snapped_location
            else:
                if self._preview_voxel:
                    self._preview_voxel.hide_viewport = True
        elif event.type == "LEFTMOUSE" and event.value == "PRESS":
            if ray_result:
                bpy.ops.celebi.voxel_cleanup()

                # Check if a voxel already exists at this location
                found = False
                for item in c.voxels:
                    voxel = bpy.data.objects.get(item.name)
                    if voxel and (voxel.location - snapped_location).length < 0.001:
                        found = True
                        break
                if not found:
                    lib_obj = c.library_items[c.T_library_index].obj
                    if lib_obj and lib_obj.name in bpy.data.objects:
                        src_obj = bpy.data.objects[lib_obj.name]
                        new_voxel = src_obj.copy()
                        new_voxel.data = src_obj.data  # keep linked mesh
                        new_voxel.location = snapped_location
                        new_voxel.name = type.voxel_name(src_obj.name, snapped_location)
                        context.collection.objects.link(new_voxel)

                        # Add to collection property
                        item = c.voxels.add()
                        item.name = new_voxel.name
                    else:
                        self.report({"WARNING"}, "Library object not found")

                        # bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                        # new_voxel = context.active_object
                        # new_voxel.name = "voxel"
                        # item = c.voxels.add()
                        # item.name = new_voxel.name

        return {"PASS_THROUGH"}

    def invoke(self, context, event):
        c = type.celebi(context)
        if c.T_voxel_hover_running:
            self.report({"WARNING"}, "Voxel Hover is already running")
            return {"CANCELLED"}
        c.T_voxel_hover_running = True
        context.window_manager.modal_handler_add(self)

        return {"RUNNING_MODAL"}


class VOXEL_OT_toggle_object_selection(bpy.types.Operator):
    bl_idname = "celebi.voxel_toggle_object_selection"
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
        c = type.celebi(context)
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


def sync_selection_to_voxels(scene):
    c = type.celebi(bpy.context)
    selected_objs = set(obj.name for obj in bpy.context.selected_objects)
    for item in c.voxels:
        item.selected = item.name in selected_objs


class VOXEL_PT_panel(Panel):
    bl_label = "Voxel Panel"
    bl_idname = "VOXEL_PT_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        l = self.layout
        c = type.celebi(context)

        # Sync selection state from objects to voxel items
        sync_selection_to_voxels(context.scene)

        l.operator(VOXEL_OT_hover.bl_idname)
        l.operator(VOXEL_OT_cleanup.bl_idname, text="Cleanup voxels")
        l.operator(VOXEL_OT_delete_all.bl_idname, text="Delete all voxels")

        l.operator("voxel.toggle_gizmo", text="Enable/Disable Gizmo")

        active = bpy.context.active_object
        if active and c.T_library_index != -1:
        # if active and active.name.startswith("voxel"):
            l.label(text=c.library_items[c.T_library_index].obj.name)

            l.label(text=active.name)
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


class VOXEL_OT_action(bpy.types.Operator):
    bl_idname = "celebi.voxel_ot_action"
    bl_label = "Voxel Action"

    action: bpy.props.StringProperty()

    def execute(self, context):
        if self.action == "CONFIRM":
            preview.my_confirm_function(context)
        elif self.action == "CANCEL":
            preview.my_cancel_function(context)
        return {"FINISHED"}


def register():
    bpy.utils.register_class(VOXEL_OT_hover)

    bpy.utils.register_class(VOXEL_OT_cleanup)
    bpy.utils.register_class(VOXEL_OT_delete_all)
    bpy.utils.register_class(VOXEL_OT_delete)

    bpy.utils.register_class(VOXEL_OT_action)
    bpy.utils.register_class(VOXEL_OT_toggle_object_selection)
    bpy.utils.register_class(VOXEL_UL_items)
    bpy.utils.register_class(VOXEL_PT_panel)

    bpy.utils.register_class(gizmo.VOXEL_GGT_offset_gizmo)
    bpy.utils.register_class(gizmo.VOXEL_OT_toggle_gizmo)
    bpy.types.Scene.voxel_gizmo_enabled = bpy.props.BoolProperty(default=True)

    bpy.utils.register_class(box.BOX_OT)


def unregister():
    bpy.utils.unregister_class(VOXEL_OT_hover)

    bpy.utils.unregister_class(VOXEL_OT_cleanup)
    bpy.utils.unregister_class(VOXEL_OT_delete_all)
    bpy.utils.unregister_class(VOXEL_OT_delete)

    bpy.utils.unregister_class(VOXEL_OT_action)
    bpy.utils.unregister_class(VOXEL_OT_toggle_object_selection)
    bpy.utils.unregister_class(VOXEL_UL_items)
    bpy.utils.unregister_class(VOXEL_PT_panel)

    bpy.utils.unregister_class(gizmo.VOXEL_GGT_offset_gizmo)
    bpy.utils.unregister_class(gizmo.VOXEL_OT_toggle_gizmo)
    del bpy.types.Scene.voxel_gizmo_enabled

    bpy.utils.unregister_class(box.BOX_OT)
