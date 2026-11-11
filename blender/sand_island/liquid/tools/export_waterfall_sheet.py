"""waterfall_simple/waterfall_sheet.glb: one waterfall sheet of the stylized waterfall (make waterfall-sheet).

blender -b stylized_waterfall/source/stylizedwaterfall.blend --python tools/export_waterfall_sheet.py

The sheet is "Plane.004" (material "waterfall"): a strip 9 quads long, flat at the top, bending
over the lip and hanging down. Only its Subdivision modifiers are kept (no Solidify: the shader
draws both sides; no Displace: the ripples come later), its scale is applied and it is moved to
the origin, its bottom edge at height 0. Nothing is saved in the .blend.
"""

import bpy
import os
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "waterfall_simple", "waterfall_sheet.glb")

source = bpy.data.objects["Plane.004"]
mesh = source.data.copy()
mesh.transform(Matrix.Diagonal(source.matrix_world.to_scale()).to_4x4())  # its scale, applied
low = min(v.co.z for v in mesh.vertices)
centre_x = sum(v.co.x for v in mesh.vertices) / len(mesh.vertices)
mesh.transform(Matrix.Translation(Vector((-centre_x, 0.0, -low))))  # centred, bottom at 0
sheet = bpy.data.objects.new("WaterfallSheet", mesh)
bpy.context.scene.collection.objects.link(sheet)
for md in source.modifiers:
    if md.type == "SUBSURF":
        copy = sheet.modifiers.new(md.name, "SUBSURF")
        copy.levels = md.render_levels
        copy.render_levels = md.render_levels
sheet.data.materials.clear()
for ob in bpy.context.view_layer.objects:
    ob.select_set(False)
sheet.select_set(True)
bpy.context.view_layer.objects.active = sheet
bpy.ops.export_scene.gltf(filepath=OUT, use_selection=True, export_apply=True, export_materials="NONE",
                          export_yup=True, export_animations=False)
xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
print("sheet: x %.2f .. %.2f, depth %.2f .. %.2f, height %.2f .. %.2f" % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
print("saved", OUT)
