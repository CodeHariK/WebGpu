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
from .operators import fk_ik_generator
from .operators import custom_shapes
from .operators import bone_colors
from .operators import rotation_mode
from .operators import mech_rig
from .operators import fk_ik_snap
from .operators import mesh_mirror

# Reload modules when Blender triggers a reload
if "bpy" in locals():
    importlib.reload(main_panel)
    importlib.reload(inspect_scene)
    importlib.reload(auto_parent)
    importlib.reload(fk_ik_generator)
    importlib.reload(custom_shapes)
    importlib.reload(bone_colors)
    importlib.reload(rotation_mode)
    importlib.reload(mech_rig)
    importlib.reload(fk_ik_snap)
    importlib.reload(mesh_mirror)

import bpy


def update_spline_y_scale_mode(self, context: bpy.types.Context) -> None:
    """Live-updates y_scale_mode on Spline IK constraints of the active selected armature only."""
    y_mode = getattr(context.scene, "mechanim_spline_y_scale_mode", "FIT_CURVE")
    active_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else None
    if not active_obj:
        armatures = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if armatures:
            active_obj = armatures[0]
            
    if active_obj and active_obj.pose:
        for pbone in active_obj.pose.bones:
            for con in pbone.constraints:
                if con.type == "SPLINE_IK":
                    con.y_scale_mode = y_mode
                    print(f"[MechAnim] Live updated active armature '{active_obj.name}' bone '{pbone.name}' Spline IK y_scale_mode -> {y_mode}")


def update_spline_order(self, context: bpy.types.Context) -> None:
    """Live-updates order_u on Spline IK Curves of the active selected armature only."""
    order_val = int(getattr(context.scene, "mechanim_spline_order", "3"))
    curves_to_update = [obj for obj in context.scene.objects if obj.type == "CURVE" and obj.name.startswith("Curve_")]
    for curve_obj in curves_to_update:
        if curve_obj.data and curve_obj.data.splines:
            spline = curve_obj.data.splines[0]
            max_ord = len(spline.points)
            spline.order_u = min(order_val, max_ord)
            print(f"[MechAnim] Live updated Curve '{curve_obj.name}' spline order_u -> {spline.order_u}")


def register() -> None:
    """Registers all classes and operators with Blender."""
    main_panel.register()
    inspect_scene.register()
    auto_parent.register()
    fk_ik_generator.register()
    custom_shapes.register()
    bone_colors.register()
    rotation_mode.register()
    mech_rig.register()
    fk_ik_snap.register()
    mesh_mirror.register()

    bpy.types.Scene.mechanim_spline_y_scale_mode = bpy.props.EnumProperty(
        name="Spline IK Y-Scale Mode",
        description="Y-Scale Mode for Spline IK chains (FIT_CURVE for squishy/elastic, NONE for fixed mechanical length)",
        items=[
            ("FIT_CURVE", "Fit Curve (Elastic/Squishy)", "Stretches bones to fit total curve length"),
            ("NONE", "None (Fixed Mechanical Length)", "Prevents bones from stretching along Y axis"),
            ("BONE_ORIGINAL", "Bone Original", "Uses original bone rest lengths"),
        ],
        default="FIT_CURVE",
        update=update_spline_y_scale_mode,
    )

    bpy.types.Scene.mechanim_spline_order = bpy.props.EnumProperty(
        name="Spline Order",
        description="Order of the NURBS spline (2 = Linear, 3 = Quadratic, 4 = Cubic, 5 = Quartic, 6 = Quintic)",
        items=[
            ("2", "Order 2 (Linear)", "Linear 1st degree spline"),
            ("3", "Order 3 (Quadratic)", "Quadratic 2nd degree spline"),
            ("4", "Order 4 (Cubic)", "Cubic 3rd degree spline"),
            ("5", "Order 5 (Quartic)", "Quartic 4th degree spline"),
            ("6", "Order 6 (Quintic)", "Quintic 5th degree spline"),
        ],
        default="3",
        update=update_spline_order,
    )


def unregister() -> None:
    """Unregisters all classes and operators from Blender."""
    if hasattr(bpy.types.Scene, "mechanim_spline_order"):
        del bpy.types.Scene.mechanim_spline_order
    if hasattr(bpy.types.Scene, "mechanim_spline_y_scale_mode"):
        del bpy.types.Scene.mechanim_spline_y_scale_mode

    mesh_mirror.unregister()
    fk_ik_snap.unregister()
    mech_rig.unregister()
    rotation_mode.unregister()
    bone_colors.unregister()
    custom_shapes.unregister()
    fk_ik_generator.unregister()
    auto_parent.unregister()
    inspect_scene.unregister()
    main_panel.unregister()


if __name__ == "__main__":
    register()
