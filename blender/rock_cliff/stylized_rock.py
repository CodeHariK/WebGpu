"""The stylized rock look in Blender — the same recipe as stylized_rock.gdshader, so the preview
matches the game. No textures: colour comes from world position and normal, light from toon bands.

Colour: two rock tones blended by big noise · horizontal strata · darker foot, lighter top ·
moss on upward facets · overhangs darker. Light (EEVEE Shader-to-RGB): shadow / mid / lit bands,
shadows tinted purple, a thin bright rim. The glTF export gets a plain Principled BSDF with the
average rock colour (use_toon(False)); in Godot put stylized_rock.gdshader on the mesh instead.
"""
import bpy

ROCK_LIGHT = (0.86, 0.74, 0.6)
ROCK_DARK = (0.66, 0.53, 0.45)
ROCK_UNDER = (0.42, 0.36, 0.42)
MOSS = (0.45, 0.62, 0.24)
SHADOW_TINT = (0.55, 0.5, 0.78)
SHADOW_LIGHT = 0.62
HEIGHT_RANGE = (-0.5, 6.0)
STRATA_SPACING = 0.6
STRATA_STRENGTH = 0.05
# Light amounts (EEVEE Shader to RGB: sun ≈ 4, sky ≈ 0.8) where shadow → mid and mid → lit.
MID_BAND = (0.78, 0.88)
LIT_BAND = (1.2, 1.32)
MOSS_BAND = (0.45, 0.58)  # noise range: lower = more moss


def _lin(rgb):
    return tuple(c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4 for c in rgb) + (1.0,)


class _Tree:
    def __init__(self, mat):
        self.nodes, self.links = mat.node_tree.nodes, mat.node_tree.links

    def new(self, kind, **props):
        node = self.nodes.new(kind)
        for key, value in props.items():
            setattr(node, key, value)
        return node

    def feed(self, socket, value):
        if isinstance(value, bpy.types.NodeSocket):
            self.links.new(value, socket)
        else:
            socket.default_value = value

    def math(self, op, a, b=0.0, clamp=False):
        node = self.new("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.feed(node.inputs[0], a)
        self.feed(node.inputs[1], b)
        return node.outputs[0]

    def mix(self, a, b, fac, blend="MIX"):
        node = self.new("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        ins = {s.identifier: s for s in node.inputs}
        self.feed(ins["Factor_Float"], fac)
        self.feed(ins["A_Color"], a)
        self.feed(ins["B_Color"], b)
        return next(s for s in node.outputs if s.identifier == "Result_Color")

    def smooth(self, value, lo, hi):
        node = self.new("ShaderNodeMapRange", interpolation_type="SMOOTHSTEP")
        self.feed(node.inputs["Value"], value)
        node.inputs["From Min"].default_value, node.inputs["From Max"].default_value = lo, hi
        return node.outputs["Result"]

    def noise(self, vector, scale):
        node = self.new("ShaderNodeTexNoise")
        node.inputs["Scale"].default_value, node.inputs["Detail"].default_value = scale, 2.0
        self.links.new(vector, node.inputs["Vector"])
        return node.outputs["Fac"]


def _albedo(t):
    geo = t.new("ShaderNodeNewGeometry")
    pos, normal = geo.outputs["Position"], geo.outputs["Normal"]
    up = t.new("ShaderNodeSeparateXYZ")
    t.links.new(normal, up.inputs[0])
    z = t.new("ShaderNodeSeparateXYZ")
    t.links.new(pos, z.inputs[0])
    up, height = up.outputs["Z"], z.outputs["Z"]

    colour = t.mix(_lin(ROCK_DARK), _lin(ROCK_LIGHT), t.smooth(t.noise(pos, 0.35), 0.35, 0.65))
    wobble = t.math("MULTIPLY", t.noise(pos, 0.3), 2.0)
    bands = t.math("SINE", t.math("MULTIPLY", t.math("ADD", t.math("DIVIDE", height, STRATA_SPACING), wobble), 6.2831853))
    strata = t.math("SUBTRACT", 1.0, t.math("MULTIPLY", t.smooth(bands, 0.6, 1.0), STRATA_STRENGTH))
    colour = t.mix(colour, strata, 1.0, "MULTIPLY")
    rise = t.smooth(height, HEIGHT_RANGE[0], HEIGHT_RANGE[1])
    foot = t.math("ADD", t.math("MULTIPLY", rise, 0.35), 0.7)
    colour = t.mix(colour, foot, 1.0, "MULTIPLY")
    moss = t.math("MULTIPLY", t.smooth(up, 0.55, 0.8), t.smooth(t.noise(pos, 1.4), MOSS_BAND[0], MOSS_BAND[1]))
    colour = t.mix(colour, _lin(MOSS), moss)
    return t.mix(colour, _lin(ROCK_UNDER), t.smooth(up, -0.2, -0.6))


def _toon(t, albedo):
    light = t.new("ShaderNodeBsdfDiffuse")
    to_rgb = t.new("ShaderNodeShaderToRGB")
    t.links.new(light.outputs["BSDF"], to_rgb.inputs["Shader"])
    bw = t.new("ShaderNodeRGBToBW")
    t.links.new(to_rgb.outputs["Color"], bw.inputs["Color"])
    value = bw.outputs["Val"]
    mid = t.smooth(value, MID_BAND[0], MID_BAND[1])
    lit = t.smooth(value, LIT_BAND[0], LIT_BAND[1])
    amount = t.math("ADD", t.math("MULTIPLY", mid, 0.6), t.math("MULTIPLY", lit, 0.4))
    shadow = tuple(c * SHADOW_LIGHT for c in _lin(SHADOW_TINT)[:3]) + (1.0,)
    tint = t.mix(shadow, (1, 1, 1, 1), amount)
    weight = t.new("ShaderNodeLayerWeight")
    weight.inputs["Blend"].default_value = 0.25
    rim = t.math("MULTIPLY", t.smooth(weight.outputs["Facing"], 0.75, 1.0), t.math("MULTIPLY", lit, 0.25))
    tint = t.mix(tint, rim, 1.0, "ADD")
    emit = t.new("ShaderNodeEmission")
    emit.name = "ToonEmit"
    t.links.new(t.mix(albedo, tint, 1.0, "MULTIPLY"), emit.inputs["Color"])
    return emit


def stylized_rock_material(name="M_StylizedRock"):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    t = _Tree(mat)
    bsdf = t.nodes["Principled BSDF"]
    average = tuple((a + b) / 2 for a, b in zip(ROCK_LIGHT, ROCK_DARK))
    bsdf.inputs["Base Color"].default_value = _lin(average)
    bsdf.inputs["Roughness"].default_value = 1.0
    mat.diffuse_color = _lin(average)
    _toon(t, _albedo(t))
    use_toon(mat, True)
    return mat


def use_toon(mat, on):
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    source = nodes["ToonEmit"].outputs["Emission"] if on else nodes["Principled BSDF"].outputs["BSDF"]
    links.new(source, nodes["Material Output"].inputs["Surface"])
