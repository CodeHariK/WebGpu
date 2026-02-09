import bpy
from mathutils import Vector


class BOX_OT(bpy.types.Operator):
    bl_idname = "wfc_blender_hash.box_ot"
    bl_label = "Box"

    action: bpy.props.StringProperty()

    def execute(self, context):
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
            except:
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
