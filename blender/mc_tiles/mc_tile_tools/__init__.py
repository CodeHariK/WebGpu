"""MC Tile Tools: manage, check and test-fit marching cubes / prism tiles.

Files:
    variants.py    tile tags (height, group, number: _ma2, _hb1; old _v2) and the per-cell pick shared with Godot
    tiles.py       which objects are tiles, and their hash (same as Godot)
    modes.py       cube set modes (68 / 256 / heightmap) and their rules
    cube.py        cube Y turns (same table as Godot) and the tile check
    pieces.py      build missing shapes from overlapping smaller tiles (edges / faces rule)
    piece_lib.py   where pieces come from (any collection), set_tiles() used to draw / build / export
    piece_ops.py   pieces box: which pieces the set has, placeholders, Generate Built Tiles
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
    seams.py       seam check logic: face outlines must match on every face pair that can touch
    seams_ui.py    Check Seams button, results in the Check panel, red / green outline overlay
    tilefilter.py  show a subset of tiles (shapes, corners, variants, problems, family) in list + viewport
    panel.py       the "MC Tiles" sidebar tab
"""

import sys

# Blender's Reload Scripts (F3) re-imports this package but not its submodules: reload them here,
# dependencies first, so edits to any file take effect without restarting Blender.
_MODULES = (
    "variants", "tiles", "modes", "cube", "pieces", "piece_lib", "props", "view", "ops", "playground", "paste", "names",
    "tilefilter", "arrange", "labels", "corners", "seams", "seams_ui", "piece_ops", "tilelist", "export", "helpers", "panel",
)
if "bpy" in locals():
    import importlib

    for _name in _MODULES:
        _module = sys.modules.get(f"{__package__}.{_name}")
        if _module is not None:
            importlib.reload(_module)

import bpy  # noqa: E402

from . import (  # noqa: E402
    arrange, corners, export, helpers, labels, piece_lib, piece_ops, seams_ui, names, ops, panel, paste, playground, props, tilefilter, tilelist, view,
)

CLASSES = (
    ops.CLASSES + paste.CLASSES + export.CLASSES + helpers.CLASSES + names.CLASSES + tilefilter.CLASSES
    + arrange.CLASSES + seams_ui.CLASSES + piece_ops.CLASSES + panel.CLASSES
)


def register():
    piece_lib.register()
    props.register()
    tilefilter.register()
    arrange.register()
    labels.register()
    corners.register()
    seams_ui.register()
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
    seams_ui.unregister()
    corners.unregister()
    labels.unregister()
    arrange.unregister()
    tilefilter.unregister()
    props.unregister()
    piece_lib.unregister()
