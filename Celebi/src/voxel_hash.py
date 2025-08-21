import bpy
from mathutils import Vector
from . import type


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
            sigy = abs(int(round(vert.y * scale)))
            # sigy = sign * int(round(vert.y * scale))
            sigz = abs(int(round(vert.z * scale)))
            sig = sigy ^ sigz
        elif normal_axis == "Y":
            # Plane is XZ, ignore y
            sigx = abs(int(round(vert.x * scale)))
            # sigx = -sign * int(round(vert.x * scale))
            sigz = abs(int(round(vert.z * scale)))
            sig = sigx ^ sigz
        else:
            # Plane is XY, ignore z
            sigx = abs(int(round(vert.x * scale)))
            sigy = abs(int(round(vert.y * scale)))
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

    # print(mat_color_int_cache.values())

    hash = to_signed32((hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF))

    print(f"{obj.name} {normal_axis} {sign} {direction}  {hash}")

    return hash


class VOXEL_OT_face_hash(bpy.types.Operator):
    bl_idname = "voxel.face_hash"
    bl_label = "Compute Voxel Face Hashes"

    def execute(self, context):
        c = type.celebi()
        lib = c.getLibraryItems()

        print("\n" * 50)

        for item in lib:
            obj = item.obj

            for normal_axis in [
                "X",
                "Y",
                # "Z"
            ]:
                for sign in [1, -1]:
                    hashes = ""

                    for direction in ["BL_TR", "BR_TL", "TR_BL", "TL_BR"]:
                        hash = plane_hash(
                            obj, normal_axis=normal_axis, sign=sign, direction=direction
                        )
                        hashes += direction + "   " + str(hash) + "\n"

                    location = (
                        obj.location.x
                        + ((0.7 if sign > 0 else -1.3) if normal_axis == "X" else 0),
                        obj.location.y
                        + ((0.9 if sign > 0 else -0.7) if normal_axis == "Y" else 0),
                        obj.location.z
                        + ((0.7 if sign > 0 else -1.3) if normal_axis == "Z" else 0),
                    )
                    name = f"hash_{obj.name}_{normal_axis}_{sign}_{direction}"
                    add_text_at(location, str(hashes), name)
                    print()

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
    bpy.context.collection.objects.link(obj)
    obj.scale = (0.08, 0.08, 0.08)  # make text small
    return obj
