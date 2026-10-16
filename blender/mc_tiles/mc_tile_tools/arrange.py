"""Arrange: move the active set's tiles into a tidy grid, and put them back with Restore.

Layouts (Scene.mc_tiles_arrange.layout):
    CORNERS     one row per number of solid corners (1 at the top), sorted by hash
    HASH        every tile in hash order, wrapped at Columns
    FAMILY      each tile next to its mirror shape (and any rotated copies), wrapped at Columns
    HEIGHTMAP   rows by how many top corners are solid (0 = flat at the bottom level, 4 = flat
                one level up); tiles with an overhang go in the last rows, wrapped at Columns

Only the tiles' positions change (rotation, scale and meshes stay), so export and Godot don't
care. The first Arrange stores each tile's own position in obj["mc_home"]; Restore moves every
tile back there and forgets it, so you can arrange as often as you like and still get your
hand-made layout back.

Slots for shapes with no tile go in a separate "Arrange" collection (never a tile, never exported):
    mirror on export   a flipped copy of the source tile (shares its mesh, can't be selected)
    missing            a red box
With "Filtered First" on, the tiles the filter shows (by default: the shapes the set's mode uses)
are laid out first; every other tile goes in a block below, after an empty row (hidden too when
Filter Viewport is on). Mirror / missing slots are only made for shapes the filter shows.
"""

import math

import bpy
from mathutils import Matrix, Vector

from . import cube
from .modes import has_overhang
from .playground import _missing_material
from .tilefilter import hash_shown, shown_tiles
from .tiles import tile_variants

COLLECTION_NAME = "Arrange"
HOME = "mc_home"  # custom property: the tile's own position before the first Arrange
LABEL = "mc_ar_label"  # custom property on slot objects: text the label overlay draws
VARIANT_STEP = 0.5  # variant rows behind their base tile, as a fraction of Spacing (2 m -> 1 m)

LAYOUT_ITEMS = [
    ("CORNERS", "By Corner Count", "One row per number of solid corners"),
    ("HASH", "By Hash", "Hash order, wrapped at Columns"),
    ("FAMILY", "By Family", "Each tile next to its mirror shape"),
    ("HEIGHTMAP", "Heightmap", "Rows by number of solid top corners; overhangs last"),
]


class Entry:
    """One slot: a modelled tile, a shape export makes by mirror, or a missing shape."""

    def __init__(self, h, kind, objs=()):
        self.h = h
        self.kind = kind  # "TILE", "MIRROR", "PIECE" (borrowed from another collection) or "MISSING"
        self.objs = list(objs)  # TILE: base + variants; MIRROR: the source tile's meshes


# ---------------------------------------------------------------- which slots (pure logic)


def entries(collection, scene):
    """(main, rest): slots laid out first, and tiles the filter leaves out (Filtered First)."""
    from .piece_lib import set_tiles

    mode = collection.mc_tiles.mode
    variants = tile_variants(collection, "CUBE", role=None)  # every role: L / H go behind M a
    drawn = set_tiles(collection)  # M a tiles + pieces borrowed from other collections
    report = cube.check(drawn.keys(), mode, pieces=collection.mc_tiles.build_from_pieces)
    own = {o for objs in variants.values() for o in objs}
    settings = scene.mc_tiles_arrange
    f = scene.mc_tiles_filter
    filtered = settings.only_filtered
    shown = shown_tiles(collection, f) if filtered else None

    out, rest = [], []
    for h, objs in variants.items():
        kept = [o for o in objs if shown is None or shown[o]]
        left = [o for o in objs if o not in kept]
        if kept:
            out.append(Entry(h, "TILE", kept))
        if left:
            rest.append(Entry(h, "TILE", left))
    if settings.show_slots:
        # Label a mirror slot with the flipped source's own hash, so label and shape agree.
        out += [Entry(cube.mirror(src), "MIRROR", drawn[src][:1]) for _, src in report.mirrored]
        out += [Entry(h, "PIECE", objs[:1]) for h, objs in drawn.items() if objs[0] not in own]
        out += [Entry(h, "MISSING") for h in report.missing]
        if filtered:
            out = [e for e in out if e.kind == "TILE" or hash_shown(f, collection, e.h)]
    return out, rest


def _wrap(items, columns):
    """Split a list into rows of at most `columns`."""
    columns = max(1, columns)
    return [items[i:i + columns] for i in range(0, len(items), columns)]


def rows(layout, items, columns, mode):
    """[[Entry or None]]: None leaves a gap. Row 0 is drawn at the top (furthest +Y)."""
    items = sorted(items, key=lambda e: e.h)
    if layout == "CORNERS":
        by_count = {}
        for e in items:
            by_count.setdefault(bin(e.h).count("1"), []).append(e)
        return [by_count[n] for n in sorted(by_count)]
    if layout == "HASH":
        return _wrap(items, columns)
    if layout == "HEIGHTMAP":
        flat = {}
        overhang = []
        for e in items:
            if has_overhang(e.h):
                overhang.append(e)
            else:
                flat.setdefault(bin(e.h >> 4).count("1"), []).append(e)
        out = [sorted(flat[n], key=lambda e: (bin(e.h & 0xF).count("1"), e.h)) for n in sorted(flat)]
        return out + _wrap(overhang, columns)
    # FAMILY: shapes related by Y turns or a left/right flip sit together, a gap between families.
    rotations = cube.MODE_ROTATIONS[mode]
    families = {}
    for e in items:
        key = min(cube.orbit(e.h, rotations) | cube.orbit(cube.mirror(e.h), rotations))
        families.setdefault(key, []).append(e)
    out, row = [], []
    for key in sorted(families):
        block = families[key]
        if row and len(row) + len(block) > columns:
            out.append(row)
            row = []
        row += block + [None]
    if row:
        out.append(row)
    return [r[:-1] if r and r[-1] is None else r for r in out]


# ---------------------------------------------------------------- Blender objects


def _arrange_collection(scene):
    collection = bpy.data.collections.get(COLLECTION_NAME)
    if collection is None:
        collection = bpy.data.collections.new(COLLECTION_NAME)
    if collection not in set(scene.collection.children_recursive):
        scene.collection.children.link(collection)
    return collection


def clear_slots():
    """Delete the mirror / missing slot objects (real tiles are never touched)."""
    collection = bpy.data.collections.get(COLLECTION_NAME)
    if collection is None:
        return
    for obj in list(collection.all_objects):
        bpy.data.objects.remove(obj, do_unlink=True)


def _home(obj):
    return Vector(obj[HOME]) if HOME in obj else obj.matrix_world.translation.copy()


def _origin(collection):
    """Top-left of the layout: the set's own top-left (+Y), from home positions, so it stays put."""
    roots = [o for objs in tile_variants(collection, "CUBE", role=None).values() for o in objs]
    if not roots:
        return Vector((0.0, 0.0, 0.0))
    homes = [_home(o) for o in roots]
    # Snapped like the playground: cell centres (x.5, y.5, z.5), so box corners are whole numbers.
    return Vector((
        math.floor(min(p.x for p in homes)) + 0.5,
        math.floor(max(p.y for p in homes)) + 0.5,
        math.floor(min(p.z for p in homes)) + 0.5,
    ))


def _move(obj, position):
    """Move a tile root (children follow), remembering its own position the first time."""
    if HOME not in obj:
        obj[HOME] = list(obj.matrix_world.translation)
    obj.matrix_world.translation = position


def _mirror_slot(collection, source, position, h, flip=True, label="mirror"):
    """Copy of the source tile (and its parts) sharing its meshes, flipped for a mirror slot, not
    selectable; also used for a piece borrowed from another collection (flip=False, label "piece")."""
    copies = {}
    for obj in [source] + list(source.children_recursive):
        copy = obj.copy()
        copy.name = f"ar_mirror_{h:08b}_{obj.name}"
        copy.hide_select = True
        for key in (HOME, "mc_filter_hidden"):
            if key in copy:
                del copy[key]
        collection.objects.link(copy)
        copies[obj] = copy
    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]
            copy.matrix_parent_inverse = obj.matrix_parent_inverse.copy()
            copy.matrix_basis = obj.matrix_basis.copy()
    root = copies[source]
    flipping = Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))) if flip else Matrix.Identity(4)
    root.matrix_world = Matrix.Translation(position) @ flipping @ source.matrix_world.to_3x3().to_4x4()
    root[LABEL] = f"{h:08b} {label}"


def _missing_slot(collection, position, h):
    mesh = bpy.data.meshes.get("ar_missing_box")
    if mesh is None:
        mesh = bpy.data.meshes.new("ar_missing_box")
        s = 0.3
        verts = [(x, y, z) for x in (-s, s) for y in (-s, s) for z in (-s, s)]
        faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
        mesh.from_pydata(verts, [], faces)
        mesh.materials.append(_missing_material())
    obj = bpy.data.objects.new(f"ar_missing_{h:08b}", mesh)
    obj.location = position
    obj.hide_select = True
    obj[LABEL] = f"{h:08b} missing"
    collection.objects.link(obj)


def arrange(scene, collection):
    """Lay out the set. Returns (tiles moved, mirror slots, missing slots)."""
    settings = scene.mc_tiles_arrange
    spacing = float(max(1, math.ceil(settings.spacing - 1e-6)))  # whole metres: box corners stay whole
    behind_step = float(max(1, round(spacing * VARIANT_STEP)))
    items, rest = entries(collection, scene)
    most_variants = max((len(e.objs) for e in items + rest if e.kind == "TILE"), default=1)
    behind = settings.variants_behind
    pitch = spacing + (behind_step * (most_variants - 1) if behind else 0.0)

    if not behind:  # variants get their own slots right after the base
        items, rest = _one_per_mesh(items), _one_per_mesh(rest)

    mode = collection.mc_tiles.mode
    grid = rows(settings.layout, items, settings.columns, mode)
    if rest:  # left out by the filter: an empty row, then hash order
        grid += [[]] + _wrap(sorted(rest, key=lambda e: e.h), settings.columns)

    clear_slots()
    slots = _arrange_collection(scene)
    origin = _origin(collection)
    counts = {"TILE": 0, "MIRROR": 0, "MISSING": 0}
    for r, row in enumerate(grid):
        for c, e in enumerate(row):
            if e is None:
                continue
            position = origin + Vector((c * spacing, -r * pitch, 0.0))
            if e.kind == "TILE":
                for k, obj in enumerate(e.objs):
                    _move(obj, position + Vector((0.0, k * behind_step, 0.0)))
                counts["TILE"] += len(e.objs)
            elif e.kind == "MIRROR":
                _mirror_slot(slots, e.objs[0], position, e.h)
                counts["MIRROR"] += 1
            elif e.kind == "PIECE":
                _mirror_slot(slots, e.objs[0], position, e.h, flip=False, label="piece")
                counts["MIRROR"] += 1
            else:
                _missing_slot(slots, position, e.h)
                counts["MISSING"] += 1
    return counts["TILE"], counts["MIRROR"], counts["MISSING"]


def _one_per_mesh(items):
    """Split each tile entry so every variant mesh gets its own slot."""
    flat = []
    for e in items:
        flat += [Entry(e.h, e.kind, [o]) for o in e.objs] if e.kind == "TILE" else [e]
    return flat


def restore(collection):
    """Move every arranged tile back to its own position. Returns how many moved."""
    moved = 0
    for objs in tile_variants(collection, "CUBE", role=None).values():
        for obj in objs:
            if HOME in obj:
                obj.matrix_world.translation = Vector(obj[HOME])
                del obj[HOME]
                moved += 1
    clear_slots()
    return moved


# ---------------------------------------------------------------- operators and panel


def _cube_set(context):
    collection = context.scene.mc_tiles_active
    return collection if collection is not None and collection.mc_tiles.kind == "CUBE" else None


class MCTILES_OT_arrange(bpy.types.Operator):
    """Move the set's tiles into the chosen layout (Restore puts them back)"""

    bl_idname = "mc_tiles.arrange"
    bl_label = "Arrange"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return _cube_set(context) is not None

    def execute(self, context):
        tiles, mirrors, missing = arrange(context.scene, _cube_set(context))
        self.report({"INFO"}, f"Arranged {tiles} meshes, {mirrors} mirror slots, {missing} missing")
        return {"FINISHED"}


class MCTILES_OT_arrange_restore(bpy.types.Operator):
    """Move every tile back to where it was before the first Arrange"""

    bl_idname = "mc_tiles.arrange_restore"
    bl_label = "Restore Layout"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return _cube_set(context) is not None

    def execute(self, context):
        moved = restore(_cube_set(context))
        self.report({"INFO"}, f"Restored {moved} meshes")
        return {"FINISHED"}


class MCTILES_OT_arrange_clear_slots(bpy.types.Operator):
    """Delete the mirror / missing slot objects"""

    bl_idname = "mc_tiles.arrange_clear_slots"
    bl_label = "Clear Slots"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        clear_slots()
        return {"FINISHED"}


class MCTilesArrangeProps(bpy.types.PropertyGroup):
    layout: bpy.props.EnumProperty(name="Layout", items=LAYOUT_ITEMS, default="CORNERS")
    spacing: bpy.props.FloatProperty(name="Spacing", default=2.0, min=1.0, max=10.0, step=100, unit="LENGTH",
                                     description="Whole metres keep every tile's box corners on whole numbers")
    columns: bpy.props.IntProperty(name="Columns", description="Row length for By Hash / By Family / Heightmap overhang rows",
                                   default=12, min=1, max=100)
    variants_behind: bpy.props.BoolProperty(
        name="Variants Behind", default=True,
        description="Put variants (_ma2, _ma3...) and the L / H tiles of a hash behind its base tile "
                    "instead of in their own slots",
    )
    show_slots: bpy.props.BoolProperty(
        name="Mirror / Missing Slots", default=True,
        description="Fill the slots of shapes with no tile: a flipped copy (mirror on export) or a red box",
    )
    only_filtered: bpy.props.BoolProperty(
        name="Filtered First", default=True,
        description="Lay out the tiles the filter shows (default: the set's mode) first, the rest in a block below",
    )


def draw_arrange(layout, context):
    settings = context.scene.mc_tiles_arrange
    layout.prop(settings, "layout", text="")
    col = layout.column(align=True)
    col.prop(settings, "spacing")
    if settings.layout in ("HASH", "FAMILY", "HEIGHTMAP"):
        col.prop(settings, "columns")
    row = layout.row(align=True)
    row.prop(settings, "variants_behind", toggle=True)
    row.prop(settings, "show_slots", toggle=True)
    layout.prop(settings, "only_filtered")
    row = layout.row(align=True)
    row.operator("mc_tiles.arrange", icon="GRID")
    row.operator("mc_tiles.arrange_restore", text="Restore", icon="LOOP_BACK")
    row.operator("mc_tiles.arrange_clear_slots", text="", icon="TRASH")
    layout.prop(context.scene, "mc_tiles_labels", icon="SMALL_CAPS")


CLASSES = (MCTILES_OT_arrange, MCTILES_OT_arrange_restore, MCTILES_OT_arrange_clear_slots)


def register():
    bpy.utils.register_class(MCTilesArrangeProps)
    bpy.types.Scene.mc_tiles_arrange = bpy.props.PointerProperty(type=MCTilesArrangeProps)


def unregister():
    del bpy.types.Scene.mc_tiles_arrange
    bpy.utils.unregister_class(MCTilesArrangeProps)
