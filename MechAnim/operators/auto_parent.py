"""
Module Path: MechAnim/operators/auto_parent.py
System Responsibility: Operator to automatically parent mesh objects to corresponding DEF/DEFIK bones and clear parenting safely.
Build Dependencies: bpy
"""

import bpy


def get_clean_name(name: str) -> str:
    """Strips DEF_, DEFIK_, or DEFSIK_ prefix from bone name to find base name."""
    if name.startswith("DEFSIK_"):
        return name[7:]
    elif name.startswith("DEFIK_"):
        return name[6:]
    elif name.startswith("DEF_"):
        return name[4:]
    return name


class MECHANIM_OT_auto_parent_meshes(bpy.types.Operator):
    """Automatically parent mesh objects to matching DEF / DEFIK bones."""

    bl_idname = "mechanim.auto_parent_meshes"
    bl_label = "Auto-Parent Meshes to Bones"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Parents selected or scene meshes to corresponding armature bones."""
        armature_objs = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if not armature_objs:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        # Use active object if armature, otherwise pick first armature in scene
        arm_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else armature_objs[0]
        arm_data = arm_obj.data

        # Map clean bone names to actual bone names in armature
        bone_map: dict[str, str] = {}
        for bone in arm_data.bones:
            clean_name = get_clean_name(bone.name)
            bone_map[clean_name] = bone.name
            bone_map[bone.name] = bone.name

        # Find mesh objects to parent
        meshes = [obj for obj in context.scene.objects if obj.type == "MESH"]
        parented_count = 0

        for mesh_obj in meshes:
            m_name = mesh_obj.name
            if m_name in bone_map:
                target_bone_name = bone_map[m_name]
                
                # Keep original transformation in world space
                matrix_world = mesh_obj.matrix_world.copy()
                
                # Assign parent armature and bone
                mesh_obj.parent = arm_obj
                mesh_obj.parent_type = "BONE"
                mesh_obj.parent_bone = target_bone_name
                
                # Restore world matrix to avoid object jumping
                mesh_obj.matrix_world = matrix_world
                parented_count += 1
                print(f"[MechAnim] Parented Mesh '{mesh_obj.name}' -> Bone '{target_bone_name}' on '{arm_obj.name}'")

        self.report({"INFO"}, f"Successfully parented {parented_count} mesh(es) to matching bones on '{arm_obj.name}'.")
        return {"FINISHED"}


class MECHANIM_OT_unparent_all_meshes(bpy.types.Operator):
    """Unparent all mesh objects in scene while preserving world transformation."""

    bl_idname = "mechanim.unparent_all_meshes"
    bl_label = "Unparent All Meshes"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Clears parent on all mesh objects keeping world position via native operator."""
        meshes = [obj for obj in context.scene.objects if obj.type == "MESH" and obj.parent is not None]
        if not meshes:
            self.report({"INFO"}, "No parented mesh objects found in scene.")
            return {"FINISHED"}

        # Store active object & selection state to restore later
        prev_active = context.active_object
        prev_selected = context.selected_objects[:]

        # Deselect all
        bpy.ops.object.select_all(action="DESELECT")

        # Select target parented meshes
        for mesh_obj in meshes:
            mesh_obj.select_set(True)

        if meshes:
            context.view_layer.objects.active = meshes[0]
            # Call native Blender operator for CLEAR_KEEP_TRANSFORM (prevents C-level memory matrix crashes)
            bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")

        # Restore selection
        bpy.ops.object.select_all(action="DESELECT")
        for obj in prev_selected:
            if obj in context.scene.objects.values():
                obj.select_set(True)
        if prev_active and prev_active in context.scene.objects.values():
            context.view_layer.objects.active = prev_active

        self.report({"INFO"}, f"Unparented {len(meshes)} mesh(es). Preserved world transformations.")
        return {"FINISHED"}


classes = (
    MECHANIM_OT_auto_parent_meshes,
    MECHANIM_OT_unparent_all_meshes,
)


def register() -> None:
    """Registers auto parent operators."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters auto parent operators."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
