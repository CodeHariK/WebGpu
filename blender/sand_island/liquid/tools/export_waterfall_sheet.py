"""waterfall_simple/: one waterfall sheet and the pool under it, from the stylized waterfall
(make waterfall-sheet).

blender -b stylized_waterfall/source/stylizedwaterfall.blend --python tools/export_waterfall_sheet.py

  waterfall_sheet.glb  "Plane.004" (material "waterfall"): a strip 9 quads long, flat at the top,
                       bending over the lip and hanging down. Only its Subdivision modifiers are
                       kept (no Solidify: the shader draws both sides; no Displace: no ripples
                       yet), its scale is applied and it is moved to the origin, its bottom edge at
                       height 0, centred across.
  waterfall_pool.glb   "Plane.005" (material "Material"): the pool the sheet falls into, one quad,
                       placed where it is in the Blender file relative to the sheet (the same
                       move, and the sheet's slight turn undone). Its UV is Blender's Generated.
Nothing is saved in the .blend.
"""

import bpy
import os
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "waterfall_simple")


def export(ob, path):
    for other in bpy.context.view_layer.objects:
        other.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.gltf(filepath=path, use_selection=True, export_apply=True, export_materials="NONE",
                              export_yup=True, export_animations=False)
    print("saved", path)


source = bpy.data.objects["Plane.004"]
mesh = source.data.copy()
mesh.transform(Matrix.Diagonal(source.matrix_world.to_scale()).to_4x4())  # its scale, applied
low = min(v.co.z for v in mesh.vertices)
centre_x = sum(v.co.x for v in mesh.vertices) / len(mesh.vertices)
offset = Vector((centre_x, 0.0, low))
mesh.transform(Matrix.Translation(-offset))  # centred, bottom at 0
sheet = bpy.data.objects.new("WaterfallSheet", mesh)
bpy.context.scene.collection.objects.link(sheet)
for md in source.modifiers:
    if md.type == "SUBSURF":
        copy = sheet.modifiers.new(md.name, "SUBSURF")
        copy.levels = md.render_levels
        copy.render_levels = md.render_levels
sheet.data.materials.clear()
export(sheet, os.path.join(OUT, "waterfall_sheet.glb"))
xs = [v.co.x for v in mesh.vertices]
ys = [v.co.y for v in mesh.vertices]
zs = [v.co.z for v in mesh.vertices]
print("sheet: x %.2f .. %.2f, depth %.2f .. %.2f, height %.2f .. %.2f" % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))

# the pool, in the sheet's frame: undo the sheet's move and turn (not its scale: applied above)
to_sheet = Matrix.Translation(-offset) @ source.matrix_world.to_quaternion().to_matrix().to_4x4().inverted() \
    @ Matrix.Translation(-source.matrix_world.translation)
pool_source = bpy.data.objects["Plane.005"]
pool_mesh = pool_source.data.copy()
pool_mesh.transform(to_sheet @ pool_source.matrix_world)
pool = bpy.data.objects.new("WaterfallPool", pool_mesh)
bpy.context.scene.collection.objects.link(pool)
pool.data.materials.clear()
export(pool, os.path.join(OUT, "waterfall_pool.glb"))
print("pool corners:", [tuple(round(c, 2) for c in v.co) for v in pool_mesh.vertices])
