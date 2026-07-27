"""
Module Path: MechAnim/ui/main_panel.py
System Responsibility: 3D Viewport Sidebar (N-Panel) interface for MechAnim. Includes clean addon reloading.
Build Dependencies: bpy, addon_utils
"""

import bpy
import addon_utils


class MECHANIM_OT_reload_addon(bpy.types.Operator):
    """Operator to live-reload MechAnim scripts and modules."""

    bl_idname = "mechanim.reload_addon"
    bl_label = "Reload MechAnim Addon"
    bl_options = {"REGISTER"}

    def execute(self, context: bpy.types.Context) -> set[str]:
        """Reloads MechAnim addon via addon_utils."""
        addon_name = "MechAnim"
        
        # Safely disable and re-enable addon through Blender's manager
        addon_utils.disable(addon_name, default_set=False)
        addon_utils.enable(addon_name, default_set=False)

        self.report({"INFO"}, "MechAnim: Cleanly reloaded addon!")
        return {"FINISHED"}


class VIEW3D_PT_mechanim_panel(bpy.types.Panel):
    """Main UI Panel for MechAnim in the 3D Viewport Sidebar."""

    bl_label = "MechAnim Tools"
    bl_idname = "VIEW3D_PT_mechanim_panel"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MechAnim"

    def draw(self, context: bpy.types.Context) -> None:
        """Draws UI components inside the sidebar panel."""
        layout = self.layout

        # Developer / Reload Helper
        row = layout.row(align=True)
        row.operator("mechanim.reload_addon", text="Reload Addon", icon="FILE_REFRESH")
        row.operator("mechanim.inspect_scene", text="Inspect Scene", icon="INFO")

        # Bone Batch Utilities Section
        box = layout.box()
        box.label(text="Bone Utilities", icon="BONE_DATA")
        col = box.column(align=True)
        col.label(text="Set Rotation Mode (Selected Bones):")
        grid = col.grid_flow(columns=2, align=True)
        
        op_xyz = grid.operator("mechanim.set_rotation_mode", text="Euler XYZ")
        if op_xyz:
            op_xyz.target_mode = "XYZ"
            
        op_quat = grid.operator("mechanim.set_rotation_mode", text="Quaternion")
        if op_quat:
            op_quat.target_mode = "QUATERNION"
            
        op_zxy = grid.operator("mechanim.set_rotation_mode", text="Euler ZXY")
        if op_zxy:
            op_zxy.target_mode = "ZXY"
            
        op_yxz = grid.operator("mechanim.set_rotation_mode", text="Euler YXZ")
        if op_yxz:
            op_yxz.target_mode = "YXZ"

        # Mech Rigging Section
        box = layout.box()
        box.label(text="Rigging Helpers", icon="ARMATURE_DATA")
        row = box.row(align=True)
        row.operator("mechanim.auto_parent_meshes", text="Auto-Parent Meshes", icon="LINKED")
        row.operator("mechanim.unparent_all_meshes", text="Unparent All", icon="UNLINKED")
        box.operator("mechanim.setup_piston", text="Setup Mechanical Piston", icon="CONSTRAINT")

        # FK/IK Snapping Section
        box = layout.box()
        box.label(text="FK / IK Tools", icon="POSE_HLT")
        box.operator("mechanim.fk_to_ik", text="Snap FK -> IK", icon="SNAP_ON")
        box.operator("mechanim.ik_to_fk", text="Snap IK -> FK", icon="SNAP_ON")

        # Mesh Utilities Section
        box = layout.box()
        box.label(text="Mesh Utilities", icon="MOD_MIRROR")
        box.operator("mechanim.mirror_mesh", text="Mirror Mech Mesh (_L/_R)", icon="MOD_MIRROR")


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
