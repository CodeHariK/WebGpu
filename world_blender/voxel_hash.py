import bpy
import bmesh
from math import radians
import time
from mathutils import Matrix, Vector

from . import type

from . import debug
logger = debug.logger

# Module-level queue used by the non-blocking timer worker (stores safe object names)
_hash_queue: list = []
_lib_index_map: dict = {}


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

_hash_to_id = {}
_next_id = 1


def get_compact_hash(h: int) -> int:
    global _next_id
    if h == 0:
        return 0

    if h not in _hash_to_id:
        _hash_to_id[h] = _next_id
        _next_id += 1

    return _hash_to_id[h]


def to_signed32(x: int) -> int:
    x &= 0xFFFFFFFF  # keep only 32 bits
    if x & 0x80000000:  # if sign bit is set
        return -((~x & 0xFFFFFFFF) + 1)
    return x


def face_vertex_sort_key(v, normal_axis, sign, direction):
    if normal_axis == "X":  # YZ plane
        return (v.y, v.z)
        # if sign > 0:
        #     if direction == "BL_TR":
        #         return (v.y, v.z)
        #     elif direction == "BR_TL":
        #         return (-v.y, v.z)
        # else:
        #     if direction == "BL_TR":
        #         return (-v.y, v.z)
        #     elif direction == "BR_TL":
        #         return (v.y, v.z)

    elif normal_axis == "Y":  # XZ plane
        return (v.x, v.z)
        # if sign > 0:
        #     if direction == "BL_TR":
        #         return (-v.x, v.z)
        #     elif direction == "BR_TL":
        #         return (v.x, v.z)
        # else:
        #     if direction == "BL_TR":
        #         return (v.x, v.z)
        #     elif direction == "BR_TL":
        #         return (-v.x, v.z)

    else:  # Z axis → XY plane
        return (v.x, v.y)
        # if sign > 0:
        #     if direction == "BL_TR":
        #         return (v.x, v.y)
        #     elif direction == "BR_TL":
        #         return (-v.x, v.y)
        # else:
        #     if direction == "BL_TR":
        #         return (v.x, -v.y)
        #     elif direction == "BR_TL":
        #         return (v.x, v.y)
    return 0


def raycast_hash(
    obj: bpy.types.Object,
    normal_axis: str,
    sign: int = 1,
    size=1.0,
    direction="BL_TR",
    grid_resolution=5,
):
    """
    Compute hash for a voxel face by raycasting on a grid.

    obj: The object to perform the raycast on.
    normal_axis: "X", "Y", or "Z".
    sign: +1 (positive side) or -1 (negative side).
    size: Voxel cube size.
    direction: The sorting order for the grid points (e.g., "BL_TR").
    grid_resolution: The number of points to sample along each axis of the face grid.
    """
    logger.debug("raycast_hash: obj=%s axis=%s sign=%d grid_res=%d", getattr(obj, 'name', str(obj)), normal_axis, sign, grid_resolution)

    axis_idx = "XYZ".index(normal_axis)
    plane_axes = [i for i in range(3) if i != axis_idx]
    u_axis, v_axis = plane_axes[0], plane_axes[1]

    hash_value = 0
    grid_points = []
    mat_colors = {}  # Cache for material colors as integer values

    # 1. Generate a grid of points on the plane
    if grid_resolution == 1:
        grid_points.append(Vector((0.0, 0.0, 0.0)))
    else:
        margin = 0.01
        sample_width = size - (margin * 2)
        start_coord = -size / 2 + margin
        step = sample_width / (grid_resolution - 1)
        for r in range(grid_resolution):
            for c in range(grid_resolution):
                u = start_coord + c * step
                v = start_coord + r * step
                point_on_plane = Vector((0.0, 0.0, 0.0))
                point_on_plane[u_axis] = u
                point_on_plane[v_axis] = v
                grid_points.append(point_on_plane)
    # # 2. Sort the grid points using the same logic as the vertex hash
    # sorted_grid_points = sorted(
    #     grid_points,
    #     key=lambda p: face_vertex_sort_key(p, normal_axis, sign, direction),
    # )

    # 3. Iterate through sorted points and perform raycast
    for point_on_plane in grid_points:
        ray_origin_list: list[float] = [0.0, 0.0, 0.0]
        ray_origin_list[axis_idx] = sign * size * 0.6
        ray_origin_list[u_axis] = point_on_plane[u_axis]
        ray_origin_list[v_axis] = point_on_plane[v_axis]

        local_ray_origin = Vector(ray_origin_list)

        ray_direction_list: list[float] = [0, 0, 0]
        ray_direction_list[axis_idx] = -sign
        local_ray_direction = Vector(ray_direction_list)

        hit, location, normal, face_index = obj.ray_cast(
            local_ray_origin, local_ray_direction, distance=0.11
        )

        mat_name = "air"
        color_int = 0  # Default for "air"
        if hit:
            mat_index = obj.data.polygons[face_index].material_index
            if mat_index in mat_colors:
                color_int = mat_colors[mat_index]
            elif obj.material_slots and mat_index < len(obj.material_slots):
                mat = obj.material_slots[mat_index].material
                if mat:
                    mat_name = mat.name
                    c = mat.diffuse_color
                    calculated_color = (
                        (int(c[0] * 255) << 16)
                        + (int(c[1] * 255) << 8)
                        + int(c[2] * 255)
                    )
                    mat_colors[mat_index] = calculated_color
                    color_int = calculated_color

        # print(
        #     # f"  - Point({r},{c}): "
        #     f"Origin({local_ray_origin.x:.2f}, {local_ray_origin.y:.2f}, {local_ray_origin.z:.2f}), "
        #     f"Dir({local_ray_direction.x:.2f}, {local_ray_direction.y:.2f}, {local_ray_direction.z:.2f}), "
        #     f"Hit='{mat_name}', ColorVal={color_int}"
        # )

        # Combine into a rolling hash
        hash_value = (hash_value * 31 + color_int) & 0xFFFFFFFFFFFFFFFF

    # Fold 64-bit hash into 32-bit and get a compact ID
    final_hash = to_signed32(
        (hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF)
    )

    compact = get_compact_hash(final_hash)
    logger.debug("raycast_hash -> compact_id=%s final_hash=%s", compact, final_hash)

    return compact


def vertex_hash(
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
        if abs(v[axis_index] - boundary) < 1e-4
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

        # Only include the two in-plane coordinates for the hash
        if normal_axis == "X":
            sig = int(vert.y) ^ int(vert.z)
            # print(f"{vert.y}, {vert.z} {sig}  {mat_color_int_cache[mat_color]}   {hash_value}")
        elif normal_axis == "Y":
            sig = int(vert.x) ^ int(vert.z)
            # print(f"{vert.x}, {vert.z} {sig}  {mat_color_int_cache[mat_color]}   {hash_value}")
        else:
            sig = int(vert.x) ^ int(vert.y)
            # print(f"{vert.x}, {vert.y} {sig}  {mat_color_int_cache[mat_color]}   {hash_value}")

        sig ^= mat_color_int_cache[mat_color]
        hash_value = (
            hash_value * 31 + sig
        ) & 0xFFFFFFFFFFFFFFFF  # rolling hash with 64-bit mask

    hash = to_signed32((hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF))

    # print(f"{obj.name} {normal_axis} {sign} {direction}  {get_compact_hash(hash)} \n")
    # return hash

    return get_compact_hash(hash)


def _build_vert_to_mat_map(mesh: bpy.types.Mesh) -> dict[int, int]:
    """
    Builds a mapping from a vertex index to the material index of a
    polygon it belongs to. This is much faster than searching polygons for
    each vertex.
    """
    vert_to_mat = {}
    # This will associate each vertex with the material of the last
    # polygon it's a part of, which is sufficient for this hashing purpose.
    for poly in mesh.polygons:
        for vert_idx in poly.vertices:
            vert_to_mat[vert_idx] = poly.material_index
    return vert_to_mat


def plane_hash(
    obj: bpy.types.Object,
    normal_axis: str,
    sign=1,
    size=1.0,
    direction="BL_TR",
    scale=100,
):
    """
    Optimized hash for a voxel face on a given plane using a pre-built
    vertex-to-material map for efficient color lookup.
    """
    logger.debug("plane_hash: obj=%s axis=%s sign=%s scale=%s", getattr(obj, 'name', str(obj)), normal_axis, sign, scale)
    mesh = obj.data
    axis_index = "XYZ".index(normal_axis)
    boundary = sign * (size / 2)

    # Collect vertex indices and their scaled coordinates that lie on the boundary.
    # We store the original index to look up the material later.
    boundary_verts = [
        (
            i,
            Vector(
                (round(v.co.x * scale), round(v.co.y * scale), round(v.co.z * scale))
            ),
        )
        for i, v in enumerate(mesh.vertices)
        if abs(v.co[axis_index] - boundary) < 1e-4
    ]
    if not boundary_verts:
        return 0  # No face touching this plane -> air

    # Sort vertices based on their 2D position on the plane.
    sorted_verts = sorted(
        boundary_verts,
        key=lambda item: face_vertex_sort_key(item[1], normal_axis, sign, direction),
    )

    # Pre-calculate vertex-to-material mapping and material color cache.
    vert_to_mat_map = _build_vert_to_mat_map(mesh)
    mat_colors = {}  # Cache for material colors as integer values

    hash_value = 0
    for idx, vert in sorted_verts:
        mat_index = vert_to_mat_map.get(idx, -1)

        if mat_index not in mat_colors:
            color_int = 0  # Default for no material or air
            if (
                obj.material_slots
                and mat_index != -1
                and mat_index < len(obj.material_slots)
            ):
                mat = obj.material_slots[mat_index].material
                if mat:
                    c = mat.diffuse_color
                    color_int = (
                        (int(c[0] * 255) << 16)
                        + (int(c[1] * 255) << 8)
                        + int(c[2] * 255)
                    )
            mat_colors[mat_index] = color_int

        color_int = mat_colors[mat_index]

        # Only include the two in-plane coordinates for the hash
        if normal_axis == "X":
            sig = int(vert.y) ^ int(vert.z)
        elif normal_axis == "Y":
            sig = int(vert.x) ^ int(vert.z)
        else:  # Z
            sig = int(vert.x) ^ int(vert.y)

        sig ^= color_int
        hash_value = (
            hash_value * 31 + sig
        ) & 0xFFFFFFFFFFFFFFFF  # rolling hash with 64-bit mask

    # Fold 64-bit hash into 32-bit and get a compact ID
    final_hash = to_signed32(
        (hash_value & 0xFFFFFFFF) ^ ((hash_value >> 32) & 0xFFFFFFFF)
    )

    return get_compact_hash(final_hash)


class VOXEL_OT_face_hash(bpy.types.Operator):
    bl_idname = "voxel.face_hash"
    bl_label = "Compute Voxel Face Hashes"

    def execute(self, context):
        logger.info("--- Starting non-blocking Voxel Face Hash Computation ---")

        c = type.world_blender()
        lib_items = list(c.getLibraryItems())

        if not lib_items:
            logger.info("Library empty — nothing to do")
            return {"CANCELLED"}

        MAX_ITEMS = 1000
        if len(lib_items) > MAX_ITEMS:
            logger.warning("Library too large (%d items) — aborting (limit=%d)", len(lib_items), MAX_ITEMS)
            return {"CANCELLED"}

        # prepare/reset state
        try:
            type.clearConfigCollection()
        except Exception:
            logger.exception("clearConfigCollection failed — continuing")
        try:
            c.purgeLibrary()
            c.clearHashes()
        except Exception:
            logger.exception("Failed to purge/clear library state")

        wm = context.window_manager
        # Build a safe, name-based queue and an index map so the timer never dereferences
        # PropertyGroup.ptrs that may become invalid during processing.
        global _hash_queue, _lib_index_map
        _hash_queue = []
        _lib_index_map = {}

        # snapshot library items as names (safe Python strings)
        for i, li in enumerate(list(c.getLibraryItems())):
            try:
                obj = li.obj
            except Exception:
                # defensive: skip items that cause RNA issues
                continue
            if obj and getattr(obj, "name", None) in bpy.data.objects:
                name = obj.name
                _hash_queue.append(name)
                _lib_index_map[name] = i

        # If user added objects but they weren't in library collection, include them by name
        for obj in [o for o in bpy.context.selected_objects if o.type == 'MESH']:
            if obj.name not in _hash_queue:
                _hash_queue.append(obj.name)

        if not _hash_queue:
            logger.info("No valid library objects to process")
            return {"CANCELLED"}

        c.T_hash_running = True
        c.T_hash_cancel_requested = False
        c.T_hash_index = 0
        c.T_hash_total = len(_hash_queue)

        try:
            wm.progress_begin(0, max(1, c.T_hash_total))
        except Exception:
            pass

        def _timer_step():
            try:
                c_local = type.world_blender()
            except Exception:
                logger.exception("Failed to read world_blender state from timer — aborting")
                return None

            lib_q = _hash_queue
            idx = c_local.T_hash_index

            if c_local.T_hash_cancel_requested:
                logger.info("Hash compute cancelled at index %d", idx)
                c_local.T_hash_running = False
                try:
                    wm.progress_end()
                except Exception:
                    pass
                _hash_queue.clear()
                return None

            if idx >= len(lib_q):
                logger.info("Hash compute finished (all %d items)", len(lib_q))
                c_local.T_hash_running = False
                try:
                    wm.progress_end()
                except Exception:
                    pass
                _hash_queue.clear()
                return None

            obj_name = lib_q[idx]

            try:
                obj = bpy.data.objects.get(obj_name)
                if obj and getattr(obj, "type", None) == "MESH":
                    ctx = bpy.context
                    prev_selected_names = [o.name for o in ctx.selected_objects if getattr(o, 'name', None)]
                    prev_active_name = getattr(ctx.view_layer.objects.active, 'name', None)
                    try:
                        # deselect via direct API (avoid using the operator which can be unsafe in timer callbacks)
                        try:
                            for o_sel in list(ctx.selected_objects):
                                try:
                                    o_sel.select_set(False)
                                except Exception:
                                    pass
                        except Exception:
                            pass

                        try:
                            obj.select_set(True)
                            ctx.view_layer.objects.active = obj
                        except Exception:
                            logger.debug("Failed to select/make active object %s", obj_name)

                        logger.info("Timer: computing hashes for %s (%d/%d)", obj_name, idx + 1, len(lib_q))
                        t0 = time.time()

                        lib_idx = _lib_index_map.get(obj_name)
                        calc_linked_configs_for_object(obj, lib_idx)

                        logger.info("Timer: finished %s in %.4f sec", obj_name, time.time() - t0)
                    finally:
                        # restore selection safely by name lookup
                        try:
                            for o in list(ctx.selected_objects):
                                try:
                                    o.select_set(False)
                                except Exception:
                                    pass
                        except Exception:
                            pass

                        for name in prev_selected_names:
                            if name in bpy.data.objects:
                                try:
                                    bpy.data.objects[name].select_set(True)
                                except Exception:
                                    pass

                        if prev_active_name and prev_active_name in bpy.data.objects:
                            try:
                                ctx.view_layer.objects.active = bpy.data.objects[prev_active_name]
                            except Exception:
                                pass
                else:
                    logger.warning("Timer: skipping non-mesh or missing object '%s' at index %d", obj_name, idx)
            except Exception:
                logger.exception("Timer: error while processing library item (name=%s) at index %d", obj_name, idx)

            c_local.T_hash_index = idx + 1
            try:
                wm.progress_update(min(c_local.T_hash_index, c_local.T_hash_total))
            except Exception:
                pass

            return 0.01

        bpy.app.timers.register(_timer_step, first_interval=0.01)

        return {"FINISHED"}


class VOXEL_OT_cancel_face_hash(bpy.types.Operator):
    bl_idname = "voxel.cancel_face_hash"
    bl_label = "Cancel Hash Compute"

    def execute(self, context):
        try:
            c = type.world_blender()
        except Exception:
            self.report({"WARNING"}, "Hash state unavailable")
            return {"CANCELLED"}

        if not c.T_hash_running:
            self.report({"INFO"}, "No hash compute running")
            return {"CANCELLED"}

        c.T_hash_cancel_requested = True
        logger.info("User requested hash cancellation")
        self.report({"INFO"}, "Hash cancellation requested")
        return {"FINISHED"}


def calc_linked_configs(oriItem: type.LibraryItem):
    """Spawn 7 linked duplicates with object-level transforms from a LibraryItem.

    Kept for backward compatibility; prefers using the provided LibraryItem where possible.
    """

    # prefer to operate on the live object if possible
    try:
        base_obj = oriItem.obj
    except Exception:
        logger.exception("calc_linked_configs: cannot access oriItem.obj — aborting")
        return

    if base_obj is None:
        logger.debug("calc_linked_configs: library item has no object")
        return

    if not hasattr(base_obj, "type") or base_obj.type != "MESH":
        logger.warning("calc_linked_configs: skipping non-mesh object %s", getattr(base_obj, "name", str(base_obj)))
        return

    logger.debug("calc_linked_configs: start processing %s", base_obj.name)

    # Compute hashes for the original orientation first using the LibraryItem
    try:
        logger.debug("calc_linked_configs: computing original orientation for %s", base_obj.name)
        t0 = time.time()
        calcHashes(oriItem, base_obj, None, None, None, None)
        logger.debug("calc_linked_configs: original orientation done for %s (%.4fs)", base_obj.name, time.time() - t0)
    except Exception:
        logger.exception("calcHashes failed for original object: %s", base_obj)

    ConfigCollection = type.getConfigCollection()

    for i, (label, mat) in enumerate(CONFIGS, start=1):
        temp_mesh_obj = None
        try:
            logger.debug("calc_linked_configs: spawning temp for %s (%s)", base_obj.name, label)
            temp_mesh_obj = spawn_temp(base_obj, ConfigCollection, mat, i * 2)
            if temp_mesh_obj is None:
                logger.warning("spawn_temp returned None for %s", base_obj.name)
                continue

            logger.debug("calc_linked_configs: computing %s for %s", label, base_obj.name)
            t1 = time.time()
            calcHashes(None, temp_mesh_obj, base_obj, mat, label, i * 2)
            logger.debug("calc_linked_configs: %s done for %s (%.4fs)", label, base_obj.name, time.time() - t1)
        except Exception:
            logger.exception("Error while processing transformed config %s for %s", label, base_obj.name)
        finally:
            # Ensure we always try to remove the temp object, but don't let failures crash Blender
            if temp_mesh_obj is not None:
                try:
                    if temp_mesh_obj.name in bpy.data.objects:
                        try:
                            ConfigCollection.objects.unlink(temp_mesh_obj)
                        except Exception:
                            logger.debug("Failed to unlink temp object %s from ConfigCollection", temp_mesh_obj.name)
                        try:
                            bpy.data.objects.remove(temp_mesh_obj, do_unlink=True)
                        except Exception as exc:
                            logger.warning("Failed to remove temp object %s: %s", temp_mesh_obj.name, exc)
                except Exception:
                    logger.exception("Unexpected error while cleaning temp object")


def calc_linked_configs_for_object(base_obj: bpy.types.Object, lib_index: int | None = None):
    """Process a mesh object by name/index without dereferencing LibraryItem pointers in the timer.

    - If lib_index is provided, the corresponding LibraryItem (by index) will be used to store the original hashes.
    - If no LibraryItem exists, a new one is created and populated.
    """
    if base_obj is None or not hasattr(base_obj, "type") or base_obj.type != "MESH":
        logger.debug("calc_linked_configs_for_object: invalid object passed")
        return

    c = type.world_blender()

    lib_item = None
    if lib_index is not None:
        try:
            if lib_index < len(c.library_items):
                lib_item = c.library_items[lib_index]
        except Exception:
            lib_item = None

    if lib_item is None:
        # create a new library entry for this object (avoid scanning existing library to prevent RNA access)
        try:
            lib_item = c.addLibraryItem(base_obj)
        except Exception:
            logger.exception("Failed to add LibraryItem for %s", base_obj.name)
            lib_item = None

    # compute original orientation hashes using calcHashes with lib_item (if available)
    try:
        if lib_item is not None:
            calcHashes(lib_item, base_obj, None, None, None, None)
        else:
            # fallback: compute hashes manually and addHash without writing to a LibraryItem
            logger.debug("calc_linked_configs_for_object: no LibraryItem available for %s — skipping original-write", base_obj.name)
    except Exception:
        logger.exception("calc_linked_configs_for_object: failed original-orientation hash for %s", base_obj.name)

    # process transformed configs
    ConfigCollection = type.getConfigCollection()
    for i, (label, mat) in enumerate(CONFIGS, start=1):
        temp_mesh_obj = None
        try:
            temp_mesh_obj = spawn_temp(base_obj, ConfigCollection, mat, i * 2)
            if temp_mesh_obj is None:
                logger.warning("spawn_temp returned None for %s (%s)", base_obj.name, label)
                continue
            calcHashes(None, temp_mesh_obj, base_obj, mat, label, i * 2)
        except Exception:
            logger.exception("calc_linked_configs_for_object: failed transformed config %s for %s", label, base_obj.name)
        finally:
            if temp_mesh_obj is not None:
                try:
                    if temp_mesh_obj.name in bpy.data.objects:
                        try:
                            ConfigCollection.objects.unlink(temp_mesh_obj)
                        except Exception:
                            logger.debug("Failed to unlink temp object %s from ConfigCollection", temp_mesh_obj.name)
                        try:
                            bpy.data.objects.remove(temp_mesh_obj, do_unlink=True)
                        except Exception as exc:
                            logger.warning("Failed to remove temp object %s: %s", temp_mesh_obj.name, exc)
                except Exception:
                    logger.exception("Unexpected error while cleaning temp object (object-based)")

def calcHashes(
    libItem: type.LibraryItem | None, temp_mesh_obj, base_obj, mat, label, y_offset
):
    c = type.world_blender()

    start = time.time()
    method = c.T_hash_method
    logger.debug("calcHashes: start (obj=%s label=%s method=%s)", getattr(temp_mesh_obj, 'name', str(base_obj)), label, method)

    try:
        if method == type.HASH_METHOD_RAYCAST:
            # Using the raycasting approach. Keep a modest grid resolution by default
            # to avoid long blocking computations when hashing many objects.
            grid_res = 6  # reduced from 10 for performance and responsiveness
            hash_PX = raycast_hash(
                temp_mesh_obj,
                normal_axis="X",
                sign=1,
                direction="BL_TR",
                grid_resolution=grid_res,
            )
            hash_NX = raycast_hash(
                temp_mesh_obj,
                normal_axis="X",
                sign=-1,
                direction="BR_TL",
                grid_resolution=grid_res,
            )
            hash_PY = raycast_hash(
                temp_mesh_obj,
                normal_axis="Y",
                sign=1,
                direction="BR_TL",
                grid_resolution=grid_res,
            )
            hash_NY = raycast_hash(
                temp_mesh_obj,
                normal_axis="Y",
                sign=-1,
                direction="BL_TR",
                grid_resolution=grid_res,
            )
            hash_PZ = raycast_hash(
                temp_mesh_obj,
                normal_axis="Z",
                sign=1,
                direction="BL_TR",
                grid_resolution=grid_res,
            )
            hash_NZ = raycast_hash(
                temp_mesh_obj,
                normal_axis="Z",
                sign=-1,
                direction="BR_TL",
                grid_resolution=grid_res,
            )
        else:  # HASH_METHOD_VERTEX
            hash_PX = plane_hash(temp_mesh_obj, normal_axis="X", sign=1, direction="BL_TR")
            hash_NX = plane_hash(temp_mesh_obj, normal_axis="X", sign=-1, direction="BR_TL")
            hash_PY = plane_hash(temp_mesh_obj, normal_axis="Y", sign=1, direction="BR_TL")
            hash_NY = plane_hash(temp_mesh_obj, normal_axis="Y", sign=-1, direction="BL_TR")
            hash_PZ = plane_hash(temp_mesh_obj, normal_axis="Z", sign=1, direction="BL_TR")
            hash_NZ = plane_hash(temp_mesh_obj, normal_axis="Z", sign=-1, direction="BR_TL")
    except Exception as exc:
        logger.exception("Hash computation failed for object %s: %s", getattr(temp_mesh_obj, 'name', str(temp_mesh_obj)), exc)
        # fallback to 'air' hashes so we don't crash or leave inconsistent state
        hash_PX = hash_NX = hash_PY = hash_NY = hash_PZ = hash_NZ = 0

    # Log computed hashes and timings
    try:
        obj_debug_name = getattr(libItem.obj, 'name', None) if libItem else getattr(base_obj, 'name', str(temp_mesh_obj))
        logger.info(
            "Hashes for %s (label=%s): PX=%s NX=%s PY=%s NY=%s PZ=%s NZ=%s (method=%s) — took %.4fs",
            obj_debug_name,
            label,
            hash_PX,
            hash_NX,
            hash_PY,
            hash_NY,
            hash_PZ,
            hash_NZ,
            method,
            time.time() - start,
        )
    except Exception:
        logger.exception("Failed to log hash summary for %s", getattr(temp_mesh_obj, 'name', str(temp_mesh_obj)))

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
            libItem.obj.location + Vector((-0.8, 0, 0)),
            str(hash_NX),
            f"hash_{libItem.obj.name}_NX",
        )
        add_text_at(
            libItem.obj.location + Vector((0.7, 0, 0)),
            str(hash_PX),
            f"hash_{libItem.obj.name}_PX",
        )
        add_text_at(
            libItem.obj.location + Vector((-0, -0.8, 0)),
            str(hash_NY),
            f"hash_{libItem.obj.name}_NY",
        )
        add_text_at(
            libItem.obj.location + Vector((-0, 0.6, 0)),
            str(hash_PY),
            f"hash_{libItem.obj.name}_PY",
        )
        add_text_at(
            libItem.obj.location + Vector((0.7, -0.8, -0.5)),
            str(hash_NZ),
            f"hash_{libItem.obj.name}_NZ",
        )
        add_text_at(
            libItem.obj.location + Vector((-0.8, 0.6, 0.5)),
            str(hash_PZ),
            f"hash_{libItem.obj.name}_PZ",
        )


def apply_transform_to_mesh(obj: bpy.types.Object, matrix: Matrix):
    """Apply a transform matrix to mesh geometry in-place using bmesh."""
    if obj.type != "MESH":
        return

    mesh = obj.data
    bm = bmesh.new()
    try:
        bm.from_mesh(mesh)
        bmesh.ops.transform(bm, matrix=matrix, verts=bm.verts)
        bm.to_mesh(mesh)
    finally:
        bm.free()

    # Reset object matrix so it stays at intended location
    obj.matrix_world = Matrix.Identity(4)


def spawn_temp(obj: bpy.types.Object, collection, mat, i: float) -> bpy.types.Object:
    # defensive guards — ensure we only operate on valid mesh objects
    if obj is None or not hasattr(obj, "type") or obj.type != "MESH":
        logger.warning("spawn_temp: invalid or non-mesh object passed: %s", getattr(obj, "name", str(obj)))
        return None
    if not getattr(obj, "data", None):
        logger.warning("spawn_temp: object %s has no mesh data", obj.name)
        return None

    new_obj = obj.copy()
    try:
        new_obj.data = obj.data.copy()  # give new mesh
    except Exception as exc:
        logger.warning("spawn_temp: failed to copy mesh data for %s: %s", obj.name, exc)
        return None

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
        try:
            bpy.data.objects.remove(bpy.data.objects[name], do_unlink=True)
        except Exception as exc:
            logger.warning("add_text_at: failed to remove existing text object %s: %s", name, exc)

    curve = bpy.data.curves.new(name, type="FONT")
    curve.body = text
    obj = bpy.data.objects.new(name, curve)
    obj.location = location

    ConfigCollection = type.getConfigCollection()
    try:
        ConfigCollection.objects.link(obj)
        obj.scale = (0.25, 0.25, 0.25)
    except Exception as exc:
        logger.warning("add_text_at: failed to link text object %s: %s", name, exc)
    return obj


# module class list (registered centrally by `__init__.py`)
classes = (
    VOXEL_OT_face_hash,
    VOXEL_OT_cancel_face_hash,
)

