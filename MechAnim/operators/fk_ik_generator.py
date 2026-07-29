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
        armature_objs = [obj for obj in context.scene.objects if obj.type == "ARMATURE"]
        if not armature_objs:
            self.report({"WARNING"}, "No Armature object found in scene.")
            return {"CANCELLED"}

        arm_obj = context.active_object if (context.active_object and context.active_object.type == "ARMATURE") else armature_objs[0]
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
            self.report({"WARNING"}, "No DEFIK or DEFSIK bones found in armature.")
            return {"CANCELLED"}

        # Store mode to restore later and ensure active object context
        prev_mode = context.mode
        context.view_layer.objects.active = arm_obj
        bpy.ops.object.mode_set(mode="EDIT")
        edit_bones = arm_data.edit_bones

        created_fk_bones: list[str] = []
        created_ik_bones: list[str] = []

        # 2. Duplicate DEFIK bones in Edit Mode for FK_ and IK_ chains
        for group_key, defik_bone_names in defik_groups.items():
            for defik_name in defik_bone_names:
                b_type, base_name = classify_bone_type(defik_name)
                orig_ebone = edit_bones.get(defik_name)
                if not orig_ebone:
                    continue

                fk_name = f"FK_{base_name}"
                ik_name = f"IK_{base_name}"

                # Create FK bone if not exists
                if fk_name not in edit_bones:
                    fk_ebone = edit_bones.new(fk_name)
                    fk_ebone.head = orig_ebone.head.copy()
                    fk_ebone.tail = orig_ebone.tail.copy()
                    fk_ebone.roll = orig_ebone.roll
                    fk_ebone.use_deform = False
                    created_fk_bones.append(fk_name)

                # Create IK bone if not exists
                if ik_name not in edit_bones:
                    ik_ebone = edit_bones.new(ik_name)
                    ik_ebone.head = orig_ebone.head.copy()
                    ik_ebone.tail = orig_ebone.tail.copy()
                    ik_ebone.roll = orig_ebone.roll
                    ik_ebone.use_deform = False
                    created_ik_bones.append(ik_name)

        # 3. Setup connected parenting for new FK_ and IK_ chains & auto-create Spline IK CTRL bones
        for group_key, defik_bone_names in defik_groups.items():
            # If chain is DEFSIK spine, auto-create CTRL_spine_start and CTRL_spine_end if not present
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
                    print(f"[MechAnim] Auto-created '{c_start.name}' at spine base.")

                if last_bone and ("CTRL_spine_end" not in edit_bones and f"CTRL_{group_key}_end" not in edit_bones):
                    c_end = edit_bones.new(end_ctrl_name)
                    c_end.head = last_bone.tail.copy()
                    c_end.tail = last_bone.tail + (last_bone.tail - last_bone.head) * 0.5
                    c_end.use_deform = False
                    print(f"[MechAnim] Auto-created '{c_end.name}' at spine top.")

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

        processed_chains = 0

        for group_key, defik_bone_names in defik_groups.items():
            # Per-chain bone collections: DEF_<key>, FK_<key>, IK_<key>
            coll_def = get_or_create_collection(arm_data, f"DEF_{group_key}")
            coll_fk = get_or_create_collection(arm_data, f"FK_{group_key}")
            coll_ik = get_or_create_collection(arm_data, f"IK_{group_key}")

            ctrl_name = f"CTRL_{group_key}"
            ctrl_pbone = pose_bones.get(ctrl_name)
            pole_name = f"POLE_{group_key}"
            pole_pbone = pose_bones.get(pole_name)

            # Assign CTRL and POLE target bones to this chain's IK collection
            if ctrl_pbone and coll_ik:
                assign_bone_to_collection(arm_data, coll_ik, ctrl_name)
            if pole_pbone and coll_ik:
                assign_bone_to_collection(arm_data, coll_ik, pole_name)

            # Ensure custom property 'FK_IK_Switch' exists on CTRL bone (0.0 = FK, 1.0 = IK)
            if ctrl_pbone:
                if "FK_IK_Switch" not in ctrl_pbone:
                    ctrl_pbone["FK_IK_Switch"] = 0.0
                    
                ui_data = ctrl_pbone.id_properties_ui("FK_IK_Switch")
                ui_data.update(min=0.0, max=1.0, description="FK/IK Switch (0.0 = FK, 1.0 = IK)")

            # Add IK or Spline IK Constraint to the generated IK_ chain
            ik_chain_bones = [pose_bones.get(f"IK_{classify_bone_type(n)[1]}") for n in defik_bone_names if pose_bones.get(f"IK_{classify_bone_type(n)[1]}")]
            if ik_chain_bones:
                # Tip bone in IK chain (lowest child in hierarchy)
                tip_ik_bone = None
                for ik_pb in ik_chain_bones:
                    has_child_in_chain = any(child.parent == ik_pb for child in ik_chain_bones)
                    if not has_child_in_chain:
                        tip_ik_bone = ik_pb
                        break
                if not tip_ik_bone:
                    tip_ik_bone = ik_chain_bones[-1]

                # Clear existing IK / Spline IK constraints on tip IK_ bone
                for constraint in list(tip_ik_bone.constraints):
                    if constraint.type in ("IK", "SPLINE_IK"):
                        tip_ik_bone.constraints.remove(constraint)

                # Special Case: Spine Chain using Spline IK Curve
                if "spine" in group_key.lower():
                    curve_name = f"Curve_spine{get_side_suffix(group_key)}"
                    curve_obj = context.scene.objects.get(curve_name)

                    # Create Bézier Curve if not existing
                    if not curve_obj:
                        curve_data = bpy.data.curves.new(name=curve_name, type="CURVE")
                        curve_data.dimensions = "3D"
                        spline = curve_data.splines.new("BEZIER")
                        spline.bezier_points.add(1) # 2 points (base and top)

                        base_ik = ik_chain_bones[0]
                        spline.bezier_points[0].co = base_ik.head
                        spline.bezier_points[0].handle_left = base_ik.head - (base_ik.tail - base_ik.head) * 0.5
                        spline.bezier_points[0].handle_right = base_ik.head + (base_ik.tail - base_ik.head) * 0.5

                        spline.bezier_points[1].co = tip_ik_bone.tail
                        spline.bezier_points[1].handle_left = tip_ik_bone.tail - (tip_ik_bone.tail - tip_ik_bone.head) * 0.5
                        spline.bezier_points[1].handle_right = tip_ik_bone.tail + (tip_ik_bone.tail - tip_ik_bone.head) * 0.5

                        curve_obj = bpy.data.objects.new(curve_name, curve_data)
                        context.scene.collection.objects.link(curve_obj)

                    # Attach Spline IK Constraint to tip spine bone
                    spline_ik = tip_ik_bone.constraints.new(type="SPLINE_IK")
                    spline_ik.target = curve_obj
                    spline_ik.chain_count = len(ik_chain_bones)
                    spline_ik.use_curve_radius = False
                    spline_ik.y_scale_mode = "FIT_CURVE"

                    # Hook Curve end handles to CTRL_spine_start (Base) and CTRL_spine_end (Top)
                    start_ctrl = pose_bones.get(f"CTRL_{group_key}_start") or pose_bones.get("CTRL_spine_start") or pose_bones.get("CTRL_waist") or pose_bones.get("ROOT")
                    end_ctrl = pose_bones.get(f"CTRL_{group_key}_end") or pose_bones.get("CTRL_spine_end") or ctrl_pbone

                    if start_ctrl and end_ctrl and curve_obj:
                        # Ensure curve is in edit mode to hook points
                        bpy.ops.object.mode_set(mode="OBJECT")
                        bpy.ops.object.select_all(action="DESELECT")
                        curve_obj.select_set(True)
                        context.view_layer.objects.active = curve_obj
                        bpy.ops.object.mode_set(mode="EDIT")

                        # Add Hook modifier for base point directly targeting armature pose bone
                        hook_base = curve_obj.modifiers.new(name="Hook_Base", type="HOOK")
                        hook_base.object = arm_obj
                        hook_base.subtarget = start_ctrl.name

                        # Add Hook modifier for top point directly targeting armature pose bone
                        hook_top = curve_obj.modifiers.new(name="Hook_Top", type="HOOK")
                        hook_top.object = arm_obj
                        hook_top.subtarget = end_ctrl.name

                        # Switch back to active armature Pose Mode
                        bpy.ops.object.mode_set(mode="OBJECT")
                        bpy.ops.object.select_all(action="DESELECT")
                        arm_obj.select_set(True)
                        context.view_layer.objects.active = arm_obj
                        bpy.ops.object.mode_set(mode="POSE")

                    print(f"[MechAnim] Created Spline IK for '{group_key}' -> Curve: '{curve_obj.name}' with Top/Base Hooks.")
                else:
                    # Standard Limb IK (Arm / Leg)
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

                # Assign bones to their respective per-chain collections
                if coll_def:
                    assign_bone_to_collection(arm_data, coll_def, defik_name)
                if coll_fk and fk_pbone:
                    assign_bone_to_collection(arm_data, coll_fk, fk_name)
                if coll_ik and ik_pbone:
                    assign_bone_to_collection(arm_data, coll_ik, ik_name)

                # Remove existing Copy Transforms constraints safely
                for constraint in list(defik_pbone.constraints):
                    if constraint.name in ("MechAnim_Copy_FK", "MechAnim_Copy_IK"):
                        try:
                            arm_obj.driver_remove(f'pose.bones["{defik_name}"].constraints["{constraint.name}"].influence')
                        except Exception:
                            pass
                        defik_pbone.constraints.remove(constraint)

                # Constraint 1: Copy Transforms from FK bone (Influence = 1.0)
                c_fk = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
                c_fk.name = "MechAnim_Copy_FK"
                c_fk.target = arm_obj
                c_fk.subtarget = fk_name
                c_fk.influence = 1.0

                # Constraint 2: Copy Transforms from IK bone (Influence driven by FK_IK_Switch)
                c_ik = defik_pbone.constraints.new(type="COPY_TRANSFORMS")
                c_ik.name = "MechAnim_Copy_IK"
                c_ik.target = arm_obj
                c_ik.subtarget = ik_name
                c_ik.influence = 0.0

                # Create Driver on IK constraint influence cleanly
                if ctrl_pbone:
                    data_path = f'pose.bones["{defik_name}"].constraints["MechAnim_Copy_IK"].influence'
                    driver_fcurve = arm_obj.driver_add(data_path)
                    if driver_fcurve:
                        driver = driver_fcurve.driver
                        driver.type = "AVERAGE"
                        for var in list(driver.variables):
                            driver.variables.remove(var)

                        var = driver.variables.new()
                        var.name = "fk_ik_switch"
                        var.type = "SINGLE_PROP"
                        target = var.targets[0]
                        target.id_type = "OBJECT"
                        target.id = arm_obj
                        target.data_path = f'pose.bones["{ctrl_name}"]["FK_IK_Switch"]'

            processed_chains += 1

        # Restore original mode
        if prev_mode in ("EDIT", "POSE", "OBJECT"):
            bpy.ops.object.mode_set(mode=prev_mode)

        self.report(
            {"INFO"},
            f"MechAnim: Generated connected FK & IK chains and IK constraints for {processed_chains} group(s).",
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
