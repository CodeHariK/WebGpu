"""Paste from Godot: rebuild a saved MCGrid in the Playground, every tile editable.

In Godot (cubes.tscn) the Stats panel's "Copy for Blender" puts the grid on the clipboard as
    {"format": "mc_cells", "version": 1, "tiles": "res://...glb", "size": [x, y, z],
     "cells": [[x, y, z, hash], ...]}
(MCGrid::cells_to_json). Here each cell gets the tile Godot would draw, turned the same way.

Every copy is selectable and keeps its real material. Copies share mesh data with the real
tiles, so Tab into Edit Mode on any of them edits the real tile and every copy of it.

Axes: Godot is +Y up, +Z towards the viewer; Blender is +Z up, -Y towards the viewer.
A Godot cell (x, y, z) goes to Blender (x, depth - 1 - z, y), so the layout reads the same.
"""

import json

import bpy
from mathutils import Vector

from . import cube, playground, view
from .piece_lib import set_tiles
from .variants import variant_index

MAX_CELLS = 20000  # a guard: every cell becomes one or more objects


def parse(text):
    """(size, cells, tiles path) from the clipboard text, or raise ValueError with a reason."""
    try:
        data = json.loads(text)
    except json.JSONDecodeError as error:
        raise ValueError("Clipboard is not JSON (use Copy for Blender in Godot)") from error
    if not isinstance(data, dict) or data.get("format") != "mc_cells":
        raise ValueError("Clipboard JSON is not an MC grid (format must be mc_cells)")
    if data.get("version") != 1:
        raise ValueError(f"Unsupported MC grid version {data.get('version')}")
    size = data.get("size")
    cells = data.get("cells")
    if not (isinstance(size, list) and len(size) == 3 and isinstance(cells, list)):
        raise ValueError("MC grid JSON is missing size or cells")
    if len(cells) > MAX_CELLS:
        raise ValueError(f"{len(cells)} cells is too many for the playground (max {MAX_CELLS})")
    clean = []
    for cell in cells:
        if not (isinstance(cell, list) and len(cell) == 4 and all(isinstance(v, int) for v in cell)):
            raise ValueError(f"Bad cell entry: {cell!r}")
        if not 0 <= cell[3] <= 255:
            raise ValueError(f"Bad cell hash: {cell!r}")
        clean.append(tuple(cell))
    return [int(v) for v in size], clean, str(data.get("tiles", ""))


def build(scene, tile_set, size, cells):
    """Place every cell's tile in the Playground. Returns (tiles placed, missing hashes)."""
    tiles = set_tiles(tile_set)  # own tiles + borrowed pieces
    lookup = playground.tile_placements(tiles.keys(), tile_set.mc_tiles.mode, pieces=tile_set.mc_tiles.build_from_pieces)
    playground.clear(scene)
    collection = playground._playground_collection(scene)
    origin = playground._origin(tile_set)
    depth = size[2]
    placed = 0
    missing = set()
    for x, y, z, h in cells:
        if h == cube.EMPTY:
            continue
        position = playground.cell_centre(origin, x, depth - 1 - z, y)
        if h in lookup:
            for base, turns, mirrored in lookup[h]:  # several for a shape built from pieces
                options = tiles[base]
                tile = options[variant_index(x, y, z, len(options))]  # Godot coordinates: same as the game
                playground._copy_tile(tile, collection, position, turns, h, "pasted", mirrored)
            placed += 1
        elif h != cube.FULL:  # full is optional: Godot draws nothing there either
            playground._missing_box(collection, position, h)
            missing.add(h)
    view.refresh(scene)
    return placed, sorted(missing)


class MCTILES_OT_paste_from_godot(bpy.types.Operator):
    """Rebuild the grid copied with "Copy for Blender" in Godot, every tile editable"""

    bl_idname = "mc_tiles.paste_from_godot"
    bl_label = "Paste from Godot"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        tile_set = context.scene.mc_tiles_active
        if tile_set is None or tile_set.mc_tiles.kind != "CUBE":
            self.report({"WARNING"}, "Pick a cube tile set first")
            return {"CANCELLED"}
        try:
            size, cells, tiles_path = parse(context.window_manager.clipboard)
        except ValueError as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        placed, missing = build(context.scene, tile_set, size, cells)
        text = f"Pasted {placed} tiles from Godot ({tiles_path or 'unknown set'}) using {tile_set.name}"
        if missing:
            text += f"; {len(missing)} hashes missing: " + ", ".join(f"{h:08b}" for h in missing[:8])
        self.report({"WARNING"} if missing else {"INFO"}, text)
        return {"FINISHED"}


CLASSES = (MCTILES_OT_paste_from_godot,)
