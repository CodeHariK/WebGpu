"""Rock Cliff — a modular limestone cliff wall for the game, textured with the packed limestone set.

The piece is 8 m wide and 6 m tall with a 2.5 m deep top, facing −Y, origin at the bottom centre
of the face (it sinks 0.5 m below z = 0 to sit into terrain). The shape repeats every 8 m in X, so
pieces placed side by side join without a gap.

Shape, one height value per point on a 0.1 m grid running up the face and back over the top:
  bulge      the wall bows out in the middle, flares at the foot and leans back near the top
  slabs      Voronoi cells stretched sideways, each pushed out by its own random amount → blocky
             ledges and steps like the limestone in the texture
  strata     thin horizontal bands
  detail     the limestone height map (Blender only; it is not shipped)
The grid is coarse enough to stay under TARGET_TRIS (Decimate only kicks in above it), and world-space box UVs tile the texture
every TILE metres. Material: albedo (AO baked in) + normal map, constant roughness.

Run in Blender: ns = runpy.run_path(".../build_rock_cliff.py"); ns["build"](); ns["export"](folder)
"""
import math
import os
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "cottage_isle"))
from isle_textures import box_uv  # noqa: E402

WIDTH, HEIGHT, CAP, SINK = 8.0, 6.0, 2.5, 0.5
STEP = 0.18  # coarse enough to need no decimation; the normal map carries the fine detail
TILE = 4.0  # metres per texture repeat (WIDTH must be a multiple, so pieces tile)
TARGET_TRIS = 5000
SLAB_SIZE = (2.6, 0.95)  # Voronoi cell scale across, up
SLAB_DEPTH = 0.9
FINE = 0.15  # metres of displacement per unit of the (centred) height map
TINT = (1.0, 0.9, 0.78)  # warms the grey limestone towards the game's cartoon palette
ROUGHNESS = 0.95
TEXTURES = os.path.join(HERE, "textures")
HEIGHT_MAP = os.path.join(HERE, "..", "texture", "limestone-cliffs-bl", "limestone-cliffs_height.png")


# --- shape -------------------------------------------------------------------------------------

def _height_map():
    img = bpy.data.images.load(HEIGHT_MAP, check_existing=True)
    w, h = img.size
    px = np.empty(w * h * 4, np.float32)
    img.pixels.foreach_get(px)
    grey = px.reshape(h, w, 4)[::4, ::4, 0]
    bpy.data.images.remove(img)
    return grey - grey.mean()


def _sample(grid, u, v):
    """Bilinear, wrapping, u/v in tiles."""
    n = grid.shape[0]
    x, y = (u % 1.0) * n, (v % 1.0) * n
    x0, y0 = np.floor(x).astype(int), np.floor(y).astype(int)
    fx, fy = x - x0, y - y0
    x1, y1 = (x0 + 1) % n, (y0 + 1) % n
    x0, y0 = x0 % n, y0 % n
    top = grid[y0, x0] * (1 - fx) + grid[y0, x1] * fx
    bot = grid[y1, x0] * (1 - fx) + grid[y1, x1] * fx
    return top * (1 - fy) + bot * fy


def _slabs(x, z, rng):
    """Per-point depth of the Voronoi slab it falls in (periodic in x with period WIDTH)."""
    cols = int(WIDTH / SLAB_SIZE[0] * 2)
    rows = int((HEIGHT + SINK + 1) / SLAB_SIZE[1] * 1.3)
    seeds_x = rng.uniform(-WIDTH / 2, WIDTH / 2, cols * rows)
    seeds_z = rng.uniform(-SINK, HEIGHT + 0.5, cols * rows)
    depth = rng.uniform(0, SLAB_DEPTH, cols * rows) ** 1.3
    dx = (x[..., None] - seeds_x) / SLAB_SIZE[0]
    dx = dx - np.round(dx * SLAB_SIZE[0] / WIDTH) * WIDTH / SLAB_SIZE[0]  # wrap: the tile edges match
    dz = (z[..., None] - seeds_z) / SLAB_SIZE[1]
    nearest = np.argmin(dx * dx + dz * dz, axis=-1)
    return _soften(depth[nearest], 2)


def _soften(field, passes):
    """Box-blur a grid a few times (wrapping across X) so slab steps become rounded ledges."""
    for _ in range(passes):
        field = (field + np.roll(field, 1, 1) + np.roll(field, -1, 1)) / 3
        field = (field + np.vstack([field[:1], field[:-1]]) + np.vstack([field[1:], field[-1:]])) / 3
    return field


def face_offset(x, z, detail, rng):
    """How far the face sticks out towards −Y at (x, z)."""
    u = (x + WIDTH / 2) / WIDTH
    bulge = 0.5 * np.sin(math.pi * u) + 0.6 * np.clip(1 - z / HEIGHT, 0, 1) ** 3 - 0.45 * np.clip(z / HEIGHT, 0, 1) ** 2
    wobble = np.sin(2 * math.pi * x / WIDTH * 2 + 1.3) * 0.15
    strata = 0.05 * np.sin(2 * math.pi * z / 0.33 + 3 * np.sin(2 * math.pi * x / WIDTH * 3))
    fine = FINE * _sample(detail, x / TILE, z / TILE)
    buttress = 0.35 * np.sin(2 * math.pi * x / WIDTH * 2 + 0.7) + 0.2 * np.sin(2 * math.pi * x / WIDTH * 3 + 2.1)
    return bulge + _slabs(x, z, rng) + strata + fine + wobble * (z / HEIGHT) + buttress


def cliff_points(seed=3):
    """A grid of points: columns across X, rows up the face and then back over the top."""
    rng = np.random.default_rng(seed)
    detail = _height_map()
    xs = np.linspace(-WIDTH / 2, WIDTH / 2, round(WIDTH / STEP) + 1)  # exact ends, so pieces meet
    up = np.linspace(-SINK, HEIGHT, round((HEIGHT + SINK) / STEP) + 1)
    back = np.linspace(STEP, CAP, round(CAP / STEP))
    X, Z = np.meshgrid(xs, up)
    face = np.empty_like(X)
    face[:, :-1] = face_offset(X[:, :-1], Z[:, :-1], detail, rng)  # wrap-around maths sees each column once
    face[:, -1] = face[:, 0]
    crown = 0.25 * np.sin(2 * math.pi * xs / WIDTH + 0.4) + 0.18 * np.sin(2 * math.pi * xs / WIDTH * 3 + 1.7) + 0.08 * np.sin(2 * math.pi * xs / WIDTH * 7 + 0.3)
    lift = np.clip(Z / HEIGHT, 0, 1) ** 2 * crown[None, :]  # an uneven skyline, still periodic in X
    front = np.stack([X, -face, Z + lift], -1)
    edge = face[-1]
    t = back / CAP
    lip = np.clip(t * 5, 0, 1)
    lip = lip * lip * (3 - 2 * lip)  # the top edge rounds over
    Xb, Tb = np.meshgrid(xs, t)
    top_y = -edge[None, :] * (1 - lip[:, None]) + Tb * CAP
    top_z = HEIGHT + crown[None, :] + 0.22 * lip[:, None] + 0.4 * _sample(detail, Xb / TILE, Tb * CAP / TILE)
    top = np.stack([Xb, top_y, top_z], -1)
    grid = np.concatenate([front, top], 0)
    return _close_seam(grid)


def _close_seam(grid):
    """The last column must equal the first one moved one piece over, so neighbours meet exactly
    (the wrap-around blur treats the duplicated end column as a separate column otherwise)."""
    grid[:, -1] = grid[:, 0] + np.array([WIDTH, 0.0, 0.0])
    return grid


def make_mesh(name, grid):
    rows, cols, _ = grid.shape
    bm = bmesh.new()
    verts = [[bm.verts.new(tuple(grid[r, c])) for c in range(cols)] for r in range(rows)]
    for r in range(rows - 1):
        for c in range(cols - 1):
            bm.faces.new((verts[r][c], verts[r][c + 1], verts[r + 1][c + 1], verts[r + 1][c]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def decimate(obj, target_tris):
    tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if tris <= target_tris:
        return
    mod = obj.modifiers.new("Decimate", "DECIMATE")
    mod.ratio = min(1.0, target_tris / tris)
    bpy.context.view_layer.objects.active = obj
    window = bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window, object=obj, active_object=obj):
        bpy.ops.object.modifier_apply(modifier=mod.name)


# --- material ----------------------------------------------------------------------------------

def limestone_material(use_normal=True):
    """Albedo (AO baked in) × TINT, plus the normal map unless use_normal is False (1 texture then)."""
    mat = bpy.data.materials.new("M_Limestone" if use_normal else "M_LimestoneAlbedo")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]
    albedo = nodes.new("ShaderNodeTexImage")
    albedo.image = bpy.data.images.load(os.path.join(TEXTURES, "limestone_albedo_1024.png"))
    tint = nodes.new("ShaderNodeMix")
    tint.data_type, tint.blend_type = "RGBA", "MULTIPLY"
    next(i for i in tint.inputs if i.identifier == "Factor_Float").default_value = 1.0
    next(i for i in tint.inputs if i.identifier == "B_Color").default_value = (*TINT, 1)
    links.new(albedo.outputs["Color"], next(i for i in tint.inputs if i.identifier == "A_Color"))
    links.new(next(o for o in tint.outputs if o.identifier == "Result_Color"), bsdf.inputs["Base Color"])
    if use_normal:
        normal_tex = nodes.new("ShaderNodeTexImage")
        normal_tex.image = bpy.data.images.load(os.path.join(TEXTURES, "limestone_normal_1024.png"))
        normal_tex.image.colorspace_settings.name = "Non-Color"
        normal_map = nodes.new("ShaderNodeNormalMap")
        links.new(normal_tex.outputs["Color"], normal_map.inputs["Color"])
        links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = ROUGHNESS
    return mat


# --- scene -------------------------------------------------------------------------------------

def _stage():
    scene = bpy.context.scene
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, -6, 0))
    ground = bpy.context.object
    ground.name = "Ground"
    grass = bpy.data.materials.new("M_Ground")
    grass.diffuse_color = (0.25, 0.42, 0.12, 1)
    grass.use_nodes = True
    grass.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.25, 0.42, 0.12, 1)
    ground.data.materials.append(grass)
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    scene.collection.objects.link(sun)
    sun.data.energy, sun.data.angle = 4.0, math.radians(3)
    sun.rotation_euler = (math.radians(58), 0, math.radians(-70))  # low and from the side: shows the relief
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.55, 0.7, 0.9, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.8
    scene.world = world
    cam = bpy.data.objects.new("View", bpy.data.cameras.new("View"))
    scene.collection.objects.link(cam)
    cam.data.lens = 32
    target = Vector((4.0, 0.0, 2.8))
    cam.location = target + Vector((-0.5, -1.0, 0.18)).normalized() * 21
    cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    scene.render.engine = "BLENDER_EEVEE"
    scene.view_settings.view_transform = "AgX"


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    cliff = make_mesh("RockCliff-col", cliff_points())
    decimate(cliff, TARGET_TRIS)
    box_uv(cliff, TILE)
    cliff.data.materials.append(limestone_material())
    neighbour = bpy.data.objects.new("RockCliff.Neighbour", cliff.data)  # a second piece to show the seam
    bpy.context.scene.collection.objects.link(neighbour)
    neighbour.location.x = WIDTH
    _stage()
    return cliff


def render(path, size=(1400, 800)):
    scene = bpy.context.scene
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "rock_cliff.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.data.objects["RockCliff-col"].select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(folder, "rock_cliff.glb"), use_selection=True,
                              export_apply=True, export_image_format="AUTO")
