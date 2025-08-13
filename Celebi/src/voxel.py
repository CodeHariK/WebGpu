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

spawned_cubes = []


# Clean up deleted cubes
def check_cubes():
    cubes_to_keep = []
    for cube in spawned_cubes:
        try:
            if cube and cube.name in bpy.data.objects:
                cubes_to_keep.append(cube)
        except ReferenceError:
            continue
    spawned_cubes[:] = cubes_to_keep


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
        ray_result, ray_location, ray_normal, ray_index, ray_obj, ray_matrix = context.scene.ray_cast(
            context.view_layer.depsgraph, ray_origin, ray_direction
        )

        # Restore preview cube visibility
        if preview_hidden:
            self._preview_cube.hide_viewport = False

        offset_location = ray_location + ray_normal * 0.1
        snapped_location = Vector(
            (
                round(offset_location.x),
                round(offset_location.y),
                round(offset_location.z),
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
                check_cubes()

                # Check if a cube already exists at this location
                for cube in spawned_cubes:
                    if (cube.location - snapped_location).length < 0.001:
                        break
                else:
                    bpy.ops.mesh.primitive_cube_add(size=1, location=snapped_location)
                    new_cube = context.active_object
                    spawned_cubes.append(new_cube)

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

        check_cubes()

        layout.label(text="Spawned Cubes:")
        for cube in spawned_cubes:
            layout.label(text=cube.name)


def register():
    bpy.utils.register_class(OBJECT_OT_voxel_hover)
    bpy.utils.register_class(VIEW3D_PT_voxel_panel)

    bpy.types.Scene.library_index = bpy.props.IntProperty()
    bpy.types.Scene.library_tags = []
    bpy.types.Scene.new_tag_name = StringProperty()


def unregister():
    bpy.utils.unregister_class(OBJECT_OT_voxel_hover)
    bpy.utils.unregister_class(VIEW3D_PT_voxel_panel)

    del bpy.types.Scene.object_library
    del bpy.types.Scene.library_index
    del bpy.types.Scene.library_tags
    del bpy.types.Scene.new_tag_name
