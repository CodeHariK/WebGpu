"""
Module Path: MechAnim/operators/inspect_scene.py
System Responsibility: Operator to classify bone types (DEF, DEFIK, CTRL, POLE, IK, FK), verify mesh matches, and check IK chain setup compatibility.
Build Dependencies: bpy
"""

import bpy

# The 6 supported MechAnim bone prefixes
VALID_BONE_TYPES = ("DEF_", "DEFIK_", "DEFSIK_", "CTRL_", "POLE_", "IK_", "FK_")


def classify_bone_type(bone_name: str) -> tuple[str, str]:
    """
    Classifies bone into one of the MechAnim types (DEF, DEFIK, DEFSIK, CTRL, POLE, IK, FK).
    Returns a tuple of (bone_type, base_name).
    """
    if bone_name.upper() == "ROOT":
        return "CTRL", "ROOT"

    for prefix in ("DEFSIK_", "DEFIK_", "DEF_", "CTRL_", "POLE_", "IK_", "FK_"):
        if bone_name.startswith(prefix):
            b_type = prefix.rstrip("_")
            return b_type, bone_name[len(prefix):]
    return "UNKNOWN", bone_name


def extract_chain_basename(base_name: str) -> str:
    """
    Extracts limb base name (e.g. 'arm' from 'arm_0.L' or 'arm.L').
    Strips side suffix (.L/.R, _L/_R) and index suffixes (_0, _1, .0, .1).
    """
    name = base_name
    # Strip side suffix
    for side in (".L", ".R", "_L", "_R"):
        if name.endswith(side):
            name = name[:-len(side)]
            break
    # Strip index suffix (e.g., _0, _1, _2)
    parts = name.rsplit("_", 1)
    if len(parts) == 2 and parts[1].isdigit():
        name = parts[0]
    parts = name.rsplit(".", 1)
    if len(parts) == 2 and parts[1].isdigit():
        name = parts[0]
    return name


def get_side_suffix(name: str) -> str:
    """Extracts side suffix (.L/.R or _L/_R) if present."""
    for side in (".L", ".R", "_L", "_R"):
        if name.endswith(side):
            return side
    return ""


class MECHANIM_OT_inspect_scene(bpy.types.Operator):
    """Operator to inspect Armatures, classify bone types, check mesh matches and IK target pairs."""

    bl_idname = "mechanim.inspect_scene"
    bl_label = "Inspect Armature & Meshes"
    bl_options = {"REGISTER"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Prints scene inspection log with 6-type classification and IK chain validation."""
        print("\n" + "=" * 65)
        print(" [MechAnim] BONE CLASSIFICATION & IK VALIDATION LOG")
        print("=" * 65)

        armatures = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        meshes = [obj for obj in context.scene.objects if obj.type == "MESH"]
        mesh_names = {mesh.name for mesh in meshes}

        print(f"\n--- ARMATURES FOUND: {len(armatures)} ---")
        matched_mesh_count = 0
        missing_mesh_count = 0

        for arm_obj in armatures:
            arm_data = arm_obj.data
            bone_names = {bone.name for bone in arm_data.bones}
            print(f"\nArmature Object: '{arm_obj.name}' | Total Bones: {len(arm_data.bones)}")
            print("-" * 65)

            for bone in arm_data.bones:
                b_name = bone.name
                parent_str = f" (Parent: '{bone.parent.name}')" if bone.parent else " (Root Bone)"
                
                b_type, base_name = classify_bone_type(b_name)
                
                # Check mesh match for deform bones (DEF, DEFIK, DEFSIK)
                mesh_status = ""
                if b_type in ("DEF", "DEFIK", "DEFSIK"):
                    has_match = (base_name in mesh_names) or (b_name in mesh_names)
                    if has_match:
                        matched_name = base_name if base_name in mesh_names else b_name
                        mesh_status = f" | [MESH: '{matched_name}']"
                        matched_mesh_count += 1
                    else:
                        mesh_status = f" | [MISSING MESH: '{base_name}']"
                        missing_mesh_count += 1

                # Check IK target matching for DEFIK / DEFSIK bones
                ik_status = ""
                if b_type in ("DEFIK", "DEFSIK"):
                    chain_base = extract_chain_basename(base_name)
                    side = get_side_suffix(b_name)
                    
                    if chain_base.lower() == "spine":
                        start_target = f"CTRL_{chain_base}_start{side}" if f"CTRL_{chain_base}_start{side}" in bone_names else ("CTRL_waist" if "CTRL_waist" in bone_names else "ROOT")
                        end_target = f"CTRL_{chain_base}_end{side}" if f"CTRL_{chain_base}_end{side}" in bone_names else f"CTRL_{chain_base}{side}"
                        ik_status = f" | [SPLINE IK READY -> Start: '{start_target}', End: '{end_target}']"
                    else:
                        # Expected CTRL and POLE bone names (e.g. CTRL_arm.L and POLE_arm.L)
                        expected_ctrl = f"CTRL_{chain_base}{side}"
                        expected_pole = f"POLE_{chain_base}{side}"
                        expected_ik = f"IK_{chain_base}{side}"
                        
                        has_ctrl = expected_ctrl in bone_names
                        has_pole = expected_pole in bone_names
                        has_ik = expected_ik in bone_names
                        
                        ctrl_target = expected_ctrl if has_ctrl else (expected_ik if has_ik else None)
                        pole_target = expected_pole if has_pole else None
                        
                        if ctrl_target or pole_target:
                            targets = []
                            if ctrl_target:
                                targets.append(f"Target: '{ctrl_target}'")
                            if pole_target:
                                targets.append(f"Pole: '{pole_target}'")
                            ik_status = f" | [IK READY -> {', '.join(targets)}]"
                        else:
                            ik_status = f" | [NO IK TARGETS FOUND (Expected: '{expected_ctrl}'/'{expected_ik}', '{expected_pole}')]"

                print(f"  - Bone: '{b_name}'{parent_str} | Type: {b_type}{mesh_status}{ik_status}")

        print(f"\n--- MESH OBJECTS FOUND: {len(meshes)} ---")
        for mesh_obj in meshes:
            parent_info = "None"
            if mesh_obj.parent:
                if mesh_obj.parent_type == "BONE":
                    parent_info = f"Bone '{mesh_obj.parent_bone}' on '{mesh_obj.parent.name}'"
                else:
                    parent_info = f"Object '{mesh_obj.parent.name}' ({mesh_obj.parent_type})"
            print(f"  - Mesh: '{mesh_obj.name}' | Parent: {parent_info}")

        print(f"\nSUMMARY: {matched_mesh_count} Deform bones matched with meshes, {missing_mesh_count} missing.")
        print("=" * 65 + "\n")
        
        self.report({"INFO"}, f"MechAnim Inspection Complete. {matched_mesh_count} Matched. Check Console.")
        return {"FINISHED"}


classes = (MECHANIM_OT_inspect_scene,)


def register() -> None:
    """Registers inspect scene operator."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters inspect scene operator."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
