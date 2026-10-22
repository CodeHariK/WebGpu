"""Toxic Factory props: trucks (tanker, box), barricades (jersey barriers, gate post + arm, cones,
sawhorses, fences, hazard signs), barrels, dead trees, rocks, pipe parts (flange, support, valve,
outfall) and the smoke puff. All at the origin, base on the ground, front facing -Y (Godot +Z)."""
import math
import random

import bmesh

from tf_kit import Part, _merge, ball, box, cone, cylinder, lathe, prism, torus


def _wheel(p, x, y, r=0.52):
    cylinder(p.bm, (x - 0.22, y, r), (x + 0.22, y, r), r, sides=10, mat=p.m("tyre"))
    side = 1 if x > 0 else -1
    cylinder(p.bm, (x + side * 0.2, y, r), (x + side * 0.25, y, r), r * 0.5, sides=8, mat=p.m("steel"))


def _cab(p, colour):
    """The front of both trucks: cab, windscreen, grille, bumper, headlights, front wheels."""
    box(p.bm, (0, -3.0, 1.0), (2.0, 8.6, 0.5), mat=p.m("metal_dark"))  # chassis
    box(p.bm, (0, -5.0, 2.1), (2.5, 2.4, 2.6), mat=p.m(colour), round_frac=0.18)
    box(p.bm, (0, -6.0, 1.4), (2.4, 0.9, 1.2), mat=p.m(colour), round_frac=0.2)  # nose
    box(p.bm, (0, -6.22, 2.85), (2.1, 0.12, 1.0), mat=p.m("glass"), round_frac=0.2)  # windscreen
    for x in (-1.26, 1.26):
        box(p.bm, (x, -5.0, 2.8), (0.1, 1.4, 0.9), mat=p.m("glass"), round_frac=0.2)
    box(p.bm, (0, -6.5, 1.25), (1.4, 0.1, 0.7), mat=p.m("steel"), round_frac=0.2)  # grille
    box(p.bm, (0, -6.55, 0.7), (2.5, 0.3, 0.35), mat=p.m("metal_dark"), round_frac=0.3)  # bumper
    for x in (-0.95, 0.95):
        box(p.bm, (x, -6.5, 1.5), (0.35, 0.1, 0.3), mat=p.m("window"), round_frac=0.3)
        cylinder(p.bm, (x * 1.15, -4.2, 3.3), (x * 1.15, -4.2, 4.3), 0.1, sides=6, mat=p.m("steel"))  # exhaust stacks
    box(p.bm, (0, -5.0, 3.45), (1.6, 1.2, 0.2), mat=p.m("hazard"), round_frac=0.3)  # roof light bar
    for x in (-1.05, 1.05):
        _wheel(p, x, -5.4)


def truck_tanker():
    p = Part("Truck_tanker")
    _cab(p, "cab_red")
    cylinder(p.bm, (0, -3.4, 2.45), (0, 2.6, 2.45), 1.25, sides=12, mat=p.m("steel"))
    for y in (-3.4, 2.6):
        ball(p.bm, (0, y, 2.45), 1.25, (1, 0.35, 1), subdiv=2, mat=p.m("steel"))
    for y in (-1.7, 0.9):  # hazard bands
        cylinder(p.bm, (0, y - 0.25, 2.45), (0, y + 0.25, 2.45), 1.29, sides=12, mat=p.m("hazard"))
    box(p.bm, (0, -0.4, 3.75), (0.8, 4.8, 0.15), mat=p.m("metal_dark"))  # walkway
    cylinder(p.bm, (0, 2.0, 3.6), (0, 2.0, 3.95), 0.35, sides=8, mat=p.m("metal_dark"))  # hatch
    ball(p.bm, (0, 3.05, 1.6), 0.25, mat=p.m("toxic"))  # leaking drip at the back
    for x in (-1.05, 1.05):
        for y in (0.4, 1.6):
            _wheel(p, x, y)
    p.done()


def truck_box():
    p = Part("Truck_box")
    _cab(p, "cab_blue")
    box(p.bm, (0, -0.6, 2.75), (2.6, 5.8, 3.0), mat=p.m("white"), round_frac=0.06)
    for i in range(8):  # hazard stripe along both sides
        y = -3.3 + 5.4 * (i + 0.5) / 8
        box(p.bm, (0, y, 1.55), (2.66, 5.4 / 8, 0.4), mat=p.m("hazard" if i % 2 else "black"), round_frac=0.05)
    for x in (-1.32, 1.32):  # a big glowing drum logo
        cylinder(p.bm, (x, -0.6, 2.9), (x * 1.02, -0.6, 2.9), 0.8, sides=10, mat=p.m("toxic"))
    for x in (-1.05, 1.05):
        for y in (0.6, 1.8):
            _wheel(p, x, y)
    p.done()


def barrels():
    for name, body, band in (("Barrel_hazard", "hazard", "black"), ("Barrel_toxic", "toxic_dark", "black"),
                             ("Barrel_rust", "rust", "metal_dark")):
        p = Part(name)
        lathe(p.bm, (0, 0), [(0.42, 0.0), (0.45, 0.08), (0.45, 1.12), (0.42, 1.2), (0.0, 1.2)], sides=10, mat=p.m(body), caps=True)
        for z in (0.38, 0.82):
            torus(p.bm, (0, 0, z), 0.46, 0.04, segs=10, ring_segs=4, mat=p.m(band))
        if body != "rust":
            box(p.bm, (0, -0.46, 0.6), (0.36, 0.05, 0.32), mat=p.m("black" if body == "hazard" else "toxic"), round_frac=0.3)
        p.done()
    p = Part("Spill")
    prism(p.bm, (0, 0), 1.0, 0.0, 0.04, sides=9, wobble=0.3, seed=4, mat=p.m("toxic"))
    p.done()


def jersey():
    """Concrete road barriers, 3 m long, red and white: alternate them and the line is striped."""
    for name, colour in (("Jersey_red", "red"), ("Jersey_white", "white")):
        p = Part(name)
        prof = [(-0.3, 0.0), (0.3, 0.0), (0.3, 0.12), (0.12, 0.35), (0.1, 0.85), (-0.1, 0.85), (-0.12, 0.35), (-0.3, 0.12)]
        tmp = bmesh.new()
        a = [tmp.verts.new((-1.48, y, z)) for (y, z) in prof]
        b = [tmp.verts.new((1.48, y, z)) for (y, z) in prof]
        n = len(prof)
        tmp.faces.new(list(reversed(a)))
        tmp.faces.new(b)
        for i in range(n):
            j = (i + 1) % n
            tmp.faces.new((a[i], a[j], b[j], b[i]))
        _merge(p.bm, tmp, p.m(colour))
        p.done()


def gate():
    p = Part("GatePost")
    box(p.bm, (0, 0, 0.6), (0.5, 0.5, 1.2), mat=p.m("red"), round_frac=0.15)
    box(p.bm, (0, 0, 1.25), (0.6, 0.6, 0.1), mat=p.m("metal_dark"))
    ball(p.bm, (0, 0, 1.45), 0.15, mat=p.m("cone"))  # warning lamp
    p.done()
    p = Part("GateArm")  # pivot at the origin, arm along +X (Godot raises it by turning around Z)
    for i in range(8):
        box(p.bm, (0.3 + 0.75 * (i + 0.5), 0, 0), (0.75, 0.12, 0.16), mat=p.m("red" if i % 2 == 0 else "white"), round_frac=0.2)
    box(p.bm, (0, 0, 0), (0.5, 0.2, 0.3), mat=p.m("metal_dark"), round_frac=0.2)
    p.done()


def cone_and_sawhorse():
    p = Part("Cone")
    box(p.bm, (0, 0, 0.04), (0.6, 0.6, 0.08), mat=p.m("black"), round_frac=0.2)
    lathe(p.bm, (0, 0), [(0.26, 0.08), (0.18, 0.35)], sides=8, mat=p.m("cone"), caps=False)
    lathe(p.bm, (0, 0), [(0.18, 0.35), (0.12, 0.5)], sides=8, mat=p.m("white"), caps=False)
    lathe(p.bm, (0, 0), [(0.12, 0.5), (0.02, 0.8)], sides=8, mat=p.m("cone"), caps=False)
    p.done()
    p = Part("Sawhorse")  # 2 m striped board on two A-frame legs
    for x in (-0.8, 0.8):
        for s in (-1, 1):
            cylinder(p.bm, (x, s * 0.35, 0.0), (x, 0.0, 1.0), 0.05, sides=4, mat=p.m("white"))
    for i in range(8):
        box(p.bm, (-1.0 + 0.25 * (i + 0.5), -0.05, 0.85), (0.25, 0.06, 0.28), mat=p.m("cone" if i % 2 == 0 else "white"), round_frac=0.1)
    ball(p.bm, (-0.8, -0.05, 1.08), 0.09, mat=p.m("cone"))
    p.done()


def fence():
    """A 4 m chain-link fence bay (posts at both ends, rails, a dark mesh panel, barbed wire)."""
    p = Part("Fence")
    for x in (-2.0, 2.0):
        cylinder(p.bm, (x, 0, 0), (x, 0, 2.3), 0.07, sides=6, mat=p.m("steel"))
    for z in (0.15, 1.15, 2.2):
        cylinder(p.bm, (-2.0, 0, z), (2.0, 0, z), 0.04, sides=4, mat=p.m("steel"))
    box(p.bm, (0, 0, 1.15), (4.0, 0.02, 2.0), mat=p.m("metal_dark"), round_frac=0.0)
    for i in range(8):  # barbed coil on top
        torus(p.bm, (-1.75 + i * 0.5, 0, 2.45), 0.2, 0.025, segs=6, ring_segs=3, rot=(0, math.pi / 2, 0), mat=p.m("steel"))
    p.done()


def hazard_sign():
    p = Part("HazardSign")
    cylinder(p.bm, (0, 0, 0), (0, 0, 2.2), 0.06, sides=6, mat=p.m("steel"))
    tmp = bmesh.new()
    pts = [(-0.6, 1.8), (0.6, 1.8), (0.0, 2.85)]
    a = [tmp.verts.new((x, -0.08, z)) for x, z in pts]
    b = [tmp.verts.new((x, -0.02, z)) for x, z in pts]
    tmp.faces.new(a)
    tmp.faces.new(list(reversed(b)))
    for i in range(3):
        j = (i + 1) % 3
        tmp.faces.new((a[j], a[i], b[i], b[j]))
    _merge(p.bm, tmp, p.m("black"))
    tmp = bmesh.new()
    a = [tmp.verts.new((x * 0.78, -0.1, 1.8 + (z - 1.8) * 0.78 + 0.08)) for x, z in pts]
    tmp.faces.new(a)
    _merge(p.bm, tmp, p.m("hazard"))
    ball(p.bm, (0, -0.12, 2.12), 0.13, (1, 0.3, 1), mat=p.m("black"))  # skull-ish blob
    p.done()


def dead_trees():
    for name, seed in (("DeadTree_a", 1), ("DeadTree_b", 2)):
        rng = random.Random(seed)
        p = Part(name)
        top = (0, 0, 3.2)
        cylinder(p.bm, (0, 0, 0), top, 0.22, sides=6, mat=p.m("dead_wood"))
        for i in range(5):
            a = i / 5 * math.tau + rng.uniform(-0.4, 0.4)
            z = rng.uniform(1.6, 3.0)
            end = (math.cos(a) * rng.uniform(1.0, 1.8), math.sin(a) * rng.uniform(1.0, 1.8), z + rng.uniform(0.6, 1.4))
            cylinder(p.bm, (0, 0, z), end, 0.09, sides=5, mat=p.m("dead_wood"))
            twig = (end[0] * 1.3, end[1] * 1.3 + rng.uniform(-0.4, 0.4), end[2] + 0.5)
            cylinder(p.bm, end, twig, 0.05, sides=4, mat=p.m("dead_wood"))
        p.done()
    p = Part("Rock")
    ball(p.bm, (0, 0, 0.3), 0.6, (1.3, 1.0, 0.7), subdiv=1, mat=p.m("concrete_dark"))
    p.done()


def pipe_parts():
    p = Part("Flange")  # ring round a pipe running along Y (Godot aligns it to the pipe)
    torus(p.bm, (0, 0, 0), 0.5, 0.12, segs=10, ring_segs=4, rot=(math.pi / 2, 0, 0), mat=p.m("metal_dark"))
    p.done()
    p = Part("PipeSupport")  # 1 m tall (Godot stretches it to the pipe height), 1.4 m wide
    for x in (-0.6, 0.6):
        box(p.bm, (x, 0, 0.5), (0.18, 0.18, 1.0), mat=p.m("metal_dark"), round_frac=0.0)
    box(p.bm, (0, 0, 0.97), (1.6, 0.25, 0.06), mat=p.m("hazard"), round_frac=0.0)
    p.done()
    p = Part("Valve")
    torus(p.bm, (0, 0, 0.0), 0.35, 0.05, segs=10, ring_segs=4, mat=p.m("red"))
    for a in (0, math.pi / 2):
        cylinder(p.bm, (-0.35 * math.cos(a), -0.35 * math.sin(a), 0), (0.35 * math.cos(a), 0.35 * math.sin(a), 0), 0.035, sides=4, mat=p.m("red"))
    cylinder(p.bm, (0, 0, -0.5), (0, 0, 0), 0.07, sides=6, mat=p.m("steel"))
    p.done()
    p = Part("Outfall")  # big rusty mouth ring for a pipe end, facing -Y
    torus(p.bm, (0, 0, 0), 0.62, 0.18, segs=12, ring_segs=5, rot=(math.pi / 2, 0, 0), mat=p.m("rust"))
    for i in range(4):
        a = i / 4 * math.tau + 0.4
        ball(p.bm, (math.cos(a) * 0.62, -0.1, math.sin(a) * 0.62), 0.09, mat=p.m("metal_dark"))
    p.done()
    p = Part("Puff")  # smoke puff (Godot tints it)
    ball(p.bm, (0, 0, 0), 1.0, subdiv=1, mat=p.m("puff"))
    p.done()


def build_all():
    truck_tanker()
    truck_box()
    barrels()
    jersey()
    gate()
    cone_and_sawhorse()
    fence()
    hazard_sign()
    dead_trees()
    pipe_parts()
