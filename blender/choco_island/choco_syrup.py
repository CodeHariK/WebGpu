"""Choco Island syrup: the chocolate syrup sea, the pond on top of block A, the main fall down its
front, the pool at the bottom, the stream across the island, the second fall off the edge into the
sea, foam rings, bubbles and cream swirls on the sea."""
import math
import random

import bmesh
from mathutils import Vector

from choco_kit import Part, ball, prism, ribbon, torus, use_collection
from choco_terrain import PLATFORMS, SEA_Z, island_radius

FALL_X = 1.4
POND = (FALL_X, 5.75, 1.25)  # x, y, radius, on top of block A
POOL = (FALL_X, 2.4, 2.0)
STREAM_MID = [(FALL_X, 2.4), (1.9, 0.0), (2.7, -3.0), (2.4, -6.0), (3.0, -9.0), (3.5, -12.0)]
END_ANGLE = math.atan2(-15.6, 3.7)


def stream_line():
    """The stream's centre line on the island top (x, y), from the pool to the island edge."""
    r = island_radius(END_ANGLE) * 0.99
    return STREAM_MID + [(math.cos(END_ANGLE) * r, math.sin(END_ANGLE) * r)]


def sea():
    use_collection("Syrup")
    p = Part("Syrup_Sea")
    rng = random.Random(5)
    bmesh.ops.create_grid(p.bm, x_segments=40, y_segments=40, size=72)
    for v in p.bm.verts:
        v.co.x += rng.uniform(-0.7, 0.7)
        v.co.y += rng.uniform(-0.7, 0.7)
        v.co.z = SEA_Z + rng.uniform(-0.09, 0.09)
    bmesh.ops.triangulate(p.bm, faces=p.bm.faces)
    for f in p.bm.faces:
        f.material_index = p.m("syrup")
    # Swirls of cream and lighter syrup drifting on the surface.
    for i in range(22):
        for _ in range(30):
            a, r = rng.uniform(0, math.tau), rng.uniform(19, 44)
            x, y = math.cos(a) * r, math.sin(a) * r
            if all(math.hypot(x - px, y - py) > pr + 3 for (px, py, _, pr, _) in PLATFORMS):
                break
        torus(p.bm, (x, y, SEA_Z + 0.05), rng.uniform(1.5, 4.5), 0.2, segs=10, ring_segs=4,
              arc=rng.uniform(1.6, 3.4), start=rng.uniform(0, math.tau), flatten=0.35,
              mat=p.m("cream" if i % 3 == 0 else "syrup_light"))
    p.done()


def falls_and_stream():
    use_collection("Syrup")
    p = Part("Syrup_Falls")
    syrup, light, foam = p.m("syrup_flow"), p.m("syrup_light"), p.m("foam")
    # Pond on block A (the source) and the pool at the foot of the fall.
    prism(p.bm, POND[:2], POND[2], 7.12, 7.27, sides=9, wobble=0.1, seed=31, mat=syrup)
    prism(p.bm, POOL[:2], POOL[2], -0.05, 0.1, sides=11, wobble=0.12, seed=32, mat=syrup)
    # Main fall: spills over A's front lip and drops to the pool.
    fall = [(5.3, 7.25), (4.6, 7.28), (4.15, 7.05), (3.95, 6.4), (3.85, 5.0), (3.8, 3.2), (3.72, 1.5),
            (3.5, 0.4), (3.2, 0.08)]
    pts = [(FALL_X, y, z) for (y, z) in fall]
    widths = [2.1 + 0.9 * i / (len(pts) - 1) for i in range(len(pts))]
    ribbon(p.bm, pts, widths, 0.26, mat=syrup, side=(-1, 0, 0), bulge=0.12)
    for dx in (-0.75, -0.1, 0.6):  # lighter streaks running down the front
        streak = [(FALL_X + dx, y - 0.16, z) for (y, z) in fall[3:-1]]
        ribbon(p.bm, streak, 0.22, 0.05, mat=light, side=(-1, 0, 0), bulge=0.02)
    # Foam ring and bubbles where the fall hits the pool.
    torus(p.bm, (FALL_X, 3.2, 0.12), 1.4, 0.16, segs=10, ring_segs=4, flatten=0.5, mat=foam)
    rng = random.Random(41)
    for _ in range(9):
        a, r = rng.uniform(0, math.tau), rng.uniform(0.4, 1.6)
        ball(p.bm, (FALL_X + math.cos(a) * r, 3.1 + math.sin(a) * r * 0.7, 0.12), rng.uniform(0.08, 0.2), mat=foam)
    # The stream across the island.
    line = stream_line()
    spts = [(x, y, 0.04) for (x, y) in line]
    sw = [2.4, 2.0, 1.8, 1.9, 1.8, 1.7, 1.7]
    ribbon(p.bm, spts, sw, 0.18, mat=syrup, hint=(0, 0, 1), bulge=0.04)
    # Second fall: off the island edge down into the sea.
    d = Vector((math.cos(END_ANGLE), math.sin(END_ANGLE), 0))
    re = island_radius(END_ANGLE)
    prof = [(-0.5, 0.04), (0.15, 0.0), (0.45, -0.4), (0.6, -1.2), (0.66, -2.2), (0.7, SEA_Z - 0.1)]
    side = (d.y, -d.x, 0)
    fpts = [d * (re + o) + Vector((0, 0, z)) for (o, z) in prof]
    ribbon(p.bm, fpts, [1.7, 1.8, 1.9, 2.0, 2.1, 2.2], 0.24, mat=syrup, side=side, bulge=0.1)
    base = d * (re + 0.9)
    torus(p.bm, (base.x, base.y, SEA_Z + 0.06), 1.1, 0.17, segs=10, ring_segs=4, flatten=0.5, mat=foam)
    for _ in range(8):
        a, r = rng.uniform(0, math.tau), rng.uniform(0.5, 1.8)
        ball(p.bm, (base.x + math.cos(a) * r, base.y + math.sin(a) * r, SEA_Z + 0.08), rng.uniform(0.08, 0.22), mat=foam)
    p.done()
