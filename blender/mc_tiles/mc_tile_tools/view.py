"""Debug material for helper meshes; real tiles keep their real materials.

A helper is anything in a tile set that isn't a tile: non-tile names, and everything in the
Helpers / Playground child collections (with their child parts). Playground neighbours count
as helpers too; the playground's centre tile keeps its real material.

The debug material is put on the helper's object-level material slots (slot link = Object), so
the mesh data keeps its own materials. Turning it off sets the slots back to Data and removes any
slot that was only added for this. Works in Solid, Material Preview and Rendered shading.
"""

import bpy

from .tiles import ignored_objects, tile_hash

MATERIAL_NAME = "MC_Helper_Debug"
COLOR = (0.55, 0.75, 0.95, 1.0)  # light blue, solid
MARK = "mc_helper_debug"  # object property: number of slots added (present = debug applied)


def _debug_material():
    """Plain opaque light-blue material (settings re-applied, so an older saved copy is updated)."""
    mat = bpy.data.materials.get(MATERIAL_NAME) or bpy.data.materials.new(MATERIAL_NAME)
    mat.diffuse_color = COLOR  # Solid shading
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf is not None:
        bsdf.inputs["Base Color"].default_value = COLOR
        bsdf.inputs["Alpha"].default_value = 1.0
    if hasattr(mat, "surface_render_method"):
        mat.surface_render_method = "DITHERED"
    return mat


def _tile_sets(scene):
    """Tile-set collections linked into this scene."""
    in_scene = set(scene.collection.children_recursive)
    return [c for c in bpy.data.collections if c.mc_tiles.is_tile_set and c in in_scene]


PLAYGROUND_ROLE = "mc_pg_role"  # set by playground.py on tile copies


def helper_objects(scene):
    """Every helper object (with child parts) in the scene's tile sets, plus playground neighbours."""
    helpers = {o for o in scene.objects if o.get(PLAYGROUND_ROLE) == "neighbour"}
    for collection in _tile_sets(scene):
        kind = collection.mc_tiles.kind
        ignored = ignored_objects(collection)
        for obj in collection.all_objects:
            if obj.parent is not None:
                continue
            if obj in ignored or tile_hash(obj.name, kind) is None:
                helpers.add(obj)
                helpers.update(obj.children_recursive)
    return {o for o in helpers if hasattr(o.data, "materials")}


def _apply(obj, mat):
    if MARK in obj:
        return
    added = 0
    if len(obj.material_slots) == 0:
        obj.data.materials.append(None)
        added = 1
    for slot in obj.material_slots:
        slot.link = "OBJECT"
        slot.material = mat
    obj[MARK] = added


def remove_debug(obj):
    if MARK not in obj:
        return
    for slot in obj.material_slots:
        slot.material = None
        slot.link = "DATA"
    for _ in range(int(obj[MARK])):
        obj.data.materials.pop()
    del obj[MARK]


def refresh(scene):
    """Apply the debug material to current helpers; take it off objects that no longer are."""
    helpers = helper_objects(scene) if scene.mc_tiles_helper_debug else set()
    for obj in bpy.data.objects:
        if MARK in obj and obj not in helpers:
            remove_debug(obj)
    if helpers:
        mat = _debug_material()
        for obj in helpers:
            _apply(obj, mat)


def _on_toggle(scene, _context):
    refresh(scene)


class MCTILES_OT_refresh_helpers(bpy.types.Operator):
    """Re-apply the helper debug material after adding, renaming or moving objects"""

    bl_idname = "mc_tiles.refresh_helpers"
    bl_label = "Refresh Helpers"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        refresh(context.scene)
        return {"FINISHED"}


def draw_view_switch(layout, scene):
    row = layout.row(align=True)
    row.prop(scene, "mc_tiles_helper_debug", toggle=True, icon="MATERIAL")
    row.operator("mc_tiles.refresh_helpers", text="", icon="FILE_REFRESH")


def register():
    bpy.utils.register_class(MCTILES_OT_refresh_helpers)
    bpy.types.Scene.mc_tiles_helper_debug = bpy.props.BoolProperty(
        name="Helper Debug Material",
        description="Show helper meshes with a plain light-blue debug material; tiles keep theirs",
        default=False,
        update=_on_toggle,
    )


def unregister():
    del bpy.types.Scene.mc_tiles_helper_debug
    bpy.utils.unregister_class(MCTILES_OT_refresh_helpers)
