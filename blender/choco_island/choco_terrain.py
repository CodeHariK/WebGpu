"""Choco Island terrain: the cake island (frosting, sponge, cream, chocolate layers), the mountain of
rounded cake cubes, and the floating cake platforms. Z is up, metres, island top at z = 0."""
import math
import random

from choco_kit import Part, ball, cone, cylinder, prism, rounded_cube, use_collection

SEA_Z = -3.0
ISLAND_R = 16.0
ISLAND_SIDES = 20
ISLAND_WOBBLE = 0.03
ISLAND_SEED = 3

# Mountain cubes: (name, x, y, base z, size, yaw degrees, body). Sizes all differ; they overlap and
# stack like a pile of cake blocks. A is the waterfall block (its pond feeds the fall), J the peak.
CUBES = [
    ("A", 0.0, 8.0, 0.0, 7.0, 0, "sponge"),
    ("B", -6.0, 6.5, 0.0, 5.5, -15, "choco"),
    ("C", 5.5, 6.0, 0.0, 5.0, 25, "choco"),
    ("D", -3.0, 11.5, 0.0, 6.2, 5, "choco"),
    ("E", 4.2, 11.0, 0.0, 5.6, -8, "sponge"),
    ("L", -9.5, 4.0, 0.0, 2.6, 30, "sponge"),
    ("M", 8.6, 3.6, 0.0, 2.2, -20, "sponge"),
    ("N", 8.0, 9.5, 0.0, 3.4, 12, "sponge"),
    ("O", -8.6, 9.8, 0.0, 3.8, -6, "sponge"),
    ("F", -1.5, 9.4, 7.0, 4.5, 18, "choco"),
    ("G", 3.6, 9.2, 5.0, 3.8, -22, "sponge"),
    ("H", -5.2, 8.0, 5.5, 3.5, 8, "sponge"),
    ("K", -4.0, 12.0, 6.2, 3.0, -30, "sponge"),
    ("I", 0.6, 9.8, 11.5, 3.2, 35, "sponge"),
    ("J", 0.2, 10.0, 14.7, 2.2, 10, "choco"),
]
WHITE_CAPS = {"I", "J", "F"}  # high blocks get white "snow" frosting

PLATFORMS = [  # (x, y, top z, radius, seed)
    (-21.0, -4.0, 5.0, 2.8, 11),
    (-22.0, 8.0, 9.5, 3.3, 12),
    (19.5, -8.0, 3.8, 2.4, 13),
    (21.5, 5.0, 8.0, 3.4, 14),
    (-9.0, -23.0, 6.0, 2.2, 15),
]


def island_jitter():
    rng = random.Random(ISLAND_SEED)
    return [1 + rng.uniform(-ISLAND_WOBBLE, ISLAND_WOBBLE) for _ in range(ISLAND_SIDES)]


def island_radius(angle, base=ISLAND_R):
    """Radius of the island's frosting edge at `angle` (interpolated between the prism's corners)."""
    jit = island_jitter()
    f = (angle % math.tau) / math.tau * ISLAND_SIDES
    i = int(f) % ISLAND_SIDES
    k = f - int(f)
    return base * (jit[i] * (1 - k) + jit[(i + 1) % ISLAND_SIDES] * k)


def island(skip_drips=()):
    """The cake island. skip_drips: angles (radians) where no frosting drip goes (the syrup fall)."""
    use_collection("Island")
    p = Part("Island")
    kw = dict(sides=ISLAND_SIDES, wobble=ISLAND_WOBBLE, seed=ISLAND_SEED)
    prism(p.bm, (0, 0), ISLAND_R, -0.6, 0.0, mat=p.m("frost_pink"), **kw)
    prism(p.bm, (0, 0), ISLAND_R * 0.98, -1.9, -0.6, mat=p.m("sponge"), **kw)
    prism(p.bm, (0, 0), ISLAND_R * 0.99, -2.3, -1.9, mat=p.m("cream"), **kw)
    prism(p.bm, (0, 0), ISLAND_R * 0.97, -6.0, -2.3, mat=p.m("choco"), taper=1.0, **kw)
    # Frosting drips down the side, all round (except where the syrup falls off).
    rng = random.Random(7)
    n = 46
    for i in range(n):
        a = i / n * math.tau + rng.uniform(-0.03, 0.03)
        if any(abs((a - s + math.pi) % math.tau - math.pi) < 0.12 for s in skip_drips):
            continue
        r = island_radius(a) * 0.985
        length = rng.uniform(0.35, 1.2)
        rad = rng.uniform(0.28, 0.5)
        c = (math.cos(a) * r, math.sin(a) * r)
        cone(p.bm, c, rad, -0.55, -0.55 - length, sides=6, mat=p.m("frost_pink"))
        ball(p.bm, (c[0], c[1], -0.55 - length * 0.55), rad * 0.75, (1, 1, 1.3), mat=p.m("frost_pink"))
    # Pistachio crumb patches on top (the "grass").
    for (x, y, r, seed) in [(-8, -6, 3.0, 1), (-11, 1, 2.2, 2), (8, -9, 2.6, 3), (10, -2, 2.0, 4),
                            (-4, -12, 2.4, 5), (-12, -6, 1.6, 6), (12, 2, 1.8, 7), (-2, -5, 1.4, 8)]:
        prism(p.bm, (x, y), r, -0.05, 0.05, sides=7, wobble=0.25, seed=seed, mat=p.m("pistachio"))
    p.done()


def _drips(p, cx, cy, top_z, half, yaw, size, color, rng):
    """Frosting drips hanging off the four sides of a cube's cap."""
    for side in range(4):
        a = yaw + side * math.pi / 2
        nx, ny = math.cos(a), math.sin(a)
        tx, ty = -ny, nx
        for k in (-0.32, 0.0, 0.3):
            if rng.random() < 0.25:
                continue
            off = k * half * 2 + rng.uniform(-0.08, 0.08) * size
            x = cx + nx * half * 1.0 + tx * off
            y = cy + ny * half * 1.0 + ty * off
            rad = 0.12 * size + 0.05
            length = rng.uniform(0.12, 0.3) * size
            cone(p.bm, (x, y), rad, top_z, top_z - length, sides=6, mat=p.m(color))
            ball(p.bm, (x, y, top_z - length * 0.5), rad * 0.8, (1, 1, 1.3), mat=p.m(color))


def cake_cube(p, x, y, base, s, yaw_deg, body, cap, rng, square=False):
    """One mountain block: a rounded cake cube with a cream band and a frosting cap with drips."""
    yaw = math.radians(yaw_deg)
    tilt = (0.0, 0.0) if square else (math.radians(rng.uniform(-4, 4)), math.radians(rng.uniform(-4, 4)))
    sy = s if square else s * rng.uniform(0.9, 1.0)
    rounded_cube(p.bm, (x, y, base + s / 2), (s, sy, s), yaw=yaw, tilt=tilt, round_frac=0.2, segments=2, mat=p.m(body))
    rounded_cube(p.bm, (x, y, base + s * 0.42), (s * 1.035, sy * 1.035, s * 0.1), yaw=yaw,
                 tilt=tilt, round_frac=0.4, segments=1, mat=p.m("cream"))
    cap_h = s * 0.16
    rounded_cube(p.bm, (x, y, base + s - cap_h / 2 + s * 0.03), (s * 1.05, sy * 1.05, cap_h), yaw=yaw,
                 tilt=tilt, round_frac=0.35, segments=2, mat=p.m(cap))
    _drips(p, x, y, base + s - cap_h + s * 0.04, min(s, sy) * 0.5, yaw, s, cap, rng)
    return base + s + s * 0.03  # top of the frosting


def mountain():
    use_collection("Mountain")
    p = Part("Mountain")
    rng = random.Random(21)
    tops = {}
    for (name, x, y, base, s, yaw, body) in CUBES:
        cap = "frost_white" if name in WHITE_CAPS else "frost_pink"
        tops[name] = cake_cube(p, x, y, base, s, yaw, body, cap, rng, square=name == "A")
    # A cherry on the very top.
    jx, jy = CUBES[-1][1], CUBES[-1][2]
    ball(p.bm, (jx, jy, tops["J"] + 0.55), 0.62, (1, 1, 0.92), subdiv=2, mat=p.m("candy_red"))
    cylinder(p.bm, (jx, jy, tops["J"] + 1.0), (jx + 0.35, jy - 0.1, tops["J"] + 1.9), 0.06, mat=p.m("pistachio"))
    p.done()
    return tops


def platforms():
    """Floating cake slices: frosted top, layered sides, a chocolate cone underneath with drips."""
    use_collection("Platforms")
    out = []
    for i, (x, y, top, r, seed) in enumerate(PLATFORMS):
        p = Part(f"Platform_{i}")
        rng = random.Random(seed)
        sides = 10
        kw = dict(sides=sides, wobble=0.08, seed=seed)
        prism(p.bm, (x, y), r, top - 0.35, top, mat=p.m("frost_pink"), **kw)
        prism(p.bm, (x, y), r * 0.97, top - 1.1, top - 0.35, mat=p.m("sponge"), **kw)
        prism(p.bm, (x, y), r * 0.99, top - 1.35, top - 1.1, mat=p.m("cream"), **kw)
        prism(p.bm, (x, y), r * 0.96, top - 1.9, top - 1.35, mat=p.m("choco"), **kw)
        cone(p.bm, (x, y), r * 0.95, top - 1.9, top - 1.9 - r * 1.3, sides=sides, mat=p.m("choco_dark"))
        for k in range(7):  # drips off the frosting
            a = k / 7 * math.tau + rng.uniform(-0.2, 0.2)
            c = (x + math.cos(a) * r * 0.98, y + math.sin(a) * r * 0.98)
            length = rng.uniform(0.2, 0.6)
            cone(p.bm, c, 0.2, top - 0.3, top - 0.3 - length, sides=5, mat=p.m("frost_pink"))
        for k in range(3):  # chocolate drips off the bottom cone
            a = k / 3 * math.tau + rng.uniform(0, 1)
            c = (x + math.cos(a) * r * 0.35, y + math.sin(a) * r * 0.35)
            cone(p.bm, c, 0.18, top - 1.9 - r * 0.8, top - 1.9 - r * 0.8 - rng.uniform(0.6, 1.2), sides=5,
                 mat=p.m("choco_dark"))
        p.done()
        out.append((x, y, top, r))
    return out
