import bpy

from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
)


class SpawnedCubeItem(bpy.types.PropertyGroup):
    name: StringProperty()


class TagItem(bpy.types.PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(default=False)


class LibraryItem(bpy.types.PropertyGroup):
    name: StringProperty()
    obj: PointerProperty(type=bpy.types.Object)
    tags: CollectionProperty(type=TagItem)


class CelebiData(bpy.types.PropertyGroup):
    spawned_cubes: CollectionProperty(type=SpawnedCubeItem)
    library_items: CollectionProperty(type=LibraryItem)

    library_tags: CollectionProperty(type=TagItem)
    library_index: IntProperty(default=-1)
    last_library_index: IntProperty(default=-1)

    new_tag_name: StringProperty()


def register():
    bpy.utils.register_class(SpawnedCubeItem)
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
    bpy.utils.unregister_class(SpawnedCubeItem)
