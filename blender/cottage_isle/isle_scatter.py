"""Geometry-nodes scattering: grass tufts and flowers on the grass, pebbles along the paths,
rocks poking out of the cliff, and hundreds of leaf cards over the tree and bushes.

Each scatter is an empty mesh object with a Geometry Nodes modifier:
  Object Info (source meshes) → Distribute Points on Faces (Poisson; faces filtered by normal and height)
  → keep points by distance to a "mask" collection (paths, plaza, house, water…) and by a noise patch
  → Instance on Points (a random pick from a collection of small prototype meshes, random size and spin,
    optionally aligned to the surface) → Realize Instances.
The glTF exporter applies the modifier, so the .glb gets plain merged meshes.
"""
import math

import bmesh
import bpy

import isle_kit
from isle_kit import bm_ball, bm_cone, mesh_object, mtx


# --- prototype meshes (kept in collections that aren't in the scene) -----------------------------

def _kinds(name):
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    return coll


def _build_into(coll, builders):
    target = isle_kit._target
    isle_kit._target = coll
    for name, colors, build in builders:
        bm = bmesh.new()
        build(bm)
        mesh_object(name, bm, colors, smooth=True)
    isle_kit._target = target
    return coll


def _tagged(bm, index, add):
    """Run add(bm) and give the faces it made material `index`."""
    before = len(bm.faces)
    add(bm)
    bm.faces.ensure_lookup_table()
    for face in bm.faces[before:]:
        face.material_index = index


def _tuft(bm, blades=4, height=0.28):
    for k in range(blades):
        a = math.tau * k / blades + 0.4 * k
        lean = 0.35 + 0.1 * (k % 2)
        h = height * (0.75 + 0.25 * ((k * 7) % 3) / 2)
        bm_cone(bm, 0.035, 0.0, h, mtx((0.04 * math.cos(a), 0.04 * math.sin(a), h / 2 * 0.9), (lean, 0, a + math.pi / 2), (1, 0.3, 1)), 3)


def _flower(bm, petal_index=0):
    _tagged(bm, 1, lambda b: bm_cone(b, 0.008, 0.008, 0.16, mtx((0, 0, 0.08)), 4))
    for k in range(5):
        a = math.tau * k / 5
        _tagged(bm, 0, lambda b, a=a: bm_ball(b, 0.045, mtx((0.05 * math.cos(a), 0.05 * math.sin(a), 0.165), (0, 0, a), (1.4, 0.8, 0.35)), 1))
    _tagged(bm, 2, lambda b: bm_ball(b, 0.028, mtx((0, 0, 0.175)), 1))


def _leaf(bm, length=0.22, width=0.11):
    """A diamond leaf card with a raised middle (4 triangles), lying in XY, pointing along X."""
    pts = [(-length, 0, 0), (0, width, 0.02), (length, 0, 0), (0, -width, 0.02)]
    verts = [bm.verts.new(p) for p in pts]
    mid = bm.verts.new((0, 0, 0.045))
    for i in range(4):
        bm.faces.new((verts[i], verts[(i + 1) % 4], mid))


def prototypes():
    grass = _build_into(_kinds("Kinds.Grass"), [
        ("Tuft.A", "blade_a", lambda bm: _tuft(bm)),
        ("Tuft.B", "blade_b", lambda bm: _tuft(bm, 5, 0.22)),
        ("Tuft.C", "blade_a", lambda bm: _tuft(bm, 3, 0.34)),
    ])
    flowers = _build_into(_kinds("Kinds.Flowers"), [
        (f"Flower.{c}", [f"petal_{c}", "sprout", "flower_eye"], _flower) for c in ("white", "pink", "yellow", "lilac")
    ])
    leaves = _build_into(_kinds("Kinds.Leaves"), [
        ("Leaf.Light", "leaf_light", lambda bm: _leaf(bm)),
        ("Leaf.Mid", "leaf", lambda bm: _leaf(bm, 0.2, 0.1)),
        ("Leaf.Dark", "leaf_dark", lambda bm: _leaf(bm, 0.24, 0.12)),
    ])
    pebbles = _build_into(_kinds("Kinds.Pebbles"), [
        ("Pebble.A", "stone", lambda bm: bm_ball(bm, 0.07, mtx(scale=(1.3, 1, 0.5)), 1)),
        ("Pebble.B", "path", lambda bm: bm_ball(bm, 0.05, mtx(scale=(1, 1.2, 0.5)), 1)),
    ])
    rocks = _build_into(_kinds("Kinds.Rocks"), [
        ("Rock.A", "rock", lambda bm: bm_ball(bm, 0.28, mtx(scale=(1.3, 1, 0.7)), 1)),
        ("Rock.B", "dirt", lambda bm: bm_ball(bm, 0.22, mtx(scale=(1, 1.4, 0.6)), 1)),
    ])
    return {"grass": grass, "flowers": flowers, "leaves": leaves, "pebbles": pebbles, "rocks": rocks}


# --- the node graph -----------------------------------------------------------------------------

class _Graph:
    def __init__(self, name):
        self.tree = bpy.data.node_groups.new(name, "GeometryNodeTree")
        self.tree.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
        self.nodes, self.links = self.tree.nodes, self.tree.links

    def new(self, kind, **props):
        node = self.nodes.new(kind)
        for key, value in props.items():
            setattr(node, key, value)
        return node

    @staticmethod
    def sock(sockets, name):
        return next(s for s in sockets if s.name == name and s.enabled)

    def set(self, node, name, value):
        target = self.sock(node.inputs, name)
        if isinstance(value, bpy.types.NodeSocket):
            self.links.new(value, target)
        else:
            target.default_value = value

    def out(self, node, name):
        return self.sock(node.outputs, name)

    def compare(self, a, op, b):
        node = self.new("FunctionNodeCompare", data_type="FLOAT", operation=op)
        self.set(node, "A", a)
        self.set(node, "B", b)
        return self.out(node, "Result")

    def both(self, a, b):
        node = self.new("FunctionNodeBooleanMath", operation="AND")
        self.links.new(a, node.inputs[0])
        self.links.new(b, node.inputs[1])
        return node.outputs[0]

    def random(self, kind, low, high, seed):
        node = self.new("FunctionNodeRandomValue", data_type=kind)
        self.set(node, "Min", low)
        self.set(node, "Max", high)
        self.set(node, "Seed", seed)
        return self.out(node, "Value")

    def xyz(self, vector):
        node = self.new("ShaderNodeSeparateXYZ")
        self.links.new(vector, node.inputs[0])
        return node.outputs


def scatter(name, sources, kinds, *, distance, density, size=(0.8, 1.2), seed=0, normal_z=(0.85, 1.01),
            below=None, above=None, mask=None, clear=(0.0, 1e9), patch=None, align=False, spin=True):
    """An object `name` whose geometry nodes scatter `kinds` over the `sources` meshes.
    normal_z: allowed range of the face normal's Z · below/above: face height limits ·
    mask/clear: keep points whose distance to the mask collection lies in clear=(min, max) ·
    patch: (noise scale, threshold) for clumps · align: stand instances on the surface normal."""
    g = _Graph(f"GN_{name}")
    joined = g.new("GeometryNodeJoinGeometry")
    for src in sources:
        info = g.new("GeometryNodeObjectInfo", transform_space="RELATIVE")
        info.inputs["Object"].default_value = src
        g.links.new(g.out(info, "Geometry"), joined.inputs[0])

    normal_z_value = g.xyz(g.out(g.new("GeometryNodeInputNormal"), "Normal"))[2]
    height = g.xyz(g.out(g.new("GeometryNodeInputPosition"), "Position"))[2]
    faces = g.both(g.compare(normal_z_value, "GREATER_THAN", normal_z[0]), g.compare(normal_z_value, "LESS_THAN", normal_z[1]))
    if below is not None:
        faces = g.both(faces, g.compare(height, "LESS_THAN", below))
    if above is not None:
        faces = g.both(faces, g.compare(height, "GREATER_THAN", above))
    dist = g.new("GeometryNodeDistributePointsOnFaces", distribute_method="POISSON")
    g.links.new(joined.outputs[0], dist.inputs["Mesh"])
    g.set(dist, "Selection", faces)
    g.set(dist, "Distance Min", distance)
    g.set(dist, "Density Max", density)
    g.set(dist, "Seed", seed)

    keep = None
    if mask is not None:
        info = g.new("GeometryNodeCollectionInfo")
        info.inputs["Collection"].default_value = mask
        info.inputs["Separate Children"].default_value = False
        real = g.new("GeometryNodeRealizeInstances")
        g.links.new(g.out(info, "Instances"), real.inputs["Geometry"])
        prox = g.new("GeometryNodeProximity")
        try:
            prox.target_element = "FACES"
        except (AttributeError, TypeError):
            pass
        g.links.new(real.outputs[0], prox.inputs[0])
        far = g.out(prox, "Distance")
        keep = g.both(g.compare(far, "GREATER_THAN", clear[0]), g.compare(far, "LESS_THAN", clear[1]))
    if patch is not None:
        noise = g.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = patch[0]
        g.links.new(g.out(g.new("GeometryNodeInputPosition"), "Position"), noise.inputs["Vector"])
        clump = g.compare(noise.outputs["Fac"], "GREATER_THAN", patch[1])
        keep = clump if keep is None else g.both(keep, clump)

    kinds_info = g.new("GeometryNodeCollectionInfo")
    kinds_info.inputs["Collection"].default_value = kinds
    kinds_info.inputs["Separate Children"].default_value = True
    kinds_info.inputs["Reset Children"].default_value = True
    inst = g.new("GeometryNodeInstanceOnPoints")
    g.links.new(g.out(dist, "Points"), inst.inputs["Points"])
    if keep is not None:
        g.set(inst, "Selection", keep)
    g.links.new(g.out(kinds_info, "Instances"), inst.inputs["Instance"])
    g.set(inst, "Pick Instance", True)
    g.set(inst, "Instance Index", g.random("INT", 0, len(kinds.objects) - 1, seed + 1))
    g.set(inst, "Scale", g.random("FLOAT", size[0], size[1], seed + 2))
    tilt = 0.0 if align else 0.25
    euler = g.new("ShaderNodeCombineXYZ")
    g.set(euler, "X", g.random("FLOAT", -tilt, tilt, seed + 3))
    g.set(euler, "Y", g.random("FLOAT", -tilt, tilt, seed + 4))
    g.set(euler, "Z", g.random("FLOAT", 0.0, math.tau if spin else 0.0, seed + 5))
    to_rot = g.new("FunctionNodeEulerToRotation")
    g.links.new(euler.outputs[0], to_rot.inputs[0])
    result = inst.outputs[0]
    if align:
        g.set(inst, "Rotation", g.out(dist, "Rotation"))
        turn = g.new("GeometryNodeRotateInstances")
        g.links.new(result, turn.inputs["Instances"])
        g.links.new(to_rot.outputs[0], turn.inputs["Rotation"])
        g.set(turn, "Local Space", True)
        result = turn.outputs[0]
    else:
        g.set(inst, "Rotation", to_rot.outputs[0])
    real = g.new("GeometryNodeRealizeInstances")
    g.links.new(result, real.inputs["Geometry"])
    output = g.new("NodeGroupOutput")
    g.links.new(real.outputs[0], output.inputs[0])

    obj = bpy.data.objects.new(name, bpy.data.meshes.new(name))
    isle_kit._target.objects.link(obj)
    obj.modifiers.new("Scatter", "NODES").node_group = g.tree
    return obj


def mask_collection(names):
    coll = bpy.data.collections.get("ScatterMask") or bpy.data.collections.new("ScatterMask")
    for name in names:
        obj = bpy.data.objects.get(name)
        if obj and obj.name not in coll.objects:
            coll.objects.link(obj)
    return coll


def scatter_scene(isle, mask_names):
    kinds = prototypes()
    mask = mask_collection(mask_names)
    objs = bpy.data.objects
    top = isle.top
    scatter("Scatter.Grass", [objs[f"{isle.name}.Grass-col"]], kinds["grass"], distance=0.16, density=24,
            above=top - 0.05, mask=mask, clear=(0.3, 1e9), patch=(0.9, 0.42), seed=1)
    scatter("Scatter.Flowers", [objs[f"{isle.name}.Grass-col"]], kinds["flowers"], distance=0.25, density=6,
            above=top - 0.05, mask=mask, clear=(0.45, 1e9), patch=(0.7, 0.58), seed=2, size=(0.8, 1.3))
    scatter("Scatter.Pebbles", [objs[f"{isle.name}.Grass-col"]], kinds["pebbles"], distance=0.2, density=7,
            above=top - 0.05, mask=mask, clear=(0.0, 0.16), patch=(1.3, 0.45), seed=3, size=(0.6, 1.3))
    scatter("Scatter.CliffRocks", [objs[f"{isle.name}.Cliff"]], kinds["rocks"], distance=0.7, density=1.2,
            normal_z=(-0.4, 0.55), below=top - 0.45, align=True, seed=4, size=(0.6, 1.5))
    scatter("Scatter.TreeLeaves", [objs["Tree.Leaves"], objs["Tree.LeavesLight"]], kinds["leaves"], distance=0.13,
            density=70, normal_z=(-0.6, 1.01), align=True, seed=5, size=(0.8, 1.4))
    scatter("Scatter.BushLeaves", [objs["Cottage.Bushes"]], kinds["leaves"], distance=0.09, density=90,
            normal_z=(-0.2, 1.01), align=True, seed=6, size=(0.5, 0.8))
