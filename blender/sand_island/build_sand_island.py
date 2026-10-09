"""Sand Island — a small beach island for the stylized sand and water shaders.

Terrain: one heightfield grid (STEP metres) shaped by a radial profile with wobbly edges:
  grass plateau (set back from the front) → steep earth bank → wide gentle beach → seabed.
Water: a flat grid at WATER_Z whose vertex colour stores how deep the water is there
(R = depth / MAX_DEPTH), so the Godot water shader can draw shallow→deep colour and shore foam
without a depth texture (works on Mobile and Compatibility renderers).
Props: two palm trees and a few shells/starfish.

The look comes from the Godot shaders (sand.gdshader, water.gdshader); Blender only gets
flat preview colours. Export: sand_island.glb (Terrain-col, Water, Palms, Shells).
Run in Blender: ns = runpy.run_path(".../build_sand_island.py"); ns["build"](); ns["export"](folder)
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

STEP = 0.3         # terrain grid
WATER_STEP = 0.5   # the water only carries a smooth depth value, so it can be coarser
HALF = 16.0  # terrain covers ±HALF metres
WATER_Z = 0.0
MAX_DEPTH = 2.0
PLATEAU = dict(centre=(0.0, 3.0), radius=5.0, height=1.7)
BANK_WIDTH = 0.8
BEACH = dict(radius=10.5, top=0.75, edge=-0.4)  # beach starts at the bank foot (top) and slopes to edge
SEABED = -2.2


def _wobble(angle, seed, amount):
    rng = np.random.default_rng(seed)
    out = np.ones_like(angle)
    for k in (2, 3, 5, 7):
        out += amount * rng.uniform(0.4, 1.0) / k ** 0.5 * np.sin(k * angle + rng.uniform(0, math.tau))
    return out


def height(x, y):
    """Terrain height at world (x, y) — numpy arrays in, array out."""
    px, py = x - PLATEAU["centre"][0], y - PLATEAU["centre"][1]
    plateau_edge = PLATEAU["radius"] * _wobble(np.arctan2(py, px), 1, 0.12)
    to_plateau = np.hypot(px, py) - plateau_edge  # < 0 inside the grass
    beach_edge = BEACH["radius"] * _wobble(np.arctan2(y, x), 2, 0.08)
    t_beach = np.clip(np.hypot(x, y) / beach_edge, 0, 1.6)

    beach = BEACH["top"] + (BEACH["edge"] - BEACH["top"]) * np.clip(t_beach, 0, 1) ** 1.6
    beach = np.where(t_beach > 1, BEACH["edge"] + (SEABED - BEACH["edge"]) * np.clip((t_beach - 1) / 0.5, 0, 1) ** 0.8, beach)
    bank = np.clip(1 - to_plateau / BANK_WIDTH, 0, 1)
    bank = bank * bank * (3 - 2 * bank)
    lumps = 0.05 * np.sin(x * 1.3) * np.cos(y * 1.1)
    return beach + (PLATEAU["height"] - beach) * bank + lumps * (bank > 0.99)  # the bank rises from the beach to the grass


def _grid(name, z_of, colour_of=None, step=STEP):
    xs = np.arange(-HALF, HALF + 1e-6, step)
    X, Y = np.meshgrid(xs, xs)
    Z = z_of(X, Y)
    bm = bmesh.new()
    n = len(xs)
    verts = [bm.verts.new((float(X[r, c]), float(Y[r, c]), float(Z[r, c]))) for r in range(n) for c in range(n)]
    for r in range(n - 1):
        for c in range(n - 1):
            a, b, d, e = r * n + c, r * n + c + 1, (r + 1) * n + c + 1, (r + 1) * n + c
            # split each quad along the diagonal that follows the slope better (fewer ugly creases)
            if abs(Z[r, c] - Z[r + 1, c + 1]) < abs(Z[r, c + 1] - Z[r + 1, c]):
                bm.faces.new((verts[a], verts[b], verts[d]))
                bm.faces.new((verts[a], verts[d], verts[e]))
            else:
                bm.faces.new((verts[a], verts[b], verts[e]))
                bm.faces.new((verts[b], verts[d], verts[e]))
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    if colour_of is not None:
        layer = mesh.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        values = colour_of(X, Y).ravel()
        for i, v in enumerate(values):
            layer.data[i].color = (float(v), 0.0, 0.0, 1.0)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def _flat_material(name, rgb):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*rgb, 1)
    mat.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
    mat.diffuse_color = (*rgb, 1)
    return mat


def terrain():
    obj = _grid("Terrain-col", height)
    obj.data.materials.append(_flat_material("M_Sand", (0.8, 0.62, 0.4)))
    return obj


def water():
    depth = lambda X, Y: np.clip((WATER_Z - height(X, Y)) / MAX_DEPTH, 0, 1)
    obj = _grid("Water", lambda X, Y: np.full_like(X, WATER_Z), depth, WATER_STEP)
    obj.data.materials.append(_flat_material("M_Water", (0.1, 0.45, 0.75)))
    return obj


# --- props -------------------------------------------------------------------------------------

def _palm(bm_trunk, bm_leaves, base, lean, seed):
    rng = np.random.default_rng(seed)
    pos = Vector(base)
    direction = Vector((math.sin(lean[0]), math.sin(lean[1]), 1)).normalized()
    for k in range(7):  # trunk: stacked tapered rings that bend over
        r = 0.2 - k * 0.015
        seg = 0.55
        rot = Vector((0, 0, 1)).rotation_difference(direction).to_matrix().to_4x4()
        bmesh.ops.create_cone(bm_trunk, cap_ends=True, segments=8, radius1=r, radius2=r * 0.82, depth=seg,
                              matrix=Matrix.Translation(pos + direction * seg / 2) @ rot)
        pos = pos + direction * seg * 0.92
        direction = (direction + Vector((lean[0], lean[1], 0)) * 0.12).normalized()
    top = pos
    for k in range(7):  # fronds: long arched leaves
        a = math.tau * k / 7 + rng.uniform(-0.2, 0.2)
        out = Vector((math.cos(a), math.sin(a), 0))
        side = Vector((-out.y, out.x, 0))
        prev = None
        for i in range(6):
            t = i / 5
            centre = top + out * (t * 2.0) + Vector((0, 0, 0.35 * math.sin(t * math.pi * 0.8) - 0.9 * t * t))
            w = 0.38 * math.sin(math.pi * min(t * 1.1, 1.0)) + 0.03
            row = (bm_leaves.verts.new(centre - side * w), bm_leaves.verts.new(centre + Vector((0, 0, 0.06))),
                   bm_leaves.verts.new(centre + side * w))
            if prev:
                bm_leaves.faces.new((prev[0], prev[1], row[1], row[0]))
                bm_leaves.faces.new((prev[1], prev[2], row[2], row[1]))
            prev = row
    bmesh.ops.create_icosphere(bm_trunk, subdivisions=1, radius=0.16, matrix=Matrix.Translation(top + Vector((0.12, 0.1, -0.15))))


def props():
    trunk, leaves = bmesh.new(), bmesh.new()
    for base, lean, seed in (((2.5, 4.5), (0.25, -0.1), 1), ((-3.2, 2.0), (-0.2, -0.25), 2)):
        _palm(trunk, leaves, (base[0], base[1], float(height(np.array(base[0]), np.array(base[1])))), lean, seed)
    for name, bm, rgb in (("Palms.Trunk", trunk, (0.55, 0.36, 0.2)), ("Palms.Leaves", leaves, (0.3, 0.62, 0.25))):
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        mesh.materials.append(_flat_material(f"M_{name}", rgb))
    shells = bmesh.new()
    rng = np.random.default_rng(4)
    for _ in range(9):
        a, r = rng.uniform(math.pi * 1.1, math.pi * 1.9), rng.uniform(6.0, 8.8)
        x, y = r * math.cos(a), r * math.sin(a)
        z = float(height(np.array(x), np.array(y)))
        if z < 0.05:
            continue
        bmesh.ops.create_cone(shells, cap_ends=True, segments=10, radius1=0.12, radius2=0.0, depth=0.08,
                              matrix=Matrix.Translation((x, y, z + 0.03)) @ Matrix.Diagonal((1.0, 0.8, 1.0, 1.0)))
    mesh = bpy.data.meshes.new("Shells")
    shells.to_mesh(mesh)
    shells.free()
    obj = bpy.data.objects.new("Shells", mesh)
    bpy.context.scene.collection.objects.link(obj)
    mesh.materials.append(_flat_material("M_Shell", (0.98, 0.8, 0.78)))


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    terrain()
    water()
    props()


def export(folder):
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "sand_island.blend"), copy=True)
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.export_scene.gltf(filepath=os.path.join(folder, "sand_island.glb"), use_selection=True,
                              export_apply=True, export_texcoords=False, export_vertex_color="ACTIVE")
