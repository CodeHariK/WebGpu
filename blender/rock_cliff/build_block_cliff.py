"""Block Cliff — a cliff built from many cuboids.

METHOD "cut" (default) builds each block with rock_pieces.py: stacked boxes sliced by random planes
(convex, so faces can never cross). METHOD "displace" is the first version, described below.

Each block: a cube scaled to size → Bevel (soft edges) → Subdivision (enough vertices to push
around) → Displace by a Voronoi texture (chunky cells, the big rock facets) → Displace by Clouds
(smaller lumps) → Decimate to its share of the triangle budget. The textures sample world space,
so neighbouring blocks share one continuous pattern instead of each repeating the same one.

The blocks are laid out in tiers like a weathered cliff: tall columns at the front, taller ones
set back behind them, flat slabs resting on top, and boulders at the foot, all sunk 0.5 m into the
ground. Then everything is applied and joined into one mesh (one draw call) with the stylized
rock material (stylized_rock.py in Blender, stylized_rock.gdshader in Godot): no textures, no UVs.

The piece is about 8 m wide and 7 m tall and faces −Y.
Run in Blender: ns = runpy.run_path(".../build_block_cliff.py"); ns["build"](); ns["export"](folder)
"""
import math
import os
import random
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
for path in (HERE, os.path.join(HERE, "..", "cottage_isle")):
    if path not in sys.path:
        sys.path.insert(0, path)
import importlib  # noqa: E402

import build_rock_cliff  # noqa: E402

importlib.reload(build_rock_cliff)  # Blender keeps modules between runs; pick up edits
from build_rock_cliff import render as _render, _stage  # noqa: E402
import rock_pieces  # noqa: E402
import stylized_rock  # noqa: E402

importlib.reload(rock_pieces)
importlib.reload(stylized_rock)

METHOD = "cut"  # "cut": plane-sliced layered boxes (never self-intersect) · "displace": Voronoi displacement
WIDTH = 8.0
SINK = 0.5
TRIS_PER_BLOCK = 320  # ~20 blocks → ~6k triangles for the whole cliff
TILE = 4.0
VORONOI = dict(scale=1.1, strength=0.75)  # cell size (Blender texture units ≈ metres) and push
CLOUDS = dict(scale=0.35, strength=0.14)


# --- layout ------------------------------------------------------------------------------------

def _row(rng, y, heights, widths, depth, lean):
    """Blocks side by side across the width: (centre, size, rotation) each."""
    blocks, x = [], -WIDTH / 2 - 0.3
    while x < WIDTH / 2 + 0.3:
        w = rng.uniform(*widths)
        h = rng.uniform(*heights)
        d = rng.uniform(*depth)
        centre = (x + w / 2, y + rng.uniform(-0.35, 0.35), h / 2 - SINK)
        rot = (math.radians(rng.uniform(-lean, 0)), math.radians(rng.uniform(-4, 4)), math.radians(rng.uniform(-9, 9)))
        blocks.append((centre, (w, d, h), rot))
        x += w * rng.uniform(0.7, 0.9)  # overlap a little so there are no see-through gaps
    return blocks


def layout(seed=5):
    rng = random.Random(seed)
    blocks = []
    blocks += _row(rng, 0.0, (3.2, 5.0), (1.1, 2.0), (1.6, 2.4), 6)  # front columns
    blocks += _row(rng, 1.4, (5.2, 7.0), (1.4, 2.4), (1.8, 2.6), 4)  # taller, set back
    for _ in range(4):  # slabs resting on the front tier
        w, h = rng.uniform(1.6, 2.6), rng.uniform(0.5, 0.9)
        x = rng.uniform(-WIDTH / 2 + 1, WIDTH / 2 - 1)
        top = rng.uniform(3.4, 4.6) - SINK
        blocks.append(((x, rng.uniform(0.4, 1.0), top + h / 2), (w, rng.uniform(1.4, 2.0), h),
                       (0, math.radians(rng.uniform(-6, 6)), math.radians(rng.uniform(-12, 12)))))
    for _ in range(5):  # boulders at the foot
        s = rng.uniform(0.7, 1.2)
        blocks.append(((rng.uniform(-WIDTH / 2, WIDTH / 2), rng.uniform(-2.3, -1.8), s / 2 - SINK * 0.6),
                       (s * rng.uniform(1.0, 1.4), s, s * rng.uniform(0.7, 1.0)),
                       tuple(math.radians(rng.uniform(-20, 20)) for _ in range(3))))
    return blocks


# --- one block -------------------------------------------------------------------------------------

def _textures():
    vor = bpy.data.textures.get("RockVoronoi") or bpy.data.textures.new("RockVoronoi", "VORONOI")
    vor.noise_scale = VORONOI["scale"]
    vor.distance_metric = "DISTANCE"
    vor.weight_1, vor.weight_2, vor.weight_3, vor.weight_4 = 1.0, 0.0, 0.0, 0.0
    vor.noise_intensity = 1.0
    clouds = bpy.data.textures.get("RockClouds") or bpy.data.textures.new("RockClouds", "CLOUDS")
    clouds.noise_scale = CLOUDS["scale"]
    clouds.noise_depth = 2
    return vor, clouds


def _displace(obj, texture, strength, name):
    mod = obj.modifiers.new(name, "DISPLACE")
    mod.texture, mod.texture_coords, mod.strength, mod.mid_level = texture, "GLOBAL", strength, 0.5


def make_block(name, centre, size, rot, textures):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=centre, rotation=rot, scale=size)
    obj = bpy.context.object
    obj.name = name
    window = bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window, object=obj, active_object=obj, selected_objects=[obj]):
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)  # even bevels on long blocks
    bevel = obj.modifiers.new("Bevel", "BEVEL")
    bevel.width, bevel.segments = min(size) * 0.18, 2
    sub = obj.modifiers.new("Subdivide", "SUBSURF")
    sub.levels = sub.render_levels = 3
    vor, clouds = textures
    _displace(obj, vor, VORONOI["strength"], "Voronoi")
    _displace(obj, clouds, CLOUDS["strength"], "Clouds")
    tris = _evaluated_tris(obj)
    dec = obj.modifiers.new("Decimate", "DECIMATE")
    dec.ratio = min(1.0, TRIS_PER_BLOCK / max(tris, 1))
    return obj


def _evaluated_tris(obj):
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    count = len(mesh.loop_triangles)
    evaluated.to_mesh_clear()
    return count


def merge(objects, name):
    """Apply every modifier and join the blocks into one mesh."""
    window = bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window, selected_objects=objects, selected_editable_objects=objects,
                                   active_object=objects[0], object=objects[0]):
        bpy.ops.object.convert(target="MESH")
        bpy.ops.object.join()
    cliff = objects[0]
    cliff.name = cliff.data.name = name
    for poly in cliff.data.polygons:
        poly.use_smooth = False  # crisp facets read as chiselled rock
    return cliff


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    if METHOD == "cut":
        blocks = [rock_pieces.rock_object(f"Block.{i:02d}", *spec, seed=i) for i, spec in enumerate(layout())]
    else:
        textures = _textures()
        blocks = [make_block(f"Block.{i:02d}", *spec, textures) for i, spec in enumerate(layout())]
    cliff = merge(blocks, "BlockCliff-col")
    cliff.data.materials.clear()
    cliff.data.materials.append(stylized_rock.stylized_rock_material())  # no textures, no UVs needed
    _stage()
    bpy.context.scene.view_settings.view_transform = "Standard"  # show the shader's colours as authored
    _aim_camera((0.0, 0.0, 3.0), 17)
    return cliff


def _aim_camera(target, distance):
    from mathutils import Vector
    cam = bpy.data.objects["View"]
    target = Vector(target)
    cam.location = target + Vector((-0.5, -1.0, 0.25)).normalized() * distance
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()


def render(path, size=(1400, 800)):
    _render(path, size)


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "block_cliff.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.data.objects["BlockCliff-col"].select_set(True)
    mat = bpy.data.objects["BlockCliff-col"].data.materials[0]
    stylized_rock.use_toon(mat, False)  # the glb gets a plain material; Godot uses stylized_rock.gdshader
    try:
        bpy.ops.export_scene.gltf(filepath=os.path.join(folder, "block_cliff.glb"), use_selection=True,
                                  export_apply=True, export_texcoords=False)
    finally:
        stylized_rock.use_toon(mat, True)
