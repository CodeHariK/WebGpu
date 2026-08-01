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


class MECHANIM_OT_generate_fk_ik_chains(bpy.types.Operator):
    """Duplicates DEFIK chains into connected FK_ and IK_ chains, attaches IK constraint on tip IK_ bone targeting CTRL and POLE, assigns per-chain collections, and sets up driven constraints."""

    bl_idname = "mechanim.generate_fk_ik_chains"
    bl_label = "Generate FK/IK Chains & Drivers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Executes FK/IK chain duplication, IK constraint attachment on IK_ chain, collection grouping, and driver setup."""
        active_obj = context.active_object
        arm_obj = active_obj if (active_obj and active_obj.type == "ARMATURE") else None
        if not arm_obj:
            armatures = [o for o in context.scene.objects if o.type == "ARMATURE"]
            if armatures:
                arm_obj = armatures[0]

        if not arm_obj:
            self.report({"WARNING"}, "No Armature object selected.")
            return {"CANCELLED"}

        prev_mode = context.mode
        arm_data = arm_obj.data

        # 1. Group DEFIK and DEFSIK bones by chain key (e.g. 'arm.L', 'leg.L', 'spine')
        defik_groups: dict[str, list[str]] = {}
        for bone in arm_data.bones:
            b_type, base_name = classify_bone_type(bone.name)
            if b_type in ("DEFIK", "DEFSIK"):
                chain_base = extract_chain_basename(base_name)
                side = get_side_suffix(bone.name)
                group_key = f"{chain_base}{side}"

                if group_key not in defik_groups:
                    defik_groups[group_key] = []
                defik_groups[group_key].append(bone.name)

        if not defik_groups:
            self.report({"WARNING"}, f"No DEFIK or DEFSIK bones found in active armature '{arm_obj.name}'.")
            return {"CANCELLED"}

        # Ensure active object context for current armature
        bpy.ops.object.mode_set(mode="OBJECT")
        bpy.ops.object.select_all(action="DESELECT")
        arm_obj.select_set(True)
        context.view_layer.objects.active = arm_obj

        # 2. Switch to Edit Mode for bone creation
        bpy.ops.object.mode_set(mode="EDIT")
        edit_bones = arm_data.edit_bones

        # 2. Duplicate DEFIK bones in Edit Mode for FK_ and IK_ chains
        for group_key, defik_bone_names in defik_groups.items():
            for defik_name in defik_bone_names:
                b_type, base_name = classify_bone_type(defik_name)
                orig_ebone = edit_bones.get(defik_name)
                if not orig_ebone:
                    continue

                fk_name = f"FK_{base_name}"
                ik_name = f"IK_{base_name}"

                if fk_name not in edit_bones:
                    fk_ebone = edit_bones.new(fk_name)
                    fk_ebone.head = orig_ebone.head.copy()
                    fk_ebone.tail = orig_ebone.tail.copy()
                    fk_ebone.roll = orig_ebone.roll
                    fk_ebone.use_deform = False

                if ik_name not in edit_bones:
                    ik_ebone = edit_bones.new(ik_name)
                    ik_ebone.head = orig_ebone.head.copy()
                    ik_ebone.tail = orig_ebone.tail.copy()
                    ik_ebone.roll = orig_ebone.roll
                    ik_ebone.use_deform = False

        # 3. Setup connected parenting for new FK_ and IK_ chains & auto-create Spline IK CTRL bones
        for group_key, defik_bone_names in defik_groups.items():
            if "spine" in group_key.lower():
                start_ctrl_name = f"CTRL_{group_key}_start" if f"CTRL_{group_key}_start" not in edit_bones else f"CTRL_spine_start"
                end_ctrl_name = f"CTRL_{group_key}_end" if f"CTRL_{group_key}_end" not in edit_bones else f"CTRL_spine_end"

                first_bone = edit_bones.get(defik_bone_names[0])
                last_bone = edit_bones.get(defik_bone_names[-1])

                if first_bone and ("CTRL_spine_start" not in edit_bones and f"CTRL_{group_key}_start" not in edit_bones):
                    c_start = edit_bones.new(start_ctrl_name)
                    c_start.head = first_bone.head.copy()
                    c_start.tail = first_bone.head + (first_bone.tail - first_bone.head) * 0.5
                    c_start.use_deform = False

                if last_bone and ("CTRL_spine_end" not in edit_bones and f"CTRL_{group_key}_end" not in edit_bones):
                    c_end = edit_bones.new(end_ctrl_name)
                    c_end.head = last_bone.tail.copy()
                    c_end.tail = last_bone.tail + (last_bone.tail - last_bone.head) * 0.5
                    c_end.use_deform = False

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

        # 4. Switch to Pose Mode for Per-Chain Bone Collections, IK Constraints, Copy Transforms, and Drivers
        bpy.ops.object.mode_set(mode="POSE")
        pose_bones = arm_obj.pose.bones

        for group_key, defik_bone_names in defik_groups.items():
            coll_def = get_or_create_collection(arm_data, f"DEF_{group_key}")
            coll_fk = get_or_create_collection(arm_data, f"FK_{group_key}")
            coll_ik = get_or_create_collection(arm_data, f"IK_{group_key}")

            ctrl_name = f"CTRL_{group_key}"
            ctrl_pbone = pose_bones.get(ctrl_name)
            pole_name = f"POLE_{group_key}"
            pole_pbone = pose_bones.get(pole_name)

            if ctrl_pbone and coll_ik:
                assign_bone_to_collection(arm_data, coll_ik, ctrl_name)
            if pole_pbone and coll_ik:
                assign_bone_to_collection(arm_data, coll_ik, pole_name)

            ctrl_targets_for_switch = [ctrl_pbone] if ctrl_pbone else []
            if "spine" in group_key.lower():
                spine_targets = [
                    pose_bones.get(f"CTRL_{group_key}_end"),
                    pose_bones.get("CTRL_spine_end"),
                    pose_bones.get(f"CTRL_{group_key}_start"),
                    pose_bones.get("CTRL_spine_start"),
                ]
                ctrl_targets_for_switch = [b for b in spine_targets if b is not None]

            for switch_target in ctrl_targets_for_switch:
                if "FK_IK_Switch" not in switch_target:
                    switch_target["FK_IK_Switch"] = 0.0
                    
                ui_data = switch_target.id_properties_ui("FK_IK_Switch")
                ui_data.update(min=0.0, max=1.0, description="FK/IK Switch (0.0 = FK, 1.0 = IK)")

            ik_chain_bones = [pose_bones.get(f"IK_{classify_bone_type(n)[1]}") for n in defik_bone_names if pose_bones.get(f"IK_{classify_bone_type(n)[1]}")]
            if ik_chain_bones:
                tip_ik_bone = None
                for ik_pb in ik_chain_bones:
                    has_child_in_chain = any(child.parent == ik_pb for child in ik_chain_bones)
                    if not has_child_in_chain:
                        tip_ik_bone = ik_pb
                        break
                if not tip_ik_bone:
                    tip_ik_bone = ik_chain_bones[-1]

                for constraint in list(tip_ik_bone.constraints):
                    if constraint.type in ("IK", "SPLINE_IK"):
                        tip_ik_bone.constraints.remove(constraint)

                if "spine" in group_key.lower() or any(classify_bone_type(n)[0] == "DEFSIK" for n in defik_bone_names):
                    curve_name = f"Curve_spine{get_side_suffix(group_key)}"
                    curve_obj = context.scene.objects.get(curve_name)

                    if not curve_obj:
                        curve_data = bpy.data.curves.new(name=curve_name, type="CURVE")
                        curve_data.dimensions = "3D"
                        spline = curve_data.splines.new("BEZIER")
                        spline.bezier_points.add(1)

                        base_def = pose_bones.get(defik_bone_names[0])
                        tip_def = pose_bones.get(defik_bone_names[-1])

                        # Transform bone head/tail coordinates using armature matrix_world
                        head_w = arm_obj.matrix_world @ base_def.head
                        tail_w = arm_obj.matrix_world @ tip_def.tail
                        dir_w = tail_w - head_w

                        spline.bezier_points[0].co = head_w
                        spline.bezier_points[0].handle_left = head_w - dir_w * 0.25
                        spline.bezier_points[0].handle_right = head_w + dir_w * 0.25

                        spline.bezier_points[1].co = tail_w
                        spline.bezier_points[1].handle_left = tail_w - dir_w * 0.25
                        spline.bezier_points[1].handle_right = tail_w + dir_w * 0.25

                        curve_obj = bpy.data.objects.new(curve_name, curve_data)
                        context.scene.collection.objects.link(curve_obj)

                    spline_ik = tip_ik_bone.constraints.new(type="SPLINE_IK")
                    spline_ik.target = curve_obj
                    spline_ik.chain_count = len(ik_chain_bones)
                    spline_ik.use_curve_radius = False
                    spline_ik.y_scale_mode = "NONE"

                    start_ctrl = pose_bones.get(f"CTRL_{group_key}_start") or pose_bones.get("CTRL_spine_start") or pose_bones.get("CTRL_waist") or pose_bones.get("ROOT")
                    end_ctrl = pose_bones.get(f"CTRL_{group_key}_end") or pose_bones.get("CTRL_spine_end") or ctrl_pbone

                    if start_ctrl and end_ctrl and curve_obj:
                        hook_b = curve_obj.modifiers.new(name="Hook_Start", type="HOOK")
                        hook_b.object = arm_obj
                        hook_b.subtarget = start_ctrl.name
                        hook_b.vertex_indices_set([0])
                        hook_b.matrix_inverse = (arm_obj.matrix_world @ start_ctrl.matrix).inverted()

                        hook_t = curve_obj.modifiers.new(name="Hook_End", type="HOOK")
                        hook_t.object = arm_obj
                        hook_t.subtarget = end_ctrl.name
                        hook_t.vertex_indices_set([1])
                        hook_t.matrix_inverse = (arm_obj.matrix_world @ end_ctrl.matrix).inverted()

                    print(f"[MechAnim] Created Spline IK on '{arm_obj.name}' for '{group_key}' -> Curve: '{curve_obj.name}'.")
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

            for defik_name in defik_bone_names:
                b_type, base_name = classify_bone_type(defik_name)
                defik_pbone = pose_bones.get(defik_name)
                fk_name = f"FK_{base_name}"
                ik_name = f"IK_{base_name}"
                fk_pbone = pose_bones.get(fk_name)
                ik_pbone = pose_bones.get(ik_name)

                if not defik_pbone:
                    continue

                if coll_def:
                    assign_bone_to_collection(arm_data, coll_def, defik_name)
                if coll_fk and fk_pbone:
                    assign_bone_to_collection(arm_data, coll_fk, fk_name)
                if coll_ik and ik_pbone:
                    assign_bone_to_collection(arm_data, coll_ik, ik_name)

                for constraint in list(defik_pbone.constraints):
                    if constraint.name in ("MechAnim_Copy_FK", "MechAnim_Copy_IK"):
                        try:
                            arm_obj.driver_remove(f'pose.bones["{defik_name}"].constraints["{constraint.name}"].influence')
                        except Exception:
                            pass
                        defik_pbone.constraints.remove(constraint)

                c_fk = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
                c_fk.name = "MechAnim_Copy_FK"
                c_fk.target = arm_obj
                c_fk.subtarget = fk_name
                c_fk.influence = 1.0

                c_ik = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
                c_ik.name = "MechAnim_Copy_IK"
                c_ik.target = arm_obj
                c_ik.subtarget = ik_name
                c_ik.influence = 0.0

                switch_ctrl_name = ctrl_name
                if "spine" in group_key.lower():
                    if f"CTRL_{group_key}_end" in pose_bones:
                        switch_ctrl_name = f"CTRL_{group_key}_end"
                    elif "CTRL_spine_end" in pose_bones:
                        switch_ctrl_name = "CTRL_spine_end"
                    elif "CTRL_spine" in pose_bones:
                        switch_ctrl_name = "CTRL_spine"

                switch_pbone = pose_bones.get(switch_ctrl_name)
                if switch_pbone:
                    data_path_ik = f'pose.bones["{defik_name}"].constraints["{c_ik.name}"].influence'
                    dfc_ik = arm_obj.driver_add(data_path_ik)
                    if dfc_ik:
                        d_ik = dfc_ik.driver
                        d_ik.type = "SCRIPTED"
                        for var in list(d_ik.variables):
                            d_ik.variables.remove(var)
                        var_ik = d_ik.variables.new()
                        var_ik.name = "sw"
                        var_ik.type = "SINGLE_PROP"
                        t_ik = var_ik.targets[0]
                        t_ik.id_type = "OBJECT"
                        t_ik.id = arm_obj
                        t_ik.data_path = f'pose.bones["{switch_ctrl_name}"]["FK_IK_Switch"]'
                        d_ik.expression = "sw"

                    data_path_fk = f'pose.bones["{defik_name}"].constraints["{c_fk.name}"].influence'
                    dfc_fk = arm_obj.driver_add(data_path_fk)
                    if dfc_fk:
                        d_fk = dfc_fk.driver
                        d_fk.type = "SCRIPTED"
                        for var in list(d_fk.variables):
                            d_fk.variables.remove(var)
                        var_fk = d_fk.variables.new()
                        var_fk.name = "sw"
                        var_fk.type = "SINGLE_PROP"
                        t_fk = var_fk.targets[0]
                        t_fk.id_type = "OBJECT"
                        t_fk.id = arm_obj
                        t_fk.data_path = f'pose.bones["{switch_ctrl_name}"]["FK_IK_Switch"]'
                        d_fk.expression = "1.0 - sw"

        # Restore original mode
        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report(
            {"INFO"},
            f"MechAnim: Generated connected FK & IK chains for active armature '{arm_obj.name}'.",
        )
        return {"FINISHED"}


class MECHANIM_OT_clear_fk_ik_chains(bpy.types.Operator):
    """Deletes all generated FK_ and IK_ bones, removes constraints/drivers, custom properties, and clears bone collections."""

    bl_idname = "mechanim.clear_fk_ik_chains"
    bl_label = "Clear FK/IK Chains & Collections"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Removes all generated FK_ and IK_ bones, drivers, constraints, CTRL properties, and bone collections safely."""
        armature_objs = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if not armature_objs:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        arm_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else armature_objs[0]
        arm_data = arm_obj.data

        # 1. Clean up Pose Mode constraints, drivers, and custom properties on bones
        prev_mode = context.mode
        context.view_layer.objects.active = arm_obj
        bpy.ops.object.mode_set(mode="POSE")

        for pbone in arm_obj.pose.bones:
            # Safely remove drivers and constraints
            for constraint in list(pbone.constraints):
                if constraint.name in ("MechAnim_Copy_FK", "MechAnim_Copy_IK") or constraint.type == "IK":
                    try:
                        arm_obj.driver_remove(f'pose.bones["{pbone.name}"].constraints["{constraint.name}"].influence')
                    except Exception:
                        pass
                    pbone.constraints.remove(constraint)

            # Remove custom property 'FK_IK_Switch' from CTRL bones
            if "FK_IK_Switch" in pbone.keys():
                try:
                    del pbone["FK_IK_Switch"]
                except Exception:
                    pass

        # 2. Delete all FK_ and IK_ bones in Edit Mode
        bpy.ops.object.mode_set(mode="EDIT")
        edit_bones = arm_data.edit_bones

        deleted_bones_count = 0
        bones_to_delete = [b.name for b in edit_bones if b.name.startswith("FK_") or b.name.startswith("IK_")]
        
        for b_name in bones_to_delete:
            ebone = edit_bones.get(b_name)
            if ebone:
                edit_bones.remove(ebone)
                deleted_bones_count += 1

        # 3. Remove all generated Bone Collections (DEF_*, FK_*, IK_*)
        if hasattr(arm_data, "collections"):
            colls_to_remove = [c.name for c in arm_data.collections if c.name.startswith("DEF_") or c.name.startswith("FK_") or c.name.startswith("IK_") or c.name in ("DEF", "FK", "IK")]
            for c_name in colls_to_remove:
                coll = arm_data.collections.get(c_name)
                if coll:
                    arm_data.collections.remove(coll)

        # 4. Remove all generated Spline IK Curves (Curve_*) and purge curve datablocks
        curves_to_remove = [obj for obj in context.scene.objects if obj.type == "CURVE" and obj.name.startswith("Curve_")]
        for curve_obj in curves_to_remove:
            c_name = curve_obj.name
            curve_data = curve_obj.data
            bpy.data.objects.remove(curve_obj, do_unlink=True)
            if curve_data and curve_data.users == 0:
                bpy.data.curves.remove(curve_data)
            print(f"[MechAnim] Deleted Spline IK Curve '{c_name}'.")

        # 5. Remove any Empty objects generated by Hook modifiers or IK targets (e.g. Empty, Hook-*, etc.)
        empties_to_remove = [obj for obj in context.scene.objects if obj.type == "EMPTY" and (obj.name.startswith("Hook") or obj.name.startswith("Empty") or obj.name.startswith("Curve_"))]
        for empty_obj in empties_to_remove:
            e_name = empty_obj.name
            bpy.data.objects.remove(empty_obj, do_unlink=True)
            print(f"[MechAnim] Deleted Empty Object '{e_name}'.")

        # Restore original mode
        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report({"INFO"}, f"MechAnim: Cleared {deleted_bones_count} generated FK/IK bone(s), curves, drivers, properties & collections.")
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
