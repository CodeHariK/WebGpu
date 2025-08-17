import math
import bpy
from bpy.types import Gizmo, GizmoGroup, Operator
from mathutils import Vector, Matrix
from typing import Dict
from . import type


class VOXEL_GGT_offset_gizmo(GizmoGroup):
    bl_idname = "VOXEL_GGT_offset_gizmo"
    bl_label = "Voxel Offset Gizmo"
    bl_space_type = "VIEW_3D"
    bl_region_type = "WINDOW"
    bl_options = {"3D", "PERSISTENT"}

    @classmethod
    def poll(cls, context):
        c = type.celebi()
        return (
            c.T_voxel_gizmo_state == type.GIZMO_MOVE
            and bpy.context.active_object != None
        )

    axes = {"X": (1, 0, 0), "Y": (0, 1, 0), "Z": (0, 0, 1)}

    axes_rot = {
        "X": Matrix.Rotation(math.radians(90), 4, "Y"),  # Point along +X
        "Y": Matrix.Rotation(math.radians(-90), 4, "X"),  # Point along +Y
        "Z": Matrix.Identity(4),  # Point along +Z
    }

    planes_rot = {
        "XY": Matrix.Identity(4),
        "YZ": Matrix.Rotation(math.radians(90), 4, "Y"),
        "ZX": Matrix.Rotation(math.radians(90), 4, "X"),
    }

    _start_locations: Dict[str, Vector] = {}
    _start_active: Vector
    _dragging: bool = False
    _axes_handles: Dict[str, Gizmo] = {}
    _plane_handles: Dict[str, Gizmo] = {}
    _draw_custom_shape = False

    def setup(self, context):
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
            self._axes_handles[axis] = gz

        verts = [
            (0.0, 2, 0.0),
            (-0.25, 1, 0.0),
            (0.25, 1, 0.0),
        ]

        for plane, mat in self.planes_rot.items():
            gz = self.gizmos.new("GIZMO_GT_move_3d")
            gz.color = (1, 0, 0)
            gz.alpha = 0.4
            gz.scale_basis = 0.75
            gz.matrix_basis = mat
            gz.draw_style = "RING_2D"

            self.custom_shape_xy = gz.new_custom_shape(
                "TRIS",
                [
                    (1.0, 0.25, 0.0),
                    (1.0, -0.25, 0.0),
                    (1.5, -0.25, 0.0),
                    (1.0, 0.25, 0.0),
                    (1.5, -0.25, 0.0),
                    (1.5, 0.25, 0.0),
                ],
            )
            self.custom_shape_yz = gz.new_custom_shape(
                "TRIS",
                [
                    (-0.25, 1.0, 0.0),
                    (0.25, 1.0, 0.0),
                    (0.25, 1.5, 0.0),
                    (-0.25, 1.0, 0.0),
                    (0.25, 1.5, 0.0),
                    (-0.25, 1.5, 0.0),
                ],
            )

            self.custom_shape_zx = gz.new_custom_shape(
                "TRIS",
                [
                    (-0.25, 1.0, 0.0),
                    (0.25, 1.0, 0.0),
                    (0.25, 1.5, 0.0),
                    (-0.25, 1.0, 0.0),
                    (0.25, 1.5, 0.0),
                    (-0.25, 1.5, 0.0),
                ],
            )

            gz.target_set_handler(
                "offset",
                get=lambda a=plane: self.get_plane_offset(a),
                set=lambda val, a=plane: self.set_plane_offset(val, a),
            )
            self._plane_handles[plane] = gz

    def draw_prepare(self, context):
        highlight = False

        active = context.active_object

        all_voxels = True
        # for obj in context.selected_objects:
        #     all_voxels = all_voxels and obj.name.startswith("voxel")

        hide = not active or not active.select_get() or not all_voxels

        for plane, gz in self._plane_handles.items():
            gz.hide = hide
            highlight = highlight or gz.is_highlight

            if not active:
                return

            gz.matrix_basis = active.matrix_world @ self.planes_rot[plane]

            if plane == "XY":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (-active.location.x, -active.location.y, -active.location.z)
                )
                if self._draw_custom_shape:
                    gz.draw_custom_shape(self.custom_shape_xy)
            if plane == "YZ":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (active.location.z, -active.location.y, -active.location.x)
                )
                if self._draw_custom_shape:
                    gz.draw_custom_shape(self.custom_shape_yz)
            if plane == "ZX":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (-active.location.x, -active.location.z, active.location.y)
                )
                if self._draw_custom_shape:
                    gz.draw_custom_shape(self.custom_shape_zx)

        for plane, gz in self._axes_handles.items():
            gz.hide = hide
            highlight = highlight or gz.is_highlight

            if not active:
                return

            gz.matrix_basis = active.matrix_world @ self.axes_rot[plane]

            if plane == "X":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -active.location.x)
                )
            if plane == "Y":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -active.location.y)
                )
            if plane == "Z":
                gz.matrix_basis = gz.matrix_basis @ Matrix.Translation(
                    (0, 0, -active.location.z)
                )

        if highlight and not self._dragging:
            self._dragging = True
            self._start_locations = {
                obj.name: obj.location.copy() for obj in context.selected_objects
            }
            self._start_active = context.active_object.location.copy()

        if self._dragging and not highlight:
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

    def get_plane_offset(self, plane):
        obj = bpy.context.active_object
        if not obj:
            return (0, 0, 0)
        return obj.location

    def set_plane_offset(self, value, plane):
        for obj in bpy.context.selected_objects:
            if obj.name in self._start_locations:
                if plane == "XY":
                    x = round(value[0] - self._start_active.x)
                    y = round(value[1] - self._start_active.y)
                    obj.location.x = self._start_locations[obj.name].x + x
                    obj.location.y = self._start_locations[obj.name].y + y
                if plane == "YZ":
                    y = round(value[1] - self._start_active.y)
                    z = round(value[2] - self._start_active.z)
                    obj.location.y = self._start_locations[obj.name].y + y
                    obj.location.z = self._start_locations[obj.name].z + z
                if plane == "ZX":
                    z = round(value[2] - self._start_active.z)
                    x = round(value[0] - self._start_active.x)
                    obj.location.z = self._start_locations[obj.name].z + z
                    obj.location.x = self._start_locations[obj.name].x + x


class VOXEL_OT_toggle_gizmo(Operator):
    bl_idname = "voxel.toggle_gizmo"
    bl_label = "Toggle Voxel Gizmo"

    def execute(self, context):
        c = type.celebi()
        if c.T_voxel_gizmo_state == type.GIZMO_DISABLED:
            c.T_voxel_gizmo_state = type.GIZMO_MOVE
        else:
            c.T_voxel_gizmo_state = type.GIZMO_DISABLED

        # Force gizmos to refresh
        bpy.ops.wm.tool_set_by_id(name="builtin.move")
        bpy.context.area.tag_redraw()

        return {"FINISHED"}
