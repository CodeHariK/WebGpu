"""Playground: the selected tile in the middle of a 3x3x3 block, neighbours filled from the set.

How it works (the same rules Godot uses):
    1. A 4x4x4 grid of corners. The centre cell's 8 corners come from the selected tile;
       every other corner is random (Shuffle picks new ones). In a Heightmap set each corner
       column gets a random height instead, so there are no overhangs.
    2. Every cell's hash is read from its 8 corners. The tile for it is a modelled tile turned
       0-3 quarter turns about Z (Godot's Y), exactly as MCNode fills its library.
    3. The tile is copied into the cell. Copies share mesh data with the real tile, so editing
       the real mesh updates every copy live. A cell whose hash has no tile gets a red box.

Copies live in a "Playground" collection and are named "pg_..." so they never look like tiles.
Every copy (centre and neighbours) is selectable and linked: Tab into Edit Mode on any of them edits
the real tile's mesh (shared data), and every copy of that tile updates too. A copy remembers its
tile (obj["mc_pg_tile"]), so Build with a copy selected re-centres on that tile. With Helper Debug
Material on, neighbours get the debug material (role stored in obj["mc_pg_role"]).
The add-on's Export Set skips them; clear the playground before using the old
ExportMergeChildMesh.py script (it exports everything visible).

Corner order inside a cell, in Blender axes (Z up, -Y front), same as Godot's C0..C7:
    C0 (x, y, z)   C1 (x+1, y, z)   C2 (x+1, y+1, z)   C3 (x, y+1, z)   C4..C7 = same at z+1
"""

import math
import random

import bpy
from mathutils import Matrix, Vector

from . import cube
from . import view
from .tiles import tile_hash, tile_variants
from .variants import variant_index

COLLECTION_NAME = "Playground"
MISSING_MATERIAL = "MC_Missing"
SIZE = 3  # cells per side; the selected tile is the middle one
CENTRE = (1, 1, 1)
CORNER_OFFSETS = (
    (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
    (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
)
GAP = 3.0  # metres between the tile set and the playground
ROLE = "mc_pg_role"  # "centre", "neighbour" or "pasted" on every tile copy
HOME = "mc_pg_home"  # a real tile's own position while a seam pair has moved it next to the playground
REF_LABEL = "mc_ar_label"  # label drawn by Hash Labels (labels.py) on seam pair copies
SOURCE = "mc_pg_tile"  # name of the real tile a copy (root) was made from
TURNS = "mc_pg_turns"  # quarter turns about Z of a copy (root)
MIRRORED = "mc_pg_mirrored"  # copy (root) is the left/right mirror


# ---------------------------------------------------------------- corner grid (pure logic)


def random_corners(centre_hash, heightmap, rng):
    """{(x, y, z): solid} for the 4x4x4 corners around the centre cell."""
    n = SIZE + 1
    fixed = {}
    for bit, (dx, dy, dz) in enumerate(CORNER_OFFSETS):
        fixed[(CENTRE[0] + dx, CENTRE[1] + dy, CENTRE[2] + dz)] = bool((centre_hash >> bit) & 1)

    corners = {}
    if not heightmap:
        for x in range(n):
            for y in range(n):
                for z in range(n):
                    key = (x, y, z)
                    corners[key] = fixed[key] if key in fixed else rng.random() < 0.5
        return corners

    # Heightmap: each corner column is solid up to a height; the centre fixes its 4 columns.
    for x in range(n):
        for y in range(n):
            low, high = 0, n
            if (x, y, 1) in fixed:
                bottom, top = fixed[(x, y, 1)], fixed[(x, y, 2)]
                if top:
                    low, high = 3, n  # solid through z = 2
                elif bottom:
                    low, high = 2, 2  # solid z <= 1 only
                else:
                    low, high = 0, 1  # empty from z = 1 up
            height = rng.randint(low, high)
            for z in range(n):
                corners[(x, y, z)] = z < height
    corners.update(fixed)  # a centre tile with an overhang keeps it
    return corners


def cell_hash(corners, cell):
    """The 8-bit hash of a cell from its corners."""
    h = 0
    for bit, (dx, dy, dz) in enumerate(CORNER_OFFSETS):
        if corners[(cell[0] + dx, cell[1] + dy, cell[2] + dz)]:
            h |= 1 << bit
    return h


def tile_lookup(tiles, mode):
    """{hash: (tile hash, quarter turns, mirrored)}: which tile, flipped or not, turned how.

    Modelled tiles first; shapes only reachable as a left/right flip use the flipped tile, the
    same tile Export Set writes for them.
    """
    lookup = {}
    turns_allowed = 4 if cube.MODE_ROTATIONS[mode] else 1
    for mirrored in (False, True):
        for t in sorted(tiles):
            h = cube.mirror(t) if mirrored else t
            for turns in range(turns_allowed):
                lookup.setdefault(h, (t, turns, mirrored))
                h = cube.permute(h, cube.ROTATE_Y)
    return lookup


# ---------------------------------------------------------------- Blender objects


def _playground_collection(scene):
    collection = bpy.data.collections.get(COLLECTION_NAME)
    if collection is None:
        collection = bpy.data.collections.new(COLLECTION_NAME)
    if collection not in set(scene.collection.children_recursive):
        scene.collection.children.link(collection)
    return collection


def clear(scene):
    """Delete every object in the Playground collection (the real tiles are never touched)."""
    from .seams_ui import clear_shown  # the seam points belong to the pair being cleared

    clear_shown()
    for obj in scene.objects:  # real tiles a seam pair moved next to the playground go back
        if HOME in obj:
            obj.matrix_world.translation = Vector(obj[HOME])
            del obj[HOME]
    collection = bpy.data.collections.get(COLLECTION_NAME)
    if collection is None:
        return
    for obj in list(collection.all_objects):
        view.remove_debug(obj)  # drops any material slot added to the shared mesh
        bpy.data.objects.remove(obj, do_unlink=True)


def _missing_material():
    mat = bpy.data.materials.get(MISSING_MATERIAL) or bpy.data.materials.new(MISSING_MATERIAL)
    mat.diffuse_color = (0.9, 0.15, 0.15, 1.0)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = (0.9, 0.15, 0.15, 1.0)
    return mat


def _missing_box(collection, position, h):
    """A red 0.6 m cube marking a cell whose hash has no tile."""
    name = f"pg_missing_{h:08b}"
    mesh = bpy.data.meshes.get("pg_missing_box")
    if mesh is None:
        mesh = bpy.data.meshes.new("pg_missing_box")
        s = 0.3
        verts = [(x, y, z) for x in (-s, s) for y in (-s, s) for z in (-s, s)]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        mesh.from_pydata(verts, [], faces)
        mesh.materials.append(_missing_material())
    obj = bpy.data.objects.new(name, mesh)
    obj.location = position
    obj.hide_select = True
    collection.objects.link(obj)


def _copy_tile(root, collection, position, turns, h, role="neighbour", mirrored=False):
    """Copy a tile (root and child parts, sharing mesh data) into a cell, turned about Z.

    role: "centre", "neighbour" (gets the debug material when it is on) or "pasted" (from Godot,
    real material). All are selectable; Edit Mode on any copy edits the real tile's mesh.
    """
    copies = {}
    for obj in [root] + list(root.children_recursive):
        copy = obj.copy()  # shares obj.data: edits to the real mesh show up here
        copy.name = f"pg_{h:08b}_{obj.name}"
        copy.hide_select = False
        copy[ROLE] = role
        collection.objects.link(copy)
        copies[obj] = copy
    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]
            # Re-parenting can change these: put back the original's, so the part sits on its tile.
            copy.matrix_parent_inverse = obj.matrix_parent_inverse.copy()
            copy.matrix_basis = obj.matrix_basis.copy()
    # Keep the tile's own rotation/scale, add the turn, move to the cell centre.
    own = root.matrix_world.to_3x3().to_4x4()
    turn = Matrix.Rotation(turns * math.pi / 2.0, 4, "Z")
    # A mirrored tile is a negative X scale: still the shared mesh (live edits), drawn flipped.
    flip = Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))) if mirrored else Matrix.Identity(4)
    copies[root].matrix_world = Matrix.Translation(position) @ turn @ flip @ own
    copies[root][SOURCE] = root.name
    copies[root][TURNS] = turns
    copies[root][MIRRORED] = mirrored
    return copies[root]


def _origin(collection, near=None, block=SIZE):
    """Lower corner of a `block`-cell playground, on whole-metre grid lines.

    near (a tile object): one empty cell to the right of the cell that tile sits in, the middle
    row / layer level with it. Without one: to the right of the whole tile set.
    """
    if near is not None:
        p = near.matrix_world.translation
        cell = Vector((math.floor(p.x), math.floor(p.y), math.floor(p.z)))  # the cell the tile sits in
        middle = block // 2
        return cell + Vector((2.0, -middle, -middle))
    xs = [o.matrix_world.translation.x for o in collection.all_objects if o.parent is None]
    ys = [o.matrix_world.translation.y for o in collection.all_objects if o.parent is None]
    if not xs:
        return Vector((0.0, 0.0, 0.0))
    return Vector((math.ceil(max(xs) + GAP), math.floor(min(ys)), 0.0))


def build(scene, tile_set, centre_hash, seed, near=None):
    """Rebuild the playground (next to tile `near` if given). Returns (cells filled, hashes with no tile)."""
    settings = tile_set.mc_tiles
    tiles = tile_variants(tile_set, "CUBE")
    lookup = tile_lookup(tiles.keys(), settings.mode)
    heightmap = settings.mode == "HEIGHTMAP" or scene.mc_tiles_pg_no_overhang
    corners = random_corners(centre_hash, heightmap, random.Random(seed))

    clear(scene)
    collection = _playground_collection(scene)
    origin = _origin(tile_set, near)
    filled = 0
    missing = []
    centre_copy = None
    for x in range(SIZE):
        for y in range(SIZE):
            for z in range(SIZE):
                h = cell_hash(corners, (x, y, z))
                if h == cube.EMPTY:
                    continue
                position = origin + Vector((x + 0.5, y + 0.5, z + 0.5))
                if h in lookup:
                    base, turns, mirrored = lookup[h]
                    is_centre = (x, y, z) == CENTRE
                    role = "centre" if is_centre else "neighbour"
                    options = tiles[base]
                    tile = options[variant_index(x, y, z, len(options))]
                    root = _copy_tile(tile, collection, position, turns, h, role, mirrored)
                    if is_centre:
                        centre_copy = root
                    filled += 1
                elif h != cube.FULL:  # full is optional: an empty spot is fine
                    _missing_box(collection, position, h)
                    missing.append(h)
    view.refresh(scene)  # neighbours get the debug material if it is on
    _select_only(centre_copy)
    return filled, missing


def build_pair(scene, tile_set, a, b, axis, near=None):
    """Put two library entries side by side (A low, B one cell further along axis), all editable.

    a, b: (object name, hash, quarter turns, mirrored); a name of "" leaves that cell empty.
    near: a tile to put the pair next to. Returns (A position, B position) of the cell centres.
    The real A and B tiles (the meshes to fix) are moved next to the pair while it is shown; clearing
    or rebuilding the playground puts them back (their own position is kept in obj["mc_pg_home"]).
    """
    clear(scene)
    collection = _playground_collection(scene)
    origin = _origin(tile_set, near, block=1) + Vector((0.5, 0.5, 0.5))
    step = Vector([1.0 if i == axis else 0.0 for i in range(3)])
    positions = (origin, origin + step)
    copies = []
    for (name, h, turns, mirrored), position, tag in zip((a, b), positions, ("A", "B")):
        tile = bpy.data.objects.get(name) if name else None
        if tile is not None:
            copy = _copy_tile(tile, collection, position, turns, h, "pasted", mirrored)
            copy[REF_LABEL] = f"{h:08b} {tag}"  # the shape in that cell, as Godot draws it
            copies.append(copy)
    # The real tiles, in a row after the pair (+X), one empty cell between each.
    x = 3.0 if axis == 0 else 2.0
    for name in dict.fromkeys(n for n, _, _, _ in (a, b) if n):  # a tile against itself: move it once
        tile = bpy.data.objects.get(name)
        if tile is None:
            continue
        if HOME not in tile:
            tile[HOME] = list(tile.matrix_world.translation)
        tile.matrix_world.translation = origin + Vector((x, 0.0, 0.0))
        x += 2.0
    view.refresh(scene)
    if copies:
        _select_only(copies[0])
        for obj in copies[1:]:
            obj.select_set(True)
    return positions


def _select_only(obj):
    """Select the centre copy, ready to Tab into Edit Mode."""
    if obj is None:
        return
    bpy.context.view_layer.update()  # new objects show up in the view layer after a sync
    if obj.name not in bpy.context.view_layer.objects:
        return
    for other in bpy.context.selected_objects:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


# ---------------------------------------------------------------- operators and panel


def _selected_tile(context):
    """(root object, hash) of the selected tile, or (None, None). A playground copy counts as its tile."""
    obj = context.active_object
    while obj is not None and obj.parent is not None:
        obj = obj.parent
    if obj is not None and obj.name.startswith("pg_"):
        obj = bpy.data.objects.get(obj.get(SOURCE, ""))
    if obj is None:
        return None, None
    h = tile_hash(obj.name, "CUBE")
    return (obj, h) if h is not None else (None, None)


def _run(operator, context, new_seed):
    scene = context.scene
    tile_set = scene.mc_tiles_active
    if tile_set is None or tile_set.mc_tiles.kind != "CUBE":
        operator.report({"WARNING"}, "Pick a cube tile set first")
        return {"CANCELLED"}
    obj, h = _selected_tile(context)
    if obj is None:
        h = scene.mc_tiles_pg_hash  # Shuffle with nothing selected keeps the last centre
        if h < 0:
            operator.report({"WARNING"}, "Select a tile first")
            return {"CANCELLED"}
        obj = tile_variants(tile_set, "CUBE").get(h, [None])[0]
    if new_seed:
        scene.mc_tiles_pg_seed += 1
    scene.mc_tiles_pg_hash = h
    filled, missing = build(scene, tile_set, h, scene.mc_tiles_pg_seed, near=obj)
    text = f"Playground around {h:08b}: {filled} tiles"
    if missing:
        text += f", {len(missing)} missing: " + ", ".join(f"{m:08b}" for m in sorted(set(missing)))
    operator.report({"WARNING"} if missing else {"INFO"}, text)
    return {"FINISHED"}


class MCTILES_OT_playground_for(bpy.types.Operator):
    """Build the playground around this tile, next to it"""

    bl_idname = "mc_tiles.playground_for"
    bl_label = "Playground for Tile"
    bl_options = {"REGISTER", "UNDO"}

    name: bpy.props.StringProperty()

    def execute(self, context):
        obj = bpy.data.objects.get(self.name)
        if obj is None or obj.name not in context.view_layer.objects:
            self.report({"WARNING"}, f"{self.name} is not in this view layer")
            return {"CANCELLED"}
        for other in context.selected_objects:
            other.select_set(False)
        obj.select_set(True)
        context.view_layer.objects.active = obj
        return _run(self, context, new_seed=False)


class MCTILES_OT_playground_build(bpy.types.Operator):
    """Put the selected tile in the middle of a 3x3x3 block and fill its neighbours from the set"""

    bl_idname = "mc_tiles.playground_build"
    bl_label = "Build Playground"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        return _run(self, context, new_seed=False)


class MCTILES_OT_playground_shuffle(bpy.types.Operator):
    """Same centre tile, new random neighbours"""

    bl_idname = "mc_tiles.playground_shuffle"
    bl_label = "Shuffle"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        return _run(self, context, new_seed=True)


class MCTILES_OT_playground_clear(bpy.types.Operator):
    """Delete the playground copies (real tiles are untouched)"""

    bl_idname = "mc_tiles.playground_clear"
    bl_label = "Clear Playground"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        clear(context.scene)
        return {"FINISHED"}


def draw_playground(layout, context):
    scene = context.scene
    obj, h = _selected_tile(context)
    if obj is not None:
        layout.label(text=f"Centre: {obj.name}", icon="MESH_CUBE")
    elif scene.mc_tiles_pg_hash >= 0:
        layout.label(text=f"Centre: {scene.mc_tiles_pg_hash:08b} (last)", icon="MESH_CUBE")
    else:
        layout.label(text="Select a tile", icon="INFO")
    row = layout.row(align=True)
    row.operator("mc_tiles.playground_build", icon="VIEW_ORTHO")
    row.operator("mc_tiles.playground_shuffle", text="", icon="FILE_REFRESH")
    row.operator("mc_tiles.playground_clear", text="", icon="TRASH")
    layout.operator("mc_tiles.paste_from_godot", icon="PASTEDOWN")
    if scene.mc_tiles_active and scene.mc_tiles_active.mc_tiles.mode != "HEIGHTMAP":
        layout.prop(scene, "mc_tiles_pg_no_overhang")


CLASSES = (
    MCTILES_OT_playground_build, MCTILES_OT_playground_for, MCTILES_OT_playground_shuffle,
    MCTILES_OT_playground_clear,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.mc_tiles_pg_seed = bpy.props.IntProperty(name="Seed", default=1)
    bpy.types.Scene.mc_tiles_pg_hash = bpy.props.IntProperty(name="Centre", default=-1)
    bpy.types.Scene.mc_tiles_pg_no_overhang = bpy.props.BoolProperty(
        name="No Overhangs",
        description="Neighbours like a heightmap terrain (solid from the bottom up)",
        default=True,
    )


def unregister():
    del bpy.types.Scene.mc_tiles_pg_no_overhang
    del bpy.types.Scene.mc_tiles_pg_hash
    del bpy.types.Scene.mc_tiles_pg_seed
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
