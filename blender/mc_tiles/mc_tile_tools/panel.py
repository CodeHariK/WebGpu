"""The "MC Tiles" tab in the 3D view sidebar (N panel).

MC Tiles         pick / make a tile set, its settings and tile count
  Tiles          filter (list + viewport) and the tile list, click to select
  Arrange        lay the tiles out (corners / hash / family / heightmap), Restore, labels
  Playground     the selected tile among random neighbours from the set
  Check          seams, missing tiles, rotated copies, tiles the mode can't use
"""

import bpy

from . import cube
from .helpers import draw_move_helpers
from .arrange import draw_arrange
from .corners import draw_toggle as draw_corner_toggle
from .playground import draw_playground
from .view import draw_view_switch
from .names import untidy
from .ops import _target_collection
from .piece_lib import set_roots
from .piece_ops import draw_pieces
from .seams_ui import draw_seams
from .tilefilter import draw_filter
from .tiles import tile_roots, tile_variants, tiles_by_role, variant_clashes
from .variants import MAIN_ROLE, role_text

MAX_ROWS = 12  # longer lists end with "... N more"


def _active_set(context):
    return context.scene.mc_tiles_active


def draw_roles(layout, collection):
    """One line per role (height + group) when the set has more than the main one:
    "M a: 52 / 53 shapes, 81 tiles". Turns and mirrors count, as Godot and the export use them."""
    roles = tiles_by_role(collection, collection.mc_tiles.kind)
    if list(roles) in ([], [MAIN_ROLE]):
        return
    col = layout.column(align=True)
    for role, tiles in roles.items():
        count = sum(len(objs) for objs in tiles.values())
        if collection.mc_tiles.kind == "CUBE":
            report = cube.check(tiles.keys(), collection.mc_tiles.mode)
            covered = report.expected - len(report.missing)
            text = f"{role_text(role)}: {covered} / {report.expected} shapes, {count} tiles"
        else:
            text = f"{role_text(role)}: {count} tiles"
        col.label(text=text, icon="CHECKMARK" if role == MAIN_ROLE else "DOT")


def _not_allowed_reason(h):
    if h == cube.EMPTY:
        return "empty: never drawn"
    return "overhang"


class MCTILES_PT_main(bpy.types.Panel):
    bl_label = "MC Tiles"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MC Tiles"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        row = layout.row(align=True)
        row.prop(scene, "mc_tiles_active", text="")
        row.operator("mc_tiles.remove_set", text="", icon="TRASH")
        row = layout.row(align=True)
        row.operator("mc_tiles.new_set", icon="ADD")
        target = _target_collection(context)
        if target is not None:  # only a top-level collection that isn't a set yet
            row.operator("mc_tiles.make_set", text=f"Make '{target.name}' a Set", icon="OUTLINER_COLLECTION")
        draw_view_switch(layout, scene)
        draw_corner_toggle(layout, scene)

        collection = _active_set(context)
        if collection is None:
            layout.label(text="Select a collection, then Make Tile Set")
            return

        settings = collection.mc_tiles
        box = layout.box()
        box.prop(settings, "kind")
        if settings.kind == "CUBE":
            box.prop(settings, "mode")
            box.prop(settings, "build_from_pieces")
        row = box.row(align=True)
        row.prop(settings, "export_path")
        row.operator("mc_tiles.export_set", text="", icon="EXPORT")

        tiles = set_roots(collection)
        if settings.kind == "CUBE":
            # Count groups covered, not objects: a rotated copy or a full block doesn't count.
            report = cube.check(tiles.keys(), settings.mode, pieces=settings.build_from_pieces)
            covered = report.expected - len(report.missing)
            icon = "CHECKMARK" if covered == report.expected else "INFO"
            box.label(text=f"Shapes covered: {covered} / {report.expected}", icon=icon)
            if report.mirrored:
                box.label(text=f"{len(report.mirrored)} made by mirror on export", icon="MOD_MIRROR")
            if report.built:
                box.label(text=f"{len(report.built)} built from pieces", icon="MOD_BOOLEAN")
            draw_pieces(box, collection, tiles)
        else:
            box.label(text=f"Tiles: {len(tiles)} (prism rules not added yet)")
        extra = sum(len(objs) - 1 for objs in tile_variants(collection, settings.kind).values())
        if extra:
            box.label(text=f"Variants: {extra} extra meshes (_ma2, _ma3...)", icon="MOD_ARRAY")
        draw_roles(box, collection)
        draw_move_helpers(box, collection)


class MCTILES_PT_tiles(bpy.types.Panel):
    bl_label = "Tiles"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MC Tiles"
    bl_parent_id = "MCTILES_PT_main"

    @classmethod
    def poll(cls, context):
        return _active_set(context) is not None

    def draw(self, context):
        collection = _active_set(context)
        draw_filter(self.layout, context, collection)
        self.layout.template_list(
            "MCTILES_UL_tiles", "", collection, "all_objects",
            context.scene, "mc_tiles_index", rows=8,
        )


class MCTILES_PT_arrange(bpy.types.Panel):
    bl_label = "Arrange"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MC Tiles"
    bl_parent_id = "MCTILES_PT_main"
    bl_options = {"DEFAULT_CLOSED"}

    @classmethod
    def poll(cls, context):
        collection = _active_set(context)
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def draw(self, context):
        draw_arrange(self.layout, context)


class MCTILES_PT_playground(bpy.types.Panel):
    bl_label = "Playground"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MC Tiles"
    bl_parent_id = "MCTILES_PT_main"

    @classmethod
    def poll(cls, context):
        collection = _active_set(context)
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def draw(self, context):
        draw_playground(self.layout, context)


class MCTILES_PT_check(bpy.types.Panel):
    bl_label = "Check"
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "MC Tiles"
    bl_parent_id = "MCTILES_PT_main"

    @classmethod
    def poll(cls, context):
        collection = _active_set(context)
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def draw(self, context):
        layout = self.layout
        collection = _active_set(context)
        mode = collection.mc_tiles.mode
        tiles = set_roots(collection)
        report = cube.check(tiles.keys(), mode, pieces=collection.mc_tiles.build_from_pieces)
        clashes = variant_clashes(collection)

        draw_seams(layout, context, collection)
        messy = untidy(collection)
        problems = report.missing or report.duplicates or report.not_allowed or clashes or report.partners or messy
        if not problems and not report.built:
            layout.label(text="All tiles present, nothing extra", icon="CHECKMARK")
            return

        self._section(
            layout, "Missing (model one of each)", "ERROR",
            [(h, None) for h in report.missing], tiles,
        )
        self._section(
            layout, "Built from pieces (model one to replace it)", "MOD_BOOLEAN",
            [(h, " + ".join(f"{p:08b}" for p in found)) for h, found in report.built], tiles,
        )
        self._section(
            layout, "Rotated copies (Godot makes these)", "DUPLICATE",
            [(h, f"copy of {kept:08b}") for h, kept in report.duplicates], tiles,
        )
        self._section(
            layout, "Not allowed in this mode", "CANCEL",
            [(h, _not_allowed_reason(h)) for h in report.not_allowed], tiles,
        )
        self._section(
            layout, "Mirror partners: export can make these, delete one?", "MOD_MIRROR",
            [(h, f"flip of {src:08b}") for h, src in report.partners], tiles,
        )
        if messy:
            box = layout.box()
            box.label(text=f"Names not in hash / hash_<tag> form: {len(messy)}", icon="SORTALPHA")
            col = box.column(align=True)
            for obj, wanted in messy[:MAX_ROWS]:
                col.label(text=f"{obj.name}  ->  {wanted}")
            if len(messy) > MAX_ROWS:
                col.label(text=f"... {len(messy) - MAX_ROWS} more")
            box.operator("mc_tiles.tidy_names", icon="SORTALPHA")
        self._section(
            layout, "Same hash and tag twice (rename one _ma2...)", "ERROR",
            [(h, f"{tag}: {names}") for h, tag, names in clashes], tiles,
        )

    def _section(self, layout, title, icon, rows, tiles):
        """A box listing hashes; tiles that exist get a button that selects them."""
        if not rows:
            return
        box = layout.box()
        box.label(text=f"{title}: {len(rows)}", icon=icon)
        col = box.column(align=True)
        for h, note in rows[:MAX_ROWS]:
            row = col.row(align=True)
            obj = tiles.get(h)
            if obj is not None:
                op = row.operator("mc_tiles.select_tile", text=f"{h:08b}", icon="RESTRICT_SELECT_OFF")
                op.name = obj.name
            else:
                row.label(text=f"{h:08b}")
            row.label(text=note or cube.corner_text(h))
        if len(rows) > MAX_ROWS:
            col.label(text=f"... {len(rows) - MAX_ROWS} more")


CLASSES = (MCTILES_PT_main, MCTILES_PT_tiles, MCTILES_PT_arrange, MCTILES_PT_playground, MCTILES_PT_check)
