"""Toxic Factory kit helpers: the palette (materials), collections and low-poly bmesh builders
(rounded boxes, prisms, cones, balls, cylinders, toruses, lathes). Copied from choco_island/choco_kit.py
with its own palette, plus emission for glowing toxins and lit windows, and lathe() for towers and tanks."""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

PALETTE = {  # sRGB, as it looks on screen
    "wall_blue": (0.42, 0.52, 0.66), "wall_teal": (0.32, 0.56, 0.56), "wall_purple": (0.56, 0.43, 0.68),
    "brick": (0.74, 0.36, 0.27), "concrete": (0.72, 0.71, 0.68), "concrete_dark": (0.5, 0.5, 0.5),
    "roof": (0.3, 0.32, 0.38), "trim": (0.95, 0.55, 0.2), "pipe": (0.96, 0.62, 0.24), "steel": (0.64, 0.67, 0.72),
    "red": (0.86, 0.22, 0.2), "white": (0.95, 0.95, 0.92), "window": (0.99, 0.86, 0.45), "glass": (0.26, 0.32, 0.42),
    "metal_dark": (0.25, 0.27, 0.3), "tyre": (0.12, 0.12, 0.13), "hazard": (0.98, 0.8, 0.15), "black": (0.1, 0.1, 0.1),
    "toxic": (0.45, 0.95, 0.2), "toxic_dark": (0.2, 0.55, 0.12), "cab_red": (0.86, 0.27, 0.22), "cab_blue": (0.27, 0.47, 0.82),
    "cone": (1.0, 0.45, 0.1), "dead_wood": (0.38, 0.3, 0.24), "smoke": (0.62, 0.62, 0.6), "rust": (0.62, 0.36, 0.2),
    "puff": (1.0, 1.0, 1.0),
}
GLOSSY = {"glass": 0.15, "steel": 0.35, "pipe": 0.4, "toxic": 0.2, "hazard": 0.5, "cab_red": 0.4, "cab_blue": 0.4}
GLOW = {"toxic": 1.5, "window": 0.6}  # emission strength (shows in Godot via the glb)

_target = None


def to_linear(rgb):
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb)


def material(name):
    """A plain Principled material in palette colour `name` (glossy for syrup, frosting, candy)."""
    key = f"M_{name}"
    mat = bpy.data.materials.get(key)
    if mat:
        return mat
    mat = bpy.data.materials.new(key)
    mat.use_nodes = True
    mat.use_backface_culling = False
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*to_linear(PALETTE[name]), 1)
    bsdf.inputs["Roughness"].default_value = GLOSSY.get(name, 0.75)
    if name in GLOW:
        bsdf.inputs["Emission Color"].default_value = (*to_linear(PALETTE[name]), 1)
        bsdf.inputs["Emission Strength"].default_value = GLOW[name]
    mat.diffuse_color = (*to_linear(PALETTE[name]), 1)
    return mat


def use_collection(name):
    """New objects go into collection `name`."""
    global _target
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    _target = coll
    return coll


def to_object(bm, name, mats, smooth=False):
    """Turn a bmesh into an object. Face material index i uses mats[i]."""
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for m in mats:
        mesh.materials.append(material(m))
    for poly in mesh.polygons:
        poly.use_smooth = smooth
    obj = bpy.data.objects.new(name, mesh)
    (_target or bpy.context.scene.collection).objects.link(obj)
    return obj


def transform(bm, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1), verts=None):
    m = Matrix.Translation(loc) @ Matrix.Rotation(rot[2], 4, "Z") @ Matrix.Rotation(rot[1], 4, "Y") @ Matrix.Rotation(rot[0], 4, "X") @ Matrix.Diagonal((*scale, 1))
    bmesh.ops.transform(bm, matrix=m, verts=verts if verts is not None else bm.verts)


def set_material(faces, index):
    for f in faces:
        f.material_index = index


# --- shapes (each adds into a bmesh and returns its new faces) -----------------------------------

def rounded_cube(bm, center, size, yaw=0.0, tilt=(0.0, 0.0), round_frac=0.22, segments=2, mat=0):
    """A box with bevelled (rounded) edges: the 'smooth cube' the mountain is built from. The rounding
    is a fraction of the smallest side, so thin slabs stay crisp and big blocks get soft."""
    sx, sy, sz = size if isinstance(size, (tuple, list)) else (size, size, size)
    tmp = bmesh.new()
    bmesh.ops.create_cube(tmp, size=1.0)
    bmesh.ops.scale(tmp, vec=(sx, sy, sz), verts=tmp.verts)
    bmesh.ops.bevel(tmp, geom=list(tmp.edges), offset=round_frac * min(sx, sy, sz), offset_type="OFFSET",
                    segments=segments, profile=0.5, affect="EDGES")
    transform(tmp, center, (tilt[0], tilt[1], yaw))
    return _merge(bm, tmp, mat)


def prism(bm, center, radius, z0, z1, sides=12, wobble=0.12, seed=0, mat=0, taper=1.0):
    """A wobbly n-sided prism from z0 to z1 (top radius = radius * taper)."""
    rng = random.Random(seed)
    jit = [1 + rng.uniform(-wobble, wobble) for _ in range(sides)]
    tmp = bmesh.new()
    bottom = [tmp.verts.new((center[0] + math.cos(i / sides * math.tau) * radius * jit[i], center[1] + math.sin(i / sides * math.tau) * radius * jit[i], z0)) for i in range(sides)]
    top = [tmp.verts.new((center[0] + math.cos(i / sides * math.tau) * radius * jit[i] * taper, center[1] + math.sin(i / sides * math.tau) * radius * jit[i] * taper, z1)) for i in range(sides)]
    tmp.faces.new(top)
    tmp.faces.new(list(reversed(bottom)))
    for i in range(sides):
        j = (i + 1) % sides
        tmp.faces.new((bottom[i], bottom[j], top[j], top[i]))
    return _merge(bm, tmp, mat)


def cone(bm, center, radius, z0, z1, sides=8, mat=0):
    """A cone from a ring at z0 to a point at z1 (drips, gumdrops, spikes)."""
    tmp = bmesh.new()
    ring = [tmp.verts.new((center[0] + math.cos(i / sides * math.tau) * radius, center[1] + math.sin(i / sides * math.tau) * radius, z0)) for i in range(sides)]
    tip = tmp.verts.new((center[0], center[1], z1))
    tmp.faces.new(list(reversed(ring)) if z1 > z0 else ring)
    for i in range(sides):
        j = (i + 1) % sides
        tmp.faces.new((ring[i], ring[j], tip) if z1 > z0 else (ring[j], ring[i], tip))
    return _merge(bm, tmp, mat)


def ball(bm, center, radius, scale=(1, 1, 1), subdiv=1, mat=0, rot=(0, 0, 0)):
    tmp = bmesh.new()
    bmesh.ops.create_icosphere(tmp, subdivisions=subdiv, radius=radius)
    transform(tmp, center, rot, scale)
    return _merge(bm, tmp, mat)


def cylinder(bm, a, b, radius, sides=6, mat=0):
    """A thin cylinder from point a to point b (sticks, canes)."""
    a, b = Vector(a), Vector(b)
    axis = b - a
    rot = axis.to_track_quat("Z", "Y").to_matrix().to_4x4()
    tmp = bmesh.new()
    bmesh.ops.create_cone(tmp, cap_ends=True, segments=sides, radius1=radius, radius2=radius, depth=axis.length)
    bmesh.ops.transform(tmp, matrix=Matrix.Translation((a + b) / 2) @ rot, verts=tmp.verts)
    return _merge(bm, tmp, mat)


def torus(bm, center, major, minor, segs=12, ring_segs=5, arc=math.tau, rot=(0, 0, 0), mat=0, flatten=1.0, start=0.0):
    """A torus (or part of one: arc < 2π) — candy cane hooks, syrup swirls."""
    tmp = bmesh.new()
    rings = []
    n = segs + (0 if arc >= math.tau - 1e-6 else 1)
    for i in range(n):
        u = start + i / segs * arc
        cu, su = math.cos(u), math.sin(u)
        rings.append([tmp.verts.new(((major + minor * math.cos(v)) * cu, (major + minor * math.cos(v)) * su, minor * math.sin(v) * flatten))
                      for v in (k / ring_segs * math.tau for k in range(ring_segs))])
    for i in range(segs if arc >= math.tau - 1e-6 else n - 1):
        r0, r1 = rings[i], rings[(i + 1) % n]
        for k in range(ring_segs):
            k1 = (k + 1) % ring_segs
            tmp.faces.new((r0[k], r1[k], r1[k1], r0[k1]))
    transform(tmp, center, rot)
    return _merge(bm, tmp, mat)


def _merge(bm, tmp, mat):
    """Copy tmp's geometry into bm (material index mat); returns the new faces."""
    vmap = {v: bm.verts.new(v.co) for v in tmp.verts}
    faces = []
    for f in tmp.faces:
        try:
            nf = bm.faces.new([vmap[v] for v in f.verts])
        except ValueError:
            continue
        nf.material_index = mat
        faces.append(nf)
    tmp.free()
    return faces


def ribbon(bm, pts, widths, thick, mat=0, side=None, hint=(0, 0, 1), bulge=0.0, caps=True):
    """A thick strip along pts (syrup streams and falls). Each cross-section is a flat pentagon with
    its middle pushed out by `bulge` on the 'front' (side x tangent), so the strip catches light in
    facets. `side` fixes the across direction (falls); otherwise it is tangent x hint (streams)."""
    pts = [Vector(p) for p in pts]
    hint = Vector(hint)
    rings = []
    for i, p in enumerate(pts):
        t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        s = Vector(side).normalized() if side is not None else t.cross(hint).normalized()
        n = s.cross(t).normalized()
        h = widths[i] / 2 if isinstance(widths, (list, tuple)) else widths / 2
        rings.append([bm.verts.new(p - s * h + n * thick / 2), bm.verts.new(p + n * (thick / 2 + bulge)),
                      bm.verts.new(p + s * h + n * thick / 2), bm.verts.new(p + s * h - n * thick / 2),
                      bm.verts.new(p - s * h - n * thick / 2)])
    faces = []
    for a, b in zip(rings, rings[1:]):
        for k in range(5):
            k1 = (k + 1) % 5
            faces.append(bm.faces.new((a[k], b[k], b[k1], a[k1])))
    if caps:
        faces.append(bm.faces.new(list(reversed(rings[0]))))
        faces.append(bm.faces.new(rings[-1]))
    for f in faces:
        f.material_index = mat
    return faces


class Part:
    """One output object: a bmesh plus its material list. m('syrup') gives the slot index."""

    def __init__(self, name):
        self.name, self.bm, self.mats = name, bmesh.new(), []

    def m(self, name):
        if name not in self.mats:
            self.mats.append(name)
        return self.mats.index(name)

    def done(self, smooth=False):
        return to_object(self.bm, self.name, self.mats, smooth)


def lathe(bm, center, profile, sides=12, mat=0, caps=True):
    """Spin a profile [(radius, z), ...] (bottom to top) round the Z axis at center (x, y): towers,
    tanks, chimneys. Caps close the bottom and top rings."""
    tmp = bmesh.new()
    rings = []
    for r, z in profile:
        rings.append([tmp.verts.new((center[0] + math.cos(i / sides * math.tau) * r, center[1] + math.sin(i / sides * math.tau) * r, z))
                      for i in range(sides)])
    for a, b in zip(rings, rings[1:]):
        for i in range(sides):
            j = (i + 1) % sides
            tmp.faces.new((a[i], a[j], b[j], b[i]))
    if caps:
        tmp.faces.new(list(reversed(rings[0])))
        tmp.faces.new(rings[-1])
    return _merge(bm, tmp, mat)


def box(bm, center, size, yaw=0.0, mat=0, round_frac=0.08):
    """A slightly rounded box (toy look) given its centre and size (x, y, z)."""
    return rounded_cube(bm, center, size, yaw=yaw, round_frac=round_frac, segments=1, mat=mat)
