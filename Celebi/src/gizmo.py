import math
import bpy
from mathutils import Vector, Matrix
from typing import Dict


class VOXEL_GGT_offset_gizmo(bpy.types.GizmoGroup):
    bl_idname = "VOXEL_GGT_offset_gizmo"
    bl_label = "Voxel Offset Gizmo"
    bl_space_type = "VIEW_3D"
    bl_region_type = "WINDOW"
    bl_options = {"3D", "PERSISTENT"}

    @classmethod
    def poll(cls, context):
        return getattr(context.scene, "voxel_gizmo_enabled", False)

    axes = {"X": (1, 0, 0), "Y": (0, 1, 0), "Z": (0, 0, 1)}

    axes_rot = {
        "X": Matrix.Rotation(math.radians(90), 4, "Y"),  # Point along +X
        "Y": Matrix.Rotation(math.radians(-90), 4, "X"),  # Point along +Y
        "Z": Matrix.Identity(4),  # Point along +Z
    }

    _start_locations: Dict[str, Vector] = {}
    _start_active: Vector

    def setup(self, context):
        self._handles = {}

        self._dragging = False

        for axis, color in self.axes.items():
            gz = self.gizmos.new("GIZMO_GT_arrow_3d")
            gz.color = color
            gz.alpha = 0.5
            gz.scale_basis = 1
            gz.draw_style = "NORMAL"
            gz.matrix_basis = self.axes_rot[axis]
            gz.target_set_handler(
                "offset",
                get=lambda a=axis: self.get_offset(a),
                set=lambda val, a=axis: self.set_offset(val, a),
            )
            self._handles[axis] = gz

    def draw_prepare(self, context):
        obj = context.active_object
        if not obj:
            return

        highlightx = True
        highlighty = True
        highlightz = True

        for axis, gz in self._handles.items():
            gz.matrix_basis = obj.matrix_world @ self.axes_rot[axis]

            if axis == "X":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -obj.location.x)
                )
                highlightx = gz.is_highlight
            if axis == "Y":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -obj.location.y)
                )
                highlighty = gz.is_highlight
            if axis == "Z":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -obj.location.z)
                )
                highlightz = gz.is_highlight

        hightlight = highlightx or highlighty or highlightz

        if hightlight and not self._dragging:
            self._dragging = True
            self._start_locations = {
                obj.name: obj.location.copy() for obj in context.selected_objects
            }
            self._start_active = context.active_object.location.copy()

        if self._dragging and not hightlight:
            self._dragging = False

    def get_offset(self, axis):
        obj = bpy.context.active_object
        if not obj:
            return 0
        return getattr(obj.location, axis.lower())

    def set_offset(self, value, axis):
        for obj in bpy.context.selected_objects:
            if obj.name in self._start_locations:
                if axis == "X":
                    x = round(value - self._start_active.x)
                    obj.location.x = self._start_locations[obj.name].x + x
                if axis == "Y":
                    y = round(value - self._start_active.y)
                    obj.location.y = self._start_locations[obj.name].y + y
                if axis == "Z":
                    z = round(value - self._start_active.z)
                    obj.location.z = self._start_locations[obj.name].z + z


class VOXEL_OT_toggle_gizmo(bpy.types.Operator):
    bl_idname = "voxel.toggle_gizmo"
    bl_label = "Toggle Voxel Gizmo"

    def execute(self, context):
        g_enabled = getattr(context.scene, "voxel_gizmo_enabled", False)
        context.scene.voxel_gizmo_enabled = not g_enabled

        # Force gizmos to refresh
        bpy.ops.wm.tool_set_by_id(name="builtin.move")
        bpy.context.area.tag_redraw()

        return {"FINISHED"}
