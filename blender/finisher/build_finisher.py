"""Finisher — a cartoon takedown in the spirit of the brawler-game finishers (slow-motion last hit,
camera push-in): Pip dashes in and lands a flying punch on the Grunt, who crashes onto his back.

Files: finisher_rig (skeleton, parts, posing, fix-ups) · finisher_cast (Pip, Grunt) ·
finisher_moves (the pose tables) · finisher_stage (ground, camera, impact star). Pip's skeleton
and body come from ../walk_cycle/build_walker.py.

In Blender: ns = runpy.run_path(".../build_finisher.py"); ns["build"](); ns["export"](folder)
The glb holds both characters and one animation with both actions (camera and set are left out).
"""
import importlib
import os
import sys

import bpy

HERE = os.path.dirname(os.path.abspath(__file__))
for path in (HERE, os.path.join(HERE, "..", "walk_cycle")):
    if path not in sys.path:
        sys.path.insert(0, path)

import build_walker  # noqa: E402
import finisher_rig  # noqa: E402
import finisher_cast  # noqa: E402
import finisher_moves  # noqa: E402
import finisher_stage  # noqa: E402

for module in (build_walker, finisher_rig, finisher_cast, finisher_moves, finisher_stage):
    importlib.reload(module)

PIP_START = (0, 3.2, 0)  # Pip faces −Y, towards the Grunt
GRUNT_AT = (0, 0, 0)  # turned round to face Pip


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    pip = finisher_cast.make_pip(PIP_START)
    grunt = finisher_cast.make_grunt(GRUNT_AT, turn=180)
    contact = finisher_moves.animate(pip, grunt)
    finisher_stage.build_set(contact)
    return pip, grunt


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "finisher.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    for name in ("Pip", "Grunt"):
        arm = bpy.data.objects[name]
        arm.select_set(True)
        for child in arm.children:
            child.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(folder, "finisher.glb"),
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIVE_ACTIONS",
        export_extras=True,
    )
