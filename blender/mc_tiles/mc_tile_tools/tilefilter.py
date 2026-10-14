"""Show a subset of the active set's tiles: one filter drives the tile list and the viewport.

Settings live on the scene (Scene.mc_tiles_filter):
    search        "0011" digits, "#3" = 3 solid corners, anything else = part of the name;
                  a full 8-digit hash no tile is named after finds the tile Godot turns /
                  mirrors into it (shown under the search box: "= 00011011 turned 2 x 90")
    subset        used by the set's mode (default: Heightmap hides overhangs) / all /
                  no overhang / overhang only                                      (cube sets)
    corners       only tiles with this many solid corners (0 = any)
    variants      all meshes / base only (hide _v2, _v3...) / only hashes with variants
    problems      only tiles Check flags (rotated copies, not allowed, mirror partners,
                  untidy names, variant clashes)                                    (cube sets)
    family        the picked tile plus every turned / mirrored shape of it          (cube sets)
    viewport      also hide the filtered-out tiles in the 3D view
    hide_helpers  also hide the set's helper objects (viewport only)

Hiding uses hide_set (eye icon, this view layer only): nothing moves, export ignores it.
Objects the filter hid are marked, so turning it off only unhides those and never anything
you hid yourself. Playground copies are never touched.
"""

import bpy

from . import cube
from .modes import has_overhang
from .names import untidy
from .tiles import tile_hash, tile_variants, variant_clashes

MARK = "mc_filter_hidden"  # custom property on objects this filter hid

SUBSET_ITEMS = [
    # Explicit numbers: .blend files save the number, and ALL / NO_OVERHANG / OVERHANG came first.
    ("MODE", "Used by Mode", "Only shapes the set's mode uses (Heightmap: no overhangs); full block always", 3),
    ("ALL", "All shapes", "Every tile", 0),
    ("NO_OVERHANG", "No overhang (Heightmap)", "Only shapes a heightmap can make: a top corner needs the one below", 1),
    ("OVERHANG", "Overhang only", "Only shapes with a solid top corner over an empty bottom corner", 2),
]
VARIANT_ITEMS = [
    ("ALL", "All meshes", "Base tiles and their variants"),
    ("BASE", "Base only", "Hide _v2, _v3... meshes"),
    ("HAS_VARIANTS", "With variants", "Only hashes that have more than one mesh"),
]


def search_matches(search, h, name):
    """Tile-list search rules: digits, #N corners, or part of the name."""
    if not search:
        return True
    if search.startswith("#") and search[1:].isdigit():
        return bin(h).count("1") == int(search[1:])
    if set(search) <= {"0", "1"}:
        return search in f"{h:08b}"
    return search.lower() in name.lower()


def family_hashes(h, mode):
    """h plus every hash Godot turns it into and their left/right mirrors."""
    rotations = cube.MODE_ROTATIONS[mode]
    return cube.orbit(h, rotations) | cube.orbit(cube.mirror(h), rotations)


def problem_hashes(collection):
    """Hashes of tiles the Check panel lists (only tiles that exist, so not "missing")."""
    roots = {h: objs[0] for h, objs in tile_variants(collection, "CUBE").items()}
    report = cube.check(roots.keys(), collection.mc_tiles.mode)
    found = {h for h, _ in report.duplicates} | set(report.not_allowed)
    found |= {h for h, _ in report.partners}
    found |= {h for h, _, _ in variant_clashes(collection)}
    found |= {tile_hash(obj.name, "CUBE") for obj, _ in untidy(collection)}
    found.discard(None)
    return found


def _hash_passes(f, h, mesh_count, is_cube, problems, family, mode):
    """Does every hash-level setting let this tile through?"""
    # Under Problems the mode's not-allowed tiles are exactly what you want to see.
    if is_cube and f.subset == "MODE" and problems is None and h != cube.FULL and not cube.allowed(h, mode):
        return False
    if is_cube and f.subset == "NO_OVERHANG" and has_overhang(h):
        return False
    if is_cube and f.subset == "OVERHANG" and not has_overhang(h):
        return False
    if f.corners and bin(h).count("1") != f.corners:
        return False
    if f.variants == "HAS_VARIANTS" and mesh_count < 2:
        return False
    if problems is not None and h not in problems:
        return False
    return family is None or h in family


def hash_shown(f, collection, h):
    """Would a shape with no tile (a mirror / missing slot) pass the filter? Never under Problems."""
    if f.problems_only:
        return False
    is_cube = collection.mc_tiles.kind == "CUBE"
    family = family_hashes(f.family_hash, collection.mc_tiles.mode) if f.family_hash >= 0 and is_cube else None
    search = f.search.strip()
    if search and not search_matches(search, h, f"{h:08b}"):
        return False
    mode = collection.mc_tiles.mode
    return f.variants != "HAS_VARIANTS" and _hash_passes(f, h, 1, is_cube, None, family, mode)


def drawn_by(collection, search):
    """For a full 8-digit hash with no tile of its own: [(tile hash, quarter turns, mirrored)] that draw
    it (one tile, or several pieces for a shape built from pieces), False when nothing does (missing),
    None when the search isn't such a hash."""
    if collection.mc_tiles.kind != "CUBE" or len(search) != 8 or not set(search) <= {"0", "1"}:
        return None
    from .piece_lib import set_tiles
    from .playground import tile_placements

    tiles = set_tiles(collection)
    h = int(search, 2)
    if h in tiles:
        return None
    settings = collection.mc_tiles
    return tile_placements(tiles.keys(), settings.mode, pieces=True).get(h, False)


def shown_tiles(collection, f):
    """{tile object: shown?} for every tile mesh (base and variants) in the set."""
    kind = collection.mc_tiles.kind
    is_cube = kind == "CUBE"
    problems = problem_hashes(collection) if f.problems_only and is_cube else None
    family = family_hashes(f.family_hash, collection.mc_tiles.mode) if f.family_hash >= 0 and is_cube else None
    search = f.search.strip()
    placed = drawn_by(collection, search)
    sources = {source for source, _, _ in placed} if placed else None  # turned / mirrored / pieces

    shown = {}
    for h, objs in tile_variants(collection, kind).items():
        passes = _hash_passes(f, h, len(objs), is_cube, problems, family, collection.mc_tiles.mode)
        for i, obj in enumerate(objs):
            base_ok = i == 0 or f.variants != "BASE"
            found = h in sources if sources is not None else search_matches(search, h, obj.name)
            shown[obj] = passes and base_ok and found
    return shown


def _named_children(collection, names):
    """Objects in child collections with one of these names (Helpers, Playground)."""
    found = set()
    for child in collection.children_recursive:
        if child.name.split(".")[0] in names:
            found.update(child.all_objects)
    return found


KEEP_VISIBLE = set()  # tile names always shown (the seam pair in the playground), see keep_visible()


def keep_visible(context, names):
    """Show these tiles whatever the filter says (until called again); unhides them now."""
    KEEP_VISIBLE.clear()
    KEEP_VISIBLE.update(n for n in names if n)
    apply(context)
    for name in KEEP_VISIBLE:
        obj = bpy.data.objects.get(name)
        if obj is not None:
            for part in [obj] + list(obj.children_recursive):
                _hide(part, False)  # also undoes a hand hide, so the pair is really visible


def _wanted_hidden(collection, f):
    """Objects of the set the filter should hide (children follow their root)."""
    if not f.viewport:
        return set()
    shown = shown_tiles(collection, f)
    for name in KEEP_VISIBLE:
        obj = bpy.data.objects.get(name)
        if obj in shown:
            shown[obj] = True
    playground = _named_children(collection, {"Playground"})
    roots = set()
    for obj in collection.all_objects:
        if obj.parent is not None or obj in playground:
            continue
        if obj in shown:
            if not shown[obj]:
                roots.add(obj)
        elif f.hide_helpers:
            roots.add(obj)  # not a tile: a helper (loose or in Helpers)
    hidden = set(roots)
    for obj in roots:
        hidden.update(obj.children_recursive)
    return hidden


def _hide(obj, hidden):
    """hide_set that ignores objects not in the view layer (excluded collections)."""
    try:
        if hidden == obj.hide_get():
            return False
        obj.hide_set(hidden)
        return True
    except RuntimeError:
        return False


def apply(context):
    """Hide what the filter excludes; unhide what it hid before and now shows."""
    scene = context.scene
    collection = scene.mc_tiles_active
    wanted = set()
    if collection is not None and collection.mc_tiles.is_tile_set:
        wanted = _wanted_hidden(collection, scene.mc_tiles_filter)

    for obj in scene.objects:
        if obj.get(MARK) and obj not in wanted:
            _hide(obj, False)
            del obj[MARK]
    for obj in wanted:
        # Already hidden by hand: leave it unmarked so turning the filter off keeps it hidden.
        if _hide(obj, True):
            obj[MARK] = True


def _update(_self, context):
    apply(context)


class MCTilesFilterProps(bpy.types.PropertyGroup):
    search: bpy.props.StringProperty(
        name="Search", description="Digits (0011), #3 for 3 solid corners, or part of a name", update=_update,
        options={"TEXTEDIT_UPDATE"},
    )
    subset: bpy.props.EnumProperty(name="Shapes", items=SUBSET_ITEMS, default="MODE", update=_update)
    corners: bpy.props.IntProperty(
        name="Corners", description="Only tiles with this many solid corners (0 = any)",
        min=0, max=8, default=0, update=_update,
    )
    variants: bpy.props.EnumProperty(name="Variants", items=VARIANT_ITEMS, default="ALL", update=_update)
    problems_only: bpy.props.BoolProperty(
        name="Problems", description="Only tiles the Check panel lists", update=_update,
    )
    family_hash: bpy.props.IntProperty(name="Family", default=-1, min=-1, max=255, update=_update)
    viewport: bpy.props.BoolProperty(
        name="Filter Viewport", description="Also hide filtered-out tiles in the 3D view", update=_update,
    )
    hide_helpers: bpy.props.BoolProperty(
        name="Hide Helpers", description="Also hide the set's helper objects", update=_update,
    )


def _root_tile(obj, kind):
    """The tile an object belongs to (itself, or the root of its parent chain)."""
    while obj is not None and obj.parent is not None:
        obj = obj.parent
    return obj if obj is not None and tile_hash(obj.name, kind) is not None else None


class MCTILES_OT_filter_family(bpy.types.Operator):
    bl_idname = "mc_tiles.filter_family"
    bl_label = "Family of Selected"
    bl_description = "Show only the selected tile and its turned / mirrored shapes"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and _root_tile(context.active_object, "CUBE") is not None

    def execute(self, context):
        tile = _root_tile(context.active_object, "CUBE")
        context.scene.mc_tiles_filter.family_hash = tile_hash(tile.name, "CUBE")
        return {"FINISHED"}


class MCTILES_OT_filter_family_clear(bpy.types.Operator):
    bl_idname = "mc_tiles.filter_family_clear"
    bl_label = "Clear Family"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        context.scene.mc_tiles_filter.family_hash = -1
        return {"FINISHED"}


class MCTILES_OT_filter_reset(bpy.types.Operator):
    bl_idname = "mc_tiles.filter_reset"
    bl_label = "Reset Filter"
    bl_description = "Show every tile again (keeps Filter Viewport on or off)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        f = context.scene.mc_tiles_filter
        for name in ("search", "subset", "corners", "variants", "problems_only", "family_hash", "hide_helpers"):
            f.property_unset(name)
        apply(context)
        return {"FINISHED"}


class MCTILES_OT_filter_refresh(bpy.types.Operator):
    bl_idname = "mc_tiles.filter_refresh"
    bl_label = "Refresh Filter"
    bl_description = "Apply the filter again (after adding or renaming tiles)"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        apply(context)
        return {"FINISHED"}


def draw_filter(layout, context, collection):
    """Filter box above the tile list."""
    f = context.scene.mc_tiles_filter
    is_cube = collection.mc_tiles.kind == "CUBE"
    box = layout.box()

    row = box.row(align=True)
    row.prop(f, "viewport", icon="HIDE_OFF" if f.viewport else "HIDE_ON", toggle=True)
    row.operator("mc_tiles.filter_refresh", text="", icon="FILE_REFRESH")
    row.operator("mc_tiles.filter_reset", text="", icon="LOOP_BACK")
    box.prop(f, "search", text="", icon="VIEWZOOM")
    placed = drawn_by(collection, f.search.strip())
    if placed and len(placed) == 1:
        h, turns, mirrored = placed[0]
        how = [f"turned {turns} x 90°"] if turns else []
        how += ["mirrored on export"] if mirrored else []
        box.label(text=f"{f.search.strip()} = {h:08b} {', '.join(how)}", icon="FILE_REFRESH")
    elif placed:
        parts = " + ".join(f"{h:08b}" + (f" ↻{t}" if t else "") + (" ⇋" if m else "") for h, t, m in placed)
        box.label(text=f"{f.search.strip()} = pieces {parts}", icon="MOD_BOOLEAN")
    elif placed is False:
        box.label(text=f"No tile draws {f.search.strip()} in this mode", icon="ERROR")

    col = box.column(align=True)
    if is_cube:
        col.prop(f, "subset", text="")
    col.prop(f, "corners", text="Corners (0 = any)")
    col.prop(f, "variants", text="")

    row = box.row(align=True)
    if is_cube:
        row.prop(f, "problems_only", toggle=True, icon="ERROR")
    row.prop(f, "hide_helpers", toggle=True, icon="OUTLINER_COLLECTION")

    if is_cube:
        row = box.row(align=True)
        if f.family_hash >= 0:
            row.label(text=f"Family of {f.family_hash:08b}", icon="MOD_MIRROR")
            row.operator("mc_tiles.filter_family_clear", text="", icon="X")
        else:
            row.operator("mc_tiles.filter_family", icon="MOD_MIRROR")

    shown = shown_tiles(collection, f)
    box.label(text=f"Showing {sum(shown.values())} / {len(shown)} meshes")


CLASSES = (MCTILES_OT_filter_family, MCTILES_OT_filter_family_clear, MCTILES_OT_filter_reset, MCTILES_OT_filter_refresh)


def register():
    bpy.utils.register_class(MCTilesFilterProps)
    bpy.types.Scene.mc_tiles_filter = bpy.props.PointerProperty(type=MCTilesFilterProps)


def unregister():
    del bpy.types.Scene.mc_tiles_filter
    bpy.utils.unregister_class(MCTilesFilterProps)
