"""
Module Path: MechAnim/operators/fk_ik_generator.py
System Responsibility: Operator to duplicate DEFIK bone chains into connected FK_ and IK_ chains, attach IK Constraint on tip IK_ bone targeting CTRL and POLE, assign per-chain bone collections, and set up drivers.
Build Dependencies: bpy
"""

import bpy
from .inspect_scene import classify_bone_type, extract_chain_basename, get_side_suffix


def get_or_create_collection(arm_data: bpy.types.Armature, coll_name: str) -> bpy.types.BoneCollection:
    """Retrieves or creates a bone collection by name."""
    if hasattr(arm_data, "collections"):
        coll = arm_data.collections.get(coll_name)
        if not coll:
            coll = arm_data.collections.new(coll_name)
        return coll
    return None


def assign_bone_to_collection(arm_data: bpy.types.Armature, coll: bpy.types.BoneCollection, bone_name: str) -> None:
    """Assigns a bone to a bone collection using Blender 4.0+/5.0+ API (coll.assign(bone))."""
    if not coll:
        return
    bone = arm_data.bones.get(bone_name)
    if bone:
        coll.assign(bone)


def parse_defsik_count(bone_name: str) -> int | None:
    """Parses point count syntax from DEFSIK bone name (e.g., DEFSIK_4_spine_0 -> 4 points)."""
    if bone_name.startswith("DEFSIK_"):
        remainder = bone_name[len("DEFSIK_"):]
        if "_" in remainder:
            spec_part, _ = remainder.split("_", 1)
            if spec_part.isdigit():
                return int(spec_part)
    return None


def sample_chain_pos(t: float, pts: list, lens: list, total: float):
    """Samples position at uniform arc-length fraction t in [0, 1]."""
    target_dist = t * total
    for i in range(len(lens) - 1):
        if lens[i] <= target_dist <= lens[i+1]:
            seg_len = lens[i+1] - lens[i]
            factor = (target_dist - lens[i]) / seg_len if seg_len > 0 else 0.0
            return pts[i].lerp(pts[i+1], factor)
    return pts[-1].copy()


def get_active_armature(context: bpy.types.Context) -> bpy.types.Object | None:
    """Finds active selected armature object or falls back to first armature in scene."""
    active_obj = context.active_object
    if active_obj and active_obj.type == "ARMATURE":
        return active_obj
    armatures = [o for o in context.scene.objects if o.type == "ARMATURE"]
    return armatures[0] if armatures else None


def group_defik_bones(arm_data: bpy.types.Armature) -> dict[str, list[str]]:
    """Groups DEFIK and DEFSIK bones by chain key (e.g. 'arm.L', 'leg.L', 'spine')."""
    groups: dict[str, list[str]] = {}
    for bone in arm_data.bones:
        b_type, base_name = classify_bone_type(bone.name)
        if b_type in ("DEFIK", "DEFSIK"):
            chain_base = extract_chain_basename(base_name)
            side = get_side_suffix(bone.name)
            group_key = f"{chain_base}{side}"
            groups.setdefault(group_key, []).append(bone.name)
    return groups


def duplicate_fk_ik_edit_bones(edit_bones: bpy.types.ArmatureEditBones, defik_bone_names: list[str]) -> None:
    """Duplicates DEFIK bones into FK_ and IK_ bones in Edit Mode."""
    for defik_name in defik_bone_names:
        b_type, base_name = classify_bone_type(defik_name)
        orig_ebone = edit_bones.get(defik_name)
        if not orig_ebone:
            continue
        for prefix in ("FK_", "IK_"):
            name = f"{prefix}{base_name}"
            if name not in edit_bones:
                ebone = edit_bones.new(name)
                ebone.head = orig_ebone.head.copy()
                ebone.tail = orig_ebone.tail.copy()
                ebone.roll = orig_ebone.roll
                ebone.use_deform = False


def create_defsik_ctrl_edit_bones(edit_bones: bpy.types.ArmatureEditBones, group_key: str, defik_bone_names: list[str]) -> int:
    """Creates point-count CTRL bones for Spline IK chain in Edit Mode."""
    num_ctrl_bones = None
    for b_name in defik_bone_names:
        cnt = parse_defsik_count(b_name)
        if cnt:
            num_ctrl_bones = cnt
            break

    if not num_ctrl_bones or num_ctrl_bones < 3:
        return 0

    chain_ebones = [edit_bones.get(n) for n in defik_bone_names if edit_bones.get(n)]
    joint_pts = [chain_ebones[0].head.copy()] + [b.tail.copy() for b in chain_ebones]
    cum_lens = [0.0]
    for i in range(len(joint_pts) - 1):
        cum_lens.append(cum_lens[-1] + (joint_pts[i+1] - joint_pts[i]).length)
    total_len = cum_lens[-1]

    for idx in range(num_ctrl_bones):
        ctrl_b_name = f"CTRL_{group_key}_{idx}"
        if ctrl_b_name not in edit_bones:
            t = idx / (num_ctrl_bones - 1)
            pos = sample_chain_pos(t, joint_pts, cum_lens, total_len)
            c_bone = edit_bones.new(ctrl_b_name)
            c_bone.head = pos.copy()
            c_bone.tail = pos + (joint_pts[-1] - joint_pts[0]).normalized() * 0.2
            c_bone.use_deform = False
            print(f"[MechAnim] Auto-created point-count control bone '{ctrl_b_name}'.")

def create_defik_ctrl_edit_bones(edit_bones: bpy.types.ArmatureEditBones, group_key: str, defik_bone_names: list[str]) -> None:
    """Auto-creates CTRL_<chain> target bone at chain tip for DEFIK limb chains if missing."""
    ctrl_name = f"CTRL_{group_key}"

    chain_ebones = [edit_bones.get(n) for n in defik_bone_names if edit_bones.get(n)]
    if not chain_ebones:
        return

    tip_ebone = chain_ebones[-1]

    # Auto-create CTRL target bone at chain tip tail if missing
    if ctrl_name not in edit_bones:
        c_bone = edit_bones.new(ctrl_name)
        c_bone.head = tip_ebone.tail.copy()
        c_bone.tail = tip_ebone.tail + (tip_ebone.tail - tip_ebone.head).normalized() * 0.2
        c_bone.use_deform = False
        print(f"[MechAnim] Auto-created limb IK target bone '{ctrl_name}'.")


def parent_fk_ik_edit_bones(edit_bones: bpy.types.ArmatureEditBones, defik_bone_names: list[str]) -> None:
    """Sets up connected parenting for FK_ and IK_ bone chains."""
    for defik_name in defik_bone_names:
        b_type, base_name = classify_bone_type(defik_name)
        orig_ebone = edit_bones.get(defik_name)
        if not orig_ebone:
            continue
        fk_ebone = edit_bones.get(f"FK_{base_name}")
        ik_ebone = edit_bones.get(f"IK_{base_name}")

        if orig_ebone.parent:
            parent_btype, parent_base = classify_bone_type(orig_ebone.parent.name)
            if parent_btype in ("DEFIK", "DEFSIK"):
                fk_parent = edit_bones.get(f"FK_{parent_base}")
                ik_parent = edit_bones.get(f"IK_{parent_base}")
                if fk_ebone and fk_parent:
                    fk_ebone.parent = fk_parent
                    fk_ebone.use_connect = orig_ebone.use_connect
                if ik_ebone and ik_parent:
                    ik_ebone.parent = ik_parent
                    ik_ebone.use_connect = orig_ebone.use_connect
            else:
                if fk_ebone:
                    fk_ebone.parent = orig_ebone.parent
                    fk_ebone.use_connect = False
                if ik_ebone:
                    ik_ebone.parent = orig_ebone.parent
                    ik_ebone.use_connect = False


def setup_spline_ik_curve(context: bpy.types.Context, arm_obj: bpy.types.Object, group_key: str, defik_bone_names: list[str], pose_bones) -> tuple[bpy.types.Object, int]:
    """Creates and configures NURBS curve and hooks for Spline IK."""
    curve_name = f"Curve_spine{get_side_suffix(group_key)}"
    curve_obj = context.scene.objects.get(curve_name)

    num_points = 3
    for b_name in defik_bone_names:
        cnt = parse_defsik_count(b_name)
        if cnt:
            num_points = cnt
            break

    if not curve_obj:
        curve_data = bpy.data.curves.new(name=curve_name, type="CURVE")
        curve_data.dimensions = "3D"
        spline = curve_data.splines.new("NURBS")
        spline.use_endpoint_u = True
        if num_points > 1:
            spline.points.add(num_points - 1)

        order_setting = int(getattr(context.scene, "mechanim_spline_order", "3"))
        spline.order_u = min(order_setting, num_points)

        curve_obj = bpy.data.objects.new(curve_name, curve_data)
        context.scene.collection.objects.link(curve_obj)

        chain_pbones = [pose_bones.get(n) for n in defik_bone_names if pose_bones.get(n)]
        joint_w = [arm_obj.matrix_world @ chain_pbones[0].head] + [arm_obj.matrix_world @ b.tail for b in chain_pbones]
        cum_w = [0.0]
        for i in range(len(joint_w) - 1):
            cum_w.append(cum_w[-1] + (joint_w[i+1] - joint_w[i]).length)
        tot_w = cum_w[-1]

        for idx in range(num_points):
            t = idx / (num_points - 1)
            pos_w = sample_chain_pos(t, joint_w, cum_w, tot_w)
            spline.points[idx].co = (pos_w.x, pos_w.y, pos_w.z, 1.0)

    # Hook Curve points directly to matching control bones
    for idx in range(num_points):
        ctrl_b_name = f"CTRL_{group_key}_{idx}"
        c_bone = pose_bones.get(ctrl_b_name)
        if c_bone and curve_obj:
            hook_m = curve_obj.modifiers.new(name=f"Hook_{idx}", type="HOOK")
            hook_m.object = arm_obj
            hook_m.subtarget = c_bone.name
            hook_m.vertex_indices_set([idx])
            hook_m.matrix_inverse = (arm_obj.matrix_world @ c_bone.matrix).inverted()

    # Limit distance between adjacent control bones
    if num_points > 1:
        for idx in range(1, num_points):
            curr_ctrl = pose_bones.get(f"CTRL_{group_key}_{idx}")
            prev_ctrl = pose_bones.get(f"CTRL_{group_key}_{idx-1}")
            if curr_ctrl and prev_ctrl:
                dist = (curr_ctrl.head - prev_ctrl.head).length
                for con in list(curr_ctrl.constraints):
                    if con.type == "LIMIT_DISTANCE" and con.name.startswith("MechAnim_Limit_"):
                        curr_ctrl.constraints.remove(con)

                lim_con = curr_ctrl.constraints.new(type="LIMIT_DISTANCE")
                lim_con.name = f"MechAnim_Limit_{idx}"
                lim_con.target = arm_obj
                lim_con.subtarget = prev_ctrl.name
                lim_con.distance = dist * 1.05
                lim_con.limit_mode = "LIMITDIST_INSIDE"

    return curve_obj, num_points


def setup_ik_constraints(context: bpy.types.Context, arm_obj: bpy.types.Object, group_key: str, defik_bone_names: list[str], pose_bones) -> None:
    """Attaches IK or Spline IK constraint onto tip IK bone."""
    ctrl_name = f"CTRL_{group_key}"
    ctrl_pbone = pose_bones.get(ctrl_name)
    pole_name = f"POLE_{group_key}"
    pole_pbone = pose_bones.get(pole_name)

    ik_chain_bones = [pose_bones.get(f"IK_{classify_bone_type(n)[1]}") for n in defik_bone_names if pose_bones.get(f"IK_{classify_bone_type(n)[1]}")]
    if not ik_chain_bones:
        return

    tip_ik_bone = next((pb for pb in ik_chain_bones if not any(child.parent == pb for child in ik_chain_bones)), ik_chain_bones[-1])

    for constraint in list(tip_ik_bone.constraints):
        if constraint.type in ("IK", "SPLINE_IK"):
            tip_ik_bone.constraints.remove(constraint)

    if "spine" in group_key.lower() or any(classify_bone_type(n)[0] == "DEFSIK" for n in defik_bone_names):
        curve_obj, num_points = setup_spline_ik_curve(context, arm_obj, group_key, defik_bone_names, pose_bones)
        spline_ik = tip_ik_bone.constraints.new(type="SPLINE_IK")
        spline_ik.target = curve_obj
        spline_ik.chain_count = len(ik_chain_bones)
        spline_ik.use_curve_radius = False
        spline_ik.y_scale_mode = getattr(context.scene, "mechanim_spline_y_scale_mode", "FIT_CURVE")
        print(f"[MechAnim] Created NURBS ({num_points} pts) Spline IK on '{arm_obj.name}' -> Curve: '{curve_obj.name}'.")
    else:
        if ctrl_pbone:
            ik_con = tip_ik_bone.constraints.new(type="IK")
            ik_con.target = arm_obj
            ik_con.subtarget = ctrl_name
            ik_con.chain_count = len(ik_chain_bones)
            if pole_pbone:
                ik_con.pole_target = arm_obj
                ik_con.pole_subtarget = pole_name
                ik_con.pole_angle = 0.0
            print(f"[MechAnim] Added Limb IK Constraint on '{tip_ik_bone.name}' -> Target: '{ctrl_name}'")


def setup_fk_ik_drivers(arm_obj: bpy.types.Object, defik_bone_names: list[str], group_key: str, pose_bones) -> None:
    """Sets up complementary scripted drivers for FK/IK Copy Transforms constraints."""
    ctrl_name = f"CTRL_{group_key}"
    switch_ctrl_name = ctrl_name
    if "spine" in group_key.lower() or any(classify_bone_type(n)[0] == "DEFSIK" for n in defik_bone_names):
        for candidate in (f"CTRL_{group_key}_0", "CTRL_spine_0", f"CTRL_{group_key}_end", "CTRL_spine_end", "CTRL_spine"):
            if candidate in pose_bones:
                switch_ctrl_name = candidate
                break

    switch_pbone = pose_bones.get(switch_ctrl_name)
    if switch_pbone:
        if "FK_IK_Switch" not in switch_pbone:
            switch_pbone["FK_IK_Switch"] = 0.0
        ui_data = switch_pbone.id_properties_ui("FK_IK_Switch")
        ui_data.update(min=0.0, max=1.0, description="FK/IK Switch (0.0 = FK, 1.0 = IK)")

    for defik_name in defik_bone_names:
        b_type, base_name = classify_bone_type(defik_name)
        defik_pbone = pose_bones.get(defik_name)
        if not defik_pbone:
            continue

        for constraint in list(defik_pbone.constraints):
            if constraint.name in ("MechAnim_Copy_FK", "MechAnim_Copy_IK"):
                try:
                    arm_obj.driver_remove(f'pose.bones["{defik_name}"].constraints["{constraint.name}"].influence')
                except Exception:
                    pass
                defik_pbone.constraints.remove(constraint)

        c_fk = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
        c_fk.name, c_fk.target, c_fk.subtarget, c_fk.influence = "MechAnim_Copy_FK", arm_obj, f"FK_{base_name}", 1.0
        c_ik = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
        c_ik.name, c_ik.target, c_ik.subtarget, c_ik.influence = "MechAnim_Copy_IK", arm_obj, f"IK_{base_name}", 0.0

        if switch_pbone:
            add_single_prop_driver(arm_obj, f'pose.bones["{defik_name}"].constraints["{c_ik.name}"].influence', switch_ctrl_name, "sw")
            add_single_prop_driver(arm_obj, f'pose.bones["{defik_name}"].constraints["{c_fk.name}"].influence', switch_ctrl_name, "1.0 - sw")


def add_single_prop_driver(arm_obj: bpy.types.Object, data_path: str, switch_ctrl_name: str, expr: str) -> None:
    """Helper to add scripted driver targeting FK_IK_Switch custom property."""
    dfc = arm_obj.driver_add(data_path)
    if dfc:
        d = dfc.driver
        d.type = "SCRIPTED"
        for var in list(d.variables):
            d.variables.remove(var)
        var = d.variables.new()
        var.name, var.type = "sw", "SINGLE_PROP"
        t = var.targets[0]
        t.id_type, t.id, t.data_path = "OBJECT", arm_obj, f'pose.bones["{switch_ctrl_name}"]["FK_IK_Switch"]'
        d.expression = expr


class MECHANIM_OT_generate_fk_ik_chains(bpy.types.Operator):
    """Duplicates DEFIK chains into connected FK_ and IK_ chains, attaches IK constraint on tip IK_ bone, and sets up driven constraints."""

    bl_idname = "mechanim.generate_fk_ik_chains"
    bl_label = "Generate FK/IK Chains & Drivers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes modularized FK/IK chain creation workflow."""
        arm_obj = get_active_armature(context)
        if not arm_obj:
            self.report({"WARNING"}, "No Armature object selected.")
            return {"CANCELLED"}

        prev_mode = context.mode
        arm_data = arm_obj.data
        defik_groups = group_defik_bones(arm_data)

        if not defik_groups:
            self.report({"WARNING"}, f"No DEFIK or DEFSIK bones found in active armature '{arm_obj.name}'.")
            return {"CANCELLED"}

        # Activate armature object
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")
        arm_obj.select_set(True)
        context.view_layer.objects.active = arm_obj

        # Edit Mode: Bone Duplication & Parent Setup
        bpy.ops.object.mode_set(mode="EDIT")
        edit_bones = arm_data.edit_bones

        for group_key, defik_bone_names in defik_groups.items():
            duplicate_fk_ik_edit_bones(edit_bones, defik_bone_names)
            if "spine" in group_key.lower() or any(classify_bone_type(n)[0] == "DEFSIK" for n in defik_bone_names):
                num_ctrls = create_defsik_ctrl_edit_bones(edit_bones, group_key, defik_bone_names)
                if num_ctrls == 0:
                    self.report({"ERROR"}, f"DEFSIK chain '{group_key}' lacks point count specification (e.g. DEFSIK_4_spine_0).")
                    bpy.ops.object.mode_set(mode=prev_mode if prev_mode in ("EDIT", "POSE", "OBJECT") else "OBJECT")
                    return {"CANCELLED"}
            else:
                create_defik_ctrl_edit_bones(edit_bones, group_key, defik_bone_names)
            parent_fk_ik_edit_bones(edit_bones, defik_bone_names)

        # Pose Mode: Constraint Setup, Collections & Drivers
        bpy.ops.object.mode_set(mode="POSE")
        pose_bones = arm_obj.pose.bones

        for group_key, defik_bone_names in defik_groups.items():
            coll_def = get_or_create_collection(arm_data, f"DEF_{group_key}")
            coll_fk = get_or_create_collection(arm_data, f"FK_{group_key}")
            coll_ik = get_or_create_collection(arm_data, f"IK_{group_key}")

            for n in defik_bone_names:
                b_type, base_name = classify_bone_type(n)
                assign_bone_to_collection(arm_data, coll_def, n)
                assign_bone_to_collection(arm_data, coll_fk, f"FK_{base_name}")
                assign_bone_to_collection(arm_data, coll_ik, f"IK_{base_name}")

            setup_ik_constraints(context, arm_obj, group_key, defik_bone_names, pose_bones)
            setup_fk_ik_drivers(arm_obj, defik_bone_names, group_key, pose_bones)

        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report({"INFO"}, f"MechAnim: Generated connected FK & IK chains for active armature '{arm_obj.name}'.")
        return {"FINISHED"}


class MECHANIM_OT_clear_fk_ik_chains(bpy.types.Operator):
    """Deletes all generated FK_, IK_, and CTRL_ bones, removes constraints/drivers, custom properties, and clears bone collections."""

    bl_idname = "mechanim.clear_fk_ik_chains"
    bl_label = "Clear FK/IK Chains & Collections"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Removes all generated FK_, IK_, and auto-created CTRL_ bones, drivers, constraints, CTRL properties, and bone collections safely."""
        arm_obj = get_active_armature(context)
        if not arm_obj:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        arm_data = arm_obj.data
        prev_mode = context.mode
        context.view_layer.objects.active = arm_obj
        bpy.ops.object.mode_set(mode="POSE")

        # 1. Clean Pose Mode constraints, drivers, custom properties
        for pbone in arm_obj.pose.bones:
            for constraint in list(pbone.constraints):
                if constraint.name in ("MechAnim_Copy_FK", "MechAnim_Copy_IK") or constraint.type in ("IK", "SPLINE_IK"):
                    try:
                        arm_obj.driver_remove(f'pose.bones["{pbone.name}"].constraints["{constraint.name}"].influence')
                    except Exception:
                        pass
                    pbone.constraints.remove(constraint)
            if "FK_IK_Switch" in pbone.keys():
                try:
                    del pbone["FK_IK_Switch"]
                except Exception:
                    pass

        # 2. Delete generated bones in Edit Mode (FK_, IK_, CTRL_ bones, excluding ROOT)
        bpy.ops.object.mode_set(mode="EDIT")
        edit_bones = arm_data.edit_bones
        deleted_count = 0
        bones_to_delete = [b.name for b in edit_bones if b.name.startswith(("FK_", "IK_", "CTRL_")) and b.name.upper() != "ROOT"]
        for b_name in bones_to_delete:
            ebone = edit_bones.get(b_name)
            if ebone:
                edit_bones.remove(ebone)
                deleted_count += 1

        # 3. Clean collections, curves, empties
        if hasattr(arm_data, "collections"):
            colls_to_remove = [c.name for c in arm_data.collections if c.name.startswith(("DEF_", "FK_", "IK_")) or c.name in ("DEF", "FK", "IK")]
            for c_name in colls_to_remove:
                coll = arm_data.collections.get(c_name)
                if coll:
                    arm_data.collections.remove(coll)

        for curve_obj in [o for o in context.scene.objects if o.type == "CURVE" and o.name.startswith("Curve_")]:
            curve_data = curve_obj.data
            bpy.data.objects.remove(curve_obj, do_unlink=True)
            if curve_data and curve_data.users == 0:
                bpy.data.curves.remove(curve_data)

        for empty_obj in [o for o in context.scene.objects if o.type == "EMPTY" and o.name.startswith(("Hook", "Empty", "Curve_"))]:
            bpy.data.objects.remove(empty_obj, do_unlink=True)

        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report({"INFO"}, f"MechAnim: Cleared {deleted_count} generated FK/IK bone(s), curves, drivers & collections.")
        return {"FINISHED"}


classes = (
    MECHANIM_OT_generate_fk_ik_chains,
    MECHANIM_OT_clear_fk_ik_chains,
)


def register() -> None:
    """Registers FK/IK generator operators."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters FK/IK generator operators."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
