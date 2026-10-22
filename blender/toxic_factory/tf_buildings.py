"""Toxic Factory buildings: the main plant (sawtooth roof), the brick annex, the loading dock, striped
chimneys, the cooling tower and storage tanks. Each is one kit piece at the origin, base on the
ground, front facing -Y (Godot +Z). Chunky rounded boxes so it reads as a toy factory."""
import math

import bmesh

from tf_kit import Part, box, cone, cylinder, lathe, torus

CHIMNEY_TOP = 16.3  # Godot puts the smoke here (also in toxic_layout.gd)
TOWER_TOP = 11.0
TANK_TOP = 6.3


def wedge(bm, x0, x1, y0, y1, z0, z1, mat=0):
    """A ramp: a box from (x0, y0, z0) to (x1, y1, z1) whose top slopes down from back (y1) to
    front (y0), leaving a vertical face at the back. Sawtooth roof teeth and gable halves."""
    tmp = bmesh.new()
    v = [tmp.verts.new(p) for p in ((x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x1, y1, z1), (x0, y1, z1))]
    for f in ((v[0], v[3], v[2], v[1]), (v[0], v[1], v[4], v[5]), (v[2], v[3], v[5], v[4]), (v[1], v[2], v[4]), (v[0], v[5], v[3])):
        tmp.faces.new(f)
    from tf_kit import _merge
    return _merge(bm, tmp, mat)


def _windows(p, y, xs, z, size=(1.4, 2.0), lit=True):
    """A row of windows on a front/back face at depth y (frames + glass)."""
    for x in xs:
        box(p.bm, (x, y, z), (size[0] + 0.3, 0.16, size[1] + 0.3), mat=p.m("trim"), round_frac=0.2)
        box(p.bm, (x, y - 0.05 * (1 if y < 0 else -1), z), (size[0], 0.16, size[1]), mat=p.m("window" if lit else "glass"), round_frac=0.1)


def _side_windows(p, x, ys, z, size=(1.4, 2.0)):
    for y in ys:
        box(p.bm, (x, y, z), (0.16, size[0] + 0.3, size[1] + 0.3), mat=p.m("trim"), round_frac=0.2)
        box(p.bm, (x + 0.05 * (1 if x > 0 else -1), y, z), (0.16, size[0], size[1]), mat=p.m("window"), round_frac=0.1)


def _rollup_door(p, x, y, z0, w, h):
    """A roll-up door: horizontal slats in two greys, a hazard strip at the bottom."""
    n = 8
    for i in range(n):
        box(p.bm, (x, y, z0 + 0.35 + (h - 0.35) * (i + 0.5) / n), (w, 0.18, (h - 0.35) / n * 0.9),
            mat=p.m("steel" if i % 2 else "metal_dark"), round_frac=0.2)
    for i in range(6):
        box(p.bm, (x - w / 2 + w * (i + 0.5) / 6, y, z0 + 0.17), (w / 6, 0.2, 0.32), mat=p.m("hazard" if i % 2 else "black"), round_frac=0.1)


def plant_main():
    p = Part("Plant_main")
    w, d, h = 14.0, 10.0, 7.0
    box(p.bm, (0, 0, 0.3), (w + 0.4, d + 0.4, 0.6), mat=p.m("concrete_dark"))
    box(p.bm, (0, 0, h / 2), (w, d, h), mat=p.m("wall_blue"), round_frac=0.04)
    box(p.bm, (0, 0, h), (w + 0.3, d + 0.3, 0.4), mat=p.m("trim"))
    teeth = 3
    for i in range(teeth):  # sawtooth roof: glazed vertical face toward the back
        y0 = -d / 2 + d * i / teeth
        y1 = y0 + d / teeth
        wedge(p.bm, -w / 2, w / 2, y0, y1, h + 0.2, h + 2.4, mat=p.m("roof"))
        box(p.bm, (0, y1 + 0.02, h + 1.25), (w - 0.6, 0.1, 1.6), mat=p.m("glass"), round_frac=0.05)
    _windows(p, -d / 2 - 0.05, [-5.3, -2.65, 2.65, 5.3], 4.9, (1.6, 1.8))
    _windows(p, -d / 2 - 0.05, [-5.3, 5.3], 2.2, (1.6, 1.6))
    _rollup_door(p, 0, -d / 2 - 0.08, 0.6, 4.0, 3.6)
    _side_windows(p, -w / 2 - 0.05, [-2.8, 0, 2.8], 4.6)
    _side_windows(p, w / 2 + 0.05, [-2.8, 0, 2.8], 4.6)
    for x in (-4.5, 0.0, 4.5):  # roof vents
        cylinder(p.bm, (x, 3.6, h + 2.2), (x, 3.6, h + 3.4), 0.45, sides=8, mat=p.m("steel"))
        cone(p.bm, (x, 3.6), 0.7, h + 3.4, h + 3.9, sides=8, mat=p.m("metal_dark"))
    p.done()


def plant_annex():
    p = Part("Plant_annex")
    w, d, h = 8.0, 6.0, 5.0
    box(p.bm, (0, 0, 0.25), (w + 0.4, d + 0.4, 0.5), mat=p.m("concrete_dark"))
    box(p.bm, (0, 0, h / 2), (w, d, h), mat=p.m("brick"), round_frac=0.04)
    for y0, y1 in ((-d / 2 - 0.3, 0), (d / 2 + 0.3, 0)):  # gable roof: two ramps meeting at the ridge
        if y0 < 0:
            wedge(p.bm, -w / 2 - 0.3, w / 2 + 0.3, y0, y1, h, h + 2.2, mat=p.m("roof"))
        else:
            tmp = bmesh.new()
            wedge(tmp, -w / 2 - 0.3, w / 2 + 0.3, -d / 2 - 0.3, 0, h, h + 2.2, mat=0)
            bmesh.ops.scale(tmp, vec=(1, -1, 1), verts=tmp.verts)
            bmesh.ops.reverse_faces(tmp, faces=tmp.faces)
            from tf_kit import _merge
            _merge(p.bm, tmp, p.m("roof"))
    box(p.bm, (0, 0, h + 0.1), (w + 0.2, d + 0.2, 0.3), mat=p.m("trim"))
    _windows(p, -d / 2 - 0.05, [-2.6, 0, 2.6], 3.3, (1.3, 1.6))
    box(p.bm, (2.6, -d / 2 - 0.08, 1.1), (1.2, 0.18, 2.2), mat=p.m("metal_dark"))
    cylinder(p.bm, (-3.0, 1.5, h + 1.2), (-3.0, 1.5, h + 4.0), 0.35, sides=8, mat=p.m("steel"))
    p.done()


def dock():
    p = Part("Dock")
    w, d, h = 12.0, 7.0, 5.0
    box(p.bm, (0, 0, h / 2), (w, d, h), mat=p.m("wall_teal"), round_frac=0.04)
    box(p.bm, (0, 0, h + 0.15), (w + 0.3, d + 0.3, 0.3), mat=p.m("trim"))
    box(p.bm, (0, 0, h + 0.4), (w - 0.4, d - 0.4, 0.3), mat=p.m("roof"))
    box(p.bm, (0, -d / 2 - 1.0, 1.2 / 2), (w, 2.0, 1.2), mat=p.m("concrete"))  # loading platform
    for x in (-4.0, 0.0, 4.0):
        _rollup_door(p, x, -d / 2 - 0.08, 1.2, 3.0, 3.0)
        box(p.bm, (x - 1.7, -d / 2 - 2.05, 0.75), (0.3, 0.2, 0.6), mat=p.m("black"))  # bumpers
        box(p.bm, (x + 1.7, -d / 2 - 2.05, 0.75), (0.3, 0.2, 0.6), mat=p.m("black"))
    box(p.bm, (0, -d / 2 - 1.3, h - 0.2), (w + 0.4, 2.8, 0.25), mat=p.m("trim"))  # canopy
    p.done()


def chimney():
    p = Part("Chimney")
    box(p.bm, (0, 0, 1.0), (3.2, 3.2, 2.0), mat=p.m("concrete_dark"))
    bands = 7
    z0, z1 = 2.0, 15.6
    for i in range(bands):
        a = z0 + (z1 - z0) * i / bands
        b = z0 + (z1 - z0) * (i + 1) / bands
        ra = 1.35 - 0.35 * i / bands
        rb = 1.35 - 0.35 * (i + 1) / bands
        lathe(p.bm, (0, 0), [(ra, a), (rb, b)], sides=12, mat=p.m("red" if i % 2 == 0 else "white"), caps=False)
    torus(p.bm, (0, 0, z1 + 0.3), 1.05, 0.28, segs=12, ring_segs=5, mat=p.m("metal_dark"))
    lathe(p.bm, (0, 0), [(1.0, z1), (1.0, z1 + 0.5)], sides=12, mat=p.m("metal_dark"), caps=False)
    lathe(p.bm, (0, 0), [(0.95, z1 + 0.1), (0.0, z1 + 0.11)], sides=12, mat=p.m("black"), caps=False)
    for i in range(14):  # ladder rungs up the front, following the taper
        z = 2.5 + i * 0.95
        y = -(1.35 - 0.35 * (z - z0) / (z1 - z0) + 0.12)
        cylinder(p.bm, (-0.3, y, z), (0.3, y, z), 0.05, sides=4, mat=p.m("metal_dark"))
    p.done()


def cooling_tower():
    p = Part("CoolingTower")
    prof = []
    for i in range(9):
        z = TOWER_TOP * i / 8
        r = 3.0 + 1.4 * ((z - 7.0) / 7.0) ** 2 if z < 7.0 else 3.0 + 0.4 * ((z - 7.0) / 4.0) ** 2
        prof.append((r, z))
    lathe(p.bm, (0, 0), prof, sides=16, mat=p.m("concrete"), caps=False)
    lathe(p.bm, (0, 0), [(3.27, 9.2), (3.33, 9.9)], sides=16, mat=p.m("trim"), caps=False)
    lathe(p.bm, (0, 0), [(3.35, 10.5), (0.0, 10.51)], sides=16, mat=p.m("black"), caps=False)
    for i in range(8):  # legs at the base
        a = i / 8 * math.tau
        cylinder(p.bm, (math.cos(a) * 4.4, math.sin(a) * 4.4, 0), (math.cos(a) * 4.2, math.sin(a) * 4.2, 1.0), 0.25, sides=5,
                 mat=p.m("concrete_dark"))
    p.done()


def tank(name, band):
    p = Part(name)
    box(p.bm, (0, 0, 0.2), (5.0, 5.0, 0.4), mat=p.m("concrete_dark"))
    lathe(p.bm, (0, 0), [(2.2, 0.4), (2.2, 5.0), (1.9, 5.7), (1.1, 6.15), (0.0, TANK_TOP)], sides=14, mat=p.m("steel"), caps=False)
    lathe(p.bm, (0, 0), [(2.24, 3.6), (2.24, 4.2)], sides=14, mat=p.m(band), caps=False)
    box(p.bm, (0, -2.2, 2.4), (0.6, 0.2, 3.0), mat=p.m("toxic"), round_frac=0.3)  # glowing level window
    for x in (-0.45, 0.45):  # ladder
        cylinder(p.bm, (x + 1.3, -1.85, 0.4), (x + 1.3, -1.85, 5.2), 0.05, sides=4, mat=p.m("metal_dark"))
    for i in range(9):
        cylinder(p.bm, (0.85, -1.85, 0.8 + i * 0.5), (1.75, -1.85, 0.8 + i * 0.5), 0.04, sides=4, mat=p.m("metal_dark"))
    p.done()


def build_all():
    plant_main()
    plant_annex()
    dock()
    chimney()
    cooling_tower()
    tank("Tank_hazard", "hazard")
    tank("Tank_toxic", "toxic_dark")
