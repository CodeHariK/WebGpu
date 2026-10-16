"""Playground: the selected tile in a 4x4x4 block, neighbours filled from the set (as the BrickBlock script).

How it works:
    1. A 5x5x5 grid of corners. The selected tile's cell keeps its 8 corners. Its cell is (1, 1, 1)
       for an M tile, in the bottom layer for an L tile and in the top layer for an H tile.
    2. Corners on the block's 4 side faces and its top face are empty; the bottom face (the ground)
       stays random. Shuffle picks new random corners.
    3. Level rules (height only decides what sits on what; beside a joined neighbour, same level):
         L  bottom layer only (M there when the set has no L tiles).
         H  a roof cell above the bottom layer (top of its column: nothing solid on top). Any layer,
            not only the top one. A whole roof region joined (solid corner on a shared side wall) to
            a non-H cell is M instead, so H only sits beside H.
         M  every other cell above the bottom layer; M roof shapes can top a column too.
       Each level uses only its own role's tiles and chosen pieces: turned 0-3 quarter turns about Z
       (Godot's Y), then mirrored, then built from that level's pieces. The role is the selected
       tile's group (a if the set has none of that group). Variant number: Godot's per-cell pick
       (variants.variant_index) for the copy's world cell, shifted by the seed so Shuffle varies it.
    4. The bottom layer's corners are solved (backtracking) so every bottom cell is an L shape. The
       rest are random (a Heightmap set: random column heights). Roof cells spread to the side
       neighbours they are joined to (their top corners cleared) so H regions stay whole.
    5. Up to PLAN_TRIES random layouts are planned (pure logic, fast) and the one with the fewest
       cells that have no tile at their level is built. A cell with no tile gets a red box.
    6. The block sits in its own row behind the tile set, on the ground, the selected tile's cell in
       the tile's column.

Copies share mesh data with the real tile, so editing the real mesh updates every copy live. They live
in a "Playground" collection, are named "pg_<cell hash>_<tile>" and carry a label ("10111000 ma1
00011101 r90 m": cell hash, tag and tile hash, turn, mirrored) for Hash Labels / Corner Boxes.
Every copy is selectable: Tab into Edit Mode edits the real tile's mesh. A copy remembers its tile
(obj["mc_pg_tile"]), so Build with a copy selected re-centres on that tile. With Helper Debug Material
on, neighbours get the debug material (role stored in obj["mc_pg_role"]). Export Set skips them.

Corner order inside a cell, in Blender axes (Z up, -Y front), same as Godot's C0..C7:
    C0 (x, y, z)   C1 (x+1, y, z)   C2 (x+1, y+1, z)   C3 (x, y+1, z)   C4..C7 = same at z+1
"""

import math
import random

import bpy
from mathutils import Matrix, Vector

from . import cube
from . import view
from .tiles import tile_hash, tile_variants, tiles_by_role
from .variants import MAIN_ROLE, name_tag, tag_text, variant_index

COLLECTION_NAME = "Playground"
MISSING_MATERIAL = "MC_Missing"
SIZE = 4  # cells per side
TILE_CELL = (1, 1, 1)  # where an M tile sits (an L tile goes in the bottom layer, an H tile in the top)
CENTRE = TILE_CELL
CORNER_OFFSETS = (
    (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
    (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
)
TOP_BITS = (4, 5, 6, 7)
SIDES = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0))
# Outer faces whose corners are always empty, as (axis, side): the 4 sides and the top. The bottom face
# is the ground and stays random (empty, every bottom cell would have only top corners: never an L shape).
EMPTY_FACES = ((0, -1), (0, 1), (1, -1), (1, 1), (2, 1))
PLAN_TRIES = 300  # random layouts tried per Build / Shuffle; the one with the fewest gaps is built
GAP = 3.0  # metres between the tile set and the playground
ROLE = "mc_pg_role"  # "centre", "neighbour" or "pasted" on every tile copy
HOME = "mc_pg_home"  # a real tile's own position while a seam pair has moved it next to the playground
REF_LABEL = "mc_ar_label"  # label drawn by Hash Labels (labels.py) on playground / seam pair copies
SOURCE = "mc_pg_tile"  # name of the real tile a copy (root) was made from
TURNS = "mc_pg_turns"  # quarter turns about Z of a copy (root)
MIRRORED = "mc_pg_mirrored"  # copy (root) is the left/right mirror
LEVEL = "mc_pg_level"  # "l", "m" or "h": the level a playground cell was filled as


# ---------------------------------------------------------------- corner grid (pure logic)


def tile_cell(height):
    """The selected tile's cell for its height letter: L bottom layer, H top layer, M (1, 1, 1)."""
    return TILE_CELL[:2] + ({"l": 0, "h": SIZE - 1}.get(height, TILE_CELL[2]),)


def _keys(cell):
    return [(cell[0] + dx, cell[1] + dy, cell[2] + dz) for dx, dy, dz in CORNER_OFFSETS]


def _inside(cell):
    return all(0 <= c < SIZE for c in cell)


def _roof(grid, cell):
    """Something solid, nothing solid on top."""
    ks = _keys(cell)
    return any(grid[k] for k in ks) and not any(grid[ks[i]] for i in TOP_BITS)


def _joined(grid, a, b):
    """A solid corner on the wall two side neighbours share."""
    return any(grid[k] for k in set(_keys(a)) & set(_keys(b)))


def solve_layer(grid, cells, ok, rng, budget=20000):
    """Fill the unset corners of `cells` (backtracking, random order) so every cell's hash is in `ok`.
    On failure the cells' free corners are left unset (filled at random later)."""
    left = [budget]

    def solve(index):
        if index == len(cells):
            return True
        left[0] -= 1
        if left[0] < 0:
            return False
        keys = _keys(cells[index])
        free = [k for k in dict.fromkeys(keys) if k not in grid]
        options = list(range(1 << len(free)))
        rng.shuffle(options)
        for option in options:
            for b, k in enumerate(free):
                grid[k] = bool((option >> b) & 1)
            if sum(1 << i for i, k in enumerate(keys) if grid[k]) in ok and solve(index + 1):
                return True
        for k in free:
            del grid[k]
        return False

    return solve(0)


def spread_roofs(grid, fixed):
    """Every roof cell above the bottom layer makes the side neighbours joined to it roofs too, by
    clearing their top corners (never a corner in `fixed`). Repeats until nothing changes."""
    cells = [(i, j, k) for i in range(SIZE) for j in range(SIZE) for k in range(1, SIZE)]
    changed = True
    while changed:
        changed = False
        for cell in cells:
            if not _roof(grid, cell):
                continue
            for d in SIDES:
                other = tuple(cell[a] + d[a] for a in range(3))
                if not _inside(other) or _roof(grid, other) or not _joined(grid, cell, other):
                    continue
                tops = [k for k in (_keys(other)[i] for i in TOP_BITS) if grid[k]]
                if not any(k in fixed for k in tops):  # a neighbour keeping the tile's corner stays
                    for k in tops:
                        grid[k] = False
                    changed = True


def h_cells(grid, centre, centre_is_h):
    """Roof cells above the bottom layer that get H; a region still joined to a non-H cell stays M, so
    H only ever sits beside H. The selected tile's cell counts as H only when its tile is H."""
    found = {(i, j, k) for i in range(SIZE) for j in range(SIZE) for k in range(1, SIZE)
             if _roof(grid, (i, j, k))}
    if centre_is_h:
        found.add(centre)
    else:
        found.discard(centre)
    changed = True
    while changed:
        changed = False
        for cell in list(found):
            for d in SIDES:
                other = tuple(cell[a] + d[a] for a in range(3))
                if _inside(other) and other not in found and _joined(grid, cell, other):
                    found.discard(cell)
                    changed = True
                    break
    return found


def _fill_heightmap(grid, rng):
    """Unset corners from a random height per corner column, between the corners already set."""
    n = SIZE + 1
    for x in range(n):
        for y in range(n):
            column = [grid.get((x, y, z)) for z in range(n)]
            low = max((z + 1 for z, v in enumerate(column) if v is True), default=0)
            high = min((z for z, v in enumerate(column) if v is False), default=n)
            height = rng.randint(low, high) if low <= high else high
            for z, v in enumerate(column):
                if v is None:
                    grid[(x, y, z)] = z < height


def playground_corners(centre_hash, centre, rng, bottom_ok=None, roofs=False, centre_is_h=False,
                       heightmap=False):
    """(corners {(x, y, z): solid}, H cells, bottom layer solved or None when not asked).

    bottom_ok: hashes every bottom cell should have (the L shapes); roofs: spread roofs and find the
    H cells (the set has H tiles)."""
    n = SIZE + 1
    grid = {k: bool((centre_hash >> bit) & 1) for bit, k in enumerate(_keys(centre))}
    fixed = set(grid)
    for key in ((i, j, k) for i in range(n) for j in range(n) for k in range(n)):
        if key not in grid and any(key[axis] == (0 if side < 0 else n - 1) for axis, side in EMPTY_FACES):
            grid[key] = False
    solved = None
    if bottom_ok:
        bottom = [(i, j, 0) for i in range(SIZE) for j in range(SIZE)]
        solved = solve_layer(grid, bottom, set(bottom_ok) | {cube.EMPTY, cube.FULL}, rng)
    if heightmap:
        _fill_heightmap(grid, rng)
    for key in ((i, j, k) for i in range(n) for j in range(n) for k in range(n)):
        if key not in grid:
            grid[key] = rng.random() < 0.5
    found = set()
    if roofs:
        spread_roofs(grid, fixed)
        found = h_cells(grid, centre, centre_is_h)
    return grid, found, solved


def cell_level(cell, found, centre, centre_height, have):
    """The level ("l", "m" or "h") of a cell. have: the levels the set has tiles for."""
    if cell == centre:
        return centre_height
    if cell[2] == 0:
        return "l" if "l" in have else "m"
    return "h" if cell in found else "m"


ORDER = {"l": 0, "m": 1, "h": 2}  # up a column: L, then M, then H; nothing above an H


def column_breaks(corners, cells):
    """Cells that break the column order: a level lower than one below it, or anything above an H."""
    breaks = 0
    for x in range(SIZE):
        for y in range(SIZE):
            top = -1
            for z in range(SIZE):
                h = cell_hash(corners, (x, y, z))
                if h == cube.EMPTY:
                    continue
                rank = ORDER[cells[(x, y, z)]]
                if top == ORDER["h"] or rank < top:
                    breaks += 1
                top = max(top, rank)
    return breaks


def plan(centre_hash, centre, centre_height, levels, seed, heightmap=False, tries=PLAN_TRIES):
    """(corners, {cell: level}, cells with no tile, bottom solved) for the best of `tries` layouts.

    levels: {height: {hash: placements}}. A cell is a gap when its hash (not empty / full) has no tile
    at its level. A layout breaking the column order (column_breaks) is only used when none keeps it.
    Stops at the first layout with no gaps and no breaks.
    """
    best = None
    for attempt in range(tries):
        rng = random.Random(f"{seed}:{attempt}")
        corners, found, solved = playground_corners(
            centre_hash, centre, rng,
            bottom_ok=levels["l"].keys() if "l" in levels else None,
            roofs="h" in levels, centre_is_h=centre_height == "h", heightmap=heightmap)
        cells = {}
        gaps = 0
        for cell in ((x, y, z) for x in range(SIZE) for y in range(SIZE) for z in range(SIZE)):
            level = cell_level(cell, found, centre, centre_height, levels)
            cells[cell] = level
            h = cell_hash(corners, cell)
            if cell != centre and h not in (cube.EMPTY, cube.FULL) and h not in levels.get(level, {}):
                gaps += 1
        score = (column_breaks(corners, cells), gaps)
        if best is None or score < best[0]:
            best = (score, (corners, cells, gaps, solved))
        if score == (0, 0):
            break
    return best[1]


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


def tile_placements(tiles, mode, pieces=False):
    """{hash: [(tile hash, quarter turns, mirrored), ...]}: one entry for a tile (turned / mirrored),
    several for a shape built from pieces (pieces.py), placed together in the same cell."""
    from .pieces import cover, drawable

    lookup = tile_lookup(tiles, mode)
    out = {h: [placed] for h, placed in lookup.items()}
    if pieces:
        shapes = drawable(tiles, mode)
        for h in range(1, 255):
            if h not in out and cube.allowed(h, mode):
                found = cover(h, shapes)
                if found:
                    out[h] = [lookup[p] for p in found]
    return out


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
    """First cell of a `block`-cell playground, as whole numbers. Cell (i, j, k) is drawn centred at
    (i + 0.5, j + 0.5, k + 0.5): every cell's bounding box has whole-number corners (see cell_centre).

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


def cell_centre(origin, x, y, z):
    """Where a playground cell's tile goes: its centre, so the cell's box corners are whole numbers."""
    return origin + Vector((x + 0.5, y + 0.5, z + 0.5))


def _block_origin(tile_set, near, centre):
    """First cell of the block, like the script: in its own row behind the tile set (+Y, GAP metres
    clear), on the set's ground level, the selected tile's cell in the tile's own column."""
    roots = [o for o in tile_set.all_objects if o.parent is None]
    if not roots:
        return Vector((0.0, 0.0, 0.0))
    ys = [o.matrix_world.translation.y for o in roots]
    zs = [o.matrix_world.translation.z for o in roots]
    x = math.floor(near.matrix_world.translation.x) - centre[0] if near is not None else \
        math.floor(min(o.matrix_world.translation.x for o in roots))
    return Vector((x, math.ceil(max(ys) + GAP), math.floor(min(zs))))


def _placed(tiles, mode, pieces=True):
    """{hash: [(objects, quarter turns, mirrored, tile hash), ...]} for `tiles` ({hash: [objects]});
    pieces: also shapes built from the tiles (the set's Build from Pieces)."""
    return {h: [(tiles[base], turns, mirrored, base) for base, turns, mirrored in placed]
            for h, placed in tile_placements(tiles.keys(), mode, pieces=pieces).items()}


def _level_tiles(tile_set, group):
    """{height: {hash: [(objects, turns, mirrored, tile hash), ...]}} for L, M and H in `group` (group a
    when the set has none of that group); a height with nothing is left out. Each level has only its
    own role's tiles (piece_lib.set_tiles: for M a with its chosen M a pieces), so an L wall piece
    never fills an M cell; shapes a level has no tile for are built from that level's own tiles."""
    from .piece_lib import set_tiles

    mode = tile_set.mc_tiles.mode
    have = tiles_by_role(tile_set, "CUBE")
    out = {}
    for height in ("l", "m", "h"):
        role = (height, group) if (height, group) in have else (height, "a")
        tiles = set_tiles(tile_set, role)
        if tiles:
            out[height] = _placed(tiles, mode, tile_set.mc_tiles.build_from_pieces)
    return out


def godot_cell(position):
    """The Godot grid cell (x, y up, z towards the viewer) of a Blender cell centre."""
    return math.floor(position.x), math.floor(position.z), -math.floor(position.y) - 1


def build(scene, tile_set, centre_hash, seed, near=None):
    """Rebuild the playground (next to tile `near` if given).

    Returns (cells filled, hashes with no tile, note): note is a warning or "".
    """
    settings = tile_set.mc_tiles
    height, group, _ = name_tag(near.name) if near is not None else (*MAIN_ROLE, 1)
    levels = _level_tiles(tile_set, group)
    centre = tile_cell(height)
    corners, cell_levels, _, solved = plan(centre_hash, centre, height, levels, seed,
                                           heightmap=settings.mode == "HEIGHTMAP")
    note = "" if solved is not False else "no all-L bottom layer fits this tile"

    clear(scene)
    collection = _playground_collection(scene)
    origin = _block_origin(tile_set, near, centre)
    filled = 0
    missing = []
    centre_copy = None
    for x in range(SIZE):
        for y in range(SIZE):
            for z in range(SIZE):
                cell = (x, y, z)
                h = cell_hash(corners, cell)
                if h == cube.EMPTY:
                    continue
                position = cell_centre(origin, x, y, z)
                if cell == centre and near is not None:  # the selected tile itself, as it is
                    centre_copy = _copy_tile(near, collection, position, 0, h, "centre")
                    centre_copy[REF_LABEL] = f"{h:08b} {tag_text(*name_tag(near.name))} {h:08b} r0"
                    centre_copy[LEVEL] = height
                    filled += 1
                    continue
                level = cell_levels[cell]
                if h not in levels.get(level, {}):
                    if h != cube.FULL:  # full is optional: an empty spot is fine
                        _missing_box(collection, position, h)
                        missing.append(h)
                    continue
                role = "centre" if cell == centre else "neighbour"
                for options, turns, mirrored, base in levels[level][h]:  # several for a shape from pieces
                    gx, gy, gz = godot_cell(position)  # same pick as Godot at this world cell,
                    tile = options[variant_index(gx + seed * SIZE, gy, gz, len(options))]  # moved by Shuffle
                    root = _copy_tile(tile, collection, position, turns, h, role, mirrored)
                    root[REF_LABEL] = (f"{h:08b} {tag_text(*name_tag(tile.name))} {base:08b} r{turns * 90}"
                                       + (" m" if mirrored else ""))
                    root[LEVEL] = level
                    if cell == centre and centre_copy is None:
                        centre_copy = root
                filled += 1
    view.refresh(scene)  # neighbours get the debug material if it is on
    _select_only(centre_copy)
    return filled, missing, note


def build_pair(scene, tile_set, a, b, axis, near=None):
    """Put two library entries side by side (A low, B one cell further along axis), all editable.

    a, b: (object name, hash, quarter turns, mirrored, pieces); a name of "" leaves that cell empty;
    pieces: [(object name, quarter turns, mirrored)] for a shape built from pieces (name unused).
    near: a tile to put the pair next to. Returns (A position, B position) of the cell centres.
    The real A and B tiles (the meshes to fix) are moved next to the pair while it is shown; clearing
    or rebuilding the playground puts them back (their own position is kept in obj["mc_pg_home"]).
    """
    clear(scene)
    collection = _playground_collection(scene)
    origin = cell_centre(_origin(tile_set, near, block=1), 0, 0, 0)
    step = Vector([1.0 if i == axis else 0.0 for i in range(3)])
    positions = (origin, origin + step)
    copies = []
    for (name, h, turns, mirrored, pieces), position, tag in zip((a, b), positions, ("A", "B")):
        for part, part_turns, part_mirrored in pieces or [(name, turns, mirrored)]:
            tile = bpy.data.objects.get(part) if part else None
            if tile is not None:
                copy = _copy_tile(tile, collection, position, part_turns, h, "pasted", part_mirrored)
                copy[REF_LABEL] = f"{h:08b} {tag}"  # the shape in that cell, as Godot draws it
                copies.append(copy)
    # The real tiles, in a row after the pair (+X), one empty cell between each.
    x = 3.0 if axis == 0 else 2.0
    names = []
    for name, _, _, _, pieces in (a, b):
        names += [p[0] for p in pieces] if pieces else [name]
    for name in dict.fromkeys(n for n in names if n):  # a tile used twice: move it once
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
        obj = bpy.data.objects.get(scene.mc_tiles_pg_tile)  # the last centre tile, L / H too
        if obj is None or tile_hash(obj.name, "CUBE") != h:
            obj = tile_variants(tile_set, "CUBE").get(h, [None])[0]
    if obj is not None and obj.name not in tile_set.all_objects:  # the tile's own set, not the active one
        owners = [c for c in bpy.data.collections if c.mc_tiles.is_tile_set and c.mc_tiles.kind == "CUBE"
                  and obj.name in c.all_objects]
        if not owners:
            homes = ", ".join(c.name for c in obj.users_collection)
            operator.report({"WARNING"}, f"{obj.name} is not in a tile set: mark {homes} as a tile set, "
                                         f"or pick a tile from {tile_set.name}")
            return {"CANCELLED"}
        tile_set = owners[0]
    if new_seed:
        scene.mc_tiles_pg_seed += 1
    scene.mc_tiles_pg_hash = h
    scene.mc_tiles_pg_tile = obj.name if obj is not None else ""
    filled, missing, note = build(scene, tile_set, h, scene.mc_tiles_pg_seed, near=obj)
    text = f"Playground around {obj.name if obj else f'{h:08b}'} ({tile_set.name}): {filled} tiles"
    if missing:
        text += f", {len(missing)} missing: " + ", ".join(f"{m:08b}" for m in sorted(set(missing)))
    if note:
        text += f"; {note}"
    operator.report({"WARNING"} if missing or note else {"INFO"}, text)
    return {"FINISHED"}


class MCTILES_OT_playground_for(bpy.types.Operator):
    """Build the playground around this tile (in its own row behind the tile set)"""

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
    """Put the selected tile in a 4x4x4 block and fill its neighbours from the set (L bottom, H roofs)"""

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


CLASSES = (
    MCTILES_OT_playground_build, MCTILES_OT_playground_for, MCTILES_OT_playground_shuffle,
    MCTILES_OT_playground_clear,
)


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    bpy.types.Scene.mc_tiles_pg_seed = bpy.props.IntProperty(name="Seed", default=1)
    bpy.types.Scene.mc_tiles_pg_hash = bpy.props.IntProperty(name="Centre", default=-1)
    bpy.types.Scene.mc_tiles_pg_tile = bpy.props.StringProperty(name="Centre Tile")


def unregister():
    del bpy.types.Scene.mc_tiles_pg_tile
    del bpy.types.Scene.mc_tiles_pg_hash
    del bpy.types.Scene.mc_tiles_pg_seed
    for cls in reversed(CLASSES):
        bpy.utils.unregister_class(cls)
