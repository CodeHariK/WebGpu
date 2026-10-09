"""Everything around the cottage: the paw-print plaza, flagstone paths, the arched bridge, the heart
tree with its basket, the garden rows, the clothesline, yarn balls, stumps, ferns, grass shading.
Small repeated things are merged into one object per colour."""
import math
import random

import bmesh
from mathutils import Vector

from isle_kit import aim, bm_ball, bm_box, bm_cone, bm_torus, box, empty, merged, mesh_object, mtx
from isle_terrain import _ribbon, catmull_rom, side_of


def dist_to_path(p, path):
    p = Vector(p[:2])
    best = math.inf
    for a, b in zip(path, path[1:]):
        a, b = Vector(a[:2]), Vector(b[:2])
        ab = b - a
        t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-9)))
        best = min(best, (a + ab * t - p).length)
    return best


# --- ground ------------------------------------------------------------------------------------

def plaza(centre, radius, facing):
    """A round flagstone plaza: an outer ring of stones, an inner disc and a raised paw print
    whose toes point along `facing` (radians)."""
    cx, cy = centre
    rng = random.Random(11)

    def ring(bm):
        n = 14
        for k in range(n):
            a = math.tau * k / n
            r = radius - 0.28
            bm_cone(bm, 0.3, 0.3, 0.06, mtx((cx + r * math.cos(a), cy + r * math.sin(a), 0.03), (0, 0, a + rng.uniform(-0.2, 0.2)), (1.15, 0.85, 1)), 7)
    merged("Plaza.Ring", "path", ring, smooth=False)
    bm = bmesh.new()
    bm_cone(bm, radius, radius, 0.04, mtx((cx, cy, 0.02)), 32)
    mesh_object("Plaza.Base", bm, "groove")
    bm = bmesh.new()
    bm_cone(bm, radius - 0.6, radius - 0.6, 0.06, mtx((cx, cy, 0.03)), 32)
    mesh_object("Plaza.Inner", bm, "tile")

    def paw(bm, grow, z):
        fx, fy = math.cos(facing), math.sin(facing)
        sx, sy = -fy, fx
        pad = (cx - fx * 0.12, cy - fy * 0.12, z)
        bm_ball(bm, 0.3 + grow, mtx(pad, (0, 0, facing), (0.9, 1.2, 0.1)), 2)
        for along, across, r in ((0.32, -0.36, 0.11), (0.46, -0.13, 0.12), (0.46, 0.13, 0.12), (0.32, 0.36, 0.11)):
            at = (cx + fx * along + sx * across, cy + fy * along + sy * across, z)
            bm_ball(bm, r + grow, mtx(at, (0, 0, facing), (1.1, 0.9, 0.12)), 2)
    merged("Plaza.PawOutline", "groove", lambda bm: paw(bm, 0.04, 0.06))
    merged("Plaza.Paw", "path", lambda bm: paw(bm, 0.0, 0.075))


def path(name, points, isle, width=1.2, seed=0):
    """A sandy strip with flagstones laid along it (clipped to the island)."""
    line = [p for p in catmull_rom([Vector((*p, 0)) for p in points], 5) if isle.inside(p.x, p.y, 0.15)]
    bm = bmesh.new()
    sides = [side_of(line, i) for i in range(len(line))]
    _ribbon(bm, [Vector((p.x, p.y, 0.012)) for p in line], sides, [width / 2] * len(line))
    mesh_object(f"{name}.Sand", bm, "tile")
    rng = random.Random(seed)

    def stones(bm):
        walked, last = 0.0, line[0]
        for i, p in enumerate(line):
            walked += (p - last).length
            last = p
            if walked < 0.42:
                continue
            walked = 0.0
            for across in (-0.27, 0.27):
                at = p + sides[i] * (across + rng.uniform(-0.06, 0.06))
                r = rng.uniform(0.2, 0.28)
                bm_cone(bm, r, r * 0.95, 0.05, mtx((at.x, at.y, 0.03), (0, 0, rng.uniform(0, 3)), (1.1, 0.9, 1)), 6)
    merged(f"{name}.Stones", "path", stones, smooth=False)
    return line


def grass_shading(isle, keep_out, count=7, seed=5):
    """Darker painted-looking blobs on the grass."""
    rng = random.Random(seed)

    def blobs(bm):
        placed = 0
        while placed < count:
            x, y = rng.uniform(-5, 5), rng.uniform(-5, 5)
            r = rng.uniform(0.7, 1.2)
            if isle.inside(x, y, r + 0.2) and keep_out(x, y, r):
                bm_ball(bm, r, mtx((x, y, 0.004), (0, 0, rng.uniform(0, 3)), (1.3, 0.8, 0.012)), 2)
                placed += 1
    merged("Grass.Shade", "grass_dark", blobs)


def tufts(isle, keep_out, count=40, seed=6):
    rng = random.Random(seed)

    def build(bm):
        placed = 0
        while placed < count:
            x, y = rng.uniform(-5, 5), rng.uniform(-5, 5)
            if isle.inside(x, y, 0.3) and keep_out(x, y, 0.1):
                for k in range(3):
                    lean = (rng.uniform(-0.4, 0.4), rng.uniform(-0.4, 0.4), 0)
                    bm_cone(bm, 0.035, 0.0, 0.25, mtx((x + 0.05 * k, y, 0.1), lean), 4)
                placed += 1
    merged("Grass.Tufts", "sprout", build, smooth=False)


# --- bridge ------------------------------------------------------------------------------------

def bridge(centre, across, span=2.6, rise=0.45, deck_width=1.05, z0=0.04):
    """Wooden planks over an arch, with chunky dark-red rails. `across` is the direction it spans."""
    yaw = math.atan2(across.y, across.x)
    root = empty("Bridge", (centre.x, centre.y, z0), (0, 0, yaw))
    arc = lambda s: rise * (1 - (2 * s / span) ** 2)
    slope = lambda s: -8 * rise * s / span ** 2
    n = 12
    bm = bmesh.new()
    for k in range(n):
        s = -span / 2 + span * (k + 0.5) / n
        bm_box(bm, (span / n - 0.02, deck_width, 0.09), mtx((s, 0, arc(s)), (0, -math.atan(slope(s)), 0)))
    mesh_object("Bridge.Deck-col", bm, "wood", parent=root, bevel=0.015)
    bm = bmesh.new()
    for side in (-1, 1):
        y = side * (deck_width / 2 + 0.02)
        pts = [(-span / 2 + span * k / 8, y) for k in range(9)]
        for (s0, _), (s1, _) in zip(pts, pts[1:]):
            mid, rot, length = aim((s0, y, arc(s0) + 0.45), (s1, y, arc(s1) + 0.45))
            bm_box(bm, (0.13, 0.13, length + 0.06), mtx(mid, rot))
            mid, rot, length = aim((s0, y, arc(s0) + 0.05), (s1, y, arc(s1) + 0.05))
            bm_box(bm, (0.1, 0.1, length + 0.04), mtx(mid, rot))
        for s in (-span / 2, 0.0, span / 2):
            bm_box(bm, (0.15, 0.15, 0.55), mtx((s, y, arc(s) + 0.22)))
    mesh_object("Bridge.Rails", bm, "rail", parent=root, bevel=0.02)
    return root


# --- living things and clutter -----------------------------------------------------------------

def heart(bm, at, size, yaw):
    for s in (-1, 1):
        bm_ball(bm, size * 0.55, mtx(at, (0, 0, yaw)) @ mtx((s * size * 0.42, 0, size * 0.25), scale=(1, 0.7, 1)), 1)
    bm_cone(bm, size * 0.78, 0.0, size * 1.0, mtx(at, (0, 0, yaw)) @ mtx((0, 0, -size * 0.35), (math.pi, 0, 0), (1, 0.6, 1)), 8)


def tree(at, seed=2):
    x, y = at
    rng = random.Random(seed)
    bm = bmesh.new()
    bm_cone(bm, 0.36, 0.2, 2.6, mtx((x, y, 1.25), (0.05, -0.08, 0)), 10)
    for k in range(5):
        a = math.tau * k / 5 + 0.3
        bm_cone(bm, 0.16, 0.03, 0.8, mtx((x + 0.35 * math.cos(a), y + 0.35 * math.sin(a), 0.12), (math.pi / 2 - 0.25, 0, a + math.pi / 2)), 6)
    for a, tilt in ((0.5, 0.7), (2.6, 0.6), (4.3, 0.75)):
        bm_cone(bm, 0.11, 0.05, 1.2, mtx((x + 0.35 * math.cos(a), y + 0.35 * math.sin(a), 2.5), (tilt, 0, a + math.pi / 2)), 8)
    mesh_object("Tree.Trunk", bm, "trunk", smooth=True)
    clumps = [(0, 0, 3.2, 1.25), (1.0, -0.3, 2.9, 0.9), (-1.0, 0.2, 2.95, 0.95), (0.3, -0.95, 2.85, 0.85),
              (-0.5, -0.7, 3.5, 0.85), (0.6, 0.6, 3.7, 0.9), (-0.2, 0.2, 4.1, 0.85), (0.9, -0.6, 3.6, 0.7),
              (-1.1, -0.4, 3.4, 0.7), (0.1, 0.9, 3.0, 0.85)]
    for k in range(14):  # small clumps round the outside give a leafy outline
        a = math.tau * k / 14
        h = rng.uniform(2.6, 4.0)
        reach = 1.35 - abs(h - 3.2) * 0.45
        clumps.append((reach * math.cos(a), reach * math.sin(a), h, rng.uniform(0.38, 0.55)))
    dark, light, hearts = bmesh.new(), bmesh.new(), bmesh.new()
    for k, (cx, cy, cz, r) in enumerate(clumps):
        bm_ball(dark if k % 2 else light, r, mtx((x + cx, y + cy, cz), (0, 0, rng.uniform(0, 3)), (1.1, 1, 0.75)), 2)
    outer = clumps[10:]  # hearts go on the outside clumps, where they can be seen
    for k in range(16):
        cx, cy, cz, r = outer[k % len(outer)]
        a = rng.uniform(math.pi * 0.9, math.pi * 1.9)  # mostly on the camera side
        at = (x + cx + r * 1.05 * math.cos(a), y + cy + r * 1.05 * math.sin(a), cz + rng.uniform(-0.1, 0.2))
        heart(hearts, at, 0.22, a + math.pi / 2)
    mesh_object("Tree.Leaves", dark, "leaf", smooth=True)
    mesh_object("Tree.LeavesLight", light, "leaf_light", smooth=True)
    mesh_object("Tree.Hearts", hearts, "heart", smooth=True)


def basket(at):
    x, y = at
    bm = bmesh.new()
    bm_cone(bm, 0.32, 0.4, 0.28, mtx((x, y, 0.14)), 14)
    bm_torus(bm, 0.4, 0.04, mtx((x, y, 0.28)), 16, 5)
    mesh_object("Basket", bm, "basket", smooth=True)
    bm = bmesh.new()
    for k in range(5):
        a = math.tau * k / 5
        heart(bm, (x + 0.17 * math.cos(a), y + 0.17 * math.sin(a), 0.34), 0.14, a)
    mesh_object("Basket.Hearts", bm, "heart", smooth=True)


def garden(at, yaw_degrees):
    root = empty("Garden", (*at, 0), (0, 0, math.radians(yaw_degrees)))
    soil, sprouts, stakes = bmesh.new(), bmesh.new(), bmesh.new()
    rng = random.Random(8)
    for row in (-0.6, 0.0, 0.6):
        bm_box(soil, (2.2, 0.42, 0.14), mtx((0, row, 0.04)))
        for k in range(5):
            sx = -0.85 + 0.42 * k
            for lean in (-0.6, 0.6):
                bm_cone(sprouts, 0.07, 0.0, 0.28, mtx((sx, row, 0.22), (lean, 0, rng.uniform(0, 3)), (1, 0.4, 1)), 6)
            if k % 2 == 0:
                bm_cone(stakes, 0.025, 0.02, 0.6, mtx((sx + 0.1, row + 0.08, 0.3)), 6)
    for px, py in ((1.1, -1.0), (1.35, -1.15), (0.85, -1.2)):  # little paw prints in the dirt
        bm_ball(soil, 0.08, mtx((px, py, 0.0), scale=(1, 1, 0.1)), 1)
        for dx, dy in ((-0.07, 0.09), (0.0, 0.12), (0.07, 0.09)):
            bm_ball(soil, 0.035, mtx((px + dx, py + dy, 0.0), scale=(1, 1, 0.1)), 1)
    mesh_object("Garden.Soil", soil, "soil", smooth=True, parent=root, bevel=0.05)
    mesh_object("Garden.Sprouts", sprouts, "sprout", smooth=True, parent=root)
    mesh_object("Garden.Stakes", stakes, "stake", parent=root)


def clothesline(a, b):
    a, b = Vector((*a, 0)), Vector((*b, 0))
    bm = bmesh.new()
    for p, lean in ((a, 0.06), (b, -0.06)):
        bm_cone(bm, 0.06, 0.05, 1.2, mtx((p.x, p.y, 0.6), (lean, 0, 0)), 8)
    mid, rot, length = aim(a + Vector((0, 0, 1.12)), b + Vector((0, 0, 1.12)))
    bm_cone(bm, 0.012, 0.012, length, mtx(mid, rot), 4)
    mesh_object("Clothesline", bm, "stake", smooth=True)
    bm = bmesh.new()
    yaw = math.atan2((b - a).y, (b - a).x)
    for t, tilt in ((0.3, 0.05), (0.66, -0.08)):
        p = a.lerp(b, t)
        bm_box(bm, (0.42, 0.03, 0.4), mtx((p.x, p.y, 0.9), (tilt, 0, yaw)))
    mesh_object("Clothes", bm, "cloth")


def yarn(name, at, color, seed):
    x, y = at
    rng = random.Random(seed)
    bm = bmesh.new()
    bm_ball(bm, 0.22, mtx((x, y, 0.21)), 2)
    for k in range(4):
        bm_torus(bm, 0.215, 0.018, mtx((x, y, 0.21), (rng.uniform(0, 3), rng.uniform(0, 3), 0)), 20, 4)
    a, prev = rng.uniform(0, math.tau), Vector((x, y, 0.03))
    for k in range(5):  # a loose strand trailing on the ground
        a += rng.uniform(-0.8, 0.8)
        nxt = prev + Vector((math.cos(a), math.sin(a), 0)) * 0.14
        if k == 0:
            nxt = Vector((x + 0.2 * math.cos(a), y + 0.2 * math.sin(a), 0.03))
        mid, rot, length = aim(prev, nxt)
        bm_cone(bm, 0.02, 0.02, length + 0.02, mtx(mid, rot), 4)
        prev = nxt
    mesh_object(name, bm, color, smooth=True)


def stumps(points):
    wood, tops = bmesh.new(), bmesh.new()
    for x, y, h in points:
        bm_cone(wood, 0.17, 0.16, h, mtx((x, y, h / 2)), 10)
        bm_cone(tops, 0.15, 0.15, 0.02, mtx((x, y, h + 0.005)), 10)
    mesh_object("Stumps", wood, "stump", smooth=True)
    mesh_object("Stumps.Tops", tops, "stump_top")


def ferns(points, seed=9):
    rng = random.Random(seed)

    def build(bm):
        for x, y in points:
            for k in range(7):
                a = math.tau * k / 7 + rng.uniform(-0.2, 0.2)
                bm_cone(bm, 0.11, 0.0, 0.6, mtx((x + 0.2 * math.cos(a), y + 0.2 * math.sin(a), 0.2), (1.0, 0, a + math.pi / 2), (1, 0.3, 1)), 4)
    merged("Ferns", "fern", build, smooth=False)
