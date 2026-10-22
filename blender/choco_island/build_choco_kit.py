"""Choco Island KIT: the few pieces Godot repeats to assemble an island procedurally.

    Blender -b --python build_choco_kit.py   → godot_demo/choco_kit.glb  +  choco_kit_sheet.png

Every piece sits at the origin, base on the ground (z = 0), at a unit size Godot scales. Blender's
front (-Y) becomes Godot's +Z after the glTF axis swap.

  Cube_*      mountain blocks, 1 x 1 x 1 m cake cubes (rounded body, cream band, frosting cap, drips).
              Godot scales them uniformly (2–7 m) so the rounding stays in proportion.
  Step_*      stair blocks, 1 x 1 x 0.6 m slabs (sugar cube, wafer, frosted chocolate, sandwich biscuit).
  Platform_*  floating cake slices, radius 1 m, frosting top at z = 0, chocolate cone hanging below.
  Drip        a frosting drip for the island edge (hangs down from z = 0, 1 m long).
  props       LolliTree_*, Swirl_*, Popsicle_*, Cane, Gumdrop_*, Rock_*, Bridge, Cherry.
  syrup bits  FoamRing (radius 1), Bubble, SeaSwirl_* (radius-1 arcs for the sea).

The island base, the sea, the pools and the syrup falls/stream are not here: their shape depends on
the layout, so Godot builds those meshes itself (choco_mesh.gd)."""
import math
import os
import random
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import choco_kit as kit  # noqa: E402
import choco_props as props  # noqa: E402
import choco_terrain as terrain  # noqa: E402
from choco_kit import Part, ball, cone, prism, rounded_cube, torus  # noqa: E402

COLORS = {"red": "candy_red", "mint": "candy_mint", "yellow": "candy_yellow", "purple": "candy_purple", "pink": "frost_pink"}


def cubes():
    seed = 0
    for body in ("choco", "sponge"):
        for cap, variants in (("pink", "ab"), ("white", "a")):
            for v in variants:
                seed += 1
                p = Part(f"Cube_{body}_{cap}_{v}")
                rng = random.Random(100 + seed)
                terrain.cake_cube(p, 0, 0, 0, 1.0, 0, body, "frost_white" if cap == "white" else "frost_pink", rng, square=True)
                p.done()


def steps():
    rng = random.Random(3)
    p = Part("Step_sugar")
    rounded_cube(p.bm, (0, 0, 0.3), (1, 1, 0.6), round_frac=0.25, segments=2, mat=p.m("frost_white"))
    for c in ("candy_red", "candy_mint", "candy_yellow", "candy_purple"):  # sprinkles
        rounded_cube(p.bm, (rng.uniform(-0.3, 0.3), rng.uniform(-0.3, 0.3), 0.61), (0.2, 0.06, 0.05),
                     yaw=rng.uniform(0, math.pi), round_frac=0.4, segments=1, mat=p.m(c))
    p.done()

    p = Part("Step_wafer")
    rounded_cube(p.bm, (0, 0, 0.3), (1, 1, 0.6), round_frac=0.12, segments=1, mat=p.m("wafer"))
    for k in (-0.25, 0.0, 0.25):
        rounded_cube(p.bm, (k, 0, 0.6), (0.06, 0.96, 0.04), round_frac=0.3, segments=1, mat=p.m("wafer_dark"))
        rounded_cube(p.bm, (0, k, 0.6), (0.96, 0.06, 0.04), round_frac=0.3, segments=1, mat=p.m("wafer_dark"))
    for z in (0.2, 0.4):  # layered sides
        rounded_cube(p.bm, (0, 0, z), (1.02, 1.02, 0.05), round_frac=0.3, segments=1, mat=p.m("wafer_dark"))
    p.done()

    p = Part("Step_choco")
    rounded_cube(p.bm, (0, 0, 0.26), (1, 1, 0.52), round_frac=0.2, segments=2, mat=p.m("choco"))
    rounded_cube(p.bm, (0, 0, 0.55), (1.05, 1.05, 0.14), round_frac=0.35, segments=2, mat=p.m("frost_pink"))
    for (x, y) in ((0.52, -0.2), (-0.2, 0.52), (-0.52, 0.15), (0.25, -0.52)):
        cone(p.bm, (x, y), 0.08, 0.5, 0.5 - rng.uniform(0.12, 0.25), sides=5, mat=p.m("frost_pink"))
    p.done()

    p = Part("Step_biscuit")
    rounded_cube(p.bm, (0, 0, 0.125), (1, 1, 0.25), round_frac=0.3, segments=2, mat=p.m("sponge"))
    rounded_cube(p.bm, (0, 0, 0.31), (0.97, 0.97, 0.13), round_frac=0.4, segments=1, mat=p.m("cream"))
    rounded_cube(p.bm, (0, 0, 0.485), (1, 1, 0.23), round_frac=0.3, segments=2, mat=p.m("sponge"))
    p.done()


def platforms():
    for v, seed in (("a", 11), ("b", 14)):
        p = Part(f"Platform_{v}")
        rng = random.Random(seed)
        kw = dict(sides=10, wobble=0.08, seed=seed)
        prism(p.bm, (0, 0), 1.0, -0.12, 0.0, mat=p.m("frost_pink"), **kw)
        prism(p.bm, (0, 0), 0.97, -0.37, -0.12, mat=p.m("sponge"), **kw)
        prism(p.bm, (0, 0), 0.99, -0.45, -0.37, mat=p.m("cream"), **kw)
        prism(p.bm, (0, 0), 0.96, -0.63, -0.45, mat=p.m("choco"), **kw)
        cone(p.bm, (0, 0), 0.95, -0.63, -1.93, sides=10, mat=p.m("choco_dark"))
        for k in range(7):
            a = k / 7 * math.tau + rng.uniform(-0.2, 0.2)
            cone(p.bm, (math.cos(a) * 0.98, math.sin(a) * 0.98), 0.07, -0.1, -0.1 - rng.uniform(0.07, 0.2), sides=5,
                 mat=p.m("frost_pink"))
        for k in range(3):
            a = k / 3 * math.tau + rng.uniform(0, 1)
            cone(p.bm, (math.cos(a) * 0.35, math.sin(a) * 0.35), 0.06, -1.4, -1.4 - rng.uniform(0.2, 0.4), sides=5,
                 mat=p.m("choco_dark"))
        p.done()


def small_pieces():
    p = Part("Drip")
    cone(p.bm, (0, 0), 0.4, 0.0, -1.0, sides=6, mat=p.m("frost_pink"))
    ball(p.bm, (0, 0, -0.55), 0.3, (1, 1, 1.3), mat=p.m("frost_pink"))
    p.done()

    for name, color in COLORS.items():
        if name != "pink":
            p = Part(f"LolliTree_{name}")
            props.ball_tree(p, 0, 0, 2.8, 1.1, color)
            p.done()
            p = Part(f"Swirl_{name}")
            props.swirl(p, 0, 0, 2.6, 1.0, color, 0.0)  # thin along Y: the disc faces forward
            p.done()
        p = Part(f"Popsicle_{name}")
        props.popsicle(p, 0, 0, color, 0)
        p.done()
        p = Part(f"Gumdrop_{name}")
        props.gumdrop(p, 0, 0, 0.5, color)
        p.done()

    p = Part("Cane")
    props.cane(p, 0, 0, 0)
    p.done()
    p = Part("Bridge")
    props.bridge(p, 0, 0, 0)
    p.done()
    for v, seed in (("a", 1), ("b", 2)):
        rng = random.Random(seed)
        p = Part(f"Rock_{v}")
        rounded_cube(p.bm, (0, 0, 0.25), (0.8, 0.6, 0.5), yaw=0.3, round_frac=0.3, segments=1, mat=p.m("choco_dark"))
        rounded_cube(p.bm, (0.35, 0.2, 0.15), (0.45, 0.4, 0.3), yaw=rng.uniform(0, 1), round_frac=0.3, segments=1,
                     mat=p.m("choco_dark"))
        p.done()
    p = Part("Cherry")
    ball(p.bm, (0, 0, 0.55), 0.62, (1, 1, 0.92), subdiv=2, mat=p.m("candy_red"))
    kit.cylinder(p.bm, (0, 0, 1.0), (0.35, -0.1, 1.9), 0.06, mat=p.m("pistachio"))
    p.done()
    p = Part("FoamRing")
    torus(p.bm, (0, 0, 0.02), 1.0, 0.14, segs=12, ring_segs=4, flatten=0.5, mat=p.m("foam"))
    p.done()
    p = Part("Bubble")
    ball(p.bm, (0, 0, 0.05), 0.15, mat=p.m("foam"))
    p.done()
    for name, color in (("cream", "cream"), ("syrup", "syrup_light")):
        p = Part(f"SeaSwirl_{name}")
        torus(p.bm, (0, 0, 0.02), 1.0, 0.07, segs=10, ring_segs=4, arc=2.4, flatten=0.35, mat=p.m(color))
        p.done()


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    kit.use_collection("Kit")
    cubes()
    steps()
    platforms()
    small_pieces()


def export(path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.data.collections["Kit"].objects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_apply=True)


def contact_sheet(path):
    """Lay the pieces out in a grid (cubes shown 2 m) and render one picture of the whole kit."""
    objs = sorted(bpy.data.collections["Kit"].objects, key=lambda o: o.name)
    cols = 8
    for i, obj in enumerate(objs):
        obj.location = ((i % cols) * 5.0, (i // cols) * 9.0, 3.0 if obj.name.startswith(("Platform", "Drip")) else 0)
        if obj.name.startswith("Cube"):
            obj.scale = (2, 2, 2)
        if obj.name.startswith(("Platform", "FoamRing", "SeaSwirl")):
            obj.scale = (1.5, 1.5, 1.5)
    scene = bpy.context.scene
    build_island = __import__("build_choco_island")
    scene.world = build_island._world()
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("View", bpy.data.cameras.new("View"))
    cam.data.type, cam.data.ortho_scale = "ORTHO", 46
    scene.collection.objects.link(cam)
    scene.camera = cam
    rows = (len(objs) + cols - 1) // cols
    target = Vector(((cols - 1) * 2.5, (rows - 1) * 4.5, 1.5))
    cam.location = target + Vector((0, -1.0, 0.75)).normalized() * 80
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    build()
    os.makedirs(os.path.join(HERE, "godot_demo"), exist_ok=True)
    export(os.path.join(HERE, "godot_demo", "choco_kit.glb"))
    print("KIT PIECES:", ", ".join(sorted(o.name for o in bpy.data.collections["Kit"].objects)))
    contact_sheet(os.path.join(HERE, "choco_kit_sheet.png"))
