"""Rock pieces made by slicing, not displacing: a box cut by random planes.

Why: pushing vertices along their normals (Displace) folds the surface over itself wherever the
push is bigger than the local curvature allows — on corners and thin parts faces cross. Cutting
a convex shape with a plane always leaves a convex shape, so a box cut N times can never
self-intersect, keeps crisp flat facets (stylised, cartoon-friendly) and stays very low-poly.

A piece = 1–4 stacked layers (limestone ledges), each a box with its own random cuts:
  mostly near-vertical cuts  → column faces
  some upward-tilted cuts    → chamfered top edges
  a few downward-tilted cuts → undercuts below the ledges
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector


def _direction(rng):
    roll = rng.random()
    if roll < 0.55:
        z = rng.uniform(-0.15, 0.25)
    elif roll < 0.85:
        z = rng.uniform(0.35, 0.8)
    else:
        z = rng.uniform(-0.7, -0.35)
    a = rng.uniform(0, math.tau)
    flat = math.sqrt(1 - z * z)
    return Vector((flat * math.cos(a), flat * math.sin(a), z))


def _extent(size, n):
    """Half the box's thickness along direction n."""
    return 0.5 * (abs(n.x) * size[0] + abs(n.y) * size[1] + abs(n.z) * size[2])


def cut_box(size, rng, cuts):
    """A box `size` centred at the origin, sliced by `cuts` random planes (holes capped)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0, matrix=Matrix.Diagonal((*size, 1)))
    for _ in range(cuts):
        n = _direction(rng)
        depth = _extent(size, n) * rng.uniform(0.5, 0.9)
        result = bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                                        plane_co=n * depth, plane_no=n, clear_outer=True)
        cut_edges = [e for e in result["geom_cut"] if isinstance(e, bmesh.types.BMEdge)]
        if cut_edges:
            bmesh.ops.holes_fill(bm, edges=cut_edges, sides=0)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return bm


def layered_rock(size, rng, cuts=13):
    """A rock of 1–4 ledges stacked up its height; returns one bmesh (centred at the origin)."""
    w, d, h = size
    layers = 1 if h < 1.6 else rng.randint(2, 4)
    shares = [rng.uniform(0.7, 1.3) for _ in range(layers)]
    total = sum(shares)
    out = bmesh.new()
    z = -h / 2
    for share in shares:
        lh = h * share / total
        layer_size = (w * rng.uniform(0.82, 1.05), d * rng.uniform(0.82, 1.05), lh * 1.06)
        piece = cut_box(layer_size, rng, cuts)
        offset = Vector((rng.uniform(-0.12, 0.12) * w, rng.uniform(-0.12, 0.12) * d, z + lh / 2))
        tilt = Matrix.Rotation(math.radians(rng.uniform(-4, 4)), 4, "X") @ Matrix.Rotation(math.radians(rng.uniform(-4, 4)), 4, "Y")
        bmesh.ops.transform(piece, matrix=Matrix.Translation(offset) @ tilt, verts=piece.verts)
        mesh = bpy.data.meshes.new("_layer")
        piece.to_mesh(mesh)
        piece.free()
        out.from_mesh(mesh)
        bpy.data.meshes.remove(mesh)
        z += lh
    return out


def rock_object(name, centre, size, rot, seed, bevel=0.03):
    """A layered, plane-cut rock placed at centre/rot, with a thin bevel to catch the light."""
    rng = random.Random(seed)
    bm = layered_rock(size, rng)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    obj.location, obj.rotation_euler = centre, rot
    if bevel:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width, mod.segments, mod.limit_method = bevel, 1, "ANGLE"
    return obj
