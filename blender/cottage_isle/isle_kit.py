"""A small modelling kit for the cottage isle: palette materials, collections, transforms and
primitives (box, cone, ball) that either become their own object or get merged into one bmesh
(many small props → one object, so the scene stays cheap for a mobile game)."""
import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

from isle_textures import TEXTURE_OF, image_name

PALETTE = {  # sampled by eye from Hari's painting
    "grass": (0.55, 0.7, 0.24), "grass_dark": (0.49, 0.65, 0.22), "dirt": (0.86, 0.56, 0.32),
    "rock": (0.66, 0.4, 0.24), "water": (0.3, 0.66, 0.6), "foam": (0.55, 0.82, 0.74),
    "path": (0.95, 0.8, 0.62), "tile": (0.9, 0.74, 0.56), "groove": (0.78, 0.6, 0.44),
    "wall": (0.98, 0.86, 0.64), "timber": (0.86, 0.46, 0.18), "timber_dark": (0.6, 0.3, 0.13),
    "roof": (0.87, 0.42, 0.6), "roof_dark": (0.75, 0.3, 0.5), "awning": (0.72, 0.25, 0.42),
    "stone": (0.78, 0.66, 0.62), "door": (0.66, 0.33, 0.16), "glass": (0.45, 0.26, 0.2),
    "trunk": (0.47, 0.27, 0.2), "leaf": (0.42, 0.64, 0.2), "leaf_light": (0.6, 0.78, 0.3),
    "heart": (0.96, 0.5, 0.55), "soil": (0.42, 0.27, 0.17), "sprout": (0.42, 0.7, 0.24),
    "stake": (0.7, 0.5, 0.3), "wood": (0.8, 0.52, 0.3), "rail": (0.6, 0.16, 0.2),
    "smoke": (0.62, 0.6, 0.62), "cloth": (0.75, 0.86, 0.96), "basket": (0.62, 0.38, 0.2),
    "yarn_blue": (0.2, 0.45, 0.8), "yarn_orange": (0.98, 0.6, 0.15), "yarn_pink": (0.93, 0.3, 0.42),
    "bush": (0.5, 0.68, 0.22), "fern": (0.3, 0.58, 0.25), "stump": (0.55, 0.32, 0.2),
    "stump_top": (0.9, 0.7, 0.45), "gold": (0.95, 0.75, 0.25),
    "blade_a": (0.55, 0.74, 0.26), "blade_b": (0.38, 0.6, 0.2), "leaf_dark": (0.32, 0.54, 0.18),
    "petal_white": (0.98, 0.96, 0.92), "petal_pink": (0.97, 0.6, 0.72), "petal_yellow": (1.0, 0.86, 0.35),
    "petal_lilac": (0.74, 0.64, 0.96), "flower_eye": (0.98, 0.72, 0.2),
}
GLOW = {}
SHINY = {"water": 0.15, "glass": 0.3}

_target = None


def use_collection(name):
    """New objects go into collection `name` (created under the scene collection if needed)."""
    global _target
    coll = bpy.data.collections.get(name)
    if coll is None:
        coll = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(coll)
    _target = coll
    return coll


def to_linear(rgb):
    """Palette colours are picked as they look on screen (sRGB); Blender and glTF want linear."""
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb)


def _socket(sockets, identifier):
    return next(s for s in sockets if s.identifier == identifier)


def material(name):
    """Palette colour × its painted detail texture (if the textures have been made)."""
    key = f"M_{name}"
    mat = bpy.data.materials.get(key)
    if mat:
        return mat
    rgb = to_linear(PALETTE[name])
    mat = bpy.data.materials.new(key)
    mat["palette"] = name
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = SHINY.get(name, 0.8)
    if name in GLOW:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
        bsdf.inputs["Emission Strength"].default_value = GLOW[name]
    image = bpy.data.images.get(image_name(TEXTURE_OF[name])) if name in TEXTURE_OF else None
    if image:
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = image
        mul = nodes.new("ShaderNodeMix")
        mul.data_type, mul.blend_type = "RGBA", "MULTIPLY"
        _socket(mul.inputs, "Factor_Float").default_value = 1.0
        _socket(mul.inputs, "A_Color").default_value = (*rgb, 1)
        links.new(tex.outputs["Color"], _socket(mul.inputs, "B_Color"))
        links.new(_socket(mul.outputs, "Result_Color"), bsdf.inputs["Base Color"])
    mat.diffuse_color = (*rgb, 1)
    return mat


def mtx(at=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    return Matrix.LocRotScale(Vector(at), Euler(rot), Vector(scale))


def aim(a, b):
    """(midpoint, rotation, length) for a part whose local Z runs from a to b."""
    a, b = Vector(a), Vector(b)
    rot = Vector((0, 0, 1)).rotation_difference((b - a).normalized()).to_euler()
    return (a + b) / 2, tuple(rot), (b - a).length


# --- add geometry into a bmesh -----------------------------------------------------------------

def bm_box(bm, size, matrix):
    bmesh.ops.create_cube(bm, size=1.0, matrix=matrix @ Matrix.Diagonal((*size, 1)))


def bm_cone(bm, r1, r2, depth, matrix, segments=12):
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=r1, radius2=r2, depth=depth, matrix=matrix)


def bm_ball(bm, r, matrix, subdivisions=2):
    bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=r, matrix=matrix)


def bm_torus(bm, major, minor, matrix, segments=16, sides=6):
    rings = []
    for i in range(segments):
        u = math.tau * i / segments
        ring = []
        for j in range(sides):
            v = math.tau * j / sides
            r = major + minor * math.cos(v)
            ring.append(bm.verts.new(matrix @ Vector((r * math.cos(u), r * math.sin(u), minor * math.sin(v)))))
        rings.append(ring)
    for i in range(segments):
        a, b = rings[i], rings[(i + 1) % segments]
        for j in range(sides):
            k = (j + 1) % sides
            bm.faces.new((a[j], b[j], b[k], a[k]))


# --- objects ----------------------------------------------------------------------------------

def mesh_object(name, bm, colors, at=(0, 0, 0), rot=(0, 0, 0), smooth=False, parent=None, bevel=0.0):
    """Turn bm into an object. colors: one palette name, or a list (faces pick by material_index)."""
    for face in bm.faces:
        face.smooth = smooth
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for color in [colors] if isinstance(colors, str) else colors:
        mesh.materials.append(material(color))
    obj = bpy.data.objects.new(name, mesh)
    _target.objects.link(obj)
    obj.location, obj.rotation_euler = at, rot
    obj.parent = parent
    if bevel:
        mod = obj.modifiers.new("Bevel", "BEVEL")
        mod.width, mod.segments, mod.limit_method = bevel, 2, "ANGLE"
    return obj


def box(name, size, at, color, rot=(0, 0, 0), bevel=0.03, parent=None):
    bm = bmesh.new()
    bm_box(bm, size, Matrix())
    return mesh_object(name, bm, color, at, rot, parent=parent, bevel=bevel)


def cone(name, r1, r2, depth, at, color, rot=(0, 0, 0), segments=12, smooth=True, parent=None):
    bm = bmesh.new()
    bm_cone(bm, r1, r2, depth, Matrix(), segments)
    return mesh_object(name, bm, color, at, rot, smooth=smooth, parent=parent)


def ball(name, r, at, color, squash=(1, 1, 1), subdivisions=2, smooth=True, rot=(0, 0, 0), parent=None):
    bm = bmesh.new()
    bm_ball(bm, r, Matrix.Diagonal((*squash, 1)), subdivisions)
    return mesh_object(name, bm, color, at, rot, smooth=smooth, parent=parent)


def beam(name, a, b, thickness, color, parent=None):
    mid, rot, length = aim(a, b)
    return box(name, (thickness, thickness, length), mid, color, rot, bevel=0.015, parent=parent)


def empty(name, at=(0, 0, 0), rot=(0, 0, 0), parent=None):
    obj = bpy.data.objects.new(name, None)
    _target.objects.link(obj)
    obj.location, obj.rotation_euler, obj.parent = at, rot, parent
    obj.empty_display_size = 0.5
    return obj


def merged(name, color, build, smooth=True):
    """One object from many small parts: build(bm) adds them all."""
    bm = bmesh.new()
    build(bm)
    return mesh_object(name, bm, color, smooth=smooth)
