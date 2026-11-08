"""Squares tile set: the 5 tiles of MCGrid's marching squares mode (a stand-alone script, not part
of the MC Tile Tools add-on; simple stand-in tiles to model over).

In squares mode each layer of corners is its own sheet: a cell draws only its 4 bottom corners, as
a straight-sided piece one cell high (Godot: src/marching_cubes/terrain/1_shape/mc_grid_squares.cpp).
The tile for bottom corners b is named by the hash with the top corners set the same as the bottom
ones (b | b << 4). 16 such hashes, 6 shapes up to Y turns, 5 to model (empty draws nothing):

    corner        00010001   1 corner
    edge          00110011   2 side by side
    diagonal      01010101   2 opposite, joined (one shape for the saddle)
    inner corner  01110111   3 corners
    full          11111111   4 corners

How a tile is shaped (Blender axes: Z up; corner order of cube.py / Godot):
    Seen from above, plain marching squares: the solid region is the square cut where a side runs
    from a solid corner to an empty one, at the side's middle. That region is pushed up the whole
    cell (z -0.5 .. 0.5): walls (Rock) along the cuts, the top cap (Grass) at z 0.5. The cap is
    always there, also where the layer above covers it (Godot draws it; it is hidden in the solid).
    No walls on the cell's sides (the neighbour carries on) and no bottom.
    Modelled in the cell cube as usual; Godot draws squares tiles half a cell lower (centred on the
    corner layer, so the cap is level with the heightmap tiles' ground: MCGrid::_tile_offset).

Run (writes the .blend and the .glb next to each other):

    Blender --background --python scripts/squares_tiles.py -- \\
        --blend ../../minecraft/project/assets/Marching/Squares.blend \\
        --glb ../../minecraft/project/assets/Marching/Squares.glb

or open it in Blender's Text Editor and Run Script (builds the "Squares" collection in the open file).
"""

import sys

GRASS = (0.13, 0.38, 0.07, 1.0)
ROCK = (0.33, 0.21, 0.13, 1.0)
SPACING = 2.0  # metres between tiles in the Blender layout
BOTTOM = -0.5
TOP = 0.5

# Corner positions (x, y) seen from above, Blender axes: 0,1 near (-Y), 0,3 left (-X) (cube.py).
# Counter-clockwise seen from above, so a region built in this order faces up.
CORNERS_2D = ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))

# The 5 shapes, by their bottom corners (bits 0..3); the tile's hash repeats them on top.
SHAPES = (
    ("corner", 0b0001),
    ("edge", 0b0011),
    ("diagonal", 0b0101),
    ("inner corner", 0b0111),
    ("full", 0b1111),
)


def tile_hash(bottom):
    return bottom | (bottom << 4)


def bits_name(h):
    return format(h, "08b")


def region(bottom):
    """The solid region seen from above, counter-clockwise: going round the square's sides, each
    solid corner, and each side's middle where the side goes from solid to empty or back. Returns
    the points and, per point, whether it is a cut (a side's middle)."""
    points = []
    cuts = []
    for i in range(4):
        j = (i + 1) % 4
        solid_i = (bottom >> i) & 1
        solid_j = (bottom >> j) & 1
        if solid_i:
            points.append(CORNERS_2D[i])
            cuts.append(False)
        if solid_i != solid_j:
            a, b = CORNERS_2D[i], CORNERS_2D[j]
            points.append(((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5))
            cuts.append(True)
    return points, cuts


def build_tile(bottom):
    """Vertices + faces (vertex indices, material: 0 grass, 1 rock) of the tile for bottom corners."""
    points, cuts = region(bottom)
    verts = []
    faces = []
    top = [len(verts) + k for k in range(len(points))]
    verts += [(x, y, TOP) for x, y in points]
    faces.append((top, 0))  # the cap: counter-clockwise from above, so it faces up
    # A wall along each cut: a region edge between two side middles runs across the cell. The
    # region is counter-clockwise, so its outside is on the right of a -> b: the quad
    # (a low, b low, b high, a high) faces there.
    n = len(points)
    for k in range(n):
        m = (k + 1) % n
        if not (cuts[k] and cuts[m]):
            continue  # along a cell side: the neighbour carries on
        (ax, ay), (bx, by) = points[k], points[m]
        base = len(verts)
        verts += [(ax, ay, BOTTOM), (bx, by, BOTTOM), (bx, by, TOP), (ax, ay, TOP)]
        faces.append(([base, base + 1, base + 2, base + 3], 1))
    return verts, faces


# --- Blender ------------------------------------------------------------------------------------

def material(bpy, name, color):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    if mat.node_tree is None:
        mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.9
    mat.diffuse_color = color
    return mat


def make_object(bpy, name, verts, faces, mats, location):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(verts, [], [f[0] for f in faces])
    for i, (_, mat_index) in enumerate(faces):
        mesh.polygons[i].material_index = mat_index
    for mat in mats:
        mesh.materials.append(mat)
    mesh.update()
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    return obj


def build_collection(bpy):
    collection = bpy.data.collections.get("Squares")
    if collection is None:
        collection = bpy.data.collections.new("Squares")
        bpy.context.scene.collection.children.link(collection)
    for obj in list(collection.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    mats = [material(bpy, "Grass", GRASS), material(bpy, "Rock", ROCK)]
    for n, (label, bottom) in enumerate(SHAPES):
        verts, faces = build_tile(bottom)
        name = bits_name(tile_hash(bottom))
        collection.objects.link(make_object(bpy, name, verts, faces, mats, (n * SPACING, 0.0, 0.0)))
        print("squares_tiles: %s %s (%d faces)" % (name, label, len(faces)))
    return collection


def main():
    import bpy

    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    blend_path = args[args.index("--blend") + 1] if "--blend" in args else ""
    glb_path = args[args.index("--glb") + 1] if "--glb" in args else ""
    if blend_path:  # a fresh file: no default cube / camera / light
        bpy.ops.wm.read_factory_settings(use_empty=True)
    collection = build_collection(bpy)
    if blend_path:
        bpy.context.preferences.filepaths.save_version = 0  # no .blend1 backup
        bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    if glb_path:
        bpy.ops.object.select_all(action="DESELECT")
        for obj in collection.objects:
            obj.select_set(True)
        bpy.ops.export_scene.gltf(filepath=glb_path, export_format="GLB", use_selection=True, export_yup=True)
        print("squares_tiles: wrote", glb_path)


if __name__ == "__main__":
    main()
