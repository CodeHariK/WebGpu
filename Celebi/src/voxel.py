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


class OBJECT_OT_delete_spawned_cube(bpy.types.Operator):
    bl_idname = "celebi.delete_spawned_cube"
    bl_label = "Delete Spawned Cube"
    bl_description = "Delete the selected spawned cube"

    cube_name: StringProperty()

    def execute(self, context):
        wm = context.window_manager
        cube_obj = bpy.data.objects.get(self.cube_name)
        if cube_obj:
            bpy.data.objects.remove(cube_obj, do_unlink=True)
        # Remove from spawned_cubes collection
        items = wm.celebi_data.spawned_cubes
        for i, item in enumerate(items):
            if item.name == self.cube_name:
                items.remove(i)
                break
        return {"FINISHED"}


class OBJECT_OT_rename_spawned_cube(bpy.types.Operator):
    bl_idname = "celebi.rename_spawned_cube"
    bl_label = "Rename Spawned Cube"
    bl_description = "Rename the selected spawned cube"
    bl_options = {"REGISTER", "INTERNAL"}

    cube_name: StringProperty()
    new_name: StringProperty(name="New Name")

    def invoke(self, context, event):
        self.new_name = self.cube_name
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        wm = context.window_manager
        cube_obj = bpy.data.objects.get(self.cube_name)
        if cube_obj and self.new_name:
            # Rename the object
            cube_obj.name = self.new_name
            # Update the collection entry
            items = wm.celebi_data.spawned_cubes
            for item in items:
                if item.name == self.cube_name:
                    item.name = self.new_name
                    break
            return {"FINISHED"}
        else:
            self.report({"WARNING"}, "Cube not found or new name invalid")
            return {"CANCELLED"}


class OBJECT_OT_voxel_cleanup(bpy.types.Operator):
    bl_idname = "celebi.voxel_cleanup"
    bl_label = "Cleanup Spawned Cubes"

    def execute(self, context):
        wm = context.window_manager
        cubes_to_keep = [
            item.name
            for item in wm.celebi_data.spawned_cubes
            if bpy.data.objects.get(item.name)
        ]

        wm.celebi_data.spawned_cubes.clear()
        for name in cubes_to_keep:
            new_item = wm.celebi_data.spawned_cubes.add()
            new_item.name = name

        return {"FINISHED"}


class OBJECT_OT_voxel_hover(bpy.types.Operator):
    bl_idname = "celebi.voxel_hover"
    bl_label = "Voxel Hover"
    bl_description = "Spawn cubes by hovering over surfaces"
    bl_options = {"REGISTER", "UNDO"}

    _preview_cube = None

    def modal(self, context, event):
        if event.type in {"RIGHTMOUSE", "ESC"}:
            if self._preview_cube:
                bpy.data.objects.remove(self._preview_cube, do_unlink=True)
                self._preview_cube = None
            return {"CANCELLED"}

        coord = (event.mouse_region_x, event.mouse_region_y)

        # Temporarily hide the preview cube to exclude it from raycast
        preview_hidden = False
        if self._preview_cube and not self._preview_cube.hide_viewport:
            self._preview_cube.hide_viewport = True
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

        # Restore preview cube visibility
        if preview_hidden:
            self._preview_cube.hide_viewport = False

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
                if self._preview_cube is None:
                    bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                    self._preview_cube = context.active_object
                    self._preview_cube.name = "Voxel_Preview_Cube"
                    self._preview_cube.hide_select = True
                    self._preview_cube.display_type = "WIRE"
                    self._preview_cube.show_in_front = True
                else:
                    self._preview_cube.hide_viewport = False
                    self._preview_cube.location = snapped_location
            else:
                if self._preview_cube:
                    self._preview_cube.hide_viewport = True
        elif event.type == "LEFTMOUSE" and event.value == "PRESS":
            if ray_result:
                bpy.ops.celebi.voxel_cleanup()

                # Check if a cube already exists at this location
                found = False
                for item in context.window_manager.celebi_data.spawned_cubes:
                    cube = bpy.data.objects.get(item.name)
                    if cube and (cube.location - snapped_location).length < 0.001:
                        found = True
                        break
                if not found:
                    bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                    new_cube = context.active_object
                    item = context.window_manager.celebi_data.spawned_cubes.add()
                    item.name = new_cube.name

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

        layout.operator(OBJECT_OT_voxel_hover.bl_idname)

        layout.operator(OBJECT_OT_voxel_cleanup.bl_idname, text="Cleanup Cubes")

        layout.label(text="Spawned Cubes:")
        for item in context.window_manager.celebi_data.spawned_cubes:
            obj = bpy.data.objects.get(item.name)
            if obj:
                row = layout.row(align=True)
                row.label(text=obj.name)
                op_del = row.operator(
                    OBJECT_OT_delete_spawned_cube.bl_idname, text="Delete"
                )
                op_del.cube_name = obj.name
                op_rename = row.operator(
                    OBJECT_OT_rename_spawned_cube.bl_idname, text="Rename"
                )
                op_rename.cube_name = obj.name


def register():
    bpy.utils.register_class(OBJECT_OT_voxel_hover)
    bpy.utils.register_class(OBJECT_OT_voxel_cleanup)
    bpy.utils.register_class(OBJECT_OT_delete_spawned_cube)
    bpy.utils.register_class(OBJECT_OT_rename_spawned_cube)
    bpy.utils.register_class(VIEW3D_PT_voxel_panel)

    bpy.types.Scene.library_index = bpy.props.IntProperty()


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_voxel_hover)
    bpy.utils.unregister_class(OBJECT_OT_voxel_cleanup)
    bpy.utils.unregister_class(OBJECT_OT_delete_spawned_cube)
    bpy.utils.unregister_class(OBJECT_OT_rename_spawned_cube)
    bpy.utils.unregister_class(VIEW3D_PT_voxel_panel)

    del bpy.types.Scene.library_index
