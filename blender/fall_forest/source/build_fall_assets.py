"""Build FallAssetsNature.blend: every mesh from AllFallAssets.fbx, textured with FallTex.png,
scaled to metres and laid out in labelled rows by kind, each marked as an asset.

blender -b --factory-startup --python build_fall_assets.py -- <folder with the fbx and png>
"""
import os
import sys

import bpy
from mathutils import Vector

FOLDER = sys.argv[sys.argv.index("--") + 1]
SCALE = 0.1  # the FBX is in large units (a tree 59 tall); 0.1 makes a tree ~6 m, a pumpkin ~1 m
GAP = 0.4  # metres between neighbours in a row
ROW_GAP = 1.2  # metres between rows

# rows, front to back: (collection / label, name prefixes)
ROWS = [
	("Leaves", ["Leaf_"]),
	("Acorns & Pinecones", ["Acorn", "Pinecone"]),
	("Mushrooms Flat", ["Mushroom_Flat_"]),
	("Mushrooms Red", ["Mushroom_Red_"]),
	("Mushrooms Round", ["Mushroom_Round_"]),
	("Pumpkins & Gourds", ["Pumpkin", "Gourd"]),
	("Bushes", ["Bush"]),
	("Trees", ["Tree"]),
]


def clear_scene():
	bpy.ops.wm.read_factory_settings(use_empty=True)


def import_meshes():
	bpy.ops.import_scene.fbx(filepath=os.path.join(FOLDER, "AllFallAssets.fbx"))
	return [o for o in bpy.data.objects if o.type == "MESH"]


def make_material():
	"""One material for all: FallTex.png (64 x 64 gradient strips the UVs pick colours from)."""
	material = bpy.data.materials.new("FallTex")
	material.use_nodes = True
	nodes = material.node_tree.nodes
	links = material.node_tree.links
	bsdf = nodes["Principled BSDF"]
	bsdf.inputs["Roughness"].default_value = 0.9
	bsdf.inputs["Specular IOR Level"].default_value = 0.2
	image = bpy.data.images.load(os.path.join(FOLDER, "FallTex.png"))
	image.filepath = "//FallTex.png"  # beside the .blend
	texture = nodes.new("ShaderNodeTexImage")
	texture.image = image
	texture.interpolation = "Linear"  # the strips are gradients: blended, not stepped
	texture.location = (-400, 300)
	links.new(texture.outputs["Color"], bsdf.inputs["Base Color"])
	return material


def fix_mesh(obj, material):
	"""Metre scale baked into the mesh (object scale stays 1), the one material, smooth off."""
	obj.data.transform(obj.matrix_basis.to_3x3().to_4x4() @ _scale_matrix())
	obj.matrix_basis.identity()
	obj.data.materials.clear()
	obj.data.materials.append(material)


def _scale_matrix():
	from mathutils import Matrix
	return Matrix.Scale(SCALE, 4)


def bounds(obj):
	xs = [v.co.x for v in obj.data.vertices]
	ys = [v.co.y for v in obj.data.vertices]
	return Vector((min(xs), min(ys))), Vector((max(xs), max(ys)))


def add_label(text, location, size, collection):
	curve = bpy.data.curves.new(text, "FONT")
	curve.body = text
	curve.size = size
	curve.align_x = "RIGHT"
	curve.align_y = "CENTER"
	label = bpy.data.objects.new("Label " + text, curve)
	label.location = location
	collection.objects.link(label)
	return label


def lay_out(objects):
	root = bpy.data.collections.new("FallAssets")
	bpy.context.scene.collection.children.link(root)
	display = bpy.data.collections.new("Display")
	bpy.context.scene.collection.children.link(display)
	row_y = 0.0
	for title, prefixes in ROWS:
		members = sorted([o for o in objects if any(o.name.startswith(p) for p in prefixes)], key=lambda o: o.name)
		collection = bpy.data.collections.new(title)
		root.children.link(collection)
		spans = [bounds(o) for o in members]
		widths = [mx.x - mn.x for mn, mx in spans]
		depth = max(mx.y - mn.y for mn, mx in spans)
		total = sum(widths) + GAP * (len(members) - 1)
		left = -total / 2.0
		row_middle = row_y + depth / 2.0
		for obj, (mn, mx), width in zip(members, spans, widths):
			for c in obj.users_collection:
				c.objects.unlink(obj)
			collection.objects.link(obj)
			obj.location = (left - mn.x, row_middle - (mn.y + mx.y) / 2.0, 0.0)
			left += width + GAP
			obj.asset_mark()
			obj.asset_data.tags.new(title)
		add_label(title, (-total / 2.0 - 0.5, row_middle, 0.01), max(0.25, min(depth * 0.35, 1.2)), display)
		row_y += depth + ROW_GAP
	return row_y, display


def add_stage(depth, display):
	bpy.ops.mesh.primitive_plane_add(size=1.0, location=(0.0, depth / 2.0 - ROW_GAP / 2.0, -0.02))
	ground = bpy.context.active_object
	ground.name = "Ground"
	ground.scale = (40.0, depth + 4.0, 1.0)
	ground_material = bpy.data.materials.new("Ground")
	ground_material.diffuse_color = (0.33, 0.27, 0.2, 1.0)
	ground_material.use_nodes = True
	ground_material.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.33, 0.27, 0.2, 1.0)
	ground.data.materials.append(ground_material)
	for c in ground.users_collection:
		c.objects.unlink(ground)
	display.objects.link(ground)

	sun_data = bpy.data.lights.new("Sun", "SUN")
	sun_data.energy = 3.0
	sun_data.angle = 0.1
	sun = bpy.data.objects.new("Sun", sun_data)
	sun.rotation_euler = (0.75, 0.2, -0.6)
	display.objects.link(sun)

	camera_data = bpy.data.cameras.new("Camera")
	camera_data.lens = 28
	camera = bpy.data.objects.new("Camera", camera_data)
	camera.location = (0.0, -15.5, 16.0)
	camera.rotation_euler = (1.05, 0.0, 0.0)
	display.objects.link(camera)
	bpy.context.scene.camera = camera

	world = bpy.data.worlds.new("World")
	world.use_nodes = True
	world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.62, 0.72, 1.0)
	world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
	bpy.context.scene.world = world


def clean_up():
	for material in list(bpy.data.materials):
		if material.name == "Material":
			bpy.data.materials.remove(material)
	for image in list(bpy.data.images):
		if image.name != "FallTex.png":
			bpy.data.images.remove(image)
	bpy.context.scene.unit_settings.system = "METRIC"
	bpy.context.scene.unit_settings.scale_length = 1.0


def main():
	clear_scene()
	material = make_material()
	objects = import_meshes()
	for obj in objects:
		fix_mesh(obj, material)
	depth, display = lay_out(objects)
	add_stage(depth, display)
	clean_up()
	scene = bpy.context.scene
	scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items.keys() else "BLENDER_EEVEE"
	scene.render.resolution_x = 1600
	scene.render.resolution_y = 900
	scene.view_settings.view_transform = "Standard"  # the palette's colours as painted (AgX washes them out)
	out = os.path.join(FOLDER, "FallAssetsNature.blend")
	bpy.ops.wm.save_as_mainfile(filepath=out, relative_remap=True)
	scene.render.filepath = os.path.join(FOLDER, "FallAssetsNature_preview.png")
	bpy.ops.render.render(write_still=True)
	print("saved", out, len(objects), "meshes")


main()
