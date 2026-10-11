"""Tile names: in Blender just the hash ("00010001") or hash + variant ("00010001_v2");
in the exported .glb the set name is added ("00010001_circus", "00010001_circus_v2").

Blender object names are unique per .blend, so only one set per file can own the bare names;
other sets keep an ending (".house") and Check lists them under "Names not in hash form".
Godot reads only the 8 digits and the "_v" number, so the set name never matters to the game.
"""

import re

import bpy

from .tiles import tile_variants
from .variants import variant_number


def clean_name(h, variant, kind="CUBE"):
    digits = 8 if kind == "CUBE" else 6
    return f"{h:0{digits}b}" + (f"_v{variant}" if variant > 1 else "")


def set_slug(collection):
    """"Circus" -> "circus", "My Set.001" -> "my_set": the set name used in exported names."""
    base = collection.name.split(".")[0].strip().lower()
    return re.sub(r"[^a-z0-9]+", "_", base).strip("_") or "set"


def export_name(obj, collection):
    """Name written to the .glb: hash, set name, variant ("00010001_circus_v2")."""
    kind = collection.mc_tiles.kind
    digits = 8 if kind == "CUBE" else 6
    variant = variant_number(obj.name)
    return obj.name[:digits] + f"_{set_slug(collection)}" + (f"_v{variant}" if variant > 1 else "")


def untidy(collection):
    """[(object, clean name)] for tiles whose name isn't already the clean form."""
    kind = collection.mc_tiles.kind
    out = []
    for h, objs in sorted(tile_variants(collection, kind).items()):
        for obj in objs:
            wanted = clean_name(h, variant_number(obj.name), kind)
            if obj.name != wanted:
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
    """Rename this set's tiles to hash / hash_vN (the set name is added back on export)"""

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
