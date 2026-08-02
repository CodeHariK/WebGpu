"""
Module Path: MechAnim/operators/inspect_scene.py
System Responsibility: Operator to classify bone types (DEF, DEFIK, DEFSIK, CTRL, POLE, IK, FK), verify mesh matches, and check IK chain setup compatibility.
Build Dependencies: bpy
"""

import bpy

VALID_BONE_TYPES = ("DEF_", "DEFIK_", "DEFSIK_", "CTRL_", "POLE_", "IK_", "FK_")


def classify_bone_type(bone_name: str) -> tuple[str, str]:
    """Classifies bone into MechAnim types (DEF, DEFIK, DEFSIK, CTRL, POLE, IK, FK)."""
    if bone_name.upper() == "ROOT":
        return "CTRL", "ROOT"

    if bone_name.startswith("DEFSIK_"):
        remainder = bone_name[len("DEFSIK_"):]
        if "_" in remainder:
            count_part, base_part = remainder.split("_", 1)
            if count_part.isdigit():
                return "DEFSIK", base_part
        return "DEFSIK", remainder

    for prefix in ("DEFIK_", "DEF_", "CTRL_", "POLE_", "IK_", "FK_"):
        if bone_name.startswith(prefix):
            return prefix.rstrip("_"), bone_name[len(prefix):]
    return "UNKNOWN", bone_name


def extract_chain_basename(base_name: str) -> str:
    """Extracts limb base name by stripping side and index suffixes."""
    name = base_name
    for side in (".L", ".R", "_L", "_R"):
        if name.endswith(side):
            name = name[:-len(side)]
            break
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


def check_mesh_match(b_type: str, b_name: str, base_name: str, mesh_names: set[str]) -> tuple[str, bool]:
    """Helper to check if deform bone matches a mesh object."""
    if b_type in ("DEF", "DEFIK", "DEFSIK"):
        if (base_name in mesh_names) or (b_name in mesh_names):
            matched_name = base_name if base_name in mesh_names else b_name
            return f" | [MESH: '{matched_name}']", True
        return f" | [MISSING MESH: '{base_name}']", False
    return "", False


def check_ik_status(b_type: str, b_name: str, base_name: str, bone_names: set[str]) -> str:
    """Helper to check IK target configuration status."""
    if b_type not in ("DEFIK", "DEFSIK"):
        return ""

    chain_base = extract_chain_basename(base_name)
    side = get_side_suffix(b_name)

    if chain_base.lower() == "spine":
        start_target = f"CTRL_{chain_base}_start{side}" if f"CTRL_{chain_base}_start{side}" in bone_names else ("CTRL_waist" if "CTRL_waist" in bone_names else "ROOT")
        end_target = f"CTRL_{chain_base}_end{side}" if f"CTRL_{chain_base}_end{side}" in bone_names else f"CTRL_{chain_base}{side}"
        return f" | [SPLINE IK READY -> Start: '{start_target}', End: '{end_target}']"

    expected_ctrl, expected_pole = f"CTRL_{chain_base}{side}", f"POLE_{chain_base}{side}"
    ctrl_target = expected_ctrl if expected_ctrl in bone_names else (f"IK_{chain_base}{side}" if f"IK_{chain_base}{side}" in bone_names else None)
    pole_target = expected_pole if expected_pole in bone_names else None

    if ctrl_target or pole_target:
        targets = [f"Target: '{t}'" for t in (ctrl_target, pole_target) if t]
        return f" | [IK READY -> {', '.join(targets)}]"
    return f" | [NO IK TARGETS FOUND (Expected: '{expected_ctrl}', '{expected_pole}')]"


class MECHANIM_OT_inspect_scene(bpy.types.Operator):
    """Operator to inspect Armatures, classify bone types, check mesh matches and IK target pairs."""

    bl_idname = "mechanim.inspect_scene"
    bl_label = "Inspect Armature & Meshes"
    bl_options = {"REGISTER"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Prints scene inspection log with bone classification and IK chain validation."""
        print("\n" + "=" * 65 + "\n [MechAnim] BONE CLASSIFICATION & IK VALIDATION LOG\n" + "=" * 65)

        armatures = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        meshes = [obj for obj in context.scene.objects if obj.type == "MESH"]
        mesh_names = {m.name for m in meshes}

        matched_mesh_count, missing_mesh_count = 0, 0

        for arm_obj in armatures:
            bone_names = {b.name for b in arm_obj.data.bones}
            print(f"\nArmature Object: '{arm_obj.name}' | Total Bones: {len(arm_obj.data.bones)}\n" + "-" * 65)

            for bone in arm_obj.data.bones:
                b_type, base_name = classify_bone_type(bone.name)
                parent_str = f" (Parent: '{bone.parent.name}')" if bone.parent else " (Root Bone)"
                
                mesh_status, is_matched = check_mesh_match(b_type, bone.name, base_name, mesh_names)
                if b_type in ("DEF", "DEFIK", "DEFSIK"):
                    if is_matched:
                        matched_mesh_count += 1
                    else:
                        missing_mesh_count += 1

                ik_status = check_ik_status(b_type, bone.name, base_name, bone_names)
                pbone = arm_obj.pose.bones.get(bone.name)
                con_str = f" | [CONSTRAINTS: {', '.join([c.type for c in pbone.constraints])}]" if (pbone and pbone.constraints) else ""

                print(f"  - Bone: '{bone.name}'{parent_str} | Type: {b_type}{mesh_status}{ik_status}{con_str}")

        print(f"\nSUMMARY: {matched_mesh_count} Deform bones matched with meshes, {missing_mesh_count} missing.\n" + "=" * 65 + "\n")
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
