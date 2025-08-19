import bpy
import hashlib
from mathutils import Vector
from functools import cmp_to_key


def face_vertex_compare(v1, v2, normal_axis, sign):
    """
    Compare two vertices for sorting along a plane.
    Returns -1 if v1<v2, 1 if v1>v2, 0 if equal.
    """

    # Sorting rule based on plane orientation
    if sign > 0:
        if normal_axis == "X":
            # +YZ plane → sort by y
            primary1, primary2 = v1.y, v2.y
            secondary1, secondary2 = v1.z, v2.z
        elif normal_axis == "Y":
            # -XZ plane → sort by -x
            primary1, primary2 = -v1.x, -v2.x
            secondary1, secondary2 = v1.z, v2.z
        else:
            # +XY plane → sort by x
            primary1, primary2 = v1.x, v2.x
            secondary1, secondary2 = v1.y, v2.y
    else:
        if normal_axis == "X":
            # -YZ plane → sort by -y
            primary1, primary2 = -v1.y, -v2.y
            secondary1, secondary2 = v1.z, v2.z
        elif normal_axis == "Y":
            # +XZ plane → sort by x
            primary1, primary2 = v1.x, v2.x
            secondary1, secondary2 = v1.z, v2.z
        else:
            # X-Y plane → sort by x
            primary1, primary2 = v1.x, v2.x
            secondary1, secondary2 = -v1.y, -v2.y

    if primary1 < primary2:
        return -1
    elif primary1 > primary2:
        return 1
    else:
        # tie-breaker
        if secondary1 < secondary2:
            return -1
        elif secondary1 > secondary2:
            return 1
        else:
            return 0


def plane_hash(
    obj: bpy.types.Object,
    normal_axis: str,
    sign=1,
    size=1.0,
    direction="LR",
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
        return "0"

    sorted_verts = sorted(
        boundary_verts,
        key=cmp_to_key(lambda a, b: face_vertex_compare(a, b, normal_axis, sign)),
        reverse=(direction == "RL"),
    )

    mat_color_cache = {}
    mat_color_int_cache = {}
    hash_value = 0
    for vert in sorted_verts:
        # Only include the two in-plane coordinates for the hash
        if normal_axis == "X":
            # Plane is YZ, ignore x
            sigy = int(round(vert.y * scale)) * sign
            sigz = int(round(vert.z * scale))
            sig = sigy ^ sigz
            # print(f"x:{sigy}, z:{sigz}, {sig}")
        elif normal_axis == "Y":
            # Plane is XZ, ignore y
            sigx = int(round(vert.x * scale)) * (-sign)
            sigz = int(round(vert.z * scale))
            sig = sigx ^ sigz
            # print(f"x:{sigx}, z:{sigz}, {sig}")
        else:
            # Plane is XY, ignore z
            sigx = int(round(vert.x * scale)) * sign
            sigy = int(round(vert.y * scale))
            sig = sigx ^ sigy
            # print(f"x:{sigx}, z:{sigy}, {sig}")

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
        sig ^= mat_color_int_cache[mat_color]
        hash_value = (
            hash_value * 31 + sig
        ) & 0xFFFFFFFFFFFFFFFF  # rolling hash with 64-bit mask

    # print(mat_color_int_cache.values())

    return hashlib.sha256(str(hash_value).encode()).hexdigest()


class VOXEL_OT_face_hash(bpy.types.Operator):
    bl_idname = "voxel.face_hash"
    bl_label = "Compute Voxel Face Hashes"

    def execute(self, context):
        obj = context.active_object
        if obj is None:
            self.report({"ERROR"}, "No active object found")
            return {"CANCELLED"}

        for axis in "XYZ":
            for sign in (+1, -1):
                h_lr = plane_hash(obj, normal_axis=axis, sign=sign, direction="LR")
                h_rl = plane_hash(obj, normal_axis=axis, sign=sign, direction="RL")
                print(f"{axis}{'+' if sign > 0 else '-'}: LR={h_lr[:8]}, RL={h_rl[:8]}")

        return {"FINISHED"}
