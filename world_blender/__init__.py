bl_info = {
    "name": "ADDON_NAME",
    "author": "AUTHOR_NAME",
    "description": "",
    "blender": (4, 2, 0),
    "version": (0, 0, 1),
    "location": "",
    "warning": "",
    "category": "Generic",
}

import bpy
from bpy.props import PointerProperty

from . import voxel
from . import library
from . import type
from . import voxel_hash

# Collect classes from submodules (order matters — `type` must register first)
_ALL_CLASSES = tuple(type.classes) + tuple(library.classes) + tuple(voxel.classes) + tuple(voxel_hash.classes)


def register():

    # register all classes
    for cls in _ALL_CLASSES:
        bpy.utils.register_class(cls)

    # create addon data on WindowManager (must be after type classes registered)
    bpy.types.WindowManager.world_blender_data = PointerProperty(type=type.world_blenderData)

def unregister():

    # remove WindowManager property
    try:
        del bpy.types.WindowManager.world_blender_data
    except Exception:
        pass

    # unregister classes in reverse order
    for cls in reversed(_ALL_CLASSES):
        try:
            bpy.utils.unregister_class(cls)
        except Exception:
            pass
