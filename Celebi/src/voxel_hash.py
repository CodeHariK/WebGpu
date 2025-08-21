import bpy
import bmesh
from mathutils import Matrix, Vector
from math import radians
from . import type

# The 7 non-identity configs (D4 group in XY plane)
R90 = Matrix.Rotation(radians(90), 4, "Z")
R180 = Matrix.Rotation(radians(180), 4, "Z")
R270 = Matrix.Rotation(radians(270), 4, "Z")
SX = Matrix.Diagonal((-1, 1, 1, 1))  # Mirror in X
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
            # -----------------------
            if direction == "BL_TR":
                return (v.y, v.z)
            elif direction == "BR_TL":
                return (-v.y, v.z)
            elif direction == "TR_BL":
                return (-v.y, -v.z)
            elif direction == "TL_BR":
                return (v.y, -v.z)
        else:
            # -----------------------
            if direction == "BL_TR":
                return (-v.y, v.z)
            elif direction == "BR_TL":
                return (v.y, v.z)
            elif direction == "TR_BL":
                return (v.y, -v.z)
            elif direction == "TL_BR":
                return (-v.y, -v.z)

    elif normal_axis == "Y":  # XZ plane
        if sign > 0:
            # ----------------------
            if direction == "BL_TR":
                return (-v.x, v.z)
            elif direction == "BR_TL":
                return (v.x, v.z)
            elif direction == "TR_BL":
                return (v.x, -v.z)
            elif direction == "TL_BR":
                return (-v.x, -v.z)
        else:
            # ----------------------
            if direction == "BL_TR":
                return (v.x, v.z)
            elif direction == "BR_TL":
                return (-v.x, v.z)
            elif direction == "TR_BL":
                return (-v.x, -v.z)
            elif direction == "TL_BR":
                return (v.x, -v.z)

    else:  # Z axis → XY plane
        if sign > 0:
            # ----------------------
            if direction == "BL_TR":
                return (v.x, v.y)
            elif direction == "BR_TL":
                return (-v.x, v.y)
            elif direction == "TR_BL":
                return (-v.x, -v.y)
            elif direction == "TL_BR":
                return (v.x, -v.y)
        else:
            # ----------------------
            if direction == "BL_TR":
                return (v.x, -v.y)
            elif direction == "BR_TL":
                return (-v.x, -v.y)
            elif direction == "TR_BL":
                return (-v.x, v.y)
            elif direction == "TL_BR":
                return (v.x, v.y)

    return 0


def plane_hash(
    libItem: type.LibraryItem,
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
    boundary_verts = [v for v in verts_local if abs(v[axis_index] - boundary) < 1e-6]
    # boundary_verts = [v for v in verts_rel if abs(v[axis_index] - boundary) < 1e-6]

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
    sigx = 0
    sigy = 0
    sigz = 0
    for vert in sorted_verts:
        # Only include the two in-plane coordinates for the hash
        if normal_axis == "X":
            # Plane is YZ, ignore x
            sigy = int(round(vert.y * scale))
            # sigy = sign * int(round(vert.y * scale))
            sigz = int(round(vert.z * scale))
            sig = sigy ^ sigz
        elif normal_axis == "Y":
            # Plane is XZ, ignore y
            sigx = int(round(vert.x * scale))
            # sigx = -sign * int(round(vert.x * scale))
            sigz = int(round(vert.z * scale))
            sig = sigx ^ sigz
        else:
            # Plane is XY, ignore z
            sigx = int(round(vert.x * scale))
            sigy = int(round(vert.y * scale))
            # sigy = sign * int(round(vert.y * scale))
            sig = sigx ^ sigy

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

        print(f"{sigx},{sigy},{sigz},  {sig}, {mat_color_int_cache[mat_color]}")

        sig ^= mat_color_int_cache[mat_color]
        hash_value = (
            hash_value * 31 + sig
        ) & 0xFFFFFFFFFFFFFFFF  # rolling hash with 64-bit mask

    hash = to_signed32((hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF))

    print(f"{obj.name} {normal_axis} {sign} {direction}  {hash}")

    location = (
        libItem.obj.location.x + ((0.6 if sign > 0 else -1.0) if normal_axis == "X" else 0),
        libItem.obj.location.y + ((0.6 if sign > 0 else -0.7) if normal_axis == "Y" else 0),
        libItem.obj.location.z + ((0.7 if sign > 0 else -1.0) if normal_axis == "Z" else 0),
    )
    name = f"hash_{obj.name}_{normal_axis}_{sign}_{direction}"
    add_text_at(location, str(hash), name)


    if normal_axis == "X" and sign > 0:
        libItem.hash_PX = hash
    if normal_axis == "X" and sign < 0:
        libItem.hash_NX = hash

    if normal_axis == "Y" and sign > 0:
        libItem.hash_PY = hash
    if normal_axis == "Y" and sign < 0:
        libItem.hash_NY = hash

    if normal_axis == "Z" and sign > 0:
        libItem.hash_PZ = hash
    if normal_axis == "Z" and sign < 0:
        libItem.hash_NZ = hash

    return hash


class VOXEL_OT_face_hash(bpy.types.Operator):
    bl_idname = "voxel.face_hash"
    bl_label = "Compute Voxel Face Hashes"

    def execute(self, context):
        c = type.celebi()
        lib = c.getLibraryItems()

        type.clearConfigCollection()

        print("\n" * 50)

        for item in lib:
            obj = item.obj

            spawn_linked_configs(item)

        return {"FINISHED"}


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
    obj.scale = (0.08, 0.08, 0.08)  # make text small
    return obj


def spawn_linked_configs(oriItem: type.LibraryItem, y_offset=2.0):
    """Spawn 7 linked duplicates with object-level transforms."""

    ConfigCollection = type.getConfigCollection()
    c = type.celebi()

    base_obj = oriItem.obj
    if base_obj == None:
        return

    calcHashes(oriItem, oriItem.obj)

    for i, (label, mat) in enumerate(CONFIGS, start=1):
        # Make a linked duplicate
        new_obj = base_obj.copy()
        new_obj.data = base_obj.data  # linked mesh
        ConfigCollection.objects.link(new_obj)

        # Apply object-level transform
        new_obj.matrix_world = base_obj.matrix_world @ mat

        # Offset along +Y to separate
        new_obj.location += Vector((0, i * y_offset, 0))

        # Name for clarity
        base_name = base_obj.name.split(".")[0]
        new_obj.name = f"{base_name}_{label}"

        linked_lib_item = c.addLibraryItem(name=new_obj.name, enabled=False, obj=new_obj)

        temp_mesh_obj = spawn_configs(base_obj, ConfigCollection, mat, i * y_offset)

        calcHashes(linked_lib_item, temp_mesh_obj)


        ConfigCollection.objects.unlink(temp_mesh_obj)      # unlink from collection
        # bpy.data.objects.remove(temp_mesh_obj, do_unlink=True)  # remove completely

def calcHashes(linked_lib_item, mesh_obj):
    plane_hash(linked_lib_item,mesh_obj, normal_axis="X", sign=-1, direction="BR_TL")
    plane_hash(linked_lib_item, mesh_obj, normal_axis="X", sign=1, direction="BL_TR")

    plane_hash(linked_lib_item,mesh_obj, normal_axis="Y", sign=-1, direction="BL_TR")
    plane_hash(linked_lib_item,mesh_obj, normal_axis="Y", sign=1, direction="BR_TL")

    plane_hash(linked_lib_item,mesh_obj, normal_axis="Z", sign=1, direction="BL_TR")
    plane_hash(linked_lib_item,mesh_obj, normal_axis="Z", sign=-1, direction="BR_TL")


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


def spawn_configs(obj: bpy.types.Object, collection, mat, i: float) -> bpy.types.Object:
    new_obj = obj.copy()
    new_obj.data = obj.data.copy()  # give new mesh
    collection.objects.link(new_obj)

    # Keep same world position as original
    new_obj.matrix_world = obj.matrix_world

    # Apply transform to mesh data
    apply_transform_to_mesh(new_obj, mat)

    # Move them along Y so they don't overlap
    new_obj.location = obj.location + Vector((2, i, 0))

    return new_obj
