"""Blender helpers for TILEABLE texture scenes (snow_texture.blend, water_texture.blend).

Tileable trick (same as sand_texture/make_sand_texture.py): every noise is sampled on a 4D torus
made from the plane's UV, (cos 2πu, sin 2πu, cos 2πv) + W = sin 2πv. Walking off one edge of the
tile lands exactly on the other edge, so ANY noise size tiles with no seam.
Noise size is given as "cells": roughly how many blobs fit across the tile, the same meaning as
the cells of the shaders' value noise (see Group.noise for the conversion to Blender's Scale).

Make:  build the node group + material + tile plane, save the .blend (make_*.py scripts).
Bake:  bake_outputs() bakes node-group outputs of a tile plane into numpy arrays (bake_*.py).
"""
import math

import bpy
import numpy as np

TAU = 2 * math.pi


# ---------------------------------------------------------------- node group building

class Group:
    """A shader node group with float inputs/outputs and small helpers to wire maths."""

    def __init__(self, name: str, inputs: list, outputs: list):
        """inputs: (name, default, min, max, description); outputs: names."""
        self.ng = bpy.data.node_groups.new(name, "ShaderNodeTree")
        for in_name, default, lo, hi, desc in inputs:
            s = self.ng.interface.new_socket(in_name, in_out="INPUT", socket_type="NodeSocketFloat")
            s.default_value, s.min_value, s.max_value, s.description = default, lo, hi, desc
        for out_name in outputs:
            self.ng.interface.new_socket(out_name, in_out="OUTPUT", socket_type="NodeSocketFloat")
        self.gin = self.node("NodeGroupInput")
        self.gout = self.node("NodeGroupOutput")
        uv = self.node("ShaderNodeTexCoord").outputs["UV"]
        sep = self.node("ShaderNodeSeparateXYZ")
        self.link(uv, sep.inputs[0])
        self.u, self.v = sep.outputs["X"], sep.outputs["Y"]
        cu = self.math("COSINE", self.math("MULTIPLY", self.u, TAU))
        su = self.math("SINE", self.math("MULTIPLY", self.u, TAU))
        cv = self.math("COSINE", self.math("MULTIPLY", self.v, TAU))
        self.sv = self.math("SINE", self.math("MULTIPLY", self.v, TAU))
        torus = self.node("ShaderNodeCombineXYZ")
        self.link(cu, torus.inputs[0])
        self.link(su, torus.inputs[1])
        self.link(cv, torus.inputs[2])
        self.torus = torus.outputs[0]

    def node(self, kind: str):
        return self.ng.nodes.new(kind)

    def link(self, a, b) -> None:
        self.ng.links.new(a, b)

    def inp(self, name: str):
        return self.gin.outputs[name]

    def out(self, name: str, socket) -> None:
        self.link(socket, self.gout.inputs[name])

    def math(self, op: str, a, b=None):
        n = self.node("ShaderNodeMath")
        n.operation = op
        for sock, val in zip(n.inputs, (a, b)):
            if val is None:
                continue
            if isinstance(val, bpy.types.NodeSocket):
                self.link(val, sock)
            else:
                sock.default_value = val
        return n.outputs[0]

    def noise(self, cells, seed: float, detail: float = 0.0, label: str = ""):
        """Tileable 4D noise, 0..1. `cells` = blobs across the tile (a group input socket or a
        number). Detail 0 = one smooth layer, like the shaders' value noise."""
        n = self.node("ShaderNodeTexNoise")
        n.noise_dimensions = "4D"
        n.label = label
        n.inputs["Detail"].default_value = detail
        # Scale = cells / 2π × 0.6: one trip round the torus is 2π long, and Blender's (Perlin) noise
        # shows ~1.6 blobs per lattice cell where value noise shows 1, so ×0.6 keeps "cells" honest
        self.link(self.math("MULTIPLY", self.math("DIVIDE", cells, TAU), 0.6), n.inputs["Scale"])
        off = self.node("ShaderNodeVectorMath")
        off.operation = "ADD"
        self.link(self.torus, off.inputs[0])
        off.inputs[1].default_value = (seed, seed * 1.7, seed * 2.3)   # another seed = another noise
        self.link(off.outputs[0], n.inputs["Vector"])
        self.link(self.math("ADD", self.sv, seed * 3.1), n.inputs["W"])
        return n.outputs["Fac"]

    def tidy(self) -> None:
        for i, n in enumerate(self.ng.nodes):
            n.location = (-1000 + (i % 7) * 200, 400 - (i // 7) * 220)
        self.gin.location = (-1300, 0)
        self.gout.location = (500, 0)


def group_node(mat, group, name: str):
    """Put `group` into material `mat` as a node called `name` (the bake finds it by name)."""
    n = mat.node_tree.nodes.new("ShaderNodeGroup")
    n.node_tree = group.ng
    n.name = n.label = name
    n.location = (-700, 0)
    return n


def new_material(name: str):
    mat = bpy.data.materials.new(name)
    mat.node_tree.nodes.clear()
    return mat


# ---------------------------------------------------------------- scene building

def tile_plane(name: str, size_m: float, mat, location=(0.0, 0.0, 0.0), collection_name: str = ""):
    """One texture repeat = one plane with UV 0..1 (size in metres = the shader's tile size).
    Plus a 3×3 tiling check next to it (copies sharing the mesh): seams would show there."""
    col = bpy.data.collections.new(collection_name or name)
    bpy.context.scene.collection.children.link(col)
    bpy.ops.mesh.primitive_plane_add(size=size_m, location=location)
    tile = bpy.context.active_object
    tile.name = name
    tile.data.name = name
    tile.data.materials.append(mat)
    for c in list(tile.users_collection):
        c.objects.unlink(tile)
    col.objects.link(tile)
    for ix in range(-1, 2):
        for iy in range(-1, 2):
            o = bpy.data.objects.new(f"{name}_check_{ix + 1}{iy + 1}", tile.data)
            o.location = (location[0] + ix * size_m, location[1] - 2.0 * size_m + iy * size_m, 0.0)
            o.scale = (0.999, 0.999, 1.0)   # hairline gap so you can see where tiles meet
            col.objects.link(o)
    return tile


def light_camera_world(cam_loc, cam_rot_deg, ortho_scale: float) -> None:
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 3.0
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(55), 0, math.radians(30))   # low sun: drifts cast relief
    bpy.context.scene.collection.objects.link(sun)
    cam_data = bpy.data.cameras.new("Camera")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = ortho_scale
    cam = bpy.data.objects.new("Camera", cam_data)
    cam.location = cam_loc
    cam.rotation_euler = tuple(math.radians(a) for a in cam_rot_deg)
    bpy.context.scene.collection.objects.link(cam)
    sc = bpy.context.scene
    sc.camera = cam
    world = bpy.data.worlds.new("World")
    sc.world = world
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.6, 0.75, 0.95, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.7
    sc.render.engine = "BLENDER_EEVEE"
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x, sc.render.resolution_y = 1600, 900


# ---------------------------------------------------------------- baking

def bake_outputs(tile_name: str, node_name: str, outputs: list, size: int) -> dict:
    """Bake the named outputs of group node `node_name` on plane `tile_name` (Cycles EMIT bake,
    float, Non-Color) and return {output: size×size array}, row 0 = TOP (Godot's orientation).
    The material is put back as it was, and the .blend is never saved by this."""
    tile = bpy.data.objects[tile_name]
    nt = tile.active_material.node_tree
    group = nt.nodes[node_name]
    out = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeOutputMaterial" and n.is_active_output)
    old = out.inputs["Surface"].links[0].from_socket if out.inputs["Surface"].is_linked else None

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 4
    bpy.ops.object.select_all(action="DESELECT")
    tile.select_set(True)
    bpy.context.view_layer.objects.active = tile

    result = {}
    for start in range(0, len(outputs), 3):               # 3 outputs per bake (R, G, B)
        names = outputs[start:start + 3]
        rgb = nt.nodes.new("ShaderNodeCombineColor")
        for name, chan in zip(names, ("Red", "Green", "Blue")):
            nt.links.new(group.outputs[name], rgb.inputs[chan])
        emit = nt.nodes.new("ShaderNodeEmission")
        nt.links.new(rgb.outputs["Color"], emit.inputs["Color"])
        nt.links.new(emit.outputs[0], out.inputs["Surface"])
        img = bpy.data.images.new("bake_tmp", size, size, alpha=False, float_buffer=True)
        img.colorspace_settings.name = "Non-Color"
        target = nt.nodes.new("ShaderNodeTexImage")
        target.image = img
        nt.nodes.active = target
        bpy.ops.object.bake(type="EMIT", margin=0, use_clear=True)
        px = np.array(img.pixels[:], dtype=np.float32).reshape(size, size, 4)[::-1]
        for i, name in enumerate(names):
            result[name] = px[:, :, i].copy()
        for n in (rgb, emit, target):
            nt.nodes.remove(n)
        bpy.data.images.remove(img)
    if old is not None:
        nt.links.new(old, out.inputs["Surface"])
    return result


def stretch(v: np.ndarray, lo_pct: float = 0.5, hi_pct: float = 99.5) -> np.ndarray:
    """Blender's noise stays mostly between ~0.25 and 0.75; the shaders' thresholds were tuned for
    value noise that uses the whole 0..1. Stretch so the same thresholds keep working."""
    lo, hi = np.percentile(v, lo_pct), np.percentile(v, hi_pct)
    return np.clip((v - lo) / max(hi - lo, 1e-6), 0.0, 1.0)
