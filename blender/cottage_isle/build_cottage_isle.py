"""Cottage Isle — Hari's cat-cottage painting as a 3D diorama for the game.

A floating island with a stream cutting across it (falling off the front edge), a cottage with a
pink fish-scale roof and a cat-eared door, a paw-print plaza, flagstone paths, an arched bridge,
a heart-fruit tree, a garden, a clothesline and yarn balls. Units are metres, Z up, the camera
looks from the front-left like the painting.

Files: isle_kit (materials, primitives) · isle_terrain (islands, stream) · isle_cottage ·
isle_props (everything else) · isle_textures (painted detail maps, box UVs) · isle_scatter
(geometry-nodes grass, flowers, pebbles, cliff rocks, leaf cards) · isle_paint (vertex colours,
watercolour toon shading). Run in Blender:
    ns = runpy.run_path(".../build_cottage_isle.py"); ns["build"](); ns["export"](folder)
The .glb holds the "Isle" collection; objects ending in -col become colliders in Godot.
"""
import importlib
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

import isle_textures  # noqa: E402
import isle_kit  # noqa: E402
import isle_terrain  # noqa: E402
import isle_cottage  # noqa: E402
import isle_props  # noqa: E402
import isle_paint  # noqa: E402
import isle_scatter  # noqa: E402

for module in (isle_textures, isle_kit, isle_terrain, isle_cottage, isle_props, isle_paint, isle_scatter):
    importlib.reload(module)

# --- layout (metres, island top at z = 0) --------------------------------------------------------
ISLE = dict(name="Isle", center=(0, 0), radius=4.8, top=0.0, depth=2.6, seed=3)
STREAM = [(3.35, 5.0), (3.1, 3.7), (3.0, 2.5), (2.75, 1.3), (2.45, 0.0), (2.05, -1.3), (1.65, -2.6), (1.45, -3.9), (1.35, -5.6)]
STREAM_WIDTH = 1.0
SPRING_RADIUS = 0.0  # the stream flows in over the back edge
COTTAGE = ((-0.1, 1.55), -15)  # (position, yaw in degrees)
PLAZA = ((-0.35, -0.75), 1.35)
BRIDGE_NEAR = (2.05, -1.3)
FRONT_PATH = [(-0.9, -1.9), (-1.3, -2.9), (-1.75, -3.9), (-2.1, -5.0)]
TREE = (-2.0, 2.7)
BASKET = (-1.35, 1.95)
GARDEN = ((-2.9, 0.35), 28)
CLOTHESLINE = ((-3.7, -1.3), (-2.75, -2.25))
YARN = [("Yarn.Blue", (-1.95, -1.05), "yarn_blue"), ("Yarn.Orange", (1.2, -0.05), "yarn_orange"), ("Yarn.Pink", (1.55, 0.5), "yarn_pink")]
STUMPS = [(3.95, 2.3, 0.4), (4.1, 1.75, 0.3), (2.05, 2.45, 0.45), (2.15, 2.0, 0.32)]
FERNS = [(3.75, 0.35), (4.05, -0.45), (3.55, -0.05), (3.9, 2.85)]

BACKGROUND = (0.91, 0.76, 0.78)
AMBIENT = 0.3
SUN = 2.6


def _keep_out(stream_line, paths):
    """True if a thing of radius r at (x, y) clears the water, house, plaza, paths, tree and garden."""
    house, (plaza_at, plaza_r) = Vector(COTTAGE[0]), PLAZA

    def ok(x, y, r):
        p = Vector((x, y))
        if isle_props.dist_to_path((x, y), stream_line) < STREAM_WIDTH / 2 + 0.25 + r:
            return False
        if SPRING_RADIUS and (p - Vector(stream_line[0])).length < SPRING_RADIUS + 0.3 + r:
            return False
        if (p - house).length < 2.0 + r or (p - Vector(plaza_at)).length < plaza_r + 0.2 + r:
            return False
        if any(isle_props.dist_to_path((x, y), line) < 0.7 + r for line in paths):
            return False
        return (p - Vector(TREE)).length > 0.6 + r and (p - Vector(GARDEN[0])).length > 1.6 + r
    return ok


SCATTER_MASK = ["FrontPath.Sand", "BridgePath.Sand", "FarPath.Sand", "Plaza.Base", "Cottage.Footing-col",
                "Cottage.Step", "Water", "Garden.Soil", "Tree.Trunk", "Stumps", "Basket", "Bridge.Deck-col",
                "Yarn.Blue", "Yarn.Orange", "Yarn.Pink", "Clothesline", "Ferns"]


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for coll in list(bpy.data.collections):
        bpy.data.collections.remove(coll)
    for group in list(bpy.data.node_groups):
        bpy.data.node_groups.remove(group)
    for mat in list(bpy.data.materials):
        bpy.data.materials.remove(mat)
    isle_textures.make_textures(os.path.join(HERE, "textures"))
    isle_kit.use_collection("Isle")
    isle = isle_terrain.Isle(**ISLE)
    grass = isle_terrain.make_isle(isle)
    stream_line = isle_terrain.make_stream(isle, grass, STREAM, STREAM_WIDTH, SPRING_RADIUS, "Helpers")
    bpy.data.collections["Helpers"].hide_render = True

    isle_cottage.make_cottage((*COTTAGE[0], 0), COTTAGE[1])
    isle_props.plaza(PLAZA[0], PLAZA[1], math.radians(90 + COTTAGE[1]))
    i = min(range(len(stream_line)), key=lambda k: (stream_line[k] - Vector(BRIDGE_NEAR)).length)
    across = isle_terrain.side_of(stream_line, i)
    centre = stream_line[i]
    isle_props.bridge(centre, across)
    west, east = centre - across.to_2d() * 1.45, centre + across.to_2d() * 1.45
    plaza_at = Vector(PLAZA[0])
    rim = lambda toward: plaza_at + (Vector(toward) - plaza_at).normalized() * (PLAZA[1] - 0.25)
    paths = [
        isle_props.path("FrontPath", [rim(FRONT_PATH[0])] + FRONT_PATH, isle, seed=1),
        isle_props.path("BridgePath", [rim(west), rim(west).lerp(west, 0.5), west], isle, seed=2),
        isle_props.path("FarPath", [east, east + Vector((0.5, -0.9)), east + Vector((0.8, -2.2))], isle, seed=3),
    ]
    keep_out = _keep_out(stream_line, paths)
    isle_props.tree(TREE)
    isle_props.basket(BASKET)
    isle_props.garden(*GARDEN)
    isle_props.clothesline(*CLOTHESLINE)
    for name, at, color in YARN:
        isle_props.yarn(name, at, color, seed=len(name))
    isle_props.stumps(STUMPS)
    isle_props.ferns(FERNS)
    isle_props.tufts(isle, keep_out)
    waterfall = ([stream_line[0] + (stream_line[0] - stream_line[1]).normalized() * 1.5] + stream_line
                 + [stream_line[-1] + (stream_line[-1] - stream_line[-2]).normalized() * 1.5])
    isle_terrain.grass_lip(isle, lambda x, y: isle_props.dist_to_path((x, y), waterfall) > STREAM_WIDTH / 2 + 0.35)
    isle_scatter.scatter_scene(isle, SCATTER_MASK)
    isle_paint.paint_scene(isle)
    meshes = list(bpy.data.collections["Isle"].all_objects) + list(bpy.data.collections["Helpers"].all_objects)
    for kinds in [c for c in bpy.data.collections if c.name.startswith("Kinds.")]:
        meshes += list(kinds.objects)
    isle_textures.uv_everything(meshes)
    isle_paint.toon_scene()
    _look()


def _world_nodes(world):
    """The camera sees the pink backdrop; the scene is lit by a dimmer, warmer sky (so it isn't washed out)."""
    nodes, links = world.node_tree.nodes, world.node_tree.links
    out = nodes["World Output"]
    seen = nodes["Background"]
    seen.inputs["Color"].default_value = (*BACKGROUND, 1)
    seen.inputs["Strength"].default_value = 1.0
    light = nodes.new("ShaderNodeBackground")
    light.inputs["Color"].default_value = (0.92, 0.92, 0.95, 1)
    light.inputs["Strength"].default_value = AMBIENT
    ray = nodes.new("ShaderNodeLightPath")
    mix = nodes.new("ShaderNodeMixShader")
    links.new(ray.outputs["Is Camera Ray"], mix.inputs["Fac"])
    links.new(light.outputs["Background"], mix.inputs[1])
    links.new(seen.outputs["Background"], mix.inputs[2])
    links.new(mix.outputs["Shader"], out.inputs["Surface"])


def _look():
    scene = bpy.context.scene
    world = bpy.data.worlds.new("Pink")
    world.use_nodes = True
    _world_nodes(world)
    scene.world = world
    isle_kit.use_collection("Stage")
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy, sun.angle, sun.color = SUN, math.radians(8), (1.0, 0.95, 0.88)
    sun_obj = bpy.data.objects.new("Sun", sun)
    isle_kit._target.objects.link(sun_obj)
    sun_obj.rotation_euler = (math.radians(45), 0, math.radians(-40))
    cam_data = bpy.data.cameras.new("View")
    cam_data.type, cam_data.ortho_scale = "ORTHO", 14.5
    cam = bpy.data.objects.new("View", cam_data)
    isle_kit._target.objects.link(cam)
    target = Vector((0.4, -0.2, 0.6))
    cam.location = target + Vector((-0.42, -1.0, 0.82)).normalized() * 40
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.view_settings.view_transform = "Standard"

    scene.render.resolution_x, scene.render.resolution_y = 1600, 1230


def render(path, size=(1000, 770)):
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "cottage_isle.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.data.collections["Isle"].all_objects:
        obj.select_set(True)
    isle_paint.set_toon(False)  # plain PBR + vertex colours in the glb; Godot does its own shading
    try:
        bpy.ops.export_scene.gltf(
            filepath=os.path.join(folder, "cottage_isle.glb"),
            use_selection=True,
            export_apply=True,
            export_extras=True,
            export_vertex_color="ACTIVE",
        )
    finally:
        isle_paint.set_toon(True)
