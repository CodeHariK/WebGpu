"""Pieces in Blender: which minimum pieces the set has, placeholders for missing ones, and
"Generate Built Tiles" (turn every shape built from pieces into a real, editable tile).

Pieces box (main panel, cube sets):
    Find Pieces                     search every collection for tiles that can be each piece
    ✓ 00000011 [00001001 v] = 00001001 ↻1   Cube
                                    dropdown: any tile in the file that is this piece or a turn /
                                    mirror of it (piece_lib.py); the set builds with that tile
    ✗ 00010001 [        v] [+]      none anywhere: + adds a blocky stand-in (a half-size cube per
                                    solid corner) named by its hash, to model over or keep
    (new tiles go in rows in front of (-Y) everything in the scene, never on top of another set)
    Generate Built Tiles            every shape now built from pieces becomes a tile in the set: a
                                    root copy of the first piece, the other pieces as its children,
                                    all sharing the pieces' meshes (edit a piece, every generated
                                    tile follows; make one single user to change it on its own).
                                    Export joins the children, so the .glb is the same as before.
"""

import math

import bpy
from mathutils import Matrix, Vector

from . import cube
from .names import clean_name, set_slug
from .pieces import minimum_set
from .playground import tile_lookup
from .piece_lib import find_pieces, is_placeholder, set_tiles

PIECE_ROLE = "mc_piece_of"  # on generated tile parts: the hash they were generated for
SPACING = 2.0  # whole metres, so every new tile's box keeps whole-number corners


def _tile_name(h, collection):
    """The hash, or hash.setname when another set already owns the bare name (one per .blend)."""
    name = clean_name(h, 1)
    return name if bpy.data.objects.get(name) is None else f"{name}.{set_slug(collection)}"


ROW = 12  # new tiles per row


def _free_positions(collection, count):
    """Positions for `count` new tiles in rows in front of (-Y) everything in the scene, so they never
    land on another set: starting under the set's left edge, ROW per row, at cell centres (x.5, y.5, z.5)."""
    scene = bpy.context.scene
    skip = {"Playground", "Arrange", "MC_Export_Temp"}
    front = None
    for obj in scene.objects:
        if any(c.name.split(".")[0] in skip for c in obj.users_collection):
            continue
        for corner in obj.bound_box:
            y = (obj.matrix_world @ Vector(corner)).y
            front = y if front is None else min(front, y)
    roots = [o for o in collection.all_objects if o.parent is None]
    left = min((o.matrix_world.translation.x for o in roots), default=0.0)
    ground = min((o.matrix_world.translation.z for o in roots), default=0.0)
    # Cell centres (x.5, y.5, z.5): each tile's box has whole-number corners, like the playground.
    start = Vector((math.floor(left) + 0.5, math.floor((front or 0.0) - 1.0) - 0.5, math.floor(ground) + 0.5))
    return [start + Vector(((i % ROW) * SPACING, -(i // ROW) * SPACING, 0.0)) for i in range(count)]


def _placeholder_mesh(h):
    """Half-size cube in every solid corner of the cell (cell from -0.5 to 0.5)."""
    verts, faces = [], []
    quad = ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))
    for bit in range(8):
        if not (h >> bit) & 1:
            continue
        dx, dy, dz = cube_offset(bit)
        low = Vector((-0.5 + 0.5 * dx, -0.5 + 0.5 * dy, -0.5 + 0.5 * dz))
        base = len(verts)
        verts += [tuple(low + Vector((x, y, z))) for x in (0, 0.5) for y in (0, 0.5) for z in (0, 0.5)]
        faces += [tuple(base + i for i in f) for f in quad]
    mesh = bpy.data.meshes.new(f"{h:08b}_placeholder")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    return mesh


def cube_offset(bit):
    """Corner position in Blender axes (x right, y far, z up), same as playground.CORNER_OFFSETS."""
    return ((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1))[bit]


def _copy_parts(tile, collection, matrix):
    """Copy a tile and its child parts (sharing mesh data) into the set at `matrix`. Returns the root."""
    copies = {}
    for obj in [tile] + list(tile.children_recursive):
        copy = obj.copy()
        for key in ("mc_home", "mc_pg_home", "mc_filter_hidden", "mc_helper_debug"):
            if key in copy:
                del copy[key]
        collection.objects.link(copy)
        copies[obj] = copy
    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]
            copy.matrix_parent_inverse = obj.matrix_parent_inverse.copy()
            copy.matrix_basis = obj.matrix_basis.copy()
    copies[tile].matrix_world = matrix
    return copies[tile]


def generate_built(collection):
    """Turn every shape built from pieces into a tile of the set. Returns the new tiles."""
    settings = collection.mc_tiles
    tiles = set_tiles(collection)  # own tiles + borrowed pieces
    report = cube.check(tiles.keys(), settings.mode, pieces=True)
    lookup = tile_lookup(tiles.keys(), settings.mode)
    positions = _free_positions(collection, len(report.built))
    made = []
    for (h, found), position in zip(report.built, positions):
        name = _tile_name(h, collection)
        root = None
        for piece in found:
            source, turns, mirrored = lookup[piece]
            tile = tiles[source][0]
            own = tile.matrix_world.to_3x3().to_4x4()
            turn = Matrix.Rotation(turns * math.pi / 2.0, 4, "Z")
            flip = Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))) if mirrored else Matrix.Identity(4)
            part = _copy_parts(tile, collection, Matrix.Translation(position) @ turn @ flip @ own)
            part[PIECE_ROLE] = name
            if root is None:
                root = part
                root.name = name
            else:
                world = part.matrix_world.copy()
                part.parent = root
                part.matrix_world = world
                part.name = f"{name.split('.')[0]}_piece"
        made.append(root)
    return made


class MCTILES_OT_add_placeholder(bpy.types.Operator):
    """Add a blocky stand-in for a missing piece (a half-size cube per solid corner), named by its hash"""

    bl_idname = "mc_tiles.add_placeholder"
    bl_label = "Add Placeholder"
    bl_options = {"REGISTER", "UNDO"}

    hash: bpy.props.IntProperty(min=0, max=255)

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        name = _tile_name(self.hash, collection)
        obj = bpy.data.objects.new(name, _placeholder_mesh(self.hash))
        obj.location = _free_positions(collection, 1)[0]
        collection.objects.link(obj)
        find_pieces(collection)  # use it if nothing better exists anywhere
        self.report({"INFO"}, f"Added placeholder {name}")
        return {"FINISHED"}


class MCTILES_OT_generate_built(bpy.types.Operator):
    """Make every shape built from pieces a real tile in the set (linked piece copies you can edit)"""

    bl_idname = "mc_tiles.generate_built"
    bl_label = "Generate Built Tiles"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def execute(self, context):
        made = generate_built(context.scene.mc_tiles_active)
        self.report({"INFO"}, f"Generated {len(made)} tiles from pieces")
        return {"FINISHED"}


def _how(source, turns, mirrored):
    return f"{source:08b}" + (f" ↻{turns}" if turns else "") + (" ⇋" if mirrored else "")


class MCTILES_OT_delete_generated(bpy.types.Operator):
    """Delete the set's generated tiles (made by Generate Built Tiles); the pieces themselves stay"""

    bl_idname = "mc_tiles.delete_generated"
    bl_label = "Delete Generated Tiles"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        doomed = [o for o in collection.all_objects if PIECE_ROLE in o]
        for obj in doomed:
            bpy.data.objects.remove(obj, do_unlink=True)
        self.report({"INFO"}, f"Deleted {len(doomed)} generated objects")
        return {"FINISHED"}


class MCTILES_OT_find_pieces(bpy.types.Operator):
    """Search every collection for tiles that can be this set's pieces and pick one for each"""

    bl_idname = "mc_tiles.find_pieces"
    bl_label = "Find Pieces"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def execute(self, context):
        chosen = find_pieces(context.scene.mc_tiles_active)
        found = sum(1 for obj in chosen.values() if obj is not None)
        self.report({"INFO"}, f"Pieces: {found} of {len(chosen)} found")
        return {"FINISHED"}


def draw_pieces(layout, collection, tiles):
    """Pieces box: a source dropdown per minimum piece (any collection), placeholders, Generate."""
    settings = collection.mc_tiles
    if settings.kind != "CUBE":
        return
    needed = minimum_set(settings.mode)
    items = {item.hash: item for item in settings.piece_sources}
    box = layout.box()
    row = box.row(align=True)
    have = sum(1 for h in needed if h in items and items[h].source is not None)
    row.label(text=f"Pieces: {have} of {len(needed)}", icon="MOD_BOOLEAN")
    row.operator("mc_tiles.find_pieces", text="Find Pieces", icon="VIEWZOOM")
    if set(items) != set(needed):
        box.label(text="Press Find Pieces to pick them from any collection", icon="INFO")
        return
    lookup = tile_lookup(tiles.keys(), settings.mode)
    col = box.column(align=True)
    for h in needed:
        item = items[h]
        row = col.row(align=True)
        row.label(text=f"{h:08b}", icon="CHECKMARK" if item.source is not None else "X")
        row.prop(item, "source", text="")
        if item.source is None:
            row.operator("mc_tiles.add_placeholder", text="", icon="ADD").hash = h
        else:
            how = _how(*lookup[h]) if h in lookup else "?"
            sub = row.row()
            sub.label(text=f"= {how}" + ("  (placeholder)" if is_placeholder(item.source) else ""))
            owners = ", ".join(c.name for c in item.source.users_collection)
            sub.label(text=owners)
    built = cube.check(tiles.keys(), settings.mode, pieces=True).built
    generated = sum(1 for o in collection.all_objects if PIECE_ROLE in o and o.parent is None)
    row = box.row(align=True)
    if built:
        row.operator("mc_tiles.generate_built", text=f"Generate {len(built)} Built Tiles", icon="MOD_BOOLEAN")
    if generated:
        row.operator("mc_tiles.delete_generated", text=f"Delete {generated} Generated", icon="TRASH")


CLASSES = (MCTILES_OT_add_placeholder, MCTILES_OT_generate_built, MCTILES_OT_delete_generated, MCTILES_OT_find_pieces)
