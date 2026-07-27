"""
Module Path: MechAnim/__init__.py
System Responsibility: Entry point for MechAnim Blender Addon. Handles addon registration, unregistration, and live reloading of submodules.
Build Dependencies: bpy, importlib
"""

bl_info = {
    "name": "MechAnim - Mech & Robot Rigging",
    "author": "Antigravity Pair",
    "version": (0, 1, 0),
    "blender": (4, 0, 0),
    "location": "3D Viewport > Sidebar (N-Panel) > MechAnim",
    "description": "Blender addon for mech/robot rigging, FK-IK snapping, and hard-surface mesh mirroring.",
    "category": "Rigging",
}

import sys
import importlib

# Always import submodules into top-level namespace
from .ui import main_panel
from .operators import inspect_scene
from .operators import auto_parent
from .operators import rotation_mode
from .operators import mech_rig
from .operators import fk_ik_snap
from .operators import mesh_mirror

# Reload modules when Blender triggers a reload
if "bpy" in locals():
    importlib.reload(main_panel)
    importlib.reload(inspect_scene)
    importlib.reload(auto_parent)
    importlib.reload(rotation_mode)
    importlib.reload(mech_rig)
    importlib.reload(fk_ik_snap)
    importlib.reload(mesh_mirror)

import bpy


def register() -> None:
    """Registers all classes and operators with Blender."""
    main_panel.register()
    inspect_scene.register()
    auto_parent.register()
    rotation_mode.register()
    mech_rig.register()
    fk_ik_snap.register()
    mesh_mirror.register()


def unregister() -> None:
    """Unregisters all classes and operators from Blender."""
    mesh_mirror.unregister()
    fk_ik_snap.unregister()
    mech_rig.unregister()
    rotation_mode.unregister()
    auto_parent.unregister()
    inspect_scene.unregister()
    main_panel.unregister()


if __name__ == "__main__":
    register()
