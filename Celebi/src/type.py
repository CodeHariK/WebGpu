import bpy
from mathutils import Vector

from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
    EnumProperty,
)
from bpy.types import Object, PropertyGroup, WindowManager
from . import preview


class VoxelItem(PropertyGroup):
    name: StringProperty()
    selected: BoolProperty(default=False)


def voxel_name(base_name: str, loc: Vector) -> str:
    """
    Generate voxel-style name from base name and location vector.
    """
    x, y, z = int(loc.x), int(loc.y), int(loc.z)
    return f"{base_name}_{x}_{y}_{z}"


class TagItem(PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(default=False)


def cube_configurations(self, context):
    return [
        ("R90", "R90", "Rotate 90°"),
        ("R180", "R180", "Rotate 180°"),
        ("R270", "R270", "Rotate 270°"),
        ("SX", "SX", "Mirror"),
        ("SXR90", "SXR90", "Mirror + Rotate 90°"),
        ("SX180", "SX180", "Mirror + Rotate 180°"),
        ("SXR270", "SXR270", "Mirror + Rotate 270°"),
    ]


class ConfigItem(PropertyGroup):
    name: EnumProperty(
        name="Config",
        description="Cube configuration",
        items=cube_configurations,
    )
    enabled: BoolProperty(default=False)


class FaceEntry(PropertyGroup):
    library_item: PointerProperty(type=PropertyGroup)
    config: EnumProperty(
        name="Config",
        description="Configuration of linked library item",
        items=cube_configurations,
    )


class LibraryItem(PropertyGroup):
    obj: PointerProperty(type=Object)
    tags: CollectionProperty(type=TagItem)
    selected: BoolProperty(default=False)

    # Configurations checkboxes for this library item
    configs: CollectionProperty(type=ConfigItem)

    face_front: CollectionProperty(type=FaceEntry)
    face_back: CollectionProperty(type=FaceEntry)
    face_left: CollectionProperty(type=FaceEntry)
    face_right: CollectionProperty(type=FaceEntry)
    face_top: CollectionProperty(type=FaceEntry)
    face_bottom: CollectionProperty(type=FaceEntry)


class CelebiData(PropertyGroup):
    voxels: CollectionProperty(type=VoxelItem)

    library_items: CollectionProperty(type=LibraryItem)
    library_tags: CollectionProperty(type=TagItem)

    T_library_index: IntProperty(default=-1)
    T_new_tag_name: StringProperty()

    T_voxels_index: IntProperty(default=-1)
    T_preview_voxels: CollectionProperty(type=VoxelItem)
    T_dim_x: IntProperty(
        name="Dim X", default=1, min=-5, max=5, update=preview.update_preview
    )
    T_dim_y: IntProperty(
        name="Dim Y", default=1, min=-5, max=5, update=preview.update_preview
    )
    T_dim_z: IntProperty(
        name="Dim Z", default=1, min=-5, max=5, update=preview.update_preview
    )

    T_voxel_hover_running: BoolProperty(default=False)
    
    T_voxel_gizmo_state: EnumProperty(
        name="Voxel Gizmo State",
        description="Current state of the voxel gizmo",
        items=[
            ('DISABLED', "Disabled", "Gizmo is disabled"),
            ('SCALE', "Scale", "Gizmo is in scale mode"),
            ('MOVE', "Move", "Gizmo is in move mode"),
        ],
        default='DISABLED'
    )

    def getCelebiLibraryItem(self) -> LibraryItem | None:
        if self.T_library_index >= 0 and self.T_library_index < len(self.library_items):
            return self.library_items[self.T_library_index]


def celebi() -> CelebiData:
    wm = bpy.context.window_manager
    if not hasattr(wm, "celebi_data"):
        # Optionally create or raise
        raise RuntimeError("CelebiData not initialized. Register the addon first.")
    return wm.celebi_data


classes = (VoxelItem, TagItem, ConfigItem, FaceEntry, LibraryItem, CelebiData)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    FaceEntry.library_item = PointerProperty(type=LibraryItem)

    # Store one instance in WindowManager (shared across scenes)
    WindowManager.celebi_data = PointerProperty(type=CelebiData)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    del WindowManager.celebi_data
