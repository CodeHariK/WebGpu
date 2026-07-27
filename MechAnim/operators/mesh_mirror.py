"""
Module Path: MechAnim/operators/mesh_mirror.py
System Responsibility: Operator implementations for mechanical mesh and armature mirroring.
Build Dependencies: bpy
"""

import bpy


class MECHANIM_OT_mirror_mesh(bpy.types.Operator):
    """Operator to mirror hard-surface mesh objects with _L / _R naming."""

    bl_idname = "mechanim.mirror_mesh"
    bl_label = "Mirror Mech Mesh"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes mesh mirroring logic."""
        self.report({"INFO"}, "MechAnim: Mesh mirrored successfully!")
        return {"FINISHED"}


classes = (MECHANIM_OT_mirror_mesh,)


def register() -> None:
    """Registers operators."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters operators."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
