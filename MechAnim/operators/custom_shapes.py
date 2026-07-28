"""
Module Path: MechAnim/operators/custom_shapes.py
System Responsibility: Operator to batch assign custom viewport shape objects to 5 bone categories (CTRL, POLE, DEF, FK, IK).
Build Dependencies: bpy
"""

import bpy
from .inspect_scene import classify_bone_type


class MECHANIM_OT_assign_custom_shapes(bpy.types.Operator):
    """Batch assign custom shape objects (widgets) for CTRL, POLE, DEF, FK, and IK bones."""

    bl_idname = "mechanim.assign_custom_shapes"
    bl_label = "Apply Custom Shapes"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Assigns custom shapes stored in Scene properties to all pose bones based on bone type."""
        armature_objs = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if not armature_objs:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        arm_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else armature_objs[0]
        scene = context.scene

        shape_map = {
            "CTRL": scene.mechanim_shape_ctrl,
            "POLE": scene.mechanim_shape_pole,
            "DEF": scene.mechanim_shape_def,
            "DEFIK": scene.mechanim_shape_def,
            "FK": scene.mechanim_shape_fk,
            "IK": scene.mechanim_shape_ik,
        }

        updated_count = 0
        for pbone in arm_obj.pose.bones:
            b_type, base_name = classify_bone_type(pbone.name)
            target_shape = shape_map.get(b_type)

            if target_shape:
                pbone.custom_shape = target_shape
                updated_count += 1

        self.report({"INFO"}, f"Applied custom shapes to {updated_count} bone(s) on '{arm_obj.name}'.")
        return {"FINISHED"}


classes = (MECHANIM_OT_assign_custom_shapes,)


def register() -> None:
    """Registers custom shapes operator and scene properties."""
    for cls in classes:
        bpy.utils.register_class(cls)

    # Register 5 object properties directly on Scene for UI panel persistence
    bpy.types.Scene.mechanim_shape_ctrl = bpy.props.PointerProperty(
        type=bpy.types.Object,
        name="CTRL Shape",
        description="Widget mesh for CTRL bones",
        poll=lambda self, obj: obj.type == "MESH",
    )
    bpy.types.Scene.mechanim_shape_pole = bpy.props.PointerProperty(
        type=bpy.types.Object,
        name="POLE Shape",
        description="Widget mesh for POLE bones",
        poll=lambda self, obj: obj.type == "MESH",
    )
    bpy.types.Scene.mechanim_shape_def = bpy.props.PointerProperty(
        type=bpy.types.Object,
        name="DEF Shape",
        description="Widget mesh for DEF/DEFIK bones",
        poll=lambda self, obj: obj.type == "MESH",
    )
    bpy.types.Scene.mechanim_shape_fk = bpy.props.PointerProperty(
        type=bpy.types.Object,
        name="FK Shape",
        description="Widget mesh for FK bones",
        poll=lambda self, obj: obj.type == "MESH",
    )
    bpy.types.Scene.mechanim_shape_ik = bpy.props.PointerProperty(
        type=bpy.types.Object,
        name="IK Shape",
        description="Widget mesh for IK bones",
        poll=lambda self, obj: obj.type == "MESH",
    )


def unregister() -> None:
    """Unregisters custom shapes operator and scene properties."""
    del bpy.types.Scene.mechanim_shape_ctrl
    del bpy.types.Scene.mechanim_shape_pole
    del bpy.types.Scene.mechanim_shape_def
    del bpy.types.Scene.mechanim_shape_fk
    del bpy.types.Scene.mechanim_shape_ik

    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
