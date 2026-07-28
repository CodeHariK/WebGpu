"""
Module Path: MechAnim/operators/bone_colors.py
System Responsibility: Operator to assign customizable viewport bone colors (Normal, Select, Active) to bones based on chain role (DEF/DEFIK, CTRL, POLE, FK, IK).
Build Dependencies: bpy
"""

import bpy
from .inspect_scene import classify_bone_type


def lighten_color(color: tuple[float, float, float], factor: float = 0.3) -> tuple[float, float, float]:
    """Helper to derive a lighter selection/active color variant."""
    return tuple(min(1.0, c + (1.0 - c) * factor) for c in color[:3])


class MECHANIM_OT_apply_bone_colors(bpy.types.Operator):
    """Assign custom viewport bone colors (Normal, Selected, Active) to bones according to their chain role (DEF, CTRL, POLE, FK, IK)."""

    bl_idname = "mechanim.apply_bone_colors"
    bl_label = "Apply Bone Colors"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Assigns normal, select, and active custom bone colors for 5 bone types."""
        armature_objs = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if not armature_objs:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        arm_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else armature_objs[0]
        scene = context.scene

        # Map each bone type to (normal, select, active) colors
        color_map = {
            "CTRL": (scene.mechanim_color_ctrl_normal, scene.mechanim_color_ctrl_select, scene.mechanim_color_ctrl_active),
            "POLE": (scene.mechanim_color_pole_normal, scene.mechanim_color_pole_select, scene.mechanim_color_pole_active),
            "DEF": (scene.mechanim_color_def_normal, scene.mechanim_color_def_select, scene.mechanim_color_def_active),
            "DEFIK": (scene.mechanim_color_def_normal, scene.mechanim_color_def_select, scene.mechanim_color_def_active),
            "FK": (scene.mechanim_color_fk_normal, scene.mechanim_color_fk_select, scene.mechanim_color_fk_active),
            "IK": (scene.mechanim_color_ik_normal, scene.mechanim_color_ik_select, scene.mechanim_color_ik_active),
        }

        prev_mode = context.mode
        bpy.ops.object.mode_set(mode="POSE")
        pose_bones = arm_obj.pose.bones

        colored_count = 0
        for pbone in pose_bones:
            b_type, base_name = classify_bone_type(pbone.name)
            colors = color_map.get(b_type)

            if colors and hasattr(pbone, "color"):
                c_normal, c_select, c_active = colors
                pbone.color.palette = "CUSTOM"
                pbone.color.custom.normal = c_normal[:3]
                pbone.color.custom.select = c_select[:3]
                pbone.color.custom.active = c_active[:3]
                colored_count += 1

        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report({"INFO"}, f"Applied custom (Normal/Select/Active) colors to {colored_count} bone(s) on '{arm_obj.name}'.")
        return {"FINISHED"}


classes = (MECHANIM_OT_apply_bone_colors,)


def register() -> None:
    """Registers bone colors operator and custom scene color properties for Normal, Select, Active."""
    for cls in classes:
        bpy.utils.register_class(cls)

    # 1. CTRL Colors (Normal, Select, Active)
    bpy.types.Scene.mechanim_color_ctrl_normal = bpy.props.FloatVectorProperty(
        name="CTRL Normal", subtype="COLOR", default=(1.0, 0.85, 0.0), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_ctrl_select = bpy.props.FloatVectorProperty(
        name="CTRL Select", subtype="COLOR", default=(1.0, 0.95, 0.4), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_ctrl_active = bpy.props.FloatVectorProperty(
        name="CTRL Active", subtype="COLOR", default=(1.0, 1.0, 0.8), min=0.0, max=1.0
    )

    # 2. POLE Colors
    bpy.types.Scene.mechanim_color_pole_normal = bpy.props.FloatVectorProperty(
        name="POLE Normal", subtype="COLOR", default=(0.0, 0.8, 1.0), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_pole_select = bpy.props.FloatVectorProperty(
        name="POLE Select", subtype="COLOR", default=(0.4, 0.9, 1.0), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_pole_active = bpy.props.FloatVectorProperty(
        name="POLE Active", subtype="COLOR", default=(0.8, 0.95, 1.0), min=0.0, max=1.0
    )

    # 3. DEF Colors
    bpy.types.Scene.mechanim_color_def_normal = bpy.props.FloatVectorProperty(
        name="DEF Normal", subtype="COLOR", default=(0.4, 0.4, 0.4), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_def_select = bpy.props.FloatVectorProperty(
        name="DEF Select", subtype="COLOR", default=(0.65, 0.65, 0.65), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_def_active = bpy.props.FloatVectorProperty(
        name="DEF Active", subtype="COLOR", default=(0.85, 0.85, 0.85), min=0.0, max=1.0
    )

    # 4. FK Colors
    bpy.types.Scene.mechanim_color_fk_normal = bpy.props.FloatVectorProperty(
        name="FK Normal", subtype="COLOR", default=(0.1, 0.9, 0.2), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_fk_select = bpy.props.FloatVectorProperty(
        name="FK Select", subtype="COLOR", default=(0.4, 1.0, 0.5), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_fk_active = bpy.props.FloatVectorProperty(
        name="FK Active", subtype="COLOR", default=(0.7, 1.0, 0.8), min=0.0, max=1.0
    )

    # 5. IK Colors
    bpy.types.Scene.mechanim_color_ik_normal = bpy.props.FloatVectorProperty(
        name="IK Normal", subtype="COLOR", default=(1.0, 0.1, 0.2), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_ik_select = bpy.props.FloatVectorProperty(
        name="IK Select", subtype="COLOR", default=(1.0, 0.4, 0.5), min=0.0, max=1.0
    )
    bpy.types.Scene.mechanim_color_ik_active = bpy.props.FloatVectorProperty(
        name="IK Active", subtype="COLOR", default=(1.0, 0.7, 0.8), min=0.0, max=1.0
    )


def unregister() -> None:
    """Unregisters bone colors operator and scene color properties."""
    del bpy.types.Scene.mechanim_color_ctrl_normal
    del bpy.types.Scene.mechanim_color_ctrl_select
    del bpy.types.Scene.mechanim_color_ctrl_active

    del bpy.types.Scene.mechanim_color_pole_normal
    del bpy.types.Scene.mechanim_color_pole_select
    del bpy.types.Scene.mechanim_color_pole_active

    del bpy.types.Scene.mechanim_color_def_normal
    del bpy.types.Scene.mechanim_color_def_select
    del bpy.types.Scene.mechanim_color_def_active

    del bpy.types.Scene.mechanim_color_fk_normal
    del bpy.types.Scene.mechanim_color_fk_select
    del bpy.types.Scene.mechanim_color_fk_active

    del bpy.types.Scene.mechanim_color_ik_normal
    del bpy.types.Scene.mechanim_color_ik_select
    del bpy.types.Scene.mechanim_color_ik_active

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
