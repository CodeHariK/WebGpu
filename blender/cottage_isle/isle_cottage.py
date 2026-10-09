"""The cottage: cream walls on a stone footing, orange timber trim, a steep pink fish-scale roof,
a round double door with cat ears and a curved awning, an upper window with a planter, a chimney
with a wisp of smoke and a paw-print cloud. Built in its own space (origin on the ground at the
centre, front facing −Y) under one empty, so it can be placed and turned as a whole."""
import math

import bmesh

from isle_kit import ball, beam, bm_ball, bm_box, bm_cone, box, cone, empty, mesh_object, mtx

W, D = 2.3, 2.0  # walls: width (x), depth (y)
FOOTING = 0.25
WALL_TOP = 2.5
PITCH = math.radians(55)
OVERHANG = 0.3
EAVE_X = W / 2 + OVERHANG
EAVE_Z = WALL_TOP - OVERHANG * math.tan(PITCH)
RIDGE_Z = WALL_TOP + W / 2 * math.tan(PITCH)
SLOPE = math.hypot(EAVE_X, RIDGE_Z - EAVE_Z)
ROOF_LENGTH = D + 0.6
FRONT = -D / 2
DOOR_Z = FOOTING + 0.55  # centre of the door's round top


def _gable(name, y, parent):
    bm = bmesh.new()
    pts = [(-W / 2, WALL_TOP), (W / 2, WALL_TOP), (0, RIDGE_Z - 0.05)]
    front = [bm.verts.new((x, y - 0.1, z)) for x, z in pts]
    back = [bm.verts.new((x, y + 0.1, z)) for x, z in pts]
    bm.faces.new(front)
    bm.faces.new(list(reversed(back)))
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new((front[j], front[i], back[i], back[j]))
    mesh_object(name, bm, "wall", parent=parent)


def _roof(root):
    roof = empty("Cottage.RoofGroup", (0, 0, WALL_TOP), (0, math.radians(2), 0), root)  # a little crooked
    slab_bm, scale_bm = bmesh.new(), bmesh.new()
    rows, per_row = int(SLOPE / 0.24), int(ROOF_LENGTH / 0.34) + 1
    for s in (1, -1):
        normal = (s * math.sin(PITCH), 0, math.cos(PITCH))
        centre = (s * EAVE_X / 2 + normal[0] * 0.1, 0, (EAVE_Z + RIDGE_Z) / 2 - WALL_TOP + normal[2] * 0.1)
        slab = mtx(centre, (0, s * PITCH, 0))
        bm_box(slab_bm, (SLOPE, ROOF_LENGTH, 0.2), slab)
        for k in range(rows):  # fish scales, eave row first; each row up the roof sits a hair higher
            u = s * (SLOPE / 2 - 0.12 - k * 0.24)
            shift = 0.17 if k % 2 else 0.0
            for j in range(per_row):
                v = -ROOF_LENGTH / 2 + 0.12 + j * 0.34 + shift
                if v < ROOF_LENGTH / 2 - 0.05:
                    bm_cone(scale_bm, 0.19, 0.19, 0.05, slab @ mtx((u, v, 0.11 + 0.01 * k)), 10)
    mesh_object("Cottage.Roof-col", slab_bm, "roof_dark", parent=roof, bevel=0.04)
    mesh_object("Cottage.Scales", scale_bm, "roof", smooth=True, parent=roof)
    cone("Cottage.Ridge", 0.12, 0.12, ROOF_LENGTH + 0.1, (0, 0, RIDGE_Z - WALL_TOP + 0.22), "timber", (math.pi / 2, 0, 0), parent=roof)
    for y in (-ROOF_LENGTH / 2, ROOF_LENGTH / 2):  # wide orange boards on both gable ends
        for s in (1, -1):
            beam("Cottage.Fascia", (s * (EAVE_X + 0.05), y, EAVE_Z - WALL_TOP), (0, y, RIDGE_Z - WALL_TOP + 0.25), 0.2, "timber", roof)
            box("Cottage.Bracket", (0.26, 0.26, 0.26), (s * (EAVE_X + 0.02), y, EAVE_Z - WALL_TOP - 0.05), "timber_dark", parent=roof)
    box("Cottage.Chimney", (0.42, 0.42, 1.9), (0.55, 0.3, 1.05), "stone", parent=roof, bevel=0.03)
    box("Cottage.ChimneyCap", (0.56, 0.56, 0.12), (0.55, 0.3, 2.0), "stone", parent=roof)
    _smoke(roof)


def _smoke(roof):
    """A curling wisp from the chimney, then a puffy cloud with a little paw print."""
    bm = bmesh.new()
    for k, (x, z, r) in enumerate(((0.6, 2.35, 0.09), (0.75, 2.6, 0.11), (0.98, 2.75, 0.12), (1.25, 2.78, 0.12), (1.48, 2.68, 0.1))):
        bm_ball(bm, r, mtx((x, 0.3, z), scale=(1.3, 0.6, 1)), 2)
    for x, z, r in ((1.95, 3.25, 0.34), (2.3, 3.4, 0.3), (2.1, 3.62, 0.3), (1.7, 3.5, 0.25), (2.4, 3.15, 0.22)):
        bm_ball(bm, r, mtx((x, 0.3, z), scale=(1, 0.55, 0.9)), 2)
    pad = (2.75, 0.3, 3.85)
    bm_ball(bm, 0.075, mtx(pad, scale=(1, 0.5, 0.85)), 1)
    for dx, dz in ((-0.09, 0.09), (0.0, 0.13), (0.09, 0.09)):
        bm_ball(bm, 0.04, mtx((pad[0] + dx, 0.3, pad[2] + dz), scale=(1, 0.5, 1)), 1)
    mesh_object("Cottage.Smoke", bm, "smoke", smooth=True, parent=roof)


def _frame(root):
    for x in (-W / 2, W / 2):
        for y in (-D / 2, D / 2):
            box("Cottage.Post", (0.2, 0.2, WALL_TOP - FOOTING), (x, y, (WALL_TOP + FOOTING) / 2), "timber", parent=root)
    for y in (FRONT - 0.05, D / 2 + 0.05):
        box("Cottage.TopBeam", (W + 0.15, 0.14, 0.2), (0, y, WALL_TOP), "timber", parent=root)
    for x in (-W / 2 - 0.05, W / 2 + 0.05):
        box("Cottage.SideBeam", (0.14, D + 0.15, 0.2), (x, 0, WALL_TOP), "timber", parent=root)
        box("Cottage.Corner", (0.3, 0.3, 0.3), (x, FRONT - 0.05, WALL_TOP), "timber_dark", parent=root)


def _cat_door(root):
    y = FRONT - 0.03
    bm = bmesh.new()  # the frame: round top, straight sides, two cat ears
    bm_cone(bm, 0.66, 0.66, 0.1, mtx((0, y + 0.02, DOOR_Z), (math.pi / 2, 0, 0)), 28)
    bm_box(bm, (1.32, 0.1, DOOR_Z - FOOTING), mtx((0, y + 0.02, (DOOR_Z + FOOTING) / 2)))
    for s in (1, -1):
        tilt = s * math.radians(38)
        at = (math.sin(tilt) * 0.62, y + 0.02, DOOR_Z + math.cos(tilt) * 0.62 + 0.04)
        bm_cone(bm, 0.24, 0.0, 0.36, mtx(at, (0, tilt, 0), (1, 0.45, 1)), 3)
    mesh_object("Cottage.DoorFrame", bm, "timber", smooth=False, parent=root)
    bm = bmesh.new()
    bm_cone(bm, 0.52, 0.52, 0.1, mtx((0, y - 0.02, DOOR_Z), (math.pi / 2, 0, 0)), 28)
    bm_box(bm, (1.04, 0.1, DOOR_Z - FOOTING), mtx((0, y - 0.02, (DOOR_Z + FOOTING) / 2)))
    mesh_object("Cottage.Door", bm, "door", parent=root)
    box("Cottage.DoorSplit", (0.04, 0.12, 1.05), (0, y - 0.05, FOOTING + 0.53), "timber_dark", bevel=0.0, parent=root)
    for s in (1, -1):
        ball("Cottage.Knob", 0.045, (s * 0.1, y - 0.1, DOOR_Z - 0.1), "gold", subdivisions=1, parent=root)
    box("Cottage.Step", (1.4, 0.5, 0.14), (0, FRONT - 0.3, 0.07), "stone", bevel=0.04, parent=root)
    _awning(root, DOOR_Z + 1.45)  # high enough that the cat ears show under it


def _awning(root, z):
    """A curved quarter-tube canopy over the door (solidified so it has thickness)."""
    bm = bmesh.new()
    radius, width, steps = 0.36, 1.2, 8
    rows = []
    for i in range(steps + 1):
        a = math.pi / 2 + (math.pi / 2) * i / steps  # from the wall (up) round to hanging out front
        rows.append([bm.verts.new((x, FRONT - 0.05 + radius * math.cos(a), z + radius * math.sin(a) - radius)) for x in (-width / 2, width / 2)])
    for (a, b), (c, d) in zip(rows, rows[1:]):
        bm.faces.new((a, b, d, c))
    awning = mesh_object("Cottage.Awning", bm, "awning", smooth=True, parent=root)
    awning.modifiers.new("Thick", "SOLIDIFY").thickness = 0.06


def _windows(root):
    gz = WALL_TOP + 0.6  # upper window in the gable, with a planter
    box("Cottage.UpperFrame", (0.68, 0.1, 0.78), (0, FRONT - 0.13, gz), "timber", parent=root)
    for s in (1, -1):
        box("Cottage.UpperPane", (0.22, 0.1, 0.56), (s * 0.14, FRONT - 0.17, gz - 0.02), "glass", bevel=0.0, parent=root)
    box("Cottage.Planter", (0.8, 0.26, 0.16), (0, FRONT - 0.26, gz - 0.46), "timber", parent=root)
    for k in range(5):
        ball("Cottage.PlanterLeaf", 0.08, (-0.28 + 0.14 * k, FRONT - 0.26, gz - 0.36), "bush", subdivisions=1, parent=root)
    sy, sz = -0.1, 1.35  # side window
    box("Cottage.SideFrame", (0.1, 0.62, 0.86), (W / 2 + 0.04, sy, sz), "timber", parent=root)
    for s in (1, -1):
        box("Cottage.SidePane", (0.1, 0.22, 0.64), (W / 2 + 0.07, sy + s * 0.13, sz), "glass", bevel=0.0, parent=root)


def _bushes(root):
    bm = bmesh.new()
    for x in (-0.95, 0.95):
        for dx, dy, dz, r in ((0, 0, 0.25, 0.3), (0.16, -0.08, 0.38, 0.2), (-0.15, -0.05, 0.36, 0.2), (0.0, -0.15, 0.2, 0.22)):
            bm_ball(bm, r, mtx((x + dx, FRONT - 0.4 + dy, dz)), 2)
    mesh_object("Cottage.Bushes", bm, "bush", smooth=True, parent=root)


def make_cottage(at, yaw_degrees):
    root = empty("Cottage", at, (0, 0, math.radians(yaw_degrees)))
    box("Cottage.Footing-col", (W + 0.25, D + 0.25, FOOTING), (0, 0, FOOTING / 2), "stone", bevel=0.06, parent=root)
    box("Cottage.Walls-col", (W, D, WALL_TOP - FOOTING), (0, 0, (WALL_TOP + FOOTING) / 2), "wall", bevel=0.0, parent=root)
    _gable("Cottage.GableFront", FRONT, root)
    _gable("Cottage.GableBack", D / 2, root)
    _frame(root)
    _cat_door(root)
    _windows(root)
    _roof(root)
    _bushes(root)
    return root
