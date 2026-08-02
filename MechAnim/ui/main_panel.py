"""
Module Path: MechAnim/ui/main_panel.py
System Responsibility: 3D Viewport Sidebar (N-Panel) interface for MechAnim. Split into clean, single-purpose helper functions under 50 lines.
Build Dependencies: bpy, addon_utils
"""

import bpy
import addon_utils


def deferred_reload() -> None:
    """Executes addon reload deferred on next main loop frame to prevent C-level UI button crash."""
    addon_name = "MechAnim"
    print("[MechAnim] Deferred reloader executing...")
    addon_utils.disable(addon_name, default_set=False)
    addon_utils.enable(addon_name, default_set=False)


class MECHANIM_OT_reload_addon(bpy.types.Operator):
    """Operator to live-reload MechAnim scripts and modules."""

    bl_idname = "mechanim.reload_addon"
    bl_label = "Reload MechAnim Addon"
    bl_options = {"REGISTER"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Schedules deferred reload on Blender timer to safely exit current operator call stack."""
        bpy.app.timers.register(deferred_reload, first_interval=0.01)
        return {"FINISHED"}


def draw_viewport_display_section(layout: bpy.types.UILayout, arm_obj: bpy.types.Object | None) -> None:
    """Draws viewport bone display accordion controls."""
    box_disp = layout.box()
    box_disp.label(text="Viewport Bone Display", icon="HIDE_OFF")
    if arm_obj and arm_obj.data:
        arm_data = arm_obj.data
        col_disp = box_disp.column(align=True)
        col_disp.prop(arm_data, "show_names", text="Show Bone Names", toggle=True, icon="FONT_DATA")
        col_disp.prop(arm_data, "show_axes", text="Show Bone Axes", toggle=True, icon="ORIENTATION_GLOBAL")
        col_disp.prop(arm_obj, "show_in_front", text="In Front (X-Ray)", toggle=True, icon="XRAY")
    else:
        box_disp.label(text="No Armature Selected", icon="INFO")


def draw_bone_shapes_and_colors_section(layout: bpy.types.UILayout, scene: bpy.types.Scene) -> None:
    """Draws custom bone shapes, 3-color palette, and rotation mode controls."""
    box = layout.box()
    box.label(text="Bone Custom Shapes & Colors", icon="BONE_DATA")
    
    col_cs = box.column(align=True)
    col_cs.label(text="Custom Shape Widgets:")
    for role in ("ctrl", "pole", "def", "fk", "ik"):
        col_cs.prop(scene, f"mechanim_shape_{role}", text=role.upper())
    box.operator("mechanim.assign_custom_shapes", text="Apply Custom Shapes", icon="MESH_DATA")

    box_clr = box.box()
    box_clr.label(text="Chain Viewport 3-Color Palette", icon="COLOR")
    grid_hdr = box_clr.grid_flow(columns=4, align=True)
    grid_hdr.label(text="Type")
    grid_hdr.label(text="Normal")
    grid_hdr.label(text="Select")
    grid_hdr.label(text="Active")

    for cat_label in ("CTRL", "POLE", "DEF", "FK", "IK"):
        row_c = box_clr.row(align=True)
        row_c.label(text=cat_label)
        prop_prefix = f"mechanim_color_{cat_label.lower()}"
        row_c.prop(scene, f"{prop_prefix}_normal", text="")
        row_c.prop(scene, f"{prop_prefix}_select", text="")
        row_c.prop(scene, f"{prop_prefix}_active", text="")

    box.operator("mechanim.apply_bone_colors", text="Apply 3-Color Palette", icon="COLOR")
    
    col = box.column(align=True)
    col.label(text="Set Rotation Mode (Selected Bones):")
    grid = col.grid_flow(columns=2, align=True)
    for mode in ("XYZ", "QUATERNION", "ZXY", "YXZ"):
        op = grid.operator("mechanim.set_rotation_mode", text=f"Euler {mode}" if mode != "QUATERNION" else "Quaternion")
        if op:
            op.target_mode = mode


def draw_fk_ik_tools_section(layout: bpy.types.UILayout, scene: bpy.types.Scene, arm_obj: bpy.types.Object | None) -> None:
    """Draws FK/IK generation, spline settings, switch sliders, and snapping controls."""
    box = layout.box()
    box.label(text="FK / IK Tools", icon="POSE_HLT")
    box.prop(scene, "mechanim_spline_order", text="Spline Order")
    box.prop(scene, "mechanim_spline_y_scale_mode", text="Spline Y-Scale Mode")
    
    row_gen = box.row(align=True)
    row_gen.operator("mechanim.generate_fk_ik_chains", text="Generate FK/IK Chains", icon="ARMATURE_DATA")
    row_gen.operator("mechanim.clear_fk_ik_chains", text="Clear Chains", icon="TRASH")

    if arm_obj and arm_obj.pose:
        switch_pbones = [pb for pb in arm_obj.pose.bones if "FK_IK_Switch" in pb.keys()]
        if switch_pbones:
            box_switches = box.box()
            box_switches.label(text="FK / IK Switch Sliders", icon="DRIVER")
            col = box_switches.column(align=True)
            for pb in switch_pbones:
                col.prop(pb, '["FK_IK_Switch"]', text=f"{pb.name}", slider=True)

    row = box.row(align=True)
    row.operator("mechanim.fk_to_ik", text="Snap FK -> IK", icon="SNAP_ON")
    row.operator("mechanim.ik_to_fk", text="Snap IK -> FK", icon="SNAP_ON")


def draw_mesh_and_rigging_section(layout: bpy.types.UILayout) -> None:
    """Draws auto-parenting, piston setup, and mesh mirroring section controls."""
    box_rig = layout.box()
    box_rig.label(text="Rigging Helpers", icon="ARMATURE_DATA")
    row = box_rig.row(align=True)
    row.operator("mechanim.auto_parent_meshes", text="Auto-Parent Meshes", icon="LINKED")
    row.operator("mechanim.unparent_all_meshes", text="Unparent All", icon="UNLINKED")
    box_rig.operator("mechanim.setup_piston", text="Setup Mechanical Piston", icon="CONSTRAINT")

    box_mesh = layout.box()
    box_mesh.label(text="Mesh Utilities", icon="MOD_MIRROR")
    box_mesh.operator("mechanim.mirror_mesh", text="Mirror Mech Mesh (_L/_R)", icon="MOD_MIRROR")


class VIEW3D_PT_mechanim_panel(bpy.types.Panel):
    """Main UI Panel for MechAnim in the 3D Viewport Sidebar."""

    bl_label = "MechAnim Tools"
    bl_idname = "VIEW3D_PT_mechanim_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MechAnim"

    def draw(self, context: bpy.types.Context) -> None:
        """Draws UI components using modular section drawer functions."""
        layout = self.layout
        scene = context.scene

        row = layout.row(align=True)
        row.operator("mechanim.reload_addon", text="Reload Addon", icon="FILE_REFRESH")
        row.operator("mechanim.inspect_scene", text="Inspect Scene", icon="INFO")

        active_obj = context.active_object
        arm_obj = active_obj if (active_obj and active_obj.type == "ARMATURE") else None
        if not arm_obj:
            armatures = [o for o in context.scene.objects if o.type == "ARMATURE"]
            if armatures:
                arm_obj = armatures[0]

        draw_viewport_display_section(layout, arm_obj)
        draw_bone_shapes_and_colors_section(layout, scene)
        draw_mesh_and_rigging_section(layout)
        draw_fk_ik_tools_section(layout, scene, arm_obj)


classes = (
    MECHANIM_OT_reload_addon,
    VIEW3D_PT_mechanim_panel,
)


def register() -> None:
    """Registers panel UI classes."""
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister() -> None:
    """Unregisters panel UI classes."""
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
