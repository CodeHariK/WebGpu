"""Export the fall assets for Godot: every mesh in the FallAssets collection to one glb,
each at the origin (its base pivot), no materials (Godot gives them one shared material).

blender -b FallAssetsNature.blend --python export_fall_assets.py -- <out.glb>
"""
import sys

import bpy

out = sys.argv[sys.argv.index("--") + 1]
root = bpy.data.collections["FallAssets"]
meshes = [o for o in root.all_objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for obj in meshes:
	obj.location = (0.0, 0.0, 0.0)  # only in this run: the .blend keeps its layout
	obj.select_set(True)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=True, export_materials="NONE",
		export_apply=True, export_yup=True, export_normals=True, export_texcoords=True)
print("exported", len(meshes), "meshes to", out)
