"""The "MC Tiles" tab in the 3D view sidebar (N panel).

MC Tiles         pick / make a tile set, its settings and tile count
  Tiles          filter (list + viewport) and the tile list, click to select
  Arrange        lay the tiles out (corners / hash / family / heightmap), Restore, labels
  Playground     the selected tile among random neighbours from the set
  Check          missing tiles, rotated copies, tiles the mode can't use
"""

import bpy

from . import cube
from .helpers import draw_move_helpers
from .arrange import draw_arrange
from .corners import draw_toggle as draw_corner_toggle
from .playground import draw_playground
from .view import draw_view_switch
from .names import untidy
from .tilefilter import draw_filter
from .tiles import tile_roots, tile_variants, variant_clashes

MAX_ROWS = 12  # longer lists end with "... N more"


def _active_set(context):
    return context.scene.mc_tiles_active


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
        layout.operator("mc_tiles.make_set", icon="ADD")
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
        row = box.row(align=True)
        row.prop(settings, "export_path")
        row.operator("mc_tiles.export_set", text="", icon="EXPORT")

        tiles = tile_roots(collection, settings.kind)
        if settings.kind == "CUBE":
            # Count groups covered, not objects: a rotated copy or a full block doesn't count.
            report = cube.check(tiles.keys(), settings.mode)
            covered = report.expected - len(report.missing)
            icon = "CHECKMARK" if covered == report.expected else "INFO"
            box.label(text=f"Tiles: {covered} / {report.expected}", icon=icon)
            if report.mirrored:
                box.label(text=f"{len(report.mirrored)} made by mirror on export", icon="MOD_MIRROR")
        else:
            box.label(text=f"Tiles: {len(tiles)} (prism rules not added yet)")
        extra = sum(len(objs) - 1 for objs in tile_variants(collection, settings.kind).values())
        if extra:
            box.label(text=f"Variants: {extra} extra meshes (_v2, _v3...)", icon="MOD_ARRAY")
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
        tiles = tile_roots(collection, "CUBE")
        report = cube.check(tiles.keys(), mode)
        clashes = variant_clashes(collection)

        messy = untidy(collection)
        problems = report.missing or report.duplicates or report.not_allowed or clashes or report.partners or messy
        if not problems:
            layout.label(text="All tiles present, nothing extra", icon="CHECKMARK")
            return

        self._section(
            layout, "Missing (model one of each)", "ERROR",
            [(h, None) for h in report.missing], tiles,
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
            box.label(text=f"Names not in hash / hash_vN form: {len(messy)}", icon="SORTALPHA")
            col = box.column(align=True)
            for obj, wanted in messy[:MAX_ROWS]:
                col.label(text=f"{obj.name}  ->  {wanted}")
            if len(messy) > MAX_ROWS:
                col.label(text=f"... {len(messy) - MAX_ROWS} more")
            box.operator("mc_tiles.tidy_names", icon="SORTALPHA")
        self._section(
            layout, "Same hash and variant twice (rename one _v2...)", "ERROR",
            [(h, f"variant {v}: {names}") for h, v, names in clashes], tiles,
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
