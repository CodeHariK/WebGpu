import bpy
import time

from . import type


def update_preview(s, _):
    c = type.celebi()
    spacing = 1.0

    time_start = time.time()

    # Remove existing preview
    for v in c.T_preview_voxels:
        obj = bpy.data.objects.get(v.name)
        if obj:
            bpy.data.objects.remove(obj, do_unlink=True)
    c.T_preview_voxels.clear()

    print("My Script Finished: %.4f sec" % (time.time() - time_start))

    base_obj = bpy.data.objects.get(c.voxels[c.T_voxels_index].name)
    if not base_obj:
        return
    base_loc = base_obj.location

    def signed_range(val):
        if val >= 0:
            return range(0, val)
        else:
            return range(val + 1, 1)  # includes 0 and negatives

    for x in signed_range(c.T_dim_x):
        for y in signed_range(c.T_dim_y):
            for z in signed_range(c.T_dim_z):
                loc = (
                    base_loc.x + (x) * spacing,
                    base_loc.y + (y) * spacing,
                    base_loc.z + (z) * spacing,
                )
                bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
                pv = bpy.context.active_object
                pv.name = "preview"
                pv.hide_select = True
                pv.display_type = "WIRE"
                pv.show_in_front = True
                item = c.T_preview_voxels.add()
                item.name = pv.name

    bpy.context.view_layer.objects.active = base_obj

    print("My Script Finished: %.4f sec" % (time.time() - time_start))


def my_confirm_function():
    print("Confirmed!")


def my_cancel_function():
    c = type.celebi()

    c.T_dim_x = 1
    c.T_dim_z = 1
    c.T_dim_y = 1

    print("Cancelled!")
