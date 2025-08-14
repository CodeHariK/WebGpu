from . import panel_test
from . import help
from . import type_test
from . import voxel
from . import library
from . import type


def register():
    # panel_test.register()
    # help.register()
    # type_test.register()

    type.register()
    library.register()
    voxel.register()


def unregister():
    # panel_test.unregister()
    # help.unregister()
    # type_test.unregister()

    type.unregister()
    library.unregister()
    voxel.unregister()
