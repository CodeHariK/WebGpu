import bpy
from mathutils import Vector

from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)
from . import preview


class VoxelItem(bpy.types.PropertyGroup):
    name: StringProperty()
    selected: BoolProperty(default=False)


def voxel_name(base_name: str, loc: Vector) -> str:
    """
    Generate voxel-style name from base name and location vector.
    """
    x, y, z = int(loc.x), int(loc.y), int(loc.z)
    return f"{base_name}_{x}_{y}_{z}"


class TagItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(default=False)


class LibraryItem(bpy.types.PropertyGroup):
    obj: PointerProperty(type=bpy.types.Object)
    tags: CollectionProperty(type=TagItem)
    selected: BoolProperty(default=False)


class CelebiData(bpy.types.PropertyGroup):
    voxels: CollectionProperty(type=VoxelItem)

    library_items: CollectionProperty(type=LibraryItem)
    library_tags: CollectionProperty(type=TagItem)

    T_library_index: IntProperty(default=-1)
    T_new_tag_name: StringProperty()

    T_voxels_index: IntProperty(default=-1)
    T_preview_voxels: CollectionProperty(type=VoxelItem)
    T_dim_x: bpy.props.IntProperty(
        name="Dim X", default=1, min=-5, max=5, update=preview.update_preview
    )
    T_dim_y: bpy.props.IntProperty(
        name="Dim Y", default=1, min=-5, max=5, update=preview.update_preview
    )
    T_dim_z: bpy.props.IntProperty(
        name="Dim Z", default=1, min=-5, max=5, update=preview.update_preview
    )

    T_voxel_hover_running: BoolProperty(default=False)


def celebi(context: bpy.context) -> CelebiData:
    wm = context.window_manager
    if not hasattr(wm, "celebi_data"):
        # Optionally create or raise
        raise RuntimeError("CelebiData not initialized. Register the addon first.")
    return wm.celebi_data


def register():
    bpy.utils.register_class(VoxelItem)
    bpy.utils.register_class(TagItem)
    bpy.utils.register_class(LibraryItem)
    bpy.utils.register_class(CelebiData)

    # Store one instance in WindowManager (shared across scenes)
    bpy.types.WindowManager.celebi_data = PointerProperty(type=CelebiData)


def unregister():
    del bpy.types.WindowManager.celebi_data

    bpy.utils.unregister_class(CelebiData)
    bpy.utils.unregister_class(LibraryItem)
    bpy.utils.unregister_class(TagItem)
    bpy.utils.unregister_class(VoxelItem)
