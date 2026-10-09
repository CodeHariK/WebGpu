"""The painted look, in two passes over the finished scene.

1. Vertex colours ("Col"): lighter grass in the middle and darker at the rim, patchy shading,
   cliff strata, leaves lighter on top, a per-stone / per-scale tint. Painted objects switch to a
   "_painted" copy of their material that multiplies Base Color by "Col" (exported as COLOR_0).
2. A toon branch on every material (EEVEE only): light → soft two-tone step → base colour or a cool
   purple shadow, emitted flat. set_toon(False) puts the plain Principled BSDF back for export.
"""
import math

import bpy
from mathutils import Vector

SHADOW_TINT = (0.7, 0.64, 0.86, 1)
STEP = (0.12, 0.28)  # light amount where shadow starts fading to lit, and where it is fully lit
ATTR = "Col"


# --- noise and colour helpers -----------------------------------------------------------------

def noise(p, scale=1.0):
    x, y, z = p.x * scale, p.y * scale, p.z * scale
    v = math.sin(x * 1.7 + math.sin(y * 2.3 + z)) * math.cos(y * 1.3 + math.sin(x * 1.1 - z * 0.7))
    v += 0.5 * math.sin(x * 3.1 - y * 2.7 + math.cos(z * 2.0))
    return max(0.0, min(1.0, 0.5 + v / 3))


def lerp3(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def smooth(edge0, edge1, x):
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3 - 2 * t)


WHITE = (1, 1, 1)


# --- painters: (world position, world normal) → rgb multiplier --------------------------------

def grass_painter(isle):
    def paint(p, n):
        if n.z < 0.5:
            return (0.72, 0.8, 0.66)
        d = (p.to_2d() - isle.center)
        rim = smooth(0.62, 1.0, d.length / isle.edge(math.atan2(d.y, d.x)))
        patch = smooth(0.55, 0.8, noise(p, 0.55)) * 0.6
        return lerp3(WHITE, (0.74, 0.84, 0.68), rim * 0.9 + patch * 0.5)
    return paint


def cliff_painter(isle):
    def paint(p, n):
        depth = (isle.top - p.z) / isle.depth
        strata = 0.5 + 0.5 * math.sin(p.z * 9 + noise(p, 0.8) * 3)
        base = lerp3(WHITE, (0.7, 0.6, 0.62), smooth(0.05, 0.9, depth))
        return lerp3(base, tuple(c * 0.86 for c in base), strata * 0.6)
    return paint


def leaves(p, n):
    return lerp3((0.74, 0.8, 0.7), WHITE, (n.z + 1) / 2 * 0.8 + noise(p, 2.0) * 0.2)


def speckle(amount, scale=3.0):
    return lambda p, n: lerp3((amount,) * 3, WHITE, noise(p, scale))


def walls(p, n):
    return lerp3((0.86, 0.84, 0.86), WHITE, smooth(0.2, 1.6, p.z))


def water(p, n):
    if n.z < 0.6:  # the waterfall: lighter streaks down the fall
        return lerp3((0.82, 0.9, 0.9), (1.15, 1.15, 1.1), noise(Vector((p.x * 3, p.y * 3, p.z * 0.3)), 2.0))
    return lerp3((0.86, 0.92, 0.92), WHITE, noise(p, 1.5))


def painters(isle):
    rules = {
        "Isle.Grass-col": grass_painter(isle), "Isle.Cliff": cliff_painter(isle),
        "Tree.Leaves": leaves, "Tree.LeavesLight": leaves, "Cottage.Bushes": leaves,
        "Cottage.Scales": speckle(0.8, 4.0), "Cottage.Walls-col": walls, "Water": water,
        "Plaza.Ring": speckle(0.84), "Garden.Soil": speckle(0.82), "Ferns": leaves, "Grass.Tufts": speckle(0.8),
        "Isle.GrassLip": lambda p, n: (0.8, 0.88, 0.76),
    }
    for obj in bpy.data.objects:
        if obj.name.endswith(".Stones"):
            rules[obj.name] = speckle(0.82, 2.5)
    return rules


# --- pass 1: vertex colours -------------------------------------------------------------------

def _socket(sockets, identifier):
    return next(s for s in sockets if s.identifier == identifier)


def painted_material(mat):
    """A copy of `mat` whose Base Color is multiplied by the "Col" vertex colour."""
    key = f"{mat.name}_painted"
    if key in bpy.data.materials:
        return bpy.data.materials[key]
    copy = mat.copy()
    copy.name = key
    nodes, links = copy.node_tree.nodes, copy.node_tree.links
    bsdf = nodes["Principled BSDF"]
    attr = nodes.new("ShaderNodeVertexColor")
    attr.layer_name = ATTR
    mul = nodes.new("ShaderNodeMix")
    mul.data_type, mul.blend_type = "RGBA", "MULTIPLY"
    _socket(mul.inputs, "Factor_Float").default_value = 1.0
    base = bsdf.inputs["Base Color"]
    if base.is_linked:  # already palette × texture: multiply that by the vertex colour
        links.new(base.links[0].from_socket, _socket(mul.inputs, "A_Color"))
    else:
        _socket(mul.inputs, "A_Color").default_value = base.default_value
    links.new(attr.outputs["Color"], _socket(mul.inputs, "B_Color"))
    links.new(_socket(mul.outputs, "Result_Color"), bsdf.inputs["Base Color"])
    return copy


def paint_object(obj, painter):
    mesh = obj.data
    if ATTR in mesh.color_attributes:
        mesh.color_attributes.remove(mesh.color_attributes[ATTR])
    layer = mesh.color_attributes.new(ATTR, "BYTE_COLOR", "CORNER")
    world = obj.matrix_world
    normal_matrix = world.to_3x3().inverted().transposed()
    for poly in mesh.polygons:
        n = (normal_matrix @ poly.normal).normalized()
        for li in poly.loop_indices:
            p = world @ mesh.vertices[mesh.loops[li].vertex_index].co
            r, g, b = painter(p, n)
            layer.data[li].color = (min(r, 1.0), min(g, 1.0), min(b, 1.0), 1.0)
    for i, mat in enumerate(mesh.materials):
        if mat and not mat.name.endswith("_painted"):
            mesh.materials[i] = painted_material(mat)


def paint_scene(isle):
    bpy.context.view_layer.update()
    for name, painter in painters(isle).items():
        obj = bpy.data.objects.get(name)
        if obj:
            paint_object(obj, painter)
    for name in ("ChannelCutter", "PondCutter"):  # booleans copy the cutter's attributes onto the cut walls
        obj = bpy.data.objects.get(name)
        if obj:
            paint_object(obj, lambda p, n: WHITE)


# --- pass 2: toon shading ---------------------------------------------------------------------

class _Nodes:
    """Tiny helper for wiring shader nodes."""

    def __init__(self, mat):
        self.nodes, self.links = mat.node_tree.nodes, mat.node_tree.links

    def new(self, kind, **props):
        node = self.nodes.new(kind)
        for key, value in props.items():
            setattr(node, key, value)
        return node

    def mix(self, a, b, fac=1.0, blend="MIX"):
        node = self.new("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        for ident, value in (("Factor_Float", fac), ("A_Color", a), ("B_Color", b)):
            target = _socket(node.inputs, ident)
            if isinstance(value, (int, float, tuple)):
                target.default_value = value
            else:
                self.links.new(value, target)
        return _socket(node.outputs, "Result_Color")

    def noise(self, coords, scale, detail=3.0):
        tex = self.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value, tex.inputs["Detail"].default_value = scale, detail
        self.links.new(coords, tex.inputs["Vector"])
        return tex.outputs["Fac"]

    def ramp(self, value, stops, interpolation="EASE"):
        node = self.new("ShaderNodeValToRGB")
        node.color_ramp.interpolation = interpolation
        elements = node.color_ramp.elements
        while len(elements) < len(stops):
            elements.new(0.5)
        for element, (pos, grey) in zip(elements, stops):
            element.position, element.color = pos, (grey, grey, grey, 1)
        self.links.new(value, node.inputs["Fac"])
        return node.outputs["Color"]

    def math(self, op, a, b):
        node = self.new("ShaderNodeMath", operation=op)
        for socket, value in zip(node.inputs, (a, b)):
            if isinstance(value, (int, float)):
                socket.default_value = value
            else:
                self.links.new(value, socket)
        return node.outputs["Value"]


def add_toon(mat):
    """Watercolour toon: a ragged light/shadow edge with a darker pigment rim, cool shadows,
    warm/cool colour drift across each object, and paper grain over everything."""
    if "ToonEmit" in mat.node_tree.nodes:
        return
    n = _Nodes(mat)
    bsdf = n.nodes["Principled BSDF"]
    base = bsdf.inputs["Base Color"]
    if base.is_linked:
        lit = base.links[0].from_socket
    else:
        rgb = n.new("ShaderNodeRGB")
        rgb.outputs[0].default_value = base.default_value
        lit = rgb.outputs[0]
    coords = n.new("ShaderNodeTexCoord")

    light = n.new("ShaderNodeBsdfDiffuse")  # white, so the step depends on light only, not colour
    to_rgb = n.new("ShaderNodeShaderToRGB")
    n.links.new(light.outputs["BSDF"], to_rgb.inputs["Shader"])
    bw = n.new("ShaderNodeRGBToBW")
    n.links.new(to_rgb.outputs["Color"], bw.inputs["Color"])
    ragged = n.math("MULTIPLY_ADD", n.noise(coords.outputs["Object"], 4.0, 4.0), 0.3)  # noise·0.3 − 0.15 + light
    ragged.node.inputs[2].default_value = -0.15
    value = n.math("ADD", bw.outputs["Val"], ragged)

    lit_amount = n.ramp(value, ((STEP[0], 0.0), (STEP[1], 1.0)))
    rim = n.ramp(value, ((STEP[0] - 0.04, 0.0), ((STEP[0] + STEP[1]) / 2, 1.0), (STEP[1] + 0.04, 0.0)), "LINEAR")
    shadow = n.mix(lit, SHADOW_TINT, blend="MULTIPLY")
    colour = n.mix(shadow, lit, lit_amount)
    colour = n.mix(colour, (0.78, 0.74, 0.88, 1), rim, blend="MULTIPLY")  # pigment pools at the edge

    drift = n.noise(coords.outputs["Object"], 0.6, 2.0)
    warm = n.mix(colour, (1.05, 1.0, 0.9, 1), blend="MULTIPLY")
    cool = n.mix(colour, (0.93, 0.97, 1.06, 1), blend="MULTIPLY")
    colour = n.mix(cool, warm, drift)

    paper = bpy.data.images.get("tex_paper")
    if paper:
        mapping = n.new("ShaderNodeMapping")
        mapping.inputs["Scale"].default_value = (4.0, 3.1, 1.0)
        n.links.new(coords.outputs["Window"], mapping.inputs["Vector"])
        tex = n.new("ShaderNodeTexImage", image=paper)
        n.links.new(mapping.outputs["Vector"], tex.inputs["Vector"])
        colour = n.mix(colour, tex.outputs["Color"], blend="MULTIPLY")

    emit = n.new("ShaderNodeEmission")
    emit.name = "ToonEmit"
    n.links.new(colour, emit.inputs["Color"])


def set_toon(on):
    for mat in bpy.data.materials:
        if not mat.node_tree or "ToonEmit" not in mat.node_tree.nodes:
            continue
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        source = nodes["ToonEmit"].outputs["Emission"] if on else nodes["Principled BSDF"].outputs["BSDF"]
        links.new(source, nodes["Material Output"].inputs["Surface"])


def toon_scene():
    for mat in bpy.data.materials:
        if mat.node_tree and "Principled BSDF" in mat.node_tree.nodes:
            add_toon(mat)
    set_toon(True)
