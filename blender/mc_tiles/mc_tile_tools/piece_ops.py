"""Pieces in Blender: which minimum pieces the set has, placeholders for missing ones, and
"Generate Built Tiles" (turn every shape built from pieces into a real, editable tile).

Pieces box (main panel, cube sets):
    Find Pieces                     search every collection for tiles that can be each piece
    ✓ 00000011 [00001001 v] = 00001001 ↻1   Cube
                                    dropdown: any tile in the file that is this piece or a turn /
                                    mirror of it (piece_lib.py); the set builds with that tile
    ✗ 00010001 [        v] [+]      none anywhere: + adds a blocky stand-in (a half-size cube per
                                    solid corner) named by its hash, to model over or keep
    (new tiles go in rows in front of (-Y) everything in the scene, never on top of another set)
    Generate Built Tiles            every shape now built from pieces becomes a tile in the set: a
                                    root copy of the first piece, the other pieces as its children,
                                    all sharing the pieces' meshes (edit a piece, every generated
                                    tile follows; make one single user to change it on its own).
                                    Export joins the children, so the .glb is the same as before.
"""

import math

import bpy
from mathutils import Matrix, Vector

from . import cube
from .names import clean_name, set_slug
from .pieces import minimum_set
from .playground import tile_lookup
from .piece_lib import find_pieces, is_placeholder, set_tiles

PIECE_ROLE = "mc_piece_of"  # on generated tile parts: the tile they were generated for
PIECE_SOURCE = "mc_piece_source"  # on generated tile parts: the piece object the part copies
DETACHED = "mc_detached"  # on a detached tile: it was generated, now it is an ordinary tile
SPACING = 2.0  # whole metres, so every new tile's box keeps whole-number corners


def _tile_name(h, collection):
    """The hash, or hash.setname when another set already owns the bare name (one per .blend)."""
    name = clean_name(h, 1)
    return name if bpy.data.objects.get(name) is None else f"{name}.{set_slug(collection)}"


ROW = 12  # new tiles per row


def _free_positions(collection, count):
    """Positions for `count` new tiles in rows in front of (-Y) everything in the scene, so they never
    land on another set: starting under the set's left edge, ROW per row, at cell centres (x.5, y.5, z.5)."""
    scene = bpy.context.scene
    skip = {"Playground", "Arrange", "MC_Export_Temp"}
    front = None
    for obj in scene.objects:
        if any(c.name.split(".")[0] in skip for c in obj.users_collection):
            continue
        for corner in obj.bound_box:
            y = (obj.matrix_world @ Vector(corner)).y
            front = y if front is None else min(front, y)
    roots = [o for o in collection.all_objects if o.parent is None]
    left = min((o.matrix_world.translation.x for o in roots), default=0.0)
    ground = min((o.matrix_world.translation.z for o in roots), default=0.0)
    # Cell centres (x.5, y.5, z.5): each tile's box has whole-number corners, like the playground.
    start = Vector((math.floor(left) + 0.5, math.floor((front or 0.0) - 1.0) - 0.5, math.floor(ground) + 0.5))
    return [start + Vector(((i % ROW) * SPACING, -(i // ROW) * SPACING, 0.0)) for i in range(count)]


def _placeholder_mesh(h):
    """Half-size cube in every solid corner of the cell (cell from -0.5 to 0.5)."""
    verts, faces = [], []
    quad = ((0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3))
    for bit in range(8):
        if not (h >> bit) & 1:
            continue
        dx, dy, dz = cube_offset(bit)
        low = Vector((-0.5 + 0.5 * dx, -0.5 + 0.5 * dy, -0.5 + 0.5 * dz))
        base = len(verts)
        verts += [tuple(low + Vector((x, y, z))) for x in (0, 0.5) for y in (0, 0.5) for z in (0, 0.5)]
        faces += [tuple(base + i for i in f) for f in quad]
    mesh = bpy.data.meshes.new(f"{h:08b}_placeholder")
    mesh.from_pydata(verts, [], faces)
    mesh.update()
    return mesh


def cube_offset(bit):
    """Corner position in Blender axes (x right, y far, z up), same as playground.CORNER_OFFSETS."""
    return ((0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0), (0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1))[bit]


def _copy_parts(tile, collection, matrix, mark_source=False):
    """Copy a tile and its child parts (sharing mesh data) into the set at `matrix`. Returns the root.
    mark_source: write PIECE_SOURCE (the exact object each part copies) on every copy."""
    copies = {}
    for obj in [tile] + list(tile.children_recursive):
        copy = obj.copy()
        if mark_source:
            copy[PIECE_SOURCE] = obj.name
        for key in ("mc_home", "mc_pg_home", "mc_filter_hidden", "mc_helper_debug"):
            if key in copy:
                del copy[key]
        collection.objects.link(copy)
        copies[obj] = copy
    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]
            copy.matrix_parent_inverse = obj.matrix_parent_inverse.copy()
            copy.matrix_basis = obj.matrix_basis.copy()
    copies[tile].matrix_world = matrix
    return copies[tile]


def generate_built(collection, places=None):
    """Turn every shape built from pieces into a tile of the set. Returns the new tiles.

    places: {hash: position} to reuse (Regenerate puts tiles back where they were).
    Every part is marked with PIECE_ROLE (the tile's name) and PIECE_SOURCE (the piece object it
    copies), so status() can tell linked / edited / detached and Select Pieces can find the source.
    """
    settings = collection.mc_tiles
    tiles = set_tiles(collection)  # own tiles + borrowed pieces
    # Generated / detached tiles are never used as pieces: every generated tile is made of real pieces.
    pieces = {h: objs for h, objs in tiles.items() if not (PIECE_ROLE in objs[0] or DETACHED in objs[0])}
    report = cube.check(tiles.keys(), settings.mode, pieces=True, piece_hashes=pieces.keys())
    lookup = tile_lookup(pieces.keys(), settings.mode)
    tiles = pieces
    places = places or {}
    fresh = iter(_free_positions(collection, sum(1 for h, _ in report.built if h not in places)))
    made = []
    for h, found in report.built:
        position = places[h] if h in places else next(fresh)
        name = _tile_name(h, collection)
        root = None
        for piece in found:
            source, turns, mirrored = lookup[piece]
            tile = tiles[source][0]
            own = tile.matrix_world.to_3x3().to_4x4()
            turn = Matrix.Rotation(turns * math.pi / 2.0, 4, "Z")
            flip = Matrix.Scale(-1.0, 4, Vector((1.0, 0.0, 0.0))) if mirrored else Matrix.Identity(4)
            part = _copy_parts(tile, collection, Matrix.Translation(position) @ turn @ flip @ own, mark_source=True)
            for obj in [part] + list(part.children_recursive):
                obj[PIECE_ROLE] = name
            if root is None:
                root = part
                root.name = name
            else:
                world = part.matrix_world.copy()
                part.parent = root
                part.matrix_world = world
                part.name = f"{name.split('.')[0]}_piece"
        made.append(root)
    return made


# ---------------------------------------------------------------- customising generated tiles


def generated_root(obj):
    """The generated tile an object belongs to (itself or a parent with PIECE_ROLE), or None."""
    while obj is not None:
        if obj.parent is None:
            return obj if PIECE_ROLE in obj else None
        obj = obj.parent
    return None


def status(root):
    """("linked" | "edited", extra objects) for a generated tile, ("detached", 0) / (None, 0) otherwise.

    linked  every part still shares its piece's mesh: editing a piece updates it, Regenerate redoes it
    edited  at least one part has its own mesh (Make Single User), or objects you added are parented
            to it: Regenerate leaves it alone
    """
    if DETACHED in root:
        return "detached", 0
    if PIECE_ROLE not in root:
        return None, 0
    parts = [root] + list(root.children_recursive)
    extras = [p for p in parts if PIECE_ROLE not in p]
    own_mesh = []
    for part in parts:
        if PIECE_ROLE not in part or part.data is None:
            continue
        source = bpy.data.objects.get(part.get(PIECE_SOURCE, ""))
        if source is None or source.data != part.data:  # its own mesh now (or the piece is gone)
            own_mesh.append(part)
    return ("edited" if own_mesh or extras else "linked"), len(extras)


def detach(root, union=False):
    """Make a generated tile an ordinary tile: its piece parts joined into one mesh of its own.

    union: Boolean-union the parts (exact solver) instead of a plain join, so hidden internal faces go
    and the crossings become real edges. Objects you parented to the tile stay as its children.
    """
    parts = [p for p in root.children_recursive if PIECE_ROLE in p and p.type == "MESH"]
    extras = [p for p in root.children if PIECE_ROLE not in p]
    root.data = root.data.copy()  # never touch the piece's mesh
    if union:
        for part in parts:
            modifier = root.modifiers.new(f"union_{part.name}", "BOOLEAN")
            modifier.operation = "UNION"
            modifier.solver = "EXACT"
            modifier.object = part
        depsgraph = bpy.context.evaluated_depsgraph_get()
        mesh = bpy.data.meshes.new_from_object(root.evaluated_get(depsgraph))
        root.modifiers.clear()
        old = root.data
        root.data = mesh
        bpy.data.meshes.remove(old)
    else:
        to_root = root.matrix_world.inverted()
        meshes = []
        for part in parts:
            mesh = part.data.copy()
            mesh.transform(to_root @ part.matrix_world)
            meshes.append(mesh)
        _join_meshes(root.data, meshes)
    for extra in extras:  # keep additions where they are, still parented to the tile
        world = extra.matrix_world.copy()
        extra.parent = root
        extra.matrix_world = world
    for part in parts:
        bpy.data.objects.remove(part, do_unlink=True)
    for key in (PIECE_ROLE, PIECE_SOURCE):
        if key in root:
            del root[key]
    root[DETACHED] = True
    root.data.name = root.name


def _join_meshes(target, meshes):
    """Append meshes (already in target's space) to target, materials included."""
    import bmesh

    bm = bmesh.new()
    bm.from_mesh(target)
    for mesh in meshes:
        for material in mesh.materials:
            if material is not None and material.name not in [m.name for m in target.materials if m]:
                target.materials.append(material)
        index = {i: [m.name if m else None for m in target.materials].index(m.name if m else None)
                 for i, m in enumerate(mesh.materials) if m is not None}
        before = len(bm.faces)
        bm.from_mesh(mesh)
        bm.faces.ensure_lookup_table()
        for face in bm.faces[before:]:
            face.material_index = index.get(face.material_index, face.material_index)
        bpy.data.meshes.remove(mesh)
    bm.to_mesh(target)
    bm.free()
    target.update()


def regenerate(collection):
    """Delete the set's still-linked generated tiles and generate them again in the same spots
    (after pieces changed shape or choice). Edited / detached tiles are left alone. Returns count."""
    places = {}
    for root in [o for o in collection.all_objects if o.parent is None and PIECE_ROLE in o]:
        if status(root)[0] != "linked":
            continue
        from .tiles import tile_hash

        places[tile_hash(root.name, "CUBE")] = root.matrix_world.translation.copy()
        for part in list(root.children_recursive) + [root]:
            bpy.data.objects.remove(part, do_unlink=True)
    return len(generate_built(collection, places))


class MCTILES_OT_detach(bpy.types.Operator):
    """Make the selected generated tile an ordinary tile with its own mesh (it stops following its pieces)"""

    bl_idname = "mc_tiles.detach"
    bl_label = "Detach"
    bl_options = {"REGISTER", "UNDO"}

    union: bpy.props.BoolProperty(
        name="Clean Union", description="Boolean-union the pieces: no hidden faces, crossings become edges",
    )

    @classmethod
    def poll(cls, context):
        return generated_root(context.active_object) is not None

    def execute(self, context):
        root = generated_root(context.active_object)
        if context.object is not None and context.object.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        detach(root, self.union)
        self.report({"INFO"}, f"{root.name} is now its own tile")
        return {"FINISHED"}


class MCTILES_OT_select_pieces(bpy.types.Operator):
    """Select the piece tiles the selected generated tile is made of (edit them, every copy follows)"""

    bl_idname = "mc_tiles.select_pieces"
    bl_label = "Select Pieces"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return generated_root(context.active_object) is not None

    def execute(self, context):
        root = generated_root(context.active_object)
        names = {p[PIECE_SOURCE] for p in [root] + list(root.children_recursive) if PIECE_SOURCE in p}
        found = [bpy.data.objects[n] for n in names if n in bpy.data.objects]
        found = list({(o.parent if o.parent is not None else o) for o in found})  # the piece tiles, not their parts
        found = [o for o in found if o.name in context.view_layer.objects]
        if not found:
            self.report({"WARNING"}, "Its pieces are gone or hidden")
            return {"CANCELLED"}
        for obj in context.selected_objects:
            obj.select_set(False)
        for obj in found:
            obj.hide_set(False)
            obj.select_set(True)
        context.view_layer.objects.active = found[0]
        return {"FINISHED"}


class MCTILES_OT_regenerate(bpy.types.Operator):
    """Rebuild the still-linked generated tiles in place (edited and detached tiles are kept)"""

    bl_idname = "mc_tiles.regenerate"
    bl_label = "Regenerate Linked"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        count = regenerate(context.scene.mc_tiles_active)
        self.report({"INFO"}, f"Regenerated {count} tiles")
        return {"FINISHED"}


class MCTILES_OT_add_placeholder(bpy.types.Operator):
    """Add a blocky stand-in for a missing piece (a half-size cube per solid corner), named by its hash"""

    bl_idname = "mc_tiles.add_placeholder"
    bl_label = "Add Placeholder"
    bl_options = {"REGISTER", "UNDO"}

    hash: bpy.props.IntProperty(min=0, max=255)

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        name = _tile_name(self.hash, collection)
        obj = bpy.data.objects.new(name, _placeholder_mesh(self.hash))
        obj.location = _free_positions(collection, 1)[0]
        collection.objects.link(obj)
        find_pieces(collection)  # use it if nothing better exists anywhere
        self.report({"INFO"}, f"Added placeholder {name}")
        return {"FINISHED"}


class MCTILES_OT_generate_built(bpy.types.Operator):
    """Make every shape built from pieces a real tile in the set (linked piece copies you can edit)"""

    bl_idname = "mc_tiles.generate_built"
    bl_label = "Generate Built Tiles"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def execute(self, context):
        made = generate_built(context.scene.mc_tiles_active)
        self.report({"INFO"}, f"Generated {len(made)} tiles from pieces")
        return {"FINISHED"}


def _how(source, turns, mirrored):
    return f"{source:08b}" + (f" ↻{turns}" if turns else "") + (" ⇋" if mirrored else "")


class MCTILES_OT_delete_generated(bpy.types.Operator):
    """Delete the set's generated tiles (made by Generate Built Tiles); the pieces themselves stay"""

    bl_idname = "mc_tiles.delete_generated"
    bl_label = "Delete Generated Tiles"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        collection = context.scene.mc_tiles_active
        doomed = [o for o in collection.all_objects if PIECE_ROLE in o]
        for obj in doomed:
            bpy.data.objects.remove(obj, do_unlink=True)
        self.report({"INFO"}, f"Deleted {len(doomed)} generated objects")
        return {"FINISHED"}


class MCTILES_OT_find_pieces(bpy.types.Operator):
    """Search every collection for tiles that can be this set's pieces and pick one for each"""

    bl_idname = "mc_tiles.find_pieces"
    bl_label = "Find Pieces"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        collection = context.scene.mc_tiles_active
        return collection is not None and collection.mc_tiles.kind == "CUBE"

    def execute(self, context):
        chosen = find_pieces(context.scene.mc_tiles_active)
        found = sum(1 for obj in chosen.values() if obj is not None)
        self.report({"INFO"}, f"Pieces: {found} of {len(chosen)} found")
        return {"FINISHED"}


def draw_pieces(layout, collection, tiles):
    """Pieces box: a source dropdown per minimum piece (any collection), placeholders, Generate."""
    settings = collection.mc_tiles
    if settings.kind != "CUBE":
        return
    needed = minimum_set(settings.mode)
    items = {item.hash: item for item in settings.piece_sources}
    box = layout.box()
    if not settings.build_from_pieces:
        box.label(text="Build from Pieces is off: only modelled tiles are used", icon="INFO")
    row = box.row(align=True)
    have = sum(1 for h in needed if h in items and items[h].source is not None)
    row.label(text=f"Pieces: {have} of {len(needed)}", icon="MOD_BOOLEAN")
    row.operator("mc_tiles.find_pieces", text="Find Pieces", icon="VIEWZOOM")
    if set(items) != set(needed):
        box.label(text="Press Find Pieces to pick them from any collection", icon="INFO")
        return
    lookup = tile_lookup(tiles.keys(), settings.mode)
    col = box.column(align=True)
    for h in needed:
        item = items[h]
        row = col.row(align=True)
        row.label(text=f"{h:08b}", icon="CHECKMARK" if item.source is not None else "X")
        row.prop(item, "source", text="")
        if item.source is None:
            row.operator("mc_tiles.add_placeholder", text="", icon="ADD").hash = h
        else:
            how = _how(*lookup[h]) if h in lookup else "?"
            sub = row.row()
            sub.label(text=f"= {how}" + ("  (placeholder)" if is_placeholder(item.source) else ""))
            owners = ", ".join(c.name for c in item.source.users_collection)
            sub.label(text=owners)
    built = cube.check(tiles.keys(), settings.mode, pieces=True).built
    roots = [o for o in collection.all_objects if PIECE_ROLE in o and o.parent is None]
    counts = {"linked": 0, "edited": 0}
    for root in roots:
        counts[status(root)[0]] += 1
    row = box.row(align=True)
    if built:
        row.operator("mc_tiles.generate_built", text=f"Generate {len(built)} Built Tiles", icon="MOD_BOOLEAN")
    if roots:
        row.operator("mc_tiles.delete_generated", text=f"Delete {len(roots)} Generated", icon="TRASH")
    if roots:
        line = box.row(align=True)
        line.label(text=f"Generated: {counts['linked']} linked, {counts['edited']} edited", icon="LINKED")
        line.operator("mc_tiles.regenerate", text="Regenerate Linked", icon="FILE_REFRESH")
    active = generated_root(bpy.context.active_object)
    if active is not None and active.name in collection.all_objects:
        state, extras = status(active)
        sub = box.box()
        sub.label(text=f"{active.name}: {state}" + (f", {extras} added objects" if extras else ""), icon="OBJECT_DATA")
        line = sub.row(align=True)
        line.operator("mc_tiles.select_pieces", icon="RESTRICT_SELECT_OFF")
        line.operator("mc_tiles.detach", text="Detach").union = False
        line.operator("mc_tiles.detach", text="Detach + Clean Union").union = True


CLASSES = (
    MCTILES_OT_add_placeholder, MCTILES_OT_generate_built, MCTILES_OT_delete_generated, MCTILES_OT_find_pieces,
    MCTILES_OT_detach, MCTILES_OT_select_pieces, MCTILES_OT_regenerate,
)
