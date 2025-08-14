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


class OBJECT_OT_delete_voxel(bpy.types.Operator):
    bl_idname = "celebi.delete_voxel"
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


class OBJECT_OT_rename_voxel(bpy.types.Operator):
    bl_idname = "celebi.rename_voxel"
    bl_label = "Rename Voxel"
    bl_description = "Rename the selected voxel"
    bl_options = {"REGISTER", "INTERNAL"}

    voxel_name: StringProperty()
    new_name: StringProperty(name="New Name")

    def invoke(self, context, event):
        self.new_name = self.voxel_name
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        wm = context.window_manager
        voxel_obj = bpy.data.objects.get(self.voxel_name)
        if voxel_obj and self.new_name:
            # Rename the object
            voxel_obj.name = self.new_name
            # Update the collection entry
            items = type.celebi(context).voxels
            for item in items:
                if item.name == self.voxel_name:
                    item.name = self.new_name
                    break
            return {"FINISHED"}
        else:
            self.report({"WARNING"}, "voxel not found or new name invalid")
            return {"CANCELLED"}


class OBJECT_OT_voxel_cleanup(bpy.types.Operator):
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

        return {"FINISHED"}


class OBJECT_OT_voxel_delete_all(bpy.types.Operator):
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


class OBJECT_OT_voxel_hover(bpy.types.Operator):
    bl_idname = "celebi.voxel_hover"
    bl_label = "Voxel Hover"
    bl_description = "Spawn voxel by hovering over surfaces"
    bl_options = {"REGISTER", "UNDO"}

    _preview_voxel = None

    def modal(self, context, event):
        if event.type in {"RIGHTMOUSE", "ESC"}:
            if self._preview_voxel:
                bpy.data.objects.remove(self._preview_voxel, do_unlink=True)
                self._preview_voxel = None
            return {"CANCELLED"}

        coord = (event.mouse_region_x, event.mouse_region_y)

        # Temporarily hide the preview voxel to exclude it from raycast
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
            if ray_result:
                if self._preview_voxel is None:
                    bpy.ops.mesh.primitive_voxel_add(size=1, location=snapped_location)
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
                c = type.celebi(context)
                for item in c.voxels:
                    voxel = bpy.data.objects.get(item.name)
                    if voxel and (voxel.location - snapped_location).length < 0.001:
                        found = True
                        break
                if not found:
                    bpy.ops.mesh.primitive_voxel_add(size=1, location=snapped_location)
                    new_voxel = context.active_object
                    item = c.voxels.add()
                    item.name = new_voxel.name

        return {"PASS_THROUGH"}

    def invoke(self, context, event):
        context.window_manager.modal_handler_add(self)
        return {"RUNNING_MODAL"}


class VIEW3D_PT_voxel_panel(Panel):
    bl_label = "Voxel Panel"
    bl_idname = "VIEW3D_PT_voxel_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "Celebi"

    def draw(self, context):
        layout = self.layout
        c = type.celebi(context)

        layout.operator(OBJECT_OT_voxel_hover.bl_idname)
        layout.operator(OBJECT_OT_voxel_cleanup.bl_idname, text="Cleanup voxels")
        layout.operator(OBJECT_OT_voxel_delete_all.bl_idname, text="Delete all voxels")

        layout.label(text="Voxels:")
        for item in c.voxels:
            obj = bpy.data.objects.get(item.name)
            if obj:
                row = layout.row(align=True)
                row.label(text=obj.name)
                op_del = row.operator(OBJECT_OT_delete_voxel.bl_idname, text="Delete")
                op_del.voxel_name = obj.name
                op_rename = row.operator(
                    OBJECT_OT_rename_voxel.bl_idname, text="Rename"
                )
                op_rename.voxel_name = obj.name


def register():
    bpy.utils.register_class(OBJECT_OT_voxel_hover)
    bpy.utils.register_class(OBJECT_OT_voxel_cleanup)
    bpy.utils.register_class(OBJECT_OT_voxel_delete_all)
    bpy.utils.register_class(OBJECT_OT_delete_voxel)
    bpy.utils.register_class(OBJECT_OT_rename_voxel)
    bpy.utils.register_class(VIEW3D_PT_voxel_panel)


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_voxel_hover)
    bpy.utils.unregister_class(OBJECT_OT_voxel_cleanup)
    bpy.utils.unregister_class(OBJECT_OT_voxel_delete_all)
    bpy.utils.unregister_class(OBJECT_OT_delete_voxel)
    bpy.utils.unregister_class(OBJECT_OT_rename_voxel)
    bpy.utils.unregister_class(VIEW3D_PT_voxel_panel)
