"""Tile names: in Blender the hash and its tag (variants.py): "00010001_ma1", "00010001_ma2",
"00010001_la1", "00010001_hb3" (a bare "00010001" is still read as ma1; Rename to Hash Names adds
the tag). In the exported .glb the set name follows: "00010001_ma1_circus", "00010001_hb3_circus".

Blender object names are unique per .blend, so only one set per file can own the bare names;
other sets keep an ending (".house") and Check lists them under "Names not in hash form".
Godot reads the 8 digits and the tag token after them; the set name never matters to the game.
"""

import re

import bpy

from .tiles import tile_variants
from .variants import MAIN_ROLE, name_tag, tag_text


def clean_name(h, variant, kind="CUBE", role=MAIN_ROLE):
    """"00010001_<height><group><number>": always tagged, "00010001_ma1", "00010001_ma2", "_hb1"."""
    digits = 8 if kind == "CUBE" else 6
    return f"{h:0{digits}b}_{tag_text(*role, variant)}"


def set_slug(collection):
    """"Circus" -> "circus", "My Set.001" -> "my_set": the set name used in exported names."""
    base = collection.name.split(".")[0].strip().lower()
    return re.sub(r"[^a-z0-9]+", "_", base).strip("_") or "set"


def export_name(obj, collection):
    """Name written to the .glb: hash, full tag, set name ("00010001_ma1_circus", "00010001_hb3_circus")."""
    kind = collection.mc_tiles.kind
    digits = 8 if kind == "CUBE" else 6
    return obj.name[:digits] + f"_{tag_text(*name_tag(obj.name))}_{set_slug(collection)}"


def untidy(collection):
    """[(object, clean name)] for tiles (every role) whose name isn't already the clean form.

    "hash.setname" is fine when another collection owns the bare hash name (names are unique per
    .blend), e.g. a linked copy of a piece: export writes hash_tag_setname either way.
    """
    kind = collection.mc_tiles.kind
    tiles = tile_variants(collection, kind, role=None)
    mine = {o for objs in tiles.values() for o in objs}
    out = []
    for h, objs in sorted(tiles.items()):
        for obj in objs:
            height, group, number = name_tag(obj.name)
            wanted = clean_name(h, number, kind, (height, group))
            if obj.name == wanted:
                continue
            owner = bpy.data.objects.get(wanted)
            base = obj.name.split(".")[0]
            if owner is not None and owner not in mine and "." in obj.name and base in (wanted, wanted.split("_")[0]):
                continue  # another set owns the name: "hash_ma1.setname" / "hash.setname" is clean here
            out.append((obj, wanted))
    return out


def tidy(collection):
    """Rename the set's tiles to their clean names. Returns (renamed, names already taken)."""
    todo = untidy(collection)
    # Two passes: park every tile on a temporary name first, so renames inside the set can't
    # collide with each other (A -> B's old name while B is still there).
    for i, (obj, _wanted) in enumerate(todo):
        obj.name = f"__mc_tidy_{i}"
    taken = []
    for obj, wanted in todo:
        obj.name = wanted
        if obj.name != wanted:  # owned by an object outside the set: Blender added ".001"
            taken.append(wanted)
    return len(todo), taken


class MCTILES_OT_tidy_names(bpy.types.Operator):
    """Rename this set's tiles to hash / hash_<height><group><number> (the set name is added back on export)"""

    bl_idname = "mc_tiles.tidy_names"
    bl_label = "Rename to Hash Names"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return context.scene.mc_tiles_active is not None

    def execute(self, context):
        renamed, taken = tidy(context.scene.mc_tiles_active)
        if taken:
            self.report({"WARNING"}, f"Renamed {renamed}; {len(taken)} names used by another set kept a .001")
        else:
            self.report({"INFO"}, f"Renamed {renamed} tiles")
        return {"FINISHED"}


CLASSES = (MCTILES_OT_tidy_names,)
