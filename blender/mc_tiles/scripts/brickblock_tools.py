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

The hash is read from the name: "index" + level letter + variant letter + decimal number.
Its corner order is the source's, not MC Tile Tools' (no bits are changed here).
"""

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


def tagged_meshes(scene):
    return [o for o in scene.objects if o.type == "MESH" and "bb_hash" in o]


def shown_levels(scene):
    return {level for level in LEVELS if getattr(scene, "bb_show_" + level, True)}


def shown_meshes(scene):
    """Tagged meshes of the ticked levels (a level that isn't L, M or H always counts as shown)."""
    levels = shown_levels(scene)
    return [o for o in tagged_meshes(scene) if o["bb_level"] in levels or o["bb_level"] not in LEVELS]


def _on_levels(_self, context):
    scene = context.scene
    shown = set(shown_meshes(scene))
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
        column_of = {h: i for i, h in enumerate(hashes)}
        row_of = {g: i for i, g in enumerate(groups)}
        for obj in meshes:
            if HOME not in obj:
                obj[HOME] = list(obj.location)
            column = column_of[obj["bb_hash"]]
            row = row_of[obj["bb_level"] + obj["bb_variant"]]
            # The cell box is +-0.5 around the origin, so origins on x.5 put its corners on integers.
            obj.location = Vector((column * SPACING + 0.5, -row * SPACING + 0.5, 0.5))
        self.report({"INFO"}, f"{len(meshes)} meshes: {len(hashes)} hash columns x {len(groups)} group rows")
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
        text = f"{h}  {h:08b}  {obj['bb_level']}{obj['bb_variant']}"
        width, _ = blf.dimensions(0, text)
        blf.position(0, point.x - width / 2.0, point.y, 0.0)
        blf.draw(0, text)


def _draw_boxes():
    """Each visible mesh's cell box: grey wireframe, orange dots on solid corners, light blue on empty."""
    context = bpy.context
    if not getattr(context.scene, "bb_corner_boxes", False):
        return
    lines, solid, empty = [], [], []
    for obj in tagged_meshes(context.scene):
        if not obj.visible_get():
            continue
        # A 1 m cube centred on the mesh's origin: corners at origin + (+-0.5, +-0.5, +-0.5).
        origin = obj.matrix_world.translation
        points = [origin + Vector(c) for c in CORNERS]
        for a, b in EDGES:
            lines += (points[a], points[b])
        h = obj["bb_hash"]
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
        for level in LEVELS:
            row.prop(scene, "bb_show_" + level)
        row = layout.row(align=True)
        row.operator("brickblock.arrange", icon="GRID")
        row.operator("brickblock.restore", icon="LOOP_BACK")
        meshes = shown_meshes(scene)
        present = {o["bb_level"] + o["bb_variant"] for o in meshes}
        layout.label(text="Rows: " + " ".join(g for g in ROWS if g in present))
        if meshes:
            layout.label(text=f"{len(meshes)} meshes, {len({o['bb_hash'] for o in meshes})} hashes")


CLASSES = (BB_OT_remove_empties, BB_OT_arrange, BB_OT_restore, BB_PT_panel)


PROPS = ("bb_labels", "bb_corner_boxes") + tuple("bb_show_" + level for level in LEVELS)
HANDLES = "brickblock_draw_handles"  # kept in driver_namespace so a re-run can remove the old handlers


def register():
    for cls in CLASSES:
        bpy.utils.register_class(cls)
    scene = bpy.types.Scene
    scene.bb_labels = bpy.props.BoolProperty(name="Hash Labels", update=_redraw)
    scene.bb_corner_boxes = bpy.props.BoolProperty(
        name="Corner Boxes", description="Cell box wireframe, orange = solid corner, blue = empty", update=_redraw)
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
