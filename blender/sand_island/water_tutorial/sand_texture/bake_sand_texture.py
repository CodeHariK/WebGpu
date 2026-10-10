"""Bake sand_texture.blend's SandTile into sand_texture/sand_ripples.png (512², RGBA, data):
  R = ripple height 0..1 · G, B = ripple slope d/dx, d/dz (height per metre, 0.5 + slope / (2·S))
  A = grain: 1 none, 0.5 dark speck, 0.25 light speck (opaque almost everywhere)
Run (from this folder, after tweaking SandPattern in the GUI and saving):
  Blender -b sand_texture.blend --python bake_sand_texture.py
The .blend is not saved by the bake, so the temporary bake nodes never end up in it.
Godot samples it at world xz / TILE. The slope is computed from the height with wrap-around
differences (the tile repeats), in Godot's texture orientation (row 0 = top = v 0)."""
import os

import bpy
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))   # this folder
SIZE = 512
TILE = 4.0        # metres per repeat (SandTile size)
S = 8.0           # slope range stored: ±8 height units per metre

tile = bpy.data.objects["SandTile"]
nt = tile.active_material.node_tree
pat = nt.nodes["SandPattern"]
rgb = nt.nodes.new("ShaderNodeCombineColor")
nt.links.new(pat.outputs["Ripple"], rgb.inputs["Red"])
nt.links.new(pat.outputs["Grain"], rgb.inputs["Green"])
nt.links.new(pat.outputs["GrainLight"], rgb.inputs["Blue"])
emit = nt.nodes.new("ShaderNodeEmission")
nt.links.new(rgb.outputs["Color"], emit.inputs["Color"])
out = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial" and n.is_active_output)
nt.links.new(emit.outputs[0], out.inputs["Surface"])

raw = bpy.data.images.new("sand_raw", SIZE, SIZE, alpha=False, float_buffer=True)
raw.colorspace_settings.name = "Non-Color"
target = nt.nodes.new("ShaderNodeTexImage")
target.image = raw
nt.nodes.active = target

scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 4
bpy.ops.object.select_all(action="DESELECT")
tile.select_set(True)
bpy.context.view_layer.objects.active = tile
bpy.ops.object.bake(type="EMIT", margin=0, use_clear=True)

px = np.array(raw.pixels[:], dtype=np.float32).reshape(SIZE, SIZE, 4)[::-1]   # row 0 = top (Godot v = 0)
height = px[:, :, 0]
grain = (px[:, :, 1] > 0.5).astype(np.float32)
light = (px[:, :, 2] > 0.5).astype(np.float32)
per_metre = SIZE / TILE
dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5 * per_metre   # along u = world x
dz = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5 * per_metre   # along v = world z
packed = np.stack([height,
                   np.clip(0.5 + dx / (2 * S), 0, 1),
                   np.clip(0.5 + dz / (2 * S), 0, 1),
                   1.0 - grain * (0.5 + 0.25 * light)], axis=-1)

img = bpy.data.images.new("sand_ripples", SIZE, SIZE, alpha=True)
img.colorspace_settings.name = "Non-Color"
img.alpha_mode = "STRAIGHT"
img.pixels = packed[::-1].ravel()          # back to Blender's bottom-up order
img.filepath_raw = os.path.join(HERE, "sand_ripples.png")
img.file_format = "PNG"
img.save()
print("slope dx min/max", float(dx.min()), float(dx.max()), "clipped %", float((np.abs(dx) > S).mean() * 100))
print("grain coverage %", float(grain.mean() * 100))
print("sand bake done")
