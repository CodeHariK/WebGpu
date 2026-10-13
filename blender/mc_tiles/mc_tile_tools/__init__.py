"""MC Tile Tools: manage, check and test-fit marching cubes / prism tiles.

Files:
    variants.py    mesh variants (_v2...) and the per-cell pick shared with Godot
    tiles.py       which objects are tiles, and their hash (same as Godot)
    modes.py       cube set modes (68 / 256 / heightmap) and their rules
    cube.py        cube Y turns (same table as Godot) and the tile check
    props.py       settings saved on each tile-set collection
    view.py        debug material for helper meshes
    ops.py         Make / Remove Tile Set, Select Tile
    tilelist.py    searchable tile list
    playground.py  selected tile among random neighbours (same rules as Godot)
    paste.py       rebuild a Godot grid (Copy for Blender) in the playground, all editable
    export.py      export a set to its own .glb, adding mirror copies (replaces ExportMergeChildMesh.py)
    helpers.py     move non-tile objects into the set's Helpers collection
    names.py       hash / hash_vN names in Blender, set name added on export
    arrange.py     move tiles into a layout (corners / hash / family / heightmap), Restore
    labels.py      hash labels under tiles in the 3D view
    corners.py     debug cell box per tile, solid / empty corners as coloured dots
    tilefilter.py  show a subset of tiles (shapes, corners, variants, problems, family) in list + viewport
    panel.py       the "MC Tiles" sidebar tab
"""

import sys

# Blender's Reload Scripts (F3) re-imports this package but not its submodules: reload them here,
# dependencies first, so edits to any file take effect without restarting Blender.
_MODULES = (
    "variants", "tiles", "modes", "cube", "props", "view", "ops", "tilelist",
    "playground", "paste", "names", "tilefilter", "arrange", "labels", "corners", "export", "helpers", "panel",
)
if "bpy" in locals():
    import importlib

    for _name in _MODULES:
        _module = sys.modules.get(f"{__package__}.{_name}")
        if _module is not None:
            importlib.reload(_module)

import bpy  # noqa: E402

from . import (  # noqa: E402
    arrange, corners, export, helpers, labels, names, ops, panel, paste, playground, props, tilefilter, tilelist, view,
)

CLASSES = (
    ops.CLASSES + paste.CLASSES + export.CLASSES + helpers.CLASSES + names.CLASSES + tilefilter.CLASSES
    + arrange.CLASSES + panel.CLASSES
)


def register():
    props.register()
    tilefilter.register()
    arrange.register()
    labels.register()
    corners.register()
    view.register()
    playground.register()
    tilelist.register()
    for cls in CLASSES:
        bpy.utils.register_class(cls)


def unregister():
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
    tilelist.unregister()
    playground.unregister()
    view.unregister()
    corners.unregister()
    labels.unregister()
    arrange.unregister()
    tilefilter.unregister()
    props.unregister()
