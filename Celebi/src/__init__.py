from . import panel_test
from . import help
from . import type_test
from . import voxel
from . import library
from . import type
from . import save


def register():
    # panel_test.register()
    # help.register()
    # type_test.register()

    type.register()
    save.register()
    library.register()
    voxel.register()


def unregister():
    # panel_test.unregister()
    # help.unregister()
    # type_test.unregister()

    type.unregister()
    save.unregister()
    library.unregister()
    voxel.unregister()
