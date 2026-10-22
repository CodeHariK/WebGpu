"""Toxic Factory KIT: the pieces Godot places and animates to build the polluted factory scene.

    Blender -b --python build_toxic_kit.py   → godot_demo/toxic_kit.glb  +  toxic_kit_sheet.png

Every piece sits at the origin, base on the ground (z = 0), front facing Blender -Y (Godot +Z after
the glTF axis swap). Buildings and chimneys (tf_buildings.py), trucks, barricades, barrels, pipe
parts and the smoke puff (tf_props.py). The ground, river, road, pipes and sludge are built in Godot
(toxic_mesh.gd) because their shape comes from the layout."""
import math
import os
import sys

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import tf_buildings  # noqa: E402
import tf_kit  # noqa: E402
import tf_props  # noqa: E402

SKY = (0.86, 0.8, 0.62)


def build():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    tf_kit.use_collection("Kit")
    tf_buildings.build_all()
    tf_props.build_all()


def export(path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in bpy.data.collections["Kit"].objects:
        obj.select_set(True)
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_apply=True)


def contact_sheet(path):
    """Lay the pieces out in rows by size and render one picture of the whole kit."""
    objs = sorted(bpy.data.collections["Kit"].objects, key=lambda o: -max(o.dimensions))
    x, y, row_depth = 0.0, 0.0, 0.0
    width = 60.0
    for obj in objs:
        w, d = obj.dimensions.x + 2.5, obj.dimensions.y + 2.5
        if x + w > width:
            x, y, row_depth = 0.0, y + row_depth, 0.0
        obj.location = (x + w / 2, y + d / 2, 0)
        x += w
        row_depth = max(row_depth, d)
    depth = y + row_depth
    scene = bpy.context.scene
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (*tf_kit.to_linear(SKY), 1)
    scene.world = world
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 3.0
    sun.rotation_euler = (math.radians(50), 0, math.radians(-35))
    scene.collection.objects.link(sun)
    cam = bpy.data.objects.new("View", bpy.data.cameras.new("View"))
    cam.data.type, cam.data.ortho_scale = "ORTHO", max(width, depth) * 1.05
    scene.collection.objects.link(cam)
    scene.camera = cam
    target = Vector((width / 2, depth / 2, 2.0))
    cam.location = target + Vector((0, -1.0, 0.9)).normalized() * 120
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    for engine in ("BLENDER_EEVEE", "BLENDER_EEVEE_NEXT"):
        try:
            scene.render.engine = engine
            break
        except TypeError:
            continue
    scene.view_settings.view_transform = "Standard"
    scene.render.resolution_x, scene.render.resolution_y = 1600, 1200
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


if __name__ == "__main__":
    build()
    os.makedirs(os.path.join(HERE, "godot_demo"), exist_ok=True)
    export(os.path.join(HERE, "godot_demo", "toxic_kit.glb"))
    print("KIT PIECES:", ", ".join(sorted(o.name for o in bpy.data.collections["Kit"].objects)))
    contact_sheet(os.path.join(HERE, "toxic_kit_sheet.png"))
