"""Export the active tile set to its own .glb (replaces ExportMergeChildMesh.py).

Same result as the old script, but per set and without the "everything visible" trap:
    - only the set's tiles: helpers, Helpers / Playground collections and pg_ copies are skipped,
      whether visible or not
    - each tile is copied (objects AND mesh data, so the originals are never changed),
      rotation and scale applied (location kept), modifiers applied, child meshes joined into it
    - named by its tile name without Blender's ".001" ("00000001_circle", "00000001_circle_v2");
      Godot reads the first 8 digits and the "_v" variant number
    - written to the set's Export path (".glb" added if missing), then the copies are deleted
"""

import os

import bpy

from .tiles import tile_variants
from .view import MATERIAL_NAME as DEBUG_MATERIAL

TEMP_COLLECTION = "MC_Export_Temp"


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


def export_set(collection):
    """Export one tile set. Returns (tiles exported, path); raises ValueError with a reason."""
    path = export_path(collection)
    if not path:
        raise ValueError(f"{collection.name} has no Export path")
    tiles = tile_variants(collection, collection.mc_tiles.kind)
    if not tiles:
        raise ValueError(f"{collection.name} has no tiles")
    if bpy.context.object is not None and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")

    temp = bpy.data.collections.new(TEMP_COLLECTION)
    bpy.context.scene.collection.children.link(temp)
    mesh_names = set()
    try:
        roots = []
        for h in sorted(tiles):
            for original in tiles[h]:  # every variant ("_v2", ...) is exported
                copies = _copy_tile(original, temp)
                mesh_names.update(c.data.name for c in copies.values() if c.type == "MESH")
                roots.append((original, copies[original]))
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
        for original, root in roots:
            parts = [c for c in root.children_recursive if c.type == "MESH"]
            if parts and root.type == "MESH":
                _select_only([root] + parts, root)
                bpy.ops.object.join()
            root.name = original.name.split(".")[0]
            exported.append(root)

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
