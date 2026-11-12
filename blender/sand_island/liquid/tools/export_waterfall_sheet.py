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
  waterfall_upper_pool.glb  "Plane.008", the pool above the falls, placed the same way
  full_layout.json     for waterfall_full.tscn (tools/build_waterfall_full.gd), in the sheet's frame
                       and Godot's axes (y up): where each of the 8 sheets goes (the sheet mesh
                       moved, turned, scaled), the foam tubes' boxes, the camera at frame 60 and
                       its focus distance
Nothing is saved in the .blend.
"""

import bpy
import json
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

# the pool above the falls
upper_source = bpy.data.objects["Plane.008"]
upper_mesh = upper_source.data.copy()
upper_mesh.transform(to_sheet @ upper_source.matrix_world)
upper = bpy.data.objects.new("WaterfallUpperPool", upper_mesh)
bpy.context.scene.collection.objects.link(upper)
upper.data.materials.clear()
export(upper, os.path.join(OUT, "waterfall_upper_pool.glb"))

# Blender's axes (z up) -> Godot's (y up): (x, y, z) -> (x, z, -y)
TO_GODOT = Matrix(((1, 0, 0, 0), (0, 0, 1, 0), (0, -1, 0, 0), (0, 0, 0, 1)))


def godot(m):
    g = TO_GODOT @ m @ TO_GODOT.inverted()
    return [g[r][c] for c in range(4) for r in range(3)]  # basis columns x, y, z, then the origin


# each sheet: the shared sheet mesh (Plane.004's, scale applied, moved by -offset) put where the
# sheet is: move back by offset, scale by its scale over Plane.004's, turn and move like it
base_scale = source.matrix_world.to_scale()
sheets = []
for name in ["Plane.007", "Plane", "Plane.004", "Plane.002", "Plane.021", "Plane.019", "Plane.020", "Plane.022"]:
    ob = bpy.data.objects[name]
    loc, rot, scale = ob.matrix_world.decompose()
    ratio = Vector((scale.x / base_scale.x, scale.y / base_scale.y, scale.z / base_scale.z))
    m = to_sheet @ Matrix.Translation(loc) @ rot.to_matrix().to_4x4() @ Matrix.Diagonal(ratio).to_4x4() @ Matrix.Translation(offset)
    sheets.append({"name": name, "upper": name in ("Plane.019", "Plane.020", "Plane.021", "Plane.022"), "transform": godot(m)})

foams = []
for name in ["NurbsPath", "NurbsPath.002", "NurbsPath.001", "NurbsPath.003", "NurbsPath.004"]:
    ob = bpy.data.objects[name]
    corners = [to_sheet @ ob.matrix_world @ Vector(c) for c in ob.bound_box]
    lo = [min(c[i] for c in corners) for i in range(3)]
    hi = [max(c[i] for c in corners) for i in range(3)]
    # Godot axes: x, z -> y, -y -> z
    foams.append({"name": name, "min": [lo[0], lo[2], -hi[1]], "max": [hi[0], hi[2], -lo[1]]})

scene = bpy.context.scene
scene.frame_set(60)
cam = scene.camera
cam_m = to_sheet @ cam.matrix_world.normalized()
focus = None
if cam.data.dof.use_dof and cam.data.dof.focus_object:
    to = cam.data.dof.focus_object.matrix_world.translation - cam.matrix_world.translation
    focus = to.dot(-cam.matrix_world.to_3x3().col[2].normalized())
layout = {"sheets": sheets, "foams": foams,
          "camera": {"transform": godot(cam_m), "fov_x": 2.0 * __import__("math").degrees(__import__("math").atan(cam.data.sensor_width / 2.0 / cam.data.lens)),
                     "focus": focus, "fstop": cam.data.dof.aperture_fstop}}
with open(os.path.join(OUT, "full_layout.json"), "w") as fh:
    json.dump(layout, fh, indent=1)
print("saved full_layout.json:", len(sheets), "sheets,", len(foams), "foams")
