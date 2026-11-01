"""Cliff tile set: generates the heightmap tiles (no overhangs) for MCGrid's heightmap mode.

Animal Crossing style: flat grass tiers with rounded cliff edges, a grass lip on top of each cliff
and a rock face below it. Every tile is computed from its corner bits, so neighbouring tiles always
meet: a stand-alone script (not part of the MC Tile Tools add-on), re-run it after changing a number.

Run (writes the .blend and the .glb next to each other):

    Blender --background --python scripts/cliff_tiles.py -- \\
        --blend ../../minecraft/project/assets/Marching/Cliff.blend \\
        --glb ../../minecraft/project/assets/Marching/Cliff.glb
    add --style blocky (Cliff_blocky.blend / .glb) for the Link's Awakening look: square tiers along
    the cells' middle lines, rounded outer corners, sharp inner ones (blocky() below);
    --style both (Cliffs.blend / .glb): one set, rounded tiles as group a, blocky as group b
    --style square (Cliff_square.blend / .glb): blocky with no curves at all — sharp outer corners
    (ROUNDING 0) and flat rock faces (no strata grooves)
    ("00001111_mb1"); MCGrid gives each connected region one group

or open it in Blender's Text Editor and Run Script (builds the "Cliff" collection in the open file).

How a tile is shaped (Blender axes: Z up; corner order of cube.py / Godot, bit i = corner i):

    bottom corners 0..3 and top corners 4..7, each solid or empty; heightmap mode means a top corner
    is only solid above a solid bottom corner.
    Seen from above, each layer's solid corners make a 2D region: inside where the bilinear blend of
    the 4 corners (solid 1, empty 0) is over one half. Along a cell side that blend only depends on
    the side's 2 corners, so the regions of neighbouring tiles always meet; inside the cell it
    curves (a single corner gives a rounded quarter, a diagonal pair a saddle).
        bottom region  solid from the cell floor (z -0.5) up to the cell middle (z 0): the ground,
                       a flat grass top at z 0 where the top region doesn't cover it
        top region     solid all the way up (z -0.5 .. 0.5): the next tier, continued by the cell above
    Walls: around the bottom region from z -0.5 to 0 (rock, its top LIP of grass: the cliff's lip,
    except where the top region's wall carries the cliff on up),
    around the top region from z 0 to 0.5 (rock). The rock face has STRATA bands per half cell: it is
    pushed in by groove(z), a function of height only that is 0 at z -0.5, 0 and 0.5, and darkened
    in the grooves (vertex colours), so stacked tiles line up.
    Seams: where a wall meets a cell side its normal (and the groove push along it) is kept in the
    side's plane, so both tiles meeting there compute the same point and the same normal.
    Faces that are never seen (the cell's bottom,
    the solid inside) are not built.

Tiles written: one per Y-turn group of the allowed hashes (Godot turns the rest), named by the hash
("00001111"); materials Grass and Rock.
"""

import math
import sys

STYLE = "rounded"  # "rounded" (Animal Crossing) or "blocky" (Link's Awakening); --style on the command line
ROUNDING = 0.15  # blocky: radius of a corner that sticks out (m)
N = 9  # sub-grid per cell side, odd: no grid point on a side's middle or the middle lines (blocky: 11)
LIP = 0.12  # metres of grass at the top of each cliff face
WALL_ROWS = 8  # wall rows per half cell (the strata need them)
STRATA = 2  # rock bands per half cell (grooves between them)
GROOVE = 0.045  # metres the grooves cut into the rock face
GROOVE_SHADE = 0.45  # how much darker the bottom of a groove is (vertex colour)
EPS = 1e-3  # regions shrink this much so a grid point is never exactly on a boundary
GRASS = (0.13, 0.38, 0.07, 1.0)
ROCK = (0.33, 0.21, 0.13, 1.0)
SPACING = 2.0  # metres between tiles in the Blender layout
PER_ROW = 8

# Corner positions (x, y) seen from above, Blender axes: 0,1 near (-Y), 0,3 left (-X) (cube.py).
CORNERS_2D = ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))
ROTATE_Y = (1, 2, 3, 0, 5, 6, 7, 4)


# --- which tiles ------------------------------------------------------------------------------

def allowed(h):
    """Heightmap rule: every solid top corner has a solid corner below it; not empty, not full."""
    if h in (0, 255):
        return False
    return all(not ((h >> (4 + i)) & 1) or ((h >> i) & 1) for i in range(4))


def turn(h):
    out = 0
    for i in range(8):
        if (h >> i) & 1:
            out |= 1 << ROTATE_Y[i]
    return out


def representatives():
    """One hash per Y-turn group of the allowed hashes (the smallest)."""
    seen = set()
    reps = []
    for h in range(256):
        if not allowed(h) or h in seen:
            continue
        group = {h}
        g = h
        for _ in range(3):
            g = turn(g)
            group.add(g)
        seen |= group
        reps.append(min(group))
    return sorted(reps)


def bits_name(h):
    """"00001111": character 7 - i is corner i (Godot's mc_hash)."""
    return "".join("1" if (h >> i) & 1 else "0" for i in range(7, -1, -1))


# --- the shape --------------------------------------------------------------------------------

def blend(layer, x, y):
    """>= 0 inside the layer's region at (x, y) in -0.5 .. 0.5 (STYLE picks the shape)."""
    if STYLE == "blocky":
        return blocky(layer, x, y)
    return rounded(layer, x, y)


def blocky(layer, x, y):
    """Each solid corner owns the quarter of the cell next to it; the region is their union, so its
    edges run along the cell's middle lines (x 0 / y 0) and the only corner it can have is the cell
    centre. That centre corner is rounded (ROUNDING) only where it sticks out: a quarter with no
    solid quarter beside it (a single corner, or a diagonal pair). Two quarters side by side make a
    straight edge, three make a sharp inner corner. Along a cell side only the side's 2 corners
    matter (the rounding stays within ROUNDING of the centre), so neighbouring tiles meet."""
    best = -1.0
    for i, (solid, (cx, cy)) in enumerate(zip(layer, CORNERS_2D)):
        if not solid:
            continue
        a = x if cx < 0 else -x  # > 0: outside this quarter, across the middle line
        b = y if cy < 0 else -y
        alone = not layer[(i + 1) % 4] and not layer[(i + 3) % 4]
        if alone:
            qa, qb = a + ROUNDING, b + ROUNDING
            sdf = math.hypot(max(qa, 0.0), max(qb, 0.0)) + min(max(qa, qb), 0.0) - ROUNDING
        else:
            sdf = max(a, b)
        best = max(best, -sdf)
    # no EPS here: quarters meet exactly on the middle lines (an EPS would crack them open); N is
    # odd, so no grid point lies on a middle line or a cell side's middle
    return best


def rounded(layer, x, y):
    """Bilinear blend of a layer's 4 corners, minus a half."""
    u, v = x + 0.5, y + 0.5
    c0, c1, c2, c3 = layer
    value = c0 * (1 - u) * (1 - v) + c1 * u * (1 - v) + c2 * u * v + c3 * (1 - u) * v
    return value - 0.5 - EPS


def blend_gradient(layer, x, y):
    """Points into the solid (rounded: exact; blocky: central differences)."""
    if STYLE == "blocky":
        e = 1e-4
        return ((blend(layer, x + e, y) - blend(layer, x - e, y)) / (2 * e),
                (blend(layer, x, y + e) - blend(layer, x, y - e)) / (2 * e))
    u, v = x + 0.5, y + 0.5
    c0, c1, c2, c3 = layer
    return ((c1 - c0) * (1 - v) + (c2 - c3) * v, (c3 - c0) * (1 - u) + (c2 - c1) * u)


def clip(poly, values, keep_positive):
    """Clip a convex polygon (2D points + one linear field value each) to value >= 0 (or <= 0)."""
    out_pts, out_vals = [], []
    sign = 1.0 if keep_positive else -1.0
    count = len(poly)
    for i in range(count):
        a, b = poly[i], poly[(i + 1) % count]
        va, vb = values[i] * sign, values[(i + 1) % count] * sign
        if va >= 0:
            out_pts.append(a)
            out_vals.append(values[i])
        if (va >= 0) != (vb >= 0):
            t = va / (va - vb)
            out_pts.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
            out_vals.append(values[i] + (values[(i + 1) % count] - values[i]) * t)
    return out_pts, out_vals


def triangles():
    """The sub-grid as triangles of 2D points."""
    step = 1.0 / N
    for j in range(N):
        for i in range(N):
            x0, y0 = -0.5 + i * step, -0.5 + j * step
            x1, y1 = x0 + step, y0 + step
            yield ((x0, y0), (x1, y0), (x1, y1))
            yield ((x0, y0), (x1, y1), (x0, y1))


def contour(layer):
    """The region's boundary as 2D segments (a, b), each oriented with the solid side on its left."""
    segments = []
    for tri in triangles():
        values = [blend(layer, *p) for p in tri]
        crossings = []
        for i in range(3):
            a, b = tri[i], tri[(i + 1) % 3]
            va, vb = values[i], values[(i + 1) % 3]
            if (va >= 0) != (vb >= 0):
                t = va / (va - vb)
                crossings.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        if len(crossings) != 2:
            continue
        a, b = crossings
        mid = ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5)
        gx, gy = blend_gradient(layer, *mid)  # points into the solid
        # solid on the left of a -> b: cross(b - a, gradient) > 0
        if (b[0] - a[0]) * gy - (b[1] - a[1]) * gx < 0:
            a, b = b, a
        segments.append((a, b))
    return segments


def outward(layer, p):
    """The wall's horizontal outward normal at p; on a cell side, only its part along that side
    (the neighbour computes the same there)."""
    gx, gy = blend_gradient(layer, *p)
    nx, ny = -gx, -gy
    if abs(abs(p[0]) - 0.5) < 1e-4 and abs(ny) > 1e-6:
        nx = 0.0
    if abs(abs(p[1]) - 0.5) < 1e-4 and abs(nx) > 1e-6:
        ny = 0.0
    length = math.hypot(nx, ny)
    if length < 1e-9:  # a flat spot of the blend (blocky: two quarters' edges meeting): any side normal
        nx, ny = -gx, -gy
        length = math.hypot(nx, ny)
        if length < 1e-9:
            return (1.0, 0.0) if abs(p[0]) >= abs(p[1]) else (0.0, 1.0)
    return (nx / length, ny / length)


def groove(z):
    """How far the rock face is pushed in at height z (0 at z -0.5, 0, 0.5: tiles stack)."""
    return GROOVE * math.sin(2.0 * math.pi * STRATA * z) ** 2


def groove_slope(z):
    w = 2.0 * math.pi * STRATA
    return GROOVE * w * math.sin(2.0 * w * z)


def wall_point(c, n, z):
    """The rock face at contour point c (outward n) and height z: pushed in by the groove, with its
    normal tilted by the groove's slope, and its shade."""
    d = groove(z)
    point = (c[0] - n[0] * d, c[1] - n[1] * d, z)
    normal = (n[0], n[1], groove_slope(z))
    length = math.sqrt(normal[0] ** 2 + normal[1] ** 2 + normal[2] ** 2)
    shade = 1.0 - GROOVE_SHADE * d / GROOVE if GROOVE > 0 else 1.0
    return point, (normal[0] / length, normal[1] / length, normal[2] / length), shade


class Builder:
    """Faces per material with a normal per corner (loop)."""

    def __init__(self):
        self.verts = []
        self.faces = []  # (vertex indices, material index, loop normals, loop shades)
        self.index = {}

    def vert(self, p):
        key = (round(p[0], 5), round(p[1], 5), round(p[2], 5))
        if key not in self.index:
            self.index[key] = len(self.verts)
            self.verts.append(key)
        return self.index[key]

    def face(self, points, material, normals, shades=None):
        """A face; corners that land on the same vertex are merged, slivers under 3 corners dropped."""
        shades = shades or [1.0] * len(points)
        ids, kept, kept_shades = [], [], []
        for p, n, shade in zip(points, normals, shades):
            v = self.vert(p)
            if v not in ids:
                ids.append(v)
                kept.append(n)
                kept_shades.append(shade)
        if len(ids) >= 3:
            self.faces.append((ids, material, kept, kept_shades))


def build_tile(h):
    """Vertices + faces of the tile for hash h (materials: 0 grass, 1 rock)."""
    bottom = [(h >> i) & 1 for i in range(4)]
    top = [(h >> (4 + i)) & 1 for i in range(4)]
    out = Builder()
    up = (0.0, 0.0, 1.0)

    # Grass top at z 0: inside the bottom region, outside the top region.
    if any(bottom):
        for tri in triangles():
            pts = list(tri)
            vb = [blend(bottom, *p) for p in pts]
            pts, vb = clip(pts, vb, True)
            if len(pts) < 3:
                continue
            vt = [blend(top, *p) for p in pts]
            pts, _ = clip(pts, vt, False)
            if len(pts) < 3:
                continue
            ring = [(p[0], p[1], 0.0) for p in pts]
            out.face(ring, 0, [up] * len(ring))

    # Walls: (layer, z from, z to, grass lip on top?)
    for layer, z0, z1, lip in ((bottom, -0.5, 0.0, True), (top, 0.0, 0.5, False)):
        if not any(layer) or all(layer):
            continue
        rows = [z0 + (z1 - z0) * k / WALL_ROWS for k in range(WALL_ROWS + 1)]
        if lip:
            rows = [z for z in rows if z < z1 - LIP] + [z1 - LIP, z1]
        for a, b in contour(layer):
            na, nb = outward(layer, a), outward(layer, b)
            mid = ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5)
            # the lip only where the cliff ends here: not where the top region's wall carries on up
            topped = lip and any(top) and blend(top, *mid) > -0.01
            for k in range(len(rows) - 1):
                lo, hi = rows[k], rows[k + 1]
                material = 0 if lip and not topped and lo >= z1 - LIP - 1e-6 else 1
                # solid on the left of a -> b, so a, b, b-up, a-up faces outward
                corners = [wall_point(a, na, lo), wall_point(b, nb, lo), wall_point(b, nb, hi), wall_point(a, na, hi)]
                shades = [c[2] for c in corners] if material == 1 else None
                out.face([c[0] for c in corners], material, [c[1] for c in corners], shades)
    return out


# --- Blender ------------------------------------------------------------------------------------

COLOR_ATTRIBUTE = "Col"


def material(bpy, name, color, shaded):
    """Plain base colour, or (shaded) the mesh's colour attribute: the rock colour, darker in the
    grooves. (Grass is plain: Godot's importer only turned the colour attribute on for one material.)"""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    if bsdf and not shaded:
        bsdf.inputs["Base Color"].default_value = color
        bsdf.inputs["Roughness"].default_value = 0.9
    if bsdf and shaded:
        attribute = nodes.get("Color Attribute") or nodes.new("ShaderNodeVertexColor")
        attribute.layer_name = COLOR_ATTRIBUTE
        attribute.location = (-300, 300)
        mat.node_tree.links.new(attribute.outputs["Color"], bsdf.inputs["Base Color"])
        bsdf.inputs["Roughness"].default_value = 0.9
    mat.diffuse_color = color
    return mat


def make_object(bpy, name, tile, mats, location):
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata(tile.verts, [], [f[0] for f in tile.faces])
    for i, (_, mat_index, _, _) in enumerate(tile.faces):
        mesh.polygons[i].material_index = mat_index
    for mat in mats:
        mesh.materials.append(mat)
    loop_normals = []
    for poly in mesh.polygons:
        _, _, normals, _ = tile.faces[poly.index]
        loop_normals.extend(normals)
        poly.use_smooth = True
    mesh.update()
    mesh.normals_split_custom_set(loop_normals)
    colors = mesh.color_attributes.new(COLOR_ATTRIBUTE, "FLOAT_COLOR", "CORNER")
    loop = 0
    for poly in mesh.polygons:
        _, mat_index, _, shades = tile.faces[poly.index]
        base = GRASS if mat_index == 0 else ROCK
        for shade in shades:
            colors.data[loop].color = (base[0] * shade, base[1] * shade, base[2] * shade, 1.0)
            loop += 1
    obj = bpy.data.objects.new(name, mesh)
    obj.location = location
    return obj


def build_collection(bpy, group="a", row=0):
    """The tiles of the current STYLE in their own collection, named by hash plus, for a group other
    than a, the tag "_m<group>1" (Godot: tile group). `row` shifts the layout down (two styles)."""
    name = "CliffBlocky" if STYLE == "blocky" else "Cliff"
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(collection)
    for obj in list(collection.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    mats = [material(bpy, "Grass", GRASS, False), material(bpy, "Rock", ROCK, True)]
    reps = representatives()
    for n, h in enumerate(reps):
        location = ((n % PER_ROW) * SPACING, -(n // PER_ROW + row) * SPACING, 0.0)
        tile_name = bits_name(h) + ("" if group == "a" else "_m%s1" % group)
        collection.objects.link(make_object(bpy, tile_name, build_tile(h), mats, location))
    print("cliff_tiles (%s, group %s): %d tiles (one per Y-turn group of %d heightmap hashes)" % (
        STYLE, group, len(reps), sum(allowed(h) for h in range(256))))
    return collection


def use_style(style):
    global STYLE, N, ROUNDING, GROOVE
    if style == "square":  # blocky, no curves: sharp corners, flat rock faces
        style = "blocky"
        ROUNDING = 0.0
        GROOVE = 0.0
    STYLE = style
    N = 11 if style == "blocky" else 9


def main():
    import bpy

    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    blend_path = args[args.index("--blend") + 1] if "--blend" in args else ""
    glb_path = args[args.index("--glb") + 1] if "--glb" in args else ""
    style = args[args.index("--style") + 1] if "--style" in args else STYLE
    if blend_path:  # a fresh file: no default cube / camera / light
        bpy.ops.wm.read_factory_settings(use_empty=True)
    collections = []
    if style == "both":  # one tile set, two groups: rounded = a, blocky = b
        use_style("rounded")
        collections.append(build_collection(bpy, "a", 0))
        use_style("blocky")
        collections.append(build_collection(bpy, "b", (len(representatives()) + PER_ROW - 1) // PER_ROW + 1))
    else:
        use_style(style)
        collections.append(build_collection(bpy))
    if blend_path:
        bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    if glb_path:
        bpy.ops.object.select_all(action="DESELECT")
        for collection in collections:
            for obj in collection.objects:
                obj.select_set(True)
        bpy.ops.export_scene.gltf(filepath=glb_path, export_format="GLB", use_selection=True, export_yup=True)
        print("cliff_tiles: wrote", glb_path)


if __name__ == "__main__":
    main()
