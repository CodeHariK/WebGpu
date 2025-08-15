import bpy

from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)


class VoxelItem(bpy.types.PropertyGroup):
    name: StringProperty()
    selected: BoolProperty(default=False)


class TagItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(default=False)


class LibraryItem(bpy.types.PropertyGroup):
    name: StringProperty()
    obj: PointerProperty(type=bpy.types.Object)
    tags: CollectionProperty(type=TagItem)
    selected: BoolProperty(default=False)


class CelebiData(bpy.types.PropertyGroup):
    voxels: CollectionProperty(type=VoxelItem)

    library_items: CollectionProperty(type=LibraryItem)
    library_tags: CollectionProperty(type=TagItem)

    T_library_index: IntProperty(default=-1)
    T_new_tag_name: StringProperty()

    voxels_index: IntProperty(default=-1)
    T_current_voxel: StringProperty()
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
