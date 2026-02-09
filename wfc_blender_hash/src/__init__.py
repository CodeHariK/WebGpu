from . import voxel
from . import library
from . import type
from . import save


def register():
    type.register()
    save.register()
    library.register()
    voxel.register()


def unregister():
    type.unregister()
    save.unregister()
    library.unregister()
    voxel.unregister()
