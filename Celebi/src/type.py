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


GIZMO_DISABLED = "DISABLED"
GIZMO_MOVE = "MOVE"
GIZMO_SCALE = "SCALE"

HASH_METHOD_RAYCAST = "RAYCAST"
HASH_METHOD_VERTEX = "VERTEX"


class HashItem(bpy.types.PropertyGroup):
    value: bpy.props.StringProperty()


class LibraryItem(PropertyGroup):
    obj: PointerProperty(type=Object)

    hash_NX: IntProperty()
    hash_PX: IntProperty()
    hash_NY: IntProperty()
    hash_PY: IntProperty()
    hash_PZ: IntProperty()
    hash_NZ: IntProperty()

    def getObjHash(self):
        return f"{self.hash_NX}_{self.hash_PX}_{self.hash_NY}_{self.hash_PY}_{self.hash_NZ}_{self.hash_PZ}"

    def serialize(self):
        if not self.obj:
            return None
        return {
            "obj": self.obj.name,
            "hash_NX": self.hash_NX,
            "hash_PX": self.hash_PX,
            "hash_NY": self.hash_NY,
            "hash_PY": self.hash_PY,
            "hash_PZ": self.hash_PZ,
            "hash_NZ": self.hash_NZ,
        }

    def deserialize(self, clone):
        if not clone:
            return
        if clone["obj"] in bpy.data.objects:
            self.obj = bpy.data.objects[clone["obj"]]
        else:
            return
        self.hash_NX = clone["hash_NX"]
        self.hash_PX = clone["hash_PX"]
        self.hash_NY = clone["hash_NY"]
        self.hash_PY = clone["hash_PY"]
        self.hash_PZ = clone["hash_PZ"]
        self.hash_NZ = clone["hash_NZ"]


class CelebiData(PropertyGroup):
    voxels: CollectionProperty(type=VoxelItem)

    library_items: CollectionProperty(type=LibraryItem)

    hashes: bpy.props.CollectionProperty(type=HashItem)

    T_library_index: IntProperty(default=-1)

    T_show_hash: BoolProperty(default=False)

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

    T_hash_method: EnumProperty(
        name="Hashing Method",
        description="Method to use for computing face hashes",
        items=[
            (HASH_METHOD_RAYCAST, "Raycast", "Use raycasting on a grid. Slower but more robust."),
            (HASH_METHOD_VERTEX, "Vertex", "Use vertex positions. Faster but sensitive to topology."),
        ],
        default=HASH_METHOD_RAYCAST,
    )


    def getCurrentLibraryItem(self) -> LibraryItem | None:
        if self.T_library_index >= 0 and self.T_library_index < len(self.library_items):
            return self.library_items[self.T_library_index]

    def updateCurrentActiveLibraryObject(self) -> Object | None:
        active = bpy.context.active_object
        # self.T_library_index = -1
        for i, item in enumerate(self.getLibraryItems()):
            if item.obj == active:
                self.T_library_index = i
                return active

    def getLibraryItems(self) -> list[LibraryItem]:
        return self.library_items

    def clearLibraryItems(self):
        self.library_items.clear()

    def is_object_really_deleted(self, item: "LibraryItem") -> bool:
        """Return True if the object's data is gone for real."""
        if not item.obj:  # pointer cleared
            return True
        if item.obj.name not in bpy.data.objects:  # object removed from datablock
            return True
        if not item.obj.users_collection and not item.obj.users_scene:
            # exists in datablock but not linked anywhere
            return True
        return False

    def purgeLibrary(self):
        clones: list[dict] = [
            item.serialize()
            for item in self.library_items
            if not self.is_object_really_deleted(item) and item.serialize()
        ]

        self.library_items.clear()

        for clone in clones:
            new_item = self.library_items.add()
            new_item.deserialize(clone)

    def addLibraryItem(self, obj: Object | None) -> LibraryItem:
        cLib = self.getLibraryItems()
        item = cLib.add()
        if obj != None:
            item.obj = obj
        return item

    def hashExists(self, hash: str) -> bool:
        for hi in self.hashes:
            if hi.value == hash:
                return True
        return False

    def addHash(self, item: LibraryItem):
        hash = item.getObjHash()
        if not self.hashExists(hash):
            hi = self.hashes.add()
            hi.value = hash

    def clearHashes(self):
        self.hashes.clear()

    def getVoxelAt(self, index: int) -> VoxelItem | None:
        if index >= 0 and index < len(self.voxels):
            return self.voxels[index]

    def getCurrentVoxel(self) -> VoxelItem | None:
        return self.getVoxelAt(self.T_voxels_index)

    def getVoxels(self) -> list[VoxelItem]:
        return self.voxels

    def purgeVoxels(self):
        voxels_to_keep = [
            item.name for item in self.voxels if bpy.data.objects.get(item.name)
        ]
        self.voxels.clear()
        for name in voxels_to_keep:
            self.addVoxel(name)

    def deleteVoxelByName(self, name: str):
        items = self.voxels
        for i, item in enumerate(items):
            if item.name == name:
                items.remove(i)
                break

    def addVoxel(self, name: str):
        voxel_item = self.voxels.add()
        voxel_item.name = name


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
    coll = bpy.data.collections.get("CONFIG")
    if coll is None:
        coll = bpy.data.collections.new("CONFIG")
        bpy.context.scene.collection.children.link(coll)
    return coll


def clearConfigCollection():
    collection = getConfigCollection()
    if collection:
        objs = list(collection.objects)
        for obj in objs:
            for col in obj.users_collection:
                col.objects.unlink(obj)
            bpy.data.objects.remove(obj, do_unlink=True)


def objLinkCollection(obj: Object):
    getVoxelCollection().objects.link(obj)


classes = (VoxelItem, HashItem, LibraryItem, CelebiData)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    # Store one instance in WindowManager (shared across scenes)
    WindowManager.celebi_data = PointerProperty(type=CelebiData)


def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    del WindowManager.celebi_data
