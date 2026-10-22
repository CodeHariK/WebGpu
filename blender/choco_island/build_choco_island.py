"""Choco Island: a low-poly candy island in a chocolate syrup sea.

    Blender -b --python build_choco_island.py      → choco_island.blend, .glb and preview pngs here

The island is a frosted layer cake. At the back a mountain of rounded cake cubes of different sizes
is stacked up to a cherry on top. Syrup wells up in a pond on the big front block, falls down its
face into a pool, runs across the island as a stream (under a wafer bridge) and falls off the edge
into the sea. Floating cake slices hang in the air around it. Candy trees, canes and gumdrops dot the
frosting."""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import choco_kit  # noqa: E402
import choco_props  # noqa: E402
import choco_syrup  # noqa: E402
import choco_terrain  # noqa: E402

SKY = (0.98, 0.78, 0.86)
SUN = 3.2
AMBIENT = 0.75


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    choco_terrain.island(skip_drips=(choco_syrup.END_ANGLE,))
    choco_terrain.mountain()
    plats = choco_terrain.platforms()
    choco_syrup.sea()
    choco_syrup.falls_and_stream()
    choco_props.props(choco_syrup.stream_line(), plats)
    _look()


def _world():
    world = bpy.data.worlds.new("Pink")
    world.use_nodes = True
    nodes, links = world.node_tree.nodes, world.node_tree.links
    seen = nodes["Background"]
    seen.inputs["Color"].default_value = (*choco_kit.to_linear(SKY), 1)
    seen.inputs["Strength"].default_value = 1.0
    light = nodes.new("ShaderNodeBackground")
    light.inputs["Color"].default_value = (*choco_kit.to_linear((1.0, 0.9, 0.93)), 1)
    light.inputs["Strength"].default_value = AMBIENT
    ray = nodes.new("ShaderNodeLightPath")
    mix = nodes.new("ShaderNodeMixShader")
    links.new(ray.outputs["Is Camera Ray"], mix.inputs["Fac"])
    links.new(light.outputs["Background"], mix.inputs[1])
    links.new(seen.outputs["Background"], mix.inputs[2])
    links.new(mix.outputs["Shader"], nodes["World Output"].inputs["Surface"])
    return world


def _look():
    scene = bpy.context.scene
    scene.world = _world()
    choco_kit.use_collection("Stage")
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy, sun.angle, sun.color = SUN, math.radians(10), (1.0, 0.95, 0.88)
    sun_obj = bpy.data.objects.new("Sun", sun)
    choco_kit._target.objects.link(sun_obj)
    sun_obj.rotation_euler = (math.radians(50), 0, math.radians(-35))
    cam = bpy.data.objects.new("View", bpy.data.cameras.new("View"))
    cam.data.lens = 30
    choco_kit._target.objects.link(cam)
    scene.camera = cam
    aim(Vector((10, -48, 19)), Vector((0, 1, 4)))
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.view_settings.view_transform = "Standard"


def aim(loc, target):
    cam = bpy.context.scene.camera
    cam.location = loc
    cam.rotation_euler = (target - loc).to_track_quat("-Z", "Y").to_euler()


def render(path, size=(1600, 900)):
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "choco_island.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    for name in ("Island", "Mountain", "Platforms", "Syrup", "Props"):
        for obj in bpy.data.collections[name].all_objects:
            obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(folder, "choco_island.glb"), use_selection=True, export_apply=True)


if __name__ == "__main__":
    build()
    export(HERE)
    render(os.path.join(HERE, "choco_island_preview.png"))
    aim(Vector((-40, -26, 26)), Vector((0, 3, 5)))
    render(os.path.join(HERE, "choco_island_preview_side.png"))
    aim(Vector((10, -48, 19)), Vector((0, 1, 4)))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "choco_island.blend"))
