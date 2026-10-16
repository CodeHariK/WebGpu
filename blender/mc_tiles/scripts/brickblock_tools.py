"""BrickBlock tools: a small stand-alone script (not part of the MC Tile Tools add-on).

Run it once from Blender's Text Editor (Open this file, Run Script); it adds a "BrickBlock" tab
to the 3D view sidebar (N panel):

    Remove Empties     every mesh parented to an "index<Level><Variant><hash>" empty (e.g.
                       indexMA13) loses its empty: the mesh keeps its exact place in the world, its
                       origin moves 0.5 above the empty's location (the empty is on the cell floor,
                       so the origin lands on the cell centre), it is renamed after the empty and
                       remembers level / variant / hash as custom properties
    Hash Labels        draws each mesh's hash under it in the viewport: decimal and 8-bit binary,
                       e.g. "13  00001101  MA"
    Corner Boxes       draws a 1 m cube centred on each mesh's origin: orange dots on solid corners,
                       light blue on empty ones (in the source's corner order, see CORNERS)
    Levels L / M / H   untick a level to hide its meshes; Arrange only lays out the ticked levels
    Arrange by Hash    a table: one column per hash (sorted), one row per group in the order
                       LA LB LC MA MB MC MD HA HB HC, only for the ticked levels and only the
                       hashes they use (a group with no mesh for a hash leaves a gap); 2 m apart,
                       cell box corners on integers. The first Arrange remembers every mesh's place
    Restore            puts every mesh back where it was before the first Arrange
    Seam Check         every neighbouring pair (all 4 turns; side by side within a level, L under
                       M / H, M under H) whose edge curves on the shared wall don't meet; click a
                       pair to see linked copies of it above the table (see "Seam check" below)
    Playground         the selected tile in a 4 x 4 x 4 block of random neighbours of its own level
                       (Shuffle: new neighbours; Clear: remove playground and seam copies). Corner
                       Boxes are drawn on all copies too

The hash is read from the name: "index" + level letter + variant letter + decimal number.
Its corner order is the source's, not MC Tile Tools' (no bits are changed here).
"""

import math
import re

import blf
import bpy
import gpu
from bpy_extras.view3d_utils import location_3d_to_region_2d
from gpu_extras.batch import batch_for_shader
from mathutils import Matrix, Vector

NAME = re.compile(r"^index([A-Z])([A-Z])(\d+)")
ROWS = ("LA", "LB", "LC", "MA", "MB", "MC", "MD", "HA", "HB", "HC")  # row order; other groups go after
LEVELS = ("L", "M", "H")
HOME = "bb_home"  # a mesh's place before the first Arrange
SPACING = 2.0
ORIGIN_ABOVE_EMPTY = 0.5  # the empties sit on the cell floor; mesh origins go to the cell centre
# The source's corner order, found from the geometry (an empty corner with empty neighbours never has
# geometry near it: 47 of 47 checks pass with this order). Relative to the origin the cell box is
# a 1 m cube centred on the origin; bit i is corner CORNERS[i].
CORNERS = (
    (-0.5, 0.5, -0.5), (0.5, 0.5, -0.5), (-0.5, 0.5, 0.5), (0.5, 0.5, 0.5),
    (-0.5, -0.5, -0.5), (0.5, -0.5, -0.5), (-0.5, -0.5, 0.5), (0.5, -0.5, 0.5),
)
EDGES = tuple((a, b) for a in range(8) for b in range(a + 1, 8)
              if sum(p != q for p, q in zip(CORNERS[a], CORNERS[b])) == 1)
SOLID = (1.0, 0.55, 0.1, 1.0)
EMPTY = (0.65, 0.75, 0.95, 1.0)
WIRE = (0.85, 0.85, 0.85, 0.8)


def parse(name):
    """(level, variant, hash) from "indexMA13...", or None."""
    m = NAME.match(name)
    return (m.group(1), m.group(2), int(m.group(3))) if m else None


# MC Tile Tools helper copies (linked piece copies, generated tiles, its playground) copy every
# custom property, bb_hash too; they are not BrickBlock tiles.
ADDON_HELPER_KEYS = ("mc_piece_link", "mc_piece_of", "mc_pg_tile", "mc_pg_role")


# "11001101", "11001101_mb1" (tag: height, group, number), and the older "11001101_v2" /
# "11001101.brickblock_l" forms an earlier Convert wrote.
MC_NAME = re.compile(r"^([01]{8})(?:_([lmh])([a-z])(\d+)|_v(\d+))?(?:[._]|$)")
OURS_TO_SRC = (4, 5, 1, 0, 6, 7, 3, 2)  # our corner i -> source bit


def from_ours(h):
    """MC Tile Tools / Godot hash -> source hash (the inverse of to_ours)."""
    return sum(1 << OURS_TO_SRC[i] for i in range(8) if (h >> i) & 1)


def name_identity(obj):
    """(level or None, group letter, source hash, number) read from the object's name, or None:
    "indexMA13" (source hash); "11001101_mb1" (our hash, M, group B, 1); "11001101" (M, A, 1 unless
    it sits in an old "BrickBlock L / H" collection); old "11001101_v2" (group B)."""
    m = NAME.match(obj.name)
    if m:
        return m.group(1), m.group(2), int(m.group(3)), 1
    m = MC_NAME.match(obj.name)
    if not m:
        return None
    h = from_ours(int(m.group(1), 2))
    if m.group(2):
        return m.group(2).upper(), m.group(3).upper(), h, int(m.group(4))
    group = chr(ord("A") + max(1, int(m.group(5) or 1)) - 1)
    return None, group, h, 1


def sync_from_names(scene):
    """The name decides what a tile is: re-tag every mesh whose name is a tile name (so a duplicated
    and renamed mesh, or one made from scratch in a BrickBlock set, is what its name says, not what
    the mesh it was copied from was). Returns how many meshes changed."""
    changed = 0
    for obj in scene.objects:
        if obj.type != "MESH" or "bb_src" in obj or any(key in obj for key in ADDON_HELPER_KEYS):
            continue  # playground / seam / add-on copies keep their tags
        identity = name_identity(obj)
        if identity is None:
            continue
        level, variant, h, number = identity
        in_sets = [c.name for c in obj.users_collection if c.name.split(".")[0] == SET_NAME
                   or c.name.startswith(SET_PREFIX)]
        if not NAME.match(obj.name) and not in_sets and "bb_hash" not in obj:
            continue  # a hash name outside BrickBlock (another tile set in this file): not ours
        if level is None:  # untagged: an old per-level collection ("BrickBlock L") says, else M
            old = [n[len(SET_PREFIX):][:1] for n in in_sets if n[len(SET_PREFIX):][:1] in LEVELS]
            level = old[0] if old else "M"
        if obj.get("bb_number") != number:
            obj["bb_number"] = number
        if (obj.get("bb_level"), obj.get("bb_variant"), obj.get("bb_hash")) != (level, variant, h):
            obj["bb_level"], obj["bb_variant"], obj["bb_hash"] = level, variant, h
            if "bb_mc_hash" in obj:
                obj["bb_mc_hash"] = to_ours(h)
            if "bb_mirror" in obj:
                del obj["bb_mirror"]  # Arrange works it out again
            changed += 1
    return changed


def tagged_meshes(scene):
    return [o for o in scene.objects if o.type == "MESH" and "bb_hash" in o
            and not any(key in o for key in ADDON_HELPER_KEYS)]


def shown_levels(scene):
    return {level for level in LEVELS if getattr(scene, "bb_show_" + level, True)}


def shown_meshes(scene):
    """Tagged meshes of the ticked levels (a level that isn't L, M or H always counts as shown).
    With Only MA ticked, just the MA group."""
    if getattr(scene, "bb_only_ma", False):
        return [o for o in tagged_meshes(scene) if o["bb_level"] + o["bb_variant"] == "MA"]
    levels = shown_levels(scene)
    return [o for o in tagged_meshes(scene) if o["bb_level"] in levels or o["bb_level"] not in LEVELS]


def mirror_hash(h):
    """The hash of a tile mirrored in X (source corner order)."""
    out = 0
    for i, c in enumerate(CORNERS):
        if (h >> i) & 1:
            out |= 1 << CORNERS.index((-c[0], c[1], c[2]))
    return out


def turn_class(h):
    """The smallest hash among h's 4 quarter turns: tiles with the same class are one mesh turned."""
    return min(turn_hash(h, t) for t in range(4))


def mirror_groups(hashes):
    """Hashes grouped with their mirror: [[h, mirror partner], [self-symmetric h], ...] in hash order.
    A partner is any hash whose turn class is the class of h mirrored (a mirror turned any way)."""
    by_class = {}
    for h in hashes:
        by_class.setdefault(turn_class(h), []).append(h)
    groups, done = [], set()
    for h in sorted(hashes):
        if h in done:
            continue
        group = [h]
        done.add(h)
        for cls in (turn_class(h), turn_class(mirror_hash(h))):
            for other in sorted(by_class.get(cls, ())):
                if other not in done:
                    group.append(other)
                    done.add(other)
        groups.append(group)
    return groups


def mirror_partner(h, hashes):
    """("partner", p) / ("symmetric", h) / ("missing", None): h's mirror among `hashes`."""
    cls = turn_class(mirror_hash(h))
    if cls == turn_class(h):
        return "symmetric", h
    found = [p for p in hashes if turn_class(p) == cls]
    return ("partner", min(found)) if found else ("missing", None)


def mirrored_meshes(meshes):
    """Meshes another mesh of the same group (MA, MB, ..) can stand in for by mirroring: of each
    mirror pair the higher hash, i.e. the one Arrange puts behind its partner."""
    by_group = {}
    for obj in meshes:
        by_group.setdefault(obj["bb_level"] + obj["bb_variant"], set()).add(obj["bb_hash"])
    out = set()
    for obj in meshes:
        kind, partner = mirror_partner(obj["bb_hash"], by_group[obj["bb_level"] + obj["bb_variant"]])
        if kind == "partner" and partner < obj["bb_hash"]:
            out.add(obj)
    return out


def checked_meshes(scene):
    """(meshes, mirrors) for Seam Check and Playground: the shown meshes, turns only; with Hide Mirrored
    on, without the mirrored meshes (52 in MA), each tried mirrored too, so all 8 transforms are used."""
    meshes = shown_meshes(scene)
    if getattr(scene, "bb_hide_mirrored", False):
        hidden = mirrored_meshes(meshes)
        return [o for o in meshes if o not in hidden], True
    return meshes, False


def _on_levels(_self, context):
    scene = context.scene
    sync_from_names(scene)
    shown = set(shown_meshes(scene))
    if getattr(scene, "bb_hide_mirrored", False):
        shown -= mirrored_meshes(list(shown))
    layer = context.view_layer
    for obj in tagged_meshes(scene):
        if obj.name in layer.objects:
            obj.hide_set(obj not in shown)
    _redraw(_self, context)


class BB_OT_remove_empties(bpy.types.Operator):
    """Remove the index empties: each child mesh keeps its world place, origin at its empty's location"""

    bl_idname = "brickblock.remove_empties"
    bl_label = "Remove Empties"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        done = 0
        empties = [o for o in context.scene.objects if o.type == "EMPTY" and parse(o.name)]
        for empty in empties:
            name = empty.name
            level, variant, h = parse(name)
            # The origin goes 0.5 above the empty (Blender Z up = the source's Y up): the cell centre.
            centre = empty.matrix_world.translation + Vector((0.0, 0.0, ORIGIN_ABOVE_EMPTY))
            children = [c for c in empty.children if c.type == "MESH"]
            for child in children:
                world = child.matrix_world.copy()
                # Geometry into world space around the empty's location, then the object sits there.
                child.data.transform(Matrix.Translation(-centre) @ world)
                child.parent = None
                child.matrix_world = Matrix.Translation(centre)
            bpy.data.objects.remove(empty, do_unlink=True)  # frees its name for the mesh
            for child in children:
                child.name = name
                child.data.name = name
                child["bb_level"], child["bb_variant"], child["bb_hash"] = level, variant, h
                done += 1
        self.report({"INFO"}, f"{done} meshes, {len(empties)} empties removed")
        return {"FINISHED"}


class BB_OT_arrange(bpy.types.Operator):
    """One column per hash (sorted), its meshes in a row behind each other by level and variant"""

    bl_idname = "brickblock.arrange"
    bl_label = "Arrange by Hash"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        sync_from_names(context.scene)
        if not tagged_meshes(context.scene):
            self.report({"WARNING"}, "Press Remove Empties first")
            return {"CANCELLED"}
        meshes = shown_meshes(context.scene)  # only the ticked levels: no empty rows or columns
        if not meshes:
            self.report({"WARNING"}, "Tick at least one level")
            return {"CANCELLED"}
        hashes = sorted({o["bb_hash"] for o in meshes})
        present = {o["bb_level"] + o["bb_variant"] for o in meshes}
        groups = [g for g in ROWS if g in present] + sorted(present - set(ROWS))
        # Columns: one per mirror group; a hash's mirror (one of the 8 turns / mirrors of the same
        # shape) sits right above it in Z, so each column holds one shape. The cell box is +-0.5 around
        # the origin, so origins on x.5 / z.5 put its corners on integers.
        column_x, height = {}, {}
        mirror_sets = mirror_groups(hashes)
        for index, group in enumerate(mirror_sets):
            for d, h in enumerate(group):
                column_x[h] = index * SPACING + 0.5
                height[h] = d
        # A group-row gets one sub-row per copy when it has several meshes for a hash.
        cells = {}
        for obj in sorted(meshes, key=lambda o: o.name):
            cells.setdefault((obj["bb_level"] + obj["bb_variant"], obj["bb_hash"]), []).append(obj)
        row_of, row = {}, 0
        for g in groups:
            row_of[g] = row
            row += max(len(objs) for (group, _h), objs in cells.items() if group == g)
        for (g, h), objs in cells.items():
            for k, obj in enumerate(objs):
                if HOME not in obj:
                    obj[HOME] = list(obj.location)
                obj.location = Vector((column_x[h], -(row_of[g] + k) * SPACING + 0.5, height[h] * SPACING + 0.5))
                kind, partner = mirror_partner(h, hashes)
                obj["bb_mirror"] = {"partner": partner, "symmetric": -1, "missing": -2}[kind]
        pairs = sum(len(g) == 2 for g in mirror_sets)
        alone = [g[0] for g in mirror_sets if len(g) == 1]
        symmetric = sum(mirror_partner(h, hashes)[0] == "symmetric" for h in alone)
        self.report({"INFO"}, f"{len(meshes)} meshes: {len(mirror_sets)} columns x {len(groups)} group rows; "
                              f"{pairs} mirror pairs, {symmetric} symmetric, {len(alone) - symmetric} mirror missing")
        return {"FINISHED"}


class BB_OT_restore(bpy.types.Operator):
    """Put every mesh back where it was before the first Arrange"""

    bl_idname = "brickblock.restore"
    bl_label = "Restore"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        moved = 0
        for obj in tagged_meshes(context.scene):
            if HOME in obj:
                obj.location = Vector(obj[HOME])
                del obj[HOME]
                moved += 1
        self.report({"INFO"}, f"Restored {moved} meshes")
        return {"FINISHED"}


# ---------------------------------------------------------------------------------------------------
# MC Tile Tools names
#
# The add-on (and Godot) read the hash from the name, in OUR corner order:
#   C0 (-x,-y,bottom) C1 (+x,-y,bottom) C2 (+x,+y,bottom) C3 (-x,+y,bottom), C4..C7 the same on top.
# The source bit i is our corner SRC_TO_OURS[i] (see the diagram in the docs: bit = x + 2*top + 4*front).
# Convert puts every tile in one tile set collection, "BrickBlock", named "<our 8 binary digits>"
# plus the tag the add-on reads (mc_tile_tools/variants.py): height l / m / h, group a..d (the
# BrickBlock variant letter), number 1, 2.. (more meshes of the same height, group and hash):
#   MA13 -> "<hash>_ma1", MB13 -> "<hash>_mb1", LA3 -> "<hash>_la1".
# The old name and collections are remembered, so Restore Names undoes it. bb_hash keeps the source
# hash, so everything else in this script still works. (An earlier Convert made one set per level,
# "BrickBlock L / M / H", with "_v2" for variant B; those names are still read and are renamed.)

SRC_TO_OURS = (3, 2, 7, 6, 0, 1, 4, 5)
SET_NAME = "BrickBlock"
SET_PREFIX = "BrickBlock "  # the old per-level sets: "BrickBlock L"


def to_ours(h):
    """Source hash -> MC Tile Tools / Godot hash."""
    return sum(1 << SRC_TO_OURS[i] for i in range(8) if (h >> i) & 1)


def addon_name(obj, number=1):
    """The MC Tile Tools name for a tile: "00001000_ma1", "00001000_mb1", "00001000_la2"."""
    tag = f"{obj['bb_level'].lower()}{obj['bb_variant'].lower()}{number}"
    return f"{to_ours(obj['bb_hash']):08b}_{tag}"


def _slug(name):
    return re.sub(r"[^a-z0-9]+", "_", name.lower()).strip("_")


class BB_OT_convert_names(bpy.types.Operator):
    """Rename every tile to its MC Tile Tools name (our corner order + height / group / number tag)
    and put them all in one tile set, "BrickBlock\""""

    bl_idname = "brickblock.convert_names"
    bl_label = "Convert to MC Tile Names"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        scene = context.scene
        sync_from_names(scene)
        meshes = tagged_meshes(scene)
        if not meshes:
            self.report({"WARNING"}, "Press Remove Empties first")
            return {"CANCELLED"}
        coll = bpy.data.collections.get(SET_NAME) or bpy.data.collections.new(SET_NAME)
        if coll.name not in scene.collection.children:
            scene.collection.children.link(coll)
        if hasattr(coll, "mc_tiles"):  # MC Tile Tools is enabled: make it a cube tile set, 68 mode
            coll.mc_tiles.is_tile_set = True
            coll.mc_tiles.kind = "CUBE"
            coll.mc_tiles.mode = "68"
            coll.mc_tiles.build_from_pieces = False  # BrickBlock has every tile it needs: nothing built
        meshes.sort(key=lambda o: (o.get("bb_number", 1), o.name))
        for i, obj in enumerate(meshes):  # park on temporary names so renames can't collide
            if "bb_name" not in obj:
                obj["bb_name"] = obj.name
                obj["bb_collections"] = [c.name for c in obj.users_collection]
            obj.name = f"__bb_convert_{i}"
        numbers = {}  # (level, group, hash) -> next free number
        for obj in meshes:
            for old in list(obj.users_collection):
                if old is not coll:
                    old.objects.unlink(obj)
            if coll not in obj.users_collection:
                coll.objects.link(obj)
            key = (obj["bb_level"], obj["bb_variant"], obj["bb_hash"])
            numbers[key] = numbers.get(key, 0) + 1
            obj.name = obj.data.name = addon_name(obj, numbers[key])
            obj["bb_number"] = numbers[key]
            obj["bb_mc_hash"] = to_ours(obj["bb_hash"])
        for level in LEVELS:  # the per-level sets an earlier Convert made, now empty
            old = bpy.data.collections.get(SET_PREFIX + level)
            if old is not None and not old.all_objects and not old.children:
                bpy.data.collections.remove(old)
        roles = sorted({(o["bb_level"], o["bb_variant"]) for o in meshes})
        self.report({"INFO"}, f"{len(meshes)} tiles renamed into '{coll.name}': "
                              + " ".join(f"{lv}{g.lower()}" for lv, g in roles))
        return {"FINISHED"}


def _addon_module(name):
    """A module of the enabled MC Tile Tools add-on (e.g. "export"), or None."""
    import sys

    return next((m for key, m in sys.modules.items() if key.endswith("mc_tile_tools." + name)), None)


class BB_OT_export(bpy.types.Operator):
    """Export the BrickBlock tile set to one .glb with MC Tile Tools' exporter: every height and
    group ("00001000_ma1_brickblock", "00001000_hb1_brickblock"), every number, and per role the
    shapes modelled only as a mirror written as flipped copies"""

    bl_idname = "brickblock.export"
    bl_label = "Export"

    def execute(self, context):
        sync_from_names(context.scene)
        export = _addon_module("export")
        if export is None:
            self.report({"ERROR"}, "Enable the MC Tile Tools add-on: its exporter writes the .glb")
            return {"CANCELLED"}
        coll = bpy.data.collections.get(SET_NAME)
        if coll is None or not coll.mc_tiles.is_tile_set:
            self.report({"ERROR"}, "Press Convert to MC Tile Names first (it makes the BrickBlock tile set)")
            return {"CANCELLED"}
        if not coll.mc_tiles.export_path.strip():  # default: beside the .blend
            coll.mc_tiles.export_path = f"//{_slug(SET_NAME)}.glb"
        selected, active = list(context.selected_objects), context.view_layer.objects.active
        try:
            count, path = export.export_set(coll)
        except (ValueError, RuntimeError) as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        finally:
            for obj in context.view_layer.objects:
                obj.select_set(obj in selected)
            context.view_layer.objects.active = active
        self.report({"INFO"}, f"Exported {count} tiles to {path}")
        return {"FINISHED"}


class BB_OT_restore_names(bpy.types.Operator):
    """Undo Convert: the index names and collections the tiles had before"""

    bl_idname = "brickblock.restore_names"
    bl_label = "Restore Names"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        meshes = [o for o in tagged_meshes(context.scene) if "bb_name" in o]
        for i, obj in enumerate(meshes):
            obj.name = f"__bb_restore_{i}"
        for obj in meshes:
            homes = [bpy.data.collections.get(n) for n in obj["bb_collections"]]
            homes = [c for c in homes if c is not None] or [context.scene.collection]
            for old in list(obj.users_collection):
                if old not in homes:
                    old.objects.unlink(obj)
            for home in homes:
                if home not in obj.users_collection:
                    home.objects.link(obj)
            obj.name = obj.data.name = obj["bb_name"]
            for key in ("bb_name", "bb_collections", "bb_mc_hash"):
                del obj[key]
        self.report({"INFO"}, f"Restored {len(meshes)} names")
        return {"FINISHED"}


# ---------------------------------------------------------------------------------------------------
# Seam check
#
# The meshes are open surfaces: where one touches a cell wall it ends in an edge curve (a boundary
# edge, or a crease against a face lying on the wall). Two tiles side by side join without a seam
# when A's curve on its right wall and B's curve on its left wall are the same curve. Each curve is
# drawn into a 1 cm bitmap of its wall; a cell of one curve with no cell of the other within 1 cm
# counts as a mismatch, and a pair clashes when more than SEAM_TOLERANCE cells mismatch (so the
# number shown is roughly the length, in cm, of curve that has no partner).
#
# Every mesh is tried in all 4 turns about Z (the set holds one mesh per turn class). Only pairs
# whose shared corners agree are neighbours, so only those are compared:
#   side:  A left of B (A's +x wall against B's -x wall), same level only (SIDE_SAME_LEVEL);
#          front / back is the same check with both tiles turned a quarter, so it is covered
#   stack: A under B (A's top wall against B's bottom wall) for the level pairs in STACKS
# Only the ticked levels are checked.

SEAM_CELL = 0.01
SEAM_TOLERANCE = 4  # mismatching cells ignored (noise)
SIDE_RANGE = ((-0.5, 0.5), (-1.6, 0.6))  # side wall: u = along the wall, v = z (L hangs a cell lower)
FLAT_RANGE = ((-0.5, 0.5), (-0.5, 0.5))  # top / bottom wall: u = x, v = y
WALL_EPS = 1e-3
SIDE_SAME_LEVEL = True
STACKS = (("L", "M"), ("L", "H"), ("M", "H"))  # (lower, upper)
PLAYGROUND = "BB Seam Playground"
PLAYGROUND_Y = 3.5  # the pair is shown above the arranged table, in the first tile's column
REPORT = "BrickBlock Seams"
WALLS = {(1, 0, 0): (0, 0.5), (-1, 0, 0): (0, -0.5), (0, 1, 0): (1, 0.5), (0, -1, 0): (1, -0.5),
         (0, 0, 1): (2, 0.5), (0, 0, -1): (2, -0.5)}  # wall normal -> (axis, plane coordinate)


def _turn_vec(p, turns):
    """p turned `turns` quarter turns counter-clockwise about Z."""
    for _ in range(turns % 4):
        p = (-p[1], p[0], p[2])
    return p


def turn_hash(h, turns):
    """The hash of a tile turned `turns` quarter turns (source corner order)."""
    out = 0
    for i, corner in enumerate(CORNERS):
        if (h >> i) & 1:
            out |= 1 << CORNERS.index(_turn_vec(corner, turns))
    return out


def _wall_pattern(h, normal):
    """The solid corners on one wall, as positions within the wall plane."""
    axis, plane = WALLS[normal]
    return frozenset(tuple(c[k] for k in range(3) if k != axis)
                     for i, c in enumerate(CORNERS) if (h >> i) & 1 and c[axis] == plane)


def _wall_points(mesh):
    """{wall normal: (n, 3) array of points sampled every 5 mm along the curves on that wall}."""
    import bmesh
    import numpy as np

    bm = bmesh.new()
    bm.from_mesh(mesh)
    result = {}
    for normal, (axis, plane) in WALLS.items():
        def on(v):
            return abs(v.co[axis] - plane) < WALL_EPS

        def flat(face):
            return all(on(v) for v in face.verts)

        chunks = []
        for edge in bm.edges:
            a, b = edge.verts
            if not (on(a) and on(b)):
                continue
            if edge.link_faces and all(flat(f) for f in edge.link_faces):
                continue  # inside a face lying on the wall: not where the surface leaves the wall
            n = max(2, int((a.co - b.co).length / (SEAM_CELL / 2.0)) + 2)
            t = np.linspace(0.0, 1.0, n)[:, None]
            chunks.append(np.array(a.co)[None, :] * (1.0 - t) + np.array(b.co)[None, :] * t)
        result[normal] = np.concatenate(chunks) if chunks else np.zeros((0, 3))
    bm.free()
    return result


class _Bitmaps:
    """Interns wall bitmaps so identical ones are stored and compared once."""

    def __init__(self):
        self.keys, self.maps, self.grown, self.cache = {}, [], [], {}

    def add(self, points2d, ranges):
        import numpy as np

        (u0, u1), (v0, v1) = ranges
        w, h = int(round((u1 - u0) / SEAM_CELL)) + 1, int(round((v1 - v0) / SEAM_CELL)) + 1
        bits = np.zeros((h, w), dtype=bool)
        if len(points2d):
            iu = np.clip(np.floor((points2d[:, 0] - u0) / SEAM_CELL).astype(int), 0, w - 1)
            iv = np.clip(np.floor((points2d[:, 1] - v0) / SEAM_CELL).astype(int), 0, h - 1)
            bits[iv, iu] = True
        key = (bits.shape, np.packbits(bits).tobytes())
        if key not in self.keys:
            grown = bits.copy()
            grown[1:] |= bits[:-1]
            grown[:-1] |= bits[1:]
            g = grown.copy()
            grown[:, 1:] |= g[:, :-1]
            grown[:, :-1] |= g[:, 1:]
            self.keys[key] = len(self.maps)
            self.maps.append(bits)
            self.grown.append(grown)
        return self.keys[key]

    def mismatch(self, a, b):
        if (a, b) not in self.cache:
            ma, mb = self.maps[a], self.maps[b]
            self.cache[(a, b)] = int((ma & ~self.grown[b]).sum() + (mb & ~self.grown[a]).sum())
        return self.cache[(a, b)]


def placed_hash(h, turns, mirrored=False):
    """The hash of a tile mirrored in X (if `mirrored`) and then turned `turns` quarter turns."""
    return turn_hash(mirror_hash(h) if mirrored else h, turns)


def _instances(meshes, bitmaps, mirrors=False):
    """One entry per mesh per quarter turn (and per mirror when `mirrors`), with its right / left /
    top / bottom curves and patterns. A mirrored instance is the mesh flipped in X, then turned."""
    out = []
    for obj in meshes:
        points = _wall_points(obj.data)
        for mirrored in ((False, True) if mirrors else (False,)):
            for turns in range(4):
                h = placed_hash(obj["bb_hash"], turns, mirrored)
                walls = {}
                for normal, pts in points.items():
                    turned = pts.copy()
                    if mirrored:
                        turned[:, 0] = -turned[:, 0]
                        normal = (-normal[0], normal[1], normal[2])
                    for _ in range(turns):
                        turned[:, 0], turned[:, 1] = -turned[:, 1].copy(), turned[:, 0].copy()
                    walls[_turn_vec(normal, turns)] = turned
                out.append(_instance(obj, turns, mirrored, h, walls, bitmaps))
    return out


def _instance(obj, turns, mirrored, h, walls, bitmaps):
    return {
        "name": obj.name, "level": obj["bb_level"], "turns": turns, "mirrored": mirrored, "hash": h,
        "right": bitmaps.add(walls[(1, 0, 0)][:, 1:3], SIDE_RANGE),
        "left": bitmaps.add(walls[(-1, 0, 0)][:, 1:3], SIDE_RANGE),
        "top": bitmaps.add(walls[(0, 0, 1)][:, 0:2], FLAT_RANGE),
        "bottom": bitmaps.add(walls[(0, 0, -1)][:, 0:2], FLAT_RANGE),
        "right_pattern": _wall_pattern(h, (1, 0, 0)), "left_pattern": _wall_pattern(h, (-1, 0, 0)),
        "top_pattern": _wall_pattern(h, (0, 0, 1)), "bottom_pattern": _wall_pattern(h, (0, 0, -1)),
    }


def seam_check(meshes, levels, mirrors=False):
    """(clashes, checked counts). A clash: dict a, ta, ma, b, tb, mb, kind ("side" / "stack"), cells
    (ma / mb: that tile is mirrored in X before its turn). `mirrors`: try mirrored tiles too."""
    bitmaps = _Bitmaps()
    instances = _instances(meshes, bitmaps, mirrors)
    clashes, checked = [], {"side": 0, "stack": 0}
    by_left = {}
    for inst in instances:
        by_left.setdefault(inst["left_pattern"], []).append(inst)
    seen = set()
    for a in instances:
        if a["level"] not in levels:
            continue
        for b in by_left.get(a["right_pattern"], ()):
            if b["level"] not in levels or (SIDE_SAME_LEVEL and b["level"] != a["level"]):
                continue
            key = (a["name"], a["turns"], a["mirrored"], b["name"], b["turns"], b["mirrored"])
            same = (b["name"], (b["turns"] + 2) % 4, b["mirrored"],  # the pair turned 180
                    a["name"], (a["turns"] + 2) % 4, a["mirrored"])
            if min(key, same) in seen:
                continue
            seen.add(min(key, same))
            checked["side"] += 1
            cells = bitmaps.mismatch(a["right"], b["left"])
            if cells > SEAM_TOLERANCE:
                clashes.append(dict(a=a["name"], ta=a["turns"], ma=a["mirrored"], b=b["name"], tb=b["turns"],
                                    mb=b["mirrored"], kind="side", cells=cells))
    for lower, upper in STACKS:
        if lower not in levels or upper not in levels:
            continue
        for a in instances:
            if a["level"] != lower or a["turns"] or a["mirrored"]:
                continue  # turning (or mirroring) the whole stack changes nothing, so A stays as modelled
            for b in instances:
                if b["level"] != upper or b["bottom_pattern"] != a["top_pattern"]:
                    continue
                checked["stack"] += 1
                cells = bitmaps.mismatch(a["top"], b["bottom"])
                if cells > SEAM_TOLERANCE:
                    clashes.append(dict(a=a["name"], ta=0, ma=False, b=b["name"], tb=b["turns"],
                                        mb=b["mirrored"], kind="stack", cells=cells))
    clashes.sort(key=lambda c: -c["cells"])
    return clashes, checked


def coverage(meshes, mirrors=False):
    """{level: (shapes present, all shapes, hashes of missing shapes)}: 68 shapes with turns only,
    53 when mirrors count as the same shape."""
    def cls(h):
        return min(placed_hash(h, t, m) for t in range(4) for m in ((False, True) if mirrors else (False,)))

    every = {cls(h) for h in range(1, 255)}
    out = {}
    for level in sorted({o["bb_level"] for o in meshes}):
        have = {cls(o["bb_hash"]) for o in meshes if o["bb_level"] == level}
        out[level] = (len(have & every), len(every), sorted(every - have))
    return out


class BB_SeamItem(bpy.types.PropertyGroup):
    a: bpy.props.StringProperty()
    ta: bpy.props.IntProperty()
    ma: bpy.props.BoolProperty()
    b: bpy.props.StringProperty()
    tb: bpy.props.IntProperty()
    mb: bpy.props.BoolProperty()
    kind: bpy.props.StringProperty()
    cells: bpy.props.IntProperty()


class BB_OT_seam_check(bpy.types.Operator):
    """Compare the edge curves where neighbouring tiles meet (all turns; ticked levels only)"""

    bl_idname = "brickblock.seam_check"
    bl_label = "Seam Check"

    def execute(self, context):
        scene = context.scene
        sync_from_names(scene)
        if not tagged_meshes(scene):
            self.report({"WARNING"}, "Press Remove Empties first")
            return {"CANCELLED"}
        meshes, mirrors = checked_meshes(scene)  # ticked levels (or just MA); with Hide Mirrored, the
        clashes, checked = seam_check(meshes, {o["bb_level"] for o in meshes}, mirrors)  # 52 + mirrors
        scene.bb_seams.clear()
        for clash in clashes:
            item = scene.bb_seams.add()
            for key, value in clash.items():
                setattr(item, key, value)
        side = sum(c["kind"] == "side" for c in clashes)
        stack = len(clashes) - side
        scene.bb_seam_summary = (f"side {side} / {checked['side']}   stack {stack} / {checked['stack']}")
        self._write_report(meshes, clashes, checked, mirrors)
        scene.bb_seam_index = -1
        self.report({"INFO"}, f"{len(clashes)} clashing pairs ({scene.bb_seam_summary}); see text '{REPORT}'")
        return {"FINISHED"}

    @staticmethod
    def _write_report(meshes, clashes, checked, mirrors):
        text = bpy.data.texts.get(REPORT) or bpy.data.texts.new(REPORT)
        what = "turns + mirrors, mirrored meshes left out" if mirrors else "turns only"
        lines = [f"Seam check ({what}): {len(clashes)} clashing pairs; checked side {checked['side']}, "
                 f"stack {checked['stack']}", "", "Shapes per level:"]
        for level, (have, every, missing) in coverage(meshes, mirrors).items():
            lines.append(f"  {level}: {have} / {every}" + (f"   missing e.g. {missing[:12]}" if missing else ""))
        lines += ["", "cells  kind   A (turn)  ->  B (turn)    side: A left of B   stack: A under B   m: mirrored in X"]
        for c in clashes:
            lines.append(f"{c['cells']:5d}  {c['kind']:5s}  {c['a']} ({c['ta'] * 90}{'m' if c['ma'] else ''})  ->  "
                         f"{c['b']} ({c['tb'] * 90}{'m' if c['mb'] else ''})")
        text.from_string("\n".join(lines) + "\n")


def _show_pair(_self, context):
    """Linked copies of the selected pair, side by side (or stacked), above the arranged table."""
    scene = context.scene
    if not 0 <= scene.bb_seam_index < len(scene.bb_seams):
        return
    item = scene.bb_seams[scene.bb_seam_index]
    coll = _fresh_collection(scene, PLAYGROUND)
    a = bpy.data.objects.get(item.a)
    base = Vector((math.floor(a.location.x if a else 0.0) + 0.5, PLAYGROUND_Y, 0.5))  # box corners on integers
    step = Vector((1.0, 0.0, 0.0)) if item.kind == "side" else Vector((0.0, 0.0, 1.0))
    for tag, name, turns, mirrored, at in (("A", item.a, item.ta, item.ma, base),
                                           ("B", item.b, item.tb, item.mb, base + step)):
        src = bpy.data.objects.get(name)
        if src is not None:
            _link_copy(coll, src, turns, at, f"seam{tag}", mirrored)


def _placement_text(src, turns, mirrored):
    """"MA78 r90" / "MA78 m r90": group + source hash, mirrored in X (m) then turned."""
    return f"{src['bb_level']}{src['bb_variant']}{src['bb_hash']} {'m ' if mirrored else ''}r{turns * 90}"


def _fresh_collection(scene, name):
    """The named collection, linked to the scene and emptied (it only ever holds our copies)."""
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    if coll.name not in scene.collection.children:
        scene.collection.children.link(coll)
    for obj in list(coll.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    return coll


def _link_copy(coll, src, turns, at, prefix, mirrored=False):
    """A copy of `src` sharing its mesh, mirrored in X (scale x -1) if `mirrored`, then turned `turns`
    quarter turns about Z. It remembers its tile (bb_src) and its placed hash (bb_box_hash, for Corner
    Boxes); it has no bb_hash, so Arrange, Seam Check and the labels ignore it."""
    placed = placed_hash(src["bb_hash"], turns, mirrored)
    # Named by what it is, not by the source mesh's name: "seamA MA78 r90 = 10111000" (OUR order, as placed).
    copy = bpy.data.objects.new(f"{prefix} {_placement_text(src, turns, mirrored)} = {to_ours(placed):08b}",
                                src.data)
    copy.location = at
    copy.rotation_euler = (0.0, 0.0, turns * math.pi / 2.0)
    copy.scale = (-1.0 if mirrored else 1.0, 1.0, 1.0)  # Blender applies scale before rotation
    copy["bb_src"] = src.name
    copy["bb_turns"] = turns
    copy["bb_mirrored"] = mirrored
    copy["bb_box_hash"] = placed
    coll.objects.link(copy)
    return copy


# ---------------------------------------------------------------------------------------------------
# Playground: the selected tile in a 4 x 4 x 4 block of its own level, at cell PG_TILE_CELL. That
# cell's 8 corners come from the tile; every other corner of the 5 x 5 x 5 corner grid is random (Shuffle
# picks new ones). Each cell's hash is read from its corners and filled with a tile of the same level
# (the tile's own variant first) turned 0-3 quarter turns. A cell with no tile gets an empty cube
# (Corner Boxes shows its corners). Copies share mesh data, so editing one edits the real tile.

TILE_PLAYGROUND = "BB Playground"
TILE_PLAYGROUND_Y = 7.5  # the selected tile's row; the block spans y 6.5 .. 9.5, beyond the seam pair


def _playground_lookup(meshes, level, variant, mirrors=False):
    """{hash: (mesh, turns, mirrored)} for one level; meshes of `variant` win, and a mesh turned wins
    over a mirrored one (mirrors are only tried when `mirrors`)."""
    lookup = {}
    ordered = sorted((o for o in meshes if o["bb_level"] == level), key=lambda o: (o["bb_variant"] != variant, o.name))
    for mirrored in ((False, True) if mirrors else (False,)):
        for obj in ordered:
            for turns in range(4):
                lookup.setdefault(placed_hash(obj["bb_hash"], turns, mirrored), (obj, turns, mirrored))
    return lookup


PG_SIZE = 4  # cells per side (x, y and z)
PG_TILE_CELL = (1, 1, 1)  # where an M tile sits in the block (an L tile goes in the bottom layer, H in the top)
# Levels, like the seam rules (each level only beside its own kind; L under M under H):
#   bottom layer: L. Its corners are chosen so L has every bottom cell (M only when impossible, e.g.
#                 the selected M tile forces a pattern L lacks).
#   above:        H for a cell with no top corner solid (a roof: H has every such pattern), else M.
#                 An H cell spreads: a left / right / front / back neighbour joined to it (a solid
#                 corner on their shared wall) gets its top corners cleared so it is H too, and so
#                 on. Only the selected tile's own corners are never cleared; a roof region still
#                 joined to a non-roof cell (only possible next to the tile) stays M, so H only ever
#                 sits beside H.
# Outer faces of the block whose corners are always empty, as (axis, side): the 4 sides and the top,
# so only the corners between them are random. The bottom face is the ground and stays random: with
# it empty too, every bottom cell would have only top corners, a pattern L never has, so the bottom
# layer could never be L. Add (2, -1) to empty it anyway.
PG_EMPTY_FACES = ((0, -1), (0, 1), (1, -1), (1, 1), (2, 1))
PG_BOTTOM_LEVELS = ("L", "M")  # tried in order in the bottom layer
PG_ROOF_LEVELS = ("H", "M")  # above it, for a cell with no top corner solid
PG_UPPER_LEVELS = ("M",)  # above it, any other cell
SIDES = ((1, 0, 0), (-1, 0, 0), (0, 1, 0), (0, -1, 0))
TOP_BITS = tuple(i for i, c in enumerate(CORNERS) if c[2] > 0)


def _cell_levels(cell, h_cells):
    if cell[2] == 0:
        return PG_BOTTOM_LEVELS
    return PG_ROOF_LEVELS if cell in h_cells else PG_UPPER_LEVELS


def _spread_roofs(grid, corner, fixed):
    """Every H (roof) cell above the bottom layer makes the side neighbours joined to it roofs too, by
    clearing their top corners (never a corner in `fixed`). Repeats until nothing changes."""
    def keys(cell):
        return [corner(cell, c) for c in CORNERS]

    def roof(cell):
        ks = keys(cell)
        return any(grid[k] for k in ks) and not any(grid[ks[i]] for i in TOP_BITS)

    cells = [(i, j, k) for i in range(PG_SIZE) for j in range(PG_SIZE) for k in range(1, PG_SIZE)]
    changed = True
    while changed:
        changed = False
        for cell in cells:
            if not roof(cell):
                continue
            for d in SIDES:
                other = tuple(cell[a] + d[a] for a in range(3))
                if not all(0 <= other[a] < PG_SIZE for a in range(3)) or roof(other):
                    continue
                shared = set(keys(cell)) & set(keys(other))
                if not any(grid[k] for k in shared):
                    continue  # not joined: nothing solid on their shared wall
                tops = [k for k in (keys(other)[i] for i in TOP_BITS) if grid[k]]
                if not any(k in fixed for k in tops):  # (a neighbour keeping the tile's corner stays)
                    for k in tops:  # the neighbour becomes a roof too
                        grid[k] = False
                    changed = True


def _h_cells(grid, corner, tile_cell, tile_is_h):
    """The roof cells that get H: every roof cell above the bottom layer, except whole roof regions
    still joined (shared solid corner on a side wall) to a non-H cell, which stay M, so H only ever
    sits beside H. After _spread_roofs that only happens next to the selected tile: its kept corners,
    or the tile itself when it is a roof-shaped M tile (it is placed as itself, M)."""
    def keys(cell):
        return [corner(cell, c) for c in CORNERS]

    def roof(cell):
        ks = keys(cell)
        return any(grid[k] for k in ks) and not any(grid[ks[i]] for i in TOP_BITS)

    h_cells = {(i, j, k) for i in range(PG_SIZE) for j in range(PG_SIZE) for k in range(1, PG_SIZE)
               if roof((i, j, k))}
    if tile_is_h:
        h_cells.add(tile_cell)
    else:
        h_cells.discard(tile_cell)
    changed = True
    while changed:
        changed = False
        for cell in list(h_cells):
            for d in SIDES:
                other = tuple(cell[a] + d[a] for a in range(3))
                if not all(0 <= other[a] < PG_SIZE for a in range(3)) or other in h_cells:
                    continue
                if any(grid[k] for k in set(keys(cell)) & set(keys(other))):
                    h_cells.discard(cell)  # joined to an M cell: the whole region falls back to M
                    changed = True
                    break
    return h_cells


def _solve_layer(grid, cells, ok, corner, rng, budget=20000):
    """Fill the unset corners of `cells` (backtracking, random order) so every cell's hash is in `ok`.
    On failure the cells' free corners are left unset (filled at random later)."""
    left = [budget]

    def solve(index):
        if index == len(cells):
            return True
        left[0] -= 1
        if left[0] < 0:
            return False
        keys = [corner(cells[index], c) for c in CORNERS]
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


def build_playground(scene, tile, seed):
    """Returns how many cells had no tile."""
    import random

    rng = random.Random(seed)
    n = PG_SIZE + 1  # corners per side

    def corner(cell, c):  # grid index of a cell's corner c
        return tuple(cell[a] + int(c[a] + 0.5) for a in range(3))

    tile_layer = {"L": 0, "H": PG_SIZE - 1}.get(tile["bb_level"], PG_TILE_CELL[2])
    tile_cell = PG_TILE_CELL[:2] + (tile_layer,)
    grid = {corner(tile_cell, c): bool((tile["bb_hash"] >> i) & 1)  # the tile's cell keeps its corners
            for i, c in enumerate(CORNERS)}
    fixed = set(grid)
    for key in ((i, j, k) for i in range(n) for j in range(n) for k in range(n)):
        on_face = any(key[axis] == (0 if side < 0 else n - 1) for axis, side in PG_EMPTY_FACES)
        if on_face and key not in grid:
            grid[key] = False  # corners on the block's outer faces stay empty: only inner corners are random
    meshes = tagged_meshes(scene)
    mirrors = getattr(scene, "bb_hide_mirrored", False)
    if mirrors:  # the 52: mirrored meshes left out, every mesh tried mirrored instead
        hidden = mirrored_meshes(meshes)
        meshes = [o for o in meshes if o not in hidden]
    lookups = {level: _playground_lookup(meshes, level, tile["bb_variant"], mirrors)
               for level in set(PG_BOTTOM_LEVELS + PG_ROOF_LEVELS + PG_UPPER_LEVELS)}
    # Bottom layer: pick its corners so every bottom cell is an L pattern, turned (or mirrored) any
    # way; only if that's impossible does a bottom cell fall back to M.
    ok = set(lookups[PG_BOTTOM_LEVELS[0]]) | {0, 255}
    bottom = [(ci, cj, 0) for ci in range(PG_SIZE) for cj in range(PG_SIZE)]
    if not _solve_layer(grid, bottom, ok, corner, rng):
        print(f"BrickBlock playground: no all-{PG_BOTTOM_LEVELS[0]} bottom layer fits {tile.name}; mixing in M")
    for key in ((i, j, k) for i in range(n) for j in range(n) for k in range(n)):
        if key not in grid:
            grid[key] = rng.random() < 0.5
    _spread_roofs(grid, corner, fixed)
    h_cells = _h_cells(grid, corner, tile_cell, tile["bb_level"] == "H")
    coll = _fresh_collection(scene, TILE_PLAYGROUND)
    # Cell (0, 0, 0) sits on the ground (z 0.5), in the tile's column; origins on x.5 keep box corners integer.
    first = Vector((math.floor(tile.location.x) + 0.5 - tile_cell[0], TILE_PLAYGROUND_Y - tile_cell[1], 0.5))
    missing = 0
    for ci in range(PG_SIZE):
        for cj in range(PG_SIZE):
            for ck in range(PG_SIZE):
                cell = (ci, cj, ck)
                h = sum(1 << i for i, c in enumerate(CORNERS) if grid[corner(cell, c)])
                at = first + Vector(cell)
                found = next((lookups[level][h] for level in _cell_levels(cell, h_cells) if h in lookups[level]),
                             None)
                if cell == tile_cell:
                    _link_copy(coll, tile, 0, at, "pg")
                elif found:
                    src, turns, mirrored = found
                    _link_copy(coll, src, turns, at, "pg", mirrored)
                elif h not in (0, 255):  # empty and full cells have nothing to draw
                    box = bpy.data.objects.new(f"pg missing {h}", None)
                    box.empty_display_type = "CUBE"
                    box.empty_display_size = 0.5
                    box.location = at
                    box["bb_box_hash"] = h
                    coll.objects.link(box)
                    missing += 1
    return missing


def _playground_tile(context):
    obj = context.active_object
    if obj is None:
        return None
    if "bb_hash" in obj:
        return obj
    return bpy.data.objects.get(obj.get("bb_src", ""))  # a copy: centre on its tile


class BB_OT_playground(bpy.types.Operator):
    """The selected tile (or the tile of a selected copy) in a 4 x 4 x 4 block of random neighbours of its level"""

    bl_idname = "brickblock.playground"
    bl_label = "Playground"
    bl_options = {"REGISTER", "UNDO"}

    shuffle: bpy.props.BoolProperty(default=False, options={"SKIP_SAVE"})

    def execute(self, context):
        scene = context.scene
        sync_from_names(scene)
        tile = bpy.data.objects.get(scene.bb_pg_tile) if self.shuffle else _playground_tile(context)
        if tile is None:
            self.report({"WARNING"}, "Select a BrickBlock mesh (or a playground copy)")
            return {"CANCELLED"}
        if self.shuffle:
            scene.bb_pg_seed += 1
        scene.bb_pg_tile = tile.name
        missing = build_playground(scene, tile, scene.bb_pg_seed)
        self.report({"INFO"}, f"Playground for {tile.name}" + (f": {missing} cells have no tile" if missing else ""))
        return {"FINISHED"}


class BB_OT_playground_clear(bpy.types.Operator):
    """Remove the playground and seam-pair copies (the real tiles are untouched)"""

    bl_idname = "brickblock.playground_clear"
    bl_label = "Clear"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        for name in (TILE_PLAYGROUND, PLAYGROUND):
            coll = bpy.data.collections.get(name)
            if coll is not None:
                for obj in list(coll.objects):
                    bpy.data.objects.remove(obj, do_unlink=True)
                bpy.data.collections.remove(coll)
        context.scene.bb_seam_index = -1
        return {"FINISHED"}


class BB_UL_seams(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index=0, flt_flag=0):
        arrow = "|" if item.kind == "side" else "/"  # "|": A left of B, "/": A under B

        def text(name, turns, mirrored):
            src = bpy.data.objects.get(name)
            return _placement_text(src, turns, mirrored) if src is not None and "bb_hash" in src else name

        row = layout.row()
        row.label(text=f"{text(item.a, item.ta, item.ma)}  {arrow}  {text(item.b, item.tb, item.mb)}",
                  icon="ARROW_LEFTRIGHT" if item.kind == "side" else "SORT_ASC")
        row.label(text=str(item.cells))

    def filter_items(self, context, data, propname):
        items = getattr(data, propname)
        active = context.active_object
        if not (context.scene.bb_seam_selected_only and active):
            return [self.bitflag_filter_item] * len(items), []
        name = active.name
        return [self.bitflag_filter_item if name in (it.a, it.b) else 0 for it in items], []


def _draw_labels():
    context = bpy.context
    if not getattr(context.scene, "bb_labels", False):
        return
    region, rv3d = context.region, context.region_data
    if region is None or rv3d is None:
        return
    blf.size(0, 11)
    blf.color(0, 1.0, 0.85, 0.4, 1.0)
    for obj in tagged_meshes(context.scene):
        if not obj.visible_get():
            continue
        point = location_3d_to_region_2d(region, rv3d, obj.matrix_world.translation + Vector((0.0, -0.6, -0.5)))
        if point is None:
            continue
        h = obj["bb_hash"]
        lines = [f"{obj['bb_level']}{obj['bb_variant']} {h}", f"{h:08b}", f"ours {to_ours(h):08b}"]
        mirror = obj.get("bb_mirror")  # set by Arrange
        if mirror is not None:
            lines.append({-1: "symmetric", -2: "no mirror"}.get(mirror, f"mirror {mirror}"))
        _draw_lines(point, lines)
    # Playground / seam copies: what was placed (source tile, mirror, turn) and the hash as placed.
    blf.color(0, 0.6, 0.9, 1.0, 1.0)
    for obj in context.scene.objects:
        src = bpy.data.objects.get(obj.get("bb_src", ""))
        if src is None or "bb_box_hash" not in obj or not obj.visible_get():
            continue
        point = location_3d_to_region_2d(region, rv3d, obj.matrix_world.translation + Vector((0.0, -0.6, -0.5)))
        if point is not None:
            placed = obj["bb_box_hash"]
            _draw_lines(point, [_placement_text(src, obj.get("bb_turns", 0), obj.get("bb_mirrored", False)),
                                f"ours {to_ours(placed):08b}"])


def _draw_lines(point, lines):
    for n, text in enumerate(lines):
        width, _ = blf.dimensions(0, text)
        blf.position(0, point.x - width / 2.0, point.y - n * 13, 0.0)
        blf.draw(0, text)


def _draw_boxes():
    """Each visible mesh's cell box: grey wireframe, orange dots on solid corners, light blue on empty."""
    context = bpy.context
    if not getattr(context.scene, "bb_corner_boxes", False):
        return
    lines, solid, empty = [], [], []
    for obj in context.scene.objects:
        # Tiles (bb_hash) and playground / seam copies (bb_box_hash: the hash as turned in the world).
        h = obj.get("bb_hash", obj.get("bb_box_hash"))
        if h is None or not obj.visible_get() or any(key in obj for key in ADDON_HELPER_KEYS):
            continue
        # A 1 m cube centred on the origin: corners at origin + (+-0.5, +-0.5, +-0.5), not turned.
        origin = obj.matrix_world.translation
        points = [origin + Vector(c) for c in CORNERS]
        for a, b in EDGES:
            lines += (points[a], points[b])
        for i, p in enumerate(points):
            (solid if (h >> i) & 1 else empty).append(p)
    if not lines:
        return
    gpu.state.depth_test_set("NONE")
    gpu.state.blend_set("ALPHA")
    line_shader = gpu.shader.from_builtin("POLYLINE_UNIFORM_COLOR")
    line_shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
    line_shader.uniform_float("lineWidth", 1.0)
    line_shader.uniform_float("color", WIRE)
    batch_for_shader(line_shader, "LINES", {"pos": lines}).draw(line_shader)
    point_shader = gpu.shader.from_builtin("POINT_UNIFORM_COLOR")
    for points, colour, size in ((empty, EMPTY, 7.0), (solid, SOLID, 10.0)):
        if points:
            gpu.state.point_size_set(size)
            point_shader.uniform_float("color", colour)
            batch_for_shader(point_shader, "POINTS", {"pos": points}).draw(point_shader)
    gpu.state.point_size_set(1.0)
    gpu.state.blend_set("NONE")


def _redraw(_self, context):
    for area in context.screen.areas if context.screen else ():
        if area.type == "VIEW_3D":
            area.tag_redraw()


class BB_PT_panel(bpy.types.Panel):
    bl_label = "BrickBlock"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "BrickBlock"

    def draw(self, context):
        layout = self.layout
        layout.operator("brickblock.remove_empties", icon="EMPTY_AXIS")
        scene = context.scene
        row = layout.row(align=True)
        row.prop(scene, "bb_labels", toggle=True, icon="SMALL_CAPS")
        row.prop(scene, "bb_corner_boxes", toggle=True, icon="MESH_CUBE")
        row = layout.row(align=True)
        row.label(text="Levels:")
        sub = row.row(align=True)
        sub.enabled = not scene.bb_only_ma
        for level in LEVELS:
            sub.prop(scene, "bb_show_" + level)
        layout.prop(scene, "bb_only_ma")
        row = layout.row(align=True)
        row.operator("brickblock.arrange", icon="GRID")
        row.operator("brickblock.restore", icon="LOOP_BACK")
        layout.prop(scene, "bb_hide_mirrored", toggle=True, icon="MOD_MIRROR")
        meshes = shown_meshes(scene)
        present = {o["bb_level"] + o["bb_variant"] for o in meshes}
        layout.label(text="Rows: " + " ".join(g for g in ROWS if g in present))
        if meshes:
            layout.label(text=f"{len(meshes)} meshes, {len({o['bb_hash'] for o in meshes})} hashes")
        row = layout.row(align=True)
        row.operator("brickblock.convert_names", icon="SORTALPHA")
        row.operator("brickblock.restore_names", text="", icon="LOOP_BACK")
        row.operator("brickblock.export", icon="EXPORT")
        row = layout.row(align=True)
        row.operator("brickblock.playground", icon="MESH_GRID")
        row.operator("brickblock.playground", text="Shuffle", icon="FILE_REFRESH").shuffle = True
        row.operator("brickblock.playground_clear", icon="X")
        box = layout.box()
        row = box.row(align=True)
        row.operator("brickblock.seam_check", icon="VIEWZOOM")
        row.prop(scene, "bb_seam_selected_only", text="", icon="RESTRICT_SELECT_OFF")
        if scene.bb_seam_summary:
            box.label(text=f"Clashes: {scene.bb_seam_summary}")
            box.template_list("BB_UL_seams", "", scene, "bb_seams", scene, "bb_seam_index", rows=8)
            box.label(text=f"Click a pair: copies at y {PLAYGROUND_Y}; full list in text '{REPORT}'")


CLASSES = (BB_SeamItem, BB_OT_remove_empties, BB_OT_arrange, BB_OT_restore, BB_OT_seam_check, BB_UL_seams,
           BB_OT_playground, BB_OT_playground_clear, BB_OT_convert_names, BB_OT_restore_names, BB_OT_export,
           BB_PT_panel)


PROPS = ("bb_labels", "bb_corner_boxes", "bb_seams", "bb_seam_index", "bb_seam_selected_only",
         "bb_seam_summary", "bb_pg_seed", "bb_pg_tile", "bb_only_ma", "bb_hide_mirrored") + tuple("bb_show_" + level for level in LEVELS)
HANDLES = "brickblock_draw_handles"  # kept in driver_namespace so a re-run can remove the old handlers


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    scene = bpy.types.Scene
    scene.bb_labels = bpy.props.BoolProperty(name="Hash Labels", update=_redraw)
    scene.bb_corner_boxes = bpy.props.BoolProperty(
        name="Corner Boxes", description="Cell box wireframe, orange = solid corner, blue = empty", update=_redraw)
    scene.bb_seams = bpy.props.CollectionProperty(type=BB_SeamItem)
    scene.bb_seam_index = bpy.props.IntProperty(default=-1, update=_show_pair)
    scene.bb_seam_selected_only = bpy.props.BoolProperty(
        name="Selected Only", description="Only list pairs with the active mesh")
    scene.bb_seam_summary = bpy.props.StringProperty()
    scene.bb_only_ma = bpy.props.BoolProperty(
        name="Only MA", update=_on_levels,
        description="Show only the MA meshes; Arrange, Seam Check and the level ticks then use just MA")
    scene.bb_hide_mirrored = bpy.props.BoolProperty(
        name="Hide Mirrored", update=_on_levels,
        description="Hide the second mesh of each mirror pair: it is one of the 8 turns / mirrors of the "
                    "mesh in front of it (Arrange still places it)")
    scene.bb_pg_seed = bpy.props.IntProperty()
    scene.bb_pg_tile = bpy.props.StringProperty()
    for level in LEVELS:
        setattr(scene, "bb_show_" + level, bpy.props.BoolProperty(
            name=level, default=True, update=_on_levels,
            description=f"Show the {level} meshes (Arrange only lays out the ticked levels)"))
    bpy.app.driver_namespace[HANDLES] = [
        bpy.types.SpaceView3D.draw_handler_add(_draw_labels, (), "WINDOW", "POST_PIXEL"),
        bpy.types.SpaceView3D.draw_handler_add(_draw_boxes, (), "WINDOW", "POST_VIEW"),
    ]


def unregister():
    for handle in bpy.app.driver_namespace.pop(HANDLES, ()):
        bpy.types.SpaceView3D.draw_handler_remove(handle, "WINDOW")
    for prop in PROPS:
        if hasattr(bpy.types.Scene, prop):
            delattr(bpy.types.Scene, prop)
    for cls in reversed(CLASSES):
        old = getattr(bpy.types, cls.__name__, None)  # the class from an earlier run of this script
        if old is not None:
            bpy.utils.unregister_class(old)


if __name__ == "__main__":
    try:  # running it again in the same session replaces the old version
        unregister()
    except (RuntimeError, AttributeError, ValueError):
        pass
    register()
