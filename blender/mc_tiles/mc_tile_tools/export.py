"""Export the active tile set to its own .glb (replaces ExportMergeChildMesh.py).

Same result as the old script, but per set and without the "everything visible" trap:
    - only the set's tiles: helpers, Helpers / Playground collections and pg_ copies are skipped,
      whether visible or not
    - each tile is copied (objects AND mesh data, so the originals are never changed),
      rotation and scale applied (location kept), modifiers applied, child meshes joined into it
    - named hash + tag + set name ("00000001_ma1_circus", "00000001_hb2_circus"; names.py,
      variants.py); Godot reads the first 8 digits and the tag (height, group, number)
    - every role (height + group) is written; cube sets: within each role, a shape modelled only
      as its left/right mirror image is written as a flipped copy ("mirror on export"), so a 68
      set needs only 53 modelled tiles, and (with Build from Pieces on) a shape it can build from its
      own pieces is built (so Godot's L / M / H tables have the same shapes as the playground levels)
    - cube sets: a shape with no tile and no mirror is built from smaller tiles overlapping (pieces.py,
      every solid edge / full face inside one piece): their meshes, turned / flipped, joined into one tile
    - generated tiles (mirror copies, built tiles) are placed above the tile they come from, LIFT
      metres per copy, so nothing in the .glb overlaps (Godot only reads the meshes)
    - written to the set's Export path (".glb" added if missing), then the copies are deleted
"""

import math
import os

import bpy
from mathutils import Matrix, Vector

from . import cube
from .names import export_name, set_slug

from .piece_lib import set_tiles
from .tiles import tiles_by_role
from .variants import MAIN_ROLE, tag_text
from .view import MATERIAL_NAME as DEBUG_MATERIAL

TEMP_COLLECTION = "MC_Export_Temp"
LIFT = 2.0  # metres between a tile and each generated copy (mirror, built) stacked above it


def _stack_above(source, copy, stacks):
    """Put a generated copy above the exported tile it comes from, one LIFT higher per copy
    (Godot only reads the meshes; this just keeps the .glb readable)."""
    level = stacks.get(source.name, 0) + 1
    stacks[source.name] = level
    copy.location = source.location + Vector((0.0, 0.0, LIFT * level))


def export_path(collection):
    """Absolute .glb path for a set, or "" if none is set."""
    path = collection.mc_tiles.export_path.strip()
    if not path:
        return ""
    path = bpy.path.abspath(path)
    if not path.lower().endswith(".glb"):
        path += ".glb"
    return path


def _copy_tile(root, temp):
    """Copy a tile and its parts (with their own mesh data) into the temp collection."""
    copies = {}
    for obj in [root] + list(root.children_recursive):
        copy = obj.copy()
        if obj.data is not None:
            copy.data = obj.data.copy()
        copy.hide_select = False
        copy.hide_viewport = False
        for key in ("mc_helper_debug", "mc_pg_role"):
            if key in copy:
                del copy[key]
        temp.objects.link(copy)
        copies[obj] = copy
    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]
            copy.matrix_parent_inverse = obj.matrix_parent_inverse.copy()
            copy.matrix_basis = obj.matrix_basis.copy()
    # The helper debug material (object-level slots) must not reach the export; other
    # object-level materials are real and stay.
    for copy in copies.values():
        for slot in copy.material_slots:
            if slot.link == "OBJECT" and slot.material and slot.material.name == DEBUG_MATERIAL:
                slot.material = None
                slot.link = "DATA"
    return copies


def _select_only(objects, active):
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = active


def _cleanup(temp, mesh_names):
    """Delete the temp copies and every mesh copy made for them (joined ones are orphans now)."""
    for obj in list(temp.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for name in mesh_names:
        mesh = bpy.data.meshes.get(name)
        if mesh is not None and mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    bpy.data.collections.remove(temp)


def _mirror_copies(collection, tiles, by_hash, temp, mesh_names, stacks):
    """Flipped copies for every shape the set only has as a left/right mirror (all variants)."""
    report = cube.check(tiles.keys(), collection.mc_tiles.mode)
    flip = Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0)))
    made = []
    for _shape, source in report.mirrored:
        for root in by_hash.get(source, []):
            if root.type != "MESH":
                continue
            copy = root.copy()
            copy.data = root.data.copy()
            mesh_names.add(copy.data.name)
            temp.objects.link(copy)
            # Rotation and scale are already applied, so the mesh is in cell axes around the
            # tile centre: flip local X, then turn the faces back the right way out.
            copy.data.transform(flip)
            copy.data.flip_normals()
            copy.data.update()
            copy.name = f"{cube.mirror(source):08b}{root.name[8:]}"
            _stack_above(root, copy, stacks)
            made.append(copy)
    return made


def _piece_mesh(root, turns, mirrored, temp, mesh_names):
    """A copy of an exported tile's mesh, flipped (mirror) then turned about Z, as its own object."""
    copy = root.copy()
    copy.data = root.data.copy()
    mesh_names.add(copy.data.name)
    temp.objects.link(copy)
    if mirrored:
        copy.data.transform(Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))))
        copy.data.flip_normals()
    if turns:
        copy.data.transform(Matrix.Rotation(turns * math.pi / 2.0, 4, "Z"))
    copy.data.update()
    copy.location = (0.0, 0.0, 0.0)
    return copy


def _built_copies(collection, tiles, by_hash, temp, mesh_names, stacks, role=MAIN_ROLE):
    """One joined tile for every shape built from pieces (base variant of each piece), all from
    one role's own tiles, named with that role's tag (so L / H are as complete as the playground)."""
    from .playground import tile_lookup  # playground imports view, which imports nothing from here

    settings = collection.mc_tiles
    report = cube.check(tiles.keys(), settings.mode, pieces=True)
    lookup = tile_lookup(tiles.keys(), settings.mode)
    made = []
    for shape, found in report.built:
        parts = []
        first = None  # the first piece's exported tile: the built tile goes above it
        for piece in found:
            source, turns, mirrored = lookup[piece]
            root = by_hash.get(source, [None])[0]
            if root is not None and root.type == "MESH":
                parts.append(_piece_mesh(root, turns, mirrored, temp, mesh_names))
                first = first or root
        if not parts:
            continue
        _select_only(parts, parts[0])
        bpy.ops.object.join()
        joined = parts[0]
        joined.name = f"{shape:08b}_{tag_text(*role, 1)}_{set_slug(collection)}"
        _stack_above(first, joined, stacks)
        made.append(joined)
    return made


def export_set(collection):
    """Export one tile set. Returns (tiles exported, path); raises ValueError with a reason."""
    path = export_path(collection)
    if not path:
        raise ValueError(f"{collection.name} has no Export path")
    # Every role (height, group; variants.py) is written once, each from its own tiles only: the
    # main role (M a) with its chosen M a pieces (piece_lib.py), the others as modelled. A piece
    # source of another role is written under that role, never as M a.
    kind = collection.mc_tiles.kind
    names = [MAIN_ROLE] + [r for r in tiles_by_role(collection, kind) if r != MAIN_ROLE]
    roles = {role: set_tiles(collection, role) for role in names}
    roles = {role: tiles for role, tiles in roles.items() if tiles}
    if not roles:
        raise ValueError(f"{collection.name} has no tiles")
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    temp = bpy.data.collections.new(TEMP_COLLECTION)
    bpy.context.scene.collection.children.link(temp)
    mesh_names = set()
    try:
        roots = []  # (role, original, copy)
        for role, tiles in roles.items():
            for h in sorted(tiles):
                for original in tiles[h]:  # every number ("_ma2", ...) is exported
                    copies = _copy_tile(original, temp)
                    mesh_names.update(c.data.name for c in copies.values() if c.type == "MESH")
                    roots.append((role, original, copies[original]))
        bpy.context.view_layer.update()
        everything = list(temp.objects)
        meshes = [o for o in everything if o.type == "MESH"]

        # Rotation and scale into the meshes (location kept), then modifiers.
        _select_only(everything, meshes[0] if meshes else everything[0])
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        if meshes:
            _select_only(meshes, meshes[0])
            bpy.ops.object.convert(target="MESH")

        # Join each tile's child meshes into it and give it its clean name.
        exported = []
        by_hash = {role: {} for role in roles}  # per role: {hash: [exported copy, ...]}
        for role, original, root in roots:
            parts = [c for c in root.children_recursive if c.type == "MESH"]
            if parts and root.type == "MESH":
                _select_only([root] + parts, root)
                bpy.ops.object.join()
            root.name = export_name(original, collection)
            exported.append(root)
            by_hash[role].setdefault(int(root.name[:8], 2), []).append(root)

        if collection.mc_tiles.kind == "CUBE":
            stacks = {}  # exported tile name -> copies stacked above it so far
            for role, tiles in roles.items():  # mirrors and pieces within each role, never mixed
                exported += _mirror_copies(collection, tiles, by_hash[role], temp, mesh_names, stacks)
                if collection.mc_tiles.build_from_pieces:  # off: only modelled tiles (and mirrors)
                    exported += _built_copies(collection, tiles, by_hash[role], temp, mesh_names, stacks, role)

        os.makedirs(os.path.dirname(path), exist_ok=True)
        _select_only(exported, exported[0])
        bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_format="GLB")
        return len(exported), path
    finally:
        _cleanup(temp, mesh_names)


class MCTILES_OT_export_set(bpy.types.Operator):
    """Export the shown tile set to its .glb (only its tiles; the .blend is not changed)"""

    bl_idname = "mc_tiles.export_set"
    bl_label = "Export Set"
    bl_options = {"REGISTER"}

    @classmethod
    def poll(cls, context):
        return context.scene.mc_tiles_active is not None

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        selected = list(context.selected_objects)
        active = context.view_layer.objects.active
        try:
            count, path = export_set(collection)
        except (ValueError, RuntimeError) as error:
            self.report({"ERROR"}, str(error))
            return {"CANCELLED"}
        finally:
            _select_only([o for o in selected if o.name in context.view_layer.objects], active)
        self.report({"INFO"}, f"Exported {count} tiles to {path}")
        return {"FINISHED"}


CLASSES = (MCTILES_OT_export_set,)
