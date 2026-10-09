"""Floating islands (a wobbly grass slab over a tapering dirt-and-rock cliff) and a stream that
runs across one in a carved channel and pours off its edge as a waterfall."""
import math
import random

import bmesh
from mathutils import Vector

from isle_kit import bm_ball, material, merged, mesh_object, mtx

SIDES = 48
GRASS_THICKNESS = 0.28
# cliff rings below the grass: (fraction of depth, radius scale)
CLIFF = [(0.08, 0.99), (0.3, 0.97), (0.55, 0.9), (0.75, 0.74), (0.9, 0.48), (0.97, 0.22)]
ROCK_FROM = 0.4  # bands deeper than this fraction are rock, above it dirt


def wobble(seed, amount):
    """A smooth random radius multiplier around the circle (a few sine waves)."""
    rng = random.Random(seed)
    waves = [(k, rng.uniform(0, math.tau), amount * rng.uniform(0.5, 1.0) / k ** 0.5) for k in (2, 3, 5, 7)]
    return lambda angle: 1 + sum(a * math.sin(k * angle + phase) for k, phase, a in waves)


class Isle:
    def __init__(self, name, center, radius, top, depth, seed):
        self.name, self.center, self.radius, self.top, self.depth = name, Vector(center), radius, top, depth
        self.seed = seed
        self.shape = wobble(seed, 0.12)

    def edge(self, angle):
        return self.radius * self.shape(angle)

    def inside(self, x, y, margin=0.0):
        d = Vector((x, y)) - self.center
        return d.length < self.edge(math.atan2(d.y, d.x)) - margin

    def ring_point(self, angle, scale, z, extra=1.0):
        r = self.edge(angle) * scale * extra
        return (self.center.x + r * math.cos(angle), self.center.y + r * math.sin(angle), z)


def _ring(bm, isle, z, scale, jitter=None):
    angles = [math.tau * i / SIDES for i in range(SIDES)]
    return [bm.verts.new(isle.ring_point(a, scale, z, jitter(a) if jitter else 1.0)) for a in angles]


def _skin(bm, upper, lower, material_index=0):
    faces = []
    for i in range(SIDES):
        j = (i + 1) % SIDES
        face = bm.faces.new((upper[i], upper[j], lower[j], lower[i]))
        face.material_index = material_index
        faces.append(face)
    return faces


TOP_RINGS = (0.9, 0.78, 0.64, 0.5, 0.36, 0.22, 0.1)  # inner rings, so vertex colours have room to vary


def _fill_top(bm, isle, outer):
    """Fill the top with concentric quad rings and a centre fan (instead of one big n-gon)."""
    rings = [outer] + [_ring(bm, isle, isle.top, scale) for scale in TOP_RINGS]
    for upper, inner in zip(rings, rings[1:]):
        _skin(bm, inner, upper)
    centre = bm.verts.new((isle.center.x, isle.center.y, isle.top))
    last = rings[-1]
    for i in range(SIDES):
        bm.faces.new((last[i], last[(i + 1) % SIDES], centre))


def grass_lip(isle, keep, seed=12):
    """Rounded grass tongues hanging over the cliff edge, like the overhang in the painting.
    keep(x, y) → False where the lip must stay open (the waterfall notch)."""
    rng = random.Random(seed)

    def build(bm):
        angle = 0.0
        while angle < math.tau:
            r = rng.uniform(0.16, 0.28)
            x, y, _ = isle.ring_point(angle, 1.02, 0)
            if keep(x, y):
                at = (x, y, isle.top - GRASS_THICKNESS + 0.02 - r * 0.3)
                bm_ball(bm, r, mtx(at, (0, 0, angle + math.pi / 2), (1.5, 0.55, 1.0)), 2)
            angle += rng.uniform(0.18, 0.34)
    return merged(f"{isle.name}.GrassLip", "grass", build)


def make_isle(isle):
    """Grass slab (named -col so Godot makes a collider) and the cliff underneath."""
    bm = bmesh.new()
    top = _ring(bm, isle, isle.top, 1.0)
    lip = _ring(bm, isle, isle.top - GRASS_THICKNESS, 1.03)
    _fill_top(bm, isle, top)
    _skin(bm, top, lip)
    bm.faces.new(list(reversed(lip)))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    grass = mesh_object(f"{isle.name}.Grass-col", bm, "grass")

    bm = bmesh.new()
    rings = [_ring(bm, isle, isle.top - GRASS_THICKNESS + 0.05, 0.99)]
    for k, (fraction, scale) in enumerate(CLIFF):
        jitter = wobble(isle.seed * 10 + k, 0.09)
        rings.append(_ring(bm, isle, isle.top - fraction * isle.depth, scale, jitter))
    for k in range(len(rings) - 1):
        _skin(bm, rings[k], rings[k + 1], 0 if CLIFF[k][0] < ROCK_FROM else 1)
    tip = bm.verts.new((isle.center.x + 0.1, isle.center.y - 0.1, isle.top - isle.depth))
    for i in range(SIDES):
        bm.faces.new((rings[-1][i], rings[-1][(i + 1) % SIDES], tip)).material_index = 1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh_object(f"{isle.name}.Cliff", bm, ["dirt", "rock"])
    return grass


# --- the stream --------------------------------------------------------------------------------

def catmull_rom(points, steps=6):
    pts = [Vector(p) for p in points]
    pts = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out = []
    for i in range(1, len(pts) - 2):
        p0, p1, p2, p3 = pts[i - 1:i + 3]
        for s in range(steps):
            t = s / steps
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(pts[-2])
    return out


def side_of(path, i):
    a, b = path[max(i - 1, 0)], path[min(i + 1, len(path) - 1)]
    t = (b - a).normalized()
    return Vector((-t.y, t.x, 0))


def _ribbon(bm, centres, sides, half_widths):
    rows = [(bm.verts.new(c - s * w), bm.verts.new(c + s * w)) for c, s, w in zip(centres, sides, half_widths)]
    for (a, b), (c, d) in zip(rows, rows[1:]):
        bm.faces.new((a, b, d, c))


def _prism(bm, path, half_width, z_top, z_bottom):
    """A closed slab following the path (the channel cutter)."""
    sides = [side_of(path, i) for i in range(len(path))]
    top = [(bm.verts.new((p.to_3d() - s * half_width) + Vector((0, 0, z_top))), bm.verts.new((p.to_3d() + s * half_width) + Vector((0, 0, z_top)))) for p, s in zip(path, sides)]
    bot = [(bm.verts.new((p.to_3d() - s * half_width) + Vector((0, 0, z_bottom))), bm.verts.new((p.to_3d() + s * half_width) + Vector((0, 0, z_bottom)))) for p, s in zip(path, sides)]
    for k in range(len(path) - 1):
        (a, b), (c, d) = top[k], top[k + 1]
        (e, f), (g, h) = bot[k], bot[k + 1]
        bm.faces.new((a, b, d, c))
        bm.faces.new((e, g, h, f))
        bm.faces.new((a, c, g, e))
        bm.faces.new((b, f, h, d))
    bm.faces.new((top[0][0], bot[0][0], bot[0][1], top[0][1]))
    bm.faces.new((top[-1][0], top[-1][1], bot[-1][1], bot[-1][0]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)  # the boolean needs a clean, outward-facing solid


def _cut(grass, cutter, name):
    mod = grass.modifiers.new(name, "BOOLEAN")
    mod.operation, mod.object, mod.solver = "DIFFERENCE", cutter, "EXACT"
    if hasattr(mod, "material_mode"):
        mod.material_mode = "TRANSFER"
    cutter.hide_render = True
    cutter.display_type = "WIRE"
    cutter.hide_set(True)  # hidden, but still evaluated (hide_viewport would switch the cut off)


def make_stream(isle, grass, points, width, pond_radius, helpers):
    """Carve a channel (and a spring pond if pond_radius) into `grass`, lay water in them, pour a waterfall off the edge.
    Returns the smoothed centre line on the island (for the bridge and keep-out tests)."""
    import isle_kit
    path = [p.to_2d() for p in catmull_rom([Vector((*p, 0)) for p in points])]
    first = next(i for i, p in enumerate(path) if isle.inside(p.x, p.y, 0.05))
    on_isle = []
    for p in path[first:]:
        if not isle.inside(p.x, p.y, 0.05):
            break
        on_isle.append(p)
    after = first + len(on_isle)
    edge = path[after] if after < len(path) else on_isle[-1]
    flow = (edge - on_isle[-1]).normalized()
    back = (on_isle[0] - on_isle[1]).normalized()
    enters = first > 0  # comes in over the back edge instead of rising from a spring
    channel_floor = isle.top - 0.2
    water_z = isle.top - 0.1

    target = isle_kit._target
    isle_kit.use_collection(helpers)
    bm = bmesh.new()
    lead = [on_isle[0] + back * 1.0] if enters else []
    _prism(bm, lead + on_isle + [edge + flow * 0.8], width / 2, isle.top + 0.2, channel_floor)
    channel = mesh_object("ChannelCutter", bm, "dirt")
    start = on_isle[0]
    pond_cutter = None
    if pond_radius:
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=pond_radius, radius2=pond_radius, depth=0.6,
                              matrix=mtx((start.x, start.y, channel_floor + 0.3)))
        pond_cutter = mesh_object("PondCutter", bm, "dirt")
    isle_kit._target = target
    _cut(grass, channel, "Channel")
    if pond_cutter:
        _cut(grass, pond_cutter, "Pond")

    bm = bmesh.new()
    inflow = [Vector((*(on_isle[0] + back * 0.25), water_z))] if enters else []
    centres = inflow + [Vector((p.x, p.y, water_z)) for p in on_isle] + [Vector((edge.x, edge.y, water_z - 0.02))]
    sides = [side_of(on_isle, 0)] * len(inflow) + [side_of(on_isle, i) for i in range(len(on_isle))] + [side_of(on_isle, len(on_isle) - 1)]
    _ribbon(bm, centres, sides, [width / 2 - 0.02] * len(centres))
    side = sides[-1]
    out = edge.to_3d()
    fall = [out + Vector((0, 0, water_z - 0.02)) + flow.to_3d() * 0.12,
            out + flow.to_3d() * 0.32 + Vector((0, 0, water_z - 0.3)),
            out + flow.to_3d() * 0.45 + Vector((0, 0, isle.top - 1.0)),
            out + flow.to_3d() * 0.55 + Vector((0, 0, isle.top - 2.2)),
            out + flow.to_3d() * 0.6 + Vector((0, 0, isle.top - isle.depth * 0.9))]
    _ribbon(bm, [centres[-1]] + fall, [side] * 6, [width / 2 - 0.02] + [width / 2 * f for f in (1.0, 1.05, 1.15, 1.3, 1.45)])
    if pond_radius:
        bmesh.ops.create_cone(bm, cap_ends=True, segments=24, radius1=pond_radius - 0.03, radius2=pond_radius - 0.03, depth=0.02,
                              matrix=mtx((start.x, start.y, water_z)))
    mesh_object("Water", bm, "water", smooth=True)

    def foam(bm):
        rng = random.Random(4)
        lip = out + flow.to_3d() * 0.15 + Vector((0, 0, water_z))
        for k in range(7):
            s = (k / 6 - 0.5) * width
            bm_ball(bm, rng.uniform(0.08, 0.14), mtx(lip + side * s + Vector((0, 0, 0.03)), scale=(1, 1, 0.6)), 1)
        low = out + flow.to_3d() * 0.6 + Vector((0, 0, isle.top - isle.depth * 0.9))
        for k in range(9):
            s = (k / 8 - 0.5) * width * 1.5
            bm_ball(bm, rng.uniform(0.12, 0.22), mtx(low + side * s, scale=(1, 1, 0.7)), 1)
    merged("Foam", "foam", foam)
    return on_isle
