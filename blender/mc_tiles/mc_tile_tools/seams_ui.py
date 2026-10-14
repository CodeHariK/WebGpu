"""Seam check in Blender: read the tiles, run seams.check, list clashing pairs, show one in the playground.

Check Seams (Check panel) reads every tile and variant as Export Set would (evaluated mesh, so
modifiers count; rotation / scale applied; children joined), caches each tile's face hashes (visible
points only: see seams.py) and
lists every pair of tiles whose shared wall isn't solid in the same places. Click a pair: the
playground is rebuilt with just those two tiles side by side (editable copies sharing the real
meshes) and the wall samples that differ are drawn:
    red  solid only on A        blue  solid only on B
Edit, then Check Seams again. The pin button lists just the pairs with the selected tile; the tile
is remembered (Scene.mc_tiles_seam_tile), so clicking a pair, which selects the playground copies,
doesn't change the list. The conflict button in the tile list pins that tile.

Face Hashes: the 6 wall ids of the selected tiles (and playground copies, turned / mirrored as
placed), drawn on each face and listed in the panel. Left / front walls are shown as their right /
back neighbour sees them, so two faces that meet show the SAME id when they match.
"- " = nothing solid on that wall, "full" = all solid.
"""

import blf
import bpy
import gpu
from bpy_extras.view3d_utils import location_3d_to_region_2d
from gpu_extras.batch import batch_for_shader
from mathutils import Vector

from . import cube, seams
from .tilefilter import KEEP_VISIBLE, apply, keep_visible, shown_tiles
from .playground import MIRRORED, SOURCE, TURNS, build_pair, tile_lookup
from .tiles import tile_hash, tile_variants

A_COLOUR = (1.0, 0.25, 0.2, 1.0)
B_COLOUR = (0.3, 0.6, 1.0, 1.0)
POINT_SIZE = 5.0
LABEL_DEPTH = 0.32  # face-hash labels sit this far from the cell centre (walls are at 0.5)
PAGE = 10

_results = {}  # collection name -> seams.Report
_shown = []  # [(world position, colour)] points of the pair in the playground
_handle = None
_text_handle = None
_faces = {}  # tile name -> seams.TileFaces, cleared whenever anything changes


def tile_geometry(root, depsgraph):
    """seams.TileGeometry of a tile and its child parts, cell-centred, as Export Set writes it."""
    origin = root.matrix_world.translation
    points, polygons = [], []
    for obj in [root] + list(root.children_recursive):
        if obj.type not in {"MESH", "CURVE", "SURFACE", "META", "FONT"}:
            continue
        evaluated = obj.evaluated_get(depsgraph)
        try:
            mesh = evaluated.to_mesh()
        except RuntimeError:
            continue
        if mesh is None:
            continue
        matrix = evaluated.matrix_world
        base = len(points)
        points += [tuple(matrix @ v.co - origin) for v in mesh.vertices]
        polygons += [tuple(base + i for i in p.vertices) for p in mesh.polygons]
        evaluated.to_mesh_clear()
    return seams.TileGeometry(points, polygons)


def run(context, collection):
    """Check the set and keep the report for the panel."""
    depsgraph = context.evaluated_depsgraph_get()
    mode = collection.mc_tiles.mode
    variants = {
        h: [(obj.name, tile_geometry(obj, depsgraph)) for obj in objs]
        for h, objs in tile_variants(collection, "CUBE").items()
    }
    geometries = {name: g for objs in variants.values() for name, g in objs}
    allowed = {h for group in cube.orbits(mode) for h in group}
    instances = seams.library(variants, tile_lookup(variants.keys(), mode), allowed)
    report = seams.check(instances, geometries)
    report.per_hash = {}  # tile hash -> clashing pairs it is in (the tile list's conflict column)
    for pair in report.pairs:
        for h in {tile_hash(n, "CUBE") for n in (pair.a.name, pair.b.name) if n}:
            report.per_hash[h] = report.per_hash.get(h, 0) + 1
    _results[collection.name] = report
    context.scene.mc_tiles_seam_page = 0
    _shown.clear()
    return report


def result(collection):
    return _results.get(collection.name) if collection is not None else None


def _selected_tile_name(context):
    """Name of the selected tile (a playground copy counts as its tile), or ""."""
    obj = context.active_object
    while obj is not None and obj.parent is not None:
        obj = obj.parent
    if obj is None:
        return ""
    name = obj.get("mc_pg_tile", obj.name)
    return name if tile_hash(name, "CUBE") is not None else ""


def _listed(context, report):
    """Pairs shown in the panel.

    Tile filter (search, shapes, corners...): only pairs where A or B comes from a tile the tile list
    shows; a turned / mirrored copy counts as its tile, so searching a hash also finds its turns and
    mirrors. Pinned tile: only pairs with that tile. Both apply together.
    """
    scene = context.scene
    pairs = report.pairs
    collection = scene.mc_tiles_active
    if collection is not None:
        shown = shown_tiles(collection, scene.mc_tiles_filter)
        if not all(shown.values()):  # the filter narrows the tiles: narrow the pairs too
            names = {obj.name for obj, ok in shown.items() if ok}
            pairs = [p for p in pairs if p.a.name in names or p.b.name in names]
    pinned = scene.mc_tiles_seam_tile
    if pinned:
        h = tile_hash(pinned, "CUBE")  # any variant of the tile counts
        pairs = [p for p in pairs if h is not None and h in (tile_hash(p.a.name, "CUBE"), tile_hash(p.b.name, "CUBE"))]
    return pairs


def conflicts(collection, h):
    """Clashing pairs a tile hash is in, or None before Check Seams."""
    report = result(collection)
    return None if report is None else getattr(report, "per_hash", {}).get(h, 0)


def _label(inst):
    """The hash drawn in that cell, and for a turned / mirrored copy the tile it comes from:
    "00010001", "01000100 (00010001 ↻2)", "00100110 (00011001 ⇋)"."""
    if not inst.name:
        return "(empty)"
    if not inst.turns and not inst.mirrored:
        return inst.name
    how = (f" ↻{inst.turns}" if inst.turns else "") + (" ⇋" if inst.mirrored else "")
    return f"{inst.h:08b} ({inst.name}{how})"


def _wall_point(u, v, axis):
    """A wall position (seen from the low cell, side +1) back to 3D in the low cell."""
    if axis == 0:
        return Vector((0.5, u, v))
    if axis == 1:
        return Vector((-u, 0.5, v))
    return Vector((u, v, 0.5))


def show_pair(context, pair):
    """Rebuild the playground with the pair side by side and mark the shared wall's points."""
    collection = context.scene.mc_tiles_active
    a = (pair.a.name, pair.a.h, pair.a.turns, pair.a.mirrored)
    b = (pair.b.name, pair.b.h, pair.b.turns, pair.b.mirrored)
    near = bpy.data.objects.get(pair.a.name) or bpy.data.objects.get(pair.b.name)
    origin, _ = build_pair(context.scene, collection, a, b, pair.axis, near)
    keep_visible(context, (pair.a.name, pair.b.name))  # the real tiles too, even if the filter hid them
    _shown.clear()
    for cover, other, colour in ((pair.cover_a, pair.cover_b, A_COLOUR), (pair.cover_b, pair.cover_a, B_COLOUR)):
        for vi, ui in zip(*(cover & ~other).nonzero()):
            point = _wall_point(float(seams.CENTRES[ui]), float(seams.CENTRES[vi]), pair.axis)
            _shown.append((origin + point, colour))
    _redraw(context)


class MCTILES_OT_check_seams(bpy.types.Operator):
    """Sample which part of every tile wall is solid and list tile pairs whose shared walls differ"""

    bl_idname = "mc_tiles.check_seams"
    bl_label = "Check Seams"
    bl_options = {"REGISTER"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def execute(self, context):
        report = run(context, context.scene.mc_tiles_active)
        n = len(report.pairs)
        self.report({"WARNING"} if n else {"INFO"}, f"Seams: {n} clashing pairs ({report.checked} tiles checked)")
        _redraw(context)
        return {"FINISHED"}


class MCTILES_OT_show_seam_pair(bpy.types.Operator):
    """Put this pair side by side in the playground: wall samples solid only on A in red, only on B in blue"""

    bl_idname = "mc_tiles.show_seam_pair"
    bl_label = "Show Pair"
    bl_options = {"REGISTER", "UNDO"}

    index: bpy.props.IntProperty()

    def execute(self, context):
        report = result(context.scene.mc_tiles_active)
        if report is None or not 0 <= self.index < len(report.pairs):
            self.report({"WARNING"}, "Run Check Seams first")
            return {"CANCELLED"}
        show_pair(context, report.pairs[self.index])
        try:
            bpy.ops.view3d.view_selected()
        except RuntimeError:
            pass  # not called from a 3D view
        return {"FINISHED"}


class MCTILES_OT_filter_conflicts(bpy.types.Operator):
    """Select this tile and list only its seam conflicts (Check panel)"""

    bl_idname = "mc_tiles.filter_conflicts"
    bl_label = "Show Conflicts"
    bl_options = {"REGISTER", "UNDO"}

    name: bpy.props.StringProperty()

    def execute(self, context):
        obj = bpy.data.objects.get(self.name)
        if obj is None or obj.name not in context.view_layer.objects:
            return {"CANCELLED"}
        for other in context.selected_objects:
            other.select_set(False)
        obj.select_set(True)
        context.view_layer.objects.active = obj
        context.scene.mc_tiles_seam_tile = obj.name
        context.scene.mc_tiles_seam_page = 0
        return {"FINISHED"}


class MCTILES_OT_seam_pin(bpy.types.Operator):
    """List only the seam conflicts of the selected tile (stays until cleared, whatever you select next)"""

    bl_idname = "mc_tiles.seam_pin"
    bl_label = "Only This Tile"
    bl_options = {"REGISTER", "UNDO"}

    clear: bpy.props.BoolProperty()

    def execute(self, context):
        name = "" if self.clear else _selected_tile_name(context)
        if not name and not self.clear:
            self.report({"WARNING"}, "Select a tile first")
            return {"CANCELLED"}
        context.scene.mc_tiles_seam_tile = name
        context.scene.mc_tiles_seam_page = 0
        return {"FINISHED"}


class MCTILES_OT_copy_hash(bpy.types.Operator):
    """Copy this hash (the shape in that cell) to the clipboard; paste it in the tile search to find its tile"""

    bl_idname = "mc_tiles.copy_hash"
    bl_label = "Copy Hash"
    bl_options = {"INTERNAL"}

    text: bpy.props.StringProperty()

    def execute(self, context):
        context.window_manager.clipboard = self.text
        self.report({"INFO"}, f"Copied {self.text}")
        return {"FINISHED"}


class MCTILES_OT_seam_page(bpy.types.Operator):
    bl_idname = "mc_tiles.seam_page"
    bl_label = "Seam Page"
    bl_options = {"INTERNAL"}

    step: bpy.props.IntProperty()

    def execute(self, context):
        context.scene.mc_tiles_seam_page = max(0, context.scene.mc_tiles_seam_page + self.step)
        return {"FINISHED"}


def draw_seams(layout, context, collection):
    """Seams box in the Check panel."""
    scene = context.scene
    box = layout.box()
    row = box.row(align=True)
    row.operator("mc_tiles.check_seams", icon="SNAP_EDGE")
    if scene.mc_tiles_seam_tile:
        row.operator("mc_tiles.seam_pin", text=f"Only {scene.mc_tiles_seam_tile}", icon="PINNED")
        row.operator("mc_tiles.seam_pin", text="", icon="X").clear = True
    else:
        row.operator("mc_tiles.seam_pin", text="", icon="UNPINNED").clear = False
    box.prop(scene, "mc_tiles_face_hashes", toggle=True, icon="LINENUMBERS_ON")
    if scene.mc_tiles_face_hashes:
        draw_face_hashes(box, context)
    report = result(collection)
    if report is None:
        box.label(text="Not checked yet", icon="INFO")
        return
    for name, n in report.outside:
        box.label(text=f"{name}: {n} vertices outside the cell", icon="ERROR")
    for name, n in report.near:
        box.label(text=f"{name}: {n} vertices near a wall, not on it", icon="INFO")
    if not report.pairs:
        box.label(text=f"All faces match ({report.checked} tiles checked)", icon="CHECKMARK")
        return

    listed = _listed(context, report)
    pages = max(1, (len(listed) + PAGE - 1) // PAGE)
    page = min(scene.mc_tiles_seam_page, pages - 1)
    head = box.row(align=True)
    total = f" of {len(report.pairs)}" if len(listed) != len(report.pairs) else ""
    head.label(text=f"Clashing pairs: {len(listed)}{total}", icon="ERROR")
    sub = head.row(align=True)
    sub.enabled = page > 0
    sub.operator("mc_tiles.seam_page", text="", icon="TRIA_LEFT").step = -1
    head.label(text=f"{page + 1}/{pages}")
    sub = head.row(align=True)
    sub.enabled = page < pages - 1
    sub.operator("mc_tiles.seam_page", text="", icon="TRIA_RIGHT").step = 1

    col = box.column(align=True)
    index_of = {id(p): i for i, p in enumerate(report.pairs)}
    for pair in listed[page * PAGE:(page + 1) * PAGE]:
        row = col.row(align=True)
        op = row.operator("mc_tiles.show_seam_pair", text=f"{_label(pair.a)}  |  {_label(pair.b)}", icon="VIEWZOOM")
        op.index = index_of[id(pair)]
        for inst in (pair.a, pair.b):
            copy = row.row(align=True)
            copy.enabled = bool(inst.name)
            copy.operator("mc_tiles.copy_hash", text="", icon="COPYDOWN").text = f"{inst.h:08b}"
        row.label(text=f"{seams.AXIS_NAMES[pair.axis]}  {seams.pattern_text(pair.pattern)}  {pair.difference:.1%}")
    box.label(text="A | B: A left / front / below. ↻ turns, ⇋ mirrored. Copy: A's / B's hash")


# ---------------------------------------------------------------- overlay


# ---------------------------------------------------------------- face hashes


def _tile_faces(obj):
    if obj.name not in _faces:
        depsgraph = bpy.context.evaluated_depsgraph_get()
        _faces[obj.name] = seams.TileFaces(tile_geometry(obj, depsgraph))
    return _faces[obj.name]


def placed_instance(obj):
    """seams.Instance for a real tile or a playground copy (root), or None."""
    while obj is not None and obj.parent is not None:
        obj = obj.parent
    if obj is None:
        return None
    turns, mirrored = 0, False
    if SOURCE in obj:
        turns, mirrored = int(obj.get(TURNS, 0)), bool(obj.get(MIRRORED, False))
        source = bpy.data.objects.get(obj[SOURCE])
    else:
        source = obj
    if source is None or tile_hash(source.name, "CUBE") is None:
        return None
    return seams.Instance(tile_hash(source.name, "CUBE"), source.name, _tile_faces(source), turns, mirrored)


def face_ids(obj):
    """{world face: (id, solid fraction)} for a placed tile, or {}."""
    inst = placed_instance(obj)
    if inst is None:
        return {}
    out = {}
    for face in seams.FACES:
        cover = seams.world_cover(inst, face)
        out[face] = (seams.short(cover), float(cover.mean()))
    return out


def draw_face_hashes(layout, context):
    """Rows for the active tile in the Seams box."""
    obj = context.active_object
    ids = face_ids(obj) if obj is not None else {}
    if not ids:
        layout.label(text="Select a tile to see its face hashes", icon="INFO")
        return
    col = layout.column(align=True)
    for face in seams.FACES:
        text, solid = ids[face]
        row = col.row(align=True)
        row.label(text=seams.FACE_NAMES[face])
        row.label(text=text)
        row.label(text=f"{solid:.0%} solid")


def _draw_text():
    context = bpy.context
    if not getattr(context.scene, "mc_tiles_face_hashes", False):
        return
    region, rv3d = context.region, context.region_data
    if region is None or rv3d is None:
        return
    blf.size(0, 11)
    blf.color(0, 1.0, 0.85, 0.3, 1.0)
    for obj in context.selected_objects:
        if obj.parent is not None or not obj.visible_get():
            continue
        origin = obj.matrix_world.translation
        for (axis, side), (text, _) in face_ids(obj).items():
            offset = Vector((0.0, 0.0, 0.0))
            offset[axis] = side * LABEL_DEPTH  # a little inside the wall, so two touching faces don't overlap
            point = location_3d_to_region_2d(region, rv3d, origin + offset)
            if point is None:
                continue
            label = f"{seams.FACE_NAMES[(axis, side)]} {text}"
            width, _ = blf.dimensions(0, label)
            blf.position(0, point.x - width / 2.0, point.y, 0.0)
            blf.draw(0, label)


def _forget_faces(*_args):
    _faces.clear()


# ---------------------------------------------------------------- overlay


def _draw():
    if not _shown:
        return
    gpu.state.blend_set("ALPHA")
    gpu.state.depth_test_set("NONE")
    shader = gpu.shader.from_builtin("POINT_UNIFORM_COLOR")
    gpu.state.point_size_set(POINT_SIZE)
    for colour in (A_COLOUR, B_COLOUR):
        points = [p for p, c in _shown if c == colour]
        if points:
            shader.uniform_float("color", colour)
            batch_for_shader(shader, "POINTS", {"pos": points}).draw(shader)
    gpu.state.point_size_set(1.0)
    gpu.state.blend_set("NONE")


def clear_shown():
    _shown.clear()
    if KEEP_VISIBLE:
        KEEP_VISIBLE.clear()
        apply(bpy.context)  # the filter may hide the pair's real tiles again


def _redraw(context, *_args):
    screen = getattr(context, "screen", None)
    for area in screen.areas if screen else ():
        if area.type == "VIEW_3D":
            area.tag_redraw()


CLASSES = (
    MCTILES_OT_check_seams, MCTILES_OT_show_seam_pair, MCTILES_OT_filter_conflicts, MCTILES_OT_seam_pin,
    MCTILES_OT_copy_hash, MCTILES_OT_seam_page,
)


def register():
    global _handle, _text_handle
    bpy.types.Scene.mc_tiles_seam_page = bpy.props.IntProperty(default=0, min=0)
    bpy.types.Scene.mc_tiles_seam_tile = bpy.props.StringProperty(
        name="Conflicts Of", description="List only the pairs that include this tile (empty = all)",
    )
    bpy.types.Scene.mc_tiles_face_hashes = bpy.props.BoolProperty(
        name="Face Hashes",
        description="Show the 6 wall ids of the selected tiles; faces that meet show the same id when they match",
        update=lambda self, context: _redraw(context),
    )
    _handle = bpy.types.SpaceView3D.draw_handler_add(_draw, (), "WINDOW", "POST_VIEW")
    _text_handle = bpy.types.SpaceView3D.draw_handler_add(_draw_text, (), "WINDOW", "POST_PIXEL")
    bpy.app.handlers.depsgraph_update_post.append(_forget_faces)


def unregister():
    global _handle, _text_handle
    if _forget_faces in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.remove(_forget_faces)
    if _text_handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_text_handle, "WINDOW")
        _text_handle = None
    if _handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_handle, "WINDOW")
        _handle = None
    del bpy.types.Scene.mc_tiles_face_hashes
    del bpy.types.Scene.mc_tiles_seam_tile
    del bpy.types.Scene.mc_tiles_seam_page
