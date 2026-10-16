"""Sockets: spots on a tile where a small decor piece (a window, a plant...) may go, picked per cell.

Authoring (Sockets box in the main panel):
    - Decor pieces: meshes in the set's "Decor" child collection named decor_<type>_<n>
      ("decor_window_1"). Model a piece with its outside facing +Z (up), its origin the attach point.
    - A socket: an empty parented to a tile (Add Socket: at the 3D cursor, turned like it; snap the
      cursor onto a face with Surface Project + Geometry orientation, so +Z points out of the face).
      obj["mc_socket"] = type, obj["mc_socket_chance"] = chance it is filled (0..1).
    - Preview: each socket shows one piece of its type (a copy sharing the piece's mesh, not
      selectable, never exported, in the "SocketPreview" collection). < > picks the piece on the
      active socket, Shuffle rolls every socket by its chance. Tab into a preview edits the real piece.
Export (export.py) writes sockets as child nodes "socket_<type>_p<percent>" (mirrored onto mirror
copies) and the pieces as "decor_<type>_<n>"; Godot fills each socket per cell (mc_chunk_builder).
"""

import random
import re

import bpy
from mathutils import Matrix

SOCKET = "mc_socket"  # socket type, on the empty
CHANCE = "mc_socket_chance"  # 0..1
PICK = "mc_socket_pick"  # which piece the preview shows (-1 = none)
PREVIEW = "mc_socket_preview"  # marks a preview copy
DECOR_COLLECTION = "Decor"
PREVIEW_COLLECTION = "SocketPreview"
_TYPE = re.compile(r"[^a-z0-9]+")


def clean_type(text):
    """Socket / decor type: lower case letters and digits only ("Roof Plant" -> "roofplant")."""
    return _TYPE.sub("", text.lower()) or "decor"


def decor_type(obj):
    """"window" for "decor_window_2" (or "decor_window_2.001"), None for anything else."""
    parts = obj.name.split(".")[0].split("_")
    return parts[1] if len(parts) >= 2 and parts[0] == "decor" and parts[1] else None


def _is_decor(coll):
    return coll.name.split(".")[0] == DECOR_COLLECTION


def decor_collection(collection):
    """The set's 'Decor' child collection, else a 'Decor' collection in the scene."""
    if collection is not None:
        for child in collection.children:
            if _is_decor(child):
                return child
    scene = bpy.context.scene if bpy.context else None
    if scene is not None:
        for child in scene.collection.children_recursive:
            if _is_decor(child):
                return child
    return None


def decor_library(collection):
    """{type: [piece objects]} from the set's Decor collection, by name."""
    out = {}
    decor = decor_collection(collection)
    if decor is None:
        return out
    for obj in sorted(decor.all_objects, key=lambda o: o.name):
        t = decor_type(obj)
        if t and obj.type == "MESH" and obj.parent is None:
            out.setdefault(t, []).append(obj)
    return out


def is_socket(obj):
    return obj is not None and obj.type == "EMPTY" and SOCKET in obj


def sockets_of(root):
    """The socket empties under a tile."""
    return [c for c in root.children_recursive if is_socket(c)]


def set_sockets(collection):
    """Every socket on the set's tiles."""
    from .tiles import tile_variants

    out = []
    for objs in tile_variants(collection, collection.mc_tiles.kind, role=None).values():
        for obj in objs:
            out += sockets_of(obj)
    return out


def export_name(socket):
    """"socket_window_p50": what Godot reads (type, chance in percent)."""
    chance = max(0.0, min(1.0, float(socket.get(CHANCE, 1.0))))
    return f"socket_{clean_type(str(socket[SOCKET]))}_p{round(chance * 100)}"


# ---------------------------------------------------------------- preview


def _preview_collection(scene):
    coll = bpy.data.collections.get(PREVIEW_COLLECTION)
    if coll is None:
        coll = bpy.data.collections.new(PREVIEW_COLLECTION)
    if coll not in set(scene.collection.children_recursive):
        scene.collection.children.link(coll)
    return coll


def _clear_preview(socket):
    for child in list(socket.children):
        if PREVIEW in child:
            bpy.data.objects.remove(child, do_unlink=True)


def refresh_preview(scene, socket, library):
    """Show the socket's picked piece on it (or nothing), when previews are on."""
    _clear_preview(socket)
    pieces = library.get(clean_type(str(socket[SOCKET])), [])
    pick = int(socket.get(PICK, 0))
    if not scene.mc_tiles_socket_preview or not pieces or pick < 0:
        return
    piece = pieces[pick % len(pieces)]
    copy = bpy.data.objects.new(f"preview_{piece.name}", piece.data)  # shares the mesh: live edits
    copy[PREVIEW] = True
    copy.hide_select = True
    _preview_collection(scene).objects.link(copy)
    copy.parent = socket
    copy.matrix_parent_inverse = Matrix.Identity(4)
    copy.matrix_basis = piece.matrix_world.to_3x3().to_4x4()  # the piece's own turn / scale, as exported


def refresh_all(scene, collection):
    library = decor_library(collection)
    for socket in set_sockets(collection):
        refresh_preview(scene, socket, library)


def _on_preview(scene, context):
    collection = scene.mc_tiles_active
    if collection is not None:
        refresh_all(scene, collection)


# ---------------------------------------------------------------- operators


def _active_tile(context):
    from .tiles import tile_hash

    obj = context.active_object
    while obj is not None and obj.parent is not None:
        obj = obj.parent
    if obj is None or tile_hash(obj.name, "CUBE") is None:
        return None
    return obj


class MCTILES_OT_add_socket(bpy.types.Operator):
    """Add a socket to the selected tile at the 3D cursor (its +Z is the outside; snap the cursor onto
    a face with Surface Project + Geometry orientation)"""

    bl_idname = "mc_tiles.add_socket"
    bl_label = "Add Socket"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return _active_tile(context) is not None

    def execute(self, context):
        scene = context.scene
        tile = _active_tile(context)
        kind = clean_type(scene.mc_tiles_socket_type)
        socket = bpy.data.objects.new(f"socket_{kind}", None)
        socket.empty_display_type = "SINGLE_ARROW"
        socket.empty_display_size = 0.3
        socket[SOCKET] = kind
        socket[CHANCE] = 0.5
        socket[PICK] = 0
        for coll in tile.users_collection:
            coll.objects.link(socket)
        socket.parent = tile
        socket.matrix_parent_inverse = tile.matrix_world.inverted()
        socket.matrix_world = scene.cursor.matrix
        for other in context.selected_objects:
            other.select_set(False)
        socket.select_set(True)
        context.view_layer.objects.active = socket
        refresh_preview(scene, socket, decor_library(scene.mc_tiles_active))
        return {"FINISHED"}


class MCTILES_OT_socket_pick(bpy.types.Operator):
    """Show the previous / next piece of its type on the active socket"""

    bl_idname = "mc_tiles.socket_pick"
    bl_label = "Pick Piece"
    bl_options = {"REGISTER", "UNDO"}

    step: bpy.props.IntProperty(default=1)

    @classmethod
    def poll(cls, context):
        return is_socket(context.active_object)

    def execute(self, context):
        socket = context.active_object
        socket[PICK] = int(socket.get(PICK, 0)) + self.step
        refresh_preview(context.scene, socket, decor_library(context.scene.mc_tiles_active))
        return {"FINISHED"}


class MCTILES_OT_socket_shuffle(bpy.types.Operator):
    """Roll every socket of the set: filled by its chance, with a random piece of its type"""

    bl_idname = "mc_tiles.socket_shuffle"
    bl_label = "Shuffle"
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        return context.scene.mc_tiles_active is not None

    def execute(self, context):
        scene = context.scene
        collection = scene.mc_tiles_active
        library = decor_library(collection)
        for socket in set_sockets(collection):
            count = len(library.get(clean_type(str(socket[SOCKET])), []))
            filled = random.random() < float(socket.get(CHANCE, 1.0))
            socket[PICK] = random.randrange(count) if filled and count else -1
            refresh_preview(scene, socket, library)
        return {"FINISHED"}


class MCTILES_OT_socket_refresh(bpy.types.Operator):
    """Rebuild every socket preview (after adding / renaming decor pieces)"""

    bl_idname = "mc_tiles.socket_refresh"
    bl_label = "Refresh Previews"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        if context.scene.mc_tiles_active is not None:
            refresh_all(context.scene, context.scene.mc_tiles_active)
        return {"FINISHED"}


def draw_sockets(layout, context):
    """Sockets box: decor types, Add Socket, the active socket's type / chance / piece, preview."""
    scene = context.scene
    collection = scene.mc_tiles_active
    box = layout.box()
    library = decor_library(collection)
    sockets = set_sockets(collection)
    box.label(text=f"Sockets: {len(sockets)} · decor types: {len(library)}", icon="EMPTY_SINGLE_ARROW")
    if library:
        box.label(text=", ".join(f"{t} ({len(p)})" for t, p in library.items()))
    else:
        box.label(text="Add pieces to a 'Decor' collection: decor_<type>_<n>", icon="INFO")
    row = box.row(align=True)
    row.prop(scene, "mc_tiles_socket_type", text="")
    row.operator("mc_tiles.add_socket", icon="ADD")
    obj = context.active_object
    if is_socket(obj):
        col = box.column(align=True)
        col.prop(obj, f'["{SOCKET}"]', text="Type")
        col.prop(obj, f'["{CHANCE}"]', text="Chance")
        row = col.row(align=True)
        row.operator("mc_tiles.socket_pick", text="", icon="TRIA_LEFT").step = -1
        count = len(library.get(clean_type(str(obj[SOCKET])), []))
        pick = int(obj.get(PICK, 0))
        row.label(text=f"piece {pick % count + 1} / {count}" if count and pick >= 0 else "empty")
        row.operator("mc_tiles.socket_pick", text="", icon="TRIA_RIGHT").step = 1
    row = box.row(align=True)
    row.prop(scene, "mc_tiles_socket_preview", toggle=True)
    row.operator("mc_tiles.socket_shuffle", icon="FILE_REFRESH")
    row.operator("mc_tiles.socket_refresh", text="", icon="LOOP_BACK")


CLASSES = (MCTILES_OT_add_socket, MCTILES_OT_socket_pick, MCTILES_OT_socket_shuffle, MCTILES_OT_socket_refresh)


def register():
    bpy.types.Scene.mc_tiles_socket_type = bpy.props.StringProperty(
        name="Socket Type", default="window", description="Type of the next socket (matches decor_<type>_<n>)"
    )
    bpy.types.Scene.mc_tiles_socket_preview = bpy.props.BoolProperty(
        name="Preview", default=True, update=_on_preview,
        description="Show a decor piece on every socket (never exported)",
    )


def unregister():
    del bpy.types.Scene.mc_tiles_socket_preview
    del bpy.types.Scene.mc_tiles_socket_type
