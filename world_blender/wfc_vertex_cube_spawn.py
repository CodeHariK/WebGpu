import bpy
from mathutils import Vector

from . import debug
logger = debug.logger

class WFC_VERTEX_CUBE_SPAWN_OT(bpy.types.Operator):
    """Operator that spawns visual voxel cubes for objects named '<name>_<mask>'.

    Behavior:
    - Scans scene objects (skips internal `voxel_base`).
    - Parses a trailing integer suffix from object names (e.g. 'block_37').
    - Interprets that integer as an 8-bit occupancy mask for a 2×2×2 voxel block.
    - Spawns linked cubes at the corresponding offsets for set bits and parents
      them to the source object. (Unset bits currently spawn a smaller placeholder.)
    - Intended for quick visual debugging/preview of voxel masks.

    Properties:
    - action (StringProperty): optional action identifier (not used by `execute`).

    Returns:
    - Blender Operator status dict ("FINISHED" / "CANCELLED").
    """

    bl_idname = "world_blender.wfc_vertex_cube_spawn_ot"
    bl_label = "WFC Vertex Cube Spawn Operator"

    # define RNA property via assignment (avoids call-expression-in-annotation warnings)
    action: bpy.props.StringProperty()

    def execute(self, context):
        """Execute the BOX_OT operator.

        Steps:
        1. Ensure a base mesh named 'voxel_base' exists (create if missing).
        2. For every object whose name contains a trailing integer mask, spawn
           linked cubes for set bits and parent them to the source object.
        3. Return {'FINISHED'} on success or {'CANCELLED'} if there are no
           valid voxel objects to process.
        """

        # Create one base cube (if doesn't exist)
        if "voxel_base" not in bpy.data.objects:
            bpy.ops.mesh.primitive_cube_add(size=0.5, location=(0, 0, 0))
            base_cube = bpy.context.active_object
            base_cube.name = "voxel_base"
        else:
            base_cube = bpy.data.objects["voxel_base"]

        # Offsets for 2×2×2 cube block (centered)
        offsets = [
            Vector((-0.5, 0.5, -0.5)),
            Vector((0.5, 0.5, -0.5)),
            Vector((0.5, -0.5, -0.5)),
            Vector((-0.5, -0.5, -0.5)),
            Vector((-0.5, 0.5, 0.5)),
            Vector((0.5, 0.5, 0.5)),
            Vector((0.5, -0.5, 0.5)),
            Vector((-0.5, -0.5, 0.5)),
        ]

        for obj in bpy.data.objects:
            if obj == base_cube:
                continue

            try:
                _, num_str = obj.name.split("_", 1)
                mask = int(num_str)
            except (ValueError, TypeError):
                logger.debug("Skipping non-voxel object name: %s", obj.name)
                continue  # skip if name format is invalid

            # Spawn cubes only for bits that are set
            for i, off in enumerate(offsets):
                if (mask >> i) & 1:  # check if bit i is 1
                    cube = base_cube.copy()
                    cube.data = base_cube.data  # linked mesh
                    cube.location = obj.location + off
                    cube.name = f"voxel_{obj.name}_{i}"
                    bpy.context.collection.objects.link(cube)
                else:
                    cube = base_cube.copy()
                    cube.data = base_cube.data  # linked mesh
                    cube.location = obj.location + off
                    cube.scale = (0.25, 0.25, 0.25)
                    cube.name = f"voxel_{obj.name}_{i}"
                    bpy.context.collection.objects.link(cube)

                cube.parent = obj
                cube.matrix_parent_inverse = obj.matrix_world.inverted()

        return {"FINISHED"}
