import bpy
import bmesh
from mathutils import Matrix, Vector
from math import radians
from . import type

# The 7 non-identity configs (D4 group in XY plane)
R90 = Matrix.Rotation(radians(90), 4, "Z")
R180 = Matrix.Rotation(radians(180), 4, "Z")
R270 = Matrix.Rotation(radians(270), 4, "Z")
# SX = Matrix.Diagonal((-1, 1, 1, 1))  # Mirror in X
SX = Matrix.Scale(-1, 4, Vector((1, 0, 0)))
SXR90 = SX @ R90
SXR180 = SX @ R180
SXR270 = SX @ R270

CONFIGS = [
    ("R90", R90),
    ("R180", R180),
    ("R270", R270),
    ("SX", SX),
    ("SXR90", SXR90),
    ("SXR180", SXR180),
    ("SXR270", SXR270),
]


def to_signed32(x: int) -> int:
    x &= 0xFFFFFFFF  # keep only 32 bits
    if x & 0x80000000:  # if sign bit is set
        return -((~x & 0xFFFFFFFF) + 1)
    return x


DIRECTIONS = ["BL_TR", "BR_TL", "TR_BL", "TL_BR"]


def face_vertex_sort_key(v, normal_axis, sign, direction):
    if normal_axis == "X":  # YZ plane
        if sign > 0:
            if direction == "BL_TR":
                return (v.y, v.z)
            elif direction == "BR_TL":
                return (-v.y, v.z)
        else:
            if direction == "BL_TR":
                return (-v.y, v.z)
            elif direction == "BR_TL":
                return (v.y, v.z)

    elif normal_axis == "Y":  # XZ plane
        if sign > 0:
            if direction == "BL_TR":
                return (-v.x, v.z)
            elif direction == "BR_TL":
                return (v.x, v.z)
        else:
            if direction == "BL_TR":
                return (v.x, v.z)
            elif direction == "BR_TL":
                return (-v.x, v.z)

    else:  # Z axis → XY plane
        if sign > 0:
            if direction == "BL_TR":
                return (v.x, v.y)
            elif direction == "BR_TL":
                return (-v.x, v.y)
        else:
            if direction == "BL_TR":
                return (v.x, -v.y)
            elif direction == "BR_TL":
                return (-v.x, -v.y)
    return 0


def plane_hash(
    obj: bpy.types.Object,
    normal_axis: str,
    sign=1,
    size=1.0,
    # direction="LR",
    direction="BL_TR",
    scale=100,
):
    """
    Compute hash for a voxel face on a given plane.

    sign: +1 (positive side) or -1 (negative side)
    size: voxel cube size
    direction: "LR" or "RL"
    """

    mesh = obj.data
    verts_local = [v.co for v in mesh.vertices]

    axis_index = "XYZ".index(normal_axis)
    boundary = sign * (size / 2)

    # collect only vertices that actually lie on this voxel boundary
    boundary_verts = [
        Vector((round(v.x * scale), round(v.y * scale), round(v.z * scale)))
        for v in verts_local
        if abs(v[axis_index] - boundary) < 1e-6
    ]

    if not boundary_verts:
        # no face touching this plane → air
        return 0

    sorted_verts = sorted(
        boundary_verts,
        key=lambda v: face_vertex_sort_key(v, normal_axis, sign, direction),
    )

    mat_color_cache = {}
    mat_color_int_cache = {}
    hash_value = 0
    for vert in sorted_verts:
        # Only include the two in-plane coordinates for the hash
        if normal_axis == "X":
            sig = int(vert.y) ^ int(vert.z)
        elif normal_axis == "Y":
            sig = int(vert.x) ^ int(vert.z)
        else:
            sig = int(vert.x) ^ int(vert.y)

        # Pick material color from one polygon that uses this vertex
        mat_color = (1, 1, 1)
        for polygon in mesh.polygons:
            if any(
                mesh.vertices[polyVertIdx].co == vert
                for polyVertIdx in polygon.vertices
            ):
                mat_index = polygon.material_index
                if mat_index in mat_color_cache:
                    mat_color = mat_color_cache[mat_index]
                else:
                    if obj.material_slots and mat_index < len(obj.material_slots):
                        mat = obj.material_slots[mat_index].material
                        if mat:
                            mat_color = tuple(mat.diffuse_color[:3])
                    mat_color_cache[mat_index] = mat_color
                break

        if mat_color not in mat_color_int_cache:
            mat_color_int_cache[mat_color] = (
                (int(mat_color[0] * 255) << 16)
                + (int(mat_color[1] * 255) << 8)
                + int(mat_color[2] * 255)
            )

        # print(f"{sigx},{sigy},{sigz},  {sig}, {mat_color_int_cache[mat_color]}")

        sig ^= mat_color_int_cache[mat_color]
        hash_value = (
            hash_value * 31 + sig
        ) & 0xFFFFFFFFFFFFFFFF  # rolling hash with 64-bit mask

    hash = to_signed32((hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF))

    # print(f"{obj.name} {normal_axis} {sign} {direction}  {hash}")

    return hash


class VOXEL_OT_face_hash(bpy.types.Operator):
    bl_idname = "voxel.face_hash"
    bl_label = "Compute Voxel Face Hashes"

    def execute(self, context):
        c = type.celebi()
        lib = c.getLibraryItems()

        type.clearConfigCollection()
        c.purgeLibrary()
        c.clearHashes()

        print("\n" * 50)

        for item in lib:
            calc_linked_configs(item)

        bpy.ops.object.select_all(action="DESELECT")

        return {"FINISHED"}


def calc_linked_configs(oriItem: type.LibraryItem):
    """Spawn 7 linked duplicates with object-level transforms."""

    base_obj = oriItem.obj
    if base_obj == None:
        return

    calcHashes(oriItem, oriItem.obj, None, None, None, None)

    ConfigCollection = type.getConfigCollection()

    for i, (label, mat) in enumerate(CONFIGS, start=1):
        temp_mesh_obj = spawn_temp(base_obj, ConfigCollection, mat, i * 2)

        calcHashes(None, temp_mesh_obj, base_obj, mat, label, i * 2)

        ConfigCollection.objects.unlink(temp_mesh_obj)
        bpy.data.objects.remove(temp_mesh_obj, do_unlink=True)


def calcHashes(
    libItem: type.LibraryItem | None, temp_mesh_obj, base_obj, mat, label, y_offset
):
    hash_NX = plane_hash(temp_mesh_obj, normal_axis="X", sign=-1, direction="BR_TL")
    hash_PX = plane_hash(temp_mesh_obj, normal_axis="X", sign=1, direction="BL_TR")

    hash_NY = plane_hash(temp_mesh_obj, normal_axis="Y", sign=-1, direction="BL_TR")
    hash_PY = plane_hash(temp_mesh_obj, normal_axis="Y", sign=1, direction="BR_TL")

    hash_NZ = plane_hash(temp_mesh_obj, normal_axis="Z", sign=-1, direction="BR_TL")
    hash_PZ = plane_hash(temp_mesh_obj, normal_axis="Z", sign=1, direction="BL_TR")

    c = type.celebi()

    if not libItem:
        objectHash = f"{hash_NX}_{hash_PX}_{hash_NY}_{hash_PY}_{hash_NZ}_{hash_PZ}"

        ConfigCollection = type.getConfigCollection()

        if c.hashExists(objectHash):
            return

        # Make a linked duplicate
        new_obj = base_obj.copy()
        new_obj.data = base_obj.data  # linked mesh
        ConfigCollection.objects.link(new_obj)

        # Apply object-level transform
        new_obj.matrix_world = base_obj.matrix_world @ mat

        # Offset along +Y to separate
        new_obj.location += Vector((0, y_offset, 0))

        # Name for clarity
        base_name = base_obj.name.split(".")[0]
        new_obj.name = f"{base_name}_{label}"

        libItem = c.addLibraryItem(obj=new_obj)

    libItem.hash_PX = hash_PX
    libItem.hash_NX = hash_NX
    libItem.hash_PY = hash_PY
    libItem.hash_NY = hash_NY
    libItem.hash_PZ = hash_PZ
    libItem.hash_NZ = hash_NZ
    c.addHash(libItem)

    if c.T_show_hash:
        add_text_at(
            libItem.obj.location + Vector((-1.5, 0, 0.5)),
            str(hash_NX),
            f"hash_{libItem.obj.name}_NX",
        )
        add_text_at(
            libItem.obj.location + Vector((0.7, 0, 0.5)),
            str(hash_PX),
            f"hash_{libItem.obj.name}_PX",
        )
        add_text_at(
            libItem.obj.location + Vector((-0.4, -0.8, 0.5)),
            str(hash_NY),
            f"hash_{libItem.obj.name}_NY",
        )
        add_text_at(
            libItem.obj.location + Vector((-0.4, 0.8, 0.5)),
            str(hash_PY),
            f"hash_{libItem.obj.name}_PY",
        )
        add_text_at(
            libItem.obj.location + Vector((0.8, -0.8, -0.5)),
            str(hash_NZ),
            f"hash_{libItem.obj.name}_NZ",
        )
        add_text_at(
            libItem.obj.location + Vector((-1.5, 0.8, 0.5)),
            str(hash_PZ),
            f"hash_{libItem.obj.name}_PZ",
        )


def apply_transform_to_mesh(obj: bpy.types.Object, matrix: Matrix):
    """Apply a transform matrix to mesh geometry in-place using bmesh."""
    if obj.type != "MESH":
        return

    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)

    bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)

    bm.to_mesh(mesh)
    bm.free()

    # Reset object matrix so it stays at intended location
    obj.matrix_world = Matrix.Identity(4)


def spawn_temp(obj: bpy.types.Object, collection, mat, i: float) -> bpy.types.Object:
    new_obj = obj.copy()
    new_obj.data = obj.data.copy()  # give new mesh
    collection.objects.link(new_obj)

    # Keep same world position as original
    new_obj.matrix_world = obj.matrix_world

    # Apply transform to mesh data
    apply_transform_to_mesh(new_obj, mat)

    # Move them along Y so they don't overlap
    new_obj.location = obj.location + Vector((2, i, 0))
    new_obj.name = obj.name + "_temp"

    return new_obj


def add_text_at(location, text, name):
    """Spawn a text object at given location for debugging."""
    # Remove existing one with same name
    if name in bpy.data.objects:
        bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)

    curve = bpy.data.curves.new(name, type="FONT")
    curve.body = text
    obj = bpy.data.objects.new(name, curve)
    obj.location = location

    ConfigCollection = type.getConfigCollection()
    ConfigCollection.objects.link(obj)
    obj.scale = (0.15, 0.15, 0.15)
    return obj
