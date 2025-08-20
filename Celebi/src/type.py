import bpy
from mathutils import Vector, Matrix

from bpy.props import (
    FloatVectorProperty,
    CollectionProperty,
    StringProperty,
    BoolProperty,
    IntProperty,
    PointerProperty,
    EnumProperty,
)
from bpy.types import Object, PropertyGroup, WindowManager, Collection
from . import preview


class VoxelItem(PropertyGroup):
    name: StringProperty()
    selected: BoolProperty(default=False)


def voxel_name(base_name: str, loc: Vector) -> str:
    """
    Generate voxel-style name from base name and location vector.
    """
    x, y, z = int(loc.x), int(loc.y), int(loc.z)
    return f"voxel_{base_name}_{x}_{y}_{z}"


class TagItem(PropertyGroup):
    name: StringProperty(name="Tag Name")
    enabled: BoolProperty(default=False)


CONFIG_R90 = "R90"
CONFIG_R180 = "R180"
CONFIG_R270 = "R270"
CONFIG_SX = "SX"
CONFIG_SXR90 = "SXR90"
CONFIG_SXR180 = "SXR180"
CONFIG_SXR270 = "SXR270"

CONFIG_TRANSFORMS = {
    CONFIG_R90: Matrix.Rotation(1.5708, 4, "Z"),  # 90° around Z
    CONFIG_R180: Matrix.Rotation(3.1416, 4, "Z"),  # 180° around Z
    CONFIG_R270: Matrix.Rotation(4.7124, 4, "Z"),  # 270° around Z
    CONFIG_SX: Matrix.Scale(-1, 4, (1, 0, 0)),  # mirror X
    CONFIG_SXR90: Matrix.Scale(-1, 4, (1, 0, 0)) @ Matrix.Rotation(1.5708, 4, "Z"),
    CONFIG_SXR180: Matrix.Scale(-1, 4, (1, 0, 0)) @ Matrix.Rotation(3.1416, 4, "Z"),
    CONFIG_SXR270: Matrix.Scale(-1, 4, (1, 0, 0)) @ Matrix.Rotation(4.7124, 4, "Z"),
}


GIZMO_DISABLED = "DISABLED"
GIZMO_MOVE = "MOVE"
GIZMO_SCALE = "SCALE"


FACE_COLLECTIONS = [
    "face_front",
    "face_back",
    "face_left",
    "face_right",
    "face_top",
    "face_bottom",
]


def cube_configurations(self, context):
    return [
        (CONFIG_R90, "R90", "Rotate 90°"),
        (CONFIG_R180, "R180", "Rotate 180°"),
        (CONFIG_R270, "R270", "Rotate 270°"),
        (CONFIG_SX, "SX", "Mirror"),
        (CONFIG_SXR90, "SXR90", "Mirror + Rotate 90°"),
        (CONFIG_SXR180, "SX180", "Mirror + Rotate 180°"),
        (CONFIG_SXR270, "SXR270", "Mirror + Rotate 270°"),
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

    hash_front_lr: IntProperty()
    hash_front_rl: IntProperty()
    hash_back_lr: IntProperty()
    hash_back_rl: IntProperty()
    hash_left_lr: IntProperty()
    hash_left_rl: IntProperty()
    hash_right_lr: IntProperty()
    hash_right_rl: IntProperty()
    hash_top_lr: IntProperty()
    hash_top_rl: IntProperty()
    hash_bottom_lr: IntProperty()
    hash_bottom_rl: IntProperty()

    face_front: CollectionProperty(type=FaceEntry)
    face_back: CollectionProperty(type=FaceEntry)
    face_left: CollectionProperty(type=FaceEntry)
    face_right: CollectionProperty(type=FaceEntry)
    face_top: CollectionProperty(type=FaceEntry)
    face_bottom: CollectionProperty(type=FaceEntry)

    def getTags(self) -> list[TagItem]:
        return self.tags

    def getConfigs(self) -> list[ConfigItem]:
        return self.configs

    def appendTag(self, tag_name: str, enabled: bool):
        tag_entry = self.tags.add()
        tag_entry.name = tag_name
        tag_entry.enabled = enabled

    def appendConfig(self, identifier: str, enabled: bool):
        cfg = self.configs.add()
        cfg.name = identifier
        cfg.enabled = enabled


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
            (GIZMO_DISABLED, "Disabled", "Gizmo is disabled"),
            (GIZMO_SCALE, "Scale", "Gizmo is in scale mode"),
            (GIZMO_MOVE, "Move", "Gizmo is in move mode"),
        ],
        default=GIZMO_DISABLED,
    )

    def getCurrentLibraryItem(self) -> LibraryItem | None:
        if self.T_library_index >= 0 and self.T_library_index < len(self.library_items):
            return self.library_items[self.T_library_index]

    def updateCurrentActiveLibraryObject(self) -> Object | None:
        active = bpy.context.active_object
        self.T_library_index = -1
        for i, item in enumerate(self.getLibraryItems()):
            if item.obj == active:
                self.T_library_index = i
                return active

    def getLibraryItems(self) -> list[LibraryItem]:
        return self.library_items

    def clearLibraryItems(self):
        self.library_items.clear()

    def addLibraryItem(
        self, name: str | None, enabled: bool | None, obj: Object | None
    ) -> LibraryItem:
        cLib = self.getLibraryItems()
        item = cLib.add()
        if name != None:
            item.name = name
        if enabled != None:
            item.enabled = enabled
        if obj != None:
            item.obj = obj
        return item

    def getLibraryTags(self) -> list[TagItem]:
        return self.library_tags

    def getVoxelAt(self, index: int) -> VoxelItem | None:
        if index >= 0 and index < len(self.voxels):
            return self.voxels[index]

    def getCurrentVoxel(self) -> VoxelItem | None:
        return self.getVoxelAt(self.T_voxels_index)

    def getVoxels(self) -> list[VoxelItem]:
        return self.voxels

    def clearVoxels(self) -> list[VoxelItem]:
        self.voxels.clear()
        return self.voxels

    def deleteVoxelByName(self, name: str):
        items = self.voxels
        for i, item in enumerate(items):
            if item.name == name:
                items.remove(i)
                break

    def appendVoxel(self, name: str):
        voxel_item = self.voxels.add()
        voxel_item.name = name

    def appendTag(self, tag_name):
        cLibTags = self.getLibraryTags()
        new_tag_entry = cLibTags.add()
        new_tag_entry.name = tag_name

    def clearTags(self):
        cLibTags = self.getLibraryTags()
        cLibTags.clear()


def celebi() -> CelebiData:
    wm = bpy.context.window_manager
    if not hasattr(wm, "celebi_data"):
        raise RuntimeError("CelebiData not initialized. Register the addon first.")
    return wm.celebi_data


def getVoxelCollection():
    coll = bpy.data.collections.get("VOXELS")
    if coll is None:
        coll = bpy.data.collections.new("VOXELS")
        bpy.context.scene.collection.children.link(coll)
    return coll


def getConfigCollection():
    coll = bpy.data.collections.get("Config")
    if coll is None:
        coll = bpy.data.collections.new("Config")
        bpy.context.scene.collection.children.link(coll)
    return coll


def objLinkCollection(obj: Object):
    getVoxelCollection().objects.link(obj)


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
