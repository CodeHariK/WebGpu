"""Choco Island props: lollipop trees, swirl lollipops, popsicle trees, candy canes, gumdrops,
chocolate rocks and a wafer bridge over the syrup stream."""
import math
import random

from choco_kit import Part, ball, cylinder, prism, rounded_cube, torus, use_collection

CANDY = ["candy_red", "candy_mint", "candy_yellow", "candy_purple", "frost_pink"]

BALL_TREES = [(-7, -2, 3.2, 1.3, "candy_red"), (-10, -3.5, 2.6, 1.0, "candy_purple"), (-5, -8, 2.8, 1.1, "candy_mint"),
              (7, -4, 3.0, 1.2, "candy_yellow"), (10.5, -6.5, 2.4, 0.9, "candy_red"), (12, -1, 3.4, 1.3, "candy_mint"),
              (-12.5, 1.5, 2.8, 1.1, "candy_yellow"), (-1.5, -2, 2.2, 0.8, "candy_purple"), (13, 4.5, 2.6, 1.0, "candy_purple")]
SWIRLS = [(6.5, -12, 3.0, 1.1, "candy_red", 0.3), (-8, -11.5, 2.6, 1.0, "candy_purple", -0.4),
          (-13.5, -4.5, 2.4, 0.9, "candy_mint", 0.9), (9.5, -10.5, 2.2, 0.8, "candy_yellow", -0.2)]
POPSICLES = [(-1.5, -9.5, "candy_mint", 0.2), (6, -0.5, "candy_red", -0.5), (-12.5, -1.5, "candy_yellow", 0.4),
             (-4, -13.5, "frost_pink", 0.0), (11.5, -9, "candy_purple", 0.3)]
CANES = [(-1, -14, 1.0), (8.5, -13, -2.0), (-14.5, -1, 0.5), (14.5, -3, 2.6), (-10, -10, -0.8)]


def dist_to_line(p, line):
    best = 1e9
    for (ax, ay), (bx, by) in zip(line, line[1:]):
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / (dx * dx + dy * dy)))
        best = min(best, math.hypot(p[0] - ax - dx * t, p[1] - ay - dy * t))
    return best


def ball_tree(p, x, y, h, r, color, z=0.0):
    cylinder(p.bm, (x, y, z), (x, y, z + h), 0.1, mat=p.m("stick"))
    ball(p.bm, (x, y, z + h + r * 0.75), r, subdiv=1, mat=p.m(color))
    torus(p.bm, (x, y, z + h + r * 0.75), r * 0.97, r * 0.16, segs=10, ring_segs=4, mat=p.m("cream"))


def swirl(p, x, y, h, r, color, yaw, z=0.0):
    """A flat swirl lollipop: a disc with rings of cream, standing up and facing `yaw`."""
    cylinder(p.bm, (x, y, z), (x, y, z + h), 0.09, mat=p.m("stick"))
    c = (x, y, z + h + r * 0.9)
    rot = (math.pi / 2, 0, yaw)
    ball(p.bm, c, r, (1, 0.3, 1), subdiv=1, mat=p.m(color), rot=(0, 0, yaw))
    torus(p.bm, c, r * 0.72, r * 0.12, segs=12, ring_segs=4, rot=rot, flatten=2.6, mat=p.m("cream"))
    torus(p.bm, c, r * 0.36, r * 0.1, segs=9, ring_segs=4, rot=rot, flatten=3.2, mat=p.m("cream"))


def popsicle(p, x, y, color, yaw, z=0.0):
    rounded_cube(p.bm, (x, y, z + 0.45), (0.22, 0.1, 0.9), yaw=yaw, round_frac=0.4, segments=1, mat=p.m("wafer"))
    rounded_cube(p.bm, (x, y, z + 1.6), (1.0, 0.5, 1.6), yaw=yaw, round_frac=0.35, segments=2, mat=p.m(color))
    # A chocolate dip on the top half.
    rounded_cube(p.bm, (x, y, z + 2.0), (1.06, 0.56, 0.85), yaw=yaw, round_frac=0.4, segments=2, mat=p.m("choco"))


def cane(p, x, y, yaw, h=2.6, z=0.0):
    """A candy cane: red/white shaft segments and a hooked top made of alternating arc pieces."""
    n = 6
    for i in range(n):
        cylinder(p.bm, (x, y, z + h * i / n), (x, y, z + h * (i + 1) / n), 0.15, sides=6,
                 mat=p.m("candy_red" if i % 2 else "frost_white"))
    R = 0.45
    cx, cy = x - R * math.cos(yaw), y - R * math.sin(yaw)
    pieces = 4
    for i in range(pieces):
        torus(p.bm, (cx, cy, z + h), R, 0.15, segs=3, ring_segs=6, arc=math.pi / pieces,
              start=i * math.pi / pieces, rot=(math.pi / 2, 0, yaw),
              mat=p.m("candy_red" if i % 2 == 0 else "frost_white"))
    cylinder(p.bm, (x - 2 * R * math.cos(yaw), y - 2 * R * math.sin(yaw), z + h),
             (x - 2 * R * math.cos(yaw), y - 2 * R * math.sin(yaw), z + h - 0.35), 0.15, sides=6, mat=p.m("candy_red"))


def gumdrop(p, x, y, r, color, z=0.0):
    prism(p.bm, (x, y), r, z, z + r * 0.6, sides=7, wobble=0.05, seed=int(x * 7 + y * 13), mat=p.m(color), taper=0.8)
    ball(p.bm, (x, y, z + r * 0.6), r * 0.8, (1, 1, 0.7), subdiv=1, mat=p.m(color))


def bridge(p, x, y, yaw, length=3.6):
    """A wafer bridge over the stream: a deck with darker slats and wafer posts at each end."""
    rounded_cube(p.bm, (x, y, 0.42), (length, 1.3, 0.28), yaw=yaw, round_frac=0.3, segments=1, mat=p.m("wafer"))
    ca, sa = math.cos(yaw), math.sin(yaw)
    for k in range(-3, 4):
        o = k * length / 8
        rounded_cube(p.bm, (x + ca * o, y + sa * o, 0.6), (0.18, 1.34, 0.1), yaw=yaw, round_frac=0.3,
                     segments=1, mat=p.m("wafer_dark"))
    for s in (-1, 1):
        for t in (-1, 1):
            px = x + ca * s * length / 2 - sa * t * 0.6
            py = y + sa * s * length / 2 + ca * t * 0.6
            rounded_cube(p.bm, (px, py, 0.55), (0.3, 0.3, 1.1), yaw=yaw, round_frac=0.3, segments=1, mat=p.m("wafer_dark"))


def props(stream, platforms):
    use_collection("Props")
    p = Part("Props")
    for t in BALL_TREES:
        ball_tree(p, *t)
    for s in SWIRLS:
        swirl(p, *s)
    for (x, y, c, yaw) in POPSICLES:
        popsicle(p, x, y, c, yaw)
    for (x, y, yaw) in CANES:
        cane(p, x, y, yaw)
    bridge(p, 2.45, -6.0, math.radians(8))
    rng = random.Random(77)
    taken = [(t[0], t[1], 1.6) for t in BALL_TREES + SWIRLS] + [(t[0], t[1], 1.2) for t in POPSICLES + CANES]
    taken.append((2.45, -6.0, 2.4))

    def free(x, y, clear):
        if math.hypot(x, y) > 14.3 or (y > 1.2 and -11.5 < x < 11):
            return False
        if dist_to_line((x, y), stream) < 1.5 + clear:
            return False
        return all(math.hypot(x - a, y - b) > c + clear for (a, b, c) in taken)

    for _ in range(26):  # gumdrops
        for _ in range(40):
            x, y = rng.uniform(-14, 14), rng.uniform(-14, 3)
            if free(x, y, 0.6):
                break
        r = rng.uniform(0.3, 0.55)
        gumdrop(p, x, y, r, rng.choice(CANDY))
        taken.append((x, y, r))
    for (x, y) in stream[1:-1]:  # chocolate rocks along the banks
        for s in (-1, 1):
            rx, ry = x + s * rng.uniform(1.1, 1.5), y + rng.uniform(-0.8, 0.8)
            rounded_cube(p.bm, (rx, ry, 0.15), (rng.uniform(0.5, 0.9), rng.uniform(0.4, 0.7), rng.uniform(0.35, 0.6)),
                         yaw=rng.uniform(0, math.pi), round_frac=0.3, segments=1, mat=p.m("choco_dark"))
    # A little something on each floating platform.
    for i, (x, y, top, r) in enumerate(platforms):
        if i % 2 == 0:
            ball_tree(p, x - r * 0.2, y, 2.0, 0.8, CANDY[i % len(CANDY)], z=top)
            gumdrop(p, x + r * 0.45, y + r * 0.3, 0.4, CANDY[(i + 2) % len(CANDY)], z=top)
        else:
            swirl(p, x, y + r * 0.2, 2.2, 0.9, CANDY[i % len(CANDY)], 0.3, z=top)
            cane(p, x + r * 0.5, y - r * 0.4, 1.2, h=1.8, z=top)
            gumdrop(p, x - r * 0.5, y - r * 0.2, 0.35, CANDY[(i + 1) % len(CANDY)], z=top)
    p.done()
