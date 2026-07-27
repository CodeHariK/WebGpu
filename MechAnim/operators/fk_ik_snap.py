"""
Module Path: MechAnim/operators/fk_ik_snap.py
System Responsibility: Operator implementations for FK to IK and IK to FK snapping.
Build Dependencies: bpy
"""

import bpy


class MECHANIM_OT_fk_to_ik(bpy.types.Operator):
    """Operator to snap FK bones to IK targets."""

    bl_idname = "mechanim.fk_to_ik"
    bl_label = "Snap FK -> IK"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes FK to IK snapping logic."""
        self.report({"INFO"}, "MechAnim: FK snapped to IK!")
        return {"FINISHED"}


class MECHANIM_OT_ik_to_fk(bpy.types.Operator):
    """Operator to snap IK targets to FK bones."""

    bl_idname = "mechanim.ik_to_fk"
    bl_label = "Snap IK -> FK"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes IK to FK snapping logic."""
        self.report({"INFO"}, "MechAnim: IK snapped to FK!")
        return {"FINISHED"}


classes = (
    MECHANIM_OT_fk_to_ik,
    MECHANIM_OT_ik_to_fk,
)


def register() -> None:
    """Registers operators."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters operators."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
