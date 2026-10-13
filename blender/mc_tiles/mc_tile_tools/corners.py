"""Debug corner boxes: a wireframe 1x1x1 cell box around each tile with its 8 corners drawn as dots.

Solid corners (bit set in the hash) are big orange dots, empty corners smaller light-blue dots, so you can
check a tile's shape against its name at a glance. Drawn in the 3D view only (gpu overlay, no
objects, never exported). Toggle: Scene.mc_tiles_corner_boxes ("Corner Boxes" in the main panel);
"Selected Only" limits it to selected objects.

Which objects get a box, and which hash it shows:
    tiles of the active set        their own name ("00010001", "00010001_v2")
    playground copies              the cell hash they fill ("pg_00100010_...": already turned)
    Arrange slots                  the hash in their label ("00100110 mirror", "... missing")
The box is axis aligned at the object's origin (the cell centre), because a hash always describes
corners in world axes: a playground copy is a turned tile, but its cell hash is already turned too.

Corner layout in Blender axes (Z up, -Y front), the same as Godot's C0..C7 (see playground.py):
    C0 (0,0,0)  C1 (1,0,0)  C2 (1,1,0)  C3 (0,1,0)  C4..C7 the same one level up
"""

import bpy
import gpu
from gpu_extras.batch import batch_for_shader
from mathutils import Vector

from .tiles import tile_hash, tile_variants

CORNER_OFFSETS = (
    (0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
    (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1),
)
EDGES = ((0, 1), (1, 2), (2, 3), (3, 0), (4, 5), (5, 6), (6, 7), (7, 4), (0, 4), (1, 5), (2, 6), (3, 7))
BOX_COLOUR = (0.85, 0.85, 0.85, 0.6)
SOLID_COLOUR = (1.0, 0.55, 0.1, 1.0)
EMPTY_COLOUR = (0.65, 0.75, 0.95, 0.95)
SOLID_SIZE = 10.0
EMPTY_SIZE = 7.0
ARRANGE_LABEL = "mc_ar_label"  # set by arrange.py
PLAYGROUND_PREFIX = "pg_"

_handle = None


def corner_points(centre, h):
    """[(world position, solid)] for the 8 corners of the cell centred at `centre`."""
    out = []
    for bit, (dx, dy, dz) in enumerate(CORNER_OFFSETS):
        offset = Vector((dx - 0.5, dy - 0.5, dz - 0.5))
        out.append((centre + offset, bool((h >> bit) & 1)))
    return out


def _playground_hash(obj):
    """Cell hash of a playground root copy ("pg_00100010_..."), or None."""
    if obj.parent is not None or not obj.name.startswith(PLAYGROUND_PREFIX):
        return None
    digits = obj.name[len(PLAYGROUND_PREFIX):].split("_")[0]
    if len(digits) == 8 and set(digits) <= {"0", "1"}:
        return int(digits, 2)
    if digits == "missing":  # "pg_missing_00100010"
        tail = obj.name.split("_")[-1]
        return int(tail, 2) if len(tail) == 8 and set(tail) <= {"0", "1"} else None
    return None


def boxes(context):
    """[(centre, hash)] for every visible object that gets a box."""
    scene = context.scene
    selected_only = scene.mc_tiles_corner_boxes_selected
    seen = set()
    out = []

    def add(obj, h):
        if h is None or obj in seen or not obj.visible_get():
            return
        if selected_only and not obj.select_get():
            return
        seen.add(obj)
        out.append((obj.matrix_world.translation.copy(), h))

    collection = scene.mc_tiles_active
    if collection is not None and collection.mc_tiles.is_tile_set:
        kind = collection.mc_tiles.kind
        if kind == "CUBE":
            for objs in tile_variants(collection, kind).values():
                for obj in objs:
                    add(obj, tile_hash(obj.name, kind))
    for obj in scene.objects:
        label = obj.get(ARRANGE_LABEL)
        if label:
            add(obj, int(label.split()[0], 2))
        else:
            add(obj, _playground_hash(obj))
    return out


def _draw():
    context = bpy.context
    scene = context.scene
    if not getattr(scene, "mc_tiles_corner_boxes", False):
        return
    found = boxes(context)
    if not found:
        return

    lines, solid, empty = [], [], []
    for centre, h in found:
        points = corner_points(centre, h)
        for a, b in EDGES:
            lines += [points[a][0], points[b][0]]
        for position, is_solid in points:
            (solid if is_solid else empty).append(position)

    gpu.state.blend_set("ALPHA")
    gpu.state.depth_test_set("NONE")  # draw through the tile so hidden corners show too

    line_shader = gpu.shader.from_builtin("POLYLINE_UNIFORM_COLOR")
    line_shader.uniform_float("viewportSize", gpu.state.viewport_get()[2:])
    line_shader.uniform_float("lineWidth", 1.0)
    line_shader.uniform_float("color", BOX_COLOUR)
    batch_for_shader(line_shader, "LINES", {"pos": lines}).draw(line_shader)

    point_shader = gpu.shader.from_builtin("POINT_UNIFORM_COLOR")
    for positions, colour, size in ((empty, EMPTY_COLOUR, EMPTY_SIZE), (solid, SOLID_COLOUR, SOLID_SIZE)):
        if not positions:
            continue
        gpu.state.point_size_set(size)
        point_shader.uniform_float("color", colour)
        batch_for_shader(point_shader, "POINTS", {"pos": positions}).draw(point_shader)

    gpu.state.point_size_set(1.0)
    gpu.state.blend_set("NONE")


def _redraw(_self, context):
    for area in context.screen.areas if context.screen else ():
        if area.type == "VIEW_3D":
            area.tag_redraw()


def draw_toggle(layout, scene):
    row = layout.row(align=True)
    row.prop(scene, "mc_tiles_corner_boxes", toggle=True, icon="MESH_CUBE")
    row.prop(scene, "mc_tiles_corner_boxes_selected", text="", icon="RESTRICT_SELECT_OFF", toggle=True)


def register():
    global _handle
    bpy.types.Scene.mc_tiles_corner_boxes = bpy.props.BoolProperty(
        name="Corner Boxes",
        description="Draw a wireframe cell box around each tile: solid corners orange, empty corners light blue",
        update=_redraw,
    )
    bpy.types.Scene.mc_tiles_corner_boxes_selected = bpy.props.BoolProperty(
        name="Selected Only", description="Corner boxes only for selected objects", update=_redraw,
    )
    _handle = bpy.types.SpaceView3D.draw_handler_add(_draw, (), "WINDOW", "POST_VIEW")


def unregister():
    global _handle
    if _handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_handle, "WINDOW")
        _handle = None
    del bpy.types.Scene.mc_tiles_corner_boxes_selected
    del bpy.types.Scene.mc_tiles_corner_boxes
