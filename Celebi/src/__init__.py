from . import panel_test
from . import help
from . import type_test
from . import voxel
from . import library


def register():
    # panel_test.register()
    # celebi.register()
    # type_test.register()
    library.register()
    voxel.register()


def unregister():
    # panel_test.unregister()
    # celebi.unregister()
    # type_test.unregister()
    library.unregister()
    voxel.unregister()
