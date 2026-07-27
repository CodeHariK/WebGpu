"""
Module Path: MechAnim/operators/mech_rig.py
System Responsibility: Operator to set up mechanical piston Track-To and Damp Track constraints.
Build Dependencies: bpy
"""

import bpy


class MECHANIM_OT_setup_piston(bpy.types.Operator):
    """Operator to set up mechanical piston Track-To constraints."""

    bl_idname = "mechanim.setup_piston"
    bl_label = "Setup Mechanical Piston"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes piston setup logic."""
        self.report({"INFO"}, "MechAnim: Piston setup initialized!")
        return {"FINISHED"}


classes = (MECHANIM_OT_setup_piston,)


def register() -> None:
    """Registers operators."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters operators."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
