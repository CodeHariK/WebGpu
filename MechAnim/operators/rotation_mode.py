"""
Module Path: MechAnim/operators/rotation_mode.py
System Responsibility: Operator to change rotation mode (Euler XYZ, ZXY, Quaternion, etc.) for all selected pose bones.
Build Dependencies: bpy
"""

import bpy


class MECHANIM_OT_set_rotation_mode(bpy.types.Operator):
    """Batch change rotation mode for all selected bones in Pose Mode."""

    bl_idname = "mechanim.set_rotation_mode"
    bl_label = "Set Rotation Mode"
    bl_options = {"REGISTER", "UNDO"}

    target_mode: bpy.props.EnumProperty(
        name="Rotation Mode",
        description="Target rotation mode for selected bones",
        items=[
            ("QUATERNION", "Quaternion (WXYZ)", "4-component rotation (Default)"),
            ("XYZ", "Euler XYZ", "Euler XYZ order"),
            ("XZY", "Euler XZY", "Euler XZY order"),
            ("YXZ", "Euler YXZ", "Euler YXZ order"),
            ("YZX", "Euler YZX", "Euler YZX order"),
            ("ZXY", "Euler ZXY", "Euler ZXY order"),
            ("ZYX", "Euler ZYX", "Euler ZYX order"),
            ("AXIS_ANGLE", "Axis Angle", "Axis Angle rotation"),
        ],
        default="XYZ",
    )

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Sets rotation mode on all currently selected pose bones."""
        active_obj = context.active_object
        if not active_obj or active_obj.type != "ARMATURE":
            self.report({"WARNING"}, "Please select an Armature in Pose Mode.")
            return {"CANCELLED"}

        selected_bones = context.selected_pose_bones
        if not selected_bones:
            self.report({"WARNING"}, "No bones selected in Pose Mode.")
            return {"CANCELLED"}

        count = 0
        for bone in selected_bones:
            # Assign selected rotation mode to bone
            bone.rotation_mode = self.target_mode
            count += 1

        self.report({"INFO"}, f"Updated rotation mode to '{self.target_mode}' for {count} bone(s).")
        return {"FINISHED"}


classes = (MECHANIM_OT_set_rotation_mode,)


def register() -> None:
    """Registers rotation mode operator."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters rotation mode operator."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
